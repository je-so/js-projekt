/* title: URLEncodeString
   Offers interface to encode/decode strings format suitable for URLs.

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

   file: C-kern/api/string/urlencode_string.h
    Header file <URLEncodeString>.

   file: C-kern/string/urlencode_string.c
    Implementation file <URLEncodeString impl>.
*/
#ifndef CKERN_STRING_URLENCODE_STRING_HEADER
#define CKERN_STRING_URLENCODE_STRING_HEADER

// forward
struct wbuffer_t ;
struct conststring_t ;

/* about: URLEncoding.
 *
 * All functions assumes that strings are utf8 encoded.
 *
 */


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_string_urlencode
 * Test url encoding/decoding of strings. */
int unittest_string_urlencode(void) ;
#endif


// section: string_t

// group: query

/* function: sizeurlencode_string
 * Determines url encoded length of parameter str.
 *
 * Set *except_char* to '/' if the path of a URL is to be encoded.
 * Set *except_char* to ' ' if a formdata field is to be encoded. */
size_t sizeurlencode_string(const struct conststring_t * str, uint8_t except_char) ;

/* function: sizeurldecode_string
 * Determines url decoded length of parameter str. */
size_t sizeurldecode_string(const struct conststring_t * str) ;

// group: urlencoding

/* function: urlencode_string
 * Encodes string str to url encoding.
 * URL encoding assumes utf8 or ascii encoded strings. Every octet is encoded
 * individually. Alphanumeric characters ("A-Z", "a-z", "0-9") are not changed
 * and directly written to the output.
 * Character codes which are neither alphanumeric nor one of "-_.*"
 * are written as triplet of '%' plus two hexadecimal digits.
 *
 * If parameter except_char is set to 0 no exception is made during encoding.
 * If it is set to a value != 0 the character is not encoded but changed into the character
 * given in parameter *changeto_char*.
 *
 * Set *except_char* to '/' and changeto_char to '/' if the path of a URL is to be encoded cause '/'
 * is considered a normal character for a path.
 *
 * Set *except_char* to ' ' and *changeto_char* to '+' if a formdata field is to be encoded.
 *
 * */
int urlencode_string(const struct conststring_t * str, uint8_t except_char, uint8_t changeto_char, struct wbuffer_t * result) ;

/* function: urldecode_string
 * Decodes a URL encoded string.
 * The decoded string is equal or smaller in size.
 *
 * Set parameter *changefrom_char* to value '+' and *changeinto_char* to value ' '
 * in case a formdata field is decoded.
 *
 * Else set changefrom_char to 0 so no character is changed therefore changeinto_char is never used. */
int urldecode_string(const struct conststring_t * str, uint8_t changefrom_char, uint8_t changeinto_char, struct wbuffer_t * result) ;


#endif