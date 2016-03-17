/* title: ThreadLocalStorage impl

   Implements <ThreadLocalStorage>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/platform/task/thread_localstore.h
    Header file <ThreadLocalStorage>.

   file: C-kern/platform/Linux/task/thread_localstore.c
    Implementation file <ThreadLocalStorage impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/platform/task/thread_localstore.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/writer/log/log.h"
#include "C-kern/api/io/writer/log/logbuffer.h"
#include "C-kern/api/io/writer/log/logwriter.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/vm.h"
#include "C-kern/api/platform/task/thread.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/io/pipe.h"
#endif



/* struct: thread_localstore_t
 * Thread variables stored in thread local storage. */
struct thread_localstore_t {
   // group: private variables
   /* variable: thread
    * Thread object itself. */
   thread_t          thread;
   /* variable: logwriter
    * A statically allocated <logwriter_t> used during init and free operations of <threadcontext_t>. */
   logwriter_t       logwriter;
   /* variable: memsize
    * Size of static memory (>= extsize_threadcontext()). */
   size_t            memsize;
   /* variable: memused
    * Nr of already allocated bytes of static memory. */
   size_t            memused;
   /* variable: logmem
    * Static memory used for call to <initstatic_logwriter>. */
   uint8_t           logmem[minbufsize_logwriter()];
   /* variable: mem
    * Static memory used in <init_threadcontext> to allocate additional memory. */
   uint8_t           mem[/*memsize*/];
};

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_threadlocalstore_errtimer
 * Simulates an erwror in <new_threadlocalstore> and <delete_threadlocalstore>. */
static test_errortimer_t   s_threadlocalstore_errtimer = test_errortimer_FREE;
#endif

// group: helper

static inline size_t sizemem_threadlocalstore(size_t sizevars)
{
   return sizevars - offsetof(thread_localstore_t, mem);
}

static inline size_t pagealign(const size_t value, const size_t pagesize)
{
   return (value + pagesize - 1) & (~(pagesize-1));
}

/* function: sizesignalstack_threadlocalstore
 * Returns the minimum size of the signal stack.
 * The returned value is a multiple of pagesize. */
static inline size_t sizesignalstack_threadlocalstore(const size_t pagesize)
{
   static_assert(MINSIGSTKSZ < size_threadlocalstore(), "size_threadlocalstore is big enough");
   return pagealign(MINSIGSTKSZ, pagesize);
}

/* function: sizestack_threadlocalstore
 * Returns the default size of the thread stack.
 * The returned value is a multiple of pagesize. */
static inline size_t sizestack_threadlocalstore(const size_t pagesize)
{
   static_assert(256*1024 < size_threadlocalstore(), "size_threadlocalstore is big enough");
   return pagealign(256*1024, pagesize);
}

/* function: sizevars_threadlocalstore
 * Returns the size of all local thread variables on the stack.
 * The returned value is a multiple of pagesize. */
static inline size_t sizevars_threadlocalstore(const size_t pagesize)
{
   // OPTIMIZE: use extsize_processcontext only if thread_threadlocalstore(tls) is main thread
   return pagealign( sizeof(thread_localstore_t)
                     + extsize_processcontext() + extsize_threadcontext(), pagesize);
}

// group: lifetime

static int init_threadlocalstore(/*out*/thread_localstore_t * tls, size_t sizevars)
{
   int err;

   tls->thread = (thread_t) thread_INIT_STATIC(tls);

   if (! PROCESS_testerrortimer(&s_threadlocalstore_errtimer, &err)) {
      err = initstatic_logwriter(&tls->logwriter, sizeof(tls->logmem), tls->logmem);
   }
   if (err) goto ONERR;

   tls->memsize = sizemem_threadlocalstore(sizevars);
   tls->memused = 0;
   memset(tls->logmem, 0, sizeof(tls->logmem));

   return 0;
ONERR:
   TRACE_LOG(AUTO, log_channel_ERR, log_flags_LAST, FUNCTION_EXIT_ERRLOG, err);
   return err;
}

static void free_threadlocalstore(thread_localstore_t * tls)
{
   freestatic_logwriter(&tls->logwriter);
}

int new_threadlocalstore(/*out*/thread_localstore_t** tls, /*out*/struct memblock_t* threadstack, /*out*/struct memblock_t* signalstack, size_t pagesize)
{
   int err;
   void * addr = MAP_FAILED;
   size_t sizevars  = sizevars_threadlocalstore(pagesize);
   size_t sizesigst = sizesignalstack_threadlocalstore(pagesize);
   size_t sizestack = sizestack_threadlocalstore(pagesize);
   size_t minsize   = 3 * pagesize /* 3 protection pages around two stacks */
                      + sizevars + sizesigst + sizestack;

   if (PROCESS_testerrortimer(&s_threadlocalstore_errtimer, &err)) {
      minsize = size_threadlocalstore() + 1;
   }

   if (minsize > size_threadlocalstore()) {
      err = ENOSPC;
      goto ONERR;
   }

   /* -- memory page layout --
    *
    * low :  thread local variables (1 or more pages, sizevars)
    *     :  protection page (1 page)
    *     :  signal stack    (X pages, sizesigst)
    *     :  protection page (1 page)
    *     :  thread stack    (256K, sizestack)
    *     :  protection page (1 page)
    *     :  reserved pages  (X pages, used as additional protection)
    * high:  == low + size_threadlocalstore()
    */

   size_t size = 2*size_threadlocalstore();
   addr = mmap(0, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
   if (PROCESS_testerrortimer(&s_threadlocalstore_errtimer, &err)) {
      munmap(addr, size);
      errno = err;
      addr = MAP_FAILED;
   }
   if (addr == MAP_FAILED) {
      err = errno;
      TRACE_LOG(AUTO, log_channel_ERR, log_flags_NONE, FUNCTION_SYSCALL_ERRLOG, "mmap", err);
      goto ONERR;
   }

   if (PROCESS_testerrortimer(&s_threadlocalstore_errtimer, &err)) goto ONERR;
   {
      uint8_t* aligned_addr = (uint8_t*) (
                                 ((uintptr_t)addr + size_threadlocalstore()-1)
                                 & ~(uintptr_t)(size_threadlocalstore()-1) );

      if ((uint8_t*)addr < aligned_addr) {
         size_t dsize = (size_t) (aligned_addr - (uint8_t*)addr);
         if (munmap(addr, dsize)) {
            err = errno;
            TRACE_LOG(AUTO, log_channel_ERR, log_flags_NONE, FUNCTION_SYSCALL_ERRLOG, "munmap", err);
            goto ONERR;
         }
         size -= dsize;
         addr  = aligned_addr;
      }
   }

   if (PROCESS_testerrortimer(&s_threadlocalstore_errtimer, &err)) goto ONERR;
   if (size > size_threadlocalstore()) {
      if (munmap((uint8_t*)addr + size_threadlocalstore(), size - size_threadlocalstore())) {
         err = errno;
         TRACE_LOG(AUTO, log_channel_ERR, log_flags_NONE, FUNCTION_SYSCALL_ERRLOG, "munmap", err);
         goto ONERR;
      }
      size = size_threadlocalstore();
   }

   size_t offset = sizevars/*thread local vars*/;
   if (PROCESS_testerrortimer(&s_threadlocalstore_errtimer, &err)) {
      errno = err;
   } else {
      err = mprotect((uint8_t*)addr + offset, pagesize, PROT_NONE);
   }
   if (err) {
      err = errno;
      TRACE_LOG(AUTO, log_channel_ERR, log_flags_NONE, FUNCTION_SYSCALL_ERRLOG, "mprotect", err);
      goto ONERR;
   }

   offset += pagesize/*protection*/ + sizesigst/*signal stack*/;
   if (PROCESS_testerrortimer(&s_threadlocalstore_errtimer, &err)) {
      errno = err;
   } else {
      err = mprotect((uint8_t*)addr + offset, pagesize, PROT_NONE);
   }
   if (err) {
      err = errno;
      TRACE_LOG(AUTO, log_channel_ERR, log_flags_NONE, FUNCTION_SYSCALL_ERRLOG, "mprotect", err);
      goto ONERR;
   }

   offset += pagesize/*protection*/ + sizestack/*thread stack*/;
   if (PROCESS_testerrortimer(&s_threadlocalstore_errtimer, &err)) {
      errno = err;
   } else {
      err = mprotect((uint8_t*)addr + offset, size_threadlocalstore() - offset, PROT_NONE);
   }
   if (err) {
      err = errno;
      TRACE_LOG(AUTO, log_channel_ERR, log_flags_NONE, FUNCTION_SYSCALL_ERRLOG, "mprotect", err);
      goto ONERR;
   }

   static_assert(
      (uintptr_t) context_threadlocalstore(0)   == offsetof(thread_t, threadcontext)
      && (uintptr_t) thread_threadlocalstore(0) == offsetof(thread_localstore_t, thread),
      "query functions use offset corresponding to struct thread_localstore_t"
   );

   err = init_threadlocalstore((thread_localstore_t*)addr, sizevars);
   if (err) goto ONERR;

   // set out param
   if (threadstack) {
      *threadstack = (memblock_t) memblock_INIT(sizestack, (uint8_t*)addr + offset - sizestack);
   }

   if (signalstack) {
      *signalstack = (memblock_t) memblock_INIT(sizesigst, (uint8_t*)addr + sizevars + pagesize);
   }

   *tls = (thread_localstore_t*) addr;

   return 0;
ONERR:
   if (addr != MAP_FAILED) {
      munmap(addr, size);
   }
   TRACE_LOG(AUTO, log_channel_ERR, log_flags_LAST, FUNCTION_EXIT_ERRLOG, err);
   return err;
}

int delete_threadlocalstore(thread_localstore_t** tls)
{
   int err;

   if (*tls) {

      free_threadlocalstore(*tls);

      err = munmap((void*)*tls, size_threadlocalstore());
      if (PROCESS_testerrortimer(&s_threadlocalstore_errtimer, &err)) {
         errno = err;
      }
      if (err) {
         err = errno;
         TRACE_LOG(AUTO, log_channel_ERR, log_flags_NONE, FUNCTION_SYSCALL_ERRLOG, "munmap", err);
      }

      *tls = 0;

      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACE_LOG(AUTO, log_channel_ERR, log_flags_LAST, FUNCTION_EXIT_FREE_RESOURCE_ERRLOG, err);
   return err;
}

// group: query

struct logwriter_t * logwriter_threadlocalstore(thread_localstore_t* tls)
{
   return &tls->logwriter;
}

void signalstack_threadlocalstore(thread_localstore_t* tls,/*out*/struct memblock_t * stackmem)
{
   size_t pagesize  = pagesize_vm();
   size_t offset    = tls ? sizevars_threadlocalstore(pagesize) + pagesize : 0;
   size_t sizesigst = tls ? sizesignalstack_threadlocalstore(pagesize) : 0;
   *stackmem = (memblock_t) memblock_INIT(sizesigst, (uint8_t*)tls + offset);
}

/* function: threadstack_threadlocalstore(const thread_localstore_t * tls)
 * Returns the thread stack from tls. */
void threadstack_threadlocalstore(thread_localstore_t* tls,/*out*/struct memblock_t* stackmem)
{
   size_t pagesize  = pagesize_vm();
   size_t offset    = tls ? sizesignalstack_threadlocalstore(pagesize) + sizevars_threadlocalstore(pagesize) + 2*pagesize : 0;
   size_t sizestack = tls ? sizestack_threadlocalstore(pagesize) : 0;
   *stackmem = (memblock_t) memblock_INIT(sizestack, (uint8_t*)tls + offset);
}

// group: static-memory

int memalloc_threadlocalstore(thread_localstore_t* tls, size_t bytesize, /*out*/struct memblock_t* memblock)
{
   int err;
   size_t alignedsize = (bytesize + KONFIG_MEMALIGN-1u) & (~(KONFIG_MEMALIGN-1u));

   if (  alignedsize < bytesize
         || alignedsize > tls->memsize - tls->memused) {
      err = ENOMEM;
      goto ONERR;
   }

   memblock->addr = tls->mem + tls->memused;
   memblock->size = alignedsize;

   tls->memused += alignedsize;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int memfree_threadlocalstore(thread_localstore_t* tls, struct memblock_t* memblock)
{
   int err;

   if (!isfree_memblock(memblock)) {
      size_t alignedsize = (memblock->size + KONFIG_MEMALIGN-1u) & (~(KONFIG_MEMALIGN-1u));
      uint8_t*    memend = (tls->mem + tls->memused);

      VALIDATE_INPARAM_TEST(  alignedsize >= memblock->size
                              && alignedsize <= tls->memused
                              && memblock->addr == memend - alignedsize, ONERR, );

      tls->memused -= alignedsize;

      *memblock = (memblock_t) memblock_FREE;
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

size_t sizestatic_threadlocalstore(const thread_localstore_t* tls)
{
   return tls->memused;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   thread_localstore_t*  tls = 0;
   const size_t   sizevars = sizevars_threadlocalstore(pagesize_vm());
   memblock_t     threadstack = memblock_FREE;
   memblock_t     signalstack = memblock_FREE;
   vmpage_t       vmpage;
   pipe_t         pipe = pipe_FREE;
   int            olderr = -1;
   maincontext_e  oldtype = g_maincontext.type;
   uint8_t *      logbuffer;
   size_t         logsize;
   size_t         oldlogsize;

   // prepare0
   TEST(0 == init_pipe(&pipe))
   olderr = dup(STDERR_FILENO);
   TEST(olderr > 0);
   TEST(STDERR_FILENO == dup2(pipe.write, STDERR_FILENO));

   for (int tc = 0; tc < 2; ++tc) {

      // prepare
      GETBUFFER_ERRLOG(&logbuffer, &oldlogsize);
      if (tc == 1) {
         // simulate not initialized main context ==> ..._LOG(AUTO uses INIT log
         g_maincontext.type = maincontext_STATIC;
      }

      // TEST new_threadlocalstore
      TEST(0 == new_threadlocalstore(&tls, 0, 0, pagesize_vm()));
      // check tls aligned
      TEST(0 != tls);
      TEST(0 == (uintptr_t)tls % size_threadlocalstore());
      // check *tls
      TEST( isstatic_threadcontext(&tls->thread.threadcontext));
      thread_t sthread = thread_INIT_STATIC(tls);
      TEST( memcmp(&tls->thread, &sthread, sizeof(sthread)) == 0);
      TEST( tls->logwriter.addr == tls->logmem);
      TEST( tls->logwriter.size == minbufsize_logwriter());
      TEST( tls->memsize == sizemem_threadlocalstore(sizevars));
      TEST( tls->memused == 0);
      for (unsigned i = 0; i < lengthof(tls->logmem); ++i) {
         TEST( 0 == tls->logmem[i]);
      }

      // TEST delete_threadlocalstore
      TEST(0 == delete_threadlocalstore(&tls));
      TEST(0 == tls);
      TEST(0 == delete_threadlocalstore(&tls));
      TEST(0 == tls);

      // TEST new_threadlocalstore: correct protection
      TEST(0 == new_threadlocalstore(&tls, &threadstack, &signalstack, pagesize_vm()));
      // variables
      vmpage = (vmpage_t) vmpage_INIT(sizevars_threadlocalstore(pagesize_vm()), (uint8_t*)tls);
      TEST(1 == ismapped_vm(&vmpage, accessmode_RDWR));
      // protection page
      vmpage = (vmpage_t) vmpage_INIT(pagesize_vm(), (uint8_t*)tls + sizevars);
      TEST(1 == ismapped_vm(&vmpage, accessmode_NONE));
      // signal stack page
      vmpage = (vmpage_t) vmpage_INIT(sizesignalstack_threadlocalstore(pagesize_vm()), (uint8_t*)tls + sizevars + 1 * pagesize_vm());
      TEST(1 == ismapped_vm(&vmpage, accessmode_RDWR));
      // check parameter signalstack
      TEST(vmpage.addr == signalstack.addr);
      TEST(vmpage.size == signalstack.size);
      // protection page
      vmpage = (vmpage_t) vmpage_INIT(pagesize_vm(), (uint8_t*)tls + sizevars + sizesignalstack_threadlocalstore(pagesize_vm()) + 1 * pagesize_vm());
      TEST(1 == ismapped_vm(&vmpage, accessmode_NONE));
      // thread stack page
      vmpage = (vmpage_t) vmpage_INIT(sizestack_threadlocalstore(pagesize_vm()), (uint8_t*)tls + sizevars + sizesignalstack_threadlocalstore(pagesize_vm()) + 2 * pagesize_vm());
      TEST(1 == ismapped_vm(&vmpage, accessmode_RDWR));
      // check parameter threadstack
      TEST(vmpage.addr == threadstack.addr);
      TEST(vmpage.size == threadstack.size);
      // protection page
      size_t offset = sizevars + sizesignalstack_threadlocalstore(pagesize_vm()) + sizestack_threadlocalstore(pagesize_vm()) + 2 * pagesize_vm();
      vmpage = (vmpage_t) vmpage_INIT(size_threadlocalstore() - offset, (uint8_t*)tls + offset);
      TEST(1 == ismapped_vm(&vmpage, accessmode_NONE));

      // TEST delete_threadlocalstore: unmap pages
      vmpage = (vmpage_t) vmpage_INIT(size_threadlocalstore(), (uint8_t*)tls);
      TEST(0 == delete_threadlocalstore(&tls));
      TEST(1 == isunmapped_vm(&vmpage));

      // TEST new_threadlocalstore: ERROR
      threadstack = (memblock_t) memblock_FREE;
      signalstack = (memblock_t) memblock_FREE;
      for (int i = 1; i; ++i) {
         init_testerrortimer(&s_threadlocalstore_errtimer, (unsigned)i, i);
         int err = new_threadlocalstore(&tls, &threadstack, &signalstack, pagesize_vm());
         if (!err) {
            TEST(9 == i);
            break;
         }
         TEST(err == (i == 1 ? ENOSPC : i));
         // check parameter
         TEST(0 == tls);
         TEST( isfree_memblock(&threadstack));
         TEST( isfree_memblock(&signalstack));
      }

      // TEST delete_threadlocalstore: ERROR
      TEST(0 != tls);
      init_testerrortimer(&s_threadlocalstore_errtimer, 1, EINVAL);
      TEST(EINVAL == delete_threadlocalstore(&tls));
      // check param tls
      TEST(0 == tls);

      GETBUFFER_ERRLOG(&logbuffer, &logsize);

      uint8_t buffer[1024];
      if (tc == 0) {
         TEST(logsize > oldlogsize); // ERRLOG used
      } else {
         TEST(logsize == oldlogsize); // INIT log used
         ssize_t bytes = read(pipe.read, buffer, sizeof(buffer));
         TEST(bytes > 0); // at least some bytes
         while (bytes > 0) {
            PRINTF_ERRLOG("%.*s", (int)bytes, buffer);
            bytes = read(pipe.read, buffer, sizeof(buffer));
         }
         TEST(bytes == -1 && errno == EAGAIN);
      }

      // reset
      TEST(-1 == read(pipe.read, buffer, sizeof(buffer)));
      g_maincontext.type = oldtype;
   }

   // reset0
   TEST(STDERR_FILENO == dup2(olderr, STDERR_FILENO));
   TEST(0 == close(olderr));
   TEST(0 == free_pipe(&pipe));

   return 0;
ONERR:
   if (tls) delete_threadlocalstore(&tls);
   if (olderr > 0) {
      dup2(olderr, STDERR_FILENO);
      close(olderr);
   }
   free_pipe(&pipe);
   g_maincontext.type = oldtype;
   return EINVAL;
}

static int test_query(void)
{
   thread_localstore_t* tls = 0;
   thread_localstore_t  tls2;
   const size_t         MINSIZE = 3*pagesize_vm() + sizesignalstack_threadlocalstore(pagesize_vm()) + sizestack_threadlocalstore(pagesize_vm()) + sizevars_threadlocalstore(pagesize_vm());
   memblock_t stackmem;

   // prepare
   TEST(0 == new_threadlocalstore(&tls, 0, 0, pagesize_vm()));

   // TEST sizesignalstack_threadlocalstore
   TEST(MINSIGSTKSZ <= sizesignalstack_threadlocalstore(pagesize_vm()));
   TEST(0 == sizesignalstack_threadlocalstore(pagesize_vm()) % pagesize_vm());

   // TEST sizestack_threadlocalstore
   TEST(PTHREAD_STACK_MIN <= sizestack_threadlocalstore(pagesize_vm()));
   TEST(0 == sizestack_threadlocalstore(pagesize_vm()) % pagesize_vm());

   // TEST sizevars_threadlocalstore
   size_t sizevars = sizevars_threadlocalstore(pagesize_vm());
   TEST(sizevars == sizevars_threadlocalstore(pagesize_vm()));
   TEST(sizevars >= sizeof(thread_localstore_t) + extsize_threadcontext() + extsize_processcontext());
   TEST(0 == sizevars % pagesize_vm());

   // TEST size_threadlocalstore
   TEST(0 == size_threadlocalstore() % pagesize_vm());
   TEST(size_threadlocalstore()/2 < MINSIZE);
   TEST(size_threadlocalstore()  >= MINSIZE);
   TEST(size_threadlocalstore()  == sys_tlssize_syscontext());

   // TEST signalstack_threadlocalstore
   signalstack_threadlocalstore(tls, &stackmem);
   TEST(stackmem.addr == (uint8_t*)tls + sizevars_threadlocalstore(pagesize_vm()) + pagesize_vm());
   TEST(stackmem.size == sizesignalstack_threadlocalstore(pagesize_vm()));

   // TEST signalstack_threadlocalstore: tls == 0
   signalstack_threadlocalstore(0, &stackmem);
   TEST(1 == isfree_memblock(&stackmem));

   // TEST threadstack_threadlocalstore
   threadstack_threadlocalstore(tls, &stackmem);
   TEST(stackmem.addr == (uint8_t*)tls + sizevars_threadlocalstore(pagesize_vm()) + sizesignalstack_threadlocalstore(pagesize_vm()) + 2*pagesize_vm());
   TEST(stackmem.size == sizestack_threadlocalstore(pagesize_vm()));

   // TEST threadstack_threadlocalstore: tls == 0
   threadstack_threadlocalstore(0, &stackmem);
   TEST(1 == isfree_memblock(&stackmem));

   // TEST self_threadlocalstore
   TEST( self_threadlocalstore() == (thread_localstore_t*) (((uintptr_t)&tls) - ((uintptr_t)&tls) % size_threadlocalstore()));
   TEST( self_threadlocalstore() == (thread_localstore_t*) sys_tcontext_syscontext());

   // TEST castPcontext_threadlocalstore
   TEST(tls == castPcontext_threadlocalstore(&tls->thread.threadcontext));
   TEST(&tls2 == castPcontext_threadlocalstore(&tls2.thread.threadcontext));

   // TEST castPthread_threadlocalstore
   TEST(tls == castPthread_threadlocalstore(&tls->thread));
   TEST(&tls2 == castPthread_threadlocalstore(&tls2.thread));

   // TEST logwriter_threadlocalstore
   TEST(&tls->logwriter == logwriter_threadlocalstore(tls));
   TEST(&tls2.logwriter == logwriter_threadlocalstore(&tls2));
   TEST(logwriter_threadlocalstore((thread_localstore_t*)0) == (logwriter_t*)offsetof(thread_localstore_t, logwriter));

   // TEST thread_threadlocalstore
   TEST(thread_threadlocalstore(tls) == &tls->thread);
   TEST(thread_threadlocalstore(&tls2) == &tls2.thread);
   TEST(thread_threadlocalstore((thread_localstore_t*)0) == (thread_t*) 0);

   // TEST context_threadlocalstore
   TEST(context_threadlocalstore(tls) == &tls->thread.threadcontext);
   TEST(context_threadlocalstore(&tls2) == &tls2.thread.threadcontext);
   TEST(context_threadlocalstore((thread_localstore_t*)0) == (threadcontext_t*) 0);

   // TEST sys_tcontext_syscontext
   TEST(sys_tcontext_syscontext() == (threadcontext_t*) (((uintptr_t)&tls) - ((uintptr_t)&tls) % size_threadlocalstore()));

   // TEST sys_tcontext2_syscontext
   for (size_t i = 0; i < 1000*size_threadlocalstore(); i += size_threadlocalstore()) {
      TEST((threadcontext_t*)i == sys_tcontext2_syscontext(i));
      TEST((threadcontext_t*)i == sys_tcontext2_syscontext(i+1));
      TEST((threadcontext_t*)i == sys_tcontext2_syscontext(i+size_threadlocalstore()-1));
   }

   // TEST sys_tlssize_syscontext
   TEST(0 == sys_tlssize_syscontext() % pagesize_vm());
   TEST( sys_tlssize_syscontext()/2 < MINSIZE);
   TEST( sys_tlssize_syscontext()  >= MINSIZE);
   TEST( sys_tlssize_syscontext()  == size_threadlocalstore());

   // unprepare
   TEST(0 == delete_threadlocalstore(&tls));

   return 0;
ONERR:
   delete_threadlocalstore(&tls);
   return EINVAL;
}

static int test_memory(void)
{
   thread_localstore_t * tls = 0;
   memblock_t mblock;
   size_t     logsize1;
   size_t     logsize2;
   uint8_t *  logbuf1;
   uint8_t *  logbuf2;

   // prepare0
   TEST(0 == new_threadlocalstore(&tls, 0, 0, pagesize_vm()));
   const size_t memsize = tls->memsize;

   // TEST memalloc_threadlocalstore
   for (size_t u = 0; u <= memsize; ++u) {
      for (size_t s = memsize-u; s <= memsize-u; --s, s -= (s > 1000 ? 1000 : 0)) {
         size_t a = s % KONFIG_MEMALIGN ? s - s % KONFIG_MEMALIGN + KONFIG_MEMALIGN : s;
         if (a > memsize - u) continue;
         tls->memused = u;
         TEST(0 == memalloc_threadlocalstore(tls, s, &mblock));
         // check parameter
         TEST(mblock.addr == tls->mem + u);
         TEST(mblock.size == a);
         // check tls
         TEST(memsize == tls->memsize);
         TEST(u + a   == tls->memused);
      }
   }

   // TEST memalloc_threadlocalstore: ENOMEM (bytesize > available)
   GETBUFFER_ERRLOG(&logbuf1, &logsize1);
   mblock = (memblock_t) memblock_FREE;
   for (size_t i = 0; i <= memsize; ++i) {
      tls->memused = i;
      TEST(ENOMEM == memalloc_threadlocalstore(tls, memsize-i+1, &mblock));
      // check parameter
      TEST( isfree_memblock(&mblock));
      // check tls
      TEST(memsize == tls->memsize);
      TEST(i == tls->memused);
      // check errlog
      GETBUFFER_ERRLOG(&logbuf2, &logsize2);
      TEST(logsize2 > logsize1);
      // reset
      TRUNCATEBUFFER_ERRLOG(logsize1);
   }

   // TEST memalloc_threadlocalstore: ENOMEM (alignedsize < bytesize)
   tls->memused = 0;
   TEST(ENOMEM == memalloc_threadlocalstore(tls, (size_t)-1, &mblock));
   // check parameter
   TEST( isfree_memblock(&mblock));
   // check tls
   TEST(memsize == tls->memsize);
   TEST(0 == tls->memused);

   // TEST memfree_threadlocalstore: mblock valid && isfree_memblock(&mblock)
   for (size_t u = 0; u <= memsize; ++u) {
      for (size_t s = u; s <= u; --s, s -= (s > 1000 ? 1000 : 0)) {
         size_t a = s % KONFIG_MEMALIGN ? s - s % KONFIG_MEMALIGN + KONFIG_MEMALIGN : s;
         if (a > u) continue;
         tls->memused = u;
         mblock = (memblock_t) memblock_INIT(s, tls->mem + u - a);
         for (int r = 0; r < 2; ++r) {
            TEST(0 == memfree_threadlocalstore(tls, &mblock));
            // check parameter
            TEST( isfree_memblock(&mblock));
            // check tls
            TEST(memsize == tls->memsize);
            TEST(u - a   == tls->memused);
         }
      }
   }

   // TEST memfree_threadlocalstore: EINVAL (alignedsize < mblock.size)
   tls->memused = memsize;
   mblock.addr = tls->mem + memsize + 1;
   mblock.size = (size_t) -1;
   TEST(EINVAL == memfree_threadlocalstore(tls, &mblock));
   TEST(! isfree_memblock(&mblock));

   // TEST memfree_threadlocalstore: EINVAL (alignedsize > memused)
   tls->memused = 31;
   mblock.addr = tls->mem;
   mblock.size = 32;
   TEST(EINVAL == memfree_threadlocalstore(tls, &mblock));
   TEST(! isfree_memblock(&mblock));

   // TEST memfree_threadlocalstore: EINVAL (addr wrong)
   for (unsigned i = 0; i <= 2; i +=2) {
      tls->memused = 128;
      mblock.addr = tls->mem + 128 - 32 -1 + i;
      mblock.size = 32;
      TEST(EINVAL == memfree_threadlocalstore(tls, &mblock));
      TEST(! isfree_memblock(&mblock));
   }

   // TEST sizestatic_threadlocalstore
   for (unsigned i = 0; i <= memsize; ++i) {
      tls->memused = i;
      TEST(i == sizestatic_threadlocalstore(tls));
   }

   // reset0
   TEST(0 == delete_threadlocalstore(&tls));

   return 0;
ONERR:
   delete_threadlocalstore(&tls);
   return EINVAL;
}

int unittest_platform_task_thread_localstore()
{
   int err;

   err = test_initfree();
   if (!err) err = test_query();
   if (!err) err = test_memory();

   return err;
}

#endif
