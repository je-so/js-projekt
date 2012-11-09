/* title: Typeadapt-GetHash

   Abstract interface (function table) to adapt
   a concrete user type to a container which needs
   to compute the hash value of a node or its associated key.

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

   file: C-kern/api/ds/typeadapt/gethash.h
    Header file <Typeadapt-GetHash>.

   file: C-kern/ds/typeadapt/gethash.c
    Implementation file <Typeadapt-GetHash impl>.
*/
#ifndef CKERN_DS_TYPEADAPT_GETHASH_HEADER
#define CKERN_DS_TYPEADAPT_GETHASH_HEADER

// forward:
struct typeadapt_t ;
struct typeadapt_object_t ;

/* typedef: struct typeadapt_gethash_it
 * Export <typeadapt_gethash_it> into global namespace. */
typedef struct typeadapt_gethash_it         typeadapt_gethash_it ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_ds_typeadapt_gethash
 * Test <typeadapt_gethash_it> functionality. */
int unittest_ds_typeadapt_gethash(void) ;
#endif


/* struct: typeadapt_gethash_it
 * Declares interface for computing a hash value from key / object.
 * If you change this interface do not forget to adapt
 * <typeadapt_gethash_EMBED> and <asgeneric_typeadaptgethash>. */
struct typeadapt_gethash_it {
   /* function: hashobject
    * Computes the hash value of the key of an object. The parameter is a pointer to the object which contains
    * the key. */
   size_t  (*hashobject) (struct typeadapt_t * typeadp, const struct typeadapt_object_t * node) ;
   /* function: hashkey
    * Computes the hash value of a key. This computation must correspond with <hashobject>. The reason is that
    * insert operations uses the object to compute the hash functions and find operations uses the key value
    * fir its hash computation. */
   size_t  (*hashkey)    (struct typeadapt_t * typeadp, const void * key) ;
} ;

// group: lifetime

/* define: typeadapt_gethash_INIT_FREEABLE
 * Static initializer. Sets all functions pointers of <typeadapt_gethash_it> to 0. */
#define typeadapt_gethash_INIT_FREEABLE      typeadapt_gethash_INIT(0,0)

/* define: typeadapt_gethash_INIT
 * Static initializer. Sets all function pointers to the provided values.
 *
 * Parameters:
 * hashobject_f - Function pointer to function returning the hash value of an object. See <typeadapt_gethash_it.hashobject>.
 * hashkey_f    - Function pointer to function returning the hash value of a key. See <typeadapt_gethash_it.hashkey>. */
#define typeadapt_gethash_INIT(hashobject_f, hashkey_f) \
   { hashobject_f, hashkey_f }

// group: query

/* function: isequal_typeadaptgethash
 * Returns true if two <typeadapt_gethash_it> objects are equal. */
bool isequal_typeadaptgethash(const typeadapt_gethash_it * lgethash, const typeadapt_gethash_it * rgethash) ;

// group: call-service

/* function: callhashobject_typeadaptgethash
 * Calls function <typeadapt_gethash_it.hashobject>.
 * The first parameter is of type <typeadapt_gethash_it> the others are the same as in <typeadapt_gethash_it.hashobject>.
 * This function is implemented as macro and supports types derived from <typeadapt_gethash_it> - see use of <typeadapt_gethash_DECLARE>. */
size_t callhashobject_typeadaptgethash(typeadapt_gethash_it * gethash, struct typeadapt_t * typeadp, const struct typeadapt_object_t * node) ;

/* function: callhashkey_typeadaptgethash
 * Calls function <typeadapt_gethash_it.hashkey>.
 * The first parameter is of type <typeadapt_gethash_it> the others are the same as in <typeadapt_gethash_it.hashkey>.
 * This function is implemented as macro and supports types derived from <typeadapt_gethash_it> - see use of <typeadapt_gethash_DECLARE>. */
size_t callhashkey_typeadaptgethash(typeadapt_gethash_it * gethash, struct typeadapt_t * typeadp, const void * key) ;

// group: generic

/* define: asgeneric_typeadaptgethash
 * Casts parameter gethash into pointer to <typeadapt_gethash_it>.
 * The parameter *gethash* has to be of type "pointer to declared_it" where declared_it
 * is the name used as first parameter in <typeadapt_gethash_DECLARE>.
 * The other parameter have to be the same as in <typeadapt_gethash_DECLARE>. */
typeadapt_gethash_it * asgeneric_typeadaptgethash(void * gethash, TYPENAME typeadapter_t, TYPENAME object_t, TYPENAME key_t) ;

/* define: typeadapt_gethash_DECLARE
 * Declares a derived interface from generic <typeadapt_gethash_it>.
 * It is structural compatible with <typeadapt_gethash_it>.
 * See <typeadapt_gethash_it> for a list of contained functions.
 *
 * Parameter:
 * declared_it   - The name of the structure which is declared as the interface.
 *                 The name should have the suffix "_it".
 * typeadapter_t - The adapter type which implements all interface functions.
 *                 The first parameter in every function is a pointer to this type.
 * object_t      - The object type that <typeadapt_gethash_it> supports.
 * key_t         - The key type that <typeadapt_gethash_it> supports. Must be of size sizeof(void*). */
void typeadapt_gethash_DECLARE(TYPENAME declared_it, TYPENAME typeadapter_t, TYPENAME object_t, TYPENAME key_t) ;

/* define: typeadapt_gethash_EMBED
 * Declares a derived interface from generic <typeadapt_gethash_it>.
 * It is structural compatible with <typeadapt_gethash_it>.
 * See <typeadapt_gethash_it> for a list of contained functions.
 *
 * Parameter:
 * typeadapter_t - The adapter type which implements all interface functions.
 *                 The first parameter in every function is a pointer to this type.
 * object_t      - The object type that <typeadapt_gethash_it> supports. */
void typeadapt_gethash_EMBED(TYPENAME typeadapter_t, TYPENAME object_t, TYPENAME key_t) ;


// section: inline implementation

/* define: asgeneric_typeadaptgethash
 * Implements <typeadapt_gethash_it.asgeneric_typeadaptgethash>. */
#define asgeneric_typeadaptgethash(gethash, typeadapter_t, object_t, key_t)      \
   ( __extension__ ({                                                            \
      static_assert(                                                             \
         offsetof(typeadapt_gethash_it, hashobject)                              \
         == offsetof(typeof(*(gethash)), hashobject)                             \
         && offsetof(typeadapt_gethash_it, hashkey)                              \
            == offsetof(typeof(*(gethash)), hashkey),                            \
         "ensure same structure") ;                                              \
      if (0) {                                                                   \
         size_t hash = 0 ;                                                       \
         hash += (gethash)->hashobject((typeadapter_t*)0, (const object_t*)0) ;  \
         hash += (gethash)->hashkey((typeadapter_t*)0, (const key_t)0) ;         \
      }                                                                          \
      (typeadapt_gethash_it*) (gethash) ;                                        \
   }))

/* define: callhashobject_typeadaptgethash
 * Implements <typeadapt_gethash_it.callhashobject_typeadaptgethash>. */
#define callhashobject_typeadaptgethash(gethash, typeadp, node) \
   ((gethash)->hashobject((typeadp), (node)))

/* define: callhashkey_typeadaptgethash
 * Implements <typeadapt_gethash_it.callhashkey_typeadaptgethash>. */
#define callhashkey_typeadaptgethash(gethash, typeadp, key) \
   ((gethash)->hashkey((typeadp), (key)))

/* define: typeadapt_gethash_DECLARE
 * Implements <typeadapt_gethash_it.typeadapt_gethash_DECLARE>. */
#define typeadapt_gethash_DECLARE(declared_it, typeadapter_t, object_t, key_t)   \
   static inline void compiletimeassert##declared_it(void) {                     \
      static_assert(sizeof(key_t)==sizeof(void*), "compatible with hashkey") ;   \
   }                                                                             \
   typedef struct declared_it             declared_it ;                          \
   struct declared_it {                                                          \
      typeadapt_gethash_EMBED(typeadapter_t, object_t, key_t) ;                  \
   }

/* define: typeadapt_gethash_EMBED
 * Implements <typeadapt_gethash_it.typeadapt_gethash_EMBED>. */
#define typeadapt_gethash_EMBED(typeadapter_t, object_t, key_t)               \
   size_t  (*hashobject) (typeadapter_t * typeadp, const object_t * node) ;   \
   size_t  (*hashkey)    (typeadapter_t * typeadp, const key_t key)

#endif
