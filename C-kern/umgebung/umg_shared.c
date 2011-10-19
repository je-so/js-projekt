/* title: Shared-Umgebung impl
   Implements <Shared-Umgebung>.

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

   file: C-kern/api/umg/umg_shared.h
    Header file of <Shared-Umgebung>.

   file: C-kern/umgebung/umg_shared.c
    Implementation file <Shared-Umgebung impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/umg/umg_shared.h"
#include "C-kern/api/err.h"
// TEXTDB:SELECT('#include "'header-name'"')FROM("C-kern/resource/text.db/initumgebung")WHERE(shared=='shared')
#include "C-kern/api/cache/valuecache.h"
// TEXTDB:END
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


static int free2_umgebungshared(umgebung_shared_t * shared, int16_t init_count)
{
   int err = 0 ;
   int err2 ;
   (void) shared ;

   switch(init_count) {
   default: assert(0 == init_count && "out of bounds" )  ;
            break ;
   case -1: // free all !
// TEXTDB:SELECT(\n"   case "row-id":  err2 = freeumgebung_"module"("(if (parameter!="") "&shared->" else "")parameter(if (parameter!="") ", ")"shared) ;"\n"            if (err2) err = err2 ;")FROM("C-kern/resource/text.db/initumgebung")WHERE(shared=='shared')

   case 1:  err2 = freeumgebung_valuecache(&shared->valuecache, shared) ;
            if (err2) err = err2 ;
// TEXTDB:END
   case 0:  break ;
   }


   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}

int free_umgebungshared(umgebung_shared_t * shared)
{
   return free2_umgebungshared(shared, -1) ;
}

int init_umgebungshared(umgebung_shared_t * shared)
{
   int err ;
   int16_t init_count = 0 ;
   (void) shared ;

// TEXTDB:SELECT(\n"   err = initumgebung_"module"("(if (parameter!="") "&shared->" else "")parameter(if (parameter!="") ", ")"shared) ;"\n"   if (err) goto ABBRUCH ;"\n"   ++ init_count ;")FROM("C-kern/resource/text.db/initumgebung")WHERE(shared=='shared')

   err = initumgebung_valuecache(&shared->valuecache, shared) ;
   if (err) goto ABBRUCH ;
   ++ init_count ;
// TEXTDB:END

   return 0 ;
ABBRUCH:
   (void) free2_umgebungshared(shared, init_count) ;
   LOG_ABORT(err) ;
   return err ;
}

#ifdef KONFIG_UNITTEST

#define TEST(CONDITION) TEST_ONERROR_GOTO(CONDITION,unittest_umgebung_shared,ABBRUCH)

static int test_initfree(void)
{
   umgebung_shared_t    shared = umgebung_shared_INIT_FREEABLE ;

   // TEST static init
   TEST(0 == shared.valuecache) ;

   // TEST init, double free
   TEST(0 == init_umgebungshared(&shared)) ;
   TEST(shared.valuecache) ;
   TEST(0 == free_umgebungshared(&shared)) ;
   TEST(0 == shared.valuecache) ;
   TEST(0 == free_umgebungshared(&shared)) ;
   TEST(0 == shared.valuecache) ;

   // free2 does nothing in case init_count == 0 ;
   shared.valuecache = (struct valuecache_t*) 1 ;
   TEST(0 == free2_umgebungshared(&shared, 0)) ;
   TEST(shared.valuecache == (struct valuecache_t*) 1) ;
   shared.valuecache = 0 ;

   // TEST init EINVAL (valuecache) does not change valuecache
   shared.valuecache = (struct valuecache_t*) 2 ;
   TEST(EINVAL == init_umgebungshared(&shared)) ;
   TEST(shared.valuecache == (struct valuecache_t*) 2) ;
   shared.valuecache = 0 ;

   return 0 ;
ABBRUCH:
   (void) free_umgebungshared(&shared) ;
   return EINVAL ;
}

int unittest_umgebung_shared()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   // store current mapping
   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())            goto ABBRUCH ;

   // TEST mapping has not changed
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
