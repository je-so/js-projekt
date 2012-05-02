/* title: Tool-HashFunction

   Hash function interface used by <genmake>.

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
#ifndef CKERN_TOOLS_HASH_HEADER
#define CKERN_TOOLS_HASH_HEADER

typedef struct hash_entry_t            hash_entry_t ;

typedef struct hash_table_t            hash_table_t ;

typedef void                        (* PFCTFreeMemory) ( hash_entry_t* entry ) ;

enum hash_status_e
{
   HASH_OK,
   HASH_NEXTPTR_NOTNULL,
   HASH_NOMEMORY,
   HASH_ENTRY_EXIST,
   HASH_ENTRY_NOTEXIST,
} ;

typedef enum hash_status_e             hash_status_e ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
int unittest_hashtable(void) ;
#endif


/* struct: hash_entry_t
 * Data node managed in <hash_table_t>.
 * The memory is managed by the user. */
struct hash_entry_t
{
   char*          name ; // user defined (must point to a valid string before a call to insert)
   char*          data ; // user defined
   uint32_t       hash ; // set after call of insert
   hash_entry_t*  next ; // set after call of insert (must be NULL before a call to insert)
} ;

/* struct: hash_table_t
 * Manages a hashed table of <hash_entry_t> objects.
 *
 * Memory Management:
 * Is is done by the user of this object.
 * Insert does not make a copy of new_entry, therefore ownership is transfered to the hashtable.
 * If a request is made to free_hashtable the memory of every entry is freed by a call
 * to the user supplied function free_memory.
 * This function is supplied as parameter to init_hashtable. */
struct hash_table_t
{
   PFCTFreeMemory  free_memory ;
   uint16_t        size ;
   hash_entry_t*   entries[/*size*/] ;
} ;

// group: lifetime

hash_status_e new_hashtable(hash_table_t** table, uint16_t size, PFCTFreeMemory free_memory) ;

hash_status_e free_hashtable(hash_table_t** table) ;

// group: query

hash_status_e search_hashtable(hash_table_t* table, const char* name, hash_entry_t** found_entry) ;

hash_status_e search_hashtable2(hash_table_t* table, const char* name, size_t namelen, hash_entry_t** found_entry) ;

// group: change

hash_status_e insert_hashtable(hash_table_t* table, hash_entry_t* new_entry) ;

// group: iterate

hash_status_e firstentry_hashtable(hash_table_t* table, hash_entry_t** first_entry) ;

hash_status_e nextentry_hashtable(hash_table_t* table, hash_entry_t* previous_entry, hash_entry_t** next_entry) ;


#endif
