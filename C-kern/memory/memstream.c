/* title: MemoryStream impl

   Implements <MemoryStream>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/memory/memstream.h
    Header file <MemoryStream>.

   file: C-kern/memory/memstream.c
    Implementation file <MemoryStream impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/memory/memstream.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/string/string.h"
#endif


// section: memstream_t

// group: lifetime


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   memstream_t    memstr    = memstream_FREE;
   memstream_ro_t memstr_ro = memstream_FREE;

   // memstream_t

   // TEST memstream_FREE
   TEST(0 == memstr.next);
   TEST(0 == memstr.end);
   TEST(0 == memstr_ro.next);
   TEST(0 == memstr_ro.end);

   // TEST memstream_INIT
   memstr = (memstream_t) memstream_INIT((uint8_t*)1, (uint8_t*)3);
   memstr_ro = (memstream_ro_t) memstream_INIT((const uint8_t*)5, (const uint8_t*)7);
   TEST(1 == (uintptr_t)memstr.next);
   TEST(3 == (uintptr_t)memstr.end);
   TEST(5 == (uintptr_t)memstr_ro .next);
   TEST(7 == (uintptr_t)memstr_ro .end);

   // TEST init_memstream
   init_memstream(&memstr, (uint8_t*)5, (uint8_t*)8);
   init_memstream(&memstr_ro, (const uint8_t*)1, (const uint8_t*)5);
   TEST(5 == (uintptr_t)memstr.next);
   TEST(8 == (uintptr_t)memstr.end);
   TEST(1 == (uintptr_t)memstr_ro.next);
   TEST(5 == (uintptr_t)memstr_ro.end);

   // TEST initPstr_memstreamro
   {
      string_t str = string_INIT(3, (const uint8_t*)4);
      initPstr_memstreamro(&memstr_ro, &str);
      TEST(4 == (uintptr_t)memstr_ro.next);
      TEST(7 == (uintptr_t)memstr_ro.end);
   }

   // TEST free_memstream
   memset(&memstr, 255, sizeof(memstr));
   memset(&memstr_ro, 255, sizeof(memstr_ro));
   free_memstream(&memstr);
   free_memstream(&memstr_ro);
   TEST(0 == memstr.next);
   TEST(0 == memstr.end);
   TEST(0 == memstr_ro.next);
   TEST(0 == memstr_ro.end);

   return 0;
ONERR:
   return EINVAL;
}

static int test_query(void)
{
   memstream_t    memstr;
   memstream_ro_t memstr_ro;
   uint8_t        buffer[100];

   // TEST isnext_memstream
   for (uintptr_t i = 1; i; i <<= 1) {
      init_memstream(&memstr, (uint8_t*)i, (uint8_t*)i);
      TEST(0 == isnext_memstream(&memstr));
      init_memstream(&memstr, 0, (uint8_t*)i);
      TEST(1 == isnext_memstream(&memstr));
      init_memstream(&memstr, (uint8_t*)i, 0); // wrong next/end is not checked for
      TEST(1 == isnext_memstream(&memstr));

      init_memstream(&memstr_ro, (const uint8_t*)i, (const uint8_t*)i);
      TEST(0 == isnext_memstream(&memstr_ro));
      init_memstream(&memstr_ro, 0, (const uint8_t*)i);
      TEST(1 == isnext_memstream(&memstr_ro));
      init_memstream(&memstr_ro, (const uint8_t*)i, 0); // wrong next/end is not checked for
      TEST(1 == isnext_memstream(&memstr_ro));
   }

   // TEST size_memstream
   for (unsigned i = 0; i <= sizeof(buffer); ++i) {
      memstr = (memstream_t) memstream_INIT(buffer, buffer+i);
      TEST(i == size_memstream(&memstr));
      TEST(memstr.next == buffer);
      TEST(memstr.end  == buffer+i);
      memstr_ro = (memstream_ro_t) memstream_INIT((const uint8_t*)buffer, (const uint8_t*)buffer+i);
      TEST(i == size_memstream(&memstr_ro));
      TEST(memstr_ro.next == buffer);
      TEST(memstr_ro.end  == buffer+i);
   }

   // TEST offset_memstream
   for (unsigned i = 0; i <= sizeof(buffer); ++i) {
      memstr = (memstream_t) memstream_INIT(buffer+i, buffer+sizeof(buffer));
      TEST(i == offset_memstream(&memstr, buffer));
      TEST(memstr.next == buffer+i);
      TEST(memstr.end  == buffer+sizeof(buffer));
      memstr_ro = (memstream_ro_t) memstream_INIT((const uint8_t*)buffer+i, (const uint8_t*)buffer+sizeof(buffer));
      TEST(i == offset_memstream(&memstr_ro, buffer));
      TEST(memstr_ro.next == buffer+i);
      TEST(memstr_ro.end  == buffer+sizeof(buffer));
   }

   // TEST next_memstream
   for (uintptr_t i = 1; i; i <<= 1) {
      memstr.next = (uint8_t*) i;
      TEST((uint8_t*) i == next_memstream(&memstr));
      memstr_ro.next = (const uint8_t*) i;
      TEST((const uint8_t*) i == next_memstream(&memstr_ro));
   }

   // TEST findbyte_memstream: size == 0
   memset(buffer, 0, sizeof(buffer));
   for (unsigned i = 0; i <= sizeof(buffer); ++i) {
      init_memstream(&memstr, buffer+i, buffer+i);
      init_memstream(&memstr_ro, buffer+i, buffer+i);
      TEST(0 == findbyte_memstream(&memstr, 0));
      TEST(0 == findbyte_memstream(&memstr_ro, 0));
   }

   // TEST findbyte_memstream: all bytes
   init_memstream(&memstr, buffer, buffer+sizeof(buffer));
   init_memstream(&memstr_ro, buffer, buffer+sizeof(buffer));
   for (unsigned i = 0; i < 256; ++i) {
      memset(buffer, !i, sizeof(buffer));
      for (unsigned off = 0; off < sizeof(buffer); ++off) {
         buffer[off] = (uint8_t)i;
         uint8_t * b1 = findbyte_memstream(&memstr, i);
         TEST(buffer + off == b1);
         const uint8_t * b2 = findbyte_memstream(&memstr_ro, i);
         TEST(buffer + off == b2);
         buffer[off] = !i;
      }
   }

   // TEST findbyte_memstream: do not find byte (byte not in buffer)
   for (unsigned i = 0; i < 256; ++i) {
      memset(buffer, !i, sizeof(buffer));
      buffer[0] = (uint8_t) i;
      for (unsigned off = 1; off < sizeof(buffer); ++off) {
         init_memstream(&memstr, buffer+1, buffer+off);
         init_memstream(&memstr_ro, buffer+1, buffer+off);
         buffer[off] = (uint8_t)i;
         uint8_t * b1 = findbyte_memstream(&memstr, i);
         TEST(b1 == 0);
         const uint8_t * b2 = findbyte_memstream(&memstr_ro, i);
         TEST(b2 == 0);
         buffer[off] = !i;
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_update(void)
{
   memstream_t    memstr;
   memstream_ro_t memstr_ro;
   uint8_t        buffer[256];

   // TEST skip_memstream
   for (unsigned i = 0; i <= sizeof(buffer); ++i) {
      memstr = (memstream_t) memstream_INIT(buffer, buffer+sizeof(buffer));
      skip_memstream(&memstr, i);
      TEST(buffer+i              == memstr.next);
      TEST(buffer+sizeof(buffer) == memstr.end);

      memstr_ro = (memstream_ro_t) memstream_INIT(buffer, buffer+sizeof(buffer));
      skip_memstream(&memstr_ro, i);
      TEST(buffer+i              == memstr_ro.next);
      TEST(buffer+sizeof(buffer) == memstr_ro.end);
   }

   // TEST tryskip_memstream: OK
   for (unsigned i = 0; i <= sizeof(buffer); ++i) {
      init_memstream(&memstr, buffer, buffer+sizeof(buffer));
      TEST(0 == tryskip_memstream(&memstr, i));
      TEST(buffer+i              == memstr.next);
      TEST(buffer+sizeof(buffer) == memstr.end);

      init_memstream(&memstr_ro, buffer, buffer+sizeof(buffer));
      TEST(0 == tryskip_memstream(&memstr_ro, i));
      TEST(buffer+i              == memstr_ro.next);
      TEST(buffer+sizeof(buffer) == memstr_ro.end);
   }

   // TEST tryskip_memstream: len == 0
   init_memstream(&memstr, buffer, buffer);
   init_memstream(&memstr_ro, buffer, buffer);
   TEST(0 == tryskip_memstream(&memstr, 0));
   TEST(0 == tryskip_memstream(&memstr_ro, 0));
   TEST(buffer == memstr.next);
   TEST(buffer == memstr.end);
   TEST(buffer == memstr_ro.next);
   TEST(buffer == memstr_ro.end);

   // TEST tryskip_memstream: EINVAL
   for (unsigned i = 0; i <= sizeof(buffer); ++i) {
      init_memstream(&memstr, buffer, buffer+i);
      TEST(EINVAL == tryskip_memstream(&memstr, i+1));
      TEST(buffer   == memstr.next);
      TEST(buffer+i == memstr.end);

      init_memstream(&memstr_ro, buffer, buffer+i);
      TEST(EINVAL == tryskip_memstream(&memstr_ro, i+1));
      TEST(buffer   == memstr_ro.next);
      TEST(buffer+i == memstr_ro.end);
   }

   // prepare
   for (unsigned i = 0; i < sizeof(buffer); ++i) {
      buffer[i] = (uint8_t) i;
   }

   // TEST nextbyte_memstream:
   for (unsigned i = 0; i < sizeof(buffer); ++i) {
      init_memstream(&memstr, buffer+i, buffer+sizeof(buffer));
      TEST(i == nextbyte_memstream(&memstr));
      TEST(buffer+i+1            == memstr.next);
      TEST(buffer+sizeof(buffer) == memstr.end);

      init_memstream(&memstr_ro, buffer+i, buffer+sizeof(buffer));
      TEST(i == nextbyte_memstream(&memstr_ro));
      TEST(buffer+i+1            == memstr_ro.next);
      TEST(buffer+sizeof(buffer) == memstr_ro.end);
   }

   // TEST nextbyte_memstream: does not check for overflow
   for (unsigned i = 0; i < sizeof(buffer); ++i) {
      init_memstream(&memstr, buffer+i, buffer+i);
      TEST(i == nextbyte_memstream(&memstr));
      TEST(buffer+i+1 == memstr.next);
      TEST(buffer+i   == memstr.end);

      init_memstream(&memstr_ro, buffer+i, buffer+i);
      TEST(i == nextbyte_memstream(&memstr_ro));
      TEST(buffer+i+1 == memstr_ro.next);
      TEST(buffer+i   == memstr_ro.end);
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_write(void)
{
   memstream_t    memstr;
   uint8_t        buffer[100];
   uint8_t        buffer2[100];

   // prepare
   for (unsigned i = 0; i < sizeof(buffer2); ++i) {
      buffer2[i] = (uint8_t) (i+1);
   }

   // TEST write_memstream
   for (unsigned i = 0; i <= sizeof(buffer); ++i) {
      memstr = (memstream_t) memstream_INIT(buffer, buffer+sizeof(buffer));
      memset(buffer, 0, sizeof(buffer));
      write_memstream(&memstr, i, buffer2);
      TEST(buffer+i              == memstr.next);
      TEST(buffer+sizeof(buffer) == memstr.end);
      TEST(0 == memcmp(buffer, buffer2, i));
      for (unsigned i2 = i; i2 < sizeof(buffer2); ++i2) {
         TEST(0 == buffer[i2]); // unchanged
      }
   }

   // TEST write_memstream: No check for overflow of memstr
   memstr = (memstream_t) memstream_INIT(buffer, buffer+1);
   memset(buffer, 0, sizeof(buffer));
   write_memstream(&memstr, sizeof(buffer2), buffer2);
   TEST(buffer+sizeof(buffer) == memstr.next);
   TEST(buffer+1              == memstr.end);
   TEST(0 == memcmp(buffer, buffer2, sizeof(buffer)));

   // TEST writebyte_memstream
   memstr = (memstream_t) memstream_INIT(buffer, buffer+sizeof(buffer));
   memset(buffer, 0, sizeof(buffer));
   for (unsigned i = 0; i < sizeof(buffer); ++i) {
      writebyte_memstream(&memstr, buffer2[i]);
      TEST(buffer+1+i            == memstr.next);
      TEST(buffer+sizeof(buffer) == memstr.end);
      TEST(buffer[i] == buffer2[i]);
   }
   TEST(0 == memcmp(buffer, buffer2, sizeof(buffer2)));

   // TEST writebyte_memstream: No check for overflow of memstr
   memstr = (memstream_t) memstream_INIT(buffer, buffer);
   memset(buffer, 0, sizeof(buffer));
   writebyte_memstream(&memstr, 100);
   TEST(buffer+1 == memstr.next);
   TEST(buffer   == memstr.end);
   TEST(buffer[0] == 100);

   // TEST printf_memstream
   for (unsigned i = 0; i <= sizeof(buffer)-6; ++i) {
      memstr = (memstream_t) memstream_INIT(buffer+i, buffer+sizeof(buffer));
      memset(buffer, 255, sizeof(buffer));
      TEST(0 == printf_memstream(&memstr, "%d,%s", 1, "abc"));
      TEST(buffer+i+5u           == memstr.next);
      TEST(buffer+sizeof(buffer) == memstr.end);
      TEST(0 == memcmp(buffer+i, "1,abc", 6));
      for (unsigned i2 = 0; i2 < i; ++i2) {
         TEST(255 == buffer[i2]);
      }
      for (unsigned i2 = i+6; i2 < sizeof(buffer); ++i2) {
         TEST(255 == buffer[i2]);
      }
   }

   // TEST printf_memstream: ENOBUFS
   memstr = (memstream_t) memstream_INIT(buffer, buffer+5);
   memset(buffer, 255, sizeof(buffer));
   TEST(ENOBUFS == printf_memstream(&memstr, "%s", "ABCDE"));
   TEST(0 == memcmp(buffer, "ABCD", 5));
   TEST(buffer   == memstr.next); // unchanged
   TEST(buffer+5 == memstr.end);

   // TEST printf_memstream: size_memstream() == 0
   memstr = (memstream_t) memstream_INIT(buffer, buffer);
   memset(buffer, 255, sizeof(buffer));
   TEST(ENOBUFS == printf_memstream(&memstr, "%s", "12345"));
   TEST(255 == buffer[0]);
   TEST(buffer == memstr.next);
   TEST(buffer == memstr.end);

   return 0;
ONERR:
   return EINVAL;
}

static int test_generic(void)
{
   memstream_t    obj1;
   memstream_ro_t obj1_ro;
   struct {
      int x;
      uint8_t * next;
      uint8_t * end;
      int y;
   }           obj2;
   struct {
      int x;
      uint8_t * pre_next;
      uint8_t * pre_end;
      int y;
   }           obj3;
   struct {
      int x;
      const uint8_t * next;
      const uint8_t * end;
      int y;
   }           obj2_ro;
   struct {
      int x;
      const uint8_t * pre_next;
      const uint8_t * pre_end;
      int y;
   }           obj3_ro;

   // TEST cast_memstream
   TEST( cast_memstream(&obj1, )     == &obj1);
   TEST( cast_memstream(&obj2, )     == (memstream_t*)&obj2.next);
   TEST( cast_memstream(&obj3, pre_) == (memstream_t*)&obj3.pre_next);

   // TEST cast_memstreamro
   TEST( cast_memstreamro(&obj1, )    == (memstream_ro_t*) &obj1);
   TEST( cast_memstreamro(&obj1_ro, ) == &obj1_ro);
   TEST( cast_memstreamro(&obj2, )     == (memstream_ro_t*)&obj2.next);
   TEST( cast_memstreamro(&obj3, pre_) == (memstream_ro_t*)&obj3.pre_next);
   TEST( cast_memstreamro(&obj2_ro, )     == (memstream_ro_t*)&obj2_ro.next);
   TEST( cast_memstreamro(&obj3_ro, pre_) == (memstream_ro_t*)&obj3_ro.pre_next);

   return 0;
ONERR:
   return EINVAL;
}

int unittest_memory_memstream()
{
   if (test_initfree())       goto ONERR;
   if (test_query())          goto ONERR;
   if (test_update())         goto ONERR;
   if (test_write())          goto ONERR;
   if (test_generic())        goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
