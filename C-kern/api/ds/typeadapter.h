/* title: DataStructure-TypeAdapter

   Adapts object types to functionality needed by data structures.
   So they can be stored in lists or arrays.

   Rationale:
      If a type wants to be managed by a certain kind of inmemory container it
      has to inherit from the corresponding container's node type.
      And it must offer a type adapter which implements all functionality
      like copying or freeing or comparing a node.

      This ensures that the type is responsible for its own memory management.

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

   file: C-kern/api/ds/typeadapter.h
    Header file <DataStructure-TypeAdapter>.

   file: C-kern/ds/typeadapter.c
    Implementation file <DataStructure-TypeAdapter impl>.
*/
#ifndef CKERN_DS_TYPEADAPTER_HEADER
#define CKERN_DS_TYPEADAPTER_HEADER

// forward
struct generic_object_t ;

/* typedef: struct typeadapter_t
 * Exports <typeadapter_t> - default implementation. */
typedef struct typeadapter_t           typeadapter_t ;

/* typedef: struct typeadapter_it
 * Export <typeadapter_it> - typeadapter interface. */
typedef struct typeadapter_it          typeadapter_it ;

/* typedef: struct typeadapter_iot
 * Export <typeadapter_iot> - object and interface implementation. */
typedef struct typeadapter_iot         typeadapter_iot ;

/* variable: g_typeadapter_iimpl
 * Interface <typeadapter_it> of default implementation <typeadapter_t>. */
const typeadapter_it                   g_typeadapter_iimpl ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_ds_typeadapter
 * Unittest for <typeadapter_t>. */
int unittest_ds_typeadapter(void) ;
#endif


/* struct: typeadapter_it
 * The function table describing the interface to typeadapters.
 * If you change the interface of <typeadapter_it> to do not forget to adapt
 * <typeadapter_it_DECLARE> to the same signature. */
struct typeadapter_it { /*!*/ union { struct {  // makes it compatible with <typeadapter_it_DECLARE>.
   /* variable: copyobj
    * Pointer function copying an object.
    * The called function makes a copy of *object* and returns it in *copiedobject*.
    * ENOMEM is returned in case there is not enough memory for a copy of object. */
   int              (* copyobj) (typeadapter_t * typeimpl, /*out*/struct generic_object_t ** copiedobject, struct generic_object_t * object) ;
   /* variable: freeobj
    * Pointer to function freeing an object.
    * Frees memory and internal resources associated with object.
    * Even in case of an error it is tried to free all remaining resources
    * and the object is marked as freed after return nonetheless. */
   int              (* freeobj) (typeadapter_t * typeimpl, struct generic_object_t * object) ;
} ; }; } ;

// group: lifetime

/* define: typeadapter_it_INIT_FREEABLE
 * Static initializer. Sets all functions pointers of <typeadapter_it> to 0. */
#define typeadapter_it_INIT_FREEABLE               { { { 0, 0 } } }

/* define: typeadapter_it_INIT
 * Static initializer. Sets all functions pointers to the provided values.
 *
 * Parameters:
 * copyobj_f  - Function pointer to copy object function. See <typeadapter_it.copyobj>.
 * freeobj_f  - Function pointer to free object function. See <typeadapter_it.freeobj>.
 * */
#define typeadapter_it_INIT(copyobj_f, freeobj_f)  { { { (copyobj_f), (freeobj_f) } } }

// group: generic

/* define: typeadapter_it_DECLARE
 * Declares an interface function table for accessing a typeadapter implementation.
 * The declared interface is structural compatible with the generic interface <typeadapter_it>.
 * See <typeadapter_it> for a list of declared functions.
 *
 * Parameter:
 * declared_it   - The name of the structure which is declared as the interface.
 *                 The name should have the suffix "_it".
 * typeadapter_t - The adapter type which implements all interface functions.
 * object_t      - The object type for which typeadapter_t is implemented.
 * */
#define typeadapter_it_DECLARE(declared_it, typeadapter_t, object_t)          \
   __extension__ struct declared_it {                                         \
      union {                                                                 \
         struct {                                                             \
            int (* copyobj) (typeadapter_t * typeimpl, /*out*/object_t ** copiedobject, object_t * object) ;   \
            int (* freeobj) (typeadapter_t * typeimpl, object_t * object) ;   \
         } ;                                                                  \
         typeadapter_it generic ;                                             \
      } ;                                                                     \
   } ;


/* struct: typeadapter_iot
 * Allows a data structure to adapt to different object types.
 * If a data structure needs to make a copy of an object provided as parameter
 * or needs to free all contained objects at the of its life cycle it needs
 * access to functions which implements this functionality in a type specifiy way.
 *
 * This adapter provides a generic interface <typeadapter_it> to let data structures
 * adapt to different object types. */
__extension__ struct typeadapter_iot { /*!*/ union { struct {  // makes it compatible with <typeadapter_iot_DECLARE>
   /* variable: object
    * The pointer to typeadapter's default implementation object <typeadapter_t>. */
   typeadapter_t        * object ;
   /* variable: iimpl
    * The pointer to typeadapter's interface <typeadapter_it>. */
   const typeadapter_it * iimpl ;
} ; } ; } ;

// group: lifetime

/* define: typeadapter_iot_INIT
 * Static initializer. Sets <typeadapter_iot.object> and <typeadapter_iot.iimpl>
 * to the values provided as arguments.
 * To initialize a <typeadapter_iot> with the standard implementation use
 * > static typeadapter_t   s_typeadapt     = typeadapter_INIT(sizeof(user_object_type_t)) ;
 * > static typeadapter_iot s_typeadapt_iot = typeadapter_iot_INIT(&s_typeadapt, iimpl_typeadapter()) ; */
#define typeadapter_iot_INIT(object, iimpl) \
      { { { (object), (iimpl) } } }

/* define: typeadapter_iot_INIT_DEFAULT
 * Static initializer. Sets <typeadapter_iot.object> to the pointer to <typeadapter_t> provided as argument
 * and <typeadapter_iot.iimpl> is set to <iimpl_typeadapter>.
 * Use this to initialize the default <typeadapter_iot> with its standard implementation <typeadapter_t>. */
#define typeadapter_iot_INIT_DEFAULT(typeadapter) \
      { { { (typeadapter), iimpl_typeadapter() } } }

/* define: typeadapter_iot_INIT_GENERIC
 * Static initializer. Sets <typeadapter_iot.object> to the pointer to <typeadapter_t> provided as argument
 * and <typeadapter_iot.iimpl> is set to <iimpl_typeadapter>.
 * Use this to initialize a subtyped <typeadapter_iot> declared with help of <typeadapter_iot_DECLARE>
 * for an interface declared with <typeadapter_it_DECLARE> to use the standard implementation <typeadapter_t>. */
#define typeadapter_iot_INIT_GENERIC(typeadapter) \
      { { .generic = { { { (typeadapter), iimpl_typeadapter() } } } } }

/* define: typeadapter_iot_INIT_FREEABLE
 * Static initializer. Sets all fields to 0. */
#define typeadapter_iot_INIT_FREEABLE  { { { 0, 0 } } }

// group: execute

/* function: execcopy_typeadapteriot
 * Calls function pointer <typeadapter_it.copyobj>.
 * The called function makes a copy of *object* and returns it in *copiedobject*.
 * ENOMEM is returned in case there is not enough memory for a copy of object. */
int execcopy_typeadapteriot(typeadapter_iot * typeadp, /*out*/struct generic_object_t ** copiedobject, struct generic_object_t  * object) ;

/* function: execfree_typeadapteriot
 * Calls <typeadapter_it.freeobj> function.
 * Frees memory and internal resources associated with object.
 * Even in case of an error it is tried to free all remaining resources
 * and the object is marked as freed after return nonetheless. */
int execfree_typeadapteriot(typeadapter_iot * typeadp, struct generic_object_t * object) ;

// group: generic

/* define: typeadapter_iot_DECLARE
 * Declares an object interface for accessing a typeadapter implementation.
 * The declared object interface is structural compatible with the generic interface <typeadapter_iot>.
 * See <typeadapter_iot> for a list of declared fields.
 *
 * Parameter:
 * declared_iot   - The name of the structure which is declared as the interface.
 *                  The name should have the suffix "_iot".
 * typeadapter_t  - The adapter type which implements all interface functions.
 *                  This should be the same value as given in <typeadapter_it_DECLARE>.
 * typeadapter_it - The interface type declared with <typeadapter_it_DECLARE>.
 * */
#define typeadapter_iot_DECLARE(declared_iot, typeadapter_t, typeadapter_it)  \
      __extension__ struct declared_iot {                                     \
         union {                                                              \
            struct {                                                          \
               typeadapter_t        * object ;                                \
               const typeadapter_it * iimpl ;                                 \
            } ;                                                               \
            typeadapter_iot   generic ;                                       \
         } ;                                                                  \
      } ;


/* struct: typeadapter_t
 * Simple default implementation of <typeadapter_it> and <typeadapter_iot>.
 * <typeadapter_t> offers a default implementation accessible as <typeadapter_it>
 * which can be read by a call to <iimpl_typeadapter>.
 *
 * Assumptions:
 * 1. This implementation assumes that objects can be copied with a simple
 *    call to memcpy and no special deep copy semantic is needed.
 * 2. The second one is that objects always have the same size.
 *
 * Both assumptions are too simple for supporting complex object types.
 * But for complex objects it is considered OK to implement their own
 * specialized *object_typeadapter_t*. */
struct typeadapter_t {
   /* variable: objectsize
    * The fixed size of supported objects. */
   size_t   objectsize ;
} ;

// group: lifetime

/* define: typeadapter_INIT_FREEABLE
 * Static initializer. Sets all fields to 0. */
#define typeadapter_INIT_FREEABLE      { 0 }

/* define: typeadapter_INIT
 * Static initializer. Sets data member objectsize to value of supplied parameter. */
#define typeadapter_INIT(objectsize)   { (objectsize) }

/* function: init_typeadapter
 * Initializes <typeadapter_t>.
 *
 * Parameter:
 * objectsize - The fixed size of allocated objects.
 *
 * Example:
 * > struct objtype_t {
 * >    uint8_t            objectname[32] ;
 * >    struct objtype_t * next ;
 * >    arraysf_node_t     objectid ;
 * >    arraystf_node_t    objectnamekey ;
 * > } ;
 *
 * The next pointer in objtype_t is used to link objects together.
 * You can use <slist_t> to manage objtype_t in a single linked list
 * cause it manages pointers to the beginning of the objects no matter where the
 * next pointer is located within the object.
 *
 * The objectid in objtype_t is used to index objects by their id.
 * TODO: make arraysf_t and arraystf_t use pointers to objtype_t and offsets !!
 *
 * */
int init_typeadapter(/*out*/typeadapter_t * tadapt, size_t objectsize) ;

/* function: free_typeadapter
 * Frees <typeadapter_t> - sets all fields to 0. */
int free_typeadapter(typeadapter_t * tadapt) ;

// group: query

/* function: iimpl_typeadapter
 * Returns the typeadapter interface <typeadapter_it>.
 * The returned interface is initialized with the
 * pointers to the private implementation functions of <typeadapter_t>. */
const typeadapter_it * iimpl_typeadapter(void) ;

/* function: asiot_typeadapter
 * Converts <typeadapter_t> into <typeadapter_iot>.
 * The data members of *typeiot* are set to (tadapt, <iimpl_typeadapter>).
 * The <typeadapter_t> is not changed but could not be const cause <typeadapter_iot>
 * expects a pointer to a non const <typeadapter_t>. */
void asiot_typeadapter(typeadapter_t * tadapt, /*out*/typeadapter_iot * typeiot) ;


// section: inline implementation

/* define: asiot_typeadapter
 * Implements <typeadapter_t.asiot_typeadapter>. */
#define asiot_typeadapter(tadapt, typeiot)            \
      do {                                            \
            typeadapter_iot * typeiot2 = (typeiot) ;  \
            typeiot2->object = (tadapt) ;             \
            typeiot2->iimpl  = iimpl_typeadapter() ;  \
      } while(0)

/* define: execcopy_typeadapteriot
 * Implements <typeadapter_iot.execcopy_typeadapteriot>. */
#define execcopy_typeadapteriot(typeadp, copiedobject, _object) \
      ((typeadp)->iimpl->copyobj((typeadp)->object, copiedobject, _object))

/* define: execfree_typeadapteriot
 * Implements <typeadapter_iot.execfree_typeadapteriot>. */
#define execfree_typeadapteriot(typeadp, _object) \
      ((typeadp)->iimpl->freeobj((typeadp)->object, _object))

/* define: iimpl_typeadapter
 * Implements <typeadapter_t.iimpl_typeadapter>. */
#define iimpl_typeadapter()            (&g_typeadapter_iimpl)


#endif
