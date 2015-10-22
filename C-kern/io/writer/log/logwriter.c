/* title: LogWriter impl
   Implements logging of error messages to standard error channel.
   See <LogWriter>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/io/writer/log/logwriter.h
    Header file of <LogWriter>.

   file: C-kern/io/writer/log/logwriter.c
    Implementation file <LogWriter impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/io/writer/log/log.h"
#include "C-kern/api/io/writer/log/logbuffer.h"
#include "C-kern/api/io/writer/log/logwriter.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/pagecache_macros.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/io/accessmode.h"
#include "C-kern/api/io/pipe.h"
#include "C-kern/api/io/filesystem/directory.h"
#include "C-kern/api/io/filesystem/mmfile.h"
#include "C-kern/api/platform/task/thread.h"
#include "C-kern/api/string/cstring.h"
#endif


/* struct: logwriter_chan_t */
struct logwriter_chan_t;

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

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_logwriter_errtimer
 * USed to simulate errors. */
static test_errortimer_t   s_logwriter_errtimer = test_errortimer_FREE;
#endif

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
   return cast_logit(&s_logwriter_interface, logwriter_t);
}

// group: helper

/* function: allocatebuffer_logwriter
 * Reserves some memory pages for internal buffer. */
static int allocatebuffer_logwriter(/*out*/memblock_t * buffer)
{
   int err;
   static_assert(16384 < INT_MAX, "size_t of buffer will be used in vnsprintf and returned as int");
   static_assert(16384 > minbufsize_logwriter(), "buffer can hold at least 2 entries");
   ONERROR_testerrortimer(&s_logwriter_errtimer, &err, ONERR);

   err = ALLOC_PAGECACHE(pagesize_16384, buffer);
   if (err) goto ONERR;

   return 0;
ONERR:
   return err;
}

/* function: freebuffer_logwriter
 * Frees internal buffer. */
static int freebuffer_logwriter(memblock_t * buffer)
{
   int err;

   err = RELEASE_PAGECACHE(buffer);
   SETONERROR_testerrortimer(&s_logwriter_errtimer, &err);
   if (err) goto ONERR;

   return 0;
ONERR:
   return err;
}

// group: lifetime

static void initchan_logwriter(/*out*/logwriter_t * lgwrt, const size_t logsize)
{
   const size_t errlogsize = lgwrt->size - (log_channel__NROF-1) * logsize;

   size_t offset = 0;

   for (uint8_t channel = 0; channel < log_channel__NROF; ++channel) {
      size_t      bufsize  = (channel == log_channel_ERR     ? errlogsize        : logsize);
      log_state_e logstate = (channel == log_channel_USERERR ? log_state_IGNORED : log_state_BUFFERED);
      lgwrt->chan[channel] = (logwriter_chan_t) logwriter_chan_INIT(bufsize, lgwrt->addr + offset, iochannel_STDERR, logstate);
      offset += logsize;
   }
}

int init_logwriter(/*out*/logwriter_t * lgwrt)
{
   int err;

   err = allocatebuffer_logwriter(cast_memblock(lgwrt, ));
   if (err) goto ONERR;

   initchan_logwriter(lgwrt, 2*log_config_MINSIZE);

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int initstatic_logwriter(/*out*/logwriter_t * lgwrt, size_t bufsize/*>= minbufsize_logwriter()*/, uint8_t logbuf[bufsize])
{
   if (bufsize < minbufsize_logwriter()) goto ONERR;

   *cast_memblock(lgwrt,) = (memblock_t) memblock_INIT(bufsize, logbuf);

   initchan_logwriter(lgwrt, log_config_MINSIZE);

   return 0;
ONERR:
   // TODO: TRACEEXIT_INITERRLOG(err);
   return EINVAL;
}

void freechan_logwriter(logwriter_t * lgwrt)
{
   for (uint8_t channel = 0; channel < log_channel__NROF; ++channel) {
      // calling free is currently not necessary
      lgwrt->chan[channel] = (logwriter_chan_t) logwriter_chan_FREE;
   }
}

int free_logwriter(logwriter_t * lgwrt)
{
   int err;

   freechan_logwriter(lgwrt);

   err = freebuffer_logwriter(cast_memblock(lgwrt,));

   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

int freestatic_logwriter(logwriter_t * lgwrt)
{
   freechan_logwriter(lgwrt);

   *cast_memblock(lgwrt,) = (memblock_t) memblock_FREE;

   return 0;
}


// group: query

void getbuffer_logwriter(const logwriter_t * lgwrt, uint8_t channel, /*out*/uint8_t ** buffer, /*out*/size_t * size)
{
   int err;

   VALIDATE_INPARAM_TEST(channel < log_channel__NROF, ONERR,);

   getbuffer_logbuffer(&lgwrt->chan[channel].logbuf, buffer, size);

   return;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return;
}

uint8_t getstate_logwriter(const logwriter_t * lgwrt, uint8_t channel)
{
   if (channel < log_channel__NROF) {
      return lgwrt->chan[channel].logstate;
   }

   return log_state_IGNORED;
}

int compare_logwriter(const logwriter_t * lgwrt, uint8_t channel, size_t logsize, const uint8_t logbuffer[logsize])
{
   int err;

   VALIDATE_INPARAM_TEST(channel < log_channel__NROF, ONERR,);

   return compare_logbuffer(&lgwrt->chan[channel].logbuf, logsize, logbuffer);
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: config

void setstate_logwriter(logwriter_t * lgwrt, uint8_t channel, uint8_t logstate)
{
   if (channel < log_channel__NROF) {
      logwriter_chan_t * chan = &lgwrt->chan[channel];
      chan->funcname = 0;
      chan->logstate = logstate;
   }
}

// group: change

void truncatebuffer_logwriter(logwriter_t * lgwrt, uint8_t channel, size_t size)
{
   int err;

   VALIDATE_INPARAM_TEST(channel < log_channel__NROF, ONERR,);

   logwriter_chan_t * chan = &lgwrt->chan[channel];

   truncate_logbuffer(&chan->logbuf, size);

   return;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return;
}

void flushbuffer_logwriter(logwriter_t * lgwrt, uint8_t channel)
{
   int err;

   VALIDATE_INPARAM_TEST(channel < log_channel__NROF, ONERR,);

   logwriter_chan_t * chan = &lgwrt->chan[channel];

   flush_logwriterchan(chan);

   return;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return;
}

#define GETCHANNEL_logwriter(          \
            /*logwriter_t* */lgwrt,    \
            /*log_channel_e*/channel,  \
            /*out*/chan)               \
         VALIDATE_INPARAM_TEST(channel < log_channel__NROF, ONERR,);       \
         chan = &lgwrt->chan[channel];                                             \
         if (chan->logstate == log_state_IGNORED) return;

static inline void beginwrite_logwriter(
   logwriter_chan_t *   chan,
   uint8_t              flags,
   const log_header_t * header)
{
   if (  header
         && chan->funcname != header->funcname) {
      printheader_logbuffer(&chan->logbuf, header);
      chan->funcname = header->funcname;
   }

   if (0 != (flags&log_flags_LAST)) {
      chan->funcname = 0;
   }
}

static inline void endwrite_logwriter(logwriter_chan_t * chan, uint8_t flags)
{
   if (  (  chan->logstate != log_state_BUFFERED
            || sizefree_logbuffer(&chan->logbuf) < log_config_MINSIZE)
         && (  chan->logstate == log_state_IMMEDIATE
               || 0 != (flags&log_flags_LAST))) {
      flush_logwriterchan(chan);
   }
}

void vprintf_logwriter(logwriter_t * lgwrt, uint8_t channel, uint8_t flags, const struct log_header_t * header, const char * format, va_list args)
{
   int err;
   logwriter_chan_t * chan;

   GETCHANNEL_logwriter(lgwrt, channel, /*out*/chan);
   beginwrite_logwriter(chan, flags, header);

   vprintf_logbuffer(&chan->logbuf, format, args);

   endwrite_logwriter(chan, flags);

   return;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return;
}

void printf_logwriter(logwriter_t * lgwrt, uint8_t channel, uint8_t flags, const struct log_header_t * header, const char * format, ...)
{
   va_list args;
   va_start(args, format);
   vprintf_logwriter(lgwrt, channel, flags, header, format, args);
   va_end(args);
}

void printtext_logwriter(logwriter_t * lgwrt, uint8_t channel, uint8_t flags, const struct log_header_t * header, log_text_f textf, ...)
{
   int err;
   logwriter_chan_t * chan;

   GETCHANNEL_logwriter(lgwrt, channel, /*out*/chan);
   beginwrite_logwriter(chan, flags, header);

   if (textf) {
      va_list args;
      va_start(args, textf);
      textf(&chan->logbuf, args);
      va_end(args);
   }

   endwrite_logwriter(chan, flags);

   return;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return;
}

// group: test

#ifdef KONFIG_UNITTEST

static bool isfree_logwriter(logwriter_t * lgwrt)
{
   if (0 != lgwrt->addr || 0 != lgwrt->size) return false;

   for (unsigned channel = 0; channel < lengthof(lgwrt->chan); ++channel) {
      if (lgwrt->chan[channel].logbuf.addr != 0
         || lgwrt->chan[channel].logbuf.size != 0
         || lgwrt->chan[channel].logbuf.io != iochannel_FREE
         || lgwrt->chan[channel].logbuf.logsize != 0
         || lgwrt->chan[channel].funcname != 0
         || lgwrt->chan[channel].logstate != 0)
         return false;
   }

   return true;
}

static int test_initfree(void)
{
   logwriter_t lgwrt = logwriter_FREE;
   uint8_t     logbuf[minbufsize_logwriter()];

   // TEST logwriter_FREE
   TEST(1 == isfree_logwriter(&lgwrt));

   // TEST init_logwriter
   TEST(0 == init_logwriter(&lgwrt));
   TEST(lgwrt.addr != 0);
   TEST(lgwrt.size == 16384);
   for (unsigned i = 0, offset = 0; i < log_channel__NROF; ++i) {
      TEST(lgwrt.chan[i].logbuf.addr == lgwrt.addr + offset);
      TEST(lgwrt.chan[i].logbuf.size == (i == log_channel_ERR ? 16384 - (log_channel__NROF-1)*2*log_config_MINSIZE : 2*log_config_MINSIZE));
      TEST(lgwrt.chan[i].logbuf.io   == iochannel_STDERR);
      TEST(lgwrt.chan[i].logbuf.logsize == 0);
      TEST(lgwrt.chan[i].logstate    == (i ? log_state_BUFFERED : log_state_IGNORED));
      offset += lgwrt.chan[i].logbuf.size;
   }

   // TEST free_logwriter
   TEST(0 == free_logwriter(&lgwrt));
   TEST(1 == isfree_logwriter(&lgwrt));
   TEST(0 == free_logwriter(&lgwrt));
   TEST(1 == isfree_logwriter(&lgwrt));
   TEST(1 == isvalid_iochannel(iochannel_STDOUT));
   TEST(1 == isvalid_iochannel(iochannel_STDERR));

   // TEST free_logwriter: EINVAL
   TEST(0 == init_logwriter(&lgwrt));
   init_testerrortimer(&s_logwriter_errtimer, 1, EINVAL);
   TEST(EINVAL == free_logwriter(&lgwrt));
   TEST(1 == isfree_logwriter(&lgwrt));

   // TEST init_logwriter: ENOMEM
   init_testerrortimer(&s_logwriter_errtimer, 1, ENOMEM);
   TEST(ENOMEM == init_logwriter(&lgwrt));
   TEST(1 == isfree_logwriter(&lgwrt));

   // TEST initstatic_logwriter
   TEST(0 == initstatic_logwriter(&lgwrt, sizeof(logbuf), logbuf));
   TEST(lgwrt.addr == logbuf);
   TEST(lgwrt.size == sizeof(logbuf));
   for (unsigned i = 0, offset = 0; i < log_channel__NROF; ++i) {
      TEST(lgwrt.chan[i].logbuf.addr == lgwrt.addr + offset);
      TEST(lgwrt.chan[i].logbuf.size == log_config_MINSIZE);
      TEST(lgwrt.chan[i].logbuf.io   == iochannel_STDERR);
      TEST(lgwrt.chan[i].logbuf.logsize == 0);
      TEST(lgwrt.chan[i].logstate    == (i ? log_state_BUFFERED : log_state_IGNORED));
      offset += lgwrt.chan[i].logbuf.size;
   }

   // TEST freestatic_logwriter
   TEST(0 == freestatic_logwriter(&lgwrt));
   TEST(1 == isfree_logwriter(&lgwrt));
   TEST(0 == freestatic_logwriter(&lgwrt));
   TEST(1 == isfree_logwriter(&lgwrt));

   // TEST initstatic_logwriter: EINVAL
   TEST(EINVAL == initstatic_logwriter(&lgwrt, minbufsize_logwriter()-1, logbuf));
   TEST(1 == isfree_logwriter(&lgwrt));

   return 0;
ONERR:
   free_logwriter(&lgwrt);
   return EINVAL;
}

static int test_query(void)
{
   logwriter_t lgwrt = logwriter_FREE;

   // prepare
   TEST(0 == init_logwriter(&lgwrt));

   // TEST getbuffer_logwriter
   for (uint8_t i = 0; i < log_channel__NROF; ++i) {
      lgwrt.chan[i].logbuf.logsize  = 0;
      printf_logbuffer(&lgwrt.chan[i].logbuf, "12345");
      uint8_t *logbuffer = 0;
      size_t   logsize   = 0;
      getbuffer_logwriter(&lgwrt, i, &logbuffer, &logsize);
      TEST(logbuffer == lgwrt.chan[i].logbuf.addr);
      TEST(logsize   == 5);
      printf_logbuffer(&lgwrt.chan[i].logbuf, "%s", "abcdef");
      getbuffer_logwriter(&lgwrt, i, &logbuffer, &logsize);
      TEST(logbuffer == lgwrt.chan[i].logbuf.addr);
      TEST(logsize   == 11);
      TEST(0 == strcmp((char*)logbuffer, "12345abcdef"));
   }

   // TEST getstate_logwriter
   for (uint8_t s = 0; s < log_state__NROF; ++s) {
      for (uint8_t i = 0; i < log_channel__NROF; ++i) {
         lgwrt.chan[i].logstate = (uint8_t) (s + 1);
      }
      for (uint8_t i = 0; i < log_channel__NROF; ++i) {
         TEST(s+1u == getstate_logwriter(&lgwrt, i));
         lgwrt.chan[i].logstate = s;
         TEST(s    == getstate_logwriter(&lgwrt, i));
      }
   }

   // TEST getstate_logwriter: channelnr out of range
   TEST(log_state_IGNORED == getstate_logwriter(&lgwrt, log_channel__NROF));
   TEST(log_state_IGNORED == getstate_logwriter(&lgwrt, (uint8_t)-1));

   // TEST compare_logwriter
   for (uint8_t i = 0; i < log_channel__NROF; ++i) {
      truncate_logbuffer(&lgwrt.chan[i].logbuf, 0);
      printf_logbuffer(&lgwrt.chan[i].logbuf, "[1: XXX]\ntest\n");
      TEST(0 == compare_logwriter(&lgwrt, i, 14, (const uint8_t*)"[1: XXX]\ntest\n"));
      TEST(0 == compare_logwriter(&lgwrt, i, 14, (const uint8_t*)"[1: YYY]\ntest\n"));
      TEST(EINVAL == compare_logwriter(&lgwrt, i, 13, (const uint8_t*)"[1: XXX]\ntest\n"));
      TEST(EINVAL == compare_logwriter(&lgwrt, i, 14, (const uint8_t*)"[1: XXX]\ntesT\n"));
   }

   // TEST compare_logwriter: channelnr out of range
   TEST(EINVAL == compare_logwriter(&lgwrt, log_channel__NROF, 14, (const uint8_t*)"[1: XXX]\ntest\n"));
   TEST(EINVAL == compare_logwriter(&lgwrt, (uint8_t)-1, 14, (const uint8_t*)"[1: XXX]\ntest\n"));

   // unprepare
   TEST(0 == free_logwriter(&lgwrt));

   return 0;
ONERR:
   free_logwriter(&lgwrt);
   return EINVAL;
}

static int test_config(void)
{
   logwriter_t lgwrt = logwriter_FREE;

   // prepare
   TEST(0 == init_logwriter(&lgwrt));

   // TEST setstate_logwriter
   for (uint8_t s = 0; s < log_state__NROF; ++s) {
      for (uint8_t i = 0; i < log_channel__NROF; ++i) {
         lgwrt.chan[i].logstate = (uint8_t) (s + 1);
      }
      for (uint8_t i = 0; i < log_channel__NROF; ++i) {
         TEST(lgwrt.chan[i].logstate == s+1u);
         lgwrt.chan[i].funcname = (void*)1;
         setstate_logwriter(&lgwrt, i, s);
         // remembered state cleared
         TEST(lgwrt.chan[i].funcname == 0);
         TEST(lgwrt.chan[i].logstate == s);
      }
   }

   // TEST setstate_logwriter: channelnr out of range
   setstate_logwriter(&lgwrt, log_channel__NROF, 0);
   setstate_logwriter(&lgwrt, (uint8_t)-1, 0);
   // check that call was ignored
   for (uint8_t i = 0; i < log_channel__NROF; ++i) {
      TEST(log_state__NROF-1 == lgwrt.chan[i].logstate);
   }

   // unprepare
   TEST(0 == free_logwriter(&lgwrt));

   return 0;
ONERR:
   free_logwriter(&lgwrt);
   return EINVAL;
}

static int compare_header(size_t buffer_size, uint8_t buffer_addr[buffer_size], const char * funcname, const char * filename, int linenr)
{
   char     buffer[100];
   int      nr1;
   uint64_t nr2;
   uint32_t nr3;
   TEST(3 == sscanf((char*)buffer_addr, "[%d: %"SCNu64".%"SCNu32, &nr1, &nr2, &nr3));
   TEST((unsigned)nr1 == threadid_maincontext());
   struct timeval tv;
   TEST(0 == gettimeofday(&tv, 0));
   TEST((uint64_t)tv.tv_sec >= nr2);
   TEST((uint64_t)tv.tv_sec <= nr2 + 1);
   TEST(nr3 < 1000000);

   snprintf(buffer, sizeof(buffer), "[%d: %"PRIu64".%06"PRIu32"s]\n%s() %s:%d\n", nr1, nr2, nr3, funcname, filename, linenr);
   TEST(strlen(buffer) == buffer_size);
   TEST(0 == memcmp(buffer, buffer_addr, strlen(buffer)));

   return 0;
ONERR:
   return EINVAL;
}

static void textres_test(logbuffer_t * logbuf, va_list vargs)
{
   vprintf_logbuffer(logbuf, "%d|%s", vargs);
}

static int test_write(void)
{
   logwriter_t    lgwrt  = logwriter_FREE;
   int            logfd  = -1;
   pipe_t         pipefd = pipe_FREE;
   memblock_t     mem    = memblock_FREE;

   // prepare
   TEST(0 == ALLOC_PAGECACHE(pagesize_16384, &mem));
   TEST(0 == init_pipe(&pipefd));

   // TEST truncatebuffer_logwriter
   TEST(0 == init_logwriter(&lgwrt));
   for (uint8_t i = 0; i < log_channel__NROF; ++i) {
      for (unsigned size = 0; size < 32; ++size) {
         lgwrt.chan[i].logbuf.logsize      = size;
         lgwrt.chan[i].logbuf.addr[size]   = 'x';
         lgwrt.chan[i].logbuf.addr[size+1] = 'x';
         // ignored if size >= logsize
         for (size_t tsize = size+1; tsize >= size && tsize <= size+1; --tsize) {
            truncatebuffer_logwriter(&lgwrt, i, tsize);
            TEST(size == lgwrt.chan[i].logbuf.logsize);
            TEST('x'  == lgwrt.chan[i].logbuf.addr[size]);
            TEST('x'  == lgwrt.chan[i].logbuf.addr[size+1]);
         }
         // executed if size < logsize
         lgwrt.chan[i].logbuf.logsize = 32;
         truncatebuffer_logwriter(&lgwrt, i, size);
         TEST(size == lgwrt.chan[i].logbuf.logsize);
         TEST(0    == lgwrt.chan[i].logbuf.addr[size]);
         TEST('x'  == lgwrt.chan[i].logbuf.addr[size+1]);
      }
   }
   TEST(0 == free_logwriter(&lgwrt));

   // TEST flushbuffer_logwriter
   TEST(0 == init_logwriter(&lgwrt));
   for (uint8_t i = 0; i < log_channel__NROF; ++i) {
      // write log into pipe
      logfd = lgwrt.chan[i].logbuf.io;
      lgwrt.chan[i].logbuf.io = pipefd.write;
      // set log buffer
      uint32_t S = lgwrt.chan[i].logbuf.size;
      for (unsigned b = 0; b < S; ++b) {
         lgwrt.chan[i].logbuf.addr[b] = (uint8_t) (1+b+i);
      }
      lgwrt.chan[i].logbuf.logsize = S;
      // test
      flushbuffer_logwriter(&lgwrt, i);
      TEST(0 == lgwrt.chan[i].logbuf.addr[0]);
      TEST(S == lgwrt.chan[i].logbuf.size);
      TEST(0 == lgwrt.chan[i].logbuf.logsize);
      // compare flushed content
      int rs = read(pipefd.read, mem.addr, mem.size);
      TESTP(S == (unsigned)rs, "rs:%d", rs);
      for (unsigned b = 0; b < S; ++b) {
         TEST(mem.addr[b] == (uint8_t) (1+b+i));
      }
      TEST(-1 == read(pipefd.read, mem.addr, mem.size));
      // reset
      lgwrt.chan[i].logbuf.io = logfd;
   }
   TEST(0 == free_logwriter(&lgwrt));

   // TEST printf_logwriter: log_flags_NONE for all log_state_e
   TEST(0 == init_logwriter(&lgwrt));
   for (uint8_t i = 0; i < log_channel__NROF; ++i) {
      const uint32_t S = lgwrt.chan[i].logbuf.size;
      // write log into pipe
      logfd = lgwrt.chan[i].logbuf.io;
      lgwrt.chan[i].logbuf.io = pipefd.write;
      for (uint8_t s = 0; s < log_state__NROF; ++s) {
         setstate_logwriter(&lgwrt, i, s);
         memset(lgwrt.chan[i].logbuf.addr, 1, log_config_MINSIZE);
         for (unsigned n = 0; n < 10; ++n) {
            printf_logwriter(&lgwrt, i, log_flags_NONE, 0, "%d", n);
            // check logbuf
            TEST(lgwrt.chan[i].logbuf.size == S);
            switch ((log_state_e)s) {
            case log_state_IGNORED:
               TEST(lgwrt.chan[i].logbuf.addr[0] == 1); // ignored
               TEST(lgwrt.chan[i].logbuf.logsize == 0);
               break;
            case log_state_BUFFERED:
            case log_state_UNBUFFERED:
               TEST(lgwrt.chan[i].logbuf.addr[n+1] == 0); // set to 0
               TEST(lgwrt.chan[i].logbuf.logsize   == n+1);
               for (unsigned n2 = 0; n2 <= n; ++n2) {
                  TEST(lgwrt.chan[i].logbuf.addr[n2] == ('0'+n2));
               }
               break;
            case log_state_IMMEDIATE:
               TEST(lgwrt.chan[i].logbuf.addr[0] == 0); // (flush)
               TEST(lgwrt.chan[i].logbuf.addr[1] == 0); // (print)
               TEST(lgwrt.chan[i].logbuf.logsize == 0);
               TEST(1 == read(pipefd.read, mem.addr, mem.size));
               TEST(mem.addr[0] == ('0'+n));
               break;
            }
         }
         lgwrt.chan[i].logbuf.logsize = 0;
      }
      // reset
      lgwrt.chan[i].logbuf.io = logfd;
   }
   TEST(0 == free_logwriter(&lgwrt));

   // TEST printf_logwriter: log_flags_LAST for all log_state_e
   TEST(0 == init_logwriter(&lgwrt));
   for (uint8_t i = 0; i < log_channel__NROF; ++i) {
      const uint32_t S = lgwrt.chan[i].logbuf.size;
      // write log into pipe
      logfd = lgwrt.chan[i].logbuf.io;
      lgwrt.chan[i].logbuf.io = pipefd.write;
      for (uint8_t s = 0; s < log_state__NROF; ++s) {
         setstate_logwriter(&lgwrt, i, s);
         for (unsigned n = 0; n < 10; ++n) {
            memset(lgwrt.chan[i].logbuf.addr, 1, log_config_MINSIZE);
            memcpy(lgwrt.chan[i].logbuf.addr, "012345678", n);
            lgwrt.chan[i].logbuf.logsize = n;
            printf_logwriter(&lgwrt, i, log_flags_LAST, 0, "%d", n);
            // check logbuf
            TEST(lgwrt.chan[i].logbuf.size == S);
            switch ((log_state_e)s) {
            case log_state_IGNORED:
               TEST(lgwrt.chan[i].logbuf.addr[n] == 1); // ignored
               TEST(lgwrt.chan[i].logbuf.logsize == n);
               break;
            case log_state_BUFFERED:
               TEST(lgwrt.chan[i].logbuf.addr[n+1] == 0); // set to 0
               TEST(lgwrt.chan[i].logbuf.logsize   == n+1);
               for (unsigned n2 = 0; n2 <= n; ++n2) {
                  TEST(lgwrt.chan[i].logbuf.addr[n2] == ('0'+n2));
               }
               break;
            case log_state_UNBUFFERED:
            case log_state_IMMEDIATE:
               TEST(lgwrt.chan[i].logbuf.addr[0] == 0);   // (flush)
               TEST(lgwrt.chan[i].logbuf.addr[n+1] == 0); // (print)
               TEST(lgwrt.chan[i].logbuf.logsize == 0);
               TEST(n+1 == (unsigned) read(pipefd.read, mem.addr, mem.size));
               for (unsigned n2 = 0; n2 <= n; ++n2) {
                  TEST(mem.addr[n2] == ('0'+n2));
               }
               break;
            }
         }
         lgwrt.chan[i].logbuf.logsize = 0;
      }
      // reset
      lgwrt.chan[i].logbuf.io = logfd;
   }
   TEST(0 == free_logwriter(&lgwrt));

   // TEST printf_logwriter: log_flags_NONE && buffer size > log_config_MINSIZE
   TEST(0 == init_logwriter(&lgwrt));
   for (uint8_t i = 0; i < log_channel__NROF; ++i) {
      const uint32_t S = lgwrt.chan[i].logbuf.size;
      TEST(S > log_config_MINSIZE);
      // write log into pipe
      logfd = lgwrt.chan[i].logbuf.io;
      lgwrt.chan[i].logbuf.io = pipefd.write;
      for (uint8_t s = 0; s < log_state__NROF; ++s) {
         if (s == log_state_IGNORED || s == log_state_IMMEDIATE) continue;
         setstate_logwriter(&lgwrt, i, s);
         memset(lgwrt.chan[i].logbuf.addr, '0', S);
         lgwrt.chan[i].logbuf.logsize = S - log_config_MINSIZE;

         // test log_flags_NONE
         printf_logwriter(&lgwrt, i, log_flags_NONE, 0, "X");
         // check logwrt.chan[i].logbuf was not flushed
         TEST(lgwrt.chan[i].logbuf.addr[0] == '0');
         TEST(lgwrt.chan[i].logbuf.addr[S-log_config_MINSIZE] == 'X');
         TEST(lgwrt.chan[i].logbuf.size    == S);
         TEST(lgwrt.chan[i].logbuf.logsize == S-log_config_MINSIZE+1);
         TEST(-1 == read(pipefd.read, mem.addr, mem.size));

         // test log_flags_LAST
         printf_logwriter(&lgwrt, i, log_flags_LAST, 0, "Z");
         // check logwrt.chan[i].logbuf was flushed
         TEST(lgwrt.chan[i].logbuf.addr[0] == 0);
         TEST(lgwrt.chan[i].logbuf.size    == S);
         TEST(lgwrt.chan[i].logbuf.logsize == 0);
         unsigned rs = (unsigned) read(pipefd.read, mem.addr, mem.size);
         TEST(rs == S-log_config_MINSIZE+2);
         for (unsigned off = 0; off < rs-2; ++off) {
            TEST(mem.addr[off] == '0');
         }
         TEST(mem.addr[rs-2] == 'X');
         TEST(mem.addr[rs-1] == 'Z');
      }
      // reset
      lgwrt.chan[i].logbuf.io = logfd;
   }
   TEST(0 == free_logwriter(&lgwrt));

   // TEST printf_logwriter: log_flags_NONE && truncate is indicated with " ..." at end
   TEST(0 == init_logwriter(&lgwrt));
   for (uint8_t i = 0; i < log_channel__NROF; ++i) {
      uint32_t S = lgwrt.chan[i].logbuf.size;
      TEST(S > 7);
      for (uint8_t s = 0; s < log_state__NROF; ++s) {
         if (s == log_state_IGNORED || s == log_state_IMMEDIATE) continue;
         setstate_logwriter(&lgwrt, i, s);
         memset(lgwrt.chan[i].logbuf.addr, '0', S);
         lgwrt.chan[i].logbuf.logsize = S - 7;
         // test nothing changes in case of already truncated message
         for (int rep = 0; rep < 2; ++rep) {
            printf_logwriter(&lgwrt, i, log_flags_NONE, 0, "XXXXXXX");
            // check lgwrt.chan[i].logbuf content is truncated
            TEST(lgwrt.chan[i].logbuf.size == S);
            TEST(lgwrt.chan[i].logbuf.logsize == S-1);
            TEST(0 == memcmp(lgwrt.chan[i].logbuf.addr+S-7, "XX ...", 7));
            for (unsigned off = 0; off < S-7; ++off) {
               TEST(lgwrt.chan[i].logbuf.addr[off] == '0');
            }
         }
      }
   }
   TEST(0 == free_logwriter(&lgwrt));

   // TEST printf_logwriter: header
   TEST(0 == init_logwriter(&lgwrt));
   for (uint8_t i = 0; i < log_channel__NROF; ++i) {
      uint32_t S = lgwrt.chan[i].logbuf.size;
      for (uint8_t s = 0; s < log_state__NROF; ++s) {
         if (s == log_state_IGNORED || s == log_state_IMMEDIATE) continue;
         setstate_logwriter(&lgwrt, i, s);
         lgwrt.chan[i].logbuf.logsize = 0;
         lgwrt.chan[i].funcname       = 0;
         log_header_t header = log_header_INIT("__func__", "__file__", 9945+i);
         printf_logwriter(&lgwrt, i, log_flags_NONE, &header, 0);
         // check lgwrt.chan[i].logbuf
         TEST(lgwrt.chan[i].logbuf.size == S);
         TEST(lgwrt.chan[i].logbuf.logsize > 0);
         TEST(lgwrt.chan[i].funcname == header.funcname);
         uint8_t * start = lgwrt.chan[i].logbuf.addr;
         uint8_t * end   = memrchr((char*) start, '\n', lgwrt.chan[i].logbuf.logsize);
         TEST(0 != end);
         TEST(0 == compare_header((size_t)(1 + end - start), start, "__func__", "__file__", 9945+i));
      }
   }
   TEST(0 == free_logwriter(&lgwrt));

   // TEST printf_logwriter: header truncated
   TEST(0 == init_logwriter(&lgwrt));
   for (uint8_t i = 0; i < log_channel__NROF; ++i) {
      uint32_t S = lgwrt.chan[i].logbuf.size;
      for (uint8_t s = 0; s < log_state__NROF; ++s) {
         if (s == log_state_IGNORED || s == log_state_IMMEDIATE) continue;
         setstate_logwriter(&lgwrt, i, s);
         lgwrt.chan[i].logbuf.logsize = 0;
         lgwrt.chan[i].funcname       = 0;
         log_header_t header = log_header_INIT("__func__", "__file__", 9945+i);
         printf_logwriter(&lgwrt, i, log_flags_NONE, &header, 0);
         for (size_t hs = lgwrt.chan[i].logbuf.logsize; hs > 4; --hs) {
            lgwrt.chan[i].logbuf.logsize = S - hs;
            lgwrt.chan[i].funcname       = 0;
            memset(lgwrt.chan[i].logbuf.addr + S - hs, 0, hs);
            // test nothing changes in case of already truncated message
            for (int rep = 0; rep < 2; ++rep) {
               printf_logwriter(&lgwrt, i, log_flags_NONE, &header, 0);
               // check lgwrt.chan[i].logbuf
               TEST(lgwrt.chan[i].logbuf.size == S);
               TEST(lgwrt.chan[i].logbuf.logsize == S-1);
               TEST(lgwrt.chan[i].funcname == header.funcname);
               TEST(hs <= 5 || lgwrt.chan[i].logbuf.addr[S - hs] == '[');
               TEST(0 == strcmp((char*)lgwrt.chan[i].logbuf.addr+S-5, " ..."));
            }
         }
      }
   }
   TEST(0 == free_logwriter(&lgwrt));

   // TEST printf_logwriter: header is ignored if funcname == last.funcname
   TEST(0 == init_logwriter(&lgwrt));
   for (uint8_t i = 0; i < log_channel__NROF; ++i) {
      uint32_t S = lgwrt.chan[i].logbuf.size;
      for (uint8_t s = 0; s < log_state__NROF; ++s) {
         setstate_logwriter(&lgwrt, i, s);
         log_header_t header = log_header_INIT("__func__", "__file__", i);
         lgwrt.chan[i].funcname = header.funcname;
         // funcname kept
         printf_logwriter(&lgwrt, i, log_flags_NONE, &header, 0);
         TEST(lgwrt.chan[i].logbuf.size    == S);
         TEST(lgwrt.chan[i].logbuf.logsize == 0);
         TEST(lgwrt.chan[i].funcname       == header.funcname);
         // log_flags_LAST ==> clears funcname
         printf_logwriter(&lgwrt, i, log_flags_LAST, &header, 0);
         TEST(lgwrt.chan[i].logbuf.size    == S);
         TEST(lgwrt.chan[i].logbuf.logsize == 0);
         TEST(lgwrt.chan[i].funcname       == (s == log_state_IGNORED ? header.funcname : 0));
      }
   }
   TEST(0 == free_logwriter(&lgwrt));

   // TEST printf_logwriter: header == format == 0
   TEST(0 == init_logwriter(&lgwrt));
   for (uint8_t i = 0; i < log_channel__NROF; ++i) {
      // write log into pipe
      logfd = lgwrt.chan[i].logbuf.io;
      lgwrt.chan[i].logbuf.io = pipefd.write;
      for (uint8_t s = 0; s < log_state__NROF; ++s) {
         setstate_logwriter(&lgwrt, i, s);
         printf_logwriter(&lgwrt, i, log_flags_LAST, 0, 0);
         // nothing is written
         TEST(lgwrt.chan[i].logbuf.logsize == 0);
         TEST(-1 == read(pipefd.read, mem.addr, mem.size));
      }
      // reset
      lgwrt.chan[i].logbuf.io = logfd;
   }
   TEST(0 == free_logwriter(&lgwrt));

   // TEST printf_logwriter: EINVAL
   TEST(0 == init_logwriter(&lgwrt));
   printf_logwriter(&lgwrt, log_channel__NROF, log_flags_NONE, 0, "123");
   for (uint8_t i = 0; i < log_channel__NROF; ++i) {
      TEST(lgwrt.chan[i].logbuf.logsize == 0);
   }
   TEST(0 == free_logwriter(&lgwrt));

   // TEST printtext_logwriter: header + text resource + truncated message
   TEST(0 == init_logwriter(&lgwrt));
   for (uint8_t i = 0; i < log_channel__NROF; ++i) {
      uint32_t S = lgwrt.chan[i].logbuf.size;
      TEST(S > log_config_MINSIZE);
      for (uint8_t s = 0; s < log_state__NROF; ++s) {
         if (s == log_state_IGNORED || s == log_state_IMMEDIATE) continue;
         setstate_logwriter(&lgwrt, i, s);
         memset(lgwrt.chan[i].logbuf.addr, '0', S);
         memset(mem.addr, 'x', log_config_MINSIZE);
         mem.addr[log_config_MINSIZE] = 0;
         lgwrt.chan[i].logbuf.logsize = S - log_config_MINSIZE;
         log_header_t header = log_header_INIT("func", "file", 100+i);
         printtext_logwriter(&lgwrt, i, log_flags_NONE, &header, textres_test, 3, (char*)mem.addr);
         // check lgwrt.chan[i].logbuf
         TEST(lgwrt.chan[i].logbuf.size == S);
         TEST(lgwrt.chan[i].logbuf.logsize == S-1);
         for (unsigned off = 0; off < S-log_config_MINSIZE; ++off) {
            TEST(lgwrt.chan[i].logbuf.addr[off] == '0');
         }
         uint8_t * start = lgwrt.chan[i].logbuf.addr+S-log_config_MINSIZE;
         uint8_t * end   = memrchr((char*) start, '\n', log_config_MINSIZE);
         TEST(0 != end);
         TEST(0 == compare_header((size_t)(1 + end - start), start, "func", "file", 100+i));
         TEST(end[1] == '3');
         TEST(end[2] == '|');
         for (uint8_t * addr = end+3; addr < lgwrt.chan[i].logbuf.addr+S-5; ++addr) {
            TEST(*addr == 'x');
         }
         TEST(0 == strcmp((char*)lgwrt.chan[i].logbuf.addr+S-5, " ..."));
      }
   }
   TEST(0 == free_logwriter(&lgwrt));

   // TEST printtext_logwriter: header + null textresource is ignored
   TEST(0 == init_logwriter(&lgwrt));
   for (uint8_t i = 0; i < log_channel__NROF; ++i) {
      uint32_t S = lgwrt.chan[i].logbuf.size;
      for (uint8_t s = 0; s < log_state__NROF; ++s) {
         if (s == log_state_IGNORED || s == log_state_IMMEDIATE) continue;
         setstate_logwriter(&lgwrt, i, s);
         lgwrt.chan[i].logbuf.logsize = 0;
         lgwrt.chan[i].funcname       = 0;
         log_header_t header = log_header_INIT("__func__", "__file__", i);
         printtext_logwriter(&lgwrt, i, log_flags_NONE, &header, 0);
         // check header written
         size_t L = lgwrt.chan[i].logbuf.logsize;
         TEST(lgwrt.chan[i].logbuf.logsize > 10);
         TEST(lgwrt.chan[i].logbuf.size    == S);
         TEST(lgwrt.chan[i].funcname       == header.funcname);
         TEST(0 == compare_header(L, lgwrt.chan[i].logbuf.addr, "__func__", "__file__", i));
         printtext_logwriter(&lgwrt, i, log_flags_NONE, &header, 0/*null textresource ignored*/);
         // check header not written
         TEST(lgwrt.chan[i].logbuf.logsize == L);
         TEST(lgwrt.chan[i].logbuf.size    == S);
         TEST(lgwrt.chan[i].funcname       == header.funcname);
      }
   }
   TEST(0 == free_logwriter(&lgwrt));

   // TEST printtext_logwriter: EINVAL
   TEST(0 == init_logwriter(&lgwrt));
   printtext_logwriter(&lgwrt, log_channel__NROF, log_flags_NONE, 0, textres_test);
   for (uint8_t i = 0; i < log_channel__NROF; ++i) {
      TEST(lgwrt.chan[i].logbuf.logsize == 0);
   }
   TEST(0 == free_logwriter(&lgwrt));

   // TEST printtext_logwriter: header == text == 0
   TEST(0 == init_logwriter(&lgwrt));
   for (uint8_t i = 0; i < log_channel__NROF; ++i) {
      // write log into pipe
      logfd = lgwrt.chan[i].logbuf.io;
      lgwrt.chan[i].logbuf.io = pipefd.write;
      for (uint8_t s = 0; s < log_state__NROF; ++s) {
         setstate_logwriter(&lgwrt, i, s);
         printtext_logwriter(&lgwrt, i, log_flags_LAST, 0, 0);
         // nothing is written
         TEST(lgwrt.chan[i].logbuf.logsize == 0);
         TEST(-1 == read(pipefd.read, mem.addr, mem.size));
      }
      // reset
      lgwrt.chan[i].logbuf.io = logfd;
   }
   TEST(0 == free_logwriter(&lgwrt));

   // unprepare
   TEST(-1 == read(pipefd.read, mem.addr, 1));   // all bytes consumed
   TEST(0 == free_pipe(&pipefd));
   TEST(0 == free_logwriter(&lgwrt));
   RELEASE_PAGECACHE(&mem);

   return 0;
ONERR:
   free_pipe(&pipefd);
   free_logwriter(&lgwrt);
   RELEASE_PAGECACHE(&mem);
   return EINVAL;
}

static int test_initthread(void)
{
   // TEST cast_logit
   TEST(cast_logit(&s_logwriter_interface, logwriter_t) == (log_it*)&s_logwriter_interface);

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
   TEST(interface_logwriter() == cast_logit(&s_logwriter_interface, logwriter_t));

   return 0;
ONERR:
   return EINVAL;
}

static int test_logmacros(void)
{
   logwriter_t *  lgwrt = (logwriter_t*) log_maincontext().object;
   int            oldfd = -1;
   int            pipefd[2] = { -1, -1 };

   // prepare
   TEST(interface_logwriter() == log_maincontext().iimpl);
   TEST(0 == pipe2(pipefd, O_CLOEXEC|O_NONBLOCK));
   oldfd = dup(lgwrt->chan[log_channel_ERR].logbuf.io);
   TEST(0 < oldfd);
   TEST(lgwrt->chan[log_channel_ERR].logbuf.io == dup2(pipefd[1], lgwrt->chan[log_channel_ERR].logbuf.io));

   // TEST GETBUFFER_LOG
   uint8_t *logbuffer = 0;
   size_t   logsize   = (size_t)-1;
   GETBUFFER_LOG(log_channel_ERR, &logbuffer, &logsize);
   TEST(logbuffer == lgwrt->chan[log_channel_ERR].logbuf.addr);
   TEST(logsize   == lgwrt->chan[log_channel_ERR].logbuf.logsize);

   // TEST COMPARE_LOG
   TEST(0 == COMPARE_LOG(log_channel_ERR, lgwrt->chan[log_channel_ERR].logbuf.logsize, lgwrt->chan[log_channel_ERR].logbuf.addr));
   TEST(EINVAL == COMPARE_LOG(log_channel_ERR, lgwrt->chan[log_channel_ERR].logbuf.logsize+1, lgwrt->chan[log_channel_ERR].logbuf.addr));

   // TEST GETSTATE_LOG
   log_state_e oldstate = lgwrt->chan[log_channel_ERR].logstate;
   lgwrt->chan[log_channel_ERR].logstate = log_state_IGNORED;
   TEST(GETSTATE_LOG(log_channel_ERR) == log_state_IGNORED);
   lgwrt->chan[log_channel_ERR].logstate = log_state_IMMEDIATE;
   TEST(GETSTATE_LOG(log_channel_ERR) == log_state_IMMEDIATE);
   lgwrt->chan[log_channel_ERR].logstate = oldstate;
   TEST(GETSTATE_LOG(log_channel_ERR) == oldstate);

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
      GETBUFFER_LOG(log_channel_ERR, &logbuffer, &logsize);
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
   lgwrt->chan[log_channel_ERR].logbuf.addr[0] = 'X';
   lgwrt->chan[log_channel_ERR].logbuf.logsize = 1;
   FLUSHBUFFER_LOG(log_channel_ERR);
   GETBUFFER_LOG(log_channel_ERR, &logbuffer, &logsize);
   TEST(0 == logsize);
   lgwrt->chan[log_channel_ERR] = oldchan;
   lgwrt->chan[log_channel_ERR].logbuf.addr[0] = oldchr;
   char chars[2] = { 0 };
   TEST(1   == read(pipefd[0], chars, 2));
   TEST('X' == chars[0]);

   // TEST SETSTATE_LOG
   oldstate = lgwrt->chan[log_channel_ERR].logstate;
   SETSTATE_LOG(log_channel_ERR, log_state_IGNORED);
   TEST(GETSTATE_LOG(log_channel_ERR) == log_state_IGNORED);
   SETSTATE_LOG(log_channel_ERR, log_state_IMMEDIATE);
   TEST(GETSTATE_LOG(log_channel_ERR) == log_state_IMMEDIATE);
   SETSTATE_LOG(log_channel_ERR, oldstate);
   TEST(GETSTATE_LOG(log_channel_ERR) == oldstate);

   // TEST == group: log-text ==
   // already tested with written log output

   // unprepare
   TEST(lgwrt->chan[log_channel_ERR].logbuf.io == dup2(oldfd, lgwrt->chan[log_channel_ERR].logbuf.io));
   TEST(0 == close(pipefd[0]));
   TEST(0 == close(pipefd[1]));
   TEST(0 == close(oldfd));

   return 0;
ONERR:
   if (oldfd >= 0) dup2(oldfd, lgwrt->chan[log_channel_ERR].logbuf.io);
   close(pipefd[0]);
   close(pipefd[1]);
   close(oldfd);
   return EINVAL;
}

static int test_errlogmacros(void)
{
   logwriter_t *  lgwrt = (logwriter_t*) log_maincontext().object;
   int            oldfd = -1;
   int            pipefd[2] = { -1, -1 };

   // prepare
   TEST(interface_logwriter() == log_maincontext().iimpl);
   TEST(0 == pipe2(pipefd, O_CLOEXEC|O_NONBLOCK));
   oldfd = dup(lgwrt->chan[log_channel_ERR].logbuf.io);
   TEST(0 < oldfd);
   TEST(lgwrt->chan[log_channel_ERR].logbuf.io == dup2(pipefd[1], lgwrt->chan[log_channel_ERR].logbuf.io));

   // TEST GETBUFFER_ERRLOG
   uint8_t *logbuffer = 0;
   size_t   logsize   = (size_t)-1;
   GETBUFFER_ERRLOG(&logbuffer, &logsize);
   TEST(logbuffer == lgwrt->chan[log_channel_ERR].logbuf.addr);
   TEST(logsize   == lgwrt->chan[log_channel_ERR].logbuf.logsize);

   // TEST COMPARE_ERRLOG
   TEST(0 == COMPARE_ERRLOG(lgwrt->chan[log_channel_ERR].logbuf.logsize, lgwrt->chan[log_channel_ERR].logbuf.addr));
   TEST(EINVAL == COMPARE_ERRLOG(lgwrt->chan[log_channel_ERR].logbuf.logsize+1, lgwrt->chan[log_channel_ERR].logbuf.addr));

   // TEST TRUNCATEBUFFER_ERRLOG
   logwriter_chan_t oldchan = lgwrt->chan[log_channel_ERR];
   for (unsigned i = 0; i < 127; ++i) {
      uint8_t buffer[128];
      memset(buffer, 'a', sizeof(buffer));
      lgwrt->chan[log_channel_ERR].logbuf.addr = buffer;
      lgwrt->chan[log_channel_ERR].logbuf.size = sizeof(buffer);
      lgwrt->chan[log_channel_ERR].logbuf.logsize = sizeof(buffer)-1;
      TRUNCATEBUFFER_ERRLOG(i+sizeof(buffer));
      TEST(0 == memchr(buffer, 0, sizeof(buffer)));
      GETBUFFER_ERRLOG(&logbuffer, &logsize);
      TEST(buffer == logbuffer);
      TEST(sizeof(buffer)-1 == logsize);
      TRUNCATEBUFFER_ERRLOG(i);
      GETBUFFER_ERRLOG(&logbuffer, &logsize);
      TEST(buffer == logbuffer);
      TEST(i == logsize);
      TEST(0 == buffer[i]);
      lgwrt->chan[log_channel_ERR] = oldchan;
   }

   // TEST FLUSHBUFFER_ERRLOG
   uint8_t oldchr = lgwrt->chan[log_channel_ERR].logbuf.addr[0];
   lgwrt->chan[log_channel_ERR].logbuf.addr[0] = 'X';
   lgwrt->chan[log_channel_ERR].logbuf.logsize = 1;
   FLUSHBUFFER_ERRLOG();
   GETBUFFER_ERRLOG(&logbuffer, &logsize);
   TEST(0 == logsize);
   lgwrt->chan[log_channel_ERR] = oldchan;
   lgwrt->chan[log_channel_ERR].logbuf.addr[0] = oldchr;
   char chars[2] = { 0 };
   TEST(1   == read(pipefd[0], chars, 2));
   TEST('X' == chars[0]);

   // TEST == group: log-text ==
   // already tested with written log output

   // unprepare
   TEST(lgwrt->chan[log_channel_ERR].logbuf.io == dup2(oldfd, lgwrt->chan[log_channel_ERR].logbuf.io));
   TEST(0 == close(pipefd[0]));
   TEST(0 == close(pipefd[1]));
   TEST(0 == close(oldfd));

   return 0;
ONERR:
   if (oldfd >= 0) dup2(oldfd, lgwrt->chan[log_channel_ERR].logbuf.io);
   close(pipefd[0]);
   close(pipefd[1]);
   close(oldfd);
   return EINVAL;
}

int unittest_io_writer_log_logwriter()
{
   if (test_initfree())       goto ONERR;
   if (test_query())          goto ONERR;
   if (test_config())         goto ONERR;
   if (test_write())          goto ONERR;
   if (test_initthread())     goto ONERR;
   if (test_logmacros())      goto ONERR;
   if (test_errlogmacros())   goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
