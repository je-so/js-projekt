/* title: ThreadLocalStorage impl

   Implements <ThreadLocalStorage>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/platform/task/thread_tls.h
    Header file <ThreadLocalStorage>.

   file: C-kern/platform/Linux/task/thread_tls.c
    Implementation file <ThreadLocalStorage impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/platform/task/thread_tls.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/writer/log/logmain.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/vm.h"
#include "C-kern/api/platform/task/thread.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif


/* struct: thread_tls_t
 * Thread variables stored in thread local storage. */
struct thread_tls_t {
   // group: private variables
   /* variable: threadcontext
    * Context of <thread_t>. */
   threadcontext_t   threadcontext;
   /* variable: thread
    * Thread object itself. */
   thread_t          thread;
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

// group: constants

/* define: thread_tls_INIT_STATIC
 * Static initializer. Used to initialize all variables of thread locval storage. */
#define thread_tls_INIT_STATIC(sizevars) \
         {  threadcontext_INIT_STATIC, thread_FREE, (sizevars) - offsetof(thread_tls_t, mem), 0 }

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_threadtls_errtimer
 * Simulates an error in <new_threadtls> and <delete_threadtls>. */
static test_errortimer_t   s_threadtls_errtimer = test_errortimer_FREE;
#endif

// group: helper

/* function: sizesignalstack_threadtls
 * Returns the minimum size of the signal stack.
 * The returned value is a multiple of pagesize. */
static inline size_t sizesignalstack_threadtls(const size_t pagesize)
{
   return (MINSIGSTKSZ + pagesize - 1) & (~(pagesize-1));
}

/* function: sizestack_threadtls
 * Returns the default size of the thread stack.
 * The returned value is a multiple of pagesize. */
static inline size_t sizestack_threadtls(const size_t pagesize)
{
   static_assert(256*1024 < size_threadtls(), "sys_size_threadtls/size_threadtls is big enough");
   return (256*1024 + pagesize - 1) & (~(pagesize-1));
}

/* function: sizevars_threadtls
 * Returns the size of all local thread variables on the stack.
 * The returned value is a multiple of pagesize. */
static inline size_t sizevars_threadtls(const size_t pagesize)
{
   // OPTIMIZE: use extsize_processcontext only if thread_threadtls(tls) is main thread
   return (sizeof(thread_tls_t) + extsize_processcontext() + extsize_threadcontext() + pagesize - 1) & (~(pagesize-1));
}

// group: lifetime

static int sysnew_threadtls(/*out*/thread_tls_t** tls, size_t pagesize, /*out*/memblock_t* threadstack, /*out*/memblock_t* signalstack)
{
   int err;
   size_t sizevars  = sizevars_threadtls(pagesize);
   size_t sizesigst = sizesignalstack_threadtls(pagesize);
   size_t sizestack = sizestack_threadtls(pagesize);
   size_t minsize   = 3 * pagesize /* 3 protection pages around two stacks */
                      + sizevars + sizesigst + sizestack;

   if (minsize > size_threadtls()) {
      return ENOMEM;
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
    * high:  == low + size_threadtls()
    */

   size_t size = 2*size_threadtls() - pagesize;
   void * addr;
   if (PROCESS_testerrortimer(&s_threadtls_errtimer)) {
      addr = MAP_FAILED;
      err = ERRCODE_testerrortimer(&s_threadtls_errtimer);
      goto ONERR;
   }
   addr = mmap(0, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
   if (addr == MAP_FAILED) {
      err = errno;
      goto ONERR;
   }

   ONERROR_testerrortimer(&s_threadtls_errtimer, &err, ONERR);
   {
      uint8_t* aligned_addr = (uint8_t*) (
                                 ((uintptr_t)addr + size_threadtls()-1)
                                 & ~(uintptr_t)(size_threadtls()-1) );

      if ((uint8_t*)addr < aligned_addr) {
         size_t dsize = (size_t) (aligned_addr - (uint8_t*)addr);
         if (munmap(addr, dsize)) {
            err = errno;
            goto ONERR;
         }
         size -= dsize;
         addr  = aligned_addr;
      }
   }

   ONERROR_testerrortimer(&s_threadtls_errtimer, &err, ONERR);
   if (size > size_threadtls()) {
      if (munmap((uint8_t*)addr + size_threadtls(), size - size_threadtls())) {
         err = errno;
         goto ONERR;
      }
      size = size_threadtls();
   }

   ONERROR_testerrortimer(&s_threadtls_errtimer, &err, ONERR);
   size_t offset = sizevars/*thread local vars*/;
   if (mprotect((uint8_t*)addr + offset, pagesize, PROT_NONE)) {
      err = errno;
      goto ONERR;
   }

   ONERROR_testerrortimer(&s_threadtls_errtimer, &err, ONERR);
   offset += pagesize/*protection*/ + sizesigst/*signal stack*/;
   if (mprotect((uint8_t*)addr + offset, pagesize, PROT_NONE)) {
      err = errno;
      goto ONERR;
   }

   ONERROR_testerrortimer(&s_threadtls_errtimer, &err, ONERR);
   offset += pagesize/*protection*/ + sizestack/*thread stack*/;
   if (mprotect((uint8_t*)addr + offset, size_threadtls() - offset, PROT_NONE)) {
      err = errno;
      goto ONERR;
   }

   // set out param
   if (threadstack) {
      *threadstack = (memblock_t) memblock_INIT(sizestack, (uint8_t*)addr + offset - sizestack);
   }

   if (signalstack) {
      *signalstack = (memblock_t) memblock_INIT(sizesigst, (uint8_t*)addr + sizevars + pagesize);
   }

   static_assert(
      (uintptr_t)context_threadtls(0)   == offsetof(thread_tls_t, threadcontext)
      && (uintptr_t)thread_threadtls(0) == offsetof(thread_tls_t, thread),
      "query functions use offset corresponding to struct thread_tls_t"
   );
   * (thread_tls_t*) addr = (thread_tls_t) thread_tls_INIT_STATIC(sizevars);
   *tls = (thread_tls_t*) addr;

   return 0;
ONERR:
   if (addr != MAP_FAILED) {
      munmap(addr, size);
   }
   return err;
}

static int sysdelete_threadtls(thread_tls_t** tls)
{
   int err;

   if (*tls) {
      err = munmap((void*)*tls, size_threadtls());
      if (err) err = errno;
      SETONERROR_testerrortimer(&s_threadtls_errtimer, &err);

      *tls = 0;

      if (err) goto ONERR;
   }

   return 0;
ONERR:
   return err;
}

int new_threadtls(/*out*/thread_tls_t** tls, /*out*/struct memblock_t* threadstack, /*out*/struct memblock_t* signalstack)
{
   int err;
   size_t   pagesize  = pagesize_vm();

   err = sysnew_threadtls(tls, pagesize, threadstack, signalstack);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int newmain_threadtls(/*out*/thread_tls_t** tls, /*out*/struct memblock_t* threadstack, /*out*/struct memblock_t* signalstack)
{
   int err;
   size_t   pagesize  = sys_pagesize_vm();

   err = sysnew_threadtls(tls, pagesize, threadstack, signalstack);
   if (err) goto ONERR;

   return 0;
ONERR:
   return err;
}

int delete_threadtls(thread_tls_t** tls)
{
   int err;

   err = sysdelete_threadtls(tls);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

int deletemain_threadtls(thread_tls_t** tls)
{
   int err;

   err = sysdelete_threadtls(tls);
   if (err) goto ONERR;

   return 0;
ONERR:
   return err;
}

// group: query

void signalstack_threadtls(thread_tls_t* tls,/*out*/struct memblock_t * stackmem)
{
   size_t pagesize  = pagesize_vm();
   size_t offset    = tls ? sizevars_threadtls(pagesize) + pagesize : 0;
   size_t sizesigst = tls ? sizesignalstack_threadtls(pagesize) : 0;
   *stackmem = (memblock_t) memblock_INIT(sizesigst, (uint8_t*)tls + offset);
}

/* function: threadstack_threadtls(const thread_tls_t * tls)
 * Returns the thread stack from tls. */
void threadstack_threadtls(thread_tls_t* tls,/*out*/struct memblock_t* stackmem)
{
   size_t pagesize  = pagesize_vm();
   size_t offset    = tls ? sizesignalstack_threadtls(pagesize) + sizevars_threadtls(pagesize) + 2*pagesize : 0;
   size_t sizestack = tls ? sizestack_threadtls(pagesize) : 0;
   *stackmem = (memblock_t) memblock_INIT(sizestack, (uint8_t*)tls + offset);
}

// group: static-memory

int allocstatic_threadtls(thread_tls_t* tls, size_t bytesize, /*out*/struct memblock_t* memblock)
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

int freestatic_threadtls(thread_tls_t* tls, struct memblock_t* memblock)
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

size_t sizestatic_threadtls(const thread_tls_t* tls)
{
   return tls->memused;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   thread_tls_t*  tls = 0;
   thread_tls_t   tls2;
   thread_t       thrfree = thread_FREE;
   memblock_t     threadstack = memblock_FREE;
   memblock_t     signalstack = memblock_FREE;
   vmpage_t       vmpage;

   // TEST thread_tls_INIT_STATIC
   for (size_t i = 0; i < 4096; i *= 2, i += (i==0)) {
      tls2 = (thread_tls_t) thread_tls_INIT_STATIC(sizeof(thread_tls_t) + i);
      TEST( isstatic_threadcontext(&tls2.threadcontext));
      TEST(0 == memcmp(&thrfree, &tls2.thread, sizeof(thread_t)));
      TEST(i == tls2.memsize);
      TEST(0 == tls2.memused);
   }

   // TEST new_threadtls
   TEST(0 == new_threadtls(&tls, 0, 0));
   // check tls aligned
   TEST(0 != tls);
   TEST(0 == (uintptr_t)tls % size_threadtls());
   // check *tls
   tls2 = (thread_tls_t) thread_tls_INIT_STATIC(sizevars_threadtls(pagesize_vm()));
   TEST(0 == memcmp(tls, &tls2, sizeof(thread_tls_t)));

   // TEST delete_threadtls
   TEST(0 == delete_threadtls(&tls));
   TEST(0 == tls);
   TEST(0 == delete_threadtls(&tls));
   TEST(0 == tls);

   // TEST new_threadtls: correct protection
   TEST(0 == new_threadtls(&tls, &threadstack, &signalstack));
   // variables
   vmpage = (vmpage_t) vmpage_INIT(sizevars_threadtls(pagesize_vm()), (uint8_t*)tls);
   TEST(1 == ismapped_vm(&vmpage, accessmode_RDWR_PRIVATE));
   // protection page
   vmpage = (vmpage_t) vmpage_INIT(pagesize_vm(), (uint8_t*)tls + sizevars_threadtls(pagesize_vm()));
   TEST(1 == ismapped_vm(&vmpage, accessmode_PRIVATE));
   // signal stack page
   vmpage = (vmpage_t) vmpage_INIT(sizesignalstack_threadtls(pagesize_vm()), (uint8_t*)tls + sizevars_threadtls(pagesize_vm()) + 1 * pagesize_vm());
   TEST(1 == ismapped_vm(&vmpage, accessmode_RDWR_PRIVATE));
   // check parameter signalstack
   TEST(vmpage.addr == signalstack.addr);
   TEST(vmpage.size == signalstack.size);
   // protection page
   vmpage = (vmpage_t) vmpage_INIT(pagesize_vm(), (uint8_t*)tls + sizevars_threadtls(pagesize_vm()) + sizesignalstack_threadtls(pagesize_vm()) + 1 * pagesize_vm());
   TEST(1 == ismapped_vm(&vmpage, accessmode_PRIVATE));
   // thread stack page
   vmpage = (vmpage_t) vmpage_INIT(sizestack_threadtls(pagesize_vm()), (uint8_t*)tls + sizevars_threadtls(pagesize_vm()) + sizesignalstack_threadtls(pagesize_vm()) + 2 * pagesize_vm());
   TEST(1 == ismapped_vm(&vmpage, accessmode_RDWR_PRIVATE));
   // check parameter threadstack
   TEST(vmpage.addr == threadstack.addr);
   TEST(vmpage.size == threadstack.size);
   // protection page
   size_t offset = sizevars_threadtls(pagesize_vm()) + sizesignalstack_threadtls(pagesize_vm()) + sizestack_threadtls(pagesize_vm()) + 2 * pagesize_vm();
   vmpage = (vmpage_t) vmpage_INIT(size_threadtls() - offset, (uint8_t*)tls + offset);
   TEST(1 == ismapped_vm(&vmpage, accessmode_PRIVATE));

   // TEST delete_threadtls: unmap pages
   vmpage = (vmpage_t) vmpage_INIT(size_threadtls(), (uint8_t*)tls);
   TEST(0 == delete_threadtls(&tls));
   TEST(1 == isunmapped_vm(&vmpage));

   // TEST new_threadtls: ERROR
   threadstack = (memblock_t) memblock_FREE;
   signalstack = (memblock_t) memblock_FREE;
   for (unsigned i = 1; i <= 4; ++i) {
      init_testerrortimer(&s_threadtls_errtimer, i, ENOMEM);
      TEST(ENOMEM == new_threadtls(&tls, &threadstack, &signalstack));
      // check parameter
      TEST(0 == tls);
      TEST( isfree_memblock(&threadstack));
      TEST( isfree_memblock(&signalstack));
   }

   // TEST delete_threadtls: ERROR
   TEST(0 == new_threadtls(&tls, 0, 0));
   init_testerrortimer(&s_threadtls_errtimer, 1, EINVAL);
   TEST(EINVAL == delete_threadtls(&tls));
   // check param tls
   TEST(0 == tls);

   return 0;
ONERR:
   delete_threadtls(&tls);
   return EINVAL;
}

static int test_initmain(void)
{
   thread_tls_t*  tls = 0;
   thread_tls_t   tls2;
   memblock_t     threadstack = memblock_FREE;
   memblock_t     signalstack = memblock_FREE;
   vmpage_t       vmpage;

   // TEST newmain_threadtls
   TEST(0 == newmain_threadtls(&tls, &threadstack, &signalstack));
   // check tls aligned
   TEST(0 != tls);
   TEST(0 == (uintptr_t)tls % size_threadtls());
   // check *tls
   tls2 = (thread_tls_t) thread_tls_INIT_STATIC(sizevars_threadtls(pagesize_vm()));
   TEST(0 == memcmp(tls, &tls2, sizeof(thread_tls_t)));
   // check other parameter
   TEST(threadstack.addr == (uint8_t*)tls + sizevars_threadtls(pagesize_vm()) + sizesignalstack_threadtls(pagesize_vm()) + 2 * pagesize_vm());
   TEST(threadstack.size == sizestack_threadtls(pagesize_vm()));
   TEST(signalstack.addr == (uint8_t*)tls + sizevars_threadtls(pagesize_vm()) + pagesize_vm());
   TEST(signalstack.size == sizesignalstack_threadtls(pagesize_vm()));

   // TEST deletemain_threadtls
   TEST(0 == deletemain_threadtls(&tls));
   TEST(0 == tls);
   TEST(0 == deletemain_threadtls(&tls));
   TEST(0 == tls);

   // TEST newmain_threadtls: correct protection
   TEST(0 == newmain_threadtls(&tls, &threadstack, &signalstack));
   // variables
   vmpage = (vmpage_t) vmpage_INIT(sizevars_threadtls(pagesize_vm()), (uint8_t*)tls);
   TEST(1 == ismapped_vm(&vmpage, accessmode_RDWR_PRIVATE));
   // protection page
   vmpage = (vmpage_t) vmpage_INIT(pagesize_vm(), (uint8_t*)tls + sizevars_threadtls(pagesize_vm()));
   TEST(1 == ismapped_vm(&vmpage, accessmode_PRIVATE));
   // signal stack page
   vmpage = (vmpage_t) vmpage_INIT(sizesignalstack_threadtls(pagesize_vm()), (uint8_t*)tls + sizevars_threadtls(pagesize_vm()) + 1 * pagesize_vm());
   TEST(1 == ismapped_vm(&vmpage, accessmode_RDWR_PRIVATE));
   TEST(vmpage.addr == signalstack.addr);
   TEST(vmpage.size == signalstack.size);
   // protection page
   vmpage = (vmpage_t) vmpage_INIT(pagesize_vm(), (uint8_t*)tls + sizevars_threadtls(pagesize_vm()) + sizesignalstack_threadtls(pagesize_vm()) + 1 * pagesize_vm());
   TEST(1 == ismapped_vm(&vmpage, accessmode_PRIVATE));
   // thread stack page
   vmpage = (vmpage_t) vmpage_INIT(sizestack_threadtls(pagesize_vm()), (uint8_t*)tls + sizevars_threadtls(pagesize_vm()) + sizesignalstack_threadtls(pagesize_vm()) + 2 * pagesize_vm());
   TEST(1 == ismapped_vm(&vmpage, accessmode_RDWR_PRIVATE));
   TEST(vmpage.addr == threadstack.addr);
   TEST(vmpage.size == threadstack.size);
   // protection page
   size_t offset = sizevars_threadtls(pagesize_vm()) + sizesignalstack_threadtls(pagesize_vm()) + sizestack_threadtls(pagesize_vm()) + 2 * pagesize_vm();
   vmpage = (vmpage_t) vmpage_INIT(size_threadtls() - offset, (uint8_t*)tls + offset);
   TEST(1 == ismapped_vm(&vmpage, accessmode_PRIVATE));

   // TEST deletemain_threadtls: unmap pages
   vmpage = (vmpage_t) vmpage_INIT(size_threadtls(), (uint8_t*)tls);
   TEST(0 == deletemain_threadtls(&tls));
   TEST(1 == isunmapped_vm(&vmpage));

   // TEST newmain_threadtls: ERROR
   threadstack = (memblock_t) memblock_FREE;
   signalstack = (memblock_t) memblock_FREE;
   for (unsigned i = 1; i <= 6; ++i) {
      init_testerrortimer(&s_threadtls_errtimer, i, ENOMEM);
      TEST(ENOMEM == newmain_threadtls(&tls, &threadstack, &signalstack));
      TEST(0 == tls);
      TEST(1 == isfree_memblock(&threadstack));
      TEST(1 == isfree_memblock(&signalstack));
   }

   // TEST deletemain_threadtls: ERROR
   TEST(0 == newmain_threadtls(&tls, 0, 0));
   init_testerrortimer(&s_threadtls_errtimer, 1, EINVAL);
   TEST(EINVAL == deletemain_threadtls(&tls));
   TEST(0 == tls);

   return 0;
ONERR:
   delete_threadtls(&tls);
   return EINVAL;
}

static int test_query(void)
{
   thread_tls_t*  tls = 0;
   memblock_t     stackmem;

   // TEST sizesignalstack_threadtls
   TEST(MINSIGSTKSZ <= sizesignalstack_threadtls(pagesize_vm()));
   TEST(0 == sizesignalstack_threadtls(pagesize_vm()) % pagesize_vm());

   // TEST sizestack_threadtls
   TEST(PTHREAD_STACK_MIN <= sizestack_threadtls(pagesize_vm()));
   TEST(0 == sizestack_threadtls(pagesize_vm()) % pagesize_vm());

   // TEST sizevars_threadtls
   TEST(sizevars_threadtls(pagesize_vm()) >= sizeof(thread_tls_t) + extsize_threadcontext() + extsize_processcontext());
   TEST(0 == sizevars_threadtls(pagesize_vm()) % pagesize_vm());

   // TEST size_threadtls
   TEST(0 == size_threadtls() % pagesize_vm());
   size_t minsize = 3*pagesize_vm() + sizesignalstack_threadtls(pagesize_vm()) + sizestack_threadtls(pagesize_vm()) + sizevars_threadtls(pagesize_vm());
   TEST(size_threadtls()/2 < minsize);
   TEST(size_threadtls()  >= minsize);

   // TEST signalstack_threadtls
   TEST(0 == new_threadtls(&tls, 0, 0));
   signalstack_threadtls(tls, &stackmem);
   TEST(stackmem.addr == (uint8_t*)tls + sizevars_threadtls(pagesize_vm()) + pagesize_vm());
   TEST(stackmem.size == sizesignalstack_threadtls(pagesize_vm()));
   TEST(0 == delete_threadtls(&tls));

   // TEST signalstack_threadtls: tls == 0
   signalstack_threadtls(tls, &stackmem);
   TEST(1 == isfree_memblock(&stackmem));

   // TEST threadstack_threadtls
   TEST(0 == new_threadtls(&tls, 0, 0));
   threadstack_threadtls(tls, &stackmem);
   TEST(stackmem.addr == (uint8_t*)tls + sizevars_threadtls(pagesize_vm()) + sizesignalstack_threadtls(pagesize_vm()) + 2*pagesize_vm());
   TEST(stackmem.size == sizestack_threadtls(pagesize_vm()));
   TEST(0 == delete_threadtls(&tls));

   // TEST threadstack_threadtls: tls == 0
   threadstack_threadtls(tls, &stackmem);
   TEST(1 == isfree_memblock(&stackmem));

   // TEST self_threadtls
   TEST(self_threadtls() == (thread_tls_t*) (((uintptr_t)&tls) - ((uintptr_t)&tls) % size_threadtls()));

   // TEST sys_self_threadtls
   TEST(sys_self_threadtls() == (thread_tls_t*) (((uintptr_t)&tls) - ((uintptr_t)&tls) % size_threadtls()));

   // TEST sys_self2_threadtls
   for (size_t i = 0; i < 1000*size_threadtls(); i += size_threadtls()) {
      TEST((thread_tls_t*)i == sys_self2_threadtls(i));
      TEST((thread_tls_t*)i == sys_self2_threadtls(i+1));
      TEST((thread_tls_t*)i == sys_self2_threadtls(i+size_threadtls()-1));
   }

   // TEST thread_threadtls
   TEST(0 == new_threadtls(&tls, 0, 0));
   TEST(thread_threadtls(tls) == (thread_t*) ((uint8_t*)tls + sizeof(threadcontext_t)));
   TEST(0 == delete_threadtls(&tls));
   TEST(thread_threadtls(tls) == (thread_t*) sizeof(threadcontext_t));
   for (size_t i = 0; i < 100; ++i) {
      thread_tls_t* tls2 = (void*) i;
      TEST((thread_t*)(i+sizeof(threadcontext_t)) == thread_threadtls(tls2));
   }

   // TEST context_threadtls
   TEST(0 == new_threadtls(&tls, 0, 0));
   TEST(context_threadtls(tls) == (threadcontext_t*) (uint8_t*)tls);
   TEST(0 == delete_threadtls(&tls));
   TEST(0 == context_threadtls(tls));
   for (size_t i = 0; i < 100; ++i) {
      thread_tls_t* tls2 = (void*) i;
      TEST((threadcontext_t*)i == context_threadtls(tls2));
   }

   // TEST sys_context_threadtls
   TEST(sys_context_threadtls() == context_threadtls(self_threadtls()));

   // TEST sys_thread_threadtls
   TEST(sys_thread_threadtls() == thread_threadtls(self_threadtls()));

   return 0;
ONERR:
   delete_threadtls(&tls);
   return EINVAL;
}

static int test_memory(void)
{
   thread_tls_t*  tls = 0;
   memblock_t     mblock;
   size_t         logsize1;
   size_t         logsize2;
   uint8_t*       logbuf1;
   uint8_t*       logbuf2;

   // prepare0
   TEST(0 == new_threadtls(&tls, 0, 0));
   const size_t memsize = tls->memsize;

   // TEST allocstatic_threadtls
   for (size_t u = 0; u <= memsize; ++u) {
      for (size_t s = memsize-u; s <= memsize-u; --s, s -= (s > 1000 ? 1000 : 0)) {
         size_t a = s % KONFIG_MEMALIGN ? s - s % KONFIG_MEMALIGN + KONFIG_MEMALIGN : s;
         if (a > memsize - u) continue;
         tls->memused = u;
         TEST(0 == allocstatic_threadtls(tls, s, &mblock));
         // check parameter
         TEST(mblock.addr == tls->mem + u);
         TEST(mblock.size == a);
         // check tls
         TEST(memsize == tls->memsize);
         TEST(u + a   == tls->memused);
      }
   }

   // TEST allocstatic_threadtls: ENOMEM (bytesize > available)
   GETBUFFER_ERRLOG(&logbuf1, &logsize1);
   mblock = (memblock_t) memblock_FREE;
   for (size_t i = 0; i <= memsize; ++i) {
      tls->memused = i;
      TEST(ENOMEM == allocstatic_threadtls(tls, memsize-i+1, &mblock));
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

   // TEST allocstatic_threadtls: ENOMEM (alignedsize < bytesize)
   tls->memused = 0;
   TEST(ENOMEM == allocstatic_threadtls(tls, (size_t)-1, &mblock));
   // check parameter
   TEST( isfree_memblock(&mblock));
   // check tls
   TEST(memsize == tls->memsize);
   TEST(0 == tls->memused);

   // TEST freestatic_threadtls: mblock valid && isfree_memblock(&mblock)
   for (size_t u = 0; u <= memsize; ++u) {
      for (size_t s = u; s <= u; --s, s -= (s > 1000 ? 1000 : 0)) {
         size_t a = s % KONFIG_MEMALIGN ? s - s % KONFIG_MEMALIGN + KONFIG_MEMALIGN : s;
         if (a > u) continue;
         tls->memused = u;
         mblock = (memblock_t) memblock_INIT(s, tls->mem + u - a);
         for (int r = 0; r < 2; ++r) {
            TEST(0 == freestatic_threadtls(tls, &mblock));
            // check parameter
            TEST( isfree_memblock(&mblock));
            // check tls
            TEST(memsize == tls->memsize);
            TEST(u - a   == tls->memused);
         }
      }
   }

   // TEST freestatic_threadtls: EINVAL (alignedsize < mblock.size)
   tls->memused = memsize;
   mblock.addr = tls->mem + memsize + 1;
   mblock.size = (size_t) -1;
   TEST(EINVAL == freestatic_threadtls(tls, &mblock));
   TEST(! isfree_memblock(&mblock));

   // TEST freestatic_threadtls: EINVAL (alignedsize > memused)
   tls->memused = 31;
   mblock.addr = tls->mem;
   mblock.size = 32;
   TEST(EINVAL == freestatic_threadtls(tls, &mblock));
   TEST(! isfree_memblock(&mblock));

   // TEST freestatic_threadtls: EINVAL (addr wrong)
   for (int i = -1; i <= 1; i +=2) {
      tls->memused = 128;
      mblock.addr = tls->mem + 128 - 32 + i;
      mblock.size = 32;
      TEST(EINVAL == freestatic_threadtls(tls, &mblock));
      TEST(! isfree_memblock(&mblock));
   }

   // TEST sizestatic_threadtls
   for (unsigned i = 0; i <= memsize; ++i) {
      tls->memused = i;
      TEST(i == sizestatic_threadtls(tls));
   }

   // reset0
   TEST(0 == delete_threadtls(&tls));

   return 0;
ONERR:
   delete_threadtls(&tls);
   return EINVAL;
}

int unittest_platform_task_thread_tls()
{
   if (test_initfree())    goto ONERR;
   if (test_initmain())    goto ONERR;
   if (test_query())       goto ONERR;
   if (test_memory())      goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
