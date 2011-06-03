/* title: Locale-support
   Supports setting and getting of process locale (C runtime libraries).

   Use it to adapt to different character encodings / environments setings.

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

   file: C-kern/api/os/locale.h
    Header file of <Locale-support>.

   file: C-kern/os/shared/locale.c
    Implementation file <Locale-support impl>.
*/
#ifndef CKERN_OS_LOCALE_HEADER
#define CKERN_OS_LOCALE_HEADER

// section: Functions

// group: query

/* function: charencoding_locale
 * Returns the name of the character encoding of the current selected locale.
 * For example "UTF-8" for utf-8 multibyte character encoding. */
extern const char * charencoding_locale(void) ;

/* function: current_locale
 * Returns the name of the current active locale.
 * "C" is returned for the locale which is set by <reset_locale>
 * "de_DE@utf8" is returned if the user runs a german locale with utf8 text encoding
 * and after <setdefault_locale> has been called. */
extern const char * current_locale(void) ;

/* function: currentmsg_locale
 * Returns the name of the current active locale for system messages. */
extern const char * currentmsg_locale(void) ;


// group: set

/* function: setdefault_locale
 * Sets the default locale defined by the program environment.
 * This sets the program locale normally to the default locale
 * the user has set for itself. */
extern int setdefault_locale(void) ;

/* function: reset_locale
 * Sets the locale a C program uses after it has started. */
extern int reset_locale(void) ;

/* function: resetmsg_locale
 * Sets the system messages locale a C program uses after it has started. */
extern int resetmsg_locale(void) ;

#endif
