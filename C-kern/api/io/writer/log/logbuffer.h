/* title: LogBuffer

   Wirte formatted error messages into a memory buffer.

   This module is *not* thread safe.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/io/writer/log/logbuffer.h
    Header file <LogBuffer>.

   file: C-kern/io/writer/log/logbuffer.c
    Implementation file <LogBuffer impl>.
*/
#ifndef CKERN_IO_WRITER_LOG_LOGBUFFER_HEADER
#define CKERN_IO_WRITER_LOG_LOGBUFFER_HEADER

// forward
struct log_header_t;

/* typedef: struct logbuffer_t
 * Export <logbuffer_t> into global namespace. */
typedef struct logbuffer_t logbuffer_t;


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
   uint8_t *         addr;
   /* variable: size
    * Holds size in bytes of memory buffer. */
   uint32_t          size;
   /* variable: logsize
    * Stores the size in bytes of the buffered log entries.
    * If the buffer is empty logsize is 0. */
   uint32_t          logsize;
   /* variable: io
    * Holds iochannel the log is written to. */
   sys_iochannel_t   io;
};

// group: lifetime

/* define: logbuffer_FREE
 * Static initializer. */
#define logbuffer_FREE { 0, 0, 0, sys_iochannel_FREE }

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

/* function: compare_logbuffer
 * Returns 0 if logbuffer compares equal to content in logbuf.
 * The compare equal the logsize must much and all written texts.
 * The timestamps are not compared so they are allowed to differ.
 * The value EINVAL is returned in case the comparison is not equal. */
int compare_logbuffer(const logbuffer_t * logbuf, size_t logsize, const uint8_t logbuffer[logsize]) ;

// group: update

/* function: truncate_logbuffer
 * Resets buffer length to smaller size without writting it out. */
void truncate_logbuffer(logbuffer_t * logbuf, size_t size) ;

/* function: write_logbuffer
 * Writes (flushes) the buffer to the configured io channel.
 * If an error occurs no logging is done only the error code is returned. */
int write_logbuffer(logbuffer_t * logbuf) ;

/* function: printf_logbuffer
 * Writes new log entry to log buffer.
 * If the written content is bigger than <sizefree_logbuffer> it is truncated.
 * A truncated message is indicated by " ..." as last characters in the buffer. */
void printf_logbuffer(logbuffer_t * logbuf, const char * format, ...) __attribute__ ((__format__ (__printf__, 2, 3))) ;

/* function: printheader_logbuffer
 * Appends header to log buffer.
 * The header looks like "[thread_id: timestamp] funcname() filename:linenr\nError NR - Description". */
void printheader_logbuffer(logbuffer_t * logbuf, const struct log_header_t * header) ;

/* function: vprintf_logbuffer
 * Same as <printf_logbuffer>. The argument after format must be of type va_list
 * and replaces the variable number of argumens in <printf_logbuffer>. */
void vprintf_logbuffer(logbuffer_t * logbuf, const char * format, va_list args) ;



// section: inline implementation

/* define: truncate_logbuffer
 * Implements <logbuffer_t.truncate_logbuffer>. */
#define truncate_logbuffer(logbuf, size) \
         do {                          \
            logbuffer_t * _lb;         \
            size_t _sz;                \
            _lb = (logbuf);            \
            _sz = (size);              \
            if (_sz < _lb->logsize) {  \
               _lb->addr[_sz] = 0;     \
               _lb->logsize   = _sz;   \
            }                          \
         } while(0)

/* define: getbuffer_logbuffer
 * Implements <logbuffer_t.getbuffer_logbuffer>. */
#define getbuffer_logbuffer(logbuf, _addr, _logsize) \
         ( __extension__ ({               \
            const logbuffer_t * _lb;      \
            _lb = (logbuf);               \
            *(_addr)    = _lb->addr;      \
            *(_logsize) = _lb->logsize;   \
         }))


/* define: io_logbuffer
 * Implements <logbuffer_t.io_logbuffer>. */
#define io_logbuffer(logbuf)  \
         ((logbuf)->io)

/* define: sizefree_logbuffer
 * Implements <logbuffer_t.sizefree_logbuffer>. */
#define sizefree_logbuffer(logbuf) \
         ( __extension__ ({            \
            const logbuffer_t * _lb;   \
            _lb = (logbuf);            \
            _lb->size - _lb->logsize;  \
         }))

#endif
