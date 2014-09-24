/* title: IOEvent
   Describes list of possible IO events.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/io/ioevent.h
    Header file <IOEvent>.
*/
#ifndef CKERN_IO_IOEVENT_HEADER
#define CKERN_IO_IOEVENT_HEADER

/* typedef: struct ioevent_t
 * Exports <ioevent_t>. */
typedef struct ioevent_t                  ioevent_t ;

/* typedef: union ioevent_id_t
 * Exports <ioevent_id_t>. */
typedef union ioevent_id_t                ioevent_id_t ;

/* enums: ioevent_e
 * Defines list of IO events. Every events is represented a single bit value
 * and therefore a set of events can be stored as bit vector in an integer of at least 8 bit.
 *
 * ioevent_READ   - You can read from file descriptor.
 *                  It is possible that this bit is set but reading returns 0 bytes read.
 *                  This can happen on a connected network socket.
 *                  In this case the peer has shutdown the connection for writing or the own side
 *                  has shutdown the connection for reading. In both cases you should
 *                  no longer wait for events of type ioevent_READ. In case of <iopoll_t> you
 *                  should call <updatefd_iopoll>.
 * ioevent_WRITE  - You can write to a file descriptor. This bit can also be set even if you can not write
 *                  cause of an error. So check first the <ioevent_ERROR> condition.
 * ioevent_ERROR  - An error condition is signaled on file descriptor.
 *                  This signals that a network error occurred or that you can no longer write
 *                  to a pipe cause the reading side has closed the connection.
 * ioevent_CLOSE  - The peer of a stream oriented connection (pipe, TCP/IP, ...)
 *                  has closed the connection. If <ioevent_READ> is also set you should
 *                  read the data before closing your side of the connection.
 * */
enum ioevent_e {
   ioevent_EMPTY    = 0,
   ioevent_READ     = 1,
   ioevent_WRITE    = 2,
   ioevent_ERROR    = 4,
   ioevent_CLOSE    = 8,

   // contains all allowed bits set (only used in implementation)
   ioevent_MASK     = (ioevent_READ|ioevent_WRITE|ioevent_ERROR|ioevent_CLOSE)
} ;

typedef enum ioevent_e                    ioevent_e ;


/* struct: ioevent_id_t
 * The id value of an event. It is registered together with the corresponding event generating object.
 * Every polled <ioevent_t> contains this id. With this id you can associate events
 * with registered objects. Therefore it should be a unique value.
 * The id can be stored as 3 different types. They use all the same memory address.
 * So only one type at any point in time is supported. */
union ioevent_id_t {
   // group: struct fields
   /* variable: ptr
    * Allows to store the id value as pointer type. */
   void        * ptr ;
   /* variable: val32
    * Allows to store the id value as 32 bit integer type. */
   uint32_t    val32 ;
   /* variable: val64
    * Allows to store the id value as 64 bit integer type. */
   uint64_t    val64 ;
} ;


/* struct: ioevent_t
 * Associates one or more <ioevent_e> with an <ioevent_id_t>.
 * This value is used to twofold. During registering <ioevents> contains the
 * list of event for which the io object is registered. The <eventid> should be
 * set to the unique object id to be able to associate events with corresponding
 * io objects. During polling <ioevents> contains the list of occurred events
 * and <eventid> contains the value which was set during registering. */
struct ioevent_t {
   // group: struct fields
   /* variable: ioevents
    * One or more <ioevent_e> values ored together in one event mask. */
   uint32_t       ioevents ;
   /* variable: eventid
    * Every event has an associated id. It is set by the caller
    * which registers an object, i.e. a <sys_iochannel_t>, with an associated <ioevent_id_t>. */
   ioevent_id_t   eventid ;
} ;

// group: lifetime

/* define: ioevent_INIT_PTR
 * Static initializer. */
#define ioevent_INIT_PTR(ioevents, eventid)      { ioevents, { .ptr = eventid } }

/* define: ioevent_INIT_VAL32
 * Static initializer. */
#define ioevent_INIT_VAL32(ioevents, eventid)    { ioevents, { .val32 = eventid } }

/* define: ioevent_INIT_VAL64
 * Static initializer. */
#define ioevent_INIT_VAL64(ioevents, eventid)    { ioevents, { .val64 = eventid } }

#endif
