/* title: Exoscheduler impl
   Implements <Exoscheduler>.

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
   (C) 2011 Jörg Seebohn

   file: C-kern/api/os/task/exoscheduler.h
    Header file of <Exoscheduler>.

   file: C-kern/os/shared/task/exoscheduler.c
    Implementation file <Exoscheduler impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/os/task/exoscheduler.h"
#include "C-kern/api/err.h"
#include "C-kern/api/os/task/exothread.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif

/* functions: exothread_list
 * All <slist_t> functions are adapted to subtype <exothread_list_t>.
 * The macro <slist_IMPLEMENT> does this for us. */
slist_IMPLEMENT(exothread_list, next, callback_aspect_t)

int init_exoscheduler(/*out*/exoscheduler_t * xsched)
{
   exoscheduler_t new_xsched = exoscheduler_INIT ;

   MEMCOPY(xsched, (const exoscheduler_t*)&new_xsched) ;

   return 0 ;
}

int free_exoscheduler(exoscheduler_t * xsched)
{
   int err ;

   // no free function is required at this time (cause memory is managed by user having registered any xthread)

   err = free_exothread_list(&xsched->runlist, 0, 0) ;
   xsched->runlist_size = 0 ;

   if (err) goto ABBRUCH ;

   return 0;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}

int register_exoscheduler(exoscheduler_t * xsched, exothread_t * xthread)
{
   int err ;

   err = insertlast_exothread_list(&xsched->runlist, xthread) ;
   if (err) goto ABBRUCH ;
   ++ xsched->runlist_size ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int run_exoscheduler(exoscheduler_t * xsched)
{
   int err = 0 ;
   exothread_t * prev     = last_exothread_list(&xsched->runlist) ;
   exothread_t * xthread  = first_exothread_list(&xsched->runlist) ;
   bool          isfinish = false ;

   assert(xthread || 0 == xsched->runlist_size) ;

   for(size_t i = xsched->runlist_size; i; --i) {

      isfinish = isfinish_exothread(xthread) ;

      if (!isfinish) {
         int err2 = run_exothread( xthread ) ;
         if (err2) {
            err = err2 ;
            assert(0 == err2 && "run_exothread should never return an error") ;
         }
         isfinish = isfinish_exothread(xthread) ;
      }

      if (isfinish) {
         // prev keeps the same
         xthread = next_exothread_list( xthread ) ;

         int err2 ;
         exothread_t * removed ;
         err2 = removeafter_exothread_list(&xsched->runlist, prev, &removed) ;
         if (err2) {
            err = err2 ;
         } else {
            -- xsched->runlist_size ;
         }
         assert(0 == err2 && "removeafter_exothread_list should never return an error") ;
      } else {
         prev    = xthread ;
         xthread = next_exothread_list( xthread ) ;
      }
   }

   if (err) goto ABBRUCH ;
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}


#ifdef KONFIG_UNITTEST

#define TEST(CONDITION) TEST_ONERROR_GOTO(CONDITION,unittest_os_task_exoscheduler,ABBRUCH)

static int simplefinish_xthread(exothread_t * xthread)
{
   finish_exothread() ;
   return 0 ;
}

static int test_initfree(void)
{
   exoscheduler_t    xsched        = exoscheduler_INIT ;
   exothread_t       xthreads[100] = { exothread_INIT_FREEABLE } ;

   // TEST static init
   memset(&xsched, 1, sizeof(xsched)) ;
   xsched = (exoscheduler_t)exoscheduler_INIT ;
   TEST(0 == xsched.runlist.last) ;
   TEST(0 == xsched.runlist_size) ;

   // TEST init, double free
   memset(&xsched, 1, sizeof(xsched)) ;
   TEST(0 == init_exoscheduler(&xsched)) ;
   TEST(0 == xsched.runlist.last) ;
   TEST(0 == xsched.runlist_size) ;
   TEST(0 == free_exoscheduler(&xsched)) ;
   TEST(0 == free_exoscheduler(&xsched)) ;
   TEST(0 == xsched.runlist.last) ;
   TEST(0 == xsched.runlist_size) ;

   // TEST register, free
   TEST(0 == init_exoscheduler(&xsched)) ;
   for(unsigned i = 0; i < nrelementsof(xthreads); ++i) {
      TEST(0 == init_exothread(&xthreads[i], &simplefinish_xthread)) ;
      TEST(0 == register_exoscheduler(&xsched, &xthreads[i])) ;
      TEST(&xthreads[0] == next_exothread_list(xsched.runlist.last)) ;
      TEST(&xthreads[i] == xsched.runlist.last) ;
      TEST(i + 1 == xsched.runlist_size) ;
   }
   TEST(0 == free_exoscheduler(&xsched)) ;
   TEST(0 == xsched.runlist_size) ;
   TEST(0 == xsched.runlist.last) ;
   for(unsigned i = 0; i < nrelementsof(xthreads); ++i) {
      TEST(0 == isfinish_exothread(&xthreads[i])) ;
      TEST(0 == free_exothread(&xthreads[i])) ;
   }

   // TEST run => automatic unregister in case of finish
   TEST(0 == init_exoscheduler(&xsched)) ;
   for(unsigned i = 0; i < nrelementsof(xthreads); ++i) {
      TEST(0 == init_exothread(&xthreads[i], &simplefinish_xthread)) ;
      TEST(0 == register_exoscheduler(&xsched, &xthreads[i])) ;
      TEST(i + 1 == xsched.runlist_size) ;
   }
   TEST(0 == run_exoscheduler(&xsched)) ;
   TEST(0 == xsched.runlist_size) ;
   TEST(0 == xsched.runlist.last) ;
   for(unsigned i = 0; i < nrelementsof(xthreads); ++i) {
      TEST(0 != isfinish_exothread(&xthreads[i])) ;
      TEST(0 == free_exothread(&xthreads[i])) ;
   }
   TEST(0 == free_exoscheduler(&xsched)) ;

   // TEST run => automatic unregister if xthread has already finished
   TEST(0 == init_exoscheduler(&xsched)) ;
   for(unsigned i = 0; i < nrelementsof(xthreads); ++i) {
      TEST(0 == init_exothread(&xthreads[i], &simplefinish_xthread)) ;
      TEST(0 == register_exoscheduler(&xsched, &xthreads[i])) ;
      TEST(i + 1 == xsched.runlist_size) ;
   }
   for(unsigned i = 0; i < nrelementsof(xthreads); ++i) {
      if (i%2) {
         TEST(0 == run_exothread(&xthreads[i])) ;
      } else {
         abort_exothread(&xthreads[i]) ;
      }
   }
   TEST(0 == run_exoscheduler(&xsched)) ;
   TEST(0 == xsched.runlist_size) ;
   TEST(0 == xsched.runlist.last) ;
   for(unsigned i = 0; i < nrelementsof(xthreads); ++i) {
      TEST(0 != isfinish_exothread(&xthreads[i])) ;
      TEST(((i%2)? 0 : ECANCELED) == returncode_exothread(&xthreads[i])) ;
      TEST(0 == free_exothread(&xthreads[i])) ;
   }
   TEST(0 == free_exoscheduler(&xsched)) ;

   return 0 ;
ABBRUCH:
   (void) free_exoscheduler(&xsched) ;
   for(unsigned i = 0; i < nrelementsof(xthreads); ++i) {
      TEST(0 == free_exothread(&xthreads[i])) ;
   }
   return EINVAL ;
}

int unittest_os_task_exoscheduler()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())          goto ABBRUCH ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
