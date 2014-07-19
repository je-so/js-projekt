/* title: SyncRunner impl

   Implements <SyncRunner>.

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

   file: C-kern/api/task/syncrunner.h
    Header file <SyncRunner>.

   file: C-kern/task/syncrunner.c
    Implementation file <SyncRunner impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/task/syncrunner.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif

// TODO: manage internally OPTIONAL field :: syncwait_node_t exitcondition;



// section: syncrunner_t

// group: lifetime


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   syncrunner_t srun = syncrunner_FREE;

   // TEST syncrunner_FREE
   for (unsigned i = 0; i < lengthof(srun.queues); ++i) {
      TEST(isfree_syncqueue(&srun.queues[i]));
   }

   return 0;
ONERR:
   return EINVAL;
}

int unittest_task_syncrunner()
{
   if (test_initfree())       goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
