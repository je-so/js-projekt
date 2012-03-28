/* title: IOControler Linux
   Implements <IOControler>.

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

   file: C-kern/api/io/iocontroler.h
    Header file <IOControler>.

   file: C-kern/platform/Linux/io/iocontroler.c
    Linux specific implementation file <IOControler Linux>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/io/iocontroler.h"
#include "C-kern/api/io/ioevent.h"
#include "C-kern/api/io/filedescr.h"
#include "C-kern/api/ds/inmem/arraysf.h"
#include "C-kern/api/ds/inmem/slist.h"
#include "C-kern/api/ds/inmem/node/arraysf_node.h"
#include "C-kern/api/ds/typeadapter.h"
#include "C-kern/api/err.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#include "C-kern/api/memory/mm/mm_it.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/io/iotimer.h"
#endif


/* struct: iocontroler_iocb_t
 * Stores <filedescr_t> and assocaited <iocallback_t>. */
struct iocontroler_iocb_t {
   /* variable: fd
    * Filedescriptor is used as index into <arraysf_t>. */
   size_t               fd ;
   /* variable: iocb
    * I/O callback handler which is called if an event has been occurred. */
   iocallback_iot       iocb ;
   /* variable: next
    * Points to next <iocontroler_iocb_t> which has changed.
    * The list of all changed <iocontroler_iocb_t> objects is cleared in <wait_iocontroler>
    * before the events are read from the system. <wait_iocontroler> is called from
    * <processevents_iocontroler>. In processevents_iocontroler <iocb> are called and they
    * are capable of unregistering an I/O handler for any file descriptor and therefore putting them
    * into the list of changed <iocontroler_iocb_t> (<next> != 0).
    * A handler (see <iocb>) for any signaled io event is only called if <next> is 0. */
   iocontroler_iocb_t   * next ;
   /* variable: isunregistered
    * Flag indicating the following change: the file descriptor is unregistered. */
   uint8_t              isunregistered ;
} ;

// group: declaration

/* struct: iocontroler_iocb_typeadapter_it
 * Declares subtyped interface of <typeadapter_it> adapted to <iocontroler_iocb_t> object types.
 * See <typeadapter_it_DECLARE>. */
typeadapter_it_DECLARE(iocontroler_iocb_typeadapter_it, typeadapter_t, iocontroler_iocb_t)

/* struct: iocontroler_iocb_typeadapter_it
 * Declares subtyped object-interface of <typeadapter_iot> adapted to <iocontroler_iocb_typeadapter_it>.
 * See <typeadapter_iot_DECLARE>. */
typeadapter_iot_DECLARE(iocontroler_iocb_typeadapter_iot, typeadapter_t, struct iocontroler_iocb_typeadapter_it)

// group: variable

/* variable: s_iocontroler_iocb_adapter_default
 * Default implementation object of <typeadapter_iot>. */
static struct typeadapter_t                     s_iocontroler_iocb_adapter_default = typeadapter_INIT(sizeof(iocontroler_iocb_t)) ;

/* variable: s_iocontroler_iocb_adapter_iot
 * Subtyped typeadapter object-interface of type <iocontroler_iocb_typeadapter_it>.
 * It is initialized with <s_iocontroler_iocb_adapter_default> as its implementation object.
 * See <typeadapter_iot_INIT_GENERIC>. */
static struct iocontroler_iocb_typeadapter_iot  s_iocontroler_iocb_adapter_iot     = typeadapter_iot_INIT_GENERIC(&s_iocontroler_iocb_adapter_default) ;

// group: data structure

/* define: slist_IMPLEMENT_iocblist
 * Implements <slist_t> for <iocontroler_iocb_t> managed by <iocontoler_iocblist_t>.
 * See <slist_IMPLEMENT>. */
slist_IMPLEMENT(iocontoler_iocblist_t, _iocblist, next)

/* define: arraysf_IMPLEMENT_iocbarray
 * Implements <arraysf_t> for <iocontroler_iocb_t> managed by <arraysf_t>.
 * See <arraysf_IMPLEMENT>. */
arraysf_IMPLEMENT(iocontroler_iocb_t, _iocbarray, fd)


// section: iocontroler_t

// group: helper

static inline void compiletime_assert(void)
{
   static_assert( offsetof(iocontroler_iocb_t, fd) == 0, "iocontroler_iocb_t inherits from arraysf_node_t") ;
}

/* function: convertepollevent_iocontroler
 * Converts <ioevent_e> into Linux EPOLL events. */
static uint32_t convertepollevent_iocontroler(uint8_t ioevent)
{
   uint32_t epollevents = 0 ;
   if (ioevent&ioevent_READ)  epollevents |= EPOLLIN ;
   if (ioevent&ioevent_WRITE) epollevents |= EPOLLOUT ;
   return epollevents ;
}

/* function: wait_iocontroler
 * Waits for events and stores them in internal cache. */
static int wait_iocontroler(iocontroler_t * iocntr, uint16_t timeout)
{
   int err ;

   if (iocntr->nr_events) {
      err = EAGAIN ;
      goto ABBRUCH ;
   }

   if (iocntr->nr_filedescr) {
      // 1. allocate enough memory (1 event per filedescriptor)
      size_t cache_size = sizeof(struct epoll_event) * iocntr->nr_filedescr ;

      if (iocntr->nr_filedescr > (((size_t)-1)/2) / sizeof(struct epoll_event)) {
         err = ENOMEM ;
         goto ABBRUCH ;
      }

      if (cache_size > iocntr->eventcache.size) {
         void * newaddr = realloc(iocntr->eventcache.addr, cache_size) ;
         if (!newaddr) {
            err = ENOMEM ;
            LOG_OUTOFMEMORY(cache_size) ;
            goto ABBRUCH ;
         }

         iocntr->eventcache.addr = newaddr ;
         iocntr->eventcache.size = cache_size ;
      }

      while (!isempty_iocblist(&iocntr->changed_list)) {
         iocontroler_iocb_t * first ;
         removefirst_iocblist(&iocntr->changed_list, &first) ;
         if (first->isunregistered) {
            err = remove_iocbarray(iocntr->iocbs, first->fd, 0) ;
            if (err) goto ABBRUCH ;
            err = execfree_typeadapteriot(&s_iocontroler_iocb_adapter_iot, first) ;
            if (err) goto ABBRUCH ;
         }
      }

      struct epoll_event * epev = (struct epoll_event*) iocntr->eventcache.addr ;

      err = epoll_wait(iocntr->sys_poll, epev, (int)iocntr->nr_filedescr, timeout) ;
      if (-1 == err) {
         err = errno ;
         LOG_SYSERR("epoll_wait", err) ;
         LOG_SIZE(iocntr->nr_filedescr) ;
         LOG_INT(iocntr->sys_poll) ;
         goto ABBRUCH ;
      }

      iocntr->nr_events   = (size_t) err ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

// group: lifetime

/* function: init_iocontroler
 * Uses Linux specific call epoll_create1. */
int init_iocontroler(/*out*/iocontroler_t * iocntr)
{
   int err ;
   int            efd ;
   arraysf_t      * iocbs = 0 ;

   efd = epoll_create1(EPOLL_CLOEXEC) ;
   if (-1 == efd) {
      err = errno ;
      LOG_SYSERR("epoll_create1", err) ;
      goto ABBRUCH ;
   }

   err = new_iocbarray(&iocbs, arraysf_6BITROOT_UNSORTED) ;
   if (err) goto ABBRUCH ;

   iocntr->sys_poll     = efd ;
   iocntr->nr_events    = 0 ;
   iocntr->nr_filedescr = 0 ;
   iocntr->eventcache   = (memblock_t) memblock_INIT_FREEABLE ;
   iocntr->changed_list = (iocontoler_iocblist_t) slist_INIT ;
   iocntr->iocbs        = iocbs ;

   return 0 ;
ABBRUCH:
   free_filedescr(&efd) ;
   LOG_ABORT(err) ;
   return err ;
}

/* function: init_iocontroler
 * Frees epoll queue by closing the associated file descriptor. */
int free_iocontroler(iocontroler_t * iocntr)
{
   int err ;
   int err2 ;

   if (iocntr->nr_events) {
      err = EAGAIN ;
      goto ABBRUCH ;
   }

   err = free_filedescr(&iocntr->sys_poll) ;

   iocntr->nr_filedescr = 0 ;

   if (!isfree_memblock(&iocntr->eventcache)) {
      free(iocntr->eventcache.addr) ;
      iocntr->eventcache = (memblock_t) memblock_INIT_FREEABLE ;
   }

   if (iocntr->iocbs) {
      err2 = delete_iocbarray(&iocntr->iocbs, &s_iocontroler_iocb_adapter_iot.generic) ;
      if (err2) err = err2 ;
   }

   iocntr->changed_list = (iocontoler_iocblist_t) slist_INIT ;

   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}

int registeriocb_iocontroler(iocontroler_t * iocntr, sys_filedescr_t fd, uint8_t ioevents, iocallback_iot iocb)
{
   int err ;
   iocontroler_iocb_t   * newiocb = 0 ;

   VALIDATE_INPARAM_TEST(isinit_filedescr(fd), ABBRUCH, ) ;
   VALIDATE_INPARAM_TEST(! (ioevents & ~(ioevent_READ|ioevent_WRITE|ioevent_ERROR|ioevent_CLOSE)), ABBRUCH, LOG_INT(ioevents) ) ;

   iocontroler_iocb_t dummy = { .fd = (size_t)fd, .iocb = iocb, .next = 0, .isunregistered = 1 } ;
   err = tryinsert_iocbarray(iocntr->iocbs, &dummy, &newiocb, &s_iocontroler_iocb_adapter_iot.generic) ;
   if (err && EEXIST != err) goto ABBRUCH ;

   struct epoll_event epevent ;
   epevent.events   = convertepollevent_iocontroler(ioevents) ;
   epevent.data.ptr = newiocb ;

   err = epoll_ctl(iocntr->sys_poll, EPOLL_CTL_ADD, fd, &epevent) ;
   if (err) {
      err = errno ;
      LOG_SYSERR("epoll_ctl(EPOLL_CTL_ADD)", err) ;
      LOG_INT(fd) ;
      goto ABBRUCH ;
   }

   ++ iocntr->nr_filedescr ;

   assert(newiocb->fd == (size_t)fd) ;
   assert(newiocb->isunregistered) ;
   newiocb->iocb           = iocb ;
   newiocb->isunregistered = 0 ;
   if (!newiocb->next) {
      insertfirst_iocblist(&iocntr->changed_list, newiocb) ;
   }

   return 0 ;
ABBRUCH:
   if (     newiocb
         && newiocb->isunregistered
         && !newiocb->next ) {
      insertfirst_iocblist(&iocntr->changed_list, newiocb) ;
   }
   LOG_ABORT(err) ;
   return err ;
}

int changemask_iocontroler(iocontroler_t * iocntr, sys_filedescr_t fd, uint8_t ioevents)
{
   int err ;
   struct epoll_event   epevent ;
   iocontroler_iocb_t   * foundiocb ;

   VALIDATE_INPARAM_TEST(! (ioevents & ~(ioevent_READ|ioevent_WRITE|ioevent_ERROR|ioevent_CLOSE)), ABBRUCH, LOG_INT(ioevents)) ;

   foundiocb = at_iocbarray(iocntr->iocbs, (size_t)fd) ;

   if (   ! foundiocb
         || foundiocb->isunregistered ) {
      err = ENOENT ;
      goto ABBRUCH ;
   }

   epevent.events   = convertepollevent_iocontroler(ioevents) ;
   epevent.data.ptr = foundiocb ;

   err = epoll_ctl(iocntr->sys_poll, EPOLL_CTL_MOD, fd, &epevent) ;
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

int changeiocb_iocontroler(iocontroler_t * iocntr, sys_filedescr_t fd, iocallback_iot iocb)
{
   int err ;
   iocontroler_iocb_t  * foundiocb = at_iocbarray(iocntr->iocbs, (size_t)fd) ;

   if (   ! foundiocb
         || foundiocb->isunregistered ) {
      err = ENOENT ;
      goto ABBRUCH ;
   }

   foundiocb->iocb = iocb ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int unregisteriocb_iocontroler(iocontroler_t * iocntr, sys_filedescr_t fd)
{
   int err ;
   struct epoll_event   dummy ;
   iocontroler_iocb_t   * foundiocb  = at_iocbarray(iocntr->iocbs, (size_t)fd) ;

   if (   ! foundiocb
         || foundiocb->isunregistered ) {
      err = ENOENT ;
      goto ABBRUCH ;
   }

   err = epoll_ctl(iocntr->sys_poll, EPOLL_CTL_DEL, fd, &dummy) ;
   if (err) {
      err = errno ;
      LOG_SYSERR("epoll_ctl(EPOLL_CTL_DEL)", err) ;
      LOG_INT(fd) ;
      goto ABBRUCH ;
   }

   -- iocntr->nr_filedescr ;

   foundiocb->isunregistered = 1 ;

   if (!foundiocb->next) {
      insertfirst_iocblist(&iocntr->changed_list, foundiocb) ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int processevents_iocontroler(iocontroler_t * iocntr, uint16_t timeout_millisec, /*out*/size_t * nr_events_processed)
{
   int err ;

   err = wait_iocontroler(iocntr, timeout_millisec) ;
   if (err) goto ABBRUCH ;

   // call iohandler

   struct epoll_event * epevent = (struct epoll_event *) iocntr->eventcache.addr ;
   for(size_t i = iocntr->nr_events; (i--) > 0; ) {
      iocontroler_iocb_t   * iocb   = (iocontroler_iocb_t*) epevent[i].data.ptr ;
      const uint32_t       events   = epevent[i].events ;
      const uint8_t        ioevent  =  (uint8_t) (
                                       (0 != (events & EPOLLIN))  * ioevent_READ
                                    +  (0 != (events & EPOLLOUT)) * ioevent_WRITE
                                    +  (0 != (events & EPOLLERR)) * ioevent_ERROR
                                    +  (0 != (events & EPOLLHUP)) * ioevent_CLOSE ) ;

      if (     0 == iocb->next    // not unregistered or unregistered+registered from previous call to iohandler
            && isinit_iocallback(&iocb->iocb)) {
         handleioevent_iocallback(&iocb->iocb, (sys_filedescr_t) iocb->fd, ioevent) ;
      }
   }

   if (nr_events_processed) {
      *nr_events_processed = iocntr->nr_events ;
   }

   iocntr->nr_events = 0 ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   iocontroler_t  iocntr = iocontroler_INIT_FREEABLE ;
   iotimer_t      timer  = iotimer_INIT_FREEABLE ;
   uint64_t       millisec ;
   size_t         nr_events ;
   int            fd[40] ;
   size_t         nrfds[2] ;

   // prepare
   memset(fd, -1, sizeof(fd)) ;
   for(unsigned i = 0; i < nrelementsof(fd); i += 2) {
      TEST(0 == pipe2(fd+i, O_CLOEXEC)) ;
   }

   // TEST static init
   TEST(-1 == iocntr.sys_poll) ;
   TEST(0 == iocntr.nr_events) ;
   TEST(0 == iocntr.nr_filedescr) ;
   TEST(isfree_memblock(&iocntr.eventcache)) ;
   TEST(0 == iocntr.iocbs) ;

   // TEST init, double free
   TEST(0 == nropen_filedescr(nrfds+0)) ;
   memset(&iocntr, -1, sizeof(iocntr)) ;
   TEST(0 == init_iocontroler(&iocntr)) ;
   TEST(0 == nropen_filedescr(nrfds+1)) ;
   TEST(nrfds[1] == 1 + nrfds[0]) ;
   TEST(0 <  iocntr.sys_poll) ;
   TEST(0 == iocntr.nr_events) ;
   TEST(0 == iocntr.nr_filedescr) ;
   TEST(isfree_memblock(&iocntr.eventcache)) ;
   TEST(0 == iocntr.changed_list.last) ;
   TEST(0 != iocntr.iocbs) ;
   TEST(0 == length_iocbarray(iocntr.iocbs)) ;
   TEST(0 == registeriocb_iocontroler(&iocntr, fd[1], ioevent_WRITE, (iocallback_iot)iocallback_iot_INIT_FREEABLE)) ;
   TEST(0 == processevents_iocontroler(&iocntr, 0, &nr_events)) ;
   TEST(0 == registeriocb_iocontroler(&iocntr, fd[0], ioevent_READ, (iocallback_iot)iocallback_iot_INIT_FREEABLE)) ;
   TEST(2 == iocntr.nr_filedescr) ;
   TEST(1 == nr_events) ;
   TEST(! isfree_memblock(&iocntr.eventcache)) ;
   TEST(0 != iocntr.changed_list.last) ;
   TEST(2 == length_iocbarray(iocntr.iocbs)) ;
   TEST(0 == free_iocontroler(&iocntr)) ;
   TEST(0 == nropen_filedescr(nrfds+1)) ;
   TEST(nrfds[1] == nrfds[0]) ;
   TEST(-1 == iocntr.sys_poll) ;
   TEST(0 == iocntr.nr_events) ;
   TEST(0 == iocntr.nr_filedescr) ;
   TEST(isfree_memblock(&iocntr.eventcache)) ;
   TEST(0 == iocntr.changed_list.last) ;
   TEST(0 == iocntr.iocbs) ;
   TEST(0 == free_iocontroler(&iocntr)) ;
   TEST(-1 == iocntr.sys_poll) ;
   TEST(0 == iocntr.nr_events) ;
   TEST(0 == iocntr.nr_filedescr) ;
   TEST(isfree_memblock(&iocntr.eventcache)) ;
   TEST(0 == iocntr.iocbs) ;

   // TEST registeriocb_iocontroler
   TEST(0 == nropen_filedescr(nrfds+0)) ;
   TEST(0 == init_iocontroler(&iocntr)) ;
   TEST(0 == nropen_filedescr(nrfds+1)) ;
   TEST(nrfds[1] == 1 + nrfds[0]) ;
   TEST(0 <  iocntr.sys_poll) ;
   for(unsigned i = 0; i < nrelementsof(fd); ++i) {
      TEST(0 == registeriocb_iocontroler(&iocntr, fd[i], (uint8_t) (i&0xF), (iocallback_iot) iocallback_iot_INIT((void*) i, 0))) ;
      TEST(i+1 == iocntr.nr_filedescr) ;
      TEST(i+1 == length_iocbarray(iocntr.iocbs)) ;
      iocontroler_iocb_t * iocb = at_iocbarray(iocntr.iocbs, (size_t)fd[i]) ;
      TEST(0 != iocb) ;
      TEST((void*)i == iocb->iocb.object) ;
      TEST(0 == iocb->isunregistered) ;
      TEST(0 != iocb->next) ;
   }
   TEST(0 == iocntr.nr_events) ;
   TEST(nrelementsof(fd) == iocntr.nr_filedescr) ;
   TEST(isfree_memblock(&iocntr.eventcache)) ;
   TEST(0 == free_iocontroler(&iocntr)) ;
   TEST(0 == nropen_filedescr(nrfds+1)) ;
   TEST(nrfds[1] == nrfds[0]) ;
   TEST(-1 == iocntr.sys_poll) ;
   TEST(0 == iocntr.nr_events) ;
   TEST(0 == iocntr.nr_filedescr) ;
   TEST(isfree_memblock(&iocntr.eventcache)) ;

   // TEST unregisteriocb_iocontroler
   TEST(0 == nropen_filedescr(nrfds+0)) ;
   TEST(0 == init_iocontroler(&iocntr)) ;
   TEST(0 == nropen_filedescr(nrfds+1)) ;
   TEST(nrfds[1] == 1 + nrfds[0]) ;
   TEST(0 <  iocntr.sys_poll) ;
   TEST(0 == iocntr.nr_events) ;
   TEST(0 == iocntr.nr_filedescr) ;
   TEST(isfree_memblock(&iocntr.eventcache)) ;
   for(uint8_t ev = 0; ev <= 15; ++ ev) {
      for(unsigned i = 0; i < nrelementsof(fd); ++i) {
         iocallback_iot iocb = iocallback_iot_INIT((void*) i, 0) ;
         TEST(0 == registeriocb_iocontroler(&iocntr, fd[i], ev, iocb)) ;
      }
      TEST(0 == iocntr.nr_events) ;
      TEST(nrelementsof(fd) == iocntr.nr_filedescr) ;
      TEST(isfree_memblock(&iocntr.eventcache)) ;
      TEST(nrelementsof(fd) == length_iocbarray(iocntr.iocbs)) ;
      for(unsigned i = nrelementsof(fd); i--; ) {
         TEST(0 == unregisteriocb_iocontroler(&iocntr, fd[i])) ;
         TEST(i == iocntr.nr_filedescr) ;
         iocontroler_iocb_t * iocb = at_iocbarray(iocntr.iocbs, (size_t)fd[i]) ;
         TEST(0 != iocb) ;
         TEST((void*)i == iocb->iocb.object) ;
         TEST(1 == iocb->isunregistered) ;
         TEST(0 != iocb->next) ;
      }
      TEST(0 == iocntr.nr_events) ;
      TEST(0 == iocntr.nr_filedescr) ;
      TEST(isfree_memblock(&iocntr.eventcache)) ;
      TEST(nrelementsof(fd) == length_iocbarray(iocntr.iocbs)) ;
   }
   TEST(0 == free_iocontroler(&iocntr)) ;
   TEST(0 == nropen_filedescr(nrfds+1)) ;
   TEST(nrfds[1] == nrfds[0]) ;

   // TEST changemask_iocontroler, changeiocb_iocontroler
   TEST(0 == init_iocontroler(&iocntr)) ;
   for(unsigned i = 0; i < nrelementsof(fd); ++i) {
      iocallback_iot iocb = iocallback_iot_INIT_FREEABLE ;
      TEST(0 == registeriocb_iocontroler(&iocntr, fd[i], (i%2) ? ioevent_WRITE : ioevent_READ, iocb)) ;
   }
   for(unsigned i = 0; i < nrelementsof(fd); ++i) {
      TEST(0 == changemask_iocontroler(&iocntr, fd[i], (uint8_t)(i&0xF))) ;
      iocontroler_iocb_t * iocb = at_iocbarray(iocntr.iocbs, (size_t)fd[i]) ;
      TEST(0 != iocb) ;
      TEST(0 == iocb->iocb.object) ;
      TEST(0 == changeiocb_iocontroler(&iocntr, fd[i], (iocallback_iot) iocallback_iot_INIT((void*) i, 0))) ;
      TEST(0 == iocntr.nr_events) ;
      TEST(nrelementsof(fd) == iocntr.nr_filedescr) ;
      TEST(isfree_memblock(&iocntr.eventcache)) ;
      TEST((void*)i == iocb->iocb.object) ;
      TEST(0 == iocb->isunregistered) ;
      TEST(0 != iocb->next) ;
   }
   TEST(0 == free_iocontroler(&iocntr)) ;

   // TEST wait_iocontrolerds does not block
   TEST(0 == init_iocontroler(&iocntr)) ;
   TEST(0 == init_iotimer(&timer, timeclock_MONOTONIC)) ;
   TEST(0 == startinterval_iotimer(timer, &(timevalue_t){ .nanosec = 1000000 } )) ;
   TEST(0 == wait_iocontroler(&iocntr, 0)) ;
   TEST(0 == expirationcount_iotimer(timer, &millisec)) ;
   TEST(0 == free_iotimer(&timer)) ;
   TEST(iocntr.nr_filedescr == 0) ;
   TEST(iocntr.nr_events    == 0) ;
   TEST(isfree_memblock(&iocntr.eventcache)) ;
   TEST(iocntr.iocbs        != 0) ;
   TEST(0 == length_iocbarray(iocntr.iocbs)) ;

   TEST(1 >= millisec) ;

   // TEST wait_iocontrolerds waits 40 msec
   for(unsigned i = 0; i < nrelementsof(fd); i += 2) {
      TEST(0 == registeriocb_iocontroler(&iocntr, fd[i], ioevent_EMPTY, (iocallback_iot)iocallback_iot_INIT_FREEABLE)) ;
      TEST(0 == registeriocb_iocontroler(&iocntr, fd[i+1], ioevent_EMPTY, (iocallback_iot)iocallback_iot_INIT_FREEABLE)) ;
      TEST(0 == write_filedescr(fd[i+1], 1, "-", 0)) ;
      iocontroler_iocb_t * iocb = at_iocbarray(iocntr.iocbs, (size_t)fd[i]) ;
      TEST(0 != iocb) ;
      TEST(0 == iocb->isunregistered) ;
      TEST(0 != iocb->next) ;
      iocb = at_iocbarray(iocntr.iocbs, (size_t)fd[i+1]) ;
      TEST(0 != iocb) ;
      TEST(0 != iocb->next) ;
   }
   TEST(0 == init_iotimer(&timer, timeclock_MONOTONIC)) ;
   TEST(0 == startinterval_iotimer(timer, &(timevalue_t){ .nanosec = 1000000 } )) ;
   TEST(0 == wait_iocontroler(&iocntr, 40)) ;
   TEST(0 == expirationcount_iotimer(timer, &millisec)) ;
   TEST(0 == free_iotimer(&timer)) ;
   TEST(iocntr.nr_filedescr == nrelementsof(fd)) ;
   TEST(iocntr.nr_events    == 0) ;
   TEST(! isfree_memblock(&iocntr.eventcache)) ;
   TEST(iocntr.iocbs        != 0) ;
   TEST(length_iocbarray(iocntr.iocbs) == nrelementsof(fd)) ;
   TEST(40 <= millisec) ;
   TEST(50 >= millisec) ;

   // TEST wait_iocontrolerds removed all entries from changed_list
   for(unsigned i = 0; i < nrelementsof(fd); ++i) {
      iocontroler_iocb_t * iocb = at_iocbarray(iocntr.iocbs, (size_t)fd[i]) ;
      TEST(0 != iocb) ;
      TEST(0 == iocb->isunregistered) ;
      TEST(0 == iocb->next) ;
   }

   // TEST EAGAIN (nr_events != 0)
   for(unsigned i = 0; i < nrelementsof(fd); i += 2) {
      TEST(0 == changemask_iocontroler(&iocntr, fd[i], ioevent_READ)) ;
      TEST(0 == changemask_iocontroler(&iocntr, fd[i+1], ioevent_WRITE)) ;
   }
   TEST(0 == wait_iocontroler(&iocntr, 0)) ;
   TEST(nrelementsof(fd) == iocntr.nr_events) ;
   TEST(EAGAIN == wait_iocontroler(&iocntr, 0)) ;
   TEST(EAGAIN == processevents_iocontroler(&iocntr, 0, 0)) ;
   TEST(EAGAIN == free_iocontroler(&iocntr)) ;
   TEST(0 <  iocntr.sys_poll) ;
   iocntr.nr_events = 0 ;
   TEST(0 == free_iocontroler(&iocntr)) ;
   TEST(-1 == iocntr.sys_poll) ;

   // TEST EINVAL (ioevent_e has wrong value)
   TEST(0 == init_iocontroler(&iocntr)) ;
   TEST(EINVAL == registeriocb_iocontroler(&iocntr, fd[0], 128, (iocallback_iot)iocallback_iot_INIT_FREEABLE)) ;
   TEST(EINVAL == changemask_iocontroler(&iocntr, fd[0], 128)) ;
   TEST(0 != iocntr.iocbs) ;
   TEST(0 == length_iocbarray(iocntr.iocbs)) ;
   TEST(0 == free_iocontroler(&iocntr)) ;

   // TEST EEXIST (fd already registered)
   TEST(0 == init_iocontroler(&iocntr)) ;
   TEST(0 == registeriocb_iocontroler(&iocntr, fd[0], ioevent_READ, (iocallback_iot)iocallback_iot_INIT((void*)1, 0))) ;
   TEST(1 == iocntr.nr_filedescr) ;
   TEST(EEXIST == registeriocb_iocontroler(&iocntr, fd[0], ioevent_READ, (iocallback_iot)iocallback_iot_INIT_FREEABLE)) ;
   TEST(0 == iocntr.nr_events) ;
   TEST(1 == iocntr.nr_filedescr) ;
   TEST(1 == length_iocbarray(iocntr.iocbs)) ;
   TEST(0 != at_iocbarray(iocntr.iocbs, (size_t)fd[0])) ;
   iocontroler_iocb_t * iocb = at_iocbarray(iocntr.iocbs, (size_t)fd[0]) ;
   TEST(fd[0] == (sys_filedescr_t)iocb->fd) ;
   TEST((void*)1 == iocb->iocb.object) ;
   TEST(0 == iocb->isunregistered) ;
   TEST(0 != iocb->next) ;
   TEST(isfree_memblock(&iocntr.eventcache)) ;
   TEST(0 == free_iocontroler(&iocntr)) ;

   // TEST ENOENT (try unregister or change unregistered fd)
   TEST(0 == init_iocontroler(&iocntr)) ;
   TEST(0 == registeriocb_iocontroler(&iocntr, fd[0], ioevent_READ, (iocallback_iot)iocallback_iot_INIT_FREEABLE)) ;
   TEST(ENOENT == changemask_iocontroler(&iocntr, fd[1], ioevent_READ)) ;
   TEST(ENOENT == changeiocb_iocontroler(&iocntr, fd[1], (iocallback_iot)iocallback_iot_INIT_FREEABLE)) ;
   TEST(ENOENT == unregisteriocb_iocontroler(&iocntr, fd[1])) ;
   TEST(0 == iocntr.nr_events) ;
   TEST(1 == iocntr.nr_filedescr) ;
   TEST(isfree_memblock(&iocntr.eventcache)) ;
   TEST(0 == free_iocontroler(&iocntr)) ;

   // unprepare
   for(unsigned i = 0; i < nrelementsof(fd); ++i) {
      TEST(0 == free_filedescr(fd+i)) ;
   }

   return 0 ;
ABBRUCH:
   for(unsigned i = 0; i < nrelementsof(fd); ++i) {
      free_filedescr(fd+i) ;
   }
   free_iocontroler(&iocntr) ;
   free_iotimer(&timer) ;
   return EINVAL ;
}

typedef struct testcallback_iot        testcallback_iot ;

typedef struct test_iohandler_t        test_iohandler_t ;

iocallback_iot_DECLARE(testcallback_iot, test_iohandler_t)

struct test_iohandler_t {
   iocontroler_t  * iocntr ;
   size_t         arraysize ;
   size_t         arraysize2 ;
   int            * fd ;
   unsigned       * isevent ;
   uint8_t        * ioevent ;
} ;

static void test1_iohandler(test_iohandler_t * iohandler, filedescr_t fd, uint8_t ioevent)
{
   for(unsigned i = 0; i < iohandler->arraysize; ++i) {
      if (fd == iohandler->fd[i]) {
         ++ iohandler->isevent[i] ;
         iohandler->ioevent[i] = ioevent ;
         break ;
      }
   }
}

static void test2_iohandler(test_iohandler_t * iohandler, filedescr_t fd, uint8_t ioevent)
{
   for(unsigned i = 0; i < iohandler->arraysize; ++i) {
      if (i < iohandler->arraysize2) {
         int err = unregisteriocb_iocontroler(iohandler->iocntr, iohandler->fd[i]) ;
         assert(! err) ;
      }
      if (fd == iohandler->fd[i]) {
         ++ iohandler->isevent[i] ;
         iohandler->ioevent[i] = ioevent ;
      }
   }
   iohandler->arraysize2 = 0 ; // do not unregister twice
}

static void test3_iohandler(test_iohandler_t * iohandler, filedescr_t fd, uint8_t ioevent)
{
   for(unsigned i = 0; i < iohandler->arraysize; ++i) {
      if (i < iohandler->arraysize2) {
         int err = unregisteriocb_iocontroler(iohandler->iocntr, iohandler->fd[i]) ;
         assert(! err) ;
         testcallback_iot iocb = iocallback_iot_INIT(iohandler, &test3_iohandler) ;
         err = registeriocb_iocontroler(iohandler->iocntr, iohandler->fd[i], (i%2)?ioevent_WRITE:ioevent_READ, iocb.generic) ;
         assert(! err) ;
      }
      if (fd == iohandler->fd[i]) {
         ++ iohandler->isevent[i] ;
         iohandler->ioevent[i] = ioevent ;
      }
   }
   iohandler->arraysize2 = 0 ; // do not unregister/register twice
}

static int test_processevents(void)
{
   iocontroler_t     iocntr = iocontroler_INIT_FREEABLE ;
   iotimer_t         timer  = iotimer_INIT_FREEABLE ;
   uint64_t          millisec ;
   size_t            nr_events ;
   int               fd[30] ;
   unsigned          isevent[nrelementsof(fd)] ;
   uint8_t           ioevent[nrelementsof(fd)] ;
   test_iohandler_t  iohandler1 = {
               .iocntr        = &iocntr,
               .arraysize     = nrelementsof(fd),
               .arraysize2    = nrelementsof(fd),
               .fd            = fd,
               .isevent       = isevent,
               .ioevent       = ioevent
            } ;
   test_iohandler_t  iohandler2 = iohandler1 ;
   test_iohandler_t  iohandler3 = iohandler1 ;
   testcallback_iot  iocb1      = iocallback_iot_INIT(&iohandler1, &test1_iohandler) ;
   testcallback_iot  iocb2      = iocallback_iot_INIT(&iohandler2, &test2_iohandler) ;
   testcallback_iot  iocb3      = iocallback_iot_INIT(&iohandler3, &test3_iohandler) ;

   // prepare
   TEST(0 == init_iocontroler(&iocntr)) ;
   memset(fd, -1, sizeof(fd)) ;
   for(unsigned i = 0; i < nrelementsof(fd); i += 2) {
      TEST(0 == pipe2(&fd[i], O_CLOEXEC)) ;
   }

   // TEST processevents_iocontroler does not block
   for(unsigned i = 0; i < nrelementsof(fd); i += 2) {
      TEST(0 == registeriocb_iocontroler(&iocntr, fd[i], ioevent_EMPTY, (iocallback_iot)iocallback_iot_INIT_FREEABLE)) ;
      TEST(0 == registeriocb_iocontroler(&iocntr, fd[i+1], ioevent_EMPTY, (iocallback_iot)iocallback_iot_INIT_FREEABLE)) ;
      TEST(0 == write_filedescr(fd[i+1], 1, "-", 0)) ;
   }
   nr_events = 1 ;
   TEST(0 == init_iotimer(&timer, timeclock_MONOTONIC)) ;
   TEST(0 == startinterval_iotimer(timer, &(timevalue_t){ .nanosec = 1000000 } )) ;
   TEST(0 == processevents_iocontroler(&iocntr, 0, &nr_events)) ;
   TEST(0 == expirationcount_iotimer(timer, &millisec)) ;
   TEST(0 == free_iotimer(&timer)) ;
   TEST(nr_events              == 0) ;
   TEST(iocntr.nr_events       == 0) ;
   TEST(iocntr.nr_filedescr    == nrelementsof(fd)) ;
   TEST(! isfree_memblock(&iocntr.eventcache)) ;
   TEST(1 >= millisec) ;

   // TEST processevents_iocontroler waits 20msec
   nr_events = 1 ;
   TEST(0 == init_iotimer(&timer, timeclock_MONOTONIC)) ;
   TEST(0 == startinterval_iotimer(timer, &(timevalue_t){ .nanosec = 1000000 } )) ;
   TEST(0 == processevents_iocontroler(&iocntr, 20, &nr_events)) ;
   TEST(0 == expirationcount_iotimer(timer, &millisec)) ;
   TEST(0 == free_iotimer(&timer)) ;
   TEST(nr_events              == 0) ;
   TEST(iocntr.nr_events       == 0) ;
   TEST(iocntr.nr_filedescr    == nrelementsof(fd)) ;
   TEST(! isfree_memblock(&iocntr.eventcache)) ;
   TEST(20 <= millisec) ;
   TEST(30 > millisec) ;

   // TEST processevents_iocontroler read all events
   for(unsigned i = 0; i < nrelementsof(fd); ++i) {
      TEST(0 == unregisteriocb_iocontroler(&iocntr, fd[i])) ;
      TEST(0 == registeriocb_iocontroler(&iocntr, fd[i], ioevent_EMPTY, (iocallback_iot)iocallback_iot_INIT_FREEABLE)) ;
   }
   for(unsigned i = 0; i < nrelementsof(fd); i += 2) {
      TEST(0 == changemask_iocontroler(&iocntr, fd[i], ioevent_READ)) ;
      TEST(0 == changemask_iocontroler(&iocntr, fd[i+1], ioevent_WRITE)) ;
      TEST(0 == changeiocb_iocontroler(&iocntr, fd[i], iocb1.generic)) ;
      TEST(0 == changeiocb_iocontroler(&iocntr, fd[i+1], iocb1.generic)) ;
   }
   nr_events = 0 ;
   MEMSET0(&isevent) ;
   TEST(0 == processevents_iocontroler(&iocntr, 0, &nr_events)) ;
   TEST(nr_events              == nrelementsof(fd)) ;
   TEST(iocntr.nr_events       == 0) ;
   TEST(iocntr.nr_filedescr    == nrelementsof(fd)) ;
   TEST(iocntr.eventcache.size == sizeof(struct epoll_event)*nrelementsof(fd)) ;
   TEST(! isfree_memblock(&iocntr.eventcache)) ;
   for(unsigned i = 0; i < nrelementsof(fd); ++i) {
      // all events read exactly once
      TEST(1 == isevent[i]) ;
      if (i % 2) {
         TEST(ioevent[i] == ioevent_WRITE) ;
      } else {
         TEST(ioevent[i] == ioevent_READ) ;
      }
   }

   // TEST processevents_iocontroler removed all iocbs from changed_list
   for(unsigned i = 0; i < nrelementsof(fd); ++i) {
      iocontroler_iocb_t * iocb = at_iocbarray(iocntr.iocbs, (size_t)fd[i]) ;
      TEST(0 != iocb) ;
      TEST(0 == iocb->next) ;
   }

   // TEST changemask_iocontroler + processevents_iocontroler read all events
   for(unsigned i = 0; i < nrelementsof(fd); i += 2) {
      TEST(0 == changemask_iocontroler(&iocntr, fd[i], ioevent_EMPTY)) ;
   }
   nr_events = 0 ;
   MEMSET0(&isevent) ;
   TEST(0 == processevents_iocontroler(&iocntr, 0, &nr_events)) ;
   TEST(nr_events              == nrelementsof(fd)/2) ;
   TEST(iocntr.nr_filedescr    == nrelementsof(fd)) ;
   TEST(iocntr.eventcache.size == sizeof(struct epoll_event)*nrelementsof(fd)) ;
   TEST(! isfree_memblock(&iocntr.eventcache)) ;
   for(unsigned i = 0; i < nrelementsof(fd); i += 2) {
      TEST(isevent[i]   == 0)
      TEST(isevent[i+1] == 1)
      TEST(ioevent[i+1] == ioevent_WRITE) ;
   }

   // TEST changemask_iocontroler + read single event
   for(unsigned i = 0; i < nrelementsof(fd); i += 2) {
      TEST(0 == changemask_iocontroler(&iocntr, fd[i+1], ioevent_EMPTY)) ;
   }
   TEST(0 == changemask_iocontroler(&iocntr, fd[0], ioevent_READ)) ;
   nr_events = 0 ;
   MEMSET0(&isevent) ;
   TEST(0 == processevents_iocontroler(&iocntr, 0, &nr_events)) ;
   TEST(nr_events              == 1) ;
   TEST(iocntr.nr_filedescr    == nrelementsof(fd)) ;
   TEST(iocntr.nr_events       == 0) ;
   TEST(isevent[0]             == 1)
   TEST(ioevent[0]             == ioevent_READ) ;
   for(unsigned i = 1; i < nrelementsof(fd); ++i) {
      TEST(0 == isevent[i]) ;
   }

   // TEST unregisteriocb_iocontroler in iohandler
   for(unsigned i = 0; i < nrelementsof(fd); i += 2) {
      TEST(0 == changemask_iocontroler(&iocntr, fd[i], ioevent_READ)) ;
      TEST(0 == changemask_iocontroler(&iocntr, fd[i+1], ioevent_WRITE)) ;
      TEST(0 == changeiocb_iocontroler(&iocntr, fd[i], iocb2.generic)) ;
      TEST(0 == changeiocb_iocontroler(&iocntr, fd[i+1], iocb2.generic)) ;
   }
   for(unsigned unregcount = 0; unregcount < nrelementsof(fd); ++unregcount) {
      MEMSET0(&isevent) ;
      iohandler2.arraysize2 = unregcount ;
      nr_events = 0 ;
      TEST(0 == processevents_iocontroler(&iocntr, 0, &nr_events)) ;
      TEST(nr_events == nrelementsof(fd)) ;
      unsigned sum = 0 ;
      for(unsigned i = 0; i < nrelementsof(fd); ++ i) {
         TEST(0 == isevent[i] || 1 == isevent[i]) ;
         sum += isevent[i] ;
         if (i < unregcount) {
            if (i % 2) {
               TEST(0 == registeriocb_iocontroler(&iocntr, fd[i], ioevent_WRITE, iocb2.generic)) ;
            } else {
               TEST(0 == registeriocb_iocontroler(&iocntr, fd[i], ioevent_READ, iocb2.generic)) ;
            }
         }
      }
      TEST(sum == (nrelementsof(fd) - unregcount) || sum == 1+(nrelementsof(fd) - unregcount)) ;
   }

   // TEST unregisteriocb_iocontroler&register in iohandler
   for(unsigned i = 0; i < nrelementsof(fd); i += 2) {
      TEST(0 == changemask_iocontroler(&iocntr, fd[i], ioevent_READ)) ;
      TEST(0 == changemask_iocontroler(&iocntr, fd[i+1], ioevent_WRITE)) ;
      TEST(0 == changeiocb_iocontroler(&iocntr, fd[i], iocb3.generic)) ;
      TEST(0 == changeiocb_iocontroler(&iocntr, fd[i+1], iocb3.generic)) ;
   }
   for(unsigned unregcount = 0; unregcount <= nrelementsof(fd); ++unregcount) {
      MEMSET0(&isevent) ;
      iohandler3.arraysize2 = unregcount ;
      nr_events = 0 ;
      TEST(0 == processevents_iocontroler(&iocntr, 0, &nr_events)) ;
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
      TEST(0 == unregisteriocb_iocontroler(&iocntr, fd[i])) ;
      TEST(0 == free_filedescr(fd+i)) ;
   }
   for(unsigned i = 0; i < nrelementsof(fd); i += 2) {
      TEST(0 == pipe2(&fd[i], O_CLOEXEC)) ;
      TEST(0 == registeriocb_iocontroler(&iocntr, fd[i], ioevent_READ, iocb1.generic)) ;
      TEST(0 == free_filedescr(fd+i+1)) ;
   }
   MEMSET0(&isevent) ;
   nr_events = 0 ;
   TEST(0 == processevents_iocontroler(&iocntr, 0, &nr_events)) ;
   TEST(nr_events == nrelementsof(fd)/2) ;
   for(unsigned i = 0; i < nrelementsof(fd); i += 2) {
      TEST(isevent[i] == 1) ;
      TEST(ioevent[i] == ioevent_CLOSE) ;
      TEST(isevent[i+1] == 0) ;
      TEST(0 == unregisteriocb_iocontroler(&iocntr, fd[i])) ;
      TEST(0 == free_filedescr(fd+i)) ;
   }

   // TEST error events on write side (closing the read side is considered an error)
   for(unsigned i = 0; i < nrelementsof(fd); i += 2) {
      TEST(0 == pipe2(&fd[i], O_CLOEXEC)) ;
      TEST(0 == free_filedescr(fd+i)) ;
      TEST(0 == registeriocb_iocontroler(&iocntr, fd[i+1], ioevent_WRITE, iocb1.generic)) ;
   }
   MEMSET0(&isevent) ;
   nr_events = 0 ;
   TEST(0 == processevents_iocontroler(&iocntr, 0, &nr_events)) ;
   TEST(nr_events == nrelementsof(fd)/2) ;
   for(unsigned i = 0; i < nrelementsof(fd); i += 2) {
      TEST(isevent[i+1] == 1) ;
      TEST(ioevent[i+1] == (ioevent_WRITE|ioevent_ERROR)) ;
      TEST(isevent[i] == 0) ;
      TEST(0 == unregisteriocb_iocontroler(&iocntr, fd[i+1])) ;
      TEST(0 == free_filedescr(fd+i+1)) ;
   }

   // TEST shutdown
   TEST(0 == socketpair(AF_UNIX, SOCK_STREAM|SOCK_CLOEXEC, 0, fd)) ;
   TEST(0 == registeriocb_iocontroler(&iocntr, fd[0], ioevent_READ|ioevent_WRITE, iocb1.generic)) ;
   TEST(0 == registeriocb_iocontroler(&iocntr, fd[1], ioevent_READ|ioevent_WRITE, iocb1.generic)) ;
   MEMSET0(&isevent) ;
   TEST(0 == processevents_iocontroler(&iocntr, 0, &nr_events)) ;
   TEST(2 == nr_events) ;
   for(unsigned i = 0; i < nr_events; ++i) {
      TEST(isevent[i] == 1) ;
      TEST(ioevent[i] == ioevent_WRITE) ;
   }
   TEST(0 == shutdown(fd[0], SHUT_WR)) ;
   TEST(0 == changemask_iocontroler(&iocntr, fd[0], ioevent_READ)) ;
   MEMSET0(&isevent) ;
   TEST(0 == processevents_iocontroler(&iocntr, 0, &nr_events)) ;
   TEST(1 == nr_events) ;
   TEST(isevent[1] == 1) ;
   TEST(ioevent[1] == (ioevent_READ/*signals shutdown connection*/|ioevent_WRITE)) ;
   {
      uint8_t buf ;
      TEST(0 == read(fd[1], &buf, 1)/*connection closed by peer*/) ;
   }

   // unprepare
   TEST(0 == free_iocontroler(&iocntr)) ;
   for(unsigned i = 0; i < nrelementsof(fd); ++i) {
      TEST(0 == free_filedescr(fd+i)) ;
   }

   return 0 ;
ABBRUCH:
   for(unsigned i = 0; i < nrelementsof(fd); ++i) {
      free_filedescr(fd+i) ;
   }
   free_iocontroler(&iocntr) ;
   free_iotimer(&timer) ;
   return EINVAL ;
}

int unittest_io_iocontroler()
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

