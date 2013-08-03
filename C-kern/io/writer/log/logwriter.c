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
#include "C-kern/api/context/errorcontext.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/io/writer/log/logbuffer.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/pagecache_macros.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/io/accessmode.h"
#include "C-kern/api/io/filesystem/directory.h"
#include "C-kern/api/io/filesystem/mmfile.h"
#include "C-kern/api/platform/task/thread.h"
#include "C-kern/api/string/cstring.h"
#endif


/* struct: logwriter_chan_t
 * Extends <logbuffer_t> with an isappend state.
 * If isappend is true the next write to the buffer will be appended. */
struct logwriter_chan_t {
   logbuffer_t logbuf ;
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
#define logwriter_chan_INIT(size, addr, io)  \
         { logbuffer_INIT(size, addr, io), false }

// group: update

/* function: flush_logwriterchan
 * Flushes <logbuffer_t> in chan. */
static void flush_logwriterchan(logwriter_chan_t * chan)
{
   // TODO: what to do in case of error during write ?
   write_logbuffer(&chan->logbuf) ;
   clear_logbuffer(&chan->logbuf) ;
}


// section: logwriter_t

// group: types

/* typedef: logwriter_it
 * Defines interface for <logwriter_t> - see <log_it_DECLARE>. */
log_it_DECLARE(logwriter_it, logwriter_t)

// group: variables

/* variable: s_logwriter_interface
 * Contains single instance of interface <logwriter_it>. */
logwriter_it      s_logwriter_interface = {
                        &printf_logwriter,
                        &printtext_logwriter,
                        &flushbuffer_logwriter,
                        &clearbuffer_logwriter,
                        &getbuffer_logwriter
                  } ;

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

   static_assert(lengthof(lgwrt->chan) == log_channel_NROFCHANNEL, "every channel is mapped") ;

   err = allocatebuffer_logwriter(genericcast_memblock(lgwrt, )) ;
   if (err) goto ONABORT ;

   // static configuration uses two buffers
   static_assert(0 == log_channel_USERERR, "first is CONSOLE") ;
   lgwrt->chan[0]  = 0 + (logwriter_chan_t*) lgwrt->addr ;  // STDERR + flush is default
   lgwrt->chan[1]  = 1 + (logwriter_chan_t*) lgwrt->addr ;  // STDERR + buffer is default
   for (unsigned i = 2; i < lengthof(lgwrt->chan); ++i) {
      lgwrt->chan[i] = 1 + (logwriter_chan_t*) lgwrt->addr ;
   }

   const size_t logoffset  = 2 * sizeof(logwriter_chan_t) ;
   const size_t logoffset2 = logoffset + 2*log_config_MINSIZE ;
   *lgwrt->chan[0] = (logwriter_chan_t) logwriter_chan_INIT(logoffset2 - logoffset, lgwrt->addr + logoffset, iochannel_STDERR) ;
   *lgwrt->chan[1] = (logwriter_chan_t) logwriter_chan_INIT(lgwrt->size - logoffset2, lgwrt->addr + logoffset2, iochannel_STDERR) ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int free_logwriter(logwriter_t * lgwrt)
{
   int err ;

   err = freebuffer_logwriter(genericcast_memblock(lgwrt,)) ;

   memset(lgwrt->chan, 0, sizeof(lgwrt->chan)) ;

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_ERRLOG(err) ;
   return err ;
}

// group: query

void getbuffer_logwriter(logwriter_t * lgwrt, uint8_t channel, /*out*/char ** buffer, /*out*/size_t * size)
{
   int err ;

   VALIDATE_INPARAM_TEST(channel < lengthof(lgwrt->chan), ONABORT,) ;

   logwriter_chan_t * chan = lgwrt->chan[channel] ;

   getbuffer_logbuffer(&chan->logbuf, (uint8_t**)buffer, size) ;

   return ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return ;
}

// group: change

void clearbuffer_logwriter(logwriter_t * lgwrt, uint8_t channel)
{
   int err ;

   VALIDATE_INPARAM_TEST(channel < lengthof(lgwrt->chan), ONABORT,) ;

   logwriter_chan_t * chan = lgwrt->chan[channel] ;

   clear_logbuffer(&chan->logbuf) ;

   return ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return ;
}

void flushbuffer_logwriter(logwriter_t * lgwrt, uint8_t channel)
{
   int err ;

   VALIDATE_INPARAM_TEST(channel < lengthof(lgwrt->chan), ONABORT,) ;

   logwriter_chan_t * chan = lgwrt->chan[channel] ;

   flush_logwriterchan(chan) ;

   return ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return ;
}

static void beginwrite_logwriter(logwriter_chan_t * chan, uint8_t flags, const struct log_header_t * header)
{
   if (  sizefree_logbuffer(&chan->logbuf) <= log_config_MINSIZE
         && (  !chan->isappend
               || 0 != (flags&log_flags_START)/*implicit END*/)) {
      flush_logwriterchan(chan) ;
   }

   if (0 != (flags&log_flags_END)) {
      chan->isappend = false ;
   } else if (0 != (flags&log_flags_START)) {
      chan->isappend = true ;
   }

   if (header) {
      printheader_logbuffer(&chan->logbuf, header) ;
   }
}

static void endwrite_logwriter(logwriter_chan_t * chan, uint8_t channel)
{
   if (  log_channel_USERERR == channel
         && ! chan->isappend) {
      // flush immediately
      flush_logwriterchan(chan) ;
   }
}

void vprintf_logwriter(logwriter_t * lgwrt, uint8_t channel, uint8_t flags, const struct log_header_t * header, const char * format, va_list args)
{
   int err ;

   VALIDATE_INPARAM_TEST(channel < lengthof(lgwrt->chan), ONABORT,) ;

   logwriter_chan_t * chan = lgwrt->chan[channel] ;

   beginwrite_logwriter(chan, flags, header) ;

   vprintf_logbuffer(&chan->logbuf, format, args) ;

   endwrite_logwriter(chan, channel) ;

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

   VALIDATE_INPARAM_TEST(channel < lengthof(lgwrt->chan), ONABORT,) ;

   logwriter_chan_t * chan = lgwrt->chan[channel] ;

   beginwrite_logwriter(chan, flags, header) ;

   va_list args ;
   va_start(args, textf) ;
   textf(&chan->logbuf, args) ;
   va_end(args) ;

   endwrite_logwriter(chan, channel) ;

   return ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return ;
}

// group: test

#ifdef KONFIG_UNITTEST

static bool isfree_logwriter(logwriter_t * lgwrt)
{
   if (  lgwrt->addr
         || lgwrt->size) {
      return false ;
   }

   for (unsigned i = 0; i < lengthof(lgwrt->chan); ++i) {
      if (lgwrt->chan[i]) return false ;
   }

   return true ;
}


static int test_initfree(void)
{
   logwriter_t lgwrt = logwriter_INIT_FREEABLE ;

   // TEST logwriter_INIT_FREEABLE
   TEST(1 == isfree_logwriter(&lgwrt)) ;

   // TEST init_logwriter
   TEST(0 == init_logwriter(&lgwrt)) ;
   TEST(lgwrt.addr != 0) ;
   TEST(lgwrt.size == 16384) ;
   TEST(lgwrt.chan[0] == (logwriter_chan_t*)lgwrt.addr) ;
   for (unsigned i = 0, offset = 2*sizeof(logwriter_chan_t); i < lengthof(lgwrt.chan); ++i) {
      TEST(lgwrt.chan[i] == (logwriter_chan_t*)(lgwrt.addr + (i!=0)*sizeof(logwriter_chan_t))) ;
      TEST(lgwrt.chan[i]->logbuf.addr == lgwrt.addr + offset) ;
      TEST(lgwrt.chan[i]->logbuf.size == (i ? 16384 - 2*sizeof(logwriter_chan_t)-2*log_config_MINSIZE : 2*log_config_MINSIZE)) ;
      TEST(lgwrt.chan[i]->logbuf.io   == (i ? iochannel_STDERR : iochannel_STDERR)) ;
      TEST(lgwrt.chan[i]->logbuf.logsize == 0) ;
      TEST(lgwrt.chan[i]->isappend    == 0) ;
      if (0 == i) offset += 2*log_config_MINSIZE ;
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
   logwriter_t lgwrt = logwriter_INIT_FREEABLE ;

   // prepare
   TEST(0 == init_logwriter(&lgwrt)) ;

   // TEST getbuffer_logwriter
   for (uint8_t i = 0; i < log_channel_NROFCHANNEL; ++i) {
      lgwrt.chan[i]->logbuf.logsize  = 0 ;
      printf_logbuffer(&lgwrt.chan[i]->logbuf, "12345") ;
      char  *  logstr  = 0 ;
      size_t   logsize = 0 ;
      getbuffer_logwriter(&lgwrt, i, &logstr, &logsize) ;
      TEST(logstr  == (char*)lgwrt.chan[i]->logbuf.addr) ;
      TEST(logsize == 5) ;
      printf_logbuffer(&lgwrt.chan[i]->logbuf, "%s", "abcdef") ;
      getbuffer_logwriter(&lgwrt, i, &logstr, &logsize) ;
      TEST(logstr  == (char*)lgwrt.chan[i]->logbuf.addr) ;
      TEST(logsize == 11) ;
      TEST(0 == strcmp(logstr, "12345abcdef")) ;
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

static int test_update(void)
{
   logwriter_t    lgwrt      = logwriter_INIT_FREEABLE ;
   int            oldstderr  = -1 ;
   int            oldstdout  = -1 ;
   int            pipefd[2][2] = { { -1, -1, }, { -1, -1 } } ;
   memblock_t     mem        = memblock_INIT_FREEABLE ;

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

   // TEST clearbuffer_logwriter
   TEST(0 == init_logwriter(&lgwrt)) ;
   for (uint8_t i = 0; i < log_channel_NROFCHANNEL; ++i) {
      lgwrt.chan[i]->logbuf.logsize = 99 ;
      lgwrt.chan[i]->logbuf.addr[0] = 'x' ;
      clearbuffer_logwriter(&lgwrt, i) ;
      TEST(0 == lgwrt.chan[i]->logbuf.logsize) ;
      TEST(0 == lgwrt.chan[i]->logbuf.addr[0]) ;
   }
   TEST(0 == free_logwriter(&lgwrt)) ;

   // TEST flushbuffer_logwriter
   TEST(0 == init_logwriter(&lgwrt)) ;
   for (uint8_t i = 0; i < log_channel_NROFCHANNEL; ++i) {
      uint32_t S = lgwrt.chan[i]->logbuf.size ;
      for (unsigned b = 0; b < S; ++b) {
         lgwrt.chan[i]->logbuf.addr[b] = (uint8_t) (1+b+i) ;
      }
      lgwrt.chan[i]->logbuf.logsize = S ;
      flushbuffer_logwriter(&lgwrt, i) ;
      TEST(0 == lgwrt.chan[i]->logbuf.addr[0]) ;
      TEST(S == lgwrt.chan[i]->logbuf.size) ;
      TEST(0 == lgwrt.chan[i]->logbuf.logsize) ;
      // compare flushed content
      TEST(S == (unsigned)read(pipefd[1][0], mem.addr, mem.size)) ;
      for (unsigned b = 0; b < S; ++b) {
         TEST(mem.addr[b] == (uint8_t) (1+b+i)) ;
      }
      TEST(-1 == read(pipefd[1][0], mem.addr, mem.size)) ;
   }
   TEST(0 == free_logwriter(&lgwrt)) ;

   // TEST printf_logwriter: append + default flush policy
   TEST(0 == init_logwriter(&lgwrt)) ;
   for (uint8_t i = 0; i < log_channel_NROFCHANNEL; ++i) {
      memset(lgwrt.chan[i]->logbuf.addr, 1, 10) ;
      uint32_t S = lgwrt.chan[i]->logbuf.size ;
      for (unsigned n = 0; n < 10; ++n) {
         printf_logwriter(&lgwrt, i, log_flags_NONE, 0, "%d", n) ;
         if (i == 0) {
            // default is flush
            TEST(lgwrt.chan[i]->logbuf.addr[0] == 0) ; // (flush)
            TEST(lgwrt.chan[i]->logbuf.addr[1] == 0) ; // (print)
            TEST(lgwrt.chan[i]->logbuf.size    == S) ;
            TEST(lgwrt.chan[i]->logbuf.logsize == 0) ;
            TEST(1 == read(pipefd[1][0], mem.addr, mem.size)) ;
            TEST(mem.addr[0] == ('0'+n)) ;
         } else {
            // default is buffer
            TEST(lgwrt.chan[i]->logbuf.addr[n+1] == 0) ; // set to 0
            TEST(lgwrt.chan[i]->logbuf.size      == S) ;
            TEST(lgwrt.chan[i]->logbuf.logsize   == n+1) ;
            for (unsigned n2 = 0; n2 <= n; ++n2) {
               TEST(lgwrt.chan[i]->logbuf.addr[n2] == ('0'+n2)) ;
            }
         }
      }
      lgwrt.chan[i]->logbuf.logsize = 0 ;
   }
   TEST(0 == free_logwriter(&lgwrt)) ;

   // TEST printf_logwriter: flush before print if buffer size <= log_config_MINSIZE
   TEST(0 == init_logwriter(&lgwrt)) ;
   for (uint8_t i = 0; i < log_channel_NROFCHANNEL; ++i) {
      uint32_t S = lgwrt.chan[i]->logbuf.size ;
      TEST(S > log_config_MINSIZE) ;
      memset(lgwrt.chan[i]->logbuf.addr, '0', S) ;
      lgwrt.chan[i]->logbuf.logsize = S - log_config_MINSIZE ;
      printf_logwriter(&lgwrt, i, log_flags_NONE, 0, "X") ;
      TEST(lgwrt.chan[i]->logbuf.addr[0] == (i != 0 ? 'X' : 0)) ;   // (flushed before ; printed; CONSOLE => + flush)
      TEST(lgwrt.chan[i]->logbuf.addr[1] == 0) ;
      TEST(lgwrt.chan[i]->logbuf.size    == S) ;
      TEST(lgwrt.chan[i]->logbuf.logsize == (i != 0 ? 1 : 0)) ;
      TEST(S-log_config_MINSIZE+(i==0) == (unsigned)read(pipefd[1][0], mem.addr, mem.size)) ;
      for (unsigned off = 0; off < S-log_config_MINSIZE; ++off) {
         TEST(mem.addr[off] == '0') ;
      }
      TEST(i != 0 || mem.addr[S-log_config_MINSIZE] == 'X') ;
   }
   TEST(0 == free_logwriter(&lgwrt)) ;

   // TEST printf_logwriter: isappend => no flush but truncate is indicated with " ..." at end
   TEST(0 == init_logwriter(&lgwrt)) ;
   for (uint8_t i = 0; i < log_channel_NROFCHANNEL; ++i) {
      uint32_t S = lgwrt.chan[i]->logbuf.size ;
      TEST(S > 5) ;
      memset(lgwrt.chan[i]->logbuf.addr, '0', S) ;
      lgwrt.chan[i]->logbuf.logsize = S - 5 ;
      lgwrt.chan[i]->isappend       = true ;
      printf_logwriter(&lgwrt, i, log_flags_NONE, 0, "XXXXX") ;
      TEST(lgwrt.chan[i]->logbuf.size    == S) ;
      TEST(lgwrt.chan[i]->logbuf.logsize == S-1) ;
      for (unsigned off = 0; off < S-5; ++off) {
         TEST(lgwrt.chan[i]->logbuf.addr[off] == '0') ;
      }
      TEST(0 == memcmp(lgwrt.chan[i]->logbuf.addr+S-5, " ...", 5)) ;
   }
   TEST(0 == free_logwriter(&lgwrt)) ;

   // TEST printf_logwriter: log_flags_START (turns isappend on, but flushed before if necessary independent of isappend)
   TEST(0 == init_logwriter(&lgwrt)) ;
   for (uint8_t i = 0; i < log_channel_NROFCHANNEL; ++i) {
      for (int isappend = 0; isappend <= 1; ++isappend) {
         uint32_t S = lgwrt.chan[i]->logbuf.size ;
         TEST(S > log_config_MINSIZE) ;
         memset(lgwrt.chan[i]->logbuf.addr, '1', S) ;
         lgwrt.chan[i]->logbuf.logsize = S - log_config_MINSIZE ;
         lgwrt.chan[i]->isappend       = isappend ;
         printf_logwriter(&lgwrt, i, log_flags_START, 0, "12345") ;
         TEST(lgwrt.chan[i]->logbuf.size    == S) ;
         TEST(lgwrt.chan[i]->logbuf.logsize == 5) ;
         TEST(lgwrt.chan[i]->isappend       == true) ;
         TEST(0 == memcmp(lgwrt.chan[i]->logbuf.addr, "12345", 6)) ;
         TEST(S-log_config_MINSIZE == (unsigned)read(pipefd[1][0], mem.addr, mem.size)) ;
         for (unsigned off = 0; off < S-log_config_MINSIZE; ++off) {
            TEST(mem.addr[off] == '1') ;
         }
      }
   }
   TEST(0 == free_logwriter(&lgwrt)) ;

   // TEST printf_logwriter: log_flags_END (logs message and truncates if necessary and then switches isappend off)
   TEST(0 == init_logwriter(&lgwrt)) ;
   for (uint8_t i = 0; i < log_channel_NROFCHANNEL; ++i) {
      uint32_t S = lgwrt.chan[i]->logbuf.size ;
      TEST(S > 10) ;
      memset(lgwrt.chan[i]->logbuf.addr, '0', S) ;
      lgwrt.chan[i]->logbuf.logsize = S - 10 ;
      lgwrt.chan[i]->isappend       = true ;
      printf_logwriter(&lgwrt, i, log_flags_END, 0, "0123456789X") ;
      TEST(lgwrt.chan[i]->logbuf.size    == S) ;
      TEST(lgwrt.chan[i]->logbuf.logsize == (i == 0 ? 0 : S-1)) ;
      TEST(lgwrt.chan[i]->isappend       == false) ;
      if (i) { // in buffer
         memcpy(mem.addr, lgwrt.chan[i]->logbuf.addr, S) ;
      } else { // flushed
         TEST(S-1 == (unsigned)read(pipefd[1][0], mem.addr, mem.size)) ;
         mem.addr[S-1] = 0 ;
      }
      for (unsigned off = 0; off < S-10; ++off) {
         TEST(mem.addr[off] == '0') ;
      }
      TEST(0 == memcmp(mem.addr+S-10, "01234 ...", 10)) ;
   }
   TEST(0 == free_logwriter(&lgwrt)) ;

   // TEST printf_logwriter: log_flags_START + log_flags_END
   TEST(0 == init_logwriter(&lgwrt)) ;
   for (uint8_t i = 0; i < log_channel_NROFCHANNEL; ++i) {
      for (int isappend = 0; isappend <= 1; ++isappend) {
         uint32_t S = lgwrt.chan[i]->logbuf.size ;
         TEST(S > log_config_MINSIZE) ;
         memset(lgwrt.chan[i]->logbuf.addr, '1', S) ;
         lgwrt.chan[i]->logbuf.logsize = S - log_config_MINSIZE ;
         lgwrt.chan[i]->isappend       = isappend ;
         printf_logwriter(&lgwrt, i, log_flags_START|log_flags_END, 0, "12345") ;
         TEST(lgwrt.chan[i]->logbuf.size    == S) ;
         TEST(lgwrt.chan[i]->logbuf.logsize == (i == 0 ? 0 : 5)) ;
         TEST(lgwrt.chan[i]->isappend       == false) ;
         if (i) {
            TEST(5 == write(pipefd[1][1], "12345", 5)) ;
         }
         TEST(S-log_config_MINSIZE+5 == (unsigned)read(pipefd[1][0], mem.addr, mem.size)) ;
         for (unsigned off = 0; off < S-log_config_MINSIZE; ++off) {
            TEST(mem.addr[off] == '1') ;
         }
         TEST(0 == memcmp(mem.addr+S-log_config_MINSIZE, "12345", 5)) ;
      }
   }
   TEST(0 == free_logwriter(&lgwrt)) ;

   // TEST printf_logwriter: header
   TEST(0 == init_logwriter(&lgwrt)) ;
   for (uint8_t i = 0; i < log_channel_NROFCHANNEL; ++i) {
      uint32_t S = lgwrt.chan[i]->logbuf.size ;
      TEST(S > log_config_MINSIZE) ;
      memset(lgwrt.chan[i]->logbuf.addr, '0', S) ;
      memset(mem.addr, 'x', log_config_MINSIZE) ;
      mem.addr[log_config_MINSIZE] = 0 ;
      lgwrt.chan[i]->logbuf.logsize = S - log_config_MINSIZE - 1 ;
      log_header_t header = log_header_INIT("__func__", "__file__", 9945+i, i) ;
      printf_logwriter(&lgwrt, i, log_flags_START, &header, "%s", (char*)mem.addr) ;
      TEST(lgwrt.chan[i]->logbuf.size    == S) ;
      TEST(lgwrt.chan[i]->logbuf.logsize == S-1) ;
      TEST(lgwrt.chan[i]->isappend       == true) ;
      for (unsigned off = 0; off < S-log_config_MINSIZE-1; ++off) {
         TEST(lgwrt.chan[i]->logbuf.addr[off] == '0') ;
      }
      char * start = (char*)lgwrt.chan[i]->logbuf.addr+S-log_config_MINSIZE-1 ;
      char * eol   = strchr(start, '\n') ? strchr(strchr(start, '\n')+1, '\n') ? strchr(strchr(strchr(start, '\n')+1, '\n')+1, '\n')+1 : 0 : 0 ;
      size_t L     = (size_t) (eol - (char*)lgwrt.chan[i]->logbuf.addr) ;
      TEST(0 != eol) ;
      TEST(0 == compare_header((size_t)(eol - start), (uint8_t*)start, "__func__", "__file__", 9945+i, i)) ;
      for (size_t off = L; off < S-5; ++off) {
         TEST(lgwrt.chan[i]->logbuf.addr[off] == 'x') ;
      }
      TEST(0 == strcmp((char*)lgwrt.chan[i]->logbuf.addr+S-5, " ...")) ;
   }
   TEST(0 == free_logwriter(&lgwrt)) ;

   // TEST printf_logwriter: EINVAL
   TEST(0 == init_logwriter(&lgwrt)) ;
   printf_logwriter(&lgwrt, log_channel_NROFCHANNEL, log_flags_NONE, 0, "123") ;
   for (uint8_t i = 0; i < log_channel_NROFCHANNEL; ++i) {
      TEST(lgwrt.chan[i]->logbuf.logsize == 0) ;
   }
   TEST(0 == free_logwriter(&lgwrt)) ;

   // TEST printtext_logwriter: test content
   TEST(0 == init_logwriter(&lgwrt)) ;
   for (uint8_t i = 0; i < log_channel_NROFCHANNEL; ++i) {
      uint32_t S = lgwrt.chan[i]->logbuf.size ;
      TEST(S > log_config_MINSIZE) ;
      memset(lgwrt.chan[i]->logbuf.addr, '0', S) ;
      memset(mem.addr, 'x', log_config_MINSIZE) ;
      mem.addr[log_config_MINSIZE] = 0 ;
      lgwrt.chan[i]->logbuf.logsize = S - log_config_MINSIZE - 1 ;
      log_header_t header = log_header_INIT("func", "file", 100+i, 1+i) ;
      printtext_logwriter(&lgwrt, i, log_flags_START, &header, textres_test, 3, (char*)mem.addr) ;
      TEST(lgwrt.chan[i]->logbuf.size    == S) ;
      TEST(lgwrt.chan[i]->logbuf.logsize == S-1) ;
      TEST(lgwrt.chan[i]->isappend       == true) ;
      for (unsigned off = 0; off < S-log_config_MINSIZE-1; ++off) {
         TEST(lgwrt.chan[i]->logbuf.addr[off] == '0') ;
      }
      char * start = (char*)lgwrt.chan[i]->logbuf.addr+S-log_config_MINSIZE-1 ;
      char * eol   = strchr(start, '\n') ? strchr(strchr(start, '\n')+1, '\n') ? strchr(strchr(strchr(start, '\n')+1, '\n')+1, '\n')+1 : 0 : 0 ;
      size_t L     = (size_t) (eol - (char*)lgwrt.chan[i]->logbuf.addr) ;
      TEST(0 != eol) ;
      TEST(0 == compare_header((size_t)(eol - start), (uint8_t*)start, "func", "file", 100+i, 1+i)) ;
      TEST(lgwrt.chan[i]->logbuf.addr[L]   == '3') ;
      TEST(lgwrt.chan[i]->logbuf.addr[L+1] == '|') ;
      for (size_t off = L+2; off < S-5; ++off) {
         TEST(lgwrt.chan[i]->logbuf.addr[off] == 'x') ;
      }
      TEST(0 == strcmp((char*)lgwrt.chan[i]->logbuf.addr+S-5, " ...")) ;
   }
   TEST(0 == free_logwriter(&lgwrt)) ;

   // TEST printtext_logwriter: EINVAL
   TEST(0 == init_logwriter(&lgwrt)) ;
   printtext_logwriter(&lgwrt, log_channel_NROFCHANNEL, log_flags_NONE, 0, textres_test) ;
   for (uint8_t i = 0; i < log_channel_NROFCHANNEL; ++i) {
      TEST(lgwrt.chan[i]->logbuf.logsize == 0) ;
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
   int bytes ;
   while (0 < (bytes = read(pipefd[0][0], mem.addr, mem.size))) {
      printf("%*s", bytes, mem.addr) ;
   }
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
   TEST(s_logwriter_interface.printf      == &printf_logwriter)
   TEST(s_logwriter_interface.flushbuffer == &flushbuffer_logwriter) ;
   TEST(s_logwriter_interface.clearbuffer == &clearbuffer_logwriter) ;
   TEST(s_logwriter_interface.getbuffer   == &getbuffer_logwriter) ;

   // TEST interface_logwriter
   TEST(interface_logwriter() == genericcast_logit(&s_logwriter_interface, logwriter_t)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_io_writer_log_logwriter()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())       goto ONABORT ;
   if (test_query())          goto ONABORT ;
   if (test_update())         goto ONABORT ;
   if (test_initthread())     goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
