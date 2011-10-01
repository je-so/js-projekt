/* title: Umgebung Default
   Implements <initdefault_umgebung>, <freedefault_umgebung>.

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

   file: C-kern/api/umgebung.h
    Header file of <Umgebung>.

   file: C-kern/umgebung/umgebung_initdefault.c
    Implementation file of <Umgebung Default>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/umgebung.h"
#include "C-kern/api/err.h"
// TEXTDB:SELECT('#include "'header-name'"')FROM("C-kern/resource/text.db/initumgebung")
#include "C-kern/api/cache/valuecache.h"
#include "C-kern/api/cache/objectcache.h"
#include "C-kern/api/writer/log.h"
#include "C-kern/api/umgebung/testerror.h"
// TEXTDB:END
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif

/* typedef: typedef resource_registry_t
 * Shortcut for <resource_registry_t>. */
typedef struct resource_registry_t  resource_registry_t ;

/* typedef: typedef umgebung_private_t
 * Shortcut for <umgebung_private_t>. */
typedef struct umgebung_private_t   umgebung_private_t ;


// section: Implementation

static int free_thread_resources(umgebung_t * umg)
{
   int err = 0 ;
   int err2 ;

   switch(umg->resource_count) {
   default:    assert(0 == umg->resource_count && "out of bounds") ;
               break ;
// TEXTDB:SELECT("   case "row-id":     err2 = "free-function"("(if (parameter!="") "&umg->" else "")parameter") ;"\n"               if (err2) err = err2 ;")FROM(C-kern/resource/text.db/initumgebung)DESCENDING
   case 4:     err2 = freeumgebung_umgebungtesterror() ;
               if (err2) err = err2 ;
   case 3:     err2 = freeumgebung_log(&umg->log) ;
               if (err2) err = err2 ;
   case 2:     err2 = freeumgebung_objectcache(&umg->objectcache) ;
               if (err2) err = err2 ;
   case 1:     err2 = freeumgebung_valuecache(&umg->valuecache) ;
               if (err2) err = err2 ;
// TEXTDB:END
   case 0:     break ;
   }

   umg->resource_count = 0 ;

   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

static int init_thread_resources(umgebung_t * umg)
{
   int err ;

// TEXTDB:SELECT(\n"   err = "init-function"("(if (parameter!="") "&umg->")parameter") ;"\n"   if (err) goto ABBRUCH ;"\n"   ++umg->resource_count ;")FROM(C-kern/resource/text.db/initumgebung)

   err = initumgebung_valuecache(&umg->valuecache) ;
   if (err) goto ABBRUCH ;
   ++umg->resource_count ;

   err = initumgebung_objectcache(&umg->objectcache) ;
   if (err) goto ABBRUCH ;
   ++umg->resource_count ;

   err = initumgebung_log(&umg->log) ;
   if (err) goto ABBRUCH ;
   ++umg->resource_count ;

   err = initumgebung_umgebungtesterror() ;
   if (err) goto ABBRUCH ;
   ++umg->resource_count ;
// TEXTDB:END

   return 0 ;
ABBRUCH:
   (void) free_thread_resources(umg) ;
   LOG_ABORT(err) ;
   return err ;
}

// section: Implementation

static int freedefault_umgebung(umgebung_t * umg)
{
   int err ;

   err = free_thread_resources(umg) ;

   umg->type           = 0 ;
   umg->free_umgebung  = 0 ;

   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int initdefault_umgebung(umgebung_t * umg)
{
   int err ;

   umg->type            = umgebung_type_DEFAULT ;
   umg->resource_count  = 0 ;
   umg->free_umgebung   = &freedefault_umgebung ;
   umg->log             = &g_main_logservice ;
   umg->objectcache     = 0 ;
   umg->valuecache      = 0 ;

   err = init_thread_resources(umg) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   (void) freedefault_umgebung(umg) ;
   LOG_ABORT(err) ;
   return err ;
}


#ifdef KONFIG_UNITTEST

#define TEST(CONDITION) TEST_ONERROR_GOTO(CONDITION,unittest_umgebung_initdefault,ABBRUCH)

static int test_init(void)
{
   umgebung_t umg = umgebung_INIT_FREEABLE ;

   // TEST init, double free
   MEMSET0(&umg) ;
   TEST(0 == initdefault_umgebung(&umg)) ;
   TEST(umgebung_type_DEFAULT == umg.type) ;
   TEST(4                     == umg.resource_count) ;
   TEST(freedefault_umgebung  == umg.free_umgebung) ;
   TEST(0 != umg.log) ;
   TEST(&g_main_logservice != umg.log) ;
   TEST(0 != umg.objectcache) ;
   TEST(0 != umg.valuecache) ;
   TEST(0 == freedefault_umgebung(&umg)) ;
   TEST(0 == umg.type) ;
   TEST(0 == umg.resource_count) ;
   TEST(0 == umg.free_umgebung) ;
   TEST(&g_main_logservice == umg.log) ;
   TEST(0 == umg.objectcache) ;
   TEST(0 == umg.valuecache) ;
   TEST(0 == freedefault_umgebung(&umg)) ;
   TEST(0 == umg.type) ;
   TEST(0 == umg.resource_count) ;
   TEST(0 == umg.free_umgebung) ;
   TEST(&g_main_logservice == umg.log) ;
   TEST(0 == umg.objectcache) ;
   TEST(0 == umg.valuecache) ;

   // TEST EINVAL init
   for(int err = EINVAL; err < EINVAL+2; ++err) {
      setiniterror_umgebungtesterror(err) ;
      umg = (umgebung_t) umgebung_INIT_FREEABLE ;
      TEST(err == initdefault_umgebung(&umg)) ;
      TEST(0 == umg.type) ;
      TEST(0 == umg.resource_count) ;
      TEST(0 == umg.free_umgebung) ;
      TEST(&g_main_logservice == umg.log) ;
      TEST(0 == umg.objectcache) ;
      TEST(0 == umg.valuecache) ;
   }
   cleariniterror_umgebungtesterror() ;

   return 0 ;
ABBRUCH:
   cleariniterror_umgebungtesterror() ;
   return EINVAL ;
}

int unittest_umgebung_initdefault()
{
   if (test_init())   goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   return 1 ;
}

#endif
