/* title: SyncCondition impl

   Implements <SyncCondition>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/task/synccond.h
    Header file <SyncCondition>.

   file: C-kern/task/synccond.c
    Implementation file <SyncCondition impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/task/synccond.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/ds/inmem/queue.h"
#include "C-kern/api/task/syncfunc.h"
#include "C-kern/api/task/syncrunner.h"
#endif


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   synccond_t scond = synccond_FREE;

   // TEST synccond_FREE
   TEST(0 == isvalid_linkd(&scond.waitfunc));

   // TEST init_synccond
   init_synccond(&scond);
   TEST(&scond.waitfunc == scond.waitfunc.prev);
   TEST(&scond.waitfunc == scond.waitfunc.next);

   // TEST free_synccond
   syncfunc_t dummy = syncfunc_FREE;
   link_synccond(&scond, &dummy);
   TEST(isvalid_linkd(&scond.waitfunc));
   free_synccond(&scond);
   TEST(! isvalid_linkd(&scond.waitfunc));
   // Link verwaist (nicht gelöscht)
   TEST(&scond.waitfunc == dummy.waitlist.next);

   // TEST free_synccond: double free
   free_synccond(&scond);
   TEST(! isvalid_linkd(&scond.waitfunc));

   return 0;
ONERR:
   return EINVAL;
}

static int test_query(void)
{
   synccond_t scond = synccond_FREE;
   syncfunc_t sfunc1;
   syncfunc_t sfunc2;

   // TEST iswaiting_synccond: synccond_FREE
   TEST(! iswaiting_synccond(&scond));

   // TEST waitfunc_synccond: synccond_FREE ==> Error precondition violated
   TEST(0 != waitfunc_synccond(&scond));

   // TEST iswaiting_synccond: after init
   init_synccond(&scond);
   TEST(! iswaiting_synccond(&scond));

   // TEST waitfunc_synccond: after init ==> Error precondition violated
   TEST(0 != waitfunc_synccond(&scond));

   // TEST iswaiting_synccond: linked
   init_linkd(&scond.waitfunc, &sfunc1.waitlist);
   TEST( iswaiting_synccond(&scond));

   // TEST waitfunc_synccond: single node
   init_linkd(&scond.waitfunc, &sfunc1.waitlist);
   TEST(&sfunc1 == waitfunc_synccond(&scond));
   init_linkd(&scond.waitfunc, &sfunc2.waitlist);
   TEST(&sfunc2 == waitfunc_synccond(&scond));

   // TEST waitfunc_synccond: multiple nodes (next is used)
   init_linkd(&scond.waitfunc, &sfunc1.waitlist);
   initprev_linkd(&sfunc2.waitlist, &scond.waitfunc);
   TEST(&sfunc1 == waitfunc_synccond(&scond));
   init_linkd(&scond.waitfunc, &sfunc1.waitlist);
   initnext_linkd(&sfunc2.waitlist, &scond.waitfunc);
   TEST(&sfunc2 == waitfunc_synccond(&scond));

   return 0;
ONERR:
   return EINVAL;
}

// /// belongs to test_update

static int s_runcount;
static int s_runerr;

static int sf_wait(syncfunc_param_t* param, uint32_t cmd)
{
   int err = EINVAL;
   synccond_t* scond = state_syncfunc(param);

   start_syncfunc(param, cmd, ONRUN, ONEXIT);

ONRUN:

   err = wait_syncfunc(param, scond);
   ++ s_runcount;

ONEXIT:
   if (err) s_runerr = err;
   exit_syncfunc(param, 0);
}

// ///

static int test_update(void)
{
   synccond_t   scond = synccond_FREE;
   syncrunner_t srun  = syncrunner_FREE;
   syncfunc_t   sfunc1 = syncfunc_FREE;
   syncfunc_t   sfunc2 = syncfunc_FREE;
   syncfunc_param_t sfparam =  syncfunc_param_INIT(&srun);

   // prepare
   TEST(0 == init_syncrunner(&srun));
   init_synccond(&scond);

   // TEST link_synccond: empty
   link_synccond(&scond, &sfunc1);
   TEST(scond.waitfunc.prev == &sfunc1.waitlist);
   TEST(scond.waitfunc.next == &sfunc1.waitlist);
   TEST(sfunc1.waitlist.prev == &scond.waitfunc);
   TEST(sfunc1.waitlist.next == &scond.waitfunc);
   TEST(&sfunc1 == waitfunc_synccond(&scond));

   // TEST link_synccond: not empty, add to end of list (prev)
   link_synccond(&scond, &sfunc2);
   TEST(scond.waitfunc.prev == &sfunc2.waitlist);
   TEST(scond.waitfunc.next == &sfunc1.waitlist);
   TEST(sfunc1.waitlist.prev == &scond.waitfunc);
   TEST(sfunc1.waitlist.next == &sfunc2.waitlist);
   TEST(sfunc2.waitlist.prev == &sfunc1.waitlist);
   TEST(sfunc2.waitlist.next == &scond.waitfunc);
   TEST(&sfunc1 == waitfunc_synccond(&scond));

   // TEST unlink_synccond: single node
   init_synccond(&scond);
   link_synccond(&scond, &sfunc1);
   unlink_synccond(&scond);
   // check scond points to self
   TEST(&scond.waitfunc == scond.waitfunc.prev);
   TEST(&scond.waitfunc == scond.waitfunc.next);
   // check sfunc1 not changed
   TEST(sfunc1.waitlist.prev == &scond.waitfunc);
   TEST(sfunc1.waitlist.next == &scond.waitfunc);

   // TEST unlink_synccond: two nodes
   init_synccond(&scond);
   link_synccond(&scond, &sfunc1);
   link_synccond(&scond, &sfunc2);
   unlink_synccond(&scond);
   // check scond points to second node
   TEST(scond.waitfunc.prev == &sfunc2.waitlist);
   TEST(scond.waitfunc.next == &sfunc2.waitlist);
   // check sfunc1 not changed
   TEST(sfunc1.waitlist.prev == &scond.waitfunc);
   TEST(sfunc1.waitlist.next == &sfunc2.waitlist);
   // check sfunc2 adapted
   TEST(sfunc2.waitlist.prev == &scond.waitfunc);
   TEST(sfunc2.waitlist.next == &scond.waitfunc);
   unlink_synccond(&scond);
   // check scond points to self
   TEST(&scond.waitfunc == scond.waitfunc.prev);
   TEST(&scond.waitfunc == scond.waitfunc.next);
   // check sfunc2 not changed
   TEST(sfunc2.waitlist.prev == &scond.waitfunc);
   TEST(sfunc2.waitlist.next == &scond.waitfunc);

   // TEST unlinkall_synccond: single node
   init_synccond(&scond);
   link_synccond(&scond, &sfunc1);
   unlinkall_synccond(&scond);
   // check scond points to self
   TEST(&scond.waitfunc == scond.waitfunc.prev);
   TEST(&scond.waitfunc == scond.waitfunc.next);
   // check sfunc1 points to itself
   TEST(sfunc1.waitlist.prev == &sfunc1.waitlist);
   TEST(sfunc1.waitlist.next == &sfunc1.waitlist);

   // TEST unlinkall_synccond: two nodes
   init_synccond(&scond);
   link_synccond(&scond, &sfunc1);
   link_synccond(&scond, &sfunc2);
   unlinkall_synccond(&scond);
   // check scond points to self
   TEST(&scond.waitfunc == scond.waitfunc.prev);
   TEST(&scond.waitfunc == scond.waitfunc.next);
   // check sfunc1, sfunc2 form a list
   TEST(sfunc1.waitlist.prev == &sfunc2.waitlist);
   TEST(sfunc1.waitlist.next == &sfunc2.waitlist);
   TEST(sfunc2.waitlist.prev == &sfunc1.waitlist);
   TEST(sfunc2.waitlist.next == &sfunc1.waitlist);

   // TEST wakeup_synccond: empty, does nothing
   TEST(! iswaiting_synccond(&scond))
   TEST(0 == wakeup_synccond(&scond, &sfparam));
   TEST(! iswaiting_synccond(&scond))
   TEST(! iswakeup_syncrunner(&srun));

   // TEST wakeupall_synccond: empty, does nothing
   TEST(! iswaiting_synccond(&scond))
   TEST(0 == wakeupall_synccond(&scond, &sfparam));
   TEST(! iswaiting_synccond(&scond))
   TEST(! iswakeup_syncrunner(&srun));

   // prepare: fill wait queue
   s_runcount = 0;
   s_runerr = 0;
   for (int i = 0; i < 10; ++i) {
      TEST(0 == addfunc_syncrunner(&srun, &sf_wait, &scond))
   }
   TEST(0 == run_syncrunner(&srun));
   TEST(! iswakeup_syncrunner(&srun));
   TEST(0 == s_runcount);
   TEST(0 == s_runerr);

   // TEST wakeup_synccond
   for (int i = 1; i <= 10; ++i) {
      TEST(iswaiting_synccond(&scond));
      TEST(0 == wakeup_synccond(&scond, &sfparam));
      TEST(iswakeup_syncrunner(&srun));
      TEST(i == s_runcount+1);
      TEST(0 == run_syncrunner(&srun));
      TEST(! iswakeup_syncrunner(&srun));
      TEST(i == s_runcount);
    }
   TEST(0 == s_runerr);
   TEST(! iswaiting_synccond(&scond));

   // prepare: fill wait queue
   s_runcount = 0;
   for (int i = 0; i < 10; ++i) {
      TEST(0 == addfunc_syncrunner(&srun, &sf_wait, &scond))
   }
   TEST(0 == run_syncrunner(&srun));
   TEST(! iswakeup_syncrunner(&srun));
   TEST(0 == s_runcount);
   TEST(0 == s_runerr);

   // TEST wakeupall_synccond
   TEST(iswaiting_synccond(&scond));
   TEST(0 == wakeupall_synccond(&scond, &sfparam));
   TEST(iswakeup_syncrunner(&srun));
   TEST(! iswaiting_synccond(&scond));
   TEST(0 == s_runcount);
   TEST(0 == run_syncrunner(&srun));
   TEST(! iswakeup_syncrunner(&srun));
   TEST(10 == s_runcount);
   TEST(0  == s_runerr);

   // prepare
   syncfunc_t dummy = syncfunc_FREE;
   link_synccond(&scond, &dummy);
   TEST(iswaiting_synccond(&scond));

   // TEST wakeup_synccond: EINVAL
   TEST(EINVAL == wakeup_synccond(&scond, &sfparam));
   // nothing changed
   TEST(scond.waitfunc.next == &dummy.waitlist);
   TEST(dummy.waitlist.next == &scond.waitfunc);
   TEST(! iswakeup_syncrunner(&srun));

   // TEST wakeupall_synccond: EINVAL
   TEST(EINVAL == wakeupall_synccond(&scond, &sfparam));
   // nothing changed
   TEST(scond.waitfunc.next == &dummy.waitlist);
   TEST(dummy.waitlist.next == &scond.waitfunc);
   TEST(! iswakeup_syncrunner(&srun));

   // unprepare
   TEST(0 == free_syncrunner(&srun));
   free_synccond(&scond);

   return 0;
ONERR:
   free_syncrunner(&srun);
   free_synccond(&scond);
   return EINVAL;
}

int unittest_task_synccond()
{
   if (test_initfree())    goto ONERR;
   if (test_query())       goto ONERR;
   if (test_update())      goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
