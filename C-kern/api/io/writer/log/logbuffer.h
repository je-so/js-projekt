/* title: LogBuffer

   Wirte formatted error messages into a memory buffer.

   This module is *not* thread safe.

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
   (C) 2013 Jörg Seebohn

   file: C-kern/api/io/writer/log/logbuffer.h
    Header file <LogBuffer>.

   file: C-kern/io/writer/log/logbuffer.c
    Implementation file <LogBuffer impl>.
*/
#ifndef CKERN_IO_WRITER_LOG_LOGBUFFER_HEADER
#define CKERN_IO_WRITER_LOG_LOGBUFFER_HEADER

/* typedef: struct logbuffer_t
 * Export <logbuffer_t> into global namespace. */
typedef struct logbuffer_t                logbuffer_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_writer_log_logbuffer
 * Test <logbuffer_t> functionality. */
int unittest_io_writer_log_logbuffer(void) ;
#endif


/* struct: logbuffer_t
 * A logbuffer_t writes error messages to a buffer.
 * If a new messages is always appended and it is truncated if there is no more space in the buffer.
 * If new messages should not be appended and less then log_config_MINSIZE plus "terminating \0 byte" bytes are free
 * the buffer is written to a configured <iochannel_t> before the new message is written into the buffer.
 * */
struct logbuffer_t {
   /* variable: addr
    * Holds start address of memory buffer. */
   uint8_t *         addr ;
   /* variable: size
    * Holds size in bytes of memory buffer. */
   uint32_t          size ;
   /* variable: logsize
    * Stores the size in bytes of the buffered log entries.
    * If the buffer is empty logsize is 0. */
   uint32_t          logsize ;
   /* variable: io
    * Holds iochannel the log is written to. */
   sys_iochannel_t   io ;
} ;

// group: lifetime

/* define: logbuffer_INIT_FREEABLE
 * Static initializer. */
#define logbuffer_INIT_FREEABLE           { 0, 0, 0, sys_iochannel_INIT_FREEABLE }

/* define: logbuffer_INIT
 * Static initializer. You do not need to free an object initialized in such a manner
 * but you can.
 *
 * Parameter:
 * size - Size of a temporary or static buffer.
 * addr - Start address of the buffer.
 * io   - <iochannel_t> the buffer is written to. */
#define logbuffer_INIT(size, addr, io) \
         { (addr), (size), ((addr)[0] = 0), (io) }

/* function: init_logbuffer
 * Initializes object. No additional resources are allocated. */
int init_logbuffer(/*out*/logbuffer_t * logbuf, uint32_t buffer_size, uint8_t buffer_addr[buffer_size], sys_iochannel_t io) ;

/* function: free_logbuffer
 * Clears all members. The memory is not freed it is considered managed by the calling object.
 * The configured <iochannel_t> is freed if it does not equal <iochannel_STDERR> or <iochannel_STDOUT>. */
int free_logbuffer(logbuffer_t * logbuf) ;

// group: query

/* function: sizefree_logbuffer
 * Returns free size usable by the next written entry.
 * Call <write_logbuffer> if this value is less than log_config_MINSIZE+"terminating \0 byte". */
uint32_t sizefree_logbuffer(const logbuffer_t * logbuf) ;

/* function: io_logbuffer
 * Returns the <iochannel_t> the content of the buffer is written to. */
sys_iochannel_t io_logbuffer(const logbuffer_t * logbuf) ;

/* function: getbuffer_logbuffer
 * Returns the start address of the memory buffer and size of written log. */
void getbuffer_logbuffer(const logbuffer_t * logbuf, /*out*/uint8_t ** addr, /*out*/size_t * logsize) ;

// group: update

/* function: clear_logbuffer
 * Clears the buffer without writting it out. */
void clear_logbuffer(logbuffer_t * logbuf) ;

/* function: write_logbuffer
 * Writes (flushes) the buffer to the configured io channel.
 * If an error occurs no logging is done only the error code is returned. */
int write_logbuffer(logbuffer_t * logbuf) ;

/* function: addtimestamp_logbuffer
 * Appends "[thread_id: timestamp]\n" to log buffer. */
void addtimestamp_logbuffer(logbuffer_t * logbuf) ;

/* function: printf_logbuffer
 * Writes new log entry to log buffer.
 * If the written content is bigger than <sizefree_logbuffer> it is truncated.
 * A truncated message is indicated by " ..." as last characters in the buffer. */
void printf_logbuffer(logbuffer_t * logbuf, const char * format, ...) __attribute__ ((__format__ (__printf__, 2, 3))) ;

/* function: vprintf_logbuffer
 * Same as <printf_logbuffer>. The argument after format must be of type va_list
 * and replaces the variable number of argumens in <printf_logbuffer>. */
void vprintf_logbuffer(logbuffer_t * logbuf, const char * format, va_list args) ;



// section: inline implementation

/* define: clear_logbuffer
 * Implements <logbuffer_t.clear_logbuffer>. */
#define clear_logbuffer(logbuf)     \
         ( __extension__ ({         \
            logbuffer_t * _lb ;     \
            _lb = (logbuf) ;        \
            _lb->addr[0] = 0 ;      \
            _lb->logsize = 0 ;      \
         }))

/* define: getbuffer_logbuffer
 * Implements <logbuffer_t.getbuffer_logbuffer>. */
#define getbuffer_logbuffer(logbuf, _addr, _logsize)  \
         ( __extension__ ({                           \
            const logbuffer_t * _lb ;                 \
            _lb = (logbuf) ;                          \
            *(_addr)    = _lb->addr ;                 \
            *(_logsize) = _lb->logsize ;              \
         }))


/* define: io_logbuffer
 * Implements <logbuffer_t.io_logbuffer>. */
#define io_logbuffer(logbuf)  \
         ((logbuf)->io)

/* define: sizefree_logbuffer
 * Implements <logbuffer_t.sizefree_logbuffer>. */
#define sizefree_logbuffer(logbuf)     \
         ( __extension__ ({            \
            const logbuffer_t * _lb ;  \
            _lb = (logbuf) ;           \
            _lb->size - _lb->logsize ; \
         }))

#endif
