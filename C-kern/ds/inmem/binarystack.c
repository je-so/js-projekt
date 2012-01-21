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
      goto ABBRUCH ;
   }

   nrpages /= pagesize ;

   err = init_vmblock(&stack->stackmem, nrpages) ;
   if (err) goto ABBRUCH ;

   stack->size = 0 ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int free_binarystack(binarystack_t * stack)
{
   int err ;

   err = free_vmblock(&stack->stackmem) ;
   if (err) goto ABBRUCH ;

   stack->size = 0 ;

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;

}

// group: access

int push_binarystack(binarystack_t * stack, size_t size, const void * data/*[size]*/)
{
   int err ;
   size_t oldsize = stack->size ;
   size_t newsize = oldsize + size ;

   if (newsize < size) {
      err = ENOMEM ;
      goto ABBRUCH ;
   }

   if (newsize > stack->stackmem.size) {
      size_t expandsize   = newsize - stack->stackmem.size ;
      size_t nrpages_incr = 1 + (expandsize / pagesize_vm()) ;
      err = movexpand_vmblock(&stack->stackmem, nrpages_incr) ;
      if (err) goto ABBRUCH ;
   }

   stack->size = newsize ;
   memcpy(stack->stackmem.addr + oldsize, data, size) ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int pop_binarystack(binarystack_t * stack, size_t size, void * data/*[size]*/)
{
   int err ;

   VALIDATE_INPARAM_TEST(size <= stack->size, ABBRUCH, ) ;

   stack->size -= size ;
   if (data) {
      memcpy(data, (const uint8_t *)stack->stackmem.addr + stack->size, size) ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

// group: test

#ifdef KONFIG_UNITTEST

#define TEST(ARG) TEST_ONERROR_GOTO(ARG, ABBRUCH)

static int test_initfree(void)
{
   binarystack_t     stack = binarystack_INIT_FREEABLE ;

   // TEST static init
   TEST(0 == stack.size) ;
   TEST(0 == stack.stackmem.addr) ;
   TEST(0 == stack.stackmem.size) ;
   TEST(1 == isempty_binarystack(&stack)) ;

   // TEST init, double free
   TEST(0 == init_binarystack(&stack, 1)) ;
   TEST(0 == stack.size) ;
   TEST(0 != stack.stackmem.addr) ;
   TEST(pagesize_vm() == stack.stackmem.size) ;
   TEST(1 == isempty_binarystack(&stack)) ;
   TEST(0 == free_binarystack(&stack)) ;
   TEST(0 == stack.size) ;
   TEST(0 == stack.stackmem.addr) ;
   TEST(0 == stack.stackmem.size) ;
   TEST(0 == free_binarystack(&stack)) ;
   TEST(0 == stack.size) ;
   TEST(0 == stack.stackmem.addr) ;
   TEST(0 == stack.stackmem.size) ;

   // TEST initial size (size==0 => one page is preallocated)
   for(unsigned i = 0; i <= 20; ++i) {
      TEST(0 == init_binarystack(&stack, i + i * pagesize_vm() )) ;
      TEST(0 == stack.size) ;
      TEST(0 != stack.stackmem.addr) ;
      TEST((i+1)*pagesize_vm() == stack.stackmem.size) ;
      TEST(1 == isempty_binarystack(&stack)) ;
      TEST(0 == free_binarystack(&stack)) ;
   }

   // TEST push, pop
   uint8_t data1[3]             = { 1, 2, 3 } ;
   uint8_t data2[sizeof(data1)] = { 0, 0, 0 } ;
   TEST(0 == init_binarystack(&stack, 1)) ;
   TEST(1 == isempty_binarystack(&stack)) ;
   TEST(0 == push_binarystack(&stack, sizeof(data1), data1)) ;
   TEST(3 == stack.size) ;
   TEST(0 == isempty_binarystack(&stack)) ;
   TEST(0 == pop_binarystack(&stack, sizeof(data2), data2)) ;
   TEST(1 == isempty_binarystack(&stack)) ;
   TEST(0 == stack.size) ;
   for(unsigned i = 1; i <= 3; ++i) {
      TEST(i == data1[i-1]) ;
      TEST(i == data2[i-1]) ;
   }

   // TEST push 10000 words
   for(uint32_t i = 1; i <= 10000; ++i) {
      size_t newsize = stack.stackmem.size ;
      if (i*sizeof(i) > stack.stackmem.size) {
         newsize += pagesize_vm() ;
      }
      TEST(0 == push_binarystack(&stack, sizeof(i), &i)) ;
      TEST(i*sizeof(i) == stack.size) ;
      TEST(newsize == stack.stackmem.size) ;
      TEST(0 == isempty_binarystack(&stack)) ;
   }

   // TEST pop 10000 words
   size_t oldsize = stack.stackmem.size ;
   for(uint32_t i = 10000; i; --i) {
      uint32_t x ;
      TEST(i*sizeof(i) == stack.size) ;
      TEST(0 == pop_binarystack(&stack, sizeof(i), &x)) ;
      TEST(i == x) ;
      TEST(oldsize == stack.stackmem.size) ;
      TEST((i==1) == isempty_binarystack(&stack)) ;
   }
   TEST(0 == free_binarystack(&stack)) ;

   // TEST ENOMEM
   TEST(ENOMEM == init_binarystack(&stack, ~(size_t)0)) ;
   TEST(0 == init_binarystack(&stack, 1)) ;
   TEST(ENOMEM == push_binarystack(&stack, ~(size_t)0, data1)) ;
   TEST(0 == push_binarystack(&stack, sizeof(data1), data1)) ;
   TEST(ENOMEM == push_binarystack(&stack, ~(size_t)0, data1)) ;
   TEST(0 == free_binarystack(&stack)) ;

   return 0 ;
ABBRUCH:
   free_binarystack(&stack) ;
   return EINVAL ;
}

int unittest_ds_inmem_binarystack()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())       goto ABBRUCH ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif


