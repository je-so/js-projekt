/* title: IOPoll

   Facility to manage a set of <sys_iochannel_t> and
   to wait and query for IO events of type <ioevent_t>.

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
   (C) 2012 Jörg Seebohn

   file: C-kern/api/io/iopoll.h
    Header file <IOPoll>.

   file: C-kern/platform/Linux/io/iopoll.c
    Linux implementation file <IOPoll Linuximpl>.
*/
#ifndef CKERN_IO_IOPOLL_HEADER
#define CKERN_IO_IOPOLL_HEADER

// forward
struct ioevent_t ;

/* typedef: struct iopoll_t
 * Export <iopoll_t> into global namespace. */
typedef struct iopoll_t                   iopoll_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_iopoll
 * Test <iopoll_t> functionality. */
int unittest_io_iopoll(void) ;
#endif


/* struct: iopoll_t
 * Event manager which stores <sys_iochannel_t> and returns associated <ioevent_t>.
 * It monitors registered file descriptors for one or more <ioevent_e>s.
 * The occurred events can be queried for with help of <wait_iopoll>.
 * A file descriptor can be registered with a call to <registerfd_iopoll>.
 * The underlying system I/O object associated with a file descriptor is the event
 * generating object. */
struct iopoll_t {
   // group: struct fields
   /* variable: sys_poll
    * Handle to the underlying system event queue. */
   sys_iochannel_t   sys_poll ;
} ;

// group: lifetime

/* define: iopoll_FREE
 * Static initializer. */
#define iopoll_FREE { sys_iochannel_FREE }

/* function: init_iopoll
 * Creates system specific event queue to query for io events (<ioevent_t>). */
int init_iopoll(/*out*/iopoll_t * iopoll) ;

/* function: free_iopoll
 * Frees system event queue. */
int free_iopoll(iopoll_t * iopoll) ;

// group: query

/* function: wait_iopoll
 * Waits *timeout_ms* milliseconds for events and returns them.
 * Set *timeout_ms* to 0 if you want to poll only for any new events without waiting.
 *
 * The value queuesize must be in the range (0 < queuesize < INT_MAX).
 *
 * On successfull return nr_events contains the number of events returned in eventqueue.
 *
 * Every bit set in <ioevent_t->ioevents> indicates an occurred event (see <ioevent_e>).
 * The value returned in <ioevent_t->eventid> is the same as set in <registerfd_iopoll> or <updatefd_iopoll>.
 * Use it to associate <ioevent_e> with the corresponding file descriptor.
 *
 * In case there are more events than queuesize only the first queuesize events are returned.
 * The next call returns the remaining events. So events for every file descriptor are reported
 * even if queuesize is too small. */
int wait_iopoll(iopoll_t * iopoll, /*out*/uint32_t * nr_events, uint32_t queuesize, /*out*/struct ioevent_t * eventqueue/*[queuesize]*/, uint16_t timeout_ms) ;

// group: change

/* function: registerfd_iopoll
 * Registers <sys_iochannel_t> *fd* and monitors it for <ioevent_t.ioevents>.
 * The parameter for_event gives the event mask for which the file descriptor is monitored
 * and the id which is returned to be used by the caller to differentiate between the
 * different file descriptors.
 *
 * Calling this function for an already registered descriptor returns EEXIST.
 * Instead use <updatefd_iopoll> to change <ioevent_t> for an already registered descriptor.
 *
 * List of unmaskable events:
 * The follwoing events can not be removed by clearing their bit in the
 * <ioevent_t.ioevents> value of parameter for_event and are always monitored.
 *
 * ioevent_ERROR  - Be always prepared to handle error conditions like network failures or closed pipes.
 * ioevent_CLOSE  - Be always prepared that the other side closes the connection. */
int registerfd_iopoll(iopoll_t * iopoll, sys_iochannel_t fd, const struct ioevent_t * for_event) ;

/* function: updatefd_iopoll
 * Changes <ioevent_t> for an already registered file descriptor. See also <registerfd_iopoll>.
 * In case <sys_iochannel_t> *fd* was not registered before the error ENOENT is returned. */
int updatefd_iopoll(iopoll_t * iopoll, sys_iochannel_t fd, const struct ioevent_t * updated_event) ;

/* function: unregisterfd_iopoll
 * Unregisters <sys_iochannel_t> *fd*.
 * <wait_iopoll> no more reports any event for an unregistered descriptor. */
int unregisterfd_iopoll(iopoll_t * iopoll, sys_iochannel_t fd) ;

#endif
