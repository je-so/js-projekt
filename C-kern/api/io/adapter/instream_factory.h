/* title: InstreamFactory

   Offers building of objects which implement interface
   <instream_it> and returns them as type <instream_t>.

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
#ifndef CKERN_IO_ADAPTER_INSTREAM_FACTORY_HEADER
#define CKERN_IO_ADAPTER_INSTREAM_FACTORY_HEADER

// forward
struct directory_t ;
struct instream_t ;

/* enums: instream_factory_impltype_e
 *
 * instream_factory_impltype_MMFILE - Implement <instream_it> with help of <mmfile_t>. See <instream_mmfile_t>.
 *
 * */
enum instream_factory_impltype_e {
   instream_factory_impltype_MMFILE
} ;

typedef enum instream_factory_impltype_e     instream_factory_impltype_e ;


/* enums: instream_factory_config_e
 *
 * instream_factory_config_DEFAULT      - Implementation can split reading into multiple buffers
 *                                        and is allowed to remove old buffers from memory.
 * instream_factory_config_SINGLEBUFFER - All data from the stream is read into a single continous buffer. */
enum instream_factory_config_e {
   instream_factory_config_DEFAULT,
   instream_factory_config_SINGLEBUFFER
} ;

typedef enum instream_factory_config_e       instream_factory_config_e ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_adapter_instream_factory
 * Test <instream_factory_t> functionality. */
int unittest_io_adapter_instream_factory(void) ;
#endif


/* struct: instream_factory_t
 * Singleton object must never be allocated or freed. */
struct instream_factory_t ;

// group: query

/* function: sizeimplobj_instreamfactory
 * Returns the size of the implementation object of a certain type. */
size_t sizeimplobj_instreamfactory(instream_factory_impltype_e type) ;

// group: object-factory

/* function: createimpl_instreamfactory
 * Inits <instream_t> and its implementing object.
 * The implementation object is stored in the memory array implobj.
 * The value of parameter implsize must be equal to the value returned from <sizeimplobj_instreamfactory>.
 * The type of the implementation object is determined by parameter type. */
int createimpl_instreamfactory(/*out*/struct instream_t * instr, instream_factory_config_e config, instream_factory_impltype_e type, size_t sizeimplobj, void * implobj/*[sizeimplobj]*/, const char * filepath, const struct directory_t * relative_to/*0 => current working dir*/) ;

/* function: destroyimpl_instreamfactory
 * Frees <instream_t> and its implementing object. */
int destroyimpl_instreamfactory(struct instream_t * instr, instream_factory_impltype_e type, size_t sizeimplobj, void * implobj/*[sizeimplobj]*/) ;

#endif
