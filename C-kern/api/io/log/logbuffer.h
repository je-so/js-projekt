/* title: LogBuffer

   Wirte formatted error messages into a memory buffer.

   This module is *not* thread safe.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/io/log/logbuffer.h
    Header file <LogBuffer>.

   file: C-kern/io/log/logbuffer.c
    Implementation file <LogBuffer impl>.
*/
#ifndef CKERN_IO_LOG_LOGBUFFER_HEADER
#define CKERN_IO_LOG_LOGBUFFER_HEADER

// import
struct log_header_t;
struct logcontext_t;

// === exported types
struct logbuffer_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_log_logbuffer
 * Test <logbuffer_t> functionality. */
int unittest_io_log_logbuffer(void);
#endif


/* struct: logbuffer_t
 * A logbuffer_t writes error messages to a buffer.
 * If a new messages is always appended and it is truncated if there is no more space in the buffer.
 * If new messages should not be appended and less then log_config_MINSIZE plus "terminating \0 byte" bytes are free
 * the buffer is written to a configured <iochannel_t> before the new message is written into the buffer.
 * */
typedef struct logbuffer_t {
   /* variable: addr
    * Holds start address of memory buffer. */
   uint8_t *       addr;
   /* variable: size
    * Holds size in bytes of memory buffer. */
   size_t          size;
   /* variable: logsize
    * Stores the size in bytes of the buffered log entries.
    * If the buffer is empty logsize is 0. */
   size_t          logsize;
   /* variable: io
    * Holds iochannel the log is written to. */
   sys_iochannel_t io;
} logbuffer_t;

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
int init_logbuffer(/*out*/logbuffer_t* logbuf, size_t buffer_size, uint8_t buffer_addr[buffer_size], sys_iochannel_t io);

/* function: free_logbuffer
 * Clears all members. The memory is not freed it is considered managed by the calling object.
 * The configured <iochannel_t> is freed if it does not equal <iochannel_STDERR> or <iochannel_STDOUT>. */
int free_logbuffer(logbuffer_t* logbuf);

// group: query

/* function: sizefree_logbuffer
 * Returns free size usable by the next written entry.
 * Call <write_logbuffer> if this value is less than log_config_MINSIZE+"terminating \0 byte". */
static inline size_t sizefree_logbuffer(const logbuffer_t* logbuf);

/* function: io_logbuffer
 * Returns the <iochannel_t> the content of the buffer is written to. */
static inline sys_iochannel_t io_logbuffer(const logbuffer_t* logbuf);

/* function: getbuffer_logbuffer
 * Returns the start address of the memory buffer and size of written log. */
static inline void getbuffer_logbuffer(const logbuffer_t* logbuf, /*out*/uint8_t ** addr, /*out*/size_t * logsize);

/* function: compare_logbuffer
 * Returns 0 if logbuffer compares equal to content in logbuf.
 * The compare equal the logsize must much and all written texts.
 * The timestamps are not compared so they are allowed to differ.
 * The value EINVAL is returned in case the comparison is not equal. */
int compare_logbuffer(const logbuffer_t* logbuf, size_t logsize, const uint8_t logbuffer[logsize]);

// group: update

/* function: truncate_logbuffer
 * Resets buffer length to smaller size without writting it out. */
static inline void truncate_logbuffer(logbuffer_t* logbuf, size_t size);

/* function: write_logbuffer
 * Writes (flushes) the buffer to the configured io channel.
 * If an error occurs no logging is done only the error code is returned. */
int write_logbuffer(logbuffer_t* logbuf);

/* function: printf_logbuffer
 * Writes new log entry to log buffer.
 * If the written content is bigger than <sizefree_logbuffer> it is truncated.
 * A truncated message is indicated by " ..." as last characters in the buffer. */
void printf_logbuffer(logbuffer_t* logbuf, const char * format, ...) __attribute__ ((__format__ (__printf__, 2, 3)));

/* function: printheader_logbuffer
 * Appends header to log buffer.
 * The header looks like "[thread_id: timestamp] funcname() filename:linenr\nError NR - Description". */
void printheader_logbuffer(logbuffer_t* logbuf, struct logcontext_t* logcontext, const struct log_header_t * header);

/* function: vprintf_logbuffer
 * Same as <printf_logbuffer>. The argument after format must be of type va_list
 * and replaces the variable number of argumens in <printf_logbuffer>. */
void vprintf_logbuffer(logbuffer_t* logbuf, const char * format, va_list args);



// section: inline implementation

/* define: truncate_logbuffer
 * Implements <logbuffer_t.truncate_logbuffer>. */
static inline void truncate_logbuffer(logbuffer_t* logbuf, size_t size)
{
         if (size < logbuf->logsize) {
            logbuf->addr[size] = 0;
            logbuf->logsize    = size;
         }
}

/* define: getbuffer_logbuffer
 * Implements <logbuffer_t.getbuffer_logbuffer>. */
static inline void getbuffer_logbuffer(const logbuffer_t* logbuf, /*out*/uint8_t ** addr, /*out*/size_t * logsize)
{
         *addr    = logbuf->addr;
         *logsize = logbuf->logsize;
}

/* define: io_logbuffer
 * Implements <logbuffer_t.io_logbuffer>. */
static inline sys_iochannel_t io_logbuffer(const logbuffer_t* logbuf)
{
         return logbuf->io;
}

/* define: sizefree_logbuffer
 * Implements <logbuffer_t.sizefree_logbuffer>. */
static inline size_t sizefree_logbuffer(const logbuffer_t* logbuf)

{
         return logbuf->size - logbuf->logsize;
}

#endif
