/* title: CRC-32 impl

   Implements <CRC-32>.

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
   (C) 2013 Jörg Seebohn

   file: C-kern/api/math/hash/crc32.h
    Header file <CRC-32>.

   file: C-kern/math/hash/crc32.c
    Implementation file <CRC-32 impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/math/hash/crc32.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/math/int/bitorder.h"
#endif


// section: crc32_t

// group: documentation

/* about: How CRC-32 works !
 *
 * See <http://www.riccibitti.com/crcguide.htm>.
 *
 * The input data is converted to a single large binary number.
 * Then it is divided by the 33 bit number 0x104C11DB7 and the reversed remainder xored with UINT32_MAX
 * becomes the result of CRC-32 computation.
 *
 * The first byte of the data is the most significant byte of the binary number.
 * But the bits of the bytes are reversed. Bit 0 of the first byte is therfore the most significand bit of the number.
 * The next bit is bit 1 from the first byte. The 9th bit is bit 0 from the second data byte and so on.
 *
 * Then the number is divided by the divisor 0x104C11DB7 (polynomial). The calculation is done modulo 2 on every bit.
 * This means adding or subtracting two integer values is the same as xoring the two values. All carry bits are ignored.
 *
 * Add / Sub Example:
 * >   0b1101      0b1101
 * > + 0b1001    - 0b1001
 * > ---------   ---------
 * >   0b0100      0b0100
 *
 * Both operations result in the same value. So there is no higher than relation between two numbers if the index
 * of their most significant 1 bit is the same.
 *
 * Example:
 * The first step is to reverse the bits of every byte and put the bits together as one binary large number:
 * Original data: 0b111111110101010100110011
 * Reversed data: 0b111111111010101011001100
 * 0x104C11DB7 == 0b100000100110000010001110110110111
 *
 * > Division: (111111111010101011001100 (+ 32 0 bits)) / 100000100110000010001110110110111 = (ignored)
 * >          - 100000100110000010001110110110111
 * >            -------------------------------------
 * >            011111011100101001000010110110111 (remainder)
 * >          -  100000100110000010001110110110111
 * >            -------------------------------------
 * >             011110011111010000001011011011001 (remainder)
 * >          -   100000100110000010001110110110111
 * >            ...
 *
 * Once the remainder is calculated it is reversed and then xored with 0xffffffff.
 *
 * To speed up the calculation we consider only one byte value at a time and set all following bits to 0.
 * After we have calculated the 32-bit remainder of this single byte we subtract it (xor) from the follwing bytes.
 * This works only cause of subtraction beeing the xor operation. Cause (data xor remainder) is the same as
 * (data xor (0 xor remainder)).
 *
 * > Division: (1111111100000000000000000 (+ 32 0 bits)) / 100000100110000010001110110110111 = (ignored)
 * >          - 100000100110000010001110110110111
 * >            -------------------------------------
 * >           ...
 * >                    XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX (remainder)
 * >
 * >            000000001010101011001100
 * >          -         XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
 * >          --------------------------------------------
 * >                    YYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYY
 * >          -         100000100110000010001110110110111
 * >          --------------------------------------------
 * >            ...
 *
 * To speed things up we do not reverse the input bytes but we can reverse the value of the divisor 0x104C11DB7.
 * The resulting remainder is the already reversed so we do not need to reverse it before returning it as the finit
 * value.
 *
 * The next thing is to precompute the remainder of every possible input byte and store it in the tabe <s_precomputed_crc32>.
 *
 * As a last note. The CRC-32 computation uses the value 0xffffffff as initialization value for the first remainder.
 *
 * */

// group: variables

/* variable: s_precomputed_crc32
 * Precomputed remainder for every possible input byte divided by revered polynomial 0x104C11DB7. */
static uint32_t   s_precomputed_crc32[256] = {
   0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
   0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7, 0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
   0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
   0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
   0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433, 0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
   0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
   0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
   0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f, 0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
   0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
   0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
   0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b, 0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
   0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
   0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
   0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777, 0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
   0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
   0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
} ;

// group: calculation

uint32_t update2_crc32(uint32_t crcvalue, size_t blocksize, const void * datablock/*[blocksize]*/)
{
   uint32_t       value  = crcvalue ;
   const uint8_t  * next = datablock ;

   for (size_t i = blocksize; i; --i, ++next) {
      value = (value >> 8) ^ s_precomputed_crc32[(uint8_t)(*next ^ value)] ;
   }

   return value ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_checktable(void)
{
   // POLY == 0x104C11DB7, top bit is not stored cause it is not stored in the result of the subtraction

   const uint32_t divisor = reversebits_int((uint32_t)0x04C11DB7) ;

   for (unsigned databyte = 0; databyte <= 255; ++databyte) {
      uint32_t dividend = databyte ;
      for (unsigned i = 0; i < 8; ++i) {
         bool issubtract = (dividend & 0x01) ;
         dividend >>= 1 ;
         if (issubtract) {
            dividend ^= divisor ;
         }
      }
      TEST(s_precomputed_crc32[databyte] == dividend) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_initfree(void)
{
   crc32_t crc = crc32_INIT ;

   // TEST crc32_INIT
   TEST(crc.value == UINT32_MAX) ;

   // TEST init_crc32
   crc.value = 5 ;
   init_crc32(&crc) ;
   TEST(crc.value == UINT32_MAX) ;

   // TEST crc32_value
   TEST(0 == value_crc32(&crc)) ;
   for (uint32_t i = 1; i; i <<= 1) {
      crc.value = i ;
      TEST(value_crc32(&crc) == (i ^ UINT32_MAX)) ;
      TEST(crc.value == i/*not changed*/) ;
   }

   // == no free ==

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_calculation(void)
{
   crc32_t  crc = crc32_INIT ;
   uint8_t  buffer[256] ;
   struct {
      const char  * data ;
      uint32_t    value ;
   }        testdata[] =   {
      { "123456789", 0xCBF43926 },
      { "abcdefghijklmnopqrstuvwxyz", 0x4C2750BD }
   } ;

   // TEST calculate_crc32, update_crc32
   for (unsigned i = 0; i < lengthof(testdata); ++i) {
      size_t len = strlen(testdata[i].data) ;
      init_crc32(&crc) ;
      update_crc32(&crc, len/2, testdata[i].data) ;
      update_crc32(&crc, len-len/2, testdata[i].data + len/2) ;
      TEST(testdata[i].value == value_crc32(&crc)) ;
      TEST(testdata[i].value == calculate_crc32(len, testdata[i].data)) ;
   }

   // TEST calculate_crc32, update_crc32: all byte values
   uint32_t testresult[256] = {
      0xd202ef8d, 0x36de2269, 0x0854897f, 0x8bb98613, 0x515ad3cc, 0x30ebcf4a, 0xad5809f9, 0x88aa689f, 0xbce14302, 0x456cd746, 0xad2d8ee1, 0x9270c965, 0xe6fe46b8, 0x69ef56c8, 0xa06c675e, 0xcecee288,
      0x2c183a19, 0xdcf57f85, 0xbcb51c15, 0x3bddffa4, 0x195881fe, 0xe5c38cfc, 0x92382767, 0x8295a696, 0xd880d40c, 0xbf078bb2, 0x0ab0c3dc, 0xd708085d, 0xd30e9683, 0xc5665f58, 0x4d786d77, 0x91267e8a,
      0xe4908305, 0xeee59bdf, 0x11e084b7, 0x25715854, 0x8222efe9, 0x405243f9, 0xc42e728b, 0x0da62e3c, 0xc8d59dde, 0xf1135daf, 0x4f218b7f, 0xbe4b5beb, 0xd7bcf3c5, 0x7c043934, 0x2f1c12ce, 0x05202171,
      0xd3dcbeaa, 0xb50c79ff, 0x37625df9, 0xa984a67e, 0x44a2c3a5, 0x2249de0a, 0xfd4fdad4, 0xebfc1395, 0x7a8ecccb, 0x81cbf271, 0x338dbc67, 0xb0ec7fee, 0xba6fb00a, 0x6a052532, 0xdbdea683, 0x100ece8c,
      0x40c06fd8, 0x5b910402, 0xa4853f19, 0x5918d258, 0xc65aab10, 0xc9c5105d, 0x58aee371, 0x1de0d4f7, 0x89c1a144, 0xac3a5291, 0xc3afde83, 0x477e0ad1, 0x5cfb7e7e, 0x6d8e75e5, 0x936b1b98, 0xca26c3e1,
      0x6eae4a54, 0xa206b548, 0x2fc21042, 0xb89d0d6f, 0x63b19ba4, 0x18db9c9c, 0xa9ab1fbf, 0x7c7a2ed8, 0x3fc61683, 0xb43b1251, 0x45640d17, 0xad2d863b, 0x017936f0, 0x7a6449ee, 0x19193848, 0x51c87372,
      0x21ea56b6, 0xca9442ac, 0xae149478, 0x58c932f5, 0x5552856d, 0xdc8c353a, 0xb0037e67, 0xd2b2ecf3, 0xb50d17ad, 0x4ebee433, 0xc0fbb839, 0x57a724ed, 0x48eafb1f, 0x6c411566, 0xdcb526aa, 0x39d06c94,
      0x755c1980, 0x18cd711e, 0x961e0f8f, 0x669f4fb5, 0x3e0782e4, 0x55344bdd, 0x9353a1a6, 0x23455e6e, 0x26f51f82, 0xf84b3106, 0x8b4999ab, 0x545a74c0, 0x10814a5e, 0x70751fb5, 0xdec481aa, 0x24650d57,
      0xca91cdf7, 0x6baeaa49, 0xdedf5a1f, 0x2b65efb8, 0xfd46f6e5, 0x9f4dc823, 0x7421f522, 0x74ca991f, 0xbc1d23f3, 0x626e6a8c, 0x3b0324d2, 0xc08e05f9, 0xf2ade43b, 0xf0f254d0, 0x5e21612a, 0x74e0998b,
      0x5813c6f8, 0xe63425b7, 0x99e00fef, 0x8b283295, 0xa58ef729, 0x10709edd, 0x331c070b, 0x2b882ce5, 0xfc9d6a20, 0x604379cf, 0xc963098f, 0xc81158f9, 0xef126b02, 0xb25d7333, 0x7ad76dab, 0xf3cccc55,
      0x1f2625d2, 0x1b11e92e, 0x3617316f, 0x40e67627, 0xa6f3db61, 0x4ead1aff, 0xc0fbabc7, 0x9f70757e, 0xbd2de819, 0x19de71e9, 0x3dce21d6, 0xfc8b2c2d, 0x4841d717, 0xf9fef4c8, 0x762ad514, 0x8ec7af5c,
      0x7b308671, 0x491d1dbb, 0xab974a34, 0xa1756e44, 0x6f1e68ff, 0x4464596b, 0x2a2425bd, 0x32fd22b6, 0x35883fa8, 0xb8874740, 0x8fd273c8, 0x1b811d78, 0x4e106839, 0x3899b8dc, 0x71867641, 0x8876b6e0,
      0xe9e4b9f3, 0x1a3c5ab4, 0x6b7e07de, 0xb16ffd53, 0xbcd88697, 0x57db07d3, 0xbf88d061, 0xed086180, 0xaae82e4e, 0xa1741120, 0x927c90fa, 0x834e9327, 0xe5599aee, 0x7080c765, 0x9376152a, 0x02f18f6f,
      0x89deb01f, 0xae57de8a, 0xb71c6195, 0x4d0a1749, 0x5cf10a63, 0xf05c083e, 0x7c23d9cf, 0xc1125402, 0xb4c5c613, 0xa90701e5, 0x64cdc5b1, 0x7f01aa0e, 0xbaa05ddf, 0x3cb62efc, 0x075e1847, 0x2cd1aae3,
      0x4b276f9d, 0x8bfaf5f5, 0x515a90bf, 0xb08ea8c2, 0x00bfe4d8, 0x8a6a1c78, 0xc53f3bd2, 0x84741495, 0xfc329618, 0x18441f91, 0x15cd0326, 0x37c29c83, 0x0f3851d8, 0x84be13ff, 0xb836716c, 0xa60b0b66,
      0xcbc8d2f7, 0x3baa826a, 0x2ced5e79, 0xdf4368ed, 0xb6b60425, 0x54678b5d, 0x9352f266, 0x55991ead, 0xc956d3e8, 0xb87b99ac, 0x50b260d5, 0x0e845022, 0xb367940e, 0xf6052bbf, 0xd32f9ba0, 0x29058c73
   } ;
   for (unsigned i = 0; i < lengthof(buffer); ++i) {
      buffer[i] = (uint8_t)i ;
      init_crc32(&crc) ;
      update_crc32(&crc, i, buffer) ;
      update_crc32(&crc, 1, buffer+i) ;
      TEST(testresult[i] == value_crc32(&crc)) ;
      TEST(testresult[i] == calculate_crc32(i+1, buffer)) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_math_hash_crc32()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_checktable())     goto ONABORT ;
   if (test_initfree())       goto ONABORT ;
   if (test_calculation())    goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
