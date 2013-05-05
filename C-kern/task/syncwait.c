/* title: SyncWait impl

   Implements <SyncWait>.

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

   file: C-kern/api/task/syncwait.h
    Header file <SyncWait>.

   file: C-kern/task/syncwait.c
    Implementation file <SyncWait impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/task/syncthread.h"
#include "C-kern/api/task/syncwait.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


// section: syncwait_t



// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree_event(void)
{
   syncevent_t    event = syncevent_INIT_FREEABLE ;
   syncwait_t     waiting ;

   // TEST syncevent_INIT_FREEABLE
   TEST(0 == event.waiting) ;

   // TEST syncevent_INIT
   event = (syncevent_t) syncevent_INIT(&waiting) ;
   TEST(event.waiting == &waiting) ;
   event = (syncevent_t) syncevent_INIT(0) ;
   TEST(event.waiting == 0) ;

   // TEST initmove_syncevent
   waiting.event = &event ;
   event.waiting = &waiting ;
   syncevent_t event2 = syncevent_INIT_FREEABLE ;
   initmove_syncevent(&event2, &event) ;
   TEST(&waiting == event.waiting) ;
   TEST(&waiting == event2.waiting) ;
   // changed link in waiting
   TEST(&event2 == waiting.event) ;
   event.waiting = 0 ;
   initmove_syncevent(&event, &event2) ;
   TEST(&waiting == event.waiting) ;
   TEST(&waiting == event2.waiting) ;
   // changed link in waiting
   TEST(&event == waiting.event) ;

   // TEST initmovesafe_syncevent
   waiting.event  = &event ;
   event.waiting  = &waiting ;
   event2.waiting = 0 ;
   initmovesafe_syncevent(&event2, &event) ;
   TEST(&waiting == event.waiting) ;
   TEST(&waiting == event2.waiting) ;
   // changed link in waiting
   TEST(&event2 == waiting.event) ;
   event.waiting = 0 ;
   initmovesafe_syncevent(&event, &event2) ;
   TEST(&waiting == event.waiting) ;
   TEST(&waiting == event2.waiting) ;
   // changed link in waiting
   TEST(&event == waiting.event) ;

   // TEST initmovesafe_syncevent: src is free
   waiting.event  = &event ;
   event.waiting  = &waiting ;
   event2.waiting = 0 ;
   initmovesafe_syncevent(&event, &event2) ;
   TEST(0 == event.waiting) ;
   TEST(0 == event2.waiting) ;
   // not changed
   TEST(&event == waiting.event) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_query_event(void)
{
   syncevent_t    event = syncevent_INIT_FREEABLE ;
   syncwait_t     waiting ;

   // TEST isfree_syncevent
   memset(&event, 255 ,sizeof(event)) ;
   TEST(0 == isfree_syncevent(&event)) ;
   event.waiting = (void*)0 ;
   TEST(1 == isfree_syncevent(&event)) ;

   // TEST iswaiting_syncevent
   event.waiting = (void*)1 ;
   TEST(1 == iswaiting_syncevent(&event)) ;
   event.waiting = (void*)0 ;
   TEST(0 == iswaiting_syncevent(&event)) ;

   // TEST waiting_syncevent
   event.waiting = (void*)999 ;
   TEST(waiting_syncevent(&event) == (void*)999) ;
   event.waiting = &waiting ;
   TEST(waiting_syncevent(&event) == &waiting) ;
   event.waiting = (void*)0 ;
   TEST(waiting_syncevent(&event) == 0) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int dummy_mainfct(syncthread_t * thread, uint32_t signalstate)
{
   return (int)state_syncthread(thread) + (int)signalstate ;
}

static int test_initfree(void)
{
   syncwait_t     waiting = syncwait_INIT_FREEABLE ;
   syncevent_t    event   = syncevent_INIT_FREEABLE ;
   syncthread_t   thread  = syncthread_INIT(&dummy_mainfct, (void*)20) ;

   // TEST syncwait_INIT_FREEABLE
   TEST(1 == isfree_syncthread(&waiting.thread)) ;
   TEST(0 == waiting.event) ;
   TEST(0 == waiting.continuelabel) ;

   // TEST init_syncwait
   event.waiting = 0 ;
   init_syncwait(&waiting, &thread, &event, (void*)9) ;
   TEST(waiting.thread.mainfct == thread.mainfct) ;
   TEST(waiting.thread.state   == thread.state) ;
   TEST(waiting.event          == &event) ;
   TEST(waiting.continuelabel  == (void*)9) ;
   // event.waiting is set
   TEST(event.waiting          == &waiting) ;

   // TEST initmove_syncwait
   syncwait_t  waiting2 = syncwait_INIT_FREEABLE ;
   syncwait_t  oldwait ;
   memcpy(&oldwait, &waiting, sizeof(waiting)) ;
   initmove_syncwait(&waiting2, &waiting) ;
   TEST(0 == memcmp(&oldwait, &waiting, sizeof(waiting))) ;
   TEST(waiting2.thread.mainfct == thread.mainfct) ;
   TEST(waiting2.thread.state   == thread.state) ;
   TEST(waiting2.event          == &event) ;
   TEST(waiting2.continuelabel  == (void*)9) ;
   // event.waiting is set
   TEST(event.waiting           == &waiting2) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_query(void)
{
   syncwait_t  waiting  = syncwait_INIT_FREEABLE ;
   syncwait_t  waiting2 = syncwait_INIT_FREEABLE ;

   // TEST thread_syncwait
   TEST(&waiting.thread  == thread_syncwait(&waiting)) ;
   TEST(&waiting2.thread == thread_syncwait(&waiting2)) ;

   // TEST event_syncwait
   TEST(0 == event_syncwait(&waiting)) ;
   TEST(0 == event_syncwait(&waiting2)) ;
   waiting.event  = (void*)1 ;
   waiting2.event = (void*)4 ;
   TEST(1 == (uintptr_t)event_syncwait(&waiting)) ;
   TEST(4 == (uintptr_t)event_syncwait(&waiting2)) ;

   // TEST continuelabel_syncwait
   TEST(0 == continuelabel_syncwait(&waiting)) ;
   TEST(0 == continuelabel_syncwait(&waiting2)) ;
   waiting.continuelabel  = (void*)2 ;
   waiting2.continuelabel = (void*)3 ;
   TEST(2 == (uintptr_t)continuelabel_syncwait(&waiting)) ;
   TEST(3 == (uintptr_t)continuelabel_syncwait(&waiting2)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_update(void)
{
   syncthread_t   thread = syncthread_INIT_FREEABLE ;
   syncwait_t     waiting = syncwait_INIT_FREEABLE ;
   syncevent_t    event1  = syncevent_INIT_FREEABLE ;
   syncevent_t    event2  = syncevent_INIT_FREEABLE ;

   // TEST update_syncwait
   init_syncwait(&waiting, &thread, &event1, (void*)8) ;
   update_syncwait(&waiting, &event2, (void*)9) ;
   TEST(event2.waiting        == &waiting) ;
   TEST(waiting.event         == &event2) ;
   TEST(waiting.continuelabel == (void*)9) ;
   event1.waiting = 0 ;
   update_syncwait(&waiting, &event1, (void*)11) ;
   TEST(event1.waiting        == &waiting) ;
   TEST(waiting.event         == &event1) ;
   TEST(waiting.continuelabel == (void*)11) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_task_syncwait()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree_event()) goto ONABORT ;
   if (test_query_event())    goto ONABORT ;
   if (test_initfree())       goto ONABORT ;
   if (test_query())          goto ONABORT ;
   if (test_update())         goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
