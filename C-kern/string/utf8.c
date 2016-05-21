/* title: UTF-8 impl
   Implements <UTF-8>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/string/utf8.h
    Header file <UTF-8>.

   file: C-kern/string/utf8.c
    Implementation file <UTF-8 impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/string/utf8.h"
#include "C-kern/api/err.h"
#include "C-kern/api/math/int/log2.h"
#include "C-kern/api/memory/memstream.h"
#include "C-kern/api/string/string.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/time/systimer.h"
#include "C-kern/api/time/timevalue.h"
#endif

// section: utf8

uint8_t g_utf8_bytesperchar[64] = {
   /* [0 .. 127]/4 */
   /* 0*/ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   // [128 .. 191]/4 no valid first byte but also encoded as 1
   /*32*/ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   // [192 .. 223]/4 2 byte sequences (but values [192..193] are invalid)
   /*48*/ 2, 2, 2, 2, 2, 2, 2, 2,
   // [224 .. 239]/4 3 byte sequences
   /*56*/ 3, 3, 3, 3,
   // [240 .. 247]/4 4 byte sequences
   /*60*/ 4, 4,
   // [248 .. 251]/4 5 byte sequences
   /*62*/ 5,
   // [252 .. 253]/4 6 byte sequences
   /*63*/ 6
   // [254 .. 255]/4 are illegal but also mapped to g_utf8_bytesperchar[63] (6 Bytes)
};


uint8_t decodechar_utf8(const uint8_t strstart[], /*out*/char32_t * uchar)
{
   char32_t uchr;
   uint8_t  firstbyte;

   firstbyte = strstart[0];

   switch (firstbyte) {
   case   0: case   1: case   2: case   3: case   4: case   5: case   6: case   7: case   8: case   9:
   case  10: case  11: case  12: case  13: case  14: case  15: case  16: case  17: case  18: case  19:
   case  20: case  21: case  22: case  23: case  24: case  25: case  26: case  27: case  28: case  29:
   case  30: case  31: case  32: case  33: case  34: case  35: case  36: case  37: case  38: case  39:
   case  40: case  41: case  42: case  43: case  44: case  45: case  46: case  47: case  48: case  49:
   case  50: case  51: case  52: case  53: case  54: case  55: case  56: case  57: case  58: case  59:
   case  60: case  61: case  62: case  63: case  64: case  65: case  66: case  67: case  68: case  69:
   case  70: case  71: case  72: case  73: case  74: case  75: case  76: case  77: case  78: case  79:
   case  80: case  81: case  82: case  83: case  84: case  85: case  86: case  87: case  88: case  89:
   case  90: case  91: case  92: case  93: case  94: case  95: case  96: case  97: case  98: case  99:
   case 100: case 101: case 102: case 103: case 104: case 105: case 106: case 107: case 108: case 109:
   case 110: case 111: case 112: case 113: case 114: case 115: case 116: case 117: case 118: case 119:
   case 120: case 121: case 122: case 123: case 124: case 125: case 126: case 127:
      // 1 byte sequence
      *uchar = firstbyte;
      return 1;

   case 128: case 129: case 130: case 131: case 132: case 133: case 134: case 135: case 136: case 137:
   case 138: case 139: case 140: case 141: case 142: case 143: case 144: case 145: case 146: case 147:
   case 148: case 149: case 150: case 151: case 152: case 153: case 154: case 155: case 156: case 157:
   case 158: case 159: case 160: case 161: case 162: case 163: case 164: case 165: case 166: case 167:
   case 168: case 169: case 170: case 171: case 172: case 173: case 174: case 175: case 176: case 177:
   case 178: case 179: case 180: case 181: case 182: case 183: case 184: case 185: case 186: case 187:
   case 188: case 189: case 190: case 191:
      // used for encoding follow bytes
      goto ONERR;

   // 0b110xxxxx
   case 192: case 193:
      // not used in two byte sequence (value < 0x80)
      goto ONERR;
   case 194: case 195: case 196: case 197: case 198: case 199: case 200: case 201: case 202: case 203:
   case 204: case 205: case 206: case 207: case 208: case 209: case 210: case 211: case 212: case 213:
   case 214: case 215: case 216: case 217: case 218: case 219: case 220: case 221: case 222: case 223:
      // 2 byte sequence
      uchr   = ((uint32_t)(firstbyte & 0x1F) << 6);
      uchr  |= (strstart[1] ^ 0x80);
      *uchar = uchr;
      return 2;

   // 0b1110xxxx
   case 224: case 225: case 226: case 227: case 228: case 229: case 230: case 231:
   case 232: case 233: case 234: case 235: case 236: case 237: case 238: case 239:
      // 3 byte sequence
      uchr   = ((uint32_t)(firstbyte & 0xF) << 12);
      uchr  ^= (char32_t)strstart[1] << 6;
      uchr  ^= strstart[2];
      *uchar = uchr ^ (((char32_t)0x80 << 6) | 0x80);
      return 3;

   // 0b11110xxx
   case 240: case 241: case 242: case 243: case 244: case 245: case 246: case 247:
      // 4 byte sequence
      uchr   = ((uint32_t)(firstbyte & 0x7) << 18);
      uchr  ^= (char32_t)strstart[1] << 12;
      uchr  ^= (char32_t)strstart[2] << 6;
      uchr  ^= strstart[3];
      *uchar = uchr ^ (((((char32_t)0x80 << 6) | 0x80) << 6) | 0x80);
      return 4;

   // 0b111110xx
   case 248: case 249: case 250: case 251:
      // 5 byte sequence
      uchr   = ((uint32_t)(firstbyte & 0x3) << 24);
      uchr  ^= (char32_t)strstart[1] << 18;
      uchr  ^= (char32_t)strstart[2] << 12;
      uchr  ^= (char32_t)strstart[3] << 6;
      uchr  ^= strstart[4];
      *uchar = uchr ^ (((((((char32_t)0x80 << 6) | 0x80) << 6) | 0x80) << 6) | 0x80);
      return 5;

   // 0b1111110x
   case 252: case 253:
      // 6 byte sequence
      uchr   = ((uint32_t)(firstbyte & 0x1) << 30);
      uchr  ^= (char32_t)strstart[1] << 24;
      uchr  ^= (char32_t)strstart[2] << 18;
      uchr  ^= (char32_t)strstart[3] << 12;
      uchr  ^= (char32_t)strstart[4] << 6;
      uchr  ^= strstart[5];
      *uchar = uchr ^ (((((((((char32_t)0x80 << 6) | 0x80) << 6) | 0x80) << 6) | 0x80) << 6) | 0x80);
      return 6;

   case 254: case 255:
      // not used for encoding
      goto ONERR;
   }

ONERR:
   return 0;
}

uint8_t encodechar_utf8(char32_t uchar, size_t strsize, /*out*/uint8_t strstart[strsize])
{
   if (uchar <= 0xffff) {
      if (uchar <= 0x7f) {
         if (strsize < 1) return 0;
         strstart[0] = (uint8_t) uchar;
         return 1;
      } else if (uchar <= 0x7ff) {
         if (strsize < 2) return 0;
         strstart[0] = (uint8_t) (0xC0 | (uchar >> 6));
         strstart[1] = (uint8_t) (0x80 | (uchar & 0x3f));
         return 2;
      } else {
         if (strsize < 3) return 0;
         strstart[0] = (uint8_t) (0xE0 | (uchar >> 12));
         strstart[1] = (uint8_t) (0x80 | ((uchar >> 6) & 0x3f));
         strstart[2] = (uint8_t) (0x80 | (uchar & 0x3f));
         return 3;
      }
   } else if (uchar <= 0x1fffff) {  // 0b11110xxx
      if (strsize < 4) return 0;
      strstart[0] = (uint8_t) (0xF0 | (uchar >> 18));
      strstart[1] = (uint8_t) (0x80 | ((uchar >> 12) & 0x3f));
      strstart[2] = (uint8_t) (0x80 | ((uchar >> 6) & 0x3f));
      strstart[3] = (uint8_t) (0x80 | (uchar & 0x3f));
      return 4;
   } else if (uchar <= 0x3FFFFFF) { // 0b111110xx
      if (strsize < 5) return 0;
      strstart[0] = (uint8_t) (0xF8 | (uchar >> 24));
      strstart[1] = (uint8_t) (0x80 | ((uchar >> 18) & 0x3f));
      strstart[2] = (uint8_t) (0x80 | ((uchar >> 12) & 0x3f));
      strstart[3] = (uint8_t) (0x80 | ((uchar >> 6) & 0x3f));
      strstart[4] = (uint8_t) (0x80 | (uchar & 0x3f));
      return 5;
   } else if (uchar <= 0x7FFFFFFF) { // 0b1111110x
      if (strsize < 6) return 0;
      strstart[0] = (uint8_t) (0xFC | (uchar >> 30));
      strstart[1] = (uint8_t) (0x80 | ((uchar >> 24) & 0x3f));
      strstart[2] = (uint8_t) (0x80 | ((uchar >> 18) & 0x3f));
      strstart[3] = (uint8_t) (0x80 | ((uchar >> 12) & 0x3f));
      strstart[4] = (uint8_t) (0x80 | ((uchar >> 6) & 0x3f));
      strstart[5] = (uint8_t) (0x80 | (uchar & 0x3f));
      return 6;
   }

   return 0;
}

size_t length_utf8(const uint8_t *strstart, const uint8_t *strend)
{
   size_t len = 0;

   if (strstart < strend) {
      const uint8_t* next = strstart;

      if ((size_t)(strend - strstart) >= maxsize_utf8()) {
         const uint8_t* end = strend - maxsize_utf8();
         do {
            const unsigned sizechr = sizePfirst_utf8(*next);
            next += sizechr;
            len  += 1;
         } while (next <= end);
      }

      while (next < strend) {
         const unsigned sizechr = sizePfirst_utf8(*next);
         if ((size_t)(strend-next) < sizechr) {
            next = strend;
         } else {
            next += sizechr;
         }
         ++ len;
      }
   }

   return len;
}

const uint8_t * find_utf8(size_t size, const uint8_t str[size], char32_t uchar)
{
   const uint8_t *found;
   uint8_t        utf8[maxsize_utf8()];
   uint8_t        len = encodechar_utf8(uchar, sizeof(utf8), utf8);

   if (! len) return 0;

   for (found = memchr(str, utf8[0], size); found; found += len, found = memchr(found, utf8[0], (size_t) (str + size - found))) {
      for (unsigned i = 1; i < len; ++i) {
         if (utf8[i] != found[i]) goto CONTINUE;
      }
      return found;
      CONTINUE: ;
   }

   return 0;
}


// section: utf8validator_t

int validate_utf8validator(utf8validator_t * utf8validator, size_t size, const uint8_t data[size], /*err*/size_t *erroffset)
{
   static_assert(sizeof(utf8validator->prefix) == maxsize_utf8(), "prefix contains at least 1 byte less");

   const uint8_t *next    = data;
   const uint8_t *endnext = data + (size >= maxsize_utf8() ? size - maxsize_utf8() : 0);
   size_t         size_missing = 0;
   size_t         erroff  = 0;

   if (size == 0) return 0;  // nothing to do

   if (utf8validator->sizeprefix) {
      // complete prefix and check it
      size_t size_needed  = sizePfirst_utf8(utf8validator->prefix[0]);
      size_missing = size_needed - utf8validator->sizeprefix;
      size_missing = size < size_missing ? size : size_missing;
      for (size_t i = 0; i < size_missing; ) {
         utf8validator->prefix[utf8validator->sizeprefix++] = data[i++];
      }
      // if size_next is too low then VALIDATE generates an error (utf8validator->prefix is filled with 0)
      next = utf8validator->prefix;
      endnext = utf8validator->prefix;
      goto VALIDATE;
   }

   for (;;) {

      if (next >= endnext) {
         if (utf8validator->sizeprefix) {
            utf8validator->sizeprefix = 0;
            next = data + size_missing;
            endnext = data + size;
            size_missing = 0;
            if ((size_t)(endnext - next) >= maxsize_utf8()) {
               endnext -= maxsize_utf8();
               goto VALIDATE;
            }
         }
         size_t nrbytes = (size_t) (data + size - next);
         if (! nrbytes) break; // whole buffer done
         if (sizePfirst_utf8(next[0]) > nrbytes) {
            utf8validator->sizeprefix = (uint8_t) nrbytes;
            for (size_t i = 0; i < nrbytes; ++i) {
               utf8validator->prefix[i] = next[i];
            }
            for (size_t i = nrbytes; i < sizeof(utf8validator->prefix); ++i) {
               utf8validator->prefix[i] = 0; // ensures error during validate (utf8validator->sizeprefix < sizePfirst_utf8(next[0])
            }
            // size_complement == 0
            next = utf8validator->prefix;
         }
         endnext = next;
      }

      // validate sequence

VALIDATE:

      switch (next[0]) {
      case   0: case   1: case   2: case   3: case   4: case   5: case   6: case   7: case   8: case   9:
      case  10: case  11: case  12: case  13: case  14: case  15: case  16: case  17: case  18: case  19:
      case  20: case  21: case  22: case  23: case  24: case  25: case  26: case  27: case  28: case  29:
      case  30: case  31: case  32: case  33: case  34: case  35: case  36: case  37: case  38: case  39:
      case  40: case  41: case  42: case  43: case  44: case  45: case  46: case  47: case  48: case  49:
      case  50: case  51: case  52: case  53: case  54: case  55: case  56: case  57: case  58: case  59:
      case  60: case  61: case  62: case  63: case  64: case  65: case  66: case  67: case  68: case  69:
      case  70: case  71: case  72: case  73: case  74: case  75: case  76: case  77: case  78: case  79:
      case  80: case  81: case  82: case  83: case  84: case  85: case  86: case  87: case  88: case  89:
      case  90: case  91: case  92: case  93: case  94: case  95: case  96: case  97: case  98: case  99:
      case 100: case 101: case 102: case 103: case 104: case 105: case 106: case 107: case 108: case 109:
      case 110: case 111: case 112: case 113: case 114: case 115: case 116: case 117: case 118: case 119:
      case 120: case 121: case 122: case 123: case 124: case 125: case 126: case 127:
         // 1 byte sequence
         next += 1;
         continue;

      case 128: case 129: case 130: case 131: case 132: case 133: case 134: case 135: case 136: case 137:
      case 138: case 139: case 140: case 141: case 142: case 143: case 144: case 145: case 146: case 147:
      case 148: case 149: case 150: case 151: case 152: case 153: case 154: case 155: case 156: case 157:
      case 158: case 159: case 160: case 161: case 162: case 163: case 164: case 165: case 166: case 167:
      case 168: case 169: case 170: case 171: case 172: case 173: case 174: case 175: case 176: case 177:
      case 178: case 179: case 180: case 181: case 182: case 183: case 184: case 185: case 186: case 187:
      case 188: case 189: case 190: case 191:
         // used for encoding following bytes
         goto ONERR;

      case 192: case 193:
         // not used in two byte sequence (value < 0x80)
         goto ONERR;

      case 194: case 195: case 196: case 197: case 198: case 199: case 200: case 201:
      case 202: case 203: case 204: case 205: case 206: case 207: case 208: case 209: case 210: case 211:
      case 212: case 213: case 214: case 215: case 216: case 217: case 218: case 219: case 220: case 221:
      case 222: case 223:
         // 2 byte sequence
         if ((next[1] & 0xC0) != 0x80) { erroff = 1; goto ONERR; }
         next += 2;
         continue;

      case 224:
         // 3 byte sequence
         if ((next[1] & 0xC0) != 0x80)  { erroff = 1; goto ONERR; }
         if (next[1] < 0xA0/*too low*/) { erroff = 1; goto ONERR; }
         goto CHECK3_COMMON;

      case 225: case 226: case 227: case 228: case 229: case 230: case 231: case 232:
      case 233: case 234: case 235: case 236: case 237: case 238: case 239:
         // 3 byte sequence
         if ((next[1] & 0xC0) != 0x80) { erroff = 1; goto ONERR; }
      CHECK3_COMMON:
         if ((next[2] & 0xC0) != 0x80) { erroff = 2; goto ONERR; }
         next += 3;
         continue;

      case 240:
         // 4 byte sequence
         if ((next[1] & 0xC0) != 0x80)  { erroff = 1; goto ONERR; }
         if (next[1] < 0x90/*too low*/) { erroff = 1; goto ONERR; }
         goto CHECK4_COMMON;

      case 241: case 242: case 243: case 244: case 245: case 246: case 247:
         // 4 byte sequence
         if ((next[1] & 0xC0) != 0x80) { erroff = 1; goto ONERR; }
      CHECK4_COMMON:
         if ((next[2] & 0xC0) != 0x80) { erroff = 2; goto ONERR; }
         if ((next[3] & 0xC0) != 0x80) { erroff = 3; goto ONERR; }
         next += 4;
         continue;

      case 248:
         // 5 byte sequence
         if ((next[1] & 0xC0) != 0x80)  { erroff = 1; goto ONERR; }
         if (next[1] < 0x88/*too low*/) { erroff = 1; goto ONERR; }
         goto CHECK5_COMMON;

      case 249: case 250: case 251:
         if ((next[1] & 0xC0) != 0x80) { erroff = 1; goto ONERR; }
      CHECK5_COMMON:
         if ((next[2] & 0xC0) != 0x80) { erroff = 2; goto ONERR; }
         if ((next[3] & 0xC0) != 0x80) { erroff = 3; goto ONERR; }
         if ((next[4] & 0xC0) != 0x80) { erroff = 4; goto ONERR; }
         next += 5;
         continue;

      case 252:
         // 6 byte sequence
         if ((next[1] & 0xC0) != 0x80)  { erroff = 1; goto ONERR; }
         if (next[1] < 0x84/*too low*/) { erroff = 1; goto ONERR; }
         goto CHECK6_COMMON;

      case 253:
         // 6 byte sequence
         if ((next[1] & 0xC0) != 0x80) { erroff = 1; goto ONERR; }
      CHECK6_COMMON:
         if ((next[2] & 0xC0) != 0x80) { erroff = 2; goto ONERR; }
         if ((next[3] & 0xC0) != 0x80) { erroff = 3; goto ONERR; }
         if ((next[4] & 0xC0) != 0x80) { erroff = 4; goto ONERR; }
         if ((next[5] & 0xC0) != 0x80) { erroff = 5; goto ONERR; }
         next += 6;
         continue;

      case 254: case 255:
         goto ONERR;

      }// switch (firstbyte)

   }// for(;;)

   return 0;
ONERR:
   // check that not the missing part of prefix generated the error (filled with 0) !
   if (utf8validator->sizeprefix) {
      // is prefix valid ?
      if (utf8validator->sizeprefix <= erroff) return 0; // it is
      if (size_missing) {
         // completion of prefix is wrong
         next = data;
         erroff -= (utf8validator->sizeprefix - size_missing)/*compensate for already stored prefix*/;
      } else {
         // last sequence is a bad prefix
         next = data + size - utf8validator->sizeprefix;
      }
      utf8validator->sizeprefix = 0;
   }
   // set err parameter
   if (erroffset) *erroffset = erroff + (size_t) (next - data);
   return EILSEQ;
}


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_utf8(void)
{
   char32_t uchar;
   const uint8_t * const utf8str[6] = {
      (const uint8_t *) "\U7fffffff",
      (const uint8_t *) "\U03ffffff",
      (const uint8_t *) "\U001fffff",
      (const uint8_t *) "\U0000ffff",
      (const uint8_t *) "\u07ff",
      (const uint8_t *) "\x7f"
   };

   // TEST maxchar_utf8
   TEST(maxchar_utf8() == 0x7fffffff);

   // TEST maxsize_utf8
   TEST(maxsize_utf8() == 6);

   // TEST sizePfirst_utf8: from encoded value
   for (size_t i = 0; i < lengthof(utf8str); ++i) {
      TEST(6-i == sizePfirst_utf8(utf8str[i][0]));
   }

   // TEST sizePfirst_utf8 of all 256 first byte values
   for (unsigned i = 0; i < 256; ++i) {
      unsigned s = 1u + (i >= 192) + (i >= 224) + (i >= 240) + (i >= 248) + (i >= 252);
      TEST(s == sizePfirst_utf8(i));
      TEST(s == sizePfirst_utf8(i+256/*Casted to uint8_t*/));
      TEST(s == sizePfirst_utf8(i+1024/*Casted to uint8_t*/));
   }

   // TEST sizechar_utf8
   for (char32_t i = 0; i <= 0x7f; ++i) {
      TEST(1 == sizechar_utf8(i));
   }
   for (char32_t i = 0x80; i < 0x800; ++i) {
      TEST(2 == sizechar_utf8(i));
   }
   for (char32_t i = 0x800; i < 0x10000; ++i) {
      TEST(3 == sizechar_utf8(i));
   }
   for (char32_t i = 0x10000; i < 0x200000; ++i) {
      TEST(4 == sizechar_utf8(i));
   }
   for (char32_t i = 0x200000; i < 0x4000000; ++i) {
      if (i == 0x200000 + 100) i = 0x4000000 - 100;
      TEST(5 == sizechar_utf8(i));
   }
   for (char32_t i = 0x4000000; i < 0x80000000; ++i) {
      if (i == 0x4000000 + 100) i = 0x80000000 - 100;
      TEST(6 == sizechar_utf8(i));
   }

   // TEST sizechar_utf8: no range check if too big
   TEST(6 == sizechar_utf8(0xffffffff));

   // TEST isfirstbyte_utf8
   for (unsigned i = 0; i < 256; ++i) {
      /* values between [128 .. 191] are used to encode follow bytes */
      bool isOK = (i < 128 || 192 <= i);
      TEST(isOK == isfirstbyte_utf8((uint8_t)i));
   }

   // TEST issinglebyte_utf8
   for (unsigned i = 0; i < 256; ++i) {
      TEST((i < 128) == issinglebyte_utf8((uint8_t)i));
   }

   // TEST length_utf8
   const char * teststrings[] = { "\U7fffffff\U00010000", "\u0800\u0999\uFFFF", "\u00A0\u00A1\u07FE\u07FF", "\x01\x02""abcde\x07e\x7f", "\U0010FFFF\uF999\u06FEY" };
   size_t       testlength[]  = { 2,                      3,                    4,                          9,                          4 };
   for (unsigned i = 0; i < lengthof(teststrings); ++i) {
      const char * endstr = (const char*) ((uintptr_t)teststrings[i] + (uintptr_t) strlen(teststrings[i]));
      TEST(testlength[i] == length_utf8((const uint8_t*)teststrings[i], (const uint8_t*)endstr));
   }

   // TEST length_utf8: empty string
   TEST(0 == length_utf8(utf8str[0], utf8str[0]));
   TEST(0 == length_utf8(0, 0));
   TEST(0 == length_utf8((void*)(uintptr_t)-1, (void*)(uintptr_t)-1));

   // TEST length_utf8: illegal sequence && last sequence not fully contained in string
   const char * teststrings2[] = { "\xFC\x80",  "b\xC2",                 "ab\xE0",  "abc\xF0" };
   size_t       testlength2[]  = { 1/*EILSEQ*/, 2/*last not contained*/, 3/*ditto*/, 4/*ditto*/ };
   for (unsigned i = 0; i < lengthof(teststrings2); ++i) {
      const char * endstr = (const char*) ((uintptr_t)teststrings2[i] + (uintptr_t) strlen(teststrings2[i]));
      TEST(testlength2[i] == length_utf8((const uint8_t*)teststrings2[i], (const uint8_t*)endstr));
   }

   // TEST skipchar_utf8: whole range of first byte
   for (unsigned i = 0; i < 256; ++i) {
      uint8_t buffer[2] = { (uint8_t)i, 0 };
      TEST( skipchar_utf8(buffer) == sizePfirst_utf8(i));
   }

   // TEST decodechar_utf8: whole range of first byte (does not check for correct encoding)
   char32_t fillchar[] = { 0x0, 0x1, 0x3F };
   for (char32_t i = 0; i < 256; ++i) {
      uint8_t buffer[6] = { (uint8_t)i, 0 };
      for (unsigned fci = 0; fci < lengthof(fillchar); ++fci) {
         char32_t expect    = 0;
         int      isInvalid = (128 <= i && i <= 193) || (i >= 254);
         memset(buffer+1, (int) (0x80u + fillchar[fci]), lengthof(buffer)-1);
         switch (isInvalid ? 0 : sizePfirst_utf8(i)) {
         case 0:
            expect = 0;
            break;
         case 1:
            expect = i;
            break;
         case 2:
            expect = ((i & 0x1f) << 6) + fillchar[fci];
            break;
         case 3:
            expect = ((i & 0xf) << 12) + (fillchar[fci] << 6) + fillchar[fci];
            break;
         case 4:
            expect = ((i & 0x7) << 18) + (fillchar[fci] << 12) + (fillchar[fci] << 6) + fillchar[fci];
            break;
         case 5:
            expect = ((i & 0x3) << 24) + (fillchar[fci] << 18) + (fillchar[fci] << 12) + (fillchar[fci] << 6) + fillchar[fci];
            break;
         case 6:
            expect = ((i & 0x1) << 30) + (fillchar[fci] << 24) + (fillchar[fci] << 18) + (fillchar[fci] << 12) + (fillchar[fci] << 6) + fillchar[fci];
            break;
         }
         uchar = 0;
         TEST( decodechar_utf8(buffer, &uchar) == (isInvalid ? 0 : sizePfirst_utf8(i)));
         TEST( uchar == expect);
      }
   }

   // TEST decodechar_utf8: check that values too low and values too high are recognized
   struct {
      const uint8_t *   str;
      char32_t          uchar;
      uint8_t           len;
   } minmaxstr[] = {
      // 0x7f encoded in 2 bytes (too small), 0x80 and 0x7ff work
      { (const uint8_t*)"\xC1\xBF", 0,     0 }, // 0x7f   0b11000001 0b10111111
      { (const uint8_t*)"\xC2\x80", 0x80,  2 }, // 0x80   0b11000010 0b10000000
      { (const uint8_t*)"\xDF\xBF", 0x7ff, 2 }, // 0x7ff  0b11011111 0b10111111
      // 0x7ff encoded in 3 bytes (too small but not checked), 0x800 and 0xffff work
      { (const uint8_t*)"\xE0\x9F\xBF", 0x7ff,  3 },  // 0x7ff   0b11100000 0b10011111 0b10111111
      { (const uint8_t*)"\xE0\xA0\x80", 0x800,  3 },  // 0x800   0b11100000 0b10100000 0b10000000
      { (const uint8_t*)"\xEF\xBF\xBF", 0xffff, 3 },  // 0xffff  0b11101111 0b10111111 0b10111111
      // 0xffff encoded in 4 bytes (too small but not checked), 0x10000 works
      { (const uint8_t*)"\xF0\x8F\xBF\xBF", 0xffff,  4 },   // 0xffff   0b11110000 0b10001111 0b10111111 0b10111111
      { (const uint8_t*)"\xF0\x90\x80\x80", 0x10000, 4 },   // 0x10000  0b11110000 0b10010000 0b10000000 0b10000000
      { (const uint8_t*)"\xF7\xBF\xBF\xBF", 0x1fffff, 4 },
      // 0x1fffff encoded in 5 bytes (too small but not checked), 0x200000 works
      { (const uint8_t*)"\xF8\x87\xBF\xBF\xBF", 0x1fffff, 5 },
      { (const uint8_t*)"\xF8\x88\x80\x80\x80", 0x200000, 5 },
      { (const uint8_t*)"\xFB\xBF\xBF\xBF\xBF", 0x3ffffff, 5 },
      // 0x3ffffff encoded in 6 bytes (too small but not checked), 0x4000000 works
      { (const uint8_t*)"\xFC\x83\xBF\xBF\xBF\xBF", 0x3ffffff, 6 },
      { (const uint8_t*)"\xFC\x84\x80\x80\x80\x80", 0x4000000, 6 },
      { (const uint8_t*)"\xFD\xBF\xBF\xBF\xBF\xBF", 0x7fffffff, 6 },
   };
   for (unsigned i = 0; i < lengthof(minmaxstr); ++i) {
      uchar = 0;
      TEST(minmaxstr[i].len   == decodechar_utf8(minmaxstr[i].str, &uchar));
      TEST(minmaxstr[i].uchar == uchar);
   }

   uint8_t utf8buffer[maxsize_utf8()];

   // TEST encodechar_utf8: 1 byte seqeunce
   for (char32_t i = 0; i < 0x80; ++i) {
      TEST(1 == encodechar_utf8(i, 1, utf8buffer));
      TEST(1 == decodechar_utf8(utf8buffer, &uchar));
      TEST(i == uchar);
   }

   // TEST encodechar_utf8: 2 byte seqeunce
   for (char32_t i = 0x80; i < 0x800; ++i) {
      TEST(2 == encodechar_utf8(i, 2, utf8buffer));
      TEST(2 == decodechar_utf8(utf8buffer, &uchar));
      TEST(i == uchar);
   }

   // TEST encodechar_utf8: 3 byte seqeunce
   for (char32_t i = 0x800; i < 0x10000; ++i) {
      TEST(3 == encodechar_utf8(i, 3, utf8buffer));
      TEST(3 == decodechar_utf8(utf8buffer, &uchar));
      TEST(i == uchar);
   }

   // TEST encodechar_utf8: 4 byte seqeunce
   for (char32_t i = 0x10000; i < 0x200000; ++i) {
      TEST(4 == encodechar_utf8(i, 4, utf8buffer));
      TEST(4 == decodechar_utf8(utf8buffer, &uchar));
      TEST(i == uchar);
   }

   // TEST encodechar_utf8: 5 byte seqeunce
   for (char32_t i = 0x200000; i < 0x4000000; ++i) {
      i += 32u * (i < 0x4000000-1024);
      TEST(5 == encodechar_utf8(i, 5, utf8buffer));
      TEST(5 == decodechar_utf8(utf8buffer, &uchar));
      TEST(i == uchar);
   }

   // TEST encodechar_utf8: 6 byte seqeunce
   for (char32_t i = 0x4000000; i < 0x80000000; ++i) {
      i += 2048u * (i < 0x80000000-4096);
      TEST(6 == encodechar_utf8(i, 6, utf8buffer));
      TEST(6 == decodechar_utf8(utf8buffer, &uchar));
      TEST(i == uchar);
   }

   // TEST encodechar_utf8: buffer too small
   TEST(0 == encodechar_utf8(0x7f, 0, utf8buffer));
   TEST(0 == encodechar_utf8(0x7ff, 1, utf8buffer));
   TEST(0 == encodechar_utf8(0xffff, 2, utf8buffer));
   TEST(0 == encodechar_utf8(0x1fffff, 3, utf8buffer));
   TEST(0 == encodechar_utf8(0x3ffffff, 4, utf8buffer));
   TEST(0 == encodechar_utf8(0x7fffffff, 5, utf8buffer));

   // TEST encodechar_utf8: out of range
   TEST(0 == encodechar_utf8(0x80000000, sizeof(utf8buffer), utf8buffer));

   return 0;
ONERR:
   return EINVAL;
}

static int test_utf8validator(void)
{
   utf8validator_t   utf8val = utf8validator_INIT;
   size_t            erroffset;
   uint8_t           buffer[256];

   // TEST utf8validator_INIT
   TEST(0 == utf8val.sizeprefix);
   for (unsigned i = 0; i < lengthof(utf8val.prefix); ++i) {
      TEST(0 == utf8val.prefix[i]);
   }

   // TEST utf8validator_INIT
   memset(&utf8val, 255, sizeof(utf8val));
   init_utf8validator(&utf8val);
   TEST(0 == utf8val.sizeprefix);
   for (unsigned i = 0; i < lengthof(utf8val.prefix); ++i) {
      TEST(0 == utf8val.prefix[i]);
   }

   // TEST free_utf8validator
   memset(&utf8val, 255, sizeof(utf8val));
   TEST(EILSEQ == free_utf8validator(&utf8val));
   TEST(0 == utf8val.sizeprefix);
   TEST(0 == free_utf8validator(&utf8val));
   TEST(0 == utf8val.sizeprefix);
   for (unsigned i = 0; i < lengthof(utf8val.prefix); ++i) {
      TEST(255 == utf8val.prefix[i]);
   }

   // TEST sizeprefix_utf8validator
   for (uint8_t i = 10; i > 0; ) {
      utf8val.sizeprefix = --i;
      TEST(i == sizeprefix_utf8validator(&utf8val));
   }

   // TEST validate_utf8validator: all 256 first bytes with invalid follow bytes
   memset(buffer, 0, maxsize_utf8());
   for (unsigned i = 0; i < 256; ++i) {
      size_t expect = (i >= 194) && (i <= 253);
      buffer[0] = (uint8_t)i;
      if (i <= 127) {
         TEST(0 == validate_utf8validator(&utf8val, 1, buffer, 0))
      } else {
         erroffset = 2;
         TEST(EILSEQ == validate_utf8validator(&utf8val, 2, buffer, &erroffset));
         TEST(expect == erroffset);
      }
      TEST(0 == sizeprefix_utf8validator(&utf8val));
   }

   // TEST validate_utf8validator: min, max and too low values
   uint8_t mincharval[5][6] = {
      { 0xc2, 0x80, 0, 0, 0, 0 },               // value 0x80 as 0b110xxxxx 0b10xxxxxx
      { 0xe0, 0x80 + 0x20, 0x80, 0, 0, 0},      // value 0x800 as 0b1110xxxx 0b10xxxxxx 0b10xxxxxx
      { 0xf0, 0x80 + 0x10, 0x80, 0x80, 0, 0},   // value 0x10000 as 0b11110xxx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx
      { 0xf8, 0x80 + 0x08, 0x80, 0x80, 0x80, 0},// value 0x200000 as 0b111110xx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx
      { 0xfc, 0x80 + 0x04, 0x80, 0x80, 0x80, 0x80} // value 0x4000000 as 0b1111110x 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx
   };
   uint8_t maxcharval[5][6] = {
      { 0xdf, 0x80+0x3f, 0, 0, 0, 0 },       // value 0x7FF as 0b110xxxxx 0b10xxxxxx
      { 0xef, 0xbf, 0xbf, 0, 0, 0},          // value 0xFFFF as 0b1110xxxx 0b10xxxxxx 0b10xxxxxx
      { 0xf7, 0xbf, 0xbf, 0xbf, 0, 0},       // value 0x1FFFFF as 0b11110xxx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx
      { 0xfb, 0xbf, 0xbf, 0xbf, 0xbf, 0},    // value 0x3ffffff as 0b111110xx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx
      { 0xfd, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf}  // value 0x7fffffff as 0b1111110x 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx
   };
   uint8_t lowcharval[5][6] = {
      { 0xc1, 0x80 + 0x3f, 0, 0, 0, 0 },           // value 0x7f as 0b110xxxxx 0b10xxxxxx
      { 0xe0, 0x80 + 0x1f, 0x80 + 0x3f, 0, 0, 0},  // value 0x7ff as 0b1110xxxx 0b10xxxxxx 0b10xxxxxx
      { 0xf0, 0x80 + 0x0f, 0xbf, 0xbf, 0, 0},      // value 0xffff as 0b11110xxx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx
      { 0xf8, 0x80 + 0x07, 0xbf, 0xbf, 0xbf, 0},   // value 0x1fffff as 0b111110xx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx
      { 0xfc, 0x80 + 0x03, 0xbf, 0xbf, 0xbf, 0xbf} // value 0x3ffffff as 0b1111110x 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx
   };
   for (unsigned i = 0; i < lengthof(mincharval); ++i) {
      char32_t chr;
      // check values in tables
      TEST( 2+i == decodechar_utf8(mincharval[i], &chr));
      TEST( chr == ((char32_t[]){0x80,0x800,0x10000,0x200000,0x4000000})[i]);
      TEST( 2+i == decodechar_utf8(maxcharval[i], &chr));
      TEST( chr == ((char32_t[]){0x800,0x10000,0x200000,0x4000000,0x80000000})[i]-1);
      if (i == 0) {
         TEST( 0 == decodechar_utf8(lowcharval[i], &chr)); // this error is recognized
      } else {
         TEST( 2+i == decodechar_utf8(lowcharval[i], &chr));
         TEST( chr == ((char32_t[]){0x80,0x800,0x10000,0x200000,0x4000000})[i]-1);
      }
      // test mincharval OK
      TEST( 0 == validate_utf8validator(&utf8val, 6, mincharval[i], 0))
      TEST( 0 == sizeprefix_utf8validator(&utf8val));
      // test maxcharval OK
      TEST( 0 == validate_utf8validator(&utf8val, 6, maxcharval[i], 0))
      TEST( 0 == sizeprefix_utf8validator(&utf8val));
      // test lowcharval ERROR
      erroffset = 2;
      TEST( EILSEQ == validate_utf8validator(&utf8val, 6, lowcharval[i], &erroffset))
      TEST( sizeprefix_utf8validator(&utf8val) == 0);
      TEST( erroffset == (i!=0));
   }

   // TEST validate_utf8validator: follow byte
   for (unsigned i = 194; i < 254; ++i) {
      buffer[0] = (uint8_t)i;
      size_t mbslen = sizePfirst_utf8(buffer[0]);
      for (unsigned fi = 1; fi < mbslen; ++fi) {
         for (unsigned tc = 0; tc < 2; ++tc) {
            buffer[1] = 0xbf;
            memset(buffer+2, tc ? 0x80+0x3f : 0x80, mbslen-2);
            // test buffer with valid follow bytes OK
            TEST(0 == validate_utf8validator(&utf8val, mbslen, buffer, 0));
            TEST(0 == sizeprefix_utf8validator(&utf8val));
            // test buffer with invalid follow byte ERROR
            buffer[fi] = (uint8_t) (buffer[fi] ^ 0x80);
            TEST( EILSEQ == validate_utf8validator(&utf8val, mbslen, buffer, &erroffset));
            TEST( sizeprefix_utf8validator(&utf8val) == 0);
            TEST( erroffset == fi);
            // test buffer with invalid follow byte ERROR
            buffer[fi] = (uint8_t) (buffer[fi] | 0x40);
            TEST( EILSEQ == validate_utf8validator(&utf8val, mbslen, buffer, &erroffset));
            TEST( sizeprefix_utf8validator(&utf8val) == 0);
            TEST( erroffset == fi);
            // test buffer with invalid follow byte ERROR
            buffer[fi] = (uint8_t) (buffer[fi] | 0x80);
            TEST( EILSEQ == validate_utf8validator(&utf8val, mbslen, buffer, &erroffset));
            TEST( sizeprefix_utf8validator(&utf8val) == 0);
            TEST( erroffset == fi);
         }
      }
   }

   // TEST validate_utf8validator: no sequence crosses buffers
   for (char32_t i = 0; i < maxsize_utf8(); ++i) {
      char32_t chr = ((char32_t[]){ 0, 0x80, 0x800, 0x10000, 0x200000, 0x4000000 })[i];
      memset(buffer, 0, sizeof(buffer));
      for (size_t len = 0; len < sizeof(buffer)-maxsize_utf8(); ++chr) {
         len += encodechar_utf8(chr, maxsize_utf8(), buffer+len);
      }
      TEST( 0 == validate_utf8validator(&utf8val, sizeof(buffer), buffer, 0))
      TEST( 0 == sizeprefix_utf8validator(&utf8val));
      for (size_t off = 0; off < sizeof(buffer); ++off) {
         uint8_t old = buffer[off];
         buffer[off] = (uint8_t) (isfirstbyte_utf8(old) ? 0x80 : old^0x80);
         TEST( EILSEQ == validate_utf8validator(&utf8val, sizeof(buffer), buffer, &erroffset));
         TEST( erroffset == off);
         TEST( 0 == sizeprefix_utf8validator(&utf8val));
         buffer[off] = old;
      }
   }

   // TEST validate_utf8validator: last sequence crosses buffer (test correct prefix)
   for (unsigned mbslen = 2; mbslen <= maxsize_utf8(); ++mbslen) {
      char32_t chr = ((char32_t[]){ 0x80, 0x800, 0x10000, 0x200000, 0x4000000 })[mbslen-2];
      char32_t step = chr;
      for (unsigned i = 0; i < 5; ++i, chr += step) {
         TEST(mbslen == encodechar_utf8(chr, maxsize_utf8(), buffer+i*mbslen));
      }
      for (unsigned buflen = 4*mbslen+1; buflen < 5*mbslen; ++buflen) {
         size_t P = buflen - 4*mbslen;
         // test
         TEST(0 == validate_utf8validator(&utf8val, buflen, buffer, &erroffset));
         // check prefix
         TEST(P == sizeprefix_utf8validator(&utf8val));
         for (unsigned i = 0; i < sizeof(utf8val.prefix); ++i) {
            TEST((i < P ? buffer[4*mbslen+i] : 0) == utf8val.prefix[i]);
         }
         // test error case (no prefix stored)
         for (unsigned i = 0; i < buflen; ++i) {
            init_utf8validator(&utf8val);
            uint8_t old = buffer[i];
            buffer[i] = (uint8_t) (isfirstbyte_utf8(old) ? 0x80 : old^0x80);
            TEST( EILSEQ == validate_utf8validator(&utf8val, buflen, buffer, &erroffset));
            TEST( 0 == sizeprefix_utf8validator(&utf8val));
            TEST( i == erroffset);
            buffer[i] = old;
         }
      }
   }

   // TEST validate_utf8validator: generate prefix, continue with next buffer
   for (unsigned mbslen = 2; mbslen <= maxsize_utf8(); ++mbslen) {
      char32_t chr = ((char32_t[]){ 0x100, 0x3800, 0x50000, 0x3000000, 0x7000000 })[mbslen-2];
      char32_t step = chr >> 4;
      for (unsigned i = 0; i < 5; ++i, chr += step) {
         TEST(mbslen == encodechar_utf8(chr, maxsize_utf8(), buffer+i*mbslen));
      }
      for (uint8_t P = 1; P < mbslen; ++P) {
         init_utf8validator(&utf8val);
         utf8val.sizeprefix = P;
         memcpy(utf8val.prefix, buffer, P);
         // buffer of length 0 is ignored
         TEST(0 == validate_utf8validator(&utf8val, 0, buffer, &erroffset));
         TEST(P == sizeprefix_utf8validator(&utf8val));
         // continue
         TEST(0 == validate_utf8validator(&utf8val, 5*mbslen-P, buffer+P, &erroffset));
         TEST(0 == sizeprefix_utf8validator(&utf8val));
         // error case
         for (unsigned i = P; i < 5*mbslen; ++i) {
            init_utf8validator(&utf8val);
            utf8val.sizeprefix = P;
            memcpy(utf8val.prefix, buffer, P);
            uint8_t old = buffer[i];
            buffer[i] = (uint8_t) (isfirstbyte_utf8(old) ? 0x80 : old^0x80);
            TEST(EILSEQ == validate_utf8validator(&utf8val, 5*mbslen-P, buffer+P, &erroffset));
            TEST(0 == sizeprefix_utf8validator(&utf8val));
            TEST(i-P == erroffset);
            buffer[i] = old;
         }
      }
   }

   // TEST validate_utf8validator: generate prefix, continue with next buffer which also ends in prefix (test correct prefix)
   for (unsigned mbslen = 2; mbslen <= maxsize_utf8(); ++mbslen) {
      char32_t chr = ((char32_t[]){ 0x300, 0x1080, 0x30500, 0x207000, 0x4700000 })[mbslen-2];
      char32_t step = chr >> 4;
      for (unsigned i = 0; i < 7; ++i, chr += step) {
         TEST(mbslen == encodechar_utf8(chr, maxsize_utf8(), buffer+i*mbslen));
      }
      for (uint8_t P = 1; P < mbslen; ++P) {
         for (uint8_t E = 1; E < mbslen; ++E) {
            init_utf8validator(&utf8val);
            utf8val.sizeprefix = P;
            memcpy(utf8val.prefix, buffer, P);
            // continue
            TEST(0 == validate_utf8validator(&utf8val, 6*mbslen-P+E, buffer+P, &erroffset));
            TEST(E == sizeprefix_utf8validator(&utf8val));
            for (unsigned i = 0; i < sizeof(utf8val.prefix); ++i) {
               TEST((i < E ? buffer[6*mbslen+i] : 0) == utf8val.prefix[i]);
            }
            // error case
            for (unsigned i = P; i < 6*mbslen+E; ++i) {
               init_utf8validator(&utf8val);
               utf8val.sizeprefix = P;
               memcpy(utf8val.prefix, buffer, P);
               uint8_t old = buffer[i];
               buffer[i] = (uint8_t) (isfirstbyte_utf8(old) ? 0x80 : old^0x80);
               TEST(EILSEQ == validate_utf8validator(&utf8val, 6*mbslen-P+E, buffer+P, &erroffset));
               TEST(0 == sizeprefix_utf8validator(&utf8val));
               TEST(i-P == erroffset);
               buffer[i] = old;
            }
         }
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_find(void)
{
   const uint8_t *utf8str = (const uint8_t*) u8"\U7fffffff\U0010fff0\U0010ffff abcd\U0000fff0\U0000ffff abcd\u07f0\u07ff abcd\x7f abcd";
   const size_t   len = strlen((const char*)utf8str);

   // TEST find_utf8
   TEST( find_utf8(len, utf8str, 0x7fffffffu) == 0+utf8str);
   TEST( find_utf8(len, utf8str, 0x10ffffu) == 10+utf8str);
   TEST( find_utf8(len, utf8str, 0xffffu) == 22+utf8str);
   TEST( find_utf8(len, utf8str, 0x7ffu) == 32+utf8str);
   TEST( find_utf8(len, utf8str, 0x7fu) == 39+utf8str);
   TEST( find_utf8(len, utf8str, 'a') == 15+utf8str);

   // TEST find_utf8: character not in stream
   TEST( 0 == find_utf8(len, utf8str, 0x10fffeu));
   TEST( 0 == find_utf8(len, utf8str, 0xfffeu));
   TEST( 0 == find_utf8(len, utf8str, 0x7feu));
   TEST( 0 == find_utf8(len, utf8str, 0x7eu));

   // TEST find_utf8: codepoint out of range
   TEST( 0 == find_utf8(len, utf8str, (char32_t)-1));

   return 0;
ONERR:
   return EINVAL;
}

static int test_speed(void)
{
   systimer_t  timer = systimer_FREE;
   uint64_t    microsec;
   uint64_t    result[2]  = { UINT64_MAX, UINT64_MAX };
   uint8_t     buffer[1024];
   size_t      len;
   size_t      nrchar;

   // prepare
   TEST(0 == init_systimer(&timer, sysclock_MONOTONIC));

   for (nrchar = len = 0; len < lengthof(buffer)-maxsize_utf8(); ++nrchar) {
      len += encodechar_utf8((char32_t)(0x2000000u + nrchar), maxsize_utf8(), buffer + len);
   }

   for (unsigned testrepeat = 0; testrepeat < 5; ++testrepeat) {
      TEST(0 == startinterval_systimer(timer, &(timevalue_t) { .nanosec = 1000 } ));
      for (unsigned decoderepeat = 0; decoderepeat < 5; ++decoderepeat) {
         for (size_t i = 0, off = 0; i < nrchar; ++i) {
            char32_t uchar = 0;
            off += decodechar_utf8(buffer + off, &uchar);
            TEST( uchar == (char32_t)(0x2000000u + i));
         }
      }
      TEST(0 == expirationcount_systimer(timer, &microsec));
      TEST(0 == stop_systimer(timer));
      if (result[0] > microsec) result[0] = microsec;
   }

   for (unsigned testrepeat = 0; testrepeat < 5; ++testrepeat) {
      TEST(0 == startinterval_systimer(timer, &(timevalue_t) { .nanosec = 1000 } ));
      for (unsigned decoderepeat = 0; decoderepeat < 5; ++decoderepeat) {
         utf8validator_t uv;
         init_utf8validator(&uv);
         TEST(0 == validate_utf8validator(&uv, len, buffer, 0));
      }
      TEST(0 == expirationcount_systimer(timer, &microsec));
      TEST(0 == stop_systimer(timer));
      if (result[1] > microsec) result[1] = microsec;
   }

   if (result[1] >= result[0]) {
      logwarning_unittest("validate is not faster than decode_utf8");
   }

   TEST(0 == free_systimer(&timer));

   return 0;
ONERR:
   free_systimer(&timer);
   return EINVAL;
}

int unittest_string_utf8()
{
   if (test_utf8())           goto ONERR;
   if (test_utf8validator())  goto ONERR;
   if (test_find())           goto ONERR;
   if (test_speed())          goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}
#endif
