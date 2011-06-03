/* title: UmgebungLocale impl
   Implement <UmgebungLocale>

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

   file: C-kern/api/umgebung/locale.h
    Header file of <UmgebungLocale>.

   file: C-kern/umgebung/locale.c
    Implementation file <UmgebungLocale impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/umgebung/locale.h"
#include "C-kern/api/errlog.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/test/malloctest.h"
#include "C-kern/api/os/virtmemory.h"
#endif


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

// section: test

#ifdef KONFIG_UNITTEST

#define TEST(CONDITION) TEST_ONERROR_GOTO(CONDITION,unittest_umgebung_locale,ABBRUCH)


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

int unittest_umgebung_locale()
{
   vm_mappedregions_t mappedregions  = vm_mappedregions_INIT_FREEABLE ;
   vm_mappedregions_t mappedregions2 = vm_mappedregions_INIT_FREEABLE ;
   size_t             malloced_bytes = allocatedsize_malloctest() ;
   char             * old_locale     = strdup(current_locale()) ;
   char             * old_msglocale  = strdup(currentmsg_locale()) ;

   // store current mapping
   TEST(0 == init_vmmappedregions(&mappedregions)) ;

   if (test_initlocale())   goto ABBRUCH ;

   // TEST mapping has not changed
   trimmemory_malloctest() ;
   TEST(0 == init_vmmappedregions(&mappedregions2)) ;
   TEST(0 == compare_vmmappedregions(&mappedregions, &mappedregions2)) ;
   TEST(0 == free_vmmappedregions(&mappedregions)) ;
   TEST(0 == free_vmmappedregions(&mappedregions2)) ;
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
   TEST(malloced_bytes == allocatedsize_malloctest()) ;

   // changes malloced memory
   if (test_initerror())   goto ABBRUCH ;

   // store current mapping
   malloced_bytes = allocatedsize_malloctest() ;
   TEST(0 == init_vmmappedregions(&mappedregions)) ;

   if (test_initerror())   goto ABBRUCH ;

   // TEST mapping has not changed
   TEST(0 == init_vmmappedregions(&mappedregions2)) ;
   TEST(0 == compare_vmmappedregions(&mappedregions, &mappedregions2)) ;
   TEST(0 == free_vmmappedregions(&mappedregions)) ;
   TEST(0 == free_vmmappedregions(&mappedregions2)) ;
   TEST(malloced_bytes == allocatedsize_malloctest()) ;

   return 0 ;
ABBRUCH:
   free(old_locale) ;
   free(old_msglocale) ;
   return 1 ;
}

#endif
