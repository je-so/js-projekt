/* title: IOPipe impl

   Implements <IOPipe>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/io/pipe.h
    Header file <IOPipe>.

   file: C-kern/platform/Linux/io/pipe.c
    Implementation file <IOPipe impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/io/pipe.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/time/systimer.h"
#include "C-kern/api/time/timevalue.h"
#endif


// section: pipe_t

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_pipe_errtimer
 * Simuliert Fehler in <free_pipe> und anderen Funktionen. */
static test_errortimer_t s_pipe_errtimer = test_errortimer_FREE;
#endif

// group: lifetime

int init_pipe(/*out*/pipe_t* pipe)
{
   int err;

   static_assert(1 + &pipe->read == &pipe->write, "Arrayzugriff möglich");

   if (/*Fehler?*/ pipe2(&pipe->read, O_NONBLOCK|O_CLOEXEC)) {
      err = errno;
      goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int free_pipe(pipe_t* pipe)
{
   int err = 0;

   if (! isfree_iochannel(pipe->read)) {
      if (close(pipe->read)) {
         err = errno;
      }
      SETONERROR_testerrortimer(&s_pipe_errtimer, &err);
      pipe->read = sys_iochannel_FREE;
   }

   if (! isfree_iochannel(pipe->write)) {
      if (close(pipe->write)) {
         err = errno;
      }
      SETONERROR_testerrortimer(&s_pipe_errtimer, &err);
      pipe->write = sys_iochannel_FREE;
   }

   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: read-write

int readall_pipe(pipe_t* pipe, size_t size, void* data/*uint8_t[size]*/, int32_t msec_timeout/*<0: infinite timeout*/)
{
   int err;
   size_t bytes = 0;

   for (;;) {
      int part = read(pipe->read, data, size-bytes);

      if (part > 0) {
         bytes += (unsigned) part;
         if (bytes == size) break/*DONE*/;
         continue;
      }

      if (part < 0) {
         err = errno;
         if (err == EAGAIN && msec_timeout) {
            // wait msec_timeout milliseconds (if (msec_timeout < 0) "inifinite timeout")
            struct pollfd pfd = { .events = POLLIN, .fd = pipe->read };
            err = poll(&pfd, 1, msec_timeout);
            if (err > 0) continue;
            err = (err < 0) ? errno : ETIMEDOUT;
         }

      } else { // end-of-input
         err = EPIPE;
      }

      // error => discard read bytes
      goto ONERR;
   }

/*DONE*/
   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int writeall_pipe(pipe_t* pipe, size_t size, void* data/*uint8_t[size]*/, int32_t msec_timeout/*<0: infinite timeout*/)
{
   int err;
   size_t bytes = 0;

   for (;;) {
      int part = write(pipe->write, data, size-bytes);

      if (part > 0) {
         bytes += (unsigned) part;
         if (bytes == size) break/*DONE*/;
         continue;
      }

      if (part < 0) {
         err = errno;
         if (err == EAGAIN && msec_timeout) {
            // wait msec_timeout milliseconds (if (msec_timeout < 0) "inifinite timeout")
            struct pollfd pfd = { .events = POLLOUT, .fd = pipe->write };
            err = poll(&pfd, 1, msec_timeout);
            if (err > 0) continue;
            err = (err < 0) ? errno : ETIMEDOUT;
         }

      } else { // end-of-input
         err = EPIPE;
      }

      // error => undo of already written data is not possible
      goto ONERR;
   }

/*DONE*/
   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   pipe_t pipe = pipe_FREE;
   int    flags;
   uint8_t data;

   // TEST pipe_FREE
   TEST(isfree_iochannel(pipe.read));
   TEST(isfree_iochannel(pipe.write));

   // TEST init_pipe
   TEST(0 == init_pipe(&pipe));
   for (int i = 0; i <= 1; ++i) {
      // check close-on-exec
      TEST(! isfree_iochannel((&pipe.read)[i]));
      flags = fcntl((&pipe.read)[i], F_GETFD);
      TEST(-1 != flags);
      TEST(FD_CLOEXEC == flags);
      // check non blocking mode && (readonly || writeonly)
      flags = fcntl((&pipe.read)[i], F_GETFL);
      TEST(-1 != flags);
      TEST((O_NONBLOCK|(i ? O_WRONLY:O_RDONLY)) == flags);
   }
   // check writing end is connected to reading end
   data = 9;
   TEST(1 == write(pipe.write, &data, 1));
   data = 0;
   TEST(1 == read(pipe.read, &data, 1));
   TEST(9 == data)

   // TEST free_pipe
   TEST(0 == free_pipe(&pipe));
   // check
   TEST(isfree_iochannel(pipe.read));
   TEST(isfree_iochannel(pipe.write));

   // TEST free_pipe: double free
   TEST(0 == free_pipe(&pipe));
   // check
   TEST(isfree_iochannel(pipe.read));
   TEST(isfree_iochannel(pipe.write));

   // TEST free_pipe: partial close
   for (int i = 0; i <= 1; ++i) {
      // prepare partial close
      TEST(0 == init_pipe(&pipe));
      TEST(0 == close((&pipe.read)[i]));
      (&pipe.read)[i] = sys_iochannel_FREE;
      // test
      TEST(0 == free_pipe(&pipe));
      // check
      TEST(isfree_iochannel(pipe.read));
      TEST(isfree_iochannel(pipe.write));
   }

   // TEST free_pipe: simluated ERROR
   for (int i = 1; i <= 2; ++i) {
      // prepare
      TEST(0 == init_pipe(&pipe));
      // test
      init_testerrortimer(&s_pipe_errtimer, (unsigned)i, i);
      TEST(i == free_pipe(&pipe));
      // check
      TEST(isfree_iochannel(pipe.read));
      TEST(isfree_iochannel(pipe.write));
   }

   return 0;
ONERR:
   free_pipe(&pipe);
   return EINVAL;
}

static int fill_internal_buffer(pipe_t* pipe, size_t max, /*out*/size_t* bufsize)
{
   uint8_t buffer[1024];
   size_t  bytes = 0;

   if (!max) {
      for (int b; 0 < (b = write(pipe->write, buffer, sizeof(buffer)));) {
         bytes += (unsigned) b;
      }
      TEST(-1 == write(pipe->write, buffer, 1));
      TEST(errno == EAGAIN);
   } else {
      for (int b; bytes != max && 0 < (b = write(pipe->write, buffer, max-bytes > sizeof(buffer) ? sizeof(buffer):max-bytes));) {
         bytes += (unsigned) b;
      }
   }

   // set out param
   if (bufsize) *bufsize = bytes;

   return 0;
ONERR:
   return EINVAL;
}

static int test_readwrite(void)
{
   pipe_t     pipe  = pipe_FREE;
   systimer_t timer = systimer_FREE;
   uint8_t    buffer[16384];
   uint64_t   expcount;
   size_t     bufsize;

   // TEST readall_pipe: EBADF (pipe==pipe_FREE)
   TEST(EBADF == readall_pipe(&pipe, 1, buffer, 0));

   // TEST writeall_pipe: EBADF (pipe==pipe_FREE)
   TEST(EBADF == writeall_pipe(&pipe, 1, buffer, 0));

   // prepare
   TEST(0 == init_pipe(&pipe));
   TEST(0 == init_systimer(&timer, sysclock_MONOTONIC));

   // TEST writeall_pipe: store data in pipe
   for (size_t i = 0; i < sizeof(buffer); ++i) {
      buffer[i] = (uint8_t) (11*i);
   }
   TEST(0 == writeall_pipe(&pipe, sizeof(buffer), buffer, 0));

   // TEST readall_pipe: read stored data
   // invalidate result
   memset(buffer, 0, sizeof(buffer));
   // test
   TEST(0 == readall_pipe(&pipe, sizeof(buffer), buffer, 0));
   // check result
   for (size_t i = 0; i < sizeof(buffer); ++i) {
      TEST(buffer[i] == (uint8_t) (11*i));
   }

   // TEST readall_pipe: EAGAIN (no timeout)
   TEST(EAGAIN == readall_pipe(&pipe, 1, buffer, 0/*no timeout*/));

   // TEST readall_pipe: ETIMEDOUT
   TEST(0 == startinterval_systimer(timer, &(timevalue_t){ .nanosec = 10000 }));
   TEST(ETIMEDOUT == readall_pipe(&pipe, 1, buffer, 5));
   TEST(0 == expirationcount_systimer(timer, &expcount));
   TEST(450 <= expcount);
   TEST(750 >= expcount);

   // TEST writeall_pipe: EAGAIN (no timeout)
   TEST(0 == fill_internal_buffer(&pipe, 0, &bufsize));
   TEST(EAGAIN == writeall_pipe(&pipe, 1, buffer, 0/*no timeout*/));

   // TEST writeall_pipe: ETIMEDOUT
   TEST(0 == startinterval_systimer(timer, &(timevalue_t){ .nanosec = 10000 }));
   TEST(ETIMEDOUT == writeall_pipe(&pipe, 1, buffer, 5));
   TEST(0 == expirationcount_systimer(timer, &expcount));
   TEST(450 <= expcount);
   TEST(750 >= expcount);

   // clear buffer
   TEST(0 == free_pipe(&pipe));
   TEST(0 == init_pipe(&pipe));

   // TEST readall_pipe: partially read data skipped
   TEST(1 == write(pipe.write, buffer, 1));
   TEST(EAGAIN == readall_pipe(&pipe, 2, buffer, 0));

   // TEST writeall_pipe: partially written data
   TEST(0 == fill_internal_buffer(&pipe, bufsize-sizeof(buffer)/2, 0));
   TEST(EAGAIN == writeall_pipe(&pipe, sizeof(buffer), buffer, 0));

   // reset
   TEST(0 == free_pipe(&pipe));
   TEST(0 == free_systimer(&timer));

   return 0;
ONERR:
   free_pipe(&pipe);
   free_systimer(&timer);
   return EINVAL;
}


static int test_generic(void)
{
   pipe_t      pipe;
   iochannel_t ioc[2];

   // TEST cast_pipe: pipe_t* -> pipe_t*
   TEST(&pipe == cast_pipe(&pipe.read, &pipe.write));

   // TEST cast_pipe: array -> pipe_t*
   TEST((pipe_t*)ioc == cast_pipe(&ioc[0], &ioc[1]));

   return 0;
ONERR:
   return EINVAL;
}

int unittest_io_pipe()
{
   if (test_initfree())    goto ONERR;
   if (test_readwrite())   goto ONERR;
   if (test_generic())     goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
