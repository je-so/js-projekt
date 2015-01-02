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

typedef struct thread_vars_t              thread_vars_t ;

/* struct: thread_vars_t
 * Variables stored in thread local storage. */
struct thread_vars_t {
   threadcontext_t   threadcontext ;
   thread_t          thread ;
} ;

// group: lifetime

/* define: thread_vars_INIT_STATIC
 * Static initializer. Used to initialize all variables of thread locval storage. */
#define thread_vars_INIT_STATIC {  threadcontext_INIT_STATIC, thread_FREE }


// section: thread_tls_t

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_threadtls_errtimer
 * Simulates an error in <init_threadtls> and <free_threadtls>. */
static test_errortimer_t   s_threadtls_errtimer = test_errortimer_FREE ;
#endif

// group: helper

/* function: sizesignalstack_threadtls
 * Returns the minimum size of the signal stack.
 * The returned value is a multiple of pagesize. */
static inline size_t sizesignalstack_threadtls(const size_t pagesize)
{
   return (MINSIGSTKSZ + pagesize - 1) & (~(pagesize-1)) ;
}

/* function: sizestack_threadtls
 * Returns the default size of the thread stack.
 * The returned value is a multiple of pagesize. */
static inline size_t sizestack_threadtls(const size_t pagesize)
{
   static_assert(256*1024 < size_threadtls(), "sys_size_threadtls/size_threadtls is big enough") ;
   return (256*1024 + pagesize - 1) & (~(pagesize-1)) ;
}

/* function: sizevars_threadtls
 * Returns the size of all local thread variables on the stack.
 * The returned value is a multiple of pagesize. */
static inline size_t sizevars_threadtls(const size_t pagesize)
{
   return (sizeof(thread_vars_t) + pagesize - 1) & (~(pagesize-1)) ;
}

// group: lifetime

int init_threadtls(/*out*/thread_tls_t * tls)
{
   int err ;
   size_t      pagesize  = pagesize_vm() ;
   size_t      sizevars  = sizevars_threadtls(pagesize) ;
   size_t      sizesigst = sizesignalstack_threadtls(pagesize) ;
   size_t      sizestack = sizestack_threadtls(pagesize) ;
   size_t      size      = (3u << log2pagesize_vm()) /* 3 protection pages around two stacks */
                           + sizevars
                           + sizesigst
                           + sizestack ;
   vmpage_t    mempage   = memblock_FREE ;

   VALIDATE_STATE_TEST(size <= size_threadtls(), ONERR, ) ;

   ONERROR_testerrortimer(&s_threadtls_errtimer, &err, ONERR);

   err = initaligned_vmpage(&mempage, size_threadtls()) ;
   if (err) goto ONERR;

   /* -- memory block layout --
    * low :  thread local variables (1 page)
    *     :  protection page (1 page)
    *     :  signal stack    (1? page)
    *     :  protection page (1 page)
    *     :  thread stack    (256K)
    *     :  protection page (X pages)
    * high:
    */

   {
      ONERROR_testerrortimer(&s_threadtls_errtimer, &err, ONERR);
      vmpage_t protpage = vmpage_INIT(pagesize, mempage.addr + sizevars) ;
      err = protect_vmpage(&protpage, accessmode_PRIVATE) ;
      if (err) goto ONERR;
   }

   {
      ONERROR_testerrortimer(&s_threadtls_errtimer, &err, ONERR);
      vmpage_t protpage = vmpage_INIT(pagesize, mempage.addr + sizevars + sizesigst + pagesize) ;
      err = protect_vmpage(&protpage, accessmode_PRIVATE) ;
      if (err) goto ONERR;
   }

   {
      ONERROR_testerrortimer(&s_threadtls_errtimer, &err, ONERR);
      size_t   offset   = sizevars + sizesigst + sizestack + 2*pagesize ;
      vmpage_t protpage = vmpage_INIT(size_threadtls() - offset, mempage.addr + offset) ;
      err = protect_vmpage(&protpage, accessmode_PRIVATE) ;
      if (err) goto ONERR;
   }

   static_assert(
      (uintptr_t)context_threadtls(&(thread_tls_t){0}) == offsetof(thread_vars_t, threadcontext)
      && (uintptr_t)thread_threadtls(&(thread_tls_t){0}) == offsetof(thread_vars_t, thread),
      "query functions use offset corresponding to struct thread_vars_t"
   ) ;
   * (thread_vars_t*) mempage.addr = (thread_vars_t) thread_vars_INIT_STATIC ;
   tls->addr = mempage.addr ;

   return 0 ;
ONERR:
   free_vmpage(&mempage) ;
   TRACEEXIT_ERRLOG(err);
   return err ;
}

int free_threadtls(thread_tls_t * tls)
{
   int err ;

   if (tls->addr) {
      vmpage_t mempage = vmpage_INIT(size_threadtls(), tls->addr) ;
      tls->addr = 0 ;

      err = free_vmpage(&mempage) ;

      if (err) goto ONERR;

      ONERROR_testerrortimer(&s_threadtls_errtimer, &err, ONERR);
   }

   return 0 ;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err ;
}

int initmain_threadtls(/*out*/thread_tls_t * tls, /*out*/struct memblock_t * threadstack, /*out*/struct memblock_t * signalstack)
{
   int err ;
   size_t   pagesize  = sys_pagesize_vm() ;
   size_t   sizevars  = sizevars_threadtls(pagesize) ;
   size_t   sizesigst = sizesignalstack_threadtls(pagesize) ;
   size_t   sizestack = sizestack_threadtls(pagesize) ;
   size_t   size      = 2*size_threadtls() - pagesize ;
   void *   addr      = MAP_FAILED ;

   ONERROR_testerrortimer(&s_threadtls_errtimer, &err, ONERR);
   addr = mmap(0, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) ;
   if (addr == MAP_FAILED) {
      err = errno ;
      goto ONERR;
   }

   uint8_t * aligned_addr = (uint8_t*) (((uintptr_t)addr + size_threadtls()-1) & ~(uintptr_t)(size_threadtls()-1)) ;

   ONERROR_testerrortimer(&s_threadtls_errtimer, &err, ONERR);
   if ((uint8_t*)addr < aligned_addr) {
      if (munmap(addr, (size_t)(aligned_addr - (uint8_t*)addr))) {
         err = errno ;
         goto ONERR;
      }
      size -= (size_t) (aligned_addr - (uint8_t*)addr) ;
      addr  = aligned_addr ;
   }

   ONERROR_testerrortimer(&s_threadtls_errtimer, &err, ONERR);
   if (size > size_threadtls()) {
      if (munmap((uint8_t*)addr + size_threadtls(), size - size_threadtls())) {
         err = errno ;
         goto ONERR;
      }
      size = size_threadtls() ;
   }

   ONERROR_testerrortimer(&s_threadtls_errtimer, &err, ONERR);
   if (mprotect((uint8_t*)addr + sizevars, pagesize, PROT_NONE)) {
      err = errno ;
      goto ONERR;
   }

   ONERROR_testerrortimer(&s_threadtls_errtimer, &err, ONERR);
   if (mprotect((uint8_t*)addr + sizevars + sizesigst + pagesize, pagesize, PROT_NONE)) {
      err = errno ;
      goto ONERR;
   }

   ONERROR_testerrortimer(&s_threadtls_errtimer, &err, ONERR);
   size_t offset = sizevars + sizesigst + sizestack + 2*pagesize ;
   if (mprotect((uint8_t*)addr + offset, size_threadtls() - offset, PROT_NONE)) {
      err = errno ;
      goto ONERR;
   }

   * (thread_vars_t*) addr = (thread_vars_t) thread_vars_INIT_STATIC ;
   tls->addr = addr ;

   *threadstack = (memblock_t) memblock_INIT(sizestack, (uint8_t*)addr + offset - sizestack) ;
   *signalstack = (memblock_t) memblock_INIT(sizesigst, (uint8_t*)addr + sizevars + pagesize) ;

   return 0 ;
ONERR:
   if (addr != MAP_FAILED) {
      munmap(addr, size) ;
   }
   return err ;
}

int freemain_threadtls(thread_tls_t * tls)
{
   int err ;

   if (tls->addr) {
      err = munmap(tls->addr, size_threadtls()) ;
      if (err) err = errno ;

      tls->addr = 0 ;

      if (err) goto ONERR;

      ONERROR_testerrortimer(&s_threadtls_errtimer, &err, ONERR);
   }

   return 0 ;
ONERR:
   return err ;
}

// group: query

void signalstack_threadtls(const thread_tls_t * tls,/*out*/struct memblock_t * stackmem)
{
   size_t pagesize  = pagesize_vm() ;
   size_t offset    = tls->addr ? sizevars_threadtls(pagesize) + pagesize : 0 ;
   size_t sizesigst = tls->addr ? sizesignalstack_threadtls(pagesize) : 0 ;
   *stackmem = (memblock_t) memblock_INIT(sizesigst, tls->addr + offset) ;
}

/* function: threadstack_threadtls(const thread_tls_t * tls)
 * Returns the thread stack from tls. */
void threadstack_threadtls(const thread_tls_t * tls,/*out*/struct memblock_t * stackmem)
{
   size_t pagesize  = pagesize_vm() ;
   size_t offset    = tls->addr ? sizesignalstack_threadtls(pagesize) + sizevars_threadtls(pagesize) + 2*pagesize : 0 ;
   size_t sizestack = tls->addr ? sizestack_threadtls(pagesize) : 0 ;
   *stackmem = (memblock_t) memblock_INIT(sizestack, tls->addr + offset) ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   thread_tls_t   tls  = thread_tls_FREE ;
   vmpage_t       vmpage ;

   // TEST thread_tls_FREE
   TEST(0 == tls.addr) ;

   // TEST init_threadtls
   TEST(0 == init_threadtls(&tls)) ;
   TEST(0 != tls.addr) ;
   TEST(0 == (uintptr_t)tls.addr % size_threadtls()) ;   // aligned
   thread_vars_t     tlsvars  = thread_vars_INIT_STATIC ;
   thread_vars_t *   tlsvars2 = (thread_vars_t*) tls.addr ;
   TEST(0 == memcmp(&tlsvars, tlsvars2, sizeof(tlsvars))) ;

   // TEST free_threadtls
   TEST(0 == free_threadtls(&tls)) ;
   TEST(0 == tls.addr) ;
   TEST(0 == free_threadtls(&tls)) ;
   TEST(0 == tls.addr) ;

   // TEST init_threadtls: correct protection
   TEST(0 == init_threadtls(&tls)) ;
   // variables
   vmpage = (vmpage_t) vmpage_INIT(sizevars_threadtls(pagesize_vm()), tls.addr) ;
   TEST(1 == ismapped_vm(&vmpage, accessmode_RDWR_PRIVATE)) ;
   // protection page
   vmpage = (vmpage_t) vmpage_INIT(pagesize_vm(), tls.addr + sizevars_threadtls(pagesize_vm())) ;
   TEST(1 == ismapped_vm(&vmpage, accessmode_PRIVATE)) ;
   // signal stack page
   vmpage = (vmpage_t) vmpage_INIT(sizesignalstack_threadtls(pagesize_vm()), tls.addr + sizevars_threadtls(pagesize_vm()) + 1 * pagesize_vm()) ;
   TEST(1 == ismapped_vm(&vmpage, accessmode_RDWR_PRIVATE)) ;
   // protection page
   vmpage = (vmpage_t) vmpage_INIT(pagesize_vm(), tls.addr + sizevars_threadtls(pagesize_vm()) + sizesignalstack_threadtls(pagesize_vm()) + 1 * pagesize_vm()) ;
   TEST(1 == ismapped_vm(&vmpage, accessmode_PRIVATE)) ;
   // thread stack page
   vmpage = (vmpage_t) vmpage_INIT(sizestack_threadtls(pagesize_vm()), tls.addr + sizevars_threadtls(pagesize_vm()) + sizesignalstack_threadtls(pagesize_vm()) + 2 * pagesize_vm()) ;
   TEST(1 == ismapped_vm(&vmpage, accessmode_RDWR_PRIVATE)) ;
   // protection page
   size_t offset = sizevars_threadtls(pagesize_vm()) + sizesignalstack_threadtls(pagesize_vm()) + sizestack_threadtls(pagesize_vm()) + 2 * pagesize_vm() ;
   vmpage = (vmpage_t) vmpage_INIT(size_threadtls() - offset, tls.addr + offset) ;
   TEST(1 == ismapped_vm(&vmpage, accessmode_PRIVATE)) ;

   // TEST free_threadtls: unmap pages
   vmpage = (vmpage_t) vmpage_INIT(size_threadtls(), tls.addr) ;
   TEST(0 == free_threadtls(&tls)) ;
   TEST(1 == isunmapped_vm(&vmpage)) ;

   // TEST init_threadtls: ERROR
   for (unsigned i = 1; i <= 4; ++i) {
      init_testerrortimer(&s_threadtls_errtimer, i, ENOMEM) ;
      TEST(ENOMEM == init_threadtls(&tls)) ;
      TEST(0 == tls.addr) ;
   }

   // TEST free_threadtls: ERROR
   TEST(0 == init_threadtls(&tls)) ;
   init_testerrortimer(&s_threadtls_errtimer, 1, EINVAL) ;
   TEST(0 != tls.addr) ;
   TEST(EINVAL == free_threadtls(&tls)) ;
   TEST(0 == tls.addr) ;

   return 0 ;
ONERR:
   free_threadtls(&tls) ;
   return EINVAL ;
}

static int test_initmain(void)
{
   thread_tls_t   tls  = thread_tls_FREE ;
   memblock_t     threadstack = memblock_FREE ;
   memblock_t     signalstack = memblock_FREE ;
   vmpage_t       vmpage ;

   // TEST initmain_threadtls
   TEST(0 == initmain_threadtls(&tls, &threadstack, &signalstack)) ;
   TEST(0 != tls.addr) ;
   TEST(0 == (uintptr_t)tls.addr % size_threadtls()) ;   // aligned
   TEST(threadstack.addr == tls.addr + sizevars_threadtls(pagesize_vm()) + sizesignalstack_threadtls(pagesize_vm()) + 2 * pagesize_vm()) ;
   TEST(threadstack.size == sizestack_threadtls(pagesize_vm())) ;
   TEST(signalstack.addr == tls.addr + sizevars_threadtls(pagesize_vm()) + pagesize_vm()) ;
   TEST(signalstack.size == sizesignalstack_threadtls(pagesize_vm())) ;
   thread_vars_t     tlsvars  = thread_vars_INIT_STATIC ;
   thread_vars_t *   tlsvars2 = (thread_vars_t*) tls.addr ;
   TEST(0 == memcmp(&tlsvars, tlsvars2, sizeof(tlsvars))) ;

   // TEST freemain_threadtls
   TEST(0 == freemain_threadtls(&tls)) ;
   TEST(0 == tls.addr) ;
   TEST(0 == freemain_threadtls(&tls)) ;
   TEST(0 == tls.addr) ;

   // TEST initmain_threadtls: correct protection
   TEST(0 == initmain_threadtls(&tls, &threadstack, &signalstack)) ;
   // variables
   vmpage = (vmpage_t) vmpage_INIT(sizevars_threadtls(pagesize_vm()), tls.addr) ;
   TEST(1 == ismapped_vm(&vmpage, accessmode_RDWR_PRIVATE)) ;
   // protection page
   vmpage = (vmpage_t) vmpage_INIT(pagesize_vm(), tls.addr + sizevars_threadtls(pagesize_vm())) ;
   TEST(1 == ismapped_vm(&vmpage, accessmode_PRIVATE)) ;
   // signal stack page
   vmpage = (vmpage_t) vmpage_INIT(sizesignalstack_threadtls(pagesize_vm()), tls.addr + sizevars_threadtls(pagesize_vm()) + 1 * pagesize_vm()) ;
   TEST(1 == ismapped_vm(&vmpage, accessmode_RDWR_PRIVATE)) ;
   TEST(vmpage.addr == signalstack.addr) ;
   TEST(vmpage.size == signalstack.size)
   // protection page
   vmpage = (vmpage_t) vmpage_INIT(pagesize_vm(), tls.addr + sizevars_threadtls(pagesize_vm()) + sizesignalstack_threadtls(pagesize_vm()) + 1 * pagesize_vm()) ;
   TEST(1 == ismapped_vm(&vmpage, accessmode_PRIVATE)) ;
   // thread stack page
   vmpage = (vmpage_t) vmpage_INIT(sizestack_threadtls(pagesize_vm()), tls.addr + sizevars_threadtls(pagesize_vm()) + sizesignalstack_threadtls(pagesize_vm()) + 2 * pagesize_vm()) ;
   TEST(1 == ismapped_vm(&vmpage, accessmode_RDWR_PRIVATE)) ;
   TEST(vmpage.addr == threadstack.addr) ;
   TEST(vmpage.size == threadstack.size)
   // protection page
   size_t offset = sizevars_threadtls(pagesize_vm()) + sizesignalstack_threadtls(pagesize_vm()) + sizestack_threadtls(pagesize_vm()) + 2 * pagesize_vm() ;
   vmpage = (vmpage_t) vmpage_INIT(size_threadtls() - offset, tls.addr + offset) ;
   TEST(1 == ismapped_vm(&vmpage, accessmode_PRIVATE)) ;

   // TEST freemain_threadtls: unmap pages
   vmpage = (vmpage_t) vmpage_INIT(size_threadtls(), tls.addr) ;
   TEST(0 == freemain_threadtls(&tls)) ;
   TEST(1 == isunmapped_vm(&vmpage)) ;

   // TEST initmain_threadtls: ERROR
   threadstack = (memblock_t) memblock_FREE ;
   signalstack = (memblock_t) memblock_FREE ;
   for (unsigned i = 1; i <= 6; ++i) {
      init_testerrortimer(&s_threadtls_errtimer, i, ENOMEM) ;
      TEST(ENOMEM == initmain_threadtls(&tls, &threadstack, &signalstack)) ;
      TEST(0 == tls.addr) ;
      TEST(1 == isfree_memblock(&threadstack)) ;
      TEST(1 == isfree_memblock(&signalstack)) ;
   }

   // TEST freemain_threadtls: ERROR
   TEST(0 == initmain_threadtls(&tls, &threadstack, &signalstack)) ;
   init_testerrortimer(&s_threadtls_errtimer, 1, EINVAL) ;
   TEST(0 != tls.addr) ;
   TEST(EINVAL == freemain_threadtls(&tls)) ;
   TEST(0 == tls.addr) ;

   return 0 ;
ONERR:
   free_threadtls(&tls) ;
   return EINVAL ;
}

static int test_query(void)
{
   thread_tls_t   tls  = thread_tls_FREE ;
   memblock_t     stackmem ;

   // TEST sizesignalstack_threadtls
   TEST(MINSIGSTKSZ <= sizesignalstack_threadtls(pagesize_vm())) ;
   TEST(0 == sizesignalstack_threadtls(pagesize_vm()) % pagesize_vm()) ;

   // TEST sizestack_threadtls
   TEST(PTHREAD_STACK_MIN <= sizestack_threadtls(pagesize_vm())) ;
   TEST(0 == sizestack_threadtls(pagesize_vm()) % pagesize_vm()) ;

   // TEST sizevars_threadtls
   TEST(sizeof(thread_vars_t) <= sizevars_threadtls(pagesize_vm())) ;
   TEST(0 == sizevars_threadtls(pagesize_vm()) % pagesize_vm()) ;

   // TEST size_threadtls
   TEST(0 == size_threadtls() % pagesize_vm()) ;
   size_t minsize = 3*pagesize_vm() + sizesignalstack_threadtls(pagesize_vm()) + sizestack_threadtls(pagesize_vm()) + sizevars_threadtls(pagesize_vm()) ;
   TEST(size_threadtls()/2 < minsize) ;
   TEST(size_threadtls()  >= minsize) ;

   // TEST signalstack_threadtls
   TEST(0 == init_threadtls(&tls)) ;
   signalstack_threadtls(&tls, &stackmem) ;
   TEST(stackmem.addr == tls.addr + sizevars_threadtls(pagesize_vm()) + pagesize_vm()) ;
   TEST(stackmem.size == sizesignalstack_threadtls(pagesize_vm())) ;
   TEST(0 == free_threadtls(&tls)) ;
   signalstack_threadtls(&tls, &stackmem) ;
   TEST(1 == isfree_memblock(&stackmem)) ;

   // TEST threadstack_threadtls
   TEST(0 == init_threadtls(&tls)) ;
   threadstack_threadtls(&tls, &stackmem) ;
   TEST(stackmem.addr == tls.addr + sizevars_threadtls(pagesize_vm()) + sizesignalstack_threadtls(pagesize_vm()) + 2*pagesize_vm()) ;
   TEST(stackmem.size == sizestack_threadtls(pagesize_vm())) ;
   TEST(0 == free_threadtls(&tls)) ;
   threadstack_threadtls(&tls, &stackmem) ;
   TEST(1 == isfree_memblock(&stackmem)) ;

   // TEST current_threadtls
   TEST(current_threadtls(&tls).addr == (uint8_t*) (((uintptr_t)&tls) - ((uintptr_t)&tls) % size_threadtls())) ;
   for (size_t i = 0; i < 1000*size_threadtls(); i += size_threadtls()) {
      TEST((uint8_t*)i == current_threadtls(i).addr) ;
      TEST((uint8_t*)i == current_threadtls(i+1).addr) ;
      TEST((uint8_t*)i == current_threadtls(i+size_threadtls()-1).addr) ;
   }

   // TEST thread_threadtls
   TEST(0 == init_threadtls(&tls)) ;
   TEST(thread_threadtls(&tls) == (thread_t*) (tls.addr + sizeof(threadcontext_t))) ;
   TEST(0 == free_threadtls(&tls)) ;
   TEST(thread_threadtls(&tls) == (thread_t*) sizeof(threadcontext_t)) ;
   for (size_t i = 0; i < 100; ++i) {
      thread_tls_t tls2 = { (void*)i } ;
      TEST((thread_t*)(i+sizeof(threadcontext_t)) == thread_threadtls(&tls2)) ;
   }

   // TEST context_threadtls
   TEST(0 == init_threadtls(&tls)) ;
   TEST(context_threadtls(&tls) == (threadcontext_t*) tls.addr) ;
   TEST(0 == free_threadtls(&tls)) ;
   TEST(0 == context_threadtls(&tls)) ;
   for (size_t i = 0; i < 100; ++i) {
      thread_tls_t tls2 = { (void*)i } ;
      TEST((threadcontext_t*)i == context_threadtls(&tls2)) ;
   }

   // TEST sys_context_threadtls
   TEST(sys_context_threadtls() == context_threadtls(&current_threadtls(&tls))) ;

   // TEST sys_context2_threadtls
   TEST(sys_context2_threadtls(&tls) == context_threadtls(&current_threadtls(&tls))) ;
   for (size_t i = 0; i < 1000*size_threadtls(); i += size_threadtls()) {
      TEST((threadcontext_t*)i == sys_context2_threadtls(i)) ;
      TEST((threadcontext_t*)i == sys_context2_threadtls(i+1)) ;
      TEST((threadcontext_t*)i == sys_context2_threadtls(i+size_threadtls()-1)) ;
   }

   // TEST sys_thread_threadtls
   TEST(sys_thread_threadtls() == thread_threadtls(&current_threadtls(&tls))) ;

   return 0 ;
ONERR:
   free_threadtls(&tls) ;
   return EINVAL ;
}


static int test_generic(void)
{
   thread_tls_t   tls ;
   thread_tls_t * ptr ;
   struct {
      int         x[4] ;
      uint8_t *   prefix_addr ;
      int         y ;
   }              tls2 ;

   // TEST cast_threadtls
   ptr = &tls ;
   TEST(ptr == cast_threadtls(&tls,)) ;
   ptr = (thread_tls_t*)&tls2.prefix_addr ;
   TEST(ptr == cast_threadtls(&tls2,prefix_)) ;

   return 0;
ONERR:
   free_threadtls(&tls);
   return EINVAL;
}

int unittest_platform_task_thread_tls()
{
   if (test_initfree())    goto ONERR;
   if (test_initmain())    goto ONERR;
   if (test_query())       goto ONERR;
   if (test_generic())     goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
