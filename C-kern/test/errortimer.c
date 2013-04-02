/* title: Test-Errortimer impl
   Implements <Test-Errortimer>.

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
   (C) 2011 Jörg Seebohn

   file: C-kern/api/test/errortimer.h
    Header file of <Test-Errortimer>.

   file: C-kern/test/errortimer.c
    Implementation file <Test-Errortimer impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/test/errortimer.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   test_errortimer_t  errtimer = test_errortimer_INIT_FREEABLE ;

   // TEST test_errortimer_INIT_FREEABLE
   TEST(0 == errtimer.timercount) ;
   TEST(0 == errtimer.errcode) ;

   // TEST init_testerrortimer
   init_testerrortimer(&errtimer, 123, 200) ;
   TEST(123 == errtimer.timercount) ;
   TEST(200 == errtimer.errcode) ;
   init_testerrortimer(&errtimer, 999, -20) ;
   TEST(999 == errtimer.timercount) ;
   TEST(-20 == errtimer.errcode) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_query(void)
{
   test_errortimer_t  errtimer = test_errortimer_INIT_FREEABLE ;

   // TEST isenabled_testerrortimer
   TEST(0 == isenabled_testerrortimer(&errtimer)) ;
   init_testerrortimer(&errtimer, 1, 0) ;
   TEST(1 == isenabled_testerrortimer(&errtimer)) ;
   init_testerrortimer(&errtimer, UINT32_MAX, 0) ;
   TEST(1 == isenabled_testerrortimer(&errtimer)) ;
   init_testerrortimer(&errtimer, 0, 20) ;
   TEST(0 == isenabled_testerrortimer(&errtimer)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}


static int test_update(void)
{
   test_errortimer_t  errtimer = test_errortimer_INIT_FREEABLE ;

   // TEST process_testerrortimer
   init_testerrortimer(&errtimer, 11, -2) ;
   for (int i = 1; i < 11; ++i) {
      // 1 .. 10th call does not fire
      TEST(0 == process_testerrortimer(&errtimer)) ;
      TEST(11-i == (int)errtimer.timercount) ;
   }
   // 11th call fires
   TEST(-2 == process_testerrortimer(&errtimer)) ;
   TEST(0  == (int)errtimer.timercount) ;
   TEST(-2 == errtimer.errcode) ;

   // TEST process_testerrortimer
   TEST(0 == process_testerrortimer(&errtimer)) ;
   TEST(0  == (int)errtimer.timercount) ;
   TEST(-2 == errtimer.errcode) ;

   // TEST ONERROR_testerrortimer
   int err = 0 ;
   init_testerrortimer(&errtimer, 2, 3) ;
   ONERROR_testerrortimer(&errtimer, ONABORT) ;
   TEST(0 == err) ;
   TEST(1 == errtimer.timercount) ;
   TEST(3 == errtimer.errcode) ;
   ONERROR_testerrortimer(&errtimer, XXX) ;  // sets err and jumps to XXX
   err = 10 ;
XXX:
   TEST(3 == err) ;
   TEST(0 == errtimer.timercount) ;
   TEST(3 == errtimer.errcode) ;
   ONERROR_testerrortimer(&errtimer, XXX2) ;  // does nothing
   err = 10 ;
XXX2:
   TEST(10== err) ;
   TEST(0 == errtimer.timercount) ;
   TEST(3 == errtimer.errcode) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_test_errortimer()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())    goto ONABORT ;
   if (test_query())       goto ONABORT ;
   if (test_update())      goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
