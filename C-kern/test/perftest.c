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

// group: communication-helper

static int readsignal_perftest(perftest_t* ptest, bool isprepare)
{
   uint8_t isabort;
   int err = readall_pipe(cast_pipe(&ptest->pipe[2*isprepare], &ptest->pipe[2*isprepare+1]), 1, &isabort, TIMEOUT_PERFTEST);
   return err ? err : isabort ? ECANCELED : 0;
}

static int writesignal_perftest(perftest_t* ptest, bool isprepare, size_t nrsignal, uint8_t isabort)
{
   int err;
   uint8_t buffer[256];
   memset(buffer, isabort, sizeof(buffer));

   for (; nrsignal > sizeof(buffer); nrsignal -= sizeof(buffer)) {
      err = writeall_pipe(cast_pipe(&ptest->pipe[2*isprepare], &ptest->pipe[2*isprepare+1]), sizeof(buffer), buffer, TIMEOUT_PERFTEST);
      if (err) return err;
   }

   err = writeall_pipe(cast_pipe(&ptest->pipe[2*isprepare], &ptest->pipe[2*isprepare+1]), nrsignal, buffer, TIMEOUT_PERFTEST);
   return err;
}

static int readready_perftest(perftest_t* ptest)
{
   uint8_t iserror;
   int err = readall_pipe(cast_pipe(&ptest->pipe[4], &ptest->pipe[5]), 1, &iserror, TIMEOUT_PERFTEST);
   return err ? err : iserror ? ECANCELED : 0;
}

static int writeready_perftest(perftest_t* ptest, uint8_t iserror)
{
   return writeall_pipe(cast_pipe(&ptest->pipe[4], &ptest->pipe[5]), 1, &iserror, TIMEOUT_PERFTEST);
}

// group: instance-helper

static int threadmain_perftest(void * arg)
{
   perftest_instance_t * tinst = arg;
   perftest_t *          ptest = tinst->ptest;
   bool             isprepared = false;

   if (0 != writeready_perftest(ptest, 0/*OK*/)) goto ONERR;

   // prepare test

   // continue or abort ?
   if (0 != readsignal_perftest(ptest, true)) goto ONERR;

   if (ptest->iimpl->prepare) {
      if (ptest->iimpl->prepare(tinst)) goto ONERR;
   }
   isprepared = true;

   if (0 != writeready_perftest(ptest, 0/*OK*/)) goto ONERR;

   // run test

   // continue or abort ?
   if (0 != readsignal_perftest(ptest, false)) goto ONERR;

   if (ptest->iimpl->run) {
      if (ptest->iimpl->run(tinst)) goto ONERR;
   }

   timevalue_t tv;
   if (0 == time_sysclock(sysclock_MONOTONIC, &tv)) {
      tinst->usec = diffus_timevalue(&tv, &ptest->start_time);
   }

   if (ptest->iimpl->unprepare) {
      isprepared = false;
      if (ptest->iimpl->unprepare(tinst)) goto ONERR;
   }

   return 0;
ONERR:
   if (isprepared && ptest->iimpl->unprepare) {
      ptest->iimpl->unprepare(tinst);
   }
   writeready_perftest(ptest, 1/*iserror*/);
   FLUSHBUFFER_ERRLOG();
   return EINVAL;
}

static int processmain_perftest(void * arg)
{
   int err;
   perftest_process_t *  proc = arg;
   perftest_instance_t * tinst = proc->tinst;
   perftest_t *          ptest = tinst->ptest;
   uint16_t               ti = 0;

   // continue or abort ?
   if (0 != readsignal_perftest(ptest, false)) goto ONERR;

   // start instances

   for (; ti < proc->nrthread; ++ti) {
      if (! PROCESS_testerrortimer(&s_perftest_errtimer2, &err)) {
         err = new_thread(&tinst[ti].thread, &threadmain_perftest, &tinst[ti]);
      }
      if (err) goto ONERR;
   }

   int iserr = 0;
   for (; ti > 0; --ti) {
      if (join_thread(tinst[ti-1].thread)) goto ONERR;
      if (returncode_thread(tinst[ti-1].thread)) iserr = 1;
      if (delete_thread(&tinst[ti-1].thread)) goto ONERR;
   }

   if (iserr) goto ONERR;

   return 0;
ONERR:
   writeready_perftest(ptest, 1/*iserror*/);
   writesignal_perftest(ptest, true, ti, 1/*isabort*/);
   while (ti > 0) {
      delete_thread(&tinst[--ti].thread);
   }
   return EINVAL;
}

// group: lifetime

int new_perftest(/*out*/perftest_t** ptest, const perftest_it* iimpl/*only ref stored*/, uint16_t nrprocess/*>0*/, uint16_t nrthread_per_process/*>0*/)
{
   int err;
   vmpage_t vmpage;
   uint16_t initstate = 0;
   uint16_t pid_started = 0;
   uint32_t nrinstance = (uint32_t)nrprocess * nrthread_per_process;
   size_t   size = sizeof(perftest_t) + nrinstance * (sizeof(perftest_process_t) + sizeof(perftest_instance_t));

   if (0 == nrprocess || 0 == nrthread_per_process || MAX_NRINSTANCE < nrinstance) {
      err = EINVAL;
      goto ONERR;
   }

   if (! PROCESS_testerrortimer(&s_perftest_errtimer, &err)) {
      err = init2_vmpage(&vmpage, size, accessmode_RDWR_SHARED);
   }
   if (err) goto ONERR;
   ++ initstate; // 1

   // init newptest
   perftest_t* newptest = (perftest_t*) vmpage.addr;
   perftest_process_t* proc = (perftest_process_t*) (vmpage.addr + sizeof(perftest_t) + nrinstance * sizeof(perftest_instance_t));
   memset(newptest, 0, size);
   newptest->pagesize = vmpage.size;
   newptest->iimpl = iimpl;
   newptest->nrinstance = nrinstance;
   newptest->nrprocess  = nrprocess;
   newptest->nrthread_per_process = nrthread_per_process;
   newptest->proc = proc;
   // init array tinst
   for (uint32_t tid = 0, throffset=(uint16_t)(nrthread_per_process-1); tid < nrinstance; ++tid, --throffset) {
      newptest->tinst[tid].proc  = proc;
      newptest->tinst[tid].ptest = newptest;
      newptest->tinst[tid].tid   = tid;
      newptest->tinst[tid].nrops = 1;
      if (0 == throffset) {
         throffset = nrthread_per_process;
         ++ proc;
      }
   }
   // init array proc
   proc = newptest->proc;
   for (uint16_t pid = 0; pid < nrprocess; ++pid, ++proc) {
      proc->pid = pid;
      proc->nrthread = nrthread_per_process;
      proc->tinst = &newptest->tinst[pid * nrthread_per_process];
   }

   // init pipes

   for (unsigned i = 0; i < lengthof(newptest->pipe); i += 2) {
      if (! PROCESS_testerrortimer(&s_perftest_errtimer, &err)) {
         err = init_pipe(cast_pipe(&newptest->pipe[i], &newptest->pipe[i+1]));
      }
      if (err) goto ONERR;
      ++ initstate; // 2,3,4
   }

   // start process (which call prepare for every tinstance)

   proc = newptest->proc;
   for (; pid_started < nrprocess; ++pid_started, ++proc) {
      if (! PROCESS_testerrortimer(&s_perftest_errtimer, &err)) {
         process_stdio_t stdfd = process_stdio_INIT_INHERIT;
         err = init_process(&proc->process, &processmain_perftest, proc, &stdfd);
      }
      if (err) goto ONERR;
   }
   err = writesignal_perftest(newptest, false, nrprocess, 0/*OK*/);
   if (err) goto ONERR;
   ++ initstate; // 5

   // wait for signal of started threads (perftest_instance_t)

   for (uint16_t tid = 0; tid < nrinstance; ++tid) {
      err = readready_perftest(newptest);
      (void) PROCESS_testerrortimer(&s_perftest_errtimer, &err);
      if (err) goto ONERR;
   }

   // set out param
   *ptest = newptest;

   return 0;
ONERR:
   switch (initstate) {
   case 5:  // abort threads (started processes wait for completion)
            writesignal_perftest(newptest, true, nrinstance, 1/*isabort*/);

   case 4:  // abort pid_started processes
            writesignal_perftest(newptest, false, nrprocess, 1/*isabort*/);
            proc = newptest->proc;
            for (uint16_t pid = 0; pid < pid_started; ++pid, ++proc) {
               (void) free_process(&proc->process);
            }
            static_assert(lengthof(newptest->pipe) == 6, "free alle pipes");
            free_pipe(cast_pipe(&newptest->pipe[4], &newptest->pipe[5]));
            /*continue at following case*/
   case 3:  free_pipe(cast_pipe(&newptest->pipe[2], &newptest->pipe[3]));
            /*continue at following case*/
   case 2:  free_pipe(cast_pipe(&newptest->pipe[0], &newptest->pipe[1]));
            /*continue at following case*/
   case 1:  free_vmpage(&vmpage);
            /*continue at following case*/
   default: break;
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

      for (unsigned i = 0; i < lengthof(delobj->pipe); i += 2) {
         err2 = free_pipe(cast_pipe(&delobj->pipe[i], &delobj->pipe[i+1]));
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

   // prepare test

   err = writesignal_perftest(ptest, true, ptest->nrinstance, 0/*OK*/);
   if (err) goto ONERR;

   // wait for OK of prepare operation

   for (uint16_t tid = 0; tid < ptest->nrinstance; ++tid) {
      err = readready_perftest(ptest);
      if (err) goto ONERR;
   }

   // start test

   err = time_sysclock(sysclock_MONOTONIC, cast_timevalue(&ptest->start_time));
   if (err) goto ONERR;

   err = writesignal_perftest(ptest, false, ptest->nrinstance, 0/*OK*/);
   if (err) goto ONERR;

   // wait for end of instances

   for (unsigned i = 0; i < ptest->nrprocess; ++i) {
      process_result_t result;
      err = wait_process(&ptest->proc[i].process, &result);
      if (err) goto ONERR;
      if (process_state_TERMINATED != result.state || 0 != result.returncode) {
         err = ECANCELED;
         goto ONERR;
      }
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
   writesignal_perftest(ptest, false, ptest->nrinstance, 1/*isabort*/);
   TRACEEXIT_ERRLOG(err);
   return err;
}

int exec_perftest(perftest_it* iimpl, void* shared_addr, size_t shared_size, uint16_t nrprocess/*>0*/, uint16_t nrthread_per_process/*>0*/, /*out*/uint64_t* nrops, /*out*/uint64_t* usec)
{
   int err;
   perftest_t* ptest;

   err = new_perftest(&ptest, iimpl, nrprocess, nrthread_per_process);
   if (err) goto ONERR;

   setshared_perftest(ptest, shared_addr, shared_size);

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

static int test_initfree(void)
{
   perftest_t * ptest = 0;
   perftest_it  iimpl = perftest_INIT(0,0,0);

   // TEST new_perftest: EINVAL
   TEST(EINVAL == new_perftest(&ptest, &iimpl, 0, 1));
   TEST(EINVAL == new_perftest(&ptest, &iimpl, 1, 0));
   TEST(EINVAL == new_perftest(&ptest, &iimpl, 40000, 1 + (MAX_NRINSTANCE / 40000)));

   for (unsigned p = 1; p <= 5; p += 2) {
      for (unsigned t = 1; t <= 10; t += 3) {

         // TEST new_perftest: 1<=nrprocess,nrthread_per_process<=5
         TESTP(0 == new_perftest(&ptest, &iimpl, (uint16_t)p, (uint16_t)t), "p=%d t=%d", p, t);
         // prüfe ptest in shared Memory
         TEST(0 != ptest);
         {
            vmpage_t page = vmpage_INIT(ptest->pagesize, (uint8_t*)ptest);
            TEST(ismapped_vm(&page, accessmode_RDWR_SHARED));
         }
         // prüfe Inhalt ptest
         size_t minsize = sizeof(perftest_t)+ p*t*(sizeof(perftest_process_t) + sizeof(perftest_instance_t));
         size_t maxsize = minsize + pagesize_vm();
         TEST(minsize <= ptest->pagesize);
         TEST(maxsize > ptest->pagesize);
         TEST(&iimpl == ptest->iimpl);
         for (unsigned i = 0; i < lengthof(ptest->pipe); ++i) {
            TEST(isvalid_iochannel(ptest->pipe[i]));
         }
         TEST(p*t == ptest->nrinstance);
         TEST(p == ptest->nrprocess);
         TEST(t == ptest->nrthread_per_process);
         TEST(0 == ptest->start_time.seconds);  // default
         TEST(0 == ptest->start_time.nanosec); // default
         TEST(0 == ptest->shared_addr); // default
         TEST(0 == ptest->shared_size); // default
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

   // TEST new_perftest: Fehler in new_perftest
   iimpl = (perftest_it) perftest_INIT(0,0,0);
   for (unsigned tc = 1; tc < 10; ++tc) {
      const int E = (int)tc;
      init_testerrortimer(&s_perftest_errtimer, tc, E);
      TEST(E == new_perftest(&ptest, &iimpl, 3, 1));
      TEST(0 == ptest);
   }

   // TEST new_perftest: Fehler in processmain_perftest
   iimpl = (perftest_it) perftest_INIT(0,0,0);
   for (unsigned tc = 1; tc <= 3; ++tc) {
      init_testerrortimer(&s_perftest_errtimer2, tc, (int)tc);
      TEST(ECANCELED == new_perftest(&ptest, &iimpl, 5, 3));
      TEST(0 == ptest);
   }
   free_testerrortimer(&s_perftest_errtimer2);

   // TEST delete_perftest: Fehler in delete_perftest
   iimpl = (perftest_it) perftest_INIT(0,0,0);
   for (unsigned tc = 1; tc < 8; ++tc) {
      const int E = (int)tc;
      TEST(0 == new_perftest(&ptest, &iimpl, 3, 1));
      init_testerrortimer(&s_perftest_errtimer, tc, E);
      TEST(E == delete_perftest(&ptest));
      TEST(0 == ptest);
   }

   return 0;
ONERR:
   delete_perftest(&ptest);
   return EINVAL;
}

static int test_queryupdate(void)
{
   perftest_t * ptest = 0;
   perftest_it  iimpl = perftest_INIT(0,0,0);

   // prepare
   TEST(0 == new_perftest(&ptest, &iimpl, 1, 1));

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
   int count_prepare;
   int count_run;
   int count_unprepare;
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
   TEST(0 == init2_vmpage(&vmpage, sizeof(test_stats_t ), accessmode_RDWR_SHARED));
   stats = (test_stats_t*) vmpage.addr;

   // TEST measure_perftest: prepare / run / unprepare werden aufgerufen
   // prepare
   TEST(0 == new_perftest(&ptest, &iimpl, 5, 4));
   memset(stats, 0, sizeof(*stats));
   ptest->shared_addr = stats;
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

   // TEST measure_perftest: Fehler in prepare
   for (unsigned i = 0; i < 4; ++i) {
      // prepare
      TEST(0 == new_perftest(&ptest, &iimpl, 2, 2));
      memset(stats, 0, sizeof(*stats));
      stats->prepare_err = ENOMEM;
      stats->errtid      = i;
      ptest->shared_addr = stats;
      // test
      TEST(ECANCELED == measure_perftest(ptest, &nrops, &usec));
      // prüfe stats
      TEST(stats->count_prepare   <= (int) ptest->nrinstance);
      TEST(stats->count_prepare   >= 1);
      TEST(stats->count_run       == 0);
      TEST(stats->count_unprepare <= stats->count_prepare);
      // reset
      TEST(0 == delete_perftest(&ptest));
   }

   // TEST measure_perftest: Fehler in run
   for (unsigned i = 0; i < 4; ++i) {
      // prepare
      TEST(0 == new_perftest(&ptest, &iimpl, 2, 2));
      memset(stats, 0, sizeof(*stats));
      stats->run_err = ENOMEM;
      stats->errtid  = i;
      ptest->shared_addr = stats;
      // test
      TEST(ECANCELED == measure_perftest(ptest, &nrops, &usec));
      // prüfe stats
      TEST(stats->count_prepare   == (int) ptest->nrinstance);
      TEST(stats->count_run       == (int) ptest->nrinstance);
      TEST(stats->count_unprepare == (int) ptest->nrinstance);
      // reset
      TEST(0 == delete_perftest(&ptest));
   }

   // TEST measure_perftest: Fehler in unprepare
   for (unsigned i = 0; i < 4; ++i) {
      // prepare
      TEST(0 == new_perftest(&ptest, &iimpl, 2, 2));
      memset(stats, 0, sizeof(*stats));
      stats->unprepare_err = ENOMEM;
      stats->errtid        = i;
      ptest->shared_addr   = stats;
      // test
      TEST(ECANCELED == measure_perftest(ptest, &nrops, &usec));
      // prüfe stats
      TEST(stats->count_prepare   == (int) ptest->nrinstance);
      TEST(stats->count_run       == (int) ptest->nrinstance);
      TEST(stats->count_unprepare == (int) ptest->nrinstance);
      // reset
      TEST(0 == delete_perftest(&ptest));
   }

   // TEST measure_perftest: usleep
   iimpl = (perftest_it) perftest_INIT(0, &run_usleep, 0);
   for (unsigned i = 1; i <= 4; ++i) {
      // prepare
      TEST(0 == new_perftest(&ptest, &iimpl, (uint16_t)(1+(i>2)), (uint16_t)(2-i%2)));
      memset(stats, 0, sizeof(*stats));
      stats->usec = 10000 + i * 1000;
      ptest->shared_addr = stats;
      // test
      TEST(0 == measure_perftest(ptest, &nrops, &usec));
      // prüfe out param
      TEST(nrops == ptest->nrinstance);
      TEST(usec >= stats->usec);
      TEST(usec <  stats->usec+2000);
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

static int prepare_loop(perftest_instance_t* tinst)
{
   tinst->nrops = 10000000;
   return 0;
}

static int run_loop(perftest_instance_t* tinst)
{
   size_t sum = 0;
   for (size_t i = (size_t)tinst->nrops; i; ) {
      sum += --i;
   }
   tinst->size = sum;
   return 0;
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

   // TEST exec_perftest: Wiederhole 10_000_000
   iimpl = (perftest_it) perftest_INIT(prepare_loop, &run_loop, 0);
   TEST(0 == exec_perftest(&iimpl, stats, 0, 1, 1, &nrops, &usec));
   uint64_t ops_per_usec1 = nrops / usec;
   TEST(0 == exec_perftest(&iimpl, stats, 0, 1, 2, &nrops, &usec));
   uint64_t ops_per_usec2 = nrops / usec;
   TEST(ops_per_usec1 < ops_per_usec2); // ! multi core !

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
