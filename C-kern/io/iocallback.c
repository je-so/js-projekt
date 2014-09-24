/* title: IOCallback impl

   Implements <IOCallback>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
#include "C-kern/api/test/unittest.h"
#endif



// group: test

#ifdef KONFIG_UNITTEST

typedef struct test_iocallback_handler_t     test_iocallback_handler_t ;

struct test_iocallback_handler_t {
   sys_iochannel_t   fd ;
   uint8_t           ioevents ;
} ;

iocallback_DECLARE(test_iocallback_t, test_iocallback_handler_t) ;

void iocallback_testiocallbackhandler(test_iocallback_handler_t * iohandler, sys_iochannel_t fd, uint8_t ioevents)
{
   iohandler->fd       = fd ;
   iohandler->ioevents = ioevents ;
}

static int test_initfree(void)
{
   iocallback_t iocb = iocallback_FREE ;

   // TEST iocallback_FREE
   TEST(0 == iocb.object) ;
   TEST(0 == iocb.iimpl) ;

   // TEST iocallback_INIT
   for (unsigned i = 1; i != 0; i <<= 1) {
      iocb = (iocallback_t) iocallback_INIT((void*)i, (iocallback_f)(3*i)) ;
      TEST(iocb.object == (void*)i) ;
      TEST(iocb.iimpl  == (iocallback_f)(3*i)) ;
   }

   return 0 ;
ONERR:
   return EINVAL ;
}

static int test_generic(void)
{
   test_iocallback_t iocb = iocallback_FREE ;

   typedef void   (* test_iocallback_f) (test_iocallback_handler_t*, sys_iochannel_t, uint8_t) ;

   // TEST iocallback_FREE
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

   // TEST genericcast_iocallback
   TEST((iocallback_t*)&iocb == genericcast_iocallback(&iocb, test_iocallback_handler_t)) ;
   for (unsigned i = 1; i != 0; i <<= 1) {
      TEST((iocallback_t*)i == genericcast_iocallback((test_iocallback_t*)i, test_iocallback_handler_t)) ;
   }

   return 0 ;
ONERR:
   return EINVAL ;
}

static int test_callback(void)
{
   test_iocallback_handler_t  iohandler = { 1, 1 } ;
   test_iocallback_t          iocb      = iocallback_INIT(&iohandler, &iocallback_testiocallbackhandler) ;
   iocallback_t               iocb2     = iocallback_INIT((iocallback_t*)&iohandler, (iocallback_f)&iocallback_testiocallbackhandler) ;

   // TEST call_iocallback
   for (sys_iochannel_t i = 0; i < 256; ++i) {
      call_iocallback(&iocb, i, (uint8_t)(255-i)) ;
      TEST(iohandler.fd       == i) ;
      TEST(iohandler.ioevents == (uint8_t)(255-i)) ;
   }
   for (int i = 0; i < 256; ++i) {
      call_iocallback(&iocb2, i, (uint8_t)(255-i)) ;
      TEST(iohandler.fd       == i) ;
      TEST(iohandler.ioevents == (uint8_t)(255-i)) ;
   }

   return 0 ;
ONERR:
   return EINVAL ;
}

int unittest_io_iocallback()
{
   if (test_initfree())       goto ONERR;
   if (test_generic())        goto ONERR;
   if (test_callback())       goto ONERR;

   return 0 ;
ONERR:
   return EINVAL ;
}

#endif
