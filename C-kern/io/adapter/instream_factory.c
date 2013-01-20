/* title: InstreamFactory impl

   Implements <InstreamFactory>.

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

   file: C-kern/api/io/adapter/instream_factory.h
    Header file <InstreamFactory>.

   file: C-kern/io/adapter/instream_factory.c
    Implementation file <InstreamFactory impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/adapter/instream_factory.h"
#include "C-kern/api/io/adapter/instream_mmfile.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/io/filesystem/directory.h"
#include "C-kern/api/string/cstring.h"
#endif


// section: instream_factory_t

// group: query

size_t sizeimplobj_instreamfactory(instream_factory_impltype_e type)
{
   switch(type) {
   case instream_factory_impltype_MMFILE:
      return sizeof(instream_mmfile_t) ;
   }

   return 0 ;
}

// group: object-factory

int createimpl_instreamfactory(/*out*/instream_t * instr, instream_factory_impltype_e type, size_t sizeimplobj, uint8_t implobj[sizeimplobj], const char * filepath, const struct directory_t * relative_to)
{
   int err ;
   union {
      const instream_it        * instream ;
      const instream_mmfile_it * mmfile ;
   }   it = { 0 } ;

   switch(type) {
   case instream_factory_impltype_MMFILE:
      if (sizeimplobj < sizeof(instream_mmfile_t)) {
         err = EINVAL ;
         goto ONABORT ;
      }
      err = init_instreammmfile((instream_mmfile_t*)implobj, &it.mmfile, filepath, relative_to) ;
      if (err) goto ONABORT ;
      break ;
   default:
      err = EINVAL ;
      goto ONABORT ;
   }

   init_instream(instr, (instream_impl_t*)implobj, it.instream) ;

   return 0 ;
ONABORT:
   if (it.instream) {
      (void) destroyimpl_instreamfactory(0, type, sizeimplobj, implobj) ;
   }
   TRACEABORT_LOG(err) ;
   return err ;
}

int destroyimpl_instreamfactory(instream_t * instr, instream_factory_impltype_e type, size_t sizeimplobj, uint8_t implobj[sizeimplobj])
{
   int err ;
   int err2 ;

   err = free_instream(instr) ;

   switch(type) {
   case instream_factory_impltype_MMFILE:
      if (sizeimplobj < sizeof(instream_mmfile_t)) {
         err = EINVAL ;
         goto ONABORT ;
      }
      err2 = free_instreammmfile((instream_mmfile_t*)implobj) ;
      if (err2) err = err2 ;
      break ;
   default:
      err = EINVAL ;
      break ;
   }

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_query(void)
{
   size_t implsize[] = { [instream_factory_impltype_MMFILE] = sizeof(instream_mmfile_t) } ;

   // TEST sizeimplobj_instreamfactory
   for (unsigned i = 0; i < lengthof(implsize); ++i) {
      size_t expect = implsize[i] ;
      size_t value  = sizeimplobj_instreamfactory((instream_factory_impltype_e)i) ;
      TEST(expect == value) ;
   }

   // TEST sizeimplobj_instreamfactory: invalid value
   TEST(0 == sizeimplobj_instreamfactory((instream_factory_impltype_e)lengthof(implsize))) ;
   TEST(0 == sizeimplobj_instreamfactory((instream_factory_impltype_e)-1)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

/* function: isinit_implobj
 * Casts implobj to correct type and calls correct isinit function. */
static bool isinit_implobj(bool expect, instream_factory_impltype_e type, uint8_t * implobj)
{
   switch (type) {
   case instream_factory_impltype_MMFILE:
      return isinit_instreammmfile((instream_mmfile_t*)implobj) ;
   }

   return !expect ;
}

typedef typeof(((instream_it*)0)->readnext) readnextimpl_f ;

static int test_factory(void)
{
   readnextimpl_f readnextimpl[] = { [instream_factory_impltype_MMFILE] = (readnextimpl_f)&readnext_instreammmfile } ;
   uint8_t     implobj[200] ;
   instream_t  instr     = instream_INIT_FREEABLE ;
   directory_t * tempdir = 0 ;
   file_t      fd        = file_INIT_FREEABLE ;
   cstring_t   tmppath   = cstring_INIT ;

   // prepare
   TEST(0 == newtemp_directory(&tempdir, "inputstream", &tmppath)) ;
   TEST(0 == initcreate_file(&fd, "inputstream", tempdir)) ;
   TEST(0 == allocate_file(fd, 4096)) ;
   TEST(0 == free_file(&fd)) ;

   // TEST array readnextimpl contains all values of type instream_factory_impltype_e
   TEST(0 == sizeimplobj_instreamfactory((instream_factory_impltype_e)lengthof(readnextimpl))) ;

   for (instream_factory_impltype_e i = 0; i < lengthof(readnextimpl); ++i) {

      // TEST createimpl_instreamfactory
      TEST(sizeimplobj_instreamfactory(i) < sizeof(implobj)) ;
      memset(implobj, 0, sizeof(implobj)) ;
      TEST(0 == createimpl_instreamfactory(&instr, i, sizeimplobj_instreamfactory(i), implobj, "inputstream", tempdir)) ;
      TEST(instr.object == (instream_impl_t*)implobj) ;
      TEST(instr.iimpl  != 0) ;
      TEST(instr.iimpl->readnext == readnextimpl[i]) ;
      TEST(true == isinit_implobj(true, i, implobj)) ;

      // TEST destroyimpl_instreamfactory
      TEST(0 == destroyimpl_instreamfactory(&instr, i, sizeimplobj_instreamfactory(i), implobj)) ;
      TEST(0 == instr.object) ;
      TEST(0 == instr.iimpl) ;
      TEST(false == isinit_implobj(false, i, implobj)) ;
      TEST(0 == destroyimpl_instreamfactory(&instr, i, sizeimplobj_instreamfactory(i), implobj)) ;
   }

   // TEST createimpl_instreamfactory, destroyimpl_instreamfactory: EINVAL
   for (instream_factory_impltype_e i = 0; i < lengthof(readnextimpl); ++i) {
      TEST(EINVAL == createimpl_instreamfactory(&instr, i, sizeimplobj_instreamfactory(i)-1, implobj, "inputstream", tempdir)) ;
      TEST(EINVAL == destroyimpl_instreamfactory(&instr, i, sizeimplobj_instreamfactory(i)-1, implobj)) ;
   }

   // unprepare
   TEST(0 == removefile_directory(tempdir, "inputstream")) ;
   TEST(0 == delete_directory(&tempdir)) ;
   TEST(0 == removedirectory_directory(0, str_cstring(&tmppath))) ;
   TEST(0 == free_cstring(&tmppath)) ;

   return 0 ;
ONABORT:
   free_file(&fd) ;
   if (tempdir) {
      removefile_directory(tempdir, "inputstream") ;
      delete_directory(&tempdir) ;
      removedirectory_directory(0, str_cstring(&tmppath)) ;
      free_cstring(&tmppath) ;
   }
   return EINVAL ;
}

int unittest_io_adapter_instream_factory()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_query())      goto ONABORT ;
   if (test_factory())    goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
