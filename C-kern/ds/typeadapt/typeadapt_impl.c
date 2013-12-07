/* title: Typeadapt-Implementation impl

   Implements <Typeadapt-Implementation>.

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

#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/ds/typeadapt.h"
#include "C-kern/api/ds/typeadapt/typeadapt_impl.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif


// section: typeadapt_impl_t

// group: implementation

int lifetime_newcopyobj_typeadaptimpl(typeadapt_impl_t * typeadp, /*out*/struct typeadapt_object_t ** destobject, const struct typeadapt_object_t * srcobject)
{
   int err ;
   memblock_t destblock = memblock_INIT_FREEABLE ;

   err = RESIZE_MM(typeadp->objectsize, &destblock) ;
   if (err) goto ONABORT ;

   memcpy(destblock.addr, srcobject, typeadp->objectsize) ;

   *destobject = (struct typeadapt_object_t*)destblock.addr ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

int lifetime_deleteobj_typeadaptimpl(typeadapt_impl_t * typeadp, struct typeadapt_object_t ** object)
{
   int err ;

   if (*object)  {
      memblock_t mblock = memblock_INIT(typeadp->objectsize, (uint8_t*)*object) ;

      *object = 0 ;

      err = FREE_MM(&mblock) ;
      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_ERRLOG(err) ;
   return err ;
}

// group: lifetime

int init_typeadaptimpl(/*out*/typeadapt_impl_t * typeadp, size_t objectsize)
{
   *typeadp = (typeadapt_impl_t) { typeadapt_INIT_LIFETIME(&lifetime_newcopyobj_typeadaptimpl, &lifetime_deleteobj_typeadaptimpl), objectsize } ;
   return 0 ;
}

int free_typeadaptimpl(typeadapt_impl_t * typeadp)
{
   *typeadp = (typeadapt_impl_t) typeadapt_impl_INIT_FREEABLE ;
   return 0 ;
}


// group: test

#ifdef KONFIG_UNITTEST

typedef struct testtype_t     testtype_t ;

struct testtype_t {
   unsigned a ;
   unsigned b ;
   unsigned c ;
} ;


static int test_initfree(void)
{
   typeadapt_t      emptytadp = typeadapt_INIT_FREEABLE ;
   typeadapt_t      initadp   = typeadapt_INIT_LIFETIME((typeof(emptytadp.lifetime.newcopy_object)) &lifetime_newcopyobj_typeadaptimpl, (typeof(emptytadp.lifetime.delete_object)) &lifetime_deleteobj_typeadaptimpl) ;
   typeadapt_impl_t typeadp   = typeadapt_impl_INIT_FREEABLE ;

   // TEST typeadapt_impl_INIT_FREEABLE
   TEST(isequal_typeadapt(&emptytadp, genericcast_typeadapt(&typeadp,typeadapt_impl_t,typeadapt_object_t,void*))) ;
   TEST(0 == typeadp.objectsize) ;

   // TEST init_typeadaptimpl, free_typeadaptimpl
   for (size_t i = 0; i < SIZE_MAX/2; i += SIZE_MAX/100) {
      TEST(0 == init_typeadaptimpl(&typeadp, i)) ;
      TEST(isequal_typeadapt(&initadp, genericcast_typeadapt(&typeadp,typeadapt_impl_t,typeadapt_object_t,void*))) ;
      TEST(i == typeadp.objectsize) ;
      TEST(0 == free_typeadaptimpl(&typeadp)) ;
      TEST(isequal_typeadapt(&emptytadp, genericcast_typeadapt(&typeadp,typeadapt_impl_t,typeadapt_object_t,void*))) ;
      TEST(0 == typeadp.objectsize) ;
      typeadp = (typeadapt_impl_t) typeadapt_impl_INIT(i) ;
      TEST(isequal_typeadapt(&initadp, genericcast_typeadapt(&typeadp,typeadapt_impl_t,typeadapt_object_t,void*))) ;
      TEST(i == typeadp.objectsize) ;
      TEST(0 == free_typeadaptimpl(&typeadp)) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_lifetime(void)
{
   typeadapt_impl_t  typeadp ;
   struct testtype_t value ;
   struct testtype_t * valuecopy[100] ;

   // prepare
   TEST(0 == init_typeadaptimpl(&typeadp, sizeof(testtype_t))) ;

   // TEST callnewcopy_typeadapt
   for (unsigned i = 0; i < lengthof(valuecopy); ++i) {
      value        = (testtype_t) { .a = i+1, .b = i+2, .c = i+3 } ;
      valuecopy[i] = 0 ;
      TEST(0 == callnewcopy_typeadapt(&typeadp, (struct typeadapt_object_t**)&valuecopy[i], (struct typeadapt_object_t*)&value)) ;
      TEST(0 != valuecopy[i]) ;
      TEST(i+1 == valuecopy[i]->a) ;
      TEST(i+2 == valuecopy[i]->b) ;
      TEST(i+3 == valuecopy[i]->c) ;
   }

   // TEST calldelete_typeadapt
   for (unsigned i = 0; i < lengthof(valuecopy); ++i) {
      TEST(i+1 == valuecopy[i]->a) ;
      TEST(i+2 == valuecopy[i]->b) ;
      TEST(i+3 == valuecopy[i]->c) ;
      TEST(0 == calldelete_typeadapt(&typeadp, (struct typeadapt_object_t**)&valuecopy[i])) ;
      TEST(0 == valuecopy[i]) ;
   }

   // unprepare
   TEST(0 == free_typeadaptimpl(&typeadp)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_ds_typeadapt_typeadaptimpl()
{
   if (test_initfree())       goto ONABORT ;
   if (test_lifetime())       goto ONABORT ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

#endif
