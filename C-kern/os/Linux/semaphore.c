/* title: Semaphore Linux
   Implements <Semaphore>.

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

   file: C-kern/api/os/sync/semaphore.h
    Header file of <Semaphore>.

   file: C-kern/os/Linux/semaphore.c
    Linux implementation file <Semaphore Linux>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/os/sync/semaphore.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif

int init_semaphore(/*out*/semaphore_t * semaobj, uint16_t init_signal_count)
{
   int err ;
   int fd ;

   fd = eventfd(init_signal_count,EFD_CLOEXEC|EFD_SEMAPHORE) ;
   if (-1 == fd) {
      err = errno ;
      LOG_SYSERR("eventfd", err) ;
      LOG_UINT32(init_signal_count) ;
      goto ABBRUCH ;
   }

   static_assert(sizeof(fd) == sizeof(semaobj), "init all fields of struct") ;
   semaobj->sys_sema = fd ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int free_semaphore(semaphore_t * semaobj)
{
   int err = 0 ;
   int err2 ;

   if (sys_semaphore_INIT_FREEABLE != semaobj->sys_sema) {
      // wake up any waiters
      int flags = fcntl(semaobj->sys_sema, F_GETFL) ;
      flags |= O_NONBLOCK ;
      fcntl(semaobj->sys_sema, F_SETFL, (long)flags) ;
      for(uint64_t increment = 0xffff; increment; increment <<= 16) {
         err2 = write(semaobj->sys_sema, &increment, sizeof(increment)) ;
         if (-1 == err2) {
            if (EAGAIN != errno) {
               err = errno ;
               LOG_SYSERR("write", err) ;
               LOG_INT(semaobj->sys_sema) ;
               break ;
            }
         }
      }
      // free resource
      err2 = close(semaobj->sys_sema) ;
      if (err2) {
         err = errno ;
         LOG_SYSERR("close", err) ;
         LOG_INT(semaobj->sys_sema) ;
      }

      semaobj->sys_sema = sys_semaphore_INIT_FREEABLE ;
      if (err) goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}

int signal_semaphore(semaphore_t * semaobj, uint32_t signal_count)
{
   int err ;
   uint64_t increment = signal_count ;

   err = write(semaobj->sys_sema, &increment, sizeof(increment)) ;
   if (-1 == err) {
      err = errno ;
      LOG_SYSERR("write", err) ;
      LOG_INT(semaobj->sys_sema) ;
      LOG_UINT32(signal_count) ;
      goto ABBRUCH ;
   }

   assert(sizeof(uint64_t) == err) ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int wait_semaphore(semaphore_t * semaobj)
{
   int err ;
   uint64_t decrement = 0 ;

   err = read(semaobj->sys_sema, &decrement, sizeof(decrement)) ;
   if (-1 == err) {
      err = errno ;
      LOG_SYSERR("read", err) ;
      LOG_INT(semaobj->sys_sema) ;
      goto ABBRUCH ;
   }

   assert(1 == decrement) ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}


// section: test

#ifdef KONFIG_UNITTEST

#define TEST(ARG) TEST_ONERROR_GOTO(ARG,unittest_os_sync_semaphore,ABBRUCH)

static int test_semaphore_init(void)
{
   semaphore_t    sema = semaphore_INIT_FREEABLE ;

   // TEST static init
   TEST(sema.sys_sema == sys_semaphore_INIT_FREEABLE) ;

   // TEST init, double free
   TEST(0 == init_semaphore(&sema, 2)) ;
   TEST(sema.sys_sema != sys_semaphore_INIT_FREEABLE) ;
   TEST(0 == free_semaphore(&sema)) ;
   TEST(sema.sys_sema == sys_semaphore_INIT_FREEABLE) ;
   TEST(0 == free_semaphore(&sema)) ;
   TEST(sema.sys_sema == sys_semaphore_INIT_FREEABLE) ;

   // TEST init, wait
   TEST(0 == init_semaphore(&sema, 13)) ;
   TEST(sema.sys_sema != sys_semaphore_INIT_FREEABLE) ;
   for(int i = 0; i < 13; ++i) {
      TEST(0 == wait_semaphore(&sema)) ;
   }
   {
      int flags = fcntl(sema.sys_sema, F_GETFL) ;
      flags |= O_NONBLOCK ;
      TEST(0 == fcntl(sema.sys_sema, F_SETFL, (long)flags)) ;
   }
   TEST(EAGAIN == wait_semaphore(&sema)) ;
   TEST(0 == free_semaphore(&sema)) ;
   TEST(sema.sys_sema == sys_semaphore_INIT_FREEABLE) ;

   // TEST signal, wait
   TEST(0 == init_semaphore(&sema, 0)) ;
   for(int i = 0; i < 13; ++i) {
      TEST(0 == signal_semaphore(&sema, 1)) ;
      TEST(0 == wait_semaphore(&sema)) ;
   }
   {
      int flags = fcntl(sema.sys_sema, F_GETFL) ;
      flags |= O_NONBLOCK ;
      TEST(0 == fcntl(sema.sys_sema, F_SETFL, (long)flags)) ;
   }
   TEST(EAGAIN == wait_semaphore(&sema)) ;
   {
      int flags = fcntl(sema.sys_sema, F_GETFL) ;
      flags &= ~(O_NONBLOCK) ;
      TEST(0 == fcntl(sema.sys_sema, F_SETFL, (long)flags)) ;
   }
   for(int i = 0; i < 3; ++i) {
      TEST(0 == signal_semaphore(&sema, 3)) ;
   }
   for(int i = 0; i < 9; ++i) {
      TEST(0 == wait_semaphore(&sema)) ;
   }
   {
      int flags = fcntl(sema.sys_sema, F_GETFL) ;
      flags |= O_NONBLOCK ;
      TEST(0 == fcntl(sema.sys_sema, F_SETFL, (long)flags)) ;
   }
   TEST(EAGAIN == wait_semaphore(&sema)) ;
   TEST(0 == free_semaphore(&sema)) ;

   return 0 ;
ABBRUCH:
   free_semaphore(&sema) ;
   return EINVAL ;
}

typedef struct semathread_arg_t  semathread_arg_t ;
struct semathread_arg_t {
   pthread_mutex_t   mutex ;
   semaphore_t       sema ;
   volatile unsigned count ;
} ;

static void * semathread(void * start_arg)
{
   semathread_arg_t * startarg = (semathread_arg_t*)start_arg ;

   if (pthread_mutex_lock(&startarg->mutex))      goto ABBRUCH ;
   ++ startarg->count ;
   if (pthread_mutex_unlock(&startarg->mutex))    goto ABBRUCH ;

   if (wait_semaphore(&startarg->sema)) goto ABBRUCH ;

   if (pthread_mutex_lock(&startarg->mutex))      goto ABBRUCH ;
   -- startarg->count ;
   if (pthread_mutex_unlock(&startarg->mutex))    goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   return (void*) 1 ;
}

static int test_semaphore_threads(void)
{
   bool              isMutex = false ;
   semathread_arg_t  startarg = { .sema = semaphore_INIT_FREEABLE } ;
   unsigned          valid_thread_index = 0 ;
   pthread_t         threads[100];

   TEST(0 == init_semaphore(&startarg.sema, 0)) ;
   TEST(0 == pthread_mutex_init(&startarg.mutex, 0)) ;
   isMutex = true ;

   // start up threads
   for(unsigned i = 0; i < nrelementsof(threads); ++i) {
      TEST(0 == pthread_create(&threads[i], 0, semathread, &startarg)) ;
      valid_thread_index = 1 + i ;
   }

   for(int w = 0; valid_thread_index != startarg.count; ++w) {
      pthread_yield() ;
      TEST(w < 100000) ;
   }
   TEST(valid_thread_index == startarg.count) ;

   // TEST singalling 1 thread runs exactly one thread
   for(unsigned i = 0; i < nrelementsof(threads)/2; ++i) {
      TEST(0 == signal_semaphore(&startarg.sema, 1)) ;
      for(int w = 0; valid_thread_index != (1 + i + startarg.count); ++w) {
         pthread_yield() ;
         TEST(w < 100000) ;
      }
      TEST(valid_thread_index == (1 + i + startarg.count)) ;
   }

   // TEST singalling many threads runs exactly that many threads
   TEST(0 == signal_semaphore(&startarg.sema, nrelementsof(threads) - nrelementsof(threads)/2)) ;
   for(int w = 0; 0 != startarg.count; ++w) {
      pthread_yield() ;
      TEST(w < 100000) ;
   }
   TEST(0 == startarg.count) ;

   for(unsigned i = nrelementsof(threads)-1; i < nrelementsof(threads); --i) {
      void * result = (void*) 1 ;
      TEST(0 == pthread_join(threads[i], &result)) ;
      valid_thread_index = i ;
      TEST(0 == result) ;
   }

   // start up threads
   for(unsigned i = 0; i < nrelementsof(threads); ++i) {
      TEST(0 == pthread_create(&threads[i], 0, semathread, &startarg)) ;
      valid_thread_index = 1 + i ;
   }

   for(int w = 0; valid_thread_index != startarg.count; ++w) {
      pthread_yield() ;
      TEST(w < 100000) ;
   }
   TEST(valid_thread_index == startarg.count) ;

   // TEST free *signals* all waiting threads
   TEST(0 == free_semaphore(&startarg.sema)) ;
   for(int w = 0; 0 != startarg.count; ++w) {
      pthread_yield() ;
      TEST(w < 100000) ;
   }
   TEST(0 == startarg.count) ;

   for(unsigned i = nrelementsof(threads)-1; i < nrelementsof(threads); --i) {
      void * result = (void*) 1 ;
      TEST(0 == pthread_join(threads[i], &result)) ;
      valid_thread_index = i ;
      TEST(0 == result) ;
   }

   isMutex = false ;
   TEST(0 == pthread_mutex_destroy(&startarg.mutex)) ;
   TEST(0 == free_semaphore(&startarg.sema)) ;

   return 0 ;
ABBRUCH:
   free_semaphore(&startarg.sema) ; // also waking up waiting threads !
   while(valid_thread_index) {
      --valid_thread_index ;
      pthread_join(threads[valid_thread_index], 0) ;
   }
   if (isMutex) {
      pthread_mutex_destroy(&startarg.mutex) ;
   }
   return EINVAL ;
}

static int test_overflow(void)
{
   sys_semaphore_t   sema = sys_semaphore_INIT_FREEABLE ;
   int               size ;
   uint64_t          value ;

   // TEST value overflow => EAGAIN value has not changed
   sema = eventfd(0,EFD_CLOEXEC|EFD_NONBLOCK) ;
   TEST(-1 != sema) ;
   value = 0x0fffffffffffffff ;
   size = write(sema, &value, sizeof(value)) ;
   TEST(sizeof(uint64_t) == size) ;
   value = 0xf000000000000000 ;
   size = write(sema, &value, sizeof(value)) ;
   TEST(-1 == size) ;
   TEST(EAGAIN == errno) ;
   size = read(sema, &value, sizeof(value)) ;
   TEST(sizeof(uint64_t) == size) ;
   TEST(0x0fffffffffffffff == value) ;
   TEST(0 == close(sema)) ;
   sema = sys_semaphore_INIT_FREEABLE ;

   return 0 ;
ABBRUCH:
   if (sys_semaphore_INIT_FREEABLE != sema) {
      close(sema) ;
   }
   return EINVAL ;
}

int unittest_os_sync_semaphore()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   // allocate possible additional (internal) malloc memory !
   if (test_semaphore_threads())       goto ABBRUCH ;

   // store current mapping
   TEST(0 == init_resourceusage(&usage)) ;

   if (test_overflow())                goto ABBRUCH ;
   if (test_semaphore_init())          goto ABBRUCH ;
   if (test_semaphore_threads())       goto ABBRUCH ;

   // TEST mapping has not changed
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
