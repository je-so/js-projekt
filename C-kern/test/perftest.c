/* title: TestPerformance impl

   Implements <TestPerformance>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/test/perftest.h
    Header file <TestPerformance>.

   file: C-kern/test/perftest.c
    Implementation file <TestPerformance impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/test/perftest.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/pipe.h"
#include "C-kern/api/memory/atomic.h"
#include "C-kern/api/memory/vm.h"
#include "C-kern/api/platform/task/process.h"
#include "C-kern/api/platform/task/thread.h"
#include "C-kern/api/time/sysclock.h"
#include "C-kern/api/time/timevalue.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/memory/atomic.h"
#endif

// section: perftest_instance_t

static void init_perftestinstance(perftest_instance_t* pinst, perftest_t* ptest, perftest_process_t* proc, uint32_t tid)
{
   pinst->thread= 0;
   pinst->proc= proc;
   pinst->ptest= ptest;
   pinst->tid= tid;
   pinst->err= 0;
   pinst->usec= 0;
   pinst->nrops= 1;
   pinst->addr= 0;
   pinst->size= 0;
}

// section: perftest_process_t

static void init_perftestprocess(perftest_process_t* proc, uint16_t pid, uint16_t nrthread_per_process, perftest_instance_t* pinst)
{
   proc->process=  sys_process_FREE;
   proc->pid=      pid;
   proc->nrthread= nrthread_per_process;
   proc->err=      0;
   proc->tinst =   pinst;
}

// section: perftest_t

// group: constants
#define MAX_NRINSTANCE \
         ((UINT32_MAX - sizeof(perftest_t)) / (sizeof(perftest_process_t) + sizeof(perftest_instance_t)))

#define TIMEOUT_PERFTEST \
         5000/*5 Sekunden*/

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_perftest_errtimer
 * Simuliert Fehler in <new_perftest> und <delete_perftest>. */
static test_errortimer_t s_perftest_errtimer = test_errortimer_FREE;
/* variable: s_perftest_errtimer2
 * Simuliert Fehler in <processmain_perftest>. */
static test_errortimer_t s_perftest_errtimer2 = test_errortimer_FREE;
#endif

// group: instance-helper

static int threadmain_perftest(void * arg)
{
   int err= 0;
   perftest_instance_t * tinst = arg;
   perftest_t *          ptest = tinst->ptest;
   bool             isprepared = false;

   // prepare test
   if (ptest->iimpl->prepare) {
      err= ptest->iimpl->prepare(tinst);
      if (err) goto ONERR;
   }
   isprepared = true;

   // wait for start or abort
   add_atomicint(&ptest->nrprepared_thread, 1);
   while (!ptest->startsignal && !ptest->aborterr) {
      pthread_yield();
   }
   if (ptest->aborterr) goto ONERR;

   // run test
   if (ptest->iimpl->run) {
      err= ptest->iimpl->run(tinst);
      if (err) goto ONERR;
   }

   timevalue_t tv;
   if (0 == time_sysclock(sysclock_MONOTONIC, &tv)) {
      tinst->usec = diffus_timevalue(&tv, &ptest->start_time);
   }

   if (ptest->iimpl->unprepare) {
      isprepared = false;
      err= ptest->iimpl->unprepare(tinst);
   }

ONERR:
   if (isprepared && ptest->iimpl->unprepare) {
      ptest->iimpl->unprepare(tinst);
   }
   FLUSHBUFFER_ERRLOG();
   if (err) {
      tinst->err= err;
      cmpxchg_atomicint(&ptest->aborterr, 0, err);
   }
   return err;
}

static int processmain_perftest(void * arg)
{
   int err;
   int iserr=0;
   perftest_process_t *  proc = arg;
   perftest_instance_t * tinst = proc->tinst;
   perftest_t *          ptest = tinst->ptest;
   uint16_t              ti;

   // start instances

   for (ti= 0; ti< proc->nrthread && !ptest->aborterr; ++ti) {
      if (! PROCESS_testerrortimer(&s_perftest_errtimer2, &err)) {
         err= new_thread(&tinst[ti].thread, &threadmain_perftest, &tinst[ti]);
      }
      if (err) {
         iserr= err;
         cmpxchg_atomicint(&ptest->aborterr, 0, err);
         break;
      }
   }

   add_atomicint(&ptest->nrprepared_process, 1);

   for (; ti> 0; --ti) {
      err= join_thread(tinst[ti-1].thread);
      iserr=  iserr?iserr:err;
      err= returncode_thread(tinst[ti-1].thread);
      iserr=  iserr?iserr:err;
      err= delete_thread(&tinst[ti-1].thread);
      iserr=  iserr?iserr:err;
   }

   err= iserr;

   if (err) {
      proc->err= err;
      cmpxchg_atomicint(&ptest->aborterr, 0, err);
   }
   return err;
}

// group: lifetime

int new_perftest(/*out*/perftest_t** ptest, const perftest_it* iimpl/*only ref stored*/, uint16_t nrprocess/*>0*/, uint16_t nrthread_per_process/*>0*/, void* shared_addr, size_t shared_size)
{
   int err;
   vmpage_t vmpage= vmpage_FREE;
   perftest_t* newptest = 0;
   uint16_t nrprocess_started = 0;
   uint32_t nrinstance = (uint32_t)nrprocess * nrthread_per_process;
   size_t   size = sizeof(perftest_t) + (size_t)nrprocess * sizeof(perftest_process_t) + nrinstance * sizeof(perftest_instance_t);

   if (0 == nrprocess || 0 == nrthread_per_process || MAX_NRINSTANCE < nrinstance) {
      err = EINVAL;
      goto ONERR;
   }

   if (! PROCESS_testerrortimer(&s_perftest_errtimer, &err)) {
      err = init2_vmpage(&vmpage, size, accessmode_RDWR_SHARED);
   }
   if (err) goto ONERR;

   // init newptest
   newptest = (perftest_t*) vmpage.addr;
   perftest_process_t* proc = (perftest_process_t*) (vmpage.addr + sizeof(perftest_t) + nrinstance * sizeof(perftest_instance_t));
   newptest->pagesize= vmpage.size;
   newptest->iimpl = iimpl;
   newptest->nrinstance= nrinstance;
   newptest->nrprocess= nrprocess;
   newptest->nrthread_per_process= nrthread_per_process;
   newptest->aborterr= 0;
   newptest->nrprepared_process= 0;
   newptest->nrprepared_thread= 0;
   newptest->startsignal= 0;
   // newptest->start_time // set before start
   newptest->shared_addr= shared_addr;
   newptest->shared_size= shared_size;
   newptest->proc= proc;

   // init every process
   uint32_t tid = 0;
   for (uint16_t pid = 0; pid < nrprocess; ++pid, ++proc) {
      init_perftestprocess(proc, pid, nrthread_per_process, &newptest->tinst[tid]);
      // init every instance
      for (uint16_t nrthread=0; nrthread<nrthread_per_process; ++nrthread, ++tid) {
         init_perftestinstance(&newptest->tinst[tid], newptest, proc, tid);
      }
   }

   // start processes (which call prepare for every tinstance)

   proc = newptest->proc;
   for (; nrprocess_started < nrprocess; ++nrprocess_started, ++proc) {
      if (! PROCESS_testerrortimer(&s_perftest_errtimer, &err)) {
         process_stdio_t stdfd = process_stdio_INIT_INHERIT;
         err = init_process(&proc->process, &processmain_perftest, proc, &stdfd);
      }
      if (err) goto ONERR;
   }

   // wait for signal of started threads (perftest_instance_t)

   while (newptest->nrprepared_process< nrprocess || newptest->nrprepared_thread< nrinstance) {
      if (newptest->aborterr) goto ONERR;
      sleepms_thread(5);
      for (uint16_t pid = 0; pid < nrprocess; ++pid, ++proc) {
         process_state_e state;
         err= state_process(&newptest->proc[pid].process, &state);
         if (err) goto ONERR;
         if (state!= process_state_RUNNABLE) {
            err= ECANCELED;
            goto ONERR;
         }
      }
   }

   // set out param
   *ptest = newptest;

   return 0;
ONERR:
   if (! isfree_vmpage(&vmpage)) {
      cmpxchg_atomicint(&newptest->aborterr, 0, err);
      err= newptest->aborterr;
      proc = newptest->proc;
      for (uint16_t pid = 0; pid < nrprocess_started; ++pid, ++proc) {
         process_result_t result;
         (void) wait_process(&proc->process, &result);
         (void) free_process(&proc->process);
      }
      free_vmpage(&vmpage);
   }
   TRACEEXIT_ERRLOG(err);
   return err;
}

int delete_perftest(perftest_t** ptest)
{
   int err;
   int err2;
   perftest_t* delobj = *ptest;

   if (delobj) {
      *ptest = 0;

      err = 0;

      for (unsigned i = 0; i < delobj->nrprocess; ++i) {
         err2 = free_process(&delobj->proc[i].process);
         (void) PROCESS_testerrortimer(&s_perftest_errtimer, &err2);
         if (err2) err = err2;
      }

      vmpage_t vmpage = vmpage_INIT(delobj->pagesize, (uint8_t*)delobj);
      err2 = free_vmpage(&vmpage);
      (void) PROCESS_testerrortimer(&s_perftest_errtimer, &err2);
      if (err2) err = err2;

      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: measure

int measure_perftest(perftest_t* ptest, /*out*/uint64_t* nrops, /*out*/uint64_t* usec)
{
   int err;

   if (ptest->tinst[0].thread == 0) {
      return EALREADY;
   }

   // start test

   err = time_sysclock(sysclock_MONOTONIC, cast_timevalue(&ptest->start_time));
   if (err) goto ONERR;

   write_atomicint(&ptest->startsignal, 1);

   // wait for end of instances

   int err2= 0;
   for (unsigned i = 0; i < ptest->nrprocess; ++i) {
      process_result_t result;
      err = wait_process(&ptest->proc[i].process, &result);
      err2= err2?err2:err;
      if (process_state_TERMINATED != result.state || 0 != result.returncode) {
         err2= err2?err2:ECANCELED;
      }
   }
   if (err2) {
      err= err2;
      goto ONERR;
   }

   // calculate time

   uint64_t sum_nrops = 0;
   uint64_t max_usec = 0;
   for (uint16_t tid = 0; tid < ptest->nrinstance; ++tid) {
      sum_nrops += ptest->tinst[tid].nrops;
      if (max_usec < (uint64_t) ptest->tinst[tid].usec) {
         max_usec = (uint64_t) ptest->tinst[tid].usec;
      }
   }

   // set out param
   *nrops = sum_nrops;
   *usec  = max_usec;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int exec_perftest(perftest_it* iimpl, void* shared_addr, size_t shared_size, uint16_t nrprocess/*>0*/, uint16_t nrthread_per_process/*>0*/, /*out*/uint64_t* nrops, /*out*/uint64_t* usec)
{
   int err;
   perftest_t* ptest;

   err = new_perftest(&ptest, iimpl, nrprocess, nrthread_per_process, shared_addr, shared_size);
   if (err) goto ONERR;

   err = measure_perftest(ptest, nrops, usec);

   int err2 = delete_perftest(&ptest);
   if (err2) err = err2;

   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_constants(void)
{

   // TEST MAX_NRINSTANCE
   const size_t elemsize  = sizeof(perftest_process_t) + sizeof(perftest_instance_t);
   const size_t arraysize = MAX_NRINSTANCE * elemsize;
   TEST(MAX_NRINSTANCE == (UINT32_MAX-sizeof(perftest_t)) / elemsize);
   TEST(arraysize >= UINT32_MAX-sizeof(perftest_t)-elemsize);

   // TEST TIMEOUT_PERFTEST
   static_assert(
      1 <= TIMEOUT_PERFTEST
      && TIMEOUT_PERFTEST <= 10*1000, "1ms <= TIMEOUT_PERFTEST <= 10s");

   return 0;
ONERR:
   return EINVAL;
}

static int prepare_count(perftest_instance_t* tinst)
{
   uint32_t* counter= sharedaddr_perftest(tinst->ptest);
   add_atomicint(counter, 1);
   return 0;
}

static int prepare_busfault(perftest_instance_t* tinst)
{
   uint32_t* tid= sharedaddr_perftest(tinst->ptest);
   if (tinst->tid== *tid) {
      tid=0;
      *tid=0;
   }
   return 0;
}


static int test_initfree(void)
{
   perftest_t * ptest = 0;
   perftest_it  iimpl = perftest_INIT(0,0,0);
   uint32_t   * counter= 0;
   vmpage_t     vmpage = vmpage_FREE;

   // prepare
   TEST(0 == init2_vmpage(&vmpage, sizeof(uint32_t), accessmode_RDWR_SHARED));
   counter= (uint32_t*) vmpage.addr;

   // TEST new_perftest: EINVAL
   TEST(EINVAL == new_perftest(&ptest, &iimpl, 0, 1, 0, 0));
   TEST(EINVAL == new_perftest(&ptest, &iimpl, 1, 0, 0, 0));
   TEST(EINVAL == new_perftest(&ptest, &iimpl, 40000, 1 + (MAX_NRINSTANCE / 40000), 0, 0));

   for (unsigned p = 1; p <= 5; p += 2) {
      for (unsigned t = 1; t <= 10; t += 3) {

         // TEST new_perftest: 1<=nrprocess,nrthread_per_process<=5
         TESTP(0 == new_perftest(&ptest, &iimpl, (uint16_t)p, (uint16_t)t, (void*)(uintptr_t)(p+1), (size_t)t+1), "p=%d t=%d", p, t);
         // prüfe ptest in shared Memory
         TEST(0 != ptest);
         {
            vmpage_t page = vmpage_INIT(ptest->pagesize, (uint8_t*)ptest);
            TEST(ismapped_vm(&page, accessmode_RDWR_SHARED));
         }
         // prüfe Inhalt ptest
         size_t minsize = sizeof(perftest_t)+ p*sizeof(perftest_process_t) + p*t*sizeof(perftest_instance_t);
         size_t maxsize = minsize + pagesize_vm();
         TEST(minsize <= ptest->pagesize);
         TEST(maxsize > ptest->pagesize);
         TEST(&iimpl == ptest->iimpl);
         TEST(p*t == ptest->nrinstance);
         TEST(p == ptest->nrprocess);
         TEST(t == ptest->nrthread_per_process);
         TEST(p+1 == (uintptr_t)ptest->shared_addr);
         TEST(t+1 == ptest->shared_size);
         // prüfe Inhalt perftest_process_t
         TEST((void*)ptest->proc == (void*)&ptest->tinst[ptest->nrinstance]);
         TEST((uint8_t*)&ptest->proc[p] <= ((uint8_t*)ptest + ptest->pagesize));
         for (unsigned i = 0; i < p; ++i) {
            TEST(ptest->proc[i].process != process_FREE); // refactor -> isfree_process()
            TEST(ptest->proc[i].pid == i);
            TEST(ptest->proc[i].nrthread == t);
            TEST(ptest->proc[i].tinst == ptest->tinst+t*i);
         }
         // prüfe Inhalt perftest_instance_t
         for (unsigned i = 0; i < ptest->nrinstance; ++i) {
            TEST(ptest->tinst[i].thread != 0);
            TEST(ptest->tinst[i].proc == ptest->proc+(i/t));
            TEST(ptest->tinst[i].ptest == ptest);
            TEST(ptest->tinst[i].tid == i);
            TEST(ptest->tinst[i].usec == 0); // default
            TEST(ptest->tinst[i].nrops == 1); // default
            TEST(ptest->tinst[i].addr == 0); // default
            TEST(ptest->tinst[i].size == 0); // default
         }

         // TEST delete_perftest
         TEST(0 == delete_perftest(&ptest));
         TEST(0 == ptest);
         TEST(0 == delete_perftest(&ptest));
         TEST(0 == ptest);
      }
   }

   // TEST new_perftest: prepare is run
   *counter= 0;
   iimpl= (perftest_it)perftest_INIT(&prepare_count,0,0);
   TEST(0 == new_perftest(&ptest, &iimpl, 5, 7, counter, 0));
   TESTP(35 == read_atomicint(counter), "counter: %u", read_atomicint(counter));
   TEST(0 == delete_perftest(&ptest));

   // TEST new_perftest: prepare does cause a hw signal
   for (uint32_t tid=0; tid<7*3; tid+= 4) {
      iimpl= (perftest_it)perftest_INIT(&prepare_busfault,0,0);
      *counter= tid;
      TEST(ECANCELED == new_perftest(&ptest, &iimpl, 7, 3, counter, 0));
      TEST(0 == delete_perftest(&ptest));
   }

   // TEST new_perftest: Fehler in new_perftest
   iimpl = (perftest_it) perftest_INIT(0,0,0);
   for (unsigned tc = 1; tc < 4; ++tc) {
      const int E = (int)tc;
      init_testerrortimer(&s_perftest_errtimer, tc, E);
      TEST(E == new_perftest(&ptest, &iimpl, 3, 1, 0, 0));
      TEST(0 == ptest);
   }

   // TEST new_perftest: Fehler in processmain_perftest
   iimpl = (perftest_it) perftest_INIT(0,0,0);
   for (unsigned tc = 1; tc <= 3; ++tc) {
      init_testerrortimer(&s_perftest_errtimer2, tc, ECANCELED);
      TEST(ECANCELED == new_perftest(&ptest, &iimpl, 5, 3, 0, 0));
      TEST(0 == ptest);
   }
   free_testerrortimer(&s_perftest_errtimer2);

   // TEST delete_perftest: Fehler in delete_perftest
   iimpl = (perftest_it) perftest_INIT(0,0,0);
   for (unsigned tc = 1; tc < 5; ++tc) {
      const int E = (int)tc;
      TEST(0 == new_perftest(&ptest, &iimpl, 3, 1, 0, 0));
      init_testerrortimer(&s_perftest_errtimer, tc, E);
      TEST(E == delete_perftest(&ptest));
      TEST(0 == ptest);
   }

   // reset
   TEST(0 == free_vmpage(&vmpage));

   return 0;
ONERR:
   free_vmpage(&vmpage);
   delete_perftest(&ptest);
   return EINVAL;
}

static int test_queryupdate(void)
{
   perftest_t * ptest = 0;
   perftest_it  iimpl = perftest_INIT(0,0,0);

   // prepare
   TEST(0 == new_perftest(&ptest, &iimpl, 1, 1, 0, 0));

   // TEST sharedaddr_perftest
   for (uintptr_t i = 1; i; i <<= 1) {
      ptest->shared_addr = (void*)i;
      TEST((void*)i == sharedaddr_perftest(ptest));
   }
   ptest->shared_addr = 0;
   TEST(0 == sharedaddr_perftest(ptest));

   // TEST sharedsize_perftest
   for (size_t i = 1; i; i <<= 1) {
      ptest->shared_size = i;
      TEST(i == sharedsize_perftest(ptest));
   }
   ptest->shared_size = 0;
   TEST(0 == sharedsize_perftest(ptest));

   // TEST setshared_perftest
   setshared_perftest(ptest, (void*)1, 2);
   TEST(ptest->shared_addr == (void*)1);
   TEST(ptest->shared_size == 2);
   setshared_perftest(ptest, (void*)2, 1);
   TEST(ptest->shared_addr == (void*)2);
   TEST(ptest->shared_size == 1);
   setshared_perftest(ptest, 0, 0);
   TEST(ptest->shared_addr == 0);
   TEST(ptest->shared_size == 0);

   // reset
   TEST(0 == delete_perftest(&ptest));

   return 0;
ONERR:
   delete_perftest(&ptest);
   return EINVAL;
}

typedef struct test_stats_t {
   volatile int count_prepare;
   volatile int count_run;
   volatile int count_unprepare;
   int prepare_err;
   int run_err;
   int unprepare_err;
   unsigned errtid;
   unsigned usec;
} test_stats_t;

static int prepare_stats(perftest_instance_t* tinst)
{
   test_stats_t* stats = sharedaddr_perftest(tinst->ptest);
   add_atomicint(&stats->count_prepare, 1);
   tinst->nrops = tinst->tid;
   return tinst->tid == stats->errtid ? stats->prepare_err : 0;
}

static int run_stats(perftest_instance_t* tinst)
{
   test_stats_t* stats = sharedaddr_perftest(tinst->ptest);
   add_atomicint(&stats->count_run, 1);
   tinst->addr = (void*) (uintptr_t) tinst->tid;
   return tinst->tid == stats->errtid ? stats->run_err : 0;
}

static int unprepare_stats(perftest_instance_t* tinst)
{
   test_stats_t* stats = sharedaddr_perftest(tinst->ptest);
   add_atomicint(&stats->count_unprepare, 1);
   tinst->size = tinst->tid;
   return tinst->tid == stats->errtid ? stats->unprepare_err : 0;
}

static int run_usleep(perftest_instance_t* tinst)
{
   test_stats_t* stats = sharedaddr_perftest(tinst->ptest);
   add_atomicint(&stats->count_run, 1);
   usleep(stats->usec);
   return 0;
}

static int test_measure(void)
{
   perftest_t * ptest = 0;
   perftest_it  iimpl = perftest_INIT(&prepare_stats, &run_stats, &unprepare_stats);
   vmpage_t     vmpage = vmpage_FREE;
   test_stats_t * stats;
   uint64_t     nrops;
   uint64_t     usec;

   // prepare
   TEST(0 == init2_vmpage(&vmpage, sizeof(test_stats_t), accessmode_RDWR_SHARED));
   stats = (test_stats_t*) vmpage.addr;

   // TEST measure_perftest: prepare / run / unprepare werden aufgerufen
   // prepare
   memset(stats, 0, sizeof(*stats));
   TEST(0 == new_perftest(&ptest, &iimpl, 5, 4, stats, 0));
   // test
   TEST(0 == measure_perftest(ptest, &nrops, &usec));
   // prüfe out param
   TEST(190 == nrops); // 0+1+2+...+19 // 19 == 5*4 -1
   TEST(1 <= usec);
   // prüfe stats
   TEST(ptest->nrinstance == (unsigned) stats->count_prepare);
   TEST(ptest->nrinstance == (unsigned) stats->count_run);
   TEST(ptest->nrinstance == (unsigned) stats->count_unprepare);
   // prüfe Thread Instanzen einzeln
   for (uintptr_t i = 0; i < ptest->nrinstance; ++i) {
      TEST(ptest->tinst[i].thread == 0);
      TEST(ptest->tinst[i].nrops == i);
      TEST(ptest->tinst[i].addr  == (void*)i);
      TEST(ptest->tinst[i].size  == i);
   }
   // TEST measure_perftest: EALREADY (mehr al einmal aufrufen)
   TEST(EALREADY == measure_perftest(ptest, &nrops, &usec));
   // reset
   TEST(0 == delete_perftest(&ptest));

   // TEST new_perftest: Fehler in prepare
   for (unsigned i = 0; i < 4; ++i) {
      // prepare
      memset(stats, 0, sizeof(*stats));
      stats->prepare_err = ENOMEM;
      stats->errtid      = i;
      // test
      TEST(ENOMEM == new_perftest(&ptest, &iimpl, 2, 2, stats, 0));
      // prüfe stats
      TEST(stats->count_prepare   <= 4);
      TEST(stats->count_prepare   >= 1);
      TEST(stats->count_run       == 0);
      TEST(stats->count_unprepare <  stats->count_prepare);
      // reset
      TEST(0 == delete_perftest(&ptest));
   }

   // TEST measure_perftest: Fehler in run
   for (unsigned i = 0; i < 4; ++i) {
      // prepare
      memset(stats, 0, sizeof(*stats));
      TEST(0 == new_perftest(&ptest, &iimpl, 2, 2, stats, 0));
      stats->run_err = ENOMEM;
      stats->errtid  = i;
      // test
      TEST(ECANCELED == measure_perftest(ptest, &nrops, &usec));
      // prüfe stats
      TEST(stats->count_prepare   == (int) ptest->nrinstance);
      TESTP(1<= stats->count_run && stats->count_run<= (int) ptest->nrinstance, "i:%d run:%d unprep:%d nrinst:%d", i, stats->count_run, stats->count_unprepare, (int) ptest->nrinstance);
      TEST(stats->count_unprepare == (int) ptest->nrinstance);
      // reset
      TEST(0 == delete_perftest(&ptest));
   }

   // TEST measure_perftest: Fehler in unprepare
   for (unsigned i = 0; i < 4; ++i) {
      // prepare
      memset(stats, 0, sizeof(*stats));
      TEST(0 == new_perftest(&ptest, &iimpl, 2, 2, stats, 0));
      stats->unprepare_err = ENOMEM;
      stats->errtid        = i;
      // test
      TEST(ECANCELED == measure_perftest(ptest, &nrops, &usec));
      // prüfe stats
      TEST(stats->count_prepare   == (int) ptest->nrinstance);
      TESTP(stats->count_run      <= (int) ptest->nrinstance, "i:%d run:%d unprep:%d nrinst:%d", i, stats->count_run, stats->count_unprepare, (int) ptest->nrinstance);
      TEST(stats->count_unprepare == (int) ptest->nrinstance);
      // reset
      TEST(0 == delete_perftest(&ptest));
   }

   // TEST measure_perftest: usleep
   iimpl = (perftest_it) perftest_INIT(0, &run_usleep, 0);
   for (unsigned i = 1; i <= 4; ++i) {
      // prepare
      memset(stats, 0, sizeof(*stats));
      TEST(0 == new_perftest(&ptest, &iimpl, (uint16_t)(1+(i>2)), (uint16_t)(2-i%2), stats, 0));
      stats->usec = 10000 + i * 1000;
      // test
      TEST(0 == measure_perftest(ptest, &nrops, &usec));
      // prüfe out param
      TEST(nrops == ptest->nrinstance);
      TEST(usec >= stats->usec);
      TESTP(usec <  stats->usec+2000, "usec:%"PRIu64" stats:%"PRIu32, usec, stats->usec);
      // prüfe stats
      TEST(stats->count_run == (int)ptest->nrinstance);
      // reset
      TEST(0 == delete_perftest(&ptest));
   }

   // reset
   TEST(0 == delete_perftest(&ptest));
   TEST(0 == free_vmpage(&vmpage));

   return 0;
ONERR:
   delete_perftest(&ptest);
   free_vmpage(&vmpage);
   return EINVAL;
}

static int test_exec(void)
{
   perftest_it  iimpl = perftest_INIT(&prepare_stats, &run_stats, &unprepare_stats);
   vmpage_t     vmpage = vmpage_FREE;
   test_stats_t * stats;
   uint64_t     nrops;
   uint64_t     usec;

   // prepare
   TEST(0 == init2_vmpage(&vmpage, sizeof(test_stats_t ), accessmode_RDWR_SHARED));
   stats = (test_stats_t*) vmpage.addr;

   // TEST exec_perftest: prepare / run / unprepare werden aufgerufen
   memset(stats, 0, sizeof(*stats));
   TEST(0 == exec_perftest(&iimpl, stats, 0, 2, 3, &nrops, &usec));
   // prüfe out param
   TEST(15 == nrops); // 0+1+2+...+5
   TEST(1 <= usec);
   // prüfe stats
   TEST(6 == stats->count_prepare);
   TEST(6 == stats->count_run);
   TEST(6 == stats->count_unprepare);

   // TEST exec_perftest: usleep
   iimpl = (perftest_it) perftest_INIT(0, &run_usleep, 0);
   for (unsigned i = 1; i <= 4; ++i) {
      // prepare
      memset(stats, 0, sizeof(*stats));
      stats->usec = 10000 + i * 1000;
      // test
      TEST(0 == exec_perftest(&iimpl, stats, 0, (uint16_t)(1+(i>2)), (uint16_t)(2-i%2), &nrops, &usec));
      unsigned nrinst = (1u+(i>2)) * (2u-i%2);
      // prüfe out param
      TEST(nrops == nrinst);
      TEST(usec >= stats->usec);
      TEST(usec <  stats->usec+2000);
      // prüfe stats
      TEST(stats->count_run == (int) nrinst);
   }

   // reset
   TEST(0 == free_vmpage(&vmpage));

   return 0;
ONERR:
   free_vmpage(&vmpage);
   return EINVAL;
}

int unittest_test_perftest()
{
   if (test_constants())      goto ONERR;
   if (test_initfree())       goto ONERR;
   if (test_queryupdate())    goto ONERR;
   if (test_measure())        goto ONERR;
   if (test_exec())           goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
