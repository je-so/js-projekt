/* title: Typeadapt-GetBinaryKey

   Abstract interface (function table) to adapt
   a concrete user type to a container which needs
   to get the key from a node as binary string.

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

   file: C-kern/api/ds/typeadapt/getbinarykey.h
    Header file <Typeadapt-GetBinaryKey>.

   file: C-kern/ds/typeadapt/getbinarykey.c
    Implementation file <Typeadapt-GetBinaryKey impl>.
*/
#ifndef CKERN_DS_TYPEADAPT_GETBINARYKEY_HEADER
#define CKERN_DS_TYPEADAPT_GETBINARYKEY_HEADER

// forward:
struct typeadapt_t ;
struct typeadapt_object_t ;

/* typedef: struct typeadapt_binarykey_t
 * Export <typeadapt_binarykey_t> into global namespace. */
typedef struct typeadapt_binarykey_t               typeadapt_binarykey_t ;

/* typedef: struct typeadapt_getbinarykey_it
 * Export <typeadapt_getbinarykey_it> into global namespace. */
typedef struct typeadapt_getbinarykey_it           typeadapt_getbinarykey_it ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_ds_typeadapt_getbinarykey
 * Test <typeadapt_getbinarykey_it> functionality. */
int unittest_ds_typeadapt_getbinarykey(void) ;
#endif


/* struct: typeadapt_binarykey_t
 * Describes byte aligned binary data used as key. */
struct typeadapt_binarykey_t {
   /* variable: addr
    * Start address of binary data of key. The lowest address in memory. */
   const uint8_t  * addr/*[size]*/ ;
   /* variable: size
    * Size in bytes of binary data <addr> points to. */
   size_t         size ;
} ;

// group: lifetime

/* define: typeadapt_binarykey_INIT_FREEABLE
 * Static initializer. */
#define typeadapt_binarykey_INIT_FREEABLE          typeadapt_binarykey_INIT(0, (const uint8_t*)0)

/* define: typeadapt_binarykey_INIT_FREEABLE
 * Inits <typeadapt_binarykey_t> with size in bytes in start (lowest) memory address. */
#define typeadapt_binarykey_INIT(size, addr)       { addr, size }

// group: generic

/* function: asgeneric_typeadaptbinarykey
 * Casts parameter conststr into pointer to <typeadapt_binarykey_t>.
 * Parameter constr should be of type pointer to <conststring_t>. */
typeadapt_binarykey_t * asgeneric_typeadaptbinarykey(void * conststr) ;


/* struct: typeadapt_getbinarykey_it
 * Declares interface for getting a description of a binary key of the object.
 * If you change this interface do not forget to adapt
 * <typeadapt_getbinarykey_EMBED> and <asgeneric_typeadaptgetbinarykey>. */
struct typeadapt_getbinarykey_it {
   /* variable: getbinarykey
    * Returns in <typeadapt_binarykey_t> the description of a binary key. */
   void  (*getbinarykey)   (struct typeadapt_t * typeadp, struct typeadapt_object_t * node, /*out*/typeadapt_binarykey_t * binkey) ;
} ;

// group: lifetime

/* define: typeadapt_getbinarykey_INIT_FREEABLE
 * Static initializer. Sets all functions pointers of <typeadapt_getbinarykey_it> to 0. */
#define typeadapt_getbinarykey_INIT_FREEABLE             { 0 }

/* define: typeadapt_getbinarykey_INIT
 * Static initializer. Sets all function pointers to the provided values.
 *
 * Parameters:
 * getbinarykey_f - Function pointer to function returning the binary key of an object. See <typeadapt_getbinarykey_it.getbinarykey>. */
#define typeadapt_getbinarykey_INIT(getbinarykey_f)      { getbinarykey_f }

// group: query

bool isequal_typeadaptgetbinarykey(const typeadapt_getbinarykey_it * ladpbinkey, const typeadapt_getbinarykey_it * radpbinkey) ;

// group: call-service

/* function: callgetbinarykey_typeadaptgetbinarykey
 * Calls function <typeadapt_getbinarykey_it.getbinarykey>.
 * The first parameter is of type <typeadapt_getbinarykey_it> the others are the same as in <typeadapt_getbinarykey_it.getbinarykey>.
 * This function is implemented as macro and supports types derived from <typeadapt_getbinarykey_it> - see use of <typeadapt_getbinaryke_DECLARE>. */
void callgetbinarykey_typeadaptgetbinarykey(typeadapt_getbinarykey_it * adpbinkey, struct typeadapt_t * typeadp, struct typeadapt_object_t * node, /*out*/typeadapt_binarykey_t * binkey) ;

// group: generic

/* define: asgeneric_typeadaptgetbinarykey
 * Casts parameter adpbinkey into pointer to <typeadapt_getbinarykey_it>.
 * The parameter *adpbinkey* has to be of type "pointer to declared_it" where declared_it
 * is the name used as first parameter in <typeadapt_getbinarykey_DECLARE>.
 * The second and third parameter must be the same as in <typeadapt_getbinarykey_DECLARE>. */
typeadapt_getbinarykey_it * asgeneric_typeadaptgetbinarykey(void * adpbinkey, TYPENAME typeadapter_t, TYPENAME object_t) ;

/* define: typeadapt_getbinarykey_DECLARE
 * Declares a derived interface from generic <typeadapt_getbinarykey_it>.
 * It is structural compatible with <typeadapt_getbinarykey_it>.
 * See <typeadapt_getbinarykey_it> for a list of contained functions.
 *
 * Parameter:
 * declared_it   - The name of the structure which is declared as the interface.
 *                 The name should have the suffix "_it".
 * typeadapter_t - The adapter type which implements all interface functions.
 *                 The first parameter in every function is a pointer to this type.
 * object_t      - The object type that <typeadapt_getbinarykey_it> supports. */
#define typeadapt_getbinarykey_DECLARE(declared_it, typeadapter_t, object_t)  \
   typedef struct declared_it             declared_it ;                       \
   struct declared_it {                                                       \
      typeadapt_getbinarykey_EMBED(typeadapter_t, object_t) ;                 \
   }

/* define: typeadapt_getbinarykey_EMBED
 * Declares a derived interface from generic <typeadapt_getbinarykey_it>.
 * It is structural compatible with <typeadapt_getbinarykey_it>.
 * See <typeadapt_getbinarykey_it> for a list of contained functions.
 *
 * Parameter:
 * typeadapter_t - The adapter type which implements all interface functions.
 *                 The first parameter in every function is a pointer to this type.
 * object_t      - The object type that <typeadapt_getbinarykey_it> supports. */
#define typeadapt_getbinarykey_EMBED(typeadapter_t, object_t)                                                  \
   void  (* getbinarykey)  (typeadapter_t * typeadp, object_t * node, /*out*/typeadapt_binarykey_t * binkey)   \


// section: inline implementation

/* define: asgeneric_typeadaptbinarykey
 * Implements <typeadapt_binarykey_t.asgeneric_typeadaptbinarykey>. */
#define asgeneric_typeadaptbinarykey(conststr)                       \
   ( __extension__ ({                                                \
      typeof(conststr) _conststr = (conststr) ;                      \
      static_assert(                                                 \
         offsetof(typeadapt_binarykey_t, addr)                       \
         == offsetof(typeof(*(conststr)), addr)                      \
         && offsetof(typeadapt_binarykey_t, size)                    \
            == offsetof(typeof(*(conststr)), size),                  \
         "ensure same structure") ;                                  \
      if (0) {                                                       \
         int _err = 0 ;                                              \
         const uint8_t * _addr = _conststr->addr ;                   \
         size_t        _size   = _conststr->size ;                   \
         _err = (int) (_addr[0] + _size) ;                           \
         (void) _err ;                                               \
      }                                                              \
      (typeadapt_binarykey_t*) _conststr ;                           \
   }))

/* define: asgeneric_typeadaptgetbinarykey
 * Implements <typeadapt_getbinarykey_it.asgeneric_typeadaptgetbinarykey>. */
#define asgeneric_typeadaptgetbinarykey(adpbinkey, typeadapter_t, object_t) \
   ( __extension__ ({                                                \
      static_assert(                                                 \
         offsetof(typeadapt_getbinarykey_it, getbinarykey)           \
         == offsetof(typeof(*(adpbinkey)), getbinarykey),            \
         "ensure same structure") ;                                  \
      if (0) {                                                       \
         (adpbinkey)->getbinarykey(                                  \
                      (typeadapter_t*)0, (object_t*)0,               \
                      (typeadapt_binarykey_t*)0) ;                   \
      }                                                              \
      (typeadapt_getbinarykey_it*) (adpbinkey) ;                     \
   }))

/* function: callgetbinarykey_typeadaptgetbinarykey
 * Implements <typeadapt_getbinarykey_it.callgetbinarykey_typeadaptgetbinarykey>. */
#define callgetbinarykey_typeadaptgetbinarykey(adpbinkey, typeadp, node, binkey) \
   ((adpbinkey)->getbinarykey((typeadp), (node), (binkey)))


#endif
