/* title: Locale-support Linux
   Implements <Locale-support>.

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

   file: C-kern/os/Linux/locale.c
    Linux specific implementation file <Locale-support Linux>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/os/locale.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif

// section: Implementation

/* function: charencoding_locale implementation
 * See <charencoding_locale> for interface description.
 * Calls POSIX conforming nl_langinfo to query the information. */
const char * charencoding_locale()
{
   return nl_langinfo(CODESET) ;
}

const char * current_locale()
{
   int err ;
   const char * lname = setlocale(LC_ALL, 0) ;

   if (!lname) {
      err = EINVAL ;
      LOG_SYSERR("setlocale",err) ;
      LOG_STRING("LC_ALL=0") ;
      goto ABBRUCH ;
   }

   return lname ;
ABBRUCH:
   LOG_ABORT(err) ;
   return 0 ;
}

const char * currentmsg_locale()
{
   int err ;
   const char * lname = setlocale(LC_MESSAGES, 0) ;

   if (!lname) {
      err = EINVAL ;
      LOG_SYSERR("setlocale",err) ;
      LOG_STRING("LC_MESSAGES=0") ;
      goto ABBRUCH ;
   }

   return lname ;
ABBRUCH:
   LOG_ABORT(err) ;
   return 0 ;
}

/* function: setdefault_locale implementation
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
 * LC_TIME     - Changes time and date formatting.
 *
 * */
int setdefault_locale()
{
   int err ;

   if (!setlocale(LC_ALL, "")) {
      err = EINVAL ;
      LOG_ERRTEXT(LOCALE_SETLOCALE) ;
      LOG_STRING(getenv("LC_ALL")) ;
      goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

/* function: reset_locale implementation
 * Calls C99 conforming function »setlocale«.
 * Set all subsystems of the C runtime environment to the standard locale "C"
 * which is active by default after process creation. */
int reset_locale()
{
   int err ;

   if (!setlocale(LC_ALL, "C")) {
      LOG_ERRTEXT(LOCALE_SETLOCALE) ;
      LOG_STRING("LC_ALL=C") ;
      err = EINVAL ;
      goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int resetmsg_locale()
{
   int err ;

   if (!setlocale(LC_MESSAGES, "C")) {
      LOG_ERRTEXT(LOCALE_SETLOCALE) ;
      LOG_STRING("LC_MESSAGES=C") ;
      err = EINVAL ;
      goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

// group: initprocess

int initprocess_locale()
{
   int err ;

   err = setdefault_locale() ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int freeprocess_locale()
{
   int err ;

   err = reset_locale() ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

#define TEST(CONDITION) TEST_ONERROR_GOTO(CONDITION,unittest_os_locale,ABBRUCH)

static int test_initerror(void)
{
   char * old_lcall = getenv("LC_ALL") ? strdup(getenv("LC_ALL")) : 0 ;

   // TEST setlocale error (consumes memory !)
   TEST(0 == setenv("LC_ALL", "XXX@unknown", 1)) ;
   TEST(EINVAL == initprocess_locale()) ;
   if (old_lcall) {
      TEST(0 == setenv("LC_ALL", old_lcall, 0)) ;
   } else {
      TEST(0 == unsetenv("LC_ALL")) ;
   }

   free(old_lcall) ;
   old_lcall = 0 ;

   return 0 ;
ABBRUCH:
   if (old_lcall) {
      TEST(0 == setenv("LC_ALL", old_lcall, 0)) ;
   } else {
      TEST(0 == unsetenv("LC_ALL")) ;
   }
   free(old_lcall) ;
   return 1 ;
}

static int test_initlocale(void)
{
   char * lname     = 0 ;

   // TEST init, double free
   TEST(0 == initprocess_locale()) ;
   TEST(current_locale()) ;
   lname = strdup(current_locale() ? current_locale() : "") ;
   TEST(lname) ;
   TEST(0 != strcmp("C", lname)) ;
   TEST(0 == freeprocess_locale()) ;
   TEST(0 == strcmp("C", current_locale())) ;
   TEST(0 == freeprocess_locale()) ;
   TEST(0 == strcmp("C", current_locale())) ;

   // TEST init sets the same name
   TEST(0 == initprocess_locale()) ;
   TEST(current_locale()) ;
   TEST(0 == strcmp(lname, current_locale())) ;
   TEST(0 == freeprocess_locale()) ;
   TEST(0 == strcmp("C", current_locale())) ;
   free(lname) ;
   lname = 0 ;

   return 0 ;
ABBRUCH:
   free(lname) ;
   return 1 ;
}

int unittest_os_locale()
{
   resourceusage_t    usage         = resourceusage_INIT_FREEABLE ;
   char             * old_locale    = 0 ;
   char             * old_msglocale = 0 ;

   // changes malloced memory
   if (test_initerror())   goto ABBRUCH ;

   // store current mapping
   TEST(0 == init_resourceusage(&usage)) ;

   old_locale    = strdup(current_locale()) ;
   TEST(old_locale) ;
   old_msglocale = strdup(currentmsg_locale()) ;
   TEST(old_msglocale) ;

   if (test_initerror())   goto ABBRUCH ;
   if (test_initlocale())  goto ABBRUCH ;

   if (0 != strcmp("C", old_locale)) {
      TEST(0 == setdefault_locale()) ;
   }
   if (0 == strcmp("C", old_msglocale)) {
      TEST(0 == resetmsg_locale()) ;
   }
   free(old_locale) ;
   free(old_msglocale) ;
   old_locale    = 0 ;
   old_msglocale = 0 ;

   // TEST resource usage has not changed
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   free(old_locale) ;
   free(old_msglocale) ;
   return EINVAL ;
}

#endif
