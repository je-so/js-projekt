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
   const int waitop = (sfunc->optflags & syncfunc_opt_WAIT_MASK);

   if (waitop) {
      if (syncfunc_opt_WAIT_RESULT != waitop) {
         link_t* waitfor = addrwaitfor_syncfunc(sfunc);
         if (isvalid_link(waitfor)) {
            relink_link(waitfor);
         }
      }

      linkd_t * waitlist = waitlist_syncfunc(sfunc);
      if (isvalid_linkd(waitlist)) {
         relink_linkd(waitlist);
      }
   }

}

void unlink_syncfunc(syncfunc_t* sfunc)
{
   const int waitop = (sfunc->optflags & syncfunc_opt_WAIT_MASK);

   if (waitop) {
      static_assert(syncfunc_opt_WAIT_RESULT == 1 && syncfunc_opt_WAIT_MASK == 3,
                  "syncfunc_opt_WAIT_RESULT is smallest possible value of syncfunc_opt_WAIT_MASK");
      if (syncfunc_opt_WAIT_RESULT < waitop) {
         link_t * waitfor = addrwaitfor_syncfunc(sfunc);
         if (isvalid_link(waitfor)) {
            unlink_link(waitfor);
         }
      }

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
   TEST(0 == sfparam.syncrun);
   TEST(0 == sfparam.contoffset);
   TEST(0 == sfparam.state);
   TEST(0 == sfparam.condition);
   TEST(0 == sfparam.err);

   // TEST syncfunc_param_INIT
   TEST(R == sfparam2.syncrun);
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
   TEST(0 == sfunc.waitfor.link);
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
         TEST(sfunc.waitfor.link  != 0);
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
         if (opt2 & syncfunc_opt_WAIT_MASK) {
            TEST(0 == sfunc2.waitfor.link);
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
      uint16_t expect = (0 != (opt & syncfunc_opt_WAIT_MASK)) ? sizeof(syncfunc_t) : offsetof(syncfunc_t, waitfor);
      TEST(expect == size);
   }

   // TEST addrwaitfor_syncfunc
   TEST(&sfunc.waitfor == addrwaitfor_syncfunc(&sfunc));

   // TEST waitlist_syncfunc
   TEST(waitlist_syncfunc(&sfunc)  == &sfunc.waitlist);

   // TEST castPwaitfor_syncfunc: Invalid value 0 ==> return invalid address != 0
   TEST(0 != castPwaitfor_syncfunc(0));

   // TEST castPwaitlist_syncfunc: Invalid value 0 ==> return invalid address != 0
   TEST(0 != castPwaitlist_syncfunc(0));

   // TEST castPwaitfor_syncfunc: correct value
   TEST(&sfunc == castPwaitfor_syncfunc(addrwaitfor_syncfunc(&sfunc)));

   // TEST castPwaitlist_syncfunc: correct value
   TEST(&sfunc == castPwaitlist_syncfunc(waitlist_syncfunc(&sfunc)));

   // TEST castPwaitfor_syncfunc: connected link
   link_t waitedfor;
   init_link(&sfunc.waitfor, &waitedfor);
   TEST(&sfunc == castPwaitfor_syncfunc(waitedfor.link));

   // TEST castPwaitlist_syncfunc: connected link
   init_linkd(&sfunc.waitlist, &sfunc2.waitlist);
   TEST(&sfunc == castPwaitlist_syncfunc(sfunc2.waitlist.next));
   TEST(&sfunc == castPwaitlist_syncfunc(sfunc2.waitlist.prev));

   // TEST changewaitflag_syncfunc
   static_assert(syncfunc_opt_WAIT_RESULT == 1 && syncfunc_opt_WAIT_MASK == 3,
               "syncfunc_opt_WAIT_RESULT is smallest possible value of syncfunc_opt_WAIT_MASK");
   for (syncfunc_opt_e opt = 0; opt <= syncfunc_opt_ALL; ++opt) {
      if (0 != (opt & syncfunc_opt_WAIT_MASK)) continue;
      for (syncfunc_opt_e opt1 = syncfunc_opt_WAIT_RESULT; opt1 <= syncfunc_opt_WAIT_MASK; ++opt1) {
         for (syncfunc_opt_e opt2 = syncfunc_opt_WAIT_RESULT; opt2 <= syncfunc_opt_WAIT_MASK; ++opt2) {
            sfunc.optflags = (uint8_t) (opt | opt1);
            changewaitflag_syncfunc(&sfunc, opt2);
            sfunc.optflags = (uint8_t) (opt | opt2);
         }
      }
   }

   // TEST changewaitflag_syncfunc: invalid value
   sfunc.optflags = (uint8_t) syncfunc_opt_WAIT_MASK;
   changewaitflag_syncfunc(&sfunc, syncfunc_opt_NONE);
   TEST(0 == sfunc.optflags);
   sfunc.optflags = 0;
   changewaitflag_syncfunc(&sfunc, syncfunc_opt_WAIT_MASK);
   TEST(syncfunc_opt_WAIT_MASK == sfunc.optflags);

   // TEST waitresult_syncfunc
   for (syncfunc_opt_e opt = 0; opt <= syncfunc_opt_ALL; ++opt) {
      for (int result = -10; result <= 10; ++result) {
         int expect = 0;
         sfunc = (syncfunc_t) syncfunc_FREE;
         sfunc.optflags = opt;
         sfunc.waitresult = result;
         if (syncfunc_opt_WAIT_RESULT == (opt & syncfunc_opt_WAIT_MASK)) {
            expect = result;
         }
         TEST(expect == waitresult_syncfunc(&sfunc));
         // no fields changed
         TEST(sfunc.optflags   == opt);
         TEST(sfunc.waitresult == result);
      }
   }

   // TEST setwaitresult_syncfunc
   for (syncfunc_opt_e opt = 0; opt <= syncfunc_opt_ALL; ++opt) {
      for (int result = -10; result <= 10; ++result) {
         sfunc = (syncfunc_t) syncfunc_FREE;
         sfunc.optflags = opt;
         setwaitresult_syncfunc(&sfunc, result);
         syncfunc_opt_e expect = opt & (syncfunc_opt_e) ~syncfunc_opt_WAIT_MASK;
         expect |= syncfunc_opt_WAIT_RESULT;
         TEST(sfunc.optflags   == expect);
         TEST(sfunc.waitresult == result);
      }
   }

   // TEST relink_syncfunc
   for (syncfunc_opt_e opt = 0; opt <= syncfunc_opt_ALL; ++opt) {
      const int  iswait = (opt & syncfunc_opt_WAIT_MASK);
      const int  isresult = (iswait == syncfunc_opt_WAIT_RESULT);

      sfunc  = (syncfunc_t) syncfunc_FREE;
      sfunc2 = (syncfunc_t) syncfunc_FREE;
      sfunc.optflags = opt;

      // test relink_syncfunc: invalid links
      relink_syncfunc(&sfunc);
      TEST(!isvalid_link(&sfunc.waitfor));
      TEST(!isvalid_linkd(&sfunc.waitlist));
      TEST(!isvalid_link(&sfunc2.waitfor));
      TEST(!isvalid_linkd(&sfunc2.waitlist));

      // test relink_syncfunc: valid links
      sfunc.waitfor.link = &sfunc2.waitfor;
      sfunc.waitlist.prev = &sfunc2.waitlist;
      sfunc.waitlist.next = &sfunc2.waitlist;
      relink_syncfunc(&sfunc);
      TEST(sfunc.waitfor.link == &sfunc2.waitfor);
      TEST(sfunc.waitlist.prev == &sfunc2.waitlist);
      TEST(sfunc.waitlist.next == &sfunc2.waitlist);
      if (iswait && !isresult) {
         TEST(sfunc2.waitfor.link == &sfunc.waitfor);
      } else {
         TEST(! isvalid_link(&sfunc2.waitfor));
      }
      if (iswait) {
         TEST(sfunc2.waitlist.prev == &sfunc.waitlist);
         TEST(sfunc2.waitlist.next == &sfunc.waitlist);
      } else {
         TEST(! isvalid_linkd(&sfunc2.waitlist));
      }
   }

   // TEST unlink_syncfunc
   for (syncfunc_opt_e opt = 0; opt <= syncfunc_opt_ALL; ++opt) {
      const int  iswait = (opt & syncfunc_opt_WAIT_MASK);
      const int  isresult = (iswait == syncfunc_opt_WAIT_RESULT);

      sfunc  = (syncfunc_t) syncfunc_FREE;
      sfunc2 = (syncfunc_t) syncfunc_FREE;
      sfunc.optflags = opt;

      // test unlink_syncfunc: invalid links (nothing is done)
      unlink_syncfunc(&sfunc);
      TEST(!isvalid_link(&sfunc.waitfor));
      TEST(!isvalid_linkd(&sfunc.waitlist));
      TEST(!isvalid_link(&sfunc2.waitfor));
      TEST(!isvalid_linkd(&sfunc2.waitlist));

      // test unlink_syncfunc: valid links
      init_link(&sfunc.waitfor, &sfunc2.waitfor);
      init_linkd(&sfunc.waitlist, &sfunc2.waitlist);
      unlink_syncfunc(&sfunc);
      // check sfunc is not changed
      TEST(sfunc.waitfor.link == &sfunc2.waitfor);
      TEST(sfunc.waitlist.prev == &sfunc2.waitlist);
      TEST(sfunc.waitlist.next == &sfunc2.waitlist);
      // check link target sfunc2 is made invalid
      if (iswait && !isresult) {
         TEST(! isvalid_link(&sfunc2.waitfor));
      } else {
         TEST(sfunc2.waitfor.link == &sfunc.waitfor);
      }
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
      TEST(sfparam.syncrun     == 0);
      TEST(sfparam.contoffset  != 0);
      TEST(sfparam.state       == 0);
      TEST(sfparam.condition   == 0);
      TEST(sfparam.err         == 10 + cmd);
   }

   // TEST start_syncfunc: invalid sfcmd value
   sfparam.contoffset = 0;
   for (int cmd = synccmd_WAIT; cmd <= synccmd_WAIT+16; ++cmd) {
      sfparam.err = 0;
      TEST(-1 == test_start_sf(&sfparam, (uint32_t)cmd));
      TEST( 0 == sfparam.syncrun);
      TEST( 0 == sfparam.contoffset);
      TEST( 0 == sfparam.state);
      TEST( 0 == sfparam.condition);
      TEST(-1 == sfparam.err);
   }

   // TEST exit_syncfunc
   for (uint32_t cmd = 0; cmd <= 100000; cmd += 10000) {
      sfparam.err = -1;
      TEST(synccmd_EXIT == test_exit_sf(&sfparam, cmd));
      TEST(sfparam.syncrun   == 0);
      TEST(sfparam.contoffset == 0);
      TEST(sfparam.state     == 0);
      TEST(sfparam.condition == 0);
      TEST(sfparam.err       == (int)cmd);
   }

   // TEST wait_syncfunc: waiterr == 0
   memset(&sfparam, 0, sizeof(sfparam));
   for (intptr_t i = 1; i <= 4; ++i) {
      uint16_t oldoff = sfparam.contoffset;
      int      result = i != 4 ? synccmd_WAIT : synccmd_EXIT;
      sfparam.condition = 0;
      TEST(result == test_wait_sf(&sfparam, i == 1 ? synccmd_RUN : synccmd_CONTINUE));
      TEST(0 == sfparam.syncrun);
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
      TEST(0 == sfparam.syncrun);
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
      TEST(0 == sfparam.syncrun);
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
