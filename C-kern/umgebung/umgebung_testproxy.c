/* Umgebung Testproxy
   Implementiert <init_umgebung_testproxy>.

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

   file: C-kern/umgebung/umgebung_testproxy.c
    Implementation file of <Umgebung Testproxy>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/umgebung.h"
#include "C-kern/api/errlog.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


int free_umgebung_testproxy(umgebung_t * umg)
{
   MEMSET0(umg) ;
   return 0 ;
}

int init_umgebung_testproxy(umgebung_t * umg)
{
   MEMSET0(umg) ;

   umg->type          = umgebung_TEST_IMPL ;
   umg->free_umgebung = &free_umgebung_testproxy ;
   return 0 ;
}

#ifdef KONFIG_UNITTEST

#define TEST(CONDITION) TEST_ONERROR_GOTO(CONDITION,unittest_umgebung_testproxy,ABBRUCH)

static int test_init(void)
{
   umgebung_t umg = umgebung_INIT_FREEABLE ;

   // TEST init, double free
   TEST(0 == init_umgebung_testproxy(&umg)) ;
   TEST(umgebung_TEST_IMPL       == umg.type) ;
   TEST(&free_umgebung_testproxy == umg.free_umgebung) ;
   TEST(0 == free_umgebung_testproxy(&umg)) ;
   TEST(0 == umg.type) ;
   TEST(0 == umg.free_umgebung) ;
   TEST(0 == free_umgebung_testproxy(&umg)) ;
   TEST(0 == umg.type) ;
   TEST(0 == umg.free_umgebung) ;

   return 0 ;
ABBRUCH:
   return 1 ;
}

int unittest_umgebung_testproxy(void)
{
   if (test_init()) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   return 1 ;
}

#endif
