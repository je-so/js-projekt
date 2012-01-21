/* title: IOPoll Linux
   Implements <IOPoll>.

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

   file: C-kern/api/io/iopoll.h
    Header file <IOPoll>.

   file: C-kern/platform/Linux/io/iopoll.c
    Linux specific implementation file <IOPoll Linux>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/io/iopoll.h"
#include "C-kern/api/io/filedescr.h"
#include "C-kern/api/io/iocallback.h"
#include "C-kern/api/ds/arraysf_node.h"
#include "C-kern/api/ds/inmem/arraysf.h"
#include "C-kern/api/ds/inmem/slist.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/io/iotimer.h"
#endif


/* struct: iopoll_fdinfo_t
 * Stores <filedescr_t> and assocaited <iocallback_t>. */
struct iopoll_fdinfo_t {
   /* variable: fd
    * Filedescriptor is used as index into <arraysf_t>. */
   arraysf_node_t    fd ;
   /* variable: iohandler
    * I/O handler which is called if an event has been occurred. */
   iocallback_t      * iohandler ;
   /* variable: next
    * Points to next fdinfo which has changed.
    * The list of all changed <iopoll_fdinfo_t> objects is cleared in <wait_iopoll>
    * before the events are read from the system. <wait_iopoll> is called from
    * <processevents_iopoll>. In processevents_iopoll <iohandler> are called and they
    * are capable of changing unregistering filedescriptors and therefore putting them
    * into the list of changed <iopoll_fdinfo_t> (<next> != 0). An <iohandler> is any read
    * event event is only called if <next> is 0. */
   iopoll_fdinfo_t   * next ;
   /* variable: isdeleted
    * Flag indicating change: filedescriptor is in a deleted state. */
   uint8_t           isdeleted ;
} ;

slist_IMPLEMENT(iopoll_fdinfolist_t, _fdinfolist, next, slist_freecb_t)


// section: iopoll_t

// group: helper

static inline void compiletime_assert(void)
{
   static_assert( (typeof(&((iopoll_fdinfo_t*)0)->fd))0 == (arraysf_node_t*)0, "iopoll_fdinfo_t inherits from arraysf_node_t") ;
   static_assert( offsetof(iopoll_fdinfo_t, fd) == 0, "iopoll_fdinfo_t inherits from arraysf_node_t") ;
}

/* function: convertepollevent_iopoll
 * Converts <ioevent_t> into Linux EPOLL events. */
static uint32_t convertepollevent_iopoll(ioevent_t ioevent)
{
   uint32_t epollevents = 0 ;
   if (ioevent&ioevent_READ)  epollevents |= EPOLLIN ;
   if (ioevent&ioevent_WRITE) epollevents |= EPOLLOUT ;
   return epollevents ;
}

/* function: wait_iopoll
 * Waits for events and stores them in internal cache. */
static int wait_iopoll(iopoll_t * iopoll, uint16_t timeout)
{
   int err ;

   if (iopoll->nr_events) {
      err = EAGAIN ;
      goto ABBRUCH ;
   }

   if (iopoll->nr_filedescr) {
      // 1. allocate enough memory (1 event per filedescriptor)
      size_t cache_size = sizeof(struct epoll_event) * iopoll->nr_filedescr ;

      if (iopoll->nr_filedescr > (((size_t)-1)/2) / sizeof(struct epoll_event)) {
         err = ENOMEM ;
         goto ABBRUCH ;
      }

      if (cache_size > iopoll->eventcache.size) {
         void * newaddr = realloc(iopoll->eventcache.addr, cache_size) ;
         if (!newaddr) {
            err = ENOMEM ;
            LOG_OUTOFMEMORY(cache_size) ;
            goto ABBRUCH ;
         }

         iopoll->eventcache.addr = newaddr ;
         iopoll->eventcache.size = cache_size ;
      }

      while (!isempty_fdinfolist(&iopoll->changed_list)) {
         iopoll_fdinfo_t * first ;
         removefirst_fdinfolist(&iopoll->changed_list, &first) ;
         if (first->isdeleted) {
            (void) remove_arraysf(iopoll->fdinfo, first->fd.pos, 0) ;
         }
      }

      struct epoll_event * epev = (struct epoll_event*) iopoll->eventcache.addr ;

      err = epoll_wait(iopoll->sys_poll, epev, (int)iopoll->nr_filedescr, timeout) ;
      if (-1 == err) {
         err = errno ;
         LOG_SYSERR("epoll_wait", err) ;
         LOG_SIZE(iopoll->nr_filedescr) ;
         LOG_INT(iopoll->sys_poll) ;
         goto ABBRUCH ;
      }

      iopoll->nr_events   = (size_t) err ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

// group: lifetime

/* function: init_iopoll
 * Uses Linux specific call epoll_create1. */
int init_iopoll(/*out*/iopoll_t * iopoll)
{
   int err ;
   int            efd ;
   arraysf_t      * fdinfo = 0 ;
   arraysf_imp_it * impit  = 0 ;

   efd = epoll_create1(EPOLL_CLOEXEC) ;
   if (-1 == efd) {
      err = errno ;
      LOG_SYSERR("epoll_create1", err) ;
      goto ABBRUCH ;
   }

   err = new_arraysfimp(&impit, sizeof(iopoll_fdinfo_t), offsetof(iopoll_fdinfo_t, fd)) ;
   if (err) goto ABBRUCH ;

   err = new_arraysf(&fdinfo, arraysf_6BITROOT_UNSORTED, impit) ;
   if (err) goto ABBRUCH ;

   iopoll->sys_poll     = efd ;
   iopoll->nr_events    = 0 ;
   iopoll->nr_filedescr = 0 ;
   iopoll->eventcache   = (memblock_t) memblock_INIT_FREEABLE ;
   iopoll->changed_list = (iopoll_fdinfolist_t) slist_INIT ;
   iopoll->fdinfo       = fdinfo ;

   return 0 ;
ABBRUCH:
   delete_arraysfimp(&impit) ;
   free_filedescr(&efd) ;
   LOG_ABORT(err) ;
   return err ;
}

/* function: init_iopoll
 * Frees epoll queue by closing the associated file descriptor. */
int free_iopoll(iopoll_t * iopoll)
{
   int err ;
   int err2 ;

   if (iopoll->nr_events) {
      err = EAGAIN ;
      goto ABBRUCH ;
   }

   err = free_filedescr(&iopoll->sys_poll) ;

   iopoll->nr_filedescr = 0 ;

   if (!isfree_memblock(&iopoll->eventcache)) {
      free(iopoll->eventcache.addr) ;
      iopoll->eventcache = (memblock_t) memblock_INIT_FREEABLE ;
   }

   if (iopoll->fdinfo) {
      arraysf_imp_it * impit = impolicy_arraysf(iopoll->fdinfo) ;
      err2 = delete_arraysf(&iopoll->fdinfo);
      if (err2) err = err2 ;
      err2 = delete_arraysfimp(&impit) ;
      if (err2) err = err2 ;
   }

   iopoll->changed_list = (iopoll_fdinfolist_t) slist_INIT ;

   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}

int registerfd_iopoll(iopoll_t * iopoll, sys_filedescr_t fd, iocallback_t * iohandler, ioevent_t ioevent)
{
   int err ;
   iopoll_fdinfo_t   * iopfdinfo = 0 ;

   VALIDATE_INPARAM_TEST(isinit_filedescr(fd), ABBRUCH, ) ;
   VALIDATE_INPARAM_TEST(! (ioevent & ~(ioevent_READ|ioevent_WRITE|ioevent_ERROR|ioevent_CLOSE)), ABBRUCH, LOG_INT(ioevent) ) ;

   iopoll_fdinfo_t dummy = { .fd = arraysf_node_INIT((size_t)fd), .iohandler = iohandler, .next = 0, .isdeleted = 1 } ;
   err = tryinsert_arraysf(iopoll->fdinfo, &dummy.fd, (arraysf_node_t**)&iopfdinfo) ;
   if (err && EEXIST != err) goto ABBRUCH ;

   struct epoll_event epevent ;
   epevent.events   = convertepollevent_iopoll(ioevent) ;
   epevent.data.ptr = iopfdinfo ;

   err = epoll_ctl(iopoll->sys_poll, EPOLL_CTL_ADD, fd, &epevent) ;
   if (err) {
      err = errno ;
      LOG_SYSERR("epoll_ctl(EPOLL_CTL_ADD)", err) ;
      LOG_INT(fd) ;
      goto ABBRUCH ;
   }

   ++ iopoll->nr_filedescr ;

   assert(iopfdinfo->fd.pos == (size_t)fd) ;
   assert(iopfdinfo->isdeleted) ;
   iopfdinfo->iohandler = iohandler ;
   iopfdinfo->isdeleted = 0 ;
   if (!iopfdinfo->next) {
      insertfirst_fdinfolist(&iopoll->changed_list, iopfdinfo) ;
   }

   return 0 ;
ABBRUCH:
   if (     iopfdinfo
         && iopfdinfo->isdeleted
         && !iopfdinfo->next ) {
      insertfirst_fdinfolist(&iopoll->changed_list, iopfdinfo) ;
   }
   LOG_ABORT(err) ;
   return err ;
}

int changeioevent_iopoll(iopoll_t * iopoll, sys_filedescr_t fd, ioevent_t ioevent)
{
   int err ;
   struct epoll_event   epevent ;
   iopoll_fdinfo_t      * iopfdinfo ;

   VALIDATE_INPARAM_TEST(! (ioevent & ~(ioevent_READ|ioevent_WRITE|ioevent_ERROR|ioevent_CLOSE)), ABBRUCH, LOG_INT(ioevent)) ;

   iopfdinfo = (iopoll_fdinfo_t*) at_arraysf(iopoll->fdinfo, (size_t)fd) ;

   if (     ! iopfdinfo
         || iopfdinfo->isdeleted ) {
      err = ENOENT ;
      goto ABBRUCH ;
   }

   epevent.events   = convertepollevent_iopoll(ioevent) ;
   epevent.data.ptr = iopfdinfo ;

   err = epoll_ctl(iopoll->sys_poll, EPOLL_CTL_MOD, fd, &epevent) ;
   if (err) {
      err = errno ;
      LOG_SYSERR("epoll_ctl(EPOLL_CTL_MOD)", err) ;
      LOG_INT(fd) ;
      goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int changeiocallback_iopoll(iopoll_t * iopoll, sys_filedescr_t fd, iocallback_t * iohandler)
{
   int err ;
   iopoll_fdinfo_t   * iopfdinfo = (iopoll_fdinfo_t*) at_arraysf(iopoll->fdinfo, (size_t)fd) ;

   if (     ! iopfdinfo
         || iopfdinfo->isdeleted ) {
      err = ENOENT ;
      goto ABBRUCH ;
   }

   iopfdinfo->iohandler = iohandler ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int unregisterfd_iopoll(iopoll_t * iopoll, sys_filedescr_t fd)
{
   int err ;
   struct epoll_event   dummy ;
   iopoll_fdinfo_t      * iopfdinfo = (iopoll_fdinfo_t*) at_arraysf(iopoll->fdinfo, (size_t)fd) ;

   if (     ! iopfdinfo
         || iopfdinfo->isdeleted ) {
      err = ENOENT ;
      goto ABBRUCH ;
   }

   err = epoll_ctl(iopoll->sys_poll, EPOLL_CTL_DEL, fd, &dummy) ;
   if (err) {
      err = errno ;
      LOG_SYSERR("epoll_ctl(EPOLL_CTL_DEL)", err) ;
      LOG_INT(fd) ;
      goto ABBRUCH ;
   }

   -- iopoll->nr_filedescr ;

   iopfdinfo->isdeleted = 1 ;

   if (!iopfdinfo->next) {
      insertfirst_fdinfolist(&iopoll->changed_list, iopfdinfo) ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int processevents_iopoll(iopoll_t * iopoll, uint16_t timeout_millisec, /*out*/size_t * nr_events_processed)
{
   int err ;

   err = wait_iopoll(iopoll, timeout_millisec) ;
   if (err) goto ABBRUCH ;

   // call iohandler

   struct epoll_event * epevent = (struct epoll_event *) iopoll->eventcache.addr ;
   for(size_t i = iopoll->nr_events; (i--) > 0; ) {
      iopoll_fdinfo_t   * iopfdinfo = (iopoll_fdinfo_t*) epevent[i].data.ptr ;
      const uint32_t    events      = epevent[i].events ;
      const ioevent_t   ioevent     =  (ioevent_t) (
                                       (0 != (events & EPOLLIN))  * ioevent_READ
                                    +  (0 != (events & EPOLLOUT)) * ioevent_WRITE
                                    +  (0 != (events & EPOLLERR)) * ioevent_ERROR
                                    +  (0 != (events & EPOLLHUP)) * ioevent_CLOSE ) ;

      if (     0 == iopfdinfo->next    // not deleted or deleted+registered from iohandler
            && iopfdinfo->iohandler
            && iopfdinfo->iohandler->fct ) {
         iopfdinfo->iohandler->fct(iopfdinfo->iohandler, (int) iopfdinfo->fd.pos, ioevent) ;
      }
   }

   if (nr_events_processed) {
      *nr_events_processed = iopoll->nr_events ;
   }

   iopoll->nr_events = 0 ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

#define TEST(ARG) TEST_ONERROR_GOTO(ARG, ABBRUCH)

static int test_initfree(void)
{
   iopoll_t    iopoll = iopoll_INIT_FREEABLE ;
   iotimer_t   timer  = iotimer_INIT_FREEABLE ;
   uint64_t    millisec ;
   size_t      nr_events ;
   int         fd[40] ;
   size_t      nrfds[2] ;

   // prepare
   memset(fd, -1, sizeof(fd)) ;
   for(unsigned i = 0; i < nrelementsof(fd); i += 2) {
      TEST(0 == pipe2(fd+i, O_CLOEXEC)) ;
   }

   // TEST static init
   TEST(-1 == iopoll.sys_poll) ;
   TEST(0 == iopoll.nr_events) ;
   TEST(0 == iopoll.nr_filedescr) ;
   TEST(isfree_memblock(&iopoll.eventcache)) ;
   TEST(0 == iopoll.fdinfo) ;

   // TEST init, double free
   TEST(0 == nropen_filedescr(nrfds+0)) ;
   memset(&iopoll, -1, sizeof(iopoll)) ;
   TEST(0 == init_iopoll(&iopoll)) ;
   TEST(0 == nropen_filedescr(nrfds+1)) ;
   TEST(nrfds[1] == 1 + nrfds[0]) ;
   TEST(0 <  iopoll.sys_poll) ;
   TEST(0 == iopoll.nr_events) ;
   TEST(0 == iopoll.nr_filedescr) ;
   TEST(isfree_memblock(&iopoll.eventcache)) ;
   TEST(0 == iopoll.changed_list.last) ;
   TEST(0 != iopoll.fdinfo) ;
   TEST(0 == nrelements_arraysf(iopoll.fdinfo)) ;
   TEST(0 == registerfd_iopoll(&iopoll, fd[1], 0, ioevent_WRITE)) ;
   TEST(0 == processevents_iopoll(&iopoll, 0, &nr_events)) ;
   TEST(0 == registerfd_iopoll(&iopoll, fd[0], 0, ioevent_READ)) ;
   TEST(2 == iopoll.nr_filedescr) ;
   TEST(1 == nr_events) ;
   TEST(! isfree_memblock(&iopoll.eventcache)) ;
   TEST(0 != iopoll.changed_list.last) ;
   TEST(2 == nrelements_arraysf(iopoll.fdinfo)) ;
   TEST(0 == free_iopoll(&iopoll)) ;
   TEST(0 == nropen_filedescr(nrfds+1)) ;
   TEST(nrfds[1] == nrfds[0]) ;
   TEST(-1 == iopoll.sys_poll) ;
   TEST(0 == iopoll.nr_events) ;
   TEST(0 == iopoll.nr_filedescr) ;
   TEST(isfree_memblock(&iopoll.eventcache)) ;
   TEST(0 == iopoll.changed_list.last) ;
   TEST(0 == iopoll.fdinfo) ;
   TEST(0 == free_iopoll(&iopoll)) ;
   TEST(-1 == iopoll.sys_poll) ;
   TEST(0 == iopoll.nr_events) ;
   TEST(0 == iopoll.nr_filedescr) ;
   TEST(isfree_memblock(&iopoll.eventcache)) ;
   TEST(0 == iopoll.fdinfo) ;

   // TEST registerfd_iopoll
   TEST(0 == nropen_filedescr(nrfds+0)) ;
   TEST(0 == init_iopoll(&iopoll)) ;
   TEST(0 == nropen_filedescr(nrfds+1)) ;
   TEST(nrfds[1] == 1 + nrfds[0]) ;
   TEST(0 <  iopoll.sys_poll) ;
   for(unsigned i = 0; i < nrelementsof(fd); ++i) {
      TEST(0 == registerfd_iopoll(&iopoll, fd[i], (iocallback_t*)i, (ioevent_t) (i&0xF))) ;
      TEST(i+1 == iopoll.nr_filedescr) ;
      TEST(i+1 == nrelements_arraysf(iopoll.fdinfo)) ;
      iopoll_fdinfo_t * iopfdinfo = (iopoll_fdinfo_t *) at_arraysf(iopoll.fdinfo, (size_t)fd[i]) ;
      TEST(0 != iopfdinfo) ;
      TEST((void*)i == iopfdinfo->iohandler) ;
      TEST(0 == iopfdinfo->isdeleted) ;
      TEST(0 != iopfdinfo->next) ;
   }
   TEST(0 == iopoll.nr_events) ;
   TEST(nrelementsof(fd) == iopoll.nr_filedescr) ;
   TEST(isfree_memblock(&iopoll.eventcache)) ;
   TEST(0 == free_iopoll(&iopoll)) ;
   TEST(0 == nropen_filedescr(nrfds+1)) ;
   TEST(nrfds[1] == nrfds[0]) ;
   TEST(-1 == iopoll.sys_poll) ;
   TEST(0 == iopoll.nr_events) ;
   TEST(0 == iopoll.nr_filedescr) ;
   TEST(isfree_memblock(&iopoll.eventcache)) ;

   // TEST unregisterfd_iopoll
   TEST(0 == nropen_filedescr(nrfds+0)) ;
   TEST(0 == init_iopoll(&iopoll)) ;
   TEST(0 == nropen_filedescr(nrfds+1)) ;
   TEST(nrfds[1] == 1 + nrfds[0]) ;
   TEST(0 <  iopoll.sys_poll) ;
   TEST(0 == iopoll.nr_events) ;
   TEST(0 == iopoll.nr_filedescr) ;
   TEST(isfree_memblock(&iopoll.eventcache)) ;
   for(int ev = 0; ev <= 15; ++ ev) {
      for(unsigned i = 0; i < nrelementsof(fd); ++i) {
         TEST(0 == registerfd_iopoll(&iopoll, fd[i], (iocallback_t*)i, (ioevent_t)ev)) ;
      }
      TEST(0 == iopoll.nr_events) ;
      TEST(nrelementsof(fd) == iopoll.nr_filedescr) ;
      TEST(isfree_memblock(&iopoll.eventcache)) ;
      TEST(nrelementsof(fd) == nrelements_arraysf(iopoll.fdinfo)) ;
      for(unsigned i = nrelementsof(fd); i--; ) {
         TEST(0 == unregisterfd_iopoll(&iopoll, fd[i])) ;
         TEST(i == iopoll.nr_filedescr) ;
         iopoll_fdinfo_t * iopfdinfo = (iopoll_fdinfo_t*) at_arraysf(iopoll.fdinfo, (size_t)fd[i]) ;
         TEST(0 != iopfdinfo) ;
         TEST((void*)i == iopfdinfo->iohandler) ;
         TEST(1 == iopfdinfo->isdeleted) ;
         TEST(0 != iopfdinfo->next) ;
      }
      TEST(0 == iopoll.nr_events) ;
      TEST(0 == iopoll.nr_filedescr) ;
      TEST(isfree_memblock(&iopoll.eventcache)) ;
      TEST(nrelementsof(fd) == nrelements_arraysf(iopoll.fdinfo)) ;
   }
   TEST(0 == free_iopoll(&iopoll)) ;
   TEST(0 == nropen_filedescr(nrfds+1)) ;
   TEST(nrfds[1] == nrfds[0]) ;

   // TEST changeioevent_iopoll, changeiocallback_iopoll
   TEST(0 == init_iopoll(&iopoll)) ;
   for(unsigned i = 0; i < nrelementsof(fd); ++i) {
      TEST(0 == registerfd_iopoll(&iopoll, fd[i], 0, (i%2) ? ioevent_WRITE : ioevent_READ)) ;
   }
   for(unsigned i = 0; i < nrelementsof(fd); ++i) {
      TEST(0 == changeioevent_iopoll(&iopoll, fd[i], (ioevent_t)(i&0xF))) ;
      iopoll_fdinfo_t * iopfdinfo = (iopoll_fdinfo_t*) at_arraysf(iopoll.fdinfo, (size_t)fd[i]) ;
      TEST(0 != iopfdinfo) ;
      TEST(0 == iopfdinfo->iohandler) ;
      TEST(0 == changeiocallback_iopoll(&iopoll, fd[i], (iocallback_t*)i)) ;
      TEST(0 == iopoll.nr_events) ;
      TEST(nrelementsof(fd) == iopoll.nr_filedescr) ;
      TEST(isfree_memblock(&iopoll.eventcache)) ;
      TEST((void*)i == iopfdinfo->iohandler) ;
      TEST(0 == iopfdinfo->isdeleted) ;
      TEST(0 != iopfdinfo->next) ;
   }
   TEST(0 == free_iopoll(&iopoll)) ;

   // TEST wait_iopollds does not block
   TEST(0 == init_iopoll(&iopoll)) ;
   TEST(0 == init_iotimer(&timer, timeclock_MONOTONIC)) ;
   TEST(0 == startinterval_iotimer(timer, &(timevalue_t){ .nanosec = 1000000 } )) ;
   TEST(0 == wait_iopoll(&iopoll, 0)) ;
   TEST(0 == expirationcount_iotimer(timer, &millisec)) ;
   TEST(0 == free_iotimer(&timer)) ;
   TEST(iopoll.nr_filedescr == 0) ;
   TEST(iopoll.nr_events    == 0) ;
   TEST(isfree_memblock(&iopoll.eventcache)) ;
   TEST(iopoll.fdinfo       != 0) ;
   TEST(0 == nrelements_arraysf(iopoll.fdinfo)) ;

   TEST(1 >= millisec) ;

   // TEST wait_iopollds waits 40 msec
   for(unsigned i = 0; i < nrelementsof(fd); i += 2) {
      TEST(0 == registerfd_iopoll(&iopoll, fd[i], 0, ioevent_EMPTY)) ;
      TEST(0 == registerfd_iopoll(&iopoll, fd[i+1], 0, ioevent_EMPTY)) ;
      TEST(0 == write_filedescr(fd[i+1], 1, "-", 0)) ;
      iopoll_fdinfo_t * iopfdinfo = (iopoll_fdinfo_t*) at_arraysf(iopoll.fdinfo, (size_t)fd[i]) ;
      TEST(0 != iopfdinfo) ;
      TEST(0 == iopfdinfo->isdeleted) ;
      TEST(0 != iopfdinfo->next) ;
      iopfdinfo = (iopoll_fdinfo_t*) at_arraysf(iopoll.fdinfo, (size_t)fd[i+1]) ;
      TEST(0 != iopfdinfo) ;
      TEST(0 != iopfdinfo->next) ;
   }
   TEST(0 == init_iotimer(&timer, timeclock_MONOTONIC)) ;
   TEST(0 == startinterval_iotimer(timer, &(timevalue_t){ .nanosec = 1000000 } )) ;
   TEST(0 == wait_iopoll(&iopoll, 40)) ;
   TEST(0 == expirationcount_iotimer(timer, &millisec)) ;
   TEST(0 == free_iotimer(&timer)) ;
   TEST(iopoll.nr_filedescr == nrelementsof(fd)) ;
   TEST(iopoll.nr_events    == 0) ;
   TEST(! isfree_memblock(&iopoll.eventcache)) ;
   TEST(iopoll.fdinfo       != 0) ;
   TEST(nrelements_arraysf(iopoll.fdinfo) == nrelementsof(fd)) ;
   TEST(40 <= millisec) ;
   TEST(50 >= millisec) ;

   // TEST wait_iopollds removed all entries from changed_list
   for(unsigned i = 0; i < nrelementsof(fd); ++i) {
      iopoll_fdinfo_t * iopfdinfo = (iopoll_fdinfo_t*) at_arraysf(iopoll.fdinfo, (size_t)fd[i]) ;
      TEST(0 != iopfdinfo) ;
      TEST(0 == iopfdinfo->isdeleted) ;
      TEST(0 == iopfdinfo->next) ;
   }

   // TEST EAGAIN (nr_events != 0)
   for(unsigned i = 0; i < nrelementsof(fd); i += 2) {
      TEST(0 == changeioevent_iopoll(&iopoll, fd[i], ioevent_READ)) ;
      TEST(0 == changeioevent_iopoll(&iopoll, fd[i+1], ioevent_WRITE)) ;
   }
   TEST(0 == wait_iopoll(&iopoll, 0)) ;
   TEST(nrelementsof(fd) == iopoll.nr_events) ;
   TEST(EAGAIN == wait_iopoll(&iopoll, 0)) ;
   TEST(EAGAIN == processevents_iopoll(&iopoll, 0, 0)) ;
   TEST(EAGAIN == free_iopoll(&iopoll)) ;
   TEST(0 <  iopoll.sys_poll) ;
   iopoll.nr_events = 0 ;
   TEST(0 == free_iopoll(&iopoll)) ;
   TEST(-1 == iopoll.sys_poll) ;

   // TEST EINVAL (ioevent_t has wrong value)
   TEST(0 == init_iopoll(&iopoll)) ;
   TEST(EINVAL == registerfd_iopoll(&iopoll, fd[0], 0, (ioevent_t)128)) ;
   TEST(EINVAL == changeioevent_iopoll(&iopoll, fd[0], (ioevent_t)128)) ;
   TEST(0 != iopoll.fdinfo) ;
   TEST(0 == nrelements_arraysf(iopoll.fdinfo)) ;
   TEST(0 == free_iopoll(&iopoll)) ;

   // TEST EEXIST (fd already registered)
   TEST(0 == init_iopoll(&iopoll)) ;
   TEST(0 == registerfd_iopoll(&iopoll, fd[0], (iocallback_t*)1, ioevent_READ)) ;
   TEST(1 == iopoll.nr_filedescr) ;
   TEST(EEXIST == registerfd_iopoll(&iopoll, fd[0], 0, ioevent_READ)) ;
   TEST(0 == iopoll.nr_events) ;
   TEST(1 == iopoll.nr_filedescr) ;
   TEST(1 == nrelements_arraysf(iopoll.fdinfo)) ;
   TEST(0 != at_arraysf(iopoll.fdinfo, (size_t)fd[0])) ;
   iopoll_fdinfo_t * iopfdinfo = (iopoll_fdinfo_t*) at_arraysf(iopoll.fdinfo, (size_t)fd[0]) ;
   TEST(fd[0] == (int)iopfdinfo->fd.pos) ;
   TEST((void*)1 == iopfdinfo->iohandler) ;
   TEST(0 == iopfdinfo->isdeleted) ;
   TEST(0 != iopfdinfo->next) ;
   TEST(isfree_memblock(&iopoll.eventcache)) ;
   TEST(0 == free_iopoll(&iopoll)) ;

   // TEST ENOENT (try unregister or change unregistered fd)
   TEST(0 == init_iopoll(&iopoll)) ;
   TEST(0 == registerfd_iopoll(&iopoll, fd[0], 0, ioevent_READ)) ;
   TEST(ENOENT == changeioevent_iopoll(&iopoll, fd[1], ioevent_READ)) ;
   TEST(ENOENT == changeiocallback_iopoll(&iopoll, fd[1], 0)) ;
   TEST(ENOENT == unregisterfd_iopoll(&iopoll, fd[1])) ;
   TEST(0 == iopoll.nr_events) ;
   TEST(1 == iopoll.nr_filedescr) ;
   TEST(isfree_memblock(&iopoll.eventcache)) ;
   TEST(0 == free_iopoll(&iopoll)) ;

   // unprepare
   for(unsigned i = 0; i < nrelementsof(fd); ++i) {
      TEST(0 == free_filedescr(fd+i)) ;
   }

   return 0 ;
ABBRUCH:
   for(unsigned i = 0; i < nrelementsof(fd); ++i) {
      free_filedescr(fd+i) ;
   }
   free_iopoll(&iopoll) ;
   free_iotimer(&timer) ;
   return EINVAL ;
}

typedef struct test_iohandler_t       test_iohandler_t ;

struct test_iohandler_t {
   iocallback_t   iohandler ;
   iopoll_t       * iopoll ;
   size_t         arraysize ;
   size_t         arraysize2 ;
   int            * fd ;
   unsigned       * isevent ;
   ioevent_t      * ioevent ;
} ;

static void test1_iohandler(iocallback_t * iohandler, filedescr_t fd, ioevent_t ioevent)
{
   test_iohandler_t * iohandler1 = (test_iohandler_t *) iohandler ;

   for(unsigned i = 0; i < iohandler1->arraysize; ++i) {
      if (fd == iohandler1->fd[i]) {
         ++ iohandler1->isevent[i] ;
         iohandler1->ioevent[i] = ioevent ;
         break ;
      }
   }
}

static void test2_iohandler(iocallback_t * iohandler, filedescr_t fd, ioevent_t ioevent)
{
   test_iohandler_t * iohandler2 = (test_iohandler_t *) iohandler ;

   for(unsigned i = 0; i < iohandler2->arraysize; ++i) {
      if (i < iohandler2->arraysize2) {
         int err = unregisterfd_iopoll(iohandler2->iopoll, iohandler2->fd[i]) ;
         assert(! err) ;
      }
      if (fd == iohandler2->fd[i]) {
         ++ iohandler2->isevent[i] ;
         iohandler2->ioevent[i] = ioevent ;
      }
   }
   iohandler2->arraysize2 = 0 ; // do not unregister twice
}

static void test3_iohandler(iocallback_t * iohandler, filedescr_t fd, ioevent_t ioevent)
{
   test_iohandler_t * iohandler3 = (test_iohandler_t *) iohandler ;

   for(unsigned i = 0; i < iohandler3->arraysize; ++i) {
      if (i < iohandler3->arraysize2) {
         int err = unregisterfd_iopoll(iohandler3->iopoll, iohandler3->fd[i]) ;
         assert(! err) ;
         err = registerfd_iopoll(iohandler3->iopoll, iohandler3->fd[i], &iohandler3->iohandler, (i%2)?ioevent_WRITE:ioevent_READ) ;
         assert(! err) ;
      }
      if (fd == iohandler3->fd[i]) {
         ++ iohandler3->isevent[i] ;
         iohandler3->ioevent[i] = ioevent ;
      }
   }
   iohandler3->arraysize2 = 0 ; // do not unregister/register twice
}

static int test_processevents(void)
{
   iopoll_t    iopoll = iopoll_INIT_FREEABLE ;
   iotimer_t   timer  = iotimer_INIT_FREEABLE ;
   uint64_t    millisec ;
   size_t      nr_events ;
   int         fd[30] ;
   unsigned    isevent[nrelementsof(fd)] ;
   ioevent_t   ioevent[nrelementsof(fd)] ;
   test_iohandler_t iohandler1 = {
               .iohandler.fct = &test1_iohandler,
               .iopoll        = &iopoll,
               .arraysize     = nrelementsof(fd),
               .arraysize2    = nrelementsof(fd),
               .fd            = fd,
               .isevent       = isevent,
               .ioevent       = ioevent
            } ;
   test_iohandler_t iohandler2 = iohandler1 ;
   test_iohandler_t iohandler3 = iohandler1 ;

   iohandler2.iohandler.fct = &test2_iohandler ;
   iohandler3.iohandler.fct = &test3_iohandler ;

   // prepare
   TEST(0 == init_iopoll(&iopoll)) ;
   memset(fd, -1, sizeof(fd)) ;
   for(unsigned i = 0; i < nrelementsof(fd); i += 2) {
      TEST(0 == pipe2(&fd[i], O_CLOEXEC)) ;
   }

   // TEST processevents_iopoll does not block
   for(unsigned i = 0; i < nrelementsof(fd); i += 2) {
      TEST(0 == registerfd_iopoll(&iopoll, fd[i], 0, ioevent_EMPTY)) ;
      TEST(0 == registerfd_iopoll(&iopoll, fd[i+1], 0, ioevent_EMPTY)) ;
      TEST(0 == write_filedescr(fd[i+1], 1, "-", 0)) ;
   }
   nr_events = 1 ;
   TEST(0 == init_iotimer(&timer, timeclock_MONOTONIC)) ;
   TEST(0 == startinterval_iotimer(timer, &(timevalue_t){ .nanosec = 1000000 } )) ;
   TEST(0 == processevents_iopoll(&iopoll, 0, &nr_events)) ;
   TEST(0 == expirationcount_iotimer(timer, &millisec)) ;
   TEST(0 == free_iotimer(&timer)) ;
   TEST(nr_events              == 0) ;
   TEST(iopoll.nr_events       == 0) ;
   TEST(iopoll.nr_filedescr    == nrelementsof(fd)) ;
   TEST(! isfree_memblock(&iopoll.eventcache)) ;
   TEST(1 >= millisec) ;

   // TEST processevents_iopoll waits 20msec
   nr_events = 1 ;
   TEST(0 == init_iotimer(&timer, timeclock_MONOTONIC)) ;
   TEST(0 == startinterval_iotimer(timer, &(timevalue_t){ .nanosec = 1000000 } )) ;
   TEST(0 == processevents_iopoll(&iopoll, 20, &nr_events)) ;
   TEST(0 == expirationcount_iotimer(timer, &millisec)) ;
   TEST(0 == free_iotimer(&timer)) ;
   TEST(nr_events              == 0) ;
   TEST(iopoll.nr_events       == 0) ;
   TEST(iopoll.nr_filedescr    == nrelementsof(fd)) ;
   TEST(! isfree_memblock(&iopoll.eventcache)) ;
   TEST(20 <= millisec) ;
   TEST(30 > millisec) ;

   // TEST processevents_iopoll read all events
   for(unsigned i = 0; i < nrelementsof(fd); ++i) {
      TEST(0 == unregisterfd_iopoll(&iopoll, fd[i])) ;
      TEST(0 == registerfd_iopoll(&iopoll, fd[i], 0, ioevent_EMPTY)) ;
   }
   for(unsigned i = 0; i < nrelementsof(fd); i += 2) {
      TEST(0 == changeioevent_iopoll(&iopoll, fd[i], ioevent_READ)) ;
      TEST(0 == changeioevent_iopoll(&iopoll, fd[i+1], ioevent_WRITE)) ;
      TEST(0 == changeiocallback_iopoll(&iopoll, fd[i], &iohandler1.iohandler)) ;
      TEST(0 == changeiocallback_iopoll(&iopoll, fd[i+1], &iohandler1.iohandler)) ;
   }
   nr_events = 0 ;
   MEMSET0(&isevent) ;
   TEST(0 == processevents_iopoll(&iopoll, 0, &nr_events)) ;
   TEST(nr_events              == nrelementsof(fd)) ;
   TEST(iopoll.nr_events       == 0) ;
   TEST(iopoll.nr_filedescr    == nrelementsof(fd)) ;
   TEST(iopoll.eventcache.size == sizeof(struct epoll_event)*nrelementsof(fd)) ;
   TEST(! isfree_memblock(&iopoll.eventcache)) ;
   for(unsigned i = 0; i < nrelementsof(fd); ++i) {
      // all events read exactly once
      TEST(1 == isevent[i]) ;
      if (i % 2) {
         TEST(ioevent[i] == ioevent_WRITE) ;
      } else {
         TEST(ioevent[i] == ioevent_READ) ;
      }
   }

   // TEST processevents_iopoll removed all fdinfo from changed_list
   for(unsigned i = 0; i < nrelementsof(fd); ++i) {
      iopoll_fdinfo_t * iopfdinfo = (iopoll_fdinfo_t*) at_arraysf(iopoll.fdinfo, (size_t)fd[i]) ;
      TEST(0 != iopfdinfo) ;
      TEST(0 == iopfdinfo->next) ;
   }

   // TEST changeioevent_iopoll + processevents_iopoll read all events
   for(unsigned i = 0; i < nrelementsof(fd); i += 2) {
      TEST(0 == changeioevent_iopoll(&iopoll, fd[i], ioevent_EMPTY)) ;
   }
   nr_events = 0 ;
   MEMSET0(&isevent) ;
   TEST(0 == processevents_iopoll(&iopoll, 0, &nr_events)) ;
   TEST(nr_events              == nrelementsof(fd)/2) ;
   TEST(iopoll.nr_filedescr    == nrelementsof(fd)) ;
   TEST(iopoll.eventcache.size == sizeof(struct epoll_event)*nrelementsof(fd)) ;
   TEST(! isfree_memblock(&iopoll.eventcache)) ;
   for(unsigned i = 0; i < nrelementsof(fd); i += 2) {
      TEST(isevent[i]   == 0)
      TEST(isevent[i+1] == 1)
      TEST(ioevent[i+1] == ioevent_WRITE) ;
   }

   // TEST changeioevent_iopoll + read single event
   for(unsigned i = 0; i < nrelementsof(fd); i += 2) {
      TEST(0 == changeioevent_iopoll(&iopoll, fd[i+1], ioevent_EMPTY)) ;
   }
   TEST(0 == changeioevent_iopoll(&iopoll, fd[0], ioevent_READ)) ;
   nr_events = 0 ;
   MEMSET0(&isevent) ;
   TEST(0 == processevents_iopoll(&iopoll, 0, &nr_events)) ;
   TEST(nr_events              == 1) ;
   TEST(iopoll.nr_filedescr    == nrelementsof(fd)) ;
   TEST(iopoll.nr_events       == 0) ;
   TEST(isevent[0]             == 1)
   TEST(ioevent[0]             == ioevent_READ) ;
   for(unsigned i = 1; i < nrelementsof(fd); ++i) {
      TEST(0 == isevent[i]) ;
   }

   // TEST unregisterfd_iopoll in iohandler
   for(unsigned i = 0; i < nrelementsof(fd); i += 2) {
      TEST(0 == changeioevent_iopoll(&iopoll, fd[i], ioevent_READ)) ;
      TEST(0 == changeioevent_iopoll(&iopoll, fd[i+1], ioevent_WRITE)) ;
      TEST(0 == changeiocallback_iopoll(&iopoll, fd[i], &iohandler2.iohandler)) ;
      TEST(0 == changeiocallback_iopoll(&iopoll, fd[i+1], &iohandler2.iohandler)) ;
   }
   for(unsigned unregcount = 0; unregcount < nrelementsof(fd); ++unregcount) {
      MEMSET0(&isevent) ;
      iohandler2.arraysize2 = unregcount ;
      nr_events = 0 ;
      TEST(0 == processevents_iopoll(&iopoll, 0, &nr_events)) ;
      TEST(nr_events == nrelementsof(fd)) ;
      unsigned sum = 0 ;
      for(unsigned i = 0; i < nrelementsof(fd); ++ i) {
         TEST(0 == isevent[i] || 1 == isevent[i]) ;
         sum += isevent[i] ;
         if (i < unregcount) {
            if (i % 2) {
               TEST(0 == registerfd_iopoll(&iopoll, fd[i], &iohandler2.iohandler, ioevent_WRITE)) ;
            } else {
               TEST(0 == registerfd_iopoll(&iopoll, fd[i], &iohandler2.iohandler, ioevent_READ)) ;
            }
         }
      }
      TEST(sum == (nrelementsof(fd) - unregcount) || sum == 1+(nrelementsof(fd) - unregcount)) ;
   }

   // TEST unregisterfd_iopoll&register in iohandler
   for(unsigned i = 0; i < nrelementsof(fd); i += 2) {
      TEST(0 == changeioevent_iopoll(&iopoll, fd[i], ioevent_READ)) ;
      TEST(0 == changeioevent_iopoll(&iopoll, fd[i+1], ioevent_WRITE)) ;
      TEST(0 == changeiocallback_iopoll(&iopoll, fd[i], &iohandler3.iohandler)) ;
      TEST(0 == changeiocallback_iopoll(&iopoll, fd[i+1], &iohandler3.iohandler)) ;
   }
   for(unsigned unregcount = 0; unregcount <= nrelementsof(fd); ++unregcount) {
      MEMSET0(&isevent) ;
      iohandler3.arraysize2 = unregcount ;
      nr_events = 0 ;
      TEST(0 == processevents_iopoll(&iopoll, 0, &nr_events)) ;
      TEST(nr_events == nrelementsof(fd)) ;
      unsigned sum = 0 ;
      for(unsigned i = 0; i < nrelementsof(fd); ++ i) {
         TEST(0 == isevent[i] || 1 == isevent[i]) ;
         sum += isevent[i] ;
      }
      TEST(sum == (nrelementsof(fd) - unregcount) || sum == 1+(nrelementsof(fd) - unregcount)) ;
   }

   // TEST close events on read side
   for(unsigned i = 0; i < nrelementsof(fd); ++i) {
      TEST(0 == unregisterfd_iopoll(&iopoll, fd[i])) ;
      TEST(0 == free_filedescr(fd+i)) ;
   }
   for(unsigned i = 0; i < nrelementsof(fd); i += 2) {
      TEST(0 == pipe2(&fd[i], O_CLOEXEC)) ;
      TEST(0 == registerfd_iopoll(&iopoll, fd[i], &iohandler1.iohandler, ioevent_READ)) ;
      TEST(0 == free_filedescr(fd+i+1)) ;
   }
   MEMSET0(&isevent) ;
   nr_events = 0 ;
   TEST(0 == processevents_iopoll(&iopoll, 0, &nr_events)) ;
   TEST(nr_events == nrelementsof(fd)/2) ;
   for(unsigned i = 0; i < nrelementsof(fd); i += 2) {
      TEST(isevent[i] == 1) ;
      TEST(ioevent[i] == ioevent_CLOSE) ;
      TEST(isevent[i+1] == 0) ;
      TEST(0 == unregisterfd_iopoll(&iopoll, fd[i])) ;
      TEST(0 == free_filedescr(fd+i)) ;
   }

   // TEST error events on write side (closing the read side is considered an error)
   for(unsigned i = 0; i < nrelementsof(fd); i += 2) {
      TEST(0 == pipe2(&fd[i], O_CLOEXEC)) ;
      TEST(0 == free_filedescr(fd+i)) ;
      TEST(0 == registerfd_iopoll(&iopoll, fd[i+1], &iohandler1.iohandler, ioevent_WRITE)) ;
   }
   MEMSET0(&isevent) ;
   nr_events = 0 ;
   TEST(0 == processevents_iopoll(&iopoll, 0, &nr_events)) ;
   TEST(nr_events == nrelementsof(fd)/2) ;
   for(unsigned i = 0; i < nrelementsof(fd); i += 2) {
      TEST(isevent[i+1] == 1) ;
      TEST(ioevent[i+1] == (ioevent_WRITE|ioevent_ERROR)) ;
      TEST(isevent[i] == 0) ;
      TEST(0 == unregisterfd_iopoll(&iopoll, fd[i+1])) ;
      TEST(0 == free_filedescr(fd+i+1)) ;
   }

   // TEST shutdown
   TEST(0 == socketpair(AF_UNIX, SOCK_STREAM|SOCK_CLOEXEC, 0, fd)) ;
   TEST(0 == registerfd_iopoll(&iopoll, fd[0], &iohandler1.iohandler, ioevent_READ|ioevent_WRITE)) ;
   TEST(0 == registerfd_iopoll(&iopoll, fd[1], &iohandler1.iohandler, ioevent_READ|ioevent_WRITE)) ;
   MEMSET0(&isevent) ;
   TEST(0 == processevents_iopoll(&iopoll, 0, &nr_events)) ;
   TEST(2 == nr_events) ;
   for(unsigned i = 0; i < nr_events; ++i) {
      TEST(isevent[i] == 1) ;
      TEST(ioevent[i] == ioevent_WRITE) ;
   }
   TEST(0 == shutdown(fd[0], SHUT_WR)) ;
   TEST(0 == changeioevent_iopoll(&iopoll, fd[0], ioevent_READ)) ;
   MEMSET0(&isevent) ;
   TEST(0 == processevents_iopoll(&iopoll, 0, &nr_events)) ;
   TEST(1 == nr_events) ;
   TEST(isevent[1] == 1) ;
   TEST(ioevent[1] == (ioevent_READ/*signals shutdown connection*/|ioevent_WRITE)) ;
   {
      uint8_t buf ;
      TEST(0 == read(fd[1], &buf, 1)/*connection closed by peer*/) ;
   }

   // unprepare
   TEST(0 == free_iopoll(&iopoll)) ;
   for(unsigned i = 0; i < nrelementsof(fd); ++i) {
      TEST(0 == free_filedescr(fd+i)) ;
   }

   return 0 ;
ABBRUCH:
   for(unsigned i = 0; i < nrelementsof(fd); ++i) {
      free_filedescr(fd+i) ;
   }
   free_iopoll(&iopoll) ;
   free_iotimer(&timer) ;
   return EINVAL ;
}

int unittest_io_iopoll()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())       goto ABBRUCH ;
   if (test_processevents())  goto ABBRUCH ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif

