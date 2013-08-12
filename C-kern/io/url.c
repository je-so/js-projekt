/* title: URL impl
   Implements <URL>.

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

#include "C-kern/konfig.h"
#include "C-kern/api/io/url.h"
#include "C-kern/api/err.h"
#include "C-kern/api/memory/wbuffer.h"
#include "C-kern/api/string/string.h"
#include "C-kern/api/string/urlencode_string.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/string/cstring.h"
#endif


// section: url_t

// group: helper

static int parse_urlscheme(url_scheme_e * scheme, const char ** encodedstr)
{
   int err ;
   const char * next = *encodedstr ;
   char         c ;

#define ISNEXTCHAR1(special)           ((special) == (c = *(next++)))
#define ISNEXTCHAR2(lowcase, upcase)   (((lowcase) == (c = *(next++))) || ((upcase) == c))

   err = EINVAL ;

   if ISNEXTCHAR2('h', 'H') {
      if (     ISNEXTCHAR2('t', 'T')
            && ISNEXTCHAR2('t', 'T')
            && ISNEXTCHAR2('p', 'P')
            && ISNEXTCHAR1(':')) {
         *scheme = url_scheme_HTTP ;
         err = 0 ;
      }
   }

   if (err) goto ONABORT ;

   *encodedstr = next ;
   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

static int parsepart_url(url_part_e part, url_parts_t * parts, const uint8_t ** encodedstr, uint8_t endmarker)
{
   const uint8_t * start = *encodedstr ;
   const uint8_t * next  = start ;
   uint8_t       c ;

   assert(endmarker < 0x80) ;

   for (c = *next; endmarker != c; c = *next) {
      ++ next ;
   }

   (*parts)[part].addr  = start ;
   (*parts)[part].size  = (size_t) (next - start) ;
   *encodedstr          = next ;

   return 0 ;
}

static int parsepart2_url(url_part_e part, url_parts_t * parts, const uint8_t ** encodedstr, uint8_t endmarker1, uint8_t endmarker2)
{
   const uint8_t * start = *encodedstr ;
   const uint8_t * next  = start ;
   uint8_t       c ;

   assert(endmarker1 < 0x80) ;
   assert(endmarker2 < 0x80) ;

   for (c = *next; endmarker1 != c && endmarker2 != c; c = *next) {
      ++ next ;
   }

   (*parts)[part].addr  = start ;
   (*parts)[part].size  = (size_t) (next - start) ;
   *encodedstr          = next ;

   return 0 ;
}

// group: implementation

int newparts_url(/*out*/url_t ** url, url_scheme_e scheme, url_parts_t * parts, bool are_parts_encoded)
{
   int err ;
   size_t      decoded_sizes[lengthof(*parts)] ;
   url_t       * newurl   = 0 ;
   size_t      len        = 0 ;
   uint16_t    buffidx    = 0 ;

   for (unsigned i = 0; i < lengthof(*parts); ++i) {
      if ((*parts)[i].addr) {
         size_t decoded_size ;

         if (are_parts_encoded) {
            decoded_size = sizeurldecode_string(&(*parts)[i]) ;
         } else {
            decoded_size = (*parts)[i].size ;
         }

         len += decoded_size + 1 ;
         if (len <= decoded_size) {
            len = 0xffff ;
            break ;
         }

         decoded_sizes[i] = decoded_size ;
      }
   }

   if (0xffff <= len) {
      err = EOVERFLOW ;
      goto ONABORT ;
   }

   size_t objsize = sizeof(url_t) + len ;
   newurl = malloc(objsize) ;
   if (!newurl) {
      err = ENOMEM ;
      TRACEOUTOFMEM_ERRLOG(objsize, err) ;
      goto ONABORT ;
   }

   // init newurl
   newurl->scheme = scheme ;
   for (unsigned i = 0; i < lengthof(*parts); ++i) {
      if ((*parts)[i].addr) {
         size_t decoded_size = decoded_sizes[i] ;
         if (decoded_size < (*parts)[i].size) {
            wbuffer_t decoded = wbuffer_INIT_STATIC(decoded_size, (uint8_t*) &newurl->buffer[buffidx]) ;

            err = urldecode_string(&(*parts)[i], 0, 0, &decoded) ;
            if (err) goto ONABORT ;

         } else {
            memcpy( &newurl->buffer[buffidx], (*parts)[i].addr, decoded_size) ;
         }

         buffidx = (uint16_t) (buffidx + decoded_size) ;
         newurl->buffer[buffidx ++] = 0 ;
      }
      static_assert(lengthof(newurl->parts) == lengthof(*parts), ) ;
      newurl->parts[i] = buffidx ;
   }

   assert(len == buffidx) ;

   *url = newurl ;
   return 0 ;
ONABORT:
   free(newurl) ;
   TRACEABORT_ERRLOG(err) ;
   return 0 ;
}

int new2_url(/*out*/url_t ** url, url_scheme_e scheme, const char * encodedstr)
{
   int err ;
   const uint8_t  * pos ;
   const uint8_t  * next     = (const uint8_t*)encodedstr ;
   const uint8_t  * slashpos = (const uint8_t*)strchrnul(encodedstr, '/') ;
   url_parts_t    parts      = url_parts_INIT_FREEABLE ;

   VALIDATE_INPARAM_TEST( (unsigned)scheme <= url_scheme_HTTP, ONABORT, PRINTINT_ERRLOG(scheme)) ;

   if (     (pos = (const uint8_t*)strchrnul((const char*)next, '@'))
         && pos < slashpos) {
      err = parsepart2_url(url_part_USER, &parts, &next, ':', '@') ;
      if (err) goto ONABORT ;
      if (pos > next) {
         ++ next ;
         err = parsepart_url(url_part_PASSWD, &parts, &next, '@') ;
         if (err) goto ONABORT ;
      }
      if (pos != next) {
         err = EINVAL ;
         goto ONABORT ;
      }
      ++ next ;
   }

   err = parsepart2_url(url_part_HOSTNAME, &parts, &next, ':', *slashpos) ;
   if (err) goto ONABORT ;

   if (':' == *next) {
      ++ next ;
      err = parsepart_url(url_part_PORT, &parts, &next, *slashpos) ;
      if (err) goto ONABORT ;
      for (const uint8_t * nr = parts[url_part_PORT].addr; nr < next; ++nr) {
         if (!('0' <= *nr && *nr <= '9')) {
            err = EINVAL ;
            goto ONABORT ;
         }
      }
   }

   pos = (const uint8_t*)strchrnul((const char*)next, '?') ;
   if (0 == *pos) {
      pos = (const uint8_t*)strchrnul((const char*)next, '#') ;
   }

   if (*next) {
      ++ next ;
      err = parsepart_url(url_part_PATH, &parts, &next, *pos) ;
      if (err) goto ONABORT ;
   }

   if ('?' == *next) {
      ++ next ;
      pos = (const uint8_t*)strchrnul((const char*)next, '#') ;
      err = parsepart_url(url_part_QUERY, &parts, &next, *pos) ;
      if (err) goto ONABORT ;
   }

   if ('#' == *next) {
      ++ next ;
      err = parsepart_url(url_part_FRAGMENT, &parts, &next, 0) ;
      if (err) goto ONABORT ;
   }

   err = newparts_url(url, scheme, &parts, true) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int new_url(/*out*/url_t ** url, const char * encodedstr)
{
   int err ;
   url_scheme_e   scheme ;
   const char     * next = encodedstr ;

   err = parse_urlscheme(&scheme, &next) ;
   if (err) goto ONABORT ;

   if (  '/' != next[0]
      || '/' != next[1]) {
      err = EINVAL ;
      goto ONABORT ;
   }
   next += 2 ;

   err = new2_url(url, scheme, next) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int delete_url(url_t ** url)
{
   url_t * delobj = *url ;

   if (delobj) {
      *url = 0 ;

      free(delobj) ;
   }

   return 0 ;
}

int encode_url(const url_t * url, /*ret*/wbuffer_t * encoded_url_string)
{
   int err ;
   size_t      sizeencoding[lengthof(url->parts)] = { 0 } ;
   uint8_t     * start_result = 0 ;
   size_t      result_size    = sizeof("http://")-1 ;

   // sizeof("http://") includes trailing \0 byte
   switch(url->scheme) {
   case url_scheme_HTTP:   break ;
   default:                err = EINVAL ; goto ONABORT ;
   }

   // calculate total length of result
   static_assert( 2 == sizeof(url->parts[0]), ) ;
   uint16_t buffer_offset = 0 ;
   for (unsigned i = 0; i < lengthof(url->parts); ++i) {
      size_t size = (size_t) (url->parts[i] - buffer_offset) ;
      if (size) {
         -- size ; // include no trailing '\0' byte
         string_t   part = string_INIT(size, &url->buffer[buffer_offset]) ;
         sizeencoding[i] = sizeurlencode_string(&part, i == url_part_PATH? '/' : 0) ;
         if (i == url_part_HOSTNAME) {
            result_size += /* no special marker for hostname */ sizeencoding[i] ;
         } else {
            result_size += 1 + sizeencoding[i] ;
         }
      }
      buffer_offset = url->parts[i] ;
   }

   clear_wbuffer(encoded_url_string) ;
   err = appendbytes_wbuffer(encoded_url_string, result_size, &start_result) ;
   if (err) goto ONABORT ;

   // encode & copy parts to result
   memcpy(start_result, "http://", sizeof("http://")-1) ;
   uint8_t* result = start_result + sizeof("http://")-1 ;
   bool     isuser = false ;
   buffer_offset = 0 ;
   for (unsigned i = 0; i < lengthof(url->parts); ++i) {
      size_t size = (size_t) (url->parts[i] - buffer_offset) ;
      if (size) {
         -- size ; // copy no trailing '\0' byte
         switch((url_part_e)i) {
         case url_part_USER:     isuser = true ;      break ;
         case url_part_PASSWD:   *(result++) = ':' ;  break ;
         case url_part_HOSTNAME: if (isuser) *(result++) = '@' ; isuser = false ; break ;
         case url_part_PORT:     *(result++) = ':' ;  break ;
         case url_part_PATH:     *(result++) = '/' ;  break ;
         case url_part_QUERY:    *(result++) = '?' ;  break ;
         case url_part_FRAGMENT: *(result++) = '#' ;  break ;
         }
         const uint8_t * urlbuffer = &url->buffer[buffer_offset] ;
         if (sizeencoding[i] > size) {

            wbuffer_t encoded = wbuffer_INIT_STATIC(sizeencoding[i], result) ;

            err = urlencode_string(&(string_t)string_INIT(size, urlbuffer), i == url_part_PATH? '/' : 0, '/', &encoded) ;
            if (err) goto ONABORT ;

         } else {
            memcpy(result, urlbuffer, sizeencoding[i]) ;
         }
         result += sizeencoding[i] ;
      } else if (isuser && url_part_HOSTNAME == i) {
         isuser = false ;
         *(result++) = '@' ;
      }
      buffer_offset = url->parts[i] ;
   }

   assert((size_t) (result - start_result) == result_size) ;

   return 0 ;
ONABORT:
   clear_wbuffer(encoded_url_string) ;
   TRACEABORT_ERRLOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_url_initfree(void)
{
   url_t      * url  = 0 ;
   cstring_t    str  = cstring_INIT ;
   wbuffer_t    wbuf = wbuffer_INIT_CSTRING(&str) ;
   const char * test ;

   // TEST new_url, delete_url
   TEST(0 == new_url(&url, "http://127.0.0.1/")) ;
   TEST(0 != url) ;
   TEST(0 == delete_url(&url)) ;
   TEST(0 == url) ;
   TEST(0 == delete_url(&url)) ;
   TEST(0 == url) ;

   // TEST new_url: full url
   test = "http://user1:passwd2@server3.de:123/d1/d2?x=a#frag9" ;
   TEST(0 == new_url(&url, test)) ;
   TEST(0 != url) ;
   TEST(0 != user_url(url)) ;
   TEST(0 != passwd_url(url)) ;
   TEST(0 != hostname_url(url)) ;
   TEST(0 != port_url(url)) ;
   TEST(0 != path_url(url)) ;
   TEST(0 != query_url(url)) ;
   TEST(0 != fragment_url(url)) ;
   TEST(0 == strcmp((const char*)user_url(url), "user1")) ;
   TEST(0 == strcmp((const char*)passwd_url(url), "passwd2")) ;
   TEST(0 == strcmp((const char*)hostname_url(url), "server3.de")) ;
   TEST(0 == strcmp((const char*)port_url(url), "123")) ;
   TEST(0 == strcmp((const char*)path_url(url), "d1/d2")) ;
   TEST(0 == strcmp((const char*)query_url(url), "x=a")) ;
   TEST(0 == strcmp((const char*)fragment_url(url), "frag9")) ;
   TEST(0 == encode_url(url, &wbuf)) ;
   test = "http://user1:passwd2@server3.de:123/d1/d2?x%3Da#frag9" ;
   TEST(strlen(test) == size_wbuffer(&wbuf)) ;
   TEST(0 == strncmp(test, str_cstring(&str), size_wbuffer(&wbuf))) ;
   TEST(0 == delete_url(&url)) ;
   TEST(0 == url) ;

   // TEST new_url: null or empty
   test = "http://" ;
   TEST(0 == new_url(&url, test)) ;
   TEST(0 != url) ;
   TEST(0 == user_url(url)) ;
   TEST(0 == passwd_url(url)) ;
   TEST(0 != hostname_url(url)) ;
   TEST(0 == strcmp((const char*)hostname_url(url), "")) ;
   TEST(0 == port_url(url)) ;
   TEST(0 == path_url(url)) ;
   TEST(0 == query_url(url)) ;
   TEST(0 == fragment_url(url)) ;
   TEST(0 == encode_url(url, &wbuf)) ;
   TEST(strlen(test) == size_wbuffer(&wbuf)) ;
   TEST(0 == strncmp(test, str_cstring(&str), size_wbuffer(&wbuf))) ;
   TEST(0 == delete_url(&url)) ;
   TEST(0 == url) ;

   // TEST new_url: '/' marks begin of path
   test = "http://www.test.de:80/user1@/d1/?a_c#fragX" ;
   TEST(0 == new_url(&url, test)) ;
   TEST(0 != url) ;
   TEST(0 == user_url(url)) ;
   TEST(0 == passwd_url(url)) ;
   TEST(0 != hostname_url(url)) ;
   TEST(0 != port_url(url)) ;
   TEST(0 != path_url(url)) ;
   TEST(0 != query_url(url)) ;
   TEST(0 != fragment_url(url)) ;
   TEST(0 == strcmp((const char*)hostname_url(url), "www.test.de")) ;
   TEST(0 == strcmp((const char*)port_url(url), "80")) ;
   TEST(0 == strcmp((const char*)path_url(url), "user1@/d1/")) ;
   TEST(0 == strcmp((const char*)query_url(url), "a_c")) ;
   TEST(0 == strcmp((const char*)fragment_url(url), "fragX")) ;
   TEST(0 == encode_url(url, &wbuf)) ;
   test = "http://www.test.de:80/user1%40/d1/?a_c#fragX" ;
   TEST(strlen(test) == size_wbuffer(&wbuf)) ;
   TEST(0 == strncmp(test, str_cstring(&str), size_wbuffer(&wbuf))) ;
   TEST(0 == delete_url(&url)) ;
   TEST(0 == url) ;

   // TEST new_url: encoded parts
   test = "http://%00%11%22%33%44%55%66%77%88%99xX:99/%Aa%Bb%Cc%Dd%Ee%FfyY/?Query/#/%aA%bB%cC%dD%eE%fFzZ" ;
   TEST(0 == new_url(&url, test)) ;
   TEST(0 != url) ;
   TEST(0 == user_url(url)) ;
   TEST(0 == passwd_url(url)) ;
   TEST(0 != hostname_url(url)) ;
   TEST(0 != port_url(url)) ;
   TEST(0 != path_url(url)) ;
   TEST(0 != query_url(url)) ;
   TEST(0 != fragment_url(url)) ;
   TEST(0 == hostname_url(url)[0]) ;
   TEST(0 == strcmp(1+(const char*)hostname_url(url), "\x11\x22\x33\x44\x55\x66\x77\x88\x99xX")) ;
   TEST(0 == strcmp((const char*)port_url(url), "99")) ;
   TEST(0 == strcmp((const char*)path_url(url), "\xaa\xbb\xcc\xdd\xee\xffyY/")) ;
   TEST(0 == strcmp((const char*)query_url(url), "Query/")) ;
   TEST(0 == strcmp((const char*)fragment_url(url), "/\xaa\xbb\xcc\xdd\xee\xffzZ")) ;
   TEST(0 == encode_url(url, &wbuf)) ;
   test = "http://%00%11%223DUfw%88%99xX:99/%AA%BB%CC%DD%EE%FFyY/?Query%2F#%2F%AA%BB%CC%DD%EE%FFzZ" ;
   TEST(strlen(test) == size_wbuffer(&wbuf)) ;
   TEST(0 == strncmp(test, str_cstring(&str), size_wbuffer(&wbuf))) ;
   TEST(0 == delete_url(&url)) ;
   TEST(0 == url) ;

   // TEST new2_url
   test = "usr:pass@a%88%99b:44/%AA%BB%FF?_1#_2" ;
   TEST(0 == new2_url(&url, url_scheme_HTTP, test)) ;
   TEST(0 != url) ;
   TEST(0 != user_url(url)) ;
   TEST(0 != passwd_url(url)) ;
   TEST(0 != hostname_url(url)) ;
   TEST(0 != port_url(url)) ;
   TEST(0 != path_url(url)) ;
   TEST(0 != query_url(url)) ;
   TEST(0 != fragment_url(url)) ;
   TEST(0 == strcmp((const char*)hostname_url(url), "a\x88\x99""b")) ;
   TEST(0 == strcmp((const char*)port_url(url), "44")) ;
   TEST(0 == strcmp((const char*)path_url(url), "\xaa\xbb\xff")) ;
   TEST(0 == strcmp((const char*)query_url(url), "_1")) ;
   TEST(0 == strcmp((const char*)fragment_url(url), "_2")) ;
   TEST(0 == encode_url(url, &wbuf)) ;
   TEST(strlen(test)+7 == size_wbuffer(&wbuf)) ;
   TEST(0 == strncmp("http://", str_cstring(&str), 7)) ;
   TEST(0 == strncmp(test, 7+str_cstring(&str), strlen(test))) ;
   TEST(0 == delete_url(&url)) ;
   TEST(0 == url) ;

   // TEST new2_url: path only
   test = "/path%88%99x" ;
   TEST(0 == new2_url(&url, url_scheme_HTTP, test)) ;
   TEST(0 != url) ;
   TEST(0 == user_url(url)) ;
   TEST(0 == passwd_url(url)) ;
   TEST(0 != hostname_url(url)) ;
   TEST(0 == port_url(url)) ;
   TEST(0 != path_url(url)) ;
   TEST(0 == query_url(url)) ;
   TEST(0 == fragment_url(url)) ;
   TEST(0 == strcmp((const char*)hostname_url(url), "")) ;
   TEST(0 == strcmp((const char*)path_url(url), "path\x88\x99x")) ;
   TEST(0 == encode_url(url, &wbuf)) ;
   TEST(strlen(test)+7 == size_wbuffer(&wbuf)) ;
   TEST(0 == strncmp("http://", str_cstring(&str), 7)) ;
   TEST(0 == strncmp(test, 7+str_cstring(&str), strlen(test))) ;
   TEST(0 == delete_url(&url)) ;
   TEST(0 == url) ;

   // TEST new2_url: port + path only
   test = ":33/path%88%99%" ;
   TEST(0 == new2_url(&url, url_scheme_HTTP, test)) ;
   TEST(0 != url) ;
   TEST(0 == user_url(url)) ;
   TEST(0 == passwd_url(url)) ;
   TEST(0 != hostname_url(url)) ;
   TEST(0 != port_url(url)) ;
   TEST(0 != path_url(url)) ;
   TEST(0 == query_url(url)) ;
   TEST(0 == fragment_url(url)) ;
   TEST(0 == strcmp((const char*)hostname_url(url), "")) ;
   TEST(0 == strcmp((const char*)port_url(url), "33")) ;
   TEST(0 == strcmp((const char*)path_url(url), "path\x88\x99%")) ;
   TEST(0 == encode_url(url, &wbuf)) ;
   TEST(strlen(test)+9 == size_wbuffer(&wbuf)) ;
   TEST(0 == strncmp("http://", str_cstring(&str), 7)) ;
   TEST(0 == strncmp(test, 7+str_cstring(&str), strlen(test))) ;
   TEST(0 == strncmp("25", 7+strlen(test)+str_cstring(&str), 2)) ;
   TEST(0 == delete_url(&url)) ;
   TEST(0 == url) ;

   // TEST new2_url: username & path only
   test = "user%FF@/path%88%9" ;
   TEST(0 == new2_url(&url, url_scheme_HTTP, test)) ;
   TEST(0 != url) ;
   TEST(0 != user_url(url)) ;
   TEST(0 == passwd_url(url)) ;
   TEST(0 != hostname_url(url)) ;
   TEST(0 == port_url(url)) ;
   TEST(0 != path_url(url)) ;
   TEST(0 == query_url(url)) ;
   TEST(0 == fragment_url(url)) ;
   TEST(0 == strcmp((const char*)user_url(url), "user\xff")) ;
   TEST(0 == strcmp((const char*)hostname_url(url), "")) ;
   TEST(0 == strcmp((const char*)path_url(url), "path\x88%9")) ;
   TEST(0 == encode_url(url, &wbuf)) ;
   TEST(strlen(test)+9 == size_wbuffer(&wbuf)) ;
   TEST(0 == strncmp("http://", str_cstring(&str), 7)) ;
   TEST(0 == strncmp(test, 7+str_cstring(&str), strlen(test)-2)) ;
   TEST(0 == strncmp("%259", 5+strlen(test)+str_cstring(&str), 4)) ;
   TEST(0 == delete_url(&url)) ;
   TEST(0 == url) ;

   // TEST newparts_url: not encoded
   test = "us:pw@serv.xx@/@:/?@/#?/#:" ;
   url_parts_t parts = {
      string_INIT(2, (const uint8_t*)&test[0]), string_INIT(2, (const uint8_t*)&test[3]), string_INIT(8, (const uint8_t*)&test[6]), string_INIT_FREEABLE,
      string_INIT(3, (const uint8_t*)&test[15]), string_INIT(2, (const uint8_t*)&test[19]), string_INIT(4, (const uint8_t*)&test[22])
   } ;
   TEST(0 == newparts_url(&url, url_scheme_HTTP, &parts, false)) ;
   TEST(0 != url) ;
   TEST(0 != user_url(url)) ;
   TEST(0 != passwd_url(url)) ;
   TEST(0 != hostname_url(url)) ;
   TEST(0 == port_url(url)) ;
   TEST(0 != path_url(url)) ;
   TEST(0 != query_url(url)) ;
   TEST(0 != fragment_url(url)) ;
   TEST(0 == strcmp((const char*)user_url(url), "us")) ;
   TEST(0 == strcmp((const char*)passwd_url(url), "pw")) ;
   TEST(0 == strcmp((const char*)hostname_url(url), "serv.xx@")) ;
   TEST(0 == strcmp((const char*)path_url(url), "@:/")) ;
   TEST(0 == strcmp((const char*)query_url(url), "@/")) ;
   TEST(0 == strcmp((const char*)fragment_url(url), "?/#:")) ;
   TEST(0 == encode_url(url, &wbuf)) ;
   test = "http://us:pw@serv.xx%40/%40%3A/?%40%2F#%3F%2F%23%3A" ;
   TEST(strlen(test) == size_wbuffer(&wbuf)) ;
   TEST(0 == strncmp(test, str_cstring(&str), strlen(test))) ;
   TEST(0 == delete_url(&url)) ;
   TEST(0 == url) ;

   // TEST newparts_url: user + undefined hostname
   test = "http://12:3@/path?q#f" ;
   memcpy( parts, (url_parts_t) {
      { (const uint8_t*)&test[7], 2 },  { (const uint8_t*)&test[10], 1 }, { 0, 0}, {0, 0},
      { (const uint8_t*)&test[13], 4 }, { (const uint8_t*)&test[18], 1},  { (const uint8_t*)&test[20], 1 }
   }, sizeof(parts)) ;
   TEST(0 == newparts_url(&url, url_scheme_HTTP, &parts, false)) ;
   TEST(0 != url) ;
   TEST(0 != user_url(url)) ;
   TEST(0 != passwd_url(url)) ;
   TEST(0 == hostname_url(url)) ;
   TEST(0 == port_url(url)) ;
   TEST(0 != path_url(url)) ;
   TEST(0 != query_url(url)) ;
   TEST(0 != fragment_url(url)) ;
   TEST(0 == strcmp((const char*)user_url(url), "12")) ;
   TEST(0 == strcmp((const char*)passwd_url(url), "3")) ;
   TEST(0 == strcmp((const char*)path_url(url), "path")) ;
   TEST(0 == strcmp((const char*)query_url(url), "q")) ;
   TEST(0 == strcmp((const char*)fragment_url(url), "f")) ;
   TEST(0 == encode_url(url, &wbuf)) ;
   TEST(strlen(test) == size_wbuffer(&wbuf)) ;
   TEST(0 == strncmp(test, str_cstring(&str), strlen(test))) ;
   TEST(0 == delete_url(&url)) ;
   TEST(0 == url) ;

   // unprepare
   TEST(0 == free_cstring(&str)) ;

   return 0 ;
ONABORT:
   delete_url(&url) ;
   free_cstring(&str) ;
   return EINVAL ;
}

int unittest_io_url()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_url_initfree())  goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif

