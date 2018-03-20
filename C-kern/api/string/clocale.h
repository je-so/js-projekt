/* title: Locale-support

   Supports setting and getting of process locale (C runtime libraries).

   Used to adapt to different character encodings and string formatting options.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/string/clocale.h
    Header file of <Locale-support>.

   file: C-kern/string/clocale.c
    Linux specific implementation file <Locale-support Linux>.
*/
#ifndef CKERN_STRING_CLOCALE_HEADER
#define CKERN_STRING_CLOCALE_HEADER


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_string_clocale
 * Tests all locale functions. */
int unittest_string_clocale(void) ;
#endif


/* struct: clocale_t
 * Supports settinchanging the current locale used by the C library.
 * Currently only the previous set locale is stored.
 * init_clocale stores the previous one before changeing it and
 * free_clocale resets it to the previous one.
 * */
typedef struct clocale_t {
   char old_locale[16];
} clocale_t;

// group: lifetime

/* define: clocale_FREE
 * Für statische Initialisierung von <clocale_t>. */
#define clocale_FREE { { 0 } }

/* function: init_clocale
 * Changes the locale to the one the user has defined.
 * The current setting is read from environment variables.
 * Called from <maincontext_t.init_maincontext>. */
int init_clocale(/*out*/clocale_t* cl);

/* function: free_clocale
 * Resets the locale to its value previously active before <init_clocale> was called.
 * Called from <maincontext_t.free_maincontext>. */
int free_clocale(clocale_t* cl);

// group: query

/* function: charencoding_clocale
 * Returns the name of the character encoding of the current selected locale.
 * For example "UTF-8" for utf-8 multibyte character encoding. */
const char* charencoding_clocale(void);

/* function: current_clocale
 * Returns the name of the current active locale.
 * "C" is returned for the locale which is set by <reset_clocale>
 * "de_DE@utf8" is returned if the user runs a german locale with utf8 text encoding
 * and after <setuser_clocale> has been called. */
const char* current_clocale(void);

/* function: currentmsg_clocale
 * Returns the name of the current active locale for system messages. */
const char* currentmsg_clocale(void);

// group: update

/* function: setuser_clocale
 * Sets the default locale defined by the user.
 * It is read from the user set environment variables.
 *
 * Calls C99 conforming function »setlocale«.
 * With category LC_ALL all different subsystems of the C runtime environment
 * are changed to the locale set by the user.
 *
 * The changed categories are:
 * LC_COLLATE  - Changes character classes in »regular expression matching« and »string compare and sorting«.
 * LC_CTYPE    - Changes character classification, conversion, case-sensitive comparison, and wide character functions.
 * LC_MESSAGES - Changes language of system messages (strerror, perror).
 * LC_MONETARY - Changes monetary formatting.
 * LC_NUMERIC  - Changes number formatting (such as the decimal point and the thousands separator).
 * LC_TIME     - Changes time and date formatting. */
int setuser_clocale(void);

/* function: set_clocale
 * Set current locale to a different value.
 * The locale name "C" is alöways defined.
 * A typical name for a locale is "de_DE.utf8" or "en_US.utf8". */
int set_clocale(const char* name);

/* function: reset_clocale
 * Sets the locale of all subsystems to default.
 * The default is the "C" locale (English) a C program uses after it has been started. */
int reset_clocale(void);

/* function: resetmsg_clocale
 * Sets the locale of system messages subsystem to default.
 * The default is the "C" locale (English) a C program uses after it has been started. */
int resetmsg_clocale(void);

#endif
