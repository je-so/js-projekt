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

   file: C-kern/api/platform/sync/signal.h
    Header file of <PosixSignals>.

   file: C-kern/platform/Linux/signal.c
    Linux specific implementation <PosixSignals Linux>.
*/
#ifndef CKERN_PLATFORM_SYNCHRONIZATION_SIGNAL_HEADER
#define CKERN_PLATFORM_SYNCHRONIZATION_SIGNAL_HEADER

/* typedef: signalconfig_t typedef
 * Export <signalconfig_t>. */
typedef struct signalconfig_t       signalconfig_t ;

/* typedef: signalcallback_f
 * Exports callback definition of a signal handler. */
typedef void                     (* signalcallback_f) (unsigned signr) ;

/* typedef: rtsignal_t
 * Exports realtime signal type as simple number.
 * The supported number range is 0..15 to be portable. */
typedef uint8_t                     rtsignal_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_sync_signal
 * Tests posix signals functionality. */
extern int unittest_platform_sync_signal(void) ;
#endif


// section: rtsignal_t

// group: change

/* function: send_rtsignal
 * Sends realtime signal to any thread.
 * If more than one thread waits (<wait_rtsignal>) for the signal
 * the order of delivery is unspecified.
 *
 * If the own process queue is full EAGAIN is returned and no signal is sent. */
extern int send_rtsignal(rtsignal_t nr) ;

/* function: wait_rtsignal
 * Waits for nr_signals realtime signals with number nr.
 * They are removed from the queue. */
extern int wait_rtsignal(rtsignal_t nr, uint32_t nr_signals) ;

/* function: trywait_rtsignal
 * Polls the queu for one realtime signals.
 * If the queue is empty EAGAIN ist returned.
 * If the queue contained the rt signal with number nr
 * it is consumed and 0 is returned. */
extern int trywait_rtsignal(rtsignal_t nr) ;

/* struct: signalconfig_t
 * Stores current state of all signal handlers and the signal mask.
 * Use this object to compare the current setting of all signal handlers
 * with the setting stored in this object to be equal. */
struct signalconfig_t ;

// group: init

/* function: initonce_signalconfig
 * Sets up a process wide signal configuration at process startup.
 * The configuration is read from "C-kern/resource/text.db/signalhandler"
 * during compilation time. */
extern int initonce_signalconfig(void) ;

/* function: freeonce_signalconfig
 * Restores default signal configuration. */
extern int freeonce_signalconfig(void) ;

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
/* define: initonce_signalconfig
 * Implement init as a no op if !((KONFIG_SUBSYS)&THREAD)
 * > #define initonce_signalconfig()  (0) */
#define initonce_signalconfig()  (0)
/* define: freeonce_signalconfig
 * Implement free as a no op if !((KONFIG_SUBSYS)&THREAD)
 * > #define freeonce_signalconfig()  (0) */
#define freeonce_signalconfig()  (0)
#endif
#undef THREAD

#endif
