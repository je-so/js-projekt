/* title: RegularExpression impl

   Implements <RegularExpression>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: C-kern/api/proglang/regexpr.h
    Header file <RegularExpression>.

   file: C-kern/proglang/regexpr.c
    Implementation file <RegularExpression impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/proglang/regexpr.h"
#include "C-kern/api/err.h"
#include "C-kern/api/ds/foreach.h"
#include "C-kern/api/ds/inmem/slist.h"
#include "C-kern/api/ds/inmem/node/slist_node.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/memstream.h"
#include "C-kern/api/memory/pagecache_macros.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif

typedef struct buffer_t       buffer_t;
typedef struct buffer_page_t  buffer_page_t;

// forward

#ifdef KONFIG_UNITTEST
static test_errortimer_t s_regex_errtimer;
#endif


/* struct: buffer_page_t
 * Beschreibt eine Speicherseite, die zu vielen von <buffer_t> verwaltet werden. */
struct buffer_page_t {
   /* variable: next
    * Verlinkt die Seiten zu einer Liste. */
   slist_node_t next;
   /* variable: data
    * Beginn der gespeicherten Daten. Diese sind auf den long Datentyp ausgerichtet. */
   long         data[1];
};

// group: constants

/* define: buffer_page_SIZE
 * Die Größe in Bytes einer <buffer_page_t>. */
#define buffer_page_SIZE 1024

// group: lifetime

/* function: new_bufferpage
 * Weist page eine Speicherseite von <buffer_page_SIZE> Bytes zu.
 * Der Speicherinhalt von **page ist undefiniert. */
static int new_bufferpage(/*out*/buffer_page_t ** page)
{
   int err;
   memblock_t memblock;

   if (! PROCESS_testerrortimer(&s_regex_errtimer, &err)) {
      err = ALLOC_PAGECACHE(pagesize_1024, &memblock);
   }
   if (err) goto ONERR;

   *page = (buffer_page_t*) memblock.addr;

   return 0;
ONERR:
   return err;
}

/* function: delete_bufferpage
 * Gibt die Speicherseite page wieder frei. Es darf kein Zugriff mehr darauf erfolgen. */
static inline int delete_bufferpage(buffer_page_t * page)
{
   int err;
   memblock_t memblock = memblock_INIT(buffer_page_SIZE, (uint8_t*) page);

   err = RELEASE_PAGECACHE(&memblock);
   (void) PROCESS_testerrortimer(&s_regex_errtimer, &err);
   if (err) goto ONERR;

   return 0;
ONERR:
   return err;
}


/* struct: buffer_t
 * Siehe auch <memstream_ro_t> und <memstream_t>.
 *
 * Invariant:
 * Anzahl lesbare Bytes == (size_t) (end-next). */
struct buffer_t {
   // group: input-buffer

   /* variable: next
    * Nächstes zu lesendes Byte, falls next != end. */
   const uint8_t * next;
   /* variable: end
    * Zeigt auf das dem letzten zu lesenden Byte Nachfolgende. */
   const uint8_t * end;

   // group: additional memory
   slist_t pagelist;

   // group: context-tree
   // Enthält Ergebnis der parse Phase

};

// group: helper-types

slist_IMPLEMENT(_pagelist, buffer_page_t, next.next)

// group: memory-helper

static int addpage_buffer(buffer_t * buffer)
{
   int err;
   buffer_page_t * page;

   err = new_bufferpage(&page);
   if (err) goto ONERR;

   // insertlast_slist
   insertlast_pagelist(&buffer->pagelist, page);

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: lifetime

/* function: init_buffer
 * Initialisiert buffer mit Eingabestring start[len]. */
static inline void init_buffer(/*out*/buffer_t * buffer, size_t len, const uint8_t * start)
{
   init_memstream(buffer, start, start + len);
   buffer->pagelist = (slist_t) slist_INIT;
}

/* function: free_buffer
 * Gibt den mit buffer verbundenen Speicher frei. */
static inline int free_buffer(buffer_t * buffer)
{
   int err = 0;
   int err2;

   // free allocated pages (list nodes) and free list
   foreach (_pagelist, page, &buffer->pagelist) {
      err2 = delete_bufferpage(page);
      if (err2) err = err2;
   }
   buffer->pagelist = (slist_t) slist_INIT;

   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}


// section: regexpr_t

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_regex_errtimer
 * Simuliert Fehler in Funktionen für <buffer_t> und <regexpr_t>. */
static test_errortimer_t   s_regex_errtimer = test_errortimer_FREE;
#endif

// group: parsing

/* function: parse_regexpr
 * TODO: */
static int parse_regexpr(buffer_t buffer)
{
   (void) buffer;

   return 0;
}

// group: lifetime

int init_regexpr(/*out*/regexpr_t* regex, size_t len, const char definition[len])
{
   int err;
   buffer_t buffer;

   init_buffer(&buffer, len, (const unsigned char*) definition);

   err = parse_regexpr(buffer);
   if (err) goto ONERR;

   regex->root = 0; // TODO:

   return 0;
ONERR:
   free_buffer(&buffer);
   TRACEEXIT_ERRLOG(err);
   return err;
}


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_bufferpage(void)
{
   buffer_page_t * page[10] = { 0 };

   // TEST buffer_page_SIZE: 2er Potenz (== 2**x)
   TEST(1024 == buffer_page_SIZE);

   // TEST new_bufferpage
   for (unsigned i = 0; i < lengthof(page); ++i) {
      size_t oldsize = SIZEALLOCATED_PAGECACHE();
      TEST(0 == new_bufferpage(&page[i]));
      // check page
      TEST(0 != page[i]);
      // check environment
      TEST(oldsize + buffer_page_SIZE == SIZEALLOCATED_PAGECACHE());
      if (i) {
         unsigned i1 = i, i2 = i-1;
         if (page[i1] < page[i2]) {
            i1 = i2;
            i2 = i;
         }
         TEST((void*) page[i1] == buffer_page_SIZE + (uint8_t*)page[i2]);
      }
   }

   // TEST delete_bufferpage
   for (unsigned i = 0; i < lengthof(page); ++i) {
      size_t oldsize = SIZEALLOCATED_PAGECACHE();
      TEST(0 == delete_bufferpage(page[i]));
      // check environment
      TEST(oldsize - buffer_page_SIZE == SIZEALLOCATED_PAGECACHE());
      page[i] = 0;
   }

   // TEST new_bufferpage: ERROR
   for (int i = 1; i < 10; ++i) {
      init_testerrortimer(&s_regex_errtimer, 1, (int) i);
      TEST(i == new_bufferpage(&page[0]));
      TEST(0 == page[0]);
   }

   // TEST delete_bufferpage: ERROR
   for (int i = 1; i < 10; ++i) {
      size_t oldsize = SIZEALLOCATED_PAGECACHE();
      TEST(0 == new_bufferpage(&page[0]));
      init_testerrortimer(&s_regex_errtimer, 1, i);
      TEST(i == delete_bufferpage(page[0]));
      TEST(oldsize == SIZEALLOCATED_PAGECACHE());
      page[0] = 0;
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_buffer(void)
{
   buffer_t buffer;
   uint8_t  data[256];

   // === lifetime + memory helper ===

   for (size_t len = 0; len < lengthof(data)/2; ++len) {
      size_t oldsize = SIZEALLOCATED_PAGECACHE();

      // TEST init_buffer
      memset(&buffer, 255, sizeof(buffer));
      TEST(! isempty_slist(&buffer.pagelist));
      init_buffer(&buffer, len, data+len);
      // check buffer
      TEST(data + len == buffer.next);
      TEST(data + 2*len == buffer.end);
      TEST(isempty_slist(&buffer.pagelist));

      // TEST free_buffer: empty pagelist
      TEST(0 == free_buffer(&buffer));
      TEST(isempty_slist(&buffer.pagelist));
      TEST(oldsize == SIZEALLOCATED_PAGECACHE());

      // TEST addpage_buffer: ERROR (empty list)
      // prepare
      init_buffer(&buffer, len, data+len);
      if (len == 0) {
         init_testerrortimer(&s_regex_errtimer, 1, 3);
         TEST(3 == addpage_buffer(&buffer));
         // check nothing changed
         TEST(isempty_slist(&buffer.pagelist));
         TEST(oldsize == SIZEALLOCATED_PAGECACHE());
      }

      // TEST addpage_buffer
      buffer_page_t * last = 0, * first = 0;
      for (unsigned i = 0; i <= len; ++i) {
         // test
         TEST(0 == addpage_buffer(&buffer));
         // check environment
         TEST(oldsize + (i+1)* buffer_page_SIZE == SIZEALLOCATED_PAGECACHE());
         // check buffer.pagelist
         TEST(! isempty_slist(&buffer.pagelist));
         TEST(last != last_pagelist(&buffer.pagelist));
         if (!i) {
            last = last_pagelist(&buffer.pagelist);
            first = next_pagelist(last);
            TEST(last == first);
         } else {
            TEST(last_pagelist(&buffer.pagelist) == next_pagelist(last));
            last = last_pagelist(&buffer.pagelist);
            TEST(next_pagelist(last) == first);
         }
      }

      // TEST addpage_buffer: ERROR (non empty list)
      if (len && len < 5) {
         init_testerrortimer(&s_regex_errtimer, 1, (int)len);
         TEST((int)len == addpage_buffer(&buffer));
         // check buffer.pagelist not changed
         TEST(last_pagelist(&buffer.pagelist) == last);
         TEST(next_pagelist(last) == first);
      }

      // TEST free_buffer: pagelist not empty (+ double free)
      for (int i = 1; i <= 2; ++i) {
         TEST(0 == free_buffer(&buffer));
         TEST(isempty_slist(&buffer.pagelist));
         TEST(oldsize == SIZEALLOCATED_PAGECACHE());
      }
   }

   // TEST free_buffer: ERROR
   for (int i = 1; i < 10; ++i) {
      size_t oldsize = SIZEALLOCATED_PAGECACHE();
      init_buffer(&buffer, sizeof(data), data);
      for (int l = 1; l <= i; ++l) {
         TEST(0 == addpage_buffer(&buffer));
      }
      TEST(oldsize + (unsigned)i * buffer_page_SIZE == SIZEALLOCATED_PAGECACHE());
      TEST(! isempty_slist(&buffer.pagelist));
      // test
      init_testerrortimer(&s_regex_errtimer, (unsigned)i, i);
      TEST(i == free_buffer(&buffer));
      // check buffer
      TEST(isempty_slist(&buffer.pagelist));
      // check environment
      TEST(oldsize == SIZEALLOCATED_PAGECACHE());
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_initfree(void)
{
   regexpr_t regex = regexpr_FREE;

   // TEST regexpr_FREE
   TEST(0 == regex.root);

   return 0;
ONERR:
   return EINVAL;
}


int unittest_proglang_regexpr()
{
   if (test_bufferpage())  goto ONERR;
   if (test_buffer())      goto ONERR;
   if (test_initfree())    goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
