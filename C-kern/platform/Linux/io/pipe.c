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
#include "C-kern/api/platform/task/thread.h"
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
      int part = read(pipe->read, (uint8_t*)data + bytes, size-bytes);

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
            err = (err < 0) ? errno : ETIME;
         }

      } else { // end-of-input (not logged)
         return EPIPE;
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

int writeall_pipe(pipe_t* pipe, size_t size, const void* data/*uint8_t[size]*/, int32_t msec_timeout/*<0: infinite timeout*/)
{
   int err;
   size_t bytes = 0;

   for (;;) {
      int part = write(pipe->write, (const uint8_t*)data + bytes, size-bytes);

      if (part >= 0) {
         bytes += (unsigned) part;
         if (bytes == size) break/*DONE*/;
         continue;
      }

      err = errno;
      if (err == EAGAIN && msec_timeout) {
         // wait msec_timeout milliseconds (if (msec_timeout < 0) "inifinite timeout")
         struct pollfd pfd = { .events = POLLOUT, .fd = pipe->write };
         err = poll(&pfd, 1, msec_timeout);
         if (err > 0) continue;
         err = (err < 0) ? errno : ETIME;
      }

      // error => undo of already written data is not possible
      goto ONERR;
   }

/*DONE*/
   return 0;
ONERR:
   if (err != EPIPE) {
      TRACEEXIT_ERRLOG(err);
   }
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

static int determinte_buffer_size(pipe_t* pipe, /*out*/size_t* bufsize)
{
   uint8_t buffer[1024];
   size_t  bytes = 0;

   for (int b; 0 < (b = write(pipe->write, buffer, sizeof(buffer)));) {
      bytes += (unsigned) b;
   }
   TEST(-1 == write(pipe->write, buffer, 1));
   TEST(errno == EAGAIN);

   // set out param
   *bufsize = bytes;

   return 0;
ONERR:
   return EINVAL;
}

static int fill_buffer(pipe_t* pipe, size_t size)
{
   uint8_t buffer[1024];
   size_t  bytes = 0;

   for (int b; bytes != size && 0 < (b = write(pipe->write, buffer, size-bytes > sizeof(buffer) ? sizeof(buffer):size-bytes));) {
      bytes += (unsigned) b;
   }

   TEST(bytes == size);

   return 0;
ONERR:
   return EINVAL;
}

#define BUFFER_SIZE 16384

static int thread_waitingwrite(void * arg)
{
   pipe_t* pipe = arg;
   uint8_t buffer[BUFFER_SIZE];

   for (size_t i = 0; i < sizeof(buffer); ++i) {
      buffer[i] = (uint8_t) (i/13);
   }

   for (unsigned i = 0; i < 8; ++i) {
      TEST(sizeof(buffer)/8 == write(pipe->write, buffer + i*(sizeof(buffer)/8), sizeof(buffer)/8));
      sleepms_thread(5);
   }

   return 0;
ONERR:
   return EINVAL;
}

typedef struct writeall_param_t {
   pipe_t*   pipe;
   thread_t* wakeup;
} writeall_param_t;

static int thread_writeall(void * arg)
{
   writeall_param_t* param = arg;
   uint8_t buffer[BUFFER_SIZE];

   for (size_t i = 0; i < sizeof(buffer); ++i) {
      buffer[i] = (uint8_t) (i/11);
   }

   if (param->wakeup) {
      resume_thread(param->wakeup);
   }

   size_t   logsize1 = 0;
   size_t   logsize2 = 1;
   uint8_t* logbuffer;

   GETBUFFER_ERRLOG(&logbuffer, &logsize1);
   int err = writeall_pipe(param->pipe, sizeof(buffer), buffer, -1/*indefinite timeout*/);
   GETBUFFER_ERRLOG(&logbuffer, &logsize2);
   TEST(logsize1 == logsize2);

   return err;
ONERR:
   return EINVAL;
}

static int test_readwrite(void)
{
   pipe_t     pipe  = pipe_FREE;
   systimer_t timer = systimer_FREE;
   uint8_t    buffer[BUFFER_SIZE];
   uint64_t   expcount;
   size_t     bufsize;
   thread_t * thr = 0;
   size_t     logsize1 = 0;
   size_t     logsize2 = 1;
   uint8_t*   logbuffer;
   writeall_param_t waparam;

   // TEST readall_pipe: EBADF (pipe==pipe_FREE)
   TEST(EBADF == readall_pipe(&pipe, 1, buffer, 0));

   // TEST writeall_pipe: EBADF (pipe==pipe_FREE)
   TEST(EBADF == writeall_pipe(&pipe, 1, buffer, 0));

   // prepare
   TEST(0 == init_pipe(&pipe));
   TEST(0 == determinte_buffer_size(&pipe, &bufsize));
   TEST(0 == free_pipe(&pipe));
   TEST(0 == init_pipe(&pipe));
   TEST(0 == init_systimer(&timer, sysclock_MONOTONIC));

   // TEST writeall_pipe: store data in pipe
   for (size_t i = 0; i < sizeof(buffer); ++i) {
      buffer[i] = (uint8_t) (i/11);
   }
   TEST(0 == writeall_pipe(&pipe, sizeof(buffer), buffer, 0));

   // TEST readall_pipe: read stored data
   memset(buffer, 0, sizeof(buffer));
   TEST(0 == readall_pipe(&pipe, sizeof(buffer), buffer, 0));
   // check result
   for (size_t i = 0; i < sizeof(buffer); ++i) {
      TEST(buffer[i] == (uint8_t) (i/11));
   }

   // TEST readall_pipe: multiple reads with waiting
   // prepare
   TEST(0 == new_thread(&thr, &thread_waitingwrite, &pipe));
   // test
   memset(buffer, 0, sizeof(buffer));
   TEST(0 == readall_pipe(&pipe, sizeof(buffer), buffer, -1/*inifinite timeout*/));
   // check thread
   TEST(0 == join_thread(thr));
   TEST(0 == returncode_thread(thr));
   // check buffer content
   for (size_t i = 0; i < sizeof(buffer); ++i) {
      TESTP(buffer[i] == (uint8_t) (i/13), "buffer[%d] != %d", i, (int)(uint8_t)(i/13));
   }
   // reset
   TEST(0 == delete_thread(&thr));

   // TEST writeall_pipe: multiple writes with waiting
   // prepare
   TEST(0 == fill_buffer(&pipe, bufsize));
   waparam.pipe = &pipe;
   waparam.wakeup = 0;
   TEST(0 == new_thread(&thr, &thread_writeall, &waparam));
   // simulate slow reading (==> writing thread does wait multiple times)
   for (size_t i = bufsize; i; ) {
      sleepms_thread(5);
      size_t b = (i > sizeof(buffer)/8 ? sizeof(buffer)/8 : i);
      TEST(b == (size_t)read(pipe.read, buffer, b));
      i -= b;
   }
   // read written content
   memset(buffer, 0, sizeof(buffer));
   for (size_t i = 0; i < 8; ++i) {
      sleepms_thread(5);
      TEST(sizeof(buffer)/8 == read(pipe.read, buffer + i*(sizeof(buffer)/8), sizeof(buffer)/8));
   }
   TEST(-1 == read(pipe.read, buffer, 1));
   TEST(EAGAIN == errno);
   // check thread
   TEST(0 == join_thread(thr));
   TEST(0 == returncode_thread(thr));
   // check buffer content
   for (size_t i = 0; i < sizeof(buffer); ++i) {
      TESTP(buffer[i] == (uint8_t) (i/11), "buffer[%d] != %d", i, (int)(uint8_t)(i/11));
   }
   // reset
   TEST(0 == delete_thread(&thr));

   // TEST readall_pipe: EAGAIN (no timeout)
   TEST(EAGAIN == readall_pipe(&pipe, 1, buffer, 0/*no timeout*/));

   // TEST writeall_pipe: EAGAIN (no timeout)
   TEST(0 == fill_buffer(&pipe, bufsize));
   TEST(EAGAIN == writeall_pipe(&pipe, 1, buffer, 0/*no timeout*/));

   // clear buffer
   TEST(0 == free_pipe(&pipe));
   TEST(0 == init_pipe(&pipe));

   // TEST readall_pipe: ETIME
   TEST(0 == startinterval_systimer(timer, &(timevalue_t){ .nanosec = 10000 }));
   TEST(ETIME == readall_pipe(&pipe, 1, buffer, 5));
   TEST(0 == expirationcount_systimer(timer, &expcount));
   TEST(450 <= expcount);
   TEST(750 >= expcount);

   // TEST writeall_pipe: ETIME
   TEST(0 == fill_buffer(&pipe, bufsize));
   TEST(0 == startinterval_systimer(timer, &(timevalue_t){ .nanosec = 10000 }));
   TEST(ETIME == writeall_pipe(&pipe, 1, buffer, 5));
   TEST(0 == expirationcount_systimer(timer, &expcount));
   TEST(450 <= expcount);
   TEST(750 >= expcount);
   // reset
   TEST(0 == free_pipe(&pipe));
   TEST(0 == init_pipe(&pipe));

   // TEST readall_pipe: EPIPE (no error log)
   // prepare
   TEST(0 == free_iochannel(&pipe.write))
   GETBUFFER_ERRLOG(&logbuffer, &logsize1);
   // test
   TEST(EPIPE == readall_pipe(&pipe, 1, buffer, -1));
   // check
   GETBUFFER_ERRLOG(&logbuffer, &logsize2);
   TEST(logsize1 == logsize2/*no error log*/);
   // reset
   TEST(0 == free_pipe(&pipe));
   TEST(0 == init_pipe(&pipe));

   // TEST writeall_pipe: EPIPE (no error log)
   // prepare
   TEST(0 == free_iochannel(&pipe.read))
   // test
   TEST(EPIPE == writeall_pipe(&pipe, 1, buffer, -1));
   // check
   GETBUFFER_ERRLOG(&logbuffer, &logsize2);
   TEST(logsize1 == logsize2/*no error log*/);
   // reset
   TEST(0 == free_pipe(&pipe));
   TEST(0 == init_pipe(&pipe));

   // TEST writeall_pipe: EPIPE (other thread waits)
   // prepare
   TEST(0 == fill_buffer(&pipe, bufsize));
   waparam.pipe = &pipe;
   waparam.wakeup = self_thread();
   TEST(0 == new_thread(&thr, &thread_writeall, &waparam));
   suspend_thread();
   TEST(0 == free_iochannel(&pipe.read))
   // check thread
   TEST(0 == join_thread(thr));
   TEST(EPIPE == returncode_thread(thr));
   // reset
   TEST(0 == delete_thread(&thr));
   TEST(0 == free_pipe(&pipe));
   TEST(0 == init_pipe(&pipe));

   // TEST readall_pipe: partially read data skipped
   TEST(1 == write(pipe.write, buffer, 1));
   TEST(EAGAIN == readall_pipe(&pipe, 2, buffer, 0));

   // TEST writeall_pipe: partially written data
   TEST(0 == fill_buffer(&pipe, bufsize-sizeof(buffer)/2));
   TEST(EAGAIN == writeall_pipe(&pipe, sizeof(buffer), buffer, 0));

   // reset
   TEST(0 == free_pipe(&pipe));
   TEST(0 == free_systimer(&timer));

   return 0;
ONERR:
   delete_thread(&thr);
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
