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
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/task/syncwait.h"
#endif

// section: syncfunc_t

// group: update

void unlink_syncfunc(syncfunc_t* sfunc)
{
   if (iswaiting_syncfunc(sfunc)) {
      unlink_linkd(&sfunc->waitnode);
      initinvalid_linkd(&sfunc->waitnode);
   }
}


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_syncfunc_it(void)
{
   syncfunc_it isrun = syncfunc_it_INIT(0,0);

   // TEST syncfunc_it_INIT
   for (uintptr_t i=0; i<2; ++i) {
      isrun = (syncfunc_it) syncfunc_it_INIT( (void(*)(syncfunc_param_t*))(i+1), (void(*)(syncfunc_param_t*,syncwait_t*))(i+2));
      TEST( isrun.exitsf == (void(*)(syncfunc_param_t*))(i+1));
      TEST( isrun.waitsf == (void(*)(syncfunc_param_t*,syncwait_t*))(i+2));
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_sfparam(void)
{

   // TEST syncfunc_param_FREE
   {
      syncfunc_param_t sfparam  = syncfunc_param_FREE;
      TEST(0 == sfparam.srun);
      TEST(0 == sfparam.sfunc);
      TEST(0 == sfparam.iimpl);
   }

   // TEST syncfunc_param_INIT
   for (uint8_t i =0; i <= 1; ++i) {
      struct syncrunner_t *R  = (struct syncrunner_t*)((intptr_t)1 + i);
      struct syncfunc_it  *I  = (struct syncfunc_it*) ((intptr_t)2 + i);
      syncfunc_param_t sfparam = (syncfunc_param_t) syncfunc_param_INIT(R,I);
      TEST(R == sfparam.srun);
      TEST(0 == sfparam.sfunc);
      TEST(I == sfparam.iimpl);
   }

   return 0;
ONERR:
   return EINVAL;

}

static void test_dummy(syncfunc_param_t* sfparam)
{
   (void) sfparam;
}

static int test_initfree(void)
{
   syncfunc_t sfunc  = syncfunc_FREE;
   syncfunc_t sfunc2 = syncfunc_FREE;

   // TEST syncfunc_FREE
   TEST( 0 == sfunc.mainfct);
   TEST( 0 == sfunc.state);
   TEST( 0 == sfunc.contoffset);
   TEST( 0 == sfunc.endoffset);
   TEST( 0 == sfunc.err);
   TEST( 0 == sfunc.waitnode.prev);
   TEST( 0 == sfunc.waitnode.next);

   // TEST init_syncfunc
   for (uintptr_t state = 0, qid=0; state != (uintptr_t)-1; state <<= 1, ++state, ++qid) {
      memset(&sfunc, 255, sizeof(sfunc));
      init_syncfunc(&sfunc, &test_dummy, (void*) state);
      TEST( sfunc.mainfct    == &test_dummy);
      TEST( sfunc.state      == (void*) state);
      TEST( sfunc.contoffset == 0);
      TEST( sfunc.endoffset  == 0);
      TEST( sfunc.err        == 0);
      TEST( sfunc.waitnode.prev == 0);
      TEST( sfunc.waitnode.next != 0); // not initialised
   }

   // TEST initcopy_syncfunc
   for (unsigned r = 0; r <= 3; ++r) {
      void   * state   = (void*) (uintptr_t) (256 * r);
      int16_t  contoff = (int16_t) (11 + r);
      int16_t  endoff  = (int16_t) (12 + r);
      int      sferr   = (int) (13 + r);
      init_syncfunc(&sfunc, &test_dummy, state);
      sfunc.contoffset = contoff;
      sfunc.endoffset = endoff;
      sfunc.err = sferr;
      sfunc2 = (syncfunc_t) syncfunc_FREE;
      initself_linkd(&sfunc2.waitnode);
      // test
      initcopy_syncfunc(&sfunc2, &sfunc);
      // check content
      TEST( sfunc2.mainfct    == &test_dummy);
      TEST( sfunc2.state      == state);
      TEST( sfunc2.contoffset == contoff);
      TEST( sfunc2.endoffset  == endoff);
      TEST( sfunc2.err        == sferr);
      TEST( !isvalid_linkd(&sfunc2.waitnode)); // cleared
   }

   for (unsigned r = 0; r <= 3; ++r) {
      void*    state   = (void*) (uintptr_t) (256 * r);
      int16_t  contoff = (int16_t) (11 + r);
      int16_t  endoff  = (int16_t) (12 + r);
      int      sferr   = (int) (13 + r);

      // TEST initmove_syncfunc: invalid waitnode
      initself_linkd(&sfunc.waitnode);
      init_syncfunc(&sfunc, &test_dummy, state);
      sfunc.contoffset = contoff;
      sfunc.endoffset = endoff;
      sfunc.err = sferr;

      memset(&sfunc2, 0, sizeof(sfunc2));
      initmove_syncfunc(&sfunc2, &sfunc);
      // check content
      TEST( sfunc2.mainfct    == &test_dummy);
      TEST( sfunc2.state      == state);
      TEST( sfunc2.contoffset == contoff);
      TEST( sfunc2.endoffset  == endoff);
      TEST( sfunc2.err        == sferr);
      TEST( sfunc2.waitnode.prev == 0);
      TEST( sfunc2.waitnode.next == &sfunc.waitnode);
      // check sfunc not changed
      TEST( sfunc.mainfct    == &test_dummy);
      TEST( sfunc.state      == state);
      TEST( sfunc.contoffset == contoff);
      TEST( sfunc.err        == sferr);
      TEST( sfunc.waitnode.prev == 0);
      TEST( sfunc.waitnode.next == &sfunc.waitnode);

      // TEST initmove_syncfunc: waitnode linked with other node
      linkd_t waitnode;
      init_linkd(&sfunc.waitnode, &waitnode);
      memset(&sfunc2, 0, sizeof(sfunc2));
      initmove_syncfunc(&sfunc2, &sfunc);
      // check content
      TEST( sfunc2.mainfct    == &test_dummy);
      TEST( sfunc2.state      == state);
      TEST( sfunc2.contoffset == contoff);
      TEST( sfunc2.err        == sferr);
      TEST( sfunc2.waitnode.prev == &waitnode);
      TEST( sfunc2.waitnode.next == &waitnode);
      // check waitnode relinked
      TEST( waitnode.prev == &sfunc2.waitnode);
      TEST( waitnode.next == &sfunc2.waitnode);
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_query(void)
{
   syncfunc_t sfunc  = syncfunc_FREE;
   syncfunc_t sfunc2 = syncfunc_FREE;

   // TEST waitnode_syncfunc
   TEST(waitnode_syncfunc(&sfunc)  == &sfunc.waitnode);

   // TEST err_syncfunc
   sfunc = (syncfunc_t) syncfunc_FREE;
   for (uint8_t result = 0; result <= 10; ++result) {
      sfunc.err = result;
      sfunc2 = sfunc;
      // test
      TEST(result == err_syncfunc(&sfunc));
      // no fields changed
      TEST( 0 == memcmp(&sfunc, &sfunc2, sizeof(sfunc)));
   }

   // TEST castPwaitnode_syncfunc: Invalid value 0 not checked, instead invalid address returned
   TEST(0 != castPwaitnode_syncfunc(0));

   // TEST castPwaitnode_syncfunc: valid value
   TEST(&sfunc == castPwaitnode_syncfunc(&sfunc.waitnode));

   // TEST castPwaitnode_syncfunc: connected link
   init_linkd(&sfunc.waitnode, &sfunc2.waitnode);
   TEST(&sfunc == castPwaitnode_syncfunc(sfunc2.waitnode.next));
   TEST(&sfunc == castPwaitnode_syncfunc(sfunc2.waitnode.prev));

   // TEST iswaiting_syncfunc
   initinvalid_linkd(&sfunc.waitnode);
   TEST( 0 == iswaiting_syncfunc(&sfunc));
   initself_linkd(&sfunc.waitnode);
   TEST( 1 == iswaiting_syncfunc(&sfunc));
   init_linkd(&sfunc.waitnode, &sfunc2.waitnode);
   TEST( 1 == iswaiting_syncfunc(&sfunc));

   // TEST contoffset_syncfunc
   for (unsigned i = 1; i<=32767; i <<= 1) {
      sfunc.contoffset = (int16_t)i;
      TEST( (int)i == contoffset_syncfunc(&sfunc));
      sfunc.contoffset = (int16_t)-(int)i;
      TEST(-(int)i == contoffset_syncfunc(&sfunc));
   }
   sfunc.contoffset = 0;
   TEST(0 == contoffset_syncfunc(&sfunc));

   return 0;
ONERR:
   return EINVAL;
}

static int test_update(void)
{
   syncfunc_t sfunc = syncfunc_FREE;
   syncfunc_t sfunc2 = syncfunc_FREE;
   syncwait_t swait;

   // prepare
   init_syncwait(&swait);

   // TEST linkwaitnode_syncfunc: single node
   seterr_syncfunc(&sfunc, 1);
   linkwaitnode_syncfunc(&sfunc, &swait);
   // check sfunc
   TEST( 1 == err_syncfunc(&sfunc)); // err not cleared
   TEST( &swait.funclist == sfunc.waitnode.prev);  // linked to swait
   TEST( &swait.funclist == sfunc.waitnode.next);  // linked to swait
   // check swait
   TEST( 0 != iswaiting_syncwait(&swait));
   TEST( &sfunc.waitnode == getfirst_syncwait(&swait));  // linked to swait
   TEST( &sfunc.waitnode == swait.funclist.prev);        // last waiting

   // TEST linkwaitnode_syncfunc: 2nd node
   seterr_syncfunc(&sfunc2, 1);
   linkwaitnode_syncfunc(&sfunc2, &swait);
   // check sfunc2
   TEST( 1 == err_syncfunc(&sfunc2)); // err not cleared
   TEST( &sfunc.waitnode == sfunc2.waitnode.prev); // linked to swait
   TEST( &swait.funclist == sfunc2.waitnode.next); // linked to swait
   // check swait
   TEST( 0 != iswaiting_syncwait(&swait));
   TEST( &sfunc.waitnode == getfirst_syncwait(&swait));  // unchanged
   TEST( &sfunc2.waitnode == swait.funclist.prev);       // last waiting changed

   // TEST seterr_syncfunc
   sfunc  = (syncfunc_t) syncfunc_FREE;
   sfunc2 = (syncfunc_t) syncfunc_FREE;
   for (uint8_t result = 0; result <= 10; ++result) {
      seterr_syncfunc(&sfunc, result);
      TEST( result == err_syncfunc(&sfunc));
      // no other fields changed
      seterr_syncfunc(&sfunc2, result);
      TEST( 0 == memcmp(&sfunc, &sfunc2, sizeof(sfunc)));
   }

   // TEST unlink_syncfunc: invalid links (nothing is done)
   sfunc  = (syncfunc_t) syncfunc_FREE;
   sfunc2 = (syncfunc_t) syncfunc_FREE;
   unlink_syncfunc(&sfunc);
   TEST( 0 == sfunc.waitnode.prev);
   TEST( 0 == sfunc.waitnode.next);

   // TEST unlink_syncfunc: valid links
   init_linkd(&sfunc.waitnode, &sfunc2.waitnode);
   seterr_syncfunc(&sfunc, 1);
   seterr_syncfunc(&sfunc2, 2);
   unlink_syncfunc(&sfunc);
   TEST( 1 == err_syncfunc(&sfunc));  // not changed
   TEST( 2 == err_syncfunc(&sfunc2)); // not changed
   TEST( 0 == iswaiting_syncfunc(&sfunc));     // made link invalid
   TEST( 1 == isself_linkd(&sfunc2.waitnode)); // removed sfunc from list

   // TEST setcontoffset_syncfunc
   for (unsigned i = 1; i<=32767; i <<= 1) {
      setcontoffset_syncfunc(&sfunc, (int16_t)i);
      TEST((int)i == contoffset_syncfunc(&sfunc));
      setcontoffset_syncfunc(&sfunc, (int16_t)-(int)i);
      TEST(-(int)i == contoffset_syncfunc(&sfunc));
   }
   setcontoffset_syncfunc(&sfunc, 0);
   TEST(0 == contoffset_syncfunc(&sfunc));

   return 0;
ONERR:
   return EINVAL;
}

static void test_return_sf(syncfunc_param_t * sfparam)
{
   syncfunc_START: ; // needed by return_syncfunc && getoffset
   __extension__ ({
      if (sfparam->sfunc->contoffset) goto  * (void*)((intptr_t)&&syncfunc_START + sfparam->sfunc->contoffset);
   });

   sfparam->sfunc->err = 10;
   return_syncfunc(sfparam);
   sfparam->sfunc->err = 20;
   return_syncfunc(sfparam);
   sfparam->sfunc->err = 30;
   return_syncfunc(sfparam);

   sfparam->sfunc->contoffset = 0;
   sfparam->sfunc->err = 40;
}

static int test_helper(void)
{
   syncfunc_param_t sfparam = syncfunc_param_FREE;
   syncfunc_t       sfunc   = syncfunc_FREE;

   // prepare
   sfparam.sfunc = &sfunc;

   // TEST getoffset_syncfunc
   {
      syncfunc_START: ;
      LABEL1: ;
      int16_t o1 = getoffset_syncfunc(LABEL1);
      int16_t o2 = getoffset_syncfunc(LABEL2);
      TEST( 0 == getoffset_syncfunc(LABEL1));
      TEST( o1 == getoffset_syncfunc(LABEL1));
      LABEL2: ;
      TEST( 0 < getoffset_syncfunc(LABEL2));
      TEST( o2 == getoffset_syncfunc(LABEL2));
      TEST( o1 < o2);
   }

   // TEST return_syncfunc: returns + sets contoffset to end of macro
   sfunc.contoffset = 0;
   sfunc.err = 0;
   for (unsigned i = 1; i <= 4; ++i) {
      int16_t oldoffset = sfunc.contoffset;
      test_return_sf(&sfparam);
      TESTP( oldoffset != sfunc.contoffset, "oldoffset:%d contoffset:%d", oldoffset, sfunc.contoffset);
      TEST(  10*(int)i == sfunc.err);
   }

   return 0;
ONERR:
   return EINVAL;
}

typedef struct syncfunc_helper_t {
   syncfunc_it    sfit;
   unsigned       exitcount;
   unsigned       waitcount;
   syncwait_t    *waitlist;
   syncfunc_t     sfunc;
} syncfunc_helper_t;

static void exitsf_helper(syncfunc_param_t *sfparam)
{
   syncfunc_it       *sfit   = sfparam->iimpl;
   syncfunc_helper_t *helper = (syncfunc_helper_t*) sfit;
   ++ helper->exitcount;
   helper->sfunc  = *sfparam->sfunc;
}

static void waitsf_helper(syncfunc_param_t *sfparam, syncwait_t *waitlist)
{
   syncfunc_it       *sfit   = sfparam->iimpl;
   syncfunc_helper_t *helper = (syncfunc_helper_t*) sfit;
   ++ helper->waitcount;
   helper->waitlist = waitlist;
   helper->sfunc  = *sfparam->sfunc;
}

static void reset_helper(syncfunc_helper_t *helper)
{
   helper->sfit = (syncfunc_it) syncfunc_it_INIT(&exitsf_helper, &waitsf_helper);
   helper->exitcount = 0;
   helper->waitcount = 0;
   helper->waitlist  = 0;
   helper->sfunc  = (syncfunc_t) syncfunc_FREE;
}

static int testexec_helper(const syncfunc_helper_t *helper, unsigned nrexit, syncfunc_t sfunc)
{
   TEST(nrexit == helper->exitcount);
   TEST(0      == helper->waitcount);
   TEST(0      == helper->waitlist);
   if (nrexit) {
      TEST(0 == memcmp(&helper->sfunc, &sfunc, sizeof(sfunc)));
   }
   return 0;
ONERR:
   return EINVAL;
}

static int testwait_helper(const syncfunc_helper_t *helper, unsigned nrwait, syncfunc_t sfunc, syncwait_t *waitlist)
{
   TEST(0      == helper->exitcount);
   TEST(nrwait == helper->waitcount);
   TEST(waitlist == helper->waitlist);
   TEST(0      == memcmp(&helper->sfunc, &sfunc, sizeof(sfunc)));
   return 0;
ONERR:
   return EINVAL;
}

static void test_end1_sf(syncfunc_param_t *sfparam)
{
   assert(sfparam->sfunc->err == EINVAL);
   end_syncfunc(sfparam, {
      if (sfparam->sfunc->err == 0) {
         sfparam->sfunc->state = (void*)(intptr_t)-1;
      }
   });
   // returns before goto
   goto syncfunc_END;
}

static void test_end2_sf(syncfunc_param_t * sfparam)
{
   sfparam->sfunc->err = EINVAL;
   goto syncfunc_END;

   end_syncfunc(sfparam, {
      if (sfparam->sfunc->err != 0) {
         sfparam->sfunc->state = (void*)(intptr_t)-1;
      }
   });
}

static void test_start_sf(syncfunc_param_t* sfparam)
{
   begin_syncfunc(sfparam);

   sfparam->sfunc->contoffset = getoffset_syncfunc(ONCONTINUE);
   seterr_syncfunc(sfparam->sfunc, 10);
   return;

ONCONTINUE:
   seterr_syncfunc(sfparam->sfunc, 11);
   return;

   end_syncfunc(sfparam, {
      seterr_syncfunc(sfparam->sfunc, 12);
   });
}

static void test_exit_sf(syncfunc_param_t * sfparam)
{
   int iserr = 0;
   exit_syncfunc(sfparam, (int)(intptr_t)(sfparam->sfunc->state));
   iserr = 1; // not executed
   end_syncfunc(sfparam, {
      if (iserr) seterr_syncfunc(sfparam->sfunc, EINVAL);
   });
}

static void test_spinwait_sf(syncfunc_param_t *sfparam)
{
   begin_syncfunc(sfparam);

   spinwait_syncfunc(sfparam, {
      unsigned *condition = state_syncfunc(sfparam);
      --(*condition) == 0;
   });

   end_syncfunc(sfparam, {
   });
}

static void test_wait_sf(syncfunc_param_t * sfparam)
{
   begin_syncfunc(sfparam);

// RUN

   int err = wait_syncfunc(sfparam, (void*)1);
   if (err) exit_syncfunc(sfparam, err);
   err = wait_syncfunc(sfparam, (void*)2);
   if (err) exit_syncfunc(sfparam, err);
   err = wait_syncfunc(sfparam, (void*)3);
   if (err) exit_syncfunc(sfparam, err);
   setcontoffset_syncfunc(sfparam->sfunc, 0);

   end_syncfunc(sfparam, {});
}

static void test_waiterr_sf(syncfunc_param_t *sfparam)
{
   begin_syncfunc(sfparam);

// RUN
   intptr_t err = err_syncfunc(sfparam->sfunc);
   err = wait_syncfunc(sfparam, (void*)err);
   err = wait_syncfunc(sfparam, (void*)err);
   err = wait_syncfunc(sfparam, (void*)err);

   end_syncfunc(sfparam, {
      setcontoffset_syncfunc(sfparam->sfunc, 0);
   });
}

static void test_yield_sf(syncfunc_param_t * sfparam)
{
   int iserr = 1;
   begin_syncfunc(sfparam);

// RUN
   sfparam->sfunc->err += 19;
   yield_syncfunc(sfparam);
   sfparam->sfunc->err += 20;
   yield_syncfunc(sfparam);
   iserr = 0;

   end_syncfunc(sfparam, {
      sfparam->sfunc->err += 21;
      setcontoffset_syncfunc(sfparam->sfunc, 0);
      if (iserr) seterr_syncfunc(sfparam->sfunc, -1);
   });
}

static int test_implsupport(void)
{
   syncfunc_helper_t helper;
   syncfunc_param_t  sfparam = syncfunc_param_INIT(0, &helper.sfit);
   syncfunc_t        sfunc;

   // prepare
   memset(&sfunc, 0, sizeof(sfunc));
   sfparam.sfunc = &sfunc;

   // TEST state_syncfunc
   for (uintptr_t i = 1; i; i <<= 1) {
      sfunc.state = (void*) i;
      TEST((void*)i == state_syncfunc(&sfparam));
   }
   sfunc.state = (void*) 0;
   TEST(0 == state_syncfunc(&sfparam));

   // TEST setstate_syncfunc
   for (uintptr_t i = 1; i; i <<= 1) {
      setstate_syncfunc(&sfparam, (void*) i);
      TEST((void*)i == state_syncfunc(&sfparam));
   }
   setstate_syncfunc(&sfparam, 0);
   TEST(0 == state_syncfunc(&sfparam));

   // TEST end_syncfunc: err set to 0 && free_resource codeblock executed && exitsf called
   reset_helper(&helper);
   init_syncfunc(&sfunc, 0, 0);
   sfunc.err = EINVAL; // expected
   test_end1_sf(&sfparam);
   // check sfparam,sfunc,helper
   TEST( sfparam.srun     == 0);
   TEST( sfparam.sfunc    == &sfunc);
   TEST( sfparam.iimpl    == &helper.sfit);
   TEST( sfunc.err        == 0);
   TEST( sfunc.state      == (void*)(intptr_t)-1); // free codeblock called
   TEST( sfunc.contoffset == 0);
   TEST( sfunc.endoffset  == 0);
   TEST( 0 == testexec_helper(&helper, 1, sfunc));

   // TEST end_syncfunc: err set + jump ==> err not cleared && free_resource codeblock executed
   reset_helper(&helper);
   init_syncfunc(&sfunc, 0, 0);
   test_end2_sf(&sfparam);
   // check sfparam,sfunc,helper
   TEST( sfparam.srun     == 0);
   TEST( sfparam.sfunc    == &sfunc);
   TEST( sfparam.iimpl    == &helper.sfit);
   TEST( sfunc.err        == EINVAL);
   TEST( sfunc.state      == (void*)(intptr_t)-1); // free codeblock called
   TEST( sfunc.contoffset == 0);
   TEST( sfunc.endoffset  == 0);
   TEST( 0 == testexec_helper(&helper, 1, sfunc));

   // TEST begin_syncfunc: contoffset / endoffset
   reset_helper(&helper);
   init_syncfunc(&sfunc, 0, 0);
   for (unsigned r = 10; r < 13; ++r) {
      unsigned isExit = (r == 12);
      sfunc.err = 0;
      if (isExit) sfunc.contoffset = sfunc.endoffset; // force terminate
      test_start_sf(&sfparam);
      // check sfparam,sfunc,helper
      TEST( sfparam.srun     == 0);
      TEST( sfparam.sfunc    == &sfunc);
      TEST( sfparam.iimpl    == &helper.sfit);
      TEST( sfunc.err        == (int)r);
      TEST( sfunc.state      == 0);
      TEST( sfunc.contoffset != 0);
      TEST( sfunc.endoffset  != 0);
      TEST( 0 == testexec_helper(&helper, isExit, sfunc));
   }

   // TEST exit_syncfunc
   for (uintptr_t i = 0; i < 3; ++i) {
      reset_helper(&helper);
      init_syncfunc(&sfunc, (syncfunc_f)0, (void*)i);
      test_exit_sf(&sfparam);
      // check sfparam,sfunc,helper
      TEST( sfparam.srun     == 0);
      TEST( sfparam.sfunc    == &sfunc);
      TEST( sfparam.iimpl    == &helper.sfit);
      TEST( sfunc.err        == (int)i);
      TEST( sfunc.state      == (void*)i);
      TEST( sfunc.contoffset == 0);
      TEST( sfunc.endoffset  == 0);
      TEST( 0 == testexec_helper(&helper, 1, sfunc));
   }

   // TEST spinwait_syncfunc
   for (unsigned i=136, condition=137; i <= 136; --i) {
      reset_helper(&helper);
      init_syncfunc(&sfunc, (syncfunc_f)0, &condition);
      test_spinwait_sf(&sfparam);
      // check condition,sfparam,sfunc,helper
      TEST( condition        == i);
      TEST( sfparam.srun     == 0);
      TEST( sfparam.sfunc    == &sfunc);
      TEST( sfparam.iimpl    == &helper.sfit);
      TEST( sfunc.err        == 0);
      TEST( sfunc.state      == &condition);
      TEST( sfunc.contoffset != 0);
      TEST( sfunc.endoffset  != 0);
      TEST( 0 == testexec_helper(&helper, (condition == 0), sfunc));
   }

   // TEST wait_syncfunc: err == 0
   init_syncfunc(&sfunc, (syncfunc_f)0, (void*)0);
   for (uintptr_t i = 1; i <= 4; ++i) {
      unsigned isExit = (i == 4);
      int16_t oldoff = sfunc.contoffset;
      reset_helper(&helper);
      test_wait_sf(&sfparam);
      // check sfparam,sfunc,helper
      TEST( sfparam.srun     == 0);
      TEST( sfparam.sfunc    == &sfunc);
      TEST( sfparam.iimpl    == &helper.sfit);
      TEST( sfunc.err        == 0);
      TEST( sfunc.state      == 0);
      TEST( sfunc.contoffset != oldoff);
      TEST( sfunc.endoffset  != 0);
      if (isExit) {
         TEST( 0 == testexec_helper(&helper, 1, sfunc));
      } else {
         syncfunc_t sf2 = sfunc;
         sf2.contoffset = oldoff; // waitsf called before contoffset is set
         TEST( 0 == testwait_helper(&helper, 1, sf2, (syncwait_t*)i));
      }
   }

   // TEST wait_syncfunc: err != 0
   init_syncfunc(&sfunc, (syncfunc_f)0, (void*)0);
   for (uintptr_t i = 1; i <= 4; ++i) {
      unsigned isExit = (i == 4);
      int16_t oldoff = sfunc.contoffset;
      seterr_syncfunc(&sfunc, (int)i);
      reset_helper(&helper);
      test_waiterr_sf(&sfparam);
      // check sfparam,sfunc,helper
      TEST( sfparam.srun     == 0);
      TEST( sfparam.sfunc    == &sfunc);
      TEST( sfparam.iimpl    == &helper.sfit);
      TEST( sfunc.err        == (isExit?0:(int)i)); // cleared at exit
      TEST( sfunc.state      == 0);
      TEST( sfunc.contoffset != oldoff);
      TEST( sfunc.endoffset  != 0);
      if (isExit) {
         TEST( 0 == testexec_helper(&helper, 1, sfunc));
      } else {
         syncfunc_t sf2 = sfunc;
         sf2.contoffset = oldoff; // waitsf called before contoffset is set
         TEST( 0 == testwait_helper(&helper, 1, sf2, (syncwait_t*)i));
      }
   }

   // TEST yield_syncfunc
   init_syncfunc(&sfunc, (syncfunc_f)0, (void*)0);
   for (unsigned i = 19; i <= 21; ++i) {
      unsigned isExit = (i == 21);
      int16_t oldoff = sfunc.contoffset;
      sfunc.err = 0;
      reset_helper(&helper);
      test_yield_sf(&sfparam);
      // check sfparam,sfunc,helper
      TEST( sfparam.srun     == 0);
      TEST( sfparam.sfunc    == &sfunc);
      TEST( sfparam.iimpl    == &helper.sfit);
      TEST( sfunc.err        == (int)i);
      TEST( sfunc.state      == 0);
      TEST( sfunc.contoffset != oldoff);
      TEST( sfunc.endoffset  != 0);
      TEST( 0 == testexec_helper(&helper, isExit, sfunc));
   }

   return 0;
ONERR:
   return EINVAL;
}

int unittest_task_syncfunc()
{
   if (test_syncfunc_it())    goto ONERR;
   if (test_sfparam())        goto ONERR;
   if (test_initfree())       goto ONERR;
   if (test_query())          goto ONERR;
   if (test_update())         goto ONERR;
   if (test_helper())         goto ONERR;
   if (test_implsupport())    goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
