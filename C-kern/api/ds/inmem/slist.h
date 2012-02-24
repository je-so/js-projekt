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
#include "C-kern/api/ds/freecallback.h"

/* typedef: struct slist_t
 * Export <slist_t>. */
typedef struct slist_t                 slist_t ;

/* typedef: struct slist_iterator_t
 * Export <slist_iterator_t>. */
typedef struct slist_iterator_t        slist_iterator_t ;

/* typedef: struct slist_freecb_t
 * Exports <slist_freecb_t>. */
typedef struct slist_freecb_t          slist_freecb_t ;

/* typedef: slist_freecb_f
 * Adaption of <freecallback_t> and <freecallback_f> to <slist_t>. */
freecallback_ADAPT(                    slist_freecb_f, slist_freecb_t, slist_node_t )


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_ds_inmem_slist
 * Test <slist_t> functionality. */
int unittest_ds_inmem_slist(void) ;
#endif


/* struct: slist_freecb_t
 * Adapted freecallback used in <free_slist> as parameter.
 * See also <freecallback_t>. */
struct slist_freecb_t {
   slist_freecb_f    fct ;
} ;


/* struct: slist_iterator_t
 * Iterates over elements contained in <slist_t>. */
struct slist_iterator_t {
   slist_node_t * next ;
} ;

// group: lifetime

/* define: slist_iterator_INIT_FREEABLE
 * Static initializer. */
#define slist_iterator_INIT_FREEABLE   { 0 }

/* function: init_slistiterator
 * Initializes an iterator for <slist_t>. */
int init_slistiterator(/*out*/slist_iterator_t * iter, slist_t * list) ;

/* function: free_slistiterator
 * Frees an iterator for <slist_t>.
 * This is a no-op. */
int free_slistiterator(slist_iterator_t * iter) ;

// group: iterate

/* function: next_slistiterator
 * Returns next iterated node.
 * The first call after <init_slistiterator> returns the first list element
 * if it is not empty.
 *
 * Returns:
 * true  - node contains a pointer to the next valid node in the list.
 * false - There is no next node. The last element was already returned or the list is empty. */
bool next_slistiterator(slist_iterator_t * iter, slist_t * list, /*out*/slist_node_t ** node) ;


/* struct: slist_t
 * Stores pointers to last element of single linked list.
 * The list is organized as a ring so the next element
 * of the last is the first or head of list.
 *
 * Generic Implementation:
 * Almost all exported interface functions take an extra argument *offset_next*.
 * This argument gives the offset of the next pointer in a node which is node of type <slist_node_t>.
 * For example if you have an object which can be contained in two lists
 * > struct gobject_t {
 * > ... ; gobject_t * next ;
 * > ... ; gobject_t * list2_next ;
 * > ... } ;
 * The first list uses gobject_t.next to link its nodes and the second list takes gobject_t.list2_next.
 * The differentiate between the two links set the parameter *offset_next* to
 * > offsetof(gobject,next)       // either
 * > offsetof(gobject,list2_next) // or
 *
 * To make things simpler use the macros <slist_DECLARE> and <slist_IMPLEMENT>
 * which makes it possible to generate a listobject (see <slist_DECLARE>) which manages
 * objects other than <slist_node_t> and to generate an interface (see <slist_IMPLEMENT>)
 * which eliminates the need to provide the parameter *offset_next*.
 * */
struct slist_t {
   /* variable: last
    * Points to last element (tail) of list. */
   slist_node_t  * last ;
} ;

// group: lifetime

/* define: slist_INIT
 * Static initializer: you can use it instead of <init_slist>.
 * After assigning you can call <free_slist> or any other function safely. */
#define slist_INIT                     { (void*)0 }

/* constructor: init_slist
 * Initializes a single linked list object.
 * The caller has to provide an unitialized list object. This function never fails. */
void init_slist(/*out*/slist_t * list) ;

/* destructor: free_slist
 * Frees memory of all linked objects (generic).
 * Calling free_slist is only safe after calling <init_slist> or after initializing it with <slist_INIT>.
 * Calling it on an already freed object does nothing. */
int free_slist(slist_t * list, slist_freecb_t * freehandler, uint32_t offset_next) ;

// group: query

/* function: isempty_slist
 * Returns true if list contains no nodes. */
int isempty_slist(const slist_t * list) ;

/* function: first_slist
 * Returns the first node in the list else NULL (if <isempty_slist>). */
slist_node_t * first_slist(const slist_t * list, uint32_t offset_next) ;

/* function: last_slist
 * Returns the last node in the list else NULL (if <isempty_slist>). */
slist_node_t * last_slist(const slist_t * list) ;

/* function: next_slist
 * Returns the node coming after this node (generic).
 * If this is the last node in the list the first is returned instead
 * cause this list is implement as a ring list. */
slist_node_t * next_slist(slist_node_t * node, uint32_t offset_next) ;

// group: iterate

/* function: iteratortype_slist
 * Function declaration to associate <slist_iterator_t> with <slist_t>.
 * The association is done with the type of the return value.
 * It must be a pointer to the iterator type. */
slist_iterator_t * iteratortype_slist(void) ;

/* function: iteratedtype_slist
 * Function declaration to associate (<slist_node_t>*) with <slist_t>.
 * The association is done with the type of the return value.
 * It must be the type of node the iterator returns with a call to <next_slistiterator>. */
slist_node_t * iteratedtype_slist(void) ;

// group: change

/* function: insertfirst_slist
 * Makes new_node the new first element of list (generic).
 * Ownership is transfered from caller to <slist_t>. */
int insertfirst_slist(slist_t * list, slist_node_t * new_node, uint32_t offset_next) ;

/* function: insertlast_slist
 * Makes new_node the new last element of list.
 * Ownership is transfered from caller to <slist_t>. */
int insertlast_slist(slist_t * list, slist_node_t * new_node, uint32_t offset_next) ;

/* function: insertafter_slist
 * Adds new_node after prev_node into list.
 * Ownership is transfered from caller to <slist_t>.
 *
 * Note:
 * Make sure that prev_node is part of the list !*/
int insertafter_slist(slist_t * list, slist_node_t * prev_node, slist_node_t * new_node, uint32_t offset_next) ;

/* function: removefirst_slist
 * Removes the first element from list.
 * If the list contains no elements ESRCH is returned. In case of success removed_node points
 * to the removed first element. Ownership of removed_node is transfered back to caller. */
int removefirst_slist(slist_t * list, slist_node_t ** removed_node, uint32_t offset_next) ;

/* function: removeafter_slist
 * Removes the next node coming after prev_node from the list.
 * If prev_node points to the last node then ESRCH is returned.
 * If the list contains no elements EINVAL is returned.
 *
 * Note:
 * Make sure that prev_node is part of the list ! */
int removeafter_slist(slist_t * list, slist_node_t * prev_node, slist_node_t ** removed_node, uint32_t offset_next) ;

// group: change set

/* function: removeall_slist
 * Removes all nodes from the list (generic).
 * For every removed node *freehandler* is called. */
int removeall_slist(slist_t * list, slist_freecb_t * freehandler, uint32_t offset_next) ;

// group: generic

/* define: slist_DECLARE
 * Declares a list object managing objects of type object_t.
 *
 * Parameter:
 * listtype_t - The type name of the newly declared list structure.
 * object_t   - Type of objects this list should handle.
 * */
#define slist_DECLARE(listtype_t, object_t)        \
   typedef struct listtype_t        listtype_t ;   \
   struct listtype_t {                             \
      object_t * last ;                            \
   } ;

/* define: slist_IMPLEMENT
 * Generates the interface for a specific single linked list.
 * The type of the list object must be declared with help of <slist_DECLARE>
 * before this macro. It is also possible to construct "listtype_t" in another way before
 * calling this macro. In the latter case "listtype_t" must have a pointer to the object
 * declared as its first field with the name *last*.
 *
 *
 * Parameter:
 * listtype_t     - This is the type of a list type structurally equivalent to <slist_t>.
 *                  See also <slist_DECLARE>. *listtype_t* must have been defined as a
 *                  typedef representing the list object.
 * _fctsuffix     - It is the suffix of the generated list interface functions, e.g. "init ## _fctsuffix".
 * name_nextptr   - The name (access path) of the next pointer in objects managed by *listtype_t*.
 *                  The object type is infered from "listtype_t.last"
 * freecallback_t - Free callback structure type. See <freecallback_t> how the structure should look like.
 * */
#define slist_IMPLEMENT(listtype_t, _fctsuffix, name_nextptr, freecallback_t) \
   typedef struct listtype_t ## iterator_t   listtype_t ## iterator_t ;                \
   struct listtype_t ## iterator_t { typeof(*((listtype_t*)0)->last) * next ; } ;      \
   listtype_t ## iterator_t        * iteratortype ## _fctsuffix(void) ;                \
   typeof(*((listtype_t*)0)->last) * iteratedtype ## _fctsuffix(void) ;                \
   static inline int  init ## _fctsuffix ## iterator ( listtype_t ## iterator_t * iter, listtype_t * list) __attribute__ ((always_inline)) ; \
   static inline int  free ## _fctsuffix ## iterator ( listtype_t ## iterator_t * iter) __attribute__ ((always_inline)) ; \
   static inline bool next ## _fctsuffix ## iterator ( listtype_t ## iterator_t * iter, listtype_t * list, typeof(*((listtype_t*)0)->last) ** node) __attribute__ ((always_inline)) ; \
   static inline void init    ## _fctsuffix(listtype_t * list) __attribute__ ((always_inline)) ; \
   static inline int  free    ## _fctsuffix(listtype_t * list, freecallback_t * freehandler) __attribute__ ((always_inline)) ; \
   static inline int  isempty ## _fctsuffix( const listtype_t * list ) __attribute__ ((always_inline)) ; \
   static inline uint32_t offsetnext ## _fctsuffix(void) __attribute__ ((always_inline)) ; \
   static inline typeof(*((listtype_t*)0)->last) * first ## _fctsuffix ( const listtype_t * list ) __attribute__ ((always_inline)) ; \
   static inline typeof(*((listtype_t*)0)->last) * last  ## _fctsuffix ( const listtype_t * list ) __attribute__ ((always_inline)) ; \
   static inline typeof(*((listtype_t*)0)->last) * next  ## _fctsuffix ( typeof(*((listtype_t*)0)->last) * node ) __attribute__ ((always_inline)) ; \
   static inline int insertfirst ## _fctsuffix ( listtype_t * list, typeof(*((listtype_t*)0)->last) * new_node ) __attribute__ ((always_inline)) ; \
   static inline int insertlast  ## _fctsuffix ( listtype_t * list, typeof(*((listtype_t*)0)->last) * new_node ) __attribute__ ((always_inline)) ; \
   static inline int insertafter ## _fctsuffix ( listtype_t * list, typeof(*((listtype_t*)0)->last) * prev_node, typeof(*((listtype_t*)0)->last) * new_node ) __attribute__ ((always_inline)) ; \
   static inline int removefirst ## _fctsuffix ( listtype_t * list, typeof(*((listtype_t*)0)->last) ** removed_node ) __attribute__ ((always_inline)) ; \
   static inline int removeafter ## _fctsuffix ( listtype_t * list, typeof(*((listtype_t*)0)->last) * prev_node, typeof(*((listtype_t*)0)->last)** removed_node ) __attribute__ ((always_inline)) ; \
   static inline int removeall   ## _fctsuffix ( listtype_t * list, freecallback_t * freehandler) __attribute__ ((always_inline)) ; \
   static inline uint32_t offsetnext ## _fctsuffix(void) { \
      static_assert( offsetof(freecallback_t, fct)   == 0, "freecallback_t has correct type" ) ; \
      static_assert( offsetof(listtype_t, last)  == 0, "listtype_t must have same structure as slist_t" ) ; \
      static_assert( offsetof( typeof(*((listtype_t*)0)->last), name_nextptr) < 65536, "Offset must fit in uint32_t (extend 65536 to pow(2,32) if needed)") ; \
      return (uint32_t) offsetof( typeof(*((listtype_t*)0)->last), name_nextptr ) ; \
   } \
   static inline void init ## _fctsuffix(listtype_t * list) { \
      init_slist(list) ; \
   } \
   static inline int free ## _fctsuffix(listtype_t * list, freecallback_t * freehandler) { \
      return free_slist( (slist_t*)list, (slist_freecb_t*) freehandler, offsetnext ## _fctsuffix() ) ; \
   } \
   static inline int isempty ## _fctsuffix( const listtype_t * list ) { \
      return isempty_slist(list) ; \
   } \
   static inline typeof(*((listtype_t*)0)->last) * first ## _fctsuffix ( const listtype_t * list ) { \
      return (typeof(((listtype_t*)0)->last)) first_slist(list, offsetnext ## _fctsuffix()) ; \
   } \
   static inline typeof(*((listtype_t*)0)->last) * last  ## _fctsuffix ( const listtype_t * list ) { \
      return last_slist(list) ; \
   } \
   static inline typeof(*((listtype_t*)0)->last) * next  ## _fctsuffix ( typeof(*((listtype_t*)0)->last) * node ) { \
      return next_slist(node, offsetnext ## _fctsuffix ()) ; \
   } \
   static inline int insertfirst ## _fctsuffix ( listtype_t * list, typeof(*((listtype_t*)0)->last) * new_node ) { \
      return insertfirst_slist( (slist_t*)list, (slist_node_t*)new_node, offsetnext ## _fctsuffix()) ; \
   } \
   static inline int insertlast  ## _fctsuffix ( listtype_t * list, typeof(*((listtype_t*)0)->last) * new_node ) { \
      return insertlast_slist( (slist_t*)list, (slist_node_t*)new_node, offsetnext ## _fctsuffix()) ; \
   } \
   static inline int insertafter ## _fctsuffix ( listtype_t * list, typeof(*((listtype_t*)0)->last) * prev_node, typeof(*((listtype_t*)0)->last) * new_node ) { \
      return insertafter_slist( (slist_t*)list, (slist_node_t*)prev_node, (slist_node_t*)new_node, offsetnext ## _fctsuffix()) ; \
   } \
   static inline int removefirst ## _fctsuffix ( listtype_t * list, typeof(*((listtype_t*)0)->last) ** removed_node ) { \
      return removefirst_slist( (slist_t*)list, (slist_node_t**)removed_node, offsetnext ## _fctsuffix()) ; \
   } \
   static inline int removeafter ## _fctsuffix ( listtype_t * list, typeof(*((listtype_t*)0)->last) * prev_node, typeof(*((listtype_t*)0)->last)** removed_node ) { \
      return removeafter_slist( (slist_t*)list, (slist_node_t*)prev_node, (slist_node_t**)removed_node, offsetnext ## _fctsuffix()) ; \
   } \
   static inline int removeall   ## _fctsuffix ( listtype_t * list, freecallback_t * freehandler) { \
      return removeall_slist( (slist_t*)list, (slist_freecb_t*)freehandler, offsetnext ## _fctsuffix()) ; \
   } \
   static inline int  init ## _fctsuffix ## iterator ( listtype_t ## iterator_t * iter, listtype_t * list) { \
      iter->next = first ## _fctsuffix(list) ; return 0 ; \
   } \
   static inline int  free ## _fctsuffix ## iterator ( listtype_t ## iterator_t * iter) { \
      (void) iter ; return 0 ; \
   } \
   static inline bool next ## _fctsuffix ## iterator ( listtype_t ## iterator_t * iter, listtype_t * list, typeof(*((listtype_t*)0)->last) ** node) { \
      bool isNext = (0 != iter->next) ; \
      if (isNext) { \
         *node = iter->next ; \
         iter->next = ((list)->last == iter->next) ? 0 : next ## _fctsuffix (iter->next) ; \
      } \
      return isNext ; \
   }


// section: inline implementations

/* define: first_slist
 * Implements <slist_t.first_slist>. */
#define first_slist(list, offset_next) ((list)->last ? next_slist((list)->last, offset_next) : 0)

/* define: init_slist
 * Implements <slist_t.init_slist>. */
#define init_slist(list)               ((list)->last = 0)

/* define: isempty_slist
 * Implements <slist_t.isempty_slist>. */
#define isempty_slist(list)            (0 == (list)->last)

/* define: last_slist
 * Implements <slist_t.last_slist>. */
#define last_slist(list)               ((list)->last)

/* define: next_slist
 * Implements <slist_t.next_slist>. */
#define next_slist(node, offset_next)  ((typeof(node))(((slist_node_t*)((uintptr_t)offset_next + (uintptr_t)(node)))->next))

/* define: removeall_slist
 * Implements <slist_t.removeall_slist> with help of <slist_t.free_slist>. */
#define removeall_slist(list, freehandler, offset_next) \
   free_slist((list), (freehandler), (offset_next))

/* define: init_slistiterator
 * Implements <slist_t.init_slistiterator>. */
#define init_slistiterator(iter, list) ((iter)->next = first_slist((list), 0), 0)

/* define: free_slistiterator
 * Implements <slist_t.free_slistiterator>. */
#define free_slistiterator(iter)       (0)

/* define: next_slistiterator
 * Implements <slist_t.next_slistiterator>. */
#define next_slistiterator(iter, list, node)          \
   ( __extension__ ({                                 \
      typeof(iter) iter2 = (iter) ;                   \
      bool isNext = (0 != iter2->next) ;              \
      if (isNext) {                                   \
         *(node)     = iter2->next ;                  \
         iter2->next = ((list)->last == iter2->next)? \
                        0 :                           \
                        next_slist(iter2->next, 0) ;  \
      }                                               \
      isNext ;                                        \
   }))


#endif
