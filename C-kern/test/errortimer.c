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

   // TEST static init
   TEST(0 == errtimer.timercount) ;
   TEST(0 == errtimer.errcode) ;

   // TEST init
   TEST(0 == init_testerrortimer(&errtimer, 123, 200)) ;
   TEST(123 == errtimer.timercount) ;
   TEST(200 == errtimer.errcode) ;

   // TEST process_testerrortimer
   TEST(0 == init_testerrortimer(&errtimer, 11, -2)) ;
   TEST(11 == errtimer.timercount) ;
   TEST(-2 == errtimer.errcode) ;
   for(int i = 1; i < 11; ++i) {
      // 1 .. 10th call does not fire
      TEST(0 == process_testerrortimer(&errtimer)) ;
      TEST(11-i == (int)errtimer.timercount) ;
   }
      // 11th call fires
   TEST(-2 == process_testerrortimer(&errtimer)) ;
      // disarmed
   TEST(0 == (int)errtimer.timercount) ;
   TEST(-2 == errtimer.errcode) ;
   TEST(0 == process_testerrortimer(&errtimer)) ;

   // TEST ONERROR_testerrortimer
   int err = 0 ;
   TEST(0 == init_testerrortimer(&errtimer, 2, 3)) ;
   ONERROR_testerrortimer(&errtimer, ABBRUCH) ;
   TEST(0 == err) ;
   TEST(1 == errtimer.timercount) ;
   TEST(3 == errtimer.errcode) ;
   ONERROR_testerrortimer(&errtimer, XXX) ;  // sets err and jumps to XXX
   err = 10 ;
XXX:
   TEST(3 == err) ;
   TEST(0 == errtimer.timercount) ;
   TEST(3 == errtimer.errcode) ;

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

int unittest_test_errortimer()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())   goto ABBRUCH ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
