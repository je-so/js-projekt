/* title: StringEncode
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

   file: C-kern/api/string/encode.h
    Header file <StringEncode>.

   file: C-kern/string/encode.c
    Implementation file <StringEncode impl>.
*/
#ifndef CKERN_STRING_ENCODE_HEADER
#define CKERN_STRING_ENCODE_HEADER

// forward
struct wbuffer_t ;
struct string_t ;

/* about: Encoding.
 *
 * All functions assumes that strings are utf8 encoded.
 *
 * This rule is not applied to functions which expect as input parameter
 * the encoding of the string.
 *
 */

// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_string_encode
 * Test encoding/decoding of strings. */
extern int unittest_string_encode(void) ;
#endif



// section: string_t

// group: query

/* function: sizeurlencode_string
 * Determines url encoded length of parameter str. */
extern size_t sizeurlencode_string(const struct string_t * str, const char * encode_additional_chars) ;

/* function: sizeurldecode_string
 * Determines url decoded length of parameter str. */
extern size_t sizeurldecode_string(const struct string_t * str) ;

// group: urlencoding

/* function: urlencode_string
 * Encodes string str to url encoding.
 * URL encoding assumes utf8 or ascii encoded strings. Every octet is encoded
 * individually. Alphanumeric characters ("A-Z", "a-z", "0-9") are not changed
 * and directly written to the output.
 * Character codes which are not alphanumeric or one of "&=$-_.+!*'(),/@"
 * are written as triplet of '%' plus two hexadecimal digits.
 *
 * Use parameter encode_additional_chars to determine if characters "/@" needs to be encoded.
 * This is useful for »username« and »password» field in a URL. */
extern int urlencode_string(const struct string_t * str, const char * encode_additional_chars, struct wbuffer_t * result) ;

/* function: urldecode_string
 * Decodes a URL encoded string.
 * The decoded is equal or smaller in size. */
extern int urldecode_string(const struct string_t * str, struct wbuffer_t * result) ;

// group: base64encoding

/* function: base64encode_string
 * Encodes string str in Base64 format.
 * The encoded size is 4/3 rounded up of the orginal size. */
extern int base64encode_string(const struct string_t * str, struct wbuffer_t * result) ;

/* function: base64decode_string
 * Decodes string str from Base64 into a binary octet stream.
 * The decoded size is 3/4 of the orginal size.
 * EINVAL is returned if the encoded size is not a multiple of 4. */
extern int base64decode_string(const struct string_t * str, struct wbuffer_t * result) ;



// section :inline implementation

#define sizebase64encode_string(str)   (4 * ((2 + (str)->size)/3))

#endif