/*
   Implements logging of error messages.

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
   (C) 2010 Jörg Seebohn
*/

#include "C-kern/konfig.h"
#include "C-kern/api/umgebung/log.h"
#include "C-kern/api/errlog.h"
#include "C-kern/api/os/virtmemory.h"

// forward

static void write_logbuffer(log_buffer_t * log) ;
static void printf_logstderr( log_config_t * log, const char * format, ... ) ;

/* variable: g_safe_logservice
 * Is used to support a safe standard log configuration.
 * If the global log service is set to <g_main_logservice> then it is switched
 * to <g_safe_logservice> before a thread safe lock is aquired.
 * This ensures that os specific implementations of <osthread_mutex_t> can use
 * the log module without producing a dead lock. */
log_config_t  g_safe_logservice = {
         .printf          = &printf_logstderr,
         .isOn            = true,
         .isBuffered      = false,
         .isConstConfig   = true,
         .log_buffer      = 0
} ;

static_assert( ((typeof(umgebung()))0) == (const umgebung_t *)0, "Ensure LOCK_SERVICE cast is OK") ;


/* struct: log_buffer_t
 * Stores the memory address and size of cached output.
 * If the buffer is nearly full it is written to the
 * configured log channel. Currently only stderr is supported. */
struct log_buffer_t
{
   vm_block_t  buffer ;
   size_t      buffered_logsize ;
} ;

// group: lifetime
/* define: log_buffer_INIT_FREEABLE
 * */
#define log_buffer_INIT_FREEABLE    { .buffer = vm_block_INIT_FREEABLE, .buffered_logsize = 0 }

/* function: free_logbuffer
 * Frees allocates memory. If the buffer is filled
 * its content is written out before any resource is freed. */
static int free_logbuffer( log_buffer_t * log )
{
   int err ;

   if (log->buffered_logsize) {
      (void) write_logbuffer(log) ;
   }

   err = free_vmblock(&log->buffer) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

/* function: init_logbuffer
 * Reserves (virtual) memory for logbuffer. */
static int init_logbuffer( /*out*/log_buffer_t * log )
{
   int err ;
   size_t  pgsize = pagesize_vm() ;
   size_t nrpages = (pgsize < 1024) ? (1023 + pgsize) / pgsize : 1 ;

   err = init_vmblock(&log->buffer, nrpages) ;
   if (err) goto ABBRUCH ;
   log->buffered_logsize = 0 ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

/* function: clear_logbuffer
 * Clears content of buffer. */
static inline void clear_logbuffer(log_buffer_t * log)
{
   log->buffered_logsize = 0 ;
   log->buffer.start[0]  = 0 ;   // NULL terminated string
}

/* function: getlogbuffer_log
 * Returns start and length of buffered log string. */
static inline void getlogbuffer_logbuffer(log_buffer_t * log, /*out*/char ** buffer, /*out*/size_t * size)
{
   *buffer = (char*) log->buffer.start ;
   *size   = log->buffered_logsize ;
}

/* function: write_logbuffer
 * Writes content of buffer to log channel.
 * Currently only stderr is supported. */
static void write_logbuffer(log_buffer_t * log)
{
   size_t bytes_written = 0 ;

   do {
      ssize_t bytes ;
      do {
         bytes = write( STDERR_FILENO, log->buffer.start + bytes_written, log->buffered_logsize - bytes_written) ;
      } while( bytes < 0 && errno == EINTR ) ;
      if (bytes <= 0) {
         // TODO: add some special log code that always works and which indicates error state in logging
         assert(errno != EAGAIN /*should be blocking i/o*/) ;
         break ;
      }
      bytes_written += (size_t) bytes ;
   } while (bytes_written < log->buffered_logsize) ;

   clear_logbuffer(log) ;
}

static void printf_logbuffer( log_config_t * logconfig, const char * format, ... )
{
   log_buffer_t * log = logconfig->log_buffer ;
   size_t buffer_size = log->buffer.size_in_bytes - log->buffered_logsize ;

   for(;;) {

      if (buffer_size < 512 ) {
         write_logbuffer(log) ;
         buffer_size = log->buffer.size_in_bytes ;
      }

      va_list args ;
      va_start(args, format) ;
      int append_size = vsnprintf( log->buffered_logsize + (char*)log->buffer.start, buffer_size, format, args) ;
      va_end(args) ;

      if ( (unsigned)append_size < buffer_size ) {
         // no truncate
         log->buffered_logsize += (unsigned)append_size ;
         break ;
      }

      if ( buffer_size == log->buffer.size_in_bytes ) {
         // => s_logbuffered.buffered_logsize == 0
         // ignore truncate
         log->buffered_logsize = buffer_size - 1/*ignore 0 byte at end*/ ;
         write_logbuffer(log) ;
         break ;
      }

      // force flush && ignore append_size bytes
      buffer_size = 0 ;
   }
}

// section: logstderr

/* function: printf_logstderr
 * */
static void printf_logstderr( log_config_t * log, const char * format, ... )
{
   (void) log ;
   va_list args ;
   va_start(args, format) ;
   vdprintf(STDERR_FILENO, format, args) ;
   va_end(args) ;
}

// section: logignore

/* function: printf_logignore
 * */
static void printf_logignore( log_config_t * log, const char * format, ... )
{
   // generate no output
   (void) log ;
   (void) format ;
   return ;
}


// section: logconfig

int new_logconfig(/*out*/log_config_t ** log)
{
   int err ;
   log_config_t * logobj ;
   size_t        objsize = sizeof(log_config_t) + sizeof(*logobj->log_buffer) ;

   if (*log) {
      err = EINVAL ;
      goto ABBRUCH ;
   }

   logobj = (log_config_t*) malloc(objsize) ;
   if (!logobj) {
      err = ENOMEM ;
      goto ABBRUCH ;
   }

   logobj->printf      = &printf_logstderr ;
   logobj->isOn        = true ;
   logobj->isBuffered  = false ;
   logobj->log_buffer  = (log_buffer_t*) &logobj[1] ;
   *logobj->log_buffer = (log_buffer_t) log_buffer_INIT_FREEABLE ;

   *log = logobj ;
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int delete_logconfig(log_config_t ** log)
{
   int err ;
   log_config_t * logobj = *log ;

   if (     logobj
         && logobj != &g_main_logservice) {
      err = free_logbuffer(logobj->log_buffer) ;
      free(logobj) ;
      *log = 0 ;
      if (err) goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

/* function: switch_printf_logconfig
 * Used to switch between different implementation.
 * Only the variable <log_config_t.printf> is switched between
 *
 * 1. <printf_logbuffer>
 * 2. <printf_logstderr>
 * 3. <printf_logignore> */
static void switch_printf_logconfig(log_config_t * log)
{
   if (log->isOn) {
      if (log->isBuffered) {
         log->printf = &printf_logbuffer ;
      } else {
         log->printf = &printf_logstderr ;
      }
   } else {
      log->printf = &printf_logignore ;
   }
}

void set_onoff_logconfig(log_config_t * log, bool onoff)
{
   int err ;

   if (log->isConstConfig) {
      err = EINVAL ;
      goto ABBRUCH ;
   }

   if (log->isOn != onoff) {

      log->isOn = onoff ;

      switch_printf_logconfig(log) ;
   }

   return ;
ABBRUCH:
   LOG_ABORT(err) ;
   return ;
}

/* function: set_buffermode_logconfig implementation
 * See <set_buffermode_logconfig> for a description of its interface.
 * The internal variable <log_config_t.log_buffer> is initialized
 * or freed depending on whether buffermode is switched on or off.
 * If the initialization (or free) returns an error nothing is
 * changed. */
void set_buffermode_logconfig(log_config_t * log, bool mode)
{
   int err ;

   if (log->isConstConfig) {
      err = EINVAL ;
      goto ABBRUCH ;
   }

   if (log->isBuffered != mode) {

      if ( mode ) {
         err = init_logbuffer(log->log_buffer) ;
         if (err) goto ABBRUCH ;
      } else {
         err = free_logbuffer(log->log_buffer) ;
         if (err) goto ABBRUCH ;
      }

      log->isBuffered = mode ;
      switch_printf_logconfig(log) ;
   }

   return ;
ABBRUCH:
   LOG_ABORT(err) ;
   return ;
}

void clearbuffer_logconfig(log_config_t * log)
{
   if (log->isBuffered) {
      clear_logbuffer(log->log_buffer) ;
   }
}

void getlogbuffer_logconfig(log_config_t * log, /*out*/char ** buffer, /*out*/size_t * size)
{
   if (log->isBuffered) {
      getlogbuffer_logbuffer( log->log_buffer, buffer, size ) ;
   } else {
      *buffer = 0 ;
      *size   = 0 ;
   }
}

// section: main_logservice

/* variable: s_buffer_main_logservice
 * Reserves space for <log_buffer_t> used in <g_main_logservice>. */
static log_buffer_t  s_buffer_main_logservice = log_buffer_INIT_FREEABLE ;

/* variable: g_main_logservice
 * Is used to support a standard log configuration.
 * Useful to log during initialization before any other init function was called.
 * Assigns space to <log_config_t.log_buffer> and sets all function pointers. */
log_config_t  g_main_logservice = {
         .printf          = &printf_logstderr,
         .isOn            = true,
         .isBuffered      = false,
         .isConstConfig   = false,
         .log_buffer      = &s_buffer_main_logservice
} ;
