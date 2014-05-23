/* title: Typeadapt-Lifetime

   Defines generic interface <typeadapt_lifetime_it>
   supporting lifetime functions for an object type that
   wants to be stored in a container.

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

   file: C-kern/api/ds/typeadapt/lifetime.h
    Header file <Typeadapt-Lifetime>.

   file: C-kern/ds/typeadapt/lifetime.c
    Implementation file <Typeadapt-Lifetime impl>.
*/
#ifndef CKERN_DS_TYPEADAPT_LIFETIME_HEADER
#define CKERN_DS_TYPEADAPT_LIFETIME_HEADER

// forward
struct typeadapt_t ;
struct typeadapt_object_t ;

/* typedef: struct typeadapt_lifetime_it
 * Export <typeadapt_lifetime_it> into global namespace. */
typedef struct typeadapt_lifetime_it            typeadapt_lifetime_it ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_ds_typeadapt_lifetime
 * Test <typeadapt_lifetime_it> functionality. */
int unittest_ds_typeadapt_lifetime(void) ;
#endif


/* struct: typeadapt_lifetime_it
 * Declares interface (function table) for managing the lifetime of objects.
 * If you change this interface do not forget to adapt
 * <typeadapt_lifetime_EMBED> and <genericcast_typeadaptlifetime>. */
struct typeadapt_lifetime_it {
   /* variable: newcopy_object
    * Function copies an object.
    * The called function makes a copy of *srcobject* and returns it in *destobject*.
    * Memory for the new object is allocated. ENOMEM is returned in case there is not
    * enough memory for a copy. */
   int              (* newcopy_object) (struct typeadapt_t * typeadp, /*out*/struct typeadapt_object_t ** destobject, const struct typeadapt_object_t * srcobject) ;
   /* variable: delete_object
    * Function frees memory and associated resources of object.
    * Even in case of an error it tries to free all remaining resources
    * and the object is marked as freed after return nonetheless.
    * Pointer *object* is set to NULL after return. */
   int              (* delete_object) (struct typeadapt_t * typeadp, struct typeadapt_object_t ** object) ;
} ;

// group: lifetime

/* define: typeadapt_lifetime_FREE
 * Static initializer. Sets all functions pointers of <typeadapt_lifetime_it> to 0. */
#define typeadapt_lifetime_FREE { 0, 0 }

/* define: typeadapt_lifetime_INIT
 * Static initializer. Sets all function pointers to the provided values.
 *
 * Parameters:
 * newcopyobj_f - Function pointer to copy object function. See <typeadapt_lifetime_it.newcopy_object>.
 * deleteobj_f  - Function pointer to free object function. See <typeadapt_lifetime_it.delete_object>. */
#define typeadapt_lifetime_INIT(newcopyobj_f, deleteobj_f)  { (newcopyobj_f), (deleteobj_f) }

// group: query

bool isequal_typeadaptlifetime(const typeadapt_lifetime_it * ladplife, const typeadapt_lifetime_it * radplife) ;

// group: call-service

/* function: callnewcopy_typeadaptlifetime
 * Calls function <typeadapt_lifetime_it.newcopy_object>.
 * The first parameter is of type <typeadapt_lifetime_it> the others are the same as in <typeadapt_lifetime_it.delete_object>.
 * This function is implemented as macro and supports types derived from <typeadapt_lifetime_it> - see <typeadapt_lifetime_DECLARE>. */
int callnewcopy_typeadaptlifetime(typeadapt_lifetime_it * adplife, struct typeadapt_t * typeadp, /*out*/struct typeadapt_object_t ** destobject, const struct typeadapt_object_t * srcobject) ;

/* function: calldelete_typeadaptlifetime
 * Calls function <typeadapt_lifetime_it.delete_object>.
 * The first parameter is of type <typeadapt_lifetime_it> the others are the same as in <typeadapt_lifetime_it.delete_object>.
 * This function is implemented as macro and supports types derived from <typeadapt_lifetime_it> - see <typeadapt_lifetime_DECLARE>. */
int calldelete_typeadaptlifetime(typeadapt_lifetime_it * adplife, struct typeadapt_t * typeadp, struct typeadapt_object_t ** object) ;

// group: generic

/* define: genericcast_typeadaptlifetime
 * Casts parameter adplife into pointer to <typeadapt_lifetime_it>.
 * The parameter *adplife* has to be of type "pointer to declared_it" where declared_it
 * is the name used as first parameter in <typeadapt_lifetime_DECLARE>.
 * The second and third parameter must be the same as in <typeadapt_lifetime_DECLARE>. */
typeadapt_lifetime_it * genericcast_typeadaptlifetime(void * adplife, TYPENAME typeadapter_t, TYPENAME object_t) ;

/* define: typeadapt_lifetime_DECLARE
 * Declares a derived interface from generic <typeadapt_lifetime_it>.
 * It is structural compatible with <typeadapt_lifetime_it>.
 * See <typeadapt_lifetime_it> for a list of contained functions.
 *
 * Parameter:
 * declared_it   - The name of the structure which is declared as the interface.
 *                 The name should have the suffix "_it".
 * typeadapter_t - The adapter type which implements all interface functions.
 *                 The first parameter in every function is a pointer to this type.
 * object_t      - The object type that <typeadapt_lifetime_it> supports. */
#define typeadapt_lifetime_DECLARE(declared_it, typeadapter_t, object_t)   \
   typedef struct declared_it             declared_it ;                    \
   struct declared_it {                                                    \
      typeadapt_lifetime_EMBED(typeadapter_t, object_t) ;                  \
   }

/* define: typeadapt_lifetime_EMBED
 * Declares the function table used to implement <typeadapt_lifetime_DECLARE>
 * Use <typeadapt_lifetime_EMBED> if you want to embed the function table directly into another structure.
 * See <typeadapt_lifetime_it> for a list of the declared functions.
 *
 * Parameter:
 * typeadapter_t - The adapter type which implements all interface functions.
 *                 The first parameter in every function is a pointer to this type.
 * object_t      - The object type that <typeadapt_lifetime_it> supports. */
#define typeadapt_lifetime_EMBED(typeadapter_t, object_t)      \
      int (* newcopy_object) (typeadapter_t * typeadp, /*out*/object_t ** destobject, const object_t * srcobject) ; \
      int (* delete_object)  (typeadapter_t * typeadp, object_t ** object)


// section: inline implementation

/* define: genericcast_typeadaptlifetime
 * Implements <typeadapt_lifetime_it.genericcast_typeadaptlifetime>. */
#define genericcast_typeadaptlifetime(adplife, typeadapter_t, object_t) \
   ( __extension__ ({                                                   \
      static_assert(                                                    \
         offsetof(typeadapt_lifetime_it, newcopy_object)                \
         == offsetof(typeof(*(adplife)), newcopy_object)                \
         && offsetof(typeadapt_lifetime_it, delete_object)              \
            == offsetof(typeof(*(adplife)), delete_object),             \
         "ensure same structure") ;                                     \
      if (0) {                                                          \
         int _err = (adplife)->newcopy_object((typeadapter_t*)0, (object_t**)0, (const object_t*)0) ; \
         _err += (adplife)->delete_object((typeadapter_t*)0, (object_t**)0) ;                         \
         (void) _err ;                                                  \
      }                                                                 \
      (typeadapt_lifetime_it*) (adplife) ;                              \
   }))

/* define: callnewcopy_typeadaptlifetime
 * Implements <typeadapt_lifetime_it.callnewcopy_typeadaptlifetime>. */
#define callnewcopy_typeadaptlifetime(adplife, typeadp, destobject, srcobject) \
   ((adplife)->newcopy_object((typeadp), (destobject), (srcobject)))

/* define: calldelete_typeadaptlifetime
 * Implements <typeadapt_lifetime_it.calldelete_typeadaptlifetime>. */
#define calldelete_typeadaptlifetime(adplife, typeadp, object) \
   ((adplife)->delete_object((typeadp), (object)))

#endif
