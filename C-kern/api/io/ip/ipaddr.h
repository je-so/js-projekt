/* title: Internetprotocol-Address
   - Resolve DNS names into IP adresses

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

   file: C-kern/api/io/ip/ipaddr.h
    Header file of <Internetprotocol-Address>.

   file: C-kern/os/Linux/io/ipaddr.c
    Linux specific implementation <Internetprotocol-Address Linux>.
*/
#ifndef CKERN_IO_INTERNETPROTOCOL_ADDRESS_HEADER
#define CKERN_IO_INTERNETPROTOCOL_ADDRESS_HEADER

#include "C-kern/api/string/cstring.h"

/* typedef: ipport_t
 * The UDP or TCP port number. Declares <ipport_t> as unsigned 16 bit.
 * The port is an id which identifies a running process on a host.
 * To communicate to a process on a remote system you need the
 * IP address of that system, its port and the protocol (UDP or TCP).
 *
 * Every application on a host must have a unique port number in the protocol domain
 * to be unambiguously identifiable by the system network layer.
 * If you do not know the correct port number but the service name use <initnamed_ipport>
 * first to determine it. */
typedef uint16_t                       ipport_t ;

/* typedef: struct ipaddr_t
 * Export <ipaddr_t>. */
typedef struct ipaddr_t                ipaddr_t ;

/* typedef: struct ipaddr_list_t
 * Export <ipaddr_list_t>. */
typedef struct ipaddr_list_t           ipaddr_list_t ;

/* enums: ipversion_e
 * Selects the version of the ip address.
 *
 * ipversion_ANY - Only useful as a filter argument to <newdnsquery_ipaddrlist>.
 *                 It means to return appropriate addresses for IPv4 and IPv6.
 * ipversion_4   - Selects IP version 4 address types of the form "A.B.C.D", e.g. "192.168.2.1".
 *                 They are stored as a 32 bit value.
 *                 This value is assigned the system specific value AF_INET.
 * ipversion_6   - Selects IP version 6 address types of the form "AABB:CCDD::XXYY" .
 *                 They are stored as a 128 bit value.
 *                 This value is assigned the system specific value AF_INET6.
 * */
enum ipversion_e {
    ipversion_ANY = AF_UNSPEC
   ,ipversion_4   = AF_INET
   ,ipversion_6   = AF_INET6
} ;
typedef enum ipversion_e               ipversion_e ;

/* enums: ipprotocol_e
 * Currently supported internet protocols.
 *
 * ipprotocol_ANY - Only useful as a filter argument to <newdnsquery_ipaddrlist>. It means to return
 *                  appropriate addresses for UDP and/or TCP protocols.
 *                  This value is assigned the system specific value IPPROTO_IP.
 * ipprotocol_TCP - Use reliable transmission control protocol to send/receive messages.
 *                  This value is assigned the system specific value IPPROTO_TCP.
 * ipprotocol_UDP - Use unreliable datagram protocol to send/receive messages.
 *                  This value is assigned the system specific value IPPROTO_UDP.
 * */
enum ipprotocol_e {
    ipprotocol_ANY = 0
   ,ipprotocol_TCP = IPPROTO_TCP
   ,ipprotocol_UDP = IPPROTO_UDP
} ;
typedef enum ipprotocol_e              ipprotocol_e ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_ipaddr
 * Unittest for resolving dns names into ip addresses. */
extern int unittest_io_ipaddr(void) ;
#endif


// section: ipport_t

// group: lifetime

/* define: ipport_ANY
 * System chooses free port number for you.
 * If you assign <ipport_ANY> to your own port number
 * the system chooses the next free port number > 0 for you
 * if you create an <ipsocket_t>. */
#define ipport_ANY                     0

/* function: initnamed_ipport
 * Returns the tcp and udp port numbers of a named IP service.
 * A returned value of 0 in tcp_port or udp_port means this kind of protocol is not supported
 * by the service. You can set tcp_port or udp_port to NULL if you want to query only for one protocol.
 *
 * Returned error codes:
 * ENOENT          - Service name does not exist.
 * EPROTONOSUPPORT - UDP or TCP protocol is not supported by service.
 * */
extern int initnamed_ipport(/*out*/ipport_t * port, const char * servicename, ipprotocol_e protocol) ;


/* struct: ipaddr_t
 * Stores description of an internet protocol address.
 * This object supports handling of IPv4 and IPv6 addresses.
 *
 * The structure of an IP address:
 * version        - Stores the protocol version; see <ipversion_e>.
 * protocol       - Stores the protocol; see <ipprotocol_e>.
 * port           - Stores the 16 bit port number.
 * internet addr. - IPv4 (32 bit) or IPv6 (128 bit) number */
struct ipaddr_t {
   /* variable: protocol
    * Indicates a specific internet protocol.
    * See <ipprotocol_e> for a list of possible values. */
   uint16_t          protocol ;
   /* variable: addrlen
    * Length of *internal* address representation <addr>. */
   uint16_t          addrlen ;
   /* variable: addr
    * Opaque representation of internet address (IPv4 or IPv6).
    * The size in bytes is stored in <addrlen>. */
   sys_socketaddr_t  addr[] ;
} ;

// group: lifetime

/* function: new_ipaddr
 * Create a new internet address and init it with the given values.
 * If you do not longer need it do not forget to free it with <delete_ipaddr>.
 *
 * Parameter:
 * protocol     - Either <ipprotocol_UDP> or <ipprotocol_TCP>.
 * numeric_addr - The numeric representation of the internet address ( "200.123.56.78" or "::1" ...)
 * port         - A port which identifies this process between all processes on this network node.
 *                If set to <ipport_ANY> (== 0) a free port is randomly chosen by the system during socket creation.
 * version      - IP version: Either <ipversion_4> or <ipversion_6>. */
extern int new_ipaddr(/*out*/ipaddr_t ** addr, ipprotocol_e protocol, const char * numeric_addr, ipport_t port, ipversion_e version) ;

/* function: newdnsquery_ipaddr
 * Resolves a hostname into its first queried ip address. */
extern int newdnsquery_ipaddr(/*out*/ipaddr_t ** addr, ipprotocol_e protocol, const char * hostname, ipport_t port, ipversion_e version) ;

/* function: newaddr_ipaddr
 * Create a new internet address and init it a system specific socket address value.
 * Same as <new_ipaddr> except that *port*, *numeric_address* and *version* are represented by system specific
 * type *sys_socketaddr_t*. If you do not longer need it do not forget to free it with <delete_ipaddr>. */
extern int newaddr_ipaddr(/*out*/ipaddr_t ** addr, ipprotocol_e protocol, uint16_t sock_addr_len, const sys_socketaddr_t * sock_addr) ;

/* function: newany_ipaddr
 * Create a new internet address suitable to listen on any network interface.
 * Same as <new_ipaddr> except that numeric_addr is set to "0.0.0.0" or "::" depending on the protocol version.
 * If you do not longer need it do not forget to free it with <delete_ipaddr>. */
extern int newany_ipaddr(/*out*/ipaddr_t ** addr, ipprotocol_e protocol, ipport_t port, ipversion_e version) ;

/* function: newloopback_ipaddr
 * Returns an ip address suitable for host only inter process communications.
 * The loopback address allows to send messages between processes on the same local system (host).
 * Connection to remote network nodes are not supported.
 * Same as <new_ipaddr> except that numeric_addr is set to the loopback address
 * "127.0.0.1" for IPv4 or "::1" for IPv6. */
extern int newloopback_ipaddr(/*out*/ipaddr_t ** addr, ipprotocol_e protocol, ipport_t port, ipversion_e version) ;

/* function: newcopy_ipaddr
 * Copy an internet address to store it for later usage.
 * If you do no longer need it do not forget to free it with <delete_ipaddr>. */
extern int newcopy_ipaddr(/*out*/ipaddr_t ** dest, const ipaddr_t * source) ;

/* function: delete_ipaddr
 * Deletes a created or copied address.
 * Never free an address returned by <ipaddr_list_t>. */
extern int delete_ipaddr(ipaddr_t ** addr) ;

// group: query

/* function: compare_ipaddr
 * Returns result of comparison (0 == equal).
 *
 * Returns:
 *  < 0  - left  < right
 * == 0  - left == right
 *  > 0  - left  > right */
extern int compare_ipaddr(const ipaddr_t * left, const ipaddr_t * right) ;

/* function: isvalid_ipaddr
 * Checks that internal fields are ok. */
extern bool isvalid_ipaddr(const ipaddr_t * addr) ;

/* function: port_ipaddr
 * Returns the port number of the address. */
extern ipport_t port_ipaddr(const ipaddr_t * addr) ;

/* variable: protocol_ipaddr
 * Returns the specified protocol for this ip address.
 * See also <ipprotocol_e>.
 *
 * Return Values:
 * ipprotocol_TCP  - Connection oriented protocol
 * ipprotocol_UDP  - Connectionless protocol */
extern ipprotocol_e protocol_ipaddr(const ipaddr_t * addr) ;

/* function: version_ipaddr
 * Returns the supported version of the address.
 * This value can not be changed after creation.
 *
 * Returned Values:
 * ipversion_4 - Internet Protocol version 4.
 * ipversion_6 - Internet Protocol version 6. */
extern ipversion_e version_ipaddr(const ipaddr_t * addr) ;

/* function: dnsname_ipaddr
 * Tries a reverse mapping from a binary ip address into its dns name representation.
 * The returned name is converted into the current character encoding in case the returned name is an ACE
 * encoded IDN (internat. domain name). ACE stands for ASCII Compatible Encoding and is used to represent an IDN
 * which contains unicode characters.
 *
 * The name is returned in dns_name. This string must in a initialized state. Previous content is overwritten.
 *
 * See:
 * <RFC: Internationalizing Domain Names in Applications at http://www.ietf.org/rfc/rfc3490.txt> */
extern int dnsname_ipaddr(const ipaddr_t * addr, cstring_t * dns_name) ;

/* function: dnsnameace_ipaddr
 * Tries a reverse mapping from a binary ip address into its dns name representation.
 * Same as <dnsname_ipaddr> except that IDN represented in ACE are left untouched (not converted into unicode).
 * The name is returned in dns_name. This string must in a initialized state. Previous content is overwritten.
 * */
extern int dnsnameace_ipaddr(const ipaddr_t * addr, cstring_t * dns_name) ;

/* function: numericname_ipaddr
 * Returns the numeric ascii representation of the ip address.
 * The returned names are composed of four decimal numbers for IPv4, i.e. '192.168.20.10',
 * or 8 16bit hexadecimal numbers separated by ':' for IPv6, i.e. '2010:0dcc:3543:0000:0000:4e9f:0370:2668'.
 * The name is returned in numeric_name. This string must in a initialized state. Previous content is overwritten.
 * */
extern int numericname_ipaddr(const ipaddr_t * addr, cstring_t * numeric_name) ;

// group: change

/* function: copy_ipaddr
 * Copies the ip number, protocol and port from source to dest.
 * If version of source is not the same as of dest EAFNOSUPPORT is returned. */
extern int copy_ipaddr(ipaddr_t * dest, const ipaddr_t * source) ;

/* function: setprotocol_ipaddr
 * Changes ip address' protocol value.
 * Returns 0 in case of success else EINVAL if protocol is not a value from <ipprotocol_e>. */
extern int setprotocol_ipaddr(ipaddr_t * addr, ipprotocol_e protocol) ;

/* function: setport_ipaddr
 * Changes ip address' port value. */
extern int setport_ipaddr(ipaddr_t * addr, ipport_t port) ;

/* function: setaddr_ipaddr
 * Sets the ip number, protocol and port.
 * If version of sock_addr is not the same as of addr EAFNOSUPPORT is returned. */
extern int setaddr_ipaddr(ipaddr_t * addr, ipprotocol_e protocol, uint16_t sock_addr_len, const sys_socketaddr_t * sock_addr) ;


// section: ipaddr_list_t

// group: lifetime

/* function: newdnsquery_ipaddrlist
 * Resolves a host name into a list of internet addresses.
 *
 * Returned error codes:
 * EADDRNOTAVAIL   - IP version for numeric name not supported (name == "127.0.0.1" && version == ipversion_6)
 * EAFNOSUPPORT    - Value in version is not supported.
 * EPROTONOSUPPORT - Value in protocol is not supported.
 * ENOENT          - The dns name is not known.
 * ENODATA         - No data received (timeout) or the specified network host exists, but does not have any network addresses defined.
 * */
extern int newdnsquery_ipaddrlist(/*out*/ipaddr_list_t ** addrlist, const char * hostname_or_numeric, ipprotocol_e protocol, ipport_t port, ipversion_e version) ;

/* function: delete_ipaddrlist
 * Frees memory of internal list of <ipaddr_list_t>. Calling it more than once is safe. */
extern int delete_ipaddrlist(ipaddr_list_t ** addrlist) ;

// group: read

/* function: gofirst_ipaddrlist
 * Resets iterator to first <ipaddr_t> of internal list of addresses.
 * After calling this function the next call to <next_ipaddrlist> returns the first address in the buffer. */
extern void gofirst_ipaddrlist(ipaddr_list_t * addrlist) ;

/* function: next_ipaddrlist
 * Returns next <ipaddr_t> from of internal list.
 * Only valid until next call to <next_ipaddrlist> or <delete_ipaddrlist>. */
extern const ipaddr_t * next_ipaddrlist(ipaddr_list_t * addrlist) ;


// section: inline implementations


#endif

