/* title: Umgebung Implementation
   Implements <init_umgebung> and defines global variables used to implement <umgebung>.

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

   file: C-kern/umgebung/umgebung_init.c
    Implementation file of <init_umgebung>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/umgebung.h"
#include "C-kern/api/errlog.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif

typedef struct resource_registry_t resource_registry_t ;

struct resource_registry_t {
   int (*init_resource) (umgebung_type_e implementation_type) ;
   int (*free_resource) (void) ;
   bool  isInit ;
} ;

static int initprocess_locale(umgebung_type_e implementation_type) ;
static int freeprocess_locale(void) ;
static int initprocess_umgebung(umgebung_type_e implementation_type) ;
static int freeprocess_umgebung(void) ;

/* variable: gt_current_umgebung
 * Assign space for global (thread-local storage) pointer to <umgebung_t>. */
__thread umgebung_t          *  gt_current_umgebung = 0 ;

/* variable: s_mainthread_umgebung
 * This is th reserved memory for <gt_current_umgebung> used in main thread. */
static umgebung_t             s_mainthread_umgebung = umgebung_INIT_FREEABLE ;

#define REG_ENTRY(module_name)   \
      { .init_resource = &initprocess_##module_name, .free_resource = freeprocess_##module_name, .isInit = false }

static resource_registry_t    s_registry[] = {
    REG_ENTRY(locale)
   ,REG_ENTRY(umgebung)
   ,{ 0, 0, false }
} ;

#undef REG_ENTRY

// locale support
static int initprocess_locale(umgebung_type_e implementation_type)
{
   int err ;

   (void) implementation_type ;

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

static int freeprocess_locale()
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

static int initprocess_umgebung(umgebung_type_e implementation_type)
{
   int err = EINVAL ;

   switch(implementation_type) {
   case umgebung_DEFAULT_IMPL:   err = init_umgebung(&s_mainthread_umgebung) ; break ;
   case umgebung_TEST_IMPL:      err = init_umgebung_testproxy(&s_mainthread_umgebung) ; break ;
   }
   if (err) goto ABBRUCH ;

   assert(s_mainthread_umgebung.type == implementation_type) ;
   assert(s_mainthread_umgebung.free_umgebung) ;

   gt_current_umgebung = &s_mainthread_umgebung ;
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

static int freeprocess_umgebung(void)
{
   int err ;

   if (gt_current_umgebung) {

      if (gt_current_umgebung != &s_mainthread_umgebung) {
         err = EINVAL ;
         goto ABBRUCH ;
      }

      gt_current_umgebung = 0 ;

      err = s_mainthread_umgebung.free_umgebung(&s_mainthread_umgebung) ;
      if (err) goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int init_umgebung(umgebung_t * umg)
{
   #if 0
   int err ;
   #endif

   MEMSET0(umg) ;
   umg->type          = umgebung_DEFAULT_IMPL ;
   umg->free_umgebung = &free_umgebung ;

   return 0 ;
   #if 0
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
   #endif
}

int free_umgebung(umgebung_t * umg)
{
   #if 0
   int err ;
   #endif

   MEMSET0(umg) ;

   return 0 ;
   #if 0
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
   #endif
}

int free_process_umgebung(void)
{
   int             err = 0 ;
   unsigned last_entry = 0 ;

   while( s_registry[last_entry].free_resource ) {
      ++ last_entry ;
   }

   while( last_entry ) {
      if (s_registry[--last_entry].isInit) {
         s_registry[last_entry].isInit = false ;

         int err2 = s_registry[last_entry].free_resource() ;
         if (err2) err = err2 ;
      }
   }
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int init_process_umgebung(umgebung_type_e implementation_type)
{
   int err ;
   int is_already_initialized = (0 != gt_current_umgebung) ;

   if (is_already_initialized) {
      err = EALREADY ;
      goto ABBRUCH ;
   }

   if (     implementation_type <= 0
         || implementation_type >  umgebung_TEST_IMPL) {
      err = EINVAL ;
      LOG_INT(implementation_type) ;
      goto ABBRUCH ;
   }

   // initialize
   for(unsigned i = 0; s_registry[i].init_resource ; ++i) {
      err = s_registry[i].init_resource(implementation_type) ;
      if (err) goto ABBRUCH ;
      s_registry[i].isInit = true ;
   }

   return 0 ;
ABBRUCH:
   (void) free_process_umgebung() ;
   LOG_ABORT(err) ;
   return err ;
}


#ifdef KONFIG_UNITTEST

#define TEST(CONDITION) TEST_ONERROR_GOTO(CONDITION,unittest_umgebung_initprocess,ABBRUCH)

static int test_setlocale(void)
{
   const char    * old_lcall = getenv("LC_ALL") ;

   // TEST setlocale error
   TEST(0 == setenv("LC_ALL", "XXX@unknown", 1)) ;
   TEST(EINVAL == init_process_umgebung(umgebung_TEST_IMPL)) ;
   TEST(0 == gt_current_umgebung) ;
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

static int test_init(void)
{
   // TEST init, double free
   TEST(0 == init_process_umgebung(umgebung_TEST_IMPL)) ;
   TEST(&s_mainthread_umgebung == gt_current_umgebung) ;
   TEST(0 == free_process_umgebung()) ;
   TEST(0 == gt_current_umgebung ) ;
   TEST(0 == free_process_umgebung()) ;
   TEST(0 == gt_current_umgebung ) ;

   // TEST EINVAL: wrong type
   TEST(EINVAL == init_process_umgebung(0)) ;
   TEST(EINVAL == init_process_umgebung(3)) ;

   return 0 ;
ABBRUCH:
   return 1 ;
}

int unittest_umgebung_initprocess(void)
{
   const int              is_init = (0 != gt_current_umgebung) ;
   const umgebung_type_e old_type = is_init ? gt_current_umgebung->type : 0 ;

   if (is_init) {
      TEST(0 == free_process_umgebung()) ;
      TEST(0 == gt_current_umgebung) ;
   }

   if (test_setlocale()) goto ABBRUCH ;
   if (test_init())      goto ABBRUCH ;

   if (is_init) {
      TEST(0 == init_process_umgebung(old_type)) ;
   }

   return 0 ;
ABBRUCH:
   if (is_init && !gt_current_umgebung) {
      init_process_umgebung(old_type) ;
   }
   return 1 ;
}

#endif
