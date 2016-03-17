/* title: CString impl
   Implements <CString>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/string/cstring.h
    Header file of <CString>.

   file: C-kern/string/cstring.c
    Implementation file <CString impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/string/cstring.h"
#include "C-kern/api/err.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#include "C-kern/api/string/string.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif

static inline void compiletime_tests(void)
{
   static_assert(sizeof(char)==1,"memory allocation assumes sizeof(char)==1");
}

int init_cstring(/*out*/cstring_t* cstr, size_t capacity)
{
   int err;
   memblock_t strmem = memblock_FREE;

   if (capacity) {
      err = ALLOC_MM(capacity, &strmem);
      if (err) goto ONERR;
   }

   cstr->size     = 0;
   cstr->capacity = strmem.size;
   cstr->chars    = strmem.addr;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int initcopy_cstring(/*out*/cstring_t* cstr, const struct string_t* copiedfrom)
{
   int err;

   err = init_cstring(cstr, copiedfrom->size + (0 != copiedfrom->size));
   if (err) goto ONERR;

   // could not fail
   append_cstring(cstr, copiedfrom->size, (const char*) copiedfrom->addr);

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int free_cstring(cstring_t* cstr)
{
   int err;

   if (cstr->capacity) {
      memblock_t strmem = memblock_INIT(cstr->capacity, cstr->chars);
      MEMSET0(cstr);

      err = FREE_MM(&strmem);
      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: change

int allocate_cstring(cstring_t* cstr, size_t capacity)
{
   int err;

   if (capacity > cstr->capacity) {

      memblock_t strmem = memblock_INIT(cstr->capacity, cstr->chars);

      size_t new_size = 2 * cstr->capacity;

      if (new_size < capacity) {
         new_size = capacity;
      }

      err = RESIZE_MM(new_size, &strmem);
      if (err) goto ONERR;

      if (!cstr->chars) {
         // allocation done for first time => add trailing '\0' byte
         strmem.addr[0] = 0;
      }

      cstr->capacity = strmem.size;
      cstr->chars    = strmem.addr;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

#define EXPAND_cstring(cstr, append_size) \
   {                                                  \
      size_t capacity = cstr->size + 1 + append_size; \
      if (capacity <= append_size) {                  \
         err = ENOMEM;                                \
         TRACEOUTOFMEM_ERRLOG(SIZE_MAX, err);         \
         goto ONERR;                                  \
      }                                               \
                                                      \
      err = allocate_cstring(cstr, capacity);         \
      if (err) goto ONERR;                            \
   }

int append_cstring(cstring_t* cstr, size_t str_len, const char str[str_len])
{
   int err;

   if (!str_len) {
      return 0;
   }

   EXPAND_cstring(cstr, str_len);

   memcpy(&cstr->chars[cstr->size], str, str_len);
   cstr->size             += str_len;
   cstr->chars[cstr->size] = 0;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int printfappend_cstring(cstring_t* cstr, const char* format, ...)
{
   int err;

   for (;;) {
      int  append_size;
      size_t free_size;

      free_size = cstr->capacity - cstr->size;

      {
         va_list args;
         va_start(args, format);
         append_size = vsnprintf(str_cstring(cstr) + cstr->size, free_size, format, args);
         va_end(args);
      }

      if (append_size < 0) {
         err = ENOMEM;
         TRACEOUTOFMEM_ERRLOG(SIZE_MAX, err);
         goto ONERR;
      }

      if ((size_t)append_size < free_size) {
         cstr->size += (size_t)append_size;
         break;
      }

      EXPAND_cstring(cstr, (size_t)append_size);
   }

   return 0;
ONERR:
   if (cstr->capacity) cstr->chars[cstr->size] = 0;
   TRACEEXIT_ERRLOG(err);
   return err;
}

// TODO: set unallocate case !!
int setsize_cstring(cstring_t* cstr, size_t new_size)
{
   int err;

   if (new_size >= cstr->capacity) {
      if (new_size == 0 && cstr->capacity == 0) {
         return 0; // unallocated case
      }
      err = EINVAL;
      goto ONERR;
   }

   cstr->size = new_size;
   cstr->chars[new_size] = 0;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

void adaptsize_cstring(cstring_t* cstr)
{
   if (cstr->capacity) {
      cstr->chars[cstr->capacity-1] = 0;
      cstr->size = strlen((char*) cstr->chars);
   }
}

void clear_cstring(cstring_t* cstr)
{
   if (cstr->size) {
      cstr->size     = 0;
      cstr->chars[0] = 0;
   }
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   cstring_t cstr  = cstring_FREE;
   cstring_t cstr2 = cstring_INIT;

   // TEST cstring_FREE
   TEST(0 == cstr.size);
   TEST(0 == cstr.capacity);
   TEST(0 == cstr.chars);

   // TEST cstring_INIT
   cstr = (cstring_t) cstring_INIT;
   TEST(0 == cstr.size);
   TEST(0 == cstr.capacity);
   TEST(0 == cstr.chars);;

   // TEST init_cstring
   cstr.size = 1;
   TEST(0 == init_cstring(&cstr, 256));
   TEST(0 == cstr.size);
   TEST(256 == cstr.capacity);
   TEST(0 != cstr.chars);

   // TEST free_cstring
   TEST(0 == free_cstring(&cstr));
   TEST(0 == cstr.size);
   TEST(0 == cstr.capacity);
   TEST(0 == cstr.chars);
   TEST(0 == free_cstring(&cstr));
   TEST(0 == cstr.size);
   TEST(0 == cstr.capacity);
   TEST(0 == cstr.chars);

   // TEST initcopy_cstring: empty from string
   string_t stringfrom = string_FREE;
   TEST(0 == initcopy_cstring(&cstr, &stringfrom));
   TEST(0 == cstr.size);
   TEST(0 == cstr.capacity);
   TEST(0 == cstr.chars);

   // TEST initcopy_cstring
   stringfrom = (string_t) string_INIT(7, (const uint8_t*)"x1y2z3!");
   TEST(0 == initcopy_cstring(&cstr, &stringfrom));
   TEST(7 == size_cstring(&cstr));
   TEST(8 == cstr.capacity);
   TEST(0 != cstr.chars);
   TEST(stringfrom.addr != (void*)cstr.chars);
   TEST(0 == strcmp(str_cstring(&cstr), "x1y2z3!"));
   TEST(0 == free_cstring(&cstr));
   TEST(0 == cstr.size);
   TEST(0 == cstr.capacity);
   TEST(0 == cstr.chars);

   // TEST initmove_cstring: empty string
   initmove_cstring(&cstr, &cstr2);
   TEST(0 == cstr.size);
   TEST(0 == cstr.capacity);
   TEST(0 == cstr.chars);
   TEST(0 == cstr2.size);
   TEST(0 == cstr2.capacity);
   TEST(0 == cstr2.chars);

   // TEST initmove_cstring
   TEST(0 == init_cstring(&cstr2, 64));
   TEST(0 == append_cstring(&cstr2, 6, "123456"));
   cstring_t xxx = cstr2;
   initmove_cstring(&cstr, &cstr2);
   TEST(0 == memcmp(&cstr, &xxx, sizeof(cstr)));
   TEST(0 == strcmp(str_cstring(&cstr), "123456"));
   TEST(64 == cstr.capacity);
   TEST(0 == cstr2.size);
   TEST(0 == cstr2.capacity);
   TEST(0 == cstr2.chars);
   TEST(0 == free_cstring(&cstr));

   return 0;
ONERR:
   free_cstring(&cstr);
   free_cstring(&cstr2);
   return EINVAL;
}

static int test_changeandquery(void)
{
   cstring_t cstr = cstring_FREE;

   // TEST size_cstring
   for (size_t i = SIZE_MAX;; i <<= 1) {
      cstr.size = i;
      TEST(i == size_cstring(&cstr));
      if (!i) break;
   }

   // TEST capacity_cstring
   for (size_t i = SIZE_MAX;; i <<= 1) {
      cstr.capacity = i;
      TEST(i == capacity_cstring(&cstr));
      if (!i) break;
   }

   // TEST addr_cstring
   for (uintptr_t i = (uintptr_t)-1;; i <<= 1) {
      cstr.chars = (uint8_t*) i;
      TEST((uint8_t*)i == addr_cstring(&cstr));
      if (!i) break;
   }

   // TEST str_cstring
   for (uintptr_t i = (uintptr_t)-1;; i <<= 1) {
      cstr.chars = (uint8_t*) i;
      TEST((char*)i == str_cstring(&cstr));
      if (!i) break;
   }

   // TEST printfappend_cstring: empty string
   TEST(0 == printfappend_cstring(&cstr, "%d%s", 123, "456"));
   TEST(6 == size_cstring(&cstr));
   TEST(7 == cstr.capacity);
   TEST(0 == strcmp(str_cstring(&cstr), "123456"));

   // TEST printfappend_cstring: size doubles
   TEST(0 == printfappend_cstring(&cstr, "7"));
   TEST(7 == size_cstring(&cstr));
   TEST(14 == cstr.capacity); // size doubled
   TEST(0 == strcmp(str_cstring(&cstr), "1234567"));

   // TEST printfappend_cstring: exact size
   const char* str = "1234567890";
   TEST(0 == printfappend_cstring(&cstr, "%s%s%s", str, str, str));
   TEST(37 == size_cstring(&cstr));
   TEST(38 == cstr.capacity); // exact size
   TEST(0 == strcmp(str_cstring(&cstr), "1234567123456789012345678901234567890"));
   TEST(0 == free_cstring(&cstr));

   // TEST printfappend_cstring: ENOMEM (does not change content in case of error)
   {  uint8_t buffer[4] = "123";
      cstring_t cstr3 = { .size = (size_t)-2, .capacity = 1 + (size_t)-2, .chars = (&buffer[3] - (size_t)-2) };
      TEST(&buffer[3] == (cstr3.chars + cstr3.size));
      TEST(ENOMEM == printfappend_cstring(&cstr3, "XX"));
      TEST(cstr3.size           == (size_t)-2);
      TEST(cstr3.capacity == 1 + (size_t)-2);
      TEST(0 == strcmp((char*)buffer, "123"));
   }

   // TEST append_cstring: empty string
   TEST(0 == append_cstring(&cstr, 6, "123456"));
   TEST(6 == size_cstring(&cstr));
   TEST(7 == cstr.capacity);
   TEST(0 == strcmp(str_cstring(&cstr), "123456"));

   // TEST append_cstring: sizes doubles
   TEST(0 == append_cstring(&cstr, 1, "7"));
   TEST(7 == size_cstring(&cstr));
   TEST(14 == cstr.capacity); // size doubled
   TEST(0 == strcmp(str_cstring(&cstr), "1234567"));

   // TEST append_cstring: exact size
   TEST(0 == free_cstring(&cstr));
   TEST(0 == init_cstring(&cstr, 4));
   TEST(4 == cstr.capacity);
   str = "123456789012345";
   TEST(0 == append_cstring(&cstr, 15, str));
   TEST(15 == size_cstring(&cstr));
   TEST(16 == cstr.capacity); // exact size
   TEST(0 == strcmp(str_cstring(&cstr), str));
   TEST(0 == free_cstring(&cstr));

   // TEST append_cstring: ENOMEM
   TEST(0 == init_cstring(&cstr, 0));
   TEST(ENOMEM == append_cstring(&cstr, (size_t)-1, ""));
   TEST(0 == cstr.size);
   TEST(0 == cstr.capacity);
   TEST(0 == cstr.chars);

   // TEST append_cstring: ENOMEM (does not change content in case of error)
   {  uint8_t buffer[10] = "123";
      cstring_t cstr3 = { .size = (size_t)-2, .capacity = 1 + (size_t)-2, .chars = (&buffer[3] - (size_t)-2) };
      TEST(&buffer[3] == (cstr3.chars + cstr3.size));
      TEST(ENOMEM == append_cstring(&cstr3, 2, "XX"));
      TEST(cstr3.size     == (size_t)-2);
      TEST(cstr3.capacity == 1 + (size_t)-2);
      TEST(0 == strcmp((char*)buffer, "123"));
   }

   // TEST allocate_cstring: size 0 / empty string
   TEST(0 == allocate_cstring(&cstr, 0));
   TEST(0 == cstr.size);
   TEST(0 == cstr.capacity);
   TEST(0 == cstr.chars);

   // TEST allocate_cstring: first time sets \0 byte
   for (size_t s = 1; s <= 9; ++s) {
      TEST(0 == free_cstring(&cstr));
      TEST(0 == allocate_cstring(&cstr, s));
      TEST(0 == cstr.size);
      TEST(s == cstr.capacity);
      TEST(0 == cstr.chars[0]); // first time allocation sets \0 byte
   }

   // TEST allocate_cstring: exact size, cause doubling is not enough
   cstr.chars[0] = 'x';
   size_t testsize[] = { 19, 43, 87, 200, 1024 };
   for (unsigned i = 0; i < lengthof(testsize); ++i) {
      size_t S = testsize[i];
      TEST(0 == allocate_cstring(&cstr, S));
      TEST(0 == cstr.size);
      TEST(S == cstr.capacity);
      TEST('x' == cstr.chars[0]); // not changed
   }

   // TEST allocate_cstring: doubles in size
   for (size_t i = 2*cstr.capacity; i <= 65536; i <<= 1) {
      TEST(0 == allocate_cstring(&cstr, cstr.capacity+1));
      TEST(0 == cstr.size);
      TEST(i == cstr.capacity);
      TEST('x' == cstr.chars[0]); // not changed
   }

   // TEST allocate_cstring: ENOMEM
   TEST(0 == free_cstring(&cstr));
   for (int i = -1; i >= -2; --i) {
      TEST(ENOMEM == allocate_cstring(&cstr, (size_t)i));
      TEST(0 == cstr.size);
      TEST(0 == cstr.capacity);
      TEST(0 == cstr.chars);
   }

   return 0;
ONERR:
   free_cstring(&cstr);
   return EINVAL;
}

static int test_changesize(void)
{
   cstring_t cstr = cstring_INIT;

   // TEST clear_cstring: empty string
   clear_cstring(&cstr);
   TEST(0 == cstr.size);
   TEST(0 == cstr.capacity);
   TEST(0 == cstr.chars);

   // TEST setsize_cstring: empty string
   TEST(0 == setsize_cstring(&cstr, 0));
   TEST(0 == cstr.size);
   TEST(0 == cstr.capacity);
   TEST(0 == cstr.chars);

   // TEST adaptsize_cstring: empty string
   adaptsize_cstring(&cstr);
   TEST(0 == cstr.size);
   TEST(0 == cstr.capacity);
   TEST(0 == cstr.chars);

   // TEST clear_cstring
   TEST(0 == init_cstring(&cstr, 256));
   for (size_t i = 255; i > 0; --i) {
      cstr.size = i;
      memset(cstr.chars, (int) i, 256);
      clear_cstring(&cstr);
      TEST(0 == cstr.size);
      TEST(256 == cstr.capacity);
      TEST(0 == cstr.chars[0]);
      for (size_t b = 1; b < cstr.capacity; ++b) {
         TEST(i == cstr.chars[b]);
      }
   }
   TEST(0 == free_cstring(&cstr));

   // TEST setsize_cstring
   TEST(0 == init_cstring(&cstr, 256));
   for (size_t i = 255; i <= 255; --i) {
      cstr.size = 256;
      memset(cstr.chars, (int)i, 256);
      TEST(0 == setsize_cstring(&cstr, i));
      TEST(i == cstr.size);
      TEST(256 == cstr.capacity);
      TEST(0 == cstr.chars[i]);
      for (size_t b = 0; b < cstr.capacity; ++b) {
         if (b == i) continue;
         TEST(i == cstr.chars[b]);
      }
   }

   // TEST setsize_cstring: EINVAL
   TEST(EINVAL == setsize_cstring(&cstr, capacity_cstring(&cstr)));
   TEST(0 == free_cstring(&cstr));
   TEST(EINVAL == setsize_cstring(&cstr, 1));

   // TEST adaptsize_cstring
   TEST(0 == init_cstring(&cstr, 256));
   for (size_t i = 255; i <= 255; --i) {
      cstr.size = 256;
      memset(cstr.chars, (int)i, 256);
      cstr.chars[i] = 0;
      adaptsize_cstring(&cstr);
      TEST(i == cstr.size);
      TEST(256 == cstr.capacity);
      TEST(0 == cstr.chars[i]);
      TEST(0 == cstr.chars[255]);
      for (size_t b = 0; b < cstr.capacity-1; ++b) {
         if (b == i) continue;
         TEST(i == cstr.chars[b]);
      }
   }

   // TEST adaptsize_cstring: no \0 byte
   for (size_t i = 255; i > 0; --i) {
      cstr.size = 256;
      memset(cstr.chars, (int)i, 256);
      adaptsize_cstring(&cstr);
      TEST(255 == cstr.size);
      TEST(256 == cstr.capacity);
      TEST(0 == cstr.chars[255]);
      for (size_t b = 0; b < cstr.size; ++b) {
         TEST(i == cstr.chars[b]);
      }
   }

   // unprepare
   TEST(0 == free_cstring(&cstr));

   return 0;
ONERR:
   free_cstring(&cstr);
   return EINVAL;
}

int unittest_string_cstring()
{
   if (test_initfree())       goto ONERR;
   if (test_changeandquery()) goto ONERR;
   if (test_changesize())     goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
