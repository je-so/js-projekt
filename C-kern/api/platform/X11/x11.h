/* title: X11-Subsystem

   Implements initialization of X11 graphics environment (makes it thread safe)
   and allows to dispatch events with <dispatchevent_X11>.

   The event handler logic is also contained <dispatchevent_X11> which allows to
   read and understand the event handler state machine.

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

   file: C-kern/api/platform/X11/x11.h
    Header file of <X11-Subsystem>.

   file: C-kern/platform/shared/X11/x11.c
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
int initonce_X11(void) ;

/* function: freeonce_X11
 * Does nothing at the moment. */
int freeonce_X11(void) ;

// group: query

// ?

// group: update

/* function: dispatchevent_X11
 * Checks event queue and dispatches 1 event if avialable.
 * If there are no waiting events this function returns immediately.
 * If no event handler is registered for the dispatched event
 * nothing else is done except for consuming one event. */
int dispatchevent_X11(struct x11display_t * x11disp) ;


// section: inline implementation

// group: X11_t

#define KONFIG_x11 1
#if !((KONFIG_USERINTERFACE)&KONFIG_x11)
/* define: initonce_X11
 * Implement init as a no op if (KONFIG_USERINTERFACE!=KONFIG_x11). */
#define initonce_X11()  (0)
/* define: freeonce_X11
 * Implement free as a no op if (KONFIG_USERINTERFACE!=KONFIG_x11). */
#define freeonce_X11()  (0)
#endif
#undef KONFIG_x11

#endif
