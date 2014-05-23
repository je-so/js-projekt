/* title: CRC-32

   Calculates the CRC-32 value of a byte sequence.
   This 32bit cyclic redundancy check value is used to determine errors caused by noise in I/O channels.
   The input data data is considered a large binary number is divided by a certain value which
   is called the "generator polynomial". The remainder with some adaption made to it is considered
   the result.

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
#ifndef CKERN_MATH_HASH_CRC32_HEADER
#define CKERN_MATH_HASH_CRC32_HEADER

/* typedef: struct crc32_t
 * Export <crc32_t> into global namespace. */
typedef struct crc32_t                 crc32_t ;


// section: Functions

// group: CRC-32

/* function: calculate_crc32
 * Calculates the CRC-32 checkusm of a data block.
 * Use type <crc32_t> if data bytes are not stored in consecutive memory addresses. */
uint32_t calculate_crc32(size_t blocksize, const void * datablock/*[blocksize]*/) ;

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_math_hash_crc32
 * Test <crc32_t> functionality. */
int unittest_math_hash_crc32(void) ;
#endif


/* struct: crc32_t
 * Type which captures the partially calculated checksum.
 *
 * Usage:
 * Use <init_crc32> to initialize it.
 * Call <update_crc32> either once or if your data is splitted over several blocks for every block once.
 * Call <value_crc32> to get the computed CRC-32 checksum value. */
struct crc32_t {
   /* variable: value
    * The crc32 value calculated from a sequence of bytes.
    * The value will be updated every time <update_crc32> is called. */
   uint32_t value ;
} ;

// group: lifetime

/* define: crc32_INIT
 * Static initializer. */
#define crc32_INIT { (uint32_t)-1 }

/* function: init_crc32
 * Initializes object of type <crc32_t>. */
void init_crc32(/*out*/crc32_t * crc) ;

// group: calculate

/* function: update_crc32
 * Udates the CRC-32 checksum with the next chunk of data. */
void update_crc32(crc32_t * crc, size_t blocksize, const void * datablock/*[blocksize]*/) ;

/* function: update2_crc32
 * Internally used function from <update_crc32> and <calculate_crc32>. */
uint32_t update2_crc32(uint32_t crcvalue, size_t blocksize, const void * datablock/*[blocksize]*/) ;

// group: query

/* function: value_crc32
 * Return the CRC32 checksum.
 * The calculated value is the checksum of the concatenated byte sequence of successive calls to <update_crc32>.
 * To start a new computation call <init_crc32> before calling <update_crc32>.
 * But calling <init_crc32> also sets the CRC value to 0 which is returned by this function. */
uint32_t value_crc32(const crc32_t * crc) ;


// section: inline implementation

// group: Functions

/* function: define calculate_crc32
 * Implements <calculate_crc32>. */
#define calculate_crc32(blocksize, datablock)   \
         (update2_crc32((uint32_t)-1, blocksize, datablock) ^ (uint32_t)-1)

// group: crc32_t

/* function: init_crc32
 * Implements <crc32_t.init_crc32>. */
#define init_crc32(crc)                         \
         ((void)(*(crc)=(crc32_t)crc32_INIT))

#define update_crc32(crc, blocksize, datablock) \
         ((void)((crc)->value = update2_crc32((crc)->value, blocksize, datablock)))

/* function: value_crc32
 * Implements <crc32_t.value_crc32>. */
#define value_crc32(crc)                        \
         ((crc)->value ^ (uint32_t)-1)

#endif
