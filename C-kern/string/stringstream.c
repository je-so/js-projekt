/* title: StringStream impl

   Implements <StringStream>.

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
   (C) 2012 Jörg Seebohn

   file: C-kern/api/string/stringstream.h
    Header file <StringStream>.

   file: C-kern/string/stringstream.c
    Implementation file <StringStream impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/string/string.h"
#include "C-kern/api/string/stringstream.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


// section: stringstream_t

// group: lifetime

int init_stringstream(/*out*/stringstream_t * strstream, const uint8_t * startaddr, const uint8_t * endaddr)
{
   int err ;

   VALIDATE_INPARAM_TEST(startaddr <= endaddr, ONABORT, ) ;

   strstream->next = startaddr ;
   strstream->end  = endaddr ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int initfromstring_string(/*out*/stringstream_t * strstream, const struct string_t * str)
{
   int err ;

   const uint8_t * end = addr_string(str) + size_string(str) ;

   VALIDATE_INPARAM_TEST(addr_string(str) <= end, ONABORT, ) ;

   strstream->next = addr_string(str) ;
   strstream->end  = end ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   stringstream_t strstream   = stringstream_INIT_FREEABLE ;
   string_t       str ;
   uint8_t        buffer[256] = { 0 } ;

   // TEST stringstream_INIT_FREEABLE
   TEST(strstream.next == 0) ;
   TEST(strstream.end  == 0) ;

   // TEST stringstream_INIT
   strstream = (stringstream_t) stringstream_INIT(buffer, buffer+sizeof(buffer)) ;
   TEST(strstream.next == buffer) ;
   TEST(strstream.end  == buffer+sizeof(buffer)) ;

   // TEST init_stringstream, free_stringstream
   TEST(0 == init_stringstream(&strstream, buffer+1, buffer+1)) ;
   TEST(strstream.next == buffer+1) ;
   TEST(strstream.end  == buffer+1) ;
   TEST(0 == init_stringstream(&strstream, buffer, buffer+sizeof(buffer))) ;
   TEST(strstream.next == buffer) ;
   TEST(strstream.end  == buffer+sizeof(buffer)) ;
   free_stringstream(&strstream) ;
   TEST(strstream.next == 0) ;
   TEST(strstream.end  == 0) ;
   free_stringstream(&strstream) ;
   TEST(strstream.next == 0) ;
   TEST(strstream.end  == 0) ;

   // TEST init_stringstream: EINVAL
   TEST(0 == init_stringstream(&strstream, buffer, buffer)) ;
   TEST(EINVAL == init_stringstream(&strstream, buffer+1, buffer)) ;
   TEST(strstream.next == buffer) ;
   TEST(strstream.end  == buffer) ;

   // TEST initfromstring_string
   for (unsigned i = 0; i <= sizeof(buffer); ++i) {
      strstream = (stringstream_t) stringstream_INIT_FREEABLE ;
      init_string(&str, sizeof(buffer)-i, buffer+i) ;
      TEST(0 == initfromstring_string(&strstream, &str)) ;
      TEST(strstream.next == buffer+i) ;
      TEST(strstream.end  == buffer+sizeof(buffer)) ;
   }

   // TEST initfromstring_string: EINVAL
   init_string(&str, 1, (const uint8_t*)(uintptr_t)-1) ;
   TEST(0 == init_stringstream(&strstream, buffer, buffer+1)) ;
   TEST(EINVAL == initfromstring_string(&strstream, &str)) ;
   TEST(strstream.next == buffer) ;
   TEST(strstream.end  == buffer+1) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_query(void)
{
   stringstream_t strstream   = stringstream_INIT_FREEABLE ;
   uint8_t        buffer[256] = { 0 } ;

   // prepare
   for (unsigned i = 0; i < sizeof(buffer); ++i) {
      buffer[i] = (uint8_t) i ;
   }

   // TEST isempty_stringstream
   for (unsigned i = 0; i < sizeof(buffer); ++i) {
      TEST(0 == init_stringstream(&strstream, buffer+i, buffer+i)) ;
      TEST(1 == isempty_stringstream(&strstream)) ;
      // in case of next > end (which is considered an error) returns also empty state
      strstream.next = buffer + i ;
      strstream.end  = buffer ;
      TEST(1 == isempty_stringstream(&strstream)) ;
   }

   // TEST size_stringstream
   for (unsigned i = 0; i < sizeof(buffer); ++i) {
      TEST(0 == init_stringstream(&strstream, buffer+i, buffer+i)) ;
      TEST(0 == size_stringstream(&strstream)) ;
      TEST(0 == init_stringstream(&strstream, buffer, buffer+i)) ;
      TEST(i == size_stringstream(&strstream)) ;
      // in case of next > end (which is considered an error) returns a negative value
      strstream.next = buffer + i ;
      strstream.end  = buffer ;
      TEST((size_t)-i == size_stringstream(&strstream)) ;
   }

   // TEST next_stringstream
   for (unsigned i = 0; i < sizeof(buffer); ++i) {
      const uint8_t * n = buffer + i ;
      TEST(0 == init_stringstream(&strstream, n, buffer+sizeof(buffer))) ;
      TEST(n == next_stringstream(&strstream)) ;
   }

   // TEST findbyte_stringstream
   TEST(0 == init_stringstream(&strstream, buffer, buffer+sizeof(buffer))) ;
   for (unsigned i = 0; i < sizeof(buffer); ++i) {
      buffer[i] = (uint8_t)(i+1) ;
      TEST(findbyte_stringstream(&strstream, i) == 0/*not found*/) ;
      buffer[i] = (uint8_t)i ;
      TEST(findbyte_stringstream(&strstream, i) == buffer+i/*found*/) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_change(void)
{
   stringstream_t strstream   = stringstream_INIT_FREEABLE ;
   uint8_t        buffer[256] = { 0 } ;

   // prepare
   for (unsigned i = 0; i < sizeof(buffer); ++i) {
      buffer[i] = (uint8_t) i ;
   }

   // TEST nextbyte_stringstream
   TEST(0 == init_stringstream(&strstream, buffer, buffer+sizeof(buffer))) ;
   for (unsigned i = 0; i < sizeof(buffer); ++i) {
      TEST((uint8_t)i == nextbyte_stringstream(&strstream)) ;
   }

   // TEST nextbyte_stringstream: does not test end of buffer reached !
   TEST(0 == init_stringstream(&strstream, buffer+3, buffer+3)) ;
   TEST(0 == size_stringstream(&strstream)) ;
   TEST(1 == isempty_stringstream(&strstream)) ;
   TEST(3 == nextbyte_stringstream(&strstream)) ;
   TEST(4 == nextbyte_stringstream(&strstream)) ;
   TEST(0 != size_stringstream(&strstream)) ;
   TEST(1 == isempty_stringstream(&strstream)) ;
   TEST(strstream.next == strstream.end + 2) ;

   // TEST skipbyte_stringstream
   TEST(0 == init_stringstream(&strstream, buffer, buffer+sizeof(buffer))) ;
   for (unsigned i = 0; i < sizeof(buffer); ++i) {
      TEST(sizeof(buffer)-i == size_stringstream(&strstream)) ;
      skipbyte_stringstream(&strstream) ;
      TEST(sizeof(buffer)-i-1 == size_stringstream(&strstream)) ;
      TEST(strstream.next == buffer + 1 + i) ;
      TEST(strstream.end  == buffer + sizeof(buffer)) ;
   }

   // TEST skipbyte_stringstream: does not unread data
   skipbyte_stringstream(&strstream) ;
   TEST(strstream.next == strstream.end + 1/*considered error*/) ;

   // TEST skipbytes_stringstream
   for (unsigned i = 0; i <= sizeof(buffer); ++i) {
      TEST(0 == init_stringstream(&strstream, buffer, buffer+sizeof(buffer))) ;
      skipbytes_stringstream(&strstream, i) ;
      TEST(sizeof(buffer)-i   == size_stringstream(&strstream)) ;
      TEST(strstream.next == buffer + i) ;
      TEST(strstream.end  == buffer + sizeof(buffer)) ;
   }

   // TEST skipbytes_stringstream: does not check unread data
   TEST(0 == init_stringstream(&strstream, buffer, buffer+sizeof(buffer))) ;
   skipbytes_stringstream(&strstream, sizeof(buffer)+1) ;
   TEST(strstream.next == strstream.end + 1/*considered error*/) ;

   // TEST tryskipbytes_stringstream
   for (unsigned i = 0; i <= sizeof(buffer); ++i) {
      TEST(0 == init_stringstream(&strstream, buffer, buffer+sizeof(buffer))) ;
      TEST(0 == tryskipbytes_stringstream(&strstream, i)) ;
      TEST(sizeof(buffer)-i   == size_stringstream(&strstream)) ;
      TEST(strstream.next == buffer + i) ;
      TEST(strstream.end  == buffer + sizeof(buffer)) ;
   }

   // TEST tryskipbytes_stringstream: EINVAL
   TEST(0 == init_stringstream(&strstream, buffer, buffer+sizeof(buffer))) ;
   TEST(EINVAL == tryskipbytes_stringstream(&strstream, sizeof(buffer)+1)) ;
   TEST(strstream.next == buffer) ;
   TEST(strstream.end  == buffer + sizeof(buffer)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}


static int test_generic(void)
{
   struct {
      int dummy1 ;
      const uint8_t  * next ;
      const uint8_t  * end ;
      int dummy2 ;
   }     strstream ;

   // TEST genericcast_stringstream
   TEST((stringstream_t*)&strstream.next == genericcast_stringstream(&strstream)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_string_stringstream()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())       goto ONABORT ;
   if (test_query())          goto ONABORT ;
   if (test_change())         goto ONABORT ;
   if (test_generic())        goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
