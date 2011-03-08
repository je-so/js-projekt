/* title: Umgebung Default
   Implements <init_default_umgebung>, <free_default_umgebung>.

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
    Header file of <Umgebung Interface>.

   file: C-kern/umgebung/umgebung_default.c
    Implementation file of <Umgebung Default>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/umgebung.h"
#include "C-kern/api/errlog.h"
// TEXTDB:SELECT('#include "'header-name'"')FROM("C-kern/resource/text.db/init_once_per_thread")
#include "C-kern/api/umgebung/log.h"
#include "C-kern/api/umgebung/object_cache.h"
// TEXTDB:END
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif

/* typedef: typedef resource_registry_t
 * Shortcut for <resource_registry_t>. */
typedef struct resource_registry_t resource_registry_t ;

/* typedef: typedef umgebung_private_t
 * Shortcut for <umgebung_private_t>. */
typedef struct umgebung_private_t  umgebung_private_t ;


/* struct: resource_registry_t
 * Registers init&free functions for one resource.
 * The registered functions are called during <init_process_umgebung> and <free_process_umgebung>. */
struct resource_registry_t {
   /* variable: init_resource
    * Called during execution of <init_process_umgebung>. */
   int (*init_resource) (/*inout*/umgebung_t * umg) ;
   /* variable: free_resource
    * Called during execution of <free_process_umgebung>.
    * It is only called if <isInit> is true. */
   int (*free_resource) (/*inout*/umgebung_t * umg) ;
} ;


// section: Implementation

/* variable: s_registry
 * The static array of all registered resources. */
static resource_registry_t    s_registry[] = {
// TEXTDB:SELECT("   { &"init-function", &"free-function" },")FROM(C-kern/resource/text.db/init_once_per_thread)
   { &init_once_per_thread_log, &free_once_per_thread_log },
   { &init_once_per_thread_objectcache, &free_once_per_thread_objectcache },
// TEXTDB:END
   { 0, 0 }
} ;


static int freeall_thread_resources(umgebung_t * umg)
{
   int err = 0 ;

   while( umg->resource_thread_count ) {
      int err2 = s_registry[--umg->resource_thread_count].free_resource(umg) ;
      if (err2) err = err2 ;
   }

   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

static int initall_thread_resources(umgebung_t * umg)
{
   int err ;

   for(; s_registry[umg->resource_thread_count].init_resource ; ++umg->resource_thread_count) {
      err = s_registry[umg->resource_thread_count].init_resource(umg) ;
      if (err) goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   (void) freeall_thread_resources(umg) ;
   LOG_ABORT(err) ;
   return err ;
}

// section: Implementation

static int free_default_umgebung(umgebung_t * umg)
{
   int err ;

   err = freeall_thread_resources(umg) ;

   umg->type           = 0 ;
   umg->free_umgebung  = 0 ;

   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int init_default_umgebung(umgebung_t * umg)
{
   int err ;

   umg->type                  = umgebung_type_DEFAULT ;
   umg->resource_thread_count = 0 ;
   umg->free_umgebung         = &free_default_umgebung ;
   umg->log                   = 0 ;
   umg->cache                 = 0 ;

   err = initall_thread_resources(umg) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}


#ifdef KONFIG_UNITTEST

#define TEST(CONDITION) TEST_ONERROR_GOTO(CONDITION,unittest_umgebung_default,ABBRUCH)

static int test_init(void)
{
   umgebung_t umg ;

   // TEST init, double free
   umg.type                  = 0 ;
   umg.resource_thread_count = 1000 ;
   TEST(0 == init_default_umgebung(&umg)) ;
   TEST(umgebung_type_DEFAULT == umg.type) ;
   TEST(2                     == umg.resource_thread_count) ;
   TEST(free_default_umgebung == umg.free_umgebung) ;
   TEST(0 == free_default_umgebung(&umg)) ;
   TEST(0 == umg.type) ;
   TEST(0 == umg.free_umgebung) ;
   TEST(0 == free_default_umgebung(&umg)) ;
   TEST(0 == umg.type) ;
   TEST(0 == umg.free_umgebung) ;

   return 0 ;
ABBRUCH:
   return 1 ;
}

int unittest_umgebung_default()
{
   const umgebung_type_e old_type = umgebung()->type ;
   const int              is_init = (0 != old_type) ;

   TEST(!is_init) ;

   if (test_init())   goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   return 1 ;
}

#endif
