/* title: ErrorContext impl

   Implements <ErrorContext>.

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
   (C) 2013 Jörg Seebohn

   file: C-kern/api/context/errorcontext.h
    Header file <ErrorContext>.

   file: C-kern/context/errorcontext.c
    Implementation file <ErrorContext impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/context/errorcontext.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


// section: errorcontext_t

// group: variables

#include "C-kern/resource/generated/errtable.h"

// group: lifetime

int init_errorcontext(/*out*/errorcontext_t * errcontext)
{
   errcontext->stroffset = s_errorcontext_stroffset ;
   errcontext->strdata   = s_errorcontext_strdata ;
   return 0 ;
}

int free_errorcontext(errorcontext_t * errcontext)
{
   errcontext->stroffset = 0 ;
   errcontext->strdata   = 0 ;
   return 0 ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   errorcontext_t errcontext = errorcontext_INIT_FREEABLE ;

   // TEST errorcontext_INIT_FREEABLE
   TEST(0 == errcontext.stroffset) ;
   TEST(0 == errcontext.strdata) ;

   // TEST init_errorcontext, free_errorcontext
   TEST(0 == init_errorcontext(&errcontext)) ;
   TEST(errcontext.stroffset == s_errorcontext_stroffset) ;
   TEST(errcontext.strdata   == s_errorcontext_strdata) ;
   TEST(0 == free_errorcontext(&errcontext)) ;
   TEST(0 == errcontext.stroffset) ;
   TEST(0 == errcontext.strdata) ;
   TEST(0 == free_errorcontext(&errcontext)) ;
   TEST(0 == errcontext.stroffset) ;
   TEST(0 == errcontext.strdata) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_query(void)
{
   errorcontext_t errcontext = errorcontext_INIT_FREEABLE ;

   // prepare
   TEST(0 == init_errorcontext(&errcontext)) ;

   // TEST maxsyserrnum_errorcontext
   TEST(0   <  maxsyserrnum_errorcontext()) ;
   TEST(255 >= maxsyserrnum_errorcontext()) ;
   for (size_t i = maxsyserrnum_errorcontext()+1; i <= 255; ++i) {
      char buffer[50] ;
      snprintf(buffer, sizeof(buffer), "Unknown error %d", (int)i) ;
      TEST(0 == strcmp(strerror((int)i), buffer)) ;
      const char * errstr = (const char*) (s_errorcontext_strdata + s_errorcontext_stroffset[i]) ;
      TEST(0 == strcmp("Unknown error", errstr)) ;
      TEST(s_errorcontext_stroffset[i]      == s_errorcontext_stroffset[1+maxsyserrnum_errorcontext()]) ;
      TEST(lengthof(s_errorcontext_strdata) == s_errorcontext_stroffset[1+maxsyserrnum_errorcontext()]+strlen(errstr)+1) ;
   }

   // TEST string_errorcontext: 0 <= errno <= 255
   // s_errorcontext_stroffset has 256 entries so string_errorcontext can use (uint8_t) to mask index
   TEST(256 == lengthof(s_errorcontext_stroffset)) ;
   // s_errorcontext_strdata: last byte is '\0' byte
   TEST(0 == s_errorcontext_strdata[lengthof(s_errorcontext_strdata)-1]) ;
   for (size_t i = 0; i <= 255; ++i) {
      const char * errstr = (const char*) (s_errorcontext_strdata + s_errorcontext_stroffset[i]) ;
      TEST(lengthof(s_errorcontext_strdata) > s_errorcontext_stroffset[i]) ;
      TEST(lengthof(s_errorcontext_strdata) > s_errorcontext_stroffset[i] + strlen(errstr)) ;
      if (i <= maxsyserrnum_errorcontext()) {
         TEST(0 == strcmp(errstr, strerror((int)i))) ;
      } else {
         TEST(0 == strcmp(errstr, "Unknown error")) ;
      }
      TEST((const uint8_t*)errstr == string_errorcontext(&errcontext, i)) ;
   }

   // TEST string_errorcontext: 255 <= errno <= SIZE_MAX
   for (size_t i = 255; true; i *= 2, ++i) {
      const uint8_t * errstr = s_errorcontext_strdata + s_errorcontext_stroffset[255] ;
      TEST(errstr == string_errorcontext(&errcontext, i)) ;
      if (i == SIZE_MAX) break ;
   }

   // unprepare
   TEST(0 == free_errorcontext(&errcontext)) ;

   return 0 ;
ONABORT:
   free_errorcontext(&errcontext) ;
   return EINVAL ;
}

static int test_generic(void)
{
   errorcontext_t errcontext ;
   struct {
      uint16_t *  stroffset ;
      uint8_t  *  strdata ;
   }              errcontext2 ;

   // TEST genericcast_errorcontext
   TEST(&errcontext                   == genericcast_errorcontext(&errcontext)) ;
   TEST((errorcontext_t*)&errcontext2 == genericcast_errorcontext(&errcontext2)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}


int unittest_context_errorcontext()
{
   resourceusage_t   usage     = resourceusage_INIT_FREEABLE ;
   char *            msglocale = 0 ;

   // switch system error messages returned from strerror to english
   msglocale = setlocale(LC_MESSAGES, "C") ;
   if (msglocale) msglocale = strdup(msglocale) ;

   if (test_query())          goto ONABORT ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())       goto ONABORT ;
   if (test_query())          goto ONABORT ;
   if (test_generic())        goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   setlocale(LC_MESSAGES, msglocale ? msglocale : "") ;
   free(msglocale) ;

   return 0 ;
ONABORT:
   (void) free(msglocale) ;
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
