/* title: IOChannel impl

   Implements <IOChannel>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/io/iochannel.h
    Header file <IOChannel>.

   file: C-kern/platform/Linux/io/iochannel.c
    Implementation file <IOChannel impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/accessmode.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/io/filesystem/directory.h"
#include "C-kern/api/io/filesystem/file.h"
#include "C-kern/api/io/ip/ipaddr.h"
#include "C-kern/api/io/ip/ipsocket.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/pagecache_macros.h"
#include "C-kern/api/memory/wbuffer.h"
#include "C-kern/api/platform/sync/signal.h"
#include "C-kern/api/platform/task/thread.h"
#include "C-kern/api/time/systimer.h"
#include "C-kern/api/time/timevalue.h"
#endif


// section: iochannel_t

// group: lifetime

int initcopy_iochannel(/*out*/iochannel_t* ioc, iochannel_t from_ioc)
{
   int err;
   int fd;

   fd = fcntl(from_ioc, F_DUPFD_CLOEXEC, (long)0);

   if (fd < 0) {
      err = errno;
      TRACESYSCALL_ERRLOG("fcntl", err);
      PRINTINT_ERRLOG(from_ioc);
      goto ONERR;
   }

   *ioc = fd;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int free_iochannel(iochannel_t* ioc)
{
   int err;
   int close_ioc = *ioc;

   if (!isfree_iochannel(close_ioc)) {
      *ioc = iochannel_FREE;

      err = close(close_ioc);
      if (err) {
         err = errno;
         TRACESYSCALL_ERRLOG("close", err);
         PRINTINT_ERRLOG(close_ioc);
         goto ONERR;
      }
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: query

/* function: nropen_iochannel
 * Uses Linux specific "/proc/self/fd" interface. */
int nropen_iochannel(/*out*/size_t * number_open)
{
   int err;
   int  fd       = iochannel_FREE;
   DIR* procself = 0;

   fd = open("/proc/self/fd", O_RDONLY|O_NONBLOCK|O_LARGEFILE|O_DIRECTORY|O_CLOEXEC);
   if (-1 == fd) {
      err = errno;
      TRACESYSCALL_ERRLOG("open(/proc/self/fd)", err);
      goto ONERR;
   }

   procself = fdopendir(fd);
   if (!procself) {
      err = errno;
      TRACESYSCALL_ERRLOG("fdopendir", err);
      goto ONERR;
   }
   fd = iochannel_FREE;

   size_t         open_iocs = (size_t)0;
   struct dirent* name;
   for (;;) {
      ++ open_iocs;
      errno = 0;
      name  = readdir(procself);
      if (!name) {
         if (errno) {
            err = errno;
            goto ONERR;
         }
         break;
      }
   }

   err = closedir(procself);
   procself = 0;
   if (err) {
      err = errno;
      TRACESYSCALL_ERRLOG("closedir", err);
      goto ONERR;
   }

   /* adapt open_iocs for
      1. counts one too high
      2. counts "."
      3. counts ".."
      4. counts fd
   */
   *number_open = open_iocs >= 4 ? open_iocs-4 : 0;

   return 0;
ONERR:
   free_iochannel(&fd);
   if (procself) {
      closedir(procself);
   }
   TRACEEXIT_ERRLOG(err);
   return err;
}

uint8_t accessmode_iochannel(const iochannel_t ioc)
{
   int err;
   int flags;

   flags = fcntl(ioc, F_GETFL);
   if (-1 == flags) {
      err = errno;
      TRACESYSCALL_ERRLOG("fcntl", err);
      PRINTINT_ERRLOG(ioc);
      goto ONERR;
   }

   static_assert((O_RDONLY+1) == accessmode_READ, "simple conversion");
   static_assert((O_WRONLY+1) == accessmode_WRITE, "simple conversion");
   static_assert((O_RDWR+1)   == (accessmode_READ|accessmode_WRITE), "simple conversion");
   static_assert(O_ACCMODE    == (O_RDWR|O_WRONLY|O_RDONLY), "simple conversion");

   return (uint8_t) (1 + (flags & O_ACCMODE));
ONERR:
   TRACEEXIT_ERRLOG(err);
   return accessmode_NONE;
}

int sizeread_iochannel(const iochannel_t ioc, /*out*/size_t * size)
{
   int err;
   int bytes;
   struct stat statbuf;

   err = fstat(ioc, &statbuf);
   if (err) {
      err = errno;
      goto ONERR;
   }

   if (  ! S_ISFIFO(statbuf.st_mode)
         && ! S_ISSOCK(statbuf.st_mode)
         && ! S_ISCHR(statbuf.st_mode) ) {
      off_t stsize = statbuf.st_size;
      if (stsize > SIZE_MAX) {
         stsize = SIZE_MAX;
      }
      *size = (size_t) stsize;

   } else {
      err = ioctl(ioc, FIONREAD, &bytes);

      if (err) {
         err = errno;
         goto ONERR;
      }

      static_assert(sizeof(size_t) >= sizeof(int), "cast possible");
      *size = (size_t) bytes;
   }

   return 0;
ONERR:
   return err;
}

bool isvalid_iochannel(const iochannel_t ioc)
{
   int err;
   err = fcntl(ioc, F_GETFD);
   return (-1 != err);
}

bool isclosedread_iochannel(const iochannel_t ioc)
{
   int err;
   struct pollfd pfd;

   pfd.events = POLLIN;
   pfd.fd     = ioc;

   err = poll(&pfd, 1, 0);
   if (err == 1/*ioc ready for read*/) {

      // needed for sockets
      if (0 != (pfd.revents & POLLIN)) {
         struct stat statbuf;
         err = fstat(ioc, &statbuf);
         if (  err
               || (  ! S_ISFIFO(statbuf.st_mode)
                     && ! S_ISSOCK(statbuf.st_mode)
                     && ! S_ISCHR(statbuf.st_mode)) ) {
            return false;
         }

         int bytes;
         err = ioctl(ioc, FIONREAD, &bytes);
         return (!err && bytes == 0) || (err && 0 != (pfd.revents & POLLHUP));
      }

      return 0 != (pfd.revents & POLLHUP);
   }

   return false;
}

bool isclosedwrite_iochannel(const iochannel_t ioc)
{
   int err;
   struct pollfd pfd;

   pfd.events = POLLOUT;
   pfd.fd     = ioc;

   err = poll(&pfd, 1, 0);
   if (err == 1/*ioc ready for write*/) {
      return (pfd.revents & POLLERR) || (POLLOUT|POLLHUP) == (pfd.revents & (POLLOUT|POLLHUP));
   }

   return false;
}

// group: I/O

int read_iochannel(iochannel_t ioc, size_t size, /*out*/void * buffer/*[size]*/, /*out*/size_t * bytes_read)
{
   int err;
   ssize_t bytes;

   for (;;) {
      bytes = read(ioc, buffer, size);

      if (-1 == bytes) {
         err = errno;
         // non blocking io ?
         if (EAGAIN == err || EWOULDBLOCK == err) return EAGAIN;
         // or interrupted ?
         if (EINTR == err) continue;
         TRACESYSCALL_ERRLOG("read", err);
         PRINTINT_ERRLOG(ioc);
         PRINTSIZE_ERRLOG(size);
         goto ONERR;
      }

      break;
   }

   if (bytes_read) {
      *bytes_read = (size_t) bytes;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int write_iochannel(iochannel_t ioc, size_t size, const void * buffer/*[size]*/, /*out*/size_t * bytes_written)
{
   int err;
   ssize_t bytes;

   VALIDATE_INPARAM_TEST(size <= SSIZE_MAX, ONERR, );

   for (;;) {
      bytes = write(ioc, buffer, size);

      if (-1 == bytes) {
         err = errno;
         // non blocking io ?
         if (EAGAIN == err || EWOULDBLOCK == err) return EAGAIN;
         if (EPIPE == err) return err;
         // or interrupted ?
         if (EINTR == err) continue;
         TRACESYSCALL_ERRLOG("write", err);
         PRINTINT_ERRLOG(ioc);
         PRINTSIZE_ERRLOG(size);
         goto ONERR;
      }

      break;
   }

   if (bytes_written) {
      *bytes_written = (size_t) bytes;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int readall_iochannel(iochannel_t ioc, size_t size, void* buffer/*uint8_t[size]*/, int32_t msec_timeout/*<0: infinite timeout*/)
{
   int err;
   size_t bytes = 0;

   for (;;) {
      int part = read(ioc, (uint8_t*)buffer + bytes, size-bytes);

      if (part > 0) {
         bytes += (unsigned) part;
         if (bytes == size) break/*DONE*/;
         continue;
      }

      if (part < 0) {
         err = errno;
         if (err == EAGAIN && msec_timeout) {
            // wait msec_timeout milliseconds (if (msec_timeout < 0) "inifinite timeout")
            struct pollfd pfd = { .events = POLLIN, .fd = ioc };
            err = poll(&pfd, 1, msec_timeout);
            if (err > 0) continue;
            err = (err < 0) ? errno : ETIME;
         }

      } else { // end-of-input (not logged)
         return EPIPE;
      }

      // error => discard read bytes
      goto ONERR;
   }

/*DONE*/
   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   PRINTINT_ERRLOG(ioc);
   return err;
}

int writeall_iochannel(iochannel_t ioc, size_t size, const void* buffer/*uint8_t[size]*/, int32_t msec_timeout/*<0: infinite timeout*/)
{
   int err;
   size_t bytes = 0;

   for (;;) {
      int part = write(ioc, (const uint8_t*)buffer + bytes, size-bytes);

      if (part >= 0) {
         bytes += (unsigned) part;
         if (bytes == size) break/*DONE*/;
         continue;
      }

      err = errno;
      if (err == EAGAIN && msec_timeout) {
         // wait msec_timeout milliseconds (if (msec_timeout < 0) "inifinite timeout")
         struct pollfd pfd = { .events = POLLOUT, .fd = ioc };
         err = poll(&pfd, 1, msec_timeout);
         if (err > 0) continue;
         err = (err < 0) ? errno : ETIME;
      }

      // error => undo of already written data is not possible
      goto ONERR;
   }

/*DONE*/
   return 0;
ONERR:
   if (err != EPIPE) {
      TRACEEXIT_ERRLOG(err);
      PRINTINT_ERRLOG(ioc);
   }
   return err;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_nropen(void)
{
   size_t  nropen;
   size_t  nropen2;
   int     ioc[128];

   // prepare
   for (unsigned i = 0; i < lengthof(ioc); ++i) {
      ioc[i] = file_FREE;
   }

   // TEST nropen_iochannel: std file descriptors are open
   nropen = 0;
   TEST(0 == nropen_iochannel(&nropen));
   TEST(3 <= nropen);

   // TEST nropen_iochannel: increment
   for (unsigned i = 0; i < lengthof(ioc); ++i) {
      ioc[i] = open("/dev/null", O_RDONLY|O_CLOEXEC);
      TEST(0 < ioc[i]);
      nropen2 = 0;
      TEST(0 == nropen_iochannel(&nropen2));
      ++ nropen;
      TEST(nropen == nropen2);
   }

   // TEST nropen_iochannel: decrement
   for (unsigned i = 0; i < lengthof(ioc); ++i) {
      TEST(0 == free_iochannel(&ioc[i]));
      nropen2 = 0;
      TEST(0 == nropen_iochannel(&nropen2));
      -- nropen;
      TEST(nropen == nropen2);
   }

   return 0;
ONERR:
   for (unsigned i = 0; i < lengthof(ioc); ++i) {
      free_iochannel(&ioc[i]);
   }
   return EINVAL;
}

static int test_initfree(void)
{
   const int   N         = 4;  // next free file descriptor number
   iochannel_t ioc       = iochannel_FREE;
   iochannel_t pipeioc[] = { iochannel_FREE, iochannel_FREE };

   // TEST iochannel_FREE
   TEST(-1 == sys_iochannel_FREE);
   TEST(-1 == ioc);

   // TEST initcopy_iochannel
   static_assert(0 == iochannel_STDIN, "used in tests");
   TEST(0 == initcopy_iochannel(&ioc, iochannel_STDIN));
   TEST(N == ioc);

   // TEST free_iochannel
   TEST(0 == free_iochannel(&ioc));
   TEST(-1 == ioc);
   TEST(0 == free_iochannel(&ioc));
   TEST(-1 == ioc);

   // TEST initcopy_iochannel: underlying data stream keeps open if another iochannel refs it
   TEST(0 == pipe2(pipeioc, O_CLOEXEC));
   for (unsigned i = 0; i < 2; ++i) {
      uint8_t buffer[1];
      TEST(0 == initcopy_iochannel(&ioc, pipeioc[i]));
      TEST(N+2 == ioc);
      if (i) {
         TEST(1 == write(ioc, "1", 1));
         TEST(1 == read(pipeioc[0], buffer, 1));
      } else {
         TEST(1 == write(pipeioc[1], "1", 1));
         TEST(1 == read(ioc, buffer, 1));
      }
      TEST('1' == buffer[0]);
      TEST(0 == free_iochannel(&ioc));
      TEST(-1 == ioc);
      TEST(1 == write(pipeioc[1], "2", 1));
      TEST(1 == read(pipeioc[0], buffer, 1));
      TEST('2' == buffer[0]);
   }
   TEST(0 == free_iochannel(&pipeioc[0]));
   TEST(0 == free_iochannel(&pipeioc[1]));

   return 0;
ONERR:
   free_iochannel(&ioc);
   free_iochannel(&pipeioc[0]);
   free_iochannel(&pipeioc[1]);
   return EINVAL;
}

static int test_query(directory_t * tempdir)
{
   iochannel_t ioc   = iochannel_FREE;
   iochannel_t pipeioc[] = { iochannel_FREE, iochannel_FREE };
   DIR       * dir   = 0;
   file_t      file  = file_FREE;
   size_t      size;
   ipsocket_t  sock  = ipsocket_FREE;
   ipsocket_t  csock = ipsocket_FREE;
   ipsocket_t  ssock = ipsocket_FREE;
   uint8_t     buf[10];
   uint8_t   * logbuffer;
   size_t      logsize1;
   size_t      logsize2;

   // TEST isfree_iochannel
   iochannel_t testioc[] =  { iochannel_STDIN, iochannel_STDOUT, iochannel_STDERR, 100, INT_MAX };
   for (unsigned i = 0; i < lengthof(testioc); ++i) {
      TEST(0 == isfree_iochannel(testioc[i]));
   }
   TEST(1 == isfree_iochannel(ioc));

   // TEST isvalid_iochannel
   TEST(0 == isvalid_iochannel(file_FREE));
   TEST(0 == isvalid_iochannel(100));
   TEST(0 == isvalid_iochannel(INT_MAX));
   TEST(1 == isvalid_iochannel(file_STDIN));
   TEST(1 == isvalid_iochannel(file_STDOUT));
   TEST(1 == isvalid_iochannel(file_STDERR));
   TEST(0 == pipe2(pipeioc, O_CLOEXEC));
   for (unsigned i = 0; i < 2; ++i) {
      ioc = pipeioc[i];
      TEST(1 == isvalid_iochannel(pipeioc[i]));
      TEST(0 == free_iochannel(&pipeioc[i]));
      TEST(0 == isvalid_iochannel(ioc));
   }

   // TEST accessmode_iochannel: predefined channels
   TEST(accessmode_NONE  == accessmode_iochannel(iochannel_FREE));
   TEST(accessmode_READ  == (accessmode_READ&accessmode_iochannel(iochannel_STDIN)));
   TEST(accessmode_WRITE == (accessmode_WRITE&accessmode_iochannel(iochannel_STDOUT)));
   TEST(accessmode_WRITE == (accessmode_WRITE&accessmode_iochannel(iochannel_STDERR)));

   // TEST accessmode_iochannel: pipe
   TEST(0 == pipe2(pipeioc, O_CLOEXEC));
   for (unsigned i = 0; i < 2; ++i) {
      accessmode_e amode = i ? accessmode_WRITE : accessmode_READ;
      TEST(amode == accessmode_iochannel(pipeioc[i]));
      ioc = pipeioc[i];
      TEST(0 == free_iochannel(&pipeioc[i]));
      TEST(accessmode_NONE == accessmode_iochannel(ioc));
   }

   // TEST accessmode_iochannel: initcopy_iochannel
   TEST(0 == pipe2(pipeioc, O_CLOEXEC));
   for (unsigned i = 0; i < 2; ++i) {
      accessmode_e amode = i ? accessmode_WRITE : accessmode_READ;
      TEST(0 == initcopy_iochannel(&ioc, pipeioc[i]));
      TEST(amode == accessmode_iochannel(ioc));
      TEST(0 == free_iochannel(&ioc));
      TEST(amode == accessmode_iochannel(pipeioc[i]));
      TEST(0 == free_iochannel(&pipeioc[i]));
   }

   // TEST accessmode_iochannel: file_t
   TEST(0 == initcreate_file(&file, "accessmode", tempdir));
   TEST(accessmode_RDWR == accessmode_iochannel(io_file(file)));
   TEST(0 == free_file(&file));
   TEST(0 == init_file(&file, "accessmode", accessmode_READ, tempdir));
   TEST(accessmode_READ == accessmode_iochannel(io_file(file)));
   TEST(0 == free_file(&file));
   TEST(0 == init_file(&file, "accessmode", accessmode_WRITE, tempdir));
   TEST(accessmode_WRITE == accessmode_iochannel(io_file(file)));
   TEST(0 == free_file(&file));
   TEST(0 == init_file(&file, "accessmode", accessmode_RDWR, tempdir));
   TEST(accessmode_RDWR == accessmode_iochannel(io_file(file)));
   TEST(0 == free_file(&file));
   TEST(0 == removefile_directory(tempdir, "accessmode"));

   // TEST accessmode_iochannel: ipsocket_t
   ipaddr_storage_t ipaddr;
   ipaddr_storage_t ipaddr2;
   TEST(0 != initany_ipaddrstorage(&ipaddr, ipprotocol_UDP, ipport_ANY, ipversion_4));
   TEST(0 == init_ipsocket(&sock, (ipaddr_t*)&ipaddr));
   TEST(accessmode_RDWR == accessmode_iochannel(io_ipsocket(&sock)));
   TEST(0 == free_ipsocket(&sock));
   TEST(0 != initany_ipaddrstorage(&ipaddr, ipprotocol_TCP, ipport_ANY, ipversion_4));
   TEST(0 == initlisten_ipsocket(&sock, (ipaddr_t*)&ipaddr, 1));
   TEST(accessmode_RDWR == accessmode_iochannel(io_ipsocket(&sock)));
   TEST(0 == localaddr_ipsocket(&sock, (ipaddr_t*)&ipaddr));
   TEST(0 != initany_ipaddrstorage(&ipaddr2, ipprotocol_TCP, ipport_ANY, ipversion_4));
   TEST(0 == initconnect_ipsocket(&csock, (const ipaddr_t*)&ipaddr, (const ipaddr_t*)&ipaddr2));
   TEST(accessmode_RDWR == accessmode_iochannel(io_ipsocket(&csock)));
   TEST(0 == free_ipsocket(&csock));
   TEST(0 == free_ipsocket(&sock));

   // TEST isclosedread_iochannel: predefined channels
   TEST(0 == isclosedread_iochannel(iochannel_FREE)); // error ignored
   TEST(0 == isclosedread_iochannel(iochannel_STDIN));
   TEST(0 == isclosedread_iochannel(iochannel_STDOUT));
   TEST(0 == isclosedread_iochannel(iochannel_STDERR));

   // TEST isclosedwrite_iochannel: predefined channels
   TEST(0 == isclosedwrite_iochannel(iochannel_FREE)); // error ignored
   TEST(0 == isclosedwrite_iochannel(iochannel_STDIN));
   TEST(0 == isclosedwrite_iochannel(iochannel_STDOUT));
   TEST(0 == isclosedwrite_iochannel(iochannel_STDERR));

   // TEST isclosedread_iochannel, isclosedwrite_iochannel: close writer
   TEST(0 == pipe2(pipeioc, O_CLOEXEC));
   for (int i = 0; i <= 1; ++i) {
      TEST(0 == isclosedread_iochannel(pipeioc[i]));
      TEST(0 == isclosedwrite_iochannel(pipeioc[i]));
   }
   TEST(0 == write_iochannel(pipeioc[1], 1, "x", 0));
   for (int i = 0; i <= 1; ++i) {
      TEST(0 == isclosedread_iochannel(pipeioc[i]));
      TEST(0 == isclosedwrite_iochannel(pipeioc[i]));
   }
   TEST(0 == free_iochannel(&pipeioc[1]));
   TEST(0 == isclosedread_iochannel(pipeioc[0]));
   TEST(0 == isclosedwrite_iochannel(pipeioc[0]));
   TEST(0 == read_iochannel(pipeioc[0], 1, buf, 0));
   TEST(1 == isclosedread_iochannel(pipeioc[0]));
   TEST(0 == isclosedwrite_iochannel(pipeioc[0]));
   TEST(0 == free_iochannel(&pipeioc[0]));

   // TEST isclosedread_iochannel, isclosedwrite_iochannel: close reader
   TEST(0 == pipe2(pipeioc, O_CLOEXEC));
   TEST(0 == free_iochannel(&pipeioc[0]));
   TEST(0 == isclosedread_iochannel(pipeioc[1]));
   TEST(1 == isclosedwrite_iochannel(pipeioc[1]));
   TEST(0 == free_iochannel(&pipeioc[1]));

   // TEST isclosedread_iochannel, isclosedwrite_iochannel: close reader (full queue)
   TEST(0 == pipe2(pipeioc, O_CLOEXEC|O_NONBLOCK));
   while (0 == write_iochannel(pipeioc[1], sizeof(buf), buf, &size)) {
   }
   TEST(0 == free_iochannel(&pipeioc[0]));
   TEST(0 == isclosedread_iochannel(pipeioc[1]));
   TEST(1 == isclosedwrite_iochannel(pipeioc[1]));
   TEST(0 == free_iochannel(&pipeioc[1]));

   // TEST isclosedread_iochannel, isclosedwrite_iochannel: initcopy_iochannel
   TEST(0 == pipe2(pipeioc, O_CLOEXEC));
   for (unsigned i = 0; i < 2; ++i) {
      TEST(0 == initcopy_iochannel(&ioc, pipeioc[i]));
      TEST(0 == isclosedread_iochannel(ioc));
      TEST(0 == isclosedwrite_iochannel(ioc));
      TEST(0 == free_iochannel(&ioc));
   }
   TEST(0 == free_iochannel(&pipeioc[1]));
   TEST(0 == initcopy_iochannel(&ioc, pipeioc[0]));
   TEST(1 == isclosedread_iochannel(ioc));
   TEST(0 == isclosedwrite_iochannel(ioc));
   TEST(0 == free_iochannel(&ioc));
   TEST(0 == free_iochannel(&pipeioc[0]));
   TEST(0 == pipe2(pipeioc, O_CLOEXEC));
   TEST(0 == free_iochannel(&pipeioc[0]));
   TEST(0 == initcopy_iochannel(&ioc, pipeioc[1]));
   TEST(0 == isclosedread_iochannel(ioc));
   TEST(1 == isclosedwrite_iochannel(ioc));
   TEST(0 == free_iochannel(&ioc));
   TEST(0 == free_iochannel(&pipeioc[1]));

   // TEST isclosedread_iochannel, isclosedwrite_iochannel: file_t
   TEST(0 == initcreate_file(&file, "isclosed", tempdir));
   TEST(0 == truncate_file(file, 100));
   TEST(0 == isclosedread_iochannel(io_file(file)));
   TEST(0 == isclosedwrite_iochannel(io_file(file)));
   TEST(0 == free_file(&file));
   for (int i = 0; i <= 2; ++i) {
      TEST(0 == init_file(&file, "isclosed", i == 2 ? accessmode_RDWR : i == 1 ? accessmode_WRITE : accessmode_READ, tempdir));
      TEST(0 == isclosedread_iochannel(io_file(file)));
      TEST(0 == isclosedwrite_iochannel(io_file(file)));
      TEST(0 == free_file(&file));
   }
   TEST(0 == remove_file("isclosed", tempdir));

   // TEST isclosedread_iochannel, isclosedwrite_iochannel: directory
   dir = opendir(".");
   TEST(0 == isclosedread_iochannel(dirfd(dir)));
   TEST(0 == isclosedwrite_iochannel(dirfd(dir)));
   TEST(0 == closedir(dir));
   dir = 0;

   // TEST isclosedread_iochannel, isclosedwrite_iochannel: ipsocket_t
   TEST(0 != initany_ipaddrstorage(&ipaddr, ipprotocol_UDP, ipport_ANY, ipversion_4));
   TEST(0 == init_ipsocket(&sock, (ipaddr_t*)&ipaddr));
   TEST(0 == isclosedread_iochannel(io_ipsocket(&sock)));
   TEST(0 == isclosedwrite_iochannel(io_ipsocket(&sock)));
   TEST(0 == free_ipsocket(&sock));
   TEST(0 != initany_ipaddrstorage(&ipaddr, ipprotocol_TCP, ipport_ANY, ipversion_4));
   TEST(0 == initlisten_ipsocket(&sock, (ipaddr_t*)&ipaddr, 1));
   TEST(0 == isclosedread_iochannel(io_ipsocket(&sock)));
   TEST(0 == isclosedwrite_iochannel(io_ipsocket(&sock)));
   TEST(0 == localaddr_ipsocket(&sock, (ipaddr_t*)&ipaddr));
   TEST(0 != initany_ipaddrstorage(&ipaddr2, ipprotocol_TCP, ipport_ANY, ipversion_4));
   TEST(0 == initconnect_ipsocket(&csock, (const ipaddr_t*)&ipaddr, (const ipaddr_t*)&ipaddr2));
   TEST(0 == isclosedread_iochannel(io_ipsocket(&sock)));
   TEST(0 == isclosedwrite_iochannel(io_ipsocket(&sock)));
   TEST(0 == isclosedread_iochannel(io_ipsocket(&csock)));
   TEST(0 == isclosedwrite_iochannel(io_ipsocket(&csock)));
   TEST(0 == initaccept_ipsocket(&ssock, &sock, 0));
   TEST(0 == isclosedread_iochannel(io_ipsocket(&sock)));
   TEST(0 == isclosedwrite_iochannel(io_ipsocket(&sock)));
   TEST(0 == isclosedread_iochannel(io_ipsocket(&csock)));
   TEST(0 == isclosedwrite_iochannel(io_ipsocket(&csock)));
   for (int i = 0; i < 3; ++i) {
      TEST(0 == write_iochannel(ssock, 1, "x", 0));
   }
   TEST(0 == isclosedread_iochannel(io_ipsocket(&csock)));
   TEST(0 == isclosedwrite_iochannel(io_ipsocket(&csock)));
   TEST(0 == free_ipsocket(&ssock)); // remote closed
   for (int i = 1; i < 10; ++i) {
      if (EPIPE == write_iochannel(csock, 1, &csock, 0)) break;
      pthread_yield();
   }
   TEST(1 == isclosedwrite_iochannel(io_ipsocket(&csock)));
   for (int i = 0; i < 3; ++i) {
      TEST(0 == isclosedread_iochannel(io_ipsocket(&csock)));
      TEST(0 == read_iochannel(csock, 1, buf, 0));
   }
   TEST(1 == isclosedread_iochannel(io_ipsocket(&csock)));
   TEST(1 == isclosedwrite_iochannel(io_ipsocket(&csock)));
   TEST(0 == free_ipsocket(&csock));
   TEST(0 == free_ipsocket(&sock));

   // TEST isclosedread_iochannel, isclosedwrite_iochannel: ipsocket_t (outbufffer full)
   TEST(0 != initany_ipaddrstorage(&ipaddr, ipprotocol_TCP, ipport_ANY, ipversion_4));
   TEST(0 == initlisten_ipsocket(&sock, (ipaddr_t*)&ipaddr, 1));
   TEST(0 == localaddr_ipsocket(&sock, (ipaddr_t*)&ipaddr));
   TEST(0 != initany_ipaddrstorage(&ipaddr2, ipprotocol_TCP, ipport_ANY, ipversion_4));
   TEST(0 == initconnect_ipsocket(&csock, (const ipaddr_t*)&ipaddr, (const ipaddr_t*)&ipaddr2));
   TEST(0 == initaccept_ipsocket(&ssock, &sock, 0));
   while (0 == write_iochannel(csock, sizeof(buf), buf, 0)) {
   }
   TEST(0 == isclosedread_iochannel(io_ipsocket(&csock)));
   TEST(0 == isclosedwrite_iochannel(io_ipsocket(&csock)));
   TEST(0 == free_ipsocket(&ssock)); // remote closed
   TEST(1 == isclosedread_iochannel(io_ipsocket(&csock)));
   TEST(1 == isclosedwrite_iochannel(io_ipsocket(&csock)));
   TEST(0 == free_ipsocket(&csock));
   TEST(0 == free_ipsocket(&sock));

   // TEST sizeread_iochannel: predefined channels
   TEST(0 == sizeread_iochannel(sys_iochannel_STDIN, &size));
   TEST(0 == size);
   TEST(0 == sizeread_iochannel(sys_iochannel_STDOUT, &size));
   TEST(0 == size);
   TEST(0 == sizeread_iochannel(sys_iochannel_STDERR, &size));
   TEST(0 == size);

   // TEST sizeread_iochannel: pipe
   TEST(0 == pipe2(pipeioc, O_CLOEXEC));
   for (int i = 0; i <= 1; ++i) {
      size = 1;
      TEST(0 == sizeread_iochannel(pipeioc[i], &size));
      TEST(0 == size);
   }
   TEST(0 == write_iochannel(pipeioc[1], 1, "x", 0));
   for (int i = 0; i <= 1; ++i) {
      TEST(0 == sizeread_iochannel(pipeioc[i], &size));
      TEST(1 == size);
   }
   for (int i = 0; i <= 1; ++i) {
      TEST(0 == free_iochannel(&pipeioc[i]));
   }

   // TEST sizeread_iochannel: file
   TEST(0 == initcreate_file(&file, "readsize", tempdir));
   off_t test_sizes[] = { 100, INT_MAX, (off_t)INT_MAX+1, SIZE_MAX, (off_t)SIZE_MAX+1, (off_t)SIZE_MAX + INT_MAX };
   for (unsigned ts = 0; ts < lengthof(test_sizes); ++ts) {
      const off_t S = test_sizes[ts];
      TEST(0 == truncate_file(file, S));
      TEST(0 == sizeread_iochannel(file, &size));
      TEST(size == (S > SIZE_MAX ? SIZE_MAX : S));
   }
   TEST(0 == free_file(&file));
   TEST(0 == remove_file("readsize", tempdir));

   // TEST sizeread_iochannel: directory
   dir = opendir(".");
   TEST(0 == sizeread_iochannel(dirfd(dir), &size));
   TEST(0 <  size);
   TEST(0 == closedir(dir));
   dir = 0;

   // TEST sizeread_iochannel: ipsocket_t UDP
   TEST(0 != initany_ipaddrstorage(&ipaddr, ipprotocol_UDP, ipport_ANY, ipversion_4));
   TEST(0 != initany_ipaddrstorage(&ipaddr2, ipprotocol_UDP, ipport_ANY, ipversion_4));
   TEST(0 == init_ipsocket(&sock, (ipaddr_t*)&ipaddr));
   TEST(0 == localaddr_ipsocket(&sock, (ipaddr_t*)&ipaddr2));
   TEST(0 == initconnect_ipsocket(&csock, (ipaddr_t*)&ipaddr2, (ipaddr_t*)&ipaddr));
   TEST(0 == sizeread_iochannel(io_ipsocket(&sock), &size));
   TEST(0 == size);
   TEST(0 == sizeread_iochannel(io_ipsocket(&csock), &size));
   TEST(0 == size);
   TEST(4 == write(io_ipsocket(&csock), "123", 4));
   TEST(3 == write(io_ipsocket(&csock), "123", 3));
   struct pollfd pfd = { .events = POLLIN, .fd = io_ipsocket(&sock) };
   TEST(1 == poll(&pfd, 1, 100));
   TEST(0 == sizeread_iochannel(io_ipsocket(&sock), &size));
   TEST(4 == size);
   TEST(0 == sizeread_iochannel(io_ipsocket(&csock), &size));
   TEST(0 == size);
   TEST(4 == read(io_ipsocket(&sock), buf, 4));
   TEST(0 == sizeread_iochannel(io_ipsocket(&sock), &size));
   TEST(3 == size);
   TEST(0 == free_ipsocket(&sock));
   TEST(0 == free_ipsocket(&csock));

   // TEST sizeread_iochannel: ipsocket_t TCP
   TEST(0 != initany_ipaddrstorage(&ipaddr, ipprotocol_TCP, ipport_ANY, ipversion_4));
   TEST(0 != initany_ipaddrstorage(&ipaddr2, ipprotocol_TCP, ipport_ANY, ipversion_4));
   TEST(0 == initlisten_ipsocket(&sock, (ipaddr_t*)&ipaddr, 1));
   TEST(EINVAL == sizeread_iochannel(io_ipsocket(&sock), &size));
   TEST(0 == localaddr_ipsocket(&sock, (ipaddr_t*)&ipaddr));
   TEST(0 == initconnect_ipsocket(&csock, (const ipaddr_t*)&ipaddr, (const ipaddr_t*)&ipaddr2));
   pfd = (struct pollfd) { .events = POLLIN, .fd = io_ipsocket(&sock) };
   TEST(1 == poll(&pfd, 1, 100));
   TEST(0 == initaccept_ipsocket(&ssock, &sock, 0));
   TEST(0 == sizeread_iochannel(io_ipsocket(&ssock), &size));
   TEST(0 == size);
   TEST(0 == sizeread_iochannel(io_ipsocket(&csock), &size));
   TEST(0 == size);
   TEST(0 == write_iochannel(ssock, 1, "x", 0));
   TEST(0 == sizeread_iochannel(io_ipsocket(&ssock), &size));
   TEST(0 == size);
   pfd = (struct pollfd) { .events = POLLIN, .fd = io_ipsocket(&csock) };
   TEST(1 == poll(&pfd, 1, 100));
   TEST(0 == sizeread_iochannel(io_ipsocket(&csock), &size));
   TEST(1 == size);
   TEST(0 == free_ipsocket(&ssock)); // remote closed
   TEST(0 == sizeread_iochannel(io_ipsocket(&csock), &size));
   TEST(1 == size);
   TEST(0 == read_iochannel(csock, 1, buf, 0));
   TEST(0 == sizeread_iochannel(io_ipsocket(&csock), &size));
   TEST(0 == size);
   TEST(0 == free_ipsocket(&csock));
   TEST(0 == free_ipsocket(&sock));

   // TEST sizeread_iochannel: EBADF
   GETBUFFER_ERRLOG(&logbuffer, &logsize1);
   TEST(EBADF == sizeread_iochannel(sys_iochannel_FREE, &size));
   GETBUFFER_ERRLOG(&logbuffer, &logsize2);
   TEST(logsize1 == logsize2);

   return 0;
ONERR:
   free_iochannel(&ioc);
   free_iochannel(&pipeioc[0]);
   free_iochannel(&pipeioc[1]);
   free_ipsocket(&sock);
   free_ipsocket(&csock);
   free_ipsocket(&ssock);
   free_file(&file);
   if (dir) closedir(dir);
   return EINVAL;
}

static void sigusr1_handler(int signr)
{
   assert(signr == SIGUSR1);
   send_signalrt(0, 0);
}

   // calculate size of pipe buffer
static size_t determine_buffer_size(void)
{
   int     fd[2] = { -1, -1 };
   size_t  buffersize = 0;
   uint8_t buffer[1024];

   TEST(0 == pipe2(fd, O_CLOEXEC|O_NONBLOCK));
   for (ssize_t written; 0 < (written = write(fd[1], buffer, sizeof(buffer)));) {
      buffersize += (size_t)written;
   }
   TEST(-1 == write(fd[1], buffer, 1));
   TEST(EAGAIN == errno);
   TEST(0 == free_iochannel(&fd[0]));
   TEST(0 == free_iochannel(&fd[1]));

   return buffersize;
ONERR:
   free_iochannel(&fd[0]);
   free_iochannel(&fd[1]);
   return 0;
}

static volatile size_t s_thread_count     = 0;
static volatile bool   s_thread_isrunning = 0;

typedef struct threadarg_t {
   iochannel_t ioc;
   uint8_t *   buffer;
   size_t      size;
} threadarg_t;

static int thread_reader(threadarg_t* arg)
{
   size_t bytes_read;
   size_t value = 0;

   s_thread_isrunning = true;

   while (s_thread_isrunning) {
      TEST(0 == read_iochannel(arg->ioc, arg->size, arg->buffer, &bytes_read));
      TEST(bytes_read == arg->size);
      for (unsigned i = 0; i < arg->size; ++i, value += 61) {
         TEST(arg->buffer[i] == (uint8_t)value);
      }
      ++ s_thread_count;
   }

   return 0;
ONERR:
   s_thread_isrunning = false;
   return EINVAL;
}

static int thread_writer(threadarg_t* arg)
{
   size_t   bytes_written;
   size_t   value = 0;

   s_thread_isrunning = true;

   while (s_thread_isrunning) {
      for (unsigned i = 0; i < arg->size; ++i, value += 61) {
         arg->buffer[i] = (uint8_t)value;
      }
      TEST(0 == write_iochannel(arg->ioc, arg->size, arg->buffer, &bytes_written));
      TEST(bytes_written == arg->size);
      ++ s_thread_count;
   }

   return 0;
ONERR:
   s_thread_isrunning = false;
   return EINVAL;
}

static int thread_readerror(threadarg_t* arg)
{
   s_thread_isrunning = true;

   int err = read_iochannel(arg->ioc, 1, arg->buffer, 0);
   ++ s_thread_count;

   s_thread_isrunning = false;
   return err;
}

static int thread_writeerror(threadarg_t* arg)
{
   s_thread_isrunning = true;

   int err = write_iochannel(arg->ioc, arg->size, arg->buffer, 0);
   ++ s_thread_count;

   if (0 == err) {
      err = write_iochannel(arg->ioc, 1, arg->buffer, 0);
      ++ s_thread_count;
   }

   s_thread_isrunning = false;
   return err;
}

static int test_readwrite(directory_t* tempdir)
{
   iochannel_t ioc       = iochannel_FREE;
   iochannel_t pipeioc[] = { iochannel_FREE, iochannel_FREE };
   thread_t *  thread    = 0;
   threadarg_t threadarg;
   file_t      file      = file_FREE;
   ipsocket_t  ssock     = ipsocket_FREE;
   ipsocket_t  csock     = ipsocket_FREE;
   memblock_t  buffer[2] = { memblock_FREE, memblock_FREE };
   bool        isoldsignalmask = false;
   sigset_t    oldsignalmask;
   sigset_t    signalmask;
   bool        isoldhandler = false;
   struct
   sigaction   newact;
   struct
   sigaction   oldact;
   size_t      buffersize;
   size_t      bytes_read;
   size_t      bytes_written;
   uint8_t   * logbuffer;
   size_t      logsize1;
   size_t      logsize2;

   // prepare
   buffersize = determine_buffer_size();
   TEST(0 < buffersize && buffersize < 1024*1204);
   TEST(0 == ALLOC_PAGECACHE(pagesize_1MB, &buffer[0]));
   TEST(0 == ALLOC_PAGECACHE(pagesize_1MB, &buffer[1]));
   TEST(0 == sigemptyset(&signalmask));
   TEST(0 == sigaddset(&signalmask, SIGUSR1));
   TEST(0 == sigprocmask(SIG_UNBLOCK, &signalmask, &oldsignalmask));
   isoldsignalmask = true;
   sigemptyset(&newact.sa_mask);
   newact.sa_flags   = 0;
   newact.sa_handler = &sigusr1_handler;
   TEST(0 == sigaction(SIGUSR1, &newact, &oldact));
   isoldhandler = true;

   // TEST read_iochannel: blocking I/O
   TEST(0 == pipe2(pipeioc, O_CLOEXEC));
   s_thread_count = 0;
   threadarg = (threadarg_t) { pipeioc[0], buffer[1].addr, buffersize };
   TEST(0 == newgeneric_thread(&thread, &thread_reader, &threadarg));
   while (!s_thread_isrunning) {
      yield_thread();
   }
   for (size_t value = 0, nrbuffer = 0; nrbuffer < 8; ++nrbuffer) {
      for (size_t i = 0; i < buffersize; ++i, value += 61) {
         buffer[0].addr[i] = (uint8_t)value;
      }
      yield_thread();
      if (nrbuffer == 7) s_thread_isrunning = false;
      TEST(s_thread_count == nrbuffer);
      TEST(buffersize == (size_t)write(pipeioc[1], buffer[0].addr, buffersize));
      while (s_thread_count == nrbuffer) {
         yield_thread();
      }
      TEST(s_thread_count == (nrbuffer+1));
   }
   TEST(0 == join_thread(thread));
   TEST(0 == returncode_thread(thread));
   TEST(0 == delete_thread(&thread));
   TEST(0 == free_iochannel(&pipeioc[0]));
   TEST(0 == free_iochannel(&pipeioc[1]));

   // TEST write_iochannel: blocking I/O
   TEST(0 == pipe2(pipeioc, O_CLOEXEC));
   s_thread_count = 0;
   threadarg = (threadarg_t) { pipeioc[1], buffer[1].addr, buffersize };
   TEST(0 == newgeneric_thread(&thread, &thread_writer, &threadarg));
   while (!s_thread_isrunning) {
      yield_thread();
   }
   for (size_t value = 0, nrbuffer = 0; nrbuffer < 8; ++nrbuffer) {
      while (s_thread_count == nrbuffer) {
         yield_thread();
      }
      TEST(s_thread_count == (nrbuffer+1));
      if (nrbuffer == 7) s_thread_isrunning = false;
      bytes_read = 0;
      TEST(0 == read_iochannel(pipeioc[0], buffersize, buffer[0].addr, &bytes_read));
      TEST(bytes_read == buffersize);
      for (unsigned i = 0; i < buffersize; ++i, value += 61) {
         TEST(buffer[0].addr[i] == (uint8_t)value);
      }
   }
   TEST(0 == join_thread(thread));
   TEST(0 == returncode_thread(thread));
   TEST(0 == delete_thread(&thread));
   TEST(0 == free_iochannel(&pipeioc[0]));
   TEST(0 == free_iochannel(&pipeioc[1]));

   // TEST write_iochannel, read_iochannel: EBADF
   TEST(0 == pipe2(pipeioc, O_CLOEXEC));
   // write on readonly and vice versa
   TEST(EBADF == read_iochannel(pipeioc[1], buffersize, buffer[0].addr, 0));
   TEST(EBADF == write_iochannel(pipeioc[0], buffersize, buffer[0].addr, 0));
   // wrong file descriptor value
   TEST(EBADF == read_iochannel(iochannel_FREE, buffersize, buffer[0].addr, 0));
   TEST(EBADF == write_iochannel(iochannel_FREE, buffersize, buffer[0].addr, 0));
   TEST(0 == free_iochannel(&pipeioc[0]));
   TEST(0 == free_iochannel(&pipeioc[1]));

   // TEST read_iochannel: end of input
   TEST(0 == pipe2(pipeioc, O_CLOEXEC));
   TEST(0 == write_iochannel(pipeioc[1], buffersize, buffer[0].addr, &bytes_written));
   TEST(bytes_written == buffersize);
   TEST(0 == read_iochannel(pipeioc[0], buffersize, buffer[0].addr, &bytes_read));
   TEST(bytes_read == buffersize);
   TEST(0 == free_iochannel(&pipeioc[1]));
   for (unsigned i = 0; i < 100; ++i) {
      TEST(0 == read_iochannel(pipeioc[0], buffersize, buffer[0].addr, &bytes_read));
      TEST(0 == bytes_read);
   }
   TEST(0 == free_iochannel(&pipeioc[0]));

   // TEST write_iochannel: EPIPE
   TEST(0 == pipe2(pipeioc, O_CLOEXEC));
   TEST(0 == write_iochannel(pipeioc[1], buffersize, buffer[0].addr, &bytes_written));
   TEST(bytes_written == buffersize);
   TEST(0 == read_iochannel(pipeioc[0], buffersize, buffer[0].addr, &bytes_read));
   TEST(bytes_read == buffersize);
   TEST(0 == free_iochannel(&pipeioc[0]));
   GETBUFFER_ERRLOG(&logbuffer, &logsize1);
   TEST(EPIPE == write_iochannel(pipeioc[1], buffersize, buffer[0].addr, &bytes_written));
   GETBUFFER_ERRLOG(&logbuffer, &logsize2);
   TEST(logsize1 == logsize2); // no log written
   TEST(0 == free_iochannel(&pipeioc[1]));

   // TEST read_iochannel, write_iochannel: non blocking I/O (EAGAIN)
   TEST(0 == pipe2(pipeioc, O_CLOEXEC|O_NONBLOCK));
   for (size_t nrbuffer = 0, wvalue = 0, rvalue = 0; nrbuffer < 8; ++nrbuffer) {
      for (size_t i = 0; i < buffersize; ++i, wvalue += 61) {
         buffer[0].addr[i] = (uint8_t)wvalue;
      }
      TEST(0 == write_iochannel(pipeioc[1], buffersize, buffer[0].addr, &bytes_written));
      TEST(bytes_written == buffersize);
      TEST(EAGAIN == write_iochannel(pipeioc[1], 1, buffer[0].addr, &bytes_written));
      TEST(EAGAIN == write_iochannel(pipeioc[1], 1, buffer[0].addr, &bytes_written));
      TEST(0 == read_iochannel(pipeioc[0], buffersize, buffer[1].addr, &bytes_read));
      TEST(bytes_read == buffersize);
      for (size_t i = 0; i < buffersize; ++i, rvalue += 61) {
         TEST(buffer[1].addr[i] == (uint8_t)rvalue);
      }
      TEST(EAGAIN == read_iochannel(pipeioc[0], 1, buffer[1].addr, &bytes_read));
      TEST(EAGAIN == read_iochannel(pipeioc[0], 1, buffer[1].addr, &bytes_read));
   }
   TEST(0 == free_iochannel(&pipeioc[0]));
   TEST(0 == free_iochannel(&pipeioc[1]));

   // TEST read_iochannel: no EINTR !!
   TEST(0 == pipe2(pipeioc, O_CLOEXEC));
   s_thread_count = 0;
   threadarg = (threadarg_t) { pipeioc[0], buffer[1].addr, buffersize };
   TEST(0 == newgeneric_thread(&thread, &thread_readerror, &threadarg));
   while (!s_thread_isrunning) {
      yield_thread();
   }
   while (0 == trywait_signalrt(0, 0));
   pthread_kill(thread->sys_thread, SIGUSR1);
   TEST(0 == wait_signalrt(0, 0)); // wait for SIGUSR1 processed
   TEST(0 == s_thread_count);
   TEST(1 == s_thread_isrunning);
   TEST(1 == write(pipeioc[1], buffer[0].addr, 1));
   TEST(0 == join_thread(thread));
   TEST(1 == s_thread_count);
   TEST(0 == s_thread_isrunning);
   TEST(0 == returncode_thread(thread));
   TEST(0 == delete_thread(&thread));
   TEST(0 == free_iochannel(&pipeioc[0]));
   TEST(0 == free_iochannel(&pipeioc[1]));

   // TEST write_iochannel: no EINTR !
   TEST(0 == pipe2(pipeioc, O_CLOEXEC));
   s_thread_count = 0;
   threadarg = (threadarg_t) { pipeioc[1], buffer[1].addr, buffersize };
   TEST(0 == newgeneric_thread(&thread, &thread_writeerror, &threadarg));
   while (!s_thread_isrunning || !s_thread_count) {
      yield_thread();
   }
   while (0 == trywait_signalrt(0, 0));
   pthread_kill(thread->sys_thread, SIGUSR1);
   TEST(0 == wait_signalrt(0, 0)); // wait for SIGUSR1 processed
   TEST(1 == s_thread_count);
   TEST(1 == s_thread_isrunning);
   TEST(buffersize == (size_t)read(pipeioc[0], buffer[0].addr, buffersize));
   TEST(0 == join_thread(thread));
   TEST(2 == s_thread_count);
   TEST(0 == s_thread_isrunning);
   TEST(0 == returncode_thread(thread));
   TEST(0 == delete_thread(&thread));
   TEST(0 == free_iochannel(&pipeioc[0]));
   TEST(0 == free_iochannel(&pipeioc[1]));

   // TEST read_iochannel, write_iochannel: file_t
   TEST(0 == initcreate_file(&file, "readtest", tempdir));
   TEST(0 == free_file(&file));
   for (size_t nrbuffer = 0, wvalue = 0, rvalue = 0; nrbuffer < 8; ++nrbuffer) {
      for (size_t i = 0; i < buffersize; ++i, wvalue += 61) {
         buffer[0].addr[i] = (uint8_t)wvalue;
      }
      TEST(0 == init_file(&file, "readtest", accessmode_WRITE, tempdir));
      TEST(0 == write_iochannel(io_file(file), buffersize, buffer[0].addr, &bytes_written));
      TEST(bytes_written == buffersize);
      TEST(0 == free_file(&file));
      TEST(0 == init_file(&file, "readtest", accessmode_READ, tempdir));
      TEST(0 == read_iochannel(io_file(file), buffersize, buffer[1].addr, &bytes_read));
      TEST(bytes_read == buffersize);
      TEST(0 == free_file(&file));
      for (size_t i = 0; i < buffersize; ++i, rvalue += 61) {
         buffer[1].addr[i] = (uint8_t)rvalue;
      }
   }
   TEST(0 == removefile_directory(tempdir, "readtest"));

   // TEST read_iochannel, write_iochannel: ipsocket_t
   ipaddr_storage_t ipaddr;
   ipaddr_storage_t ipaddr2;
   TEST(0 != initany_ipaddrstorage(&ipaddr, ipprotocol_TCP, ipport_ANY, ipversion_4));
   TEST(0 != initany_ipaddrstorage(&ipaddr2, ipprotocol_TCP, ipport_ANY, ipversion_4));
   TEST(0 == initlisten_ipsocket(&ssock, (ipaddr_t*)&ipaddr, 1));
   TEST(0 == localaddr_ipsocket(&ssock, (ipaddr_t*)&ipaddr));
   TEST(0 == initconnect_ipsocket(&csock, (const ipaddr_t*)&ipaddr, (const ipaddr_t*)&ipaddr2));
   ipsocket_t lsock = ssock;
   TEST(0 == initaccept_ipsocket(&ssock, &lsock, 0));
   TEST(0 == free_ipsocket(&lsock));
   for (size_t nrbuffer = 0, wvalue = 0, rvalue = 0; nrbuffer < 8; ++nrbuffer) {
      for (size_t i = 0; i < buffersize; ++i, wvalue += 61) {
         buffer[0].addr[i] = (uint8_t)wvalue;
      }
      TEST(0 == write_iochannel(io_ipsocket(&ssock), buffersize, buffer[0].addr, &bytes_written));
      TEST(bytes_written == buffersize);
      TEST(0 == read_iochannel(io_ipsocket(&csock), buffersize, buffer[1].addr, &bytes_read));
      if (bytes_read < buffersize) {
         size_t extra;
         TEST(0 == read_iochannel(io_ipsocket(&csock), buffersize - bytes_read, buffer[1].addr, &extra));
         bytes_read += extra;
      }
      TEST(bytes_read == buffersize);
      for (size_t i = 0; i < buffersize; ++i, rvalue += 61) {
         buffer[1].addr[i] = (uint8_t)rvalue;
      }
   }
   TEST(0 == shutdown(io_ipsocket(&csock), SHUT_RD));
   TEST(0 == shutdown(io_ipsocket(&ssock), SHUT_WR));
   TEST(EPIPE == write_iochannel(io_ipsocket(&ssock), buffersize, buffer[0].addr, &bytes_written));
   TEST(0 == read_iochannel(io_ipsocket(&csock), buffersize, buffer[1].addr, &bytes_read));
   TEST(0 == bytes_read);
   TEST(0 == free_ipsocket(&csock));
   TEST(0 == free_ipsocket(&ssock));

   // reset
   TEST(0 == RELEASE_PAGECACHE(&buffer[0]));
   TEST(0 == RELEASE_PAGECACHE(&buffer[1]));
   isoldsignalmask = false;
   TEST(0 == sigprocmask(SIG_SETMASK, &oldsignalmask, 0));
   isoldhandler = false;
   TEST(0 == sigaction(SIGUSR1, &oldact, 0));

   return 0;
ONERR:
   if (isoldsignalmask) {
      sigprocmask(SIG_SETMASK, &oldsignalmask, 0);
   }
   if (isoldhandler) {
      sigaction(SIGUSR1, &oldact, 0);
   }
   free_iochannel(&ioc);
   free_iochannel(&pipeioc[0]);
   free_iochannel(&pipeioc[1]);
   free_ipsocket(&ssock);
   free_ipsocket(&csock);
   free_file(&file);
   delete_thread(&thread);
   RELEASE_PAGECACHE(&buffer[0]);
   RELEASE_PAGECACHE(&buffer[1]);
   return EINVAL;
}

typedef struct threadarg_all_t {
   iochannel_t ioc;
   memblock_t  buffer;
   thread_t*   wakeup;
   bool        isSameLogsize;
} threadarg_all_t;

static int thread_dorwall(threadarg_all_t* arg, bool isRead)
{
   size_t logsize1 = 0;
   size_t logsize2 = 1;
   uint8_t* logbuffer;

   GETBUFFER_ERRLOG(&logbuffer, &logsize1);

   if (arg->wakeup) {
      resume_thread(arg->wakeup); // wakeup main thread
   }

   int err = isRead
           ? readall_iochannel(arg->ioc, arg->buffer.size, arg->buffer.addr, -1/*indefinite timeout*/)
           : writeall_iochannel(arg->ioc, arg->buffer.size, arg->buffer.addr, -1/*indefinite timeout*/);

   GETBUFFER_ERRLOG(&logbuffer, &logsize2);

   arg->isSameLogsize = (logsize1 == logsize2);

   CLEARBUFFER_ERRLOG();

   return err;
}

static int thread_readall(threadarg_all_t* arg)
{
   return thread_dorwall(arg, true);
}

static int thread_writeall(threadarg_all_t* arg)
{
   return thread_dorwall(arg, false);
}

static int thread_writeslow(threadarg_all_t* arg)
{
   const size_t blocksize = (arg->buffer.size/64);
   for (unsigned i = 0; i < 64; ++i) {
      sleepms_thread(1);
      TEST(blocksize == (size_t) write(arg->ioc, arg->buffer.addr + i*blocksize, blocksize));
   }

   return 0;
ONERR:
   pthread_kill(arg->wakeup->sys_thread, SIGUSR1);
   return EINVAL;
}

static int open_channel(int type/*0..2*/, directory_t* tempdir, size_t buffersize, /*out*/iochannel_t* rio, /*out*/iochannel_t* wio)
{
   ipsocket_t  ssock = ipsocket_FREE;
   ipsocket_t  csock = ipsocket_FREE;

   switch (type) {
   case 0: // open file
      if (0 == trypath_directory(tempdir, "rdwralltest")) {
         TEST(0 == removefile_directory(tempdir, "rdwralltest"));
      }
      TEST(0 == initcreate_file(wio, "rdwralltest", tempdir));
      TEST(0 == free_file(wio));
      TEST(0 == init_file(wio, "rdwralltest", accessmode_WRITE, tempdir));
      TEST(0 == init_file(rio, "rdwralltest", accessmode_READ, tempdir));
      break;
   case 1:
      {
         int fd[2];
         TEST(0 == pipe2(fd, O_NONBLOCK|O_CLOEXEC));
         *rio = fd[0];
         *wio = fd[1];
      }
      break;
   case 2:
      {
         ipaddr_storage_t ipaddr;
         ipaddr_storage_t ipaddr2;
         ipsocket_t lsock;
         TEST(0 != initany_ipaddrstorage(&ipaddr, ipprotocol_TCP, ipport_ANY, ipversion_4));
         TEST(0 != initany_ipaddrstorage(&ipaddr2, ipprotocol_TCP, ipport_ANY, ipversion_4));
         TEST(0 == initlisten_ipsocket(&lsock, (ipaddr_t*)&ipaddr, 1));
         *wio = io_ipsocket(&lsock);
         TEST(0 == localaddr_ipsocket(&lsock, (ipaddr_t*)&ipaddr));
         TEST(0 == initconnect_ipsocket(&csock, (const ipaddr_t*)&ipaddr, (const ipaddr_t*)&ipaddr2));
         *rio = io_ipsocket(&csock);
         TEST(0 == initaccept_ipsocket(&ssock, &lsock, 0));
         *wio = io_ipsocket(&ssock);
         TEST(0 == free_ipsocket(&lsock));
         TEST(0 == setqueuesize_ipsocket(&ssock, 2*buffersize, 2*buffersize));
         TEST(0 == setqueuesize_ipsocket(&csock, 2*buffersize, 2*buffersize));
      }
      break;
   }

   return 0;
ONERR:
   free_iochannel(rio);
   free_iochannel(wio);
   return EINVAL;
}

static int test_rdwrall(directory_t* tempdir)
{
   thread_t *  thread    = 0;
   threadarg_all_t threadarg;
   systimer_t  timer   = systimer_FREE;
   iochannel_t rio     = iochannel_FREE;
   iochannel_t wio     = iochannel_FREE;
   memblock_t  rbuffer = memblock_FREE;
   memblock_t  wbuffer = memblock_FREE;
   bool        isoldsignalmask = false;
   sigset_t    oldsignalmask;
   sigset_t    signalmask;
   bool        isoldhandler = false;
   struct
   sigaction   newact;
   struct
   sigaction   oldact;
   size_t      buffersize;
   uint64_t    expcount;
   uint8_t   * logbuffer;
   size_t      logsize1;
   size_t      logsize2;

   // prepare
   buffersize = determine_buffer_size();
   TEST(0 < buffersize && buffersize < 1024*1204);
   TEST(0 == init_systimer(&timer, sysclock_MONOTONIC));
   TEST(0 == ALLOC_PAGECACHE(pagesize_1MB, &rbuffer));
   TEST(0 == ALLOC_PAGECACHE(pagesize_1MB, &wbuffer));
   TEST(0 == sigemptyset(&signalmask));
   TEST(0 == sigaddset(&signalmask, SIGUSR1));
   TEST(0 == sigprocmask(SIG_UNBLOCK, &signalmask, &oldsignalmask));
   isoldsignalmask = true;
   sigemptyset(&newact.sa_mask);
   newact.sa_flags   = 0;
   newact.sa_handler = &sigusr1_handler;
   TEST(0 == sigaction(SIGUSR1, &newact, &oldact));
   isoldhandler = true;

   // fill buffer
   for (uint32_t i = 0; i < wbuffer.size/sizeof(uint32_t); ++i) {
      ((uint32_t*)wbuffer.addr)[i] = i;
   }

   // TEST readall_iochannel: EBADF
   TEST(EBADF == readall_iochannel(sys_iochannel_FREE, 1, rbuffer.addr, 0));

   // TEST writeall_iochannel: EBADF
   TEST(EBADF == writeall_iochannel(sys_iochannel_FREE, 1, wbuffer.addr, 0));

   for (int tc = 0; tc < 3; ++tc) {
      TEST(0 == open_channel(tc, tempdir, buffersize, &rio, &wio));

      // TEST readall_iochannel: single block
      TEST(buffersize == (size_t) write(wio, wbuffer.addr, buffersize));
      memset(rbuffer.addr, 0, buffersize);
      TEST(0 == readall_iochannel(rio, buffersize, rbuffer.addr, 0));
      // check rbuffer
      for (uint32_t i = 0; i < buffersize/sizeof(uint32_t); ++i) {
         TEST(((const uint32_t*)rbuffer.addr)[i] == i);
      }
      // check everything read
      TEST((tc==0?0:-1) == read(rio, rbuffer.addr, 1));
      TEST(EAGAIN == errno || tc == 0);

      // TEST writeall_iochannel: single block
      TEST(0 == writeall_iochannel(wio, buffersize, wbuffer.addr, 0));
      // check written content
      memset(rbuffer.addr, 0, buffersize);
      TEST(buffersize == (size_t) read(rio, rbuffer.addr, buffersize));
      for (uint32_t i = 0; i < buffersize/sizeof(uint32_t); ++i) {
         TEST(((const uint32_t*)rbuffer.addr)[i] == i);
      }
      // check no more bytes written than buffersize
      TEST((tc==0?0:-1) == read(rio, rbuffer.addr, 1));
      TEST(EAGAIN == errno || tc == 0);

      // TEST readall_iochannel: multiple blocks
      if (tc != 0/*no file*/) {  // file I/O can not wait with poll
         // prepare
         threadarg = (threadarg_all_t) { wio, wbuffer, self_thread(), 1 };
         TEST(0 == newgeneric_thread(&thread, &thread_writeslow, &threadarg));
         // test
         memset(rbuffer.addr, 0, rbuffer.size);
         TEST(0 == readall_iochannel(rio, rbuffer.size, rbuffer.addr, -1));
         // check thread
         TEST(0 == join_thread(thread));
         TEST(0 == returncode_thread(thread));
         // check rbuffer
         for (uint32_t i = 0; i < rbuffer.size/sizeof(uint32_t); ++i) {
            TEST(((const uint32_t*)rbuffer.addr)[i] == i);
         }
         // check everything read
         TEST((tc==0?0:-1) == read(rio, rbuffer.addr, 1));
         TEST(EAGAIN == errno || tc == 0);
         // reset
         TEST(0 == delete_thread(&thread));
      }

      // TEST writeall_iochannel: multiple blocks
      // prepare
      threadarg = (threadarg_all_t) { wio, wbuffer, self_thread(), 0 };
      TEST(0 == newgeneric_thread(&thread, &thread_writeall, &threadarg));
      // test (simulate slow reader)
      suspend_thread();
      memset(rbuffer.addr, 0, rbuffer.size);
      for (unsigned i = 0; i < 64; ++i) {
         sleepms_thread(1);
         const size_t blocksize = (rbuffer.size/64);
         TEST(blocksize == (size_t) read(rio, rbuffer.addr + i*blocksize, blocksize));
      }
      // check thread
      TEST(0 == join_thread(thread));
      TEST(0 == returncode_thread(thread));
      // check rbuffer
      for (uint32_t i = 0; i < rbuffer.size/sizeof(uint32_t); ++i) {
         TEST(((const uint32_t*)rbuffer.addr)[i] == i);
      }
      // check everything read
      TEST((tc==0?0:-1) == read(rio, rbuffer.addr, 1));
      TEST(EAGAIN == errno || tc == 0);
      // reset
      TEST(0 == delete_thread(&thread));

      // TEST readall_iochannel: EAGAIN
      if (tc != 0) {
         TEST(EAGAIN == readall_iochannel(rio, 1, rbuffer.addr, 0));
      }

      // TEST writeall_iochannel: EAGAIN
      if (tc != 0) {
         while (0 < write(wio, wbuffer.addr, wbuffer.size)) { }
         TEST(EAGAIN == writeall_iochannel(wio, 1, wbuffer.addr, 0));
         // reset
         while (0 < read(wio, rbuffer.addr, rbuffer.size)) { }
      }

      // TEST readall_iochannel: EBADF (not open for writing)
      if (tc != 2) {
         TEST(EBADF == readall_iochannel(wio, 1, rbuffer.addr, 0));
      }

      // TEST writeall_iochannel: EBADF (not open for writing)
      if (tc != 2) {
         TEST(EBADF == writeall_iochannel(rio, 1, wbuffer.addr, 0));
      }

      // TEST readall_iochannel: EINTR
      if (tc != 0) {
         // prepare
         threadarg = (threadarg_all_t) { rio, rbuffer, self_thread(), 1 };
         TEST(0 == newgeneric_thread(&thread, &thread_readall, &threadarg));
         // test (generate interrupt)
         suspend_thread();
         for (;;) {
            sleepms_thread(1);
            pthread_kill(thread->sys_thread, SIGUSR1);
            sleepms_thread(1);
            if (0 == tryjoin_thread(thread)) break;
         }
         // check thread
         TEST(EINTR == returncode_thread(thread));
         TEST(0 == threadarg.isSameLogsize);
         // reset
         while (0 == trywait_signalrt(0, 0)/*SIGUSR1 processed*/) { }
         TEST(0 == delete_thread(&thread));
      }

      // TEST writeall_iochannel: EINTR
      if (tc != 0) {
         // prepare
         while (0 < write(wio, wbuffer.addr, wbuffer.size)) { }
         threadarg = (threadarg_all_t) { wio, wbuffer, self_thread(), 1 };
         TEST(0 == newgeneric_thread(&thread, &thread_writeall, &threadarg));
         // test (generate interrupt)
         suspend_thread();
         for (;;) {
            sleepms_thread(1);
            pthread_kill(thread->sys_thread, SIGUSR1);
            sleepms_thread(1);
            if (0 == tryjoin_thread(thread)) break;
         }
         // check thread
         TEST(EINTR == returncode_thread(thread));
         TEST(0 == threadarg.isSameLogsize);
         // reset
         while (0 < read(rio, rbuffer.addr, rbuffer.size));
         while (0 == trywait_signalrt(0, 0)/*SIGUSR1 processed*/) { }
         TEST(0 == delete_thread(&thread));
      }

      // prepare
      GETBUFFER_ERRLOG(&logbuffer, &logsize1);

      // TEST readall_iochannel: EPIPE
      while (0 < read(rio, rbuffer.addr, rbuffer.size)) { }
      if (tc == 0) {
         // End Of File == EPIPE
         TEST(EPIPE == readall_iochannel(rio, 1, rbuffer.addr, 0));
      } else {
         TEST(0 == free_iochannel(&wio));
         TEST(EPIPE == readall_iochannel(rio, 1, rbuffer.addr, 0));
         // reset
         TEST(0 == free_iochannel(&rio));
         TEST(0 == open_channel(tc, tempdir, buffersize, &rio, &wio));
      }

      // TEST writeall_iochannel: EPIPE
      if (tc != 0) {
         TEST(0 == free_iochannel(&rio));
         while (0 < write(wio, wbuffer.addr, wbuffer.size)) { } // fill buffer
         TEST(EPIPE == writeall_iochannel(wio, 1, wbuffer.addr, 0));
         // reset
         TEST(0 == free_iochannel(&wio));
         TEST(0 == open_channel(tc, tempdir, buffersize, &rio, &wio));
      }

      // check no LOG for EPIPE
      GETBUFFER_ERRLOG(&logbuffer, &logsize2);
      TEST(logsize1 == logsize2);

      // TEST readall_iochannel: ETIME
      if (tc != 0) {
         TEST(0 == startinterval_systimer(timer, &(timevalue_t){ .nanosec = 10000 }));
         TEST(ETIME == readall_iochannel(rio, 1, rbuffer.addr, 4));
         TEST(0 == expirationcount_systimer(timer, &expcount));
         TEST(350 <= expcount);
         TEST(650 >= expcount);
      }

      // TEST writeall_iochannel: ETIME
      if (tc != 0) {
         while (0 < write(wio, wbuffer.addr, wbuffer.size)) { } // fill buffer
         TEST(0 == startinterval_systimer(timer, &(timevalue_t){ .nanosec = 10000 }));
         TEST(ETIME == writeall_iochannel(wio, 1, wbuffer.addr, 4));
         TEST(0 == expirationcount_systimer(timer, &expcount));
         TEST(350 <= expcount);
         TEST(650 >= expcount);
      }

      // reset
      TEST(0 == free_iochannel(&wio));
      TEST(0 == free_iochannel(&rio));
   }

   // reset
   TEST(0 == free_systimer(&timer));
   TEST(0 == removefile_directory(tempdir, "rdwralltest"));
   TEST(0 == RELEASE_PAGECACHE(&rbuffer));
   TEST(0 == RELEASE_PAGECACHE(&wbuffer));
   isoldsignalmask = false;
   TEST(0 == sigprocmask(SIG_SETMASK, &oldsignalmask, 0));
   isoldhandler = false;
   TEST(0 == sigaction(SIGUSR1, &oldact, 0));

   return 0;
ONERR:
   if (isoldsignalmask) {
      sigprocmask(SIG_SETMASK, &oldsignalmask, 0);
   }
   if (isoldhandler) {
      sigaction(SIGUSR1, &oldact, 0);
   }
   free_systimer(&timer);
   free_iochannel(&wio);
   free_iochannel(&rio);
   delete_thread(&thread);
   RELEASE_PAGECACHE(&rbuffer);
   RELEASE_PAGECACHE(&wbuffer);
   return EINVAL;
}

int unittest_io_iochannel()
{
   uint8_t      tmppath[256];
   directory_t* tempdir = 0;

   TEST(0 == newtemp_directory(&tempdir, "iochanneltest"));
   TEST(0 == path_directory(tempdir, &(wbuffer_t)wbuffer_INIT_STATIC(sizeof(tmppath), tmppath)));

   if (test_nropen())            goto ONERR;
   if (test_initfree())          goto ONERR;
   if (test_query(tempdir))      goto ONERR;
   if (test_readwrite(tempdir))  goto ONERR;
   if (test_rdwrall(tempdir))    goto ONERR;

   TEST(0 == removedirectory_directory(0, (const char*)tmppath));
   TEST(0 == delete_directory(&tempdir));

   return 0;
ONERR:
   (void) delete_directory(&tempdir);
   return EINVAL;
}

#endif
