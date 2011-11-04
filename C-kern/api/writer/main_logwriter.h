/* title: Main-LogWriter
   Used in static initializer for <umgebung_t>.

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

   file: C-kern/api/writer/main_logwriter.h
    Header file of <Main-LogWriter>.

   file: C-kern/writer/main_logwriter.c
    Implementation file of <Main-LogWriter impl>.
*/
#ifndef CKERN_WRITER_MAIN_LOGWRITER_HEADER
#define CKERN_WRITER_MAIN_LOGWRITER_HEADER

#define THREAD 1
#if ((KONFIG_SUBSYS)&THREAD)
#define  logwritermt_t              logwritermt_t
#define  logwritermt_it             logwritermt_it
#else
#define  logwritermt_t              logwriter_t
#define  logwritermt_it             logwriter_it
#endif
#undef THREAD

// forward
struct logwritermt_t ;
struct logwritermt_it ;

/* variable: g_main_logwriter
 * Used to support basic logging in main thread.
 * Before anything is initialized.
 * Supports also safe logging after freeing the log resource in <umgebung_t>.
 * This logservice is thread safe. */
extern struct logwritermt_t         g_main_logwriter ;

/* variable: g_main_logwriter_interface
 * Adapted interface (see <log_it>) to access <g_main_logwritermt>. */
extern struct logwritermt_it        g_main_logwriter_interface ;

#undef logwritermt_t
#undef logwritermt_it


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_writer_mainlogwriter
 * Tests global variable <g_main_logwriter> and interface <g_main_logwriter_interface>. */
extern int unittest_writer_mainlogwriter(void) ;
#endif

#endif