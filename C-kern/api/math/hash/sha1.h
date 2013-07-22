/* title: SHA-1-Hashcode
   Offers interface to calculate sha1 hash value of
   data of arbitrary size in bytes.

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
#ifndef CKERN_MATH_HASH_SHA1_HEADER
#define CKERN_MATH_HASH_SHA1_HEADER

/* typedef: struct sha1_hash_t
 * Export <sha1_hash_t> into global namespace. */
typedef struct sha1_hash_t             sha1_hash_t ;

/* typedef: sha1_hashvalue_t
 * Defines <sha1_hashvalue_t> as 160 bit value. */
typedef uint8_t                        sha1_hashvalue_t[20] ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_math_hash_sha1
 * Test <sha1_hash_t>. */
int unittest_math_hash_sha1(void) ;
#endif


/* struct: sha1_hash_t
 * Stores the data needed to calculate SHA1 hash from binary data.
 * Call <init_sha1hash> and the <calculate_sha1hash> for your data.
 * You can call it more than once if your data can not be processed
 * as one single large data chunk. Use <value_sha1hash> to return
 * the hash value. <value_sha1hash> does itself some processing
 * before the value in <h> is returned.
 *
 * Attention:
 * The length in bits is added automatically to the input before <value_sha1hash> returns the computed hash.
 * This makes the computed SHA-1 mores resistent to length extension attacks.
 * */
struct sha1_hash_t {
   /* variable: datalen
    * Stores the number of bytes process so far. This value is needed
    * to calculate the hash value in <value_sha1hash>.
    *
    * Special Value:
    * The special value (uint64_t)-1 indicates
    * that <value_sha1hash> was called and the hash value in <h> is valid.
    * Calling <value_sha1hash> a second time only returns a pointer to <h>. */
   uint64_t    datalen ;
   /* variable: h
    * Current hash value. */
   uint32_t    h[5] ;
   /* variable: block
    * Collects data until size is 64 bytes.
    * Hash calculation is only done in data blocks of 64 bytes. */
   uint8_t     block[64] ;
} ;

// group: lifetime

/* function: init_sha1hash
 * Inits internal fields to start values. */
void init_sha1hash(/*out*/sha1_hash_t * sha1) ;

// group: calculate

/* function: calculate_sha1hash
 * Udpates internal fields with content from buffer[buffer_size].
 * You can call this function more than once if the data is not in one linear memory block.
 * To start a new calculation either call <value_sha1hash> or <init_sha1hash> before calling
 * this function. */
int calculate_sha1hash(sha1_hash_t * sha1, size_t buffer_size, const uint8_t buffer[buffer_size]) ;

// group: query

/* function: value_sha1hash
 * Udpates internal fields the last time and returns hash value.
 * Calling this function more than once always returns the same value.
 * The returned pointer is valid as long as you do not call any other function
 * than <value_sha1hash>. */
sha1_hashvalue_t * value_sha1hash(sha1_hash_t * sha1) ;


#endif
