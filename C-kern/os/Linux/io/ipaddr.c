/* title: Internetprotocol-Address Linux
   Implements <Internetprotocol-Address> on Linux.

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

#include "C-kern/konfig.h"
#include "C-kern/api/io/ip/ipaddr.h"
#include "C-kern/api/err.h"
#include "C-kern/api/math/int/signum.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif

#undef ntohs // use unoptimized ntohs to suppress warning in gcc in case of optimization on
#undef htons // use unoptimized htons to suppress warning in gcc in case of optimization on

#define ipaddr_MAXSIZE  (sizeof(ipaddr_t) + sizeof(struct sockaddr_in6))

// section: ipaddr_t

// group: helper

static inline void compiletime_tests(void)
{
   // "check that ipversion_e is defined correctly"
   static_assert((int)ipversion_ANY == 0, ) ;
   static_assert((int)ipversion_4   == AF_INET, ) ;
   static_assert((int)ipversion_6   == AF_INET6, ) ;
   // "check that ipprotocol_e is defined correctly"
   static_assert((int)ipprotocol_ANY == 0, ) ;
   static_assert((int)ipprotocol_TCP == IPPROTO_TCP, ) ;
   static_assert((int)ipprotocol_UDP == IPPROTO_UDP, ) ;
   // ipaddr_t->family is of type uint16_t
   static_assert( 0 <= AF_INET && AF_INET < (uint16_t)-1, ) ;
   static_assert( 0 <= AF_INET6 && AF_INET6 < (uint16_t)-1, ) ;
   // ipaddr_t->protocol is of type uint16_t
   static_assert( 2 == sizeof(((struct sockaddr_in*)0)->sin_port), ) ;
   static_assert( 0 <  (typeof(((struct sockaddr_in*)0)->sin_port))-1, ) ;
   static_assert( 0 <= IPPROTO_UDP && IPPROTO_UDP < (uint16_t)-1, ) ;
   static_assert( 0 <= IPPROTO_UDP && IPPROTO_UDP < (uint16_t)-1, ) ;
   static_assert( 0 <= IPPROTO_TCP && IPPROTO_TCP < (uint16_t)-1, ) ;
   static_assert( 0 <= IPPROTO_IP  && IPPROTO_IP  < (uint16_t)-1, ) ;
   // code is same for both address families
   static_assert( &((struct sockaddr_in*)0)->sin_family == &((sys_socketaddr_t*)0)->sa_family, ) ;
   static_assert( &((struct sockaddr_in*)0)->sin_family == &((struct sockaddr_in6*)0)->sin6_family, ) ;
   static_assert( &((struct sockaddr_in*)0)->sin_port == &((struct sockaddr_in6*)0)->sin6_port, ) ;
   static_assert( &((struct sockaddr_in*)0)->sin_addr != (void*)&((struct sockaddr_in6*)0)->sin6_addr, ) ;
   // addrlen fits in uint16_t
   static_assert( sizeof(struct sockaddr_in) <  sizeof(struct sockaddr_in6), ) ;
   static_assert( ipaddr_MAXSIZE <= 256, ) ;
   static_assert( sizeof(ipaddr_t) + sizeof(struct sockaddr_in6) == ipaddr_MAXSIZE, ) ;

}

static int convert_eai_errorcodes(int err)
{
   switch(err) {
   case EAI_ADDRFAMILY: // Address family for name not supported
      err = EADDRNOTAVAIL ;
      break ;
   case EAI_AGAIN:      // The name server returned a temporary failure indication.  Try again later.
      err = EAGAIN ;
      break ;
   case EAI_BADFLAGS:   // hints.ai_flags  contains  invalid  flags;  or, hints.ai_flags included AI_CANONNAME and name was NULL.
      err = EINVAL ; // should never occur
      break ;
   case EAI_FAIL:    // The name server returned a permanent failure indication.
      err = ECONNRESET ;
      break ;
   case EAI_FAMILY:  // The requested address family is not supported.
      err = EAFNOSUPPORT ;
      break ;
   case EAI_MEMORY:  // Out of memory.
      err = ENOMEM ;
      break ;
   case EAI_NODATA:  // The specified network host exists, but does not have any network addresses defined.
                     // Or no data is sent back.
      err = ENODATA ;
      break ;
   case EAI_NONAME:  // The node or service is not known; or both node and service are NULL; or AI_NUMERICSERV was
                     // specified in hints.ai_flags and service was not a numeric port-number string.
      err = ENOENT ;
      break ;
   case EAI_OVERFLOW:   // buffer overflow getnameinfo
      err = ENOMEM ;
      break ;
   case EAI_SERVICE: // The  requested  service/protocol is  not  available  for the requested socket type.  It may be available
                     // through another socket type.  For example, this error could occur if service was "shell" (a
                     // service  only  available  on  stream  sockets),  and  either  hints.ai_protocol was IPPROTO_UDP, or
                     // hints.ai_socktype was SOCK_DGRAM; or the  error  could  occur  if  service  was  not  NULL,  and
                     // hints.ai_socktype was SOCK_RAW (a socket type that does not support the concept of services).
      err = EPROTONOSUPPORT ;
      break ;
   case EAI_SOCKTYPE:      // socket type / protocol type mismatch
      err = EPROTOTYPE ;   // should never occur (cause ai_socktype == 0)
      break ;
   case EAI_SYSTEM:  // Other system error, check errno for details.
      err = errno ;
      break ;
   }
   if (err <= 0) err = EINVAL ;
   return err ;
}

static int new_addrinfo(struct addrinfo ** addrinfo_list, const char * name_or_numeric, int flags, ipprotocol_e protocol, ipport_t port, ipversion_e version)
{
   int err ;
   struct addrinfo   filter ;
   char              portstr[10] ;

   memset(&filter, 0, sizeof(filter)) ;
   filter.ai_family   = version ;
   filter.ai_protocol = protocol ;
   filter.ai_flags    = AI_NUMERICSERV | flags ;

   snprintf(portstr, sizeof(portstr), "%u", port) ;

   err = getaddrinfo(name_or_numeric, portstr, &filter, /*result*/addrinfo_list) ;
   if (err) {
      *addrinfo_list = 0 ;
      err = convert_eai_errorcodes(err) ;
      LOG_SYSERR("getaddrinfo", err) ;
      LOG_STRING(name_or_numeric) ;
      goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

static void delete_addrinfo(struct addrinfo ** addrinfo_list)
{
   if (*addrinfo_list) {
      freeaddrinfo(*addrinfo_list) ;
      *addrinfo_list = 0 ;
   }
}

// group: implementation

int new_ipaddr(/*out*/ipaddr_t ** addr, ipprotocol_e protocol, const char * numeric_addr, ipport_t port, ipversion_e version)
{
   int err ;
   ipaddr_t * new_addr = 0 ;

   PRECONDITION_INPUT(0 == (*addr), ABBRUCH, ) ;
   PRECONDITION_INPUT(numeric_addr, ABBRUCH, ) ;

   if (protocol != ipprotocol_TCP && protocol != ipprotocol_UDP) {
      err = EPROTONOSUPPORT ;
      goto ABBRUCH ;
   }

   if (version != ipversion_4 && version != ipversion_6) {
      err = EAFNOSUPPORT ;
      goto ABBRUCH ;
   }

   size_t size = sizeof(ipaddr_t) + ((ipversion_4 == version) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6)) ;

   new_addr = (ipaddr_t*) malloc(size) ;
   if (!new_addr) {
      LOG_OUTOFMEMORY(size) ;
      err = ENOMEM ;
      goto ABBRUCH ;
   }

   memset(new_addr, 0, size) ;
   new_addr->protocol = protocol ;
   new_addr->addrlen  = (uint16_t) (size - sizeof(ipaddr_t)) ;
   ((struct sockaddr_in*)new_addr->addr)->sin_family = version ;
   ((struct sockaddr_in*)new_addr->addr)->sin_port   = htons(port) ;
   if (ipversion_4 == version) {
      if (1 != inet_pton(version, numeric_addr, &((struct sockaddr_in*)new_addr->addr)->sin_addr)) {
         err = EADDRNOTAVAIL ;
         goto ABBRUCH ;
      }
   } else {
      if (1 != inet_pton(version, numeric_addr, &((struct sockaddr_in6*)new_addr->addr)->sin6_addr)) {
         err = EADDRNOTAVAIL ;
         goto ABBRUCH ;
      }
   }

   *addr = new_addr ;
   return 0 ;
ABBRUCH:
   delete_ipaddr(&new_addr) ;
   LOG_ABORT(err) ;
   return err ;
}

int newdnsquery_ipaddr(/*out*/ipaddr_t ** addr, ipprotocol_e protocol, const char * hostname, ipport_t port, ipversion_e version)
{
   int err ;
   struct addrinfo * addrinfo_list = 0 ;

   PRECONDITION_INPUT(0 == (*addr), ABBRUCH, ) ;
   PRECONDITION_INPUT(hostname, ABBRUCH, ) ;

   if (protocol != ipprotocol_TCP && protocol != ipprotocol_UDP) {
      err = EPROTONOSUPPORT ;
      goto ABBRUCH ;
   }

   if (version != AF_INET && version != AF_INET6) {
      err = EAFNOSUPPORT ;
      goto ABBRUCH ;
   }

   err = new_addrinfo(&addrinfo_list, hostname, AI_IDN|AI_IDN_ALLOW_UNASSIGNED, protocol, port, version) ;
   if (err) goto ABBRUCH ;

   if (addrinfo_list->ai_addrlen >= 256) {
      err = EINVAL ;
      goto ABBRUCH ;
   }

   err = newaddr_ipaddr(addr, (ipprotocol_e)addrinfo_list->ai_protocol, (uint16_t)addrinfo_list->ai_addrlen, addrinfo_list->ai_addr) ;
   if (err) goto ABBRUCH ;

   delete_addrinfo(&addrinfo_list) ;

   return 0 ;
ABBRUCH:
   delete_addrinfo(&addrinfo_list) ;
   LOG_ABORT(err) ;
   return err ;
}

int newaddr_ipaddr(/*out*/ipaddr_t ** addr, ipprotocol_e protocol, uint16_t sock_addr_len, const sys_socketaddr_t * sock_addr)
{
   int err ;
   ipaddr_t * new_addr = 0 ;

   PRECONDITION_INPUT(0 == (*addr), ABBRUCH, ) ;
   PRECONDITION_INPUT(sock_addr, ABBRUCH, ) ;

   if (protocol != ipprotocol_TCP && protocol != ipprotocol_UDP) {
      err = EPROTONOSUPPORT ;
      goto ABBRUCH ;
   }

   if (  (     (sizeof(struct sockaddr_in) != sock_addr_len)
            || (AF_INET != sock_addr->sa_family))
         && (  (sizeof(struct sockaddr_in6) != sock_addr_len)
            || (AF_INET6 != sock_addr->sa_family))) {
      err = EAFNOSUPPORT ;
      goto ABBRUCH ;
   }

   size_t size = sizeof(ipaddr_t) + sock_addr_len ;

   new_addr = (ipaddr_t*) malloc(size) ;
   if (!new_addr) {
      LOG_OUTOFMEMORY(size) ;
      err = ENOMEM ;
      goto ABBRUCH ;
   }

   new_addr->protocol = protocol ;
   new_addr->addrlen  = sock_addr_len ;
   memcpy( new_addr->addr, sock_addr, sock_addr_len ) ;

   *addr = new_addr ;
   return 0 ;
ABBRUCH:
   delete_ipaddr(&new_addr) ;
   LOG_ABORT(err) ;
   return err ;
}

int newany_ipaddr(/*out*/ipaddr_t ** addr, ipprotocol_e protocol, ipport_t port, ipversion_e version)
{
   return new_ipaddr(addr, protocol, version == ipversion_4 ? "0.0.0.0" : "::", port, version) ;
}

int newloopback_ipaddr(/*out*/ipaddr_t ** addr, ipprotocol_e protocol, ipport_t port, ipversion_e version)
{
   return new_ipaddr(addr, protocol, version == ipversion_4 ? "127.0.0.1" : "::1", port, version) ;
}

int newcopy_ipaddr(/*out*/ipaddr_t ** dest, const ipaddr_t * source)
{
   int err ;

   PRECONDITION_INPUT(0 == *dest, ABBRUCH, ) ;
   PRECONDITION_INPUT(isvalid_ipaddr(source), ABBRUCH, ) ;

   err = newaddr_ipaddr(dest, source->protocol, source->addrlen, source->addr) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int delete_ipaddr(ipaddr_t ** addr)
{
   ipaddr_t * delobj = *addr ;

   if (delobj) {
      *addr = 0 ;

      free(delobj) ;
   }

   return 0 ;
}

int compare_ipaddr(const ipaddr_t * left, const ipaddr_t * right)
{
   if (left && right) {
      {
         int proto1 = left->protocol ;
         int proto2 = right->protocol ;
         if (proto1 != proto2) return signum(proto1 - proto2) ;
      }
      {
         int alen1 = left->addrlen ;
         int alen2 = right->addrlen ;
         if (alen1 != alen2) return signum(alen1 - alen2) ;
      }
      {
         int family1 = ((const struct sockaddr_in*)(left->addr))->sin_family ;
         int family2 = ((const struct sockaddr_in*)(right->addr))->sin_family ;
         if (family1 != family2) return signum(family1 - family2) ;
      }
      {
         int port1 = ((const struct sockaddr_in*)(left->addr))->sin_port ;
         int port2 = ((const struct sockaddr_in*)(right->addr))->sin_port ;
         if (port1 != port2) return signum(port1 - port2) ;
      }

      size_t addrlen = left->addrlen ;
      return memcmp(left->addr, right->addr, addrlen) ;
   } else {
      if (left < right)       return -1 ;
      else if (left > right)  return +1 ;
   }
   return 0 ;
}

bool isvalid_ipaddr(const ipaddr_t * addr)
{
   if (  addr
      && (     ipprotocol_UDP == addr->protocol
            || ipprotocol_TCP == addr->protocol)
      && (     (     sizeof(struct sockaddr_in)  == addr->addrlen
                  && ipversion_4 == version_ipaddr(addr))
            || (     sizeof(struct sockaddr_in6) == addr->addrlen
                  && ipversion_6 == version_ipaddr(addr))))  {
      return true ;
   }

   return false ;
}

ipport_t port_ipaddr(const ipaddr_t * addr)
{
   uint16_t port = ((const struct sockaddr_in*)(addr->addr))->sin_port ;
   return ntohs(port) ;
}

ipprotocol_e protocol_ipaddr(const ipaddr_t * addr)
{
   return addr->protocol ;
}

ipversion_e version_ipaddr(const ipaddr_t * addr)
{
   uint16_t family = ((const struct sockaddr_in*)(addr->addr))->sin_family ;
   return (ipversion_e) family ;
}

int dnsname_ipaddr(const ipaddr_t * addr, cstring_t * dns_name)
{
   int err ;

   err = allocate_cstring(dns_name, NI_MAXHOST) ;
   if (err) goto ABBRUCH ;

   for(;;) {
      err = getnameinfo( addr->addr, addr->addrlen, dns_name->chars, dns_name->allocated_size, 0, 0, NI_IDN|NI_IDN_ALLOW_UNASSIGNED|NI_NAMEREQD) ;
      if (err) {
         if (     EAI_OVERFLOW == err
               && dns_name->allocated_size < 4096) {
            err = allocate_cstring(dns_name, 2*dns_name->allocated_size) ;
            if (err) goto ABBRUCH ;
            continue ;
         }
         err = convert_eai_errorcodes(err) ;
         goto ABBRUCH ;
      }
      break ;
   }

   adaptlength_cstring(dns_name) ;
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int dnsnameace_ipaddr(const ipaddr_t * addr, cstring_t * dns_name)
{
   int err ;

   err = allocate_cstring(dns_name, NI_MAXHOST) ;
   if (err) goto ABBRUCH ;

   for(;;) {
      err = getnameinfo( addr->addr, addr->addrlen, dns_name->chars, dns_name->allocated_size, 0, 0, NI_NAMEREQD) ;
      if (err) {
         if (     EAI_OVERFLOW == err
               && dns_name->allocated_size < 4096) {
            err = allocate_cstring(dns_name, 2*dns_name->allocated_size) ;
            if (err) goto ABBRUCH ;
            continue ;
         }
         err = convert_eai_errorcodes(err) ;
         goto ABBRUCH ;
      }
      break ;
   }

   adaptlength_cstring( dns_name ) ;
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int numericname_ipaddr(const ipaddr_t * addr, cstring_t * numeric_name)
{
   int err ;

   err = allocate_cstring(numeric_name, 32) ;
   if (err) goto ABBRUCH ;

   for(;;) {
      err = getnameinfo( addr->addr, addr->addrlen, numeric_name->chars, numeric_name->allocated_size, 0, 0, NI_NUMERICHOST) ;
      if (err) {
         if (     EAI_OVERFLOW == err
               && numeric_name->allocated_size < 4096) {
            err = allocate_cstring(numeric_name, 2*numeric_name->allocated_size) ;
            if (err) goto ABBRUCH ;
            continue ;
         }
         err = convert_eai_errorcodes(err) ;
         goto ABBRUCH ;
      }
      break ;
   }

   adaptlength_cstring( numeric_name ) ;
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int copy_ipaddr(ipaddr_t * dest, const ipaddr_t * source)
{
   int err ;

   PRECONDITION_INPUT( isvalid_ipaddr(source), ABBRUCH, ) ;

   if (  source->addrlen        != dest->addrlen
      || version_ipaddr(source) != version_ipaddr(dest)) {
      err = EAFNOSUPPORT ;
      goto ABBRUCH ;
   }

   dest->protocol = source->protocol ;
   memcpy(dest->addr, source->addr, dest->addrlen) ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int setprotocol_ipaddr(ipaddr_t * addr, ipprotocol_e protocol)
{
   int err ;

   if (protocol != ipprotocol_TCP && protocol != ipprotocol_UDP) {
      err = EPROTONOSUPPORT ;
      goto ABBRUCH ;
   }

   addr->protocol = protocol ;
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int setport_ipaddr(ipaddr_t * addr, ipport_t port)
{
   int err ;

   PRECONDITION_INPUT( isvalid_ipaddr(addr), ABBRUCH, ) ;

   ((struct sockaddr_in*)addr->addr)->sin_port = htons(port) ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int setaddr_ipaddr(ipaddr_t * addr, ipprotocol_e protocol, uint16_t sock_addr_len, const sys_socketaddr_t * sock_addr)
{
   int err ;

   PRECONDITION_INPUT( isvalid_ipaddr(addr), ABBRUCH, ) ;

   if (protocol != ipprotocol_TCP && protocol != ipprotocol_UDP) {
      err = EPROTONOSUPPORT ;
      goto ABBRUCH ;
   }

   if (  addr->addrlen != sock_addr_len
      || version_ipaddr(addr) != (sock_addr)->sa_family) {
      err = EAFNOSUPPORT ;
      goto ABBRUCH ;
   }

   addr->protocol = protocol ;
   memcpy(addr->addr, sock_addr, sock_addr_len) ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}


/* struct: ipaddr_list_t
 * Stores list of <ipaddr_t>. Allows to iterate ovr every entry. */
struct ipaddr_list_t
{
   /* variable: current_addr
    * Points to <ipaddr_t> storage used to return next address. */
   struct ipaddr_t * current ;
   /* variable: first
    * Points to start of list. This is the system specific type *struct addrinfo*. */
   struct addrinfo * first ;
   /* variable: next
    * Points in list. Indicates which address to return as next. */
   struct addrinfo * next ;
   uint8_t           storage[ipaddr_MAXSIZE] ;
} ;

// group: implementation

int newdnsquery_ipaddrlist(/*out*/ipaddr_list_t ** addrlist, const char * hostname_or_numeric, ipprotocol_e protocol, ipport_t port, ipversion_e version)
{
   int err ;
   struct addrinfo * addrinfo_list = 0 ;
   ipaddr_list_t   * new_addrlist ;

   new_addrlist = (ipaddr_list_t*) malloc(sizeof(ipaddr_list_t)) ;
   if (!new_addrlist) {
      LOG_OUTOFMEMORY(sizeof(ipaddr_list_t)) ;
      err = ENOMEM ;
      goto ABBRUCH ;
   }

   err = new_addrinfo(&addrinfo_list, hostname_or_numeric, AI_IDN|AI_IDN_ALLOW_UNASSIGNED, protocol, port, version) ;
   if (err) goto ABBRUCH ;

   memset(new_addrlist, 0, sizeof(*new_addrlist)) ;
   new_addrlist->current = (ipaddr_t*) new_addrlist->storage ;
   new_addrlist->first   = addrinfo_list ;
   new_addrlist->next    = addrinfo_list ;

   *addrlist = new_addrlist ;
   return 0 ;
ABBRUCH:
   free(new_addrlist) ;
   LOG_ABORT(err) ;
   return err ;
}

int delete_ipaddrlist(ipaddr_list_t ** addrlist)
{
   ipaddr_list_t * delobj = *addrlist ;

   if (delobj) {
      *addrlist = 0 ;

      delete_addrinfo(&delobj->first) ;
      delobj->current = 0 ;
      delobj->next    = 0 ;

      free(delobj) ;
   }

   return 0 ;
}

void gofirst_ipaddrlist(ipaddr_list_t * addrlist)
{
   addrlist->next = addrlist->first ;
}

const ipaddr_t * next_ipaddrlist(ipaddr_list_t * addrlist)
{
   struct addrinfo * next ;

   do {
      next = addrlist->next ;

      if (!next) {
         return 0 ;
      }

      // move forward
      addrlist->next = next->ai_next ;
   } while (      IPPROTO_IP == next->ai_protocol
               || (     sizeof(struct sockaddr_in)  != next->ai_addrlen
                     && sizeof(struct sockaddr_in6) != next->ai_addrlen ) ) ;

   // prepare content of cached ipaddr_t
   addrlist->current->protocol = (uint16_t) next->ai_protocol ;
   addrlist->current->addrlen  = (uint16_t) next->ai_addrlen ;
   memcpy(addrlist->current->addr, next->ai_addr, next->ai_addrlen) ;

   return addrlist->current ;
}


// section: ipport_t

// group: implementation

int initnamed_ipport(/*out*/ipport_t * port, const char * servicename, ipprotocol_e protocol)
{
   int err ;
   struct addrinfo   addrinfo_filter ;
   struct addrinfo * addrinfo_list ;

   PRECONDITION_INPUT(ipprotocol_TCP == protocol || ipprotocol_UDP == protocol, ABBRUCH, LOG_INT(protocol)) ;

   memset( &addrinfo_filter, 0, sizeof(addrinfo_filter)) ;
   addrinfo_filter.ai_family   = AF_INET ;
   addrinfo_filter.ai_protocol = 0 ;

   err = getaddrinfo(0, servicename, &addrinfo_filter, &addrinfo_list) ;
   if (err) {
      if (EAI_SERVICE == err) {
         err = ENOENT ;
      } else {
         err = convert_eai_errorcodes(err) ;
      }
      goto ABBRUCH ;
   }

   err = EPROTONOSUPPORT ;

   for(struct addrinfo * next = addrinfo_list; next ; next = next->ai_next) {
      if ((int)protocol == next->ai_protocol) {
         *port = ntohs(((struct sockaddr_in *)next->ai_addr)->sin_port) ;
         err = 0 ;
         break ;
      }
   }

   freeaddrinfo(addrinfo_list) ;

   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}


// section: Functions

// group: implementation

#ifdef KONFIG_UNITTEST

#define TEST(ARG) TEST_ONERROR_GOTO(ARG,unittest_io_ipaddr,ABBRUCH)

static int test_ipport(void)
{
   ipport_t tcp_port = ipport_ANY ;
   ipport_t udp_port = ipport_ANY ;
   static struct {
      const char * name ;
      uint16_t     tcp_port ;
      uint16_t     udp_port ;
   } test_service[8] =  {
         { "echo", 7, 7 },
         { "ftp", 21, 0 },
         { "http", 80, 0 },
         { "https", 443, 443 },
         { "irc", 194, 194 },
         { "snmp", 161, 161 },
         { "ssh", 22, 22 },
         { "telnet", 23, 0 },
   } ;

   // TEST static init
   TEST(0 == tcp_port) ;
   TEST(0 == udp_port) ;

   // TEST query tcp,udp / ERROR EPROTONOSUPPORT
   for(int i = 0; i < 8; ++i) {
      if (test_service[i].tcp_port) {
         tcp_port = 0 ;
         TEST(0 == initnamed_ipport(&tcp_port, test_service[i].name, ipprotocol_TCP)) ;
         TEST(test_service[i].tcp_port == tcp_port) ;
      } else {
         TEST(EPROTONOSUPPORT == initnamed_ipport(&tcp_port, test_service[i].name, ipprotocol_TCP)) ;
      }
      if (test_service[i].udp_port) {
         udp_port = 0 ;
         TEST(0 == initnamed_ipport(&udp_port, test_service[i].name, ipprotocol_UDP)) ;
         TEST(test_service[i].udp_port == udp_port) ;
      } else {
         TEST(EPROTONOSUPPORT == initnamed_ipport(&udp_port, test_service[i].name, ipprotocol_UDP)) ;
      }
   }

   // TEST ENOENT "service name does not exist"
   TEST(ENOENT == initnamed_ipport(&tcp_port, "XXX-not_exist-XXX", ipprotocol_TCP)) ;

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

static int test_ipaddr(void)
{
   cstring_t         name     = cstring_INIT ;
   ipaddr_t       *  ipaddr   = 0 ;
   ipaddr_t       *  ipaddr2  = 0 ;
   ipaddr_list_t  *  addrlist = 0 ;

   // TEST init, double free
   TEST(0 == new_ipaddr(&ipaddr, ipprotocol_TCP, "1.2.3.4", 1, ipversion_4 )) ;
   TEST(ipaddr) ;
   TEST(port_ipaddr(ipaddr)    == 1) ;
   TEST(protocol_ipaddr(ipaddr)== ipprotocol_TCP) ;
   TEST(version_ipaddr(ipaddr) == ipversion_4) ;
   TEST(ipaddr->addrlen        == sizeof(struct sockaddr_in)) ;
   TEST(0 == numericname_ipaddr(ipaddr, &name)) ;
   TEST(0 == strcmp( str_cstring(&name), "1.2.3.4")) ;
   TEST(0 == delete_ipaddr(&ipaddr)) ;
   TEST(0 == ipaddr) ;
   TEST(0 == delete_ipaddr(&ipaddr)) ;
   TEST(0 == ipaddr) ;

   // TEST newdnsquery
   TEST(0 == newdnsquery_ipaddr(&ipaddr, ipprotocol_TCP, "www.heise.de", 50, ipversion_4 )) ;
   TEST(0 == newdnsquery_ipaddr(&ipaddr2, ipprotocol_UDP, "::23", 50, ipversion_6 )) ;
   TEST(ipaddr) ;
   TEST(port_ipaddr(ipaddr)    == 50) ;
   TEST(protocol_ipaddr(ipaddr)== ipprotocol_TCP) ;
   TEST(version_ipaddr(ipaddr) == ipversion_4) ;
   TEST(ipaddr->addrlen        == sizeof(struct sockaddr_in)) ;
   TEST(0 == numericname_ipaddr(ipaddr, &name)) ;
   TEST(0 == strcmp( str_cstring(&name), "193.99.144.85")) ;
   TEST(0 == dnsname_ipaddr(ipaddr, &name)) ;
   TEST(0 == strcmp( str_cstring(&name), "www.heise.de")) ;
   TEST(ipaddr2) ;
   TEST(port_ipaddr(ipaddr2)    == 50) ;
   TEST(protocol_ipaddr(ipaddr2)== ipprotocol_UDP) ;
   TEST(version_ipaddr(ipaddr2) == ipversion_6) ;
   TEST(ipaddr2->addrlen        == sizeof(struct sockaddr_in6)) ;
   TEST(0 == numericname_ipaddr(ipaddr2, &name)) ;
   TEST(0 == strcmp( str_cstring(&name), "::23")) ;
   TEST(0 == delete_ipaddr(&ipaddr)) ;
   TEST(0 == ipaddr) ;
   TEST(0 == delete_ipaddr(&ipaddr2)) ;
   TEST(0 == ipaddr2) ;

   // TEST newany
   TEST(0 == newany_ipaddr(&ipaddr, ipprotocol_UDP, 2, ipversion_4 )) ;
   TEST(0 == newany_ipaddr(&ipaddr2, ipprotocol_UDP, 2, ipversion_6 )) ;
   TEST(ipaddr) ;
   TEST(port_ipaddr(ipaddr)    == 2) ;
   TEST(protocol_ipaddr(ipaddr)== ipprotocol_UDP) ;
   TEST(version_ipaddr(ipaddr) == ipversion_4) ;
   TEST(ipaddr->addrlen        == sizeof(struct sockaddr_in)) ;
   TEST(0 == numericname_ipaddr(ipaddr, &name)) ;
   TEST(0 == strcmp( str_cstring(&name), "0.0.0.0")) ;
   TEST(ipaddr2) ;
   TEST(port_ipaddr(ipaddr2)    == 2) ;
   TEST(protocol_ipaddr(ipaddr2)== ipprotocol_UDP) ;
   TEST(version_ipaddr(ipaddr2) == ipversion_6) ;
   TEST(ipaddr2->addrlen        == sizeof(struct sockaddr_in6)) ;
   TEST(0 == numericname_ipaddr(ipaddr2, &name)) ;
   TEST(0 == strcmp( str_cstring(&name), "::")) ;
   TEST(0 == delete_ipaddr(&ipaddr)) ;
   TEST(0 == ipaddr) ;
   TEST(0 == delete_ipaddr(&ipaddr2)) ;
   TEST(0 == ipaddr2) ;

   // TEST newloopback
   TEST(0 == newloopback_ipaddr(&ipaddr, ipprotocol_TCP, 1002, ipversion_4 )) ;
   TEST(0 == newloopback_ipaddr(&ipaddr2, ipprotocol_TCP, 1002, ipversion_6 )) ;
   TEST(ipaddr) ;
   TEST(port_ipaddr(ipaddr)    == 1002) ;
   TEST(protocol_ipaddr(ipaddr)== ipprotocol_TCP) ;
   TEST(version_ipaddr(ipaddr) == ipversion_4) ;
   TEST(ipaddr->addrlen        == sizeof(struct sockaddr_in)) ;
   TEST(0 == numericname_ipaddr(ipaddr, &name)) ;
   TEST(0 == strcmp( str_cstring(&name), "127.0.0.1")) ;
   TEST(ipaddr2) ;
   TEST(port_ipaddr(ipaddr2)    == 1002) ;
   TEST(protocol_ipaddr(ipaddr2)== ipprotocol_TCP) ;
   TEST(version_ipaddr(ipaddr2) == ipversion_6) ;
   TEST(ipaddr2->addrlen        == sizeof(struct sockaddr_in6)) ;
   TEST(0 == numericname_ipaddr(ipaddr2, &name)) ;
   TEST(0 == strcmp( str_cstring(&name), "::1")) ;
   TEST(0 == delete_ipaddr(&ipaddr)) ;
   TEST(0 == ipaddr) ;
   TEST(0 == delete_ipaddr(&ipaddr2)) ;
   TEST(0 == ipaddr2) ;

   // TEST combinations
   static struct {
      ipprotocol_e   protocol ;
      const char   * addr ;
      uint16_t       port ;
      ipversion_e    version ;
   } testdata [7] = {
      { ipprotocol_UDP, "200.100.6.8", 65535, ipversion_4 },
      { ipprotocol_TCP, "8.8.3.200", 36, ipversion_4 },
      { ipprotocol_TCP, "0.0.0.0", 8036, ipversion_4 },
      { ipprotocol_TCP, "127.0.0.1", 9036, ipversion_4 },
      { ipprotocol_UDP, "::1", 112, ipversion_6 },
      { ipprotocol_TCP, "::", 2964, ipversion_6 },
      { ipprotocol_TCP, "1234:5678:abcd:ef00:ef00:abcd:cccc:aa55", 964, ipversion_6 },
   } ;
   for(unsigned i = 0; i < nrelementsof(testdata); ++i) {
      TEST(0 == new_ipaddr(&ipaddr, testdata[i].protocol, testdata[i].addr, testdata[i].port, testdata[i].version )) ;
      TEST(ipaddr) ;
      TEST(port_ipaddr(ipaddr)    == testdata[i].port) ;
      TEST(version_ipaddr(ipaddr) == testdata[i].version) ;
      TEST(ipaddr->protocol == testdata[i].protocol) ;
      TEST(ipaddr->addrlen  == ((testdata[i].version == ipversion_4) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6))) ;
      TEST(ipaddr->addr     == (struct sockaddr*) &ipaddr[1]) ;
      TEST(0 == numericname_ipaddr(ipaddr, &name)) ;
      TEST(0 == strcasecmp( str_cstring(&name), testdata[i].addr)) ;
      TEST(0 == newaddr_ipaddr( &ipaddr2, testdata[i].protocol, ipaddr->addrlen, ipaddr->addr)) ;
      TEST(ipaddr2) ;
      TEST(0 == compare_ipaddr(ipaddr2, ipaddr)) ;
      TEST(0 == delete_ipaddr(&ipaddr2)) ;
      TEST(0 == newcopy_ipaddr( &ipaddr2, ipaddr)) ;
      TEST(ipaddr2) ;
      TEST(0 == compare_ipaddr(ipaddr2, ipaddr)) ;
      TEST(0 == delete_ipaddr(&ipaddr2)) ;
      TEST(!ipaddr2) ;
      TEST(0 == newdnsquery_ipaddrlist(&addrlist, testdata[i].addr, testdata[i].protocol, testdata[i].port, testdata[i].version)) ;
      const ipaddr_t * next = next_ipaddrlist(addrlist) ;
      TEST(next) ;
      TEST(0 == compare_ipaddr(next, ipaddr)) ;
      TEST(0 == next_ipaddrlist(addrlist)) ;
      TEST(0 == delete_ipaddrlist(&addrlist)) ;
      TEST(0 == addrlist) ;
      TEST(0 == delete_ipaddr(&ipaddr)) ;
      TEST(!ipaddr) ;
   }

   for(unsigned i = 0; i < nrelementsof(testdata); ++i) {
      TEST(0 == new_ipaddr(&ipaddr, testdata[i].protocol, testdata[i].addr, testdata[i].port, testdata[i].version )) ;

      // TEST copy
      if (ipprotocol_UDP == testdata[i].protocol) {
         TEST(0 == newany_ipaddr(&ipaddr2, ipprotocol_TCP, (ipport_t)(testdata[i].port+1), testdata[i].version)) ;
      } else {
         TEST(0 == newany_ipaddr(&ipaddr2, ipprotocol_UDP, (ipport_t)(testdata[i].port+1), testdata[i].version)) ;
      }
      TEST(0 != compare_ipaddr(ipaddr2, ipaddr)) ;
      TEST(0 == copy_ipaddr(ipaddr2, ipaddr)) ;
      TEST(0 == compare_ipaddr(ipaddr2, ipaddr)) ;
      TEST(protocol_ipaddr(ipaddr2)== testdata[i].protocol) ;
      TEST(port_ipaddr(ipaddr2)    == testdata[i].port) ;
      TEST(version_ipaddr(ipaddr2) == testdata[i].version) ;
      TEST(0 == numericname_ipaddr(ipaddr2, &name)) ;
      TEST(0 == strcasecmp( str_cstring(&name), testdata[i].addr)) ;
      TEST(0 == delete_ipaddr(&ipaddr2)) ;

      // TEST setaddr
      if (ipprotocol_UDP == testdata[i].protocol) {
         TEST(0 == newany_ipaddr(&ipaddr2, ipprotocol_TCP, (ipport_t)(testdata[i].port+1), testdata[i].version)) ;
      } else {
         TEST(0 == newany_ipaddr(&ipaddr2, ipprotocol_UDP, (ipport_t)(testdata[i].port+1), testdata[i].version)) ;
      }
      TEST(0 != compare_ipaddr(ipaddr2, ipaddr)) ;
      TEST(0 == setaddr_ipaddr(ipaddr2, protocol_ipaddr(ipaddr), ipaddr->addrlen, ipaddr->addr)) ;
      TEST(0 == compare_ipaddr(ipaddr2, ipaddr)) ;
      TEST(protocol_ipaddr(ipaddr2)== testdata[i].protocol) ;
      TEST(port_ipaddr(ipaddr2)    == testdata[i].port) ;
      TEST(version_ipaddr(ipaddr2) == testdata[i].version) ;
      TEST(0 == numericname_ipaddr(ipaddr2, &name)) ;
      TEST(0 == strcasecmp( str_cstring(&name), testdata[i].addr)) ;
      TEST(0 == delete_ipaddr(&ipaddr2)) ;

      // TEST setprotocol
      TEST(0 == setprotocol_ipaddr(ipaddr, ipprotocol_UDP)) ;
      TEST(ipprotocol_UDP == protocol_ipaddr(ipaddr)) ;
      TEST(0 == setprotocol_ipaddr(ipaddr, ipprotocol_TCP)) ;
      TEST(ipprotocol_TCP == protocol_ipaddr(ipaddr)) ;
      TEST(0 == setprotocol_ipaddr(ipaddr, testdata[i].protocol)) ;
      TEST(protocol_ipaddr(ipaddr)== testdata[i].protocol) ;
      TEST(port_ipaddr(ipaddr)    == testdata[i].port) ;
      TEST(version_ipaddr(ipaddr) == testdata[i].version) ;
      TEST(0 == numericname_ipaddr(ipaddr, &name)) ;
      TEST(0 == strcasecmp( str_cstring(&name), testdata[i].addr)) ;

      // TEST setport
      for(unsigned p = 0; p < 65536; p += 250) {
         TEST(0 == setport_ipaddr(ipaddr, (uint16_t)p)) ;
         TEST(port_ipaddr(ipaddr)    == (uint16_t)p) ;
      }
      TEST(0 == setport_ipaddr(ipaddr, testdata[i].port)) ;
      TEST(protocol_ipaddr(ipaddr)== testdata[i].protocol) ;
      TEST(port_ipaddr(ipaddr)    == testdata[i].port) ;
      TEST(version_ipaddr(ipaddr) == testdata[i].version) ;
      TEST(0 == numericname_ipaddr(ipaddr, &name)) ;
      TEST(0 == strcasecmp( str_cstring(&name), testdata[i].addr)) ;
      TEST(0 == delete_ipaddr(&ipaddr)) ;
   }

   // TEST EAFNOSUPPORT (family must be either AF_INET or AF_INET6)
   TEST(EAFNOSUPPORT == new_ipaddr(&ipaddr, ipprotocol_TCP, "1.2.3.4", 1, (ipversion_e)AF_APPLETALK)) ;
   TEST(!ipaddr) ;

   // TEST EPROTONOSUPPORT (protocol must be either TCP or UDP)
   TEST(EPROTONOSUPPORT == new_ipaddr(&ipaddr, (ipprotocol_e)IPPROTO_ICMP, "1.2.3.4", 1, ipversion_4)) ;
   TEST(!ipaddr) ;

   // TEST EADDRNOTAVAIL (address literal must match family)
   TEST(EADDRNOTAVAIL == new_ipaddr(&ipaddr, ipprotocol_UDP, "::1", 1, ipversion_4)) ;
   TEST(!ipaddr) ;

   TEST(0 == free_cstring(&name)) ;

   return 0 ;
ABBRUCH:
   (void) free_cstring(&name) ;
   (void) delete_ipaddr(&ipaddr) ;
   (void) delete_ipaddr(&ipaddr2) ;
   (void) delete_ipaddrlist(&addrlist) ;
   return EINVAL ;
}

static int test_ipaddrlist(void)
{
   cstring_t         name       = cstring_INIT ;
   ipaddr_t        * copiedaddr = 0 ;
   ipaddr_list_t   * addrlist   = 0 ;
   struct addrinfo * first ;
   const ipaddr_t  * ipaddr ;

   // TEST init, double free
   TEST(0 == newdnsquery_ipaddrlist(&addrlist, "127.0.0.1", ipprotocol_TCP, 12345, ipversion_4)) ;
   TEST(addrlist) ;
   TEST(addrlist->first != 0) ;
   TEST(addrlist->first == addrlist->next) ;
   TEST(0 == delete_ipaddrlist(&addrlist)) ;
   TEST(0 == addrlist) ;
   TEST(0 == delete_ipaddrlist(&addrlist)) ;
   TEST(0 == addrlist) ;

   // TEST 1 element in result list
   TEST(0 == newdnsquery_ipaddrlist(&addrlist, "::1", ipprotocol_UDP, 234, ipversion_6)) ;
   TEST(addrlist) ;
   TEST(addrlist->first != 0) ;
   TEST(addrlist->first == addrlist->next) ;
   first = addrlist->first ;
   TEST(0 != (ipaddr = next_ipaddrlist(addrlist))) ;
   TEST(addrlist->first == first) ;
   TEST(addrlist->next  == 0) ;
   TEST(port_ipaddr(ipaddr)     == 234) ;
   TEST(protocol_ipaddr(ipaddr) == ipprotocol_UDP) ;
   TEST(version_ipaddr(ipaddr)  == ipversion_6) ;
   TEST(0 == numericname_ipaddr(ipaddr, &name)) ;
   TEST(0 == strcmp( str_cstring(&name), "::1")) ;
   TEST(0 == next_ipaddrlist(addrlist)) ;
   TEST(addrlist->first == first) ;
   TEST(addrlist->next  == 0) ;
   gofirst_ipaddrlist(addrlist) ;
   TEST(addrlist->first == first) ;
   TEST(addrlist->next  == first) ;
   TEST(0 == delete_ipaddrlist(&addrlist)) ;
   TEST(0 == addrlist) ;

   // TEST 2 elements (UDP + TCP protocol)
   TEST(0 == newdnsquery_ipaddrlist(&addrlist, "127.0.0.1", ipprotocol_ANY, 3, ipversion_4)) ;
   TEST(addrlist) ;
   TEST(addrlist->first != 0) ;
   TEST(addrlist->first == addrlist->next) ;
   first = addrlist->first ;
   TEST(0 != (ipaddr = next_ipaddrlist(addrlist))) ;
   TEST(addrlist->first == first) ;
   TEST(addrlist->next  != 0) ;
   TEST(protocol_ipaddr(ipaddr)== ipprotocol_TCP) ;
   TEST(port_ipaddr(ipaddr)    == 3) ;
   TEST(version_ipaddr(ipaddr) == ipversion_4) ;
   TEST(ipaddr->addrlen        == sizeof(struct sockaddr_in)) ;
   TEST(ipprotocol_TCP == protocol_ipaddr(ipaddr)) ;
   TEST(0 == numericname_ipaddr(ipaddr, &name)) ;
   TEST(0 == strcmp( str_cstring(&name), "127.0.0.1")) ;
   TEST(0 != (ipaddr = next_ipaddrlist(addrlist))) ;
   TEST(addrlist->first == first) ;
   TEST(addrlist->next  != 0/*IPPROTO_IP*/) ;
   TEST(protocol_ipaddr(ipaddr)== ipprotocol_UDP) ;
   TEST(port_ipaddr(ipaddr)    == 3) ;
   TEST(version_ipaddr(ipaddr) == ipversion_4) ;
   TEST(ipaddr->addrlen        == sizeof(struct sockaddr_in)) ;
   TEST(0 == numericname_ipaddr(ipaddr, &name)) ;
   TEST(0 == strcmp( str_cstring(&name), "127.0.0.1")) ;
   TEST(0 == next_ipaddrlist(addrlist)) ; // IPPROTO_IP skipped
   TEST(addrlist->first == first) ;
   TEST(addrlist->next  == 0) ;
   gofirst_ipaddrlist(addrlist) ;
   TEST(addrlist->first == first) ;
   TEST(addrlist->next  == first) ;
   TEST(0 == delete_ipaddrlist(&addrlist)) ;
   TEST(0 == addrlist) ;

   // TEST 4 elements (UDP + TCP protocol)
   TEST(0 == newdnsquery_ipaddrlist(&addrlist, 0, ipprotocol_ANY, 5, ipversion_ANY)) ;
   TEST(addrlist) ;
   TEST(addrlist->first != 0) ;
   TEST(addrlist->first == addrlist->next) ;
   first = addrlist->first ;
   for(int i = 0; i < 4; ++i) {
      TEST(addrlist->next  != 0) ;
      TEST(0 != (ipaddr = next_ipaddrlist(addrlist))) ;
      TEST(addrlist->first == first) ;
      TEST(port_ipaddr(ipaddr) == 5) ;
      if (ipversion_4 == version_ipaddr(ipaddr)) {
         TEST(ipaddr->addrlen == sizeof(struct sockaddr_in)) ;
         TEST(0 == numericname_ipaddr(ipaddr, &name)) ;
         TEST(0 == strcmp( str_cstring(&name), "127.0.0.1")) ;
      } else {
         TEST(ipaddr->addrlen == sizeof(struct sockaddr_in6)) ;
         TEST(0 == numericname_ipaddr(ipaddr, &name)) ;
         TEST(0 == strcmp( str_cstring(&name), "::1")) ;
      }
      switch(i) {
      case 0:
         TEST(protocol_ipaddr(ipaddr)== ipprotocol_TCP) ;
         TEST(version_ipaddr(ipaddr) == ipversion_6) ;
         break ;
      case 1:
         TEST(protocol_ipaddr(ipaddr)== ipprotocol_UDP) ;
         TEST(version_ipaddr(ipaddr) == ipversion_6) ;
         break ;
      case 2:
         TEST(protocol_ipaddr(ipaddr)== ipprotocol_TCP) ;
         TEST(version_ipaddr(ipaddr) == ipversion_4) ;
         break ;
      case 3:
         TEST(protocol_ipaddr(ipaddr)== ipprotocol_UDP) ;
         TEST(version_ipaddr(ipaddr) == ipversion_4) ;
         break ;
      }
   }
   TEST(0 == next_ipaddrlist(addrlist)) ;
   TEST(addrlist->first == first) ;
   TEST(addrlist->next  == 0) ;
   gofirst_ipaddrlist(addrlist) ;
   TEST(addrlist->first == first) ;
   TEST(addrlist->next  == first) ;
   TEST(0 == delete_ipaddrlist(&addrlist)) ;
   TEST(0 == addrlist) ;

   // TEST resolve dns name
   // ipversion_6 does not work with my provider
   TEST(0 == newdnsquery_ipaddrlist(&addrlist, "www.heise.de", ipprotocol_UDP, 0, ipversion_4)) ;
   // check result (UDP protocol)
   TEST(0 != (ipaddr = next_ipaddrlist(addrlist))) ;
   TEST(version_ipaddr(ipaddr)  == ipversion_4) ;
   TEST(protocol_ipaddr(ipaddr) == ipprotocol_UDP) ;
   TEST(port_ipaddr(ipaddr)     == 0) ;
   TEST(0 == numericname_ipaddr(ipaddr, &name)) ;
   TEST(0 == strcmp( str_cstring(&name), "193.99.144.85")) ;
   TEST(0 == dnsname_ipaddr(ipaddr, &name)) ;
   TEST(0 == strcmp( str_cstring(&name), "www.heise.de")) ;
   TEST(0 == next_ipaddrlist(addrlist)) ;
   TEST(0 == delete_ipaddrlist(&addrlist)) ;
   TEST(0 == addrlist) ;

   // Test IDN
   TEST(0 == newdnsquery_ipaddrlist(&addrlist, "www.café.com", ipprotocol_TCP, 3, ipversion_4)) ;
   // check result (TCP protocol)
   TEST(0 != (ipaddr = next_ipaddrlist(addrlist))) ;
   TEST(version_ipaddr(ipaddr)  == ipversion_4) ;
   TEST(protocol_ipaddr(ipaddr) == ipprotocol_TCP) ;
   TEST(port_ipaddr(ipaddr)     == 3) ;
   TEST(0 == dnsname_ipaddr(ipaddr, &name)) ;
   TEST(0 == strcmp( str_cstring(&name), "www.café.com")) ;
   TEST(0 == newcopy_ipaddr(&copiedaddr, ipaddr)) ;
   TEST(copiedaddr) ;
   TEST(0 == next_ipaddrlist(addrlist)) ;
   TEST(0 == delete_ipaddrlist(&addrlist)) ;
   TEST(0 == addrlist) ;

      // compare same result as ACE encoded
   TEST(0 == newdnsquery_ipaddrlist(&addrlist, "www.xn--caf-dma.com", ipprotocol_TCP, 3, ipversion_4)) ;
   // check result (TCP protocol)
   TEST(0 != (ipaddr = next_ipaddrlist(addrlist))) ;
   TEST(version_ipaddr(ipaddr)  == ipversion_4) ;
   TEST(protocol_ipaddr(ipaddr) == ipprotocol_TCP) ;
   TEST(port_ipaddr(ipaddr)     == 3) ;
   TEST(ipaddr->addrlen         == sizeof(struct sockaddr_in)) ;
   TEST(ipaddr->addrlen         == copiedaddr->addrlen) ;
   TEST(0 == memcmp(ipaddr->addr, copiedaddr->addr, copiedaddr->addrlen)) ;
   TEST(0 == delete_ipaddr(&copiedaddr)) ;
   TEST(0 == copiedaddr) ;
   TEST(0 == dnsname_ipaddr(ipaddr, &name)) ;
   TEST(0 == strcmp( str_cstring(&name), "www.café.com")) ;
   TEST(0 == dnsnameace_ipaddr(ipaddr, &name)) ;
   TEST(0 == strcmp( str_cstring(&name), "www.xn--caf-dma.com")) ;
   TEST(0 == next_ipaddrlist(addrlist)) ;
   TEST(0 == delete_ipaddrlist(&addrlist)) ;
   TEST(0 == addrlist) ;

   // TEST ERROR "wrong protocol type"
   TEST(EPROTONOSUPPORT == newdnsquery_ipaddrlist(&addrlist, "127.0.0.1", (ipprotocol_e)10000, 0, ipversion_4)) ;
   TEST(0 == addrlist) ;

   // TEST ERROR address not available in address family
   TEST(EADDRNOTAVAIL == newdnsquery_ipaddrlist(&addrlist, "127.0.0.1", ipprotocol_ANY, 0, ipversion_6)) ;
   TEST(0 == addrlist) ;
   TEST(EADDRNOTAVAIL == newdnsquery_ipaddrlist(&addrlist, "::1", ipprotocol_ANY, 0, ipversion_4)) ;
   TEST(0 == addrlist) ;

   // TEST ERROR "family not supported"
   TEST(EAFNOSUPPORT == newdnsquery_ipaddrlist(&addrlist, "127.0.0.1", ipprotocol_ANY, 0, (ipversion_e)AF_UNIX)) ;
   TEST(0 == addrlist) ;
   TEST(EAFNOSUPPORT == newdnsquery_ipaddrlist(&addrlist, "127.0.0.1", ipprotocol_ANY, 0, (ipversion_e)10000)) ;
   TEST(0 == addrlist) ;

   // TEST ERROR ENOENT
   TEST(ENOENT == newdnsquery_ipaddrlist(&addrlist, "192.68.2.1.2", ipprotocol_ANY, 0, ipversion_4)) ;
   TEST(0 == addrlist) ;

   // TEST ERROR ENODATA (label (text between two '.') is longer than 63 characters)
   TEST(ENODATA == newdnsquery_ipaddrlist(&addrlist, "www.ein-label-das-zu-lange-ist-und-einen-fehler-ausloesen-sollte-123456789.de", ipprotocol_ANY, 0, ipversion_4)) ;
   TEST(0 == addrlist) ;

   TEST(0 == free_cstring(&name)) ;

   return 0 ;
ABBRUCH:
   (void) free_cstring(&name) ;
   (void) delete_ipaddr(&copiedaddr) ;
   (void) delete_ipaddrlist(&addrlist) ;
   return EINVAL ;
}

int unittest_io_ipaddr()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   if (test_ipport())         goto ABBRUCH ;
   if (test_ipaddrlist())     goto ABBRUCH ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_ipport())         goto ABBRUCH ;
   if (test_ipaddr())         goto ABBRUCH ;
   if (test_ipaddrlist())     goto ABBRUCH ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
