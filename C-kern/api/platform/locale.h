/* title: Locale-support
   Supports setting and getting of process locale (C runtime libraries).

   Use it to adapt to different character encodings / environments setings.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/platform/locale.h
    Header file of <Locale-support>.

   file: C-kern/platform/Linux/locale.c
    Linux specific implementation file <Locale-support Linux>.
*/
#ifndef CKERN_PLATFORM_LOCALE_HEADER
#define CKERN_PLATFORM_LOCALE_HEADER


// section: Functions

// group: initonce

/* function: initonce_locale
 * Called from <maincontext_t.init_maincontext>. */
int initonce_locale(void);

/* function: freeonce_locale
 * Called from <maincontext_t.free_maincontext>. */
int freeonce_locale(void);

// group: query

/* function: charencoding_locale
 * Returns the name of the character encoding of the current selected locale.
 * For example "UTF-8" for utf-8 multibyte character encoding. */
const char* charencoding_locale(void);

/* function: current_locale
 * Returns the name of the current active locale.
 * "C" is returned for the locale which is set by <reset_locale>
 * "de_DE@utf8" is returned if the user runs a german locale with utf8 text encoding
 * and after <setdefault_locale> has been called. */
const char * current_locale(void) ;

/* function: currentmsg_locale
 * Returns the name of the current active locale for system messages. */
const char * currentmsg_locale(void) ;

// group: set

/* function: setdefault_locale
 * Sets the default locale defined by the program environment.
 * This sets the program locale normally to the default locale
 * the user has set for itself. */
int setdefault_locale(void) ;

/* function: reset_locale
 * Sets the locale of all subsystems to default.
 * The default is the "C" locale (English) a C program uses after it has been started. */
int reset_locale(void) ;

/* function: resetmsg_locale
 * Sets the locale of system messages subsystem to default.
 * The default is the "C" locale (English) a C program uses after it has been started. */
int resetmsg_locale(void) ;

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_locale
 * Tests all locale functions. */
int unittest_platform_locale(void) ;
#endif

#endif
