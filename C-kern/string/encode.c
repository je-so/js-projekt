/* title: StringEncode impl
   Imeplements <StringEncode>.

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

   file: C-kern/api/string/encode.h
    Header file <StringEncode>.

   file: C-kern/string/encode.c
    Implementation file <StringEncode impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/string/encode.h"
#include "C-kern/api/string/string.h"
#include "C-kern/api/memory/wbuffer.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
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

size_t sizeurlencode_string(const string_t * str, char except_char)
{
   const char  * next       = str->addr ;
   size_t      encoded_size = str->size ;

   if (except_char) {
      for(size_t size = str->size; size; ++next, --size) {
         const uint8_t c = (uint8_t) *next ;
         encoded_size += (size_t) (s_isurlencode[c] & ((except_char != c)<<1)) ;
      }
   } else {
      for(size_t size = str->size; size; ++next, --size) {
         const uint8_t c = (uint8_t) *next ;
         encoded_size += s_isurlencode[c] ;
      }
   }
   return encoded_size ;
}

size_t sizeurldecode_string(const string_t * str)
{
   size_t decoded_size = str->size ;
   size_t count        = decoded_size ;

   for(const char * next = str->addr; count; ++ next, --count) {
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

int urlencode_string(const string_t * str, char except_char, char changeto_char, wbuffer_t * result)
{
   int err ;
   uint8_t     * encodedchar ;
   const char  * next       = str->addr ;

   for(size_t count = str->size; count; ++next, --count) {
      const char c = *next ;

      if (s_isurlencode[(uint8_t)c]) {
         if (  except_char
            && c == except_char ) {
            err = appendchar_wbuffer(result, changeto_char) ;
            if (err) goto ABBRUCH ;
         } else {
            err = appendalloc_wbuffer(result, 3, &encodedchar) ;
            if (err) goto ABBRUCH ;

            encodedchar[0] = '%' ;
            int nibble = ((uint8_t)c / 16) + '0' ;
            if (nibble > '9') nibble += ('A' - '0' - 10) ;
            encodedchar[1] =  (uint8_t) nibble ;
            nibble = ((uint8_t)c % 16) + '0' ;
            if (nibble > '9') nibble += ('A' - '0' - 10) ;
            encodedchar[2] = (uint8_t) nibble ;
         }
      } else {
         err = appendchar_wbuffer(result, c) ;
         if (err) goto ABBRUCH ;
      }

   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int urldecode_string(const string_t * str, char changefrom_char, char changeinto_char, wbuffer_t * result)
{
   int err ;
   const char  * next = str->addr ;
   size_t      count  = str->size ;

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
         err = appendchar_wbuffer(result, (char) (nibb1 * 16 + nibb2)) ;
         if (err) goto ABBRUCH ;
      } else {
         if (changefrom_char == next[0]) {
            err = appendchar_wbuffer(result, changeinto_char) ;
         } else {
            err = appendchar_wbuffer(result, next[0]) ;
         }
         if (err) goto ABBRUCH ;
      }
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

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

int base64encode_string(const string_t * str, wbuffer_t * result)
{
   int err ;
   const uint8_t  * next = (const uint8_t*) str->addr ;
   size_t         count  = str->size / 3 ;
   size_t         mod3   = str->size % 3 ;
   uint32_t       tripple ;
   uint8_t        * dest ;

   size_t quadruples  = count + (mod3 != 0) ;
   size_t result_size = 4 * quadruples ;

   if (result_size <= quadruples) {
      err = EOVERFLOW ;
      goto ABBRUCH ;
   }

   err = appendalloc_wbuffer(result, result_size, &dest) ;
   if (err) goto ABBRUCH ;

   for( ; count; -- count) {
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
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int base64decode_string(const string_t * str, wbuffer_t * result)
{
   int err ;
   const uint8_t  * next = (const uint8_t*) str->addr ;
   size_t         count  = str->size / 4 ;
   unsigned       mod3 ;
   int32_t        quadr ;
   uint8_t        * dest ;

   PRECONDITION_INPUT(0 == (str->size % 4), ABBRUCH, ) ;

   if (!count) {
      return 0 ;
   }

   mod3 = (unsigned) ('=' == str->addr[str->size-1]) + ('=' == str->addr[str->size-2]) ;

   size_t result_size = 3 * count - mod3 ;

   err = appendalloc_wbuffer(result, result_size, &dest) ;
   if (err) goto ABBRUCH ;

   for(count = count - (mod3 != 0); count; -- count) {
      quadr = s_base64values[*(next ++)] ;
      quadr <<= 6 ;
      quadr |= s_base64values[*(next ++)] ;
      quadr <<= 6 ;
      quadr |= s_base64values[*(next ++)] ;
      quadr <<= 6 ;
      quadr |= s_base64values[*(next ++)] ;
      if (quadr < 0) {
         err = EINVAL ;
         goto ABBRUCH ;
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
               goto ABBRUCH ;
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
               goto ABBRUCH ;
            }
            quadr >>= 4 ;
            dest[0] = (uint8_t) quadr ;
            break ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}



// group: test

#ifdef KONFIG_UNITTEST

#define TEST(ARG) TEST_ONERROR_GOTO(ARG, ABBRUCH)

static int test_urlencode(void)
{
   const char  * test ;
   wbuffer_t   result = wbuffer_INIT ;

   // TEST alphabetical chars are not encoded
   test = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ" ;
   TEST(52 == sizeurlencode_string(&(string_t)string_INIT(strlen(test),test), 0)) ;
   TEST(0 == urlencode_string(&(string_t)string_INIT(strlen(test),test), 0, 0, &result)) ;
   TEST(52 == sizecontent_wbuffer(&result)) ;
   TEST(0 == strncmp(test, (char*)content_wbuffer(&result), 52)) ;
   reset_wbuffer(&result) ;

   // TEST numerical chars are not encoded
   test = "0123456789" ;
   TEST(10 == sizeurlencode_string(&(string_t)string_INIT(strlen(test),test), 0)) ;
   TEST(0 == urlencode_string(&(string_t)string_INIT(strlen(test),test), 0, 0, &result)) ;
   TEST(10 == sizecontent_wbuffer(&result)) ;
   TEST(0 == strncmp(test, (char*)content_wbuffer(&result), 10)) ;
   reset_wbuffer(&result) ;

   // TEST special chars are not encoded
   test = "-_.*" ;
   TEST(4 == sizeurlencode_string(&(string_t)string_INIT(strlen(test),test), 0)) ;
   TEST(0 == urlencode_string(&(string_t)string_INIT(strlen(test),test), 0, 0, &result)) ;
   TEST(4 == sizecontent_wbuffer(&result)) ;
   TEST(0 == strncmp(test, (char*)content_wbuffer(&result), strlen(test))) ;
   reset_wbuffer(&result) ;

   // TEST except_char, changeto_char
   test = "&=$-_.+!*'(),/@" ;
   TEST(35 == sizeurlencode_string(&(string_t)string_INIT(strlen(test),test), '/')) ;
   TEST(0 == urlencode_string(&(string_t)string_INIT(strlen(test),test), '/', '/', &result)) ;
   TEST(35 == sizecontent_wbuffer(&result)) ;
   test = "%26%3D%24-_.%2B%21*%27%28%29%2C/%40" ;
   TEST(0 == strncmp(test, (char*)content_wbuffer(&result), strlen(test))) ;
   reset_wbuffer(&result) ;
   test = "&=$-_.+!*'(),/@" ;
   TEST(35 == sizeurlencode_string(&(string_t)string_INIT(strlen(test),test), '@')) ;
   TEST(0 == urlencode_string(&(string_t)string_INIT(strlen(test),test), '@', '+', &result)) ;
   TEST(35 == sizecontent_wbuffer(&result)) ;
   test = "%26%3D%24-_.%2B%21*%27%28%29%2C%2F+" ;
   TEST(0 == strncmp(test, (char*)content_wbuffer(&result), strlen(test))) ;
   reset_wbuffer(&result) ;

   // TEST all other characters
   test =   "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "0123456789"
            "-_.*" ;
   for(size_t i = 0; i < 256; ++i) {
      char c[4] = { (char)i, 0 } ;
      if (c[0] && strchr(test, c[0])) {
         TEST(1 == sizeurlencode_string(&(string_t)string_INIT(1,c), 0)) ;
         TEST(0 == urlencode_string(&(string_t)string_INIT(1,c), 0, 0, &result)) ;
         TEST(1 == sizecontent_wbuffer(&result)) ;
         TEST(0 == strncmp(c, (char*)content_wbuffer(&result), 1)) ;
         reset_wbuffer(&result) ;
      } else {
         TEST(3 == sizeurlencode_string(&(string_t)string_INIT(1,c), 0)) ;
         TEST(0 == urlencode_string(&(string_t)string_INIT(1,c), 0, 0, &result)) ;
         TEST(3 == sizecontent_wbuffer(&result)) ;
         sprintf(c, "%%%c%c", (i/16) > 9 ? (i/16)+'A'-10 : (i/16)+'0', (i%16) > 9 ? (i%16)+'A'-10 : (i%16)+'0') ;
         TEST(0 == strncmp(c, (char*)content_wbuffer(&result), 3)) ;
         reset_wbuffer(&result) ;
         TEST(1 == sizeurlencode_string(&(string_t)string_INIT(1,c), c[0])) ;
         TEST(0 == urlencode_string(&(string_t)string_INIT(1,c), c[0], (char)(i+5), &result)) ;
         TEST(1 == sizecontent_wbuffer(&result)) ;
         c[0] = (char)(i+5) ;
         TEST(0 == strncmp(c, (char*)content_wbuffer(&result), 1)) ;
         reset_wbuffer(&result) ;
      }
   }

   TEST(0 == free_wbuffer(&result)) ;
   return 0 ;
ABBRUCH:
   free_wbuffer(&result) ;
   return EINVAL ;
}

static int test_urldecode(void)
{
   const char  * test ;
   wbuffer_t   result = wbuffer_INIT ;

   // TEST empty decode
   TEST(0 == content_wbuffer(&result)) ;
   TEST(0 == sizecontent_wbuffer(&result)) ;
   TEST(0 == sizeurldecode_string(&(string_t)string_INIT(0,""))) ;
   TEST(0 == urldecode_string(&(string_t)string_INIT(0,""), 0, 0, &result)) ;
   TEST(0 == sizecontent_wbuffer(&result)) ;
   TEST(0 == content_wbuffer(&result)) ;

   // TEST every possible value
   for(unsigned i = 0; i < 256; ++i) {
      char c[18] = "123%32x%AAy%11%22" ;
      sprintf(c+12, "%c%c", (i/16) > 9 ? (i/16)+'A'-10 : (i/16)+'0', (i%16) > 9 ? (i%16)+'A'-10 : (i%16)+'0') ;
      sprintf(c+14, "%%%c%c", (i/16) > 9 ? (i/16)+'a'-10 : (i/16)+'0', (i%16) > 9 ? (i%16)+'a'-10 : (i%16)+'0') ;
      string_t str = string_INIT(17, c) ;
      TEST(9 == sizeurldecode_string(&str)) ;
      TEST(0 == urldecode_string(&str, 'y', (char)(1+i), &result)) ;
      TEST(9 == sizecontent_wbuffer(&result)) ;
      TEST(0 == strncmp("123\x32x\xAA", (char*)content_wbuffer(&result), 6)) ;
      TEST((uint8_t)(1+i) == content_wbuffer(&result)[6]) ;
      TEST((uint8_t)i == content_wbuffer(&result)[7]) ;
      TEST((uint8_t)i == content_wbuffer(&result)[8]) ;
      reset_wbuffer(&result) ;
   }

   // TEST invalid encoded are not decoded
   test = "%9x%ZF%g2%/A%:9" ;
   TEST(strlen(test) == sizeurldecode_string(&(string_t)string_INIT(strlen(test),test))) ;
   TEST(0 == urldecode_string(&(string_t)string_INIT(strlen(test),test), 0, 0, &result)) ;
   TEST(strlen(test) == sizecontent_wbuffer(&result)) ;
   TEST(0 == strncmp(test, (char*)content_wbuffer(&result), strlen(test))) ;
   reset_wbuffer(&result) ;

   // TEST end of string is considered
   test = "123%99" ;
   for(unsigned i = 0; i < strlen(test); ++i) {
      TEST(i == sizeurldecode_string(&(string_t)string_INIT(i,test))) ;
      TEST(0 == urldecode_string(&(string_t)string_INIT(i,test), 0, 0, &result)) ;
      TEST(i == sizecontent_wbuffer(&result)) ;
      TEST(0 == strncmp(test, (char*)content_wbuffer(&result), i)) ;
      reset_wbuffer(&result) ;
   }

   // TEST octets are not changed except if changefrom_char is set
   {
      char buffer[256] ;
      for(size_t i = 0; i < 256; ++i) {
         buffer[i] = (char) i ;
         TEST(1+i == sizeurldecode_string(&(string_t)string_INIT(1+i,buffer))) ;
         TEST(0 == urldecode_string(&(string_t)string_INIT(1+i,buffer), 0, 0, &result)) ;
         TEST(1+i == sizecontent_wbuffer(&result)) ;
         TEST(0 == memcmp(buffer, content_wbuffer(&result), 1+i)) ;
         reset_wbuffer(&result) ;
         TEST(0 == urldecode_string(&(string_t)string_INIT(1+i,buffer), (char)i, '@', &result)) ;
         TEST(1+i == sizecontent_wbuffer(&result)) ;
         TEST(0 == memcmp(buffer, content_wbuffer(&result), i)) ;
         TEST('@' == content_wbuffer(&result)[i]) ;
         reset_wbuffer(&result) ;
      }
   }

   TEST(0 == free_wbuffer(&result)) ;
   return 0 ;
ABBRUCH:
   free_wbuffer(&result) ;
   return EINVAL ;
}

static int test_base64(void)
{
   const char  * test ;
   const char  * test2 ;
   wbuffer_t   result  = wbuffer_INIT ;
   wbuffer_t   result2 = wbuffer_INIT ;

   // TEST encode / decode
   test = "Test this string : 123" ;
   test2 = "VGVzdCB0aGlzIHN0cmluZyA6IDEyMw==" ;
   TEST(0 == base64encode_string(&(string_t)string_INIT(strlen(test), test), &result)) ;
   TEST(strlen(test2) == sizecontent_wbuffer(&result)) ;
   TEST(0 == strncmp(test2, (char*)content_wbuffer(&result), strlen(test2))) ;
   TEST(0 == base64decode_string(&(string_t)string_INIT(sizecontent_wbuffer(&result), (char*)content_wbuffer(&result)), &result2)) ;
   TEST(strlen(test) == sizecontent_wbuffer(&result2)) ;
   TEST(0 == strncmp(test, (char*)content_wbuffer(&result2), strlen(test))) ;
   reset_wbuffer(&result) ;
   reset_wbuffer(&result2) ;

   // TEST all valid characters in decoding
   test  =  "\x00\x10\x83\x10\x51\x87\x20\x92\x8b\x30\xd3\x8f\x41\x14\x93\x51"
            "\x55\x97\x61\x96\x9b\x71\xd7\x9f\x82\x18\xa3\x92\x59\xa7\xa2\x9a"
            "\xab\xb2\xdb\xaf\xc3\x1c\xb3\xd3\x5d\xb7\xe3\x9e\xbb\xf3\xdf\xbf" ;
   test2 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/" ;
   TEST(0 == base64encode_string(&(string_t)string_INIT(48, test), &result)) ;
   TEST(64 == sizecontent_wbuffer(&result)) ;
   TEST(0 == strncmp(test2, (char*)content_wbuffer(&result), 64)) ;
   TEST(0 == base64decode_string(&(string_t)string_INIT(sizecontent_wbuffer(&result), (char*)content_wbuffer(&result)), &result2)) ;
   TEST(48 == sizecontent_wbuffer(&result2)) ;
   TEST(0 == strncmp(test, (char*)content_wbuffer(&result2), 48)) ;
   reset_wbuffer(&result) ;
   reset_wbuffer(&result2) ;

   // TEST all octets in encoding
   {
      test =   "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4"
               "OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5fYGFiY2RlZmdoaWprbG1ub3Bx"
               "cnN0dXZ3eHl6e3x9fn+AgYKDhIWGh4iJiouMjY6PkJGSk5SVlpeYmZqbnJ2en6ChoqOkpaanqKmq"
               "q6ytrq+wsbKztLW2t7i5uru8vb6/wMHCw8TFxsfIycrLzM3Oz9DR0tPU1dbX2Nna29zd3t/g4eLj"
               "5OXm5+jp6uvs7e7v8PHy8/T19vf4+fr7/P3+/w==" ;
      char buffer[256] ;
      for(unsigned i = 0; i < 256; ++i) {
         buffer[i] = (char) i ;
      }
      TEST(0 == base64encode_string(&(string_t)string_INIT(256, buffer), &result)) ;
      TEST((4*((256+2) / 3)) == sizecontent_wbuffer(&result)) ;
      TEST((4*((256+2) / 3)) == strlen(test)) ;
      TEST(0 == strncmp(test, (char*)content_wbuffer(&result), strlen(test))) ;
      TEST(0 == base64decode_string(&(string_t)string_INIT(sizecontent_wbuffer(&result), (char*)content_wbuffer(&result)), &result2)) ;
      TEST(256 == sizecontent_wbuffer(&result2)) ;
      TEST(0 == memcmp(buffer, content_wbuffer(&result2), 256)) ;
      reset_wbuffer(&result) ;
      reset_wbuffer(&result2) ;
   }

   // TEST not multiple of 3
   test  = "X" ;
   test2 = "WA==" ;
   TEST(0 == base64encode_string(&(string_t)string_INIT(strlen(test), test), &result)) ;
   TEST(4 == sizecontent_wbuffer(&result)) ;
   TEST(0 == strncmp(test2, (char*)content_wbuffer(&result), 4)) ;
   TEST(0 == base64decode_string(&(string_t)string_INIT(sizecontent_wbuffer(&result), (char*)content_wbuffer(&result)), &result2)) ;
   TEST(1 == sizecontent_wbuffer(&result2)) ;
   TEST(0 == strncmp(test, (char*)content_wbuffer(&result2), strlen(test))) ;
   reset_wbuffer(&result) ;
   reset_wbuffer(&result2) ;
   test  = "XY" ;
   test2 = "WFk=" ;
   TEST(0 == base64encode_string(&(string_t)string_INIT(strlen(test), test), &result)) ;
   TEST(4 == sizecontent_wbuffer(&result)) ;
   TEST(0 == strncmp(test2, (char*)content_wbuffer(&result), 4)) ;
   TEST(0 == base64decode_string(&(string_t)string_INIT(sizecontent_wbuffer(&result), (char*)content_wbuffer(&result)), &result2)) ;
   TEST(2 == sizecontent_wbuffer(&result2)) ;
   TEST(0 == strncmp(test, (char*)content_wbuffer(&result2), strlen(test))) ;
   reset_wbuffer(&result) ;
   reset_wbuffer(&result2) ;

   // TEST EINVAL decode not multiple of 4
   for(unsigned i = 1; i < 4; ++i) {
      TEST(EINVAL == base64decode_string(&(string_t)string_INIT(4*i+i, ""), &result)) ;
      TEST(0 == sizecontent_wbuffer(&result)) ;
   }

   // TEST EINVAL decode wrong chars
   TEST(EINVAL == base64decode_string(&(string_t)string_INIT(4, "Ab3@"), &result)) ;
   TEST(EINVAL == base64decode_string(&(string_t)string_INIT(4, "Ab=4"), &result)) ;
   TEST(EINVAL == base64decode_string(&(string_t)string_INIT(4, "=b34"), &result)) ;

   TEST(0 == free_wbuffer(&result)) ;
   TEST(0 == free_wbuffer(&result2)) ;
   return 0 ;
ABBRUCH:
   free_wbuffer(&result) ;
   free_wbuffer(&result2) ;
   return EINVAL ;
}

int unittest_string_encode()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_urlencode())   goto ABBRUCH ;
   if (test_urldecode())   goto ABBRUCH ;
   if (test_base64())      goto ABBRUCH ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif

