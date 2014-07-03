/* title: SingleLinkedList
   Manages a circularly linked list of objects.
   > ---------     ---------             ---------
   > | First |     | Node2 |             | Last  |
   > ---------     ---------             ---------
   > | *next | --> | *next | --> ...-->  | *next |--┐
   > ---------     ---------             ---------  |
   >    ^-------------------------------------------┘

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
   (C) 2011 Jörg Seebohn

   file: C-kern/api/ds/inmem/slist.h
    Header file of <SingleLinkedList>.

   file: C-kern/ds/inmem/slist.c
    Implementation file of <SingleLinkedList impl>.
*/
#ifndef CKERN_DS_INMEM_SLIST_HEADER
#define CKERN_DS_INMEM_SLIST_HEADER

#include "C-kern/api/ds/inmem/node/slist_node.h"

// forward
struct typeadapt_t ;

/* typedef: struct slist_t
 * Export <slist_t>. */
typedef struct slist_t                 slist_t ;

/* typedef: struct slist_iterator_t
 * Export <slist_iterator_t>. */
typedef struct slist_iterator_t        slist_iterator_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_ds_inmem_slist
 * Test <slist_t> functionality. */
int unittest_ds_inmem_slist(void) ;
#endif


/* struct: slist_iterator_t
 * Iterates over elements contained in <slist_t>.
 * The iterator supports removing or deleting of the current node.
 *
 * Example:
 * > slist_t list ;
 * > fill_list(&list) ;
 * > slist_node_t * prev = last_slist(&list) ;
 * > foreach (_slist, node, &list) {
 * >    if (need_to_remove(node)) {
 * >       removeafter_slist(&list, prev, node) ;
 * >       delete_node(node) ;
 * >    } else {
 * >       prev = node ;
 * >    }
 * > }
 * */
struct slist_iterator_t {
   struct slist_node_t * next ;
   slist_t             * list ;
} ;

// group: lifetime

/* define: slist_iterator_FREE
 * Static initializer. */
#define slist_iterator_FREE { 0, 0 }

/* function: initfirst_slistiterator
 * Initializes an iterator for <slist_t>. */
int initfirst_slistiterator(/*out*/slist_iterator_t * iter, slist_t * list) ;

/* function: free_slistiterator
 * Frees an iterator of <slist_t>. This is a no-op. */
int free_slistiterator(slist_iterator_t * iter) ;

// group: iterate

/* function: next_slistiterator
 * Returns next iterated node.
 * The first call after <initfirst_slistiterator> returns the first list element
 * if it is not empty.
 *
 * Returns:
 * true  - node contains a pointer to the next valid node in the list.
 * false - There is no next node. The last element was already returned or the list is empty. */
bool next_slistiterator(slist_iterator_t * iter, /*out*/struct slist_node_t ** node) ;


/* struct: slist_t
 * Points to last object in a list of objects.
 * Every object points to its successor. The list is organized as a ring.
 * So the last object points to "its successor" which is the first object or head of list.
 *
 * To search for an element in the list needs time O(n).
 * Adding and removing needs O(1).
 *
 * The list stores nodes of type <slist_node_t>. To manage objects of arbitrary type
 * add a struct member of this type and cast the pointers from <slist_node_t> to your object-type
 * with help of offsetof(object-type, slist_node_name) or use the macro <slist_IMPLEMENT> which
 * generates small wrappers for every single slist function which do this conversion automatically.
 *
 * Example:
 * > typedef struct object_t object_t ;
 * > typedef struct objectadapt_t objectadapt_t ;
 * > struct object_t {
 * > ... ; slist_node_t slnode ; ...
 * > } ;
 * > struct objectadapt_t {
 * >    typeadapt_EMBED(objectadapt_t, object_t, void) ; // void: keytype not used in list
 * > } ;
 * > slist_IMPLEMENT(_mylist, object_t, slnode.next) ;
 * >
 * >
 * > void function() {
 * >  slist_t mylist ;
 * >  init_mylist(&mylist) ;
 * >  object_t * new_object = ... ;
 * >  new_object->slnode = slist_node_INIT ;
 * >
 * >  // instead of
 * >  err = insertfirst_slist(&mylist, &new_object->slnode)
 * >  // you can now do
 * >  err = insertfirst_mylist(&mylist, new_object) ;
 * >
 * >  // instead of
 * >  object_t * first = (object_t*)((uintptr_t)first_slist(&mylist) - offsetof(object_t,slnode)) ;
 * >  // you can now do
 * >  object_t * first = first_mylist(&mylist) ;
 * >
 * > // free objects automatically
 * > objectadapt_t      typeadapt = typeadapt_INIT_LIFETIME(0, &delete_object_function) ;
 * > err = free_mylist(&mylist, genericcast_typeadapt(&typeadapt, object_t, void)) ;
 * > }
 *
 * */
struct slist_t {
   /* variable: last
    * Points to last element (tail) of list. */
   struct slist_node_t  * last ;
} ;

// group: lifetime

/* define: slist_INIT
 * Static initializer. You can use it instead of <init_slist>.
 * After assigning you can call <free_slist> or any other function safely. */
#define slist_INIT { (void*)0 }

/* constructor: init_slist
 * Initializes a single linked list object.
 * The caller has to provide an unitialized list object. This function never fails. */
void init_slist(/*out*/slist_t * list) ;

/* constructor: initsingle_slist
 * Initializes a single linked list object containing a single node. */
void initsingle_slist(/*out*/slist_t * list, slist_node_t * node) ;

/* destructor: free_slist
 * Frees memory of all contained objects.
 * Set nodeadp to 0 if you do not want to call a free memory on every node. */
int free_slist(slist_t * list, uint16_t nodeoffset, struct typeadapt_t * typeadp/*0=>no free called*/) ;

// group: query

/* function: isempty_slist
 * Returns true if list contains no nodes. */
int isempty_slist(const slist_t * list) ;

/* function: first_slist
 * Returns the first element in the list.
 * Returns NULL in case list contains no elements. */
struct slist_node_t * first_slist(const slist_t * list) ;

/* function: last_slist
 * Returns the last node in the list.
 * Returns NULL in case list contains no elements. */
struct slist_node_t * last_slist(const slist_t * list) ;

/* function: next_slist
 * Returns the node coming after this node.
 * If node is the last node in the list the first is returned instead. */
struct slist_node_t * next_slist(struct slist_node_t * node) ;

/* function: isinlist_slist
 * Returns true if node is stored in a list else false. */
bool isinlist_slist(struct slist_node_t * node) ;

// group: foreach-support

/* typedef: iteratortype_slist
 * Declaration to associate <slist_iterator_t> with <slist_t>. */
typedef slist_iterator_t      iteratortype_slist ;

/* typedef: iteratedtype_slist
 * Declaration to associate <slist_node_t> with <slist_t>. */
typedef struct slist_node_t * iteratedtype_slist ;

// group: change

/* function: insertfirst_slist
 * Makes new_node the new first element of list.
 * Ownership is transfered from caller to <slist_t>.
 *
 * Note:
 * Make sure that new_node is not part of a list ! */
void insertfirst_slist(slist_t * list, struct slist_node_t * new_node) ;

/* function: insertlast_slist
 * Makes new_node the new last element of list.
 * Ownership is transfered from caller to <slist_t>.
 *
 * Note:
 * Make sure that new_node is not part of a list ! */
void insertlast_slist(slist_t * list, struct slist_node_t * new_node) ;

/* function: insertafter_slist
 * Adds new_node after prev_node into list.
 * Ownership is transfered from caller to <slist_t>.
 *
 * Note:
 * Make sure that new_node is not part of a list
 * and that prev_node is part of the list ! */
void insertafter_slist(slist_t * list, struct slist_node_t * prev_node, struct slist_node_t * new_node) ;

/* function: removefirst_slist
 * Removes the first element from list.
 * If list contains no elements EINVAL is returned. If list contains elements removed_node points
 * to the removed first elementa and 0 is returned. Ownership of removed_node is transfered back to caller. */
int removefirst_slist(slist_t * list, struct slist_node_t ** removed_node) ;

/* function: removeafter_slist
 * Removes the next node coming after prev_node from the list.
 * If the list contains no elements EINVAL is returned.
 * Ownership of removed_node is transfered back to caller.
 *
 * Note:
 * Make sure that prev_node is part of the list else behaviour is undefined ! */
int removeafter_slist(slist_t * list, struct slist_node_t * prev_node, struct slist_node_t ** removed_node) ;

// group: change set

/* function: removeall_slist
 * Removes all nodes from the list.
 * For every removed node <typeadapt_lifetime_it.delete_object> is called.
 * Set nodeadp to 0 if you do not want to call a free memory on every node. */
int removeall_slist(slist_t * list, uint16_t nodeoffset, struct typeadapt_t * typeadp/*0=>no free called*/) ;

// group: generic

/* function: genericcast_slist
 * Casts list into <slist_t> if that is possible.
 * The generic object list must have a last pointer
 * as first member. */
slist_t * genericcast_slist(void * list) ;

/* define: slist_IMPLEMENT
 * Generates interface of single linked list storing elements of type object_t.
 *
 * Parameter:
 * _fsuffix     - It is the suffix of the generated list interface functions, e.g. "init##_fsuffix".
 * object_t     - The type of object which can be stored and retrieved from this list.
 *                The object must contain a field of type <slist_node_t> or use <slist_node_EMBED>.
 * name_nextptr - The name (access path) of the next pointer in object_t.
 *                Use either structmembername.next or the name used in macro <slist_node_EMBED>.
 * */
void slist_IMPLEMENT(IDNAME _fsuffix, TYPENAME object_t, IDNAME name_nextptr) ;


// section: inline implementation

// group: slist_iterator_t

/* define: free_slistiterator
 * Implements <slist_t.free_slistiterator>. */
#define free_slistiterator(iter)                      \
         ((iter)->next = 0, 0)

/* define: initfirst_slistiterator
 * Implements <slist_t.initfirst_slistiterator>. */
#define initfirst_slistiterator(iter, list)           \
         ( __extension__ ({                           \
            typeof(iter) _iter = (iter) ;             \
            typeof(list) _list = (list) ;             \
            *_iter = (typeof(*_iter))                 \
                     { first_slist(_list), _list } ;  \
            0 ;                                       \
         }))

/* define: next_slistiterator
 * Implements <slist_t.next_slistiterator>. */
#define next_slistiterator(iter, node)                \
         ( __extension__ ({                           \
            typeof(iter) _iter = (iter) ;             \
            typeof(node) _nd   = (node) ;             \
            bool _isNext = (0 != _iter->next) ;       \
            if (_isNext) {                            \
               *_nd = _iter->next ;                   \
               if (_iter->list->last == _iter->next)  \
                  _iter->next = 0 ;                   \
               else                                   \
                  _iter->next =                       \
                           next_slist(_iter->next) ;  \
            }                                         \
            _isNext ;                                 \
         }))

// group: slist_t

/* define: genericcast_slist
 * Implements <slist_t.genericcast_slist>. */
#define genericcast_slist(list)                                                  \
   ( __extension__ ({                                                            \
      static_assert(offsetof(typeof(*(list)), last) == offsetof(slist_t, last),  \
         "ensure same structure") ;                                              \
      static_assert((typeof((list)->last))0 == (slist_node_t*)0,                 \
         "ensure same type") ;                                                   \
      (slist_t*) (list) ;                                                        \
   }))

/* define: isinlist_slist
 * Implements <slist_t.isinlist_slist>. */
#define isinlist_slist(node) \
         (0 != (node)->next)

/* define: first_slist
 * Implements <slist_t.first_slist>. */
#define first_slist(list)                    ((list)->last ? next_slist((list)->last) : 0)

/* define: init_slist
 * Implements <slist_t.init_slist>. */
#define init_slist(list)                     ((void)(*(list) = (slist_t)slist_INIT))

/* define: initsingle_slist
 * Implements <slist_t.initsingle_slist>. */
#define initsingle_slist(list, node)         ((void)(*(list) = (slist_t){ node }, (node)->next = (node)))

/* define: isempty_slist
 * Implements <slist_t.isempty_slist>. */
#define isempty_slist(list)                  (0 == (list)->last)

/* define: last_slist
 * Implements <slist_t.last_slist>. */
#define last_slist(list)                     ((list)->last)

/* define: next_slist
 * Implements <slist_t.next_slist>. */
#define next_slist(node)                     ((node)->next)

/* define: removeall_slist
 * Implements <slist_t.removeall_slist> with help of <slist_t.free_slist>. */
#define removeall_slist(list, nodeoffset, typeadp) \
         free_slist((list), (nodeoffset), (typeadp))

/* define: slist_IMPLEMENT
 * Implements <slist_t.slist_IMPLEMENT>. */
#define slist_IMPLEMENT(_fsuffix, object_t, name_nextptr)    \
   typedef slist_iterator_t   iteratortype##_fsuffix ;       \
   typedef object_t        *  iteratedtype##_fsuffix ;       \
   static inline int  initfirst##_fsuffix##iterator(slist_iterator_t * iter, slist_t * list) __attribute__ ((always_inline)) ; \
   static inline int  free##_fsuffix##iterator(slist_iterator_t * iter) __attribute__ ((always_inline)) ; \
   static inline bool next##_fsuffix##iterator(slist_iterator_t * iter, object_t ** node) __attribute__ ((always_inline)) ; \
   static inline void init##_fsuffix(slist_t * list) __attribute__ ((always_inline)) ; \
   static inline int  free##_fsuffix(slist_t * list, struct typeadapt_t * typeadp) __attribute__ ((always_inline)) ; \
   static inline int  isempty##_fsuffix(const slist_t * list) __attribute__ ((always_inline)) ; \
   static inline object_t * first##_fsuffix(const slist_t * list) __attribute__ ((always_inline)) ; \
   static inline object_t * last##_fsuffix(const slist_t * list) __attribute__ ((always_inline)) ; \
   static inline object_t * next##_fsuffix(object_t * node) __attribute__ ((always_inline)) ; \
   static inline bool isinlist##_fsuffix(object_t * node) __attribute__ ((always_inline)) ; \
   static inline void insertfirst##_fsuffix(slist_t * list, object_t * new_node) __attribute__ ((always_inline)) ; \
   static inline void insertlast##_fsuffix(slist_t * list, object_t * new_node) __attribute__ ((always_inline)) ; \
   static inline void insertafter##_fsuffix(slist_t * list, object_t * prev_node, object_t * new_node) __attribute__ ((always_inline)) ; \
   static inline int removefirst##_fsuffix(slist_t * list, object_t ** removed_node) __attribute__ ((always_inline)) ; \
   static inline int removeafter##_fsuffix(slist_t * list, object_t * prev_node, object_t ** removed_node) __attribute__ ((always_inline)) ; \
   static inline int removeall##_fsuffix(slist_t * list, struct typeadapt_t * typeadp) __attribute__ ((always_inline)) ; \
   static inline slist_node_t * asnode##_fsuffix(object_t * object) { \
      static_assert(&((object_t*)0)->name_nextptr == (slist_node_t**)offsetof(object_t, name_nextptr), "correct type") ; \
      return (slist_node_t *) ((uintptr_t)object + offsetof(object_t, name_nextptr)) ; \
   } \
   static inline object_t * asobject##_fsuffix(slist_node_t * node) { \
      return (object_t *) ((uintptr_t)node - offsetof(object_t, name_nextptr)) ; \
   } \
   static inline object_t * asobjectnull##_fsuffix(slist_node_t * node) { \
      return node ? (object_t *) ((uintptr_t)node - offsetof(object_t, name_nextptr)) : 0 ; \
   } \
   static inline void init##_fsuffix(slist_t * list) { \
      init_slist(list) ; \
   } \
   static inline void initsingle##_fsuffix(slist_t * list, object_t * node) { \
      initsingle_slist(list, asnode##_fsuffix(node)) ; \
   } \
   static inline int free##_fsuffix(slist_t * list, struct typeadapt_t * typeadp) { \
      return free_slist(list, offsetof(object_t, name_nextptr), typeadp) ; \
   } \
   static inline int isempty##_fsuffix(const slist_t * list) { \
      return isempty_slist(list) ; \
   } \
   static inline object_t * first##_fsuffix(const slist_t * list) { \
      return asobjectnull##_fsuffix(first_slist(list)) ; \
   } \
   static inline object_t * last##_fsuffix(const slist_t * list) { \
      return asobjectnull##_fsuffix(last_slist(list)) ; \
   } \
   static inline object_t * next##_fsuffix(object_t * node) { \
      return asobject##_fsuffix(next_slist(asnode##_fsuffix(node))) ; \
   } \
   static inline bool isinlist##_fsuffix(object_t * node) { \
      return isinlist_slist(asnode##_fsuffix(node)) ; \
   } \
   static inline void insertfirst##_fsuffix(slist_t * list, object_t * new_node) { \
      insertfirst_slist(list, asnode##_fsuffix(new_node)) ; \
   } \
   static inline void insertlast##_fsuffix(slist_t * list, object_t * new_node) { \
      insertlast_slist(list, asnode##_fsuffix(new_node)) ; \
   } \
   static inline void insertafter##_fsuffix(slist_t * list, object_t * prev_node, object_t * new_node) { \
      insertafter_slist(list, asnode##_fsuffix(prev_node), asnode##_fsuffix(new_node)) ; \
   } \
   static inline int removefirst##_fsuffix(slist_t * list, object_t ** removed_node) { \
      int err = removefirst_slist(list, (slist_node_t**)removed_node) ; \
      if (!err) *removed_node = asobject##_fsuffix(*(slist_node_t**)removed_node) ; \
      return err ; \
   } \
   static inline int removeafter##_fsuffix(slist_t * list, object_t * prev_node, object_t ** removed_node) { \
      int err = removeafter_slist(list, asnode##_fsuffix(prev_node), (slist_node_t**)removed_node) ; \
      if (!err) *removed_node = asobject##_fsuffix(*(slist_node_t**)removed_node) ; \
      return err ; \
   } \
   static inline int removeall##_fsuffix(slist_t * list, struct typeadapt_t * typeadp) { \
      return removeall_slist(list, offsetof(object_t, name_nextptr), typeadp) ; \
   } \
   static inline int initfirst##_fsuffix##iterator(slist_iterator_t * iter, slist_t * list) { \
      return initfirst_slistiterator(iter, list) ; \
   } \
   static inline int free##_fsuffix##iterator(slist_iterator_t * iter) { \
      return free_slistiterator(iter) ; \
   } \
   static inline bool next##_fsuffix##iterator(slist_iterator_t * iter, object_t ** node) { \
      bool isNext = next_slistiterator(iter, (slist_node_t**)node) ;    \
      if (isNext) *node = asobject##_fsuffix(*(slist_node_t**)node) ;   \
      return isNext ; \
   }

#endif
