/* title: Locale-support Linux
   Implements <Locale-support>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/platform/locale.h
    Header file of <Locale-support>.

   file: C-kern/platform/Linux/locale.c
    Linux specific implementation file <Locale-support Linux>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/platform/locale.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/resourceusage.h"
#include "C-kern/api/test/unittest.h"
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
      TRACESYSCALL_ERRLOG("setlocale", err) ;
      goto ONERR;
   }

   return lname ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return 0 ;
}

const char * currentmsg_locale()
{
   int err ;
   const char * lname = setlocale(LC_MESSAGES, 0) ;

   if (!lname) {
      err = EINVAL ;
      TRACESYSCALL_ERRLOG("setlocale", err) ;
      goto ONERR;
   }

   return lname ;
ONERR:
   TRACEEXIT_ERRLOG(err);
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
      TRACE_NOARG_ERRLOG(log_flags_NONE, LOCALE_SETLOCALE, err) ;
      PRINTCSTR_ERRLOG(getenv("LC_ALL")) ;
      goto ONERR;
   }

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
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
      err = EINVAL ;
      TRACE_NOARG_ERRLOG(log_flags_NONE, LOCALE_SETLOCALE, err) ;
      PRINTCSTR_ERRLOG("LC_ALL=C") ;
      goto ONERR;
   }

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}

int resetmsg_locale()
{
   int err ;

   if (!setlocale(LC_MESSAGES, "C")) {
      err = EINVAL ;
      TRACE_NOARG_ERRLOG(log_flags_NONE, LOCALE_SETLOCALE, err) ;
      PRINTCSTR_ERRLOG("LC_MESSAGES=C") ;
      goto ONERR;
   }

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}

// group: init

int initonce_locale()
{
   int err ;

   err = setdefault_locale() ;
   if (err) goto ONERR;

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}

int freeonce_locale()
{
   int err ;

   err = reset_locale() ;
   if (err) goto ONERR;

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initerror(void)
{
   char old_lcall[100] = { 0 } ;

   if (getenv("LC_ALL")) {
      strncpy(old_lcall, getenv("LC_ALL"), sizeof(old_lcall)-1) ;
   }

   // TEST setlocale error (consumes memory !)
   TEST(0 == setenv("LC_ALL", "XXX@unknown", 1)) ;
   TEST(EINVAL == initonce_locale()) ;
   if (old_lcall[0]) {
      TEST(0 == setenv("LC_ALL", old_lcall, 0)) ;
   } else {
      TEST(0 == unsetenv("LC_ALL")) ;
   }

   return 0 ;
ONERR:
   if (old_lcall[0]) {
      setenv("LC_ALL", old_lcall, 0) ;
   } else {
      unsetenv("LC_ALL") ;
   }
   return EINVAL ;
}

static int test_initlocale(void)
{
   char lname[100] = { 0 } ;

   // TEST initonce_locale
   TEST(0 == initonce_locale()) ;
   TEST(current_locale()) ;
   strncpy(lname, current_locale() ? current_locale() : "", sizeof(lname)-1) ;
   TEST(0 != strcmp("C", lname)) ;

   // TEST freeonce_locale
   TEST(0 == freeonce_locale()) ;
   TEST(0 == strcmp("C", current_locale())) ;
   TEST(0 == freeonce_locale()) ;
   TEST(0 == strcmp("C", current_locale())) ;

   // TEST init sets the same name
   TEST(0 == initonce_locale()) ;
   TEST(current_locale()) ;
   TEST(0 == strcmp(lname, current_locale())) ;
   TEST(0 == freeonce_locale()) ;
   TEST(0 == strcmp("C", current_locale())) ;

   return 0 ;
ONERR:
   return EINVAL ;
}

static int childprocess_unittest(void)
{
   resourceusage_t usage           = resourceusage_FREE ;
   char            old_locale[100] = { 0 } ;

   // changes malloced memory
   if (test_initerror())   goto ONERR;

   // store current mapping
   TEST(0 == init_resourceusage(&usage)) ;

   strncpy(old_locale, current_locale(), sizeof(old_locale)-1) ;

   if (test_initerror())   goto ONERR;
   if (test_initlocale())  goto ONERR;

   if (0 != strcmp("C", old_locale)) {
      TEST(0 == setdefault_locale()) ;
   }

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONERR:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

int unittest_platform_locale()
{
   int err;

   TEST(0 == execasprocess_unittest(&childprocess_unittest, &err));

   return 0;
ONERR:
   return EINVAL;
}


#endif
