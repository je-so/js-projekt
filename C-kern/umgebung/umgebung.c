/* title: Umgebung Generic
   Implementation file of generic init anf free functions.

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

   file: C-kern/umgebung/umgebung.c
    Implementation file of <Umgebung Generic>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/umgebung.h"
#include "C-kern/api/errlog.h"
#include "C-kern/api/umgebung/object_cache.h"
#define X11 1
#if (KONFIG_SUBSYS_GRAPHIK==X11)
// TEXTDB:SELECT('#include "'header-name'"')FROM(C-kern/resource/text.db/init_once_per_process)WHERE(subsystem==''||subsystem=='X11')
#include "C-kern/api/locale.h"
#include "C-kern/api/graphik/X11/x11.h"
// TEXTDB:END
#else
// TEXTDB:SELECT('#include "'header-name'"')FROM(C-kern/resource/text.db/init_once_per_process)WHERE(subsystem=='')
#include "C-kern/api/locale.h"
// TEXTDB:END
#endif
#undef X11
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
   int (*init_resource) (void) ;
   /* variable: free_resource
    * Called during execution of <free_process_umgebung>.
    * It is only called if <isInit> is true. */
   int (*free_resource) (void) ;
} ;


// section: Implementation

/* variable: gt_umgebung
 * Defines thread-local storage for global context <umgebung_t> which every modul can access. */
__thread umgebung_t   gt_umgebung = umgebung_INIT_MAINSERVICES ;

/* variable: s_registry
 * The static array of all registered resources. */
static resource_registry_t    s_registry[] = {
#define X11 1
#if (KONFIG_SUBSYS_GRAPHIK==X11)
// TEXTDB:SELECT("   { &"init-function", &"free-function" },")FROM("C-kern/resource/text.db/init_once_per_process")WHERE(subsystem==''||subsystem=='X11')
   { &init_once_per_process_locale, &free_once_per_process_locale },
   { &init_once_per_process_X11, &free_once_per_process_X11 },
// TEXTDB:END
#else
// TEXTDB:SELECT("   { &"init-function", &"free-function" },")FROM("C-kern/resource/text.db/init_once_per_process")WHERE(subsystem=='')
   { &init_once_per_process_locale, &free_once_per_process_locale },
// TEXTDB:END
#endif
#undef X11
   { 0, 0 }
} ;

/* variable: s_registry_init_count
 * Counts how many resources has been inituialized successfully. */
static uint16_t  s_registry_init_count = 0 ;


static int freeall_process_resources(void)
{
   int err = 0 ;

   while( s_registry_init_count ) {
      int err2 = s_registry[--s_registry_init_count].free_resource() ;
      if (err2) err = err2 ;
   }

   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

static int initall_process_resources(void)
{
   int err ;

   for(; s_registry[s_registry_init_count].init_resource ; ++s_registry_init_count) {
      err = s_registry[s_registry_init_count].init_resource() ;
      if (err) goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   (void) freeall_process_resources() ;
   LOG_ABORT(err) ;
   return err ;
}

int free_thread_umgebung(umgebung_t * umg)
{
   int err ;

   if (umg->free_umgebung) {
      err = umg->free_umgebung(umg) ;
      if (err) goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int init_thread_umgebung(umgebung_t * umg, umgebung_type_e implementation_type)
{
   int err ;

   if (     implementation_type <= umgebung_type_STATIC
         || implementation_type >  umgebung_type_TEST) {
      err = EINVAL ;
      LOG_INT(implementation_type) ;
      goto ABBRUCH ;
   }

   switch(implementation_type) {
   case umgebung_type_STATIC:    assert(0) ; break ;
   case umgebung_type_DEFAULT:   err = init_default_umgebung(umg) ; break ;
   case umgebung_type_TEST:      err = init_testproxy_umgebung(umg) ; break ;
   }

   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int free_process_umgebung(void)
{
   int err ;
   static_assert_void( ((typeof(umgebung()))0) == (const umgebung_t *)0 ) ;
   umgebung_t       * umg = (umgebung_t*) (intptr_t) umgebung() ;
   int     is_initialized = (0 != umg->type) ;

   if (is_initialized) {
      err = free_thread_umgebung(umg) ;
      assert(&g_safe_logservice == umg->log || &g_main_logservice == umg->log) ;
      umg->log = &g_main_logservice ;
      int err2 = freeall_process_resources() ;
      if (err2) err = err2 ;
      if (err) goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int init_process_umgebung(umgebung_type_e implementation_type)
{
   int err ;
   static_assert_void( ((typeof(umgebung()))0) == (const umgebung_t *)0 ) ;
   umgebung_t           * umg = (umgebung_t*) (intptr_t) umgebung() ;
   int is_already_initialized = (0 != umg->type) ;

   if (is_already_initialized) {
      err = EALREADY ;
      goto ABBRUCH ;
   }

   err = initall_process_resources() ;
   if (err) goto ABBRUCH ;
   umgebung_t temp_umg ;
   err = init_thread_umgebung(&temp_umg, implementation_type) ;
   if (err) goto ABBRUCH ;
   memcpy( umg, (const umgebung_t*)&temp_umg, sizeof(umgebung_t)) ;
   err = move_objectcache( umg->cache, &g_main_objectcache ) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   (void) freeall_process_resources() ;
   LOG_ABORT(err) ;
   return err ;
}


#ifdef KONFIG_UNITTEST

#define TEST(CONDITION) TEST_ONERROR_GOTO(CONDITION,unittest_umgebung,ABBRUCH)

static int test_query_service(void)
{
   const umgebung_t           * umg = umgebung() ;
   const log_config_t * log_service = log_umgebung() ;

   TEST( umg ) ;
   TEST( log_service ) ;

   TEST( umg         == &gt_umgebung ) ;
   TEST( log_service == gt_umgebung.log ) ;

   return 0 ;
ABBRUCH:
   return 1 ;
}

static int test_resource_setlocale(void)
{
   const char    * old_lcall = getenv("LC_ALL") ;

   // TEST setlocale error
   TEST(0 == setenv("LC_ALL", "XXX@unknown", 1)) ;
   TEST(EINVAL == init_process_umgebung(umgebung_type_DEFAULT)) ;
   TEST(0 == umgebung()->type) ;
   if (old_lcall) {
      TEST(0 == setenv("LC_ALL", old_lcall, 0)) ;
   } else {
      TEST(0 == unsetenv("LC_ALL")) ;
   }

   return 0 ;
ABBRUCH:
   if (old_lcall)
      setenv("LC_ALL", old_lcall, 1) ;
   else
      unsetenv("LC_ALL") ;
   return 1 ;
}

static int test_process_init(void)
{
   // TEST EINVAL: wrong type
   TEST(0      == umgebung_type_STATIC) ;
   TEST(EINVAL == init_process_umgebung(umgebung_type_STATIC)) ;
   TEST(EINVAL == init_process_umgebung(3)) ;

   // TEST init, double free
   TEST(0 == umgebung()->type) ;
   TEST(0 == init_process_umgebung(umgebung_type_DEFAULT)) ;
   TEST(umgebung_type_DEFAULT == umgebung()->type) ;
   TEST(0 == free_process_umgebung()) ;
   TEST(0 == umgebung()->type ) ;
   TEST(0 == free_process_umgebung()) ;
   TEST(0 == umgebung()->type ) ;

   // TEST init, double free
   TEST(0 == umgebung()->type) ;
   TEST(0 == init_process_umgebung(umgebung_type_TEST)) ;
   TEST(umgebung_type_TEST == umgebung()->type) ;
   TEST(0 == free_process_umgebung()) ;
   TEST(0 == umgebung()->type ) ;

   return 0 ;
ABBRUCH:
   return 1 ;
}

int unittest_umgebung()
{
   const umgebung_type_e old_type = umgebung()->type ;
   const int              is_init = (0 != old_type) ;

   TEST(!is_init) ;

   if (test_query_service())        goto ABBRUCH ;
   if (test_resource_setlocale())   goto ABBRUCH ;
   if (test_process_init())         goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   return 1 ;
}

#endif

