/* title: LogMain
   Used in static initializer for <threadcontext_t>.

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
