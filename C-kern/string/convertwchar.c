/* title: Convert-wchar impl
   Implements <Convert-wchar>.

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

   file: C-kern/api/string/convertwchar.h
    Header file of <Convert-wchar>.

   file: C-kern/string/convertwchar.c
    Implementation file <Convert-wchar impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/string/convertwchar.h"
#include "C-kern/api/platform/locale.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


int init_convertwchar(/*out*/convert_wchar_t * conv, size_t input_len, const char * input_string)
{
   conv->len  = input_len ;
   conv->next = input_string ;
   memset( &conv->internal_state, 0, sizeof(conv->internal_state)) ;
   return 0 ;
}

int initcopy_convertwchar(/*out*/convert_wchar_t * restrict dest, const convert_wchar_t * restrict source)
{
   memcpy( dest, source, sizeof(*dest)) ;
   return 0 ;
}

int skip_convertwchar(convert_wchar_t * conv, size_t char_count)
{
   int err ;
   for(size_t i = char_count; i; --i) {
      wchar_t  dummy ;

      if (!conv->len) {
         err = ENODATA ;
         goto ONABORT ;
      }

      size_t bytes = mbrtowc( &dummy, conv->next, conv->len, &conv->internal_state) ;

      if (bytes > conv->len) {
         err = EILSEQ ;
         goto ONABORT ;
      } else {
         conv->len  -= bytes ;
         conv->next += bytes ;
      }
   }

   return 0 ;
ONABORT:
   PRINTABORT_LOG(err) ;
   return err ;
}

int peek_convertwchar(const convert_wchar_t * conv, size_t char_count, wchar_t * wchar_array)
{
   int err ;
   convert_wchar_t conv2 ;

   memcpy( &conv2, conv, sizeof(convert_wchar_t)) ;

   for(size_t i = 0; i < char_count; ++i) {

      if (!conv2.len) {
         wchar_array[i] = 0 ;
         continue ;
      }

      size_t bytes = mbrtowc( &wchar_array[i], conv2.next, conv2.len, &conv2.internal_state) ;

      if (bytes > conv2.len) {
         err = EILSEQ ;
         goto ONABORT ;
      } else {
         conv2.len  -= bytes ;
         conv2.next += bytes ;
      }
   }

   return 0 ;
ONABORT:
   PRINTABORT_LOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

/* function: test_fromutf8
 * Works only in utf8 - locale
 */
static int test_fromutf8(void)
{
   convert_wchar_t conv ;
   const char        * encoding = charencoding_locale() ;

   if (strcmp(encoding,"UTF-8")) {
      PRINTCSTR_LOG(encoding) ;
      TEST(0 == strcmp(encoding,"UTF-8")) ;
   }

   // TEST init + double free
   TEST(0 == init_convertwchar(&conv, 1, "")) ;
   TEST(0 == free_convertwchar(&conv)) ;
   TEST(0 == free_convertwchar(&conv)) ;

   // TEST nextwchar, skip, peek, copy
   struct TestCase {
      const char    * cstring ;
      const wchar_t * wstring ;
   } testcases[] = {
       { /*utf8*/"\xc3\xa4\xc3\xb6\xc3\xbc", L"\x00e4\x00f6\x00fc" }
      ,{ /*utf8*/"\xf0\x90\x8e\xa3\xf0\x90\x8e\xa4\xf0\x90\x8e\xa5", L"\x103A3\x103A4\x103A5" }
      ,{ 0, 0 }
   } ;
   for(int i = 0; testcases[i].cstring ; ++i) {
      wchar_t next_wchar ;
      TEST(0 == init_convertwchar(&conv, strlen(testcases[i].cstring), testcases[i].cstring)) ;
      convert_wchar_t conv2 ;
      TEST(0 == initcopy_convertwchar(&conv2, &conv)) ;
      // skip
      for(int len = 0; testcases[i].wstring[len]; ++len) {
         convert_wchar_t conv3 ;
         TEST(0 == initcopy_convertwchar(&conv3, &conv2)) ;
         TEST(0 == skip_convertwchar(&conv3, (size_t)len)) ;
         TEST(0 == next_convertwchar( &conv3, &next_wchar )) ;
         TEST(next_wchar == testcases[i].wstring[len]) ;
         TEST(0 == free_convertwchar(&conv3)) ;
         if (!testcases[i].wstring[len+1]) {
            TEST(0 == initcopy_convertwchar(&conv3, &conv2)) ;
            TEST(ENODATA == skip_convertwchar(&conv3, (size_t)len+2)) ;
            TEST(conv3.len  == 0) ;
            TEST(conv3.next == testcases[i].cstring + strlen(testcases[i].cstring)) ;
            TEST(0 == free_convertwchar(&conv3)) ;
         }
      }
      // peek
      for(int len = 0; testcases[i].wstring[len-1]; ++len) {
         wchar_t wchar_array[10] = {0} ;
         convert_wchar_t conv3 ;
         assert( len < 10 ) ;
         TEST(0 == initcopy_convertwchar(&conv3, &conv2)) ;
         TEST(0 == peek_convertwchar(&conv3, (size_t)len, wchar_array )) ;
         for(int len2 = 0; len2 < 10; ++len2) {
            if (len2 < len) {
               TEST(wchar_array[len2] == testcases[i].wstring[len2]) ;
            } else {
               TEST(wchar_array[len2] == 0) ;
            }
         }
         TEST(0 == next_convertwchar( &conv3, &next_wchar )) ;
         TEST(next_wchar == testcases[i].wstring[0]) ;
         TEST(0 == free_convertwchar(&conv3)) ;
         if (!testcases[i].wstring[len]) {
            TEST(0 == initcopy_convertwchar(&conv3, &conv2)) ;
            TEST(0 == peek_convertwchar(&conv3, (size_t)len+1, wchar_array)) ;
            TEST(conv3.len  == strlen(testcases[i].cstring)) ;
            TEST(conv3.next == testcases[i].cstring) ;
            for(int len2 = 0; len2 < 10; ++len2) {
               if (len2 < len) {
                  TEST(wchar_array[len2] == testcases[i].wstring[len2]) ;
               } else {
                  TEST(wchar_array[len2] == 0) ;
               }
            }
            TEST(0 == free_convertwchar(&conv3)) ;
         }
      }
      // next + copy
      for(int len = 0; testcases[i].wstring[len]; ++len) {
         {
            convert_wchar_t conv3 ;
            TEST(0 == initcopy_convertwchar(&conv3, &conv2)) ;
            next_wchar = 0 ;
            TEST(0 == next_convertwchar( &conv3, &next_wchar )) ;
            TEST(next_wchar == testcases[i].wstring[len]) ;
            TEST(0 == free_convertwchar(&conv3)) ;
         }
         next_wchar = 0 ;
         TEST(0 == next_convertwchar( &conv, &next_wchar )) ;
         TEST(next_wchar == testcases[i].wstring[len]) ;
         next_wchar = 0 ;
         TEST(0 == next_convertwchar( &conv2, &next_wchar )) ;
         TEST(next_wchar == testcases[i].wstring[len]) ;
      }
      // Test end of input
      next_wchar = (wchar_t)1 ;
      TEST(0 == next_convertwchar( &conv, &next_wchar )) ;
      TEST(0 == next_wchar) ;
      next_wchar = (wchar_t)1 ;
      TEST(0 == next_convertwchar( &conv, &next_wchar )) ;
      TEST(0 == next_wchar) ;
      next_wchar = (wchar_t)1 ;
      TEST(0 == next_convertwchar( &conv2, &next_wchar )) ;
      TEST(0 == next_wchar) ;
      next_wchar = (wchar_t)1 ;
      TEST(0 == next_convertwchar( &conv2, &next_wchar )) ;
      TEST(0 == next_wchar) ;
      TEST(0 == free_convertwchar(&conv2)) ;
      TEST(0 == free_convertwchar(&conv)) ;
   }

   // TEST EILSEQ (incomplete sequence)
   {
      wchar_t next_wchar ;
      const char * cstring = "\xf0\x90\x8e\xa3\xf0\x90\x8e\xa3" ;
      TEST(0 == init_convertwchar(&conv, strlen(cstring)-1/*incomplete sequence*/, cstring)) ;
      TEST(0 == next_convertwchar( &conv, &next_wchar )) ;
      TEST(EILSEQ == next_convertwchar( &conv, &next_wchar )) ;
      TEST(conv.len  == 3) ;
      TEST(conv.next == cstring+4) ;
      TEST(0 == free_convertwchar(&conv)) ;
      TEST(0 == init_convertwchar(&conv, strlen(cstring)-1/*incomplete sequence*/, cstring)) ;
      wchar_t wchar_array[2] = { 0, 0 } ;
      TEST(EILSEQ == peek_convertwchar( &conv, 2, wchar_array)) ;
      TEST(conv.len  == 7) ;
      TEST(conv.next == cstring) ;
      TEST(0 == free_convertwchar(&conv)) ;
      TEST(0 == init_convertwchar(&conv, strlen(cstring)-1/*incomplete sequence*/, cstring)) ;
      TEST(EILSEQ == skip_convertwchar( &conv, 2)) ;
      TEST(conv.len  == 3) ;
      TEST(conv.next == cstring+4) ;
      TEST(0 == free_convertwchar(&conv)) ;
   }

   // TEST EILSEQ (invalid sequence)
   {
      wchar_t next_wchar ;
      const char * cstring = "\xf0\x90\x8e\xa3\xf0\x40" ;
      TEST(0 == init_convertwchar(&conv, strlen(cstring)/*invalid sequence*/, cstring)) ;
      TEST(0 == next_convertwchar( &conv, &next_wchar )) ;
      TEST(EILSEQ == next_convertwchar( &conv, &next_wchar )) ;
      TEST(conv.len  == 2) ;
      TEST(conv.next == cstring+4) ;
      TEST(0 == free_convertwchar(&conv)) ;
      wchar_t wchar_array[2] = { 0, 0 } ;
      TEST(0 == init_convertwchar(&conv, strlen(cstring), cstring)) ;
      TEST(EILSEQ == peek_convertwchar( &conv, 2, wchar_array)) ;
      TEST(conv.len  == 6) ;
      TEST(conv.next == cstring) ;
      TEST(wchar_array[0] == L'\x103A3' ) ;
      TEST(0 == free_convertwchar(&conv)) ;
      TEST(0 == init_convertwchar(&conv, strlen(cstring), cstring)) ;
      TEST(EILSEQ == skip_convertwchar( &conv, 2)) ;
      TEST(conv.len  == 2) ;
      TEST(conv.next == cstring+4) ;
      TEST(0 == free_convertwchar(&conv)) ;
   }

   return 0 ;
ONABORT:
   (void) free_convertwchar(&conv) ;
   return EINVAL ;
}

int unittest_string_convertwchar()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   if (test_fromutf8()) goto ONABORT ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_fromutf8()) goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
