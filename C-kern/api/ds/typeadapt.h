/* title: Typeadapt

   Exports typeadapter which allows containers in the data store to
   manage the lifetime of stored objects or to compare objects and keys.

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

   file: C-kern/api/ds/typeadapt.h
    Header file <Typeadapt>.

   file: C-kern/ds/typeadapt.c
    Implementation file <Typeadapt impl>.
*/
#ifndef CKERN_DS_TYPEADAPT_HEADER
#define CKERN_DS_TYPEADAPT_HEADER

#include "C-kern/api/ds/typeadapt/comparator.h"
#include "C-kern/api/ds/typeadapt/gethash.h"
#include "C-kern/api/ds/typeadapt/getkey.h"
#include "C-kern/api/ds/typeadapt/lifetime.h"
#include "C-kern/api/ds/typeadapt/nodeoffset.h"

/* typedef: struct typeadapt_t
 * Export <typeadapt_t> into global namespace. */
typedef struct typeadapt_t             typeadapt_t ;

/* typedef: typeadapt_member_t
 * Export <typeadapt_member_t> into global namespace. */
typedef struct typeadapt_member_t      typeadapt_member_t ;

/* typedef: typeadapt_object_t
 * Declares abstract object type.
 * It is used to tag objects that want to be stored in the data store. */
typedef struct typeadapt_object_t      typeadapt_object_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_ds_typeadapt
 * Test <typeadapt_t> functionality. */
int unittest_ds_typeadapt(void) ;
#endif


/* struct: typeadapt_member_t
 * Relates <typeadapt_nodeoffset_t> with <typeadapt_t>.
 * With this object it is possible to call services on a pointer.
 * The pointer needs not point to the start address of the object but
 * could point to any struct member. The variable <nodeoff> stores
 * the information to cast the pointer into the generic type
 * <typeadapt_object_t> which is used to tag any object type. */
struct typeadapt_member_t {
   /* variable: typeadp
    * Pointer to <typeadapt_t>. */
   typeadapt_t         *   typeadp ;
   /* variable: nodeoff
    * Stores <typeadapt_nodeoffset_t>. */
   typeadapt_nodeoffset_t  nodeoff ;
} ;

// group: lifetime

/* define: typeadapt_member_INIT
 * Static initializer. */
#define typeadapt_member_INIT(typeadp, nodeoffset)    { typeadp, typeadapt_nodeoffset_INIT(nodeoffset) }

/* define: typeadapt_member_INIT_FREEABLE
 * Static initializer. */
#define typeadapt_member_INIT_FREEABLE                typeadapt_member_INIT(0, 0)

// group: query

/* function: isequal_typeadaptmember
 * Returns true if both <typeadapt_member_t>s are equal. */
bool isequal_typeadaptmember(const typeadapt_member_t * ltypeadp, const typeadapt_member_t * rtypeadp) ;

// group: call-services

/* function: callnewcopy_typeadaptmember
 * See <callnewcopy_typeadapt>. */
int callnewcopy_typeadaptmember(typeadapt_member_t * nodeadp, ...) ;

/* function: calldelete_typeadaptmember
 * See <calldelete_typeadapt>. */
int calldelete_typeadaptmember(typeadapt_member_t * nodeadp, ...) ;

/* function: callcmpkeyobj_typeadaptmember
 * See <callcmpkeyobj_typeadapt>. */
int callcmpkeyobj_typeadaptmember(typeadapt_member_t * nodeadp, ...) ;

/* function: callcmpobj_typeadaptmember
 * See <callcmpobj_typeadapt>. */
int callcmpobj_typeadaptmember(typeadapt_member_t * nodeadp, ...) ;

/* function: callhashobject_typeadaptmember
 * See <callhashobject_typeadapt>. */
int callhashobject_typeadaptmember(typeadapt_member_t * nodeadp, ...) ;

/* function: callhashkey_typeadaptmember
 * See <callhashkey_typeadapt>. */
int callhashkey_typeadaptmember(typeadapt_member_t * nodeadp, ...) ;

/* function: callgetbinarykey_typeadaptmember
 * See <callgetbinarykey_typeadapt>. */
void callgetbinarykey_typeadaptmember(typeadapt_member_t * nodeadp, ...) ;

// group: conversion

/* function: memberasobject_typeadaptmember
 * See <memberasobject_typeadaptnodeoffset>. */
struct typeadapt_object_t * memberasobject_typeadaptmember(typeadapt_member_t * nodeadp, void * node) ;

/* function: objectasmember_typeadaptmember
 * See <objectasmember_typeadaptnodeoffset>. */
void * objectasmember_typeadaptmember(typeadapt_member_t * nodeadp, struct typeadapt_object_t * object) ;


/* struct: typeadapt_t
 * Interface to services needed by containers in the data store.
 * With this interface you adapt any type to be manageable by containers
 * in the data store. */
struct typeadapt_t {
   /* variable: comparator
    * Interface to adapt comparison of key and object. See <typeadapt_comparator_it>. */
   typeadapt_comparator_it    comparator ;
   /* variable: gethash
    * Interface to adapt reading of a hash value. See <typeadapt_gethash_it>. */
   typeadapt_gethash_it       gethash ;
   /* variable: getkey
    * Interface to adapt reading of a key as binary data. See <typeadapt_getkey_it>. */
   typeadapt_getkey_it        getkey ;
   /* variable: lifetime
    * Interface to adapt the lifetime of an object type. See <typeadapt_lifetime_it>. */
   typeadapt_lifetime_it      lifetime ;
} ;

// group: lifetime

/* define: typeadapt_INIT_FREEABLE
 * Static initializer. */
#define typeadapt_INIT_FREEABLE \
   { typeadapt_comparator_INIT_FREEABLE, typeadapt_gethash_INIT_FREEABLE, typeadapt_getkey_INIT_FREEABLE, typeadapt_lifetime_INIT_FREEABLE }

/* define: typeadapt_INIT_LIFETIME
 * Static initializer. Uses <typeadapt_lifetime_INIT> to init interface <typeadapt_t.lifetime>. */
#define typeadapt_INIT_LIFETIME(newcopyobj_f, deleteobj_f) \
   { typeadapt_comparator_INIT_FREEABLE, typeadapt_gethash_INIT_FREEABLE, typeadapt_getkey_INIT_FREEABLE, typeadapt_lifetime_INIT(newcopyobj_f, deleteobj_f) }

/* define: typeadapt_INIT_CMP
 * Static initializer. Uses <typeadapt_comparator_INIT> to init interface <typeadapt_t.comparator>. */
#define typeadapt_INIT_CMP(cmpkeyobj_f, cmpobj_f) \
   { typeadapt_comparator_INIT(cmpkeyobj_f, cmpobj_f), typeadapt_gethash_INIT_FREEABLE, typeadapt_getkey_INIT_FREEABLE, typeadapt_lifetime_INIT_FREEABLE }

/* define: typeadapt_INIT_LIFECMP
 * Static initializer. Initializes <typeadapt_t.lifetime> and <typeadapt_t.comparator> service interfaces. */
#define typeadapt_INIT_LIFECMP(newcopyobj_f, deleteobj_f, cmpkeyobj_f, cmpobj_f) \
   { typeadapt_comparator_INIT(cmpkeyobj_f, cmpobj_f), typeadapt_gethash_INIT_FREEABLE, typeadapt_getkey_INIT_FREEABLE, typeadapt_lifetime_INIT(newcopyobj_f, deleteobj_f) }

/* define: typeadapt_INIT_LIFEKEY
 * Static initializer. Initializes <typeadapt_t.lifetime> and <typeadapt_t.getkey> service interfaces. */
#define typeadapt_INIT_LIFEKEY(newcopyobj_f, deleteobj_f, getbinarykey_f) \
   { typeadapt_comparator_INIT_FREEABLE, typeadapt_gethash_INIT_FREEABLE, typeadapt_getkey_INIT(getbinarykey_f), typeadapt_lifetime_INIT(newcopyobj_f, deleteobj_f) }

/* define: typeadapt_INIT_LIFECMPKEY
 * Static initializer. Initializes <typeadapt_t.lifetime>, <typeadapt_t.comparator>, and <typeadapt_t.getkey> service interfaces. */
#define typeadapt_INIT_LIFECMPKEY(newcopyobj_f, deleteobj_f, cmpkeyobj_f, cmpobj_f, getbinarykey_f) \
   { typeadapt_comparator_INIT(cmpkeyobj_f, cmpobj_f), typeadapt_gethash_INIT_FREEABLE, typeadapt_getkey_INIT(getbinarykey_f), typeadapt_lifetime_INIT(newcopyobj_f, deleteobj_f) }

/* define: typeadapt_INIT_LIFECMPHASH
 * Static initializer. Initializes <typeadapt_t.lifetime>, <typeadapt_t.comparator>, and <typeadapt_t.gethash> service interfaces. */
#define typeadapt_INIT_LIFECMPHASH(newcopyobj_f, deleteobj_f, cmpkeyobj_f, cmpobj_f, hashobject_f, hashkey_f) \
   { typeadapt_comparator_INIT(cmpkeyobj_f, cmpobj_f), typeadapt_gethash_INIT(hashobject_f, hashkey_f), typeadapt_getkey_INIT_FREEABLE, typeadapt_lifetime_INIT(newcopyobj_f, deleteobj_f) }

/* define: typeadapt_INIT_LIFECMPHASHKEY
 * Static initializer. Initializes <typeadapt_t.lifetime>, <typeadapt_t.comparator>, <typeadapt_t.gethash>, and <typeadapt_t.getkey> service interfaces. */
#define typeadapt_INIT_LIFECMPHASHKEY(newcopyobj_f, deleteobj_f, cmpkeyobj_f, cmpobj_f, hashobject_f, hashkey_f, getbinarykey_f) \
   { typeadapt_comparator_INIT(cmpkeyobj_f, cmpobj_f), typeadapt_gethash_INIT(hashobject_f, hashkey_f), typeadapt_getkey_INIT(getbinarykey_f), typeadapt_lifetime_INIT(newcopyobj_f, deleteobj_f) }

// group: query

/* function: isequal_typeadapt
 * Returns true if both <typeadapt_t> are equal. */
bool isequal_typeadapt(const typeadapt_t * ltypeadp, const typeadapt_t * rtypeadp) ;

/* function: iscalldelete_typeadapt
 * Returns true if <typeadapt_lifetime_it.delete_object> is not NULL. */
bool iscalldelete_typeadapt(const typeadapt_t * typeadp) ;

// group: lifetime-service

/* function: callnewcopy_typeadapt
 * Wrapper to call <callnewcopy_typeadaptlifetime>. */
int callnewcopy_typeadapt(typeadapt_t * typeadp, ...) ;

/* function: calldelete_typeadapt
 * Wrapper to call <calldelete_typeadaptlifetime>. */
int calldelete_typeadapt(typeadapt_t * typeadp, ...) ;

// group: comparator-service

/* function: callcmpkeyobj_typeadapt
 * Wrapper to call <callcmpkeyobj_typeadaptcomparator>. */
int callcmpkeyobj_typeadapt(typeadapt_t * typeadp, ...) ;

/* function: callcmpobj_typeadapt
 * Wrapper to call <callcmpobj_typeadaptcomparator>. */
int callcmpobj_typeadapt(typeadapt_t * typeadp, ...) ;

// group: gethash-service

/* function: callhashobject_typeadapt
 * Wrapper to call <callhashobject_typeadaptgethash>. */
int callhashobject_typeadapt(typeadapt_t * typeadp, ...) ;

/* function: callhashkey_typeadapt
 * Wrapper to call <callhashkey_typeadaptgethash>. */
int callhashkey_typeadapt(typeadapt_t * typeadp, ...) ;

// group: getkey-service

/* function: callgetbinarykey_typeadapt
 * Wrapper to call <callgetbinarykey_typeadaptgetkey>. */
int callgetbinarykey_typeadapt(typeadapt_t * typeadp, ...) ;

// group: generic

/* function: genericcast_typeadapt
 * Casts parameter typeadp into pointer to <typeadapt_t>.
 * The parameter *typeadp* has to be of type "pointer to type" that
 * embeds the generic public interface with <typeadapt_EMBED> as first member. */
typeadapt_t * genericcast_typeadapt(void * typeadp, TYPENAME testadapter_t, TYPENAME object_t) ;

/* function: typeadapt_DECLARE
 * Declares a derived interface from generic <typeadapt_t>.
 * It is structural compatible with <typeadapt_t>.
 * See <typeadapt_t> for a list of embedded services.
 *
 * Parameter:
 * typeadapter_t - The name of the structure which is declared as the interface.
 *                 The first parameter of every function is a also pointer to this type.
 * object_t      - The object type that <typeadapter_t> supports.
 * key_t         - The key type that <typeadapt_t> supports. Must be of size sizeof(void*). */
void typeadapt_DECLARE(TYPENAME typeadapter_t, TYPENAME object_t, TYPENAME key_t) ;

/* function: typeadapt_EMBED
 * Embeds the public interface part from generic <typeadapt_t>.
 * It is structural compatible with <typeadapt_t>.
 * See <typeadapt_t> for a list of embedded services.
 *
 * Parameter:
 * typeadapter_t - The adapter type which implements all interface functions.
 *                 The first parameter in every function is a pointer to this type.
 * object_t      - The object type that <typeadapter_t> supports.
 * key_t         - The key type that <typeadapt_t> supports. Must be of size sizeof(void*).
 * */
void typeadapt_EMBED(TYPENAME typeadapter_t, TYPENAME object_t, TYPENAME key_t) ;


// section: inline implementation

/* define: genericcast_typeadapt
 * Implements <typeadapt_t.genericcast_typeadapt>. */
#define genericcast_typeadapt(typeadp, typeadapter_t, object_t, key_t)                       \
   ( __extension__ ({                                                                        \
      static_assert(                                                                         \
         offsetof(typeof(*(typeadp)), comparator) == offsetof(typeadapt_t, comparator)       \
         && offsetof(typeof(*(typeadp)), gethash) == offsetof(typeadapt_t, gethash)          \
         && offsetof(typeof(*(typeadp)), getkey) == offsetof(typeadapt_t, getkey)            \
         && offsetof(typeof(*(typeadp)), lifetime) == offsetof(typeadapt_t, lifetime),       \
         "ensure same structure") ;                                                          \
      if (0) {                                                                               \
         (void) genericcast_typeadaptcomparator((typeof((typeadp)->comparator)*)0, typeadapter_t, object_t, key_t) ;   \
         (void) genericcast_typeadaptgethash((typeof((typeadp)->gethash)*)0, typeadapter_t, object_t, key_t) ;         \
         (void) genericcast_typeadaptgetkey((typeof((typeadp)->getkey)*)0, typeadapter_t, object_t) ;      \
         (void) genericcast_typeadaptlifetime((typeof((typeadp)->lifetime)*)0, typeadapter_t, object_t) ;  \
      }                                                                                      \
      (typeadapt_t*) (typeadp) ;                                                             \
   }))

/* define: callcmpkeyobj_typeadapt
 * Implements <typeadapt_t.callcmpkeyobj_typeadapt>. */
#define callcmpkeyobj_typeadapt(typeadp, ...)         callcmpkeyobj_typeadaptcomparator(&(typeadp)->comparator, typeadp, __VA_ARGS__)

/* define: callcmpkeyobj_typeadaptmember
 * Implements <typeadapt_member_t.callcmpkeyobj_typeadaptmember>. */
#define callcmpkeyobj_typeadaptmember(nodeadp, ...)   callcmpkeyobj_typeadapt((nodeadp)->typeadp, __VA_ARGS__)

/* define: callcmpobj_typeadapt
 * Implements <typeadapt_t.callcmpobj_typeadapt>. */
#define callcmpobj_typeadapt(typeadp, ...)            callcmpobj_typeadaptcomparator(&(typeadp)->comparator, typeadp, __VA_ARGS__)

/* define: callcmpobj_typeadaptmember
 * Implements <typeadapt_member_t.callcmpobj_typeadaptmember>. */
#define callcmpobj_typeadaptmember(nodeadp, ...)      callcmpobj_typeadapt((nodeadp)->typeadp, __VA_ARGS__)

/* define: calldelete_typeadapt
 * Implements <typeadapt_t.calldelete_typeadapt>. */
#define calldelete_typeadapt(typeadp, ...)            calldelete_typeadaptlifetime(&(typeadp)->lifetime, typeadp, __VA_ARGS__)

/* define: calldelete_typeadaptmember
 * Implements <typeadapt_member_t.calldelete_typeadaptmember>. */
#define calldelete_typeadaptmember(nodeadp, ...)      calldelete_typeadapt((nodeadp)->typeadp, __VA_ARGS__)

/* define: callgetbinarykey_typeadapt
 * Implements <typeadapt_t.callgetbinarykey_typeadapt>. */
#define callgetbinarykey_typeadapt(typeadp, ...)      callgetbinarykey_typeadaptgetkey(&(typeadp)->getkey, typeadp, __VA_ARGS__)

/* define: callgetbinarykey_typeadaptmember
 * Implements <typeadapt_member_t.callgetbinarykey_typeadaptmember>. */
#define callgetbinarykey_typeadaptmember(nodeadp, ...)   callgetbinarykey_typeadapt((nodeadp)->typeadp, __VA_ARGS__)

/* define: callhashkey_typeadapt
 * Implements <typeadapt_t.callhashkey_typeadapt>. */
#define callhashkey_typeadapt(typeadp, ...)           callhashkey_typeadaptgethash(&(typeadp)->gethash, typeadp, __VA_ARGS__)

/* define: callhashkey_typeadaptmember
 * Implements <typeadapt_member_t.callhashkey_typeadaptmember>. */
#define callhashkey_typeadaptmember(nodeadp, ...)     callhashkey_typeadapt((nodeadp)->typeadp, __VA_ARGS__)

/* define: callhashobject_typeadapt
 * Implements <typeadapt_t.callhashobject_typeadapt>. */
#define callhashobject_typeadapt(typeadp, ...)        callhashobject_typeadaptgethash(&(typeadp)->gethash, typeadp, __VA_ARGS__)

/* define: callhashobject_typeadaptmember
 * Implements <typeadapt_member_t.callhashobject_typeadaptmember>. */
#define callhashobject_typeadaptmember(nodeadp, ...)  callhashobject_typeadapt((nodeadp)->typeadp, __VA_ARGS__)

/* define: callnewcopy_typeadapt
 * Implements <typeadapt_t.callnewcopy_typeadapt>. */
#define callnewcopy_typeadapt(typeadp, ...)           callnewcopy_typeadaptlifetime(&(typeadp)->lifetime, typeadp, __VA_ARGS__)

/* define: callnewcopy_typeadaptmember
 * Implements <typeadapt_member_t.callnewcopy_typeadaptmember>. */
#define callnewcopy_typeadaptmember(nodeadp, ...)     callnewcopy_typeadapt((nodeadp)->typeadp, __VA_ARGS__)

/* define: iscalldelete_typeadapt
 * Implements <typeadapt_t.iscalldelete_typeadapt>. */
#define iscalldelete_typeadapt(typeadp)               (0 != (typeadp)->lifetime.delete_object)

/* define: memberasobject_typeadaptmember
 * Imeplements <typeadapt_member_t.memberasobject_typeadaptmember>. */
#define memberasobject_typeadaptmember(nodeadp, node) memberasobject_typeadaptnodeoffset((nodeadp)->nodeoff, node)

/* define: objectasmember_typeadaptmember
 * Imeplements <typeadapt_member_t.objectasmember_typeadaptmember>. */
#define objectasmember_typeadaptmember(nodeadp, object)  objectasmember_typeadaptnodeoffset((nodeadp)->nodeoff, object)

/* define: typeadapt_EMBED
 * Implements <typeadapt_t.typeadapt_EMBED>. */
#define typeadapt_EMBED(typeadapter_t, object_t, key_t)              \
   struct {                                                          \
      typeadapt_comparator_EMBED(typeadapter_t, object_t, key_t) ;   \
   } comparator ;                                                    \
   struct {                                                          \
      typeadapt_gethash_EMBED(typeadapter_t, object_t, key_t) ;      \
   } gethash ;                                                       \
   struct {                                                          \
      typeadapt_getkey_EMBED(typeadapter_t, object_t) ;              \
   } getkey ;                                                        \
   struct {                                                          \
      typeadapt_lifetime_EMBED(typeadapter_t, object_t) ;            \
   } lifetime                                                        \

/* define: typeadapt_DECLARE
 * Implements <typeadapt_t.typeadapt_DECLARE>. */
#define typeadapt_DECLARE(typeadapter_t, object_t, key_t)            \
   typedef struct typeadapter_t           typeadapter_t ;            \
   struct typeadapter_t {                                            \
      typeadapt_EMBED(typeadapter_t, object_t, key_t) ;              \
   }

#endif
