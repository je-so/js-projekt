/* title: Tool-HashFunction impl

   Hash function implementation used by <genmake>.

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
   (C) 2012 Jörg Seebohn

   file: C-kern/tools/hash.h
    Header file <Tool-HashFunction>.

   file: C-kern/tools/hash.c
    Implementation file <Tool-HashFunction impl>.
*/

#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "C-kern/tools/hash.h"

static inline uint32_t charhash(const char c)
{
   typedef unsigned char uchar ;
   if (c == '_') return 0 ;
   if ( (uchar)c >= (uchar)'a' && (uchar)c <= (uchar)'z')   return 1u + (uint32_t) ((uchar)c - (uchar)'a') ;
   if ( (uchar)c >= (uchar)'A' && (uchar)c <= (uchar)'Z')   return 27u + (uint32_t) ((uchar)c - (uchar)'A') ;
   return (uchar)c ;
}

static uint32_t build_hash(const char* name, size_t namelen)
{
   uint32_t hash = 0 ;
   if (namelen)
   {
      for(size_t i = 1; i < namelen; ++i)
      {
         hash += charhash(name[i]) ;
      }
      hash = charhash(name[0]) + 53 * hash ;
   }
   return hash ;
}

hash_status_e new_hashtable(hash_table_t** table, uint16_t size, PFCTFreeMemory free_memory)
{
   size_t memsize = sizeof(hash_table_t) + size * sizeof(hash_entry_t) ;
   if (  !size || (memsize-sizeof(hash_table_t)) / sizeof(hash_entry_t) != size ) {
      return HASH_NOMEMORY ;
   }

   hash_table_t* result_table = (hash_table_t*) malloc( memsize ) ;
   if (!result_table) {
      return HASH_NOMEMORY ;
   }

   memset(result_table, 0, memsize) ;
   result_table->size = size ;
   result_table->free_memory = free_memory ;
   *table = result_table ;

   return HASH_OK ;
}

hash_status_e free_hashtable(hash_table_t** table)
{
   assert(table) ;

   hash_table_t* const table2 = *table ;

   if (!table2) {
      return HASH_OK ;
   }

   PFCTFreeMemory free_memory = table2->free_memory ;
   if (free_memory)
   {
      for(int i=table2->size-1; i >= 0; --i) {
         hash_entry_t* entry =  table2->entries[i] ;
         while( entry ) {
            hash_entry_t* next =  entry->next ;
            entry->next = 0 ;
            free_memory(entry) ;
            entry = next ;
         }
      }
   }

   free(table2) ;
   *table = 0 ;
   return HASH_OK ;
}

hash_status_e search_hashtable2(
   hash_table_t* table,
   const char*   name,
   size_t        namelen,
   hash_entry_t** found_entry)
{
   const unsigned hash  =  build_hash(name,namelen)  ;
   hash_entry_t*  entry =  table->entries[hash % table->size] ;
   while( entry )
   {
      if (   hash == entry->hash
         &&  0==strncmp(entry->name, name, namelen)
         &&  0==entry->name[namelen] ) {
         *found_entry = entry ;
         return HASH_OK ;
      }
      entry = entry->next ;
   }
   return HASH_ENTRY_NOTEXIST ;
}

hash_status_e search_hashtable(
   hash_table_t* table,
   const char*   name,
   hash_entry_t** found_entry)
{
   return search_hashtable2(table, name, strlen(name), found_entry) ;
}

hash_status_e insert_hashtable(hash_table_t* table, hash_entry_t* new_entry)
{
   const unsigned hash = build_hash(new_entry->name,strlen(new_entry->name))  ;
   hash_entry_t** tablepos =  & table->entries[hash % table->size] ;
   const hash_entry_t* entry =  tablepos[0] ;
   while( entry )
   {
      if (   hash == entry->hash
         &&  0==strcmp(entry->name, new_entry->name)) {
         return HASH_ENTRY_EXIST ;
      }
      entry = entry->next ;
   }
   if (new_entry->next) {
      return HASH_NEXTPTR_NOTNULL ;
   } else {
      new_entry->hash = hash ;
      new_entry->next = tablepos[0] ;
      tablepos[0] = new_entry ;
      return HASH_OK ;
   }
}

hash_status_e firstentry_hashtable(hash_table_t* table, hash_entry_t** first_entry)
{
   for(size_t i = 0; i < table->size; ++i) {
      if (table->entries[i]) {
         *first_entry = table->entries[i] ;
         return HASH_OK ;
      }
   }
   return HASH_ENTRY_NOTEXIST ;
}

hash_status_e nextentry_hashtable(hash_table_t* table, hash_entry_t* previous_entry, hash_entry_t** next_entry)
{
   if (previous_entry->next) {
      *next_entry = previous_entry->next ;
      return HASH_OK ;
   }
   for(size_t i = 1+(previous_entry->hash%table->size) ; i < table->size ; ++i) {
      if (table->entries[i]) {
         *next_entry = table->entries[i] ;
         return HASH_OK ;
      }
   }
   return HASH_ENTRY_NOTEXIST ;
}



#ifdef KONFIG_UNITTEST


typedef struct unittest_entry_t        unittest_entry_t ;

struct unittest_entry_t
{
   hash_entry_t     entry ;
   int              isFreeMemoryCalled ;
} ;

void test_hashtable_freememory(hash_entry_t * entry)
{
   unittest_entry_t*  test_entry = (unittest_entry_t*) entry ;
   test_entry->isFreeMemoryCalled += 1 ;
   free( entry->name ) ;
   entry->name = 0 ;
}


#define TEST(ARG) \
   if ( !(ARG) ) { \
      fprintf( stderr, "%s:%d: %s():\n FAILED TEST (%s)\n ", __FILE__, __LINE__, "unittest_hashtable", #ARG) ;\
      goto ABBRUCH ; \
   }

int unittest_hashtable()
{
   {
      TEST(build_hash("",0) == 0) ;
      TEST(build_hash("_",1) == 0) ;
      TEST(build_hash("a",1) == 1) ;
      TEST(build_hash("z",1) == 26) ;
      TEST(build_hash("A",1) == 27) ;
      TEST(build_hash("Z",1) == 52) ;
      char name[] = { (char)230, (char)128, (char)127, 0} ;
      char name2[] = "_ABzw" ;
      TEST(build_hash(name,strlen(name)) == (230 + 53 * (128 + 127))) ;
      TEST(build_hash(name2,strlen(name2)) == (0 + 53 * (27 + 28 + 26 + 23))) ;
   }

   hash_table_t*    table ;
   unittest_entry_t entries[10][3] = { { { {0,0,0,0}, 0} } } ;

   TEST(new_hashtable(&table, 0, &test_hashtable_freememory)==HASH_NOMEMORY) ;
   TEST(new_hashtable(&table, 1023, &test_hashtable_freememory)==HASH_OK) ;
   TEST(table->size == 1023) ;
   TEST(table->free_memory == &test_hashtable_freememory) ;

   hash_entry_t notnull_entry = { .name = (char*) (intptr_t) "123", .next = (hash_entry_t*) 8, .hash = 0 } ;
   TEST(insert_hashtable(table, &notnull_entry)==HASH_NEXTPTR_NOTNULL) ;

   for(int i=0; i < 10; ++i) {
      char name[100] ;
      for(int t=0; t < 3; ++t) {
         sprintf(name, "%c_Test_%d%s", 'a' + i, i, t==0?"001":(t==1?"010":"100")) ;
         entries[i][t].entry.name = strdup(name) ;
         entries[i][t].entry.data = (void*) (3*i + t) ;
         assert(entries[i][t].isFreeMemoryCalled == 0) ;
         assert(entries[i][t].entry.name != 0) ;
         TEST(insert_hashtable(table, &entries[i][t].entry)==HASH_OK) ;
         unsigned hash = build_hash(name,strlen(name)) % table->size ;
         TEST(table->entries[hash]==&entries[i][t].entry) ;
      }
   }

   for(int i=0; i < 10; ++i) {
      char name[100] ;
      for(int t=0; t < 3; ++t) {
         sprintf(name, "%c_Test_%d%s", 'a' + i, i, t==0?"001":(t==1?"010":"100")) ;
         hash_entry_t * found_entry ;
         TEST(search_hashtable(table, name, &found_entry)==HASH_OK) ;
         TEST(0==strcmp(found_entry->name,name)) ;
         TEST(found_entry->data == (void*)(3*i+t)) ;
      }
   }

   for(int i=0; i < 10; ++i) {
      char name[100] ;
      for(int t=0; t < 3; ++t) {
         sprintf(name, "%c_Test_%d%s", 'a' + i, i, t==0?"001":(t==1?"010":"100")) ;
         unittest_entry_t * entry = &entries[i][t] ;
         TEST(entry->isFreeMemoryCalled == 0) ;
         TEST(entry->entry.name != 0 && 0==strcmp(entry->entry.name,name)) ;
         TEST(entry->entry.data == (void*)(3*i+t)) ;
         TEST(t==0 || entry->entry.next == &entries[i][t-1].entry) ;
      }
   }

   unittest_entry_t special[2] = { { {0,0,0,0}, 0} } ;    // positioned at start and end of table
   special[0].entry.name = strdup("_") ;
   special[1].entry.name = strdup("\x0F\x13") ;
   TEST(insert_hashtable(table, &special[0].entry)==HASH_OK) ;
   TEST(insert_hashtable(table, &special[1].entry)==HASH_OK) ;
   TEST(table->entries[0] == &special[0].entry) ;
   TEST(table->entries[table->size-1] == &special[1].entry) ;
   TEST(special[0].isFreeMemoryCalled == 0) ;
   TEST(special[1].isFreeMemoryCalled == 0) ;

   TEST(free_hashtable(&table)==HASH_OK) ;
   TEST(table==NULL) ;

   TEST(special[0].isFreeMemoryCalled == 1) ;   // first entry of table freed
   TEST(special[1].isFreeMemoryCalled == 1) ;   // last entry of table freed

   for(int i=0; i < 10; ++i) {
      for(int t=0; t < 3; ++t) {
         unittest_entry_t * entry = &entries[i][t] ;
         TEST(entry->isFreeMemoryCalled == 1) ;
         TEST(entry->entry.name == 0 ) ;
         TEST(entry->entry.data == (void*)(3*i+t)) ;
      }
   }

   TEST(table==NULL) ;
   TEST(free_hashtable(&table)==HASH_OK) ;
   TEST(table==NULL) ;

   return 0 ;
ABBRUCH:
   return 1 ;
}
#endif //KONFIG_UNITTEST
