/* title: ThreadLocalStorage impl

   Implements <ThreadLocalStorage>.

   about: Copyright
   This program is free software.
   You can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

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
#include "C-kern/api/test.h"
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
#define thread_vars_INIT_STATIC           {  threadcontext_INIT_STATIC, thread_INIT_STATIC }


// section: thread_tls_t

// group: variables

#ifdef KONFIG_UNITTEST
/* variable: s_threadtls_errtimer
 * Simulates an error in <init_threadtls> and <free_threadtls>. */
static test_errortimer_t   s_threadtls_errtimer = test_errortimer_INIT_FREEABLE ;
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
 * Returns the minimum size of the thread stack.
 * The returned value is a multiple of pagesize. */
static inline size_t sizestack_threadtls(const size_t pagesize)
{
   return (2*PTHREAD_STACK_MIN + pagesize - 1) & (~(pagesize-1)) ;
}

/* function: sizevars_threadtls
 * Returns the size all local thread variables on the stack.
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
   vmpage_t    mempage   = memblock_INIT_FREEABLE ;

   VALIDATE_STATE_TEST(size <= size_threadtls(), ONABORT, ) ;

   ONERROR_testerrortimer(&s_threadtls_errtimer, ONABORT) ;

   err = initaligned_vmpage(&mempage, size_threadtls()) ;
   if (err) goto ONABORT ;

   /* -- memory block layout --
    * low :  thread local variables (1 page)
    *     :  protection page (1 page)
    *     :  signal stack    (1? page)
    *     :  protection page (1 page)
    *     :  thread stack (4? pages)
    *     :  protection page (1 page)
    * high:
    */

   {
      ONERROR_testerrortimer(&s_threadtls_errtimer, ONABORT) ;
      vmpage_t protpage = vmpage_INIT(pagesize, mempage.addr + sizevars) ;
      err = protect_vmpage(&protpage, accessmode_PRIVATE) ;
      if (err) goto ONABORT ;
   }

   {
      ONERROR_testerrortimer(&s_threadtls_errtimer, ONABORT) ;
      vmpage_t protpage = vmpage_INIT(pagesize, mempage.addr + sizevars + sizesigst + pagesize) ;
      err = protect_vmpage(&protpage, accessmode_PRIVATE) ;
      if (err) goto ONABORT ;
   }

   {
      ONERROR_testerrortimer(&s_threadtls_errtimer, ONABORT) ;
      size_t   offset   = sizevars + sizesigst + sizestack + 2*pagesize ;
      vmpage_t protpage = vmpage_INIT(size_threadtls() - offset, mempage.addr + offset) ;
      err = protect_vmpage(&protpage, accessmode_PRIVATE) ;
      if (err) goto ONABORT ;
   }

   static_assert(
      (uintptr_t)context_threadtls(&(thread_tls_t){0}) == offsetof(thread_vars_t, threadcontext)
      && (uintptr_t)thread_threadtls(&(thread_tls_t){0}) == offsetof(thread_vars_t, thread),
      "query functions use offset corresponding to struct thread_vars_t"
   ) ;
   * (thread_vars_t*) mempage.addr = (thread_vars_t) thread_vars_INIT_STATIC ;
   tls->addr = mempage.addr ;

   return 0 ;
ONABORT:
   free_vmpage(&mempage) ;
   TRACEABORT_LOG(err) ;
   return err ;
}

int free_threadtls(thread_tls_t * tls)
{
   int err ;

   if (tls->addr) {
      vmpage_t mempage = vmpage_INIT(size_threadtls(), tls->addr) ;
      tls->addr = 0 ;

      err = free_vmpage(&mempage) ;

      if (err) goto ONABORT ;

      ONERROR_testerrortimer(&s_threadtls_errtimer, ONABORT) ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

int initstartup_threadtls(/*out*/thread_tls_t * tls, /*out*/struct memblock_t * threadstack, /*out*/struct memblock_t * signalstack)
{
   int err ;
   size_t   pagesize  = sys_pagesize_vm() ;
   size_t   sizevars  = sizevars_threadtls(pagesize) ;
   size_t   sizesigst = sizesignalstack_threadtls(pagesize) ;
   size_t   sizestack = sizestack_threadtls(pagesize) ;
   size_t   size      = 2*size_threadtls() - pagesize ;
   void *   addr      = MAP_FAILED ;

   ONERROR_testerrortimer(&s_threadtls_errtimer, ONABORT) ;
   addr = mmap(0, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) ;
   if (addr == MAP_FAILED) {
      err = errno ;
      goto ONABORT ;
   }

   uint8_t * aligned_addr = (uint8_t*) (((uintptr_t)addr + size_threadtls()-1) & ~(uintptr_t)(size_threadtls()-1)) ;

   ONERROR_testerrortimer(&s_threadtls_errtimer, ONABORT) ;
   if ((uint8_t*)addr < aligned_addr) {
      if (munmap(addr, (size_t)(aligned_addr - (uint8_t*)addr))) {
         err = errno ;
         goto ONABORT ;
      }
      size -= (size_t) (aligned_addr - (uint8_t*)addr) ;
      addr  = aligned_addr ;
   }

   ONERROR_testerrortimer(&s_threadtls_errtimer, ONABORT) ;
   if (size > size_threadtls()) {
      if (munmap((uint8_t*)addr + size_threadtls(), size - size_threadtls())) {
         err = errno ;
         goto ONABORT ;
      }
      size = size_threadtls() ;
   }

   ONERROR_testerrortimer(&s_threadtls_errtimer, ONABORT) ;
   if (mprotect((uint8_t*)addr + sizevars, pagesize, PROT_NONE)) {
      err = errno ;
      goto ONABORT ;
   }

   ONERROR_testerrortimer(&s_threadtls_errtimer, ONABORT) ;
   if (mprotect((uint8_t*)addr + sizevars + sizesigst + pagesize, pagesize, PROT_NONE)) {
      err = errno ;
      goto ONABORT ;
   }

   ONERROR_testerrortimer(&s_threadtls_errtimer, ONABORT) ;
   size_t offset = sizevars + sizesigst + sizestack + 2*pagesize ;
   if (mprotect((uint8_t*)addr + offset, size_threadtls() - offset, PROT_NONE)) {
      err = errno ;
      goto ONABORT ;
   }

   * (thread_vars_t*) addr = (thread_vars_t) thread_vars_INIT_STATIC ;
   tls->addr = addr ;

   *threadstack = (memblock_t) memblock_INIT(sizestack, (uint8_t*)addr + offset - sizestack) ;
   *signalstack = (memblock_t) memblock_INIT(sizesigst, (uint8_t*)addr + sizevars + pagesize) ;

   return 0 ;
ONABORT:
   if (addr != MAP_FAILED) {
      munmap(addr, size) ;
   }
   return err ;
}

int freestartup_threadtls(thread_tls_t * tls)
{
   int err ;

   if (tls->addr) {
      err = munmap(tls->addr, size_threadtls()) ;
      if (err) err = errno ;

      tls->addr = 0 ;

      if (err) goto ONABORT ;

      ONERROR_testerrortimer(&s_threadtls_errtimer, ONABORT) ;
   }

   return 0 ;
ONABORT:
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
   thread_tls_t   tls  = thread_tls_INIT_FREEABLE ;
   vmpage_t       vmpage ;

   // TEST thread_tls_INIT_FREEABLE
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
ONABORT:
   free_threadtls(&tls) ;
   return EINVAL ;
}

static int test_startup(void)
{
   thread_tls_t   tls  = thread_tls_INIT_FREEABLE ;
   memblock_t     threadstack = memblock_INIT_FREEABLE ;
   memblock_t     signalstack = memblock_INIT_FREEABLE ;
   vmpage_t       vmpage ;

   // TEST initstartup_threadtls
   TEST(0 == initstartup_threadtls(&tls, &threadstack, &signalstack)) ;
   TEST(0 != tls.addr) ;
   TEST(0 == (uintptr_t)tls.addr % size_threadtls()) ;   // aligned
   TEST(threadstack.addr == tls.addr + sizevars_threadtls(pagesize_vm()) + sizesignalstack_threadtls(pagesize_vm()) + 2 * pagesize_vm()) ;
   TEST(threadstack.size == sizestack_threadtls(pagesize_vm())) ;
   TEST(signalstack.addr == tls.addr + sizevars_threadtls(pagesize_vm()) + pagesize_vm()) ;
   TEST(signalstack.size == sizesignalstack_threadtls(pagesize_vm())) ;
   thread_vars_t     tlsvars  = thread_vars_INIT_STATIC ;
   thread_vars_t *   tlsvars2 = (thread_vars_t*) tls.addr ;
   TEST(0 == memcmp(&tlsvars, tlsvars2, sizeof(tlsvars))) ;

   // TEST freestartup_threadtls
   TEST(0 == freestartup_threadtls(&tls)) ;
   TEST(0 == tls.addr) ;
   TEST(0 == freestartup_threadtls(&tls)) ;
   TEST(0 == tls.addr) ;

   // TEST initstartup_threadtls: correct protection
   TEST(0 == initstartup_threadtls(&tls, &threadstack, &signalstack)) ;
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

   // TEST freestartup_threadtls: unmap pages
   vmpage = (vmpage_t) vmpage_INIT(size_threadtls(), tls.addr) ;
   TEST(0 == freestartup_threadtls(&tls)) ;
   TEST(1 == isunmapped_vm(&vmpage)) ;

   // TEST initstartup_threadtls: ERROR
   threadstack = (memblock_t) memblock_INIT_FREEABLE ;
   signalstack = (memblock_t) memblock_INIT_FREEABLE ;
   for (unsigned i = 1; i <= 6; ++i) {
      init_testerrortimer(&s_threadtls_errtimer, i, ENOMEM) ;
      TEST(ENOMEM == initstartup_threadtls(&tls, &threadstack, &signalstack)) ;
      TEST(0 == tls.addr) ;
      TEST(1 == isfree_memblock(&threadstack)) ;
      TEST(1 == isfree_memblock(&signalstack)) ;
   }

   // TEST freestartup_threadtls: ERROR
   TEST(0 == initstartup_threadtls(&tls, &threadstack, &signalstack)) ;
   init_testerrortimer(&s_threadtls_errtimer, 1, EINVAL) ;
   TEST(0 != tls.addr) ;
   TEST(EINVAL == freestartup_threadtls(&tls)) ;
   TEST(0 == tls.addr) ;

   return 0 ;
ONABORT:
   free_threadtls(&tls) ;
   return EINVAL ;
}

static int test_query(void)
{
   thread_tls_t   tls  = thread_tls_INIT_FREEABLE ;
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
   for (size_t i = 0; i < 10000*size_threadtls(); i += size_threadtls()) {
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

   // TEST sys_context_thread
   // TODO: TEST sys_context_thread: same as context_threadtls

   return 0 ;
ONABORT:
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

   // TEST genericcast_threadtls
   ptr = &tls ;
   TEST(ptr == genericcast_threadtls(&tls,)) ;
   ptr = (thread_tls_t*)&tls2.prefix_addr ;
   TEST(ptr == genericcast_threadtls(&tls2,prefix_)) ;

   return 0 ;
ONABORT:
   free_threadtls(&tls) ;
   return EINVAL ;
}

int unittest_platform_task_thread_tls()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())    goto ONABORT ;
   if (test_startup())     goto ONABORT ;
   if (test_query())       goto ONABORT ;
   if (test_generic())     goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
