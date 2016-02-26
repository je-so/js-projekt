/* title: Test-Errortimer impl
   Implements <Test-Errortimer>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
#include "C-kern/api/test/unittest.h"
#endif


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   test_errortimer_t  errtimer = test_errortimer_FREE;

   // TEST test_errortimer_FREE
   TEST(0 == errtimer.timercount);
   TEST(0 == errtimer.errcode);

   // TEST init_testerrortimer
   init_testerrortimer(&errtimer, 123, 200);
   TEST(123 == errtimer.timercount);
   TEST(200 == errtimer.errcode);
   init_testerrortimer(&errtimer, 999, -20);
   TEST(999 == errtimer.timercount);
   TEST(-20 == errtimer.errcode);

   // TEST free_testerrortimer
   free_testerrortimer(&errtimer);
   TEST(0 == errtimer.timercount);
   TEST(0 == errtimer.errcode);

   return 0;
ONERR:
   return EINVAL;
}

static int test_query(void)
{
   test_errortimer_t  errtimer = test_errortimer_FREE;

   // TEST isenabled_testerrortimer
   TEST(0 == isenabled_testerrortimer(&errtimer));
   for (int err = 0; err < 2; ++err) {
      init_testerrortimer(&errtimer, 0, err);
      TEST(0 == isenabled_testerrortimer(&errtimer));
      init_testerrortimer(&errtimer, 1, err);
      TEST(1 == isenabled_testerrortimer(&errtimer));
      init_testerrortimer(&errtimer, UINT32_MAX, err);
      TEST(1 == isenabled_testerrortimer(&errtimer));
   }

   // TEST errcode_testerrortimer
   free_testerrortimer(&errtimer);
   TEST(0 == errcode_testerrortimer(&errtimer));
   for (int i = 0; i < 10; ++i) {
      init_testerrortimer(&errtimer, 0, i);
      TEST(i == errcode_testerrortimer(&errtimer));
      init_testerrortimer(&errtimer, 1, i);
      TEST(i == errcode_testerrortimer(&errtimer));
   }

   return 0;
ONERR:
   return EINVAL;
}


static int test_update(void)
{
   int err;
   test_errortimer_t errtimer = test_errortimer_FREE;

   // TEST process_testerrortimer
   err = 1;
   init_testerrortimer(&errtimer, 11, -2);
   for (unsigned i = 1; i < 11; ++i) {
      // 1 .. 10th call does not fire
      TEST(0 == process_testerrortimer(&errtimer, &err));
      // check err not changed
      TEST(1 == err);
      // check errtimer
      TEST(11-i == errtimer.timercount);
      TEST(-2   == errtimer.errcode);
   }
   // 11th call fires
   TEST(-2 == process_testerrortimer(&errtimer, &err));
   TEST(-2 == err);
   TEST(0  == errtimer.timercount);
   TEST(-2 == errtimer.errcode);

   // TEST process_testerrortimer: already expired timer
   err = 1;
   TEST(0 == process_testerrortimer(&errtimer, &err));
   TEST(1 == err);
   TEST(0 == errtimer.timercount);
   TEST(-2 == errtimer.errcode);

   // TEST PROCESS_testerrortimer: timer not expired
   err = 0;
   init_testerrortimer(&errtimer, 2, 5);
   TEST(0 == PROCESS_testerrortimer(&errtimer, &err));
   TEST(0 == err);
   TEST(1 == errtimer.timercount);

   // TEST PROCESS_testerrortimer: timer expires
   TEST(5 == PROCESS_testerrortimer(&errtimer, &err));
   TEST(5 == err);
   TEST(0 == errtimer.timercount);

   // TEST PROCESS_testerrortimer: already expired
   err = 0;
   TEST(0 == PROCESS_testerrortimer(&errtimer, &err));
   TEST(0 == err);
   TEST(0 == errtimer.timercount);
   TEST(5 == errtimer.errcode);

   return 0;
ONERR:
   return EINVAL;
}

int unittest_test_errortimer()
{
   if (test_initfree())    goto ONERR;
   if (test_query())       goto ONERR;
   if (test_update())      goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
