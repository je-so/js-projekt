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
   char * preallocated_buffer = 0 ;

   if (preallocate_size) {
      preallocated_buffer = (char*) malloc( preallocate_size ) ;
      if (!preallocated_buffer) {
         LOG_OUTOFMEMORY(preallocate_size) ;
         err = ENOMEM ;
         goto ABBRUCH ;
      }
   }

   cstr->length         = 0 ;
   cstr->allocated_size = preallocate_size ;
   cstr->chars          = preallocated_buffer ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err);
   return err ;
}

int free_cstring(cstring_t * cstr)
{
   if (cstr->allocated_size) {
      free(cstr->chars) ;
      MEMSET0(cstr) ;
   }

   return 0 ;
}

char * str_cstring(cstring_t * cstr)
{
   if (cstr->chars) {
      return cstr->chars ;
   }

   return (char*) (intptr_t) "" ;
}

int allocate_cstring(cstring_t * cstr, size_t allocate_size)
{
   int err ;

   if (allocate_size <= cstr->allocated_size) {
      return 0 ;
   }

   size_t new_size = 2 * cstr->allocated_size ;

   if (new_size < allocate_size) {
      new_size = allocate_size ;
   }

   char * expanded_buffer = ((ssize_t)new_size > 0) ? (char *) realloc( cstr->chars, new_size ) : 0 ;

   if (!expanded_buffer) {
      LOG_OUTOFMEMORY(new_size) ;
      err = ENOMEM ;
      goto ABBRUCH ;
   }

   if (!cstr->chars) {
      expanded_buffer[0] = 0 ;
   }

   cstr->allocated_size = new_size ;
   cstr->chars          = expanded_buffer ;
   return 0 ;
ABBRUCH:
   LOG_ABORT(err);
   return err ;
}

#define EXPAND_cstring(cstr, append_size)                      \
   {                                                           \
      size_t allocate_size = cstr->length + 1 + append_size ;  \
      if (allocate_size < append_size) {                       \
         LOG_OUTOFMEMORY(-1) ;                                 \
         err = ENOMEM ;                                        \
         goto ABBRUCH ;                                        \
      }                                                        \
                                                               \
      err = allocate_cstring(cstr, allocate_size) ;            \
      if (err) goto ABBRUCH ;                                  \
   }

int append_cstring(cstring_t * cstr, size_t str_len, const char str[str_len])
{
   int err ;

   if (!str_len) {
      return 0 ;
   }

   EXPAND_cstring(cstr, str_len) ;

   memcpy( &cstr->chars[cstr->length], str, str_len) ;
   cstr->length              += str_len ;
   cstr->chars[cstr->length]  = 0 ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err);
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
         int append_size_ = vsnprintf(cstr->chars + cstr->length, preallocated_size, format, args) ;
         va_end(args) ;

         if (append_size_ < 0) {
            LOG_OUTOFMEMORY(-1) ;
            err = ENOMEM ;
            goto ABBRUCH ;
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
ABBRUCH:
   if (cstr->allocated_size) cstr->chars[cstr->length] = 0 ;
   LOG_ABORT(err);
   return err ;
}

int truncate_cstring(cstring_t * cstr, size_t new_length)
{
   int err ;

   PRECONDITION_INPUT(new_length <= cstr->length, ABBRUCH, LOG_SIZE(cstr->length) ; LOG_SIZE(new_length)) ;

   if (new_length < cstr->length) {
      cstr->length            = new_length ;
      cstr->chars[new_length] = 0 ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err);
   return err ;
}


#ifdef KONFIG_UNITTEST

#define TEST(ARG) TEST_ONERROR_GOTO(ARG, ABBRUCH)

static int test_initfree(void)
{
   cstring_t   cstr  = cstring_INIT_FREEABLE ;
   cstring_t   cstr2 = cstring_INIT ;

   // TEST static init
   TEST(0 == cstr.allocated_size)
   TEST(0 == cstr.length)
   TEST(0 == cstr.chars)
   static_assert( sizeof(cstr) == 2*sizeof(size_t)+sizeof(void*), "" );
   TEST(0 == cstr2.allocated_size)
   TEST(0 == cstr2.length)
   TEST(0 == cstr2.chars)

   // TEST str_cstring
   TEST(0 == cstr.chars) ;
   TEST(0 != str_cstring(&cstr)) ;
   TEST(0 == cstr2.chars) ;
   TEST(0 != str_cstring(&cstr2)) ;
   TEST(0 == strcmp("", str_cstring(&cstr))) ;
   TEST(0 == strcmp("", str_cstring(&cstr2))) ;
   {
      cstring_t   cstr3 = cstring_INIT ;
      char        testbuffer[100] ;
      TEST(0 != str_cstring(&cstr3)) ;
      TEST(0 == strcmp("", str_cstring(&cstr3))) ;
      cstr3.chars = (char*)1 ;
      TEST((char*)1 == str_cstring(&cstr3)) ;
      cstr3.chars = testbuffer ;
      TEST(testbuffer == str_cstring(&cstr3)) ;
   }

   // TEST init,double free
   cstr.length = 1 ;
   TEST(0 == init_cstring(&cstr, 256)) ;
   TEST(0 == cstr.length)
   TEST(256 == cstr.allocated_size)
   TEST(0 != cstr.chars)
   TEST(0 == free_cstring(&cstr)) ;
   TEST(0 == cstr.allocated_size)
   TEST(0 == cstr.length)
   TEST(0 == cstr.chars)
   TEST(0 == free_cstring(&cstr)) ;
   TEST(0 == cstr.allocated_size)
   TEST(0 == cstr.length)
   TEST(0 == cstr.chars)

   // TEST printfappend on empty string
   TEST(0 == printfappend_cstring(&cstr, "%d%s", 123, "456")) ;
   TEST(6 == cstr.length) ;
   TEST(7 == cstr.allocated_size) ;
   TEST(0 == strcmp(cstr.chars, "123456")) ;

   // TEST printfappend (size doubled)
   TEST(0 == printfappend_cstring(&cstr, "7")) ;
   TEST(7 == cstr.length) ;
   TEST(14 == cstr.allocated_size) ; // size doubled
   TEST(0 == strcmp(cstr.chars, "1234567")) ;

   // TEST printfappend (exact size)
   const char * str = "1234567890" ;
   TEST(0 == printfappend_cstring(&cstr, "%s%s%s", str, str, str)) ;
   TEST(37 == cstr.length) ;
   TEST(38 == cstr.allocated_size) ; // exact size
   TEST(0 == strcmp(cstr.chars, "1234567123456789012345678901234567890")) ;
   TEST(0 == free_cstring(&cstr)) ;

   // TEST printfappend (does not change content in case of error)
   {  char buffer[10] = "123" ;
      cstring_t cstr3 = { .length = (size_t)-10, .allocated_size = 2 + (size_t)-10, .chars = (&buffer[3] - (size_t)-10) } ;
      TEST(&buffer[3] == (cstr3.chars + cstr3.length) ) ;
      TEST(ENOMEM == printfappend_cstring(&cstr3, "XX")) ;
      TEST(cstr3.length         == (size_t)-10) ;
      TEST(cstr3.allocated_size == 2 + (size_t)-10) ;
      TEST(0 == strcmp(buffer, "123")) ;
   }

   // TEST append on empty string
   TEST(0 == append_cstring(&cstr, 6, "123456")) ;
   TEST(6 == cstr.length) ;
   TEST(7 == cstr.allocated_size) ;
   TEST(0 == strcmp(cstr.chars, "123456")) ;

   // TEST append (size doubled)
   TEST(0 == append_cstring(&cstr, 1, "7")) ;
   TEST(7 == cstr.length) ;
   TEST(14 == cstr.allocated_size) ; // size doubled
   TEST(0 == strcmp(cstr.chars, "1234567")) ;

   // TEST append (exact size)
   TEST(0 == free_cstring(&cstr)) ;
   TEST(0 == init_cstring(&cstr, 4)) ;
   TEST(4 == cstr.allocated_size) ;
   str = "123456789012345" ;
   TEST(0 == append_cstring(&cstr, 15, str)) ;
   TEST(15 == cstr.length) ;
   TEST(16 == cstr.allocated_size) ; // exact size
   TEST(0 == strcmp(cstr.chars, str)) ;
   TEST(0 == free_cstring(&cstr)) ;

   // TEST append (does not change content in case of error)
   {  char buffer[10] = "123" ;
      cstring_t cstr3 = { .length = (size_t)-10, .allocated_size = 2 + (size_t)-10, .chars = (&buffer[3] - (size_t)-10) } ;
      TEST(&buffer[3] == (cstr3.chars + cstr3.length) ) ;
      TEST(ENOMEM == append_cstring(&cstr3, 2, "XX")) ;
      TEST(cstr3.length         == (size_t)-10) ;
      TEST(cstr3.allocated_size == 2 + (size_t)-10) ;
      TEST(0 == strcmp(buffer, "123")) ;
   }

   // TEST truncate on empty string
   TEST(0 == truncate_cstring(&cstr, 0)) ;
   TEST(0 == cstr.allocated_size) ;
   TEST(0 == cstr.length) ;
   TEST(0 == cstr.chars) ;

   // TEST truncate
   TEST(0 == init_cstring(&cstr, 256)) ;
   TEST(cstr.allocated_size == 256) ;
   TEST(cstr.length == 0) ;
   TEST(0 == append_cstring( &cstr, strlen("->123456: "), "->123456: ")) ;
   TEST(strlen("->123456: ") == cstr.length) ;
   for(unsigned i = strlen("->123456: ") + 1; i > 0; ) {
      -- i ;
      TEST(0 == truncate_cstring(&cstr, i)) ;
      TEST(i == cstr.length) ;
      TEST(0 == strncmp(cstr.chars, "->123456: ", i)) ;
      TEST(0 == cstr.chars[i]) ;
   }
   TEST(0 == free_cstring(&cstr)) ;

   // TEST adaptlength_cstring on empty string
   adaptlength_cstring(&cstr) ;
   TEST(0 == cstr.allocated_size) ;
   TEST(0 == cstr.length) ;
   TEST(0 == cstr.chars) ;

   // TEST adaptlength
   TEST(0 == init_cstring(&cstr, 64)) ;
   TEST(cstr.allocated_size == 64) ;
   TEST(cstr.length == 0) ;
   TEST(0 == printfappend_cstring(&cstr, "->%s: ", "123456")) ;
   TEST(strlen("->123456: ") == cstr.length) ;
   for(unsigned i = strlen("->123456: ") + 1; i > 0; ) {
      -- i ;
      cstr.chars[i] = 0 ;
      adaptlength_cstring(&cstr) ;
      TEST(i == cstr.length) ;
      TEST(0 == strncmp(cstr.chars, "->123456: ", i)) ;
   }
   TEST(0 == free_cstring(&cstr)) ;

   // TEST initmove on empty string
   TEST(0 == initmove_cstring(&cstr, &cstr2)) ;
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
   TEST(0 == initmove_cstring(&cstr, &cstr2)) ;
   TEST(0 == memcmp(&cstr, &xxx, sizeof(cstr))) ;
   TEST(0 == strcmp(cstr.chars, "123456")) ;
   TEST(64 == cstr.allocated_size) ;
   TEST(0 == cstr2.allocated_size) ;
   TEST(0 == cstr2.length) ;
   TEST(0 == cstr2.chars) ;
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
ABBRUCH:
   free_cstring(&cstr) ;
   free_cstring(&cstr2) ;
   return EINVAL ;
}

int unittest_string_cstring()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree()) goto ABBRUCH ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
