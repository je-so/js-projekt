/* title: SyncCondition impl

   Implements <SyncCondition>.

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
   synccond_t   scond = synccond_FREE;

   // TEST synccond_FREE
   TEST(0 == isvalid_link(&scond.waitfunc));

   // TEST init_synccond
   memset(&scond, 255, sizeof(scond));
   TEST(0 == init_synccond(&scond));
   TEST(0 == isvalid_link(&scond.waitfunc));

   // TEST free_synccond
   syncfunc_t dummy = syncfunc_FREE;
   link_synccond(&scond, &dummy);
   TEST(isvalid_link(&scond.waitfunc));
   TEST(0 == free_synccond(&scond));
   TEST(! isvalid_link(&scond.waitfunc));
   // Link verwaist (nicht gelöscht)
   TEST(&scond.waitfunc == dummy.waitfor.link);

   // TEST free_synccond: double free
   TEST(0 == free_synccond(&scond));
   TEST(! isvalid_link(&scond.waitfunc));

   return 0;
ONERR:
   return EINVAL;
}

static int test_query(void)
{
   synccond_t scond = synccond_FREE;
   syncfunc_t sfunc1;
   syncfunc_t sfunc2;

   // TEST iswaiting_synccond: empty
   TEST(! iswaiting_synccond(&scond));

   // TEST waitfunc_synccond: empty ==> precondition violated
   TEST(0 != waitfunc_synccond(&scond));

   // TEST iswaiting_synccond: linked
   init_link(&scond.waitfunc, &sfunc1.waitfor);
   TEST( iswaiting_synccond(&scond));

   // TEST waitfunc_synccond: linked
   init_link(&scond.waitfunc, &sfunc1.waitfor);
   TEST(&sfunc1 == waitfunc_synccond(&scond));
   init_link(&scond.waitfunc, &sfunc2.waitfor);
   TEST(&sfunc2 == waitfunc_synccond(&scond));

   return 0;
ONERR:
   return EINVAL;
}

static int test_update(void)
{
   synccond_t   scond = synccond_FREE;
   syncrunner_t srun  = syncrunner_FREE;
   syncfunc_t * sfunc[10];
   syncfunc_t   sfunc1 = syncfunc_FREE;
   syncfunc_t   sfunc2 = syncfunc_FREE;
   unsigned     qidx;

   // prepare
   TEST(0 == init_syncrunner(&srun));
   TEST(0 == init_synccond(&scond));

   // TEST link_synccond: empty
   link_synccond(&scond, &sfunc1);
   TEST(&scond.waitfunc == sfunc1.waitfor.link);
   TEST(&sfunc1.waitfor == scond.waitfunc.link);
   TEST(&sfunc1 == waitfunc_synccond(&scond));

   // TEST link_synccond: not empty => overwrite link
   link_synccond(&scond, &sfunc2);
   TEST(&scond.waitfunc == sfunc2.waitfor.link);
   TEST(&sfunc2.waitfor == scond.waitfunc.link);
   TEST(&sfunc2 == waitfunc_synccond(&scond));
   // dangling old link !!
   TEST(&scond.waitfunc == sfunc1.waitfor.link);

   // TEST unlink_synccond
   link_synccond(&scond, &sfunc1);
   unlink_synccond(&scond);
   TEST(0 == scond.waitfunc.link);
   TEST(0 == sfunc1.waitfor.link);
   link_synccond(&scond, &sfunc2);
   unlink_synccond(&scond);
   TEST(0 == scond.waitfunc.link);
   TEST(0 == sfunc2.waitfor.link);

   // TEST unlink_synccond: empty => does nothing
   TEST(0 == scond.waitfunc.link);
   unlink_synccond(&scond);
   TEST(0 == scond.waitfunc.link);

   // TEST wakeup_synccond: leer, niemand wartet
   TEST(! iswaiting_synccond(&scond))
   TEST(0 == wakeup_synccond(&scond, &srun));
   TEST(! iswaiting_synccond(&scond))
   TEST(isself_linkd(&srun.wakeup));

   // TEST wakeupall_synccond: leer, niemand wartet
   TEST(! iswaiting_synccond(&scond))
   TEST(0 == wakeupall_synccond(&scond, &srun));
   TEST(! iswaiting_synccond(&scond))
   TEST(isself_linkd(&srun.wakeup));

   // prepare: fill wait queue
   syncfunc_opt_e optfields = syncfunc_opt_WAITFOR | syncfunc_opt_WAITLIST;
   for (qidx = lengthof(srun.rwqueue)-1; qidx; --qidx) {
      if (getsize_syncfunc(optfields) == elemsize_syncqueue(&srun.rwqueue[qidx])) break;
   }
   TEST(getsize_syncfunc(optfields) == elemsize_syncqueue(&srun.rwqueue[qidx]));
   for (int i = 0; i < 10; ++i) {
      TEST(0 == preallocate_syncqueue(&srun.rwqueue[qidx]));
      sfunc[i] = nextfree_syncqueue(&srun.rwqueue[qidx]);
      memset(sfunc[i], 0, elemsize_syncqueue(&srun.rwqueue[qidx]));
      if (i == 0) {
         init_link(addrwaitfor_syncfunc(sfunc[i]), &scond.waitfunc);
         initself_linkd(addrwaitlist_syncfunc(sfunc[i], true));
      } else {
         initprev_linkd(addrwaitlist_syncfunc(sfunc[i], true), addrwaitlist_syncfunc(sfunc[0], true));
      }
   }

   // TEST wakeup_synccond: wakeup nächste Funktion
   TEST(isself_linkd(&srun.wakeup));
   for (int i = 0; i < 10; ++i) {
      TEST(scond.waitfunc.link == &sfunc[i]->waitfor);
      TEST(&scond.waitfunc     == sfunc[i]->waitfor.link);
      if (i < 9) {
         TEST(&sfunc[i+1]->waitlist == sfunc[i]->waitlist.next);
         TEST(&sfunc[9]->waitlist   == sfunc[i]->waitlist.prev);
      } else {
         TEST(0 == sfunc[i]->waitlist.next);
         TEST(0 == sfunc[i]->waitlist.prev);
      }
      TEST(0 == wakeup_synccond(&scond, &srun));
      TEST(0 == sfunc[i]->waitfor.link);
      if (i > 0) {
         TEST(&srun.wakeup == sfunc[i]->waitlist.next);
         TEST(&sfunc[i-1]->waitlist == sfunc[i]->waitlist.prev);
      } else {
         TEST(&srun.wakeup == sfunc[i]->waitlist.next);
         TEST(&srun.wakeup == sfunc[i]->waitlist.prev);
      }
   }
   TEST(0 == scond.waitfunc.link);

   // prepare: fill wait queue
   TEST(0 == free_syncrunner(&srun));
   TEST(0 == init_syncrunner(&srun));
   TEST(getsize_syncfunc(optfields) == elemsize_syncqueue(&srun.rwqueue[qidx]));
   for (int i = 0; i < 10; ++i) {
      TEST(0 == preallocate_syncqueue(&srun.rwqueue[qidx]));
      sfunc[i] = nextfree_syncqueue(&srun.rwqueue[qidx]);
      memset(sfunc[i], 0, elemsize_syncqueue(&srun.rwqueue[qidx]));
      if (i == 0) {
         init_link(addrwaitfor_syncfunc(sfunc[i]), &scond.waitfunc);
         initself_linkd(addrwaitlist_syncfunc(sfunc[i], true));
      } else {
         initprev_linkd(addrwaitlist_syncfunc(sfunc[i], true), addrwaitlist_syncfunc(sfunc[0], true));
      }
   }

   // TEST wakeupall_synccond
   TEST(isself_linkd(&srun.wakeup));
   TEST(scond.waitfunc.link == &sfunc[0]->waitfor);
   TEST(&scond.waitfunc     == sfunc[0]->waitfor.link);
   TEST(0 == wakeupall_synccond(&scond, &srun));
   // link cleared
   TEST(! isvalid_link(&sfunc[0]->waitfor));
   TEST(! isvalid_link(&scond.waitfunc));
   // alle Wartenden in wakeup list aufgenommen
   TEST(addrwaitlist_syncfunc(sfunc[0], true) == srun.wakeup.next);
   TEST(addrwaitlist_syncfunc(sfunc[9], true) == srun.wakeup.prev);
   TEST(addrwaitlist_syncfunc(sfunc[0], true)->prev == &srun.wakeup);
   TEST(addrwaitlist_syncfunc(sfunc[9], true)->next == &srun.wakeup);
   for (int i = 0; i < 10; ++i) {
      if (i > 0) {
         TEST(addrwaitlist_syncfunc(sfunc[i-1], true) == addrwaitlist_syncfunc(sfunc[i], true)->prev);
      }
      if (i < 9) {
         TEST(addrwaitlist_syncfunc(sfunc[i+1], true) == addrwaitlist_syncfunc(sfunc[i], true)->next);
      }
   }

   // prepare
   TEST(0 == free_syncrunner(&srun));
   TEST(0 == init_syncrunner(&srun));

   // TEST wakeup_synccond: EINVAL
   syncfunc_t dummy = syncfunc_FREE;
   link_synccond(&scond, &dummy);
   TEST(isvalid_link(&scond.waitfunc));
   TEST(EINVAL == wakeup_synccond(&scond, &srun));
   // nothing changed
   TEST(scond.waitfunc.link == &dummy.waitfor);
   TEST(&scond.waitfunc     == dummy.waitfor.link);
   TEST(isself_linkd(&srun.wakeup));

   // TEST wakeupall_synccond: EINVAL
   TEST(EINVAL == wakeupall_synccond(&scond, &srun));
   // nothing changed
   TEST(scond.waitfunc.link == &dummy.waitfor);
   TEST(&scond.waitfunc     == dummy.waitfor.link);
   TEST(isself_linkd(&srun.wakeup));

   // unprepare
   TEST(0 == free_syncrunner(&srun));
   TEST(0 == free_synccond(&scond));

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
