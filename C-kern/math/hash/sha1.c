/* title: SHA-1-Hashcode impl
   Implements <SHA-1-Hashcode>.

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

   file: C-kern/api/math/hash/sha1.h
    Header file <SHA-1-Hashcode>.

   file: C-kern/math/hash/sha1.c
    Implementation file <SHA-1-Hashcode impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/math/hash/sha1.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


#define ROTATE_LEFT(v,nr)              (((v) << nr) | ((v) >> (32-nr)))

#define STEP(i,a,b,c,d,e,f,k)                      \
   read_value(i) ;                                 \
   e += ROTATE_LEFT(a,5) + f(b,c,d) + k + value ;  \
   b  = ROTATE_LEFT(b,30) ;

#define F1(b,c,d)                      (d ^ (b & (c ^ d)))
#define F2(b,c,d)                      (b ^ c ^ d)
#define F3(b,c,d)                      ((b & c) | (d & (b | c)))
#define F4(b,c,d)                      (b ^ c ^ d)

#define K1                             0x5A827999
#define K2                             0x6ED9EBA1
#define K3                             0x8F1BBCDC
#define K4                             0xCA62C1D6

/* function: update_sha1hash
 * Implements calculating hash of 1 512 bit block.
 * See <http://de.wikipedia.org/wiki/Sha1#SHA-1-Pseudocode>. */
static void update_sha1hash(sha1_hash_t * sha1, const uint8_t block[64])
{
   const uint8_t  * next = block ;
   uint32_t       * w    = (uint32_t*) sha1->block ;
   uint32_t       value ;
   uint32_t       a, b, c, d, e ;

   a = sha1->h[0] ;
   b = sha1->h[1] ;
   c = sha1->h[2] ;
   d = sha1->h[3] ;
   e = sha1->h[4] ;

   if ((uintptr_t)block & 0x3) {

#define read_value(i)                     \
      value  = *(next++) ; value <<= 8 ;  \
      value |= *(next++) ; value <<= 8 ;  \
      value |= *(next++) ; value <<= 8 ;  \
      value |= *(next++) ; *(w++) = value ;

      STEP( 0,a,b,c,d,e,F1,K1)
      STEP( 1,e,a,b,c,d,F1,K1)
      STEP( 2,d,e,a,b,c,F1,K1)
      STEP( 3,c,d,e,a,b,F1,K1)
      STEP( 4,b,c,d,e,a,F1,K1)
      STEP( 5,a,b,c,d,e,F1,K1)
      STEP( 6,e,a,b,c,d,F1,K1)
      STEP( 7,d,e,a,b,c,F1,K1)
      STEP( 8,c,d,e,a,b,F1,K1)
      STEP( 9,b,c,d,e,a,F1,K1)
      STEP(10,a,b,c,d,e,F1,K1)
      STEP(11,e,a,b,c,d,F1,K1)
      STEP(12,d,e,a,b,c,F1,K1)
      STEP(13,c,d,e,a,b,F1,K1)
      STEP(14,b,c,d,e,a,F1,K1)
      STEP(15,a,b,c,d,e,F1,K1)

#undef read_value

   } else {

#define read_value(i)                            \
      value  = ntohl(* (const uint32_t*) next) ; \
      next  += 4 ;                               \
      *(w++) = value ;

      STEP( 0,a,b,c,d,e,F1,K1)
      STEP( 1,e,a,b,c,d,F1,K1)
      STEP( 2,d,e,a,b,c,F1,K1)
      STEP( 3,c,d,e,a,b,F1,K1)
      STEP( 4,b,c,d,e,a,F1,K1)
      STEP( 5,a,b,c,d,e,F1,K1)
      STEP( 6,e,a,b,c,d,F1,K1)
      STEP( 7,d,e,a,b,c,F1,K1)
      STEP( 8,c,d,e,a,b,F1,K1)
      STEP( 9,b,c,d,e,a,F1,K1)
      STEP(10,a,b,c,d,e,F1,K1)
      STEP(11,e,a,b,c,d,F1,K1)
      STEP(12,d,e,a,b,c,F1,K1)
      STEP(13,c,d,e,a,b,F1,K1)
      STEP(14,b,c,d,e,a,F1,K1)
      STEP(15,a,b,c,d,e,F1,K1)

#undef read_value

   }

   w = (uint32_t*) sha1->block ;

#define read_value(i)               \
   value  = w[(i-16)&0xf] ;         \
   value ^= w[(i-14)&0xf] ;         \
   value ^= w[(i-8)&0xf] ;          \
   value ^= w[(i-3)&0xf] ;          \
   value  = ROTATE_LEFT(value,1) ;  \
   w[i&0xf] = value ;


   STEP(16,e,a,b,c,d,F1,K1)
   STEP(17,d,e,a,b,c,F1,K1)
   STEP(18,c,d,e,a,b,F1,K1)
   STEP(19,b,c,d,e,a,F1,K1)

   STEP(20,a,b,c,d,e,F2,K2)
   STEP(21,e,a,b,c,d,F2,K2)
   STEP(22,d,e,a,b,c,F2,K2)
   STEP(23,c,d,e,a,b,F2,K2)
   STEP(24,b,c,d,e,a,F2,K2)
   STEP(25,a,b,c,d,e,F2,K2)
   STEP(26,e,a,b,c,d,F2,K2)
   STEP(27,d,e,a,b,c,F2,K2)
   STEP(28,c,d,e,a,b,F2,K2)
   STEP(29,b,c,d,e,a,F2,K2)
   STEP(30,a,b,c,d,e,F2,K2)
   STEP(31,e,a,b,c,d,F2,K2)
   STEP(32,d,e,a,b,c,F2,K2)
   STEP(33,c,d,e,a,b,F2,K2)
   STEP(34,b,c,d,e,a,F2,K2)
   STEP(35,a,b,c,d,e,F2,K2)
   STEP(36,e,a,b,c,d,F2,K2)
   STEP(37,d,e,a,b,c,F2,K2)
   STEP(38,c,d,e,a,b,F2,K2)
   STEP(39,b,c,d,e,a,F2,K2)

   STEP(40,a,b,c,d,e,F3,K3)
   STEP(41,e,a,b,c,d,F3,K3)
   STEP(42,d,e,a,b,c,F3,K3)
   STEP(43,c,d,e,a,b,F3,K3)
   STEP(44,b,c,d,e,a,F3,K3)
   STEP(45,a,b,c,d,e,F3,K3)
   STEP(46,e,a,b,c,d,F3,K3)
   STEP(47,d,e,a,b,c,F3,K3)
   STEP(48,c,d,e,a,b,F3,K3)
   STEP(49,b,c,d,e,a,F3,K3)
   STEP(50,a,b,c,d,e,F3,K3)
   STEP(51,e,a,b,c,d,F3,K3)
   STEP(52,d,e,a,b,c,F3,K3)
   STEP(53,c,d,e,a,b,F3,K3)
   STEP(54,b,c,d,e,a,F3,K3)
   STEP(55,a,b,c,d,e,F3,K3)
   STEP(56,e,a,b,c,d,F3,K3)
   STEP(57,d,e,a,b,c,F3,K3)
   STEP(58,c,d,e,a,b,F3,K3)
   STEP(59,b,c,d,e,a,F3,K3)

   STEP(60,a,b,c,d,e,F4,K4)
   STEP(61,e,a,b,c,d,F4,K4)
   STEP(62,d,e,a,b,c,F4,K4)
   STEP(63,c,d,e,a,b,F4,K4)
   STEP(64,b,c,d,e,a,F4,K4)
   STEP(65,a,b,c,d,e,F4,K4)
   STEP(66,e,a,b,c,d,F4,K4)
   STEP(67,d,e,a,b,c,F4,K4)
   STEP(68,c,d,e,a,b,F4,K4)
   STEP(69,b,c,d,e,a,F4,K4)
   STEP(70,a,b,c,d,e,F4,K4)
   STEP(71,e,a,b,c,d,F4,K4)
   STEP(72,d,e,a,b,c,F4,K4)
   STEP(73,c,d,e,a,b,F4,K4)
   STEP(74,b,c,d,e,a,F4,K4)
   STEP(75,a,b,c,d,e,F4,K4)
   STEP(76,e,a,b,c,d,F4,K4)
   STEP(77,d,e,a,b,c,F4,K4)
   STEP(78,c,d,e,a,b,F4,K4)
   STEP(79,b,c,d,e,a,F4,K4)

   sha1->h[0] += a ;
   sha1->h[1] += b ;
   sha1->h[2] += c ;
   sha1->h[3] += d ;
   sha1->h[4] += e ;
}

void init_sha1hash(/*out*/sha1_hash_t * sha1)
{
   sha1->datalen = 0 ;
   sha1->h[0]    = 0x67452301 ;
   sha1->h[1]    = 0xEFCDAB89 ;
   sha1->h[2]    = 0x98BADCFE ;
   sha1->h[3]    = 0x10325476 ;
   sha1->h[4]    = 0xC3D2E1F0 ;
}

int calculate_sha1hash(sha1_hash_t * sha1, size_t buffer_size, const uint8_t buffer[buffer_size])
{
   int err ;

   if ((uint64_t)-1 == sha1->datalen) {
      init_sha1hash(sha1) ;
   }

   unsigned blocksize = sha1->datalen & 63 ;

   sha1->datalen += buffer_size ;

   if (  (buffer_size & (~(((size_t)-1)>>1))) != 0
         || (sha1->datalen & 0xe000000000000000)) {
      err = EOVERFLOW ;
      goto ONABORT ;
   }

   size_t datasize = blocksize + buffer_size ;

   if (datasize < 64) {

      memcpy(&sha1->block[blocksize], buffer, buffer_size) ;

   } else {
      const uint8_t * next = buffer ;

      if (blocksize) {
         unsigned missing_size = 64 - blocksize ;
         memcpy(&sha1->block[blocksize], next, missing_size) ;
         next     += missing_size ;
         datasize -= 64 ;
         update_sha1hash(sha1, sha1->block) ;
      }

      while (datasize >= 64) {
         update_sha1hash(sha1, next) ;
         next     += 64 ;
         datasize -= 64 ;
      }

      if (datasize) {
         memcpy(sha1->block, next, datasize) ;
      }
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

sha1_hashvalue_t * value_sha1hash(sha1_hash_t * sha1)
{
   if ((uint64_t)-1 != sha1->datalen) {

      unsigned blocksize = sha1->datalen & 63 ;

      sha1->block[blocksize++] = 0x80 ;

      if (blocksize > 56) {
         memset(&sha1->block[blocksize], 0, 64 - blocksize) ;
         update_sha1hash(sha1, sha1->block) ;
         blocksize = 0 ;
      }

      memset(&sha1->block[blocksize], 0, 56 - blocksize) ;
      // length in bits
      * (uint32_t*) &sha1->block[56] = htonl((uint32_t) (sha1->datalen >> 29)) ;
      * (uint32_t*) &sha1->block[60] = htonl((uint32_t) (sha1->datalen << 3)) ;
      update_sha1hash(sha1, sha1->block) ;

      sha1->h[0] = htonl(sha1->h[0]) ;
      sha1->h[1] = htonl(sha1->h[1]) ;
      sha1->h[2] = htonl(sha1->h[2]) ;
      sha1->h[3] = htonl(sha1->h[3]) ;
      sha1->h[4] = htonl(sha1->h[4]) ;

      sha1->datalen = (uint64_t)-1 ;
   }

   return (sha1_hashvalue_t*) sha1->h ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_unevenaddr(sha1_hashvalue_t * sha1sum, const char * string)
{
   char        buffer[1024] ;
   sha1_hash_t sha1 ;
   char        * even   = (char*) (((uintptr_t)buffer + 4) & ~(uintptr_t)0x03) ;
   char        * uneven = (char*) ((uintptr_t)buffer | (uintptr_t)0x03) ;
   size_t      len      = strlen(string) ;

   // TEST as single byte
   init_sha1hash(&sha1) ;
   TEST(len >= 64) ;
   TEST(len < sizeof(buffer)-4) ;
   for (unsigned i = 0; i < len; ++i) {
      TEST(0 == calculate_sha1hash(&sha1, 1, (const uint8_t*)&string[i])) ;
   }
   static_assert(20 == sizeof(*sha1sum), ) ;
   TEST(0 == strncmp((char*)*sha1sum, (char*)value_sha1hash(&sha1), sizeof(*sha1sum))) ;

   // TEST even addr
   strcpy(even, string) ;
   TEST(0 == calculate_sha1hash(&sha1, len, (uint8_t*)even)) ;
   TEST(0 == strncmp((char*)*sha1sum, (char*)value_sha1hash(&sha1), sizeof(*sha1sum))) ;

   // TEST uneven addr
   strcpy(uneven, string) ;
   TEST(0 == calculate_sha1hash(&sha1, len, (uint8_t*)uneven)) ;
   TEST(0 == strncmp((char*)*sha1sum, (char*)value_sha1hash(&sha1), sizeof(*sha1sum))) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_sha1(void)
{
   sha1_hash_t       sha1 ;
   sha1_hashvalue_t  sha1sum ;

   // TEST init
   memset(&sha1, 0xff, sizeof(sha1)) ;
   init_sha1hash(&sha1) ;
   TEST(0 == sha1.datalen) ;
   TEST(0x67452301 == sha1.h[0]) ;
   TEST(0xEFCDAB89 == sha1.h[1]) ;
   TEST(0x98BADCFE == sha1.h[2]) ;
   TEST(0x10325476 == sha1.h[3]) ;
   TEST(0xC3D2E1F0 == sha1.h[4]) ;
   for (int i = 0; i < 64; ++i) {
      // TEST sha1.block is not changed !
      TEST(255 == sha1.block[i]) ;
   }

   // TEST block collects data and after 64 a starts from beginning
   for (int i = 0; i < 256; ++i) {
      uint8_t buffer = (uint8_t) i ;
      TEST(0 == calculate_sha1hash(&sha1, 1, &buffer)) ;
      TEST((size_t) (1+i) == sha1.datalen) ;
      if (63 != (i&63)) {
         for (int g = (i&(~63)); g <= i; ++g) {
            TEST(g == sha1.block[g&63]) ;
         }
      }
   }

   // TEST value_sha1hash
   TEST((uint64_t)-1 != sha1.datalen) ;
   TEST((uint8_t*)sha1.h == (uint8_t*)value_sha1hash(&sha1)) ;
   TEST((uint64_t)-1 == sha1.datalen) ;

   // TEST value_sha1hash does not change value
   TEST((uint64_t)-1 == sha1.datalen) ;
   memcpy(sha1sum, sha1.h, sizeof(sha1sum)) ;
   TEST((uint8_t*)sha1.h == (uint8_t*)value_sha1hash(&sha1)) ;
   TEST((uint64_t)-1 == sha1.datalen) ;
   TEST(0 == memcmp(sha1sum, sha1.h, sizeof(sha1sum))) ;
   TEST((uint8_t*)sha1.h == (uint8_t*)value_sha1hash(&sha1)) ;
   TEST((uint64_t)-1 == sha1.datalen) ;
   TEST(0 == memcmp(sha1sum, sha1.h, sizeof(sha1sum))) ;

   // TEST calculate_sha1hash calls init_sha1sum
   TEST((uint64_t)-1 == sha1.datalen) ;
   memset(sha1.block, 0xff, sizeof(sha1.block)) ;
   TEST(0 == calculate_sha1hash(&sha1, 1, (const uint8_t*)"")) ;
   TEST(1 == sha1.datalen) ;
   TEST(0x67452301 == sha1.h[0]) ;
   TEST(0xEFCDAB89 == sha1.h[1]) ;
   TEST(0x98BADCFE == sha1.h[2]) ;
   TEST(0x10325476 == sha1.h[3]) ;
   TEST(0xC3D2E1F0 == sha1.h[4]) ;
   TEST(0 == sha1.block[0]) ;
   TEST(0xff == sha1.block[1]) ;
   TEST(0xff == sha1.block[63]) ;

   // TEST SHA1("")
   static_assert(20 == sizeof(sha1sum), ) ;
   init_sha1hash(&sha1) ;
   strncpy((char*)sha1sum, "\xda\x39\xa3\xee\x5e\x6b\x4b\x0d\x32\x55\xbf\xef\x95\x60\x18\x90\xaf\xd8\x07\x09", 20) ;
   TEST(0 == memcmp(sha1sum, value_sha1hash(&sha1), 20)) ;

   // TEST SHA1("Frank jagt im komplett verwahrlosten Taxi quer durch Bayern")
   init_sha1hash(&sha1) ;
   const char * string = "Franz jagt im komplett verwahrlosten Taxi quer durch Bayern" ;
   TEST(0 == calculate_sha1hash(&sha1, strlen(string), (const uint8_t*)string)) ;
   strncpy((char*)sha1sum, "\x68\xac\x90\x64\x95\x48\x0a\x34\x04\xbe\xee\x48\x74\xed\x85\x3a\x03\x7a\x7a\x8f", 20) ;
   TEST(0 == memcmp(sha1sum, value_sha1hash(&sha1), 20)) ;

   // TEST SHA1("Franz jagt im komplett verwahrlosten Taxi quer durch Bayern")
   init_sha1hash(&sha1) ;
   string = "Frank jagt im komplett verwahrlosten Taxi quer durch Bayern" ;
   TEST(0 == calculate_sha1hash(&sha1, strlen(string), (const uint8_t*)string)) ;
   strncpy((char*)sha1sum, "\xd8\xe8\xec\xe3\x9c\x43\x7e\x51\x5a\xa8\x99\x7c\x1a\x1e\x94\xf1\xed\x2a\x0e\x62", 20) ;
   TEST(0 == memcmp(sha1sum, value_sha1hash(&sha1), 20)) ;

   // TEST 4 * 40 bytes
   string = "1234567890123456789012345678901234567890" ;
   TEST(0 == calculate_sha1hash(&sha1, strlen(string), (const uint8_t*)string)) ;
   TEST(0 == calculate_sha1hash(&sha1, strlen(string), (const uint8_t*)string)) ;
   TEST(0 == calculate_sha1hash(&sha1, strlen(string), (const uint8_t*)string)) ;
   TEST(0 == calculate_sha1hash(&sha1, strlen(string), (const uint8_t*)string)) ;
   strncpy((char*)sha1sum, "\x38\xf1\x1b\xc1\xb1\xf1\x90\x16\xe2\x53\xc3\x10\x64\xe0\x42\x59\xd9\x44\xb3\x25", 20) ;
   TEST(0 == memcmp(sha1sum, value_sha1hash(&sha1), 20)) ;

   // TEST hash of function
   string = "int unittest_math_hash_sha1()\n"
            "{\n"
            "   resourceusage_t usage = resourceusage_INIT_FREEABLE ;\n"
            "\n"
            "   TEST(0 == init_resourceusage(&usage)) ;\n"
            "\n"
            "   if (test_sha1())     goto ONABORT ;\n"
            "\n"
            "   TEST(0 == same_resourceusage(&usage)) ;\n"
            "   TEST(0 == free_resourceusage(&usage)) ;\n"
            "\n"
            "   return 0 ;\n"
            "ONABORT:\n"
            "   (void) free_resourceusage(&usage) ;\n"
            "   return EINVAL ;\n"
            "}\n" ;
   strncpy((char*)sha1sum, "\xea\xbf\xc3\xbc\xc1\x82\x9b\xa3\x37\x61\x0a\xb2\xf9\xb5\x4d\x73\x9a\x18\xae\xa8", 20) ;
   TEST(0 == test_unevenaddr(&sha1sum, string)) ;

   // TEST EOVERFLOW
   init_sha1hash(&sha1) ;
   TEST(EOVERFLOW == calculate_sha1hash(&sha1, sizeof(size_t)==sizeof(uint32_t) ? 0x80000000 : 0x8000000000000000, (const uint8_t*)"")) ;
   init_sha1hash(&sha1) ;
   TEST(EOVERFLOW == calculate_sha1hash(&sha1, (size_t)-1, (const uint8_t*)"")) ;
   init_sha1hash(&sha1) ;
   sha1.datalen = 0x1fffffffffffffff ;
   TEST(EOVERFLOW == calculate_sha1hash(&sha1, 1, (const uint8_t*)"")) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}


int unittest_math_hash_sha1()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_sha1())        goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
