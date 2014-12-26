/* title: SyncFunc impl

   Implementiere <SyncFunc>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/task/syncfunc.h
    Header file <SyncFunc>.

   file: C-kern/task/syncfunc.c
    Implementation file <SyncFunc impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/task/syncfunc.h"
#include "C-kern/api/task/synccmd.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif

// section: syncfunc_t

// group: update

void relink_syncfunc(syncfunc_t* sfunc)
{
   const int waitop = (sfunc->optflags & syncfunc_opt_WAITFIELDS);

   if (waitop) {
      linkd_t * waitlist = waitlist_syncfunc(sfunc);
      if (isvalid_linkd(waitlist)) {
         relink_linkd(waitlist);
      }
   }
}

void unlink_syncfunc(syncfunc_t* sfunc)
{
   const int waitop = (sfunc->optflags & syncfunc_opt_WAITFIELDS);

   if (waitop) {
      linkd_t * waitlist = waitlist_syncfunc(sfunc);
      if (isvalid_linkd(waitlist)) {
         unlink0_linkd(waitlist);
      }
   }
}


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_sfparam(void)
{
   struct syncrunner_t *  R  = (struct syncrunner_t*)1;
   syncfunc_param_t sfparam  = syncfunc_param_FREE;
   syncfunc_param_t sfparam2 = (syncfunc_param_t) syncfunc_param_INIT(R);

   // TEST syncfunc_param_FREE
   TEST(0 == sfparam.srun);
   TEST(0 == sfparam.sfunc);
   TEST(0 == sfparam.contoffset);
   TEST(0 == sfparam.state);
   TEST(0 == sfparam.condition);
   TEST(0 == sfparam.err);

   // TEST syncfunc_param_INIT
   TEST(R == sfparam2.srun);
   TEST(0 == sfparam.sfunc);
   TEST(0 == sfparam2.contoffset);
   TEST(0 == sfparam2.state);
   TEST(0 == sfparam2.condition);
   TEST(0 == sfparam2.err);

   return 0;
ONERR:
   return EINVAL;

}

// forward
static int test_start_sf(syncfunc_param_t * sfparam, uint32_t sfcmd);

static int test_initfree(void)
{
   syncfunc_t sfunc  = syncfunc_FREE;
   syncfunc_t sfunc2 = syncfunc_FREE;

   // TEST syncfunc_FREE
   TEST(0 == sfunc.mainfct);
   TEST(0 == sfunc.state);
   TEST(0 == sfunc.contoffset);
   TEST(0 == sfunc.optflags);
   TEST(0 == sfunc.waitresult);
   TEST(0 == sfunc.waitlist.prev);
   TEST(0 == sfunc.waitlist.next);

   // TEST init_syncfunc
   for (syncfunc_opt_e opt = 0; opt <= syncfunc_opt_ALL; ++opt) {
      for (uintptr_t state = 0; state != (uintptr_t)-1; state <<= 1, ++state) {
         memset(&sfunc, 255, sizeof(sfunc));
         init_syncfunc(&sfunc, &test_start_sf, (void*) state, opt);
         TEST(sfunc.mainfct    == &test_start_sf);
         TEST(sfunc.state      == (void*) state);
         TEST(sfunc.contoffset == 0);
         TEST(sfunc.optflags   == opt);
         // not initialised
         TEST(sfunc.waitresult    != 0);
         TEST(sfunc.waitlist.prev != 0);
         TEST(sfunc.waitlist.next != 0);
      }
   }

   // TEST init2_syncfunc
   for (syncfunc_opt_e opt = 0; opt <= syncfunc_opt_ALL; ++opt) {
      for (syncfunc_opt_e opt2 = 0; opt2 <= syncfunc_opt_ALL; ++opt2) {
         uint16_t size  = getsize_syncfunc(opt);
         uint16_t size2 = getsize_syncfunc(opt2);
         void*    state = (void*) (uintptr_t) (256 * opt + opt2);
         uint16_t contoff = (uint16_t) (11 + opt + opt2);
         memset(&sfunc, 255, sizeof(sfunc));
         memset(&sfunc2, 0, sizeof(sfunc));
         init_syncfunc(&sfunc, &test_start_sf, state, opt);
         init2_syncfunc(&sfunc2, size2, contoff, opt2, state, &sfunc, size);
         TEST(sfunc2.mainfct    == &test_start_sf);
         TEST(sfunc2.state      == state);
         TEST(sfunc2.contoffset == contoff);
         TEST(sfunc2.optflags   == opt2);
         // not initialised
         if (opt2 & syncfunc_opt_WAITFIELDS) {
            TEST(0 == sfunc2.waitresult);
            TEST(0 == sfunc2.waitlist.prev);
            TEST(0 == sfunc2.waitlist.next);
         }
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_getset(void)
{
   syncfunc_t sfunc = syncfunc_FREE;
   syncfunc_t sfunc2 = syncfunc_FREE;

   // TEST getsize_syncfunc
   for (syncfunc_opt_e opt = 0; opt <= syncfunc_opt_ALL; ++opt) {
      uint16_t size   = getsize_syncfunc(opt);
      uint16_t expect = (0 != (opt & syncfunc_opt_WAITFIELDS)) ? sizeof(syncfunc_t) : offsetof(syncfunc_t, waitresult);
      TEST(expect == size);
   }

   // TEST waitlist_syncfunc
   TEST(waitlist_syncfunc(&sfunc)  == &sfunc.waitlist);

   // TEST castPwaitlist_syncfunc: Invalid value 0 ==> return invalid address != 0
   TEST(0 != castPwaitlist_syncfunc(0));

   // TEST castPwaitlist_syncfunc: correct value
   TEST(&sfunc == castPwaitlist_syncfunc(waitlist_syncfunc(&sfunc)));

   // TEST castPwaitlist_syncfunc: connected link
   init_linkd(&sfunc.waitlist, &sfunc2.waitlist);
   TEST(&sfunc == castPwaitlist_syncfunc(sfunc2.waitlist.next));
   TEST(&sfunc == castPwaitlist_syncfunc(sfunc2.waitlist.prev));

   // TEST waitresult_syncfunc
   sfunc = (syncfunc_t) syncfunc_FREE;
   for (syncfunc_opt_e opt = 0; opt <= syncfunc_opt_ALL; ++opt) {
      for (int result = -10; result <= 10; ++result) {
         int expect = (opt & syncfunc_opt_WAITFIELDS) ? result : 0;
         sfunc.optflags = opt;
         sfunc.waitresult = result;
         TEST(expect == waitresult_syncfunc(&sfunc));
         // no fields changed
         TEST(sfunc.optflags   == opt);
         TEST(sfunc.waitresult == result);
      }
   }

   // TEST setwaitresult_syncfunc
   sfunc = (syncfunc_t) syncfunc_FREE;
   for (syncfunc_opt_e opt = 0; opt <= syncfunc_opt_ALL; ++opt) {
      for (int result = -10; result <= 10; ++result) {
         sfunc.optflags = opt;
         setwaitresult_syncfunc(&sfunc, result);
         TEST(sfunc.optflags   == opt);
         TEST(sfunc.waitresult == result);
      }
   }

   // TEST relink_syncfunc
   for (syncfunc_opt_e opt = 0; opt <= syncfunc_opt_ALL; ++opt) {
      const int iswait = (opt & syncfunc_opt_WAITFIELDS);

      sfunc  = (syncfunc_t) syncfunc_FREE;
      sfunc2 = (syncfunc_t) syncfunc_FREE;
      sfunc.optflags = opt;

      // test relink_syncfunc: invalid links
      relink_syncfunc(&sfunc);
      TEST(0 == sfunc.waitresult);
      TEST(!isvalid_linkd(&sfunc.waitlist));
      TEST(0 == sfunc2.waitresult);
      TEST(!isvalid_linkd(&sfunc2.waitlist));

      // test relink_syncfunc: valid links
      sfunc.waitlist.prev = &sfunc2.waitlist;
      sfunc.waitlist.next = &sfunc2.waitlist;
      relink_syncfunc(&sfunc);
      // check sfunc not changed
      TEST(sfunc.waitresult == 0);
      TEST(sfunc.waitlist.prev == &sfunc2.waitlist);
      TEST(sfunc.waitlist.next == &sfunc2.waitlist);
      // check sfunc2 adapted if iswait != 0
      TEST(sfunc2.waitresult == 0);
      if (iswait) {
         TEST(sfunc2.waitlist.prev == &sfunc.waitlist);
         TEST(sfunc2.waitlist.next == &sfunc.waitlist);
      } else {
         TEST(! isvalid_linkd(&sfunc2.waitlist));
      }
   }

   // TEST unlink_syncfunc
   for (syncfunc_opt_e opt = 0; opt <= syncfunc_opt_ALL; ++opt) {
      const int  iswait = (opt & syncfunc_opt_WAITFIELDS);

      sfunc  = (syncfunc_t) syncfunc_FREE;
      sfunc2 = (syncfunc_t) syncfunc_FREE;
      sfunc.optflags = opt;

      // test unlink_syncfunc: invalid links (nothing is done)
      unlink_syncfunc(&sfunc);
      TEST(0 == sfunc.waitresult);
      TEST(!isvalid_linkd(&sfunc.waitlist));
      TEST(0 == sfunc2.waitresult);
      TEST(!isvalid_linkd(&sfunc2.waitlist));

      // test unlink_syncfunc: valid links
      init_linkd(&sfunc.waitlist, &sfunc2.waitlist);
      unlink_syncfunc(&sfunc);
      // check sfunc is not changed
      TEST(sfunc.waitresult    == 0);
      TEST(sfunc.waitlist.prev == &sfunc2.waitlist);
      TEST(sfunc.waitlist.next == &sfunc2.waitlist);
      // check sfunc2 waitlist invalidated if iswait != 0
      TEST(sfunc2.waitresult == 0);
      if (iswait) {
         TEST(! isvalid_linkd(&sfunc2.waitlist));
      } else {
         TEST(isvalid_linkd(&sfunc2.waitlist));
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_start_sf(syncfunc_param_t* sfparam, uint32_t sfcmd)
{
   start_syncfunc(sfparam, sfcmd, ONRUN, ONEXIT);

   // is executed in case of wrong sfcmd value
   sfparam->err = -1;
   return -1;

ONRUN:
   sfparam->contoffset = (uint16_t) __extension__ ((uintptr_t) &&ONCONTINUE - (uintptr_t) &&syncfunc_START);
   sfparam->err = 10 + synccmd_RUN;
   return synccmd_RUN;

ONCONTINUE:
   static_assert(synccmd_CONTINUE > synccmd_RUN, "must run after continue");
   sfparam->err = 10 + synccmd_CONTINUE;
   return synccmd_CONTINUE;

ONEXIT:
   sfparam->err = 10 + synccmd_EXIT;
   return synccmd_EXIT;
}

static int test_exit_sf(syncfunc_param_t * sfparam, uint32_t sfcmd)
{
   exit_syncfunc(sfparam, (int)sfcmd);
}

static int test_wait_sf(syncfunc_param_t * sfparam, uint32_t sfcmd)
{
   start_syncfunc(sfparam, sfcmd, ONRUN, ONERR);

   goto ONERR;

ONRUN: ;
   int err = wait_syncfunc(sfparam, (void*)1);
   if (err) goto ONERR;
   err = wait_syncfunc(sfparam, (void*)2);
   if (err) goto ONERR;
   err = wait_syncfunc(sfparam, (void*)3);
   if (err) goto ONERR;
   return synccmd_EXIT;

ONERR:
   return -1;
}

static int test_waiterr_sf(syncfunc_param_t * sfparam, uint32_t sfcmd)
{
   start_syncfunc(sfparam, sfcmd, ONRUN, ONERR);

   goto ONERR;

ONRUN: ;
   intptr_t err = sfparam->err;
   err = wait_syncfunc(sfparam, (void*)err);
   err = wait_syncfunc(sfparam, (void*)err);
   err = wait_syncfunc(sfparam, (void*)err);
   return synccmd_EXIT;

ONERR:
   return -1;
}

static int test_yield_sf(syncfunc_param_t * sfparam, uint32_t sfcmd)
{
   start_syncfunc(sfparam, sfcmd, ONRUN, ONERR);

   goto ONERR;

ONRUN:
   sfparam->err += 19;
   yield_syncfunc(sfparam);
   sfparam->err += 20;
   yield_syncfunc(sfparam);
   sfparam->err += 21;
   sfparam->contoffset = 0;
   return synccmd_EXIT;

ONERR:
   return -1;
}

static int test_implsupport(void)
{
   syncfunc_param_t sfparam = syncfunc_param_FREE;

   // TEST state_syncfunc
   TEST(0 == state_syncfunc(&sfparam));
   for (uintptr_t i = 1; i; i <<= 1) {
      sfparam.state = (void*) i;
      TEST((void*)i == state_syncfunc(&sfparam));
   }

   // TEST setstate_syncfunc
   memset(&sfparam, 0, sizeof(sfparam));
   for (uintptr_t i = 1; i; i <<= 1) {
      setstate_syncfunc(&sfparam, (void*) i);
      TEST((void*)i == state_syncfunc(&sfparam));
   }
   setstate_syncfunc(&sfparam, 0);
   TEST(0 == state_syncfunc(&sfparam));

   // TEST start_syncfunc: valid sfcmd values
   static_assert(synccmd_RUN == 0 && synccmd_EXIT== 2, "3 different sfcmd");
   for (int cmd = synccmd_RUN; cmd <= synccmd_EXIT; ++cmd) {
      sfparam.err = 0;
      TEST(cmd == test_start_sf(&sfparam, (uint32_t)cmd));
      TEST(sfparam.srun       == 0);
      TEST(sfparam.contoffset != 0);
      TEST(sfparam.state      == 0);
      TEST(sfparam.condition  == 0);
      TEST(sfparam.err        == 10 + cmd);
   }

   // TEST start_syncfunc: invalid sfcmd value
   sfparam.contoffset = 0;
   for (int cmd = synccmd_WAIT; cmd <= synccmd_WAIT+16; ++cmd) {
      sfparam.err = 0;
      TEST(-1 == test_start_sf(&sfparam, (uint32_t)cmd));
      TEST( 0 == sfparam.srun);
      TEST( 0 == sfparam.contoffset);
      TEST( 0 == sfparam.state);
      TEST( 0 == sfparam.condition);
      TEST(-1 == sfparam.err);
   }

   // TEST exit_syncfunc
   for (uint32_t cmd = 0; cmd <= 100000; cmd += 10000) {
      sfparam.err = -1;
      TEST(synccmd_EXIT == test_exit_sf(&sfparam, cmd));
      TEST(sfparam.srun       == 0);
      TEST(sfparam.contoffset == 0);
      TEST(sfparam.state      == 0);
      TEST(sfparam.condition  == 0);
      TEST(sfparam.err        == (int)cmd);
   }

   // TEST wait_syncfunc: waiterr == 0
   memset(&sfparam, 0, sizeof(sfparam));
   for (intptr_t i = 1; i <= 4; ++i) {
      uint16_t oldoff = sfparam.contoffset;
      int      result = i != 4 ? synccmd_WAIT : synccmd_EXIT;
      sfparam.condition = 0;
      TEST(result == test_wait_sf(&sfparam, i == 1 ? synccmd_RUN : synccmd_CONTINUE));
      TEST(0 == sfparam.srun);
      TEST(0 == sfparam.state);
      if (i != 4) {
         TEST(oldoff   != sfparam.contoffset);
         TEST((void*)i == sfparam.condition);
      } else {
         TEST(oldoff == sfparam.contoffset);
         TEST(0      == sfparam.condition);
      }
      TEST(0 == sfparam.err);
   }

   // TEST wait_syncfunc: waiterr != 0
   memset(&sfparam, 0, sizeof(sfparam));
   for (intptr_t i = 1; i <= 4; ++i) {
      uint16_t oldoff = sfparam.contoffset;
      int      result = i != 4 ? synccmd_WAIT : synccmd_EXIT;
      sfparam.condition = 0;
      sfparam.err       = i;
      TEST(result == test_waiterr_sf(&sfparam, i == 1 ? synccmd_RUN : synccmd_CONTINUE));
      TEST(0 == sfparam.srun);
      TEST(0 == sfparam.state);
      if (i != 4) {
         TEST(oldoff   != sfparam.contoffset);
         TEST((void*)i == sfparam.condition);
      } else {
         TEST(oldoff == sfparam.contoffset);
         TEST(0      == sfparam.condition);
      }
      TEST(i == sfparam.err);
   }

   // TEST yield_syncfunc
   memset(&sfparam, 0, sizeof(sfparam));
   for (int i = 19; i <= 21; ++i) {
      uint16_t oldoff = sfparam.contoffset;
      int      result = i != 21 ? synccmd_CONTINUE : synccmd_EXIT;
      sfparam.err = 0;
      TEST(result == test_yield_sf(&sfparam, i == 19 ? synccmd_RUN : synccmd_CONTINUE));
      TEST(0 == sfparam.srun);
      TEST(0 == sfparam.state);
      TEST(oldoff != sfparam.contoffset);
      TEST(0 == sfparam.condition);
      TEST(i == sfparam.err);
   }

   return 0;
ONERR:
   return EINVAL;
}

int unittest_task_syncfunc()
{
   if (test_sfparam())        goto ONERR;
   if (test_initfree())       goto ONERR;
   if (test_getset())         goto ONERR;
   if (test_implsupport())    goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
