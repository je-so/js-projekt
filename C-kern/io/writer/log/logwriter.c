/* title: LogWriter impl
   Implements logging of error messages to standard error channel.
   See <LogWriter>.

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

   file: C-kern/api/io/writer/log/logwriter.h
    Header file of <LogWriter>.

   file: C-kern/io/writer/log/logwriter.c
    Implementation file <LogWriter impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/io/writer/log/logwriter.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/io/writer/log/logbuffer.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/pagecache_macros.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/io/accessmode.h"
#include "C-kern/api/io/filesystem/directory.h"
#include "C-kern/api/io/filesystem/mmfile.h"
#include "C-kern/api/platform/task/thread.h"
#include "C-kern/api/string/cstring.h"
#endif


/* struct: logwriter_chan_t
 * Extends <logbuffer_t> with isappend mode and <log_state_e>.
 * If isappend is true the next write to the buffer will be appended
 * even if the buffer is full. */
struct logwriter_chan_t {
   logbuffer_t logbuf ;
   const char* funcname ;
   log_state_e logstate ;
   bool        isappend ;
} ;

// group: lifetime

/* define: logwriter_chan_INIT
 * Static initializer.
 *
 * Parameter:
 * size - Size of a temporary or static buffer.
 * addr - Start address of the buffer.
 * io   - <iochannel_t> the buffer is written to. */
#define logwriter_chan_INIT(size, addr, io, logstate)  \
         { logbuffer_INIT(size, addr, io), 0, logstate, false }

// group: update

/* function: flush_logwriterchan
 * Flushes <logbuffer_t> in chan. */
static void flush_logwriterchan(logwriter_chan_t * chan)
{
   // TODO: what to do in case of error during write ?
   write_logbuffer(&chan->logbuf);
   truncate_logbuffer(&chan->logbuf, 0);
}


// section: logwriter_t

// group: types

/* typedef: logwriter_it
 * Defines interface for <logwriter_t> - see <log_it_DECLARE>. */
log_it_DECLARE(logwriter_it, logwriter_t)

// group: static variables

/* variable: s_logwriter_interface
 * Contains single instance of interface <logwriter_it>. */
static logwriter_it  s_logwriter_interface = {
                        &printf_logwriter,
                        &printtext_logwriter,
                        &flushbuffer_logwriter,
                        &truncatebuffer_logwriter,
                        &getbuffer_logwriter,
                        &getstate_logwriter,
                        &compare_logwriter,
                        &setstate_logwriter
                     };

// group: initthread

struct log_it * interface_logwriter(void)
{
   return genericcast_logit(&s_logwriter_interface, logwriter_t) ;
}

// group: helper

/* function: allocatebuffer_logwriter
 * Reserves some memory pages for internal buffer. */
static int allocatebuffer_logwriter(/*out*/memblock_t * buffer)
{
   static_assert(16384 <= INT_MAX, "size_t of buffer will be used in vnsprintf and returned as int") ;
   static_assert(16384 >  log_channel_NROFCHANNEL*(2*log_config_MINSIZE + sizeof(logbuffer_t)), "buffer can hold at least 2 entries") ;
   return ALLOC_PAGECACHE(pagesize_16384, buffer) ;
}

/* function: freebuffer_logwriter
 * Frees internal buffer. */
static int freebuffer_logwriter(memblock_t * buffer)
{
   return RELEASE_PAGECACHE(buffer) ;
}

// group: lifetime

int init_logwriter(/*out*/logwriter_t * lgwrt)
{
   int err ;

   err = allocatebuffer_logwriter(genericcast_memblock(lgwrt, )) ;
   if (err) goto ONABORT ;
   lgwrt->chan = (logwriter_chan_t*) lgwrt->addr ;

   const size_t arraysize = log_channel_NROFCHANNEL * sizeof(logwriter_chan_t) ;
   const size_t othersize = 2*log_config_MINSIZE ;
   const size_t errsize   = lgwrt->size - arraysize - (log_channel_NROFCHANNEL-1) * othersize ;

   size_t offset = arraysize ;
   for (uint8_t channel = 0; channel < log_channel_NROFCHANNEL; ++channel) {
      size_t      logsize  = (channel == log_channel_ERR     ? errsize           : othersize) ;
      log_state_e logstate = (channel == log_channel_USERERR ? log_state_IGNORED : log_state_BUFFERED) ;
      lgwrt->chan[channel] = (logwriter_chan_t) logwriter_chan_INIT(logsize, lgwrt->addr + offset, iochannel_STDERR, logstate) ;
      offset += logsize ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int free_logwriter(logwriter_t * lgwrt)
{
   int err ;

   err = freebuffer_logwriter(genericcast_memblock(lgwrt,)) ;

   lgwrt->chan = 0 ;

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_ERRLOG(err) ;
   return err ;
}

// group: query

void getbuffer_logwriter(const logwriter_t * lgwrt, uint8_t channel, /*out*/uint8_t ** buffer, /*out*/size_t * size)
{
   int err ;

   VALIDATE_INPARAM_TEST(channel < log_channel_NROFCHANNEL, ONABORT,) ;

   logwriter_chan_t * chan = &lgwrt->chan[channel] ;

   getbuffer_logbuffer(&chan->logbuf, buffer, size) ;

   return ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return ;
}

uint8_t getstate_logwriter(const logwriter_t * lgwrt, uint8_t channel)
{
   if (channel < log_channel_NROFCHANNEL) {
      logwriter_chan_t * chan = &lgwrt->chan[channel] ;
      return chan->logstate ;
   }

   return log_state_IGNORED ;
}

int compare_logwriter(const logwriter_t * lgwrt, uint8_t channel, size_t logsize, const uint8_t logbuffer[logsize])
{
   int err ;

   VALIDATE_INPARAM_TEST(channel < log_channel_NROFCHANNEL, ONABORT,) ;

   logwriter_chan_t * chan = &lgwrt->chan[channel] ;

   return compare_logbuffer(&chan->logbuf, logsize, logbuffer) ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err;
}

// group: config

void setstate_logwriter(logwriter_t * lgwrt, uint8_t channel, uint8_t logstate)
{
   if (channel < log_channel_NROFCHANNEL) {
      logwriter_chan_t * chan = &lgwrt->chan[channel] ;
      chan->funcname = 0 ;
      chan->logstate = logstate ;
      chan->isappend = false ;
   }
}

// group: change

void truncatebuffer_logwriter(logwriter_t * lgwrt, uint8_t channel, size_t size)
{
   int err ;

   VALIDATE_INPARAM_TEST(channel < log_channel_NROFCHANNEL, ONABORT,) ;

   logwriter_chan_t * chan = &lgwrt->chan[channel] ;

   truncate_logbuffer(&chan->logbuf, size);

   return;
ONABORT:
   TRACEABORT_ERRLOG(err);
   return;
}

void flushbuffer_logwriter(logwriter_t * lgwrt, uint8_t channel)
{
   int err ;

   VALIDATE_INPARAM_TEST(channel < log_channel_NROFCHANNEL, ONABORT,) ;

   logwriter_chan_t * chan = &lgwrt->chan[channel] ;

   flush_logwriterchan(chan) ;

   return ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return ;
}

#define GETCHANNEL_logwriter(          \
            /*logwriter_t* */lgwrt,    \
            /*log_channel_e*/channel,  \
            /*out*/chan)               \
         VALIDATE_INPARAM_TEST(channel < log_channel_NROFCHANNEL, ONABORT,) ;       \
         chan = &lgwrt->chan[channel] ;                                             \
         if (chan->logstate == log_state_IGNORED) return ;

static inline void beginwrite_logwriter(
   logwriter_chan_t *   chan,
   uint8_t              flags,
   const log_header_t * header)
{
   if (  sizefree_logbuffer(&chan->logbuf) <= log_config_MINSIZE
         && (  !chan->isappend
               || 0 != (flags&log_flags_START)/*implicit END*/)) {
      flush_logwriterchan(chan) ;
   }
   if (  header
         && (  0 == (flags&log_flags_OPTIONALHEADER)
               || (chan->funcname != header->funcname))) {
      printheader_logbuffer(&chan->logbuf, header) ;
      chan->funcname = header->funcname ;
   }
   if (0 != (flags&log_flags_END)) {
      chan->funcname = 0 ;
      chan->isappend = false ;
   } else if ( 0 != (flags&log_flags_START)
               && chan->logstate != log_state_IMMEDIATE) {
      chan->isappend = true ;
   }
}

static inline void endwrite_logwriter(logwriter_chan_t * chan)
{
   if (  chan->logstate != log_state_BUFFERED
         && ! chan->isappend) {
      // flush immediately
      flush_logwriterchan(chan) ;
   }
}

void vprintf_logwriter(logwriter_t * lgwrt, uint8_t channel, uint8_t flags, const struct log_header_t * header, const char * format, va_list args)
{
   int err ;
   logwriter_chan_t * chan ;

   GETCHANNEL_logwriter(lgwrt, channel, /*out*/chan) ;
   beginwrite_logwriter(chan, flags, header) ;

   vprintf_logbuffer(&chan->logbuf, format, args) ;

   endwrite_logwriter(chan) ;

   return ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return ;
}

void printf_logwriter(logwriter_t * lgwrt, uint8_t channel, uint8_t flags, const struct log_header_t * header, const char * format, ...)
{
   va_list args ;
   va_start(args, format) ;
   vprintf_logwriter(lgwrt, channel, flags, header, format, args) ;
   va_end(args) ;
}

void printtext_logwriter(logwriter_t * lgwrt, uint8_t channel, uint8_t flags, const struct log_header_t * header, log_text_f textf, ...)
{
   int err ;
   logwriter_chan_t * chan ;

   GETCHANNEL_logwriter(lgwrt, channel, /*out*/chan) ;
   beginwrite_logwriter(chan, flags, header) ;

   if (textf) {
      va_list args ;
      va_start(args, textf) ;
      textf(&chan->logbuf, args) ;
      va_end(args) ;
   }

   endwrite_logwriter(chan) ;

   return ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return ;
}

// group: test

#ifdef KONFIG_UNITTEST

static bool isfree_logwriter(logwriter_t * lgwrt)
{
   return   0 == lgwrt->addr
            && 0 == lgwrt->size
            && 0 == lgwrt->chan ;
}

static int test_initfree(void)
{
   logwriter_t lgwrt = logwriter_FREE ;

   // TEST logwriter_FREE
   TEST(1 == isfree_logwriter(&lgwrt)) ;

   // TEST init_logwriter
   TEST(0 == init_logwriter(&lgwrt)) ;
   TEST(lgwrt.addr != 0) ;
   TEST(lgwrt.size == 16384) ;
   TEST(lgwrt.chan == (logwriter_chan_t*)lgwrt.addr) ;
   for (unsigned i = 0, offset = log_channel_NROFCHANNEL*sizeof(logwriter_chan_t); i < log_channel_NROFCHANNEL; ++i) {
      TEST(lgwrt.chan[i].logbuf.addr == lgwrt.addr + offset) ;
      TEST(lgwrt.chan[i].logbuf.size == (i == log_channel_ERR ? 16384 - log_channel_NROFCHANNEL*sizeof(logwriter_chan_t)-(log_channel_NROFCHANNEL-1)*2*log_config_MINSIZE : 2*log_config_MINSIZE)) ;
      TEST(lgwrt.chan[i].logbuf.io   == iochannel_STDERR) ;
      TEST(lgwrt.chan[i].logbuf.logsize == 0) ;
      TEST(lgwrt.chan[i].logstate    == (i ? log_state_BUFFERED : log_state_IGNORED)) ;
      TEST(lgwrt.chan[i].isappend    == 0) ;
      offset += lgwrt.chan[i].logbuf.size ;
   }

   // TEST free_logwriter
   TEST(0 == free_logwriter(&lgwrt)) ;
   TEST(1 == isfree_logwriter(&lgwrt)) ;
   TEST(0 == free_logwriter(&lgwrt)) ;
   TEST(1 == isfree_logwriter(&lgwrt)) ;
   TEST(1 == isvalid_iochannel(iochannel_STDOUT)) ;
   TEST(1 == isvalid_iochannel(iochannel_STDERR)) ;

   return 0 ;
ONABORT:
   free_logwriter(&lgwrt) ;
   return EINVAL ;
}

static int test_query(void)
{
   logwriter_t lgwrt = logwriter_FREE ;

   // prepare
   TEST(0 == init_logwriter(&lgwrt)) ;

   // TEST getbuffer_logwriter
   for (uint8_t i = 0; i < log_channel_NROFCHANNEL; ++i) {
      lgwrt.chan[i].logbuf.logsize  = 0 ;
      printf_logbuffer(&lgwrt.chan[i].logbuf, "12345") ;
      uint8_t *logbuffer = 0 ;
      size_t   logsize   = 0 ;
      getbuffer_logwriter(&lgwrt, i, &logbuffer, &logsize) ;
      TEST(logbuffer == lgwrt.chan[i].logbuf.addr) ;
      TEST(logsize   == 5) ;
      printf_logbuffer(&lgwrt.chan[i].logbuf, "%s", "abcdef") ;
      getbuffer_logwriter(&lgwrt, i, &logbuffer, &logsize) ;
      TEST(logbuffer == lgwrt.chan[i].logbuf.addr) ;
      TEST(logsize   == 11) ;
      TEST(0 == strcmp((char*)logbuffer, "12345abcdef")) ;
   }

   // TEST getstate_logwriter
   for (uint8_t s = 0; s < log_state_NROFSTATE; ++s) {
      for (uint8_t i = 0; i < log_channel_NROFCHANNEL; ++i) {
         lgwrt.chan[i].logstate = (uint8_t) (s + 1) ;
      }
      for (uint8_t i = 0; i < log_channel_NROFCHANNEL; ++i) {
         TEST(s+1u == getstate_logwriter(&lgwrt, i)) ;
         lgwrt.chan[i].logstate = s ;
         TEST(s    == getstate_logwriter(&lgwrt, i)) ;
      }
   }

   // TEST getstate_logwriter: channelnr out of range
   TEST(log_state_IGNORED == getstate_logwriter(&lgwrt, log_channel_NROFCHANNEL)) ;
   TEST(log_state_IGNORED == getstate_logwriter(&lgwrt, (uint8_t)-1)) ;

   // TEST compare_logwriter
   for (uint8_t i = 0; i < log_channel_NROFCHANNEL; ++i) {
      truncate_logbuffer(&lgwrt.chan[i].logbuf, 0);
      printf_logbuffer(&lgwrt.chan[i].logbuf, "[1: XXX]\ntest\n");
      TEST(0 == compare_logwriter(&lgwrt, i, 14, (const uint8_t*)"[1: XXX]\ntest\n"));
      TEST(0 == compare_logwriter(&lgwrt, i, 14, (const uint8_t*)"[1: YYY]\ntest\n"));
      TEST(EINVAL == compare_logwriter(&lgwrt, i, 13, (const uint8_t*)"[1: XXX]\ntest\n"));
      TEST(EINVAL == compare_logwriter(&lgwrt, i, 14, (const uint8_t*)"[1: XXX]\ntesT\n"));
   }

   // TEST compare_logwriter: channelnr out of range
   TEST(EINVAL == compare_logwriter(&lgwrt, log_channel_NROFCHANNEL, 14, (const uint8_t*)"[1: XXX]\ntest\n"));
   TEST(EINVAL == compare_logwriter(&lgwrt, (uint8_t)-1, 14, (const uint8_t*)"[1: XXX]\ntest\n"));

   // unprepare
   TEST(0 == free_logwriter(&lgwrt));

   return 0;
ONABORT:
   free_logwriter(&lgwrt);
   return EINVAL;
}

static int test_config(void)
{
   logwriter_t lgwrt = logwriter_FREE ;

   // prepare
   TEST(0 == init_logwriter(&lgwrt)) ;

   // TEST setstate_logwriter
   for (uint8_t s = 0; s < log_state_NROFSTATE; ++s) {
      for (uint8_t i = 0; i < log_channel_NROFCHANNEL; ++i) {
         lgwrt.chan[i].logstate = (uint8_t) (s + 1) ;
      }
      for (uint8_t i = 0; i < log_channel_NROFCHANNEL; ++i) {
         TEST(lgwrt.chan[i].logstate == s+1u) ;
         lgwrt.chan[i].funcname = (void*)1 ;
         lgwrt.chan[i].isappend = true ;
         setstate_logwriter(&lgwrt, i, s) ;
         // remembered state cleared
         TEST(lgwrt.chan[i].funcname == 0) ;
         TEST(lgwrt.chan[i].logstate == s) ;
         TEST(lgwrt.chan[i].isappend == false) ;
      }
   }

   // TEST setstate_logwriter: channelnr out of range
   setstate_logwriter(&lgwrt, log_channel_NROFCHANNEL, 0) ;
   setstate_logwriter(&lgwrt, (uint8_t)-1, 0) ;
   // check that call was ignored
   for (uint8_t i = 0; i < log_channel_NROFCHANNEL; ++i) {
      TEST(log_state_NROFSTATE-1 == lgwrt.chan[i].logstate) ;
   }

   // unprepare
   TEST(0 == free_logwriter(&lgwrt)) ;

   return 0 ;
ONABORT:
   free_logwriter(&lgwrt) ;
   return EINVAL ;
}

static int compare_header(size_t buffer_size, uint8_t buffer_addr[buffer_size], const char * funcname, const char * filename, int linenr, int err)
{
   char     buffer[100] ;
   int      nr1 ;
   uint64_t nr2 ;
   uint32_t nr3 ;
   TEST(3 == sscanf((char*)buffer_addr, "[%d: %"SCNu64".%"SCNu32, &nr1, &nr2, &nr3)) ;
   TEST((unsigned)nr1 == threadid_maincontext()) ;
   struct timeval tv ;
   TEST(0 == gettimeofday(&tv, 0)) ;
   TEST((uint64_t)tv.tv_sec >= nr2) ;
   TEST((uint64_t)tv.tv_sec <= nr2 + 1) ;
   TEST(nr3 < 1000000) ;

   snprintf(buffer, sizeof(buffer), "[%d: %"PRIu64".%06"PRIu32"s]\n%s() %s:%d\nError %d - %s\n", nr1, nr2, nr3, funcname, filename, linenr, err, (const char*)str_errorcontext(error_maincontext(), err)) ;
   TEST(strlen(buffer) == buffer_size) ;
   TEST(0 == memcmp(buffer, buffer_addr, strlen(buffer))) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static void textres_test(logbuffer_t * logbuf, va_list vargs)
{
   vprintf_logbuffer(logbuf, "%d|%s", vargs) ;
}

static int test_write(void)
{
   logwriter_t    lgwrt      = logwriter_FREE ;
   int            oldstderr  = -1 ;
   int            oldstdout  = -1 ;
   int            pipefd[2][2] = { { -1, -1, }, { -1, -1 } } ;
   memblock_t     mem        = memblock_FREE ;

   // prepare
   TEST(0 == ALLOC_PAGECACHE(pagesize_16384, &mem)) ;
   TEST(0 == pipe2(pipefd[0], O_CLOEXEC|O_NONBLOCK)) ;
   TEST(0 == pipe2(pipefd[1], O_CLOEXEC|O_NONBLOCK)) ;
   oldstdout = dup(STDOUT_FILENO) ;    // STDOUT not used but may be in future => checked that not written to
   TEST(0 < oldstdout) ;
   oldstderr = dup(STDERR_FILENO) ;
   TEST(0 < oldstderr) ;
   TEST(STDOUT_FILENO == dup2(pipefd[0][1], STDOUT_FILENO)) ;
   TEST(STDERR_FILENO == dup2(pipefd[1][1], STDERR_FILENO)) ;

   // TEST truncatebuffer_logwriter
   TEST(0 == init_logwriter(&lgwrt)) ;
   for (uint8_t i = 0; i < log_channel_NROFCHANNEL; ++i) {
      for (unsigned size = 0; size < 32; ++size) {
         lgwrt.chan[i].logbuf.logsize      = size;
         lgwrt.chan[i].logbuf.addr[size]   = 'x';
         lgwrt.chan[i].logbuf.addr[size+1] = 'x';
         // ignored if size >= logsize
         truncatebuffer_logwriter(&lgwrt, i, size+1);
         truncatebuffer_logwriter(&lgwrt, i, size);
         TEST(size == lgwrt.chan[i].logbuf.logsize);
         TEST('x'  == lgwrt.chan[i].logbuf.addr[size]);
         TEST('x'  == lgwrt.chan[i].logbuf.addr[size+1]);
         // executed if size < logsize
         lgwrt.chan[i].logbuf.logsize = 32;
         truncatebuffer_logwriter(&lgwrt, i, size);
         TEST(size == lgwrt.chan[i].logbuf.logsize);
         TEST(0    == lgwrt.chan[i].logbuf.addr[size]);
         TEST('x'  == lgwrt.chan[i].logbuf.addr[size+1]);
      }
   }
   TEST(0 == free_logwriter(&lgwrt)) ;

   // TEST flushbuffer_logwriter
   TEST(0 == init_logwriter(&lgwrt)) ;
   for (uint8_t i = 0; i < log_channel_NROFCHANNEL; ++i) {
      uint32_t S = lgwrt.chan[i].logbuf.size ;
      for (unsigned b = 0; b < S; ++b) {
         lgwrt.chan[i].logbuf.addr[b] = (uint8_t) (1+b+i) ;
      }
      lgwrt.chan[i].logbuf.logsize = S ;
      flushbuffer_logwriter(&lgwrt, i) ;
      TEST(0 == lgwrt.chan[i].logbuf.addr[0]) ;
      TEST(S == lgwrt.chan[i].logbuf.size) ;
      TEST(0 == lgwrt.chan[i].logbuf.logsize) ;
      // compare flushed content
      TEST(S == (unsigned)read(pipefd[1][0], mem.addr, mem.size)) ;
      for (unsigned b = 0; b < S; ++b) {
         TEST(mem.addr[b] == (uint8_t) (1+b+i)) ;
      }
      TEST(-1 == read(pipefd[1][0], mem.addr, mem.size)) ;
   }
   TEST(0 == free_logwriter(&lgwrt)) ;

   // TEST printf_logwriter: append + different states
   TEST(0 == init_logwriter(&lgwrt)) ;
   for (uint8_t i = 0; i < log_channel_NROFCHANNEL; ++i) {
      const uint32_t S = lgwrt.chan[i].logbuf.size ;
      for (uint8_t s = 0; s < log_state_NROFSTATE; ++s) {
         setstate_logwriter(&lgwrt, i, s) ;
         memset(lgwrt.chan[i].logbuf.addr, 1, 10) ;
         for (unsigned n = 0; n < 10; ++n) {
            printf_logwriter(&lgwrt, i, log_flags_NONE, 0, "%d", n) ;
            switch ((log_state_e)s) {
            case log_state_IGNORED:
               TEST(lgwrt.chan[i].logbuf.addr[0] == 1) ; // ignored
               TEST(lgwrt.chan[i].logbuf.size    == S) ;
               TEST(lgwrt.chan[i].logbuf.logsize == 0) ;
               break ;
            case log_state_BUFFERED:
               TEST(lgwrt.chan[i].logbuf.addr[n+1] == 0) ; // set to 0
               TEST(lgwrt.chan[i].logbuf.size      == S) ;
               TEST(lgwrt.chan[i].logbuf.logsize   == n+1) ;
               for (unsigned n2 = 0; n2 <= n; ++n2) {
                  TEST(lgwrt.chan[i].logbuf.addr[n2] == ('0'+n2)) ;
               }
               break ;
            case log_state_UNBUFFERED:
            case log_state_IMMEDIATE:
               TEST(lgwrt.chan[i].logbuf.addr[0] == 0) ; // (flush)
               TEST(lgwrt.chan[i].logbuf.addr[1] == 0) ; // (print)
               TEST(lgwrt.chan[i].logbuf.size    == S) ;
               TEST(lgwrt.chan[i].logbuf.logsize == 0) ;
               TEST(1 == read(pipefd[1][0], mem.addr, mem.size)) ;
               TEST(mem.addr[0] == ('0'+n)) ;
            }
         }
         lgwrt.chan[i].logbuf.logsize = 0 ;
      }
   }
   TEST(0 == free_logwriter(&lgwrt)) ;

   // TEST printf_logwriter: flush before print if buffer size <= log_config_MINSIZE
   TEST(0 == init_logwriter(&lgwrt)) ;
   for (uint8_t i = 0; i < log_channel_NROFCHANNEL; ++i) {
      const uint32_t S = lgwrt.chan[i].logbuf.size ;
      TEST(S > log_config_MINSIZE) ;
      for (uint8_t s = 0; s < log_state_NROFSTATE; ++s) {
         setstate_logwriter(&lgwrt, i, s) ;
         memset(lgwrt.chan[i].logbuf.addr, '0', S) ;
         lgwrt.chan[i].logbuf.logsize = S - log_config_MINSIZE ;
         printf_logwriter(&lgwrt, i, log_flags_NONE, 0, "X") ;
         if (s == log_state_IGNORED) {
            TEST(lgwrt.chan[i].logbuf.size    == S) ;
            TEST(lgwrt.chan[i].logbuf.logsize == S-log_config_MINSIZE) ;
            for (size_t off = 0; off < S; ++off) {
               TEST('0' == lgwrt.chan[i].logbuf.addr[0]) ;
            }
         } else {
            // flushed before
            TEST(lgwrt.chan[i].logbuf.addr[0] == (s == log_state_BUFFERED ? 'X' : 0/*flushed after*/)) ;
            TEST(lgwrt.chan[i].logbuf.addr[1] == 0) ;
            TEST(lgwrt.chan[i].logbuf.size    == S) ;
            TEST(lgwrt.chan[i].logbuf.logsize == (s == log_state_BUFFERED ? 1 : 0)) ;
            unsigned read_size = (unsigned) read(pipefd[1][0], mem.addr, mem.size) ;
            TEST(read_size == S-log_config_MINSIZE+(s != log_state_BUFFERED)) ;
            for (unsigned off = 0; off < S-log_config_MINSIZE; ++off) {
               TEST(mem.addr[off] == '0') ;
            }
            TEST(s == log_state_BUFFERED || 'X' == mem.addr[S-log_config_MINSIZE]) ;
         }
      }
   }
   TEST(0 == free_logwriter(&lgwrt)) ;

   // TEST printf_logwriter: isappend => no flush but truncate is indicated with " ..." at end
   TEST(0 == init_logwriter(&lgwrt)) ;
   for (uint8_t i = 0; i < log_channel_NROFCHANNEL; ++i) {
      uint32_t S = lgwrt.chan[i].logbuf.size ;
      TEST(S > 5) ;
      for (uint8_t s = 0; s < log_state_NROFSTATE; ++s) {
         setstate_logwriter(&lgwrt, i, s) ;
         memset(lgwrt.chan[i].logbuf.addr, '0', S) ;
         lgwrt.chan[i].logbuf.logsize = S - 5 ;
         lgwrt.chan[i].isappend       = true ;
         printf_logwriter(&lgwrt, i, log_flags_NONE, 0, "XXXXX") ;
         TEST(lgwrt.chan[i].logbuf.size    == S) ;
         if (s == log_state_IGNORED) {
            TEST(lgwrt.chan[i].logbuf.logsize == S-5) ;
         } else {
            TEST(lgwrt.chan[i].logbuf.logsize == S-1) ;
            TEST(0 == memcmp(lgwrt.chan[i].logbuf.addr+S-5, " ...", 5)) ;
         }
         for (unsigned off = 0; off < S-5; ++off) {
            TEST(lgwrt.chan[i].logbuf.addr[off] == '0') ;
         }
      }
   }
   TEST(0 == free_logwriter(&lgwrt)) ;

   // TEST printf_logwriter: log_flags_START (turns isappend on, but flushed before if necessary independent of isappend)
   TEST(0 == init_logwriter(&lgwrt)) ;
   for (uint8_t i = 0; i < log_channel_NROFCHANNEL; ++i) {
      for (int isappend = 0; isappend <= 1; ++isappend) {
         uint32_t S = lgwrt.chan[i].logbuf.size ;
         TEST(S > log_config_MINSIZE) ;
         for (uint8_t s = 0; s < log_state_NROFSTATE; ++s) {
            setstate_logwriter(&lgwrt, i, s) ;
            memset(lgwrt.chan[i].logbuf.addr, '1', S) ;
            lgwrt.chan[i].logbuf.logsize = S - log_config_MINSIZE ;
            lgwrt.chan[i].isappend       = isappend ;
            printf_logwriter(&lgwrt, i, log_flags_START, 0, "12345") ;
            TEST(lgwrt.chan[i].logbuf.size    == S) ;
            if (s == log_state_IGNORED) {
               TEST(lgwrt.chan[i].logbuf.logsize == S - log_config_MINSIZE) ;
               TEST(lgwrt.chan[i].isappend       == isappend) ;
            } else {
               // isappend is only set if state != log_state_IMMEDIATE
               TEST(lgwrt.chan[i].isappend       == ((s != log_state_IMMEDIATE) || isappend/*don't changed if already set*/)) ;
               TEST(lgwrt.chan[i].logbuf.logsize == (lgwrt.chan[i].isappend ? 5 : 0)) ;
               if (lgwrt.chan[i].logbuf.logsize) {
                  TEST(0 == memcmp(lgwrt.chan[i].logbuf.addr, "12345", 6)) ;
                  TEST(S-log_config_MINSIZE   == (unsigned)read(pipefd[1][0], mem.addr, mem.size)) ;
               } else {
                  TEST(S-log_config_MINSIZE+5 == (unsigned)read(pipefd[1][0], mem.addr, mem.size)) ;
               }
               for (unsigned off = 0; off < S-log_config_MINSIZE; ++off) {
                  TEST(mem.addr[off] == '1') ;
               }
               for (unsigned off = 0; off < 5-lgwrt.chan[i].logbuf.logsize; ++off) {
                  TEST(mem.addr[off+S-log_config_MINSIZE] == "12345"[off]) ;
               }
            }
         }
      }
   }
   TEST(0 == free_logwriter(&lgwrt)) ;

   // TEST printf_logwriter: log_flags_END (logs message and truncates if necessary and then switches isappend off)
   TEST(0 == init_logwriter(&lgwrt)) ;
   for (uint8_t i = 0; i < log_channel_NROFCHANNEL; ++i) {
      uint32_t S = lgwrt.chan[i].logbuf.size ;
      TEST(S > 10) ;
      for (uint8_t s = 0; s < log_state_NROFSTATE; ++s) {
         setstate_logwriter(&lgwrt, i, s) ;
         memset(lgwrt.chan[i].logbuf.addr, '0', S) ;
         lgwrt.chan[i].logbuf.logsize = S - 10 ;
         lgwrt.chan[i].funcname       = (void*) 1 ;
         lgwrt.chan[i].isappend       = true ;
         printf_logwriter(&lgwrt, i, log_flags_END, 0, "0123456789X") ;
         if (s == log_state_IGNORED) {
            TEST(lgwrt.chan[i].logbuf.logsize == S - 10) ;
            TEST(lgwrt.chan[i].funcname       != 0) ;
            TEST(lgwrt.chan[i].isappend       == true) ;
         } else {
            TEST(lgwrt.chan[i].logbuf.size    == S) ;
            TEST(lgwrt.chan[i].logbuf.logsize == (s != log_state_BUFFERED ? 0 : S-1)) ;
            TEST(lgwrt.chan[i].funcname       == 0/*cleared*/) ;
            TEST(lgwrt.chan[i].isappend       == false) ;
            if (lgwrt.chan[i].logbuf.logsize) { // in buffer
               TEST(S-1 == (unsigned)write(pipefd[1][1], lgwrt.chan[i].logbuf.addr, S-1)) ;
            }
            TEST(S-1 == (unsigned)read(pipefd[1][0], mem.addr, mem.size)) ;
            mem.addr[S-1] = 0 ;
            for (unsigned off = 0; off < S-10; ++off) {
               TEST(mem.addr[off] == '0') ;
            }
            TEST(0 == memcmp(mem.addr+S-10, "01234 ...", 10)) ;
         }
      }
   }
   TEST(0 == free_logwriter(&lgwrt)) ;

   // TEST printf_logwriter: log_flags_START + log_flags_END
   TEST(0 == init_logwriter(&lgwrt)) ;
   for (uint8_t i = 0; i < log_channel_NROFCHANNEL; ++i) {
      const uint32_t S = lgwrt.chan[i].logbuf.size ;
      TEST(S > log_config_MINSIZE) ;
      for (int isappend = 0; isappend <= 1; ++isappend) {
         for (uint8_t s = 0; s < log_state_NROFSTATE; ++s) {
            setstate_logwriter(&lgwrt, i, s) ;
            memset(lgwrt.chan[i].logbuf.addr, '1', S) ;
            lgwrt.chan[i].logbuf.logsize = S - log_config_MINSIZE ;
            lgwrt.chan[i].isappend       = isappend ;
            printf_logwriter(&lgwrt, i, log_flags_START|log_flags_END, 0, "12345") ;
            TEST(lgwrt.chan[i].logbuf.size    == S) ;
            if (s == log_state_IGNORED) {
               TEST(lgwrt.chan[i].logbuf.logsize == S - log_config_MINSIZE) ;
               TEST(lgwrt.chan[i].isappend       == isappend) ;
            } else {
               TEST(lgwrt.chan[i].logbuf.logsize == (s != log_state_BUFFERED ? 0 : 5)) ;
               TEST(lgwrt.chan[i].isappend       == false) ;
               if (lgwrt.chan[i].logbuf.logsize) {
                  TEST(5 == write(pipefd[1][1], "12345", 5)) ;
               }
               TEST(S-log_config_MINSIZE+5 == (unsigned)read(pipefd[1][0], mem.addr, mem.size)) ;
               for (unsigned off = 0; off < S-log_config_MINSIZE; ++off) {
                  TEST(mem.addr[off] == '1') ;
               }
               TEST(0 == memcmp(mem.addr+S-log_config_MINSIZE, "12345", 5)) ;
            }
         }
      }
   }
   TEST(0 == free_logwriter(&lgwrt)) ;

   // TEST printf_logwriter: header
   TEST(0 == init_logwriter(&lgwrt)) ;
   for (uint8_t i = 0; i < log_channel_NROFCHANNEL; ++i) {
      uint32_t S = lgwrt.chan[i].logbuf.size ;
      TEST(S > log_config_MINSIZE) ;
      for (uint8_t s = 0; s < log_state_NROFSTATE; ++s) {
         setstate_logwriter(&lgwrt, i, s) ;
         memset(lgwrt.chan[i].logbuf.addr, '0', S) ;
         memset(mem.addr, 'x', log_config_MINSIZE) ;
         mem.addr[log_config_MINSIZE] = 0 ;
         lgwrt.chan[i].logbuf.logsize = S - log_config_MINSIZE ;
         lgwrt.chan[i].funcname       = 0 ;
         lgwrt.chan[i].isappend       = true ;
         log_header_t header = log_header_INIT("__func__", "__file__", 9945+i, i) ;
         printf_logwriter(&lgwrt, i, log_flags_NONE, &header, "%s", (char*)mem.addr) ;
         TEST(lgwrt.chan[i].logbuf.size    == S) ;
         TEST(lgwrt.chan[i].isappend       == true) ;
         for (unsigned off = 0; off < S-log_config_MINSIZE; ++off) {
            TEST(lgwrt.chan[i].logbuf.addr[off] == '0') ;
         }
         if (s == log_state_IGNORED) {
            TEST(lgwrt.chan[i].logbuf.logsize == S - log_config_MINSIZE) ;
            TEST(lgwrt.chan[i].funcname       == 0) ;
         } else {
            TEST(lgwrt.chan[i].logbuf.logsize == S-1) ;
            TEST(lgwrt.chan[i].funcname       == header.funcname) ;
            char * start = (char*)lgwrt.chan[i].logbuf.addr+S-log_config_MINSIZE ;
            char * eol   = strchr(start, '\n') ? strchr(strchr(start, '\n')+1, '\n') ? strchr(strchr(strchr(start, '\n')+1, '\n')+1, '\n')+1 : 0 : 0 ;
            size_t L     = (size_t) (eol - (char*)lgwrt.chan[i].logbuf.addr) ;
            TEST(0 != eol) ;
            TEST(0 == compare_header((size_t)(eol - start), (uint8_t*)start, "__func__", "__file__", 9945+i, i)) ;
            for (size_t off = L; off < S-5; ++off) {
               TEST(lgwrt.chan[i].logbuf.addr[off] == 'x') ;
            }
            TEST(0 == strcmp((char*)lgwrt.chan[i].logbuf.addr+S-5, " ...")) ;
         }
      }
   }
   TEST(0 == free_logwriter(&lgwrt)) ;

   // TEST printf_logwriter: optional header
   TEST(0 == init_logwriter(&lgwrt)) ;
   for (uint8_t i = 0; i < log_channel_NROFCHANNEL; ++i) {
      uint32_t S = lgwrt.chan[i].logbuf.size ;
      for (uint8_t s = 0; s < log_state_NROFSTATE; ++s) {
         setstate_logwriter(&lgwrt, i, s) ;
         lgwrt.chan[i].logbuf.logsize = 0 ;
         lgwrt.chan[i].funcname       = 0 ;
         lgwrt.chan[i].isappend       = true ;
         log_header_t header = log_header_INIT("__func__", "__file__", i, 1+i) ;
         printf_logwriter(&lgwrt, i, log_flags_OPTIONALHEADER, &header, " ") ;
         if (s == log_state_IGNORED) {
            TEST(lgwrt.chan[i].logbuf.logsize == 0) ;
            TEST(lgwrt.chan[i].funcname       == 0) ;
         } else {
            // header written cause funcname == 0
            TEST(lgwrt.chan[i].logbuf.logsize > 10) ;
            TEST(lgwrt.chan[i].funcname       == header.funcname) ;
            size_t logsize = lgwrt.chan[i].logbuf.logsize ;
            printf_logwriter(&lgwrt, i, log_flags_OPTIONALHEADER, &header, " ") ;
            TEST(lgwrt.chan[i].logbuf.logsize == logsize/*header not written*/ + (logsize != 0)/*single space*/) ;
            TEST(lgwrt.chan[i].logbuf.size    == S) ;
            TEST(lgwrt.chan[i].funcname       == header.funcname) ;
            TEST(lgwrt.chan[i].isappend       == true) ;
         }
      }
   }
   TEST(0 == free_logwriter(&lgwrt)) ;

   // TEST printf_logwriter: EINVAL
   TEST(0 == init_logwriter(&lgwrt)) ;
   printf_logwriter(&lgwrt, log_channel_NROFCHANNEL, log_flags_NONE, 0, "123") ;
   for (uint8_t i = 0; i < log_channel_NROFCHANNEL; ++i) {
      TEST(lgwrt.chan[i].logbuf.logsize == 0) ;
   }
   TEST(0 == free_logwriter(&lgwrt)) ;

   // TEST printtext_logwriter: header + text resource + truncated message
   TEST(0 == init_logwriter(&lgwrt)) ;
   for (uint8_t i = 0; i < log_channel_NROFCHANNEL; ++i) {
      uint32_t S = lgwrt.chan[i].logbuf.size ;
      TEST(S > log_config_MINSIZE) ;
      for (uint8_t s = 0; s < log_state_NROFSTATE; ++s) {
         setstate_logwriter(&lgwrt, i, s) ;
         memset(lgwrt.chan[i].logbuf.addr, '0', S) ;
         memset(mem.addr, 'x', log_config_MINSIZE) ;
         mem.addr[log_config_MINSIZE] = 0 ;
         lgwrt.chan[i].logbuf.logsize = S - log_config_MINSIZE ;
         lgwrt.chan[i].isappend       = true ;
         log_header_t header = log_header_INIT("func", "file", 100+i, 1+i) ;
         printtext_logwriter(&lgwrt, i, log_flags_NONE, &header, textres_test, 3, (char*)mem.addr) ;
         TEST(lgwrt.chan[i].logbuf.size    == S) ;
         TEST(lgwrt.chan[i].isappend       == true) ;
         for (unsigned off = 0; off < S-log_config_MINSIZE; ++off) {
            TEST(lgwrt.chan[i].logbuf.addr[off] == '0') ;
         }
         if (s == log_state_IGNORED) {
            TEST(lgwrt.chan[i].logbuf.logsize == S - log_config_MINSIZE) ;
         } else {
            TEST(lgwrt.chan[i].logbuf.logsize == S-1) ;
            char * start = (char*)lgwrt.chan[i].logbuf.addr+S-log_config_MINSIZE ;
            char * eol   = strchr(start, '\n') ? strchr(strchr(start, '\n')+1, '\n') ? strchr(strchr(strchr(start, '\n')+1, '\n')+1, '\n')+1 : 0 : 0 ;
            size_t L     = (size_t) (eol - (char*)lgwrt.chan[i].logbuf.addr) ;
            TEST(0 != eol) ;
            TEST(0 == compare_header((size_t)(eol - start), (uint8_t*)start, "func", "file", 100+i, 1+i)) ;
            TEST(lgwrt.chan[i].logbuf.addr[L]   == '3') ;
            TEST(lgwrt.chan[i].logbuf.addr[L+1] == '|') ;
            for (size_t off = L+2; off < S-5; ++off) {
               TEST(lgwrt.chan[i].logbuf.addr[off] == 'x') ;
            }
            TEST(0 == strcmp((char*)lgwrt.chan[i].logbuf.addr+S-5, " ...")) ;
         }
      }
   }
   TEST(0 == free_logwriter(&lgwrt)) ;

   // TEST printtext_logwriter: null textresource and optional header
   TEST(0 == init_logwriter(&lgwrt)) ;
   for (uint8_t i = 0; i < log_channel_NROFCHANNEL; ++i) {
      uint32_t S = lgwrt.chan[i].logbuf.size ;
      for (uint8_t s = 0; s < log_state_NROFSTATE; ++s) {
         setstate_logwriter(&lgwrt, i, s) ;
         lgwrt.chan[i].logbuf.logsize = 0 ;
         lgwrt.chan[i].funcname       = 0 ;
         lgwrt.chan[i].isappend       = true ;
         log_header_t header = log_header_INIT("__func__", "__file__", i, 1+i) ;
         printtext_logwriter(&lgwrt, i, log_flags_OPTIONALHEADER, &header, 0/*null textresource ignored (only header written)*/) ;
         if (s == log_state_IGNORED) {
            TEST(lgwrt.chan[i].logbuf.logsize == 0) ;
         } else {
            // header written cause funcname == 0
            TEST(lgwrt.chan[i].logbuf.logsize > 10) ;
            TEST(lgwrt.chan[i].logbuf.size    == S) ;
            TEST(lgwrt.chan[i].funcname       == header.funcname) ;
            TEST(lgwrt.chan[i].isappend       == true) ;
            size_t logsize = lgwrt.chan[i].logbuf.logsize ;
            printtext_logwriter(&lgwrt, i, log_flags_OPTIONALHEADER, &header, 0/*null textresource ignored*/) ;
            TEST(lgwrt.chan[i].logbuf.logsize == logsize/*header not written*/) ;
            TEST(lgwrt.chan[i].logbuf.size    == S) ;
            TEST(lgwrt.chan[i].funcname       == header.funcname) ;
            TEST(lgwrt.chan[i].isappend       == true) ;
         }
      }
   }
   TEST(0 == free_logwriter(&lgwrt)) ;

   // TEST printtext_logwriter: EINVAL
   TEST(0 == init_logwriter(&lgwrt)) ;
   printtext_logwriter(&lgwrt, log_channel_NROFCHANNEL, log_flags_NONE, 0, textres_test) ;
   for (uint8_t i = 0; i < log_channel_NROFCHANNEL; ++i) {
      TEST(lgwrt.chan[i].logbuf.logsize == 0) ;
   }
   TEST(0 == free_logwriter(&lgwrt)) ;

   // unprepare
   TEST(-1 == read(pipefd[0][0], mem.addr, 1)) ;   // all bytes consumed
   TEST(-1 == read(pipefd[1][0], mem.addr, 1)) ;   // all bytes consumed
   TEST(STDOUT_FILENO == dup2(oldstdout, STDOUT_FILENO)) ;
   TEST(STDERR_FILENO == dup2(oldstderr, STDERR_FILENO)) ;
   TEST(0 == free_iochannel(&oldstdout)) ;
   TEST(0 == free_iochannel(&oldstderr)) ;
   for (unsigned i = 0; i < 4; ++i) {
      TEST(0 == free_iochannel(&pipefd[i/2][i%2])) ;
   }
   TEST(0 == free_logwriter(&lgwrt)) ;
   RELEASE_PAGECACHE(&mem) ;

   return 0 ;
ONABORT:
   if (oldstdout > 0) dup2(oldstdout, STDOUT_FILENO) ;
   if (oldstderr > 0) dup2(oldstderr, STDERR_FILENO) ;
   int bytes = 0 ;
   while (0 < (bytes = read(pipefd[0][0], mem.addr, mem.size))) {
      printf("%.*s", bytes > 100 ? 100 : bytes, mem.addr) ;
      if (bytes > 100) break ;
   }
   printf("\n") ;
   free_iochannel(&oldstdout) ;
   free_iochannel(&oldstderr) ;
   for (unsigned i = 0; i < 4; ++i) {
      free_iochannel(&pipefd[i/2][i%2]) ;
   }
   free_logwriter(&lgwrt) ;
   RELEASE_PAGECACHE(&mem) ;
   return EINVAL ;
}

static int test_initthread(void)
{
   // TEST genericcast_logit
   TEST(genericcast_logit(&s_logwriter_interface, logwriter_t) == (log_it*)&s_logwriter_interface) ;

   // TEST s_logwriter_interface
   TEST(s_logwriter_interface.printf         == &printf_logwriter);
   TEST(s_logwriter_interface.printtext      == &printtext_logwriter);
   TEST(s_logwriter_interface.flushbuffer    == &flushbuffer_logwriter);
   TEST(s_logwriter_interface.truncatebuffer == &truncatebuffer_logwriter);
   TEST(s_logwriter_interface.getbuffer      == &getbuffer_logwriter);
   TEST(s_logwriter_interface.getstate       == &getstate_logwriter);
   TEST(s_logwriter_interface.compare        == &compare_logwriter);
   TEST(s_logwriter_interface.setstate       == &setstate_logwriter);

   // TEST interface_logwriter
   TEST(interface_logwriter() == genericcast_logit(&s_logwriter_interface, logwriter_t)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_logmacros(void)
{
   logwriter_t *  lgwrt = (logwriter_t*) log_maincontext().object ;
   int            oldfd = -1 ;
   int            pipefd[2] = { -1, -1 } ;

   // prepare
   TEST(interface_logwriter() == log_maincontext().iimpl) ;
   TEST(0 == pipe2(pipefd, O_CLOEXEC|O_NONBLOCK)) ;
   oldfd = dup(lgwrt->chan[log_channel_ERR].logbuf.io) ;
   TEST(0 < oldfd) ;
   TEST(lgwrt->chan[log_channel_ERR].logbuf.io == dup2(pipefd[1], lgwrt->chan[log_channel_ERR].logbuf.io)) ;

   // TEST GETBUFFER_LOG
   uint8_t *logbuffer = 0 ;
   size_t   logsize   = (size_t)-1 ;
   GETBUFFER_LOG(log_channel_ERR, &logbuffer, &logsize) ;
   TEST(logbuffer == lgwrt->chan[log_channel_ERR].logbuf.addr) ;
   TEST(logsize   == lgwrt->chan[log_channel_ERR].logbuf.logsize) ;

   // TEST COMPARE_LOG
   TEST(0 == COMPARE_LOG(log_channel_ERR, lgwrt->chan[log_channel_ERR].logbuf.logsize, lgwrt->chan[log_channel_ERR].logbuf.addr));
   TEST(EINVAL == COMPARE_LOG(log_channel_ERR, lgwrt->chan[log_channel_ERR].logbuf.logsize+1, lgwrt->chan[log_channel_ERR].logbuf.addr));

   // TEST GETSTATE_LOG
   log_state_e oldstate = lgwrt->chan[log_channel_ERR].logstate ;
   lgwrt->chan[log_channel_ERR].logstate = log_state_IGNORED ;
   TEST(GETSTATE_LOG(log_channel_ERR) == log_state_IGNORED) ;
   lgwrt->chan[log_channel_ERR].logstate = log_state_IMMEDIATE ;
   TEST(GETSTATE_LOG(log_channel_ERR) == log_state_IMMEDIATE) ;
   lgwrt->chan[log_channel_ERR].logstate = oldstate ;
   TEST(GETSTATE_LOG(log_channel_ERR) == oldstate) ;

   // TEST TRUNCATEBUFFER_LOG
   logwriter_chan_t oldchan = lgwrt->chan[log_channel_ERR];
   for (unsigned i = 0; i < 127; ++i) {
      uint8_t buffer[128];
      memset(buffer, 'a', sizeof(buffer));
      lgwrt->chan[log_channel_ERR].logbuf.addr = buffer;
      lgwrt->chan[log_channel_ERR].logbuf.size = sizeof(buffer);
      lgwrt->chan[log_channel_ERR].logbuf.logsize = sizeof(buffer)-1;
      TRUNCATEBUFFER_LOG(log_channel_ERR, i+sizeof(buffer));
      TEST(0 == memchr(buffer, 0, sizeof(buffer)));
      GETBUFFER_LOG(log_channel_ERR, &logbuffer, &logsize) ;
      TEST(buffer == logbuffer);
      TEST(sizeof(buffer)-1 == logsize);
      TRUNCATEBUFFER_LOG(log_channel_ERR, i);
      GETBUFFER_LOG(log_channel_ERR, &logbuffer, &logsize);
      TEST(buffer == logbuffer);
      TEST(i == logsize);
      TEST(0 == buffer[i]);
      lgwrt->chan[log_channel_ERR] = oldchan;
   }

   // TEST FLUSHBUFFER_LOG
   uint8_t oldchr = lgwrt->chan[log_channel_ERR].logbuf.addr[0];
   lgwrt->chan[log_channel_ERR].logbuf.addr[0] = 'X' ;
   lgwrt->chan[log_channel_ERR].logbuf.logsize = 1 ;
   FLUSHBUFFER_LOG(log_channel_ERR) ;
   GETBUFFER_LOG(log_channel_ERR, &logbuffer, &logsize) ;
   TEST(0 == logsize) ;
   lgwrt->chan[log_channel_ERR] = oldchan ;
   lgwrt->chan[log_channel_ERR].logbuf.addr[0] = oldchr ;
   char chars[2] = { 0 } ;
   TEST(1   == read(pipefd[0], chars, 2)) ;
   TEST('X' == chars[0]) ;

   // TEST SETSTATE_LOG
   oldstate = lgwrt->chan[log_channel_ERR].logstate ;
   SETSTATE_LOG(log_channel_ERR, log_state_IGNORED) ;
   TEST(GETSTATE_LOG(log_channel_ERR) == log_state_IGNORED) ;
   SETSTATE_LOG(log_channel_ERR, log_state_IMMEDIATE) ;
   TEST(GETSTATE_LOG(log_channel_ERR) == log_state_IMMEDIATE) ;
   SETSTATE_LOG(log_channel_ERR, oldstate) ;
   TEST(GETSTATE_LOG(log_channel_ERR) == oldstate) ;

   // TEST == group: log-text ==
   // already tested with written log output

   // unprepare
   TEST(lgwrt->chan[log_channel_ERR].logbuf.io == dup2(oldfd, lgwrt->chan[log_channel_ERR].logbuf.io)) ;
   TEST(0 == close(pipefd[0])) ;
   TEST(0 == close(pipefd[1])) ;
   TEST(0 == close(oldfd)) ;

   return 0 ;
ONABORT:
   if (oldfd >= 0) dup2(oldfd, lgwrt->chan[log_channel_ERR].logbuf.io) ;
   close(pipefd[0]) ;
   close(pipefd[1]) ;
   close(oldfd) ;
   return EINVAL ;
}

int unittest_io_writer_log_logwriter()
{
   if (test_initfree())       goto ONABORT ;
   if (test_query())          goto ONABORT ;
   if (test_config())         goto ONABORT ;
   if (test_write())          goto ONABORT ;
   if (test_initthread())     goto ONABORT ;
   if (test_logmacros())      goto ONABORT ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

#endif
