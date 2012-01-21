/* title: IOPoll
   Facility to manage a set of <filedescr_t> and
   to wait and query for IO events.

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
#ifndef CKERN_IO_IOPOLL_HEADER
#define CKERN_IO_IOPOLL_HEADER

#include "C-kern/api/io/iocallback.h"
#include "C-kern/api/memory/memblock.h"

// forward
struct arraysf_t ;

/* typedef: struct iopoll_t
 * Exports <iopoll_t>. */
typedef struct iopoll_t                iopoll_t ;

/* typedef: struct iopoll_fdinfo_t
 * Exports opaque type <iopoll_fdinfo_t>.
 * This object is used internally to store
 * pointer to <iohandler_t> for the corresponding filedescriptor. */
typedef struct iopoll_fdinfo_t         iopoll_fdinfo_t ;

/* typedef: struct iopoll_fdinfolist_t
 * Exports <iopoll_fdinfolist_t>. */
typedef struct iopoll_fdinfolist_t     iopoll_fdinfolist_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_iopoll
 * Unittest for <iopoll_t>. */
extern int unittest_io_iopoll(void) ;
#endif


/* struct: iopoll_fdinfolist_t
 * This object is used internally to store a list of <iopoll_fdinfo_t>.
 * See <iopoll_t.changed_list>. */
struct iopoll_fdinfolist_t {
   /* variable: last
    * Points to last fdinfo which has changed. */
    iopoll_fdinfo_t  * last ;
} ;


/* struct: iopoll_t
 * This object manages a set of <filedescr_t>.
 * And it allows to wait for <ioevent_t> on one or more
 * of the managed file descriptors. */
struct iopoll_t {
   /* variable: sys_poll
    * Handle to the underlying system object. */
   sys_filedescr_t   sys_poll ;
   /* variable: nr_events
    * Number of events stored in <eventcache>. */
   size_t            nr_events ;
   /* variable: nr_filedescr
    * Number of <filedescr_t> registered at this object. */
   size_t            nr_filedescr ;
   /* variable: eventcache
    * Contains <nr_events> events read from <sys_poll>.
    * This buffer is filled by calling <processevents_iopoll>. */
   memblock_t        eventcache ;
   /* variable: changed_list
    * List of changed <iopoll_fdinfo_t>. This list cleared in <processevents_iopoll>. */
   iopoll_fdinfolist_t  changed_list ;
   /* variable: fdinfo
    * Contains for every registered filedescriptor the corresponding <iocallback_t>.
    * This info is set by calling <registerfd_iopoll> or cleared by calling <unregisterfd_iopoll>. */
   struct arraysf_t  * fdinfo ;
} ;

// group: lifetime

/* define: iopoll_INIT_FREEABLE
 * Static initializer. */
#define iopoll_INIT_FREEABLE           { filedescr_INIT_FREEABLE, 0, 0, memblock_INIT_FREEABLE, slist_INIT, 0 }

/* function: init_iopoll
 * Allocates system resources for <iopoll_t>. */
extern int init_iopoll(/*out*/iopoll_t * iopoll) ;

/* function: free_iopoll
 * Frees all allocated system resources.
 * If there is any unfreed dynamic set <iopoll_ds_t>  for this object
 * EAGAIN is returned and nothing is freed. */
extern int free_iopoll(iopoll_t * iopoll) ;

// group: manage

/* function: registerfd_iopoll
 * Registers <filedescr_t> *fd* and monitors for <ioevent_t> *ioevent*.
 * The parameter *ioevent* is the event mask. The parameter *iohandler* is the
 * io event handler which is called back if any event occurred whose bit is set in *ioevent*.
 *
 * Calling this function for an already registered descriptor returns EEXIST.
 *
 * List of unmaskable events:
 * The follwoing events can not be removed by clearing their bit
 * in the ioevent parameter and are always monitored.
 *
 * ioevent_ERROR  - Be always prepared to handle error conditions like network failures.
 * ioevent_CLOSE  - Be always prepared that the other side closed the connection.
 *
 * */
extern int registerfd_iopoll(iopoll_t * iopoll, sys_filedescr_t fd, iocallback_t * iohandler, ioevent_t ioevent) ;

/* function: changeioevent_iopoll
 * Changes <ioevent_t> *ioevent* <filedescr_t> fd is monitored for.
 * It returns ENOENT if the filedescriptor *fd* is not registered already. */
extern int changeioevent_iopoll(iopoll_t * iopoll, sys_filedescr_t fd, ioevent_t ioevent) ;

/* function: changeiocallback_iopoll
 * Changes <iocallback_t> *iohandler* for <filedescr_t> fd.
 * It returns ENOENT if the filedescriptor *fd* is not registered already. */
extern int changeiocallback_iopoll(iopoll_t * iopoll, sys_filedescr_t fd, iocallback_t * iohandler) ;

/* function: unregisterfd_iopoll
 * Unregisters <filedescr_t> *fd*.
 * No more event handler is called for filedescriptor *fd* after return of this call. */
extern int unregisterfd_iopoll(iopoll_t * iopoll, sys_filedescr_t fd) ;

/* function: processevents_iopoll
 * Waits *timeout_millisec* milli secconds for events and calls registered <iohandler_t>.
 * If *timeout_millisec* is set to 0 then this function polls only for any new events
 * but does not wait. If *timeout_millisec* has a value != 0 this function waits until any
 * event occurs but no longer than timeout_millisec milli seconds.
 *
 * After successfull return *nr_events_process* contains the number of occurred events.
 * If an <iocallback_f> event handler was registered for a filedescriptor
 * it is called with the occurred <ioevent_t> as one of its parameters. */
extern int processevents_iopoll(iopoll_t * iopoll, uint16_t timeout_millisec, /*out*/size_t * nr_events_processed) ;


#endif
