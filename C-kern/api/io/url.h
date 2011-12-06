/* title: URL

   Uniform Resource Locator:
   URLs are used to `locate' resources, by providing an abstract
   identification of the resource location.

   A generic URL consist of two main parts
   > <name-of-scheme> ':' <scheme-specific-part>

   Common Internet Scheme Syntax:
   URL schemes that involve the direct use of an IP-based protocol to a specified host
   on the Internet use a common syntax for the scheme-specific data. The scheme specific
   data start with a double slash "//".

   Only URLs which use the common Internet scheme syntax are supported by this implementation:
   > <scheme>'://'<user>':'<passwd>'@'<hostname>:<port>'/'<path>'?'<query>'#'<fragment>

   The current implementation supports only the scheme 'http'.

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

   file: C-kern/api/io/url.h
    Header file <URL>.

   file: C-kern/io/url.c
    Implementation file <URL impl>.
*/
#ifndef CKERN_IO_URL_HEADER
#define CKERN_IO_URL_HEADER

#include "C-kern/api/string/string.h"

// forward
struct wbuffer_t ;

/* typedef: struct url_t
 * Exports <url_t>. */
typedef struct url_t                   url_t ;

/* typedef: url_parts_t
 * Defines <url_parts_t> as an array of 7 strings.
 * See <string_t>. */
typedef string_t                       url_parts_t[7] ;

/* enums: url_scheme_e
 *
 * url_scheme_HTTP - URL for HTTP protocol.
 *                   Example: "http://name:pasword@www.server.com/path/to/resource" */
enum url_scheme_e {
   url_scheme_HTTP,
} ;

typedef enum url_scheme_e              url_scheme_e ;

enum url_part_e {
    url_part_USER
   ,url_part_PASSWD
   ,url_part_HOSTNAME
   ,url_part_PORT
   ,url_part_PATH
   ,url_part_QUERY
   ,url_part_FRAGMENT
} ;

typedef enum url_part_e                url_part_e ;

/* about: URL Syntax
 *
 * See *RFC 1738*.
 *
 * Encoded characters in a URL:
 *
 * All characters except alphanumerics, the special characters "$-_.+!*'(),",
 * and reserved characters used for their reserved purposes may be used
 * unencoded within a URL.
 *
 * The characters ";", "/", "?", ":", "@", "=" and "&" (HTTP also "#") are the characters
 * which may be reserved for special meaning within a scheme. No other characters may
 * be reserved within a scheme.
 *
 * How to Encode a Character:
 * Bytes are encoded by a character triplet consisting of the character "%" followed
 * by the two hexadecimal digits forming the hexadecimal value of the byte.
 *
 * Some or all of the parts "<user>:<password>@", ":<password>", ":<port>", and "/<url-path>"
 * may be excluded.
 *
 * */


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_url
 * Unittest for parsing URLs from strings. */
extern int unittest_io_url(void) ;
#endif


/* struct: url_t
 * Describes URL which has the common Internet scheme syntax.
 * Therefore the resource must be located on the Internet.
 *
 * Any URL consist of two main parts:
 * > <name-of-scheme> ':' <scheme-specific-part>
 * The scheme specific data start with a double slash "//" to indicate that it complies with
 * the common Internet scheme syntax.
 *
 * Undefined Fields vs. Empty Fields:
 * If a field is not defined *null* is returned from the corresponding query function.
 * For an empty field the empty string "" is returned.
 * */
struct url_t {
   /* variable: scheme
    * The url scheme. See <url_scheme_e> for a list of all supported values. */
   uint16_t          scheme ;
   uint16_t          parts[7] ;
   char              buffer[] ;
} ;

// group: lifetime

/* function: new_url
 * Parses full url from encoded string and fills in all components.
 * The string must contain a scheme prefix.
 * The enocded string is decoded ("%AB" -> character code 0xAB) and
 * no conversion into the current local is made. Encoded url strings should be
 * be encoded from utf8 characeter encoding. */
extern int new_url(/*out*/url_t ** url, const char * encodedstr) ;

/* function: new2_url
 * Parses url from encoded string and fills in all components.
 * The url string must not contain any scheme prefix, it is read from parameter scheme.
 * See also <new_url>. */
extern int new2_url(/*out*/url_t ** url, url_scheme_e scheme, const char * encodedstr) ;

/* function: new_url
 * Fills in url components from substrings describing components.
 * If parameter *are_parts_encoded* is set to true the substrings are considered
 * encoded and are therefore decoded before out parameter *url* is constructed. */
extern int newparts_url(/*out*/url_t ** url, url_scheme_e scheme, url_parts_t * parts, bool are_parts_encoded) ;

/* function: delete_url
 * Frees all resources bound to url. */
extern int delete_url(url_t ** url) ;

// group: query

/* function: encode_url
 * Encodes all parts and combines them into one string. */
extern int encode_url(const url_t * url, struct wbuffer_t * encoded_url_string) ;

/* function: getpart_url
 * Return part of url as string. */
extern const char * getpart_url(const url_t * url, enum url_part_e part) ;

/* function: fragment_url
 * Returns the anchor/fragment part of the url.
 * Returns "<fragment>" for the following url:
 * > http://<host>/path?<query>#<fragment>. */
extern const char * fragment_url(const url_t * url) ;

/* function: hostname_url
 * Returns name of IP network node or NULL if undefined. */
extern const char * hostname_url(const url_t * url) ;

/* function: passwd_url
 * Returns password or NULL if undefined. */
extern const char * passwd_url(const url_t * url) ;

/* function: path_url
 * Returns password or NULL if undefined. */
extern const char * path_url(const url_t * url) ;

/* function: port_url
 * Returns port or NULL if undefined. */
extern const char * port_url(const url_t * url) ;

/* function: query_url
 * Returns the query part of the url.
 * Returns "<query>" for the following url:
 * > http://<host>/path?<query>#<fragment>. */
extern const char * query_url(const url_t * url) ;

/* function: user_url
 * Returns username or NULL if undefined. */
extern const char * user_url(const url_t * url) ;


// section: url_parts_t

// group: lifetime

/* define: url_parts_INIT_FREEABLE
 * Static initializer. */
#define url_parts_INIT_FREEABLE        { string_INIT_FREEABLE, string_INIT_FREEABLE, string_INIT_FREEABLE, string_INIT_FREEABLE, string_INIT_FREEABLE, string_INIT_FREEABLE, string_INIT_FREEABLE }



// section: inline implementations

/* define: getpart_url
 * Implements <url_t.getpart_url>. */
#define getpart_url(url, part)                                    \
   ( __extension__ ({  const char * _result = 0 ;                 \
      if ( (size_t)(part) < nrelementsof((url)->parts) ) {        \
         uint16_t _offset = ((part) ? (url)->parts[(size_t)(part)-1] : 0) ; \
         if ((url)->parts[(size_t)(part)] > _offset) {            \
         _result = &(url)->buffer[_offset] ;                      \
         }                                                        \
      }                                                           \
      _result ; }))

/* define: fragment_url
 * Implements <url_t.fragment_url>. */
#define fragment_url(url)              getpart_url(url, url_part_FRAGMENT)

/* define: hostname_url
 * Implements <url_t.hostname_url>. */
#define hostname_url(url)              getpart_url(url, url_part_HOSTNAME)

/* define: passwd_url
 * Implements <url_t.passwd_url>. */
#define passwd_url(url)                getpart_url(url, url_part_PASSWD)

/* define: path_url
 * Implements <url_t.path_url>. */
#define path_url(url)                  getpart_url(url, url_part_PATH)

/* define: port_url
 * Implements <url_t.port_url>. */
#define port_url(url)                  getpart_url(url, url_part_PORT)

/* define: query_url
 * Implements <url_t.query_url>. */
#define query_url(url)                 getpart_url(url, url_part_QUERY)

/* define: user_url
 * Implements <url_t.user_url>. */
#define user_url(url)                  getpart_url(url, url_part_USER)


#endif
