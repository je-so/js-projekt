/* title: PosixSignals
   - Offers storing and comparing of different signal handler configurations.
   - Offers interface to set signal handling configuration at process start up.
     The configuration is read from "C-kern/resource/text.db/signalhandler"
     during compilation time.

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

   file: C-kern/api/os/sync/signal.h
    Header file of <PosixSignals>.

   file: C-kern/os/Linux/signal.c
    Linux specific implementation <PosixSignals Linux>.
*/
#ifndef CKERN_OS_SYNCHRONIZATION_SIGNAL_HEADER
#define CKERN_OS_SYNCHRONIZATION_SIGNAL_HEADER

/* typedef: signalconfig_t typedef
 * Export <signalconfig_t>. */
typedef struct signalconfig_t       signalconfig_t ;

/* typedef: signalcallback_f
 * Exports callback definition of a signal handler. */
typedef void                     (* signalcallback_f) (unsigned signr) ;

// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_os_sync_signal
 * Tests posix signals functionality. */
extern int unittest_os_sync_signal(void) ;
#endif


/* struct: signalconfig_t
 * Stores current state of all signal handlers and the signal mask.
 * Use this object to compare the current setting of all signal handlers
 * with the setting stored in this object to be equal. */
struct signalconfig_t ;

// group: initprocess

/* function: initprocess_signalconfig
 * Sets up a process wide signal configuration at process startup.
 * The configuration is read from "C-kern/resource/text.db/signalhandler"
 * during compilation time. */
extern int initprocess_signalconfig(void) ;

/* function: freeprocess_signalconfig
 * Restores default signal configuration. */
extern int freeprocess_signalconfig(void) ;

// group: lifetime

/* function: new_signalconfig
 * Stores in <signalconfig_t> the current settings of all signal handlers. */
extern int new_signalconfig(/*out*/signalconfig_t ** sigconfig) ;

/* function: delete_signalconfig
 * Frees any resources associated with sigconfig. */
extern int delete_signalconfig(signalconfig_t ** sigconfig) ;

// group: query

/* function: compare_signalconfig
 * Returns 0 if sigconfig1 and sigconfig2 contains equal settings. */
extern int compare_signalconfig(const signalconfig_t * sigconfig1, const signalconfig_t * sigconfig2) ;


// section: inline implementations

// group: KONFIG_SUBSYS

#define THREAD 1
#if (!((KONFIG_SUBSYS)&THREAD))
/* define: initprocess_signalconfig
 * Implement init as a no op if !((KONFIG_SUBSYS)&THREAD)
 * > #define initprocess_X11()  (0) */
#define initprocess_signalconfig()  (0)
/* define: freeprocess_signalconfig
 * Implement free as a no op if !((KONFIG_SUBSYS)&THREAD)
 * > #define freeprocess_X11()  (0) */
#define freeprocess_signalconfig()  (0)
#endif
#undef X11

#endif
