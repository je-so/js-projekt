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
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/vm.h"
#include "C-kern/api/platform/task/thread.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/test/errortimer.h"
#endif


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
 * The returned value is a multiple of <pagesize_vm>. */
static inline size_t sizesignalstack_threadtls(void)
{
   const size_t page_size = pagesize_vm() ;
   return (MINSIGSTKSZ + page_size - 1) & (~(page_size-1)) ;
}

/* function: sizestack_threadtls
 * Returns the minimum size of the thread stack.
 * The returned value is a multiple of <pagesize_vm>. */
static inline size_t sizestack_threadtls(void)
{
   const size_t page_size = pagesize_vm() ;
   return (PTHREAD_STACK_MIN + page_size - 1) & (~(page_size-1)) ;
}

/* function: sizevars_threadtls
 * Returns the size all local thread variables on the stack.
 * The returned value is a multiple of <pagesize_vm>. */
static inline size_t sizevars_threadtls(void)
{
   const size_t page_size = pagesize_vm() ;
   return (sizeof(threadcontext_t) + sizeof(thread_t) + page_size - 1) & (~(page_size-1)) ;
}

// group: lifetime

int init_threadtls(/*out*/thread_tls_t * tls)
{
   int err ;
   size_t      sizevars  = sizevars_threadtls() ;
   size_t      sizesigst = sizesignalstack_threadtls() ;
   size_t      sizestack = sizestack_threadtls() ;
   size_t      size      = (3u << log2pagesize_vm()) /* 3 protection pages around two stacks */
                           + sizevars
                           + sizesigst
                           + sizestack ;
   vmpage_t    mempage   = memblock_INIT_FREEABLE ;

   VALIDATE_STATE_TEST( size <= size_threadtls()
                        && size > size_threadtls()/2, ONABORT, ) ;

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
      vmpage_t protpage = vmpage_INIT(pagesize_vm(), mempage.addr + sizevars) ;
      err = protect_vmpage(&protpage, accessmode_PRIVATE) ;
      if (err) goto ONABORT ;
   }

   {
      ONERROR_testerrortimer(&s_threadtls_errtimer, ONABORT) ;
      vmpage_t protpage = vmpage_INIT(pagesize_vm(), mempage.addr + sizevars + sizesigst + pagesize_vm()) ;
      err = protect_vmpage(&protpage, accessmode_PRIVATE) ;
      if (err) goto ONABORT ;
   }

   {
      ONERROR_testerrortimer(&s_threadtls_errtimer, ONABORT) ;
      size_t   offset   = sizevars + sizesigst + sizestack + 2*pagesize_vm() ;
      vmpage_t protpage = vmpage_INIT(size_threadtls() - offset, mempage.addr + offset) ;
      err = protect_vmpage(&protpage, accessmode_PRIVATE) ;
      if (err) goto ONABORT ;
   }

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

// group: query

void signalstack_threadtls(const thread_tls_t * tls,/*out*/struct memblock_t * stackmem)
{
   size_t offset    = tls->addr ? sizevars_threadtls() + pagesize_vm() : 0 ;
   size_t sizesigst = tls->addr ? sizesignalstack_threadtls() : 0 ;
   *stackmem = (memblock_t) memblock_INIT(sizesigst, tls->addr + offset) ;
}

/* function: threadstack_threadtls(const thread_tls_t * tls)
 * Returns the thread stack from tls. */
void threadstack_threadtls(const thread_tls_t * tls,/*out*/struct memblock_t * stackmem)
{
   size_t offset    = tls->addr ? sizesignalstack_threadtls() + sizevars_threadtls() + 2*pagesize_vm() : 0 ;
   size_t sizestack = tls->addr ? sizestack_threadtls() : 0 ;
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
   TEST(0 == free_threadtls(&tls)) ;
   TEST(0 == tls.addr) ;
   TEST(0 == free_threadtls(&tls)) ;
   TEST(0 == tls.addr) ;

   // TEST init_threadtls: add protection pages
   TEST(0 == init_threadtls(&tls)) ;
   // variables
   vmpage = (vmpage_t) vmpage_INIT(sizevars_threadtls(), tls.addr) ;
   TEST(1 == ismapped_vm(&vmpage, accessmode_RDWR_PRIVATE)) ;
   // protection page
   vmpage = (vmpage_t) vmpage_INIT(pagesize_vm(), tls.addr + sizevars_threadtls()) ;
   TEST(1 == ismapped_vm(&vmpage, accessmode_PRIVATE)) ;
   // signal stack page
   vmpage = (vmpage_t) vmpage_INIT(sizesignalstack_threadtls(), tls.addr + sizevars_threadtls() + 1 * pagesize_vm()) ;
   TEST(1 == ismapped_vm(&vmpage, accessmode_RDWR_PRIVATE)) ;
   // protection page
   vmpage = (vmpage_t) vmpage_INIT(pagesize_vm(), tls.addr + sizevars_threadtls() + sizesignalstack_threadtls() + 1 * pagesize_vm()) ;
   TEST(1 == ismapped_vm(&vmpage, accessmode_PRIVATE)) ;
   // thread stack page
   vmpage = (vmpage_t) vmpage_INIT(sizestack_threadtls(), tls.addr + sizevars_threadtls() + sizesignalstack_threadtls() + 2 * pagesize_vm()) ;
   TEST(1 == ismapped_vm(&vmpage, accessmode_RDWR_PRIVATE)) ;
   // protection page
   size_t offset = sizevars_threadtls() + sizesignalstack_threadtls() + sizestack_threadtls() + 2 * pagesize_vm() ;
   vmpage = (vmpage_t) vmpage_INIT(size_threadtls() - offset, tls.addr + offset) ;
   TEST(1 == ismapped_vm(&vmpage, accessmode_PRIVATE)) ;

   // TEST free_threadtls: remove protection pages
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

static int test_query(void)
{
   thread_tls_t   tls  = thread_tls_INIT_FREEABLE ;
   memblock_t     stackmem ;

   // TEST sizesignalstack_threadtls
   TEST(MINSIGSTKSZ <= sizesignalstack_threadtls()) ;
   TEST(0 == sizesignalstack_threadtls() % pagesize_vm()) ;

   // TEST sizestack_threadtls
   TEST(PTHREAD_STACK_MIN <= sizestack_threadtls()) ;
   TEST(0 == sizestack_threadtls() % pagesize_vm()) ;

   // TEST sizevars_threadtls
   TEST(sizeof(thread_t) + sizeof(threadcontext_t) <= sizevars_threadtls()) ;
   TEST(0 == sizevars_threadtls() % pagesize_vm()) ;

   // TEST size_threadtls
   TEST(0 == size_threadtls() % pagesize_vm()) ;
   TEST(size_threadtls()/4 <  3*pagesize_vm() + sizesignalstack_threadtls() + sizestack_threadtls() + sizevars_threadtls()) ;
   TEST(size_threadtls()   >= 3*pagesize_vm() + sizesignalstack_threadtls() + sizestack_threadtls() + sizevars_threadtls()) ;

   // TEST signalstack_threadtls
   TEST(0 == init_threadtls(&tls)) ;
   signalstack_threadtls(&tls, &stackmem) ;
   TEST(stackmem.addr == tls.addr + sizevars_threadtls() + pagesize_vm()) ;
   TEST(stackmem.size == sizesignalstack_threadtls()) ;
   TEST(0 == free_threadtls(&tls)) ;
   signalstack_threadtls(&tls, &stackmem) ;
   TEST(1 == isfree_memblock(&stackmem)) ;

   // TEST threadstack_threadtls
   TEST(0 == init_threadtls(&tls)) ;
   threadstack_threadtls(&tls, &stackmem) ;
   TEST(stackmem.addr == tls.addr + sizevars_threadtls() + sizesignalstack_threadtls() + 2*pagesize_vm()) ;
   TEST(stackmem.size == sizestack_threadtls()) ;
   TEST(0 == free_threadtls(&tls)) ;
   threadstack_threadtls(&tls, &stackmem) ;
   TEST(1 == isfree_memblock(&stackmem)) ;

   // TEST context_threadtls
   TEST(context_threadtls() == (threadcontext_t*) (((uintptr_t)&tls) - ((uintptr_t)&tls) % size_threadtls())) ;

   // TEST context2_threadtls
   for (size_t i = 0; i < 10000*size_threadtls(); i += size_threadtls()) {
      TEST((threadcontext_t*)i == context2_threadtls(i)) ;
      TEST((threadcontext_t*)i == context2_threadtls(i+1)) ;
      TEST((threadcontext_t*)i == context2_threadtls(i+size_threadtls()-1)) ;
   }

   // TEST sys_context_thread
   // TODO: TEST sys_context_thread: same as context_threadtls

   // TEST sys_context2_thread
   // TODO: TEST sys_context2_thread: same as context2_threadtls

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
