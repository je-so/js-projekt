/* title: Locale-support impl
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

   file: C-kern/os/shared/locale.c
    Implementation file <Locale-support impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/os/locale.h"
#include "C-kern/api/errlog.h"

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
