/* title: ThreadStack impl

   Implements <ThreadStack>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/platform/task/thread_stack.h
    Header file <ThreadStack>.

   file: C-kern/platform/Linux/task/thread_stack.c
    Implementation file <ThreadStack impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/platform/task/thread_stack.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/log/logbuffer.h"
#include "C-kern/api/io/log/logwriter.h"
#include "C-kern/api/math/int/power2.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/vm.h"
#include "C-kern/api/platform/task/thread.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/io/pipe.h"
#endif



/* struct: thread_stack_t
 * Thread variables stored in thread local storage. */
struct thread_stack_t {
   // group: private variables
   /* variable: thread
    * Thread object itself. */
   thread_t          thread;
   /* variable: pagesize
    * Aligned size of thread stack. */
   size_t            pagesize;
   /* variable: memsize
    * Size of static memory (>= extsize_threadcontext()). */
   size_t            memsize;
   /* variable: memused
    * Nr of already allocated bytes of static memory. */
   size_t            memused;
   /* variable: mem
    * Static memory used in <init_threadcontext> to allocate additional memory. */
   uint8_t           mem[/*memsize*/];
};

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_threadstack_errtimer
 * Simulates an erwror in <new_threadstack> and <delete_threadstack>. */
static test_errortimer_t   s_threadstack_errtimer = test_errortimer_FREE;
#endif

// group: helper

static inline size_t compute_memsize(size_t sizevars)
{
   return sizevars - offsetof(thread_stack_t, mem);
}

static inline size_t get_sizevars(const thread_stack_t* st)
{
   return st->memsize + offsetof(thread_stack_t, mem);
}

static inline size_t get_pagesize(const thread_stack_t* st)
{
   return st->pagesize;
}

/* function: compute_signalstacksize
 * Returns the minimum size of the signal stack.
 * The returned value is a multiple of pagesize. */
static inline size_t compute_signalstacksize(const size_t pagesize)
{
   static_assert(MINSIGSTKSZ < size_threadstack(), "size_threadstack is big enough");
   return alignpower2_int((size_t)MINSIGSTKSZ, pagesize);
}

/* function: compute_stacksize
 * Returns the default size of the thread stack.
 * The returned value is a multiple of pagesize. */
static inline size_t compute_stacksize(const size_t pagesize)
{
   static_assert(256*1024 < size_threadstack(), "size_threadstack is big enough");
   return alignpower2_int((size_t)256*1024, pagesize);
}

/* function: compute_sizevars
 * Returns the size of all local thread variables on the stack.
 * The returned value is a multiple of pagesize. */
static inline size_t compute_sizevars(const size_t static_size, const size_t pagesize)
{
   return alignpower2_int( sizeof(thread_stack_t) + static_size, pagesize);
}

// group: lifetime

static void init_threadstack(/*out*/thread_stack_t* st, size_t sizevars, size_t pagesize)
{
   st->thread = (thread_t) thread_FREE;
   st->pagesize = pagesize;
   st->memsize = compute_memsize(sizevars);
   st->memused = 0;
}

static void free_threadstack(thread_stack_t* st)
{
   (void)st; // do nothing
}

int new_threadstack(/*out*/thread_stack_t** st, ilog_t* initlog, const size_t static_size, /*out*/struct memblock_t* threadstack, /*out*/struct memblock_t* signalstack)
{
   int err;
   void * addr = MAP_FAILED;
   size_t pagesize  = sys_pagesize_vm(); // function also called during initialization (syscontext_t not accessible)
   size_t sizevars  = compute_sizevars(static_size, pagesize);
   size_t sizesigst = compute_signalstacksize(pagesize);
   size_t sizestack = compute_stacksize(pagesize);
   size_t minsize   = 3 * pagesize /* 3 protection pages around two stacks */
                      + sizevars + sizesigst + sizestack;

   if (PROCESS_testerrortimer(&s_threadstack_errtimer, &err)) {
      minsize = size_threadstack() + 1;
   }

   if (minsize > size_threadstack() || static_size > UINT16_MAX) {
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
    * high:  == low + size_threadstack()
    */

   size_t size = 2*size_threadstack();
   addr = mmap(0, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
   if (PROCESS_testerrortimer(&s_threadstack_errtimer, &err)) {
      munmap(addr, size);
      errno = err;
      addr = MAP_FAILED;
   }
   if (addr == MAP_FAILED) {
      err = errno;
      TRACE_LOG(initlog, log_channel_ERR, log_flags_NONE, FUNCTION_SYSCALL_ERRLOG, "mmap", err);
      goto ONERR;
   }

   if (PROCESS_testerrortimer(&s_threadstack_errtimer, &err)) goto ONERR;
   {
      uint8_t* aligned_addr = (uint8_t*) (
                                 ((uintptr_t)addr + size_threadstack()-1)
                                 & ~(uintptr_t)(size_threadstack()-1) );

      if ((uint8_t*)addr < aligned_addr) {
         size_t dsize = (size_t) (aligned_addr - (uint8_t*)addr);
         if (munmap(addr, dsize)) {
            err = errno;
            TRACE_LOG(initlog, log_channel_ERR, log_flags_NONE, FUNCTION_SYSCALL_ERRLOG, "munmap", err);
            goto ONERR;
         }
         size -= dsize;
         addr  = aligned_addr;
      }
   }

   if (PROCESS_testerrortimer(&s_threadstack_errtimer, &err)) goto ONERR;
   if (size > size_threadstack()) {
      if (munmap((uint8_t*)addr + size_threadstack(), size - size_threadstack())) {
         err = errno;
         TRACE_LOG(initlog, log_channel_ERR, log_flags_NONE, FUNCTION_SYSCALL_ERRLOG, "munmap", err);
         goto ONERR;
      }
      size = size_threadstack();
   }

   size_t offset = sizevars/*thread local vars*/;
   if (PROCESS_testerrortimer(&s_threadstack_errtimer, &err)) {
      errno = err;
   } else {
      err = mprotect((uint8_t*)addr + offset, pagesize, PROT_NONE);
   }
   if (err) {
      err = errno;
      TRACE_LOG(initlog, log_channel_ERR, log_flags_NONE, FUNCTION_SYSCALL_ERRLOG, "mprotect", err);
      goto ONERR;
   }

   offset += pagesize/*protection*/ + sizesigst/*signal stack*/;
   if (PROCESS_testerrortimer(&s_threadstack_errtimer, &err)) {
      errno = err;
   } else {
      err = mprotect((uint8_t*)addr + offset, pagesize, PROT_NONE);
   }
   if (err) {
      err = errno;
      TRACE_LOG(initlog, log_channel_ERR, log_flags_NONE, FUNCTION_SYSCALL_ERRLOG, "mprotect", err);
      goto ONERR;
   }

   offset += pagesize/*protection*/ + sizestack/*thread stack*/;
   if (PROCESS_testerrortimer(&s_threadstack_errtimer, &err)) {
      errno = err;
   } else {
      err = mprotect((uint8_t*)addr + offset, size_threadstack() - offset, PROT_NONE);
   }
   if (err) {
      err = errno;
      TRACE_LOG(initlog, log_channel_ERR, log_flags_NONE, FUNCTION_SYSCALL_ERRLOG, "mprotect", err);
      goto ONERR;
   }

   static_assert(
      (uintptr_t) context_threadstack(0)   == offsetof(thread_t, threadcontext)
      && (uintptr_t) thread_threadstack(0) == offsetof(thread_stack_t, thread),
      "query functions use offset corresponding to struct thread_stack_t"
   );

   init_threadstack((thread_stack_t*)addr, sizevars, pagesize);

   // set out param
   if (threadstack) {
      *threadstack = (memblock_t) memblock_INIT(sizestack, (uint8_t*)addr + offset - sizestack);
   }

   if (signalstack) {
      *signalstack = (memblock_t) memblock_INIT(sizesigst, (uint8_t*)addr + sizevars + pagesize);
   }

   *st = (thread_stack_t*) addr;

   return 0;
ONERR:
   if (addr != MAP_FAILED) {
      munmap(addr, size);
   }
   TRACE_LOG(initlog, log_channel_ERR, log_flags_LAST, FUNCTION_EXIT_ERRLOG, err);
   return err;
}

int delete_threadstack(thread_stack_t** st, ilog_t* initlog)
{
   int err;

   if (*st) {

      free_threadstack(*st);

      err = munmap((void*)*st, size_threadstack());
      if (PROCESS_testerrortimer(&s_threadstack_errtimer, &err)) {
         errno = err;
      }
      if (err) {
         err = errno;
         TRACE_LOG(initlog, log_channel_ERR, log_flags_NONE, FUNCTION_SYSCALL_ERRLOG, "munmap", err);
      }

      *st = 0;

      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACE_LOG(initlog, log_channel_ERR, log_flags_LAST, FUNCTION_EXIT_FREE_RESOURCE_ERRLOG, err);
   return err;
}

// group: query

struct memblock_t signalstack_threadstack(thread_stack_t* st)
{
   size_t pagesize  = get_pagesize(st);
   size_t offset    = get_sizevars(st) + pagesize;
   size_t sizesigst = compute_signalstacksize(pagesize);
   return (memblock_t) memblock_INIT(sizesigst, (uint8_t*)st + offset);
}

struct memblock_t threadstack_threadstack(thread_stack_t* st)
{
   size_t pagesize  = get_pagesize(st);
   size_t offset    = get_sizevars(st) + compute_signalstacksize(pagesize) + 2*pagesize;
   size_t sizestack = compute_stacksize(pagesize);
   return (memblock_t) memblock_INIT(sizestack, (uint8_t*)st + offset);
}

// group: static-memory

int allocstatic_threadstack(thread_stack_t* st, ilog_t* initlog, size_t bytesize, /*out*/struct memblock_t* memblock)
{
   int err;
   size_t alignedsize = (bytesize + KONFIG_MEMALIGN-1u) & (~(KONFIG_MEMALIGN-1u));

   if (  alignedsize < bytesize
         || alignedsize > st->memsize - st->memused) {
      err = ENOMEM;
      goto ONERR;
   }

   memblock->addr = st->mem + st->memused;
   memblock->size = alignedsize;

   st->memused += alignedsize;

   return 0;
ONERR:
   TRACE_LOG(initlog, log_channel_ERR, log_flags_LAST, FUNCTION_EXIT_ERRLOG, err);
   return err;
}

int freestatic_threadstack(thread_stack_t* st, ilog_t* initlog, struct memblock_t* memblock)
{
   int err;

   if (!isfree_memblock(memblock)) {
      size_t alignedsize = alignpower2_int(memblock->size, KONFIG_MEMALIGN);
      uint8_t*    memend = (st->mem + st->memused);

      VALIDATE_INPARAM_TEST(  alignedsize >= memblock->size
                              && alignedsize <= st->memused, ONERR, );
      if (memblock->addr != memend - alignedsize) {
         err = EMEMLEAK;
         goto ONERR;
      }

      st->memused -= alignedsize;

      *memblock = (memblock_t) memblock_FREE;
   }

   return 0;
ONERR:
   TRACE_LOG(initlog, log_channel_ERR, log_flags_LAST, FUNCTION_EXIT_FREE_RESOURCE_ERRLOG, err);
   return err;
}

size_t sizestatic_threadstack(const thread_stack_t* st)
{
   return st->memused;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   thread_stack_t*  st = 0;
   memblock_t     threadstack = memblock_FREE;
   memblock_t     signalstack = memblock_FREE;
   vmpage_t       vmpage;
   uint8_t *      logbuffer;
   size_t         logsize;
   size_t         oldlogsize;
   ilog_t       * defaultlog = GETWRITER0_LOG();

   const size_t test_static_size[] = { 0, extsize_threadcontext() + extsize_maincontext(), 12345, 65535 };
   for (unsigned si=0; si<lengthof(test_static_size); ++si) {
      const size_t static_size = test_static_size[si];
      const size_t sizevars = compute_sizevars(static_size, pagesize_vm());
      // prepare
      GETBUFFER_ERRLOG(&logbuffer, &oldlogsize);

      // TEST new_threadstack
      TEST(0 == new_threadstack(&st, defaultlog, static_size, 0, 0));
      // check st aligned
      TEST(0 != st);
      TEST(0 == (uintptr_t)st % size_threadstack());
      // check *st
      TEST( st->pagesize == sys_pagesize_vm());
      TEST( memcmp(&st->thread, &(thread_t)thread_FREE, sizeof(st->thread)) == 0);
      TEST( st->memsize == compute_memsize(sizevars));
      TEST( st->memused == 0);

      // TEST delete_threadstack
      TEST(0 == delete_threadstack(&st, defaultlog));
      TEST(0 == st);
      TEST(0 == delete_threadstack(&st, defaultlog));
      TEST(0 == st);

      // TEST new_threadstack: correct protection
      TEST(0 == new_threadstack(&st, defaultlog, static_size, &threadstack, &signalstack));
      // variables
      vmpage = (vmpage_t) vmpage_INIT(sizevars, (uint8_t*)st);
      TEST(1 == ismapped_vm(&vmpage, accessmode_RDWR));
      // protection page
      vmpage = (vmpage_t) vmpage_INIT(pagesize_vm(), (uint8_t*)st + sizevars);
      TEST(1 == ismapped_vm(&vmpage, accessmode_NONE));
      // signal stack page
      vmpage = (vmpage_t) vmpage_INIT(compute_signalstacksize(pagesize_vm()), (uint8_t*)st + sizevars + 1 * pagesize_vm());
      TEST(1 == ismapped_vm(&vmpage, accessmode_RDWR));
      // check parameter signalstack
      TEST(vmpage.addr == signalstack.addr);
      TEST(vmpage.size == signalstack.size);
      // protection page
      vmpage = (vmpage_t) vmpage_INIT(pagesize_vm(), (uint8_t*)st + sizevars + compute_signalstacksize(pagesize_vm()) + 1 * pagesize_vm());
      TEST(1 == ismapped_vm(&vmpage, accessmode_NONE));
      // thread stack page
      vmpage = (vmpage_t) vmpage_INIT(compute_stacksize(pagesize_vm()), (uint8_t*)st + sizevars + compute_signalstacksize(pagesize_vm()) + 2 * pagesize_vm());
      TEST(1 == ismapped_vm(&vmpage, accessmode_RDWR));
      // check parameter threadstack
      TEST(vmpage.addr == threadstack.addr);
      TEST(vmpage.size == threadstack.size);
      // protection page
      size_t offset = sizevars + compute_signalstacksize(pagesize_vm()) + compute_stacksize(pagesize_vm()) + 2 * pagesize_vm();
      vmpage = (vmpage_t) vmpage_INIT(size_threadstack() - offset, (uint8_t*)st + offset);
      TEST(1 == ismapped_vm(&vmpage, accessmode_NONE));

      // TEST delete_threadstack: unmap pages
      vmpage = (vmpage_t) vmpage_INIT(size_threadstack(), (uint8_t*)st);
      TEST(0 == delete_threadstack(&st, defaultlog));
      TEST(1 == isunmapped_vm(&vmpage));

      // TEST new_threadstack: ERROR
      threadstack = (memblock_t) memblock_FREE;
      signalstack = (memblock_t) memblock_FREE;
      for (int i = 1; i; ++i) {
         init_testerrortimer(&s_threadstack_errtimer, (unsigned)i, i);
         int err = new_threadstack(&st, defaultlog, static_size, &threadstack, &signalstack);
         if (!err) {
            TEST(8 == i);
            break;
         }
         TEST(err == (i == 1 ? ENOSPC : i));
         // check parameter
         TEST(0 == st);
         TEST( isfree_memblock(&threadstack));
         TEST( isfree_memblock(&signalstack));
      }

      // TEST delete_threadstack: ERROR
      TEST(0 != st);
      init_testerrortimer(&s_threadstack_errtimer, 1, EINVAL);
      TEST(EINVAL == delete_threadstack(&st, defaultlog));
      // check param st
      TEST(0 == st);

      // ERRLOG used
      GETBUFFER_ERRLOG(&logbuffer, &logsize);
      TEST(logsize > oldlogsize);
   }

   return 0;
ONERR:
   if (st) delete_threadstack(&st, defaultlog);
   return EINVAL;
}

static int test_query(void)
{
   thread_stack_t st  = { .pagesize = 0 };
   thread_stack_t st2 = { .pagesize = 0 };
   const size_t   MINSIZE = 3*pagesize_vm() + compute_signalstacksize(pagesize_vm()) + compute_stacksize(pagesize_vm()) + compute_sizevars(0, pagesize_vm());
   memblock_t     stackmem;

   // TEST compute_signalstacksize
   TEST( MINSIGSTKSZ <= compute_signalstacksize(pagesize_vm()));
   TEST( 0 == compute_signalstacksize(pagesize_vm()) % pagesize_vm());

   // TEST compute_stacksize
   TEST( PTHREAD_STACK_MIN <= compute_stacksize(pagesize_vm()));
   TEST( 0 == compute_stacksize(pagesize_vm()) % pagesize_vm());

   for (unsigned static_size=0; static_size < 70000; static_size += 1600) {
      // TEST compute_sizevars
      size_t sizevars = compute_sizevars(static_size, sys_pagesize_vm());
      TEST( sizevars == compute_sizevars(static_size, pagesize_vm()));
      TEST( sizevars >= sizeof(thread_stack_t) + static_size);
      TEST( 0 == sizevars % pagesize_vm());

      // TEST compute_memsize
      TEST( sizevars - offsetof(thread_stack_t, mem) == compute_memsize(sizevars));
   }

   // TEST get_pagesize
   for (unsigned i=0; i<20; ++i) {
      st.pagesize = i;
      TEST( i == get_pagesize(&st));
   }
   st.pagesize = pagesize_vm();
   TEST( pagesize_vm() == get_pagesize(&st));

   for (unsigned static_size=0; static_size < 70000; static_size += 1600) {
      size_t sizevars = compute_sizevars(static_size, pagesize_vm());
      st.pagesize = pagesize_vm();
      st.memsize  = compute_memsize(sizevars);

      // TEST get_sizevars
      TEST( sizevars == get_sizevars(&st));

      // TEST signalstack_threadstack
      stackmem = signalstack_threadstack(&st);
      TEST(stackmem.addr == (uint8_t*)&st + sizevars + pagesize_vm());
      TEST(stackmem.size == compute_signalstacksize(pagesize_vm()));

      // TEST threadstack_threadstack
      stackmem = threadstack_threadstack(&st);
      TEST(stackmem.addr == (uint8_t*)&st + sizevars + compute_signalstacksize(pagesize_vm()) + 2*pagesize_vm());
      TEST(stackmem.size == compute_stacksize(pagesize_vm()));
   }

   // TEST size_threadstack
   TEST(0 == size_threadstack() % pagesize_vm());
   TEST(size_threadstack()/2 < MINSIZE);
   TEST(size_threadstack()  >= MINSIZE);
   TEST(size_threadstack()  == stacksize_syscontext());

   // TEST self_threadstack
   TEST( self_threadstack() == (thread_stack_t*) (((uintptr_t)&st) - ((uintptr_t)&st) % size_threadstack()));
   TEST( self_threadstack() == (thread_stack_t*) context_syscontext());

   // TEST castPcontext_threadstack
   TEST(&st == castPcontext_threadstack(&st.thread.threadcontext));
   TEST(&st2 == castPcontext_threadstack(&st2.thread.threadcontext));

   // TEST castPthread_threadstack
   TEST(&st == castPthread_threadstack(&st.thread));
   TEST(&st2 == castPthread_threadstack(&st2.thread));

   // TEST thread_threadstack
   TEST( thread_threadstack(&st) == &st.thread);
   TEST( thread_threadstack(&st2) == &st2.thread);
   TEST( thread_threadstack((thread_stack_t*)0) == (thread_t*) 0);

   // TEST context_threadstack
   TEST( context_threadstack(&st) == &st.thread.threadcontext);
   TEST( context_threadstack(&st2) == &st2.thread.threadcontext);
   TEST( context_threadstack((thread_stack_t*)0) == (threadcontext_t*) 0);

   return 0;
ONERR:
   return EINVAL;
}

static int test_memory(void)
{
   thread_stack_t* st = 0;
   memblock_t mblock;
   size_t     logsize1;
   size_t     logsize2;
   uint8_t *  logbuf1;
   uint8_t *  logbuf2;
   ilog_t  *  defaultlog = GETWRITER0_LOG();

   // prepare0
   TEST(0 == new_threadstack(&st, defaultlog, 2012, 0, 0));
   const size_t memsize = st->memsize;

   // TEST allocstatic_threadstack
   for (size_t u = 0; u <= memsize; ++u) {
      for (size_t s = memsize-u; s <= memsize-u; --s, s -= (s > 1000 ? 1000 : 0)) {
         size_t a = s % KONFIG_MEMALIGN ? s - s % KONFIG_MEMALIGN + KONFIG_MEMALIGN : s;
         if (a > memsize - u) continue;
         st->memused = u;
         TEST(0 == allocstatic_threadstack(st, defaultlog, s, &mblock));
         // check parameter
         TEST(mblock.addr == st->mem + u);
         TEST(mblock.size == a);
         // check st
         TEST(memsize == st->memsize);
         TEST(u + a   == st->memused);
      }
   }

   // TEST allocstatic_threadstack: ENOMEM (bytesize > available)
   GETBUFFER_ERRLOG(&logbuf1, &logsize1);
   mblock = (memblock_t) memblock_FREE;
   for (size_t i = 0; i <= memsize; ++i) {
      st->memused = i;
      TEST(ENOMEM == allocstatic_threadstack(st, defaultlog, memsize-i+1, &mblock));
      // check parameter
      TEST( isfree_memblock(&mblock));
      // check st
      TEST(memsize == st->memsize);
      TEST(i == st->memused);
      // check errlog
      GETBUFFER_ERRLOG(&logbuf2, &logsize2);
      TEST(logsize2 > logsize1);
      // reset
      TRUNCATEBUFFER_ERRLOG(logsize1);
   }

   // TEST allocstatic_threadstack: ENOMEM (alignedsize < bytesize)
   st->memused = 0;
   TEST(ENOMEM == allocstatic_threadstack(st, defaultlog, (size_t)-1, &mblock));
   // check parameter
   TEST( isfree_memblock(&mblock));
   // check st
   TEST(memsize == st->memsize);
   TEST(0 == st->memused);

   // TEST freestatic_threadstack: mblock valid && isfree_memblock(&mblock)
   for (size_t u = 0; u <= memsize; ++u) {
      for (size_t s = u; s <= u; --s, s -= (s > 1000 ? 1000 : 0)) {
         size_t a = s % KONFIG_MEMALIGN ? s - s % KONFIG_MEMALIGN + KONFIG_MEMALIGN : s;
         if (a > u) continue;
         st->memused = u;
         mblock = (memblock_t) memblock_INIT(s, st->mem + u - a);
         for (int r = 0; r < 2; ++r) {
            TEST(0 == freestatic_threadstack(st, defaultlog, &mblock));
            // check parameter
            TEST( isfree_memblock(&mblock));
            // check st
            TEST(memsize == st->memsize);
            TEST(u - a   == st->memused);
         }
      }
   }

   // TEST freestatic_threadstack: EINVAL (alignedsize < mblock.size)
   st->memused = memsize;
   mblock.addr = st->mem + memsize + 1;
   mblock.size = (size_t) -1;
   TEST(EINVAL == freestatic_threadstack(st, defaultlog, &mblock));
   TEST(! isfree_memblock(&mblock));

   // TEST freestatic_threadstack: EINVAL (alignedsize > memused)
   st->memused = 31;
   mblock.addr = st->mem;
   mblock.size = 32;
   TEST(EINVAL == freestatic_threadstack(st, defaultlog, &mblock));
   TEST(! isfree_memblock(&mblock));

   // TEST freestatic_threadstack: EMEMLEAK (addr wrong)
   for (unsigned i = 0; i <= 2; i +=2) {
      st->memused = 128;
      mblock.addr = st->mem + 128 - 32 -1 + i;
      mblock.size = 32;
      TEST(EMEMLEAK == freestatic_threadstack(st, defaultlog, &mblock));
      TEST(! isfree_memblock(&mblock));
   }

   // TEST sizestatic_threadstack
   for (unsigned i = 0; i <= memsize; ++i) {
      st->memused = i;
      TEST(i == sizestatic_threadstack(st));
   }

   // reset0
   TEST(0 == delete_threadstack(&st, defaultlog));

   return 0;
ONERR:
   delete_threadstack(&st, defaultlog);
   return EINVAL;
}

int unittest_platform_task_thread_stack()
{
   int err;

   err = test_initfree();
   if (!err) err = test_query();
   if (!err) err = test_memory();

   return err;
}

#endif
