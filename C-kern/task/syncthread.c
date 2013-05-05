/* title: SyncThread impl

   Implements <SyncThread>.

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

   file: C-kern/api/task/syncthread.h
    Header file <SyncThread>.

   file: C-kern/task/syncthread.c
    Implementation file <SyncThread impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/task/syncthread.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


// section: syncthread_t

// group: query

bool isfree_syncthread(const syncthread_t * sthread)
{
   return sthread->mainfct == 0 && sthread->state == 0 ;
}



// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   syncthread_t sthread = syncthread_INIT_FREEABLE ;

   // TEST syncthread_INIT_FREEABLE
   TEST(0 == sthread.mainfct) ;
   TEST(0 == sthread.state) ;

   // TEST init_syncthread
   for (uintptr_t i = 1; i; i <<= 1) {
      init_syncthread(&sthread, (syncthread_f)i, (void*)(i+1)) ;
      TEST(sthread.mainfct == (syncthread_f)i) ;
      TEST(sthread.state   == (void*)(i+1)) ;
   }

   // TEST free_syncthread
   TEST(sthread.mainfct != 0) ;
   TEST(sthread.state   != 0) ;
   free_syncthread(&sthread) ;
   TEST(sthread.mainfct == 0) ;
   TEST(sthread.state   == 0) ;
   free_syncthread(&sthread) ;
   TEST(sthread.mainfct == 0) ;
   TEST(sthread.state   == 0) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_query(void)
{
   syncthread_t sthread = syncthread_INIT_FREEABLE ;

   // TEST state_syncthread
   TEST(0 == state_syncthread(&sthread)) ;
   for (uintptr_t i = 1; i; i <<= 1) {
      sthread.state = (void*) i ;
      TEST(state_syncthread(&sthread) == (void*)i) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_execstate(void)
{
   syncthread_t   sthread = syncthread_INIT_FREEABLE ;
   void *         label ;
   volatile int   jumpflag ;

   // TEST setcontinuelabel_syncthread
TESTLABEL1:
   label = __extension__ && TESTLABEL1 ;
   setcontinuelabel_syncthread(&sthread, TESTLABEL1) ;
   TEST(sthread.state == label) ;
TESTLABEL2:
   label = __extension__ && TESTLABEL2 ;
   setcontinuelabel_syncthread(&sthread, TESTLABEL2) ;
   TEST(sthread.state == label) ;

   // TEST continue_syncthread
   jumpflag = 0 ;
   setcontinuelabel_syncthread(&sthread, SET_JUMPFLAG) ;
   continue_syncthread(&sthread) ;
   goto TEST_JUMPFLAG;
SET_JUMPFLAG:
   ++ jumpflag ;
TEST_JUMPFLAG:
   TEST(jumpflag) ;
   while (jumpflag < 10) {
      continue_syncthread(&sthread) ;
   }
   TEST(10 == jumpflag) ;

   // TEST setstate_syncthread
   sthread.mainfct = (syncthread_f)0 ;
   setstate_syncthread(&sthread, (void*)5) ;
   TEST(sthread.mainfct == (syncthread_f)0) ;
   TEST(sthread.state   == (void*)5) ;
   setstate_syncthread(&sthread, (void*)1) ;
   TEST(sthread.mainfct == (syncthread_f)0) ;
   TEST(sthread.state   == (void*)1) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_signalstate(void)
{
   volatile uint32_t    signalstate ;
   volatile int         jumpflag ;

   // TEST handlesignal_syncthread: syncthread_signal_NULL
   jumpflag    = 0 ;
   signalstate = syncthread_signal_NULL ;
   handlesignal_syncthread(signalstate, 0, ONABORT, ONRUN0, ONABORT) ;
   goto TEST0 ;
ONRUN0:
   jumpflag = 99 ;
TEST0:
   TEST(99 == jumpflag) ;

   // TEST handlesignal_syncthread: syncthread_signal_WAKEUP
   void * wakeuplabel = (__extension__ ({ && ONWAKEUP1 ; })) ;
   jumpflag    = 0 ;
   signalstate = syncthread_signal_WAKEUP ;
   handlesignal_syncthread(signalstate, wakeuplabel, ONABORT, ONRUN0, ONABORT) ;
   goto TEST1 ;
ONWAKEUP1:
   jumpflag = 1 ;
TEST1:
   TEST(1 == jumpflag) ;

   // TEST handlesignal_syncthread: syncthread_signal_INIT
   jumpflag    = 0 ;
   signalstate = syncthread_signal_INIT ;
   handlesignal_syncthread(signalstate, 0, ONINIT2, ONABORT, ONABORT) ;
   goto TEST2 ;
ONINIT2:
   jumpflag = 2 ;
TEST2:
   TEST(2 == jumpflag) ;

   // TEST handlesignal_syncthread: syncthread_signal_ABORT
   jumpflag    = 0 ;
   signalstate = syncthread_signal_ABORT ;
   handlesignal_syncthread(signalstate, 0, ONABORT, ONABORT, ONABORT3) ;
   goto TEST3 ;
ONABORT3:
   jumpflag = 3 ;
TEST3:
   TEST(3 == jumpflag) ;

   // TEST handlesignal_syncthread: invalid value same as syncthread_signal_ABORT
   jumpflag    = 0 ;
   signalstate = UINT32_MAX ;
   handlesignal_syncthread(signalstate, 0, ONABORT, ONABORT, ONABORT4) ;
   goto TEST4 ;
ONABORT4:
   jumpflag = 4 ;
TEST4:
   TEST(4 == jumpflag) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int maintest_syncthread(syncthread_t * sthread, uint32_t signalstate)
{
   handlesignal_syncthread(signalstate, __extension__ && ONWAKEUP, ONINIT, ONRUN, ONABORT) ;

ONINIT:
   *(int*)state_syncthread(sthread) = *(int*)state_syncthread(sthread) + 1 ;
   return 1 ;

ONRUN:
   *(int*)state_syncthread(sthread) = *(int*)state_syncthread(sthread) + 2 ;
   return 2 ;

ONWAKEUP:
   *(int*)state_syncthread(sthread) = *(int*)state_syncthread(sthread) + 3 ;
   return 3 ;

ONABORT:
   *(int*)state_syncthread(sthread) = *(int*)state_syncthread(sthread) + 4 ;
   return 4 ;
}

static int test_callconvention(void)
{
   syncthread_t   sthread = syncthread_INIT_FREEABLE ;
   int            var ;

   // TEST callinit_syncthread
   for (int i = 0; i < 10; ++i) {
      var = i ;
      init_syncthread(&sthread, &maintest_syncthread, &var) ;
      TEST(1 == callinit_syncthread(&sthread)) ;
      TEST(var == i+1) ;
   }

   // TEST callrun_syncthread
   for (int i = 0; i < 10; ++i) {
      var = i ;
      init_syncthread(&sthread, &maintest_syncthread, &var) ;
      TEST(2 == callrun_syncthread(&sthread)) ;
      TEST(var == i+2) ;
   }

   // TEST callwakeup_syncthread
   for (int i = 0; i < 10; ++i) {
      var = i ;
      init_syncthread(&sthread, &maintest_syncthread, &var) ;
      TEST(3 == callwakeup_syncthread(&sthread)) ;
      TEST(var == i+3) ;
   }

   // TEST callabort_syncthread
   for (int i = 0; i < 10; ++i) {
      var = i ;
      init_syncthread(&sthread, &maintest_syncthread, &var) ;
      TEST(4 == callabort_syncthread(&sthread)) ;
      TEST(var == i+4) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_task_syncthread()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())       goto ONABORT ;
   if (test_query())          goto ONABORT ;
   if (test_execstate())      goto ONABORT ;
   if (test_signalstate())    goto ONABORT ;
   if (test_callconvention()) goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
