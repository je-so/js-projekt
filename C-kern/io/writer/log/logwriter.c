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

static uint8_t       s_logwriter_sharedbuffer[log_config_MINSIZE];


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

   if (! PROCESS_testerrortimer(&s_logwriter_errtimer, &err)) {
      err = ALLOC_PAGECACHE(pagesize_16384, buffer);
   }
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
   (void) PROCESS_testerrortimer(&s_logwriter_errtimer, &err);
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
      log_state_e logstate = (channel == log_channel_USERERR || bufsize == 0 ? log_state_IGNORED : log_state_BUFFERED);
      lgwrt->chan[channel] = (logwriter_chan_t) logwriter_chan_INIT(bufsize, lgwrt->addr + offset, iochannel_STDERR, logstate);
      offset += logsize;
   }
}

static void freechan_logwriter(logwriter_t * lgwrt)
{
   for (uint8_t channel = 0; channel < log_channel__NROF; ++channel) {
      // calling free is currently not necessary
      lgwrt->chan[channel] = (logwriter_chan_t) logwriter_chan_FREE;
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
   if (bufsize < log_config_MINSIZE) goto ONERR;

   *cast_memblock(lgwrt,) = (memblock_t) memblock_INIT(bufsize, logbuf);

   initchan_logwriter(lgwrt, bufsize >= minbufsize_logwriter() ? log_config_MINSIZE : 0);

   return 0;
ONERR:
   TRACE_LOG(AUTO, log_channel_ERR, log_flags_LAST, FUNCTION_EXIT_ERRLOG, EINVAL);
   return EINVAL;
}

void initshared_logwriter(/*out*/logwriter_t * lgwrt)
{
   (void) initstatic_logwriter(lgwrt, sizeof(s_logwriter_sharedbuffer), s_logwriter_sharedbuffer);
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

void freestatic_logwriter(logwriter_t * lgwrt)
{
   freechan_logwriter(lgwrt);

   *cast_memblock(lgwrt,) = (memblock_t) memblock_FREE;
}


// group: query

bool isfree_logwriter(const logwriter_t * lgwrt)
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

#define CHECK_CHANNEL(channel) \
   if (! (channel < log_channel__NROF)) { \
      err = EINVAL;                       \
      TRACE_LOG(AUTO,                     \
         log_channel_ERR, log_flags_NONE, TEST_INPARAM_FALSE_ERRLOG, "channel < log_channel__NROF"); \
      goto ONERR;                         \
   }

void getbuffer_logwriter(const logwriter_t * lgwrt, uint8_t channel, /*out*/uint8_t ** buffer, /*out*/size_t * size)
{
   int err;

   CHECK_CHANNEL(channel);

   getbuffer_logbuffer(&lgwrt->chan[channel].logbuf, buffer, size);

   return;
ONERR:
   TRACE_LOG(AUTO, log_channel_ERR, log_flags_LAST, FUNCTION_EXIT_ERRLOG, err);
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

   CHECK_CHANNEL(channel);

   return compare_logbuffer(&lgwrt->chan[channel].logbuf, logsize, logbuffer);
ONERR:
   TRACE_LOG(AUTO, log_channel_ERR, log_flags_LAST, FUNCTION_EXIT_ERRLOG, err);
   return err;
}

// group: config

void setstate_logwriter(logwriter_t * lgwrt, uint8_t channel, uint8_t logstate)
{
   if (channel < log_channel__NROF && logstate < log_state__NROF) {
      logwriter_chan_t * chan = &lgwrt->chan[channel];
      chan->funcname = 0;
      chan->logstate = logstate;
   }
}

// group: change

void truncatebuffer_logwriter(logwriter_t * lgwrt, uint8_t channel, size_t size)
{
   int err;

   CHECK_CHANNEL(channel);

   logwriter_chan_t * chan = &lgwrt->chan[channel];

   truncate_logbuffer(&chan->logbuf, size);

   return;
ONERR:
   TRACE_LOG(AUTO, log_channel_ERR, log_flags_LAST, FUNCTION_EXIT_ERRLOG, err);
   return;
}

void flushbuffer_logwriter(logwriter_t * lgwrt, uint8_t channel)
{
   int err;

   CHECK_CHANNEL(channel);

   logwriter_chan_t * chan = &lgwrt->chan[channel];

   flush_logwriterchan(chan);

   return;
ONERR:
   TRACE_LOG(AUTO, log_channel_ERR, log_flags_LAST, FUNCTION_EXIT_ERRLOG, err);
   return;
}

#define GETCHANNEL_logwriter(          \
            /*logwriter_t* */lgwrt,    \
            /*log_channel_e*/channel,  \
            /*out*/chan)               \
         CHECK_CHANNEL(channel);       \
         chan = &lgwrt->chan[channel]; \
         if (chan->logstate == log_state_IGNORED) return;

static inline void beginwrite_logwriter(
   logwriter_chan_t *   chan,
   uint8_t              flags,
   const log_header_t * header)
{
   if (header && chan->funcname != header->funcname) {
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
   TRACE_LOG(AUTO, log_channel_ERR, log_flags_LAST, FUNCTION_EXIT_ERRLOG, err);
   return;
}

void printf_logwriter(logwriter_t * lgwrt, uint8_t channel, uint8_t flags, const struct log_header_t * header, const char * format, ...)
{
   va_list args;
   va_start(args, format);
   vprintf_logwriter(lgwrt, channel, flags, header, format, args);
   va_end(args);
}

void printtext_logwriter(logwriter_t * lgwrt, uint8_t channel, uint8_t flags, const struct log_header_t * header, log_text_f textf, void * params)
{
   int err;
   logwriter_chan_t * chan;

   GETCHANNEL_logwriter(lgwrt, channel, /*out*/chan);
   beginwrite_logwriter(chan, flags, header);

   if (textf) {
      textf(&chan->logbuf, params);
   }

   endwrite_logwriter(chan, flags);

   return;
ONERR:
   TRACE_LOG(AUTO, log_channel_ERR, log_flags_LAST, FUNCTION_EXIT_ERRLOG, err);
   return;
}

// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   logwriter_t lgwrt = logwriter_FREE;
   uint8_t     logbuf[minbufsize_logwriter()];
   pipe_t      pipe = pipe_FREE;
   int         oldfd = -1;
   maincontext_e oldtype = g_maincontext.type;

   // prepare0
   TEST(0 == init_pipe(&pipe));
   oldfd = dup(STDERR_FILENO);
   TEST(0 < oldfd);
   TEST(STDERR_FILENO == dup2(pipe.write, STDERR_FILENO));

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
   for (int tc = 0; tc < 2; ++tc) {
      TEST(0 == free_logwriter(&lgwrt));
      TEST(1 == isfree_logwriter(&lgwrt));
      TEST(1 == isvalid_iochannel(iochannel_STDOUT));
      TEST(1 == isvalid_iochannel(iochannel_STDERR));
   }

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

   // TEST freestatic_logwriter: double free
   for (int tc = 0; tc < 2; ++tc) {
      freestatic_logwriter(&lgwrt);
      TEST(1 == isfree_logwriter(&lgwrt));
   }

   // TEST initstatic_logwriter: log_config_MINSIZE <= buffersize < minbufsize_logwriter()
   for (size_t bs = log_config_MINSIZE; bs < sizeof(logbuf); bs += log_config_MINSIZE) {
      TEST(0 == initstatic_logwriter(&lgwrt, bs, logbuf));
      TEST(lgwrt.addr == logbuf);
      TEST(lgwrt.size == bs);
      for (unsigned i = 0, offset = 0; i < log_channel__NROF; ++i) {
         int isErr = (i == log_channel_ERR);
         TEST(lgwrt.chan[i].logbuf.addr == lgwrt.addr + offset);
         TEST(lgwrt.chan[i].logbuf.size == (isErr ? bs : 0));
         TEST(lgwrt.chan[i].logbuf.io   == iochannel_STDERR);
         TEST(lgwrt.chan[i].logbuf.logsize == 0);
         TEST(lgwrt.chan[i].logstate    == (isErr ? log_state_BUFFERED : log_state_IGNORED));
         offset += lgwrt.chan[i].logbuf.size;
      }
   }
   freestatic_logwriter(&lgwrt);
   TEST(1 == isfree_logwriter(&lgwrt));

   // TEST initshared_logwriter
   initshared_logwriter(&lgwrt);
   TEST(lgwrt.addr == s_logwriter_sharedbuffer);
   TEST(lgwrt.size == sizeof(s_logwriter_sharedbuffer));
   for (unsigned i = 0, offset = 0; i < log_channel__NROF; ++i) {
      int isErr = (i == log_channel_ERR);
      TEST(lgwrt.chan[i].logbuf.addr == lgwrt.addr + offset);
      TEST(lgwrt.chan[i].logbuf.size == (isErr ? lgwrt.size : 0));
      TEST(lgwrt.chan[i].logbuf.io   == iochannel_STDERR);
      TEST(lgwrt.chan[i].logbuf.logsize == 0);
      TEST(lgwrt.chan[i].logstate    == (isErr ? log_state_BUFFERED : log_state_IGNORED));
      offset += lgwrt.chan[i].logbuf.size;
   }

   // TEST freeshared_logwriter
   for (int tc = 0; tc < 2; ++tc) {
      freeshared_logwriter(&lgwrt);
      TEST(1 == isfree_logwriter(&lgwrt));
   }

   // TEST initstatic_logwriter: EINVAL (maincontext valid)
   uint8_t * logbuffer;
   size_t    logsize;
   GETBUFFER_ERRLOG(&logbuffer, &logsize);
   TEST(EINVAL == initstatic_logwriter(&lgwrt, log_config_MINSIZE-1, logbuf));
   // check lgwrt
   TEST(1 == isfree_logwriter(&lgwrt));
   // check ERRLOG written
   uint8_t * logbuffer2;
   size_t    logsize2;
   GETBUFFER_ERRLOG(&logbuffer2, &logsize2);
   TEST(logbuffer == logbuffer2);
   TEST(logsize <  logsize2);

   // TEST initstatic_logwriter: EINVAL (maincontext invalid)
   g_maincontext.type = maincontext_STATIC;
   TEST(-1 == read(pipe.read, logbuf, sizeof(logbuf)));
   TEST(EINVAL == initstatic_logwriter(&lgwrt, log_config_MINSIZE-1, logbuf));
   // check lgwrt
   TEST(1 == isfree_logwriter(&lgwrt));
   // check INITLOG written
   ssize_t bytes = read(pipe.read, logbuf, sizeof(logbuf));
   TEST(bytes > 0);
   TEST(-1 == read(pipe.read, logbuf, sizeof(logbuf)));
   PRINTF_ERRLOG("%.*s", (int)bytes, logbuf);
   // reset
   g_maincontext.type = oldtype;

   // reset0
   TEST(0 == free_pipe(&pipe));
   TEST(STDERR_FILENO == dup2(oldfd, STDERR_FILENO));
   TEST(0 == close(oldfd));

   return 0;
ONERR:
   g_maincontext.type = oldtype;
   if (oldfd > 0) {
      dup2(oldfd, STDERR_FILENO);
      close(oldfd);
   }
   free_logwriter(&lgwrt);
   free_pipe(&pipe);
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

   // TEST compare_logwriter
   for (uint8_t i = 0; i < log_channel__NROF; ++i) {
      truncate_logbuffer(&lgwrt.chan[i].logbuf, 0);
      printf_logbuffer(&lgwrt.chan[i].logbuf, "[1: XXX]\ntest\n");
      TEST(0 == compare_logwriter(&lgwrt, i, 14, (const uint8_t*)"[1: XXX]\ntest\n"));
      TEST(0 == compare_logwriter(&lgwrt, i, 14, (const uint8_t*)"[1: YYY]\ntest\n"));
      TEST(EINVAL == compare_logwriter(&lgwrt, i, 13, (const uint8_t*)"[1: XXX]\ntest\n"));
      TEST(EINVAL == compare_logwriter(&lgwrt, i, 14, (const uint8_t*)"[1: XXX]\ntesT\n"));
   }

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

   // TEST setstate_logwriter: logstate out of range
   for (uint8_t i = 0; i < log_channel__NROF; ++i) {
      // test log_state__NROF
      setstate_logwriter(&lgwrt, i, log_state__NROF);
      // check ignored
      TEST(log_state__NROF-1 == lgwrt.chan[i].logstate);
      // test -1
      setstate_logwriter(&lgwrt, i, (uint8_t)-1);
      // check ignored
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

struct p_textres_test { int i; const char* str; };
static void textres_test(logbuffer_t * logbuf, void * _p)
{
   struct p_textres_test * p = _p;
   printf_logbuffer(logbuf, "%d|%s", p->i, p->str);
}

typedef void p_noarg_textres_noarg_test;
static void textres_noarg_test(logbuffer_t * logbuf, void * _p)
{
   (void) _p;
   printf_logbuffer(logbuf, "12345");
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
         struct p_textres_test params = { 3, (char*)mem.addr };
         printtext_logwriter(&lgwrt, i, log_flags_NONE, &header, textres_test, &params);
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
         printtext_logwriter(&lgwrt, i, log_flags_NONE, &header, 0, 0);
         // check header written
         size_t L = lgwrt.chan[i].logbuf.logsize;
         TEST(lgwrt.chan[i].logbuf.logsize > 10);
         TEST(lgwrt.chan[i].logbuf.size    == S);
         TEST(lgwrt.chan[i].funcname       == header.funcname);
         TEST(0 == compare_header(L, lgwrt.chan[i].logbuf.addr, "__func__", "__file__", i));
         printtext_logwriter(&lgwrt, i, log_flags_NONE, &header, 0/*null textresource ignored*/, 0);
         // check header not written
         TEST(lgwrt.chan[i].logbuf.logsize == L);
         TEST(lgwrt.chan[i].logbuf.size    == S);
         TEST(lgwrt.chan[i].funcname       == header.funcname);
      }
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
         printtext_logwriter(&lgwrt, i, log_flags_LAST, 0, 0, 0);
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
   logwriter_t    oldlog = *lgwrt;
   uint8_t        buffer[128];
   logwriter_chan_t newchan = logwriter_chan_INIT(sizeof(buffer), buffer, STDERR_FILENO, log_state_BUFFERED);
   log_header_t   header = log_header_INIT(__FUNCTION__, __FILE__, __LINE__);

   // prepare
   TEST(interface_logwriter() == log_maincontext().iimpl);
   TEST(0 == pipe2(pipefd, O_CLOEXEC|O_NONBLOCK));
   oldfd = dup(STDERR_FILENO);
   TEST(0 < oldfd);
   TEST(STDERR_FILENO == dup2(pipefd[1], STDERR_FILENO));

   // TEST GETBUFFER_LOG
   for (uint8_t c = 0; c < log_channel__NROF; ++c) {
      uint8_t* logbuffer = 0;
      size_t   logsize   = 0;
      logwriter_chan_t oldchan = lgwrt->chan[c];
      lgwrt->chan[c] = newchan;
      lgwrt->chan[c].logbuf.logsize = sizeof(buffer)-1;
      GETBUFFER_LOG(, c, &logbuffer, &logsize);
      TEST(logbuffer == buffer);
      TEST(logsize   == sizeof(buffer)-1);
      lgwrt->chan[c] = oldchan;
   }

   // TEST COMPARE_LOG
   for (uint8_t c = 0; c < log_channel__NROF; ++c) {
      logwriter_chan_t oldchan = lgwrt->chan[c];
      lgwrt->chan[c] = newchan;
      lgwrt->chan[c].logbuf.logsize = sizeof(buffer)-1;
      TEST(0 == COMPARE_LOG(, c, lgwrt->chan[c].logbuf.logsize, lgwrt->chan[c].logbuf.addr));
      TEST(EINVAL == COMPARE_LOG(, c, lgwrt->chan[c].logbuf.logsize+1, lgwrt->chan[c].logbuf.addr));
      lgwrt->chan[c] = oldchan;
   }

   // TEST GETSTATE_LOG
   for (uint8_t c = 0; c < log_channel__NROF; ++c) {
      log_state_e oldstate = lgwrt->chan[c].logstate;
      for (uint8_t s = 0; s < log_state__NROF; ++s) {
         lgwrt->chan[c].logstate = s;
         TEST(s == GETSTATE_LOG(,c));
      }
      lgwrt->chan[c].logstate = oldstate;
   }

   // TEST SETSTATE_LOG
   for (uint8_t c = 0; c < log_channel__NROF; ++c) {
      log_state_e oldstate = lgwrt->chan[c].logstate;
      for (uint8_t s = 0; s < log_state__NROF; ++s) {
         SETSTATE_LOG(, c, s);
         TEST(s == lgwrt->chan[c].logstate);
      }
      lgwrt->chan[c].logstate = oldstate;
   }

   // TEST TRUNCATEBUFFER_LOG
   for (uint8_t c = 0; c < log_channel__NROF; ++c) {
      logwriter_chan_t oldchan = lgwrt->chan[c];
      for (unsigned i = 0; i < 127; ++i) {
         memset(buffer, 'a', sizeof(buffer));
         lgwrt->chan[c] = newchan;
         lgwrt->chan[c].logbuf.logsize = sizeof(buffer)-1;
         TRUNCATEBUFFER_LOG(, c, i+sizeof(buffer));
         TEST(0 == memchr(buffer, 0, sizeof(buffer)));
         TEST(sizeof(buffer)-1 == lgwrt->chan[c].logbuf.logsize);
         TRUNCATEBUFFER_LOG(, c, i);
         TEST(i == lgwrt->chan[c].logbuf.logsize);
         TEST(0 == buffer[i]);
      }
      lgwrt->chan[c] = oldchan;
   }

   // TEST FLUSHBUFFER_LOG
   for (uint8_t c = 0; c < log_channel__NROF; ++c) {
      logwriter_chan_t oldchan = lgwrt->chan[c];
      lgwrt->chan[c] = newchan;
      lgwrt->chan[c].logbuf.logsize = 1;
      buffer[0] = 'X';
      FLUSHBUFFER_LOG(, c);
      TEST(0 == lgwrt->chan[c].logbuf.logsize);
      char chars[2] = { 0 };
      TEST(1   == read(pipefd[0], chars, 2));
      TEST('X' == chars[0]);
      lgwrt->chan[c] = oldchan;
   }

   // TEST == group: log-text ==

   // TEST PRINTF_LOG
   for (uint8_t c = 0; c < log_channel__NROF; ++c) {
      logwriter_chan_t oldchan = lgwrt->chan[c];
      lgwrt->chan[c] = newchan;
      memset(buffer, 0, sizeof(buffer));
      PRINTF_LOG(, c, log_flags_NONE, &header, "%zd-%s\n", (size_t)1, "2");
      TEST(4 < lgwrt->chan[c].logbuf.logsize);
      TEST(0 == memcmp(buffer + lgwrt->chan[c].logbuf.logsize - 4, "1-2\n", 5));
      TEST(0 == compare_header(lgwrt->chan[c].logbuf.logsize - 4, buffer, header.funcname, header.filename, header.linenr));
      lgwrt->chan[c] = oldchan;
   }

   // TEST PRINTTEXT_LOG
   for (uint8_t c = 0; c < log_channel__NROF; ++c) {
      logwriter_chan_t oldchan = lgwrt->chan[c];
      lgwrt->chan[c] = newchan;
      memset(buffer, 0, sizeof(buffer));
      PRINTTEXT_LOG(, c, log_flags_NONE, &header, textres_test, 3, "30");
      TEST(4 < lgwrt->chan[c].logbuf.logsize);
      TEST(0 == memcmp(buffer + lgwrt->chan[c].logbuf.logsize - 4, "3|30", 5));
      TEST(0 == compare_header(lgwrt->chan[c].logbuf.logsize - 4, buffer, header.funcname, header.filename, header.linenr));
      lgwrt->chan[c] = oldchan;
   }

   // TEST PRINTTEXT_NOARG_LOG
   for (uint8_t c = 0; c < log_channel__NROF; ++c) {
      logwriter_chan_t oldchan = lgwrt->chan[c];
      lgwrt->chan[c] = newchan;
      memset(buffer, 0, sizeof(buffer));
      PRINTTEXT_NOARG_LOG(, c, log_flags_NONE, &header, textres_noarg_test);
      TEST(5 < lgwrt->chan[c].logbuf.logsize);
      TEST(0 == memcmp(buffer + lgwrt->chan[c].logbuf.logsize - 5, "12345", 6));
      TEST(0 == compare_header(lgwrt->chan[c].logbuf.logsize - 5, buffer, header.funcname, header.filename, header.linenr));
      lgwrt->chan[c] = oldchan;
   }

   // TEST TRACE_LOG
   for (uint8_t c = 0; c < log_channel__NROF; ++c) {
      logwriter_chan_t oldchan = lgwrt->chan[c];
      lgwrt->chan[c] = newchan;
      memset(buffer, 0, sizeof(buffer));
      TRACE_LOG(, c, log_flags_NONE, textres_test, 1, "23");
      TEST(4 < lgwrt->chan[c].logbuf.logsize);
      TEST(0 == memcmp(buffer + lgwrt->chan[c].logbuf.logsize - 4, "1|23", 5));
      TEST(0 == compare_header(lgwrt->chan[c].logbuf.logsize - 4, buffer, header.funcname, header.filename, __LINE__ - 3));
      lgwrt->chan[c] = oldchan;
   }

   // TEST TRACE2_LOG
   for (uint8_t c = 0; c < log_channel__NROF; ++c) {
      logwriter_chan_t oldchan = lgwrt->chan[c];
      lgwrt->chan[c] = newchan;
      memset(buffer, 0, sizeof(buffer));
      TRACE2_LOG(, c, log_flags_NONE, textres_test, "C", "F", 99, 1, "23");
      TEST(4 < lgwrt->chan[c].logbuf.logsize);
      TEST(0 == memcmp(buffer + lgwrt->chan[c].logbuf.logsize - 4, "1|23", 5));
      TEST(0 == compare_header(lgwrt->chan[c].logbuf.logsize - 4, buffer, "C", "F", 99));
      lgwrt->chan[c] = oldchan;
   }

   // TEST TRACE_NOARG_LOG
   for (uint8_t c = 0; c < log_channel__NROF; ++c) {
      logwriter_chan_t oldchan = lgwrt->chan[c];
      lgwrt->chan[c] = newchan;
      memset(buffer, 0, sizeof(buffer));
      TRACE_NOARG_LOG(, c, log_flags_NONE, textres_noarg_test);
      TEST(5 < lgwrt->chan[c].logbuf.logsize);
      TEST(0 == memcmp(buffer + lgwrt->chan[c].logbuf.logsize - 5, "12345", 6));
      TEST(0 == compare_header(lgwrt->chan[c].logbuf.logsize - 5, buffer, header.funcname, header.filename, __LINE__ - 3));
      lgwrt->chan[c] = oldchan;
   }

   // unprepare
   TEST(-1 == read(pipefd[0], buffer, sizeof(buffer)));
   TEST(STDERR_FILENO == dup2(oldfd, STDERR_FILENO));
   TEST(0 == close(pipefd[0]));
   TEST(0 == close(pipefd[1]));
   TEST(0 == close(oldfd));

   return 0;
ONERR:
   *lgwrt = oldlog;
   if (oldfd >= 0) dup2(oldfd, STDERR_FILENO);
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

static int test_initlogmacros(void)
{
   logwriter_t *  lgwrt = self_maincontext()->initlog;
   int            oldfd = -1;
   int            pipefd[2] = { -1, -1 };
   uint8_t        buffer[128];
   logwriter_chan_t oldchan = lgwrt->chan[log_channel_ERR];
   logwriter_chan_t newchan = logwriter_chan_INIT(sizeof(buffer), buffer, STDERR_FILENO, log_state_BUFFERED);
   log_header_t   header = log_header_INIT(__FUNCTION__, __FILE__, __LINE__);

   // prepare
   TEST(interface_logwriter() == log_maincontext().iimpl);
   TEST(0 == pipe2(pipefd, O_CLOEXEC|O_NONBLOCK));
   oldfd = dup(STDERR_FILENO);
   TEST(0 < oldfd);
   TEST(STDERR_FILENO == dup2(pipefd[1], STDERR_FILENO));

   // TEST GETBUFFER_LOG(INIT
   uint8_t *logbuffer = 0;
   size_t   logsize   = (size_t)-1;
   GETBUFFER_LOG(INIT, log_channel_ERR, &logbuffer, &logsize);
   TEST(logbuffer == lgwrt->chan[log_channel_ERR].logbuf.addr);
   TEST(logsize   == lgwrt->chan[log_channel_ERR].logbuf.logsize);

   // TEST COMPARE_LOG(INIT
   TEST(0 == COMPARE_LOG(INIT, log_channel_ERR, lgwrt->chan[log_channel_ERR].logbuf.logsize, lgwrt->chan[log_channel_ERR].logbuf.addr));
   TEST(EINVAL == COMPARE_LOG(INIT, log_channel_ERR, lgwrt->chan[log_channel_ERR].logbuf.logsize+1, lgwrt->chan[log_channel_ERR].logbuf.addr));

   // TEST GETSTATE_LOG(INIT
   for (uint8_t s = 0; s < log_state__NROF; ++s) {
      lgwrt->chan[log_channel_ERR].logstate = s;
      TEST(s == GETSTATE_LOG(INIT, log_channel_ERR));
   }

   // TEST SETSTATE_LOG(INIT
   for (uint8_t s = 0; s < log_state__NROF; ++s) {
      SETSTATE_LOG(INIT, log_channel_ERR, s);
      TEST(s == lgwrt->chan[log_channel_ERR].logstate);
   }

   // TEST TRUNCATEBUFFER_LOG(INIT
   lgwrt->chan[log_channel_ERR] = newchan;
   for (unsigned i = 0; i < 127; ++i) {
      memset(buffer, 'a', sizeof(buffer));
      lgwrt->chan[log_channel_ERR].logbuf.logsize = sizeof(buffer)-1;
      TRUNCATEBUFFER_LOG(INIT, log_channel_ERR, i+sizeof(buffer));
      TEST(0 == memchr(buffer, 0, sizeof(buffer)));
      GETBUFFER_LOG(INIT, log_channel_ERR, &logbuffer, &logsize);
      TEST(buffer == logbuffer);
      TEST(sizeof(buffer)-1 == logsize);
      TRUNCATEBUFFER_LOG(INIT, log_channel_ERR, i);
      GETBUFFER_LOG(INIT, log_channel_ERR, &logbuffer, &logsize);
      TEST(buffer == logbuffer);
      TEST(i == logsize);
      TEST(0 == buffer[i]);
   }

   // TEST FLUSHBUFFER_LOG(INIT
   lgwrt->chan[log_channel_ERR] = newchan;
   lgwrt->chan[log_channel_ERR].logbuf.logsize = 1;
   buffer[0] = 'X';
   FLUSHBUFFER_LOG(INIT, log_channel_ERR);
   TEST(0 == lgwrt->chan[log_channel_ERR].logbuf.logsize);
   char chars[2] = { 0 };
   TEST(1   == read(pipefd[0], chars, 2));
   TEST('X' == chars[0]);

   // TEST == group: log-text ==

   // TEST PRINTF_LOG(INIT,
   lgwrt->chan[log_channel_ERR] = newchan;
   PRINTF_LOG(INIT, log_channel_ERR, log_flags_NONE, &header, "%d%s\n", 1, "2");
   TEST(3 < lgwrt->chan[log_channel_ERR].logbuf.logsize);
   TEST(0 == memcmp(buffer + lgwrt->chan[log_channel_ERR].logbuf.logsize - 3, "12\n", 4));
   TEST(0 == compare_header(lgwrt->chan[log_channel_ERR].logbuf.logsize - 3, buffer, header.funcname, header.filename, header.linenr));

   // TEST PRINTTEXT_LOG(INIT,
   lgwrt->chan[log_channel_ERR] = newchan;
   PRINTTEXT_LOG(INIT, log_channel_ERR, log_flags_NONE, &header, textres_test, 3, "30");
   TEST(4 < lgwrt->chan[log_channel_ERR].logbuf.logsize);
   TEST(0 == memcmp(buffer + lgwrt->chan[log_channel_ERR].logbuf.logsize - 4, "3|30", 5));
   TEST(0 == compare_header(lgwrt->chan[log_channel_ERR].logbuf.logsize - 4, buffer, header.funcname, header.filename, header.linenr));

   // TEST PRINTTEXT_NOARG_LOG(INIT,
   lgwrt->chan[log_channel_ERR] = newchan;
   PRINTTEXT_NOARG_LOG(INIT, log_channel_ERR, log_flags_NONE, &header, textres_noarg_test);
   TEST(5 < lgwrt->chan[log_channel_ERR].logbuf.logsize);
   TEST(0 == memcmp(buffer + lgwrt->chan[log_channel_ERR].logbuf.logsize - 5, "12345", 6));
   TEST(0 == compare_header(lgwrt->chan[log_channel_ERR].logbuf.logsize - 5, buffer, header.funcname, header.filename, header.linenr));

   // TEST TRACE_LOG(INIT
   lgwrt->chan[log_channel_ERR] = newchan;
   memset(buffer, 0, sizeof(buffer));
   TRACE_LOG(INIT, log_channel_ERR, log_flags_NONE, textres_test, 1, "23");
   TEST(4 < lgwrt->chan[log_channel_ERR].logbuf.logsize);
   TEST(0 == memcmp(buffer + lgwrt->chan[log_channel_ERR].logbuf.logsize - 4, "1|23", 5));
   TEST(0 == compare_header(lgwrt->chan[log_channel_ERR].logbuf.logsize - 4, buffer, header.funcname, header.filename, __LINE__ - 3));

   // TEST TRACE2_LOG(INIT
   lgwrt->chan[log_channel_ERR] = newchan;
   memset(buffer, 0, sizeof(buffer));
   TRACE2_LOG(INIT, log_channel_ERR, log_flags_NONE, textres_test, "C", "F", 99, 1, "23");
   TEST(4 < lgwrt->chan[log_channel_ERR].logbuf.logsize);
   TEST(0 == memcmp(buffer + lgwrt->chan[log_channel_ERR].logbuf.logsize - 4, "1|23", 5));
   TEST(0 == compare_header(lgwrt->chan[log_channel_ERR].logbuf.logsize - 4, buffer, "C", "F", 99));

   // TEST TRACE_NOARG_LOG(INIT
   lgwrt->chan[log_channel_ERR] = newchan;
   memset(buffer, 0, sizeof(buffer));
   TRACE_NOARG_LOG(INIT, log_channel_ERR, log_flags_NONE, textres_noarg_test);
   TEST(5 < lgwrt->chan[log_channel_ERR].logbuf.logsize);
   TEST(0 == memcmp(buffer + lgwrt->chan[log_channel_ERR].logbuf.logsize - 5, "12345", 6));
   TEST(0 == compare_header(lgwrt->chan[log_channel_ERR].logbuf.logsize - 5, buffer, header.funcname, header.filename, __LINE__ - 3));

   // unprepare
   TEST(-1 == read(pipefd[0], chars, sizeof(char)));
   lgwrt->chan[log_channel_ERR] = oldchan;
   TEST(STDERR_FILENO == dup2(oldfd, STDERR_FILENO));
   TEST(0 == close(pipefd[0]));
   TEST(0 == close(pipefd[1]));
   TEST(0 == close(oldfd));

   return 0;
ONERR:
   lgwrt->chan[log_channel_ERR] = oldchan;
   if (oldfd >= 0) dup2(oldfd, STDERR_FILENO);
   close(pipefd[0]);
   close(pipefd[1]);
   close(oldfd);
   return EINVAL;
}

static int test_autologmacros(void)
{
   logwriter_t *  default_lgwrt = (void*) log_maincontext().object;
   logwriter_t *  init_lgwrt = self_maincontext()->initlog;
   maincontext_e  oldtype = g_maincontext.type;
   int            oldfd = -1;
   pipe_t         pipe = pipe_FREE;
   uint8_t        buffer[128];
   log_header_t   header = log_header_INIT(__FUNCTION__, __FILE__, __LINE__);
   uint8_t *      logbuffer;
   size_t         logsize;

   // prepare0
   TEST(interface_logwriter() == log_maincontext().iimpl);
   TEST(0 == init_pipe(&pipe));
   oldfd = dup(STDERR_FILENO);
   TEST(0 < oldfd);
   TEST(STDERR_FILENO == dup2(pipe.write, STDERR_FILENO));

   // === test switching ===

   for (int tc = 0; tc < 2; ++tc) {

      // prepare
      logwriter_t * const lgwrt = (tc == 0 ? default_lgwrt : init_lgwrt);
      if (lgwrt == init_lgwrt) {
         // simulate uninitialized maincontext => AUTO uses INIT log
         g_maincontext.type = maincontext_STATIC;
      }

      // TEST GETBUFFER_LOG(AUTO
      for (uint8_t c = 0; c < log_channel__NROF; ++c) {
         GETBUFFER_LOG(AUTO, c, &logbuffer, &logsize);
         TEST(logbuffer == lgwrt->chan[c].logbuf.addr);
         TEST(logsize   == lgwrt->chan[c].logbuf.logsize);
      }

      // TEST PRINTF_LOG(AUTO,
      GETBUFFER_LOG(AUTO, log_channel_ERR, &logbuffer, &logsize);
      PRINTF_LOG(AUTO, log_channel_ERR, log_flags_NONE, &header, "1%d%s\n", 2, "3");
      TEST(logsize + 4 < lgwrt->chan[log_channel_ERR].logbuf.logsize);
      TEST(0 == memcmp(logbuffer + lgwrt->chan[log_channel_ERR].logbuf.logsize - 4, "123\n", 5));
      TEST(0 == compare_header(lgwrt->chan[log_channel_ERR].logbuf.logsize - 4 - logsize, logbuffer + logsize, header.funcname, header.filename, header.linenr));

      // TEST TRUNCATEBUFFER_LOG(AUTO
      TRUNCATEBUFFER_LOG(AUTO, log_channel_ERR, logsize);
      // check logsize
      uint8_t * logbuffer2;
      size_t    logsize2;
      GETBUFFER_LOG(AUTO, log_channel_ERR, &logbuffer2, &logsize2);
      TEST(logbuffer == logbuffer2);
      TEST(logsize   == logsize2);

      // reset
      g_maincontext.type = oldtype;
   }

   // unprepare0
   TEST(-1 == read(pipe.read, buffer, sizeof(buffer)));
   TEST(STDERR_FILENO == dup2(oldfd, STDERR_FILENO));
   TEST(0 == free_pipe(&pipe));
   TEST(0 == close(oldfd));

   return 0;
ONERR:
   g_maincontext.type = oldtype;
   if (oldfd > 0) {
      dup2(oldfd, STDERR_FILENO);
      close(oldfd);
   }
   free_pipe(&pipe);
   return EINVAL;
}

static int test_freeisignored(void)
{
   // calling functions with lgwrt set to logwriter_FREE does no harm
   logwriter_t lgwrt = logwriter_FREE;
   uint8_t     logbuf[1];
   uint8_t *   buffer;
   size_t      size;
   va_list     args;
   log_header_t header = log_header_INIT(__FUNCTION__, __FILE__, __LINE__);
   memset(&args, 0, sizeof(args));

   for (uint8_t chan = 0; chan < log_channel__NROF; ++chan) {

      // TEST getstate_logwriter: log_state_IGNORED is default
      TEST(log_state_IGNORED == getstate_logwriter(&lgwrt, chan));

   }

   for (uint8_t chan = 0; chan < log_channel__NROF; ++chan) {
      for (uint8_t state = log_state_IGNORED; state < log_state__NROF; ++state) {
         for (int tc = 0; tc < 2; ++tc) {

            // TEST setstate_logwriter
            setstate_logwriter(&lgwrt, chan, state);

            // TEST getstate_logwriter
            TEST(state == getstate_logwriter(&lgwrt, chan));

            // TEST getbuffer_logwriter
            buffer = (void*) 1;
            size = 1;
            getbuffer_logwriter(&lgwrt, chan, &buffer, &size);
            TEST(0 == buffer);
            TEST(0 == size);

            // TEST compare_logwriter
            TEST(EINVAL == compare_logwriter(&lgwrt, chan, 1, logbuf));
            TEST(0 == compare_logwriter(&lgwrt, chan, 0, logbuf));

            // TEST truncatebuffer_logwriter(logwriter_t * lgwrt, uint8_t channel, size_t size)
            truncatebuffer_logwriter(&lgwrt, chan, 0);
            truncatebuffer_logwriter(&lgwrt, chan, 1);

            // TEST flushbuffer_logwriter
            flushbuffer_logwriter(&lgwrt, chan);

            // TEST vprintf_logwriter
            vprintf_logwriter(&lgwrt, chan, log_flags_LAST, &header, "123", args);

            // TEST printf_logwriter
            printf_logwriter(&lgwrt, chan, log_flags_LAST, &header, "123");

            // TEST printtext_logwriter
            struct p_MEMORY_OUT_OF_ERRLOG params = { 100, 1 };
            printtext_logwriter(&lgwrt, chan, log_flags_LAST, &header, MEMORY_OUT_OF_ERRLOG, &params);

         }
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int check_error_log(int tc, pipe_t * pipe, uint8_t * const logbuffer, size_t const logsize)
{
   uint8_t   buffer[log_config_MINSIZE];
   uint8_t * logbuffer2;
   size_t    logsize2;
   const uint16_t MINSIZE = 150;

   GETBUFFER_ERRLOG(&logbuffer2, &logsize2);
   TEST(logbuffer == logbuffer2);

   if (tc == 0) {
      // check ERR log written
      TEST(logsize + MINSIZE < logsize2);
      // check no INIT log writtem
      TEST(-1 == read(pipe->read, buffer, sizeof(buffer)) && errno == EAGAIN);

   } else {
      // check no ERR log written
      TEST(logsize == logsize2);
      // check INIT log written
      ssize_t bytes = read(pipe->read, buffer, sizeof(buffer));
      TEST(MINSIZE < bytes);
      // no more bytes
      TEST(-1 == read(pipe->read, buffer, sizeof(buffer)) && errno == EAGAIN);

      PRINTF_ERRLOG("%.*s", (int)bytes, buffer);
   }

   return 0;
ONERR:
   return EINVAL;
}

static int check_empty_log(pipe_t * pipe, uint8_t * const logbuffer, size_t const logsize)
{
   uint8_t   buffer[log_config_MINSIZE];
   uint8_t * logbuffer2;
   size_t    logsize2;

   // check error log empty
   GETBUFFER_ERRLOG(&logbuffer2, &logsize2);
   TEST(logbuffer == logbuffer2);
   TEST(logsize   == logsize2);

   // check init log empty
   TEST(-1 == read(pipe->read, buffer, sizeof(buffer)));

   return 0;
ONERR:
   return EINVAL;
}

static int test_invalidchannel(void)
{
   logwriter_t    lgwrt = logwriter_FREE;
   logwriter_t    oldlgwrt;
   maincontext_e  oldtype = g_maincontext.type;
   log_header_t   header = log_header_INIT(__FUNCTION__, __FILE__, __LINE__);
   int            oldfd = -1;
   pipe_t         pipe = pipe_FREE;
   uint8_t        buffer[128];
   uint8_t *      logbuffer;
   size_t         logsize;
   va_list        args;
   memset(&args, 0, sizeof(args));

   // prepare0
   TEST(0 == init_pipe(&pipe));
   oldfd = dup(STDERR_FILENO);
   TEST(0 < oldfd);
   TEST(STDERR_FILENO == dup2(pipe.write, STDERR_FILENO));
   TEST(0 == init_logwriter(&lgwrt));
   memcpy(&oldlgwrt, &lgwrt, sizeof(oldlgwrt));

   // === test switching ===

   for (int tc = 0; tc < 2; ++tc) {

      // prepare
      // simulate uninitialized maincontext (tc == 1) => AUTO uses INIT log
      g_maincontext.type = (tc == 0 ? oldtype : maincontext_STATIC);

      // TEST getbuffer_logwriter: EINVAL
      uint8_t * logbuffer2;
      size_t    logsize2 = (size_t) -1;
      GETBUFFER_ERRLOG(&logbuffer, &logsize);
      getbuffer_logwriter(&lgwrt, log_channel__NROF, &logbuffer2, &logsize2);
      TEST((size_t)-1 == logsize2);
      // check error log written
      TEST(0 == check_error_log(tc, &pipe, logbuffer, logsize));
      // check that call was ignored
      TEST(0 == memcmp(&oldlgwrt, &lgwrt, sizeof(oldlgwrt)));

      // TEST getstate_logwriter: EINVAL ignored
      GETBUFFER_ERRLOG(&logbuffer, &logsize);
      TEST(log_state_IGNORED == getstate_logwriter(&lgwrt, log_channel__NROF));
      TEST(log_state_IGNORED == getstate_logwriter(&lgwrt, (uint8_t)-1));
      // check error log empty
      TEST(0 == check_empty_log(&pipe, logbuffer, logsize));
      // check that call was ignored
      TEST(0 == memcmp(&oldlgwrt, &lgwrt, sizeof(oldlgwrt)));

      // TEST compare_logwriter: EINVAL
      GETBUFFER_ERRLOG(&logbuffer, &logsize);
      TEST(EINVAL == compare_logwriter(&lgwrt, log_channel__NROF, 14, (const uint8_t*)"[1: XXX]\ntest\n"));
      // check error log written
      TEST(0 == check_error_log(tc, &pipe, logbuffer, logsize));
      // check that call was ignored
      TEST(0 == memcmp(&oldlgwrt, &lgwrt, sizeof(oldlgwrt)));

      // TEST setstate_logwriter: EINVAL
      GETBUFFER_ERRLOG(&logbuffer, &logsize);
      setstate_logwriter(&lgwrt, log_channel__NROF, log_state_BUFFERED);
      setstate_logwriter(&lgwrt, (uint8_t)-1, 0);
      // check error log empty
      TEST(0 == check_empty_log(&pipe, logbuffer, logsize));
      // check that call was ignored
      TEST(0 == memcmp(&oldlgwrt, &lgwrt, sizeof(oldlgwrt)));

      // TEST truncatebuffer_logwriter: EINVAL
      GETBUFFER_ERRLOG(&logbuffer, &logsize);
      truncatebuffer_logwriter(&lgwrt, log_channel__NROF, 0);
      // check error log written
      TEST(0 == check_error_log(tc, &pipe, logbuffer, logsize));
      // check that call was ignored
      TEST(0 == memcmp(&oldlgwrt, &lgwrt, sizeof(oldlgwrt)));

      // TEST flushbuffer_logwriter: EINVAL
      GETBUFFER_ERRLOG(&logbuffer, &logsize);
      flushbuffer_logwriter(&lgwrt, log_channel__NROF);
      // check error log written
      TEST(0 == check_error_log(tc, &pipe, logbuffer, logsize));
      // check that call was ignored
      TEST(0 == memcmp(&oldlgwrt, &lgwrt, sizeof(oldlgwrt)));

      // TEST vprintf_logwriter: EINVAL
      GETBUFFER_ERRLOG(&logbuffer, &logsize);
      vprintf_logwriter(&lgwrt, log_channel__NROF, log_flags_NONE, &header, "ERR", args);
      // check error log written
      TEST(0 == check_error_log(tc, &pipe, logbuffer, logsize));
      // check that call was ignored
      TEST(0 == memcmp(&oldlgwrt, &lgwrt, sizeof(oldlgwrt)));

      // TEST printf_logwriter: EINVAL
      GETBUFFER_ERRLOG(&logbuffer, &logsize);
      printf_logwriter(&lgwrt, log_channel__NROF, log_flags_NONE, &header, "ERR");
      // check error log written
      TEST(0 == check_error_log(tc, &pipe, logbuffer, logsize));
      // check that call was ignored
      TEST(0 == memcmp(&oldlgwrt, &lgwrt, sizeof(oldlgwrt)));

      // TEST printtext_logwriter: EINVAL
      GETBUFFER_ERRLOG(&logbuffer, &logsize);
      struct p_textres_test p = { 1, "2" };
      printtext_logwriter(&lgwrt, log_channel__NROF, log_flags_NONE, &header, &textres_test, &p);
      // check error log written
      TEST(0 == check_error_log(tc, &pipe, logbuffer, logsize));
      // check that call was ignored
      TEST(0 == memcmp(&oldlgwrt, &lgwrt, sizeof(oldlgwrt)));

      // reset
      g_maincontext.type = oldtype;
   }

   // unprepare0
   TEST(-1 == read(pipe.read, buffer, sizeof(buffer)));
   TEST(STDERR_FILENO == dup2(oldfd, STDERR_FILENO));
   TEST(0 == free_pipe(&pipe));
   TEST(0 == close(oldfd));
   TEST(0 == free_logwriter(&lgwrt));

   return 0;
ONERR:
   g_maincontext.type = oldtype;
   if (oldfd > 0) {
      dup2(oldfd, STDERR_FILENO);
      close(oldfd);
   }
   free_pipe(&pipe);
   free_logwriter(&lgwrt);
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
   if (test_initlogmacros())  goto ONERR;
   if (test_autologmacros())  goto ONERR;
   if (test_freeisignored())  goto ONERR;
   if (test_invalidchannel()) goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
