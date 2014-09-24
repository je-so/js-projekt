/* title: Internetprotocol-Sockets
   ALlows to connect & communicate to other systems via *TCP* or *UDP*.

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

   file: C-kern/api/io/ip/ipsocket.h
    Header file of <Internetprotocol-Sockets>.

   file: C-kern/api/io/ip/ipaddr.h
    Use <Internetprotocol-Address> to construct ip-addresses.

   file: C-kern/platform/Linux/io/ipsocket.c
    Linux specific implementation <Internetprotocol-Sockets Linux>.
*/
#ifndef CKERN_IO_INTERNETPROTOCOL_SOCKET_HEADER
#define CKERN_IO_INTERNETPROTOCOL_SOCKET_HEADER

// forward
struct ipaddr_t ;

/* typedef: ipsocket_t
 * Defines <ipsocket_t> as alias for <sys_iochannel_t>.
 * A socket is the abstraction of a network connection.
 * It is considered as the communication end point of one side.
 * Connected sockets can be considered like files except that
 * written data is transported to another communication endpoint.
 * */
typedef sys_iochannel_t                   ipsocket_t ;

/* typedef: struct ipsocket_async_t
 * Exports <ipsocket_async_t> to asynchronously establish a connection. */
typedef struct ipsocket_async_t           ipsocket_async_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_ipsocket
 * Unittest client/server tcp/udp communications. */
int unittest_io_ipsocket(void) ;
#endif


// struct: ipsocket_t

// group: lifetime

/* define: ipsocket_FREE
 * Static initializer for <ipsocket_t>. Makes calling of <free_ipsocket> safe. */
#define ipsocket_FREE sys_iochannel_FREE

/* function: init_ipsocket
 * Creates a new unconnected UDP network communication endpoint.
 * If localaddr does not describe an <ipprotocol_UDP> address EPROTONOSUPPORT is returned.
 * Unconnected sockets can be used to send and receive to/from a many network peers. */
int init_ipsocket(/*out*/ipsocket_t * ipsock, const struct ipaddr_t * localaddr) ;

/* function: initconnect_ipsocket
 * Creates a connected network communication endpoint.
 * The socket has a local address described by localaddr and tries to establish a connection to
 * a remote peer with its network address stored in remoteaddr.
 * The connection is only established if <ipprotocol_TCP> is used.
 * In case of <ipprotocol_UDP> the socket is configured to allow only to send to and receive from
 * from a network peer with address remoteaddr.
 *
 * EINVAL is returned if localaddr or remoteaddr is invalid or if they have different protocols.
 * Error code EAFNOSUPPORT indicates that <ipversion_e> from localaddr differs from remoteaddr.
 *
 * Special value:
 * In case localaddr is 0 it is set to newany_ipaddr with the same version and protocol as remoteaddr
 * and the port number set to 0 (choose next free number).
 * After successfull connection the local address of the created <ipsocket_t> ipsock is assigned
 * a free ip port and has an ip address of the network interface which connects to remoteaddr.
 *
 * Performance:
 * For TCP it is possible that this call needs some time to complete especially with a slow network connection.
 * So calling this function in a dedicated system worker thread is a good idea. */
int initconnect_ipsocket(/*out*/ipsocket_t * ipsock, const struct ipaddr_t * remoteaddr, const struct ipaddr_t * localaddr/*0=newany_ipaddr*/) ;

/* function: initconnectasync_ipsocket
 * Same as <initconnect_ipsocket> except do not wait until connection is established.
 * If ipsock is ready for writing, the connection has been established or an error occurred.
 * Use <waitconnect_ipsocket> to wait for ipsock to be ready for writing and to return if
 * the connection has been established successfully. */
int initconnectasync_ipsocket(/*out*/ipsocket_t * ipsock, const struct ipaddr_t * remoteaddr, const struct ipaddr_t * localaddr/*0=newany_ipaddr*/);

/* function: initlisten_ipsocket
 * Creates a TCP server socket for accepting connections from peers (clients).
 * The parameter *max_outstanding_connections* sets the number of connections which are established
 * automatically from the underlying operating system without calling <initaccept_ipsocket>.
 * To establish a new connections call <initaccept_ipsocket>.
 *
 * Returns *EOPNOTSUPP* if localaddr uses a protocol other than TCP. */
int initlisten_ipsocket(/*out*/ipsocket_t * ipsock, const struct ipaddr_t * localaddr, uint16_t max_outstanding_connections) ;

/* function: initaccept_ipsocket
 * Waits for an incoming connection request from a peer (client).
 * The parameter listensock must point to an <ipsocket_t> created with <initlisten_ipsocket>.
 * On successfull return the newly established connection is stored in ipsock.
 * The remote peer network address is returned in remoteaddr (if set != 0).
 *
 * Returns:
 * 0            - Success
 * EINVAL       - Parameter listensock is of type <ipprotocol_TCP> but not a listen socket.
 * EOPNOTSUPP   - Parameter listensock is not of type <ipprotocol_TCP> and does not support this operation.
 * EAFNOSUPPORT - Parameter remoteaddr does not have same IP version than listensock. */
int initaccept_ipsocket(/*out*/ipsocket_t * ipsock, ipsocket_t * listensock, struct ipaddr_t * remoteaddr/*0 => ignored*/) ;

/* function: free_ipsocket
 * Closes communication channel and frees system resources. */
int free_ipsocket(/*out*/ipsocket_t * ipsock) ;

// group: async-support

/* function: waitconnect_ipsocket
 * Waits until a socket connected.
 * Use this function only with after <initconnectasync_ipsocket> was called on the socket.
 *
 * Returns:
 * 0    - Connection established.
 * != 0 - Error code, no connection established, call <free_ipsocket>. */
int waitconnect_ipsocket(const ipsocket_t * ipsock);

// group: query

/* function: io_ipsocket
 * Returns the file descriptor <sys_iochannel_t> of the socket. */
sys_iochannel_t io_ipsocket(const ipsocket_t * ipsock) ;

/* function: isconnected_ipsocket
 * Returns if the socket is connected to a peer.
 * In case of an error *false* is returned. */
bool isconnected_ipsocket(const ipsocket_t * ipsock) ;

/* function: islisten_ipsocket
 * Returns true if the socket listens for incoming connections.
 * With this socket type you can wait for incoming connection requests from peers.
 * Use <initaccept_ipsocket> to query for neyl established connections.
 * In case of an error *false* is returned. */
bool islisten_ipsocket(const ipsocket_t * ipsock) ;

/* function: protocol_ipsocket
 * Returns protocol <ipprotocol_e> of the socket.
 * In case of error <ipprotocol_ANY> is returned. */
uint16_t protocol_ipsocket(const ipsocket_t * ipsock) ;

/* function: version_ipsocket
 * Returns protocol version <ipversion_e>.
 * In case of an error <ipversion_ANY> is returned. */
uint16_t version_ipsocket(const ipsocket_t * ipsock) ;

/* function: localaddr_ipsocket
 * Returns local ip address in localaddr.
 * The parameter localaddr must have been allocated with <new_ipaddr> before this function is called.
 * Returns *EAFNOSUPPORT* in case ipversion of localaddr is not the same as of ipsock. */
int localaddr_ipsocket(const ipsocket_t * ipsock, struct ipaddr_t * localaddr) ;

/* function: remoteaddr_ipsocket
 * Returns remote ip address of peer in remoteaddr.
 * The parameter remoteaddr must have been allocated with <new_ipaddr> before this function is called.
 * Returns *EAFNOSUPPORT* in case ipversion of remoteaddr is not the same as of ipsock. */
int remoteaddr_ipsocket(const ipsocket_t * ipsock, struct ipaddr_t * remoteaddr) ;

// group: buffer management

/* function: bytestoread_ipsocket
 * Returns the number of bytes in the receive queue.
 * They can be read without waiting. If you try to read more byte than the returned value
 * in unread_bytes the read operation will not block. But it could be return less than
 * unread_bytes bytes in case socket is of type TCP and a out-of-band (urgent) data byte
 * is stored in the receive queue. */
int bytestoread_ipsocket(const ipsocket_t * ipsock, /*out*/size_t * unread_bytes) ;

/* function: bytestowrite_ipsocket
 * Returns the number of bytes in the send queue.
 * The number of bytes which can be written is the size of the send queue minus the
 * returned value in unsend_bytes. But the write operation is never blocking. It writes only that
 * much bytes as will fit into send queue. */
int bytestowrite_ipsocket(const ipsocket_t * ipsock, /*out*/size_t * unsend_bytes) ;

/* function: queuesize_ipsocket
 * Returns the buffer size in bytes.
 * *readsize* gives the number of bytes which can be received without reading.
 * *writesize* gives the number of bytes which can be written without sending. */
int queuesize_ipsocket(const ipsocket_t * ipsock, /*out*/uint32_t * readsize, /*out*/uint32_t * writesize) ;

/* function: setqueuesize_ipsocket
 * Changes the size of the read and write queue.
 * To change only the read or write queue size set the other value to 0. */
int setqueuesize_ipsocket(ipsocket_t * ipsock, uint32_t queuesize_read/*0 => no change*/, uint32_t queuesize_write/*0 => no change*/) ;

// group: connected io

/* function: read_ipsocket
 * Reads bytes from read queue at max maxdata_len bytes.
 * This call returns EAGAIN if the read queue is empty. It returns less than maxdata_len bytes
 * if the queue contains less bytes. */
int read_ipsocket(ipsocket_t * ipsock, size_t maxdata_len, /*out*/uint8_t data[maxdata_len], /*out*/size_t * bytes_read ) ;

/* function: write_ipsocket
 * Transfers maxdata_len bytes into internal write (send) queue.
 * This call returns EAGAIN if the write queue is full.
 * It writes less than maxdata_len bytes if the queue contains less free space. */
int write_ipsocket(ipsocket_t * ipsock, size_t maxdata_len, const uint8_t data[maxdata_len], /*out*/size_t * bytes_written ) ;

/* function: readoob_ipsocket
 * Same as <read_ipsocket> excepts returns additional OOB index.
 * This index indicates if there is "out of band" data in the stream.
 * *oob_offset* is set to *maxdata_len* if there is no oob data byte in the stream.
 * If there is a oob data byte stored in the read queue
 * *oob_offset* points to it (data[oob_offset]) and has a value smaller than maxdata_len.
 *
 * OODB_INLINE:
 * This option is set during <init_ipsocket> and all oob-data could also be read with a normal <read_ipsocket>.
 * The difference is that <read_ipsocket> returns less data than possible (see <bytestoread_ipsocket>) if it encounters
 * an oob byte. And only the next call to <read_ipsocket> will return the oob byte and any following data.
 * <readoob_ipsocket> will handle this for you and returns an index which indicates if an oob was read. */
int readoob_ipsocket(ipsocket_t * ipsock, size_t maxdata_len, /*out*/uint8_t data[maxdata_len], /*out*/size_t * bytes_read, /*out*/size_t * oob_offset) ;

/* function: writeoob_ipsocket
 * Writes a single byte of "out of band" data (urgent data).
 * This operation is only supported by TCP and on an UDP socket EOPNOTSUPP is returned.
 * If you call it more than once and if the previous oob-data byte was not read by the receiver
 * the previous written oob byte will loose its special oob state and is returned as normal data. */
int writeoob_ipsocket(ipsocket_t * ipsock, uint8_t data ) ;

// group: unconnected io

/* function: readfrom_ipsocket
 * Same as <read_ipsocket> but for unconnected (UDP) sockets.
 * In addition to the read bytes the address of the sender is also returned in remoteaddr.
 * Error code EAFNOSUPPORT is returned in case remoteaddr is not the same <ipversion_e> as <ipsocket_t>. */
int readfrom_ipsocket(ipsocket_t * ipsock, struct ipaddr_t * remoteaddr, size_t maxdata_len, /*out*/uint8_t data[maxdata_len], /*out*/size_t * bytes_read ) ;

/* function: writeto_ipsocket
 * Same as <write_ipsocket> but for unconnected (UDP) sockets.
 * In addition to bytes to write the caller has to supply the network address of the receiver.
 *
 * Returns:
 * 0               - Success.
 * EAFNOSUPPORT    - remoteaddr is not the same <ipversion_e> as <ipsocket_t>.
 * EPROTONOSUPPORT - remoteaddr describes no <ipprotocol_UDP> address.  */
int writeto_ipsocket(ipsocket_t * ipsock, const struct ipaddr_t * remoteaddr, size_t maxdata_len, const uint8_t data[maxdata_len], /*out*/size_t * bytes_written ) ;



// section: inline implementation

/* define: io_ipsocket
 * Implements <ipsocket_t.io_ipsocket>. */
#define io_ipsocket(ipsock)            (*(ipsock))

#endif
