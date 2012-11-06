/* title: IOCallback impl

   Implements <IOCallback>.

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

   file: C-kern/api/io/iocallback.h
    Header file <IOCallback>.

   file: C-kern/io/iocallback.c
    Implementation file <IOCallback impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/iocallback.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif



// group: test

#ifdef KONFIG_UNITTEST

typedef struct test_iocallback_handler_t     test_iocallback_handler_t ;

struct test_iocallback_handler_t {
   sys_filedescr_t   fd ;
   uint8_t           ioevents ;
} ;

iocallback_DECLARE(test_iocallback_t, test_iocallback_handler_t) ;

void iocallback_testiocallbackhandler(test_iocallback_handler_t * iohandler, sys_filedescr_t fd, uint8_t ioevents)
{
   iohandler->fd       = fd ;
   iohandler->ioevents = ioevents ;
}

static int test_initfree(void)
{
   iocallback_t iocb = iocallback_INIT_FREEABLE ;

   // TEST iocallback_INIT_FREEABLE
   TEST(0 == iocb.object) ;
   TEST(0 == iocb.iimpl) ;

   // TEST iocallback_INIT
   for (unsigned i = 1; i != 0; i <<= 1) {
      iocb = (iocallback_t) iocallback_INIT((void*)i, (iocallback_f)(3*i)) ;
      TEST(iocb.object == (void*)i) ;
      TEST(iocb.iimpl  == (iocallback_f)(3*i)) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_generic(void)
{
   test_iocallback_t iocb = iocallback_INIT_FREEABLE ;

   typedef void   (* test_iocallback_f) (test_iocallback_handler_t*, sys_filedescr_t, uint8_t) ;

   // TEST iocallback_INIT_FREEABLE
   TEST(0 == iocb.object) ;
   TEST(0 == iocb.iimpl) ;

   // TEST iocallback_INIT
   for (unsigned i = 1; i != 0; i <<= 1) {
      iocb = (test_iocallback_t) iocallback_INIT((test_iocallback_handler_t*)(3*i), (test_iocallback_f)i) ;
      TEST(iocb.object == (test_iocallback_handler_t*)(3*i)) ;
      TEST(iocb.iimpl  == (test_iocallback_f)i) ;
   }

   // TEST iocallback_DECLARE
   static_assert(sizeof(test_iocallback_t) == sizeof(iocallback_t), "iocallback_DECLARE declares compatible type") ;
   static_assert(offsetof(test_iocallback_t, object) == offsetof(iocallback_t, object), "iocallback_DECLARE declares compatible type") ;
   static_assert(offsetof(test_iocallback_t, iimpl) == offsetof(iocallback_t, iimpl), "iocallback_DECLARE declares compatible type") ;

   // TEST asgeneric_iocallback
   TEST((iocallback_t*)&iocb == asgeneric_iocallback(&iocb, test_iocallback_handler_t)) ;
   for (unsigned i = 1; i != 0; i <<= 1) {
      TEST((iocallback_t*)i == asgeneric_iocallback((test_iocallback_t*)i, test_iocallback_handler_t)) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_callback(void)
{
   test_iocallback_handler_t  iohandler = { 1, 1 } ;
   test_iocallback_t          iocb      = iocallback_INIT(&iohandler, &iocallback_testiocallbackhandler) ;
   iocallback_t               iocb2     = iocallback_INIT((iocallback_t*)&iohandler, (iocallback_f)&iocallback_testiocallbackhandler) ;

   // TEST call_iocallback
   for (unsigned i = 0; i < 256; ++i) {
      call_iocallback(&iocb, (sys_filedescr_t)i, (uint8_t)(255-i)) ;
      TEST((sys_filedescr_t)i == iohandler.fd) ;
      TEST((uint8_t)(255-i)   == iohandler.ioevents) ;
   }
   for (unsigned i = 0; i < 256; ++i) {
      call_iocallback(&iocb2, (sys_filedescr_t)i, (uint8_t)(255-i)) ;
      TEST((sys_filedescr_t)i == iohandler.fd) ;
      TEST((uint8_t)(255-i)   == iohandler.ioevents) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_io_iocallback()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())       goto ONABORT ;
   if (test_generic())        goto ONABORT ;
   if (test_callback())       goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
