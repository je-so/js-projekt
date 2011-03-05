/* title: Locale implementation
   Implement functions for localization support.

   - adapt to different character encodings

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
   (C) 2010 Jörg Seebohn

   file: C-kern/api/locale.h
    Header file of <Locale support>.

   file: C-kern/os/shared/locale.c
    Implementation file of <Locale support>.
*/
#include "C-kern/konfig.h"
#include "C-kern/api/locale.h"
#include "C-kern/api/errlog.h"

/* function: init_once_per_process_locale implementation
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
int init_once_per_process_locale()
{
   int err ;

   if (!setlocale(LC_ALL, "")) {
      LOG_TEXT(LOCALE_SETLOCALE) ;
      LOG_STRING(getenv("LC_ALL")) ;
      err = EINVAL ;
      goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

/* function: free_once_per_process_locale implementation
 * Calls C99 conforming function »setlocale«.
 * Set all subsystems of the C runtime environment to the standard locale "C"
 * which is active by default after process creation. */
int free_once_per_process_locale()
{
   int err ;

   if (!setlocale(LC_ALL, "C")) {
      LOG_TEXT(LOCALE_SETLOCALE) ;
      LOG_STRING("LC_ALL=C") ;
      err = EINVAL ;
      goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

/* function: charencoding_locale implementation
 * See <charencoding_locale> for interface description.
 * Calls POSIX conforming nl_langinfo to query the information. */
const char* charencoding_locale()
{
   return nl_langinfo(CODESET) ;
}

