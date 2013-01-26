/* title: Base64EncodeString
   Offers interface to encode/decode strings into different formats.

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
#ifndef CKERN_STRING_BASE64ENCODE_STRING_HEADER
#define CKERN_STRING_BASE64ENCODE_STRING_HEADER

// forward
struct wbuffer_t ;
struct string_t ;

/* about: Base64Encoding.
 *
 * All functions assumes that strings are utf8 encoded.
 *
 */


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_string_base64encode
 * Test encoding/decoding of strings. */
int unittest_string_base64encode(void) ;
#endif


// struct: string_t

// group: query

/* function: sizebase64encode_string
 * Returns the size of the encoded string.
 * The encoded size is 4/3 of the orginal size. If the size is not a multiple of 3
 * then it is rounded up to the next higher value which is a multiple of 3. */
size_t sizebase64encode_string(const struct string_t * str) ;

/* function: sizebase64decode_string
 * Returns the size of the decoded string.
 * The decoded size is 3/4 of the orginal size minus 0, 1, or 2. */
size_t sizebase64decode_string(const struct string_t * str) ;

// group: base64encoding

/* function: base64encode_string
 * Encodes string str in Base64 format.
 * The encoded size is 4/3 of the orginal size. If the size is not a multiple of 3
 * then it is rounded up to the next higher value which is a multiple of 3. */
int base64encode_string(const struct string_t * str, /*ret*/struct wbuffer_t * result) ;

/* function: base64decode_string
 * Decodes string str from Base64 into a binary octet stream.
 * The decoded size is 3/4 of the orginal size.
 * EINVAL is returned if the encoded size is not a multiple of 4. */
int base64decode_string(const struct string_t * str, /*ret*/struct wbuffer_t * result) ;


// section: inline implementation

/* define: sizebase64encode_string
 * Implements <string_t.sizebase64encode_string>. */
#define sizebase64encode_string(str)   (4 * ((2 + (str)->size)/3))

#endif
