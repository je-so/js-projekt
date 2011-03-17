/*
   Makefile Generator: C-kern/tools/hash.h
   Copyright (C) 2010 Jörg Seebohn

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
*/

#ifndef CKERN_TOOLS_HASH_HEADER
#define CKERN_TOOLS_HASH_HEADER

/** Memory management is done by user.
 * Insert does not make a copy of new_entry, therefore ownership
 * is transfered to the hashtable.
 * If a request is made to free_hashtable memory of every entry is freed by a call
 * to the user defined function free_memory which is a parameter of init_hashtable.
**/


typedef struct hash_entry hash_entry_t ;
typedef struct hash_table hash_table_t ;
typedef void (*PFCTFreeMemory) ( hash_entry_t* entry ) ;

struct hash_entry /// memory managed by user
{
   char*          name ; // user defined (must point to a valid string before a call to insert)
   char*          data ; // user defined
   uint32_t       hash ; // set after call of insert
   hash_entry_t*  next ; // set after call of insert (must be NULL before a call to insert)
} ;

struct hash_table
{
   PFCTFreeMemory  free_memory ;
   uint16_t        size ;
   hash_entry_t*   entries[/*size*/] ;
} ;

enum status_hashtable
{
   HASH_OK,
   HASH_NEXTPTR_NOTNULL,
   HASH_NOMEMORY,
   HASH_ENTRY_EXIST,
   HASH_ENTRY_NOTEXIST,
} ;

extern int new_hashtable(hash_table_t** table, uint16_t size, PFCTFreeMemory free_memory) ;
extern int free_hashtable(hash_table_t** table) ;
extern int search_hashtable(hash_table_t* table, const char* name, hash_entry_t** found_entry) ;
extern int search_hashtable2(hash_table_t* table, const char* name, size_t namelen, hash_entry_t** found_entry) ;
extern int insert_hashtable(hash_table_t* table, hash_entry_t* new_entry) ;
extern int firstentry_hashtable(hash_table_t* table, hash_entry_t** first_entry) ;
extern int nextentry_hashtable(hash_table_t* table, hash_entry_t* previous_entry, hash_entry_t** next_entry) ;

#ifdef KONFIG_UNITTEST
extern int unittest_hashtable(void) ;
#endif

#endif
