/* title: Typeadapt-Nodeoffset

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

   file: C-kern/api/ds/typeadapt/nodeoffset.h
    Header file <Typeadapt-Nodeoffset>.

   file: C-kern/ds/typeadapt/nodeoffset.c
    Implementation file <Typeadapt-Nodeoffset impl>.
*/
#ifndef CKERN_DS_TYPEADAPT_NODEOFFSET_HEADER
#define CKERN_DS_TYPEADAPT_NODEOFFSET_HEADER

// forward
struct typeadapt_object_t ;

/* typedef: struct typeadapt_nodeoffset_t
 * Export <typeadapt_nodeoffset_t> into global namespace. */
typedef uint16_t           typeadapt_nodeoffset_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_ds_typeadapt_nodeoffset
 * Test <typeadapt_nodeoffset_t> functionality. */
int unittest_ds_typeadapt_nodeoffset(void) ;
#endif


/* struct: typeadapt_nodeoffset_t
 * Describes an offset to a struct member.
 * Is is a positive value interpreted as a byte offset.
 * The offset is computed between the address in memory of the object member which
 * contains container specific data and the start address of the object in memory.
 *
 * This member or data field of the object is called node. Cause any container managing objects
 * stores nodes (list nodes, tree nodes ...). The offset is always positive cause every object
 * laid out in memory extends from lower address numbers to higher ones. The start address of an object
 * is always its lowest address number. */

// group: lifetime

/* define: typeadapt_nodeoffset_INIT
 * Static initializer. Use offsetof to calculate the parameter. */
#define typeadapt_nodeoffset_INIT(nodeoffset)      nodeoffset

/* function: init_typeadaptnodeoffset
 * Inits <typeadapt_nodeoffset_t> structure with offset to struct member.
 * Use offsetof to calculate the parameter. */
void init_typeadaptnodeoffset(/*out*/typeadapt_nodeoffset_t * nodeoff, uint16_t nodeoffset) ;

// group: query

/* function: isequal_typeadaptnodeoffset
 * Returns true if both <typeadapt_nodeoffset_t>s are equal. */
bool isequal_typeadaptnodeoffset(const typeadapt_nodeoffset_t lnodeoff, const typeadapt_nodeoffset_t rnodeoff) ;

// group: conversion

/* function: memberasobject_typeadaptnodeoffset
 * Converts a pointer to a struct member to the object which contains it.
 * The member is usually the data type that is used in containers to store their management information.
 * See also <typeadapt_object_t>. */
struct typeadapt_object_t * memberasobject_typeadaptnodeoffset(const typeadapt_nodeoffset_t nodeoff, void * node) ;

/* function: objectasmember_typeadaptnodeoffset
 * Converts object pointer to pointer to struct member.
 * It is the reverse operation of <memberasobject_typeadaptnodeoffset>. */
void * objectasmember_typeadaptnodeoffset(const typeadapt_nodeoffset_t nodeoff, struct typeadapt_object_t * object) ;


// section: inline implementation

/* define: memberasobject_typeadaptnodeoffset
 * Implements <typeadapt_nodeoffset_t.memberasobject_typeadaptnodeoffset>. */
#define memberasobject_typeadaptnodeoffset(nodeoff, node)                 \
         ( __extension__ ({                                             \
            uint32_t _off = (nodeoff)  ;                                \
            (struct typeadapt_object_t*) ((uintptr_t)(node) - _off) ;   \
         }))

/* define: objectasmember_typeadaptnodeoffset
 * Implements <typeadapt_nodeoffset_t.objectasmember_typeadaptnodeoffset>. */
#define objectasmember_typeadaptnodeoffset(nodeoff, object)               \
         ( __extension__ ({                                             \
            uint32_t _off = (nodeoff)  ;                                \
            (void*) ((uintptr_t)(object) + _off) ;                      \
         }))

/* define: init_typeadaptnodeoffset
 * Implements <typeadapt_nodeoffset_t.init_typeadaptnodeoffset>. */
#define init_typeadaptnodeoffset(nodeoff, nodeoffset) \
         ((void) (*(nodeoff) = nodeoffset))

/* define: isequal_typeadaptnodeoffset
 * Implements <typeadapt_nodeoffset_t.isequal_typeadaptnodeoffset>. */
#define isequal_typeadaptnodeoffset(lnodeoff, rnodeoff)    \
         ( __extension__ ({                              \
            typeadapt_nodeoffset_t _loff = (lnodeoff) ;  \
            typeadapt_nodeoffset_t _roff = (rnodeoff) ;  \
            _loff == _roff ;                             \
         }))

#endif
