/* title: URLEncodeString impl
   Implements <URLEncodeString>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/string/urlencode_string.h
    Header file <URLEncodeString>.

   file: C-kern/string/urlencode_string.c
    Implementation file <URLEncodeString impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/string/urlencode_string.h"
#include "C-kern/api/string/string.h"
#include "C-kern/api/memory/wbuffer.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#endif


// section: string_t

// group: urlencoding

/* variable: s_isurlencode
 * Table of 256 octets (first are 128 ascii codes).
 * A value of 2 indicates that the corresponding character
 * has to be encoded. All other values are 0. */
static const uint8_t s_isurlencode[256] = {
    [0] = 2,  [1] = 2,  [2] = 2,  [3] = 2,  [4] = 2,  [5] = 2,  [6] = 2,  [7] = 2,  [8] = 2,  [9] = 2,
   [10] = 2, [11] = 2, [12] = 2, [13] = 2, [14] = 2, [15] = 2, [16] = 2, [17] = 2, [18] = 2, [19] = 2,
   [20] = 2, [21] = 2, [22] = 2, [23] = 2, [24] = 2, [25] = 2, [26] = 2, [27] = 2, [28] = 2, [29] = 2,
   [30] = 2, [31] = 2,
   [' '] = 2, ['!'] = 2, ['"'] = 2, ['#'] = 2, ['$'] = 2, ['%'] = 2, ['&'] = 2, ['\'']= 2, ['('] = 2,
   [')'] = 2, ['*'] = 0, ['+'] = 2, [','] = 2, ['-'] = 0, ['.'] = 0, ['/'] = 2, [':'] = 2, [';'] = 2,
   ['<'] = 2, ['='] = 2, ['>'] = 2, ['?'] = 2, ['@'] = 2, ['['] = 2, ['\\']= 2, [']'] = 2, ['^'] = 2,
   ['_'] = 0, ['`'] = 2, ['{'] = 2, ['|'] = 2, ['}'] = 2, ['~'] = 2,
   [127] = 2, [128] = 2, [129] = 2, [130] = 2, [131] = 2, [132] = 2, [133] = 2, [134] = 2, [135] = 2,
   [136] = 2, [137] = 2, [138] = 2, [139] = 2, [140] = 2, [141] = 2, [142] = 2, [143] = 2, [144] = 2, [145] = 2,
   [146] = 2, [147] = 2, [148] = 2, [149] = 2, [150] = 2, [151] = 2, [152] = 2, [153] = 2, [154] = 2, [155] = 2,
   [156] = 2, [157] = 2, [158] = 2, [159] = 2, [160] = 2, [161] = 2, [162] = 2, [163] = 2, [164] = 2, [165] = 2,
   [166] = 2, [167] = 2, [168] = 2, [169] = 2, [170] = 2, [171] = 2, [172] = 2, [173] = 2, [174] = 2, [175] = 2,
   [176] = 2, [177] = 2, [178] = 2, [179] = 2, [180] = 2, [181] = 2, [182] = 2, [183] = 2, [184] = 2, [185] = 2,
   [186] = 2, [187] = 2, [188] = 2, [189] = 2, [190] = 2, [191] = 2, [192] = 2, [193] = 2, [194] = 2, [195] = 2,
   [196] = 2, [197] = 2, [198] = 2, [199] = 2, [200] = 2, [201] = 2, [202] = 2, [203] = 2, [204] = 2, [205] = 2,
   [206] = 2, [207] = 2, [208] = 2, [209] = 2, [210] = 2, [211] = 2, [212] = 2, [213] = 2, [214] = 2, [215] = 2,
   [216] = 2, [217] = 2, [218] = 2, [219] = 2, [220] = 2, [221] = 2, [222] = 2, [223] = 2, [224] = 2, [225] = 2,
   [226] = 2, [227] = 2, [228] = 2, [229] = 2, [230] = 2, [231] = 2, [232] = 2, [233] = 2, [234] = 2, [235] = 2,
   [236] = 2, [237] = 2, [238] = 2, [239] = 2, [240] = 2, [241] = 2, [242] = 2, [243] = 2, [244] = 2, [245] = 2,
   [246] = 2, [247] = 2, [248] = 2, [249] = 2, [250] = 2, [251] = 2, [252] = 2, [253] = 2, [254] = 2, [255] = 2
} ;

size_t sizeurlencode_string(const string_t * str, uint8_t except_char)
{
   const uint8_t  * next       = str->addr ;
   size_t         encoded_size = str->size ;

   if (except_char) {
      for(size_t size = str->size; size; ++next, --size) {
         const uint8_t c = *next ;
         encoded_size += (size_t) (s_isurlencode[c] & ((except_char != c)<<1)) ;
      }
   } else {
      for(size_t size = str->size; size; ++next, --size) {
         const uint8_t c = *next ;
         encoded_size += s_isurlencode[c] ;
      }
   }
   return encoded_size ;
}

size_t sizeurldecode_string(const string_t * str)
{
   size_t decoded_size = str->size ;
   size_t count        = decoded_size ;

   for(const uint8_t * next = str->addr; count; ++ next, --count) {
      if (  ('%' == next[0])
         && (count > 2)
         && (     ('0' <= next[1] && next[1] <= '9')
               || ('A' <= next[1] && next[1] <= 'F')
               || ('a' <= next[1] && next[1] <= 'f'))
         && (     ('0' <= next[2] && next[2] <= '9')
               || ('A' <= next[2] && next[2] <= 'F')
               || ('a' <= next[2] && next[2] <= 'f'))) {
         decoded_size -= 2 ;
         count -= 2 ;
         next  += 2 ;
      }
   }

   return decoded_size ;
}

int urlencode_string(const string_t * str, uint8_t except_char, uint8_t changeto_char, /*ret*/wbuffer_t * result)
{
   int err ;
   uint8_t       *encodedchar ;
   const uint8_t *next    = str->addr ;
   size_t         oldsize = size_wbuffer(result);

   for(size_t count = str->size; count; ++next, --count) {
      const uint8_t c = *next ;

      if (s_isurlencode[c]) {
         if (  except_char
            && c == except_char ) {
            err = appendbyte_wbuffer(result, changeto_char) ;
            if (err) goto ONERR;
         } else {
            err = appendbytes_wbuffer(result, 3, &encodedchar) ;
            if (err) goto ONERR;

            encodedchar[0] = '%' ;
            int nibble = (c / 16) + '0' ;
            if (nibble > '9') nibble += ('A' - '0' - 10) ;
            encodedchar[1] =  (uint8_t) nibble ;
            nibble = (c % 16) + '0' ;
            if (nibble > '9') nibble += ('A' - '0' - 10) ;
            encodedchar[2] = (uint8_t) nibble ;
         }
      } else {
         err = appendbyte_wbuffer(result, c) ;
         if (err) goto ONERR;
      }

   }

   return 0 ;
ONERR:
   shrink_wbuffer(result, oldsize) ;
   TRACEEXIT_ERRLOG(err);
   return err ;
}

int urldecode_string(const string_t * str, uint8_t changefrom_char, uint8_t changeinto_char, /*ret*/wbuffer_t * result)
{
   int err ;
   const uint8_t *next    = str->addr ;
   size_t         count   = str->size ;
   size_t         oldsize = size_wbuffer(result);

   for( ; count; ++ next, --count) {
      if (  ('%' == next[0])
         && (count > 2)
         && (     ('0' <= next[1] && next[1] <= '9')
               || ('A' <= next[1] && next[1] <= 'F')
               || ('a' <= next[1] && next[1] <= 'f'))
         && (     ('0' <= next[2] && next[2] <= '9')
               || ('A' <= next[2] && next[2] <= 'F')
               || ('a' <= next[2] && next[2] <= 'f'))) {
         static_assert( '0' < 'A' && 'A' < 'a', ) ;
         int nibb1 = ((++next)[0] - '0') ;
         int nibb2 = ((++next)[0] - '0') ;
         count -= 2 ;
         if (nibb1 > 9)  nibb1 -= ('A' - '0' - 10) ;
         if (nibb2 > 9)  nibb2 -= ('A' - '0' - 10) ;
         if (nibb1 > 15) nibb1 -= ('a' - 'A') ;
         if (nibb2 > 15) nibb2 -= ('a' - 'A') ;
         err = appendbyte_wbuffer(result, (uint8_t) (nibb1 * 16 + nibb2)) ;
         if (err) goto ONERR;
      } else {
         if (changefrom_char == next[0]) {
            err = appendbyte_wbuffer(result, changeinto_char) ;
         } else {
            err = appendbyte_wbuffer(result, next[0]) ;
         }
         if (err) goto ONERR;
      }
   }

   return 0 ;
ONERR:
   shrink_wbuffer(result, oldsize);
   TRACEEXIT_ERRLOG(err);
   return err;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_urlencode(void)
{
   const uint8_t* test ;
   memblock_t     data   = memblock_FREE ;
   wbuffer_t      result = wbuffer_INIT_MEMBLOCK(&data) ;

   // TEST urlencode_string: alphabetical chars are not encoded
   test = (const uint8_t*)"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ" ;
   TEST(52 == sizeurlencode_string(&(string_t)string_INIT(strlen((const char*)test),test), 0)) ;
   TEST(0 == urlencode_string(&(string_t)string_INIT(strlen((const char*)test),test), 0, 0, &result)) ;
   TEST(52 == size_wbuffer(&result)) ;
   TEST(0 == strncmp((const char*)test, (char*)addr_memblock(&data), 52)) ;
   clear_wbuffer(&result);

   // TEST urlencode_string: numerical chars are not encoded
   test = (const uint8_t*)"0123456789" ;
   TEST(10 == sizeurlencode_string(&(string_t)string_INIT(strlen((const char*)test),test), 0)) ;
   TEST(0 == urlencode_string(&(string_t)string_INIT(strlen((const char*)test),test), 0, 0, &result)) ;
   TEST(10 == size_wbuffer(&result)) ;
   TEST(0 == strncmp((const char*)test, (char*)addr_memblock(&data), 10)) ;
   clear_wbuffer(&result);

   // TEST urlencode_string: special chars are not encoded
   test = (const uint8_t*)"-_.*" ;
   TEST(4 == sizeurlencode_string(&(string_t)string_INIT(strlen((const char*)test),test), 0)) ;
   TEST(0 == urlencode_string(&(string_t)string_INIT(strlen((const char*)test),test), 0, 0, &result)) ;
   TEST(4 == size_wbuffer(&result)) ;
   TEST(0 == strncmp((const char*)test, (char*)addr_memblock(&data), strlen((const char*)test))) ;
   clear_wbuffer(&result);

   // TEST urlencode_string: except_char, changeto_char
   test = (const uint8_t*)"&=$-_.+!*'(),/@" ;
   TEST(35 == sizeurlencode_string(&(string_t)string_INIT(strlen((const char*)test),test), '/')) ;
   TEST(0 == urlencode_string(&(string_t)string_INIT(strlen((const char*)test),test), '/', '/', &result)) ;
   TEST(35 == size_wbuffer(&result)) ;
   test = (const uint8_t*)"%26%3D%24-_.%2B%21*%27%28%29%2C/%40" ;
   TEST(0 == strncmp((const char*)test, (char*)addr_memblock(&data), strlen((const char*)test))) ;
   test = (const uint8_t*)"&=$-_.+!*'(),/@" ;
   TEST(35 == sizeurlencode_string(&(string_t)string_INIT(strlen((const char*)test),test), '@')) ;
   clear_wbuffer(&result);
   TEST(0 == urlencode_string(&(string_t)string_INIT(strlen((const char*)test),test), '@', '+', &result)) ;
   TEST(35 == size_wbuffer(&result)) ;
   test = (const uint8_t*)"%26%3D%24-_.%2B%21*%27%28%29%2C%2F+" ;
   TEST(0 == strncmp((const char*)test, (char*)addr_memblock(&data), strlen((const char*)test))) ;
   clear_wbuffer(&result);

   // TEST urlencode_string: all other characters
   test =   (const uint8_t*)"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "0123456789"
            "-_.*" ;
   for(size_t i = 0; i < 256; ++i) {
      uint8_t c[4] = { (uint8_t)i, 0 } ;
      if (c[0] && strchr((const char*)test, c[0])) {
         TEST(1 == sizeurlencode_string(&(string_t)string_INIT(1,c), 0)) ;
         TEST(0 == urlencode_string(&(string_t)string_INIT(1,c), 0, 0, &result)) ;
         TEST(1 == size_wbuffer(&result)) ;
         TEST(0 == strncmp((const char*)c, (char*)addr_memblock(&data), 1)) ;
      } else {
         TEST(3 == sizeurlencode_string(&(string_t)string_INIT(1,c), 0)) ;
         TEST(0 == urlencode_string(&(string_t)string_INIT(1,c), 0, 0, &result)) ;
         TEST(3 == size_wbuffer(&result)) ;
         sprintf((char*)c, "%%%c%c", (i/16) > 9 ? (i/16)+'A'-10 : (i/16)+'0', (i%16) > 9 ? (i%16)+'A'-10 : (i%16)+'0') ;
         TEST(0 == strncmp((const char*)c, (char*)addr_memblock(&data), 3)) ;
         TEST(1 == sizeurlencode_string(&(string_t)string_INIT(1,c), c[0])) ;
         clear_wbuffer(&result);
         TEST(0 == urlencode_string(&(string_t)string_INIT(1,c), c[0], (uint8_t)(i+5), &result)) ;
         TEST(1 == size_wbuffer(&result)) ;
         c[0] = (uint8_t)(i+5) ;
         TEST(0 == strncmp((const char*)c, (char*)addr_memblock(&data), 1)) ;
      }
      clear_wbuffer(&result);
   }

   // unprepare
   TEST(0 == FREE_MM(&data)) ;

   return 0 ;
ONERR:
   FREE_MM(&data) ;
   return EINVAL ;
}

static int test_urldecode(void)
{
   memblock_t     data   = memblock_FREE ;
   wbuffer_t      result = wbuffer_INIT_MEMBLOCK(&data) ;
   const uint8_t *test ;

   // TEST urldecode_string: empty decode
   TEST(0 == appendbyte_wbuffer(&result, 1)) ;
   TEST(1 == size_wbuffer(&result)) ;
   TEST(0 == sizeurldecode_string(&(string_t)string_INIT(0, (const uint8_t*)""))) ;
   TEST(0 == urldecode_string(&(string_t)string_INIT(0, (const uint8_t*)""), 0, 0, &result)) ;
   TEST(1 == size_wbuffer(&result));
   clear_wbuffer(&result);

   // TEST urldecode_string: every possible value
   for(unsigned i = 0; i < 256; ++i) {
      char c[18] = "123%32x%AAy%11%22" ;
      sprintf(c+12, "%c%c", (i/16) > 9 ? (i/16)+'A'-10 : (i/16)+'0', (i%16) > 9 ? (i%16)+'A'-10 : (i%16)+'0') ;
      sprintf(c+14, "%%%c%c", (i/16) > 9 ? (i/16)+'a'-10 : (i/16)+'0', (i%16) > 9 ? (i%16)+'a'-10 : (i%16)+'0') ;
      string_t str = string_INIT(17, (const uint8_t*)c) ;
      TEST(9 == sizeurldecode_string(&str)) ;
      TEST(0 == urldecode_string(&str, 'y', (uint8_t)(1+i), &result)) ;
      TEST(9 == size_wbuffer(&result)) ;
      TEST(0 == strncmp("123\x32x\xAA", (char*)addr_memblock(&data), 6)) ;
      TEST((uint8_t)(1+i) == addr_memblock(&data)[6]) ;
      TEST((uint8_t)i == addr_memblock(&data)[7]) ;
      TEST((uint8_t)i == addr_memblock(&data)[8]) ;
      clear_wbuffer(&result);
   }

   // TEST urldecode_string: invalid encoded are not decoded
   test = (const uint8_t*)"%9x%ZF%g2%/A%:9" ;
   TEST(strlen((const char*)test) == sizeurldecode_string(&(string_t)string_INIT(strlen((const char*)test),test))) ;
   TEST(0 == urldecode_string(&(string_t)string_INIT(strlen((const char*)test),test), 0, 0, &result)) ;
   TEST(strlen((const char*)test) == size_wbuffer(&result)) ;
   TEST(0 == strncmp((const char*)test, (char*)addr_memblock(&data), size_wbuffer(&result))) ;
   clear_wbuffer(&result);

   // TEST urldecode_string: end of string is considered
   test = (const uint8_t*)"123%99" ;
   for(unsigned i = 0; i < strlen((const char*)test); ++i) {
      TEST(i == sizeurldecode_string(&(string_t)string_INIT(i,test))) ;
      TEST(0 == urldecode_string(&(string_t)string_INIT(i,test), 0, 0, &result)) ;
      TEST(i == size_wbuffer(&result)) ;
      TEST(0 == strncmp((const char*)test, (char*)addr_memblock(&data), i)) ;
      clear_wbuffer(&result);
   }

   // TEST urldecode_string: octets are not changed except if changefrom_char is set
   {
      uint8_t buffer[256] ;
      for(size_t i = 0; i < 256; ++i) {
         buffer[i] = (uint8_t) i ;
         TEST(1+i == sizeurldecode_string(&(string_t)string_INIT(1+i,buffer))) ;
         TEST(0 == urldecode_string(&(string_t)string_INIT(1+i,buffer), 0, 0, &result)) ;
         TEST(1+i == size_wbuffer(&result)) ;
         TEST(0 == memcmp(buffer, addr_memblock(&data), 1+i)) ;
         clear_wbuffer(&result);
         TEST(0 == urldecode_string(&(string_t)string_INIT(1+i,buffer), (uint8_t)i, '@', &result)) ;
         TEST(1+i == size_wbuffer(&result)) ;
         TEST(0 == memcmp(buffer, addr_memblock(&data), i)) ;
         TEST('@' == addr_memblock(&data)[i]) ;
         clear_wbuffer(&result);
      }
   }

   // unprepare
   TEST(0 == FREE_MM(&data)) ;

   return 0 ;
ONERR:
   FREE_MM(&data) ;
   return EINVAL ;
}

int unittest_string_urlencode()
{
   if (test_urlencode())   goto ONERR;
   if (test_urldecode())   goto ONERR;

   return 0 ;
ONERR:
   return EINVAL ;
}

#endif
