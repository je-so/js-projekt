/* title: TransC-StringTable

   Helps to manage constant strings used as initialization values
   encountered during the compilation process of trans-C source code.

   TODO: Replace module with functionality offered by persistent
         database layer.

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
   (C) 2013 Jörg Seebohn

   file: C-kern/api/clang/transC/transCstringtable.h
    Header file <TransC-StringTable>.

   file: C-kern/clang/transC/transCstringtable.c
    Implementation file <TransC-StringTable impl>.
*/
#ifndef CKERN_CLANG_TRANSC_TRANSCSTRINGTABLE_HEADER
#define CKERN_CLANG_TRANSC_TRANSCSTRINGTABLE_HEADER

// forward
struct memblock_t ;
struct slist_node_t ;

/* typedef: struct transCstringtable_t
 * Export <transCstringtable_t> into global namespace. */
typedef struct transCstringtable_t              transCstringtable_t ;

/* typedef: struct transCstringtable_iterator_t
 * Export <transCstringtable_iterator_t> into global namespace. */
typedef struct transCstringtable_iterator_t     transCstringtable_iterator_t ;

/* typedef: struct transCstringtable_entry_t
 * Export <transCstringtable_entry_t> -- private type to implement a linked list. */
typedef struct transCstringtable_entry_t        transCstringtable_entry_t ;

/* typedef: struct transCstringtable_page_t
 * Export <transCstringtable_page_t> -- private type to manage list of memory blocks. */
typedef struct transCstringtable_page_t         transCstringtable_page_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_lang_transc_transCstringtable
 * Test <transCstringtable_t> functionality. */
int unittest_lang_transc_transCstringtable(void) ;
#endif


/* struct: transCstringtable_entry_t
 * Stores a single data block of a string. */
struct transCstringtable_entry_t {
   /* variable: next
    * Points to next data block. See <strsize> how to detect the end of a string
    * and the beginning of a new string. */
   transCstringtable_entry_t *   next ;
   /* variable: strsize
    * Only 15 bit of strsize are used to describe the size.
    * Therefore the supported size of a single data block is always <= 32767.
    * Bit 15 (value 32768) is used to mark a block as an extension of the previous data block.
    * It belongs to the same string as the previous block if bit 15 is set. */
   uint16_t                      strsize ;
   /* variable: strdata
    * Points to the start address of the string data. */
   uint8_t                       strdata[] ;
} ;

// group: lifetime

/* define: transCstringtable_entry_INIT
 * Static initializer. Sets strsize - the size of the string. */
#define transCstringtable_entry_INIT(strsize) \
         { 0, strsize }

/* define: transCstringtable_entry_INIT_EXTENSION
 * Static initializer. Sets strsize - the size of the string. */
#define transCstringtable_entry_INIT_EXTENSION(strsize) \
         { 0, (32768 | (strsize)) }

// group: query

/* function: isextension_transCstringtableentry
 * Returns bit 15 of member <transCstringtable_entry_t.strsize>. */
uint16_t isextension_transCstringtableentry(const transCstringtable_entry_t * entry) ;

/* function: objectsize_transCstringtableentry
 * Returns the memory aligned object size computed from strsize. */
size_t objectsize_transCstringtableentry(uint16_t strsize) ;

/* function: strsize_transCstringtableentry
 * Returns string length of stored data. */
uint16_t strsize_transCstringtableentry(const transCstringtable_entry_t * entry) ;

/* function: strsizemax_transCstringtableentry
 * Returns the maximum supported string length. */
uint16_t strsizemax_transCstringtableentry(void) ;

// group: update

/* function: setextbit_transCstringtableentry
 * Sets bit 15 of member <transCstringtable_entry_t.strsize>. */
void setextbit_transCstringtableentry(const transCstringtable_entry_t * entry) ;


/* struct: transCstringtable_iterator_t
 * Allows to iterate the data of a string with a given id.
 * The iteration of all contained strings in a table is currently not implemented. */
struct transCstringtable_iterator_t {
   transCstringtable_entry_t * next ;
} ;

// group: lifetime

/* define: transCstringtable_iterator_INIT_FREEABLE
 * Static initializer. */
#define transCstringtable_iterator_INIT_FREEABLE   { 0 }

/* function: initfirst_transcstringtableiterator
 * Initializes iter to point to the first datablock of string with id strid. */
int initfirst_transcstringtableiterator(/*out*/transCstringtable_iterator_t * iter, transCstringtable_t * strtable, void * strid) ;

/* function: free_transcstringtableiterator
 * Sets iterator iter to <transCstringtable_iterator_INIT_FREEABLE>. */
void free_transcstringtableiterator(transCstringtable_iterator_t * iter) ;

// group: iterate

/* function: next_transcstringtableiterator
 * Returns data the next datablock of the string.
 * A return value of true indicates success a value of false that no more data is available.
 * In case of false data is not changed. */
bool next_transcstringtableiterator(transCstringtable_iterator_t * iter, /*out*/struct memblock_t * data) ;


/* struct: transCstringtable_t
 * Stores string literals used for initialization.
 * The stored values contain no more escape sequences.
 * They must have been replaced by the corresponding byte sequence
 * of the UTF-8 encoded codepoint before inserting a string. */
struct transCstringtable_t {
   uint8_t *                     next ;
   uint8_t *                     end ;
   transCstringtable_entry_t *   first ;
   transCstringtable_entry_t **  prev ;
   struct {
      struct slist_node_t *      last ;
   }                             pagelist ;
} ;

// group: lifetime

/* define: transCstringtable_INIT_FREEABLE
 * Static initializer. */
#define transCstringtable_INIT_FREEABLE      { 0, 0, 0, 0, { 0 } }

/* function: init_transcstringtable
 * Initialize strtable and allocate an empty memory page. */
int init_transcstringtable(/*out*/transCstringtable_t * strtable) ;

/* function: free_transcstringtable
 * Free strtable and all allocated memory pages. */
int free_transcstringtable(transCstringtable_t * strtable) ;

// group: query

/* typedef: iteratortype_transcstringtable
 * Associate <transCstringtable_iterator_t> with <transCstringtable_t>.
 * Use this iterator to query stored data. */
typedef transCstringtable_iterator_t   iteratortype_transcstringtable ;

/* typedef: iteratedtype_transcstringtable
 * Associate <memblock_t> with return value of <next_transcstringtableiterator>. */
typedef struct memblock_t              iteratedtype_transcstringtable ;

// group: update

/* function: insertstring_transcstringtable
 * Adds a new string to the table. Only memory is reservered.
 * The size of the string is determined by size and the address of the reserved memory block
 * is returned in addr. The id of the new string is returned in strid. You can later use strid
 * to iterate over the string content. */
int insertstring_transcstringtable(transCstringtable_t * strtable, /*out*/void ** strid, /*out*/uint8_t ** addr, uint8_t size) ;

/* function: appendstring_transcstringtable
 * Increases last inserted string by size bytes. The start address of the appended memory is returned in addr.
 * It is possible that the string data is split over multiple memory pages and therefore non contiguous. */
int appendstring_transcstringtable(transCstringtable_t * strtable, /*out*/uint8_t ** addr, uint8_t size) ;

/* function: shrinkstring_transcstringtable
 * Shrinks the last inserted or appended string memory block.
 * The values addr and size describe the parameter given or returned from the last call
 * to <insertstring_transcstringtable> or <appendstring_transcstringtable>.
 * The endaddr must be in the range [addr, addr + size]. An endaddr of set to addr shrinks the
 * last memory block to size 0. An endaddr set to value (addr+size) does not change the size of last
 * inserted or appended string data block. */
int shrinkstring_transcstringtable(transCstringtable_t * strtable, uint8_t * endaddr) ;


// section: inline implementation

// group: transCstringtable_iterator_t

/* define: free_transcstringtableiterator
 * Implements <transCstringtable_iterator_t.free_transcstringtableiterator>. */
#define free_transcstringtableiterator(iter) \
         ((void)(*(iter) = (transCstringtable_iterator_t) transCstringtable_iterator_INIT_FREEABLE))

// group: transCstringtable_entry_t

/* define: isextension_transCstringtableentry
 * Implements <transCstringtable_entry_t.isextension_transCstringtableentry>. */
#define isextension_transCstringtableentry(entry) \
         ((entry)->strsize & 32768)

/* define: objectsize_transCstringtableentry
 * Implements <transCstringtable_entry_t.objectsize_transCstringtableentry>. */
#define objectsize_transCstringtableentry(strsize) \
         ((offsetof(transCstringtable_entry_t, strdata) + (size_t)KONFIG_MEMALIGN -1u + (strsize)) & ~((size_t)KONFIG_MEMALIGN-1u))

/* define: strsizemax_transCstringtableentry
 * Implements <transCstringtable_entry_t.strsizemax_transCstringtableentry>. */
#define strsizemax_transCstringtableentry() \
         (32767)

/* define: strsize_transCstringtableentry
 * Implements <transCstringtable_entry_t.strsize_transCstringtableentry>. */
#define strsize_transCstringtableentry(entry) \
         ((uint16_t)((entry)->strsize & ~32768))

/* define: setextbit_transCstringtableentry
 * Implements <transCstringtable_entry_t.setextbit_transCstringtableentry>. */
#define setextbit_transCstringtableentry(entry) \
         ((entry)->strsize |= 32768)

#endif
