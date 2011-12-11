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
 * Export <sha1_hash_t>. */
typedef struct sha1_hash_t             sha1_hash_t ;

/* typedef: sha1_hash_value_t
 * Defines <sha1_hash_value_t> as 160 bit value (uint8_t[20]). */
typedef uint8_t                        sha1_hash_value_t[20] ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_math_hash_sha1
 * Test <sha1_hash_t>. */
extern int unittest_math_hash_sha1(void) ;
#endif



/* struct: sha1_hash_t
 * */
struct sha1_hash_t {
   uint64_t    datalen ;
   /* variable: h
    * Current hash value. */
   uint32_t    h[5] ;
   /* variable: isvalue
    * Indicates if h[5] contains valid SHA1 hash value.
    * This value is true if last call was <value_sha1hash>.
    * If this value is true and <value_sha1hash> is called
    * no more calculation is executed. */
   bool        isvalue ;
   /* variable: block
    * Collects data until size is 64 bytes.
    * Hash calculation is only done in data blocks of 64 bytes. */
   uint8_t     block[64] ;
} ;

// group: lifetime

/* function: sha1_hash_INIT
 * Inits internal fields to start values. */
extern void init_sha1hash(/*out*/sha1_hash_t * sha1) ;

// group: calculate

/* function: calculate_sha1hash
 * Udpates internal fields with content from buffer[buffer_size].
 * You can call this function more than once if the data is not in one linear memory block.
 * To start a new calculation either call <value_sha1hash> or <init_sha1hash> before calling
 * this function. */
extern int calculate_sha1hash(sha1_hash_t * sha1, size_t buffer_size, const uint8_t buffer[buffer_size]) ;

// group: query

/* function: gethash_sha1hash
 * Udpates internal fields the last time and returns hash value.
 * Calling this function more than once returns always returns the same value.
 * The returned pointer is valid as long as you do not call any other function
 * than <value_sha1hash>. */
extern sha1_hash_value_t * value_sha1hash(sha1_hash_t * sha1) ;


#endif
