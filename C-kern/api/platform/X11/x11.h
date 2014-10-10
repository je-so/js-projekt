/* title: X11-Subsystem

   Implements initialization of X11 graphics environment (makes it thread safe)
   and allows to dispatch events with <dispatchevent_X11>.

   The event handler logic is also contained <dispatchevent_X11> which allows to
   read and understand the event handler state machine.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/platform/X11/x11.h
    Header file of <X11-Subsystem>.

   file: C-kern/platform/X11/x11.c
    Implementation file <X11-Subsystem impl>.
*/
#ifndef CKERN_PLATFORM_X11_HEADER
#define CKERN_PLATFORM_X11_HEADER

// foward
struct x11display_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_X11
 * Test initialization process succeeds. */
int unittest_platform_X11(void) ;
#endif


// struct: X11_t

// group: init

/* function: initonce_X11
 * Init Xlib such that calling into it is thread safe.
 *
 * This may be removed if every thread has its own <x11display_t>
 * connection and draws into its own window. */
int initonce_X11(void);

/* function: freeonce_X11
 * Does nothing at the moment. */
int freeonce_X11(void);

// group: query

// ?

// group: update

/* function: dispatchevent_X11
 * Checks event queue and dispatches 1 event if avialable.
 * If there are no waiting events this function returns immediately.
 * If no event handler is registered for the dispatched event
 * nothing else is done except for consuming one event. */
int dispatchevent_X11(struct x11display_t * x11disp) ;

/* function: nextevent_X11
 * Waits until there is one event in the queue and calls <dispatchevent_X11>. */
int nextevent_X11(struct x11display_t * x11disp) ;

// section: inline implementation

// group: X11_t

#if !defined(KONFIG_USERINTERFACE_X11)
/* define: initonce_X11
 * Implement init as a no op if defined KONFIG_USERINTERFACE_X11. */
#define initonce_X11()  (0)
/* define: freeonce_X11
 * Implement free as a no op if defined KONFIG_USERINTERFACE_X11. */
#define freeonce_X11()  (0)
#endif

#endif
