/* title: LogUmgebung impl
   Implements <LogUmgebung>.

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

   file: C-kern/api/umgebung/log.h
    Header file of <LogUmgebung>.

   file: C-kern/umgebung/log.c
    Implementation file <LogUmgebung impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/umgebung/log.h"
#include "C-kern/api/errlog.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif

// section: Implementation

// group: init

int initumgebung_log(log_config_t ** log)
{
   int err ;

   err = new_logconfig( log ) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int freeumgebung_log(log_config_t ** log)
{
   int err ;
   log_config_t * log2 = *log ;

   *log = &g_main_logservice ;

   err = delete_logconfig( &log2 ) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

// group: test

#ifdef KONFIG_UNITTEST

#define TEST(CONDITION) TEST_ONERROR_GOTO(CONDITION,unittest_umgebung_log,ABBRUCH)

static int test_initumgebung(void)
{
   log_config_t * log = 0 ;

   // TEST EINVAL initumgebung
   log = (log_config_t*) 1 ;
   TEST(EINVAL == initumgebung_log(&log)) ;

   // TEST initumgebung, double freeumgebung
   log = 0 ;
   TEST(0 == initumgebung_log(&log)) ;
   TEST(log) ;
   TEST(0 == freeumgebung_log(&log)) ;
   TEST(&g_main_logservice == log) ;
   TEST(0 == freeumgebung_log(&log)) ;
   TEST(&g_main_logservice == log) ;

   return 0 ;
ABBRUCH:
   return 1 ;
}

int unittest_umgebung_log()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   // store current mapping
   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initumgebung())   goto ABBRUCH ;

   // TEST mapping has not changed
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
