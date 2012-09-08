/* title: BinaryStack impl
   Implements <BinaryStack>.

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
   (C) 2012 Jörg Seebohn

   file: C-kern/api/ds/inmem/binarystack.h
    Header file <BinaryStack>.

   file: C-kern/ds/inmem/binarystack.c
    Implementation file <BinaryStack impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/ds/inmem/binarystack.h"
#include "C-kern/api/platform/virtmemory.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


// section: binarystack_t

// group: lifetime

int init_binarystack(/*out*/binarystack_t * stack, size_t initial_size)
{
   int err ;
   size_t   pagesize = pagesize_vm() ;
   size_t   nrpages  = initial_size - (0 != initial_size) + pagesize ;

   if (nrpages < pagesize) {
      err = ENOMEM ;
      goto ONABORT ;
   }

   nrpages /= pagesize ;

   err = init_vmblock(&stack->stackmem, nrpages) ;
   if (err) goto ONABORT ;

   stack->size = 0 ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int free_binarystack(binarystack_t * stack)
{
   int err ;

   stack->size = 0 ;

   err = free_vmblock(&stack->stackmem) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;

}

// group: query

void * lastpushed_binarystack(binarystack_t * stack, size_t size_lastpushed)
{
   if (stack->size < size_lastpushed) {
      return 0 ;
   }

   return &stack->stackmem.addr[stack->size - size_lastpushed] ;
}

// group: operations

int push_binarystack(binarystack_t * stack, size_t size, /*out*/void ** lastpushed)
{
   int err ;
   size_t newsize  = stack->size + size ;

   if (newsize < size) {
      err = ENOMEM ;
      goto ONABORT ;
   }

   if (newsize > stack->stackmem.size) {
      size_t expandsize   = newsize - stack->stackmem.size ;
      size_t nrpages_incr = 1 + (expandsize / pagesize_vm()) ;
      err = movexpand_vmblock(&stack->stackmem, nrpages_incr) ;
      if (err) goto ONABORT ;
   }

   *lastpushed = &stack->stackmem.addr[stack->size] ;
   stack->size = newsize ;

   return 0;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int pop_binarystack(binarystack_t * stack, size_t size)
{
   int err ;

   VALIDATE_INPARAM_TEST(size <= stack->size, ONABORT, ) ;

   stack->size -= size ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   binarystack_t  stack = binarystack_INIT_FREEABLE ;
   void           * current ;
   void           * current2 ;

   // TEST static init
   TEST(0 == stack.size) ;
   TEST(0 == stack.stackmem.addr) ;
   TEST(0 == stack.stackmem.size) ;
   TEST(1 == isempty_binarystack(&stack)) ;
   TEST(0 == size_binarystack(&stack)) ;
   TEST(0 == lastpushed_binarystack(&stack, 0)) ;
   TEST(0 == lastpushed_binarystack(&stack, 1)) ;

   // TEST init, double free
   TEST(0 == init_binarystack(&stack, 1)) ;
   TEST(1 == isempty_binarystack(&stack)) ;
   TEST(0 == size_binarystack(&stack)) ;
   TEST(0 != lastpushed_binarystack(&stack, 0)) ;
   TEST(0 != stack.stackmem.addr) ;
   TEST(pagesize_vm() == stack.stackmem.size) ;
   TEST(0 == free_binarystack(&stack)) ;
   TEST(0 == size_binarystack(&stack)) ;
   TEST(0 == lastpushed_binarystack(&stack, 0)) ;
   TEST(0 == stack.stackmem.addr) ;
   TEST(0 == stack.stackmem.size) ;
   TEST(0 == free_binarystack(&stack)) ;
   TEST(0 == size_binarystack(&stack)) ;
   TEST(0 == stack.stackmem.addr) ;
   TEST(0 == stack.stackmem.size) ;

   // TEST initial size (size==0 => one page is preallocated)
   for(unsigned i = 0; i <= 20; ++i) {
      TEST(0 == init_binarystack(&stack, i + i * pagesize_vm() )) ;
      TEST(0 == size_binarystack(&stack)) ;
      TEST(1 == isempty_binarystack(&stack)) ;
      TEST(stack.stackmem.addr != 0) ;
      TEST(stack.stackmem.size == (i+1)*pagesize_vm()) ;
      TEST(stack.stackmem.addr == lastpushed_binarystack(&stack, 0)) ;
      TEST(0 == free_binarystack(&stack)) ;
   }

   // TEST push, pop
   uint8_t data1[3] = { 1, 2, 3 } ;
   TEST(0 == init_binarystack(&stack, 1)) ;
   TEST(0 == push_binarystack(&stack, sizeof(data1), &current)) ;
   TEST(3 == size_binarystack(&stack)) ;
   TEST(0 == isempty_binarystack(&stack)) ;
   TEST(current == stack.stackmem.addr) ;
   TEST(current == lastpushed_binarystack(&stack, sizeof(data1))) ;
   memcpy(current, data1, sizeof(data1)) ;
   TEST(0 == push_binarystack(&stack, 0, &current2)) ;
   TEST(current2 == (uint8_t*)current + 3) ;
   TEST(current2 == lastpushed_binarystack(&stack, 0)) ;
   TEST(3 == size_binarystack(&stack)) ;
   TEST(0 == pop_binarystack(&stack, 0)) ;
   TEST(current == lastpushed_binarystack(&stack, sizeof(data1))) ;
   TEST(3 == size_binarystack(&stack)) ;
   for(unsigned i = 1; i <= 3; ++i) {
      TEST(i == ((uint8_t*)current)[i-1]) ;
   }
   TEST(0 == pop_binarystack(&stack, 3)) ;
   TEST(1 == isempty_binarystack(&stack)) ;
   TEST(0 == size_binarystack(&stack)) ;
   TEST(current == lastpushed_binarystack(&stack, 0)) ;

   // TEST push 10000 uint32_t
   TEST(1 == isempty_binarystack(&stack)) ;
   for(uint32_t i = 1; i <= 10000; ++i) {
      uint32_t * ptri  = 0 ;
      size_t   newsize = stack.stackmem.size ;
      if (i*sizeof(i) > stack.stackmem.size) {
         newsize += pagesize_vm() ;
      }
      TEST(0 == pushgeneric_binarystack(&stack, &ptri)) ;
      TEST(0 != ptri) ;
      *ptri = i ;
      TEST(i*sizeof(i) == size_binarystack(&stack)) ;
      TEST(newsize     == stack.stackmem.size) ;
      TEST(0           == isempty_binarystack(&stack)) ;
   }

   // TEST pop 10000 uint32_t
   uint8_t * oldaddr = stack.stackmem.addr ;
   size_t  oldsize   = stack.stackmem.size ;
   for(uint32_t i = 10000; i; --i) {
      uint32_t * ptri = lastpushed_binarystack(&stack, sizeof(i)) ;
      TEST(i*sizeof(i) == size_binarystack(&stack)) ;
      TEST(ptri    != 0) ;
      TEST(*ptri   == i) ;
      TEST(0 == pop_binarystack(&stack, sizeof(i))) ;
      TEST(oldaddr == stack.stackmem.addr) ;
      TEST(oldsize == stack.stackmem.size) ;
      TEST((i==1)  == isempty_binarystack(&stack)) ;
   }
   TEST(0 == lastpushed_binarystack(&stack, 1)) ;
   TEST(0 == size_binarystack(&stack)) ;
   TEST(0 == free_binarystack(&stack)) ;

   // TEST ENOMEM
   TEST(ENOMEM == init_binarystack(&stack, SIZE_MAX)) ;
   TEST(0 == init_binarystack(&stack, 1)) ;
   TEST(ENOMEM == push_binarystack(&stack, SIZE_MAX, &current)) ;
   TEST(0 == size_binarystack(&stack)) ;
   TEST(0 == push_binarystack(&stack, 3, &current)) ;
   TEST(3 == size_binarystack(&stack)) ;
   TEST(ENOMEM == push_binarystack(&stack, SIZE_MAX-3, &current)) ;
   TEST(3 == size_binarystack(&stack)) ;
   TEST(0 == free_binarystack(&stack)) ;

   // TEST EINVAL
   TEST(0 == init_binarystack(&stack, 1)) ;
   TEST(0 == pop_binarystack(&stack, 0)) ;
   TEST(0 == push_binarystack(&stack, 0, &current)) ;
   TEST(EINVAL == pop_binarystack(&stack, 1)) ;
   TEST(0 == push_binarystack(&stack, 1, &current)) ;
   TEST(1 == size_binarystack(&stack)) ;
   TEST(0 == pop_binarystack(&stack, 1)) ;
   TEST(0 == size_binarystack(&stack)) ;
   TEST(EINVAL == pop_binarystack(&stack, 1)) ;
   TEST(0 == free_binarystack(&stack)) ;

   return 0 ;
ONABORT:
   free_binarystack(&stack) ;
   return EINVAL ;
}

int unittest_ds_inmem_binarystack()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())       goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif


