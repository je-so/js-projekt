/* title: Typeadapt-GetKey

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

   file: C-kern/api/ds/typeadapt/getkey.h
    Header file <Typeadapt-GetKey>.

   file: C-kern/ds/typeadapt/getkey.c
    Implementation file <Typeadapt-GetKey impl>.
*/
#ifndef CKERN_DS_TYPEADAPT_GETKEY_HEADER
#define CKERN_DS_TYPEADAPT_GETKEY_HEADER

// forward:
struct typeadapt_t ;
struct typeadapt_object_t ;

/* typedef: struct typeadapt_binarykey_t
 * Export <typeadapt_binarykey_t> into global namespace. */
typedef struct typeadapt_binarykey_t               typeadapt_binarykey_t ;

/* typedef: struct typeadapt_getkey_it
 * Export <typeadapt_getkey_it> into global namespace. */
typedef struct typeadapt_getkey_it                 typeadapt_getkey_it ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_ds_typeadapt_getkey
 * Test <typeadapt_getkey_it> functionality. */
int unittest_ds_typeadapt_getkey(void) ;
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

/* define: typeadapt_binarykey_INIT
 * Inits <typeadapt_binarykey_t> with size in bytes in start (lowest) memory address. */
#define typeadapt_binarykey_INIT(size, addr)       { addr, size }

// group: generic

/* function: genericcast_typeadaptbinarykey
 * Casts parameter conststr into pointer to <typeadapt_binarykey_t>.
 * Parameter constr should be of type pointer to <string_t>. */
typeadapt_binarykey_t * genericcast_typeadaptbinarykey(void * conststr) ;


/* struct: typeadapt_getkey_it
 * Declares interface for getting a description of a binary key of the object.
 * If you change this interface do not forget to adapt
 * <typeadapt_getkey_EMBED> and <genericcast_typeadaptgetkey>. */
struct typeadapt_getkey_it {
   /* variable: getbinarykey
    * Returns in <typeadapt_binarykey_t> the description of a binary key. */
   void  (*getbinarykey)   (struct typeadapt_t * typeadp, struct typeadapt_object_t * node, /*out*/typeadapt_binarykey_t * binkey) ;
} ;

// group: lifetime

/* define: typeadapt_getkey_INIT_FREEABLE
 * Static initializer. Sets all functions pointers of <typeadapt_getkey_it> to 0. */
#define typeadapt_getkey_INIT_FREEABLE             { 0 }

/* define: typeadapt_getkey_INIT
 * Static initializer. Sets all function pointers to the provided values.
 *
 * Parameters:
 * getbinarykey_f - Function pointer to function returning the binary key of an object. See <typeadapt_getkey_it.getbinarykey>. */
#define typeadapt_getkey_INIT(getbinarykey_f)      { getbinarykey_f }

// group: query

bool isequal_typeadaptgetkey(const typeadapt_getkey_it * ladpgetkey, const typeadapt_getkey_it * radpgetkey) ;

// group: call-service

/* function: callgetbinarykey_typeadaptgetkey
 * Calls function <typeadapt_getkey_it.getbinarykey>.
 * The first parameter is of type <typeadapt_getkey_it> the others are the same as in <typeadapt_getkey_it.getbinarykey>.
 * This function is implemented as macro and supports types derived from <typeadapt_getkey_it> - see use of <typeadapt_getkey_DECLARE>. */
void callgetbinarykey_typeadaptgetkey(typeadapt_getkey_it * adpgetkey, struct typeadapt_t * typeadp, struct typeadapt_object_t * node, /*out*/typeadapt_binarykey_t * binkey) ;

// group: generic

/* define: genericcast_typeadaptgetkey
 * Casts parameter adpgetkey into pointer to <typeadapt_getkey_it>.
 * The parameter *adpgetkey* has to be of type "pointer to declared_it" where declared_it
 * is the name used as first parameter in <typeadapt_getkey_DECLARE>.
 * The second and third parameter must be the same as in <typeadapt_getkey_DECLARE>. */
typeadapt_getkey_it * genericcast_typeadaptgetkey(void * adpgetkey, TYPENAME typeadapter_t, TYPENAME object_t) ;

/* define: typeadapt_getkey_DECLARE
 * Declares a derived interface from generic <typeadapt_getkey_it>.
 * It is structural compatible with <typeadapt_getkey_it>.
 * See <typeadapt_getkey_it> for a list of contained functions.
 *
 * Parameter:
 * declared_it   - The name of the structure which is declared as the interface.
 *                 The name should have the suffix "_it".
 * typeadapter_t - The adapter type which implements all interface functions.
 *                 The first parameter in every function is a pointer to this type.
 * object_t      - The object type that <typeadapt_getkey_it> supports. */
#define typeadapt_getkey_DECLARE(declared_it, typeadapter_t, object_t)  \
   typedef struct declared_it             declared_it ;                 \
   struct declared_it {                                                 \
      typeadapt_getkey_EMBED(typeadapter_t, object_t) ;                 \
   }

/* define: typeadapt_getkey_EMBED
 * Declares a derived interface from generic <typeadapt_getkey_it>.
 * It is structural compatible with <typeadapt_getkey_it>.
 * See <typeadapt_getkey_it> for a list of contained functions.
 *
 * Parameter:
 * typeadapter_t - The adapter type which implements all interface functions.
 *                 The first parameter in every function is a pointer to this type.
 * object_t      - The object type that <typeadapt_getkey_it> supports. */
#define typeadapt_getkey_EMBED(typeadapter_t, object_t)                                                        \
   void  (* getbinarykey)  (typeadapter_t * typeadp, object_t * node, /*out*/typeadapt_binarykey_t * binkey)   \


// section: inline implementation

/* define: genericcast_typeadaptbinarykey
 * Implements <typeadapt_binarykey_t.genericcast_typeadaptbinarykey>. */
#define genericcast_typeadaptbinarykey(conststr)                     \
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

/* define: genericcast_typeadaptgetkey
 * Implements <typeadapt_getkey_it.genericcast_typeadaptgetkey>. */
#define genericcast_typeadaptgetkey(adpgetkey, typeadapter_t, object_t)    \
   ( __extension__ ({                                                      \
      static_assert(                                                       \
         offsetof(typeadapt_getkey_it, getbinarykey)                       \
         == offsetof(typeof(*(adpgetkey)), getbinarykey),                  \
         "ensure same structure") ;                                        \
      if (0) {                                                             \
         (adpgetkey)->getbinarykey(                                        \
                      (typeadapter_t*)0, (object_t*)0,                     \
                      (typeadapt_binarykey_t*)0) ;                         \
      }                                                                    \
      (typeadapt_getkey_it*) (adpgetkey) ;                                 \
   }))

/* function: callgetbinarykey_typeadaptgetkey
 * Implements <typeadapt_getkey_it.callgetbinarykey_typeadaptgetkey>. */
#define callgetbinarykey_typeadaptgetkey(adpgetkey, typeadp, node, binkey) \
   ((adpgetkey)->getbinarykey((typeadp), (node), (binkey)))


#endif
