/* title: Typeadapt-Implementation

   Implements <typeadapt_t> lifetime services which can be
   used for simple types.

   Pecondition:
   1. - Include "C-kern/api/ds/typeadapt.h"

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

   file: C-kern/api/ds/typeadapt/typeadapt_impl.h
    Header file <Typeadapt-Implementation>.

   file: C-kern/ds/typeadapt/typeadapt_impl.c
    Implementation file <Typeadapt-Implementation impl>.
*/
#ifndef CKERN_DS_TYPEADAPT_IMPL_HEADER
#define CKERN_DS_TYPEADAPT_IMPL_HEADER

/* typedef: struct typeadapt_impl_t
 * Export <typeadapt_impl_t> into global namespace. */
typedef struct typeadapt_impl_t           typeadapt_impl_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_ds_typeadapt_typeadapt_impl
 * Test <typeadapt_impl_t> functionality. */
int unittest_ds_typeadapt_impl(void) ;
#endif


/* struct: typeadapt_impl_t
 * Default implementation of <typeadapt_t>.
 *
 * Assumptions:
 * 1. This implementation assumes that objects can be copied with a simple
 *    call to memcpy and no special deep copy semantic is needed.
 * 2. The second one is that objects always have the same size.
 *
 * Both assumptions are too simple for supporting complex object types.
 * But for complex objects it is considered OK to implement their own
 * <typeadapt_t>*. */
struct typeadapt_impl_t {
   struct {
      typeadapt_EMBED(typeadapt_impl_t, typeadapt_object_t, void*) ;
   } ;
   /* variable: objectsize
    * The size of supported structure. */
   size_t   objectsize ;
} ;

// group: lifetime

/* define: typeadapt_impl_INIT_FREEABLE
 * Static initializer. */
#define typeadapt_impl_INIT_FREEABLE         { typeadapt_INIT_FREEABLE, 0 }

/* define: typeadapt_impl_INIT
 * Static initializer. */
#define typeadapt_impl_INIT(objectsize)      { typeadapt_INIT_LIFETIME(&lifetime_newcopyobj_typeadaptimpl, &lifetime_deleteobj_typeadaptimpl), objectsize }

/* function: init_typeadaptimpl
 * Initializes implementation to support objectes of size *objectsize*. */
int init_typeadaptimpl(/*out*/typeadapt_impl_t * typeadp, size_t objectsize) ;

/* function: free_typeadaptimpl
 * Sets all fields to 0. No additional resources are held. Memory of objects
 * which are not freed is kept intact. */
int free_typeadaptimpl(typeadapt_impl_t * typeadp) ;

// group: lifetime-service-implementation

/* function: lifetime_newcopyobj_typeadaptimpl
 * Implements <typeadapt_lifetime_it.newcopy_object>. */
int lifetime_newcopyobj_typeadaptimpl(typeadapt_impl_t * typeadp, /*out*/struct typeadapt_object_t ** destobject, const struct typeadapt_object_t * srcobject) ;

/* function: lifetime_deleteobj_typeadaptimpl
 * Implements <typeadapt_lifetime_it.delete_object>. */
int lifetime_deleteobj_typeadaptimpl(typeadapt_impl_t * typeadp, struct typeadapt_object_t ** object) ;

#endif
