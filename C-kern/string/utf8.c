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
#include "C-kern/api/string/string.h"
#include "C-kern/api/string/stringstream.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/time/systimer.h"
#include "C-kern/api/time/timevalue.h"
#endif

// section: utf8

uint8_t g_utf8_bytesperchar[256] = {
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /*0 .. 127*/
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /*128..191 not the first byte => error*/
   0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
   2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, /*192..193 (< 0x80)*/ /*194..223*/
   3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, /*224..239*/
   4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0  /*240..244*/ /*245..247 too big*/ /*248..250, 252..253, 254..255 error*/
} ;

uint8_t decodechar_utf8(const uint8_t strstart[], /*out*/char32_t * uchar)
{
   char32_t uchr ;
   uint8_t  firstbyte ;

   firstbyte = strstart[0] ;

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
      *uchar = firstbyte ;
      return 1 ;

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
      uchr   = ((uint32_t)(firstbyte & 0x1F) << 6) ;
      uchr  |= (strstart[1] ^ 0x80) ;
      *uchar = uchr ;
      return 2 ;

   case 224:
      // 3 byte sequence
      uchr   = ((uint32_t)(224 & 0xF) << 12) ;
      uchr  ^= (char32_t)strstart[1] << 6 ;
      uchr  ^= strstart[2] ;
      *uchar = uchr ^ (((char32_t)0x80 << 6) | 0x80) ;
      return 3 ;

   case 225: case 226: case 227: case 228: case 229: case 230: case 231: case 232: case 233:
   case 234: case 235: case 236: case 237: case 238: case 239:
      // 3 byte sequence
      uchr   = ((uint32_t)(firstbyte & 0xF) << 12) ;
      uchr  ^= (char32_t)strstart[1] << 6 ;
      uchr  ^= strstart[2] ;
      *uchar = uchr ^ (((char32_t)0x80 << 6) | 0x80) ;
      return 3 ;

   case 240:
      // 4 byte sequence
      uchr   = ((uint32_t)(240 & 0x7) << 18) ;
      uchr  ^= (char32_t)strstart[1] << 12 ;
      uchr  ^= (char32_t)strstart[2] << 6 ;
      uchr  ^= strstart[3] ;
      *uchar = uchr ^ (((((char32_t)0x80 << 6) | 0x80) << 6) | 0x80) ;
      return 4 ;

   case 241: case 242: case 243:
      // 4 byte sequence
      uchr   = ((uint32_t)(firstbyte & 0x7) << 18) ;
      uchr  ^= (char32_t)strstart[1] << 12 ;
      uchr  ^= (char32_t)strstart[2] << 6 ;
      uchr  ^= strstart[3] ;
      *uchar = uchr ^ (((((char32_t)0x80 << 6) | 0x80) << 6) | 0x80) ;
      return 4 ;

   case 244:
      // 4 byte sequence
      uchr   = ((char32_t)(244 & 0x7) << 18) ;
      uchr  ^= (char32_t)strstart[1] << 12 ;
      uchr  ^= (char32_t)strstart[2] << 6 ;
      uchr  ^= strstart[3] ;
      *uchar = uchr ^ (((((char32_t)0x80 << 6) | 0x80) << 6) | 0x80) ;
      return 4 ;

   case 245: case 246: case 247:
   case 248: case 249: case 250: case 251: case 252: case 253: case 254: case 255:
      // not used for encoding to make it compatible with UTF-16
      goto ONERR;
   }

ONERR:
   return 0 ;  /*EILSEQ or not enough data*/
}

uint8_t encodechar_utf8(size_t strsize, /*out*/uint8_t strstart[strsize], char32_t uchar)
{
   if (uchar <= 0x7f) {
      if (!strsize) return 0 ;
      strstart[0] = (uint8_t) uchar ;
      return 1 ;
   } else if (uchar <= 0x7ff) {
      if (strsize < 2) return 0 ;
      strstart[0] = (uint8_t) (0xC0 | (uchar >> 6)) ;
      strstart[1] = (uint8_t) (0x80 | (uchar & 0x3f)) ;
      return 2 ;
   } else if (uchar <= 0xffff) {
      if (strsize < 3) return 0 ;
      strstart[0] = (uint8_t) (0xE0 | (uchar >> 12)) ;
      strstart[1] = (uint8_t) (0x80 | ((uchar >> 6) & 0x3f)) ;
      strstart[2] = (uint8_t) (0x80 | (uchar & 0x3f)) ;
      return 3 ;
   } else if (uchar <= 0x10ffff) {
      if (strsize < 4) return 0 ;
      strstart[0] = (uint8_t) (0xF0 | (uchar >> 18)) ;
      strstart[1] = (uint8_t) (0x80 | ((uchar >> 12) & 0x3f)) ;
      strstart[2] = (uint8_t) (0x80 | ((uchar >> 6) & 0x3f)) ;
      strstart[3] = (uint8_t) (0x80 | (uchar & 0x3f)) ;
      return 4 ;
   }

   return 0 ;
}

size_t length_utf8(const uint8_t * strstart, const uint8_t * strend)
{
   size_t len = 0 ;

   if (strstart < strend) {
      const uint8_t * next = strstart ;
      const uint8_t * end  = strend - maxsize_utf8() ;

      size_t bytelen = (size_t) (strend - strstart) ;
      if (bytelen >= maxsize_utf8()) {
         do {
            const unsigned sizechr = sizePfirst_utf8(*next) ;
            next += sizechr + (0 == sizechr) ;
            len  += (0 != sizechr) ;
         } while (next <= end) ;
      }

      while (next < strend) {
         const unsigned sizechr = sizePfirst_utf8(*next) ;
         if ((size_t)(strend-next) < sizechr) {
            next = strend ;
         } else {
            next += sizechr + (0 == sizechr) ;
         }
         len += (0 != sizechr) ;
      }
   }

   return len ;
}



// section: utf8validator_t

static size_t erroffset_utf8validator(const uint8_t errdata[])
{
   uint8_t nextbyte ;
   uint8_t firstbyte ;

   firstbyte = errdata[0] ;

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
      return 0 ;  // no error ?

   case 128: case 129: case 130: case 131: case 132: case 133: case 134: case 135: case 136: case 137:
   case 138: case 139: case 140: case 141: case 142: case 143: case 144: case 145: case 146: case 147:
   case 148: case 149: case 150: case 151: case 152: case 153: case 154: case 155: case 156: case 157:
   case 158: case 159: case 160: case 161: case 162: case 163: case 164: case 165: case 166: case 167:
   case 168: case 169: case 170: case 171: case 172: case 173: case 174: case 175: case 176: case 177:
   case 178: case 179: case 180: case 181: case 182: case 183: case 184: case 185: case 186: case 187:
   case 188: case 189: case 190: case 191:
      // used for encoding following bytes
      return 0 ;  // first byte wrong

   case 192: case 193:
      // not used in two byte sequence (value < 0x80)
      return 0 ;  // first byte wrong

   case 194: case 195: case 196: case 197: case 198: case 199: case 200: case 201:
   case 202: case 203: case 204: case 205: case 206: case 207: case 208: case 209: case 210: case 211:
   case 212: case 213: case 214: case 215: case 216: case 217: case 218: case 219: case 220: case 221:
   case 222: case 223:
      // 2 byte sequence
      return 1 ;  // assume error in second byte

   case 224:
      // 3 byte sequence
      nextbyte = errdata[1] ^ 0x80 ;
      if ((nextbyte & 0xC0) || nextbyte < 0x20) return 1 ;
      return 2 ;  // assume error in third byte

   case 225: case 226: case 227: case 228: case 229: case 230: case 231: case 232: case 233:
   case 234: case 235: case 236: case 237: case 238: case 239:
      // 3 byte sequence
      nextbyte = errdata[1] ^ 0x80 ;
      if ((nextbyte & 0xC0)) return 1 ;
      return 2 ;  // assume error in third byte

   case 240:
      // 4 byte sequence
      nextbyte = errdata[1] ^ 0x80 ;
      if ((nextbyte & 0xC0) || nextbyte < 0x10) return 1 ;
      nextbyte = errdata[2] ^ 0x80 ;
      if ((nextbyte & 0xC0)) return 2 ;
      return 3 ;  // assume error in fourth byte

   case 241: case 242: case 243:
      // 4 byte sequence
      nextbyte = errdata[1] ^ 0x80 ;
      if ((nextbyte & 0xC0)) return 1 ;
      nextbyte = errdata[2] ^ 0x80 ;
      if ((nextbyte & 0xC0)) return 2 ;
      return 3 ;  // assume error in fourth byte

   case 244:
      // 4 byte sequence
      nextbyte = errdata[1] ^ 0x80 ;
      if (nextbyte > 0x0f) return 1 ;
      nextbyte = errdata[2] ^ 0x80 ;
      if ((nextbyte & 0xC0)) return 2 ;
      return 3 ;  // assume error in fourth byte

   case 245: case 246: case 247:
   case 248: case 249: case 250: case 251: case 252: case 253: case 254: case 255:
      // not used for encoding to make it compatible with UTF-16
      return 0 ;  // first byte wrong
   }
}

int validate_utf8validator(utf8validator_t * utf8validator, size_t size, const uint8_t data[size], /*err*/size_t * erroffset)
{
   static_assert(sizeof(utf8validator->prefix) == maxsize_utf8(), "prefix contains at least 1 byte less") ;

   size_t            size_to_validate = size ;
   const uint8_t *   data_to_validate = data ;
   size_t            size_min         = 0 ;
   size_t            erroffset2 ;

   if (utf8validator->size_of_prefix) {
      // complete prefix and check it
      size_t size_needed  = sizePfirst_utf8(utf8validator->prefix[0]) ;
      size_t size_missing = size_needed - utf8validator->size_of_prefix ;

      if (size_to_validate == 0) return 0 ;  // nothing to do
      size_min = size_to_validate < size_missing ? size_to_validate : size_missing ;
      // size_min != 0

      memcpy(utf8validator->prefix + utf8validator->size_of_prefix, data, size_min) ;
      utf8validator->size_of_prefix = (uint8_t) (utf8validator->size_of_prefix + size_min) ;
      // if size_to_validate is too low then VALIDATE generates an error (utf8validator->prefix is filled with 0)
      size_to_validate = utf8validator->size_of_prefix ;
      data_to_validate = utf8validator->prefix ;
      goto VALIDATE ;
   }

   for (;;) {

      if (size_to_validate < maxsize_utf8()) {
         if (utf8validator->size_of_prefix) {
            size_to_validate = size - size_min ;
            data_to_validate = data + size_min ;
            size_min = 0 ;
            utf8validator->size_of_prefix = 0 ;
         }
         if (!size_to_validate) break ; // whole buffer done
         if (sizePfirst_utf8(data_to_validate[0]) > size_to_validate) {
            memset(utf8validator->prefix, 0, sizeof(utf8validator->prefix)) ; // ensures error
            memcpy(utf8validator->prefix, data_to_validate, size_to_validate) ;
            utf8validator->size_of_prefix = (uint8_t) size_to_validate ;
            // size_to_validate is not used cause VALIDATE generates an error (utf8validator->prefix is filled with 0)
            // size_min == 0
            data_to_validate = utf8validator->prefix ;
         }
      }

      // validate sequence

      uint8_t flags ;
      uint8_t firstbyte ;

VALIDATE:

      firstbyte = data_to_validate[0] ;

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
         data_to_validate += 1 ;
         size_to_validate -= 1 ;
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
         flags = data_to_validate[1] ^ 0x80 ;
         if ((flags & 0xC0)) goto ONERR;
         data_to_validate += 2 ;
         size_to_validate -= 2 ;
         continue;

      case 224:
         // 3 byte sequence
         flags = data_to_validate[1] ^ 0x80 ;
         if (flags < 0x20) goto ONERR; /*too low*/
         flags |= data_to_validate[2] ^ 0x80 ;
         if ((flags & 0xC0)) goto ONERR;
         data_to_validate += 3 ;
         size_to_validate -= 3 ;
         continue;

      case 225: case 226: case 227: case 228: case 229: case 230: case 231: case 232: case 233:
      case 234: case 235: case 236: case 237: case 238: case 239:
         // 3 byte sequence
         flags = data_to_validate[1] ^ 0x80 ;
         flags |= data_to_validate[2] ^ 0x80 ;
         if ((flags & 0xC0)) goto ONERR;
         data_to_validate += 3 ;
         size_to_validate -= 3 ;
         continue;

      case 240:
         // 4 byte sequence
         flags = data_to_validate[1] ^ 0x80 ;
         if (flags < 0x10) goto ONERR; /*too low*/
         flags |= data_to_validate[2] ^ 0x80 ;
         flags |= data_to_validate[3] ^ 0x80 ;
         if ((flags & 0xC0)) goto ONERR;
         data_to_validate += 4 ;
         size_to_validate -= 4 ;
         continue;

      case 241: case 242: case 243:
         // 4 byte sequence
         flags = data_to_validate[1] ^ 0x80 ;
         flags |= data_to_validate[2] ^ 0x80 ;
         flags |= data_to_validate[3] ^ 0x80 ;
         if ((flags & 0xC0)) goto ONERR;
         data_to_validate += 4 ;
         size_to_validate -= 4 ;
         continue;

      case 244:
         // 4 byte sequence
         flags = data_to_validate[1] ^ 0x80 ;
         if (flags > 0x0f) goto ONERR; /*too big*/
         flags |= data_to_validate[2] ^ 0x80 ;
         flags |= data_to_validate[3] ^ 0x80 ;
         if ((flags & 0xC0)) goto ONERR;
         data_to_validate += 4 ;
         size_to_validate -= 4 ;
         continue;

      case 245: case 246: case 247:
      case 248: case 249: case 250: case 251: case 252: case 253: case 254: case 255:
         // not used for encoding to make it compatible with UTF-16
         goto ONERR;
      }// switch (firstbyte)

   }// for(;;)

   return 0 ;
ONERR:
   erroffset2 = erroffset_utf8validator(data_to_validate) ;
   // check that not the missing part of prefix generated the error (filled with 0) !
   if (utf8validator->size_of_prefix) {
      // is prefix is valid ?
      if (utf8validator->size_of_prefix <= erroffset2) return 0 ; // it is
      if (size_min) {
         // completion of prefix is wrong
         data_to_validate = data - ((utf8validator->size_of_prefix - size_min)/*size of uncompleted prefix*/) ;
      } else {
         // last sequence is a bad prefix
         data_to_validate = data + size - utf8validator->size_of_prefix ;
      }
      utf8validator->size_of_prefix = 0 ;
   }
   // set err parameter
   if (erroffset) *erroffset = erroffset2 + (size_t) (data_to_validate - data) ;
   return EILSEQ ;
}



// section: stringstream_t

// group: read-utf8

int nextutf8_stringstream(struct stringstream_t * strstream, /*out*/char32_t * uchar)
{
   size_t   size = size_stringstream(strstream) ;
   uint8_t  len ;

   if (size < maxchar_utf8()) {
      /* all data decoded ? */
      if (!size) return ENODATA ;
      len = sizePfirst_utf8(strstream->next[0]) ;
      /* not enough data ? */
      if (size < len) return ENOTEMPTY ;
   }

   len = decodechar_utf8(strstream->next, uchar) ;
   if (!len) return EILSEQ ;

   utf8validator_t utf8validator = utf8validator_INIT ;
   int err = validate_utf8validator(&utf8validator, len, strstream->next, 0) ;
   if (err) return err ;

   strstream->next += len ;
   return 0 ;
}

void skipillegalutf8_strstream(struct stringstream_t * strstream)
{
   while (strstream->next < strstream->end) {
      uint8_t size = sizePfirst_utf8(*strstream->next) ;

      if (size) {
         char32_t uchar ;
         if (size > size_stringstream(strstream))
            size = (uint8_t) size_stringstream(strstream) ;
         else if (0 == peekutf8_stringstream(strstream, &uchar)) /*is next character valid ? */
            break ;
         strstream->next += size ; // skip wrong character

      } else {
         ++ strstream->next ; // skip wrong byte
      }
   }
}

// group: find-utf8

const uint8_t * findutf8_stringstream(const struct stringstream_t * strstream, char32_t uchar)
{
   const size_t   size = size_stringstream(strstream) ;
   const uint8_t  * found  ;
   uint8_t        utf8c[3] ;

   if (uchar < 0x80) {
      found = memchr(strstream->next, (int)uchar, size) ;

   } else if (uchar < 0x800) {
      //0b110xxxxx 0b10xxxxxx
      utf8c[0] = (uint8_t) (0x80 | (uchar & 0x3f)) ;
      uint32_t uc = uchar >> 6 ;
      uc   = (0xc0 | uc) ;
      for (found = memchr(strstream->next, (int)uc, size); found; found = memchr(found, (int)uc, (size_t)(strstream->end - found))) {
         if (utf8c[0] == found[1]) {
            break ;
         }
         found += 2 ;
      }

   } else if (uchar < 0x10000) {
      //0b1110xxxx 0b10xxxxxx 0b10xxxxxx
      utf8c[1] = (uint8_t) (0x80 | (uchar & 0x3f)) ;
      uint32_t uc = uchar >> 6 ;
      utf8c[0] = (uint8_t) (0x80 | (uc & 0x3f)) ;
      uc >>= 6 ;
      uc   = (0xe0 | uc) ;
      for (found = memchr(strstream->next, (int)uc, size); found; found = memchr(found, (int)uc, (size_t)(strstream->end - found))) {
         if (  utf8c[0] == found[1]
               && utf8c[1] == found[2]) {
            break ;
         }
         found += 3 ;
      }

   } else if (uchar <= 0x10FFFF) {
      //0b11110xxx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx
      utf8c[2] = (uint8_t) (0x80 | (uchar & 0x3f)) ;
      uint32_t uc = uchar >> 6 ;
      utf8c[1] = (uint8_t) (0x80 | (uc & 0x3f)) ;
      uc >>= 6 ;
      utf8c[0] = (uint8_t) (0x80 | (uc & 0x3f)) ;
      uc >>= 6 ;
      uc   = (0xf0 | uc) ;
      for (found = memchr(strstream->next, (int)uc, size); found; found = memchr(found, (int)uc, (size_t)(strstream->end - found))) {
         if (  utf8c[0] == found[1]
               && utf8c[1] == found[2]
               && utf8c[2] == found[3]) {
            break ;
         }
         found += 4 ;
      }
   } else {
      goto ONERR;
   }

   return found ;
ONERR:
   // IGNORE ERROR uc > 0x10FFFF
   return 0 ;
}

// group: test

#ifdef KONFIG_UNITTEST

static int test_utf8(void)
{
   char32_t                uchar ;
   const uint8_t *  const  utf8str[4] = {
      (const uint8_t *) "\U0010ffff",
      (const uint8_t *) "\U0000ffff",
      (const uint8_t *) "\u07ff",
      (const uint8_t *) "\x7f"
   } ;

   // TEST maxchar_utf8
   TEST(0x10ffff == maxchar_utf8()) ;

   // TEST maxsize_utf8
   TEST(4 == maxsize_utf8()) ;

   // TEST sizePfirst_utf8 of string
   for (unsigned i = 0; i < lengthof(utf8str); ++i) {
      TEST(4-i == sizePfirst_utf8(utf8str[i][0])) ;
   }

   // TEST sizePfirst_utf8 of all 256 first byte values
   for (unsigned i = 0; i < 256; ++i) {
      unsigned bpc = (i < 128) ;
      /* values between [128 .. 193] and [245..255] indicate an error (returned value is 0) */
      if (194 <= i && i <= 244) {
         bpc = 0 ;
         for (unsigned i2 = i; (i2 & 0x80); i2 <<= 1) {
            ++ bpc ;
         }
      }
      TEST(bpc == sizePfirst_utf8(i)) ;
      TEST(bpc == sizePfirst_utf8(256+i)) ;
      TEST(bpc == sizePfirst_utf8(1024+i)) ;
   }

   // TEST sizechar_utf8
   for (char32_t i = 0; i <= 0x7f; ++i) {
      TEST(1 == sizechar_utf8(i)) ;
   }
   for (char32_t i = 0x80; i <= 0x7ff; ++i) {
      TEST(2 == sizechar_utf8(i)) ;
   }
   for (char32_t i = 0x800; i <= 0xffff; ++i) {
      TEST(3 == sizechar_utf8(i)) ;
   }
   for (char32_t i = 0x10000; i <= 0x10ffff; ++i) {
      TEST(4 == sizechar_utf8(i)) ;
   }

   // TEST sizechar_utf8: no range check if too big
   TEST(4 == sizechar_utf8(0x7fffffff)) ;

   // TEST isvalidfirstbyte_utf8 of all 256 first byte values
   for (unsigned i = 0; i < 256; ++i) {
      /* values between [128 .. 193] and [245..255] indicate an error */
      bool isOK = (i < 128 || (194 <= i && i <= 244)) ;
      TEST(isOK == isvalidfirstbyte_utf8((uint8_t)i)) ;
   }

   // TEST isfirstbyte_utf8
   for (unsigned i = 0; i < 256; ++i) {
      /* values between [128 .. 191] are used to encode follow bytes */
      bool isOK = (i < 128 || 192 <= i) ;
      TEST(isOK == isfirstbyte_utf8((uint8_t)i)) ;
   }

   // TEST length_utf8
   const char * teststrings[] = { "\U0010FFFF\U00010000", "\u0800\u0999\uFFFF", "\u00A0\u00A1\u07FE\u07FF", "\x01\x02""abcde\x07e\x7f", "\U0010FFFF\uF999\u06FEY" } ;
   size_t       testlength[]  = { 2,                      3,                    4,                          9,                          4 } ;
   for (unsigned i = 0; i < lengthof(teststrings); ++i) {
      TEST(testlength[i] == length_utf8((const uint8_t*)teststrings[i], (const uint8_t*)teststrings[i]+strlen(teststrings[i]))) ;
   }

   // TEST length_utf8: empty string
   TEST(0 == length_utf8(utf8str[0], utf8str[0])) ;
   TEST(0 == length_utf8(0, 0)) ;
   TEST(0 == length_utf8((void*)(uintptr_t)-1, (void*)(uintptr_t)-1)) ;

   // TEST length_utf8: illegal sequence && last sequence not fully contained in string
   const char * teststrings2[] = { "\xFC\x80",  "b\xC2",                 "ab\xE0",  "abc\xF0" } ;
   size_t       testlength2[]  = { 0/*EILSEQ*/, 2/*last not contained*/, 3/*ditto*/, 4/*ditto*/ } ;
   for (unsigned i = 0; i < lengthof(teststrings2); ++i) {
      TEST(testlength2[i] == length_utf8((const uint8_t*)teststrings2[i], (const uint8_t*)teststrings2[i]+strlen(teststrings2[i]))) ;
   }

   // TEST decodechar_utf8, skipchar_utf8: whole range of first byte
   char32_t fillchar[] = { 0x0, 0x1, 0x3F } ;
   for (char32_t i = 0; i < 256; ++i) {
      for (unsigned fci = 0; fci < lengthof(fillchar); ++fci) {
         uint8_t  buffer[4] = { (uint8_t)i, (uint8_t)(0x80 + fillchar[fci]), (uint8_t)(0x80 + fillchar[fci]), (uint8_t)(0x80 + fillchar[fci]) } ;
         char32_t expect    = 0 ;
         switch (sizePfirst_utf8(i)) {
         case 0:
            expect = 0 ;
            break ;
         case 1:
            expect = i ;
            break ;
         case 2:
            expect = ((i & 0x1f) << 6) + fillchar[fci] ;
            break ;
         case 3:
            expect = ((i & 0xf) << 12) + (fillchar[fci] << 6) + fillchar[fci] ;
            break ;
         case 4:
            expect = ((i & 0x7) << 18) + (fillchar[fci] << 12) + (fillchar[fci] << 6) + fillchar[fci] ;
            break ;
         }
         if (0 == sizePfirst_utf8(i)) {
            TEST(0 == decodechar_utf8(buffer, &uchar)) ;
            TEST(0 == skipchar_utf8(buffer)) ;
         } else {
            TEST(sizePfirst_utf8(i) == decodechar_utf8(buffer, &uchar)) ;
            TEST(uchar == expect) ;
            TEST(sizePfirst_utf8(i) == skipchar_utf8(buffer)) ;
         }
      }
   }

   // TEST decodechar_utf8, skipchar_utf8: check that values too low and values too high are recognized
   struct {
      const uint8_t *   str ;
      char32_t          uchar ;
      uint8_t           offset ;
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
      // 0x110000 encoded in 4 bytes (too big but not checked), 0x10ffff works
      { (const uint8_t*)"\xF4\x90\x80\x80", 0x110000, 4 },   // 0x110000  0b11110100 0b10010000 0b10000000 0b10000000
      { (const uint8_t*)"\xF4\x8F\xBF\xBF", 0x10ffff, 4 },   // 0x10ffff  0b11110100 0b10001111 0b10111111 0b10111111
   } ;
   for (unsigned i = 0; i < lengthof(minmaxstr); ++i) {
      uchar = 0 ;
      TEST(minmaxstr[i].offset == decodechar_utf8(minmaxstr[i].str, &uchar)) ;
      TEST(minmaxstr[i].uchar  == uchar) ;
      TEST(minmaxstr[i].offset == skipchar_utf8(minmaxstr[i].str)) ;
   }

   // TEST encodechar_utf8
   uint8_t utf8buffer[4] ;
   for (char32_t i = 0; i <= 0x7f; ++i) {
      TEST(1 == encodechar_utf8(1, utf8buffer, i)) ;
      TEST(1 == decodechar_utf8(utf8buffer, &uchar)) ;
      TEST(i == uchar) ;
   }
   for (char32_t i = 0x80; i <= 0x7ff; ++i) {
      TEST(2 == encodechar_utf8(2, utf8buffer, i)) ;
      TEST(2 == decodechar_utf8(utf8buffer, &uchar)) ;
      TEST(i == uchar) ;
   }
   for (char32_t i = 0x800; i <= 0xffff; ++i) {
      TEST(3 == encodechar_utf8(3, utf8buffer, i)) ;
      TEST(3 == decodechar_utf8(utf8buffer, &uchar)) ;
      TEST(i == uchar) ;
   }
   for (char32_t i = 0x10000; i <= 0x10ffff; ++i) {
      TEST(4 == encodechar_utf8(4, utf8buffer, i)) ;
      TEST(4 == decodechar_utf8(utf8buffer, &uchar)) ;
      TEST(i == uchar) ;
   }

   // TEST encodechar_utf8: buffer too small
   TEST(0 == encodechar_utf8(0, utf8buffer, 0x7f)) ;
   TEST(0 == encodechar_utf8(1, utf8buffer, 0x7ff)) ;
   TEST(0 == encodechar_utf8(2, utf8buffer, 0xffff)) ;
   TEST(0 == encodechar_utf8(3, utf8buffer, 0x10ffff)) ;

   // TEST encodechar_utf8: out of range
   TEST(0 == encodechar_utf8(4, utf8buffer, 0x110000)) ;

   return 0 ;
ONERR:
   return EINVAL ;
}

static int test_utf8validator(void)
{
   utf8validator_t   utf8val = utf8validator_INIT ;
   size_t            erroffset ;
   uint8_t           buffer[256] ;

   // TEST utf8validator_INIT
   TEST(0 == utf8val.size_of_prefix) ;
   for (unsigned i = 0; i < lengthof(utf8val.prefix); ++i) {
      TEST(0 == utf8val.prefix[i]) ;
   }

   // TEST utf8validator_INIT
   memset(&utf8val, 255, sizeof(utf8val)) ;
   init_utf8validator(&utf8val) ;
   TEST(0 == utf8val.size_of_prefix) ;
   for (unsigned i = 0; i < lengthof(utf8val.prefix); ++i) {
      TEST(0 == utf8val.prefix[i]) ;
   }

   // TEST free_utf8validator
   memset(&utf8val, 255, sizeof(utf8val)) ;
   TEST(EILSEQ == free_utf8validator(&utf8val)) ;
   TEST(0 == utf8val.size_of_prefix) ;
   TEST(0 == free_utf8validator(&utf8val)) ;
   TEST(0 == utf8val.size_of_prefix) ;
   for (unsigned i = 0; i < lengthof(utf8val.prefix); ++i) {
      TEST(255 == utf8val.prefix[i]) ;
   }

   // TEST sizeprefix_utf8validator
   for (uint8_t i = 10; i > 0; ) {
      utf8val.size_of_prefix = --i ;
      TEST(i == sizeprefix_utf8validator(&utf8val)) ;
   }

   // TEST erroffset_utf8validator, validate_utf8validator: all first byte with invalid follow byte
   memset(buffer, 0, maxsize_utf8()) ;
   for (unsigned i = 0; i < 256; ++i) {
      buffer[0] = (uint8_t)i ;
      size_t erroff = erroffset_utf8validator(buffer) ;
      size_t expect = (i >= 128) && (0 != sizePfirst_utf8((uint8_t)i)) ;
      TEST(erroff == expect) ;
      if (i <= 127) {
         TEST(0 == validate_utf8validator(&utf8val, 1, buffer, 0))
      } else {
         erroffset = 2 ;
         TEST(EILSEQ == validate_utf8validator(&utf8val, 2, buffer, &erroffset)) ;
         TEST(expect == erroffset) ;
      }
      TEST(0 == sizeprefix_utf8validator(&utf8val)) ;
   }

   // TEST erroffset_utf8validator, validate_utf8validator: min values
   uint8_t mincharval[3][4] = {
      { 0xc2, 0x80, 0, 0 },               // value 0x80 as 0b110xxxxx 0b10xxxxxx
      { 0xe0, 0x80 + 0x20, 0x80, 0},      // value 0x800 as 0b1110xxxx 0b10xxxxxx 0b10xxxxxx
      { 0xf0, 0x80 + 0x10, 0x80, 0x80}    // value 0x10000 as 0b11110xxx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx
   } ;
   uint8_t lowcharval[3][4] = {
      { 0xc1, 0x80 + 0x3f, 0, 0 },                    // value 0x7f as 0b110xxxxx 0b10xxxxxx
      { 0xe0, 0x80 + 0x1f, 0x80 + 0x3f, 0},           // value 0x7ff as 0b1110xxxx 0b10xxxxxx 0b10xxxxxx
      { 0xf0, 0x80 + 0x0f, 0x80 + 0x3f, 0x80 + 0x3f}  // value 0xffff as 0b11110xxx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx
   } ;
   for (unsigned i = 0; i < lengthof(mincharval); ++i) {
      // returns last index as default in case of correct encoding
      TEST(i+1 == erroffset_utf8validator(mincharval[i])) ;
      // returns 0 (2 byte encoding) or 1 (3 or 4 byte encoding) as index
      TEST((i!=0) == erroffset_utf8validator(lowcharval[i])) ;
      TEST(0 == validate_utf8validator(&utf8val, 4, mincharval[i], 0))
      TEST(0 == sizeprefix_utf8validator(&utf8val)) ;
      erroffset = 2 ;
      TEST(EILSEQ == validate_utf8validator(&utf8val, 4, lowcharval[i], &erroffset))
      TEST(0 == sizeprefix_utf8validator(&utf8val)) ;
      TEST((i!=0) == erroffset) ;
   }

   // TEST erroffset_utf8validator, validate_utf8validator: max value
   uint8_t maxcharval[3][4] = {
      { 0xdf, 0x80+0x3f, 0, 0 },                // value 0x7FF as 0b110xxxxx 0b10xxxxxx
      { 0xef, 0x80+0x3f, 0x80+0x3f, 0},         // value 0xFFFF as 0b1110xxxx 0b10xxxxxx 0b10xxxxxx
      { 0xf4, 0x80+0x0f, 0x80+0x3f, 0x80+0x3f}  // value 0x10FFFF as 0b11110xxx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx
   } ;
   uint8_t highcharval[1][4] = {
      { 0xf4, 0x80+0x10, 0x80, 0x80}  // value 0x110000 as 0b11110xxx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx
   } ;
   for (unsigned i = 0; i < lengthof(maxcharval); ++i) {
      // returns last index as default in case of correct encoding
      TEST(i+1  == erroffset_utf8validator(maxcharval[i])) ;
      if (i == 2) {
         TEST(1 == erroffset_utf8validator(highcharval[0])) ;
      }
      TEST(0 == validate_utf8validator(&utf8val, 4, maxcharval[i], 0))
      TEST(0 == sizeprefix_utf8validator(&utf8val)) ;
      if (i == 2) {
         erroffset = 0 ;
         TEST(EILSEQ == validate_utf8validator(&utf8val, 4, lowcharval[i], &erroffset))
         TEST(0 == sizeprefix_utf8validator(&utf8val)) ;
         TEST(1 == erroffset) ;
      }
   }

   // TEST erroffset_utf8validator, validate_utf8validator: invalid follow byte
   for (unsigned i = 0; i < 256; ++i) {
      size_t mbslen = sizePfirst_utf8((uint8_t)i) ;
      if (mbslen < 2) continue ;
      buffer[0] = (uint8_t)i ;
      for (unsigned fi = 1; fi < mbslen; ++fi) {
         memset(buffer+1, 0x80+0x3f, mbslen-1) ;
         if (i == 0xf4) buffer[1] = 0x80+0x0f ;
         TEST(mbslen-1 == erroffset_utf8validator(buffer)) ; // (no error)
         TEST(0 == validate_utf8validator(&utf8val, mbslen, buffer, 0)) ;
         TEST(0 == sizeprefix_utf8validator(&utf8val)) ;
         buffer[fi] = (uint8_t) (buffer[fi] ^ 0x80) ;
         TEST(fi == erroffset_utf8validator(buffer)) ;
         TEST(EILSEQ == validate_utf8validator(&utf8val, mbslen, buffer, &erroffset)) ;
         TEST(0 == sizeprefix_utf8validator(&utf8val)) ;
         TEST(fi == erroffset) ;
         buffer[fi] = (uint8_t) (buffer[fi] | 0x40) ;
         TEST(fi == erroffset_utf8validator(buffer)) ;
         TEST(EILSEQ == validate_utf8validator(&utf8val, mbslen, buffer, &erroffset)) ;
         TEST(0 == sizeprefix_utf8validator(&utf8val)) ;
         TEST(fi == erroffset) ;
         buffer[fi] = (uint8_t) (buffer[fi] | 0x80) ;
         TEST(fi == erroffset_utf8validator(buffer)) ;
         TEST(EILSEQ == validate_utf8validator(&utf8val, mbslen, buffer, &erroffset)) ;
         TEST(0 == sizeprefix_utf8validator(&utf8val)) ;
         TEST(fi == erroffset) ;
      }
   }

   // TEST validate_utf8validator: no sequence crosses buffers
   for (char32_t i = 0; i < 4; ++i) {
      char32_t chr = i == 0 ? 0 : i == 1 ? 0x80 : i == 2 ? 0x800 : 0x10000 ;
      unsigned offset ;
      for (offset = 0; offset < sizeof(buffer)-maxsize_utf8(); ++chr) {
         offset += encodechar_utf8(maxsize_utf8(), buffer+offset, chr) ;
      }
      memset(buffer+offset, 0, sizeof(buffer)-offset) ;
      TEST(0 == validate_utf8validator(&utf8val, sizeof(buffer), buffer, 0))
      TEST(0 == sizeprefix_utf8validator(&utf8val)) ;
      for (offset = 0; offset < sizeof(buffer); ++offset) {
         uint8_t old = buffer[offset] ;
         buffer[offset] = (uint8_t) (isfirstbyte_utf8(buffer[offset]) ? 0x80 : old^0x80) ;
         TEST(EILSEQ == validate_utf8validator(&utf8val, sizeof(buffer), buffer, &erroffset)) ;
         TEST(offset == erroffset) ;
         TEST(0 == sizeprefix_utf8validator(&utf8val)) ;
         buffer[offset] = old ;
      }
   }

   // TEST validate_utf8validator: last sequence crosses buffer (test correct prefix)
   for (unsigned mbslen = 2; mbslen <= maxsize_utf8(); ++mbslen) {
      char32_t chr  = mbslen == 2 ? 0x80 : mbslen == 3 ? 0x800 : 0x10000 ;
      char32_t step = chr ;
      for (unsigned i = 0; i < 5; ++i, chr += step) {
         TEST(mbslen == encodechar_utf8(maxsize_utf8(), buffer+i*mbslen, chr)) ;
      }
      for (unsigned buflen = 4*mbslen+1; buflen < 5*mbslen; ++buflen) {
         size_t P = buflen - 4*mbslen ;
         TEST(0 == validate_utf8validator(&utf8val, buflen, buffer, &erroffset)) ;
         TEST(P == sizeprefix_utf8validator(&utf8val)) ;
         for (unsigned i = 0; i < sizeof(utf8val.prefix); ++i) {
            TEST((i < P ? buffer[4*mbslen+i] : 0) == utf8val.prefix[i]) ;
         }
         // error case (no prefix stored)
         for (unsigned i = 0; i < buflen; ++i) {
            init_utf8validator(&utf8val) ;
            uint8_t old = buffer[i] ;
            buffer[i] = (uint8_t) (isfirstbyte_utf8(buffer[i]) ? 0x80 : old^0x80) ;
            TEST(EILSEQ == validate_utf8validator(&utf8val, buflen, buffer, &erroffset)) ;
            TEST(0 == sizeprefix_utf8validator(&utf8val)) ;
            TEST(i == erroffset) ;
            buffer[i] = old ;
         }
      }
   }

   // TEST validate_utf8validator: generate prefix, continue with next buffer
   for (unsigned mbslen = 2; mbslen <= maxsize_utf8(); ++mbslen) {
      char32_t chr  = mbslen == 2 ? 0x100 : mbslen == 3 ? 0x3800 : 0x50000 ;
      char32_t step = chr >> 4 ;
      for (unsigned i = 0; i < 5; ++i, chr += step) {
         TEST(mbslen == encodechar_utf8(maxsize_utf8(), buffer+i*mbslen, chr)) ;
      }
      for (uint8_t P = 1; P < mbslen; ++P) {
         init_utf8validator(&utf8val) ;
         utf8val.size_of_prefix = P ;
         memcpy(utf8val.prefix, buffer, P) ;
         // buffer of length 0 is ignored
         TEST(0 == validate_utf8validator(&utf8val, 0, buffer, &erroffset)) ;
         TEST(P == sizeprefix_utf8validator(&utf8val)) ;
         // continue
         TEST(0 == validate_utf8validator(&utf8val, 5*mbslen-P, buffer+P, &erroffset)) ;
         TEST(0 == sizeprefix_utf8validator(&utf8val)) ;
         // error case
         for (unsigned i = P; i < 5*mbslen; ++i) {
            init_utf8validator(&utf8val) ;
            utf8val.size_of_prefix = P ;
            memcpy(utf8val.prefix, buffer, P) ;
            uint8_t old = buffer[i] ;
            buffer[i] = (uint8_t) (isfirstbyte_utf8(buffer[i]) ? 0x80 : old^0x80) ;
            TEST(EILSEQ == validate_utf8validator(&utf8val, 5*mbslen-P, buffer+P, &erroffset)) ;
            TEST(0 == sizeprefix_utf8validator(&utf8val)) ;
            TEST(i-P == erroffset) ;
            buffer[i] = old ;
         }
      }
   }

   // TEST validate_utf8validator: generate prefix, continue with next buffer which also ends in prefix (test correct prefix)
   for (unsigned mbslen = 2; mbslen <= maxsize_utf8(); ++mbslen) {
      char32_t chr  = mbslen == 2 ? 0x300 : mbslen == 3 ? 0x1080 : 0x30500 ;
      char32_t step = chr >> 4 ;
      for (unsigned i = 0; i < 7; ++i, chr += step) {
         TEST(mbslen == encodechar_utf8(maxsize_utf8(), buffer+i*mbslen, chr)) ;
      }
      for (uint8_t P = 1; P < mbslen; ++P) {
         for (uint8_t E = 1; E < mbslen; ++E) {
            init_utf8validator(&utf8val) ;
            utf8val.size_of_prefix = P ;
            memcpy(utf8val.prefix, buffer, P) ;
            // continue
            TEST(0 == validate_utf8validator(&utf8val, 6*mbslen-P+E, buffer+P, &erroffset)) ;
            TEST(E == sizeprefix_utf8validator(&utf8val)) ;
            for (unsigned i = 0; i < sizeof(utf8val.prefix); ++i) {
               TEST((i < E ? buffer[6*mbslen+i] : 0) == utf8val.prefix[i]) ;
            }
            // error case
            for (unsigned i = P; i < 6*mbslen+E; ++i) {
               init_utf8validator(&utf8val) ;
               utf8val.size_of_prefix = P ;
               memcpy(utf8val.prefix, buffer, P) ;
               uint8_t old = buffer[i] ;
               buffer[i] = (uint8_t) (isfirstbyte_utf8(buffer[i]) ? 0x80 : old^0x80) ;
               TEST(EILSEQ == validate_utf8validator(&utf8val, 6*mbslen-P+E, buffer+P, &erroffset)) ;
               TEST(0 == sizeprefix_utf8validator(&utf8val)) ;
               TEST(i-P == erroffset) ;
               buffer[i] = old ;
            }
         }
      }
   }

   return 0 ;
ONERR:
   return EINVAL ;
}

static int test_readstrstream(void)
{
   stringstream_t strstream ;
   uint32_t       uchar       = 0 ;
   uint32_t       uchar2      = 0 ;

   // TEST nextutf8_stringstream, peekutf8_stringstream, skiputf8_stringstream: all characters
   for (char32_t i = 0; i < maxchar_utf8(); ++i) {
      uint8_t buffer[maxsize_utf8()] ;
      uint8_t len = encodechar_utf8(maxsize_utf8(), buffer, i) ;
      TEST(0 == init_stringstream(&strstream, buffer, buffer+maxsize_utf8())) ;
      TEST(0 == peekutf8_stringstream(&strstream, &uchar)) ;
      TEST(i == uchar) ;
      TEST(strstream.next == buffer) ;
      TEST(strstream.end  == buffer+maxsize_utf8()) ;
      --uchar ;
      TEST(0 == nextutf8_stringstream(&strstream, &uchar)) ;
      TEST(i == uchar) ;
      TEST(strstream.next == buffer+len) ;
      TEST(strstream.end  == buffer+maxsize_utf8()) ;
      strstream.next = buffer ;
      TEST(0 == skiputf8_stringstream(&strstream)) ;
      TEST(strstream.next == buffer+len) ;
      TEST(strstream.end  == buffer+maxsize_utf8()) ;
   }

   // TEST nextutf8_stringstream, peekutf8_stringstream, skiputf8_stringstream: check that values too low and values too high are recognized
   struct {
      const uint8_t *   str ;
      char32_t          uchr ;
      uint8_t           offset ;
   } minmaxstr[] = {
      // 0x7f encoded in 2 bytes (too small), 0x80 and 0x7ff work
      { (const uint8_t*)"\xC1\xBF", 0,     0 }, // 0x7f   0b11000001 0b10111111
      { (const uint8_t*)"\xC2\x80", 0x80,  2 }, // 0x80   0b11000010 0b10000000
      { (const uint8_t*)"\xDF\xBF", 0x7ff, 2 }, // 0x7ff  0b11011111 0b10111111
      // 0x7ff encoded in 3 bytes (too small), 0x800 and 0xffff work
      { (const uint8_t*)"\xE0\x9F\xBF", 0,      0 },  // 0x7ff   0b11100000 0b10011111 0b10111111
      { (const uint8_t*)"\xE0\xA0\x80", 0x800,  3 },  // 0x800   0b11100000 0b10100000 0b10000000
      { (const uint8_t*)"\xEF\xBF\xBF", 0xffff, 3 },  // 0xffff  0b11101111 0b10111111 0b10111111
      // 0xffff encoded in 4 bytes (too small), 0x10000 works
      { (const uint8_t*)"\xF0\x8F\xBF\xBF", 0,       0 },   // 0xffff   0b11110000 0b10001111 0b10111111 0b10111111
      { (const uint8_t*)"\xF0\x90\x80\x80", 0x10000, 4 },   // 0x10000  0b11110000 0b10010000 0b10000000 0b10000000
      // 0x110000 encoded in 4 bytes (too big), 0x10ffff works
      { (const uint8_t*)"\xF4\x90\x80\x80", 0,        0 },   // 0x110000  0b11110100 0b10010000 0b10000000 0b10000000
      { (const uint8_t*)"\xF4\x8F\xBF\xBF", 0x10ffff, 4 },   // 0x10ffff  0b11110100 0b10001111 0b10111111 0b10111111
   } ;
   for (unsigned i = 0; i < lengthof(minmaxstr); ++i) {
      TEST(0 == init_stringstream(&strstream, minmaxstr[i].str, minmaxstr[i].str+4)) ;
      if (minmaxstr[i].uchr) {
         TEST(0 == peekutf8_stringstream(&strstream, &uchar2)) ;
         TEST(uchar2 == minmaxstr[i].uchr) ;
      } else {
         TEST(EILSEQ == peekutf8_stringstream(&strstream, &uchar2)) ;
      }
      TEST(strstream.next == minmaxstr[i].str) ;
      TEST(strstream.end  == minmaxstr[i].str+4) ;
      if (minmaxstr[i].uchr) {
         TEST(0 == nextutf8_stringstream(&strstream, &uchar)) ;
         TEST(uchar == minmaxstr[i].uchr) ;
      } else {
         TEST(EILSEQ == nextutf8_stringstream(&strstream, &uchar)) ;
      }
      TEST(strstream.next == minmaxstr[i].str+minmaxstr[i].offset) ;
      TEST(strstream.end  == minmaxstr[i].str+4) ;
      strstream.next = minmaxstr[i].str ;
      if (minmaxstr[i].uchr) {
         TEST(0 == skiputf8_stringstream(&strstream)) ;
      } else {
         TEST(EILSEQ == skiputf8_stringstream(&strstream)) ;
      }
      TEST(strstream.next == minmaxstr[i].str+minmaxstr[i].offset) ;
      TEST(strstream.end  == minmaxstr[i].str+4) ;
   }

   // TEST nextutf8_stringstream, peekutf8_stringstream, skiputf8_stringstream: ENODATA
   const uint8_t * empty = (const uint8_t *) "" ;
   TEST(0 == init_stringstream(&strstream, empty, empty)) ;
   TEST(ENODATA == nextutf8_stringstream(&strstream, &uchar)) ;
   TEST(ENODATA == peekutf8_stringstream(&strstream, &uchar)) ;
   TEST(ENODATA == skiputf8_stringstream(&strstream)) ;
   TEST(strstream.next == empty) ;
   TEST(strstream.end  == empty) ;

   // TEST nextutf8_stringstream, peekutf8_stringstream, skiputf8_stringstream: whole range of first byte, ENOTEMPTY, EILSEQ
   char32_t fillchar[] = { 0x0, 0x1, 0x3F } ;
   for (char32_t i = 0; i < 256; ++i) {
      bool is_check_follow_bytes = false ;
      for (unsigned fci = 0; fci < lengthof(fillchar); ++fci) {
         uint8_t  buffer[4] = { (uint8_t)i, (uint8_t)(0x80 + fillchar[fci]), (uint8_t)(0x80 + fillchar[fci]), (uint8_t)(0x80 + fillchar[fci]) } ;
         TEST(0 == init_stringstream(&strstream, buffer, buffer+4)) ;
         char32_t expect = 0 ;
         switch (sizePfirst_utf8(i)) {
         case 0:
            expect = 0 ;
            break ;
         case 1:
            expect = i ;
            break ;
         case 2:
            expect = ((i & 0x1f) << 6) + fillchar[fci] ;
            expect = (expect < 0x80) ? 0 : expect ;
            break ;
         case 3:
            expect = ((i & 0xf) << 12) + (fillchar[fci] << 6) + fillchar[fci] ;
            expect = (expect < 0x800) ? 0 : expect ;
            break ;
         case 4:
            expect = ((i & 0x7) << 18) + (fillchar[fci] << 12) + (fillchar[fci] << 6) + fillchar[fci] ;
            expect = (expect < 0x10000 || expect > 0x10FFFF) ? 0 : expect ;
            break ;
         }
         if (i && !expect) {
            TEST(EILSEQ == peekutf8_stringstream(&strstream, &uchar)) ;
            TEST(strstream.next == buffer) ;
            TEST(EILSEQ == nextutf8_stringstream(&strstream, &uchar)) ;
            TEST(strstream.next == buffer) ;
            TEST(EILSEQ == skiputf8_stringstream(&strstream)) ;
            TEST(strstream.next == buffer) ;
         } else {
            uchar = expect + 1 ;
            TEST(0 == peekutf8_stringstream(&strstream, &uchar)) ;
            TEST(uchar == expect) ;
            TEST(strstream.next == buffer) ;
            uchar = expect + 1 ;
            TEST(0 == nextutf8_stringstream(&strstream, &uchar)) ;
            TEST(uchar == expect) ;
            TEST(strstream.next == buffer+sizePfirst_utf8(i)) ;
            strstream.next = buffer ;
            TEST(0 == skiputf8_stringstream(&strstream)) ;
            TEST(strstream.next == buffer+sizePfirst_utf8(i)) ;
         }
         TEST(strstream.end == buffer+4) ;
         if (sizePfirst_utf8(i) > 1) {
            TEST(0 == init_stringstream(&strstream, buffer, buffer + sizePfirst_utf8(i)-1)) ;
            TEST(ENOTEMPTY == peekutf8_stringstream(&strstream, &uchar)) ;
            TEST(strstream.next == buffer) ;
            TEST(ENOTEMPTY == nextutf8_stringstream(&strstream, &uchar)) ;
            TEST(strstream.next == buffer) ;
            TEST(ENOTEMPTY == skiputf8_stringstream(&strstream)) ;
            TEST(strstream.next == buffer) ;
            TEST(strstream.end  == buffer+sizePfirst_utf8(i)-1) ;
         }
         if (!i || expect) {
            is_check_follow_bytes = true ;
            for (int i2 = sizePfirst_utf8(i)-1; i2 > 0; --i2) {
               TEST(0 == init_stringstream(&strstream, buffer, buffer+4)) ;
               buffer[i2] = (uint8_t)(fillchar[fci]) ;
               TEST(EILSEQ == peekutf8_stringstream(&strstream, &uchar)/*illegal fill byte*/) ;
               TEST(EILSEQ == nextutf8_stringstream(&strstream, &uchar)/*illegal fill byte*/) ;
               TEST(EILSEQ == skiputf8_stringstream(&strstream        )/*illegal fill byte*/) ;
               buffer[i2] = (uint8_t)(0x40 + fillchar[fci]) ;
               TEST(EILSEQ == peekutf8_stringstream(&strstream, &uchar)/*illegal fill byte*/) ;
               TEST(EILSEQ == nextutf8_stringstream(&strstream, &uchar)/*illegal fill byte*/) ;
               TEST(EILSEQ == skiputf8_stringstream(&strstream        )/*illegal fill byte*/) ;
               buffer[i2] = (uint8_t)(0xc0 + fillchar[fci]) ;
               TEST(EILSEQ == peekutf8_stringstream(&strstream, &uchar)/*illegal fill byte*/) ;
               TEST(EILSEQ == nextutf8_stringstream(&strstream, &uchar)/*illegal fill byte*/) ;
               TEST(EILSEQ == skiputf8_stringstream(&strstream        )/*illegal fill byte*/) ;
               buffer[i2] = (uint8_t)(0x80 + fillchar[fci]) ;
               TEST(0 == peekutf8_stringstream(&strstream, &uchar)/*OK*/) ;
               TEST(strstream.next == buffer) ;
               TEST(strstream.end  == buffer+4) ;
            }
         }
      }
      TEST(sizePfirst_utf8(i) == 0 || is_check_follow_bytes) ;
   }

   // TEST skipillegalutf8_strstream: first byte encoded illegal
   const size_t   utf8strsize = 8+6+4+2 ;
   const uint8_t* utf8str = (const uint8_t *) (
                           "\U0010ffff" "\U00010000"
                           "\U0000ffff" "\U00000800"
                           "\u07ff"     "\xC2\x80"
                           "\x7f"       "\x00") ;
   uint8_t illseq[256] ;
   TEST(sizeof(illseq) > utf8strsize) ;
   memcpy(illseq, utf8str, utf8strsize) ;
   illseq[0] = 0x80 ;
   illseq[4] = 0x80 ;
   illseq[8] = 0x80 ;
   illseq[11] = 0x80 ;
   illseq[14] = 0x80 ;
   illseq[16] = 0x80 ;
   TEST(0 == init_stringstream(&strstream, illseq, illseq+utf8strsize)) ;
   skipillegalutf8_strstream(&strstream) ;
   TEST(strstream.next == illseq+utf8strsize-2) ;
   TEST(0 == nextutf8_stringstream(&strstream, &uchar)) ;
   TEST(0x7f == uchar) ;
   TEST(0 == nextutf8_stringstream(&strstream, &uchar)) ;
   TEST(0 == uchar) ;
   TEST(ENODATA == nextutf8_stringstream(&strstream, &uchar)) ;

   // TEST skipillegalutf8_strstream: follow byte encoded illegal
   memcpy(illseq, utf8str, utf8strsize) ;
   illseq[1] = 0xFF ;
   illseq[5] = 0xFF ;
   illseq[9] = 0xFF ;
   illseq[12] = 0xFF;
   illseq[15] = 0xFF ;
   illseq[17] = 0xFF ;
   TEST(0 == init_stringstream(&strstream, illseq, illseq+utf8strsize)) ;
   skipillegalutf8_strstream(&strstream) ;
   TEST(strstream.next == illseq+utf8strsize-2) ;

   // TEST skipillegalutf8_strstream: last not fully encoded byte is also skipped
   memcpy(illseq, utf8str, utf8strsize) ;
   TEST(0 == init_stringstream(&strstream, illseq, illseq+7)) ;
   illseq[1] = 0xFF ;
   skipillegalutf8_strstream(&strstream) ;
   TEST(ENODATA == nextutf8_stringstream(&strstream, &uchar)) ;
   TEST(strstream.next == illseq+7) ;

   return 0 ;
ONERR:
   return EINVAL ;
}

static int test_findstrstream(void)
{
   const char     * utf8str = "\U0010fff0\U0010ffff abcd\U0000fff0\U0000ffff abcd\u07f0\u07ff abcd\x7f abcd" ;
   stringstream_t strstream = stringstream_INIT((const uint8_t*)utf8str, (const uint8_t*)utf8str+strlen(utf8str)) ;
   const uint8_t  * found ;

   // TEST findutf8_stringstream
   found = findutf8_stringstream(&strstream, 0x10ffffu) ;
   TEST(found ==  4+(const uint8_t*)utf8str) ;
   found = findutf8_stringstream(&strstream, 0xffffu) ;
   TEST(found == 16+(const uint8_t*)utf8str) ;
   found = findutf8_stringstream(&strstream, 0x7ffu) ;
   TEST(found == 26+(const uint8_t*)utf8str) ;
   found = findutf8_stringstream(&strstream, 0x7fu) ;
   TEST(found == 33+(const uint8_t*)utf8str) ;
   found = findutf8_stringstream(&strstream, (char32_t) 'a') ;
   TEST(found == 9+(const uint8_t*)utf8str) ;

   // TEST findutf8_stringstream: character not in stream
   TEST(0 == findutf8_stringstream(&strstream, (char32_t) 0x10fffe)) ;
   TEST(0 == findutf8_stringstream(&strstream, (char32_t) 0xfffe)) ;
   TEST(0 == findutf8_stringstream(&strstream, (char32_t) 0x7fe)) ;
   TEST(0 == findutf8_stringstream(&strstream, (char32_t) 0x7e)) ;

   // TEST findutf8_stringstream: codepoint out of range
   TEST(0 == findutf8_stringstream(&strstream, (char32_t)-1)) ;

   return 0 ;
ONERR:
   return EINVAL ;
}

static int test_speed(void)
{
   systimer_t  timer = systimer_FREE ;
   uint64_t    microsec ;
   uint64_t    result[2]  = { UINT64_MAX, UINT64_MAX } ;
   union {
      uint32_t initval ;
      uint32_t buffer32[1024/4];
      uint8_t  buffer[1024];
   }           data       = { .buffer = "\U0010FFFF" } ;

   // prepare
   TEST(0 == init_systimer(&timer, sysclock_MONOTONIC)) ;

   for (size_t i = 1; i < lengthof(data.buffer32); ++i) {
      data.buffer32[i] = data.initval;
   }

   for (unsigned testrepeat = 0 ; testrepeat < 5; ++testrepeat) {
      TEST(0 == startinterval_systimer(timer, &(timevalue_t) { .nanosec = 1000 } )) ;
      for (unsigned decoderepeat = 0 ; decoderepeat < 5; ++decoderepeat) {
         for (unsigned nrchars = 0 ; nrchars < sizeof(data.buffer)/4; ++nrchars) {
            char32_t uchar = 0 ;
            TEST(4 == decodechar_utf8(data.buffer + 4 * nrchars, &uchar)) ;
            TEST(uchar == 0x10FFFF) ;
         }
      }
      TEST(0 == expirationcount_systimer(timer, &microsec)) ;
      TEST(0 == stop_systimer(timer)) ;
      if (result[0] > microsec) result[0] = microsec ;
   }

   for (unsigned testrepeat = 0 ; testrepeat < 5; ++testrepeat) {
      TEST(0 == startinterval_systimer(timer, &(timevalue_t) { .nanosec = 1000 } )) ;
      for (unsigned decoderepeat = 0 ; decoderepeat < 5; ++decoderepeat) {
         stringstream_t strstream = stringstream_INIT(data.buffer, data.buffer + sizeof(data.buffer)) ;
         for (unsigned nrchars = 0 ; nrchars < sizeof(data.buffer)/4; ++nrchars) {
            char32_t uchar = 0 ;
            TEST(0 == nextutf8_stringstream(&strstream, &uchar)) ;
            TEST(uchar == 0x10FFFF) ;
         }
         TEST(strstream.next == strstream.end) ;
      }
      TEST(0 == expirationcount_systimer(timer, &microsec)) ;
      TEST(0 == stop_systimer(timer)) ;
      if (result[1] > microsec) result[1] = microsec ;
   }

   if (result[1] <= result[0]) {
      logwarning_unittest("decode_utf8 is not faster") ;
   }

   TEST(0 == free_systimer(&timer)) ;

   return 0 ;
ONERR:
   free_systimer(&timer) ;
   return EINVAL ;
}

int unittest_string_utf8()
{
   if (test_utf8())           goto ONERR;
   if (test_utf8validator())  goto ONERR;
   if (test_readstrstream())  goto ONERR;
   if (test_findstrstream())  goto ONERR;
   if (test_speed())          goto ONERR;

   return 0 ;
ONERR:
   return EINVAL ;
}
#endif
