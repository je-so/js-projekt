/* title: String-Converter impl
   Implements <String-Converter>.

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

   file: C-kern/api/string/converter.h
    Header file of <String-Converter>.

   file: C-kern/string/converter.c
    Implementation file <String-Converter impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/string/converter.h"
#include "C-kern/api/os/locale.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


int init_wstringconverter(/*out*/ wstring_converter_t * conv, size_t input_len, const char * input_string )
{
   memset( &conv->internal_state, 0, sizeof(conv->internal_state)) ;
   conv->next_input_char = input_string ;
   conv->input_len       = input_len ;
   return 0 ;
}

int copy_wstringconverter(/*out*/wstring_converter_t * restrict dest, const wstring_converter_t * restrict source )
{
   memcpy( dest, source, sizeof(*dest)) ;
   return 0 ;
}

int skip_wstringconverter( wstring_converter_t * conv, size_t char_count)
{
   int err ;
   for(size_t i = char_count; i; --i) {
      wchar_t  dummy ;

      if (!conv->input_len) {
         err = ENODATA ;
         goto ABBRUCH ;
      }

      size_t bytes = mbrtowc( &dummy, conv->next_input_char, conv->input_len, &conv->internal_state) ;

      if (bytes > conv->input_len) {
         err = EILSEQ ;
         goto ABBRUCH ;
      } else {
         conv->input_len       -= bytes ;
         conv->next_input_char += bytes ;
      }
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int peek_wstringconverter( const wstring_converter_t * conv, size_t char_count, wchar_t * wchar_array )
{
   int err ;
   wstring_converter_t conv2 ;

   memcpy( &conv2, conv, sizeof(wstring_converter_t)) ;

   for(size_t i = 0; i < char_count; ++i) {

      if (!conv2.input_len) {
         wchar_array[i] = 0 ;
         continue ;
      }

      size_t bytes = mbrtowc( &wchar_array[i], conv2.next_input_char, conv2.input_len, &conv2.internal_state) ;

      if (bytes > conv2.input_len) {
         err = EILSEQ ;
         goto ABBRUCH ;
      } else {
         conv2.input_len       -= bytes ;
         conv2.next_input_char += bytes ;
      }
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}



#ifdef KONFIG_UNITTEST

#define TEST(ARG) TEST_ONERROR_GOTO(ARG,unittest_string_converter,ABBRUCH)

/* Works only in utf8 - locale */
static int test_wstringconverter_fromutf8(void)
{
   wstring_converter_t conv ;
   const char        * encoding = charencoding_locale() ;

   if (strcmp(encoding,"UTF-8")) {
      LOG_STRING(encoding) ;
      TEST(0 == strcmp(encoding,"UTF-8")) ;
   }

   // TEST init + double free
   TEST(0 == init_wstringconverter(&conv, 1, "")) ;
   TEST(0 == free_wstringconverter(&conv)) ;
   TEST(0 == free_wstringconverter(&conv)) ;

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
      TEST(0 == init_wstringconverter(&conv, strlen(testcases[i].cstring), testcases[i].cstring)) ;
      wstring_converter_t conv2 ;
      TEST(0 == copy_wstringconverter(&conv2, &conv)) ;
      // skip
      for(int len = 0; testcases[i].wstring[len-1]; ++len) {
         wstring_converter_t conv3 ;
         TEST(0 == copy_wstringconverter(&conv3, &conv2)) ;
         TEST(0 == skip_wstringconverter(&conv3, (size_t)len)) ;
         TEST(0 == nextwchar_wstringconverter( &conv3, &next_wchar )) ;
         TEST(next_wchar == testcases[i].wstring[len]) ;
         TEST(0 == free_wstringconverter(&conv3)) ;
         if (!testcases[i].wstring[len]) {
            TEST(0 == copy_wstringconverter(&conv3, &conv2)) ;
            TEST(ENODATA == skip_wstringconverter(&conv3, (size_t)len+1)) ;
            TEST(conv3.input_len       == 0) ;
            TEST(conv3.next_input_char == testcases[i].cstring + strlen(testcases[i].cstring)) ;
            TEST(0 == free_wstringconverter(&conv3)) ;
         }
      }
      // peek
      for(int len = 0; testcases[i].wstring[len-1]; ++len) {
         wchar_t wchar_array[10] = {0} ;
         wstring_converter_t conv3 ;
         assert( len < 10 ) ;
         TEST(0 == copy_wstringconverter(&conv3, &conv2)) ;
         TEST(0 == peek_wstringconverter(&conv3, (size_t)len, wchar_array )) ;
         for(int len2 = 0; len2 < 10; ++len2) {
            if (len2 < len) {
               TEST(wchar_array[len2] == testcases[i].wstring[len2]) ;
            } else {
               TEST(wchar_array[len2] == 0) ;
            }
         }
         TEST(0 == nextwchar_wstringconverter( &conv3, &next_wchar )) ;
         TEST(next_wchar == testcases[i].wstring[0]) ;
         TEST(0 == free_wstringconverter(&conv3)) ;
         if (!testcases[i].wstring[len]) {
            TEST(0 == copy_wstringconverter(&conv3, &conv2)) ;
            TEST(0 == peek_wstringconverter(&conv3, (size_t)len+1, wchar_array)) ;
            TEST(conv3.input_len       == strlen(testcases[i].cstring)) ;
            TEST(conv3.next_input_char == testcases[i].cstring) ;
            for(int len2 = 0; len2 < 10; ++len2) {
               if (len2 < len) {
                  TEST(wchar_array[len2] == testcases[i].wstring[len2]) ;
               } else {
                  TEST(wchar_array[len2] == 0) ;
               }
            }
            TEST(0 == free_wstringconverter(&conv3)) ;
         }
      }
      // next + copy
      for(int len = 0; testcases[i].wstring[len]; ++len) {
         {
            wstring_converter_t conv3 ;
            TEST(0 == copy_wstringconverter(&conv3, &conv2)) ;
            next_wchar = 0 ;
            TEST(0 == nextwchar_wstringconverter( &conv3, &next_wchar )) ;
            TEST(next_wchar == testcases[i].wstring[len]) ;
            TEST(0 == free_wstringconverter(&conv3)) ;
         }
         next_wchar = 0 ;
         TEST(0 == nextwchar_wstringconverter( &conv, &next_wchar )) ;
         TEST(next_wchar == testcases[i].wstring[len]) ;
         next_wchar = 0 ;
         TEST(0 == nextwchar_wstringconverter( &conv2, &next_wchar )) ;
         TEST(next_wchar == testcases[i].wstring[len]) ;
      }
      // Test end of input
      next_wchar = (wchar_t)1 ;
      TEST(0 == nextwchar_wstringconverter( &conv, &next_wchar )) ;
      TEST(0 == next_wchar) ;
      next_wchar = (wchar_t)1 ;
      TEST(0 == nextwchar_wstringconverter( &conv, &next_wchar )) ;
      TEST(0 == next_wchar) ;
      next_wchar = (wchar_t)1 ;
      TEST(0 == nextwchar_wstringconverter( &conv2, &next_wchar )) ;
      TEST(0 == next_wchar) ;
      next_wchar = (wchar_t)1 ;
      TEST(0 == nextwchar_wstringconverter( &conv2, &next_wchar )) ;
      TEST(0 == next_wchar) ;
      TEST(0 == free_wstringconverter(&conv2)) ;
      TEST(0 == free_wstringconverter(&conv)) ;
   }

   // TEST EILSEQ (incomplete sequence)
   {
      wchar_t next_wchar ;
      const char * cstring = "\xf0\x90\x8e\xa3\xf0\x90\x8e\xa3" ;
      TEST(0 == init_wstringconverter(&conv, strlen(cstring)-1/*incomplete sequence*/, cstring)) ;
      TEST(0 == nextwchar_wstringconverter( &conv, &next_wchar )) ;
      TEST(EILSEQ == nextwchar_wstringconverter( &conv, &next_wchar )) ;
      TEST(conv.input_len == 3) ;
      TEST(conv.next_input_char == cstring+4) ;
      TEST(0 == free_wstringconverter(&conv)) ;
      TEST(0 == init_wstringconverter(&conv, strlen(cstring)-1/*incomplete sequence*/, cstring)) ;
      wchar_t wchar_array[2] = { 0, 0 } ;
      TEST(EILSEQ == peek_wstringconverter( &conv, 2, wchar_array)) ;
      TEST(conv.input_len == 7) ;
      TEST(conv.next_input_char == cstring) ;
      TEST(0 == free_wstringconverter(&conv)) ;
      TEST(0 == init_wstringconverter(&conv, strlen(cstring)-1/*incomplete sequence*/, cstring)) ;
      TEST(EILSEQ == skip_wstringconverter( &conv, 2)) ;
      TEST(conv.input_len == 3) ;
      TEST(conv.next_input_char == cstring+4) ;
      TEST(0 == free_wstringconverter(&conv)) ;
   }

   // TEST EILSEQ (invalid sequence)
   {
      wchar_t next_wchar ;
      const char * cstring = "\xf0\x90\x8e\xa3\xf0\x40" ;
      TEST(0 == init_wstringconverter(&conv, strlen(cstring)/*invalid sequence*/, cstring)) ;
      TEST(0 == nextwchar_wstringconverter( &conv, &next_wchar )) ;
      TEST(EILSEQ == nextwchar_wstringconverter( &conv, &next_wchar )) ;
      TEST(conv.input_len == 2) ;
      TEST(conv.next_input_char == cstring+4) ;
      TEST(0 == free_wstringconverter(&conv)) ;
      wchar_t wchar_array[2] = { 0, 0 } ;
      TEST(0 == init_wstringconverter(&conv, strlen(cstring), cstring)) ;
      TEST(EILSEQ == peek_wstringconverter( &conv, 2, wchar_array)) ;
      TEST(conv.input_len == 6) ;
      TEST(conv.next_input_char == cstring) ;
      TEST( wchar_array[0] == L'\x103A3' ) ;
      TEST(0 == free_wstringconverter(&conv)) ;
      TEST(0 == init_wstringconverter(&conv, strlen(cstring), cstring)) ;
      TEST(EILSEQ == skip_wstringconverter( &conv, 2)) ;
      TEST(conv.input_len == 2) ;
      TEST(conv.next_input_char == cstring+4) ;
      TEST(0 == free_wstringconverter(&conv)) ;
   }

   return 0 ;
ABBRUCH:
   (void) free_wstringconverter(&conv) ;
   return 1 ;
}

int unittest_string_converter()
{
   if (test_wstringconverter_fromutf8()) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   return 1 ;
}

#endif
