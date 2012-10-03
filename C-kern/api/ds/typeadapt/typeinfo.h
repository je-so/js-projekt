/* title: Typeadapt-Typeinfo

   Describes abstract object type <typeadapt_object_t>.
   This information is needed to adapt any type of object to the specific
   needs of a container of the data store.

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

   file: C-kern/api/ds/typeadapt/typeinfo.h
    Header file <Typeadapt-Typeinfo>.

   file: C-kern/ds/typeadapt/typeinfo.c
    Implementation file <Typeadapt-Typeinfo impl>.
*/
#ifndef CKERN_DS_TYPEADAPT_TYPEINFO_HEADER
#define CKERN_DS_TYPEADAPT_TYPEINFO_HEADER

// forward
struct typeadapt_object_t ;

/* typedef: struct typeadapt_typeinfo_t
 * Export <typeadapt_typeinfo_t> into global namespace. */
typedef struct typeadapt_typeinfo_t          typeadapt_typeinfo_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_ds_typeadapt_typeinfo
 * Test <typeadapt_typeinfo_t> functionality. */
int unittest_ds_typeadapt_typeinfo(void) ;
#endif


/* struct: typeadapt_typeinfo_t
 * Describes a*/
struct typeadapt_typeinfo_t {
   /* variable: memberoffset
    * Positive value which is a byte offset.
    * The offset is computed between the address in memory of the object field which
    * contains container specific data and the start address of the object in memory.
    *
    * This member or data field of the object is called node. Cause container where objects are stored
    * manage nodes (list nodes, tree nodes ...). The offset is always positive cause every object
    * laid out in memory extends from lower address numbers to higher ones. The start address of an object
    * is always its lowest address number. */
   uint32_t    memberoffset ;
} ;

// group: lifetime

/* define: typeadapt_typeinfo_INIT
 * Static initializer. Use offsetof to calculate the parameter. */
#define typeadapt_typeinfo_INIT(memberoffset)      { (memberoffset) }

/* function: init_typeadapttypeinfo
 * Inits <typeadapt_typeinfo_t> structure with offset to struct member.
 * Use offsetof to calculate the parameter. */
void init_typeadapttypeinfo(/*out*/typeadapt_typeinfo_t * tinfo, uint32_t memberoffset) ;


// group: object↔node

/* function: memberasobject_typeadapttypeinfo
 * Converts a pointer to a struct member to the object which contains it.
 * The member is usually the data type that is used in containers to store their management information.
 * See also <typeadapt_object_t>. */
struct typeadapt_object_t * memberasobject_typeadapttypeinfo(const typeadapt_typeinfo_t tinfo, void * node) ;

/* function: objectasmember_typeadapttypeinfo
 * Converts object pointer to pointer to struct member.
 * It is the reverse operation of <memberasobject_typeadapttypeinfo>. */
void * objectasmember_typeadapttypeinfo(const typeadapt_typeinfo_t tinfo, struct typeadapt_object_t * object) ;


// section: inline implementation

/* define: memberasobject_typeadapttypeinfo
 * Implements <typeadapt_storedobject_t.memberasobject_typeadapttypeinfo>. */
#define memberasobject_typeadapttypeinfo(tinfo, node)    ((struct typeadapt_object_t*) (((uintptr_t)(node)) - (tinfo.memberoffset)))

/* define: objectasmember_typeadapttypeinfo
 * Implements <typeadapt_storedobject_t.objectasmember_typeadapttypeinfo>. */
#define objectasmember_typeadapttypeinfo(tinfo, object)  ((void*) (((uintptr_t)(object)) + (tinfo.memberoffset)))

/* define: init_typeadapttypeinfo
 * Implements <typeadapt_storedobject_t.init_typeadapttypeinfo>. */
#define init_typeadapttypeinfo(tinfo, _memberoffset)     ((void) ((tinfo)->memberoffset = _memberoffset))

#endif
