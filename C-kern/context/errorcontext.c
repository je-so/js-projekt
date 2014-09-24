/* title: ErrorContext impl

   Implements <ErrorContext>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/platform/task/process.h"
#endif


// section: errorcontext_t

// group: global variables


/* variable: g_errorcontext_stroffset
 * Holds offset into array <g_errorcontext_strdata>. */
uint16_t g_errorcontext_stroffset[];

/* variable: g_errorcontext_strdata
 * Holds all message strings stored in one array.
 * Every string ends with a \0 byte. */
uint8_t g_errorcontext_strdata[];

// generated by text resource compiler
#include STR(C-kern/resource/generated/errtab.KONFIG_LANG)


// group: lifetime

int init_errorcontext(/*out*/errorcontext_t * errcontext)
{
   errcontext->stroffset = g_errorcontext_stroffset ;
   errcontext->strdata   = g_errorcontext_strdata ;
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
   errorcontext_t errcontext = errorcontext_FREE ;

   // TEST errorcontext_FREE
   TEST(0 == errcontext.stroffset) ;
   TEST(0 == errcontext.strdata) ;

   // TEST errorcontext_INIT_STATIC
   errcontext = (errorcontext_t) errorcontext_INIT_STATIC ;
   TEST(errcontext.stroffset == g_errorcontext_stroffset) ;
   TEST(errcontext.strdata   == g_errorcontext_strdata) ;

   // TEST init_errorcontext
   errcontext = (errorcontext_t) errorcontext_FREE ;
   TEST(0 == init_errorcontext(&errcontext)) ;
   TEST(errcontext.stroffset == g_errorcontext_stroffset) ;
   TEST(errcontext.strdata   == g_errorcontext_strdata) ;

   // TEST free_errorcontext
   TEST(0 == free_errorcontext(&errcontext)) ;
   TEST(0 == errcontext.stroffset) ;
   TEST(0 == errcontext.strdata) ;
   TEST(0 == free_errorcontext(&errcontext)) ;
   TEST(0 == errcontext.stroffset) ;
   TEST(0 == errcontext.strdata) ;

   return 0 ;
ONERR:
   return EINVAL ;
}

static int test_query_strerror(void * dummy)
{
   (void)dummy;

   setlocale(LC_MESSAGES, "C") ;

   // TEST maxsyserrnum_errorcontext: compares to strerror
   for (size_t i = maxsyserrnum_errorcontext()+1; i <= 255; ++i) {
      char buffer[50] ;
      snprintf(buffer, sizeof(buffer), "Unknown error %d", (int)i) ;
      TEST(0 == strcmp(strerror((int)i), buffer)) ;
   }

   return 0 ;
ONERR:
   return EINVAL ;
}

static int test_query(void)
{
   errorcontext_t errcontext = errorcontext_FREE ;

   // prepare
   TEST(0 == init_errorcontext(&errcontext)) ;

   // TEST maxsyserrnum_errorcontext: start of unknown error
   TEST(0   < maxsyserrnum_errorcontext()) ;
   TEST(255 > maxsyserrnum_errorcontext()) ;
   for (size_t i = maxsyserrnum_errorcontext()+1; i <= 255; ++i) {
      const char * errstr = (const char*) (g_errorcontext_strdata + g_errorcontext_stroffset[i]) ;
      TEST(0 == strcmp("Unknown error", errstr)) ;
      TEST(g_errorcontext_stroffset[i] == g_errorcontext_stroffset[1+maxsyserrnum_errorcontext()]) ;
   }

   // TEST maxsyserrnum_errorcontext: compare with strerror
   process_t child;
   process_result_t child_result;
   TEST(0 == init_process(&child, &test_query_strerror, 0, 0));
   TEST(0 == wait_process(&child, &child_result));
   TEST(0 == free_process(&child));
   TEST(0 == child_result.returncode);

   // TEST g_errorcontext_stroffset: 256(syserror) + 256(extended error)
   TEST(512 == lengthof(g_errorcontext_stroffset)) ;

   // TEST g_errorcontext_strdata: last byte is '\0' byte
   TEST(0 == g_errorcontext_strdata[lengthof(g_errorcontext_strdata)-1]) ;
   TEST(0 != g_errorcontext_strdata[lengthof(g_errorcontext_strdata)-2]) ;

   // TEST str_errorcontext: 0 <= errno <= 255
   for (size_t i = 0; i <= 255; ++i) {
      const char * errstr = (const char*) (g_errorcontext_strdata + g_errorcontext_stroffset[i]) ;
      TEST(lengthof(g_errorcontext_strdata) > g_errorcontext_stroffset[i]) ;
      TEST(lengthof(g_errorcontext_strdata) > g_errorcontext_stroffset[i] + strlen(errstr)) ;
      if (i <= maxsyserrnum_errorcontext()) {
         TEST(0 != strcmp(errstr, "Unknown error")) ;
         switch (i) {
         case 0:
            TEST(0 == strcmp(errstr, "Success")) ;
            break;
         case EPERM:
            TEST(0 == strcmp(errstr, "Operation not permitted")) ;
            break;
         case ENOENT:
            TEST(0 == strcmp(errstr, "No such file or directory")) ;
            break;
         case EBADR:
            TEST(0 == strcmp(errstr, "Invalid request descriptor")) ;
            break;
         case EREMCHG:
            TEST(0 == strcmp(errstr, "Remote address changed")) ;
            break;
         case EAFNOSUPPORT:
            TEST(0 == strcmp(errstr, "Address family not supported by protocol")) ;
            break;
         case ETIMEDOUT:
            TEST(0 == strcmp(errstr, "Connection timed out")) ;
            break;
         case EOWNERDEAD:
            TEST(0 == strcmp(errstr, "Owner died")) ;
            break;
         }
      } else {
         TEST(0 == strcmp(errstr, "Unknown error")) ;
      }
      TEST((const uint8_t*)errstr == str_errorcontext(errcontext, i)) ;
   }

   // TEST str_errorcontext: 256 <= errno <= 260
   for (size_t i = 256; i <= 260; ++i) {
      const uint8_t * errstr = g_errorcontext_strdata + g_errorcontext_stroffset[255] ;
      // valid content
      TEST(errstr != str_errorcontext(errcontext, i)) ;
   }

   // TEST str_errorcontext: 261 <= errno <= SIZE_MAX
   for (size_t i = 261; true; i = (i < 511) ? i : 2*i, ++i) {
      const uint8_t * errstr = g_errorcontext_strdata + g_errorcontext_stroffset[255] ;
      TEST(errstr == str_errorcontext(errcontext, i)) ;
      if (i == SIZE_MAX) break ;
   }

   // unprepare
   TEST(0 == free_errorcontext(&errcontext)) ;

   return 0 ;
ONERR:
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

   // TEST cast_errorcontext
   TEST(&errcontext                   == cast_errorcontext(&errcontext)) ;
   TEST((errorcontext_t*)&errcontext2 == cast_errorcontext(&errcontext2)) ;

   // TEST init_errorcontext
   TEST(0 == init_errorcontext(cast_errorcontext(&errcontext2))) ;

   // TEST str_errorcontext
   const uint8_t * errstr = str_errorcontext(errcontext2, maxsyserrnum_errorcontext()+1) ;
   TEST(errstr != 0) ;
   TEST(0 == strcmp((const char*)errstr, "Unknown error")) ;

   // TEST free_errorcontext
   TEST(0 == init_errorcontext(cast_errorcontext(&errcontext2))) ;

   return 0 ;
ONERR:
   return EINVAL ;
}

static int test_initonce(void)
{
   errorcontext_t errcontext = errorcontext_FREE ;


   // TEST initonce_errorcontext
   TEST(0 == initonce_errorcontext(&errcontext)) ;
   TEST(errcontext.stroffset == g_errorcontext_stroffset) ;
   TEST(errcontext.strdata   == g_errorcontext_strdata) ;

   // TEST freeonce_errorcontext: no-op
   TEST(0 == freeonce_errorcontext(&errcontext)) ;
   TEST(errcontext.stroffset == g_errorcontext_stroffset) ;
   TEST(errcontext.strdata   == g_errorcontext_strdata) ;

   return 0 ;
ONERR:
   return EINVAL ;
}


int unittest_context_errorcontext()
{
   if (test_initfree())       goto ONERR;
   if (test_query())          goto ONERR;
   if (test_generic())        goto ONERR;
   if (test_initonce())       goto ONERR;

   return 0 ;
ONERR:
   return EINVAL ;
}

#endif
