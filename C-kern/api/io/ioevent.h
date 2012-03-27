/* title: IOEvent
   Describes list of possible IO events.

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

   file: C-kern/api/io/ioevent.h
    Header file <IOEvent>.
*/
#ifndef CKERN_IO_IOEVENT_HEADER
#define CKERN_IO_IOEVENT_HEADER

/* enums: ioevent_e
 * Defines list of IO events. Every events is represented a single bit value
 * and therefore a set of events can be stored as bit vector in an integer of at least 8 bit.
 *
 * ioevent_READ   - You can read from file descriptor.
 *                  It is possible that this bit is set but reading returns 0 bytes read.
 *                  This can happen on a connected network socket.
 *                  In this case the peer has shutdown the connection for writing or my side
 *                  has shutdown the connection for reading. In both cases you should
 *                  no longer wait for events of type ioevent_READ. In case of <iopoll_t> you
 *                  should call <changefd_iopoll>.
 * ioevent_WRITE  - You can write to file descriptor. This bit can also be set even if you can not write
 *                  cause of an error. So check first the <ioevent_ERROR> condition.
 * ioevent_ERROR  - An error condition is signaled on file descriptor.
 *                  This signals that a network error occurred or that you can no longer write
 *                  to a pipe casue the reading side has closed the connection.
 * ioevent_CLOSE  - The peer of a stream oriented connection (pipe, TCP/IP, ...)
 *                  has closed the connection. If <ioevent_READ> is also set you should
 *                  read the data before closing your side of the connection.
 * */
enum ioevent_e {
    ioevent_EMPTY    = 0
   ,ioevent_READ     = 1
   ,ioevent_WRITE    = 2
   ,ioevent_ERROR    = 4
   ,ioevent_CLOSE    = 8
} ;

typedef enum ioevent_e                 ioevent_e ;


#endif
