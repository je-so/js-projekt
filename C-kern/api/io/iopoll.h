/* title: IOPoll

   Facility to manage a set of <sys_iochannel_t> and
   to wait and query for IO events of type <ioevent_t>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
struct ioevent_t;

/* typedef: struct iopoll_t
 * Export <iopoll_t> into global namespace. */
typedef struct iopoll_t iopoll_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_iopoll
 * Test <iopoll_t> functionality. */
int unittest_io_iopoll(void);
#endif


/* struct: iopoll_t
 * Event manager which stores <sys_iochannel_t> and returns associated <ioevent_t>.
 * It monitors registered file descriptors for one or more <ioevent_e>s.
 * The occurred events can be queried for with help of <wait_iopoll>.
 * A file descriptor can be registered with a call to <register_iopoll>.
 * The underlying system I/O object associated with a file descriptor is the event
 * generating object.
 *
 * Level Triggered:
 * <wait_iopoll> returns after every call all fiuledescriptor which are ready
 * for their registered events. So if you do not need any further notifications for
 * <ioevent_WRITE> for example, call <update_iopoll> to unregister the filedescriptor
 * for this kind of event.
 *
 * Edge Triggered:
 * This event generation policy is currently not supported.
 * Edfge triggered events are only sent once.
 * So if <wait_iopoll> returned an <ioevent_READ> for a filedescriptor the same event
 * is not reported on this filedescriptor the next time <wait_iopoll> is called.
 * Only if new data arrives on the channel another event is generated.
 *
 * */
struct iopoll_t {
   // group: struct fields
   /* variable: sys_poll
    * Handle to the underlying system event queue. */
   sys_iochannel_t   sys_poll;
};

// group: lifetime

/* define: iopoll_FREE
 * Static initializer. */
#define iopoll_FREE { sys_iochannel_FREE }

/* function: init_iopoll
 * Creates system specific event queue to query for io events (<ioevent_t>). */
int init_iopoll(/*out*/iopoll_t * iopoll);

/* function: free_iopoll
 * Frees system event queue. */
int free_iopoll(iopoll_t * iopoll);

// group: query

/* function: wait_iopoll
 * Waits *timeout_ms* milliseconds for events and returns them in eventqueue.
 * Set *timeout_ms* to 0 if you want to poll only for any new events without waiting.
 *
 * The value queuesize must be in the range (0 < queuesize < INT_MAX).
 *
 * Every bit set in <ioevent_t->ioevents> indicates an occurred event (see <ioevent_e>).
 * The value returned in <ioevent_t->eventid> is the same as set in <register_iopoll> or <update_iopoll>.
 * Use it to associate <ioevent_e> with the corresponding file descriptor.
 *
 * In case there are more events than queuesize only the first queuesize events are returned.
 * The next call returns the remaining events. So events for every file descriptor are reported
 * by successive calls to this function even if queuesize is too small.
 *
 * Returns:
 * 0     - OK, nr_events contains the number of events returned in eventqueue. nr_events is always less
 *         or equal than queuesize. In case of a timeout nr_events is set to 0.
 * EINTR - In case of SIGSTOP/SIGCONT or any other called interrupt handler. EINTR is not logged as error code.
 * EBADF - Object iopoll contains is in a freed state.
 * EINVAL - Object iopoll contains a wrong file descriptor. */
int wait_iopoll(iopoll_t * iopoll, /*out*/uint32_t * nr_events, uint32_t queuesize, /*out*/struct ioevent_t * eventqueue/*[queuesize]*/, uint16_t timeout_ms);

// group: change

/* function: register_iopoll
 * Registers <sys_iochannel_t> *fd* and monitors it for <ioevent_t.ioevents>.
 * The parameter for_event gives the event mask for which the file descriptor is monitored
 * and the id which is returned to be used by the caller to differentiate between the
 * different file descriptors.
 *
 * List of unmaskable events:
 * The follwoing events can not be removed by clearing their bit in the
 * <ioevent_t.ioevents> value of parameter for_event and are always monitored.
 *
 * ioevent_ERROR  - Be always prepared to handle error conditions like network failures or closed pipes.
 * ioevent_CLOSE  - Be always prepared that the other side closes the connection.
 *
 * Returns:
 * 0      - fd is registered for <ioevent_t> »for_event«.
 * EPERM  - fd refers to an <iochannel_t> of type directory.
 * EEXIST - Function is called for an already registered descriptor.
 *          Call <update_iopoll> to change <ioevent_t> for an already registered descriptor. */
int register_iopoll(iopoll_t * iopoll, sys_iochannel_t fd, const struct ioevent_t * for_event);

/* function: update_iopoll
 * Changes <ioevent_t> for an already registered file descriptor. See also <register_iopoll>.
 * In case <sys_iochannel_t> *fd* was not registered before the error ENOENT is returned. */
int update_iopoll(iopoll_t * iopoll, sys_iochannel_t fd, const struct ioevent_t * updated_event);

/* function: unregister_iopoll
 * Unregisters <sys_iochannel_t> *fd*.
 * <wait_iopoll> no more reports any event for an unregistered descriptor. */
int unregister_iopoll(iopoll_t * iopoll, sys_iochannel_t fd);

#endif
