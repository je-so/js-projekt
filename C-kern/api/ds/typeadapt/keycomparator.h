/* title: Typeadapt-KeyComparator

   Abstract interface (function table) to adapt
   a concrete user type to a container which needs
   comparing nodes and nodes and keys.

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

   file: C-kern/api/ds/typeadapt/keycomparator.h
    Header file <Typeadapt-KeyComparator>.

   file: C-kern/ds/typeadapt/keycomparator.c
    Implementation file <Typeadapt-KeyComparator impl>.
*/
#ifndef CKERN_DS_TYPEADAPT_KEYCOMPARATOR_HEADER
#define CKERN_DS_TYPEADAPT_KEYCOMPARATOR_HEADER

// forward:
struct typeadapt_t ;
struct typeadapt_object_t ;

/* typedef: struct typeadapt_keycomparator_it
 * Export <typeadapt_keycomparator_it> into global namespace. */
typedef struct typeadapt_keycomparator_it          typeadapt_keycomparator_it ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_ds_typeadapt_keycomparator
 * Test <typeadapt_keycomparator_it> functionality. */
int unittest_ds_typeadapt_keycomparator(void) ;
#endif


/* struct: typeadapt_keycomparator_it
 * Declares interface for comparing two objects and key with object.
 * If you change this interface do not forget to adapt
 * <typeadapt_keycomparator_EMBED> and <asgeneric_typeadaptkeycomparator>. */
struct typeadapt_keycomparator_it {
   /* variable: cmp_key_object
    * Compares a key with an object. lkey is the left operand and robject the right one.
    *
    * Returns:
    * -1 - key <  robject
    * 0  - key == robject
    * +1 - key >  robject */
   int  (* cmp_key_object) (struct typeadapt_t * typeadp, const void * lkey, const struct typeadapt_object_t * robject) ;
   /* variable: cmp_object
    * Compares two objects. lobject is left operand and robject the right one.
    *
    * Returns:
    * -1 - lobject <  robject
    * 0  - lobject == robject
    * +1 - lobject >  robject */
   int  (* cmp_object)     (struct typeadapt_t * typeadp, const struct typeadapt_object_t * lobject, const struct typeadapt_object_t * robject) ;
} ;

// group: lifetime

/* define: typeadapt_keycomparator_INIT_FREEABLE
 * Static initializer. Sets all functions pointers of <typeadapt_keycomparator_it> to 0. */
#define typeadapt_keycomparator_INIT_FREEABLE                  { 0, 0 }

/* define: typeadapt_keycomparator_INIT
 * Static initializer. Sets all function pointers to the provided values.
 *
 * Parameters:
 * cmpkeyobj_f - Function pointer to function comparing key with object. See <typeadapt_keycomparator_it.cmp_key_object>.
 * cmpobj_f    - Function pointer to function comparing object with object. See <typeadapt_keycomparator_it.cmp_object>. */
#define typeadapt_keycomparator_INIT(cmpkeyobj_f, cmpobj_f)    { cmpkeyobj_f, cmpobj_f }

// group: query

bool isequal_typeadaptkeycomparator(const typeadapt_keycomparator_it * ladpcmp, const typeadapt_keycomparator_it * radpcmp) ;

// group: call-service

/* function: callcmpkeyobj_typeadaptkeycomparator
 * Calls function <typeadapt_keycomparator_it.cmp_key_object>.
 * The first parameter is of type <typeadapt_keycomparator_it> the others are the same as in <typeadapt_keycomparator_it.cmp_key_object>.
 * This function is implemented as macro and supports types derived from <typeadapt_keycomparator_it> - see * with use of <typeadapt_keycomparator_DECLARE>. */
int callcmpkeyobj_typeadaptkeycomparator(typeadapt_keycomparator_it * adpcmp, struct typeadapt_t * typeadp, const void * key, const struct typeadapt_object_t * robject) ;

/* function: callcmpobj_typeadaptkeycomparator
 * Calls function <typeadapt_keycomparator_it.cmp_object>.
 * The first parameter is of type <typeadapt_keycomparator_it> the others are the same as in <typeadapt_keycomparator_it.cmp_object>.
 * This function is implemented as macro and supports types derived from <typeadapt_keycomparator_it> - see * with use of <typeadapt_keycomparator_DECLARE>. */
int callcmpobj_typeadaptkeycomparator(typeadapt_keycomparator_it * adpcmp, struct typeadapt_t * typeadp, const struct typeadapt_object_t * lobject, const struct typeadapt_object_t * robject) ;

// group: generic

/* define: asgeneric_typeadaptkeycomparator
 * Casts parameter adpcmp into pointer to <typeadapt_keycomparator_it>.
 * The parameter *adpcmp* has to be of type "pointer to declared_it" where declared_it
 * is the name used as first parameter in <typeadapt_keycomparator_DECLARE>.
 * The second and third parameter must be the same as in <typeadapt_keycomparator_DECLARE>. */
typeadapt_keycomparator_it * asgeneric_typeadaptkeycomparator(void * adpcmp, TYPENAME typeadapter_t, TYPENAME object_t, TYPENAME key_t) ;

/* define: typeadapt_keycomparator_DECLARE
 * Declares a derived interface from generic <typeadapt_keycomparator_it>.
 * It is structural compatible with <typeadapt_keycomparator_it>.
 * See <typeadapt_keycomparator_it> for a list of contained functions.
 *
 * Parameter:
 * declared_it   - The name of the structure which is declared as the interface.
 *                 The name should have the suffix "_it".
 * typeadapter_t - The adapter type which implements all interface functions.
 *                 The first parameter in every function is a pointer to this type.
 * object_t      - The object type that <typeadapt_keycomparator_it> supports.
 * key_t         - The key type that <typeadapt_keycomparator_it> supports. */
#define typeadapt_keycomparator_DECLARE(declared_it, typeadapter_t, object_t, key_t)   \
   typedef struct declared_it             declared_it ;                                \
   struct declared_it {                                                                \
      typeadapt_keycomparator_EMBED(typeadapter_t, object_t, key_t) ;                  \
   }

/* define: typeadapt_keycomparator_EMBED
 * Declares a derived interface from generic <typeadapt_keycomparator_it>.
 * It is structural compatible with <typeadapt_keycomparator_it>.
 * See <typeadapt_keycomparator_it> for a list of contained functions.
 *
 * Parameter:
 * declared_it   - The name of the structure which is declared as the interface.
 *                 The name should have the suffix "_it".
 * typeadapter_t - The adapter type which implements all interface functions.
 *                 The first parameter in every function is a pointer to this type.
 * object_t      - The object type that <typeadapt_keycomparator_it> supports.
 * key_t         - The key type that <typeadapt_keycomparator_it> supports. */
#define typeadapt_keycomparator_EMBED(typeadapter_t, object_t, key_t)                                          \
   int  (* cmp_key_object)  (typeadapter_t * typeadp, const key_t * lkey, const object_t * robject) ;          \
   int  (* cmp_object)      (typeadapter_t * typeadp, const object_t * lobject, const object_t  * robject)


// section: inline implementation

/* define: asgeneric_typeadaptkeycomparator
 * Implements <typeadapt_keycomparator_it.asgeneric_typeadaptkeycomparator>. */
#define asgeneric_typeadaptkeycomparator(adpcmp, typeadapter_t, object_t, key_t) \
   ( __extension__ ({                                                   \
      static_assert(                                                    \
         offsetof(typeadapt_keycomparator_it, cmp_key_object)           \
         == offsetof(typeof(*(adpcmp)), cmp_key_object)                 \
         && offsetof(typeadapt_keycomparator_it, cmp_object)            \
            == offsetof(typeof(*(adpcmp)), cmp_object),                 \
         "ensure same structure") ;                                     \
      if (0) {                                                          \
         int _err = (adpcmp)->cmp_key_object((typeadapter_t*)0, (const key_t*)0, (const object_t*)0) ;   \
         _err += (adpcmp)->cmp_object((typeadapter_t*)0, (const object_t*)0, (const object_t*)0) ;       \
         (void) _err ;                                                  \
      }                                                                 \
      (typeadapt_keycomparator_it*) (adpcmp) ;                          \
   }))

/* function: callcmpkeyobj_typeadaptkeycomparator
 * Implements <typeadapt_keycomparator_it.callcmpkeyobj_typeadaptkeycomparator>. */
#define callcmpkeyobj_typeadaptkeycomparator(adpcmp, typeadp, key, robject) \
   ((adpcmp)->cmp_key_object((typeadp), (key), (robject)))

/* function: callcmpobj_typeadaptkeycomparator
 * Implements <typeadapt_keycomparator_it.callcmpobj_typeadaptkeycomparator>. */
#define callcmpobj_typeadaptkeycomparator(adpcmp, typeadp, lobject, robject) \
   ((adpcmp)->cmp_object((typeadp), (lobject), (robject)))

#endif
