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
#include "C-kern/api/string/encode.h"
#include "C-kern/api/string/string.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
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

   if (err) goto ABBRUCH ;

   *encodedstr = next ;
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

static int parsepart_url(url_part_e part, url_parts_t * parts, const char ** encodedstr, char endmarker)
{
   const char * start = *encodedstr ;
   const char * next  = start ;
   char         c ;

   assert((unsigned char)endmarker < 0x80) ;

   for(c = *next; endmarker != c; c = *next) {
      ++ next ;
   }

   (*parts)[part].addr  = start ;
   (*parts)[part].size  = (size_t) (next - start) ;
   *encodedstr          = next ;

   return 0 ;
}

static int parsepart2_url(url_part_e part, url_parts_t * parts, const char ** encodedstr, char endmarker1, char endmarker2)
{
   const char * start = *encodedstr ;
   const char * next  = start ;
   char         c ;

   assert((unsigned char)endmarker1 < 0x80) ;
   assert((unsigned char)endmarker2 < 0x80) ;

   for(c = *next; endmarker1 != c && endmarker2 != c; c = *next) {
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
   size_t      decoded_sizes[nrelementsof(*parts)] ;
   url_t       * newurl   = 0 ;
   size_t      len        = 0 ;
   uint16_t    buffidx    = 0 ;

   for(unsigned i = 0; i < nrelementsof(*parts); ++i) {
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
      goto ABBRUCH ;
   }

   size_t objsize = sizeof(url_t) + len ;
   newurl = malloc(objsize) ;
   if (!newurl) {
      err = ENOMEM ;
      LOG_OUTOFMEMORY(objsize) ;
      goto ABBRUCH ;
   }

   // init newurl
   newurl->scheme = scheme ;
   for(unsigned i = 0; i < nrelementsof(*parts); ++i) {
      if ((*parts)[i].addr) {
         size_t decoded_size = decoded_sizes[i] ;
         if (decoded_size < (*parts)[i].size) {
            wbuffer_t decoded = wbuffer_INIT_STATIC(decoded_size, (uint8_t*) &newurl->buffer[buffidx]) ;

            err = urldecode_string(&(*parts)[i], &decoded) ;
            if (err) goto ABBRUCH ;

         } else {
            memcpy( &newurl->buffer[buffidx], (*parts)[i].addr, decoded_size) ;
         }

         buffidx = (uint16_t) (buffidx + decoded_size) ;
         newurl->buffer[buffidx ++] = 0 ;
      }
      static_assert( nrelementsof(newurl->parts) == nrelementsof(*parts), ) ;
      newurl->parts[i] = buffidx ;
   }

   assert(len == buffidx) ;

   *url = newurl ;
   return 0 ;
ABBRUCH:
   free(newurl) ;
   LOG_ABORT(err) ;
   return 0 ;
}

int new2_url(/*out*/url_t ** url, url_scheme_e scheme, const char * encodedstr)
{
   int err ;
   const char     * pos ;
   const char     * next     = encodedstr ;
   const char     * slashpos = strchrnul(encodedstr, '/') ;
   url_parts_t    parts      = url_parts_INIT_FREEABLE ;

   PRECONDITION_INPUT( (unsigned)scheme <= url_scheme_HTTP, ABBRUCH, LOG_INT(scheme)) ;

   if (     (pos = strchrnul(next, '@'))
         && pos < slashpos) {
      err = parsepart2_url(url_part_USER, &parts, &next, ':', '@') ;
      if (err) goto ABBRUCH ;
      if (pos > next) {
         ++ next ;
         err = parsepart_url(url_part_PASSWD, &parts, &next, '@') ;
         if (err) goto ABBRUCH ;
      }
      if (pos != next) {
         err = EINVAL ;
         goto ABBRUCH ;
      }
      ++ next ;
   }

   err = parsepart2_url(url_part_HOSTNAME, &parts, &next, ':', *slashpos) ;
   if (err) goto ABBRUCH ;

   if (':' == *next) {
      ++ next ;
      err = parsepart_url(url_part_PORT, &parts, &next, *slashpos) ;
      if (err) goto ABBRUCH ;
      for(const char * nr = parts[url_part_PORT].addr; nr < next; ++nr) {
         if (!('0' <= *nr && *nr <= '9')) {
            err = EINVAL ;
            goto ABBRUCH ;
         }
      }
   }

   pos = strchrnul(next, '?') ;
   if (0 == *pos) {
      pos = strchrnul(next, '#') ;
   }

   if (*next) {
      ++ next ;
      err = parsepart_url(url_part_PATH, &parts, &next, *pos) ;
      if (err) goto ABBRUCH ;
   }

   if ('?' == *next) {
      ++ next ;
      pos = strchrnul(next, '#') ;
      err = parsepart_url(url_part_QUERY, &parts, &next, *pos) ;
      if (err) goto ABBRUCH ;
   }

   if ('#' == *next) {
      ++ next ;
      err = parsepart_url(url_part_FRAGMENT, &parts, &next, 0) ;
      if (err) goto ABBRUCH ;
   }

   err = newparts_url(url, scheme, &parts, true) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int new_url(/*out*/url_t ** url, const char * encodedstr)
{
   int err ;
   url_scheme_e   scheme ;
   const char     * next = encodedstr ;

   err = parse_urlscheme(&scheme, &next) ;
   if (err) goto ABBRUCH ;

   if (  '/' != next[0]
      || '/' != next[1]) {
      err = EINVAL ;
      goto ABBRUCH ;
   }
   next += 2 ;

   err = new2_url(url, scheme, next) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
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

int encode_url(const url_t * url, wbuffer_t * encoded_url_string)
{
   int err ;
   size_t      sizeencoding[nrelementsof(url->parts)] = { 0 } ;
   size_t      result_size = 0 ;
   const char  * extrachar ;
   uint8_t     * start_result = 0 ;

   // sizeof("http://") includes trailing \0 byte
   switch(url->scheme) {
   case url_scheme_HTTP:   result_size = sizeof("http://")-1 ; break ;
   default:                err = EINVAL ; goto ABBRUCH ;
   }

   // calculate total length of result
   static_assert( 2 == sizeof(url->parts[0]), ) ;
   uint16_t buffer_offset = 0 ;
   for(unsigned i = 0; i < nrelementsof(url->parts); ++i) {
      size_t size = (size_t) (url->parts[i] - buffer_offset) ;
      if (size) {
         -- size ; // not consider trailing '\0' byte
         string_t part = string_INIT(size, &url->buffer[buffer_offset]) ;
         if (url_part_HOSTNAME == i) -- result_size ; // no special marker for hostname
         if (i < url_part_HOSTNAME) {
            extrachar = "@/" ;
         } else if (i < url_part_PATH) {
            extrachar = "/" ;
         } else {
            extrachar = 0 ;
         }
         sizeencoding[i]  = sizeurlencode_string(&part, extrachar) ;
         result_size     += 1 + sizeencoding[i] ;
      }
      buffer_offset = url->parts[i] ;
   }

   err = appendalloc_wbuffer(encoded_url_string, result_size, &start_result) ;
   if (err) goto ABBRUCH ;

   // encode & copy parts to result
   bool     isuser   = false ;
   uint8_t  * result = start_result ;
   memcpy( result, "http://", sizeof("http://")-1) ;
   result += sizeof("http://")-1 ;
   buffer_offset = 0 ;
   for(unsigned i = 0; i < nrelementsof(url->parts); ++i) {
      size_t size = (size_t) (url->parts[i] - buffer_offset) ;
      if (size) {
         -- size ; // copy not trailing '\0' byte
         switch((url_part_e)i) {
         case url_part_USER:     isuser = true ;      break ;
         case url_part_PASSWD:   *(result++) = ':' ;  break ;
         case url_part_HOSTNAME: if (isuser) *(result++) = '@' ; isuser = false ; break ;
         case url_part_PORT:     *(result++) = ':' ;  break ;
         case url_part_PATH:     *(result++) = '/' ;  break ;
         case url_part_QUERY:    *(result++) = '?' ;  break ;
         case url_part_FRAGMENT: *(result++) = '#' ;  break ;
         }
         const char * next = &url->buffer[buffer_offset] ;
         if (sizeencoding[i] > size) {
            if (i < url_part_HOSTNAME) {
               extrachar  = "@/" ;
            } else if (i < url_part_PATH) {
               extrachar  = "/" ;
            } else {
               extrachar = 0 ;
            }

            wbuffer_t encoded = wbuffer_INIT_STATIC(sizeencoding[i], result) ;

            err = urlencode_string(&(string_t)string_INIT(size, next), extrachar, &encoded) ;
            if (err) goto ABBRUCH ;

         } else {
            memcpy(result, next, sizeencoding[i]) ;
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
ABBRUCH:
   if (start_result) popbytes_wbuffer(encoded_url_string, result_size) ;
   LOG_ABORT(err) ;
   return err ;
}



#ifdef KONFIG_UNITTEST

#define TEST(ARG) TEST_ONERROR_GOTO(ARG, ABBRUCH)

static int test_url_initfree(void)
{
   url_t      * url  = 0 ;
   wbuffer_t    str  = wbuffer_INIT ;
   const char * test ;

   // TEST init, double free
   TEST(0 == new_url(&url, "http://127.0.0.1/")) ;
   TEST(0 != url) ;
   TEST(0 == delete_url(&url)) ;
   TEST(0 == url) ;
   TEST(0 == delete_url(&url)) ;
   TEST(0 == url) ;

   // TEST full url
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
   TEST(0 == strcmp(user_url(url), "user1")) ;
   TEST(0 == strcmp(passwd_url(url), "passwd2")) ;
   TEST(0 == strcmp(hostname_url(url), "server3.de")) ;
   TEST(0 == strcmp(port_url(url), "123")) ;
   TEST(0 == strcmp(path_url(url), "d1/d2")) ;
   TEST(0 == strcmp(query_url(url), "x=a")) ;
   TEST(0 == strcmp(fragment_url(url), "frag9")) ;
   TEST(0 == encode_url(url, &str)) ;
   TEST(strlen(test) == sizecontent_wbuffer(&str)) ;
   TEST(0 == strncmp(test, (char*)content_wbuffer(&str), sizecontent_wbuffer(&str))) ;
   reset_wbuffer(&str);
   TEST(0 == delete_url(&url)) ;
   TEST(0 == url) ;

   // TEST null or empty
   test = "http://" ;
   TEST(0 == new_url(&url, test)) ;
   TEST(0 != url) ;
   TEST(0 == user_url(url)) ;
   TEST(0 == passwd_url(url)) ;
   TEST(0 != hostname_url(url)) ;
   TEST(0 == strcmp(hostname_url(url), "")) ;
   TEST(0 == port_url(url)) ;
   TEST(0 == path_url(url)) ;
   TEST(0 == query_url(url)) ;
   TEST(0 == fragment_url(url)) ;
   TEST(0 == encode_url(url, &str)) ;
   TEST(strlen(test) == sizecontent_wbuffer(&str)) ;
   TEST(0 == strncmp(test, (char*)content_wbuffer(&str), sizecontent_wbuffer(&str))) ;
   reset_wbuffer(&str);
   TEST(0 == delete_url(&url)) ;
   TEST(0 == url) ;

   // TEST '/' marks begin of path
   test = "http://www.test.de:80/user1@/d1/?a=b#fragX" ;
   TEST(0 == new_url(&url, test)) ;
   TEST(0 != url) ;
   TEST(0 == user_url(url)) ;
   TEST(0 == passwd_url(url)) ;
   TEST(0 != hostname_url(url)) ;
   TEST(0 != port_url(url)) ;
   TEST(0 != path_url(url)) ;
   TEST(0 != query_url(url)) ;
   TEST(0 != fragment_url(url)) ;
   TEST(0 == strcmp(hostname_url(url), "www.test.de")) ;
   TEST(0 == strcmp(port_url(url), "80")) ;
   TEST(0 == strcmp(path_url(url), "user1@/d1/")) ;
   TEST(0 == strcmp(query_url(url), "a=b")) ;
   TEST(0 == strcmp(fragment_url(url), "fragX")) ;
   TEST(0 == encode_url(url, &str)) ;
   TEST(strlen(test) == sizecontent_wbuffer(&str)) ;
   TEST(0 == strncmp(test, (char*)content_wbuffer(&str), sizecontent_wbuffer(&str))) ;
   reset_wbuffer(&str);
   TEST(0 == delete_url(&url)) ;
   TEST(0 == url) ;

   // TEST encoded parts
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
   TEST(0 == strcmp(1+hostname_url(url), "\x11\x22\x33\x44\x55\x66\x77\x88\x99xX")) ;
   TEST(0 == strcmp(port_url(url), "99")) ;
   TEST(0 == strcmp(path_url(url), "\xaa\xbb\xcc\xdd\xee\xffyY/")) ;
   TEST(0 == strcmp(query_url(url), "Query/")) ;
   TEST(0 == strcmp(fragment_url(url), "/\xaa\xbb\xcc\xdd\xee\xffzZ")) ;
   TEST(0 == encode_url(url, &str)) ;
   test = "http://%00%11%223DUfw%88%99xX:99/%AA%BB%CC%DD%EE%FFyY/?Query/#/%AA%BB%CC%DD%EE%FFzZ" ;
   TEST(strlen(test) == sizecontent_wbuffer(&str)) ;
   TEST(0 == strncmp(test, (char*)content_wbuffer(&str), sizecontent_wbuffer(&str))) ;
   reset_wbuffer(&str);
   TEST(0 == delete_url(&url)) ;
   TEST(0 == url) ;

   // TEST new2
   test = "usr:pass@a%88%99b:44/%AA%BB%FF?@1/#@2" ;
   TEST(0 == new2_url(&url, url_scheme_HTTP, test)) ;
   TEST(0 != url) ;
   TEST(0 != user_url(url)) ;
   TEST(0 != passwd_url(url)) ;
   TEST(0 != hostname_url(url)) ;
   TEST(0 != port_url(url)) ;
   TEST(0 != path_url(url)) ;
   TEST(0 != query_url(url)) ;
   TEST(0 != fragment_url(url)) ;
   TEST(0 == strcmp(hostname_url(url), "a\x88\x99""b")) ;
   TEST(0 == strcmp(port_url(url), "44")) ;
   TEST(0 == strcmp(path_url(url), "\xaa\xbb\xff")) ;
   TEST(0 == strcmp(query_url(url), "@1/")) ;
   TEST(0 == strcmp(fragment_url(url), "@2")) ;
   TEST(0 == encode_url(url, &str)) ;
   TEST(strlen(test)+7 == sizecontent_wbuffer(&str)) ;
   TEST(0 == strncmp("http://", (char*)content_wbuffer(&str), 7)) ;
   TEST(0 == strncmp(test, 7+(char*)content_wbuffer(&str), strlen(test))) ;
   reset_wbuffer(&str);
   TEST(0 == delete_url(&url)) ;
   TEST(0 == url) ;

   // TEST new2 path only
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
   TEST(0 == strcmp(hostname_url(url), "")) ;
   TEST(0 == strcmp(path_url(url), "path\x88\x99x")) ;
   TEST(0 == encode_url(url, &str)) ;
   TEST(strlen(test)+7 == sizecontent_wbuffer(&str)) ;
   TEST(0 == strncmp("http://", (char*)content_wbuffer(&str), 7)) ;
   TEST(0 == strncmp(test, 7+(char*)content_wbuffer(&str), strlen(test))) ;
   reset_wbuffer(&str);
   TEST(0 == delete_url(&url)) ;
   TEST(0 == url) ;

   // TEST new2 port + path only
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
   TEST(0 == strcmp(hostname_url(url), "")) ;
   TEST(0 == strcmp(port_url(url), "33")) ;
   TEST(0 == strcmp(path_url(url), "path\x88\x99%")) ;
   TEST(0 == encode_url(url, &str)) ;
   TEST(strlen(test)+9 == sizecontent_wbuffer(&str)) ;
   TEST(0 == strncmp("http://", (char*)content_wbuffer(&str), 7)) ;
   TEST(0 == strncmp(test, 7+(char*)content_wbuffer(&str), strlen(test))) ;
   TEST(0 == strncmp("25", 7+strlen(test)+(char*)content_wbuffer(&str), 2)) ;
   reset_wbuffer(&str);
   TEST(0 == delete_url(&url)) ;
   TEST(0 == url) ;

   // TEST new2 username & path only
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
   TEST(0 == strcmp(user_url(url), "user\xff")) ;
   TEST(0 == strcmp(hostname_url(url), "")) ;
   TEST(0 == strcmp(path_url(url), "path\x88%9")) ;
   TEST(0 == encode_url(url, &str)) ;
   TEST(strlen(test)+9 == sizecontent_wbuffer(&str)) ;
   TEST(0 == strncmp("http://", (char*)content_wbuffer(&str), 7)) ;
   TEST(0 == strncmp(test, 7+(char*)content_wbuffer(&str), strlen(test)-2)) ;
   TEST(0 == strncmp("%259", 5+strlen(test)+(char*)content_wbuffer(&str), 4)) ;
   reset_wbuffer(&str);
   TEST(0 == delete_url(&url)) ;
   TEST(0 == url) ;

   // TEST newparts not encoded
   test = "us:pw@serv.xx@/@:/?@/#?/#:" ;
   url_parts_t parts = {
      string_INIT(2, &test[0]), string_INIT(2, &test[3]), string_INIT(8, &test[6]), string_INIT_FREEABLE,
      string_INIT(3, &test[15]), string_INIT(2, &test[19]), string_INIT(4, &test[22])
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
   TEST(0 == strcmp(user_url(url), "us")) ;
   TEST(0 == strcmp(passwd_url(url), "pw")) ;
   TEST(0 == strcmp(hostname_url(url), "serv.xx@")) ;
   TEST(0 == strcmp(path_url(url), "@:/")) ;
   TEST(0 == strcmp(query_url(url), "@/")) ;
   TEST(0 == strcmp(fragment_url(url), "?/#:")) ;
   TEST(0 == encode_url(url, &str)) ;
   test = "http://us:pw@serv.xx@/@%3A/?@/#%3F/%23%3A" ;
   TEST(strlen(test) == sizecontent_wbuffer(&str)) ;
   TEST(0 == strncmp(test, (char*)content_wbuffer(&str), strlen(test))) ;
   reset_wbuffer(&str);
   TEST(0 == delete_url(&url)) ;
   TEST(0 == url) ;

   // TEST newparts user + undefined hostname
   test = "http://12:3@/path?q#f" ;
   memcpy( parts, (url_parts_t) {
      { &test[7], 2 },  { &test[10], 1 }, { 0, 0}, {0, 0},
      { &test[13], 4 }, { &test[18], 1},  { &test[20], 1 }
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
   TEST(0 == strcmp(user_url(url), "12")) ;
   TEST(0 == strcmp(passwd_url(url), "3")) ;
   TEST(0 == strcmp(path_url(url), "path")) ;
   TEST(0 == strcmp(query_url(url), "q")) ;
   TEST(0 == strcmp(fragment_url(url), "f")) ;
   TEST(0 == encode_url(url, &str)) ;
   TEST(strlen(test) == sizecontent_wbuffer(&str)) ;
   TEST(0 == strncmp(test, (char*)content_wbuffer(&str), strlen(test))) ;
   reset_wbuffer(&str);
   TEST(0 == delete_url(&url)) ;
   TEST(0 == url) ;

   TEST(0 == free_wbuffer(&str)) ;

   return 0 ;
ABBRUCH:
   delete_url(&url) ;
   free_wbuffer(&str) ;
   return EINVAL ;
}

int unittest_io_url()
{
   resourceusage_t usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_url_initfree())  goto ABBRUCH ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif

