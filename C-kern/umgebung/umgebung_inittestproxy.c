/* title: Umgebung Testproxy
   Implements <inittestproxy_umgebung>, <freetestproxy_umgebung>.

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

   file: C-kern/umgebung/umgebung_inittestproxy.c
    Implementation file <Umgebung Testproxy>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/umgebung.h"
#include "C-kern/api/errlog.h"
#include "C-kern/api/umgebung/object_cache.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


int freetestproxy_umgebung(umgebung_t * umg)
{
   *umg = (umgebung_t) umgebung_INIT_MAINSERVICES ;
   return 0 ;
}

int inittestproxy_umgebung(umgebung_t * umg)
{
   *umg = (umgebung_t) umgebung_INIT_MAINSERVICES ;

   umg->type            = umgebung_type_TEST ;
   umg->resource_count  = 0 ;
   umg->free_umgebung   = &freetestproxy_umgebung ;

   return 0 ;
}

#ifdef KONFIG_UNITTEST

#define TEST(CONDITION) TEST_ONERROR_GOTO(CONDITION,unittest_umgebung_testproxy,ABBRUCH)

static int test_init(void)
{
   umgebung_t umg ;

   // TEST init, double free
   TEST(0 == inittestproxy_umgebung(&umg)) ;
   TEST(umgebung_type_TEST       == umg.type) ;
   TEST(0                        == umg.resource_count) ;
   TEST(&freetestproxy_umgebung  == umg.free_umgebung) ;
   TEST(0 == freetestproxy_umgebung(&umg)) ;
   TEST(0 == umg.type) ;
   TEST(0 == umg.resource_count) ;
   TEST(0 == umg.free_umgebung) ;
   TEST(0 == freetestproxy_umgebung(&umg)) ;
   TEST(0 == umg.type) ;
   TEST(0 == umg.resource_count) ;
   TEST(0 == umg.free_umgebung) ;

   return 0 ;
ABBRUCH:
   return 1 ;
}

int unittest_umgebung_testproxy()
{
   if (test_init()) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   return 1 ;
}

#endif
