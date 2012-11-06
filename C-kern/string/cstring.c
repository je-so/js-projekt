/* title: CString impl
   Implements <CString>.

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
#include "C-kern/api/test.h"
#endif

static inline void compiletime_tests(void)
{
   static_assert(sizeof(char)==1,"memory allocation assumes sizeof(char)==1") ;
}

int init_cstring(/*out*/cstring_t * cstr, size_t preallocate_size)
{
   int err ;
   memblock_t strmem = memblock_INIT_FREEABLE ;

   if (preallocate_size) {
      err = RESIZE_MM(preallocate_size, &strmem) ;
      if (err) goto ONABORT ;
   }

   cstr->length         = 0 ;
   cstr->allocated_size = strmem.size ;
   cstr->chars          = strmem.addr ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err);
   return err ;
}

int initfromstring_cstring(/*out*/cstring_t * cstr, struct conststring_t * copiedfrom)
{
   int err ;

   if (copiedfrom->size) {
      err = init_cstring(cstr, copiedfrom->size + 1) ;
      if (err) goto ONABORT ;

      err = append_cstring(cstr, copiedfrom->size, (const char*) copiedfrom->addr) ;
      if (err) goto ONABORT ;
   } else {
      err = init_cstring(cstr, copiedfrom->size) ;
      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err);
   return err ;
}

int free_cstring(cstring_t * cstr)
{
   int err ;

   if (cstr->allocated_size) {
      memblock_t strmem = memblock_INIT(cstr->allocated_size, cstr->chars) ;
      MEMSET0(cstr) ;

      err = FREE_MM(&strmem) ;
      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err);
   return err ;
}

// group: change

int allocate_cstring(cstring_t * cstr, size_t allocate_size)
{
   int err ;

   if (allocate_size > cstr->allocated_size) {

      memblock_t strmem = memblock_INIT(cstr->allocated_size, cstr->chars) ;

      size_t new_size = 2 * cstr->allocated_size ;

      if (new_size < allocate_size) {
         new_size = allocate_size ;
      }

      err = RESIZE_MM(new_size, &strmem) ;
      if (err) goto ONABORT ;

      if (!cstr->chars) {
         // allocation done for first time => add trailing '\0' byte
         strmem.addr[0] = 0 ;
      }

      cstr->allocated_size = strmem.size ;
      cstr->chars          = strmem.addr ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err);
   return err ;
}

#define EXPAND_cstring(cstr, append_size)                      \
   {                                                           \
      size_t allocate_size = cstr->length + 1 + append_size ;  \
      if (allocate_size < append_size) {                       \
         TRACEOUTOFMEM_LOG(SIZE_MAX) ;                         \
         err = ENOMEM ;                                        \
         goto ONABORT ;                                        \
      }                                                        \
                                                               \
      err = allocate_cstring(cstr, allocate_size) ;            \
      if (err) goto ONABORT ;                                  \
   }

int append_cstring(cstring_t * cstr, size_t str_len, const char str[str_len])
{
   int err ;

   if (!str_len) {
      return 0 ;
   }

   EXPAND_cstring(cstr, str_len) ;

   memcpy(&cstr->chars[cstr->length], str, str_len) ;
   cstr->length              += str_len ;
   cstr->chars[cstr->length]  = 0 ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err);
   return err ;
}

int printfappend_cstring(cstring_t * cstr, const char * format, ...)
{
   int err ;

   for(;;) {
      size_t append_size ;
      size_t preallocated_size ;

      preallocated_size = cstr->allocated_size - cstr->length ;

      {
         va_list args ;
         va_start(args, format) ;
         int append_size_ = vsnprintf(str_cstring(cstr) + cstr->length, preallocated_size, format, args) ;
         va_end(args) ;

         if (append_size_ < 0) {
            TRACEOUTOFMEM_LOG(SIZE_MAX) ;
            err = ENOMEM ;
            goto ONABORT ;
         }

         append_size  = (size_t) append_size_ ;
      }

      if (append_size < preallocated_size) {
         cstr->length += append_size ;
         break ;
      }

      EXPAND_cstring(cstr, append_size) ;
   }

   return 0 ;
ONABORT:
   if (cstr->allocated_size) cstr->chars[cstr->length] = 0 ;
   TRACEABORT_LOG(err);
   return err ;
}

int resize_cstring(cstring_t * cstr, size_t new_length)
{
   int err ;

   if (new_length < cstr->length) {
      err = truncate_cstring(cstr, new_length) ;
      if (err) goto ONABORT ;
   } else if (new_length > cstr->length) {
      size_t append_size = new_length - cstr->length ;
      EXPAND_cstring(cstr, append_size) ;
      cstr->length = new_length ;
      cstr->chars[new_length] = 0 ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err);
   return err ;
}

int truncate_cstring(cstring_t * cstr, size_t new_length)
{
   int err ;

   VALIDATE_INPARAM_TEST(new_length <= cstr->length, ONABORT, PRINTSIZE_LOG(cstr->length) ; PRINTSIZE_LOG(new_length)) ;

   if (new_length < cstr->length) {
      cstr->length            = new_length ;
      cstr->chars[new_length] = 0 ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err);
   return err ;
}

void clear_cstring(cstring_t * cstr)
{
   if (cstr->length) {
      cstr->length   = 0 ;
      cstr->chars[0] = 0 ;
   }
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   cstring_t   cstr  = cstring_INIT_FREEABLE ;
   cstring_t   cstr2 = cstring_INIT ;

   // TEST static init cstring_INIT_FREEABLE
   TEST(0 == cstr.allocated_size)
   TEST(0 == cstr.length)
   TEST(0 == cstr.chars)

   // TEST static init cstring_INIT
   cstr = (cstring_t) cstring_INIT ;
   TEST(0 == cstr.allocated_size) ;
   TEST(0 == cstr.length) ;
   TEST(0 == cstr.chars) ;

   // TEST init, double free
   cstr.length = 1 ;
   TEST(0 == init_cstring(&cstr, 256)) ;
   TEST(0 == cstr.length) ;
   TEST(256 == cstr.allocated_size) ;
   TEST(0 != cstr.chars) ;
   TEST(0 == free_cstring(&cstr)) ;
   TEST(0 == cstr.allocated_size) ;
   TEST(0 == cstr.length) ;
   TEST(0 == cstr.chars) ;
   TEST(0 == free_cstring(&cstr)) ;
   TEST(0 == cstr.allocated_size) ;
   TEST(0 == cstr.length) ;
   TEST(0 == cstr.chars) ;

   // TEST initfromstring_cstring on empty string
   conststring_t stringfrom = conststring_INIT_FREEABLE ;
   TEST(0 == initfromstring_cstring(&cstr, &stringfrom)) ;
   TEST(0 == cstr.length) ;
   TEST(0 == cstr.allocated_size) ;
   TEST(0 == cstr.chars) ;

   // TEST initfromstring_cstring
   stringfrom = (conststring_t) conststring_INIT(7, (const uint8_t*)"x1y2z3!") ;
   TEST(0 == initfromstring_cstring(&cstr, &stringfrom)) ;
   TEST(7 == cstr.length) ;
   TEST(8 == cstr.allocated_size) ;
   TEST(0 != cstr.chars) ;
   TEST(stringfrom.addr != (void*)cstr.chars) ;
   TEST(0 == strcmp(str_cstring(&cstr), "x1y2z3!")) ;
   TEST(0 == free_cstring(&cstr)) ;
   TEST(0 == cstr.allocated_size) ;
   TEST(0 == cstr.length) ;
   TEST(0 == cstr.chars) ;

   // TEST initmove on empty string
   initmove_cstring(&cstr, &cstr2) ;
   TEST(0 == cstr.allocated_size) ;
   TEST(0 == cstr.length) ;
   TEST(0 == cstr.chars) ;
   TEST(0 == cstr2.allocated_size) ;
   TEST(0 == cstr2.length) ;
   TEST(0 == cstr2.chars) ;

   // TEST initmove
   TEST(0 == init_cstring(&cstr2, 64)) ;
   TEST(0 == append_cstring(&cstr2, 6, "123456" )) ;
   cstring_t xxx = cstr2 ;
   initmove_cstring(&cstr, &cstr2) ;
   TEST(0 == memcmp(&cstr, &xxx, sizeof(cstr))) ;
   TEST(0 == strcmp(str_cstring(&cstr), "123456")) ;
   TEST(64 == cstr.allocated_size) ;
   TEST(0 == cstr2.allocated_size) ;
   TEST(0 == cstr2.length) ;
   TEST(0 == cstr2.chars) ;
   TEST(0 == free_cstring(&cstr)) ;

   return 0 ;
ONABORT:
   free_cstring(&cstr) ;
   free_cstring(&cstr2) ;
   return EINVAL ;
}

static int test_changeandquery(void)
{
   cstring_t   cstr  = cstring_INIT_FREEABLE ;
   cstring_t   cstr2 = cstring_INIT ;

   // TEST allocatedsize_cstring
   TEST(0 == allocatedsize_cstring(&cstr)) ;
   TEST(0 == allocatedsize_cstring(&cstr2)) ;
   cstr.allocated_size = 123 ;
   cstr2.allocated_size = 125 ;
   TEST(123 == allocatedsize_cstring(&cstr)) ;
   TEST(125 == allocatedsize_cstring(&cstr2)) ;
   cstr.allocated_size = 0 ;
   cstr2.allocated_size = 0 ;
   TEST(0 == allocatedsize_cstring(&cstr)) ;
   TEST(0 == allocatedsize_cstring(&cstr2)) ;

   // TEST str_cstring
   TEST(0 == cstr.chars) ;
   TEST(0 == str_cstring(&cstr)) ;
   TEST(0 == cstr2.chars) ;
   TEST(0 == str_cstring(&cstr2)) ;
   {
      cstring_t   cstr3 = cstring_INIT ;
      char        testbuffer[100] ;
      TEST(0 == str_cstring(&cstr3)) ;
      cstr3.chars = (uint8_t*) 1 ;
      TEST((char*)1 == str_cstring(&cstr3)) ;
      cstr3.chars = (uint8_t*) testbuffer ;
      TEST(testbuffer == str_cstring(&cstr3)) ;
   }

   // TEST printfappend on empty string
   TEST(0 == printfappend_cstring(&cstr, "%d%s", 123, "456")) ;
   TEST(6 == cstr.length) ;
   TEST(7 == cstr.allocated_size) ;
   TEST(0 == strcmp(str_cstring(&cstr), "123456")) ;

   // TEST printfappend (size doubled)
   TEST(0 == printfappend_cstring(&cstr, "7")) ;
   TEST(7 == cstr.length) ;
   TEST(14 == cstr.allocated_size) ; // size doubled
   TEST(0 == strcmp(str_cstring(&cstr), "1234567")) ;

   // TEST printfappend (exact size)
   const char * str = "1234567890" ;
   TEST(0 == printfappend_cstring(&cstr, "%s%s%s", str, str, str)) ;
   TEST(37 == cstr.length) ;
   TEST(38 == cstr.allocated_size) ; // exact size
   TEST(0 == strcmp(str_cstring(&cstr), "1234567123456789012345678901234567890")) ;
   TEST(0 == free_cstring(&cstr)) ;

   // TEST printfappend (does not change content in case of error)
   {  uint8_t buffer[4] = "123" ;
      cstring_t cstr3 = { .length = (size_t)-2, .allocated_size = 1 + (size_t)-2, .chars = (&buffer[3] - (size_t)-2) } ;
      TEST(&buffer[3] == (cstr3.chars + cstr3.length) ) ;
      TEST(ENOMEM == printfappend_cstring(&cstr3, "XX")) ;
      TEST(cstr3.length         == (size_t)-2) ;
      TEST(cstr3.allocated_size == 1 + (size_t)-2) ;
      TEST(0 == strcmp((char*)buffer, "123")) ;
   }

   // TEST append on empty string
   TEST(0 == append_cstring(&cstr, 6, "123456")) ;
   TEST(6 == cstr.length) ;
   TEST(7 == cstr.allocated_size) ;
   TEST(0 == strcmp(str_cstring(&cstr), "123456")) ;

   // TEST append (size doubled)
   TEST(0 == append_cstring(&cstr, 1, "7")) ;
   TEST(7 == cstr.length) ;
   TEST(14 == cstr.allocated_size) ; // size doubled
   TEST(0 == strcmp(str_cstring(&cstr), "1234567")) ;

   // TEST append (exact size)
   TEST(0 == free_cstring(&cstr)) ;
   TEST(0 == init_cstring(&cstr, 4)) ;
   TEST(4 == cstr.allocated_size) ;
   str = "123456789012345" ;
   TEST(0 == append_cstring(&cstr, 15, str)) ;
   TEST(15 == cstr.length) ;
   TEST(16 == cstr.allocated_size) ; // exact size
   TEST(0 == strcmp(str_cstring(&cstr), str)) ;
   TEST(0 == free_cstring(&cstr)) ;

   // TEST append (does not change content in case of error)
   {  uint8_t buffer[10] = "123" ;
      cstring_t cstr3 = { .length = (size_t)-2, .allocated_size = 1 + (size_t)-2, .chars = (&buffer[3] - (size_t)-2) } ;
      TEST(&buffer[3] == (cstr3.chars + cstr3.length) ) ;
      TEST(ENOMEM == append_cstring(&cstr3, 2, "XX")) ;
      TEST(cstr3.length         == (size_t)-2) ;
      TEST(cstr3.allocated_size == 1 + (size_t)-2) ;
      TEST(0 == strcmp((char*)buffer, "123")) ;
   }

   // TEST truncate_cstring on empty string
   TEST(0 == truncate_cstring(&cstr, 0)) ;
   TEST(0 == cstr.allocated_size) ;
   TEST(0 == cstr.length) ;
   TEST(0 == cstr.chars) ;

   // TEST truncate_cstring
   TEST(0 == init_cstring(&cstr, 256)) ;
   TEST(cstr.allocated_size == 256) ;
   TEST(cstr.length == 0) ;
   TEST(0 == append_cstring( &cstr, strlen("->123456: "), "->123456: ")) ;
   TEST(strlen("->123456: ") == cstr.length) ;
   for(unsigned i = strlen("->123456: ") + 1; i > 0; ) {
      -- i ;
      TEST(0 == truncate_cstring(&cstr, i)) ;
      TEST(i == cstr.length) ;
      TEST(0 == strncmp(str_cstring(&cstr), "->123456: ", i)) ;
      TEST(0 == cstr.chars[i]) ;
   }
   TEST(0 == free_cstring(&cstr)) ;

   // TEST allocate_cstring
   cstr = (cstring_t) cstring_INIT ;
   TEST(0 == cstr.allocated_size) ;
   TEST(0 == allocate_cstring(&cstr, 0)) ;
   TEST(0 == cstr.allocated_size) ;
   TEST(0 == allocate_cstring(&cstr, 10)) ;
   TEST(10 == cstr.allocated_size /*exact cause doubling is not enough*/) ;
   TEST(0 == allocate_cstring(&cstr, 21)) ;
   TEST(21 == cstr.allocated_size /*exact cause doubling is not enough*/) ;
   TEST(0 == allocate_cstring(&cstr, 40)) ;
   TEST(42 == cstr.allocated_size /*doubled*/) ;
   TEST(0 == allocate_cstring(&cstr, 200)) ;
   TEST(200 == cstr.allocated_size /*exact cause doubling is not enough*/) ;
   TEST(0 == free_cstring(&cstr)) ;

   return 0 ;
ONABORT:
   free_cstring(&cstr) ;
   free_cstring(&cstr2) ;
   return EINVAL ;
}

static int test_clear(void)
{
   cstring_t   cstr = cstring_INIT ;

   // TEST clear_cstring on empty string
   clear_cstring(&cstr) ;
   TEST(0 == cstr.allocated_size) ;
   TEST(0 == cstr.length) ;
   TEST(0 == cstr.chars) ;

   // TEST clear_cstring
   TEST(0 == init_cstring(&cstr, 256)) ;
   TEST(cstr.allocated_size == 256) ;
   for(int i = 255; i > 0; --i) {
      cstr.length = (size_t) i ;
      memset(cstr.chars, i, 256) ;
      clear_cstring(&cstr) ;
      TEST(0 == cstr.length) ;
      TEST(0 == cstr.chars[0]) ;
      TEST(i == cstr.chars[1]) ;
      TEST(i == cstr.chars[255]) ;
   }
   TEST(0 == free_cstring(&cstr)) ;

   return 0 ;
ONABORT:
   free_cstring(&cstr) ;
   return EINVAL ;
}

static int test_resize(void)
{
   cstring_t   cstr = cstring_INIT ;

   // TEST resize_cstring on empty string (do nothing)
   resize_cstring(&cstr, 0) ;
   TEST(0 == cstr.allocated_size) ;
   TEST(0 == cstr.length) ;
   TEST(0 == cstr.chars) ;

   // TEST resize_cstring on empty string (allocate)
   resize_cstring(&cstr, 255) ;
   TEST(256 == cstr.allocated_size) ;
   TEST(255 == cstr.length) ;
   TEST(0   != cstr.chars) ;
   TEST(0   == cstr.chars[255]) ;
   TEST(0   == free_cstring(&cstr)) ;

   // TEST resize_cstring: shrink
   TEST(0 == init_cstring(&cstr, 256)) ;
   TEST(cstr.allocated_size == 256) ;
   for(int i = 255; i >= 0; --i) {
      cstr.length = 256 ;
      memset(cstr.chars, i, 256) ;
      TEST(0 == resize_cstring(&cstr, (size_t) i)) ;
      TEST(i == (ssize_t)cstr.length) ;
      TEST(0 == cstr.chars[i]) ;
      TEST(i == cstr.chars[0]) ;
      if (i != 255) {
         TEST(i == cstr.chars[255]) ;
      }
   }
   TEST(0 == free_cstring(&cstr)) ;

   // TEST resize_cstring: growth
   TEST(0 == init_cstring(&cstr, 16)) ;
   TEST(cstr.allocated_size == 16) ;
   for(unsigned i = 16, previ = 0; i <= 4096; previ = i, i *= 2) {
      TEST(0 == resize_cstring(&cstr, i-1)) ;
      TEST(i == cstr.allocated_size) ;
      TEST(i-1 == cstr.length) ;
      TEST(0 == cstr.chars[i-1]) ;
      memset(&cstr.chars[previ], 1, i-1-previ) ;
   }
   for(unsigned i = 0, izero = 15; i < 4096; ++i) {
      if (i == izero) {
         TEST(0 == cstr.chars[i]) ;
         izero = izero * 2 + 1 ;
      } else {
         TEST(1 == cstr.chars[i]) ;
      }
   }
   TEST(0 == free_cstring(&cstr)) ;

   return 0 ;
ONABORT:
   free_cstring(&cstr) ;
   return EINVAL ;
}

int unittest_string_cstring()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())       goto ONABORT ;
   if (test_changeandquery()) goto ONABORT ;
   if (test_clear())          goto ONABORT ;
   if (test_resize())         goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
