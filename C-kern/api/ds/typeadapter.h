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

/* typedef: struct typeadapter_oit
 * Export <typeadapter_oit> - object interface. */
typedef struct typeadapter_oit         typeadapter_oit ;

/* variable: g_typeadapter_functable
 * Interface <typeadapter_it> of default implementation <typeadapter_t>. */
const typeadapter_it                   g_typeadapter_functable ;


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
struct typeadapter_it {
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
} ;

// group: lifetime

/* define: typeadapter_it_INIT_FREEABLE
 * Static initializer. Sets all functions pointers of <typeadapter_it> to 0. */
#define typeadapter_it_INIT_FREEABLE   { 0, 0 }

/* define: typeadapter_it_INIT_FREEABLE
 * Static initializer. Sets all functions pointers to the provided values.
 *
 * Parameters:
 * copyobj_f  - Function pointer to copy object function. See <typeadapter_it.copyobj>.
 * freeobj_f  - Function pointer to free object function. See <typeadapter_it.freeobj>.
 * */
#define typeadapter_it_INIT(copyobj_f, freeobj_f) \
      { (copyobj_f), (freeobj_f) }

// group: set

/* function: setcopy_typeadapterit
 * Set copy function in a generic way (implemented as macro).
 * The function member <typeadapter_it.copyobj> is set to parameter *copyobj*.
 * Before setting copyobj is checked to have signature
 * > int (* copyobj) (typeadapter_t *, object_t **, object_t *) ;
 * If it has the correct signature its signature its casted to that of <typeadapter_it.copyobj>.
 *
 * Typeparameter:
 * Parameter *typeadapter_t* and *object_t* are type names like *testnode_t*. Do
 * not any value just the type of the specialized typeadapter and the name of the object type. */
void setcopy_typeadapterit(typeadapter_it * typeit, int (*copyobj)(void*), int typeadapter_t, int object_t) ;

/* function: setfree_typeadapterit
 * Set free function in a generic way (implemented as macro).
 * The function member <typeadapter_it.freeobj> is set to parameter *freeobj*.
 * Before setting freeobj is checked to have signature
 * > int (* freeobj) (typeadapter_t *, object_t *) ;
 * If it has the correct signature its signature its casted to that of <typeadapter_it.freeobj>.
 *
 * Typeparameter:
 * Parameter *typeadapter_t* and *object_t* are type names like *testnode_t*. Do
 * not any value just the type of the specialized typeadapter and the name of the object type. */
void setfree_typeadapterit(typeadapter_it * typeit, int (*freeobj)(void*), int typeadapter_t, int object_t) ;

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
#define typeadapter_it_DECLARE(declared_it, typeadapter_t, object_t)                                     \
   struct declared_it {                                                                                  \
      int (* copyobj) (typeadapter_t * typeimpl, /*out*/object_t ** copiedobject, object_t * object) ;   \
      int (* freeobj) (typeadapter_t * typeimpl, object_t * object) ;                                    \
   } ;


/* struct: typeadapter_oit
 * Allows a data structure to adapt to different object types.
 * If a data structure needs to make a copy of an object provided as parameter
 * or needs to free all contained objects at the of its life cycle it needs
 * access to functions which implements this functionality in a type specifiy way.
 *
 * This adapter provides a generic interface <typeadapter_it> to let data structures
 * adapt to different object types. */ __extension__
struct typeadapter_oit {
   union { struct {  // only there to make it compatible with <typeadapter_oit_DECLARE>.
   /* variable: object
    * The pointer to typeadapter's default implementation object <typeadapter_t>. */
   typeadapter_t        * object ;
   /* variable: functable
    * The pointer to typeadapter's interface <typeadapter_it>. */
   const typeadapter_it * functable ;
   } ; } ;
} ;

// group: lifetime

/* define: typeadapter_oit_INIT
 * Static initializer. Sets <typeadapter_oit.object> and <typeadapter_oit.functable>
 * to the values provided as arguments.
 * To initialize a <typeadapter_oit> with the standard implementation use
 * > static typeadapter_t   s_typeadapt     = typeadapter_INIT(sizeof(user_object_type_t)) ;
 * > static typeadapter_oit s_typeadapt_oit = typeadapter_oit_INIT(&s_typeadapt, functable_typeadapter()) ; */
#define typeadapter_oit_INIT(object, functable) \
      { { { (object), (functable) } } }

/* define: typeadapter_oit_INIT_DEFAULT
 * Static initializer. Sets <typeadapter_oit.object> to the pointer to <typeadapter_t> provided as argument
 * and <typeadapter_oit.functable> is set to <functable_typeadapter>.
 * Use this to initialize the default <typeadapter_oit> with its standard implementation <typeadapter_t>. */
#define typeadapter_oit_INIT_DEFAULT(typeadapter) \
      { { { (typeadapter), functable_typeadapter() } } }

/* define: typeadapter_oit_INIT_GENERIC
 * Static initializer. Sets <typeadapter_oit.object> to the pointer to <typeadapter_t> provided as argument
 * and <typeadapter_oit.functable> is set to <functable_typeadapter>.
 * Use this to initialize a subtyped <typeadapter_oit> declared with help of <typeadapter_oit_DECLARE>
 * for an interface declared with <typeadapter_it_DECLARE> to use the standard implementation <typeadapter_t>. */
#define typeadapter_oit_INIT_GENERIC(typeadapter) \
      { { .generic = { { { (typeadapter), functable_typeadapter() } } } } }

/* define: typeadapter_oit_INIT_FREEABLE
 * Static initializer. Sets all fields to 0. */
#define typeadapter_oit_INIT_FREEABLE  { { { 0, 0 } } }

// group: execute

/* function: execcopy_typeadapteroit
 * Calls function pointer <typeadapter_it.copyobj>.
 * The called function makes a copy of *object* and returns it in *copiedobject*.
 * ENOMEM is returned in case there is not enough memory for a copy of object. */
int execcopy_typeadapteroit(typeadapter_oit * typeadp, /*out*/struct generic_object_t ** copiedobject, struct generic_object_t  * object) ;

/* function: execfree_typeadapteroit
 * Calls <typeadapter_it.freeobj> function.
 * Frees memory and internal resources associated with object.
 * Even in case of an error it is tried to free all remaining resources
 * and the object is marked as freed after return nonetheless. */
int execfree_typeadapteroit(typeadapter_oit * typeadp, struct generic_object_t * object) ;

// group: generic

/* define: typeadapter_oit_DECLARE
 * Declares an object interface for accessing a typeadapter implementation.
 * The declared object interface is structural compatible with the generic interface <typeadapter_oit>.
 * See <typeadapter_oit> for a list of declared fields.
 *
 * Parameter:
 * declared_oit   - The name of the structure which is declared as the interface.
 *                  The name should have the suffix "_oit".
 * typeadapter_t  - The adapter type which implements all interface functions.
 *                  This should be the same value as given in <typeadapter_it_DECLARE>.
 * typeadapter_it - The interface type declared with <typeadapter_it_DECLARE>.
 * */
#define typeadapter_oit_DECLARE(declared_oit, typeadapter_t, typeadapter_it)  \
      __extension__ struct declared_oit {                                     \
         union {                                                              \
            struct {                                                          \
               typeadapter_t        * object ;                                \
               const typeadapter_it * functable ;                             \
            } ;                                                               \
            typeadapter_oit   generic ;                                       \
         } ;                                                                  \
      } ;


/* struct: typeadapter_t
 * Simple default implementation of <typeadapter_it> and <typeadapter_oit>.
 * <typeadapter_t> offers a default implementation accessible as <typeadapter_it>
 * which can be read by a call to <functable_typeadapter>.
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

/* function: functable_typeadapter
 * Returns the typeadapter interface <typeadapter_it>.
 * The returned interface is initialized with the
 * pointers to the private implementation functions of <typeadapter_t>. */
const typeadapter_it * functable_typeadapter(void) ;

/* function: asoit_typeadapter
 * Converts <typeadapter_t> into <typeadapter_oit>.
 * The data members of *typeoit* are set to (tadapt, <functable_typeadapter>).
 * The <typeadapter_t> is not changed but could not be const cause <typeadapter_oit>
 * expects a pointer to a non const <typeadapter_t>. */
void asoit_typeadapter(typeadapter_t * tadapt, /*out*/typeadapter_oit * typeoit) ;


// section: inline implementation

/* define: asoit_typeadapter
 * Implements <typeadapter_t.asoit_typeadapter>. */
#define asoit_typeadapter(tadapt, typeoit)                  \
      do {                                                  \
            typeadapter_oit * typeoit2 = (typeoit) ;        \
            typeoit2->object    = (tadapt) ;                \
            typeoit2->functable = functable_typeadapter() ; \
      } while(0)

/* define: execcopy_typeadapteroit
 * Implements <typeadapter_oit.execcopy_typeadapteroit>. */
#define execcopy_typeadapteroit(typeadp, copiedobject, _object) \
      ((typeadp)->functable->copyobj((typeadp)->object, copiedobject, _object))

/* define: execfree_typeadapteroit
 * Implements <typeadapter_oit.execfree_typeadapteroit>. */
#define execfree_typeadapteroit(typeadp, _object) \
      ((typeadp)->functable->freeobj((typeadp)->object, _object))

/* define: functable_typeadapter
 * Implements <typeadapter_t.functable_typeadapter>. */
#define functable_typeadapter()        (&g_typeadapter_functable)

/* define: setcopy_typeadapterit
 * Implements <typeadapter_it.setcopy_typeadapterit>. */
#define setcopy_typeadapterit(typeit, _copyobj, typeadapter_t, object_t)   \
   do {                                                                    \
      int (* _copyobj2) (typeadapter_t *, object_t **, object_t *) ;       \
      _copyobj2 = _copyobj ;                                               \
      (typeit)->copyobj = (typeof((typeit)->copyobj)) _copyobj2 ;          \
   } while(0)

/* define: setfree_typeadapterit
 * Implements <typeadapter_it.setfree_typeadapterit>. */
#define setfree_typeadapterit(typeit, _freeobj,typeadapter_t, object_t)    \
   do {                                                                    \
      int (* _freeobj2) (typeadapter_t *, object_t *) ;                    \
      _freeobj2 = _freeobj ;                                               \
      (typeit)->freeobj = (typeof((typeit)->freeobj)) _freeobj2 ;          \
   } while(0)


#endif
