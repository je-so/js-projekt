/* title: String impl
   Implements <String>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/string/string.h
    Header file of <String>.

   file: C-kern/string/string.c
    Implementation file <String impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/string/string.h"
#include "C-kern/api/string/stringstream.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif


// section: string_t

// group: lifetime

int initse_string(/*out*/string_t * str, const uint8_t * start, const uint8_t * end)
{
   int err ;

   VALIDATE_INPARAM_TEST((uintptr_t)end >= (uintptr_t)start, ONERR, ) ;

   str->addr = start ;
   str->size = (size_t) (end - start) ;

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}

int initsubstr_string(/*out*/string_t * str, const string_t * restrict fromstr, size_t start_offset, size_t size)
{
   int err ;
   const size_t maxsize = size_string(fromstr) - start_offset ;

   VALIDATE_INPARAM_TEST(start_offset <= size_string(fromstr) && size <= maxsize, ONERR, ) ;

   str->addr = fromstr->addr + start_offset ;
   str->size = size ;

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}

void initPstream_string(/*out*/string_t * str, const struct stringstream_t * strstream)
{
   str->addr = next_stringstream(strstream) ;
   str->size = size_stringstream(strstream) ;
}

// group: compare

bool isequalasciicase_string(const string_t * str, const string_t * str2)
{
   if (str->size != str2->size) return false ;

   for (size_t i = str->size; (i --);) {
      uint8_t c1 = str->addr[i] ;
      uint8_t c2 = str2->addr[i] ;
      uint8_t x  = c1 ^ c2 ;
      if (x) {
         static_assert(0x20 == ('a' - 'A'), ) ;
         if (0x20 == x) {
            x |= c2 ;
            if ('a' <= x && x <= 'z') continue ;
         }
         return false ;
      }
   }

   return true ;
}

// group: change

int substr_string(string_t * str, size_t start_offset, size_t size)
{
   int err ;
   const size_t maxsize = size_string(str) - start_offset ;

   VALIDATE_INPARAM_TEST(start_offset <= size_string(str) && size <= maxsize, ONERR, ) ;

   str->addr += start_offset ;
   str->size  = size ;

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}

int shrinkleft_string(string_t * str, size_t nr_bytes_removed_from_string_start)
{
   int err ;

   VALIDATE_INPARAM_TEST(nr_bytes_removed_from_string_start <= size_string(str), ONERR, ) ;

   str->addr += nr_bytes_removed_from_string_start ;
   str->size -= nr_bytes_removed_from_string_start ;

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}

int shrinkright_string(string_t * str, size_t nr_bytes_removed_from_string_end)
{
   int err ;

   VALIDATE_INPARAM_TEST(nr_bytes_removed_from_string_end <= size_string(str), ONERR, ) ;

   str->size -= nr_bytes_removed_from_string_end ;

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   string_t       str ;
   stringstream_t strstream ;
   uint8_t        buffer[256] ;

   // TEST string_FREE
   str = (string_t) string_FREE ;
   TEST(0 == str.addr) ;
   TEST(0 == str.size) ;
   TEST(1 == isempty_string(&str)) ;
   TEST(1 == isfree_string(&str)) ;

   // TEST string_INIT
   str = (string_t) string_INIT(5, &buffer[10]) ;
   TEST(str.addr == &buffer[10]) ;
   TEST(str.size == 5) ;
   str = (string_t) string_INIT(3, &buffer[11]) ;
   TEST(str.addr == &buffer[11]) ;
   TEST(str.size == 3) ;

   // TEST string_INIT_CSTR
   const char * cstr[] = { "123", "123456" } ;
   for (unsigned i = 0; i < lengthof(cstr); ++i) {
      str = (string_t) string_INIT_CSTR(cstr[i]) ;
      TEST(str.addr == (const uint8_t*)cstr[i]) ;
      TEST(str.size == strlen(cstr[i])) ;
   }

   // TEST init_string
   init_string(&str, 3, &buffer[12]) ;
   TEST(str.addr == &buffer[12]) ;
   TEST(str.size == 3) ;
   init_string(&str, 5, &buffer[13]) ;
   TEST(str.addr == &buffer[13]) ;
   TEST(str.size == 5) ;

   // TEST init_string: free string
   init_string(&str, 0, 0) ;
   TEST(0 == str.addr) ;
   TEST(0 == str.size) ;

   // TEST init_string: empty string
   init_string(&str, 0, (const uint8_t*)"") ;
   TEST(str.addr == (const uint8_t*)"") ;
   TEST(str.size == 0) ;

   // TEST initcopy_string
   string_t srcstr ;
   for (unsigned i = 0; i <= sizeof(buffer); ++i) {
      srcstr = (string_t) string_INIT(sizeof(buffer)-i, buffer+i) ;
      initcopy_string(&str, &srcstr) ;
      TEST(addr_string(&str) == buffer+i) ;
      TEST(size_string(&str) == sizeof(buffer)-i) ;
   }

   // TEST initsubstr_string
   srcstr = (string_t) string_INIT(sizeof(buffer), buffer) ;
   for (unsigned i = 0; i <= sizeof(buffer); ++i) {
      TEST(0 == initsubstr_string(&str, &srcstr, i, sizeof(buffer)-i)) ;
      TEST(addr_string(&str) == buffer+i) ;
      TEST(size_string(&str) == sizeof(buffer)-i) ;
   }

   // TEST initsubstr_string: EINVAL
   srcstr = (string_t) string_INIT(sizeof(buffer), buffer) ;
   TEST(EINVAL == initsubstr_string(&str, &srcstr, sizeof(buffer)+1, 0)) ;
   TEST(EINVAL == initsubstr_string(&str, &srcstr, 0, sizeof(buffer)+1)) ;

   // TEST initse_string
   TEST(0 == initse_string(&str,  &buffer[21], &buffer[24])) ;
   TEST(str.addr == &buffer[21]) ;
   TEST(str.size == 3) ;
   TEST(0 == initse_string(&str, &buffer[24], &buffer[29])) ;
   TEST(str.addr == &buffer[24]) ;
   TEST(str.size == 5) ;
   TEST(0 == initse_string(&str, &buffer[25], &buffer[26])) ;
   TEST(str.addr == &buffer[25]) ;
   TEST(str.size == 1) ;

   // TEST initse_string: empty string
   TEST(0 == initse_string(&str,  &buffer[2], &buffer[2])) ;
   TEST(str.addr == &buffer[2]) ;
   TEST(str.size == 0) ;

   // TEST initse_string: EINVAL
   TEST(0 == initse_string(&str,  &buffer[1], &buffer[4])) ;
   TEST(EINVAL == initse_string(&str,  &buffer[2], &buffer[1])) ;
   TEST(str.addr == &buffer[1]) ;
   TEST(str.size == 3) ;

   // TEST initPstream_string
   for (unsigned i = 0; i <= sizeof(buffer); ++i) {
      TEST(0 == init_stringstream(&strstream, buffer+i, buffer+sizeof(buffer))) ;
      initPstream_string(&str, &strstream) ;
      TEST(addr_string(&str) == buffer+i) ;
      TEST(size_string(&str) == sizeof(buffer)-i) ;
   }

   return 0 ;
ONERR:
   return EINVAL ;
}


static int test_query(void)
{
   string_t       str ;
   uint8_t        buffer[256] ;

   // prepare
   for (unsigned i = 0; i < sizeof(buffer); ++i) {
      buffer[i] = (uint8_t)i ;
   }

   // TEST isempty_string: checks only size == 0
   str.addr = (const uint8_t*) "" ;
   str.size = 0 ;
   TEST(1 == isempty_string(&str)) ;
   str.size = 1 ;
   TEST(0 == isempty_string(&str)) ;
   str.addr = 0 ;
   TEST(0 == isempty_string(&str)) ;
   str.size = 0 ;
   TEST(1 == isempty_string(&str)) ;

   // TEST isfree_string: checks size == 0 && addr == 0
   str = (string_t) string_FREE ;
   TEST(1 == isfree_string(&str)) ;
   str.addr = (const uint8_t*) "" ;
   TEST(0 == isfree_string(&str)) ;
   str.addr = 0 ;
   str.size = 1 ;
   TEST(0 == isfree_string(&str)) ;
   str.size = 0 ;
   TEST(1 == isfree_string(&str)) ;

   // TEST addr_string
   for (unsigned i = 0; i < 256; ++i) {
      init_string(&str, 0, (uint8_t*)i) ;
      TEST(addr_string(&str) == (uint8_t*)i) ;
   }

   // TEST size_string
   for (unsigned i = 0; i < 256; ++i) {
      init_string(&str, i, 0) ;
      TEST(size_string(&str) == i) ;
   }

   // TEST findbyte_string
   init_string(&str, sizeof(buffer), buffer) ;
   for (unsigned i = 0; i < sizeof(buffer); ++i) {
      buffer[i] = (uint8_t)(i+1) ;
      TEST(findbyte_string(&str, i) == 0/*not found*/) ;
      buffer[i] = (uint8_t)i ;
      TEST(findbyte_string(&str, i) == buffer+i/*found*/) ;
   }

   return 0 ;
ONERR:
   return EINVAL ;
}

static int test_compare(void)
{
   string_t    str1 ;
   string_t    str2 ;
   const char  * utf8str1 ;
   const char  * utf8str2 ;

   // TEST isequalasciicase_string simple utf8 string
   utf8str1 = "\U001fffff abcd\U0000ffff abcd\u07ff abcd\x7f abcd" ;
   utf8str2 = "\U001fffff ABCD\U0000ffff ABCD\u07ff abcd\x7f ABCD" ;
   init_string(&str1, strlen(utf8str1), (const uint8_t*)utf8str1) ;
   init_string(&str2, strlen(utf8str2), (const uint8_t*)utf8str2) ;
   TEST(true == isequalasciicase_string(&str1, &str2)) ;

   // TEST isequalasciicase_string with all characters from A-Z
   utf8str1 = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@^¬^°üöä" ;
   utf8str2 = "abcdefghijklmnopqrstuvwxyz1234567890!@^¬^°üöä" ;
   init_string(&str1, strlen(utf8str1), (const uint8_t*)utf8str1) ;
   init_string(&str2, strlen(utf8str2), (const uint8_t*)utf8str2) ;
   TEST(true == isequalasciicase_string(&str1, &str2)) ;
   TEST(true == isequalasciicase_string(&str1, &str1)) ;
   TEST(true == isequalasciicase_string(&str2, &str2)) ;

   // TEST isequalasciicase_string inequality for all other 256 characters
   uint8_t buffer1[256] ;
   uint8_t buffer2[256] ;
   for (unsigned i = 0; i < sizeof(buffer1); ++i) {
      buffer1[i] = (uint8_t)i ;
      buffer2[i] = (uint8_t)i ;
   }
   init_string(&str1, sizeof(buffer1), buffer1) ;
   init_string(&str2, sizeof(buffer2), buffer2) ;
   TEST(true == isequalasciicase_string(&str1, &str2)) ;
   for (unsigned i = 0; i < sizeof(buffer1); ++i) {
      bool isEqual = (  ('a' <= i && i <= 'z')
                     || ('A' <= i && i <= 'Z')) ;
      buffer1[i] ^= 0x20 ; // change case (works only for a-z or A-Z)
      TEST(isEqual == isequalasciicase_string(&str1, &str2)) ;
      buffer1[i] = (uint8_t)i ;
      buffer2[i] ^= 0x20 ; // change case (works only for a-z or A-Z)
      TEST(isEqual == isequalasciicase_string(&str1, &str2)) ;
      buffer2[i] = (uint8_t)i ;
   }

   // TEST isequalasciicase_string inequality for size
   for (unsigned i = 0; i < sizeof(buffer1); ++i) {
      TEST(true == isequalasciicase_string(&str1, &str2)) ;
      str1.size = i ;
      TEST(false == isequalasciicase_string(&str1, &str2)) ;
      str1.size = sizeof(buffer1) ;
      str2.size = i ;
      TEST(false == isequalasciicase_string(&str1, &str2)) ;
      str2.size = sizeof(buffer2) ;
   }

   // TEST isequalasciicase_string inequality for one differing char
   for (unsigned i = 0; i < sizeof(buffer1); ++i) {
      TEST(true == isequalasciicase_string(&str1, &str2)) ;
      buffer1[i] = (uint8_t)(i+1) ;
      TEST(false == isequalasciicase_string(&str1, &str2)) ;
      buffer1[i] = (uint8_t)i ;
      buffer2[i] = (uint8_t)(i+1) ;
      TEST(false == isequalasciicase_string(&str1, &str2)) ;
      buffer2[i] = (uint8_t)i ;
   }

   return 0 ;
ONERR:
   return EINVAL ;
}

static int test_change(void)
{
   string_t str ;
   uint8_t  buffer[256] ;

   // TEST substr_string
   for (size_t i = 0; i <= sizeof(buffer); ++i) {
      init_string(&str, sizeof(buffer), buffer) ;
      TEST(0 == substr_string(&str, 0, i)) ;
      TEST(addr_string(&str) == buffer) ;
      TEST(size_string(&str) == i) ;
      init_string(&str, sizeof(buffer), buffer) ;
      TEST(0 == substr_string(&str, i, 0)) ;
      TEST(addr_string(&str) == buffer + i) ;
      TEST(size_string(&str) == 0) ;
      init_string(&str, sizeof(buffer), buffer) ;
      TEST(0 == substr_string(&str, i, sizeof(buffer)-i)) ;
      TEST(addr_string(&str) == buffer + i) ;
      TEST(size_string(&str) == sizeof(buffer)-i) ;
   }

   // TEST substr_string: EINVAL
   init_string(&str, sizeof(buffer), buffer) ;
   TEST(EINVAL == substr_string(&str, sizeof(buffer)+1, 0)) ;
   TEST(EINVAL == substr_string(&str, (size_t)-1, 0)) ;
   TEST(addr_string(&str) == buffer) ;
   TEST(size_string(&str) == sizeof(buffer)) ;
   TEST(EINVAL == substr_string(&str, 0, sizeof(buffer)+1)) ;
   TEST(EINVAL == substr_string(&str, 0, (size_t)-1)) ;
   TEST(addr_string(&str) == buffer) ;
   TEST(size_string(&str) == sizeof(buffer)) ;

   // TEST shrinkleft_string
   for (size_t i = 0; i <= sizeof(buffer); ++i) {
      init_string(&str, sizeof(buffer), buffer) ;
      TEST(0 == shrinkleft_string(&str, i)) ;
      TEST(addr_string(&str) == buffer+i) ;
      TEST(size_string(&str) == sizeof(buffer)-i) ;
   }

   // TEST shrinkleft_string: EINVAL
   init_string(&str, sizeof(buffer), buffer) ;
   TEST(EINVAL == shrinkleft_string(&str, sizeof(buffer)+1)) ;
   TEST(addr_string(&str) == buffer) ;
   TEST(size_string(&str) == sizeof(buffer)) ;

   // TEST shrinkright_string
   for (size_t i = 0; i <= sizeof(buffer); ++i) {
      init_string(&str, sizeof(buffer), buffer) ;
      TEST(0 == shrinkright_string(&str, i)) ;
      TEST(addr_string(&str) == buffer) ;
      TEST(size_string(&str) == sizeof(buffer)-i) ;
   }

   // TEST shrinkright_string: EINVAL
   init_string(&str, sizeof(buffer), buffer) ;
   TEST(EINVAL == shrinkright_string(&str, sizeof(buffer)+1)) ;
   TEST(addr_string(&str) == buffer) ;
   TEST(size_string(&str) == sizeof(buffer)) ;

   // TEST skipbyte_string
   str = (string_t) string_INIT(0, (uint8_t*)0) ;
   skipbyte_string(&str) ;
   TEST(str.addr == (uint8_t*)1) ;
   TEST(str.size == (size_t)-1) ;
   str = (string_t) string_INIT(44, (uint8_t*)0) ;
   for (unsigned i = 1; i <= 44; ++i) {
      skipbyte_string(&str) ;
      TEST(str.addr == (uint8_t*)i) ;
      TEST(str.size == 44u-i) ;
   }

   return 0 ;
ONERR:
   return EINVAL ;
}

static int test_generic(void)
{
   struct {
      int            dummy1 ;
      const uint8_t  * addr ;
      size_t         size ;
      int            dummy2 ;
   }        str1 ;

   // TEST genericcast_string
   TEST((const string_t*)&str1.addr == genericcast_string(&str1)) ;

   return 0 ;
ONERR:
   return EINVAL ;
}

int unittest_string()
{
   if (test_initfree())       goto ONERR;
   if (test_query())          goto ONERR;
   if (test_compare())        goto ONERR;
   if (test_change())         goto ONERR;
   if (test_generic())        goto ONERR;

   return 0 ;
ONERR:
   return EINVAL ;
}

#endif
