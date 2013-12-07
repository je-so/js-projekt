/* title: Base64EncodeString impl
   Implements <Base64EncodeString>.

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

   file: C-kern/api/string/base64encode_string.h
    Header file <Base64EncodeString>.

   file: C-kern/string/base64encode_string.c
    Implementation file <Base64EncodeString impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/string/base64encode_string.h"
#include "C-kern/api/string/string.h"
#include "C-kern/api/memory/wbuffer.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#endif


// section: string_t

// group: base64encoding

static uint8_t    s_base64chars[64] = {
   "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
} ;

static int8_t     s_base64values[256] = {
   [0]  =-1,  [1] =-1,  [2] =-1,  [3] =-1,  [4] =-1,  [5] =-1,  [6] =-1,  [7] =-1,  [8] =-1,  [9] =-1,
   [10] =-1, [11] =-1, [12] =-1, [13] =-1, [14] =-1, [15] =-1, [16] =-1, [17] =-1, [18] =-1, [19] =-1,
   [20] =-1, [21] =-1, [22] =-1, [23] =-1, [24] =-1, [25] =-1, [26] =-1, [27] =-1, [28] =-1, [29] =-1,
   [30] =-1, [31] =-1, [32] =-1, [33] =-1, [34] =-1, [35] =-1, [36] =-1, [37] =-1, [38] =-1, [39] =-1,
   [40] =-1, [41] =-1, [42] =-1, ['+']=62, [44] =-1, [45] =-1, [46] =-1, ['/']=63, ['0']=52, ['1']=53,
   ['2']=54, ['3']=55, ['4']=56, ['5']=57, ['6']=58, ['7']=59, ['8']=60, ['9']=61, [58] =-1, [59] =-1,
   [60] =-1, [61] =-1, [62] =-1, [63] =62, [64] =-1, ['A']= 0, ['B']= 1, ['C']= 2, ['D']= 3, ['E']= 4,
   ['F']= 5, ['G']= 6, ['H']= 7, ['I']= 8, ['J']= 9, ['K']=10, ['L']=11, ['M']=12, ['N']=13, ['O']=14,
   ['P']=15, ['Q']=16, ['R']=17, ['S']=18, ['T']=19, ['U']=20, ['V']=21, ['W']=22, ['X']=23, ['Y']=24,
   ['Z']=25, [91] =-1, [92] =-1, [93] =-1, [94] =-1, [95] =-1, [96] =-1, ['a']=26, ['b']=27, ['c']=28,
   ['d']=29, ['e']=30, ['f']=31, ['g']=32, ['h']=33, ['i']=34, ['j']=35, ['k']=36, ['l']=37, ['m']=38,
   ['n']=39, ['o']=40, ['p']=41, ['q']=42, ['r']=43, ['s']=44, ['t']=45, ['u']=46, ['v']=47, ['w']=48,
   ['x']=49, ['y']=50, ['z']=51, [123]=-1, [124]=-1, [125]=-1, [126]=-1, [127]=-1, [128]=-1, [129]=-1,
   [130]=-1, [131]=-1, [132]=-1, [133]=-1, [134]=-1, [135]=-1, [136]=-1, [137]=-1, [138]=-1, [139]=-1,
   [140]=-1, [141]=-1, [142]=-1, [143]=-1, [144]=-1, [145]=-1, [146]=-1, [147]=-1, [148]=-1, [149]=-1,
   [150]=-1, [151]=-1, [152]=-1, [153]=-1, [154]=-1, [155]=-1, [156]=-1, [157]=-1, [158]=-1, [159]=-1,
   [160]=-1, [161]=-1, [162]=-1, [163]=-1, [164]=-1, [165]=-1, [166]=-1, [167]=-1, [168]=-1, [169]=-1,
   [170]=-1, [171]=-1, [172]=-1, [173]=-1, [174]=-1, [175]=-1, [176]=-1, [177]=-1, [178]=-1, [179]=-1,
   [180]=-1, [181]=-1, [182]=-1, [183]=-1, [184]=-1, [185]=-1, [186]=-1, [187]=-1, [188]=-1, [189]=-1,
   [190]=-1, [191]=-1, [192]=-1, [193]=-1, [194]=-1, [195]=-1, [196]=-1, [197]=-1, [198]=-1, [199]=-1,
   [200]=-1, [201]=-1, [202]=-1, [203]=-1, [204]=-1, [205]=-1, [206]=-1, [207]=-1, [208]=-1, [209]=-1,
   [210]=-1, [211]=-1, [212]=-1, [213]=-1, [214]=-1, [215]=-1, [216]=-1, [217]=-1, [218]=-1, [219]=-1,
   [220]=-1, [221]=-1, [222]=-1, [223]=-1, [224]=-1, [225]=-1, [226]=-1, [227]=-1, [228]=-1, [229]=-1,
   [230]=-1, [231]=-1, [232]=-1, [233]=-1, [234]=-1, [235]=-1, [236]=-1, [237]=-1, [238]=-1, [239]=-1,
   [240]=-1, [241]=-1, [242]=-1, [243]=-1, [244]=-1, [245]=-1, [246]=-1, [247]=-1, [248]=-1, [249]=-1,
   [250]=-1, [251]=-1, [252]=-1, [253]=-1, [254]=-1, [255]=-1
} ;

size_t sizebase64decode_string(const struct string_t * str)
{
   size_t size = str->size / 4;

   if (size) {
      size *= 3 ;

      if ('=' == str->addr[str->size-1]) {
         --size ;
         if ('=' == str->addr[str->size-2]) {
            --size ;
         }
      }
   }

   return size ;
}

int base64encode_string(const string_t * str, /*ret*/wbuffer_t * result)
{
   int err ;
   const uint8_t *next    = str->addr ;
   size_t         count   = str->size / 3 ;
   size_t         mod3    = str->size % 3 ;
   size_t         oldsize = size_wbuffer(result);
   uint32_t       tripple ;
   uint8_t       *dest ;

   size_t quadruples  = count + (mod3 != 0) ;
   size_t result_size = 4 * quadruples ;

   if (result_size <= quadruples) {
      err = EOVERFLOW ;
      goto ONABORT ;
   }

   err = appendbytes_wbuffer(result, result_size, &dest) ;
   if (err) goto ONABORT ;

   for (; count; -- count) {
      tripple = *(next ++) ;
      tripple <<= 8 ;
      tripple += *(next ++) ;
      tripple <<= 8 ;
      tripple += *(next ++) ;
      dest[3] = s_base64chars[tripple&63] ;
      tripple >>= 6 ;
      dest[2] = s_base64chars[tripple&63] ;
      tripple >>= 6 ;
      dest[1] = s_base64chars[tripple&63] ;
      tripple >>= 6 ;
      dest[0] = s_base64chars[tripple] ;
      dest += 4 ;
   }

   switch(mod3) {
   case 0:  break ;
   case 1:  tripple = *(next ++) ;
            tripple <<= 4 ;
            dest[3] = '=' ;
            dest[2] = '=' ;
            dest[1] = s_base64chars[tripple&63] ;
            tripple >>= 6 ;
            dest[0] = s_base64chars[tripple] ;
            break ;
   case 2:  tripple = *(next ++) ;
            tripple <<= 8 ;
            tripple += *(next ++) ;
            tripple <<= 2 ;
            dest[3] = '=' ;
            dest[2] = s_base64chars[tripple&63] ;
            tripple >>= 6 ;
            dest[1] = s_base64chars[tripple&63] ;
            tripple >>= 6 ;
            dest[0] = s_base64chars[tripple] ;
            break ;
   }

   return 0 ;
ONABORT:
   shrink_wbuffer(result, oldsize);
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int base64decode_string(const string_t * str, /*ret*/wbuffer_t * result)
{
   int err ;
   const uint8_t *next    = str->addr ;
   size_t         count   = str->size / 4 ;
   size_t         oldsize = size_wbuffer(result);
   unsigned       mod3 ;
   int32_t        quadr ;
   uint8_t       *dest ;

   VALIDATE_INPARAM_TEST(0 == (str->size % 4), ONABORT, ) ;

   if (!count) {
      return 0 ;
   }

   mod3 = (unsigned) ('=' == str->addr[str->size-1]) + ('=' == str->addr[str->size-2]) ;

   size_t result_size = 3 * count - mod3 ;

   err = appendbytes_wbuffer(result, result_size, &dest);
   if (err) goto ONABORT ;

   for (count = count - (mod3 != 0); count; -- count) {
      quadr = s_base64values[*(next ++)] ;
      quadr <<= 6 ;
      quadr |= s_base64values[*(next ++)] ;
      quadr <<= 6 ;
      quadr |= s_base64values[*(next ++)] ;
      quadr <<= 6 ;
      quadr |= s_base64values[*(next ++)] ;
      if (quadr < 0) {
         err = EINVAL ;
         goto ONABORT ;
      }
      dest[2] = (uint8_t) quadr ;
      quadr >>= 8 ;
      dest[1] = (uint8_t) quadr ;
      quadr >>= 8 ;
      dest[0] = (uint8_t) quadr ;
      dest += 3 ;
   }

   switch(mod3) {
   case 0:  break ;
   case 1:  quadr = s_base64values[*(next ++)] ;
            quadr <<= 6 ;
            quadr |= s_base64values[*(next ++)] ;
            quadr <<= 6 ;
            quadr |= s_base64values[*(next ++)] ;
            if (quadr < 0) {
               err = EINVAL ;
               goto ONABORT ;
            }
            quadr >>= 2 ;
            dest[1] = (uint8_t) quadr ;
            quadr >>= 8 ;
            dest[0] = (uint8_t) quadr ;
            break ;
   case 2:  quadr = s_base64values[*(next ++)] ;
            quadr <<= 6 ;
            quadr |= s_base64values[*(next ++)] ;
            if (quadr < 0) {
               err = EINVAL ;
               goto ONABORT ;
            }
            quadr >>= 4 ;
            dest[0] = (uint8_t) quadr ;
            break ;
   }

   return 0 ;
ONABORT:
   shrink_wbuffer(result, oldsize);
   TRACEABORT_ERRLOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_base64(void)
{
   memblock_t     data    = memblock_INIT_FREEABLE ;
   memblock_t     data2   = memblock_INIT_FREEABLE ;
   wbuffer_t      result  = wbuffer_INIT_MEMBLOCK(&data) ;
   wbuffer_t      result2 = wbuffer_INIT_MEMBLOCK(&data2) ;
   const uint8_t* test ;
   const uint8_t* test2 ;

   // TEST encode / decode
   test  = (const uint8_t*) "Test this string : 123" ;
   test2 = (const uint8_t*) "VGVzdCB0aGlzIHN0cmluZyA6IDEyMw==" ;
   TEST(0 == base64encode_string(&(string_t)string_INIT(strlen((const char*)test), test), &result)) ;
   TEST(strlen((const char*)test2) == size_wbuffer(&result)) ;
   TEST(0 == strncmp((const char*)test2, (char*)addr_memblock(&data), size_wbuffer(&result))) ;
   TEST(0 == base64decode_string(&(string_t)string_INIT(size_wbuffer(&result), addr_memblock(&data)), &result2)) ;
   TEST(strlen((const char*)test) == size_wbuffer(&result2)) ;
   TEST(0 == strncmp((const char*)test, (char*)addr_memblock(&data2), size_wbuffer(&result2))) ;

   // TEST all valid characters in decoding
   test  =  (const uint8_t*) "\x00\x10\x83\x10\x51\x87\x20\x92\x8b\x30\xd3\x8f\x41\x14\x93\x51"
            "\x55\x97\x61\x96\x9b\x71\xd7\x9f\x82\x18\xa3\x92\x59\xa7\xa2\x9a"
            "\xab\xb2\xdb\xaf\xc3\x1c\xb3\xd3\x5d\xb7\xe3\x9e\xbb\xf3\xdf\xbf" ;
   test2 = (const uint8_t*) "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/" ;
   clear_wbuffer(&result);
   TEST(0 == base64encode_string(&(string_t)string_INIT(48, test), &result)) ;
   TEST(64 == size_wbuffer(&result)) ;
   TEST(0 == strncmp((const char*)test2, (char*)addr_memblock(&data), size_wbuffer(&result))) ;
   clear_wbuffer(&result2);
   TEST(0 == base64decode_string(&(string_t)string_INIT(size_wbuffer(&result), addr_memblock(&data)), &result2)) ;
   TEST(48 == size_wbuffer(&result2)) ;
   TEST(0 == strncmp((const char*)test, (char*)addr_memblock(&data2), size_wbuffer(&result2))) ;

   // TEST all octets in encoding
   {
      test =   (const uint8_t*) "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4"
               "OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5fYGFiY2RlZmdoaWprbG1ub3Bx"
               "cnN0dXZ3eHl6e3x9fn+AgYKDhIWGh4iJiouMjY6PkJGSk5SVlpeYmZqbnJ2en6ChoqOkpaanqKmq"
               "q6ytrq+wsbKztLW2t7i5uru8vb6/wMHCw8TFxsfIycrLzM3Oz9DR0tPU1dbX2Nna29zd3t/g4eLj"
               "5OXm5+jp6uvs7e7v8PHy8/T19vf4+fr7/P3+/w==" ;
      uint8_t buffer[256] ;
      for (unsigned i = 0; i < 256; ++i) {
         buffer[i] = (uint8_t) i ;
      }
      clear_wbuffer(&result);
      TEST(0 == base64encode_string(&(string_t)string_INIT(256, buffer), &result)) ;
      TEST((4*((256+2) / 3)) == size_wbuffer(&result)) ;
      TEST((4*((256+2) / 3)) == strlen((const char*)test)) ;
      TEST(0 == strncmp((const char*)test, (char*)addr_memblock(&data), size_wbuffer(&result))) ;
      clear_wbuffer(&result2);
      TEST(0 == base64decode_string(&(string_t)string_INIT(size_wbuffer(&result), addr_memblock(&data)), &result2)) ;
      TEST(256 == size_wbuffer(&result2)) ;
      TEST(0 == memcmp(buffer, addr_memblock(&data2), size_wbuffer(&result2))) ;
   }

   // TEST not multiple of 3
   test  = (const uint8_t*)"X" ;
   test2 = (const uint8_t*)"WA==" ;
   clear_wbuffer(&result);
   TEST(0 == base64encode_string(&(string_t)string_INIT(strlen((const char*)test), test), &result)) ;
   TEST(4 == size_wbuffer(&result)) ;
   TEST(0 == strncmp((const char*)test2, (char*)addr_memblock(&data), 4)) ;
   clear_wbuffer(&result2);
   TEST(0 == base64decode_string(&(string_t)string_INIT(size_wbuffer(&result), addr_memblock(&data)), &result2)) ;
   TEST(1 == size_wbuffer(&result2)) ;
   TEST(0 == strncmp((const char*)test, (char*)addr_memblock(&data2), 1)) ;
   test  = (const uint8_t*)"XY" ;
   test2 = (const uint8_t*)"WFk=" ;
   clear_wbuffer(&result);
   TEST(0 == base64encode_string(&(string_t)string_INIT(strlen((const char*)test), test), &result)) ;
   TEST(4 == size_wbuffer(&result)) ;
   TEST(0 == strncmp((const char*)test2, (char*)addr_memblock(&data), 4)) ;
   clear_wbuffer(&result2);
   TEST(0 == base64decode_string(&(string_t)string_INIT(size_wbuffer(&result), addr_memblock(&data)), &result2)) ;
   TEST(2 == size_wbuffer(&result2)) ;
   TEST(0 == strncmp((const char*)test, (char*)addr_memblock(&data2), 2)) ;

   // TEST base64decode_string: EINVAL decode not multiple of 4
   size_t oldsize = size_wbuffer(&result);
   TEST(0 < oldsize);
   for (unsigned i = 1; i < 4; ++i) {
      TEST(EINVAL == base64decode_string(&(string_t)string_INIT(4*i+i, (const uint8_t*)""), &result)) ;
      TEST(oldsize == size_wbuffer(&result)) ;
   }

   // TEST base64decode_string: EINVAL decode wrong chars
   TEST(EINVAL == base64decode_string(&(string_t)string_INIT_CSTR("Ab3@"), &result)) ;
   TEST(EINVAL == base64decode_string(&(string_t)string_INIT_CSTR("Ab=4"), &result)) ;
   TEST(EINVAL == base64decode_string(&(string_t)string_INIT_CSTR("=b34"), &result)) ;
   TEST(oldsize == size_wbuffer(&result)) ;

   // unprepare
   TEST(0 == FREE_MM(&data)) ;
   TEST(0 == FREE_MM(&data2)) ;

   return 0 ;
ONABORT:
   FREE_MM(&data) ;
   FREE_MM(&data2) ;
   return EINVAL ;
}

static int test_sizebase64(void)
{
   memblock_t  data    = memblock_INIT_FREEABLE ;
   memblock_t  data2   = memblock_INIT_FREEABLE ;
   wbuffer_t   result  = wbuffer_INIT_MEMBLOCK(&data) ;
   wbuffer_t   result2 = wbuffer_INIT_MEMBLOCK(&data2) ;
   uint8_t     buffer[256] ;

   // TEST input length 1..3 sizebase64encode_string
   TEST(4 == sizebase64encode_string(&(string_t)string_INIT(1, (const uint8_t*)"xxx"))) ;
   TEST(4 == sizebase64encode_string(&(string_t)string_INIT(2, (const uint8_t*)"xxx"))) ;
   TEST(4 == sizebase64encode_string(&(string_t)string_INIT(3, (const uint8_t*)"xxx"))) ;

   // TEST output length 1..3 sizebase64decode_string
   TEST(1 == sizebase64decode_string(&(string_t)string_INIT_CSTR("WA=="))) ;
   TEST(2 == sizebase64decode_string(&(string_t)string_INIT_CSTR("WAA="))) ;
   TEST(3 == sizebase64decode_string(&(string_t)string_INIT_CSTR("WAAA"))) ;

   // TEST sizes 1 .. 256 of sizebase64encode_string / sizebase64encode_string
   for (unsigned i = 0; i < 256; ++i) {
      buffer[i] = (uint8_t)i ;
      TEST(4*((i+3)/3) == sizebase64encode_string(&(string_t)string_INIT(i+1, buffer))) ;
      clear_wbuffer(&result);
      TEST(0 == base64encode_string(&(string_t)string_INIT(i+1, buffer), &result)) ;
      TEST(4*((i+3)/3) == size_wbuffer(&result)) ;
      TEST(i+1 == sizebase64decode_string(&(string_t)string_INIT(size_wbuffer(&result), addr_memblock(&data)))) ;
      clear_wbuffer(&result2);
      TEST(0 == base64decode_string(&(string_t)string_INIT(size_wbuffer(&result), addr_memblock(&data)), &result2)) ;
      TEST(i+1 == size_wbuffer(&result2)) ;
      TEST(0 == memcmp(buffer, addr_memblock(&data2), i+1)) ;
   }

   // unprepare
   TEST(0 == FREE_MM(&data)) ;
   TEST(0 == FREE_MM(&data2)) ;

   return 0 ;
ONABORT:
   FREE_MM(&data) ;
   FREE_MM(&data2) ;
   return EINVAL ;
}


int unittest_string_base64encode()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_base64())      goto ONABORT ;
   if (test_sizebase64())  goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
