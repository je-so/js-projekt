/* title: Internetprotocol-Sockets Linux
   Impplements <Internetprotocol-Sockets> on Linux.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/io/ip/ipsocket.h
    Header file of <Internetprotocol-Sockets>.

   file: C-kern/platform/Linux/io/ipsocket.c
    Linux specific implementation <Internetprotocol-Sockets Linux>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/io/ip/ipsocket.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/ip/ipaddr.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/platform/task/thread.h"
#endif


// section: ipsocket_t

// group: helper

static int waitconnect(ipsocket_t ipsock)
{
   int err;
   struct pollfd pollfds;

   pollfds = (struct pollfd) { .fd = ipsock, .events = POLLOUT };

   for (;;) {
      err = poll(&pollfds, 1, -1/*wait indefinitely*/);
      if (err == -1) {
         err = errno;
         if (err == EINTR) continue;
         TRACESYSCALL_ERRLOG("poll", err) ;
         PRINTINT_ERRLOG(ipsock);
         goto ONERR;
      }

      break;
   }

   socklen_t len = sizeof(err);
   if (getsockopt(ipsock, SOL_SOCKET, SO_ERROR, &err, &len)) {
      err = errno;
      TRACESYSCALL_ERRLOG("getsockopt", err);
      goto ONERR;
   }

   if (     ! err
         && ! (pollfds.revents & POLLOUT)) {
      err = EPROTO;
   }

ONERR:
   return err;
}

// group: lifetime

int free_ipsocket(ipsocket_t * ipsock)
{
   int err ;
   int fd = *ipsock;

   if (fd != ipsocket_FREE) {
      *ipsock = ipsocket_FREE;

      err = close(fd);
      if (err) {
         err = errno;
         TRACESYSCALL_ERRLOG("close", err);
         PRINTINT_ERRLOG(fd);
         goto ONERR;
      }
   }

   return 0 ;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

int initsocket_helper(/*out*/ipsocket_t * ipsock, const ipaddr_t * localaddr)
{
   int err;
   int fd = -1;
   int socktype;

   switch(protocol_ipaddr(localaddr)) {
   case ipprotocol_ANY: socktype = SOCK_RAW ;   break ;
   case ipprotocol_UDP: socktype = SOCK_DGRAM ; break ;
   case ipprotocol_TCP: socktype = SOCK_STREAM; break ;
   default:  err = EPROTONOSUPPORT ; goto ONERR;
   }

   fd = socket(version_ipaddr(localaddr), socktype|SOCK_NONBLOCK|SOCK_CLOEXEC, protocol_ipaddr(localaddr)) ;
   if (-1 == fd) {
      err = errno ;
      TRACESYSCALL_ERRLOG("socket", err) ;
      goto ONERR_LOG;
   }

   struct linger l = { .l_onoff = 0, .l_linger = 0 } ;
   if (setsockopt(fd, SOL_SOCKET, SO_LINGER, &l, sizeof(l))) {
      err = errno ;
      TRACESYSCALL_ERRLOG("setsockopt(SO_LINGER)", err) ;
      goto ONERR_LOG;
   }

   if (ipprotocol_TCP == protocol_ipaddr(localaddr)) {
      const int on = 1 ;
      if (setsockopt(fd, SOL_SOCKET, SO_OOBINLINE, &on, sizeof(on))) {
         err = errno ;
         TRACESYSCALL_ERRLOG("setsockopt(SO_OOBINLINE)", err) ;
         goto ONERR_LOG;
      }
   }

   if (bind(fd, localaddr->addr, localaddr->addrlen)) {
      err = errno ;
      TRACESYSCALL_ERRLOG("bind", err) ;
      goto ONERR_LOG;
   }

   // out param
   *ipsock = fd ;

   return 0 ;
ONERR_LOG:
   logurl_ipaddr(localaddr, "local", log_channel_ERR) ;
ONERR:
   free_ipsocket(&fd);
   TRACEEXIT_ERRLOG(err);
   return err ;
}

int init_ipsocket(/*out*/ipsocket_t * ipsock, const ipaddr_t * localaddr)
{
   int err ;

   VALIDATE_INPARAM_TEST(isvalid_ipaddr(localaddr), ONERR, ) ;

   if (ipprotocol_UDP != protocol_ipaddr(localaddr)) {
      err = EPROTONOSUPPORT ;
      goto ONERR;
   }

   err = initsocket_helper(ipsock, localaddr);
   if (err) goto ONERR;

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}

int initconnectasync_ipsocket(/*out*/ipsocket_t * ipsock, const struct ipaddr_t * remoteaddr, const struct ipaddr_t * localaddr)
{
   int err ;
   ipsocket_t        new_ipsock = ipsocket_FREE;
   ipaddr_storage_t  localaddr2;

   VALIDATE_INPARAM_TEST(isvalid_ipaddr(remoteaddr), ONERR, );
   if (localaddr) {
      VALIDATE_INPARAM_TEST(isvalid_ipaddr(localaddr), ONERR, );
      VALIDATE_INPARAM_TEST(protocol_ipaddr(localaddr) == protocol_ipaddr(remoteaddr), ONERR, );
   } else {
      localaddr = initany_ipaddrstorage(&localaddr2, protocol_ipaddr(remoteaddr), 0, version_ipaddr(remoteaddr));
   }

   err = initsocket_helper(&new_ipsock, localaddr);
   if (err) goto ONERR;

   err = connect(new_ipsock, remoteaddr->addr, remoteaddr->addrlen);
   if (err) {
      err = errno;
      if (err != EINPROGRESS) {
         TRACESYSCALL_ERRLOG("connect", err);
         PRINTINT_ERRLOG(new_ipsock);
         goto ONERR;
      }
   }

   // set out param
   *ipsock = new_ipsock;

   return 0;
ONERR:
   free_ipsocket(&new_ipsock);
   TRACEEXIT_ERRLOG(err);
   return err;
}

int initconnect_ipsocket(/*out*/ipsocket_t * ipsock, const struct ipaddr_t * remoteaddr, const struct ipaddr_t * localaddr)
{
   int err ;
   ipsocket_t new_ipsock = ipsocket_FREE;

   err = initconnectasync_ipsocket(&new_ipsock, remoteaddr, localaddr);
   if (err) goto ONERR;

   err = waitconnect(new_ipsock);
   if (err) goto ONERR;

   // set out param
   *ipsock = new_ipsock;

   return 0;
ONERR:
   free_ipsocket(&new_ipsock);
   TRACEEXIT_ERRLOG(err);
   return err;
}

int initlisten_ipsocket(/*out*/ipsocket_t * ipsock, const struct ipaddr_t * localaddr, uint16_t max_outstanding_connections)
{
   int err ;
   int fd ;
   ipsocket_t new_ipsock = ipsocket_FREE;

   VALIDATE_INPARAM_TEST(isvalid_ipaddr(localaddr), ONERR, );

   err = initsocket_helper(&new_ipsock, localaddr);
   if (err) goto ONERR;

   fd = new_ipsock ;

   if (listen(fd, max_outstanding_connections)) {
      err = errno ;
      TRACESYSCALL_ERRLOG("listen", err) ;
      PRINTINT_ERRLOG(fd) ;
      PRINTINT_ERRLOG(max_outstanding_connections) ;
      goto ONERR;
   }

   *ipsock = new_ipsock ;
   return 0 ;
ONERR:
   free_ipsocket(&new_ipsock) ;
   TRACEEXIT_ERRLOG(err);
   return err ;
}

int initaccept_ipsocket(/*out*/ipsocket_t * ipsock, ipsocket_t * listensock, struct ipaddr_t * remoteaddr/*0 => ignored*/)
{
   int err;
   struct
   sockaddr_storage  saddr;
   socklen_t         len        = sizeof(saddr);
   int               new_socket = -1;
   int               fd         = *listensock;

   if (  remoteaddr
         && version_ipaddr(remoteaddr) != version_ipsocket(listensock)) {
      err = EAFNOSUPPORT;
      goto ONERR;
   }

   new_socket = accept4(fd, (struct sockaddr*) &saddr, &len, SOCK_CLOEXEC);
   if (-1 == new_socket) {
      err = errno;
      if (err == EWOULDBLOCK) err = EAGAIN;
      if (err == EAGAIN) return err;
      TRACESYSCALL_ERRLOG("accept4", err);
      PRINTINT_ERRLOG(fd);
      goto ONERR;
   }

   // set out param
   if (remoteaddr) {
      ipprotocol_e protocol = protocol_ipsocket(listensock) ;
      err = setaddr_ipaddr( remoteaddr, protocol, (uint16_t)len, (struct sockaddr*) &saddr ) ;
      if (err) goto ONERR;
   }

   *ipsock = new_socket;

   return 0;
ONERR:
   free_ipsocket(&new_socket);
   TRACEEXIT_ERRLOG(err);
   return err ;
}

// group: async-support

int waitconnect_ipsocket(const ipsocket_t * ipsock)
{
   int err;

   err = waitconnect(*ipsock);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: async-support

bool isconnected_ipsocket(const ipsocket_t * ipsock)
{
   int err ;
   struct sockaddr_storage saddr ;
   socklen_t               len = sizeof(saddr) ;
   int                     fd  = *ipsock ;

   if (getpeername(fd, (struct sockaddr*) &saddr, &len)) {
      err = errno ;
      if (err != ENOTCONN) {
         TRACESYSCALL_ERRLOG("getpeername", err) ;
         PRINTINT_ERRLOG(fd) ;
         goto ONERR;
      }
      return false ;
   }

   return true ;
ONERR:
   TRACEEXIT_ERRLOG(err);
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
      TRACESYSCALL_ERRLOG("getsockopt(SO_ACCEPTCONN)", err) ;
      PRINTINT_ERRLOG(fd) ;
      goto ONERR;
   }
   assert(len == sizeof(int)) ;

   return (bool) value ;
ONERR:
   TRACEEXIT_ERRLOG(err);
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
      TRACESYSCALL_ERRLOG("getsockopt(SO_PROTOCOL)", err) ;
      PRINTINT_ERRLOG(fd) ;
      goto ONERR;
   }
   assert(len == sizeof(int)) ;

   return (uint16_t) value ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return ipprotocol_ANY ;
}

uint16_t version_ipsocket(const ipsocket_t * ipsock)
{
   int err;
   int value;
   int fd = *ipsock;
   socklen_t len = sizeof(value);

   if (getsockopt(fd, SOL_SOCKET, SO_DOMAIN, &value, &len)) {
      err = errno ;
      TRACESYSCALL_ERRLOG("getsockopt(SO_DOMAIN)", err) ;
      PRINTINT_ERRLOG(fd) ;
      goto ONERR;
   }

   return (uint16_t) value ;
ONERR:
   TRACEEXIT_ERRLOG(err);
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
      goto ONERR;
   }

   if (getsockname(fd, (struct sockaddr*) &saddr, &len)) {
      err = errno ;
      TRACESYSCALL_ERRLOG("getsockname", err) ;
      PRINTINT_ERRLOG(fd) ;
      goto ONERR;
   }

   ipprotocol_e protocol = protocol_ipsocket(ipsock) ;

   err = setaddr_ipaddr( localaddr, protocol, (uint16_t)len, (struct sockaddr*) &saddr ) ;
   if (err) goto ONERR;

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
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
      goto ONERR;
   }

   if (getpeername(fd, (struct sockaddr*) &saddr, &len)) {
      err = errno ;
      TRACESYSCALL_ERRLOG("getpeername", err) ;
      PRINTINT_ERRLOG(fd) ;
      goto ONERR;
   }

   ipprotocol_e protocol = protocol_ipsocket(ipsock) ;

   err = setaddr_ipaddr( remoteaddr, protocol, (uint16_t)len, (struct sockaddr*) &saddr) ;
   if (err) goto ONERR;

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}

int bytestoread_ipsocket(const ipsocket_t * ipsock, /*out*/size_t * unread_bytes)
{
   int err ;
   int bytes ;
   int fd = *ipsock ;

   if (ioctl(fd, FIONREAD, &bytes)) {
      err = errno ;
      TRACESYSCALL_ERRLOG("ioctl(FIONREAD)", err) ;
      PRINTINT_ERRLOG(fd) ;
      goto ONERR;
   }

   *unread_bytes = (unsigned)bytes ;
   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}

int bytestowrite_ipsocket(const ipsocket_t * ipsock, /*out*/size_t * unsend_bytes)
{
   int err;
   int bytes;
   int fd = *ipsock;

   if (ioctl(fd, TIOCOUTQ, &bytes)) {
      err = errno;
      TRACESYSCALL_ERRLOG("ioctl(TIOCOUTQ)", err);
      PRINTINT_ERRLOG(fd);
      goto ONERR;
   }

   *unsend_bytes = (unsigned)bytes;
   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int queuesize_ipsocket(const ipsocket_t * ipsock, /*out*/uint32_t * readsize, /*out*/uint32_t * writesize)
{
   int err;
   int value;
   int fd = *ipsock;
   socklen_t len = sizeof(value);

   if (readsize) {
      if (getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &value, &len)) {
         err = errno;
         goto ONERR;
      }
      *readsize = (unsigned)value;
   }

   if (writesize) {
      if (getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &value, &len)) {
         err = errno;
         goto ONERR;
      }
      *writesize = (unsigned)value;
   }

   return 0;
ONERR:
   TRACESYSCALL_ERRLOG("getsockopt", err);
   PRINTINT_ERRLOG(fd);
   TRACEEXIT_ERRLOG(err);
   return err;
}

int setqueuesize_ipsocket(ipsocket_t * ipsock, uint32_t queuesize_read, uint32_t queuesize_write)
{
   int err;
   int        value;
   socklen_t  len = sizeof(int);
   int        fd  = *ipsock;

   VALIDATE_INPARAM_TEST(  queuesize_read  <= INT_MAX
                           && queuesize_write <= INT_MAX, ONERR, PRINTSIZE_ERRLOG(queuesize_read);
                           PRINTSIZE_ERRLOG(queuesize_write) );

   if (queuesize_read)  {
      value = (int) (queuesize_read/2);
      if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &value, len)) {
         err = errno;
         TRACESYSCALL_ERRLOG("setsockopt(SO_RCVBUF)", err);
         PRINTINT_ERRLOG(fd);
         PRINTINT_ERRLOG(queuesize_read);
         goto ONERR;
      }
   }

   if (queuesize_write)  {
      value = (int) (queuesize_write/2);
      if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &value, len)) {
         err = errno;
         TRACESYSCALL_ERRLOG("setsockopt(SO_SNDBUF)", err);
         PRINTINT_ERRLOG(fd);
         PRINTINT_ERRLOG(queuesize_write);
         goto ONERR;
      }
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

// === connected I/O ===

int read_ipsocket(ipsocket_t * ipsock, size_t maxdata_len, /*out*/uint8_t data[maxdata_len], /*out*/size_t * bytes_read)
{
   int     err;
   int     fd    = *ipsock;
   ssize_t bytes = recv(fd, data, maxdata_len, MSG_DONTWAIT);

   if (-1 == bytes) {
      err = errno;
      if (EWOULDBLOCK == err) err = EAGAIN;
      if (err == EAGAIN) return err;
      TRACESYSCALL_ERRLOG("recv", err);
      PRINTINT_ERRLOG(fd);
      PRINTSIZE_ERRLOG(maxdata_len);
      goto ONERR;
   }

   if (bytes_read) {
      *bytes_read = (size_t) bytes;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int write_ipsocket(ipsocket_t * ipsock, size_t maxdata_len, const uint8_t data[maxdata_len], /*out*/size_t * bytes_written )
{
   int     err ;
   int     fd    = *ipsock ;
   ssize_t bytes = send(fd, data, maxdata_len, MSG_NOSIGNAL|MSG_DONTWAIT) ;

   if (-1 == bytes) {
      err = errno ;
      if (EWOULDBLOCK == err) err = EAGAIN;
      if (err == EAGAIN) return err;
      TRACESYSCALL_ERRLOG("send", err) ;
      PRINTINT_ERRLOG(fd) ;
      PRINTSIZE_ERRLOG(maxdata_len) ;
      goto ONERR;
   }

   if (bytes_written) {
      *bytes_written = (size_t) bytes ;
   }

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
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
      if (err == ENOTTY) err = EOPNOTSUPP ;
      TRACESYSCALL_ERRLOG("ioctl(SIOCATMARK)", err) ;
      PRINTINT_ERRLOG(fd) ;
      goto ONERR;
   }

   bytes = recv(fd, data, maxdata_len, MSG_DONTWAIT) ;
   if (-1 == bytes) {
      err = errno ;
      if (EWOULDBLOCK == err) err = EAGAIN;
      if (err == EAGAIN) return err;
      TRACESYSCALL_ERRLOG("recv", err);
      PRINTINT_ERRLOG(fd);
      PRINTSIZE_ERRLOG(maxdata_len);
      goto ONERR;
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
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}

int writeoob_ipsocket(ipsocket_t * ipsock, uint8_t data)
{
   int     err;
   int     fd    = *ipsock;
   ssize_t bytes = send(fd, &data, 1, MSG_OOB|MSG_NOSIGNAL|MSG_DONTWAIT);

   if (1 != bytes) {
      if (-1 == bytes) {
         err = errno;
         if (EWOULDBLOCK == err) err = EAGAIN;
         if (err == EAGAIN) return err;
         TRACESYSCALL_ERRLOG("send", err);
         PRINTINT_ERRLOG(fd);
         goto ONERR;
      }
      return EAGAIN;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}


// === unconnected I/O ===

int readPaddr_ipsocket(ipsocket_t * ipsock, ipaddr_t * remoteaddr, size_t maxdata_len, /*out*/uint8_t data[maxdata_len], /*out*/size_t * bytes_read)
{
   int err;
   struct sockaddr_storage saddr;
   socklen_t               slen  = sizeof(saddr);
   int                     fd    = *ipsock;
   ssize_t                 bytes;

   if (remoteaddr) {
      VALIDATE_INPARAM_TEST(isvalid_ipaddr(remoteaddr), ONERR, );

      if (version_ipaddr(remoteaddr) != version_ipsocket(ipsock)) {
         err = EAFNOSUPPORT;
         goto ONERR;
      }
   }

   bytes = recvfrom(fd, data, maxdata_len, MSG_DONTWAIT, (struct sockaddr*)&saddr, &slen);
   if (-1 == bytes) {
      err = errno;
      if (EWOULDBLOCK == err) err = EAGAIN;
      if (err == EAGAIN) return err;
      TRACESYSCALL_ERRLOG("recv", err);
      PRINTINT_ERRLOG(fd);
      PRINTSIZE_ERRLOG(maxdata_len);
      goto ONERR;
   }

   // set out param
   if (remoteaddr) {
      ipprotocol_e protocol = protocol_ipsocket( ipsock );
      err = setaddr_ipaddr(remoteaddr, protocol, (uint16_t)slen, (struct sockaddr*)&saddr);
      if (err) goto ONERR;
   }

   if (bytes_read) {
      *bytes_read = (size_t) bytes;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int writePaddr_ipsocket(ipsocket_t * ipsock, const ipaddr_t * remoteaddr, size_t maxdata_len, const uint8_t data[maxdata_len], /*out*/size_t * bytes_written )
{
   int     err;
   int     fd = *ipsock;
   ssize_t bytes;

   VALIDATE_INPARAM_TEST(isvalid_ipaddr(remoteaddr), ONERR, );

   if (ipprotocol_UDP != protocol_ipaddr(remoteaddr)) {
      err = EPROTONOSUPPORT;
      goto ONERR;
   }

   if (version_ipaddr(remoteaddr) != version_ipsocket(ipsock)) {
      err = EAFNOSUPPORT;
      goto ONERR;
   }

   bytes = sendto(fd, data, maxdata_len, MSG_NOSIGNAL|MSG_DONTWAIT, remoteaddr->addr, remoteaddr->addrlen);
   if (-1 == bytes) {
      err = errno;
      if (EWOULDBLOCK == err) err = EAGAIN;
      if (err == EAGAIN) return err;
      TRACESYSCALL_ERRLOG("sendto", err);
      PRINTINT_ERRLOG(fd);
      PRINTSIZE_ERRLOG(maxdata_len);
      goto ONERR;
   }

   if (bytes_written) {
      *bytes_written = (size_t) bytes;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}


// section: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   ipaddr_t   * ipaddr  = 0;
   ipaddr_t   * ipaddr2 = 0;
   ipsocket_t   ipsock  = ipsocket_FREE;
   ipsocket_t   ipsock2 = ipsocket_FREE;

   // TEST static init
   TEST(-1 == ipsock);

   // TEST init, double free
   TEST(0 == newloopback_ipaddr(&ipaddr, ipprotocol_UDP, 0, ipversion_4));
   TEST(0 == init_ipsocket(&ipsock, ipaddr));
   TEST(0 < ipsock);
   TEST(0 == free_ipsocket(&ipsock));
   TEST(-1 == ipsock);
   TEST(0 == free_ipsocket(&ipsock));
   TEST(-1 == ipsock);
   TEST(0 == setprotocol_ipaddr(ipaddr, ipprotocol_TCP));
   TEST(0 == initlisten_ipsocket(&ipsock, ipaddr, 1));
   TEST(0 < ipsock);
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
   for (int i = 0; i < 8; ++i) {
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
      TEST(ipsocket_FREE == ipsock) ;
   }

   // TEST init_ipsocket: EINVAL (ipaddr_t != 0)
   TEST(EINVAL == init_ipsocket(&ipsock, 0)) ;

   // TEST init_ipsocket: EINVAL (ipaddr_t->addr != 0 && ipaddr_t->addrlen != 0)
   TEST(EINVAL == init_ipsocket(&ipsock, &(ipaddr_t){ .protocol = ipprotocol_UDP, .addrlen = 0 })) ;

   // TEST init_ipsocket: EPROTONOSUPPORT (ipprotocol_TCP not supported)
   TEST(0 == newloopback_ipaddr(&ipaddr, ipprotocol_TCP, 20009, ipversion_4)) ;
   TEST(EPROTONOSUPPORT == init_ipsocket(&ipsock, ipaddr)) ;
   TEST(0 == delete_ipaddr(&ipaddr)) ;

   static_assert( ipversion_4 < ipversion_6, "" ) ;
   for (ipversion_e version = ipversion_4; version <= ipversion_6; version = (ipversion_e) (version + (ipversion_6-ipversion_4)) ) {

      // TEST init_ipsocket: ipprotocol_UDP / EADDRINUSE (two sockets bind to same port)
      TEST(0 == newloopback_ipaddr(&ipaddr, ipprotocol_UDP, 20009, version)) ;
      TEST(0 == init_ipsocket(&ipsock, ipaddr)) ;
      TEST(EADDRINUSE == init_ipsocket(&ipsock2, ipaddr)) ;
      TEST(0 == free_ipsocket(&ipsock)) ;
      TEST(0 == delete_ipaddr(&ipaddr)) ;

      // TEST initconnect_ipsocket: ipprotocol_TCP / EADDRINUSE (two sockets bind to same port)
      TEST(0 == newloopback_ipaddr(&ipaddr, ipprotocol_TCP, 20009, version)) ;
      TEST(0 == newloopback_ipaddr(&ipaddr2, ipprotocol_TCP, 20019, version)) ;
      TEST(0 == initlisten_ipsocket(&ipsock, ipaddr, 1)) ;
      TEST(0 == initconnect_ipsocket(&ipsock2, ipaddr, ipaddr2)) ;
      TEST(0 == free_ipsocket(&ipsock)) ;
      TEST(EADDRINUSE == initconnect_ipsocket(&ipsock, ipaddr, ipaddr2)) ;
      TEST(0 == free_ipsocket(&ipsock2)) ;
      TEST(0 == delete_ipaddr(&ipaddr)) ;
      TEST(0 == delete_ipaddr(&ipaddr2)) ;

      // TEST initlisten_ipsocket: ipprotocol_TCP / EADDRINUSE (two sockets bind to same port)
      TEST(0 == newloopback_ipaddr(&ipaddr, ipprotocol_TCP, 20009, version)) ;
      TEST(0 == initlisten_ipsocket(&ipsock, ipaddr, 1)) ;
      TEST(EADDRINUSE == initlisten_ipsocket(&ipsock2, ipaddr, 1)) ;
      TEST(0 == free_ipsocket(&ipsock)) ;
      TEST(0 == delete_ipaddr(&ipaddr)) ;
   }

   return 0 ;
ONERR:
   (void) delete_ipaddr(&ipaddr) ;
   (void) delete_ipaddr(&ipaddr2) ;
   (void) free_ipsocket(&ipsock) ;
   (void) free_ipsocket(&ipsock2) ;
   return EINVAL ;
}

static int test_connect(void)
{
   ipaddr_t  * ipaddr   = 0 ;
   ipaddr_t  * ipaddr2  = 0 ;
   cstring_t   name     = cstring_INIT ;
   ipsocket_t  ipsockCL = ipsocket_FREE ;
   ipsocket_t  ipsockLT = ipsocket_FREE ;
   ipsocket_t  ipsockSV = ipsocket_FREE ;
   uint8_t   * logbuffer;
   size_t      logsize;
   size_t      logsize2;

   // TEST connect TCP
   for (unsigned islocal = 0; islocal < 2; ++islocal) {
      TEST(0 == newloopback_ipaddr(&ipaddr, ipprotocol_TCP, 0, ipversion_4 )) ;
      TEST(0 == initlisten_ipsocket(&ipsockLT, ipaddr, 1)) ;
      TEST(0 == isconnected_ipsocket(&ipsockLT)) ;
      TEST(0 == newcopy_ipaddr(&ipaddr2, ipaddr)) ;
      TEST(0 == localaddr_ipsocket(&ipsockLT, ipaddr2)) ;
      TEST(0 == initconnect_ipsocket(&ipsockCL, ipaddr2, islocal ? ipaddr : 0)) ;
      TEST(1 == isconnected_ipsocket(&ipsockCL)) ;
      TEST(0 == initaccept_ipsocket(&ipsockSV, &ipsockLT, ipaddr)) ;
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
   for (unsigned islocal = 0; islocal < 2; ++islocal) {
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

   // TEST initaccept_ipsocket: EAGAIN
   GETBUFFER_ERRLOG(&logbuffer, &logsize);
   TEST(0 == newloopback_ipaddr(&ipaddr, ipprotocol_TCP, 0, ipversion_4));
   TEST(0 == initlisten_ipsocket(&ipsockLT, ipaddr, 1));
   TEST(EAGAIN == initaccept_ipsocket(&ipsockSV, &ipsockLT, ipaddr));
   TEST(0 == delete_ipaddr(&ipaddr));
   TEST(0 == free_ipsocket(&ipsockLT));
   GETBUFFER_ERRLOG(&logbuffer, &logsize2);
   TEST(logsize == logsize2); // not logged

   // TEST EINVAL (accept on TCP socket but not listener)
   TEST(0 == newloopback_ipaddr(&ipaddr, ipprotocol_TCP, 0, ipversion_4 )) ;
   TEST(0 == newcopy_ipaddr(&ipaddr2, ipaddr)) ;
   TEST(0 == initlisten_ipsocket(&ipsockLT, ipaddr, 1)) ;
   TEST(0 == localaddr_ipsocket(&ipsockLT, ipaddr2)) ;
   TEST(0 == setprotocol_ipaddr(ipaddr2, ipprotocol_UDP)) ;
   TEST(EINVAL == initconnect_ipsocket(&ipsockCL, ipaddr2, ipaddr)) ;
   TEST(0 == localaddr_ipsocket(&ipsockLT, ipaddr2)) ;
   TEST(0 == initconnect_ipsocket(&ipsockCL, ipaddr2, ipaddr)) ;
   GETBUFFER_ERRLOG(&logbuffer, &logsize);
   TEST(EINVAL == initaccept_ipsocket(&ipsockSV, &ipsockCL, ipaddr)) ;
   GETBUFFER_ERRLOG(&logbuffer, &logsize2);
   TEST(logsize < logsize2); // logged
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
   TEST(EOPNOTSUPP == initaccept_ipsocket(&ipsockSV, &ipsockLT, ipaddr)) ;
   TEST(0 == delete_ipaddr(&ipaddr)) ;
   TEST(0 == free_ipsocket(&ipsockLT)) ;


   TEST(0 == free_cstring(&name)) ;

   return 0 ;
ONERR:
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
   ipaddr_t  * ipaddr2  = 0;
   ipsocket_t  ipsockCL = ipsocket_FREE;
   ipsocket_t  ipsockLT = ipsocket_FREE;
   ipsocket_t  ipsockSV = ipsocket_FREE;
   memblock_t  buffer   = memblock_FREE;
   size_t      unsend_bytes;
   size_t      unread_bytes;
   uint8_t   * logbuffer;
   size_t      logsize;
   size_t      logsize2;

   // prepare
   TEST(0 == ALLOC_MM(3 * 65536u, &buffer));

   for (unsigned i = 0; i < 3; ++i) {
      const unsigned  buffer_size  = 3 * 65536u/4 * (i+1);
      const unsigned  sockbuf_size = 65536u/2 * (i+1) ;
      TEST(buffer.size >= buffer_size);

         // connect TCP
      TEST(0 == newloopback_ipaddr(&ipaddr, ipprotocol_TCP, 0, ipversion_4)) ;
      TEST(0 == newcopy_ipaddr(&ipaddr2, ipaddr)) ;
      TEST(0 == initlisten_ipsocket(&ipsockLT, ipaddr, 1)) ;
      TEST(0 == localaddr_ipsocket(&ipsockLT, ipaddr2)) ;
      TEST(0 == initconnect_ipsocket(&ipsockCL, ipaddr2, ipaddr)) ;
      TEST(0 == delete_ipaddr(&ipaddr)) ;
      TEST(0 == delete_ipaddr(&ipaddr2)) ;
      TEST(0 == initaccept_ipsocket(&ipsockSV, &ipsockLT, 0)) ;

      // TEST setqueuesize_ipsocket: 0 does not change values
      uint32_t rwsize[4] = { 0 };
      TEST(0 == queuesize_ipsocket( &ipsockCL, &rwsize[0], &rwsize[1]));
      TEST(0 == setqueuesize_ipsocket( &ipsockCL, 0, 0));
      TEST(0 == queuesize_ipsocket( &ipsockCL, &rwsize[2], &rwsize[3]));
      TEST(rwsize[0] == rwsize[2]);
      TEST(rwsize[1] == rwsize[3]);

      // TEST setqueuesize_ipsocket: sockbuf_size
      TEST(0 == setqueuesize_ipsocket( &ipsockCL, sockbuf_size*2, sockbuf_size));
      TEST(0 == queuesize_ipsocket( &ipsockCL, &rwsize[0], &rwsize[1]));
      TEST(rwsize[0] == sockbuf_size*2) ;
      TEST(rwsize[1] == sockbuf_size) ;
      TEST(0 == setqueuesize_ipsocket( &ipsockCL, sockbuf_size, sockbuf_size));
      TEST(0 == queuesize_ipsocket( &ipsockCL, &rwsize[0], &rwsize[1]));
      TEST(rwsize[0] == sockbuf_size);
      TEST(rwsize[1] == sockbuf_size);
      TEST(0 == setqueuesize_ipsocket( &ipsockSV, sockbuf_size, sockbuf_size));
      TEST(0 == queuesize_ipsocket( &ipsockSV, &rwsize[2], &rwsize[3]));
      TEST(rwsize[2] == sockbuf_size);
      TEST(rwsize[3] == sockbuf_size);

      // TEST bytestoread_ipsocket, bytestowrite_ipsocket: no data read/written
      size_t size = 0 ;
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
         TEST(0 == write_ipsocket(&ipsockSV, buffer_size, buffer.addr, &size)) ;
         TEST(0 < size && size < buffer_size);
         size_t writecount = size ;
         for (int si = 0; si < 100; ++si) {
            TEST(0 == bytestowrite_ipsocket(&ipsockSV, &unsend_bytes));
            if (! unsend_bytes) break ;
            sleepms_thread(1) ;
         }
         // second write transfers all (cause send queue is empty)
         TEST(0 == write_ipsocket(&ipsockSV, buffer_size-writecount, writecount + buffer.addr, &size)) ;
         TEST(size == buffer_size - writecount) ;
      }

      // TEST after 2nd/3rd read write queue is empty
      {
         TEST(0 == bytestoread_ipsocket(&ipsockCL, &unread_bytes)) ;
         TEST(0 <  unread_bytes) ;
         TEST(0 == bytestowrite_ipsocket(&ipsockSV, &unsend_bytes));
         TEST(0 <  unsend_bytes);
         TEST(0 == read_ipsocket(&ipsockCL, unread_bytes, buffer.addr, &size));
         TEST(unread_bytes == size);
         size_t readcount = size;
         for (int si = 0; si < 1000; ++si) {
            TEST(0 == bytestoread_ipsocket(&ipsockCL, &unread_bytes));
            if (0 < unread_bytes) break;
            sleepms_thread(1);
         }
         TEST(0 <  unread_bytes);
         TEST(0 == read_ipsocket(&ipsockCL, unread_bytes, buffer.addr, &size));
         TEST(unread_bytes == size) ;
         readcount += size ;  // 2nd
         for (int si = 0; si < 1000; ++si) {
            TEST(0 == bytestowrite_ipsocket(&ipsockSV, &unsend_bytes)) ;
            if (! unsend_bytes) break ;
            sleepms_thread(1) ;
         }
         // check write queue is empty
         TEST(0 == unsend_bytes);
         // check 3rd read transfers all data
         TEST(0 == bytestoread_ipsocket( &ipsockCL, &unread_bytes));
         if (unread_bytes) {
            TEST(0 == read_ipsocket(&ipsockCL, unread_bytes, buffer.addr, &size));
            TEST(unread_bytes == size);
            readcount += size;
         }
         TEST(buffer_size == readcount);
         // check read queue empty
         TEST(0 == bytestoread_ipsocket(&ipsockCL, &unread_bytes));
         TEST(0 == unread_bytes) ;
      }

      // TEST read on empty queue does not block
      if (0 == i) {
         GETBUFFER_ERRLOG(&logbuffer, &logsize);
         TEST(EAGAIN == read_ipsocket(&ipsockCL, 1, buffer.addr, &size)) ;
         TEST(EAGAIN == read_ipsocket(&ipsockSV, 1, buffer.addr, &size)) ;
         GETBUFFER_ERRLOG(&logbuffer, &logsize2);
         TEST(logsize == logsize2); // EAGAIN is not logged
      }

      TEST(0 == free_ipsocket(&ipsockCL)) ;
      TEST(0 == free_ipsocket(&ipsockLT)) ;
      TEST(0 == free_ipsocket(&ipsockSV)) ;
   }

   for (unsigned i = 0; i < 3; ++i) {
      const unsigned buffer_size = (i+1) * 16384u;
      TEST(buffer.size >= buffer_size);
      memset(buffer.addr, 0, buffer_size);

      // prepare - connect UDP
      TEST(0 == newloopback_ipaddr(&ipaddr, ipprotocol_UDP, 0, ipversion_4)) ;
      TEST(0 == newloopback_ipaddr(&ipaddr2, ipprotocol_UDP, (uint16_t)(10000+i), ipversion_4)) ;
      TEST(0 == initconnect_ipsocket(&ipsockCL, ipaddr2, ipaddr)) ;
      TEST(0 == localaddr_ipsocket(&ipsockCL, ipaddr)) ;
      TEST(0 == initconnect_ipsocket(&ipsockSV, ipaddr, ipaddr2)) ;
      TEST(0 == delete_ipaddr(&ipaddr)) ;
      TEST(0 == delete_ipaddr(&ipaddr2)) ;

      // TEST setqueuesize_ipsocket
      for (size_t bs = buffer_size/2; bs <= buffer_size; bs += buffer_size/2) {
         uint32_t rsize, wsize;
         TEST(0 == setqueuesize_ipsocket(&ipsockCL, buffer_size, bs));
         TEST(0 == queuesize_ipsocket(&ipsockCL, &rsize, &wsize));
         TEST(rsize == buffer_size);
         TEST(wsize == bs);
         TEST(0 == setqueuesize_ipsocket(&ipsockSV, buffer_size, bs));
         TEST(0 == queuesize_ipsocket(&ipsockSV, &rsize, &wsize));
         TEST(rsize == buffer_size);
         TEST(wsize == bs);
      }

      // TEST bytestoread_ipsocket: no data transfered
      size_t size;
      TEST(0 == bytestoread_ipsocket(&ipsockCL, &size));
      TEST(0 == size);
      TEST(0 == bytestoread_ipsocket(&ipsockSV, &size));
      TEST(0 == size);

      // TEST bytestowrite_ipsocket: no data transfered
      TEST(0 == bytestowrite_ipsocket(&ipsockCL, &size));
      TEST(0 == size);
      TEST(0 == bytestowrite_ipsocket(&ipsockSV, &size));
      TEST(0 == size);

      // TEST datagram equal to buffer_size will be discarded on receiver side (buffer stores also control info)
      TEST(0 == write_ipsocket(&ipsockSV, buffer_size/4, buffer.addr, &size));
      TEST(buffer_size/4 == size);
      TEST(0 == write_ipsocket(&ipsockSV, buffer_size/3, buffer.addr, &size));
      TEST(buffer_size/3 == size);
         // third datagram will be discarded on receiver side
      TEST(0 == write_ipsocket(&ipsockSV, buffer_size/2, buffer.addr, &size));
      TEST(buffer_size/2 == size);

      // TEST bytestoread_ipsocket: returns size of datagram
      for (int dg = 0; dg <= 1; ++dg) {
         size_t dgsize = dg ? buffer_size/3 : buffer_size/4;
         for (int r = 0; r < 1000; ++r) {
            TEST(0 == bytestoread_ipsocket(&ipsockCL, &size));
            if (0 != size) break;
            sleepms_thread(1);
         }
         TEST(dgsize == size);

         // TEST read_ipsocket: only dgsize bytes returned
         TEST(0 == read_ipsocket(&ipsockCL, 2*dgsize, buffer.addr, &size));
         TEST(dgsize == size);
      }
      for (int r = 0; r < 1000; ++r) {
         TEST(0 == bytestowrite_ipsocket(&ipsockSV, &size));
         if (0 == size) break;
         sleepms_thread(1);
      }
      TEST(0 == size);
         // third was -- silently -- discarded
      TEST(0 == bytestoread_ipsocket( &ipsockCL, &size));
      TEST(0 == size);
      TEST(0 == bytestowrite_ipsocket( &ipsockSV, &size));
      TEST(0 == size);

      // TEST read on empty queue does not block
      if (0 == i)  {
         GETBUFFER_ERRLOG(&logbuffer, &logsize);
         TEST(EAGAIN == read_ipsocket(&ipsockCL, 1, buffer.addr, &size));
         TEST(EAGAIN == read_ipsocket(&ipsockSV, 1, buffer.addr, &size));
         GETBUFFER_ERRLOG(&logbuffer, &logsize2);
         TEST(logsize == logsize2); // EAGAIN is not logged
      }

      TEST(0 == free_ipsocket(&ipsockCL)) ;
      TEST(0 == free_ipsocket(&ipsockSV)) ;
   }

   // unprepare
   TEST(0 == FREE_MM(&buffer));

   return 0;
ONERR:
   (void) FREE_MM(&buffer);
   (void) delete_ipaddr(&ipaddr);
   (void) free_ipsocket(&ipsockCL);
   (void) free_ipsocket(&ipsockLT);
   (void) free_ipsocket(&ipsockSV);
   return EINVAL;
}

static int test_helper_oob(ipsocket_t * ipsockSV, ipsocket_t * ipsockCL, const size_t buffer_size, uint8_t * buffer)
{
   size_t size;
   size_t oob_offset;
   size_t unsend_bytes;
   size_t unread_bytes;
   uint8_t * logbuffer;
   size_t    logsize;
   size_t    logsize2;

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
   GETBUFFER_ERRLOG(&logbuffer, &logsize);
   TEST(EAGAIN == readoob_ipsocket(ipsockCL, 1, buffer, &size, &oob_offset));
   GETBUFFER_ERRLOG(&logbuffer, &logsize2);
   TEST(logsize2 == logsize); // EAGAIN not logged
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
ONERR:
   return EINVAL ;
}

static int test_outofbandData(void)
{
   ipaddr_t     * ipaddr      = 0;
   ipaddr_t     * ipaddr2     = 0;
   ipsocket_t     ipsockCL    = ipsocket_FREE;
   ipsocket_t     ipsockLT    = ipsocket_FREE;
   ipsocket_t     ipsockSV    = ipsocket_FREE;
   size_t         oob_offset  = 0;
   const size_t   buffer_size = 512;
   uint8_t        buffer[512] = { 0 };
   size_t         size;
   size_t         unsend_bytes;
   size_t         unread_bytes;

   // TEST TCP oob
   TEST(0 == newloopback_ipaddr(&ipaddr, ipprotocol_TCP, 0, ipversion_4 )) ;
   TEST(0 == newcopy_ipaddr(&ipaddr2, ipaddr)) ;
   TEST(0 == initlisten_ipsocket(&ipsockLT, ipaddr, 1)) ;
   TEST(0 == localaddr_ipsocket(&ipsockLT, ipaddr2)) ;
   TEST(0 == initconnect_ipsocket(&ipsockCL, ipaddr2, ipaddr)) ;
   TEST(0 == delete_ipaddr(&ipaddr)) ;
   TEST(0 == delete_ipaddr(&ipaddr2)) ;
   TEST(0 == initaccept_ipsocket(&ipsockSV, &ipsockLT, 0)) ;

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
   TEST(0 == strncmp( (char*)buffer, "abc", 3));
   TEST(EAGAIN == read_ipsocket(&ipsockCL, 1, buffer, &size));  // test buffer is empty

   TEST(0 == free_ipsocket(&ipsockCL));
   TEST(0 == free_ipsocket(&ipsockSV));

   return 0;
ONERR:
   (void) delete_ipaddr(&ipaddr);
   (void) free_ipsocket(&ipsockCL);
   (void) free_ipsocket(&ipsockLT);
   (void) free_ipsocket(&ipsockSV);
   return EINVAL;
}

int test_udpIO(void)
{
   ipaddr_t     * ipaddr       = 0 ;
   ipaddr_t     * ipaddr2      = 0 ;
   ipsocket_t     ipsockCL[2]  = { ipsocket_FREE } ;
   ipsocket_t     ipsockSV[10] = { ipsocket_FREE } ;
   cstring_t      name         = cstring_INIT ;
   uint8_t        buffer[512];
   const size_t   buffer_size  = sizeof(buffer);
   uint16_t       portCL[lengthof(ipsockCL)] ;
   uint16_t       portSV[lengthof(ipsockSV)] ;
   size_t         size ;

   // TEST ipversion_4, ipversion_6
   static_assert( ipversion_4 < ipversion_6, "" ) ;
   for (ipversion_e version = ipversion_4; version <= ipversion_6; version = (ipversion_e) (version + (ipversion_6-ipversion_4)) ) {

      // TEST connected send&receive (messages from other client discarded)
      TEST(0 == newloopback_ipaddr(&ipaddr, ipprotocol_UDP, 0, version)) ;
      TEST(0 == newcopy_ipaddr(&ipaddr2, ipaddr)) ;
      for (uint16_t i = 0; i < lengthof(ipsockCL); ++i) {
         TEST(0 == init_ipsocket(&ipsockCL[i], ipaddr)) ;
         TEST(0 == localaddr_ipsocket(&ipsockCL[i], ipaddr2)) ;
         portCL[i] = port_ipaddr(ipaddr2) ;
      }
      for (uint16_t i = 0; i < lengthof(ipsockSV); ++i) {
         unsigned ci = (i % lengthof(ipsockCL)) ;
         TEST(0 == setport_ipaddr(ipaddr2, portCL[ci])) ;
         TEST(0 == initconnect_ipsocket(&ipsockSV[i], ipaddr2, ipaddr)) ;
         TEST(0 == localaddr_ipsocket(&ipsockSV[i], ipaddr2)) ;
         portSV[i] = port_ipaddr(ipaddr2) ;
      }

      for (uint16_t i = 0; i < lengthof(ipsockSV); ++i) {
         memset( buffer, (int)i, buffer_size) ;
         for (uint16_t ci = 0; ci < lengthof(ipsockCL); ++ci) {
            TEST(0 == setport_ipaddr(ipaddr2, portSV[i])) ;
            TEST(0 == writePaddr_ipsocket(&ipsockCL[ci], ipaddr2, buffer_size, buffer, &size)) ;
            TEST(buffer_size == size) ;
         }
      }

      for (uint16_t i = 0; i < lengthof(ipsockSV); ++i) {
         unsigned ci = (i % lengthof(ipsockCL)) ;
         TEST(0 == bytestoread_ipsocket(&ipsockSV[i], &size)) ;
         TEST(buffer_size == size) ;
         TEST(0 == readPaddr_ipsocket(&ipsockSV[i], ipaddr, buffer_size, buffer, &size)) ;
         TEST(buffer_size == size) ;
         for (unsigned b = 0; b < buffer_size; ++b) {
            TEST(buffer[b] == i) ;
         }
         TEST(0 == setport_ipaddr(ipaddr2, portCL[ci])) ;
         TEST(0 == compare_ipaddr(ipaddr, ipaddr2)) ;
      }
      for (uint16_t i = 0; i < lengthof(ipsockSV); ++i) {
         TEST(0 == bytestoread_ipsocket(&ipsockSV[i], &size)) ;
         TEST(0 == size) ;
      }

      // TEST unconnected send & receive
      TEST(0 == setport_ipaddr(ipaddr, 0)) ;
      for (uint16_t i = 0; i < lengthof(ipsockCL); ++i) {
         TEST(0 == free_ipsocket(&ipsockCL[i])) ;
         TEST(0 == init_ipsocket(&ipsockCL[i], ipaddr)) ;
         TEST(0 == localaddr_ipsocket(&ipsockCL[i], ipaddr2)) ;
         portCL[i] = port_ipaddr(ipaddr2) ;
      }
      for (uint16_t i = 0; i < lengthof(ipsockSV); ++i) {
         TEST(0 == free_ipsocket(&ipsockSV[i])) ;
         TEST(0 == init_ipsocket(&ipsockSV[i], ipaddr)) ;
         TEST(0 == localaddr_ipsocket(&ipsockSV[i], ipaddr2)) ;
         portSV[i] = port_ipaddr(ipaddr2) ;
      }

      for (uint16_t i = 0; i < lengthof(ipsockSV); ++i) {
         memset( buffer, (int)i, buffer_size) ;
         for (uint16_t ci = 0; ci < lengthof(ipsockCL); ++ci) {
            TEST(0 == setport_ipaddr(ipaddr2, portSV[i])) ;
            TEST(0 == writePaddr_ipsocket(&ipsockCL[ci], ipaddr2, buffer_size, buffer, &size)) ;
            TEST(buffer_size == size) ;
         }
      }

      for (uint16_t i = 0; i < lengthof(ipsockSV); ++i) {
         for (uint16_t ci = 0; ci < lengthof(ipsockCL); ++ci) {
            for (unsigned w = 0; w < 100; ++w) {
               TEST(0 == bytestoread_ipsocket(&ipsockSV[i], &size)) ;
               if (buffer_size == size) break ;
               yield_thread() ;
            }
            TEST(buffer_size == size) ;
            TEST(0 == readPaddr_ipsocket(&ipsockSV[i], ipaddr, buffer_size, buffer, &size)) ;
            TEST(buffer_size == size) ;
            for (unsigned b = 0; b < buffer_size; ++b) {
               TEST(buffer[b] == i) ;
            }
            TEST(0 == setport_ipaddr(ipaddr2, portCL[ci])) ;
            TEST(0 == compare_ipaddr(ipaddr, ipaddr2)) ;
         }
      }
      for (uint16_t i = 0; i < lengthof(ipsockSV); ++i) {
         TEST(0 == bytestoread_ipsocket(&ipsockSV[i], &size)) ;
         TEST(0 == size) ;
      }

      // TEST EAFNOSUPPORT (ipaddr is of wrong version)
      TEST(0 == delete_ipaddr(&ipaddr)) ;
      TEST(0 == newloopback_ipaddr(&ipaddr, ipprotocol_UDP, 0, version == ipversion_4 ? ipversion_6 : ipversion_4 )) ;
      TEST(EAFNOSUPPORT == writePaddr_ipsocket(&ipsockSV[0], ipaddr, buffer_size, buffer, &size)) ;
      TEST(EAFNOSUPPORT == readPaddr_ipsocket(&ipsockSV[0], ipaddr, buffer_size, buffer, &size)) ;

      // TEST EPROTONOSUPPORT (ipaddr contains wrong protocol)
      TEST(0 == delete_ipaddr(&ipaddr)) ;
      TEST(0 == newloopback_ipaddr(&ipaddr, ipprotocol_TCP, 0, version)) ;
      TEST(EPROTONOSUPPORT == writePaddr_ipsocket(&ipsockSV[0], ipaddr, buffer_size, buffer, &size)) ;

      // TEST close
      for (uint16_t i = 0; i < lengthof(ipsockCL); ++i) {
         TEST(0 == free_ipsocket(&ipsockCL[i])) ;
      }
      for (uint16_t i = 0; i < lengthof(ipsockSV); ++i) {
         TEST(0 == free_ipsocket(&ipsockSV[i])) ;
      }

      TEST(0 == delete_ipaddr(&ipaddr)) ;
      TEST(0 == delete_ipaddr(&ipaddr2)) ;
   }

   TEST(0 == free_cstring(&name)) ;
   return 0 ;
ONERR:
   (void) free_cstring(&name) ;
   (void) delete_ipaddr(&ipaddr) ;
   (void) delete_ipaddr(&ipaddr2) ;
   for (uint16_t i = 0; i < lengthof(ipsockCL); ++i) {
      free_ipsocket(&ipsockCL[i]) ;
   }
   for (uint16_t i = 0; i < lengthof(ipsockSV); ++i) {
      free_ipsocket(&ipsockSV[i]) ;
   }
   return EINVAL ;
}

static int test_async(void)
{
   ipsocket_t  iplisten = ipsocket_FREE;
   ipsocket_t  ipsockC  = ipsocket_FREE;
   ipsocket_t  ipsockS  = ipsocket_FREE;
   ipaddr_t  * ipaddr   = 0;
   ipaddr_t  * ipaddr2  = 0;
   ipaddr_t  * ipaddr3  = 0;
   uint8_t     buffer[100];
   size_t      size;

   // prepare
   TEST(0 == newloopback_ipaddr(&ipaddr, ipprotocol_TCP, 0, ipversion_4));
   TEST(0 == initlisten_ipsocket(&iplisten, ipaddr, 1));
   TEST(0 == localaddr_ipsocket(&iplisten, ipaddr));
   TEST(0 == newloopback_ipaddr(&ipaddr2, ipprotocol_TCP, (ipport_t) (port_ipaddr(ipaddr)+1), ipversion_4));
   TEST(0 == newloopback_ipaddr(&ipaddr3, ipprotocol_TCP, (ipport_t) (port_ipaddr(ipaddr)+2), ipversion_4));

   // TEST initconnectasync_ipsocket
   TEST(0 == initconnectasync_ipsocket(&ipsockC, ipaddr2, 0));
   TEST(ipsocket_FREE != ipsockC);

   // TEST free_ipsocket: no error if connect in progress
   TEST(0 == free_ipsocket(&ipsockC));

   // TEST waitconnect_ipsocket: ECONNREFUSED
   TEST(0 == initconnectasync_ipsocket(&ipsockC, ipaddr2, 0));
   TEST(ECONNREFUSED == waitconnect_ipsocket(&ipsockC));
   TEST(0 == free_ipsocket(&ipsockC));

   // TEST initconnectasync_ipsocket, waitconnect_ipsocket: TCP establish connection
   TEST(0 == initconnectasync_ipsocket(&ipsockC, ipaddr, ipaddr2));
   TEST(0 == waitconnect_ipsocket(&ipsockC));
   TEST(0 == initaccept_ipsocket(&ipsockS, &iplisten, ipaddr3));
   TEST(0 == compare_ipaddr(ipaddr2, ipaddr3));
   TEST(0 == write_ipsocket(&ipsockC, sizeof(buffer), buffer, &size)) ;
   TEST(sizeof(buffer) == size) ;
   TEST(0 == read_ipsocket(&ipsockS, sizeof(buffer), buffer, &size)) ;
   TEST(sizeof(buffer) == size) ;

   // TEST waitconnect_ipsocket: already connected socket
   TEST(0 == waitconnect_ipsocket(&ipsockC));
   TEST(0 == waitconnect_ipsocket(&ipsockS));
   TEST(0 == free_ipsocket(&ipsockC));
   TEST(0 == free_ipsocket(&ipsockS));

   // TEST initconnectasync_ipsocket, waitconnect_ipsocket: UDP (completes immediately)
   TEST(0 == localaddr_ipsocket(&iplisten, ipaddr2));
   TEST(0 == localaddr_ipsocket(&iplisten, ipaddr3));
   TEST(0 == setprotocol_ipaddr(ipaddr2, ipprotocol_UDP));
   TEST(0 == setprotocol_ipaddr(ipaddr3, ipprotocol_UDP));
   TEST(0 == setport_ipaddr(ipaddr2, (ipport_t) (port_ipaddr(ipaddr)+1)));
   TEST(0 == setport_ipaddr(ipaddr3, (ipport_t) (port_ipaddr(ipaddr)+2)));
   TEST(0 == initconnectasync_ipsocket(&ipsockC, ipaddr3, ipaddr2));
   TEST(0 == waitconnect_ipsocket(&ipsockC));
   TEST(0 == initconnectasync_ipsocket(&ipsockS, ipaddr2, ipaddr3));
   TEST(0 == waitconnect_ipsocket(&ipsockS));
   TEST(0 == write_ipsocket(&ipsockC, sizeof(buffer), buffer, &size));
   TEST(sizeof(buffer) == size);
   TEST(0 == read_ipsocket(&ipsockS, sizeof(buffer), buffer, &size));
   TEST(sizeof(buffer) == size);
   TEST(0 == free_ipsocket(&ipsockC));
   TEST(0 == free_ipsocket(&ipsockS));

   // TEST initconnectasync_ipsocket: EINVAL (different protocols)
   TEST(0 == localaddr_ipsocket(&iplisten, ipaddr));
   TEST(0 == setprotocol_ipaddr(ipaddr2, ipprotocol_UDP)) ;
   TEST(EINVAL == initconnectasync_ipsocket(&ipsockC, ipaddr, ipaddr2));

   // TEST initconnectasync_ipsocket: EAFNOSUPPORT (different versions)
   TEST(0 == delete_ipaddr(&ipaddr));
   TEST(0 == newloopback_ipaddr(&ipaddr, ipprotocol_UDP, 0, ipversion_6));
   TEST(0 == localaddr_ipsocket(&iplisten, ipaddr2));
   TEST(0 == setprotocol_ipaddr(ipaddr2, ipprotocol_UDP));
   TEST(EAFNOSUPPORT == initconnectasync_ipsocket(&ipsockC, ipaddr, ipaddr2));

   // unprepare
   TEST(0 == free_ipsocket(&iplisten));
   TEST(0 == delete_ipaddr(&ipaddr));
   TEST(0 == delete_ipaddr(&ipaddr2));
   TEST(0 == delete_ipaddr(&ipaddr3));

   return 0;
ONERR:
   free_ipsocket(&iplisten);
   free_ipsocket(&ipsockC);
   free_ipsocket(&ipsockS);
   delete_ipaddr(&ipaddr);
   delete_ipaddr(&ipaddr2);
   delete_ipaddr(&ipaddr3);
   return EINVAL;
}

int unittest_io_ipsocket()
{
   if (test_initfree())       goto ONERR;
   if (test_connect())        goto ONERR;
   if (test_buffersize())     goto ONERR;
   if (test_outofbandData())  goto ONERR;
   if (test_udpIO())          goto ONERR;
   if (test_async())          goto ONERR;

   return 0 ;
ONERR:
   return EINVAL ;
}

#endif
