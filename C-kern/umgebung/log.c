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

typedef void (* const constprintf_log_f) (const char * format, ...) ;
typedef void (* printf_log_f) (const char * format, ...) ;

typedef struct log_buffered_t log_buffered_t ;
struct log_buffered_t
{
   virtmemory_block_t   buffer ;
   size_t     buffered_logsize ;
   bool                 isInit ;
} ;

static log_buffered_t s_logbuffered = { .buffer = virtmemoryblock_INIT_FREEABLE, .buffered_logsize = 0, .isInit = false } ;

/* Never free s_logbuffered !
 *
 * static int free_logbuffered( log_buffered_t * log )
 * {
 *
 * }
 */

static int init_logbuffered( /*out*/log_buffered_t * log )
{
   int err ;

   assert(!log->isInit) ;
   err = map_virtmemory(&log->buffer, 1) ;
   if (err) goto ABBRUCH ;
   assert(log->buffer.size_in_bytes >= 1024) ;

   log->buffered_logsize = 0 ;
   log->isInit = true ;
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

static void printf_logstderr( const char * format, ... )
{
   va_list args ;
   va_start(args, format) ;
   vdprintf(STDERR_FILENO, format, args) ;
   va_end(args) ;
}

static void write_logbuffered(void)
{
   size_t bytes_written = 0 ;

   do {
      ssize_t bytes ;
      do {
         bytes = write( STDERR_FILENO, s_logbuffered.buffer.start + bytes_written, s_logbuffered.buffered_logsize - bytes_written) ;
      } while( bytes < 0 && errno == EINTR ) ;
      if (bytes <= 0) {
         // TODO: add some special log code that always works and which indicates error state in logging
         assert(errno != EAGAIN /*should be blocking i/o*/) ;
         break ;
      }
      bytes_written += (size_t) bytes ;
   } while (bytes_written < s_logbuffered.buffered_logsize) ;

   s_logbuffered.buffered_logsize = 0 ;
   s_logbuffered.buffer.start[0]  = 0 ;   // NULL terminated string
}

static void printf_logbuffered( const char * format, ... )
{
   size_t buffer_size = s_logbuffered.buffer.size_in_bytes - s_logbuffered.buffered_logsize ;

   for(;;) {

      if (buffer_size < 512 ) {
         write_logbuffered() ;
         buffer_size = s_logbuffered.buffer.size_in_bytes ;
      }

      va_list args ;
      va_start(args, format) ;
      int append_size = vsnprintf( s_logbuffered.buffered_logsize + (char*)s_logbuffered.buffer.start, buffer_size, format, args) ;
      va_end(args) ;

      if ( (unsigned)append_size < buffer_size ) {
         // no truncate
         s_logbuffered.buffered_logsize += (unsigned)append_size ;
         break ;
      }

      if ( buffer_size == s_logbuffered.buffer.size_in_bytes ) {
         // => s_logbuffered.buffered_logsize == 0
         // ignore truncate
         s_logbuffered.buffered_logsize = buffer_size - 1/*ignore 0 byte at end*/ ;
         write_logbuffered() ;
         break ;
      }

      // force flush && ignore append_size bytes
      buffer_size = 0 ;
   }

}

static void printf_logignore( const char * format, ... )
{  // generate no output
   (void) format ;
   return ;
}

static void switch_printf_log(void)
{
   static_assert_void( &g_service_log.printf == (constprintf_log_f*) &g_service_log.printf ) ;

   if (g_service_log.isOn) {
      if (g_service_log.isBuffered) {
         * (printf_log_f*) (intptr_t) &g_service_log.printf = &printf_logbuffered ;
      } else {
         * (printf_log_f*) (intptr_t) &g_service_log.printf = &printf_logstderr ;
      }
   } else {
      * (printf_log_f*) (intptr_t) &g_service_log.printf = &printf_logignore ;
   }
}

static void config_onoff_log(bool onoff)
{
   if (g_service_log.isOn != onoff) {

      static_assert_void( &g_service_log.isOn == (const bool *) &g_service_log.isOn ) ;
      * (bool*) (intptr_t) &g_service_log.isOn = onoff ;

      switch_printf_log() ;
   }
}

static void config_buffered_log(bool bufferedstate)
{
   int err ;
   if (g_service_log.isBuffered != bufferedstate) {

      // init s_logbuffered
      if (  bufferedstate
            && !s_logbuffered.isInit) {
         if ((err = init_logbuffered(&s_logbuffered))) goto ABBRUCH ;
      }

      static_assert_void( &g_service_log.isBuffered == (const bool *) &g_service_log.isBuffered ) ;
      * (bool*) (intptr_t) &g_service_log.isBuffered = bufferedstate ;

      switch_printf_log() ;
   }

   return ;
ABBRUCH:
   LOG_ABORT(err) ;
   return ;
}

void clearlogbuffer_log(void)
{
   if (g_service_log.isBuffered) {
      s_logbuffered.buffered_logsize = 0 ;
      s_logbuffered.buffer.start[0]  = 0 ;   // \0 byte at end of string
   }
}

void getlogbuffer_log(/*out*/char ** buffer, /*out*/size_t * size)
{
   if (g_service_log.isBuffered) {
      *buffer = (char*) s_logbuffered.buffer.start ;
      *size   = s_logbuffered.buffered_logsize ;
   } else {
      *buffer = 0 ;
      *size   = 0 ;
   }
}

log_interface_t  g_service_log = {
         .printf          = &printf_logstderr,
         .config_onoff    = &config_onoff_log,
         .config_buffered = &config_buffered_log,
         .clearlogbuffer  = &clearlogbuffer_log,
         .getlogbuffer    = &getlogbuffer_log,
         .isOn       = true,
         .isBuffered = false,
} ;
