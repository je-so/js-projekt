/* title: LogMain
   Used in static initializer for <threadcontext_t>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/io/writer/log/logmain.h
    Header file of <LogMain>.

   file: C-kern/io/writer/log/logmain.c
    Implementation file of <LogMain impl>.
*/
#ifndef CKERN_IO_WRITER_LOG_LOGMAIN_HEADER
#define CKERN_IO_WRITER_LOG_LOGMAIN_HEADER

// forward
struct log_it;

/* variable: g_logmain_interface
 * Adapted interface (see <log_it>) to static log service. */
extern struct log_it g_logmain_interface;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_writer_log_logmain
 * Tests static log service <g_logmain_interface>. */
int unittest_io_writer_log_logmain(void);
#endif

#endif
