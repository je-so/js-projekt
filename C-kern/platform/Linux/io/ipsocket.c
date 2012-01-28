/* title: Internetprotocol-Sockets Linux
   Impplements <Internetprotocol-Sockets> on Linux.

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

   file: C-kern/platform/Linux/io/ipsocket.c
    Linux specific implementation <Internetprotocol-Sockets Linux>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/io/ip/ipsocket.h"
#include "C-kern/api/io/ip/ipaddr.h"
#include "C-kern/api/io/filedescr.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/platform/thread.h"
#endif


int free_ipsocket(ipsocket_t * ipsock)
{
   int err ;

   err = free_filedescr(ipsock) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}

int initsocket_helper(/*out*/ipsocket_t * ipsock, const ipaddr_t * localaddr)
{
   int err ;
   int fd = -1 ;
   int socktype ;

   switch(protocol_ipaddr(localaddr)) {
   case ipprotocol_ANY: socktype = SOCK_RAW ;   break ;
   case ipprotocol_UDP: socktype = SOCK_DGRAM ; break ;
   case ipprotocol_TCP: socktype = SOCK_STREAM; break ;
   default:  err = EPROTONOSUPPORT ; goto ABBRUCH ;
   }

   fd = socket(version_ipaddr(localaddr), socktype|SOCK_CLOEXEC, protocol_ipaddr(localaddr)) ;
   if (-1 == fd) {
      err = errno ;
      LOG_SYSERR("socket", err) ;
      goto ABBRUCH_LOG ;
   }

   const int on = 1 ;
   if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) {
      err = errno ;
      LOG_SYSERR("setsockopt(SO_REUSEADDR)", err) ;
      goto ABBRUCH_LOG ;
   }

   if (ipprotocol_TCP == protocol_ipaddr(localaddr)) {
      if (setsockopt(fd, SOL_SOCKET, SO_OOBINLINE, &on, sizeof(on))) {
         err = errno ;
         LOG_SYSERR("setsockopt(SO_OOBINLINE)", err) ;
         goto ABBRUCH_LOG ;
      }
   }

   if (bind(fd, localaddr->addr, localaddr->addrlen)) {
      err = errno ;
      LOG_SYSERR("bind", err) ;
      goto ABBRUCH_LOG ;
   }

   *ipsock = fd ;
   return 0 ;
ABBRUCH_LOG:
   LOG_INT(port_ipaddr(localaddr)) ;
   LOG_INT(protocol_ipaddr(localaddr)) ;
   LOG_INT(version_ipaddr(localaddr)) ;
   LOG_INT(socktype) ;
   LOG_INT(fd) ;
   cstring_t name = cstring_INIT ;
   if (0 == numericname_ipaddr(localaddr, &name)) {
      LOG_STRING(str_cstring(&name)) ;
   }
   (void) free_cstring(&name) ;
ABBRUCH:
   free_filedescr(&fd) ;
   LOG_ABORT(err) ;
   return err ;
}

int init_ipsocket(/*out*/ipsocket_t * ipsock, const ipaddr_t * localaddr)
{
   int err ;

   VALIDATE_INPARAM_TEST(isvalid_ipaddr(localaddr), ABBRUCH, ) ;

   if (ipprotocol_UDP != protocol_ipaddr(localaddr)) {
      err = EPROTONOSUPPORT ;
      goto ABBRUCH ;
   }

   err = initsocket_helper(ipsock, localaddr) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int initconnect_ipsocket(/*out*/ipsocket_t * ipsock, const struct ipaddr_t * remoteaddr, const struct ipaddr_t * localaddr)
{
   int err ;
   int fd ;
   ipsocket_t        new_ipsock = ipsocket_INIT_FREEABLE ;
   ipaddr_storage_t  localaddr2 ;

   VALIDATE_INPARAM_TEST(isvalid_ipaddr(remoteaddr), ABBRUCH, ) ;
   if (localaddr) {
      VALIDATE_INPARAM_TEST(isvalid_ipaddr(localaddr), ABBRUCH, ) ;
      VALIDATE_INPARAM_TEST(protocol_ipaddr(localaddr) == protocol_ipaddr(remoteaddr), ABBRUCH, ) ;
   } else {
      localaddr = initany_ipaddrstorage(&localaddr2, protocol_ipaddr(remoteaddr), 0, version_ipaddr(remoteaddr)) ;
   }

   err = initsocket_helper(&new_ipsock, localaddr) ;
   if (err) goto ABBRUCH ;

   fd = new_ipsock ;

   err = connect(fd, remoteaddr->addr, remoteaddr->addrlen) ;
   if (err) {
      err = errno ;
      LOG_SYSERR("connect", err) ;
      LOG_INT(fd) ;
      goto ABBRUCH ;
   }

   *ipsock = new_ipsock ;
   return 0 ;
ABBRUCH:
   free_ipsocket(&new_ipsock) ;
   LOG_ABORT(err) ;
   return err ;
}

int initlisten_ipsocket(/*out*/ipsocket_t * ipsock, const struct ipaddr_t * localaddr, uint16_t max_outstanding_connections)
{
   int err ;
   int fd ;
   ipsocket_t new_ipsock = ipsocket_INIT_FREEABLE ;

   VALIDATE_INPARAM_TEST(isvalid_ipaddr(localaddr), ABBRUCH, ) ;

   err = initsocket_helper(&new_ipsock, localaddr) ;
   if (err) goto ABBRUCH ;

   fd = new_ipsock ;

   if (listen(fd, max_outstanding_connections)) {
      err = errno ;
      LOG_SYSERR("listen", err) ;
      LOG_INT(fd) ;
      LOG_INT(max_outstanding_connections) ;
      goto ABBRUCH ;
   }

   *ipsock = new_ipsock ;
   return 0 ;
ABBRUCH:
   free_ipsocket(&new_ipsock) ;
   LOG_ABORT(err) ;
   return err ;
}

int initwaitconnect_ipsocket(/*out*/ipsocket_t * ipsock, ipsocket_t * listensock, struct ipaddr_t * remoteaddr/*0 => ignored*/)
{
   int err ;
   struct sockaddr_storage saddr ;
   socklen_t               len        = sizeof(saddr) ;
   int                     new_socket = -1 ;
   int                     fd         = *listensock ;

   if (  remoteaddr
      && version_ipaddr(remoteaddr) != version_ipsocket(listensock)) {
      err = EAFNOSUPPORT ;
      goto ABBRUCH ;
   }

   new_socket = accept4(fd, (struct sockaddr*) &saddr, &len, SOCK_CLOEXEC) ;
   if (-1 == new_socket) {
      err = errno ;
      LOG_SYSERR("accept4", err) ;
      LOG_INT(fd) ;
      goto ABBRUCH ;
   }

   if (remoteaddr) {
      ipprotocol_e protocol = protocol_ipsocket(listensock) ;

      err = setaddr_ipaddr( remoteaddr, protocol, (uint16_t)len, (struct sockaddr*) &saddr ) ;
      if (err) goto ABBRUCH ;
   }

   *ipsock = new_socket ;
   return 0 ;
ABBRUCH:
   free_filedescr(&new_socket) ;
   LOG_ABORT(err) ;
   return err ;
}

bool isconnected_ipsocket(const ipsocket_t * ipsock)
{
   int err ;
   struct sockaddr_storage saddr ;
   socklen_t               len = sizeof(saddr) ;
   int                     fd  = *ipsock ;

   if (getpeername(fd, (struct sockaddr*) &saddr, &len)) {
      err = errno ;
      if (err != ENOTCONN) {
         LOG_SYSERR("getpeername", err) ;
         LOG_INT(fd) ;
         goto ABBRUCH ;
      }
      return false ;
   }

   return true ;
ABBRUCH:
   LOG_ABORT(err) ;
   return false ;
}

bool islisten_ipsocket(const ipsocket_t * ipsock)
{
   int err ;
   int       value ;
   socklen_t len = sizeof(value) ;
   int       fd  = *ipsock ;

   if (getsockopt(fd, SOL_SOCKET, SO_ACCEPTCONN, &value, &len)) {
      err = errno ;
      LOG_SYSERR("getsockopt(SO_ACCEPTCONN)", err) ;
      LOG_INT(fd) ;
      goto ABBRUCH ;
   }
   assert(len == sizeof(int)) ;

   return (bool) value ;
ABBRUCH:
   LOG_ABORT(err) ;
   return false ;
}

uint16_t protocol_ipsocket(const ipsocket_t * ipsock)
{
   int err ;
   int       value ;
   socklen_t len = sizeof(value) ;
   int       fd  = *ipsock ;

   if (getsockopt(fd, SOL_SOCKET, SO_PROTOCOL, &value, &len)) {
      err = errno ;
      LOG_SYSERR("getsockopt(SO_PROTOCOL)", err) ;
      LOG_INT(fd) ;
      goto ABBRUCH ;
   }
   assert(len == sizeof(int)) ;

   return (uint16_t) value ;
ABBRUCH:
   LOG_ABORT(err) ;
   return ipprotocol_ANY ;
}

uint16_t version_ipsocket(const ipsocket_t * ipsock)
{
   int err ;
   int       value ;
   socklen_t len = sizeof(value) ;
   int       fd  = *ipsock ;

   if (getsockopt(fd, SOL_SOCKET, SO_DOMAIN, &value, &len)) {
      err = errno ;
      LOG_SYSERR("getsockopt(SO_DOMAIN)", err) ;
      LOG_INT(fd) ;
      goto ABBRUCH ;
   }
   assert(len == sizeof(int)) ;

   return (uint16_t) value ;
ABBRUCH:
   LOG_ABORT(err) ;
   return ipversion_ANY ;
}

int localaddr_ipsocket(const ipsocket_t * ipsock, /*out*/ipaddr_t * localaddr)
{
   int err ;
   struct sockaddr_storage saddr ;
   socklen_t               len = sizeof(saddr) ;
   int                     fd  = *ipsock ;

   if (version_ipaddr(localaddr) != version_ipsocket(ipsock)) {
      err = EAFNOSUPPORT ;
      goto ABBRUCH ;
   }

   if (getsockname(fd, (struct sockaddr*) &saddr, &len)) {
      err = errno ;
      LOG_SYSERR("getsockname", err) ;
      LOG_INT(fd) ;
      goto ABBRUCH ;
   }

   ipprotocol_e protocol = protocol_ipsocket(ipsock) ;

   err = setaddr_ipaddr( localaddr, protocol, (uint16_t)len, (struct sockaddr*) &saddr ) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int remoteaddr_ipsocket(const ipsocket_t * ipsock, ipaddr_t * remoteaddr)
{
   int err ;
   struct sockaddr_storage saddr ;
   socklen_t               len = sizeof(saddr) ;
   int                     fd  = *ipsock ;

   if (version_ipaddr(remoteaddr) != version_ipsocket(ipsock)) {
      err = EAFNOSUPPORT ;
      goto ABBRUCH ;
   }

   if (getpeername(fd, (struct sockaddr*) &saddr, &len)) {
      err = errno ;
      LOG_SYSERR("getpeername", err) ;
      LOG_INT(fd) ;
      goto ABBRUCH ;
   }

   ipprotocol_e protocol = protocol_ipsocket(ipsock) ;

   err = setaddr_ipaddr( remoteaddr, protocol, (uint16_t)len, (struct sockaddr*) &saddr) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int bytestoread_ipsocket(const ipsocket_t * ipsock, /*out*/size_t * unread_bytes)
{
   int err ;
   int bytes ;
   int fd = *ipsock ;

   if (ioctl(fd, FIONREAD, &bytes)) {
      err = errno ;
      LOG_SYSERR("ioctl(FIONREAD)", err) ;
      LOG_INT(fd) ;
      goto ABBRUCH ;
   }

   *unread_bytes = (unsigned)bytes ;
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int bytestowrite_ipsocket(const ipsocket_t * ipsock, /*out*/size_t * unsend_bytes)
{
   int err ;
   int bytes ;
   int fd = *ipsock ;

   if (ioctl(fd, TIOCOUTQ, &bytes)) {
      err = errno ;
      LOG_SYSERR("ioctl(TIOCOUTQ)", err) ;
      LOG_INT(fd) ;
      goto ABBRUCH ;
   }

   *unsend_bytes = (unsigned)bytes ;
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int queuesizeread_ipsocket(const ipsocket_t * ipsock, /*out*/size_t * queuesize_read)
{
   int err ;
   int         value ;
   socklen_t   len = sizeof(value) ;
   int         fd  = *ipsock ;

   if (getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &value, &len)) {
      err = errno ;
      LOG_SYSERR("getsockopt(SO_RCVBUF)", err) ;
      LOG_INT(fd) ;
      goto ABBRUCH ;
   }
   assert(len == sizeof(int)) ;

   *queuesize_read = (unsigned)value ;
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int queuesizewrite_ipsocket(const ipsocket_t * ipsock, /*out*/size_t * queuesize_write)
{
   int err ;
   int         value ;
   socklen_t   len = sizeof(value) ;
   int         fd  = *ipsock ;

   if (getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &value, &len)) {
      err = errno ;
      LOG_SYSERR("getsockopt(SO_SNDBUF)", err) ;
      LOG_INT(fd) ;
      goto ABBRUCH ;
   }
   assert(len == sizeof(int)) ;

   *queuesize_write = (unsigned)value ;
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int setqueuesize_ipsocket(ipsocket_t * ipsock, size_t queuesize_read, size_t queuesize_write)
{
   int err ;
   int        rvalue = (int) (queuesize_read/2) ;
   int        wvalue = (int) (queuesize_write/2) ;
   socklen_t  len    = sizeof(rvalue) ;
   int        fd     = *ipsock ;

   VALIDATE_INPARAM_TEST(  queuesize_read  <= INT_MAX
                        && queuesize_write <= INT_MAX, ABBRUCH, LOG_SIZE(queuesize_read); LOG_SIZE(queuesize_write) ) ;

   if (queuesize_read)  {
      if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rvalue, len)) {
         err = errno ;
         LOG_SYSERR("setsockopt(SO_RCVBUF)", err) ;
         LOG_INT(fd) ;
         LOG_INT(queuesize_read) ;
         goto ABBRUCH ;
      }
   }

   if (queuesize_write)  {
      if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &wvalue, len)) {
         err = errno ;
         LOG_SYSERR("setsockopt(SO_SNDBUF)", err) ;
         LOG_INT(fd) ;
         LOG_INT(queuesize_write) ;
         goto ABBRUCH ;
      }
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

// === connected I/O ===

int read_ipsocket(ipsocket_t * ipsock, size_t maxdata_len, /*out*/uint8_t data[maxdata_len], /*out*/size_t * bytes_read )
{
   int     err ;
   int     fd    = *ipsock ;
   ssize_t bytes = recv(fd, data, maxdata_len, MSG_DONTWAIT) ;

   if (-1 == bytes) {
      err = errno ;
      if (EWOULDBLOCK == err) err = EAGAIN ;
      LOG_SYSERR("recv", err) ;
      LOG_INT(fd) ;
      LOG_SIZE(maxdata_len) ;
      goto ABBRUCH ;
   }

   if (bytes_read) {
      *bytes_read = (size_t) bytes ;
   }
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int write_ipsocket(ipsocket_t * ipsock, size_t maxdata_len, const uint8_t data[maxdata_len], /*out*/size_t * bytes_written )
{
   int     err ;
   int     fd    = *ipsock ;
   ssize_t bytes = send(fd, data, maxdata_len, MSG_NOSIGNAL|MSG_DONTWAIT) ;

   if (-1 == bytes) {
      err = errno ;
      if (EWOULDBLOCK == err) err = EAGAIN ;
      LOG_SYSERR("send", err) ;
      LOG_INT(fd) ;
      LOG_SIZE(maxdata_len) ;
      goto ABBRUCH ;
   }

   if (bytes_written) {
      *bytes_written = (size_t) bytes ;
   }
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int readoob_ipsocket(ipsocket_t * ipsock, size_t maxdata_len, /*out*/uint8_t data[maxdata_len], /*out*/size_t * bytes_read, /*out*/size_t * oob_offset)
{
   int err ;
   int     isUrgent      = 0 ;
   int     fd            = *ipsock ;
   size_t  urgent_offset = maxdata_len ; /* no urgent data */
   ssize_t bytes ;

   /* If isUrgent is a 1 on return, the next read will return the oob data as first byte */
   err = ioctl(fd, SIOCATMARK, &isUrgent) ;
   if (-1 == err) {
      err = errno ;
      if (err == EINVAL) err = EOPNOTSUPP ;
      LOG_SYSERR("ioctl(SIOCATMARK)", err) ;
      LOG_INT(fd) ;
      goto ABBRUCH ;
   }

   bytes = recv(fd, data, maxdata_len, MSG_DONTWAIT) ;
   if (-1 == bytes) {
      err = errno ;
      if (EWOULDBLOCK == err) err = EAGAIN ;
      LOG_SYSERR("recv", err) ;
      LOG_INT(fd) ;
      LOG_SIZE(maxdata_len) ;
      goto ABBRUCH ;
   }

   if (isUrgent) {
      urgent_offset = 0 ;
   } else if ((size_t)bytes < maxdata_len) {
      err = ioctl(fd, SIOCATMARK, &isUrgent) ;
      // ignore possible error
      if (     isUrgent
            && -1 != err) {
         /* recv returned less than maxdata_len cause the next read will return the oob data as first byte */
         ssize_t bytes2 = recv(fd, data + bytes, maxdata_len-(size_t)bytes, MSG_DONTWAIT) ;
         // ignore possible error
         if (0 < bytes2) {
            urgent_offset = (size_t) bytes ;
            bytes        += bytes2 ;
         }
      }
   }

   if (bytes_read) {
      *bytes_read = (size_t) bytes ;
   }
   if (oob_offset) {
      *oob_offset = urgent_offset ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int writeoob_ipsocket(ipsocket_t * ipsock, uint8_t data)
{
   int     err ;
   int     fd    = *ipsock ;
   ssize_t bytes = send(fd, &data, 1, MSG_OOB|MSG_NOSIGNAL|MSG_DONTWAIT) ;

   if (1 != bytes) {
      if (-1 == bytes) {
         err = errno ;
         if (EWOULDBLOCK == err) err = EAGAIN ;
      } else {
         err = EAGAIN ;
      }
      LOG_SYSERR("send", err) ;
      LOG_INT(fd) ;
      goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}


// === unconnected I/O ===

int readfrom_ipsocket(ipsocket_t * ipsock, ipaddr_t * remoteaddr, size_t maxdata_len, /*out*/uint8_t data[maxdata_len], /*out*/size_t * bytes_read)
{
   int err ;
   struct sockaddr_storage saddr ;
   socklen_t               slen  = sizeof(saddr) ;
   int                     fd    = *ipsock ;
   ssize_t                 bytes ;

   if (remoteaddr) {
      VALIDATE_INPARAM_TEST(isvalid_ipaddr(remoteaddr), ABBRUCH, ) ;

      if (version_ipaddr(remoteaddr) != version_ipsocket(ipsock)) {
         err = EAFNOSUPPORT ;
         goto ABBRUCH ;
      }
   }

   bytes = recvfrom(fd, data, maxdata_len, MSG_DONTWAIT, (struct sockaddr*)&saddr, &slen) ;
   if (-1 == bytes) {
      err = errno ;
      if (EWOULDBLOCK == err) err = EAGAIN ;
      LOG_SYSERR("recv", err) ;
      LOG_INT(fd) ;
      LOG_SIZE(maxdata_len) ;
      goto ABBRUCH ;
   }

   if (remoteaddr) {
      ipprotocol_e protocol = protocol_ipsocket( ipsock ) ;

      err = setaddr_ipaddr(remoteaddr, protocol, (uint16_t)slen, (struct sockaddr*)&saddr) ;
      if (err) goto ABBRUCH ;
   }

   if (bytes_read) {
      *bytes_read = (size_t) bytes ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int writeto_ipsocket(ipsocket_t * ipsock, const ipaddr_t * remoteaddr, size_t maxdata_len, const uint8_t data[maxdata_len], /*out*/size_t * bytes_written )
{
   int     err ;
   int     fd    = *ipsock ;
   ssize_t bytes ;

   VALIDATE_INPARAM_TEST(isvalid_ipaddr(remoteaddr), ABBRUCH, ) ;

   if (ipprotocol_UDP != protocol_ipaddr(remoteaddr)) {
      err = EPROTONOSUPPORT ;
      goto ABBRUCH ;
   }

   if (version_ipaddr(remoteaddr) != version_ipsocket(ipsock)) {
      err = EAFNOSUPPORT ;
      goto ABBRUCH ;
   }

   bytes = sendto(fd, data, maxdata_len, MSG_NOSIGNAL|MSG_DONTWAIT, remoteaddr->addr, remoteaddr->addrlen) ;
   if (-1 == bytes) {
      err = errno ;
      if (EWOULDBLOCK == err) err = EAGAIN ;
      LOG_SYSERR("sendto", err) ;
      LOG_INT(fd) ;
      LOG_SIZE(maxdata_len) ;
      goto ABBRUCH ;
   }

   if (bytes_written) {
      *bytes_written = (size_t) bytes ;
   }
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}


// section: ipsocket_async_t

int free_ipsocketasync(ipsocket_async_t * ipsockasync)
{
   int err ;

   ipsockasync->err = 0 ;

   err = free_ipsocket(&ipsockasync->ipsock) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}

int initconnect_ipsocketasync(/*out*/ipsocket_async_t * ipsockasync, const struct ipaddr_t * remoteaddr, const struct ipaddr_t * localaddr)
{
   int err ;
   int fd ;
   ipsocket_t        new_ipsock = ipsocket_INIT_FREEABLE ;
   ipaddr_storage_t  localaddr2 ;

   VALIDATE_INPARAM_TEST(isvalid_ipaddr(remoteaddr), ABBRUCH, ) ;
   if (localaddr) {
      VALIDATE_INPARAM_TEST(isvalid_ipaddr(localaddr), ABBRUCH, ) ;
      VALIDATE_INPARAM_TEST(protocol_ipaddr(localaddr) == protocol_ipaddr(remoteaddr), ABBRUCH, ) ;
   } else {
      localaddr = initany_ipaddrstorage(&localaddr2, protocol_ipaddr(remoteaddr), 0, version_ipaddr(remoteaddr)) ;
   }

   err = initsocket_helper(&new_ipsock, localaddr) ;
   if (err) goto ABBRUCH ;

   fd = new_ipsock ;

   err = fcntl(fd, F_GETFL) ;
   if (-1 != err) {
      long fdflags = err ;
      err = fcntl(fd, F_SETFL, fdflags|O_NONBLOCK) ;
   }
   if (-1 == err) {
      err = errno ;
      LOG_SYSERR("fcntl", err) ;
      goto ABBRUCH ;
   }

   err = connect(fd, remoteaddr->addr, remoteaddr->addrlen) ;
   if (err) {
      err = errno ;
      if (EINPROGRESS != err) {
         LOG_SYSERR("connect", err) ;
         LOG_INT(fd) ;
         goto ABBRUCH ;
      }
   }

   ipsockasync->ipsock = new_ipsock ;
   ipsockasync->err    = err ;
   return 0 ;
ABBRUCH:
   free_ipsocket(&new_ipsock) ;
   LOG_ABORT(err) ;
   return err ;
}

int convert_ipsocketasync(ipsocket_async_t * ipsockasync, /*out*/ipsocket_t * ipsock)
{
   int err ;
   int fd ;

   if (ipsockasync->err) {
      return ipsockasync->err ;
   }

   fd = fd_ipsocket(&ipsockasync->ipsock) ;

   err = fcntl(fd, F_GETFL) ;
   if (-1 != err) {
      long fdflags = err ;
      err = fcntl(fd, F_SETFL, (fdflags&(~O_NONBLOCK))) ;
   }
   if (-1 == err) {
      err = errno ;
      LOG_SYSERR("fcntl", err) ;
      goto ABBRUCH ;
   }

   // convert
   *ipsock             = ipsockasync->ipsock ;
   ipsockasync->ipsock = ipsocket_INIT_FREEABLE ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int success_ipsocketasync(ipsocket_async_t * ipsockasync)
{
   int err ;
   int fd ;
   struct pollfd  pollfds ;

   if (EINPROGRESS != ipsockasync->err) {
      return ipsockasync->err ;
   }

   fd      = fd_ipsocket(&ipsockasync->ipsock) ;
   pollfds = (struct pollfd) { .fd = fd, .events = POLLOUT } ;

   err = poll(&pollfds, 1, 0) ;
   if (-1 == err) {
      err = errno ;
      LOG_SYSERR("getsockopt", err) ;
      goto ABBRUCH ;
   }

   if (1 == err) {
      socklen_t len = sizeof(err) ;
      if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len)) {
         err = errno ;
         LOG_SYSERR("getsockopt", err) ;
         goto ABBRUCH ;
      }
      assert(len == sizeof(int)) ;

      if (  ! err
         && ! (pollfds.revents & POLLOUT)) {
         ipsockasync->err = EINVAL ;
      } else {
         ipsockasync->err = (err == EINPROGRESS ? EINVAL : err) ;
      }
   }

   return ipsockasync->err ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int waitms_ipsocketasync(ipsocket_async_t * ipsockasync, uint32_t millisec)
{
   int err ;
   int fd ;
   struct pollfd   pollfds ;
   struct timespec ts ;

   if (EINPROGRESS != ipsockasync->err) {
      return 0 ; // already completed
   }

   fd      = fd_ipsocket(&ipsockasync->ipsock) ;
   pollfds = (struct pollfd)   { .fd = fd, .events = POLLOUT } ;
   ts      = (struct timespec) { .tv_sec = (long) (millisec / 1000), .tv_nsec = 1000000 * (long) (millisec%1000) } ;

   err = ppoll(&pollfds, 1, &ts, 0) ;
   if (-1 == err) {
      err = errno ;
      LOG_SYSERR("poll", err) ;
      goto ABBRUCH ;
   }

   return (1 == err) ? 0 : EINPROGRESS ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}


// section: test

#ifdef KONFIG_UNITTEST

#define TEST(ARG) TEST_ONERROR_GOTO(ARG, ABBRUCH)

static int test_initfree(void)
{
   ipaddr_t   * ipaddr  = 0 ;
   ipaddr_t   * ipaddr2 = 0 ;
   ipsocket_t   ipsock  = ipsocket_INIT_FREEABLE ;
   cstring_t    name    = cstring_INIT ;

   // TEST static init
   TEST(-1 == ipsock) ;

   // TEST init, double free
   TEST(0 == newloopback_ipaddr(&ipaddr, ipprotocol_UDP, 0, ipversion_4)) ;
   TEST(0 == init_ipsocket(&ipsock, ipaddr)) ;
   TEST(0 < ipsock) ;
   TEST(0 == free_ipsocket(&ipsock)) ;
   TEST(-1 == ipsock) ;
   TEST(0 == free_ipsocket(&ipsock)) ;
   TEST(-1 == ipsock) ;
   TEST(0 == setprotocol_ipaddr(ipaddr, ipprotocol_TCP)) ;
   TEST(0 == initlisten_ipsocket(&ipsock, ipaddr, 1)) ;
   TEST(0 < ipsock) ;
   TEST(0 == free_ipsocket(&ipsock)) ;
   TEST(-1 == ipsock) ;
   TEST(0 == free_ipsocket(&ipsock)) ;
   TEST(-1 == ipsock) ;
   TEST(0 == delete_ipaddr(&ipaddr)) ;

   // TEST query getlocaladdr / isListen
   static const struct {
      ipprotocol_e   protocol ;
      const char   * addr ;
      uint16_t       port ;
      ipversion_e    version ;
   } testdata[8] = {
      { ipprotocol_UDP, "0.0.0.0", 0/*choose random*/, ipversion_4 },
      { ipprotocol_TCP, "0.0.0.0", 0/*choose random*/, ipversion_4 },
      { ipprotocol_UDP, "127.0.0.1", 31234, ipversion_4 },
      { ipprotocol_TCP, "127.0.0.1", 31236, ipversion_4 },
      { ipprotocol_UDP, "::", 0/*choose random*/, ipversion_6 },
      { ipprotocol_TCP, "::", 0/*choose random*/, ipversion_6 },
      { ipprotocol_UDP, "::1", 31234, ipversion_6 },
      { ipprotocol_TCP, "::1", 31236, ipversion_6 },
   } ;
   for(int i = 0; i < 8; ++i) {
      bool isListen = (ipprotocol_TCP == testdata[i].protocol) ;
      TEST(0 == new_ipaddr(&ipaddr, testdata[i].protocol, testdata[i].addr, testdata[i].port, testdata[i].version)) ;
      if (isListen ) {
         TEST(0 == initlisten_ipsocket(&ipsock, ipaddr, (uint16_t)(i+1))) ;
      } else {
         TEST(0 == init_ipsocket(&ipsock, ipaddr)) ;
      }
      TEST(0 < ipsock) ;
      TEST(isListen  == islisten_ipsocket(&ipsock)) ;
      TEST(testdata[i].protocol == protocol_ipsocket(&ipsock)) ;
      TEST(testdata[i].version  == version_ipsocket(&ipsock)) ;
      TEST(0 == isconnected_ipsocket(&ipsock)) ;
      TEST(0 == newcopy_ipaddr(&ipaddr2, ipaddr)) ;
      TEST(0 == localaddr_ipsocket(&ipsock, ipaddr2)) ;
      if (0 == testdata[i].port) {
         TEST(0 == port_ipaddr(ipaddr)) ;
         TEST(0  < port_ipaddr(ipaddr2)) ;
         TEST(0 == setport_ipaddr(ipaddr2, 0)) ;
      }
      TEST(0 == compare_ipaddr(ipaddr, ipaddr2)) ;
      TEST(ENOTCONN == remoteaddr_ipsocket(&ipsock, ipaddr2)) ;
      TEST(0 == delete_ipaddr(&ipaddr2)) ;
      TEST(0 == delete_ipaddr(&ipaddr)) ;
      TEST(0 == free_ipsocket(&ipsock)) ;
      TEST(ipsocket_INIT_FREEABLE == ipsock) ;
   }

   // TEST EINVAL (ipaddr_t != 0 && ipaddr_t->addr != 0 && ipaddr_t->addrlen != 0
   TEST(EINVAL == init_ipsocket(&ipsock, 0)) ;
   TEST(EINVAL == init_ipsocket(&ipsock, &(ipaddr_t){ .protocol = ipprotocol_TCP, .addrlen = 0 })) ;

   TEST(0 == free_cstring(&name)) ;

   return 0 ;
ABBRUCH:
   (void) free_cstring(&name) ;
   (void) delete_ipaddr(&ipaddr) ;
   (void) free_ipsocket(&ipsock) ;
   return EINVAL ;
}

static int test_connect(void)
{
   ipaddr_t  * ipaddr   = 0 ;
   ipaddr_t  * ipaddr2  = 0 ;
   cstring_t   name     = cstring_INIT ;
   ipsocket_t  ipsockCL = ipsocket_INIT_FREEABLE ;
   ipsocket_t  ipsockLT = ipsocket_INIT_FREEABLE ;
   ipsocket_t  ipsockSV = ipsocket_INIT_FREEABLE ;

   // TEST connect TCP
   for(unsigned islocal = 0; islocal < 2; ++islocal) {
      TEST(0 == newloopback_ipaddr(&ipaddr, ipprotocol_TCP, 0, ipversion_4 )) ;
      TEST(0 == initlisten_ipsocket(&ipsockLT, ipaddr, 1)) ;
      TEST(0 == isconnected_ipsocket(&ipsockLT)) ;
      TEST(0 == newcopy_ipaddr(&ipaddr2, ipaddr)) ;
      TEST(0 == localaddr_ipsocket(&ipsockLT, ipaddr2)) ;
      TEST(0 == initconnect_ipsocket(&ipsockCL, ipaddr2, islocal ? ipaddr : 0)) ;
      TEST(1 == isconnected_ipsocket(&ipsockCL)) ;
      TEST(0 == initwaitconnect_ipsocket(&ipsockSV, &ipsockLT, ipaddr)) ;
      TEST(0 == localaddr_ipsocket(&ipsockCL, ipaddr2)) ;
      TEST(0 == numericname_ipaddr(ipaddr2, &name)) ;
      TEST(0 == strcmp(str_cstring(&name), "127.0.0.1")) ;
      TEST(0 == compare_ipaddr(ipaddr, ipaddr2)) ;
      TEST(0 == remoteaddr_ipsocket(&ipsockCL, ipaddr)) ;
      TEST(0 == localaddr_ipsocket(&ipsockSV, ipaddr2)) ;
      TEST(0 == compare_ipaddr(ipaddr, ipaddr2)) ;
      TEST(0 == remoteaddr_ipsocket(&ipsockSV, ipaddr)) ;
      TEST(0 == localaddr_ipsocket(&ipsockCL, ipaddr2)) ;
      TEST(0 == compare_ipaddr(ipaddr, ipaddr2)) ;
      TEST(0 == delete_ipaddr(&ipaddr)) ;
      TEST(0 == delete_ipaddr(&ipaddr2)) ;
      TEST(0 == free_ipsocket(&ipsockCL)) ;
      TEST(0 == free_ipsocket(&ipsockLT)) ;
      TEST(0 == free_ipsocket(&ipsockSV)) ;
   }

   // TEST connect UDP
   for(unsigned islocal = 0; islocal < 2; ++islocal) {
      TEST(0 == newloopback_ipaddr(&ipaddr, ipprotocol_UDP, 0, ipversion_4)) ;
      TEST(0 == newloopback_ipaddr(&ipaddr2, ipprotocol_UDP, 12345, ipversion_4)) ;
      TEST(0 == initconnect_ipsocket(&ipsockCL, ipaddr2, islocal ? ipaddr : 0)) ;
      TEST(0 == localaddr_ipsocket(&ipsockCL, ipaddr)) ;
      TEST(0 == initconnect_ipsocket(&ipsockSV, ipaddr, ipaddr2)) ;
      TEST(1 == isconnected_ipsocket(&ipsockCL)) ;
      TEST(1 == isconnected_ipsocket(&ipsockSV)) ;
      TEST(0 == remoteaddr_ipsocket(&ipsockCL, ipaddr)) ;
      TEST(0 == localaddr_ipsocket(&ipsockSV, ipaddr2)) ;
      TEST(0 == compare_ipaddr(ipaddr, ipaddr2)) ;
      TEST(0 == remoteaddr_ipsocket(&ipsockSV, ipaddr)) ;
      TEST(0 == localaddr_ipsocket(&ipsockCL, ipaddr2)) ;
      TEST(0 == numericname_ipaddr(ipaddr2, &name)) ;
      TEST(0 == strcmp(str_cstring(&name), "127.0.0.1")) ;
      TEST(0 == compare_ipaddr(ipaddr, ipaddr2)) ;
      TEST(0 == delete_ipaddr(&ipaddr)) ;
      TEST(0 == delete_ipaddr(&ipaddr2)) ;
      TEST(0 == free_ipsocket(&ipsockCL)) ;
      TEST(0 == free_ipsocket(&ipsockSV)) ;
   }

   // TEST EINVAL (accept on TCP socket but not listener)
   TEST(0 == newloopback_ipaddr(&ipaddr, ipprotocol_TCP, 0, ipversion_4 )) ;
   TEST(0 == newcopy_ipaddr(&ipaddr2, ipaddr)) ;
   TEST(0 == initlisten_ipsocket(&ipsockLT, ipaddr, 1)) ;
   TEST(0 == localaddr_ipsocket(&ipsockLT, ipaddr2)) ;
   TEST(0 == setprotocol_ipaddr(ipaddr2, ipprotocol_UDP)) ;
   TEST(EINVAL == initconnect_ipsocket(&ipsockCL, ipaddr2, ipaddr)) ;
   TEST(0 == localaddr_ipsocket(&ipsockLT, ipaddr2)) ;
   TEST(0 == initconnect_ipsocket(&ipsockCL, ipaddr2, ipaddr)) ;
   TEST(EINVAL == initwaitconnect_ipsocket(&ipsockSV, &ipsockCL, ipaddr)) ;
   TEST(0 == delete_ipaddr(&ipaddr)) ;
   TEST(0 == delete_ipaddr(&ipaddr2)) ;
   TEST(0 == free_ipsocket(&ipsockLT)) ;
   TEST(0 == free_ipsocket(&ipsockCL)) ;

   // TEST EAFNOSUPPORT (different adddress versions)
   TEST(0 == newloopback_ipaddr(&ipaddr, ipprotocol_TCP, 0, ipversion_4 )) ;
   TEST(0 == newloopback_ipaddr(&ipaddr2, ipprotocol_TCP, 2000, ipversion_6 )) ;
   TEST(EAFNOSUPPORT == initconnect_ipsocket(&ipsockCL, ipaddr2, ipaddr)) ;
   TEST(0 == delete_ipaddr(&ipaddr)) ;
   TEST(0 == delete_ipaddr(&ipaddr2)) ;

   // TEST EOPNOTSUPP
   TEST(0 == newloopback_ipaddr(&ipaddr, ipprotocol_UDP, 0, ipversion_4)) ;
   TEST(EOPNOTSUPP == initlisten_ipsocket(&ipsockLT, ipaddr, 1)) ;
   TEST(0 == init_ipsocket(&ipsockLT, ipaddr)) ;
   TEST(EOPNOTSUPP == initwaitconnect_ipsocket(&ipsockSV, &ipsockLT, ipaddr)) ;
   TEST(0 == delete_ipaddr(&ipaddr)) ;
   TEST(0 == free_ipsocket(&ipsockLT)) ;


   TEST(0 == free_cstring(&name)) ;

   return 0 ;
ABBRUCH:
   (void) free_cstring(&name) ;
   (void) delete_ipaddr(&ipaddr) ;
   (void) free_ipsocket(&ipsockCL) ;
   (void) free_ipsocket(&ipsockLT) ;
   (void) free_ipsocket(&ipsockSV) ;
   return EINVAL ;
}

static int test_buffersize(void)
{
   ipaddr_t  * ipaddr   = 0 ;
   ipaddr_t  * ipaddr2  = 0 ;
   ipsocket_t  ipsockCL = ipsocket_INIT_FREEABLE ;
   ipsocket_t  ipsockLT = ipsocket_INIT_FREEABLE ;
   ipsocket_t  ipsockSV = ipsocket_INIT_FREEABLE ;
   void      * buffer   = 0 ;
   size_t      unsend_bytes ;
   size_t      unread_bytes ;


   for(unsigned i = 0; i < 3; ++i) {
      const unsigned  buffer_size  = 65536u * (i+1) ;
      const unsigned  sockbuf_size = 3 * buffer_size / 4 ;
      buffer = malloc(buffer_size) ;
      TEST(buffer) ;

         // connect TCP
      TEST(0 == newloopback_ipaddr(&ipaddr, ipprotocol_TCP, 0, ipversion_4)) ;
      TEST(0 == newcopy_ipaddr(&ipaddr2, ipaddr)) ;
      TEST(0 == initlisten_ipsocket(&ipsockLT, ipaddr, 1)) ;
      TEST(0 == localaddr_ipsocket(&ipsockLT, ipaddr2)) ;
      TEST(0 == initconnect_ipsocket(&ipsockCL, ipaddr2, ipaddr)) ;
      TEST(0 == delete_ipaddr(&ipaddr)) ;
      TEST(0 == delete_ipaddr(&ipaddr2)) ;
      TEST(0 == initwaitconnect_ipsocket(&ipsockSV, &ipsockLT, 0)) ;
      {
         // TEST setqueuesize_ipsocket( 0 ) does not change value
         size_t rsize = 0 ;
         size_t wsize = 0 ;
         size_t size = 0 ;
         TEST(0 == queuesizeread_ipsocket( &ipsockCL, &rsize)) ;
         TEST(0 == queuesizewrite_ipsocket( &ipsockCL, &wsize)) ;
         TEST(0 == setqueuesize_ipsocket( &ipsockCL, 0, wsize/2)) ;
         TEST(0 == queuesizeread_ipsocket( &ipsockCL, &size)) ;
         TEST(size == rsize) ;
         TEST(0 == queuesizewrite_ipsocket( &ipsockCL, &size)) ;
         TEST(size == wsize/2) ;
         TEST(0 == setqueuesize_ipsocket( &ipsockCL, rsize/2, 0)) ;
         TEST(0 == queuesizeread_ipsocket( &ipsockCL, &size)) ;
         TEST(size == rsize/2) ;
         TEST(0 == queuesizewrite_ipsocket( &ipsockCL, &size)) ;
         TEST(size == wsize/2) ;
      }
      // TEST setqueuesize_ipsocket(sockbuf_size,sockbuf_size)
      size_t size = 0 ;
      TEST(0 == setqueuesize_ipsocket( &ipsockCL, sockbuf_size, sockbuf_size)) ;
      TEST(0 == setqueuesize_ipsocket( &ipsockSV, sockbuf_size, sockbuf_size)) ;
      TEST(0 == queuesizeread_ipsocket( &ipsockCL, &size)) ;
      TEST(size == sockbuf_size) ;
      TEST(0 == queuesizeread_ipsocket( &ipsockSV, &size)) ;
      TEST(size == sockbuf_size) ;
      TEST(0 == queuesizewrite_ipsocket( &ipsockSV, &size)) ;
      TEST(size == sockbuf_size) ;
      TEST(0 == queuesizewrite_ipsocket( &ipsockCL, &size)) ;
      TEST(size == sockbuf_size) ;

      // TEST bytestoread_ipsocket, bytestowrite_ipsocket on connected socket without data read/written
      TEST(0 == bytestoread_ipsocket( &ipsockCL, &size)) ;
      TEST(0 == size) ;
      TEST(0 == bytestoread_ipsocket( &ipsockSV, &size)) ;
      TEST(0 == size) ;
      TEST(0 == bytestowrite_ipsocket( &ipsockCL, &size)) ;
      TEST(0 == size) ;
      TEST(0 == bytestowrite_ipsocket( &ipsockSV, &size)) ;
      TEST(0 == size) ;

      // TEST write transfers less than buffer_size cause of size of write queue
      {
         TEST(0 == write_ipsocket(&ipsockSV, buffer_size, buffer, &size)) ;
         TEST(0 < size && size < buffer_size) ;
         size_t writecount = size ;
         for(int si = 0; si < 100; ++si) {
            TEST(0 == bytestowrite_ipsocket( &ipsockSV, &unsend_bytes)) ;
            if (! unsend_bytes) break ;
            sleepms_thread(1) ;
         }
         // second write transfers all (cause send queue is empty)
         TEST(0 == write_ipsocket(&ipsockSV, buffer_size-writecount, writecount + (uint8_t*)buffer, &size)) ;
         TEST(size == buffer_size - writecount) ;
      }

      // TEST after read write queue is empty
      {
         TEST(0 == bytestoread_ipsocket(&ipsockCL, &unread_bytes)) ;
         TEST(0 <  unread_bytes) ;
         TEST(0 == bytestowrite_ipsocket(&ipsockSV, &unsend_bytes)) ;
         TEST(0 <  unsend_bytes) ;
         TEST(0 == read_ipsocket(&ipsockCL, unread_bytes, buffer, &size)) ;
         TEST(unread_bytes == size) ;
         size_t readcount = size ;
         for(int si = 0; si < 100; ++si) {
            TEST(0 == bytestowrite_ipsocket(&ipsockSV, &unsend_bytes)) ;
            if(!unsend_bytes) break ;
            sleepms_thread(1) ;
         }
         // check write queue is empty
         TEST(0 == bytestowrite_ipsocket(&ipsockSV, &unsend_bytes)) ;
         TEST(0 == unsend_bytes) ;
         // check second read transfers all data
         TEST(0 == bytestoread_ipsocket( &ipsockCL, &unread_bytes)) ;
         TEST(0 <  unread_bytes) ;
         TEST(0 == read_ipsocket(&ipsockCL, unread_bytes, buffer, &size)) ;
         TEST(unread_bytes == size) ;
         TEST(size == buffer_size - readcount) ;
         // check read queue empty
         TEST(0 == bytestoread_ipsocket( &ipsockCL, &unread_bytes)) ;
         TEST(0 == unread_bytes) ;
      }

      // TEST read on empty queue does not block
      if (0 == i) {
         TEST(EAGAIN == read_ipsocket(&ipsockCL, 1, buffer, &size)) ;
      }

      TEST(0 == free_ipsocket(&ipsockCL)) ;
      TEST(0 == free_ipsocket(&ipsockLT)) ;
      TEST(0 == free_ipsocket(&ipsockSV)) ;
      free(buffer) ;
      buffer = 0 ;
   }

   for(unsigned i = 0; i < 3; ++i) {
      const unsigned   buffer_size = 4*4096u + i * 4096u ;
      buffer = malloc(buffer_size) ;
      TEST(buffer) ;
      memset( buffer, 0, buffer_size ) ;
         // connect UDP
      TEST(0 == newloopback_ipaddr(&ipaddr, ipprotocol_UDP, 0, ipversion_4)) ;
      TEST(0 == newloopback_ipaddr(&ipaddr2, ipprotocol_UDP, (uint16_t)(10000+i), ipversion_4)) ;
      TEST(0 == initconnect_ipsocket(&ipsockCL, ipaddr2, ipaddr)) ;
      TEST(0 == localaddr_ipsocket(&ipsockCL, ipaddr)) ;
      TEST(0 == initconnect_ipsocket(&ipsockSV, ipaddr, ipaddr2)) ;
      TEST(0 == delete_ipaddr(&ipaddr)) ;
      TEST(0 == delete_ipaddr(&ipaddr2)) ;
      // TEST setqueuesize_ipsocket(buffer_size,buffer_size)
      TEST(0 == setqueuesize_ipsocket( &ipsockCL, buffer_size, buffer_size)) ;
      TEST(0 == setqueuesize_ipsocket( &ipsockSV, buffer_size, buffer_size)) ;
      size_t size = 0 ;
      TEST(0 == queuesizeread_ipsocket( &ipsockCL, &size)) ;
      TEST(size == buffer_size) ;  size = 0 ;
      TEST(0 == queuesizeread_ipsocket( &ipsockSV, &size)) ;
      TEST(size == buffer_size) ;  size = 0 ;
      TEST(0 == queuesizewrite_ipsocket( &ipsockSV, &size)) ;
      TEST(size == buffer_size) ;  size = 0 ;
      TEST(0 == queuesizewrite_ipsocket( &ipsockCL, &size)) ;
      TEST(size == buffer_size) ;

      // TEST bytestoread_ipsocket, bytestowrite_ipsocket on connected socket without data read/written
      TEST(0 == bytestoread_ipsocket( &ipsockCL, &size)) ;
      TEST(0 == size) ; size = 1 ;
      TEST(0 == bytestoread_ipsocket( &ipsockSV, &size)) ;
      TEST(0 == size) ; size = 1 ;
      TEST(0 == bytestowrite_ipsocket( &ipsockCL, &size)) ;
      TEST(0 == size) ; size = 1 ;
      TEST(0 == bytestowrite_ipsocket( &ipsockSV, &size)) ;
      TEST(0 == size) ;

      // TEST datagram equal tp buffer_size will be discarded on receiver side (buffer stores also control info)
      TEST(0 == write_ipsocket(&ipsockSV, buffer_size/4, (uint8_t*)buffer, &size)) ;
      TEST(buffer_size/4 == size) ;
      TEST(0 == write_ipsocket(&ipsockSV, buffer_size/4, (uint8_t*)buffer, &size)) ;
      TEST(buffer_size/4 == size) ;
         // third datagram will be discarded on receiver side
      TEST(0 == write_ipsocket(&ipsockSV, buffer_size/2, (uint8_t*)buffer, &size)) ;
      TEST(buffer_size/2 == size) ;
      sleepms_thread(1) ;
      TEST(0 == bytestowrite_ipsocket( &ipsockSV, &unsend_bytes)) ;
      TEST(0 == unsend_bytes) ;
      // TEST bytestoread_ipsocket returns size of 1 datagram (not the sum of all)
      TEST(0 == bytestoread_ipsocket( &ipsockCL, &unread_bytes)) ;
      TEST(buffer_size/4 == unread_bytes) ;
         // read first
      TEST(0 == read_ipsocket(&ipsockCL, unread_bytes, buffer, &size)) ;
      TEST(buffer_size/4 == size) ;
         // read second
      TEST(0 == bytestoread_ipsocket( &ipsockCL, &unread_bytes)) ;
      TEST(buffer_size/4 == unread_bytes) ;
      TEST(0 == read_ipsocket(&ipsockCL, unread_bytes, buffer, &size)) ;
      TEST(buffer_size/4 == size) ;
         // third was -- silently -- discarded
      TEST(0 == bytestoread_ipsocket( &ipsockCL, &unread_bytes)) ;
      TEST(0 == unread_bytes) ;
      TEST(0 == bytestowrite_ipsocket( &ipsockSV, &unsend_bytes)) ;
      TEST(0 == unsend_bytes) ;

      // TEST read on empty queue does not block
      if (0 == i)  {
         TEST(EAGAIN == read_ipsocket(&ipsockCL, 1, buffer, &size)) ;
      }

      TEST(0 == free_ipsocket(&ipsockCL)) ;
      TEST(0 == free_ipsocket(&ipsockSV)) ;
      free(buffer) ;
      buffer = 0 ;
   }

   return 0 ;
ABBRUCH:
   free(buffer) ;
   (void) delete_ipaddr(&ipaddr) ;
   (void) free_ipsocket(&ipsockCL) ;
   (void) free_ipsocket(&ipsockLT) ;
   (void) free_ipsocket(&ipsockSV) ;
   return EINVAL ;
}

static int test_helper_oob(ipsocket_t * ipsockSV, ipsocket_t * ipsockCL, const size_t buffer_size, uint8_t * buffer)
{
   size_t size ;
   size_t oob_offset ;
   size_t unsend_bytes ;
   size_t unread_bytes ;

   // TEST send & receive oob data (in the mid)
   TEST(0 == write_ipsocket(ipsockSV, buffer_size/2, (uint8_t*)buffer, &size)) ;
   TEST(buffer_size/2 == size) ;
   TEST(0 == writeoob_ipsocket(ipsockSV, 'x')) ;
   TEST(0 == write_ipsocket(ipsockSV, buffer_size/2, (uint8_t*)buffer, &size)) ;
   TEST(buffer_size/2 == size) ;
   sleepms_thread(10) ;
   TEST(0 == bytestowrite_ipsocket( ipsockSV, &unsend_bytes)) ;
   TEST(0 == bytestoread_ipsocket( ipsockCL, &unread_bytes)) ;
   TEST(0 == unsend_bytes && unread_bytes == buffer_size+1) ;
   buffer[buffer_size/2] = 0 ;
   TEST(0 == readoob_ipsocket(ipsockCL, buffer_size/2+2, buffer, &size, &oob_offset)) ;
   TEST(buffer_size/2 + 2 == size) ;
   TEST(oob_offset == buffer_size/2)   // marks FOUND out of band data
   TEST('x' == buffer[oob_offset]) ;
   TEST(0 == readoob_ipsocket(ipsockCL, buffer_size/2-1, buffer, &size, &oob_offset)) ;
   TEST(buffer_size/2 - 1 == size) ;
   TEST(size == oob_offset) ;          // marks NO out of band data
   TEST(0 == bytestoread_ipsocket(ipsockCL, &unread_bytes)) ;
   TEST(0 == unread_bytes) ;
   TEST(0 == bytestowrite_ipsocket(ipsockSV, &unsend_bytes)) ;
   TEST(0 == unsend_bytes) ;
   TEST(EAGAIN == readoob_ipsocket(ipsockCL, 1, buffer, &size, &oob_offset)) ;
      // send & receive oob data (at the beginning)
   TEST(0 == writeoob_ipsocket(ipsockSV, 'x')) ;
   TEST(0 == write_ipsocket(ipsockSV, buffer_size-1, (uint8_t*)buffer, &size)) ;
   TEST(buffer_size-1 == size) ;
   sleepms_thread(10) ;
   TEST(0 == bytestowrite_ipsocket(ipsockSV, &unsend_bytes)) ;
   TEST(0 == bytestoread_ipsocket(ipsockCL, &unread_bytes)) ;
   TEST(0 == unsend_bytes && unread_bytes == buffer_size) ;
   buffer[0] = 0 ;
   TEST(0 == readoob_ipsocket(ipsockCL, buffer_size, buffer, &size, &oob_offset)) ;
   TEST(buffer_size == size) ;
   TEST(oob_offset  == 0) ;   // marks FOUND out of band data
   TEST('x' == buffer[oob_offset]) ;
      // send & receive 2 * oob data (first data is overwritten with second time)
   TEST(0 == writeoob_ipsocket(ipsockSV, 'x')) ;
   TEST(0 == write_ipsocket(ipsockSV, buffer_size-2, (uint8_t*)buffer, &size)) ;
   TEST(0 == writeoob_ipsocket(ipsockSV, 'y')) ;
   TEST(buffer_size-2 == size) ;
   sleepms_thread(10) ;
   TEST(0 == bytestowrite_ipsocket(ipsockSV, &unsend_bytes)) ;
   TEST(0 == bytestoread_ipsocket(ipsockCL, &unread_bytes)) ;
   TEST(0 == unsend_bytes && unread_bytes == buffer_size) ;
   buffer[buffer_size-1] = buffer[0] = 0 ;
   TEST(0 == readoob_ipsocket(ipsockCL, buffer_size, buffer, &size, &oob_offset)) ;
   TEST(buffer_size == size) ;
   TEST(oob_offset  == buffer_size-1) ;   // marks FOUND out of band data
   TEST('x' == buffer[0]) ;               // oob state of 'x' is lost 1
   TEST('y' == buffer[oob_offset]) ;      // oob state of 'y' is the newest !

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

static int test_outofbandData(void)
{
   ipaddr_t     * ipaddr      = 0 ;
   ipaddr_t     * ipaddr2     = 0 ;
   ipsocket_t     ipsockCL    = ipsocket_INIT_FREEABLE ;
   ipsocket_t     ipsockLT    = ipsocket_INIT_FREEABLE ;
   ipsocket_t     ipsockSV    = ipsocket_INIT_FREEABLE ;
   size_t         oob_offset  = 0 ;
   const size_t   buffer_size = 512u  ;
   uint8_t      * buffer      = (uint8_t*) malloc(buffer_size) ;
   size_t         size ;
   size_t         unsend_bytes ;
   size_t         unread_bytes ;

   TEST(buffer) ;
   memset( buffer, 0, buffer_size ) ;

   // TEST TCP oob
   TEST(0 == newloopback_ipaddr(&ipaddr, ipprotocol_TCP, 0, ipversion_4 )) ;
   TEST(0 == newcopy_ipaddr(&ipaddr2, ipaddr)) ;
   TEST(0 == initlisten_ipsocket(&ipsockLT, ipaddr, 1)) ;
   TEST(0 == localaddr_ipsocket(&ipsockLT, ipaddr2)) ;
   TEST(0 == initconnect_ipsocket(&ipsockCL, ipaddr2, ipaddr)) ;
   TEST(0 == delete_ipaddr(&ipaddr)) ;
   TEST(0 == delete_ipaddr(&ipaddr2)) ;
   TEST(0 == initwaitconnect_ipsocket(&ipsockSV, &ipsockLT, 0)) ;

   TEST(0 == test_helper_oob(&ipsockSV, &ipsockCL, buffer_size, buffer)) ;
   TEST(0 == test_helper_oob(&ipsockCL, &ipsockSV, buffer_size, buffer)) ;

   TEST(0 == free_ipsocket(&ipsockCL)) ;
   TEST(0 == free_ipsocket(&ipsockLT)) ;
   TEST(0 == free_ipsocket(&ipsockSV)) ;

   // TEST UDP oob
   TEST(0 == newloopback_ipaddr(&ipaddr, ipprotocol_UDP, 0, ipversion_4)) ;
   TEST(0 == newloopback_ipaddr(&ipaddr2, ipprotocol_UDP, 20000, ipversion_4)) ;
   TEST(0 == initconnect_ipsocket(&ipsockCL, ipaddr2, ipaddr)) ;
   TEST(0 == localaddr_ipsocket(&ipsockCL, ipaddr)) ;
   TEST(0 == initconnect_ipsocket(&ipsockSV, ipaddr, ipaddr2)) ;
   TEST(0 == delete_ipaddr(&ipaddr)) ;
   TEST(0 == delete_ipaddr(&ipaddr2)) ;
      // writing oob is an EOPNOTSUPP
   TEST(0 == write_ipsocket(&ipsockSV, 3, (const uint8_t*)"abc", &size)) ;
   TEST(3 == size) ;
   TEST(EOPNOTSUPP == writeoob_ipsocket(&ipsockSV, 'd')) ;
      // reading oob is also an EOPNOTSUPP
   TEST(0 == bytestowrite_ipsocket( &ipsockSV, &unsend_bytes)) ;
   TEST(0 == bytestoread_ipsocket( &ipsockCL, &unread_bytes)) ;
   TEST(0 == unsend_bytes && 3 == unread_bytes) ;
   TEST(EOPNOTSUPP == readoob_ipsocket(&ipsockCL, unread_bytes, buffer, &size, &oob_offset)) ;
      // reading normal works
   TEST(0 == read_ipsocket(&ipsockCL, unread_bytes, buffer, &size)) ;
   TEST(unread_bytes == size) ;
   TEST(0 == strncmp( (char*)buffer, "abc", 3)) ;
   TEST(EAGAIN == read_ipsocket(&ipsockCL, 1, buffer, &size)) ;  // test buffer is empty

   TEST(0 == free_ipsocket(&ipsockCL)) ;
   TEST(0 == free_ipsocket(&ipsockSV)) ;
   free(buffer) ;

   return 0 ;
ABBRUCH:
   free(buffer) ;
   (void) delete_ipaddr(&ipaddr) ;
   (void) free_ipsocket(&ipsockCL) ;
   (void) free_ipsocket(&ipsockLT) ;
   (void) free_ipsocket(&ipsockSV) ;
   return EINVAL ;
}

int test_udpIO(void)
{
   ipaddr_t     * ipaddr       = 0 ;
   ipaddr_t     * ipaddr2      = 0 ;
   ipsocket_t     ipsockCL[2]  = { ipsocket_INIT_FREEABLE } ;
   ipsocket_t     ipsockSV[10] = { ipsocket_INIT_FREEABLE } ;
   cstring_t      name         = cstring_INIT ;
   const size_t   buffer_size  = 512u  ;
   uint8_t      * buffer       = (uint8_t*) malloc(buffer_size) ;
   uint16_t       portCL[nrelementsof(ipsockCL)] ;
   uint16_t       portSV[nrelementsof(ipsockSV)] ;
   size_t         size ;

   TEST(buffer) ;

   // TEST ipversion_4, ipversion_6
   static_assert( ipversion_4 < ipversion_6, "" ) ;
   for(ipversion_e version = ipversion_4; version <= ipversion_6; version = (ipversion_e) (version + (ipversion_6-ipversion_4)) ) {

      // TEST connected send&receive (messages from other client discarded)
      TEST(0 == newloopback_ipaddr(&ipaddr, ipprotocol_UDP, 0, version)) ;
      TEST(0 == newcopy_ipaddr(&ipaddr2, ipaddr)) ;
      for(uint16_t i = 0; i < nrelementsof(ipsockCL); ++i) {
         TEST(0 == init_ipsocket(&ipsockCL[i], ipaddr)) ;
         TEST(0 == localaddr_ipsocket(&ipsockCL[i], ipaddr2)) ;
         portCL[i] = port_ipaddr(ipaddr2) ;
      }
      for(uint16_t i = 0; i < nrelementsof(ipsockSV); ++i) {
         unsigned ci = (i % nrelementsof(ipsockCL)) ;
         TEST(0 == setport_ipaddr(ipaddr2, portCL[ci])) ;
         TEST(0 == initconnect_ipsocket(&ipsockSV[i], ipaddr2, ipaddr)) ;
         TEST(0 == localaddr_ipsocket(&ipsockSV[i], ipaddr2)) ;
         portSV[i] = port_ipaddr(ipaddr2) ;
      }

      for(uint16_t i = 0; i < nrelementsof(ipsockSV); ++i) {
         memset( buffer, (int)i, buffer_size) ;
         for(uint16_t ci = 0; ci < nrelementsof(ipsockCL); ++ci) {
            TEST(0 == setport_ipaddr(ipaddr2, portSV[i])) ;
            TEST(0 == writeto_ipsocket(&ipsockCL[ci], ipaddr2, buffer_size, buffer, &size)) ;
            TEST(buffer_size == size) ;
         }
      }

      for(uint16_t i = 0; i < nrelementsof(ipsockSV); ++i) {
         unsigned ci = (i % nrelementsof(ipsockCL)) ;
         TEST(0 == bytestoread_ipsocket(&ipsockSV[i], &size)) ;
         TEST(buffer_size == size) ;
         TEST(0 == readfrom_ipsocket(&ipsockSV[i], ipaddr, buffer_size, buffer, &size)) ;
         TEST(buffer_size == size) ;
         for(unsigned b = 0; b < buffer_size; ++b) {
            TEST(buffer[b] == i) ;
         }
         TEST(0 == setport_ipaddr(ipaddr2, portCL[ci])) ;
         TEST(0 == compare_ipaddr(ipaddr, ipaddr2)) ;
      }
      for(uint16_t i = 0; i < nrelementsof(ipsockSV); ++i) {
         TEST(0 == bytestoread_ipsocket(&ipsockSV[i], &size)) ;
         TEST(0 == size) ;
      }

      // TEST unconnected send & receive
      TEST(0 == setport_ipaddr(ipaddr, 0)) ;
      for(uint16_t i = 0; i < nrelementsof(ipsockCL); ++i) {
         TEST(0 == free_ipsocket(&ipsockCL[i])) ;
         TEST(0 == init_ipsocket(&ipsockCL[i], ipaddr)) ;
         TEST(0 == localaddr_ipsocket(&ipsockCL[i], ipaddr2)) ;
         portCL[i] = port_ipaddr(ipaddr2) ;
      }
      for(uint16_t i = 0; i < nrelementsof(ipsockSV); ++i) {
         TEST(0 == free_ipsocket(&ipsockSV[i])) ;
         TEST(0 == init_ipsocket(&ipsockSV[i], ipaddr)) ;
         TEST(0 == localaddr_ipsocket(&ipsockSV[i], ipaddr2)) ;
         portSV[i] = port_ipaddr(ipaddr2) ;
      }

      for(uint16_t i = 0; i < nrelementsof(ipsockSV); ++i) {
         memset( buffer, (int)i, buffer_size) ;
         for(uint16_t ci = 0; ci < nrelementsof(ipsockCL); ++ci) {
            TEST(0 == setport_ipaddr(ipaddr2, portSV[i])) ;
            TEST(0 == writeto_ipsocket(&ipsockCL[ci], ipaddr2, buffer_size, buffer, &size)) ;
            TEST(buffer_size == size) ;
         }
      }

      for(uint16_t i = 0; i < nrelementsof(ipsockSV); ++i) {
         for(uint16_t ci = 0; ci < nrelementsof(ipsockCL); ++ci) {
            TEST(0 == bytestoread_ipsocket(&ipsockSV[i], &size)) ;
            TEST(buffer_size == size) ;
            TEST(0 == readfrom_ipsocket(&ipsockSV[i], ipaddr, buffer_size, buffer, &size)) ;
            TEST(buffer_size == size) ;
            for(unsigned b = 0; b < buffer_size; ++b) {
               TEST(buffer[b] == i) ;
            }
            TEST(0 == setport_ipaddr(ipaddr2, portCL[ci])) ;
            TEST(0 == compare_ipaddr(ipaddr, ipaddr2)) ;
         }
      }
      for(uint16_t i = 0; i < nrelementsof(ipsockSV); ++i) {
         TEST(0 == bytestoread_ipsocket(&ipsockSV[i], &size)) ;
         TEST(0 == size) ;
      }

      // TEST EAFNOSUPPORT (ipaddr is of wrong version)
      TEST(0 == delete_ipaddr(&ipaddr)) ;
      TEST(0 == newloopback_ipaddr(&ipaddr, ipprotocol_UDP, 0, version == ipversion_4 ? ipversion_6 : ipversion_4 )) ;
      TEST(EAFNOSUPPORT == writeto_ipsocket(&ipsockSV[0], ipaddr, buffer_size, buffer, &size)) ;
      TEST(EAFNOSUPPORT == readfrom_ipsocket(&ipsockSV[0], ipaddr, buffer_size, buffer, &size)) ;

      // TEST EPROTONOSUPPORT (ipaddr contains wrong protocol)
      TEST(0 == delete_ipaddr(&ipaddr)) ;
      TEST(0 == newloopback_ipaddr(&ipaddr, ipprotocol_TCP, 0, version)) ;
      TEST(EPROTONOSUPPORT == writeto_ipsocket(&ipsockSV[0], ipaddr, buffer_size, buffer, &size)) ;

      // TEST close
      for(uint16_t i = 0; i < nrelementsof(ipsockCL); ++i) {
         TEST(0 == free_ipsocket(&ipsockCL[i])) ;
      }
      for(uint16_t i = 0; i < nrelementsof(ipsockSV); ++i) {
         TEST(0 == free_ipsocket(&ipsockSV[i])) ;
      }

      TEST(0 == delete_ipaddr(&ipaddr)) ;
      TEST(0 == delete_ipaddr(&ipaddr2)) ;
   }

   TEST(0 == free_cstring(&name)) ;
   free(buffer) ;
   return 0 ;
ABBRUCH:
   free(buffer) ;
   (void) free_cstring(&name) ;
   (void) delete_ipaddr(&ipaddr) ;
   for(uint16_t i = 0; i < nrelementsof(ipsockCL); ++i) {
      free_ipsocket(&ipsockCL[i]) ;
   }
   for(uint16_t i = 0; i < nrelementsof(ipsockSV); ++i) {
      free_ipsocket(&ipsockSV[i]) ;
   }
   return EINVAL ;
}

static int test_async(void)
{
   ipsocket_t        iplisten = ipsocket_INIT_FREEABLE ;
   ipsocket_t        ipsock1  = ipsocket_INIT_FREEABLE ;
   ipsocket_t        ipsock2  = ipsocket_INIT_FREEABLE ;
   ipaddr_t        * ipaddr   = 0 ;
   ipaddr_t        * ipaddr2  = 0 ;
   ipsocket_async_t  ipasync  = ipsocket_async_INIT_FREEABLE ;
   uint8_t           buffer[100] ;
   size_t            size ;

   TEST(0 == newloopback_ipaddr(&ipaddr, ipprotocol_TCP, 0, ipversion_4)) ;
   TEST(0 == newloopback_ipaddr(&ipaddr2, ipprotocol_TCP, 2000, ipversion_4)) ;
   TEST(0 == initlisten_ipsocket(&iplisten, ipaddr, 1)) ;

   // TEST static init
   TEST(-1 == ipasync.ipsock) ;
   TEST(0 == ipasync.err) ;

   // TEST TCP init, double free
   TEST(0 == initconnect_ipsocketasync(&ipasync, ipaddr2, ipaddr)) ;
   TEST(EINPROGRESS == ipasync.err) ;
   TEST(0 <  ipasync.ipsock) ;
   TEST(0 == free_ipsocketasync(&ipasync)) ;
   TEST(-1 == ipasync.ipsock) ;
   TEST(0 == ipasync.err) ;
   TEST(0 == free_ipsocketasync(&ipasync)) ;
   TEST(-1 == ipasync.ipsock) ;
   TEST(0 == ipasync.err) ;

   // TEST TCP async connect
   for(unsigned islocal = 0; islocal < 2; ++islocal) {
      TEST(0 == localaddr_ipsocket(&iplisten, ipaddr2)) ;
      TEST(0 == initconnect_ipsocketasync(&ipasync, ipaddr2, islocal ? ipaddr : 0)) ;
      TEST(EINPROGRESS == ipasync.err) ;
      TEST(0 <  ipasync.ipsock) ;
      TEST(0 == waitms_ipsocketasync(&ipasync, 100)) ;
      TEST(0 == success_ipsocketasync(&ipasync)) ;
      TEST(0 == convert_ipsocketasync(&ipasync, &ipsock1)) ;
      TEST(-1 == ipasync.ipsock) ;
      TEST(0 == ipasync.err) ;
      TEST(0 == initwaitconnect_ipsocket(&ipsock2, &iplisten, 0)) ;
      TEST(0 == write_ipsocket(&ipsock1, sizeof(buffer), buffer, &size)) ;
      TEST(sizeof(buffer) == size) ;
      TEST(0 == read_ipsocket(&ipsock2, sizeof(buffer), buffer, &size)) ;
      TEST(sizeof(buffer) == size) ;
      TEST(0 == free_ipsocket(&ipsock1)) ;
      TEST(0 == free_ipsocket(&ipsock2)) ;
   }

   // TEST UDP (completes immediately)
   for(unsigned islocal = 0; islocal < 2; ++islocal) {
      TEST(0 == localaddr_ipsocket(&iplisten, ipaddr2)) ;
      TEST(0 == setprotocol_ipaddr(ipaddr, ipprotocol_UDP)) ;
      TEST(0 == setprotocol_ipaddr(ipaddr2, ipprotocol_UDP)) ;
      TEST(0 == initconnect_ipsocketasync(&ipasync, ipaddr2, islocal ? ipaddr : 0)) ;
      TEST(0 == ipasync.err) ;
      TEST(0 <  ipasync.ipsock) ;
      TEST(0 == success_ipsocketasync(&ipasync)) ;
      TEST(0 == waitms_ipsocketasync(&ipasync, 0)) ;
      TEST(0 == convert_ipsocketasync(&ipasync, &ipsock1)) ;
      TEST(-1 == ipasync.ipsock) ;
      TEST(0 == ipasync.err) ;
      TEST(0 == localaddr_ipsocket(&ipsock1, ipaddr)) ;
      TEST(0 == initconnect_ipsocket(&ipsock2, ipaddr, ipaddr2)) ;
      TEST(0 == write_ipsocket(&ipsock1, sizeof(buffer), buffer, &size)) ;
      TEST(sizeof(buffer) == size) ;
      TEST(0 == read_ipsocket(&ipsock2, sizeof(buffer), buffer, &size)) ;
      TEST(sizeof(buffer) == size) ;
      TEST(0 == free_ipsocket(&ipsock1)) ;
      TEST(0 == free_ipsocket(&ipsock2)) ;
   }

   // TEST TCP ECONNREFUSED
   TEST(0 == setprotocol_ipaddr(ipaddr, ipprotocol_TCP)) ;
   TEST(0 == localaddr_ipsocket(&iplisten, ipaddr2)) ;
   TEST(0 == setport_ipaddr(ipaddr2, 2000)) ;
   TEST(0 == initconnect_ipsocketasync(&ipasync, ipaddr, ipaddr2)) ;
   TEST(EINPROGRESS == ipasync.err) ;
   TEST(0 <  ipasync.ipsock) ;
   TEST(0 == waitms_ipsocketasync(&ipasync, 100)) ;
   TEST(ECONNREFUSED == success_ipsocketasync(&ipasync)) ;
   TEST(0 == free_ipsocketasync(&ipasync)) ;
   TEST(-1 == ipasync.ipsock) ;
   TEST(0 == ipasync.err) ;

   // TEST EINVAL (different protocols)
   TEST(0 == localaddr_ipsocket(&iplisten, ipaddr2)) ;
   TEST(0 == setprotocol_ipaddr(ipaddr, ipprotocol_UDP)) ;
   TEST(EINVAL == initconnect_ipsocketasync(&ipasync, ipaddr, ipaddr2)) ;

   // TEST EAFNOSUPPORT (different versions)
   TEST(0 == delete_ipaddr(&ipaddr)) ;
   TEST(0 == newloopback_ipaddr(&ipaddr, ipprotocol_UDP, 0, ipversion_6)) ;
   TEST(0 == localaddr_ipsocket(&iplisten, ipaddr2)) ;
   TEST(0 == setprotocol_ipaddr(ipaddr2, ipprotocol_UDP)) ;
   TEST(EAFNOSUPPORT == initconnect_ipsocketasync(&ipasync, ipaddr, ipaddr2)) ;

   TEST(0 == free_ipsocket(&iplisten)) ;
   TEST(0 == delete_ipaddr(&ipaddr)) ;
   TEST(0 == delete_ipaddr(&ipaddr2)) ;
   return 0 ;
ABBRUCH:
   free_ipsocket(&iplisten) ;
   free_ipsocket(&ipsock1) ;
   free_ipsocket(&ipsock1) ;
   free_ipsocketasync(&ipasync) ;
   delete_ipaddr(&ipaddr) ;
   delete_ipaddr(&ipaddr2) ;
   return EINVAL ;
}

int unittest_io_ipsocket()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())       goto ABBRUCH ;
   if (test_connect())        goto ABBRUCH ;
   if (test_buffersize())     goto ABBRUCH ;
   if (test_outofbandData())  goto ABBRUCH ;
   if (test_udpIO())          goto ABBRUCH ;
   if (test_async())          goto ABBRUCH ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif

