/* title: Threadpool impl
   Implements interface for managing a number of threads.

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

   file: C-kern/api/os/task/threadpool.h
    Header file of <Threadpool>.

   file: C-kern/os/shared/task/threadpool.c
    Implementation file of <Threadpool impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/os/task/threadpool.h"
#include "C-kern/api/err.h"
#include "C-kern/api/os/thread.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


// section: threapool

// group: helper

static int threadmain_threadpool(threadpool_t * pool)
{
   int err ;

   for(;;) {

      err = wait_waitlist(&pool->idle) ;
      assert(!err) ;

      void * const command = command_osthread(self_osthread()) ;

      if (!command) {
         slock_mutex(&pool->idle.lock) ;
         -- pool->poolsize ;
         sunlock_mutex(&pool->idle.lock) ;
         break ;
      }

      assert(0 && "not implemented") ;

   }

   return 0 ;
}

// group: implementation

int init_threadpool(/*out*/threadpool_t * pool, uint8_t nr_of_threads)
{
   int err ;

   pool->idle     = (waitlist_t) waitlist_INIT_FREEABLE ;
   pool->poolsize = nr_of_threads ;
   pool->threads  = 0 ;

   err = init_waitlist(&pool->idle) ;
   if (err) goto ABBRUCH ;

   err = newgroup_osthread(&pool->threads, &threadmain_threadpool, pool, nr_of_threads) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   pool->poolsize = 0 ;
   (void) free_waitlist(&pool->idle) ;
   LOG_ABORT(err) ;
   return err ;
}

int free_threadpool(threadpool_t * pool)
{
   int err ;
   int err2 ;

   if (pool->poolsize) {

      size_t poolsize ;

      do {
         err = trywakeup_waitlist(&pool->idle, 0) ;
         assert(0 == err || EAGAIN == err) ;
         if (err == EAGAIN) {
            sleepms_osthread(10) ;
         }
         slock_mutex(&pool->idle.lock) ;
         poolsize = pool->poolsize ;
         sunlock_mutex(&pool->idle.lock) ;
      } while(poolsize) ;

      err = delete_osthread(&pool->threads) ;

      err2 = free_waitlist(&pool->idle) ;
      if (err2) err = err2 ;

      if (err) goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}


#ifdef KONFIG_UNITTEST

#define TEST(CONDITION) TEST_ONERROR_GOTO(CONDITION,unittest_os_task_threadpool,ABBRUCH)

static int test_initfree(void)
{
   threadpool_t   pool = threadpool_INIT_FREEABLE ;

   // TEST static initializer
   TEST(0 == pool.idle.last) ;
   TEST(0 == pool.idle.nr_waiting) ;
   TEST(0 == pool.poolsize) ;
   TEST(0 == pool.threads) ;

   // TEST init, double free
   TEST(0 == init_threadpool(&pool, 8)) ;
   TEST(8 == pool.poolsize) ;
   TEST(0 != pool.threads) ;
   for(int i = 0; i < 100000; ++i) {
      if (8 == pool.idle.nr_waiting) break ;
      sleepms_osthread(1) ;
   }
   TEST(0 != pool.idle.last) ;
   TEST(8 == pool.idle.nr_waiting) ;
   TEST(0 == free_threadpool(&pool)) ;
   TEST(0 == pool.idle.last) ;
   TEST(0 == pool.idle.nr_waiting) ;
   TEST(0 == pool.poolsize) ;
   TEST(0 == pool.threads) ;
   TEST(0 == free_threadpool(&pool)) ;
   TEST(0 == pool.idle.last) ;
   TEST(0 == pool.idle.nr_waiting) ;
   TEST(0 == pool.poolsize) ;
   TEST(0 == pool.threads) ;

   // TEST free waits until all threads started (registerd with pool)
   TEST(0 == init_threadpool(&pool, 3)) ;
   TEST(3 == pool.poolsize) ;
   TEST(0 != pool.threads) ;
   TEST(3 > pool.idle.nr_waiting) ;
   TEST(0 == free_threadpool(&pool)) ;
   TEST(0 == pool.idle.last) ;
   TEST(0 == pool.idle.nr_waiting) ;
   TEST(0 == pool.poolsize) ;
   TEST(0 == pool.threads) ;

   return 0 ;
ABBRUCH:
   (void) free_threadpool(&pool) ;
   return EINVAL ;
}

int unittest_os_task_threadpool()
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