/* title: IOPoll Linuximpl

   Implements <IOPoll>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2012 Jörg Seebohn

   file: C-kern/api/io/iopoll.h
    Header file <IOPoll>.

   file: C-kern/platform/Linux/io/iopoll.c
    Implementation file <IOPoll Linuximpl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/io/iopoll.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/io/ioevent.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/time/timevalue.h"
#include "C-kern/api/time/sysclock.h"
#endif


/* section: iopoll_t
 * Uses Linux epoll - I/O event notification facility.
 * See: man 4 epoll. */

// group: helper

/* function: convert2ioeventbits_iopoll
 * Converts Linux specific epoll_event->events into <ioevent_e>. */
static inline uint32_t convert2ioeventbits_iopoll(uint32_t events)
{
   int32_t ioevents  = (0 != (events & EPOLLIN))  * ioevent_READ
                     + (0 != (events & EPOLLOUT)) * ioevent_WRITE
                     + (0 != (events & EPOLLERR)) * ioevent_ERROR
                     + (0 != (events & EPOLLHUP)) * ioevent_CLOSE ;
   return (uint32_t)ioevents ;
}

/* function: convert2epolleventbits
 * Converts <ioevent_e> into Linux epoll event bits. */
static inline uint32_t convert2epolleventbits_iopoll(uint32_t ioevents)
{
   int32_t epollevents  = (0 != (ioevents&ioevent_READ))  * EPOLLIN
                        + (0 != (ioevents&ioevent_WRITE)) * EPOLLOUT ;
                        // ioevent_ERROR and ioevent_CLOSE are always selected they are not maskable !
   return (uint32_t) epollevents ;
}

/* function: convert2epollevent_iopoll
 * Converts <ioevent_t> into Linux struct epoll_event. */
static inline void convert2epollevent_iopoll(/*out*/struct epoll_event * epevent, const ioevent_t * ioevent)
{
   epevent->events = convert2epolleventbits_iopoll(ioevent->ioevents) ;
   if (sizeof(ioevent->eventid.val64) > sizeof(ioevent->eventid.ptr)) {
      epevent->data.u64 = ioevent->eventid.val64 ;
   } else {
      epevent->data.ptr = ioevent->eventid.ptr ;
   }
}

// group: lifetime

/* function: init_iopoll
 * Creates epoll event notification facility. */
int init_iopoll(/*out*/iopoll_t * iopoll)
{
   int err ;
   int efd = epoll_create1(EPOLL_CLOEXEC) ;

   if (-1 == efd) {
      err = errno ;
      TRACESYSCALL_ERRLOG("epoll_create1", err) ;
      goto ONERR;
   }

   iopoll->sys_poll = efd ;

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}

/* function: free_iopoll
 * Frees Linux epoll object. */
int free_iopoll(iopoll_t * iopoll)
{
   int err ;

   err = free_iochannel(&iopoll->sys_poll) ;
   if (err) goto ONERR;

   return 0 ;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err ;
}

// group: query

int wait_iopoll(iopoll_t * iopoll, /*out*/uint32_t * nr_events, uint32_t queuesize, /*out*/struct ioevent_t * eventqueue/*[queuesize]*/, uint16_t timeout_ms)
{
   int err ;
   int resultsize ;

   VALIDATE_INPARAM_TEST(0 < queuesize && queuesize < INT_MAX, ONERR, PRINTUINT32_ERRLOG(queuesize) );

   static_assert( sizeof(int) > sizeof(timeout_ms), "(int)timeout_ms is never -1");
   static_assert( sizeof(ioevent_t) == sizeof(struct epoll_event)
                  && sizeof(((ioevent_t*)0)->ioevents) == sizeof(((struct epoll_event*)0)->events)
                  && offsetof(ioevent_t, ioevents) == offsetof(struct epoll_event, events)
                  && sizeof(((ioevent_t*)0)->eventid) == sizeof(((struct epoll_event*)0)->data)
                  && offsetof(ioevent_t, eventid) == offsetof(struct epoll_event, data),
                  "struct epoll_event compatible with ioevent_t");

   resultsize = epoll_wait(iopoll->sys_poll, (struct epoll_event*)eventqueue, (int)queuesize, timeout_ms) ;
   if (resultsize < 0) {
      err = errno ;
      TRACESYSCALL_ERRLOG("epoll_wait", err) ;
      PRINTINT_ERRLOG(iopoll->sys_poll) ;
      goto ONERR;
   }

   for (int i = 0; i < resultsize; ++i) {
      eventqueue[i].ioevents = convert2ioeventbits_iopoll(eventqueue[i].ioevents) ;
   }

   *nr_events = (unsigned) resultsize ;

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}

// group: change

/* function: registerfd_iopoll
 * Adds file descriptor to Linux epoll object. */
int registerfd_iopoll(iopoll_t * iopoll, sys_iochannel_t fd, const struct ioevent_t * for_event)
{
   int err ;

   VALIDATE_INPARAM_TEST(0 == (for_event->ioevents & ~(uint32_t)ioevent_MASK), ONERR, PRINTUINT32_ERRLOG(for_event->ioevents)) ;

   struct epoll_event epevent ;
   convert2epollevent_iopoll(&epevent, for_event) ;

   err = epoll_ctl(iopoll->sys_poll, EPOLL_CTL_ADD, fd, &epevent) ;
   if (err) {
      err = errno ;
      TRACESYSCALL_ERRLOG("epoll_ctl(EPOLL_CTL_ADD)", err) ;
      PRINTINT_ERRLOG(iopoll->sys_poll) ;
      PRINTINT_ERRLOG(fd) ;
      goto ONERR;
   }

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}

/* function: updatefd_iopoll
 * Updates event mask/value of <sys_iochannel_t> registered at Linux epoll object. */
int updatefd_iopoll(iopoll_t * iopoll, sys_iochannel_t fd, const struct ioevent_t * updated_event)
{
   int err ;

   VALIDATE_INPARAM_TEST(0 == (updated_event->ioevents & ~(uint32_t)ioevent_MASK), ONERR, PRINTUINT32_ERRLOG(updated_event->ioevents)) ;

   struct epoll_event epevent ;
   convert2epollevent_iopoll(&epevent, updated_event) ;

   err = epoll_ctl(iopoll->sys_poll, EPOLL_CTL_MOD, fd, &epevent) ;
   if (err) {
      err = errno ;
      TRACESYSCALL_ERRLOG("epoll_ctl(EPOLL_CTL_MOD)", err) ;
      PRINTINT_ERRLOG(iopoll->sys_poll) ;
      PRINTINT_ERRLOG(fd) ;
      goto ONERR;
   }

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}

/* function: updatefd_iopoll
 * Unregisteres <sys_iochannel_t> at Linux epoll object. */
int unregisterfd_iopoll(iopoll_t * iopoll, sys_iochannel_t fd)
{
   int err ;
   struct epoll_event dummy ;

   err = epoll_ctl(iopoll->sys_poll, EPOLL_CTL_DEL, fd, &dummy) ;
   if (err) {
      err = errno ;
      TRACESYSCALL_ERRLOG("epoll_ctl(EPOLL_CTL_DEL)", err) ;
      PRINTINT_ERRLOG(iopoll->sys_poll) ;
      PRINTINT_ERRLOG(fd) ;
      goto ONERR;
   }

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_ioevent(void)
{
   ioevent_t   ioevent ;

   // TEST ioevent_INIT_PTR
   ioevent = (ioevent_t) ioevent_INIT_PTR(ioevent_EMPTY, &ioevent) ;
   TEST(ioevent.ioevents    == ioevent_EMPTY) ;
   TEST(ioevent.eventid.ptr == &ioevent) ;

   // TEST ioevent_INIT_VAL32
   ioevent = (ioevent_t) ioevent_INIT_VAL32(ioevent_READ|ioevent_WRITE, 0x8123abcf) ;
   TEST(ioevent.ioevents      == (ioevent_READ + ioevent_WRITE)) ;
   TEST(ioevent.eventid.val32 == 0x8123abcf) ;

   // TEST ioevent_INIT_VAL64
   ioevent = (ioevent_t) ioevent_INIT_VAL64(ioevent_READ|ioevent_WRITE|ioevent_ERROR|ioevent_CLOSE, 0x112233448123abcf) ;
   TEST(ioevent.ioevents      == (ioevent_READ + ioevent_WRITE + ioevent_ERROR + ioevent_CLOSE)) ;
   TEST(ioevent.eventid.val64 == 0x112233448123abcf) ;

   return 0 ;
ONERR:
   return EINVAL ;
}

static int test_initfree(void)
{
   iopoll_t  iopoll = iopoll_FREE ;

   // TEST iopoll_FREE
   TEST(iopoll.sys_poll == sys_iochannel_FREE) ;

   // TEST init_iopoll, free_iopoll
   TEST(0 == init_iopoll(&iopoll)) ;
   TEST(iopoll.sys_poll > 0) ;
   TEST(0 == free_iopoll(&iopoll)) ;
   TEST(iopoll.sys_poll == iochannel_FREE) ;
   TEST(0 == free_iopoll(&iopoll)) ;
   TEST(iopoll.sys_poll == iochannel_FREE) ;

   // TEST free_iopoll: removes regisestered fds
   TEST(0 == init_iopoll(&iopoll)) ;
   TEST(iopoll.sys_poll > 0) ;
   TEST(0 == registerfd_iopoll(&iopoll, iochannel_STDIN, &(ioevent_t)ioevent_INIT_VAL64(ioevent_READ, 1))) ;
   TEST(0 == registerfd_iopoll(&iopoll, iochannel_STDOUT, &(ioevent_t)ioevent_INIT_VAL64(ioevent_WRITE, 2))) ;
   TEST(0 == registerfd_iopoll(&iopoll, iochannel_STDERR, &(ioevent_t)ioevent_INIT_VAL64(ioevent_WRITE, 3))) ;
   TEST(0 == free_iopoll(&iopoll)) ;
   TEST(iopoll.sys_poll == sys_iochannel_FREE) ;

   return 0 ;
ONERR:
   return EINVAL ;
}

static int test_registerfd(void)
{
   iopoll_t    iopoll = iopoll_FREE ;
   uint32_t    nr_events ;
   int         fd[20][2] ;
   ioevent_t   ioevents[21] ;

   // prepare
   memset(fd, -1, sizeof(fd)) ;
   for (unsigned i = 0; i < lengthof(fd); ++ i) {
      TEST(0 == pipe2(fd[i], O_CLOEXEC)) ;
      TEST(3 == write(fd[i][1], "123", 3)) ;
   }
   TEST(0 == init_iopoll(&iopoll)) ;

   // TEST registerfd_iopoll
   for (unsigned i = 0; i < lengthof(fd); ++ i) {
      TEST(0 == registerfd_iopoll(&iopoll, fd[i][0], &(ioevent_t)ioevent_INIT_VAL64(ioevent_READ, 5*i))) ;
      // check that all registered fd generate an event
      memset(ioevents, 0, sizeof(ioevents)) ;
      TEST(0 == wait_iopoll(&iopoll, &nr_events, lengthof(ioevents), ioevents, 0)) ;
      TEST(nr_events == 1+i) ;
      for (unsigned i2 = 0; i2 < i+1; ++ i2) {
         TEST(ioevents[i2].ioevents      == ioevent_READ) ;
         TEST(ioevents[i2].eventid.val64 == 5*i2) ;
      }
   }

   // TEST updatefd_iopoll: change ioevent mask
   for (unsigned i = 0; i < lengthof(fd); ++ i) {
      TEST(0 == updatefd_iopoll(&iopoll, fd[i][0], &(ioevent_t)ioevent_INIT_VAL64(ioevent_WRITE, 5*i))) ;
   }
   TEST(0 == wait_iopoll(&iopoll, &nr_events, lengthof(ioevents), ioevents, 0)) ;
   TEST(0 == nr_events) ;

   // TEST updatefd_iopoll: change ioevent mask and eventid
   for (unsigned i = 0; i < lengthof(fd); ++ i) {
      TEST(0 == updatefd_iopoll(&iopoll, fd[i][0], &(ioevent_t)ioevent_INIT_VAL64(ioevent_READ, 5*i+1))) ;
   }
   memset(ioevents, 0, sizeof(ioevents)) ;
   TEST(0 == wait_iopoll(&iopoll, &nr_events, lengthof(ioevents), ioevents, 0)) ;
   TEST(nr_events == lengthof(fd)) ;
   for (unsigned i = 0; i < lengthof(fd); ++ i) {
      TEST(ioevents[i].ioevents      == ioevent_READ) ;
      TEST(ioevents[i].eventid.val64 == 5*i+1) ;
   }

   // TEST unregisterfd_iopoll
   for (unsigned i = 0; i < lengthof(fd); ++ i) {
      TEST(0 == unregisterfd_iopoll(&iopoll, fd[i][0])) ;
      // check that no unregistered fd generates an event
      memset(ioevents, 0, sizeof(ioevents)) ;
      TEST(0 == wait_iopoll(&iopoll, &nr_events, lengthof(ioevents), ioevents, 0)) ;
      TEST(nr_events == lengthof(fd)-1-i) ;
      for (unsigned i2 = 0; i2 < lengthof(fd)-1-i; ++ i2) {
         TEST(ioevents[i2].ioevents      == ioevent_READ) ;
         TEST(ioevents[i2].eventid.val64 == 5*(i+1+i2)+1) ;
      }
   }

   // TEST EINVAL: wrong event mask
   TEST(EINVAL == registerfd_iopoll(&iopoll, fd[0][0], &(ioevent_t)ioevent_INIT_VAL64(~(uint32_t)ioevent_MASK, 0))) ;
   TEST(0 == registerfd_iopoll(&iopoll, fd[0][0], &(ioevent_t)ioevent_INIT_VAL64(ioevent_MASK, 0))) ;
   TEST(EINVAL == updatefd_iopoll(&iopoll, fd[0][0], &(ioevent_t)ioevent_INIT_VAL64(ioevent_MASK+1, 0))) ;
   TEST(EINVAL == updatefd_iopoll(&iopoll, fd[0][0], &(ioevent_t)ioevent_INIT_VAL64(ioevent_MASK+1, 0))) ;

   // TEST EEXIST: sys_iochannel_t registered twice
   TEST(EEXIST == registerfd_iopoll(&iopoll, fd[0][0], &(ioevent_t)ioevent_INIT_VAL64(ioevent_READ, 0))) ;

   // TEST ENOENT: sys_iochannel_t not registered
   TEST(ENOENT == updatefd_iopoll(&iopoll, fd[0][1], &(ioevent_t)ioevent_INIT_VAL64(ioevent_READ, 0))) ;
   TEST(ENOENT == unregisterfd_iopoll(&iopoll, fd[0][1])) ;

   // TEST EBADF: invalid file descriptor value
   TEST(EBADF == registerfd_iopoll(&iopoll, -1, &(ioevent_t)ioevent_INIT_VAL64(ioevent_READ, 0))) ;
   TEST(EBADF == updatefd_iopoll(&iopoll, -1, &(ioevent_t)ioevent_INIT_VAL64(ioevent_READ, 0))) ;
   TEST(EBADF == unregisterfd_iopoll(&iopoll, -1)) ;

   // TEST EBADF: closed file descriptor value
   close(fd[0][0]) ;
   TEST(EBADF == registerfd_iopoll(&iopoll, fd[0][0], &(ioevent_t)ioevent_INIT_VAL64(ioevent_READ, 0))) ;
   TEST(EBADF == updatefd_iopoll(&iopoll, fd[0][0], &(ioevent_t)ioevent_INIT_VAL64(ioevent_READ, 0))) ;
   TEST(EBADF == unregisterfd_iopoll(&iopoll, fd[0][0])) ;
   fd[0][0] = -1 ;

   // TEST EBADF: iopoll system object freed
   TEST(0 == free_iopoll(&iopoll)) ;
   TEST(EBADF == registerfd_iopoll(&iopoll, fd[1][0], &(ioevent_t)ioevent_INIT_VAL64(ioevent_READ, 0))) ;
   TEST(EBADF == updatefd_iopoll(&iopoll, fd[1][0], &(ioevent_t)ioevent_INIT_VAL64(ioevent_READ, 0))) ;
   TEST(EBADF == unregisterfd_iopoll(&iopoll, fd[1][0])) ;

   // unprepare
   TEST(0 == free_iopoll(&iopoll)) ;
   for (unsigned i = 0; i < lengthof(fd); ++i) {
      TEST(0 == free_iochannel(fd[i])) ;
      TEST(0 == free_iochannel(fd[i]+1)) ;
   }

   return 0 ;
ONERR:
   free_iopoll(&iopoll) ;
   for (unsigned i = 0; i < lengthof(fd); ++i) {
      free_iochannel(fd[i]) ;
      free_iochannel(fd[i]+1) ;
   }
   return EINVAL ;
}

static int test_waitevents(void)
{
   iopoll_t    iopoll = iopoll_FREE ;
   size_t      nr_events ;
   char        buffer[128] ;
   int         fd[15][2] ;
   ioevent_t   ioevents[15*2+1] ;

   // prepare
   memset(fd, -1, sizeof(fd)) ;
   for (unsigned i = 0; i < lengthof(fd); ++ i) {
      TEST(0 == pipe2(fd[i], O_CLOEXEC|O_NONBLOCK)) ;
      TEST(2 == write(fd[i][1], "89", 2)) ;
   }
   TEST(0 == init_iopoll(&iopoll)) ;

   // TEST wait_iopoll: calling it twice returns the same nr of events (level triggered)
   for (unsigned i = 0; i < lengthof(fd); ++ i) {
      TEST(0 == registerfd_iopoll(&iopoll, fd[i][0], &(ioevent_t)ioevent_INIT_VAL32(ioevent_READ, i))) ;
   }
   memset(ioevents, 0, sizeof(ioevents)) ;
   TEST(0 == wait_iopoll(&iopoll, &nr_events, lengthof(ioevents), ioevents, 0)) ;
   TEST(nr_events == lengthof(fd)) ;
   for (unsigned i = 0; i < lengthof(fd); ++ i) {
      TEST(ioevents[i].ioevents      == ioevent_READ) ;
      TEST(ioevents[i].eventid.val32 == i) ;
   }
   memset(ioevents, 0, sizeof(ioevents)) ;
   TEST(0 == wait_iopoll(&iopoll, &nr_events, lengthof(ioevents), ioevents, 0)) ;
   TEST(nr_events == lengthof(fd)) ;
   for (unsigned i = 0; i < lengthof(fd); ++ i) {
      TEST(ioevents[i].ioevents      == ioevent_READ) ;
      TEST(ioevents[i].eventid.val32 == i) ;
   }

   // TEST wait_iopoll: in case eventqueue too small => consecutive calls return all events
   for (unsigned offset = 0; offset < lengthof(fd); offset += 5) {
      static_assert(lengthof(fd) > 5, "reading 5 at once") ;
      static_assert(0 == (lengthof(fd) % 5), "all events read in steps of 5") ;
      TEST(0 == wait_iopoll(&iopoll, &nr_events, 5, ioevents, 0)) ;
      TEST(5 == nr_events) ;
      for (unsigned i = 0; i < 5; ++ i) {
         TEST(ioevents[i].ioevents      == ioevent_READ) ;
         TEST(ioevents[i].eventid.val32 == (offset+i)) ;
      }
   }

   // TEST wait_iopoll: no registered fd => result size 0
   for (unsigned i = 0; i < lengthof(fd); ++ i) {
      TEST(0 == unregisterfd_iopoll(&iopoll, fd[i][0])) ;
   }
   TEST(0 == wait_iopoll(&iopoll, &nr_events, lengthof(ioevents), ioevents, 0)) ;
   TEST(0 == nr_events) ;

   // TEST wait_iopoll: ioevent_READ + ioevent_WRITE events
   for (unsigned i = 0; i < lengthof(fd); ++ i) {
      TEST(0 == registerfd_iopoll(&iopoll, fd[i][0], &(ioevent_t)ioevent_INIT_VAL32(ioevent_READ, 2*i))) ;
      TEST(0 == registerfd_iopoll(&iopoll, fd[i][1], &(ioevent_t)ioevent_INIT_VAL32(ioevent_WRITE, 2*i+1))) ;
   }
   TEST(0 == wait_iopoll(&iopoll, &nr_events, lengthof(ioevents), ioevents, 0)) ;
   TEST(nr_events == 2*lengthof(fd)) ;
   for (unsigned i = 0; i < 2*lengthof(fd); ++ i) {
      TEST(ioevents[i].ioevents      == ((i%2) ? ioevent_WRITE : ioevent_READ)) ;
      TEST(ioevents[i].eventid.val32 == i) ;
   }

   // TEST wait_iopoll: no ioevent_READ after reading
   for (unsigned i = 0; i < lengthof(fd); ++ i) {
      memset(buffer, 0 , 3) ;
      TEST(2 == read(fd[i][0], buffer, 2)) ;
      TEST(0 == strcmp(buffer, "89")) ;
      errno = 0 ;
      TEST(-1 == read(fd[i][0], buffer, 1)/*not closed*/) ;
      TEST(EAGAIN == errno) ;
   }
   TEST(0 == wait_iopoll(&iopoll, &nr_events, lengthof(ioevents), ioevents, 0)) ;
   TEST(nr_events == lengthof(fd)) ;
   for (unsigned i = 0; i < lengthof(fd); ++ i) {
      TEST(ioevents[i].ioevents      == ioevent_WRITE) ;
      TEST(ioevents[i].eventid.val32 == 2*i+1) ;
   }

   // TEST wait_iopoll: no ioevent_WRITE after buffer full
   for (unsigned i = 0; i < lengthof(fd); ++ i) {
      while (sizeof(buffer) == write(fd[i][1], buffer, sizeof(buffer))) ;
      errno = 0 ;
      TEST(-1 == write(fd[i][1], buffer, 1)) ;
      TEST(EAGAIN == errno) ;
   }
   TEST(0 == wait_iopoll(&iopoll, &nr_events, lengthof(ioevents), ioevents, 0)) ;
   TEST(nr_events == lengthof(fd)) ;
   for (unsigned i = 0; i < lengthof(fd); ++ i) {
      TEST(ioevents[i].ioevents      == ioevent_READ) ;
      TEST(ioevents[i].eventid.val32 == 2*i) ;
   }

   // TEST wait_iopoll: ioevent_READ+ioevent_CLOSE / no ioevent_WRITE after close
   for (unsigned i = 0; i < lengthof(fd); ++ i) {
      TEST(0 == free_iochannel(&fd[i][1])) ;
   }
   TEST(0 == wait_iopoll(&iopoll, &nr_events, lengthof(ioevents), ioevents, 0)) ;
   TEST(nr_events == lengthof(fd)) ;
   for (unsigned i = 0; i < lengthof(fd); ++ i) {
      TEST(ioevents[i].ioevents      == (ioevent_READ|ioevent_CLOSE)) ;
      TEST(ioevents[i].eventid.val32 == 2*i) ;
   }

   // TEST wait_iopoll: only ioevent_CLOSE after reading data
   for (unsigned i = 0; i < lengthof(fd); ++ i) {
      while (sizeof(buffer) == read(fd[i][0], buffer, sizeof(buffer))) ;
      TEST(0 == read(fd[i][0], buffer, 1)/*closed*/) ;
   }
   TEST(0 == wait_iopoll(&iopoll, &nr_events, lengthof(ioevents), ioevents, 0)) ;
   TEST(nr_events == lengthof(fd)) ;
   for (unsigned i = 0; i < lengthof(fd); ++ i) {
      TEST(ioevents[i].ioevents      == ioevent_CLOSE) ;
      TEST(ioevents[i].eventid.val32 == 2*i) ;
   }

   // TEST wait_iopoll: close removes files except if another fd refers to the same system file object
   fd[0][1] = dup(fd[0][0]) ;
   for (unsigned i = 0; i < lengthof(fd); ++ i) {
      TEST(0 == free_iochannel(&fd[i][0])) ;
   }
   TEST(0 == wait_iopoll(&iopoll, &nr_events, lengthof(ioevents), ioevents, 0)) ;
   TEST(1 == nr_events) ;
   TEST(ioevents[0].ioevents      == ioevent_CLOSE) ;
   TEST(ioevents[0].eventid.val32 == 0) ;
   TEST(0 == free_iochannel(&fd[0][1])) ;
   TEST(0 == wait_iopoll(&iopoll, &nr_events, lengthof(ioevents), ioevents, 0)) ;
   TEST(0 == nr_events) ;

   // TEST wait_iopoll: registered with ioevent_EMPTY returns also ioevent_CLOSE
   TEST(0 == pipe2(fd[0], O_CLOEXEC|O_NONBLOCK)) ;
   TEST(0 == registerfd_iopoll(&iopoll, fd[0][0], &(ioevent_t)ioevent_INIT_VAL32(ioevent_READ, 10))) ;
   TEST(1 == write(fd[0][1], "1", 1)) ;
   TEST(0 == free_iochannel(&fd[0][1])) ;
   TEST(0 == wait_iopoll(&iopoll, &nr_events, lengthof(ioevents), ioevents, 0)) ;
   TEST(1 == nr_events) ;
   TEST(ioevents[0].ioevents      == (ioevent_READ|ioevent_CLOSE)) ;
   TEST(ioevents[0].eventid.val32 == 10) ;
   TEST(0 == updatefd_iopoll(&iopoll, fd[0][0], &(ioevent_t)ioevent_INIT_VAL32(ioevent_EMPTY, 11))) ;
   TEST(0 == wait_iopoll(&iopoll, &nr_events, lengthof(ioevents), ioevents, 0)) ;
   TEST(1 == nr_events) ;
   TEST(ioevents[0].ioevents      == ioevent_CLOSE) ;
   TEST(ioevents[0].eventid.val32 == 11) ;
   TEST(0 == free_iochannel(&fd[0][0])) ;

   // TEST wait_iopoll: registered with ioevent_EMPTY returns also ioevent_ERROR
   TEST(0 == pipe2(fd[0], O_CLOEXEC|O_NONBLOCK)) ;
   TEST(0 == registerfd_iopoll(&iopoll, fd[0][1], &(ioevent_t)ioevent_INIT_VAL32(ioevent_WRITE, 10))) ;
   TEST(0 == wait_iopoll(&iopoll, &nr_events, lengthof(ioevents), ioevents, 0)) ;
   TEST(1 == nr_events) ;
   TEST(ioevents[0].ioevents      == ioevent_WRITE) ;
   TEST(ioevents[0].eventid.val32 == 10) ;
   TEST(0 == free_iochannel(&fd[0][0])) ;
   TEST(0 == updatefd_iopoll(&iopoll, fd[0][1], &(ioevent_t)ioevent_INIT_VAL32(ioevent_EMPTY, 11))) ;
   TEST(0 == wait_iopoll(&iopoll, &nr_events, lengthof(ioevents), ioevents, 0)) ;
   TEST(1 == nr_events) ;
   TEST(ioevents[0].ioevents      == ioevent_ERROR) ;
   TEST(ioevents[0].eventid.val32 == 11) ;
   TEST(0 == free_iochannel(&fd[0][1])) ;

   // TEST wait_iopoll: shutdown on unix sockets closed connection is signaled with ioevent_READ
   TEST(0 == socketpair(AF_UNIX, SOCK_STREAM|SOCK_CLOEXEC|SOCK_NONBLOCK, 0, fd[0])) ;
   TEST(0 == registerfd_iopoll(&iopoll, fd[0][0], &(ioevent_t)ioevent_INIT_VAL32(ioevent_READ|ioevent_WRITE, 0))) ;
   TEST(0 == registerfd_iopoll(&iopoll, fd[0][1], &(ioevent_t)ioevent_INIT_VAL32(ioevent_READ|ioevent_WRITE, 1))) ;
   TEST(0 == wait_iopoll(&iopoll, &nr_events, lengthof(ioevents), ioevents, 0)) ;
   TEST(2 == nr_events) ;
   for (unsigned i = 0; i < 2; ++ i) {
      TEST(ioevents[i].ioevents      == ioevent_WRITE) ;
      TEST(ioevents[i].eventid.val32 == i) ;
   }
   errno = 0 ;
   TEST(-1 == read(fd[0][1], buffer, 1)/*not closed*/) ;
   TEST(EAGAIN == errno) ;
   TEST(0 == shutdown(fd[0][0], SHUT_WR)) ;
   TEST(0 == read(fd[0][1], buffer, 1)/*connection closed in reading direction*/) ;
   TEST(0 == updatefd_iopoll(&iopoll, fd[0][0], &(ioevent_t)ioevent_INIT_VAL32(ioevent_READ, 0))) ;
   TEST(0 == wait_iopoll(&iopoll, &nr_events, lengthof(ioevents), ioevents, 0)) ;
   TEST(1 == nr_events) ;
   TEST(ioevents[0].ioevents      == (ioevent_WRITE|ioevent_READ/*signals shutdown connection*/)) ;
   TEST(ioevents[0].eventid.val32 == 1) ;
   TEST(0 == read(fd[0][1], buffer, 1)/*connection closed by peer*/) ;

   // TEST wait_iopoll: unix sockets closed => ioevent_CLOSE
   TEST(0 == free_iochannel(&fd[0][0])) ;
   TEST(0 == wait_iopoll(&iopoll, &nr_events, lengthof(ioevents), ioevents, 0)) ;
   TEST(1 == nr_events) ;
   TEST(ioevents[0].ioevents      == (ioevent_WRITE|ioevent_READ|ioevent_CLOSE/*signals peer closed connection*/)) ;
   TEST(ioevents[0].eventid.val32 == 1) ;
   TEST(0 == free_iochannel(&fd[0][1])) ;

   // TEST wait_iopoll: reading side of unix pipe closed => ioevent_ERROR on writing side
   TEST(0 == pipe2(fd[0], O_CLOEXEC|O_NONBLOCK)) ;
   TEST(0 == registerfd_iopoll(&iopoll, fd[0][1], &(ioevent_t)ioevent_INIT_VAL32(ioevent_WRITE, 1))) ;
   TEST(0 == free_iochannel(&fd[0][0])) ;
   TEST(0 == wait_iopoll(&iopoll, &nr_events, lengthof(ioevents), ioevents, 0)) ;
   TEST(1 == nr_events) ;
   TEST(ioevents[0].ioevents      == (ioevent_WRITE|ioevent_ERROR/*signals peer closed connection*/)) ;
   TEST(ioevents[0].eventid.val32 == 1) ;
   TEST(0 == free_iochannel(&fd[0][1])) ;

   // TEST wait_iopoll: waits ~ 40 milli seconds for events
   TEST(0 == pipe2(fd[0], O_CLOEXEC|O_NONBLOCK)) ;
   TEST(0 == registerfd_iopoll(&iopoll, fd[0][0], &(ioevent_t)ioevent_INIT_VAL32(ioevent_READ, 0))) ;
   timevalue_t starttv ;
   timevalue_t endtv ;
   TEST(0 == time_sysclock(sysclock_MONOTONIC, &starttv)) ;
   TEST(0 == wait_iopoll(&iopoll, &nr_events, lengthof(ioevents), ioevents, 40)) ;
   TEST(0 == time_sysclock(sysclock_MONOTONIC, &endtv)) ;
   int64_t millisec = diffms_timevalue(&endtv, &starttv) ;
   TEST(30 <= millisec && millisec <= 50) ;

   // TEST EINVAL: queue size zero
   TEST(EINVAL == wait_iopoll(&iopoll, &nr_events, 0, ioevents, 0)) ;

   // TEST EINVAL: queue size too large
   TEST(EINVAL == wait_iopoll(&iopoll, &nr_events, INT_MAX, ioevents, 0)) ;

   // TEST EINVAL: file descriptor is not of type epoll
   int old = iopoll.sys_poll ;
   iopoll.sys_poll = fd[0][0] ;
   TEST(EINVAL == wait_iopoll(&iopoll, &nr_events, 1, ioevents, 0)) ;
   iopoll.sys_poll = old ;

   // TEST EBADF: iopoll system object freed
   TEST(0 == free_iopoll(&iopoll)) ;
   TEST(EBADF == wait_iopoll(&iopoll, &nr_events, 1, ioevents, 0)) ;

   // unprepare
   TEST(0 == free_iopoll(&iopoll)) ;
   for (unsigned i = 0; i < lengthof(fd); ++i) {
      free_iochannel(fd[i]) ;
      free_iochannel(fd[i]+1) ;
   }

   return 0 ;
ONERR:
   free_iopoll(&iopoll) ;
   for (unsigned i = 0; i < lengthof(fd); ++i) {
      free_iochannel(fd[i]) ;
      free_iochannel(fd[i]+1) ;
   }
   return EINVAL ;
}

int unittest_io_iopoll()
{
   if (test_ioevent())        goto ONERR;
   if (test_initfree())       goto ONERR;
   if (test_registerfd())     goto ONERR;
   if (test_waitevents())     goto ONERR;

   return 0 ;
ONERR:
   return EINVAL ;
}

#endif
