/* title: IOControler
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

   file: C-kern/api/io/iocontroler.h
    Header file <IOControler>.

   file: C-kern/platform/Linux/io/iocontroler.c
    Linux specific implementation file <IOControler Linux>.
*/
#ifndef CKERN_IO_IOCONTROLER_HEADER
#define CKERN_IO_IOCONTROLER_HEADER

#include "C-kern/api/io/iocallback_iot.h"
#include "C-kern/api/memory/memblock.h"

// forward
struct arraysf_t ;

/* typedef: struct iocontroler_t
 * Exports <iocontroler_t>. */
typedef struct iocontroler_t           iocontroler_t ;

/* typedef: struct iocontroler_iocb_t
 * Exports opaque type <iocontroler_iocb_t>.
 * This object is used internally to associate <iocallback_iot>
 * with the corresponding file descriptor. */
typedef struct iocontroler_iocb_t      iocontroler_iocb_t ;

/* typedef: struct iocontoler_iocblist_t
 * Exports <iocontoler_iocblist_t>. */
typedef struct iocontoler_iocblist_t   iocontoler_iocblist_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_iocontroler
 * Unittest for <iocontroler_t>. */
int unittest_io_iocontroler(void) ;
#endif


/* struct: iocontoler_iocblist_t
 * This object is used internally to store a list of <iocontroler_iocb_t>.
 * See <iocontroler_t.changed_list>. */
struct iocontoler_iocblist_t {
   /* variable: last
    * Points to last <iocontroler_iocb_t> which has changed. */
    iocontroler_iocb_t  * last ;
} ;


/* struct: iocontroler_t
 * This object manages a set of <filedescr_t> and associated <iocallback_iot> callbacks.
 * It monitors the registered file desciptor for one or more <ioevent_e>s and calls
 * back the associated I/O handlers for any occurred I/O events. */
struct iocontroler_t {
   /* variable: sys_poll
    * Handle to the underlying system object. */
   sys_filedescr_t         sys_poll ;
   /* variable: nr_events
    * Number of events stored in <eventcache>. */
   size_t                  nr_events ;
   /* variable: nr_filedescr
    * Number of <filedescr_t> registered at this object. */
   size_t                  nr_filedescr ;
   /* variable: eventcache
    * Contains <nr_events> events read from <sys_poll>.
    * This buffer is filled by calling <processevents_iocontroler>. */
   memblock_t              eventcache ;
   /* variable: changed_list
    * List of changed <iocontroler_iocb_t>. This list cleared in <processevents_iocontroler>. */
   iocontoler_iocblist_t   changed_list ;
   /* variable: iocbs
    * Contains for every registered filedescriptor the corresponding <iocallback_t>.
    * This info is set by calling <registeriocb_iocontroler> or cleared by calling <unregisteriocb_iocontroler>. */
   struct arraysf_t        * iocbs ;
} ;

// group: lifetime

/* define: iocontroler_INIT_FREEABLE
 * Static initializer. */
#define iocontroler_INIT_FREEABLE      { filedescr_INIT_FREEABLE, 0, 0, memblock_INIT_FREEABLE, slist_INIT, 0 }

/* function: init_iocontroler
 * Allocates system resources for <iocontroler_t>. */
int init_iocontroler(/*out*/iocontroler_t * iocntr) ;

/* function: free_iocontroler
 * Frees all allocated system resources.
 * If this function is called as a reaction during processing of <processevents_iocontroler>
 * EAGAIN is returned and nothing is freed. */
int free_iocontroler(iocontroler_t * iocntr) ;

// group: manage

/* function: registeriocb_iocontroler
 * Registers <iocallback_iot> *iocb* for <filedescr_t> *fd* and monitors for <ioevent_e> *ioevents*.
 * The parameter *ioevent* is the event mask. The parameter *iocb* is the
 * io event handler which is called back if any event occurred whose bit is set in *ioevent*.
 *
 * Calling this function for an already registered descriptor returns EEXIST.
 *
 * List of unmaskable events:
 * The follwoing events can not be removed by clearing their bit
 * in the ioevent parameter and are always monitored.
 *
 * ioevent_ERROR  - Be always prepared to handle error conditions like network failures.
 * ioevent_CLOSE  - Be always prepared that the other side closes the connection.
 *
 * */
int registeriocb_iocontroler(iocontroler_t * iocntr, sys_filedescr_t fd, uint8_t ioevents, struct iocallback_iot iocb) ;

/* function: changemask_iocontroler
 * Changes <ioevent_e> mask *ioevents* the file descriptor fd is monitored for.
 * In case <sys_filedescr_t> *fd* is not registered the error ENOENT is returned. */
int changemask_iocontroler(iocontroler_t * iocntr, sys_filedescr_t fd, uint8_t ioevents) ;

/* function: changeiocb_iocontroler
 * Changes <iocallback_t> *iocb* for <filedescr_t> fd.
 * It returns ENOENT if the filedescriptor *fd* is not registered already. */
int changeiocb_iocontroler(iocontroler_t * iocntr, sys_filedescr_t fd, struct iocallback_iot iocb) ;

/* function: unregisteriocb_iocontroler
 * Unregisters <filedescr_t> *fd*.
 * No more event handler is called for filedescriptor *fd* after return of this call. */
int unregisteriocb_iocontroler(iocontroler_t * iocntr, sys_filedescr_t fd) ;

/* function: processevents_iocontroler
 * Waits *timeout_millisec* milli secconds for events and calls registered <iocallback_iot>.
 * If *timeout_millisec* is set to 0 then this function polls only for any new events
 * but does not wait. If *timeout_millisec* has a value != 0 this function waits until any
 * event occurs but no longer than timeout_millisec milli seconds.
 *
 * After successfull return *nr_events_process* contains the number of occurred events.
 * If an <iocallback_iot> event handler was registered for a filedescriptor
 * it is called with the occurred <ioevent_e> as one of its parameters. */
int processevents_iocontroler(iocontroler_t * iocntr, uint16_t timeout_millisec, /*out*/size_t * nr_events_processed) ;


#endif
