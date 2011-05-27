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
#if (KONFIG_GRAPHIK==X11)
// TEXTDB:SELECT('#include "'header-name'"')FROM(C-kern/resource/text.db/initprocess)WHERE(subsystem==''||subsystem=='X11')
#include "C-kern/api/locale.h"
// TEXTDB:END
#else
// TEXTDB:SELECT('#include "'header-name'"')FROM(C-kern/resource/text.db/initprocess)WHERE(subsystem=='')
#include "C-kern/api/locale.h"
// TEXTDB:END
#endif
#undef X11
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


// section: Implementation

/* variable: gt_umgebung
 * Defines thread-local storage for global context <umgebung_t> which every modul can access.
 * */
__thread umgebung_t          gt_umgebung = umgebung_INIT_MAINSERVICES ;

/* variable: s_initpos_presource
 * Rememberes how many resources has been initialized successfully.
 * Used in <init_process_resources> and <free_process_resources>
 * which are called from <initprocess_umgebung> and <freeprocess_umgebung>. */
static uint16_t    s_initpos_presource = 0 ;


static int free_process_resources(void)
{
   int err = 0 ;
   int err2 ;

   switch(s_initpos_presource) {
   default: assert(0 == s_initpos_presource && "out of bounds" )  ;
            break ;
#define X11 1
#if (KONFIG_GRAPHIK==X11)
// TEXTDB:SELECT("   case "row-id":  err2 = "free-function"() ;"\n"            if (err2) err=err2 ;")FROM("C-kern/resource/text.db/initprocess")WHERE(subsystem==''||subsystem=='X11')DESCENDING
   case 1:  err2 = freeprocess_locale() ;
            if (err2) err=err2 ;
// TEXTDB:END
#else
// TEXTDB:SELECT("   case "row-id":  err2 = "free-function"() ;"\n"            if (err2) err=err2 ;")FROM("C-kern/resource/text.db/initprocess")WHERE(subsystem=='')DESCENDING
   case 1:  err2 = freeprocess_locale() ;
            if (err2) err=err2 ;
// TEXTDB:END
#endif
#undef X11
   case 0:  break ;
   }

   s_initpos_presource = 0 ;

   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

static int init_process_resources(void)
{
   int err ;

   if (s_initpos_presource) {
      return 0 ;
   }

#define X11 1
#if (KONFIG_GRAPHIK==X11)
// TEXTDB:SELECT(\n"   err = "init-function"() ;"\n"   if (err) goto ABBRUCH ;"\n"   ++ s_initpos_presource ;")FROM("C-kern/resource/text.db/initprocess")WHERE(subsystem==''||subsystem=='X11')

   err = initprocess_locale() ;
   if (err) goto ABBRUCH ;
   ++ s_initpos_presource ;
// TEXTDB:END

#else
// TEXTDB:SELECT(\n"   err = "init-function"() ;"\n"   if (err) goto ABBRUCH ;"\n"   ++ s_initpos_presource ;")FROM("C-kern/resource/text.db/initprocess")WHERE(subsystem=='')

   err = initprocess_locale() ;
   if (err) goto ABBRUCH ;
   ++ s_initpos_presource ;
// TEXTDB:END
#endif
#undef X11

   return 0 ;
ABBRUCH:
   (void) free_process_resources() ;
   LOG_ABORT(err) ;
   return err ;
}

int free_umgebung(umgebung_t * umg)
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

int init_umgebung(umgebung_t * umg, umgebung_type_e implementation_type)
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
   case umgebung_type_DEFAULT:   err = initdefault_umgebung(umg) ; break ;
   case umgebung_type_TEST:      err = inittestproxy_umgebung(umg) ; break ;
   }

   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int freeprocess_umgebung(void)
{
   int err ;
   static_assert_void( ((typeof(umgebung()))0) == (const umgebung_t *)0 ) ;
   umgebung_t       * umg = (umgebung_t*) (intptr_t) umgebung() ;
   int     is_initialized = (0 != umg->type) ;

   if (is_initialized) {
      err = free_umgebung(umg) ;
      assert(&g_safe_logservice == umg->log || &g_main_logservice == umg->log) ;
      assert(!umg->cache || umg->cache == &g_main_objectcache) ;
      *umg = (umgebung_t) umgebung_INIT_MAINSERVICES ;
      int err2 = free_process_resources() ;
      if (err2) err = err2 ;
      if (err) goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int initprocess_umgebung(umgebung_type_e implementation_type)
{
   int err ;
   static_assert_void( ((typeof(umgebung()))0) == (const umgebung_t *)0 ) ;
   umgebung_t           * umg = (umgebung_t*) (intptr_t) umgebung() ;
   umgebung_t        temp_umg = umgebung_INIT_FREEABLE ;
   int is_already_initialized = (0 != umg->type) ;

   if (is_already_initialized) {
      err = EALREADY ;
      goto ABBRUCH ;
   }

   err = init_process_resources() ;
   if (err) goto ABBRUCH ;

   err = init_umgebung(&temp_umg, implementation_type) ;
   if (err) goto ABBRUCH ;

   err = move_objectcache( temp_umg.cache, &g_main_objectcache ) ;
   if (err) goto ABBRUCH ;

   // move is simple memcpy !
   memcpy( umg, (const umgebung_t*)&temp_umg, sizeof(umgebung_t)) ;

   return 0 ;
ABBRUCH:
   (void) free_umgebung(&temp_umg) ;
   (void) free_process_resources() ;
   LOG_ABORT(err) ;
   return err ;
}


#ifdef KONFIG_UNITTEST

#define TEST(CONDITION) TEST_ONERROR_GOTO(CONDITION,unittest_umgebung,ABBRUCH)

static int test_resource_setlocale(void)
{
   const char    * old_lcall = getenv("LC_ALL") ;

   // TEST setlocale error
   TEST(0 == setenv("LC_ALL", "XXX@unknown", 1)) ;
   TEST(0 == s_initpos_presource) ;
   TEST(EINVAL == initprocess_umgebung(umgebung_type_DEFAULT)) ;
   TEST(0 == s_initpos_presource) ;
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
   TEST(EINVAL == initprocess_umgebung(umgebung_type_STATIC)) ;
   TEST(EINVAL == initprocess_umgebung(3)) ;

   // TEST init, double free
   TEST(0 == umgebung()->type) ;
   TEST(0 == initprocess_umgebung(umgebung_type_DEFAULT)) ;
   TEST(umgebung_type_DEFAULT == umgebung()->type) ;
   TEST(0 != umgebung()->resource_count) ;
   TEST(0 != umgebung()->free_umgebung) ;
   TEST(0 != umgebung()->log) ;
   TEST(0 != umgebung()->cache) ;
   TEST(&g_main_logservice  != umgebung()->log) ;
   TEST(&g_main_objectcache != umgebung()->cache) ;
   TEST(0 == freeprocess_umgebung()) ;
   TEST(0 == umgebung()->type ) ;
   TEST(0 == umgebung()->resource_count) ;
   TEST(0 == umgebung()->free_umgebung) ;
   TEST(&g_main_logservice  == umgebung()->log) ;
   TEST(&g_main_objectcache == umgebung()->cache) ;
   TEST(0 == freeprocess_umgebung()) ;
   TEST(0 == umgebung()->type ) ;
   TEST(0 == umgebung()->resource_count) ;
   TEST(0 == umgebung()->free_umgebung) ;
   TEST(&g_main_logservice  == umgebung()->log) ;
   TEST(&g_main_objectcache == umgebung()->cache) ;

   // TEST init, double free
   TEST(0 == umgebung()->type) ;
   TEST(0 == initprocess_umgebung(umgebung_type_TEST)) ;
   TEST(umgebung_type_TEST  == umgebung()->type) ;
   TEST(&g_main_logservice  == umgebung()->log) ;
   TEST(&g_main_objectcache == umgebung()->cache) ;
   TEST(0 == umgebung()->resource_count) ;
   TEST(0 != umgebung()->free_umgebung) ;
   TEST(0 == freeprocess_umgebung()) ;
   TEST(0 == umgebung()->type ) ;
   TEST(0 == umgebung()->resource_count) ;
   TEST(0 == umgebung()->free_umgebung) ;
   TEST(&g_main_logservice  == umgebung()->log) ;
   TEST(&g_main_objectcache == umgebung()->cache) ;

   return 0 ;
ABBRUCH:
   return 1 ;
}

int unittest_umgebung()
{
   const umgebung_t    * umg      = umgebung() ;
   umgebung_type_e       old_type = umg ? umg->type : 0 ;

   if (umgebung_type_STATIC != old_type) {
      TEST(0 == freeprocess_umgebung()) ;
   }

   // TEST static init
   TEST( umg->type           == umgebung_type_STATIC ) ;
   TEST( umg->cache          == &g_main_objectcache );
   TEST( umg->log            == &g_main_logservice );
   TEST( umg->free_umgebung  == 0 ) ;
   TEST( umg->resource_count == 0 ) ;

   // TEST query umgebung
   TEST( 0 != umgebung() ) ;
   TEST( 0 != log_umgebung() ) ;
   TEST( 0 != cache_umgebung() ) ;
   {
      TEST( umgebung() == &gt_umgebung ) ;
   }
   {
      log_config_t * oldlog = gt_umgebung.log ;
      gt_umgebung.log = (log_config_t *) 0 ;
      TEST( ! log_umgebung() ) ;
      gt_umgebung.log = oldlog ;
      TEST( log_umgebung() == oldlog ) ;
   }
   {
      object_cache_t * oldcache = gt_umgebung.cache ;
      gt_umgebung.cache = (object_cache_t *) 0 ;
      TEST( ! cache_umgebung() ) ;
      gt_umgebung.cache = oldcache ;
      TEST( cache_umgebung() == oldcache ) ;
   }

   if (test_resource_setlocale())   goto ABBRUCH ;
   if (test_process_init())         goto ABBRUCH ;

   // TEST static init has not changed
   TEST( umg->type           == umgebung_type_STATIC ) ;
   TEST( umg->cache          == &g_main_objectcache );
   TEST( umg->log            == &g_main_logservice );
   TEST( umg->free_umgebung  == 0 ) ;
   TEST( umg->resource_count == 0 ) ;

   if (umgebung_type_STATIC != old_type) {
      TEST(0 == initprocess_umgebung(old_type)) ;
   }

   return 0 ;
ABBRUCH:
   return 1 ;
}

#endif

