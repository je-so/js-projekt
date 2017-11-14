/* title: SyncWaitlist impl

   Implements <SyncWaitlist>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/task/syncwait.h
    Header file <SyncWaitlist>.

   file: C-kern/task/syncwait.c
    Implementation file <SyncWaitlist impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/task/syncwait.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/ds/inmem/queue.h"
#include "C-kern/api/task/syncrunner.h"
#endif


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   syncwait_t swait = syncwait_FREE;

   // TEST syncwait_FREE
   TEST(0 == isvalid_linkd(&swait.funclist));

   // TEST init_syncwait
   init_syncwait(&swait);
   TEST(&swait.funclist == swait.funclist.prev);
   TEST(&swait.funclist == swait.funclist.next);

   // TEST free_syncwait
   linkd_t dummy = linkd_FREE;
   addnode_syncwait(&swait, &dummy);
   TEST(isvalid_linkd(&swait.funclist));
   free_syncwait(&swait);
   TEST(! isvalid_linkd(&swait.funclist));
   // Link verwaist (nicht gelöscht)
   TEST(&swait.funclist == dummy.next);

   // TEST free_syncwait: double free
   free_syncwait(&swait);
   TEST(! isvalid_linkd(&swait.funclist));

   return 0;
ONERR:
   return EINVAL;
}

static int test_query(void)
{
   syncwait_t swait = syncwait_FREE;
   linkd_t sfunc1;
   linkd_t sfunc2;

   // TEST iswaiting_syncwait: syncwait_FREE
   TEST(! iswaiting_syncwait(&swait));

   // TEST getfirst_syncwait: syncwait_FREE
   TEST(0 == getfirst_syncwait(&swait));

   // TEST iswaiting_syncwait: after init
   init_syncwait(&swait);
   TEST(! iswaiting_syncwait(&swait));

   // TEST getfirst_syncwait: after init ==> Error precondition violated
   TEST(0 != getfirst_syncwait(&swait));

   // TEST iswaiting_syncwait: linked
   init_linkd(&swait.funclist, &sfunc1);
   TEST( iswaiting_syncwait(&swait));

   // TEST getfirst_syncwait: single node
   init_linkd(&swait.funclist, &sfunc1);
   TEST(&sfunc1 == getfirst_syncwait(&swait));
   init_linkd(&swait.funclist, &sfunc2);
   TEST(&sfunc2 == getfirst_syncwait(&swait));

   // TEST getfirst_syncwait: multiple nodes (next is used)
   init_linkd(&swait.funclist, &sfunc1);
   initprev_linkd(&sfunc2, &swait.funclist);
   TEST(&sfunc1 == getfirst_syncwait(&swait));
   init_linkd(&swait.funclist, &sfunc1);
   initnext_linkd(&sfunc2, &swait.funclist);
   TEST(&sfunc2 == getfirst_syncwait(&swait));

   return 0;
ONERR:
   return EINVAL;
}

static int test_update(void)
{
   syncwait_t swait = syncwait_FREE;
   linkd_t    sfunc1 = linkd_FREE;
   linkd_t    sfunc2 = linkd_FREE;

   // prepare
   init_syncwait(&swait);

   // TEST addnode_syncwait: empty
   addnode_syncwait(&swait, &sfunc1);
   TEST(swait.funclist.prev == &sfunc1);
   TEST(swait.funclist.next == &sfunc1);
   TEST(sfunc1.prev == &swait.funclist);
   TEST(sfunc1.next == &swait.funclist);
   TEST(&sfunc1 == getfirst_syncwait(&swait));

   // TEST addnode_syncwait: not empty, add to end of list (prev)
   addnode_syncwait(&swait, &sfunc2);
   TEST(swait.funclist.prev == &sfunc2);
   TEST(swait.funclist.next == &sfunc1);
   TEST(sfunc1.prev == &swait.funclist);
   TEST(sfunc1.next == &sfunc2);
   TEST(sfunc2.prev == &sfunc1);
   TEST(sfunc2.next == &swait.funclist);
   TEST(&sfunc1 == getfirst_syncwait(&swait));

   // TEST removenode_syncwait: single node
   init_syncwait(&swait);
   addnode_syncwait(&swait, &sfunc1);
   TEST( removenode_syncwait(&swait) == &sfunc1);
   // check swait points to self
   TEST(&swait.funclist == swait.funclist.prev);
   TEST(&swait.funclist == swait.funclist.next);
   // check sfunc1 not changed
   TEST(sfunc1.prev == &swait.funclist);
   TEST(sfunc1.next == &swait.funclist);

   // TEST removenode_syncwait: two nodes
   init_syncwait(&swait);
   addnode_syncwait(&swait, &sfunc1);
   addnode_syncwait(&swait, &sfunc2);
   TEST( removenode_syncwait(&swait) == &sfunc1);
   // check swait points to second node
   TEST(swait.funclist.prev == &sfunc2);
   TEST(swait.funclist.next == &sfunc2);
   // check sfunc1 not changed
   TEST(sfunc1.prev == &swait.funclist);
   TEST(sfunc1.next == &sfunc2);
   // check sfunc2 adapted
   TEST(sfunc2.prev == &swait.funclist);
   TEST(sfunc2.next == &swait.funclist);
   TEST( removenode_syncwait(&swait) == &sfunc2);
   // check swait points to self
   TEST(&swait.funclist == swait.funclist.prev);
   TEST(&swait.funclist == swait.funclist.next);
   // check sfunc2 not changed
   TEST(sfunc2.prev == &swait.funclist);
   TEST(sfunc2.next == &swait.funclist);

   // TEST removelist_syncwait: single node
   init_syncwait(&swait);
   addnode_syncwait(&swait, &sfunc1);
   TEST( removelist_syncwait(&swait) == &sfunc1);
   // check swait points to self
   TEST(&swait.funclist == swait.funclist.prev);
   TEST(&swait.funclist == swait.funclist.next);
   // check sfunc1 points to itself
   TEST(sfunc1.prev == &sfunc1);
   TEST(sfunc1.next == &sfunc1);

   // TEST removelist_syncwait: two nodes
   init_syncwait(&swait);
   addnode_syncwait(&swait, &sfunc1);
   addnode_syncwait(&swait, &sfunc2);
   TEST( removelist_syncwait(&swait) == &sfunc1);
   // check swait points to self
   TEST(&swait.funclist == swait.funclist.prev);
   TEST(&swait.funclist == swait.funclist.next);
   // check sfunc1, sfunc2 form a list
   TEST(sfunc1.prev == &sfunc2);
   TEST(sfunc1.next == &sfunc2);
   TEST(sfunc2.prev == &sfunc1);
   TEST(sfunc2.next == &sfunc1);

   // unprepare
   free_syncwait(&swait);

   return 0;
ONERR:
   free_syncwait(&swait);
   return EINVAL;
}

int unittest_task_syncwait()
{
   if (test_initfree())    goto ONERR;
   if (test_query())       goto ONERR;
   if (test_update())      goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
