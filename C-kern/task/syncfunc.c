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

void relink_syncfunc(syncfunc_t * sfunc, const size_t structsize)
{
   const int iswaitfor = (sfunc->optfields & syncfunc_opt_WAITFOR);
   if (iswaitfor && !(sfunc->optfields & syncfunc_opt_WAITRESULT)) {
      link_t * waitfor = addrwaitfor_syncfunc(sfunc);
      if (isvalid_link(waitfor)) {
         relink_link(waitfor);
      }
   }

   if (0 != (sfunc->optfields & syncfunc_opt_WAITLIST)) {
      linkd_t * waitlist = addrwaitlist_syncfunc(sfunc, iswaitfor);
      if (isvalid_linkd(waitlist)) {
         relink_linkd(waitlist);
      }
   }

   if (0 != (sfunc->optfields & syncfunc_opt_CALLER)) {
      const int isstate = (sfunc->optfields & syncfunc_opt_STATE);
      link_t * caller = addrcaller_syncfunc(sfunc, structsize, isstate);
      if (isvalid_link(caller)) {
         relink_link(caller);
      }
   }
}

void unlink_syncfunc(syncfunc_t * sfunc, const size_t structsize)
{
   const int iswaitfor = (sfunc->optfields & syncfunc_opt_WAITFOR);
   if (iswaitfor && !(sfunc->optfields & syncfunc_opt_WAITRESULT)) {
      link_t * waitfor = addrwaitfor_syncfunc(sfunc);
      if (isvalid_link(waitfor)) {
         unlink_link(waitfor);
      }
   }

   if (0 != (sfunc->optfields & syncfunc_opt_WAITLIST)) {
      linkd_t * waitlist = addrwaitlist_syncfunc(sfunc, iswaitfor);
      if (isvalid_linkd(waitlist)) {
         unlink0_linkd(waitlist);
      }
   }

   if (0 != (sfunc->optfields & syncfunc_opt_CALLER)) {
      const int isstate = (sfunc->optfields & syncfunc_opt_STATE);
      link_t * caller = addrcaller_syncfunc(sfunc, structsize, isstate);
      if (isvalid_link(caller)) {
         unlink_link(caller);
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
   TEST(0 == sfparam.waiterr);
   TEST(0 == sfparam.retcode);

   // TEST syncfunc_param_INIT
   TEST(R == sfparam2.syncrun);
   TEST(0 == sfparam2.contoffset);
   TEST(0 == sfparam2.state);
   TEST(0 == sfparam2.condition);
   TEST(0 == sfparam2.waiterr);
   TEST(0 == sfparam2.retcode);

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
   TEST(0 == sfunc.contoffset);
   TEST(0 == sfunc.optfields);
   TEST(0 == sfunc.waitfor.link);
   TEST(0 == sfunc.waitlist.prev);
   TEST(0 == sfunc.waitlist.next);
   TEST(0 == sfunc.caller.link);
   TEST(0 == sfunc.state);

   // TEST init_syncfunc
   for (syncfunc_opt_e opt = 0; opt <= syncfunc_opt_ALL; ++opt) {
      memset(&sfunc, 255, sizeof(sfunc));
      init_syncfunc(&sfunc, &test_start_sf, opt);
      TEST(sfunc.mainfct    == &test_start_sf);
      TEST(sfunc.contoffset == 0);
      TEST(sfunc.optfields  == opt);
      // not initialised
      TEST(sfunc.waitfor.link  != 0);
      TEST(sfunc.waitlist.prev != 0);
      TEST(sfunc.waitlist.next != 0);
      TEST(sfunc.caller.link   != 0);
      TEST(sfunc.state         != 0);
   }

   // TEST initmove_syncfunc
   for (syncfunc_opt_e opt = 0; opt <= syncfunc_opt_ALL; ++opt) {
      link_t waitfor = link_FREE;
      for (syncfunc_opt_e opt2 = 0; opt2 <= syncfunc_opt_ALL; ++opt2) {
         if ((opt & syncfunc_opt_CALLER) != (opt2 & syncfunc_opt_CALLER)) continue;
         uint16_t size  = getsize_syncfunc(opt);
         uint16_t size2 = getsize_syncfunc(opt2);
         void   * state = (void*) (uintptr_t) (opt2 & syncfunc_opt_STATE ? 12 + 256 * opt + opt2 : 0);
         uint16_t contoff = (uint16_t) (11 + opt + opt2);
         memset(&sfunc, 255, sizeof(sfunc));
         memset(&sfunc2, 0, sizeof(sfunc));
         init_syncfunc(&sfunc, &test_start_sf, opt);
         if (opt & syncfunc_opt_CALLER) {
            init_link(addrcaller_syncfunc(&sfunc, size, (opt & syncfunc_opt_STATE)), &waitfor);
         }
         initmove_syncfunc(&sfunc2, size2, contoff, opt2, state, &sfunc, size, (opt & syncfunc_opt_STATE));
         TEST(sfunc2.mainfct    == &test_start_sf);
         TEST(sfunc2.contoffset == contoff);
         TEST(sfunc2.optfields  == opt2);
         if (opt2 & syncfunc_opt_STATE) {
            TEST(state == *addrstate_syncfunc(&sfunc2, size2));
         }
         if (opt2 & syncfunc_opt_CALLER) {
            TEST(&waitfor == addrcaller_syncfunc(&sfunc2, size2, (opt2 & syncfunc_opt_STATE))->link);
         }
         // not initialised
         if (opt2 & syncfunc_opt_WAITFOR) {
            TEST(0 == sfunc2.waitfor.link);
         }
         if (opt2 & syncfunc_opt_WAITLIST) {
            TEST(0 == addrwaitlist_syncfunc(&sfunc2, (opt2 & syncfunc_opt_WAITFOR))->prev);
            TEST(0 == addrwaitlist_syncfunc(&sfunc2, (opt2 & syncfunc_opt_WAITFOR))->next);
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
   size_t     size;

   // TEST getsize_syncfunc: return uint16_t type
   uint16_t nonesize = getsize_syncfunc(syncfunc_opt_NONE);

   // TEST getsize_syncfunc: syncfunc_opt_NONE
   TEST(offsetof(syncfunc_t, waitfor) == getsize_syncfunc(syncfunc_opt_NONE));
   TEST(offsetof(syncfunc_t, waitfor) == nonesize);

   // TEST getsize_syncfunc: syncfunc_opt_ALL
   TEST(sizeof(syncfunc_t) == getsize_syncfunc(syncfunc_opt_ALL));

   // TEST getsize_syncfunc: combination of flags
   for (syncfunc_opt_e opt = 0; opt <= syncfunc_opt_ALL; ++opt) {
      size = offsetof(syncfunc_t, waitfor);
      if (opt & syncfunc_opt_WAITFOR)  size += sizeof(sfunc.waitfor);
      if (opt & syncfunc_opt_WAITLIST) size += sizeof(sfunc.waitlist);
      if (opt & syncfunc_opt_CALLER)   size += sizeof(sfunc.caller);
      if (opt & syncfunc_opt_STATE)    size += sizeof(sfunc.state);
      TEST(size == getsize_syncfunc(opt));
   }

   // TEST offwaitfor_syncfunc
   TEST(offsetof(syncfunc_t, waitfor) == offwaitfor_syncfunc());

   // TEST offwaitlist_syncfunc
   TEST(offwaitlist_syncfunc(false) == offwaitfor_syncfunc());
   TEST(offwaitlist_syncfunc(true)  == offwaitfor_syncfunc() + sizeof(sfunc.waitfor));

   // TEST offcaller_syncfunc
   for (size = getsize_syncfunc(syncfunc_opt_ALL); size >= offwaitfor_syncfunc(); --size) {
      for (int isstate = 0; isstate <= 1; ++isstate) {
         for (int iscaller = 0; iscaller <= 1; ++iscaller) {
            const size_t expect = size
                                - (isstate ? sizeof(sfunc.state) : 0)
                                - (iscaller ? sizeof(sfunc.caller) : 0);
            TEST(expect == offcaller_syncfunc(size, isstate, iscaller));
         }
      }
   }

   // TEST offstate_syncfunc
   for (size = getsize_syncfunc(syncfunc_opt_ALL); size >= offwaitfor_syncfunc(); --size) {
      for (int isstate = 0; isstate <= 1; ++isstate) {
         const size_t expect = size
                             - (isstate ? sizeof(sfunc.state) : 0);
         TEST(expect == offstate_syncfunc(size, isstate));
      }
   }

   // TEST addrwaitresult_syncfunc
   TEST(&sfunc.waitresult == addrwaitresult_syncfunc(&sfunc));

   // TEST addrwaitfor_syncfunc
   TEST(&sfunc.waitfor == addrwaitfor_syncfunc(&sfunc));

   // TEST addrwaitlist_syncfunc
   TEST(addrwaitlist_syncfunc(&sfunc, true)  == &sfunc.waitlist);
   TEST(addrwaitlist_syncfunc(&sfunc, false) == (linkd_t*)&sfunc.waitfor);

   // TEST addrcaller_syncfunc
   size = getsize_syncfunc(syncfunc_opt_ALL);
   TEST(addrcaller_syncfunc(&sfunc, size, true) == &sfunc.caller);
   for (size = getsize_syncfunc(syncfunc_opt_ALL); size >= offwaitfor_syncfunc(); --size) {
      for (int isstate = 0; isstate <= 1; ++isstate) {
         void * expect = (uint8_t*) &sfunc
                       + size
                       - (isstate ? sizeof(sfunc.state) : 0)
                       - sizeof(sfunc.caller);
         TEST(addrcaller_syncfunc(&sfunc, size, isstate) == (link_t*)expect);
      }
   }

   // TEST addrstate_syncfunc
   size = getsize_syncfunc(syncfunc_opt_ALL);
   TEST(addrstate_syncfunc(&sfunc, size) == &sfunc.state);
   for (size = getsize_syncfunc(syncfunc_opt_ALL); size >= offwaitfor_syncfunc(); --size) {
      void * expect = (uint8_t*) &sfunc
                    + size
                    - sizeof(sfunc.state);
      TEST(addrstate_syncfunc(&sfunc, size) == (void**)expect);
   }

   // TEST castPwaitfor_syncfunc: 0 value return invalid address
   TEST(0 != castPwaitfor_syncfunc(0));

   // TEST castPwaitlist_syncfunc: 0 value return invalid address
   TEST(0 != castPwaitlist_syncfunc(0, true));
   TEST(0 != castPwaitlist_syncfunc(0, false));

   // TEST castPwaitfor_syncfunc: value returned from addrwaitfor_syncfunc(&sfunc)
   TEST(&sfunc == castPwaitfor_syncfunc(addrwaitfor_syncfunc(&sfunc)));

   // TEST castPwaitlist_syncfunc: value returned from addrwaitlist_syncfunc(&sfunc, ?)
   TEST(&sfunc == castPwaitlist_syncfunc(addrwaitlist_syncfunc(&sfunc, true), true));
   TEST(&sfunc == castPwaitlist_syncfunc(addrwaitlist_syncfunc(&sfunc, false), false));

   // TEST castPwaitfor_syncfunc: connected link
   init_link(&sfunc.waitfor, &sfunc2.caller);
   TEST(&sfunc == castPwaitfor_syncfunc(sfunc2.caller.link));

   // TEST castPwaitlist_syncfunc: connected link
   init_linkd(addrwaitlist_syncfunc(&sfunc, true), &sfunc2.waitlist);
   TEST(&sfunc == castPwaitlist_syncfunc(sfunc2.waitlist.next, true));
   TEST(&sfunc == castPwaitlist_syncfunc(sfunc2.waitlist.prev, true));
   init_linkd(addrwaitlist_syncfunc(&sfunc, false), &sfunc2.waitlist);
   TEST(&sfunc == castPwaitlist_syncfunc(sfunc2.waitlist.next, false));
   TEST(&sfunc == castPwaitlist_syncfunc(sfunc2.waitlist.prev, false));

   // TEST clearopt_syncfunc
   for (int i = 0; i <= syncfunc_opt_ALL; ++i) {
      for (int i2 = 0; i2 <= syncfunc_opt_ALL; ++i2) {
         sfunc.optfields = (uint8_t) i;
         clearopt_syncfunc(&sfunc, i2);
         TEST(sfunc.optfields == (uint8_t) (i&~i2));
      }
   }

   // TEST setopt_syncfunc
   for (int i = 0; i <= syncfunc_opt_ALL; ++i) {
      for (int i2 = 0; i2 <= syncfunc_opt_ALL; ++i2) {
         sfunc.optfields = (uint8_t) i;
         setopt_syncfunc(&sfunc, i2);
         TEST(sfunc.optfields == (uint8_t) (i|i2));
      }
   }

   // TEST setresult_syncfunc
   for (syncfunc_opt_e opt = 0; opt <= syncfunc_opt_ALL; ++opt) {
      for (int result = -10; result <= 10; ++result) {
         sfunc = (syncfunc_t) syncfunc_FREE;
         sfunc.optfields = opt;
         setresult_syncfunc(&sfunc, result);
         TEST(sfunc.optfields  == (opt | syncfunc_opt_WAITRESULT));
         TEST(sfunc.waitresult == result);
      }
   }

   // TEST relink_syncfunc
   for (syncfunc_opt_e opt = 0; opt <= syncfunc_opt_ALL; ++opt) {
      const int  iswaitfor = (opt & syncfunc_opt_WAITFOR);
      const int  isresult  = (opt & syncfunc_opt_WAITRESULT);
      const int  iswaitlist = (opt & syncfunc_opt_WAITLIST);
      const int  iscaller = (opt & syncfunc_opt_CALLER);
      const int  isstate  = (opt & syncfunc_opt_STATE);

      size = getsize_syncfunc(opt);
      sfunc  = (syncfunc_t) syncfunc_FREE;
      sfunc2 = (syncfunc_t) syncfunc_FREE;
      sfunc.optfields = opt;

      // test relink_syncfunc: invalid links
      relink_syncfunc(&sfunc, size);
      if (iswaitfor) {
         addrwaitfor_syncfunc(&sfunc)->link = &sfunc2.waitfor;
      }
      if (iswaitlist) {
         addrwaitlist_syncfunc(&sfunc, iswaitfor)->prev = &sfunc2.waitlist;
         addrwaitlist_syncfunc(&sfunc, iswaitfor)->next = &sfunc2.waitlist;
      }
      if (iscaller) {
         addrcaller_syncfunc(&sfunc, size, isstate)->link = &sfunc2.caller;
      }

      // test relink_syncfunc: valid links
      relink_syncfunc(&sfunc, size);
      if (iswaitfor) {
         TEST(addrwaitfor_syncfunc(&sfunc)->link == &sfunc2.waitfor);
         if (!isresult) {
            TEST(addrwaitfor_syncfunc(&sfunc) == sfunc2.waitfor.link);
         } else {
            TEST(0 == isvalid_link(&sfunc2.waitfor));
         }
      } else {
         TEST(0 == isvalid_link(&sfunc2.waitfor));
      }
      if (iswaitlist) {
         TEST(addrwaitlist_syncfunc(&sfunc, iswaitfor)->prev == &sfunc2.waitlist);
         TEST(addrwaitlist_syncfunc(&sfunc, iswaitfor)->next == &sfunc2.waitlist);
         TEST(addrwaitlist_syncfunc(&sfunc, iswaitfor) == sfunc2.waitlist.prev);
         TEST(addrwaitlist_syncfunc(&sfunc, iswaitfor) == sfunc2.waitlist.next);
      } else {
         TEST(0 == isvalid_linkd(&sfunc2.waitlist));
      }
      if (iscaller) {
         TEST(addrcaller_syncfunc(&sfunc, size, isstate)->link == &sfunc2.caller);
         TEST(addrcaller_syncfunc(&sfunc, size, isstate) == sfunc2.caller.link);
      } else {
         TEST(0 == isvalid_link(&sfunc2.caller));
      }
   }

   // TEST unlink_syncfunc
   for (syncfunc_opt_e opt = 0; opt <= syncfunc_opt_ALL; ++opt) {
      const int  iswaitfor = (opt & syncfunc_opt_WAITFOR);
      const int  isresult  = (opt & syncfunc_opt_WAITRESULT);
      const int  iswaitlist = (opt & syncfunc_opt_WAITLIST);
      const int  iscaller = (opt & syncfunc_opt_CALLER);
      const int  isstate  = (opt & syncfunc_opt_STATE);

      size = getsize_syncfunc(opt);
      sfunc  = (syncfunc_t) syncfunc_FREE;
      sfunc2 = (syncfunc_t) syncfunc_FREE;
      sfunc.optfields = opt;

      // test unlink_syncfunc: invalid links (nothing is done)
      unlink_syncfunc(&sfunc, size);
      if (iswaitfor) {
         init_link(addrwaitfor_syncfunc(&sfunc), &sfunc2.waitfor);
      }
      if (iswaitlist) {
         init_linkd(addrwaitlist_syncfunc(&sfunc, iswaitfor), &sfunc2.waitlist);
      }
      if (iscaller) {
         init_link(addrcaller_syncfunc(&sfunc, size, isstate), &sfunc2.caller);
      }

      // test unlink_syncfunc: valid links
      unlink_syncfunc(&sfunc, size);
      // link target sfunc2 is made invalid
      if (iswaitfor && isresult) {
         TEST(addrwaitfor_syncfunc(&sfunc) == sfunc2.waitfor.link);
      } else {
         TEST(0 == isvalid_link(&sfunc2.waitfor));
      }
      TEST(0 == isvalid_linkd(&sfunc2.waitlist));
      TEST(0 == isvalid_link(&sfunc2.caller));
      // sfunc is not changed
      if (iswaitfor) {
         TEST(addrwaitfor_syncfunc(&sfunc)->link == &sfunc2.waitfor);
      }
      if (iswaitlist) {
         TEST(addrwaitlist_syncfunc(&sfunc, iswaitfor)->prev == &sfunc2.waitlist);
         TEST(addrwaitlist_syncfunc(&sfunc, iswaitfor)->next == &sfunc2.waitlist);
      }
      if (iscaller) {
         TEST(addrcaller_syncfunc(&sfunc, size, isstate)->link == &sfunc2.caller);
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_start_sf(syncfunc_param_t * sfparam, uint32_t sfcmd)
{
   start_syncfunc(sfparam, sfcmd, ONRUN, ONEXIT);

   // is executed in case of wrong sfcmd value
   sfparam->retcode = -1;
   return -1;

ONRUN:
   sfparam->contoffset = (uint16_t) __extension__ ((uintptr_t) &&ONCONTINUE - (uintptr_t) &&syncfunc_START);
   sfparam->retcode = 10;
   return synccmd_RUN;

ONCONTINUE:
   static_assert(synccmd_CONTINUE > synccmd_RUN, "must run after continue");
   sfparam->retcode = 11;
   return synccmd_CONTINUE;

ONEXIT:
   sfparam->retcode = 12;
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
   intptr_t err = sfparam->waiterr;
   err = wait_syncfunc(sfparam, (void*)err);
   err = wait_syncfunc(sfparam, (void*)err);
   err = wait_syncfunc(sfparam, (void*)err);
   return synccmd_EXIT;

ONERR:
   return -1;
}

static int test_waitexit_sf(syncfunc_param_t * sfparam, uint32_t sfcmd)
{
   start_syncfunc(sfparam, sfcmd, ONRUN, ONERR);

   goto ONERR;

ONRUN: ;
   int retcode = sfparam->retcode;
   int err;
   sfparam->state = (void*)(intptr_t)retcode;
   err = waitexit_syncfunc(sfparam, &retcode);
   if (err) goto ONERR;
   sfparam->state = (void*)(intptr_t)retcode;
   err = waitexit_syncfunc(sfparam, &retcode);
   if (err) goto ONERR;
   sfparam->state = (void*)(intptr_t)retcode;
   err = waitexit_syncfunc(sfparam, &retcode);
   if (err) goto ONERR;
   sfparam->state = (void*)(intptr_t)retcode;
   return synccmd_EXIT;

ONERR:
   return -1;
}

static int test_waitexiterr_sf(syncfunc_param_t * sfparam, uint32_t sfcmd)
{
   int retcode = -1;
   int err;
   start_syncfunc(sfparam, sfcmd, ONRUN, ONERR);

   goto ONERR;

ONRUN: ;
   err = waitexit_syncfunc(sfparam, &retcode);
   sfparam->waiterr = err;
   sfparam->retcode = retcode;
   err = waitexit_syncfunc(sfparam, &retcode);
   sfparam->waiterr = err;
   sfparam->retcode = retcode;
   err = waitexit_syncfunc(sfparam, &retcode);
   sfparam->waiterr = err;
   sfparam->retcode = retcode;
   return synccmd_EXIT;

ONERR:
   return -1;
}

static int test_yield_sf(syncfunc_param_t * sfparam, uint32_t sfcmd)
{
   start_syncfunc(sfparam, sfcmd, ONRUN, ONERR);

   goto ONERR;

ONRUN:
   sfparam->retcode += 19;
   yield_syncfunc(sfparam);
   sfparam->retcode += 20;
   yield_syncfunc(sfparam);
   sfparam->retcode += 21;
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
      sfparam.retcode = 0;
      TEST(cmd == test_start_sf(&sfparam, (uint32_t)cmd));
      TEST(sfparam.syncrun     == 0);
      TEST(sfparam.contoffset  != 0);
      TEST(sfparam.state       == 0);
      TEST(sfparam.condition   == 0);
      TEST(sfparam.waiterr     == 0);
      TEST(sfparam.retcode -10 == cmd);
   }

   // TEST start_syncfunc: invalid sfcmd value
   sfparam.contoffset = 0;
   for (int cmd = synccmd_WAIT; cmd <= synccmd_WAIT+16; ++cmd) {
      sfparam.retcode = 0;
      TEST(-1 == test_start_sf(&sfparam, (uint32_t)cmd));
      TEST( 0 == sfparam.syncrun);
      TEST( 0 == sfparam.contoffset);
      TEST( 0 == sfparam.state);
      TEST( 0 == sfparam.condition);
      TEST( 0 == sfparam.waiterr);
      TEST(-1 == sfparam.retcode);
   }

   // TEST exit_syncfunc
   for (uint32_t cmd = 0; cmd <= 100000; cmd += 10000) {
      sfparam.retcode = -1;
      TEST(synccmd_EXIT == test_exit_sf(&sfparam, cmd));
      TEST(sfparam.syncrun   == 0);
      TEST(sfparam.contoffset == 0);
      TEST(sfparam.state     == 0);
      TEST(sfparam.condition == 0);
      TEST(sfparam.waiterr   == 0);
      TEST(sfparam.retcode   == (int)cmd);
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
      TEST(0 == sfparam.waiterr);
      TEST(0 == sfparam.retcode);
   }

   // TEST wait_syncfunc: waiterr != 0
   memset(&sfparam, 0, sizeof(sfparam));
   for (intptr_t i = 1; i <= 4; ++i) {
      uint16_t oldoff = sfparam.contoffset;
      int      result = i != 4 ? synccmd_WAIT : synccmd_EXIT;
      sfparam.condition = 0;
      sfparam.waiterr   = i;
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
      TEST(i == sfparam.waiterr);
      TEST(0 == sfparam.retcode);
   }

   // TEST waitexit_syncfunc: waiterr == 0
   memset(&sfparam, 0, sizeof(sfparam));
   for (intptr_t i = 1; i <= 4; ++i) {
      uint16_t oldoff = sfparam.contoffset;
      int      result = i != 4 ? synccmd_WAIT : synccmd_EXIT;
      sfparam.state     = 0 ;
      sfparam.condition = (void*)1;
      sfparam.retcode   = (int) i;
      TEST(result == test_waitexit_sf(&sfparam, i == 1 ? synccmd_RUN : synccmd_CONTINUE));
      TEST(0        == sfparam.syncrun);
      TEST((void*)i == sfparam.state);
      if (i != 4) {
         TEST(oldoff != sfparam.contoffset);
         TEST(0      == sfparam.condition); // clears condition
      } else {
         TEST(oldoff   == sfparam.contoffset);
         TEST((void*)1 == sfparam.condition);
      }
      TEST(0 == sfparam.waiterr);
      TEST(i == sfparam.retcode);
   }

   // TEST waitexit_syncfunc: waiterr != 0
   memset(&sfparam, 0, sizeof(sfparam));
   for (intptr_t i = 1; i <= 4; ++i) {
      uint16_t oldoff = sfparam.contoffset;
      int      result = i != 4 ? synccmd_WAIT : synccmd_EXIT;
      sfparam.state     = 0;
      sfparam.condition = (void*)1;
      sfparam.waiterr   = i;
      sfparam.retcode   = (int) -i;
      TEST(result == test_waitexiterr_sf(&sfparam, i == 1 ? synccmd_RUN : synccmd_CONTINUE));
      TEST( i == sfparam.waiterr);
      TEST(-i == sfparam.retcode);
      if (i != 4) {
         TEST(oldoff != sfparam.contoffset);
         TEST(0      == sfparam.condition); // clears condition
      } else {
         TEST(oldoff   == sfparam.contoffset);
         TEST((void*)1 == sfparam.condition);
      }
   }

   // TEST yield_syncfunc
   memset(&sfparam, 0, sizeof(sfparam));
   for (int i = 19; i <= 21; ++i) {
      uint16_t oldoff = sfparam.contoffset;
      int      result = i != 21 ? synccmd_CONTINUE : synccmd_EXIT;
      sfparam.retcode = 0;
      TEST(result == test_yield_sf(&sfparam, i == 19 ? synccmd_RUN : synccmd_CONTINUE));
      TEST(0 == sfparam.syncrun);
      TEST(0 == sfparam.state);
      TEST(oldoff != sfparam.contoffset);
      TEST(0 == sfparam.condition);
      TEST(0 == sfparam.waiterr);
      TEST(i == sfparam.retcode);
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
