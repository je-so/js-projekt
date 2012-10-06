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

#include "C-kern/api/ds/typeadapt/keycomparator.h"
#include "C-kern/api/ds/typeadapt/lifetime.h"
#include "C-kern/api/ds/typeadapt/typeinfo.h"

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
 * Relates <typeadapt_typeinfo_t> with <typeadapt_t>.
 * With this object it is possible to call services on a pointer.
 * The pointer needs not point to the start address of the object but
 * could point to any struct member. The variable <typeinfo> stores
 * the information to cast the pointer into the generic type
 * <typeadapt_object_t> which is used to tag any object type. */
struct typeadapt_member_t {
   /* variable: typeadp
    * Pointer to <typeadapt_t>. */
   typeadapt_t          * typeadp ;
   /* variable: typeinfo
    * Stores <typeadapt_typeinfo_t>. */
   typeadapt_typeinfo_t typeinfo ;
} ;

// group: lifetime

/* define: typeadapt_member_INIT
 * Static initializer. */
#define typeadapt_member_INIT(typeadp, nodeoffset)    { typeadp, typeadapt_typeinfo_INIT(nodeoffset) }

/* define: typeadapt_member_INIT_FREEABLE
 * Static initializer. */
#define typeadapt_member_INIT_FREEABLE                typeadapt_member_INIT(0, 0)

// group: query

/* function: isequal_typeadaptmember
 * Returns true if both <typeadapt_member_t>s are equal. */
bool isequal_typeadaptmember(const typeadapt_member_t * ltypeadp, const typeadapt_member_t * rtypeadp) ;


/* struct: typeadapt_t
 * Interface to services needed by containers in the data store.
 * With this interface you adapt any type to be manageable by containers
 * in the data store. */
struct typeadapt_t {
   /* variable: lifetime
    * Interface to adapt the lifetime of an object type. See <typeadapt_lifetime_it>. */
   struct {
      typeadapt_lifetime_EMBED(typeadapt_t, typeadapt_object_t) ;
   } lifetime ;
   /* variable: keycomparator
    * Interface to adapt comparison of key and object. See <typeadapt_keycomparator_it>. */
   struct {
      typeadapt_keycomparator_EMBED(typeadapt_t, typeadapt_object_t, void) ;
   } keycomparator ;
} ;

// group: lifetime

/* define: typeadapt_INIT_FREEABLE
 * Static initializer. */
#define typeadapt_INIT_FREEABLE                    { typeadapt_lifetime_INIT_FREEABLE, typeadapt_keycomparator_INIT_FREEABLE }

/* define: typeadapt_INIT_LIFETIME
 * Static initializer. Uses <typeadapt_lifetime_INIT> to init interface <typeadapt_t.lifetime>. */
#define typeadapt_INIT_LIFETIME(newcopyobj_f, deleteobj_f) \
  { typeadapt_lifetime_INIT(newcopyobj_f, deleteobj_f), typeadapt_keycomparator_INIT_FREEABLE }

/* define: typeadapt_INIT_KEYCMP
 * Static initializer. Uses <typeadapt_keycomparator_INIT> to init interface <typeadapt_t.keycomparator>. */
#define typeadapt_INIT_KEYCMP(cmpkeyobj_f, cmpobj_f) \
  { typeadapt_lifetime_INIT_FREEABLE, typeadapt_keycomparator_INIT(cmpkeyobj_f, cmpobj_f) }

/* define: typeadapt_INIT_LIFEKEYCMP
 * Static initializer. Initializes <typeadapt_t.lifetime> and <typeadapt_t.keycomparator> service interfaces. */
#define typeadapt_INIT_LIFEKEYCMP(newcopyobj_f, deleteobj_f, cmpkeyobj_f, cmpobj_f) \
   { typeadapt_lifetime_INIT(newcopyobj_f, deleteobj_f), typeadapt_keycomparator_INIT(cmpkeyobj_f, cmpobj_f) }

// group: query

/* function: islifetimedelete_typeadapt
 * Returns true if <typeadapt_lifetime_it.delete_object> is not NULL. */
bool islifetimedelete_typeadapt(const typeadapt_t * typeadp) ;

// group: lifetime-service

/* function: callnewcopy_typeadapt
 * Wrapper to call <callnewcopy_typeadaptlifetime>. */
int callnewcopy_typeadapt(typeadapt_t * typeadp, ...) ;

/* function: calldelete_typeadapt
 * Wrapper to call <calldelete_typeadaptlifetime>. */
int calldelete_typeadapt(typeadapt_t * typeadp, ...) ;

// group: keycomparator-service

/* function: callcmpkeyobj_typeadapt
 * Wrapper to call <callcmpkeyobj_typeadaptkeycomparator>. */
int callcmpkeyobj_typeadapt(typeadapt_t * typeadp, ...) ;

/* function: callcmpobj_typeadapt
 * Wrapper to call <callcmpobj_typeadaptkeycomparator>. */
int callcmpobj_typeadapt(typeadapt_t * typeadp, ...) ;

// group: generic

/* function: asgeneric_typeadapt
 * Casts parameter typeadp into pointer to <typeadapt_t>.
 * The parameter *typeadp* has to be of type "pointer to own typeadapter" that
 * embeds the generic public interface with <typeadapt_EMBED> as first member. */
typeadapt_t * asgeneric_typeadapt(void * typeadp, TYPENAME testadapter_t, TYPENAME object_t) ;

/* function: typeadapt_EMBED
 * Embeds the public interface part from generic <typeadapt_t>.
 * It is structural compatible with <typeadapt_t>.
 * See <typeadapt_t> for a list of embedded services.
 *
 * Parameter:
 * typeadapter_t - The adapter type which implements all interface functions.
 *                 The first parameter in every function is a pointer to this type.
 * object_t      - The object type that typeadapter_t supports.
 * */
void typeadapt_EMBED(TYPENAME typeadapter_t, TYPENAME object_t, TYPENAME key_t) ;


// section: inline implementation

/* define: asgeneric_typeadapt
 * Implements <typeadapt_t.asgeneric_typeadapt>. */
#define asgeneric_typeadapt(typeadp, typeadapter_t, object_t, key_t)                                     \
   ( __extension__ ({                                                                                    \
      static_assert(offsetof(typeof(*(typeadp)), lifetime) == offsetof(typeadapt_t, lifetime),           \
         "ensure same structure") ;                                                                      \
      static_assert(offsetof(typeof(*(typeadp)), keycomparator) == offsetof(typeadapt_t, keycomparator), \
         "ensure same structure") ;                                                                      \
      static_assert((typeadapt_lifetime_it*)&(typeadp)->lifetime                                         \
         == asgeneric_typeadaptlifetime(&(typeadp)->lifetime, typeadapter_t, object_t),                  \
         "ensure correct lifetime service") ;                                                            \
      static_assert((typeadapt_keycomparator_it*)&(typeadp)->keycomparator                               \
         == asgeneric_typeadaptkeycomparator(&(typeadp)->keycomparator, typeadapter_t, object_t, key_t), \
         "ensure correct keycomparator service") ;                                                       \
      (typeadapt_t*) (typeadp) ;                                                                         \
   }))

/* define: callnewcopy_typeadapt
 * Implements <typeadapt_t.callnewcopy_typeadapt>. */
#define callnewcopy_typeadapt(typeadp, ...)     callnewcopy_typeadaptlifetime(&(typeadp)->lifetime, typeadp, __VA_ARGS__)

/* define: calldelete_typeadapt
 * Implements <typeadapt_t.calldelete_typeadapt>. */
#define calldelete_typeadapt(typeadp, ...)      calldelete_typeadaptlifetime(&(typeadp)->lifetime, typeadp, __VA_ARGS__)

/* function: callcmpkeyobj_typeadapt
 * Implements <typeadapt_t.callcmpkeyobj_typeadapt>. */
#define callcmpkeyobj_typeadapt(typeadp, ...)   callcmpkeyobj_typeadaptkeycomparator(&(typeadp)->keycomparator, typeadp, __VA_ARGS__)

/* function: callcmpobj_typeadapt
 * Implements <typeadapt_t.callcmpobj_typeadapt>. */
#define callcmpobj_typeadapt(typeadp, ...)      callcmpobj_typeadaptkeycomparator(&(typeadp)->keycomparator, typeadp, __VA_ARGS__)

/* define: isequal_typeadaptmember
 * Implements <typeadapt_t.isequal_typeadaptmember>. */
#define isequal_typeadaptmember(ltypeadp, rtypeadp)                                 \
   (  (ltypeadp)->typeadp == (rtypeadp)->typeadp                                    \
      && isequal_typeadapttypeinfo(&(ltypeadp)->typeinfo, &(rtypeadp)->typeinfo))

/* define: islifetimedelete_typeadapt
 * Implements <typeadapt_t.islifetimedelete_typeadapt>. */
#define islifetimedelete_typeadapt(typeadp)     (0 != (typeadp)->lifetime.delete_object)

/* define: typeadapt_EMBED
 * Implements <typeadapt_t.typeadapt_EMBED>. */
#define typeadapt_EMBED(typeadapter_t, object_t, key_t)                 \
   struct {                                                             \
      typeadapt_lifetime_EMBED(typeadapter_t, object_t) ;               \
   } lifetime ;                                                         \
   struct {                                                             \
      typeadapt_keycomparator_EMBED(typeadapter_t, object_t, key_t) ;   \
   } keycomparator

#endif
