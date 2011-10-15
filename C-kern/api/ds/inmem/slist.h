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
#ifndef CKERN_DATASTRUCTURE_INMEMORY_SINGLELINKEDLIST_HEADER
#define CKERN_DATASTRUCTURE_INMEMORY_SINGLELINKEDLIST_HEADER

#include "C-kern/api/aspect/callback.h"
#include "C-kern/api/aspect/slistnode.h"
#include "C-kern/api/aspect/callback/free.h"

/* typedef: slist_t typedef
 * Exports <slist_t>. */
typedef struct slist_t              slist_t ;

/* typedef: freecb_slist_f
 * Free resource callback.
 * <freecb_slist_f> is defined as:
 * > int (*freecb_slist_f) ( callback_param_t *, slist_aspect * ) */
free_callback_ADAPT(       freecb_slist, callback_param_t, slist_aspect_t )


// section: functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_ds_inmem_slist
 * Test single linked list functionality. */
extern int unittest_ds_inmem_slist(void) ;
#endif


/* struct: slist_t
 * Stores pointers to first/last element of single linked list. */
struct slist_t {
   /* variable: last
    * Points to last element (tail) of list. */
   slist_aspect_t  * last ;
} ;

// group: lifetime

/* define: slist_INIT
 * Static initializer: you can use it instead of <init_slist>.
 * After assigning you can call <free_slist> or any other function safely. */
#define slist_INIT                  { (void*)0 }

/* constructor: init_slist
 * Initializes a single linked list object.
 * The caller has to provide an unitialized list object. This function never fails. */
extern void init_slist(/*out*/slist_t * list ) ;

/* destructor: free_slist
 * Frees memory of all linked objects.
 * Calling free_slist is only safe after calling <init_slist> or after initializing it with <slist_INIT>.
 * Calling it on an already freed object does nothing. <removeall_slist> is called internally to free memory. */
extern int free_slist( slist_t * list, callback_param_t * cb, freecb_slist_f free_callback ) ;

// group: query

/* function: isempty_slist
 * Returns true if list contains no nodes. */
extern int isempty_slist( const slist_t * list ) ;

/* function: first_slist
 * Returns the first node in the list else NULL. */
extern slist_aspect_t * first_slist( const slist_t * list ) ;

/* function: last_slist
 * Returns the last node in the list else NULL. */
extern slist_aspect_t * last_slist( const slist_t * list ) ;

/* function: next_slist
 * Returns the node coming after this node.
 * If this is the last node in the list NULL is returned instead. */
extern slist_aspect_t * next_slist( const slist_aspect_t * node ) ;

// group: iterate

/* define: foreach_slist
 * Macro to iterate all contained nodes.
 * > #define foreach_slist( list, node )
 *
 * Parameter:
 * listobj - Pointer to <slist_t>.
 * node    - The name of the variable of type typeof(listobj->last) which iterates from first to last stored node in list.
 *
 * Do *not* remove the current node.
 * If you want to remove/delete the current node after visiting it use instead:
 * > while( (node=first_slist( &list )) ) {  .... ; removefirst_slist( &list ) ; } */
#define foreach_slist( /*slist_t * */listobj, /*name of slist_aspect_t * */node ) \
   foreach_generic_slist(slist, listobj, node)

// group: change

/* function: insertfirst_slist
 * Makes new_node the new first element of list.
 * Ownership is transfered from caller to <slist_t>. */
extern int insertfirst_slist( slist_t * list, slist_aspect_t * new_node ) ;

/* function: insertlast_slist
 * Makes new_node the new last element of list.
 * Ownership is transfered from caller to <slist_t>. */
extern int insertlast_slist( slist_t * list, slist_aspect_t * new_node ) ;

/* function: insertafter_slist
 * Adds new_node after prev_node into list.
 * Ownership is transfered from caller to <slist_t>.
 *
 * Note:
 * Make sure that prev_node is part of the list !*/
extern int insertafter_slist( slist_t * list, slist_aspect_t * prev_node, slist_aspect_t * new_node ) ;

/* function: removefirst_slist
 * Removes the first element from list.
 * If the list contains no elements ESRCH is returned. In case of success removed_node points
 * to the removed first element. Ownership of removed_node is transfered back to caller. */
extern int removefirst_slist( slist_t * list, slist_aspect_t ** removed_node ) ;

/* function: removeafter_slist
 * Removes the next node coming after prev_node from the list.
 * If prev_node points to the last node then ESRCH is returned.
 * If the list contains no elements EINVAL is returned.
 *
 * Note:
 * Make sure that prev_node is part of the list ! */
extern int removeafter_slist( slist_t * list, slist_aspect_t * prev_node, slist_aspect_t ** removed_node ) ;

// group: change set

/* function: removeall_slist
 * Removes all nodes from the list.
 * For every removed node *free_callback* is called. */
extern int removeall_slist( slist_t * list, callback_param_t * cb, freecb_slist_f remove_callback ) ;

// group: interface adaption

/* define: slist_DECLARE
 * Declares a list object managing objects of type object_t.
 *
 * Parameter:
 * listname  - The name of the newly declared list structure is:  listname ## _t
 * object_t  - Type of objects this list should handle.
 * */
#define slist_DECLARE(listname, object_t) \
   typedef struct listname ## _t    listname ## _t ; \
   struct listname ## _t {    \
      object_t * last ;    \
   } ;

/* define: slist_IMPLEMENT
 * Generates the interface for a specific single linked list.
 * The type of the list object must be declared with help of <slist_DECLARE>
 * before this macro.
 * > #define slist_IMPLEMENT(listname, name_nextaspect, cb_t)
 *
 * Parameter:
 * listname        - The name of the newly declared list.
 *                   The type of the list object must structure is:  listname ## _t
 * name_nextaspect - The name (access path) of the next pointer in object type managed by this list.
 * cb_t            - Type of the first callback parameter.
 * */
#define slist_IMPLEMENT(listname, name_nextaspect, cb_t) \
   typedef free_callback_SIGNATURE( freecb ## _ ## listname ## _f, cb_t, typeof(*((listname ## _t*)0)->last) ) ; \
   static inline void init ## _ ## listname(listname ## _t * list) __attribute__ ((always_inline)) ; \
   static inline int free ## _ ## listname(listname ## _t * list, cb_t * cb, freecb ## _ ## listname ## _f free_callback) __attribute__ ((always_inline)) ; \
   static inline int isempty ## _ ## listname( const listname ## _t * list ) __attribute__ ((always_inline)) ; \
   static inline uint32_t next_offset ## _ ## listname(void) __attribute__ ((always_inline)) ; \
   static inline typeof(*((listname ## _t*)0)->last) * first ## _ ## listname ( const listname ## _t * list ) __attribute__ ((always_inline)) ; \
   static inline typeof(*((listname ## _t*)0)->last) * last  ## _ ## listname ( const listname ## _t * list ) __attribute__ ((always_inline)) ; \
   static inline typeof(*((listname ## _t*)0)->last) * next  ## _ ## listname ( const typeof(*((listname ## _t*)0)->last) * node ) __attribute__ ((always_inline)) ; \
   static inline int insertfirst ## _ ## listname ( listname ## _t * list, typeof(*((listname ## _t*)0)->last) * new_node ) __attribute__ ((always_inline)) ; \
   static inline int insertlast  ## _ ## listname ( listname ## _t * list, typeof(*((listname ## _t*)0)->last) * new_node ) __attribute__ ((always_inline)) ; \
   static inline int insertafter ## _ ## listname ( listname ## _t * list, typeof(*((listname ## _t*)0)->last) * prev_node, typeof(*((listname ## _t*)0)->last) * new_node ) __attribute__ ((always_inline)) ; \
   static inline int removefirst ## _ ## listname ( listname ## _t * list, typeof(*((listname ## _t*)0)->last) ** removed_node ) __attribute__ ((always_inline)) ; \
   static inline int removeafter ## _ ## listname ( listname ## _t * list, typeof(*((listname ## _t*)0)->last) * prev_node, typeof(*((listname ## _t*)0)->last)** removed_node ) __attribute__ ((always_inline)) ; \
   static inline int removeall   ## _ ## listname ( listname ## _t * list, cb_t * cb, freecb ## _ ## listname ## _f free_callback ) __attribute__ ((always_inline)) ; \
   static inline uint32_t next_offset ## _ ## listname(void) { \
      static_assert( offsetof(listname ## _t, last)  == 0, #listname "_t must have same structure as slist_t" ) ; \
      static_assert( offsetof( typeof(*((listname ## _t*)0)->last), name_nextaspect) < 65536, "Offset must fit in uint32_t (extend 65536 to pow(2,32) if needed)") ; \
      return (uint32_t) offsetof( typeof(*((listname ## _t*)0)->last), name_nextaspect ) ; \
   } \
   static inline void init ## _ ## listname(listname ## _t * list) { \
      init_slist(list) ; \
   } \
   static inline int free ## _ ## listname(listname ## _t * list, cb_t * cb, freecb ## _ ## listname ## _f free_callback) { \
      return free_generic_slist( (slist_t*)list, cb, (freecb_slist_f) free_callback, next_offset ## _ ## listname() ) ; \
   } \
   static inline int isempty ## _ ## listname( const listname ## _t * list ) { \
      return isempty_slist(list) ; \
   } \
   static inline typeof(*((listname ## _t*)0)->last) * first ## _ ## listname ( const listname ## _t * list ) { \
      return last_slist(list) ? next_ ## listname(last_slist(list)) : 0 ; \
   } \
   static inline typeof(*((listname ## _t*)0)->last) * last  ## _ ## listname ( const listname ## _t * list ) { \
      return last_slist(list) ; \
   } \
   static inline typeof(*((listname ## _t*)0)->last) * next  ## _ ## listname ( const typeof(*((listname ## _t*)0)->last) * node ) { \
      return (typeof(*((listname ## _t*)0)->last)*) ((const slist_aspect_t*)(next_offset ## _ ## listname() + (const uint8_t*)node))-> next ; \
   } \
   static inline int insertfirst ## _ ## listname ( listname ## _t * list, typeof(*((listname ## _t*)0)->last) * new_node ) { \
      return insertfirst_generic_slist( (slist_t*)list, (slist_aspect_t*)new_node, next_offset ## _ ## listname()) ; \
   } \
   static inline int insertlast  ## _ ## listname ( listname ## _t * list, typeof(*((listname ## _t*)0)->last) * new_node ) { \
      return insertlast_generic_slist( (slist_t*)list, (slist_aspect_t*)new_node, next_offset ## _ ## listname()) ; \
   } \
   static inline int insertafter ## _ ## listname ( listname ## _t * list, typeof(*((listname ## _t*)0)->last) * prev_node, typeof(*((listname ## _t*)0)->last) * new_node ) { \
      return insertafter_generic_slist( (slist_t*)list, (slist_aspect_t*)prev_node, (slist_aspect_t*)new_node, next_offset ## _ ## listname()) ; \
   } \
   static inline int removefirst ## _ ## listname ( listname ## _t * list, typeof(*((listname ## _t*)0)->last) ** removed_node ) { \
      return removefirst_generic_slist( (slist_t*)list, (slist_aspect_t**)removed_node, next_offset ## _ ## listname()) ; \
   } \
   static inline int removeafter ## _ ## listname ( listname ## _t * list, typeof(*((listname ## _t*)0)->last) * prev_node, typeof(*((listname ## _t*)0)->last)** removed_node ) { \
      return removeafter_generic_slist( (slist_t*)list, (slist_aspect_t*)prev_node, (slist_aspect_t**)removed_node, next_offset ## _ ## listname()) ; \
   } \
   static inline int removeall   ## _ ## listname ( listname ## _t * list, cb_t * cb, freecb ## _ ## listname ## _f free_callback ) { \
      return removeall_generic_slist( (slist_t*)list, (callback_param_t*)cb, (freecb_slist_f)free_callback, next_offset ## _ ## listname()) ; \
   }


// group: generic implementation

/* define: foreach_generic_slist
 * Macro to iterate all contained nodes (see <foreach_slist>).
 * > #define foreach_generic_slist( listname, listobj, node)
 * Do *not* remove the current node.
 * If you want to remove/delete the current node after visiting it use instead:
 * > while( (node=first_slist( &list )) ) {  .... ; removefirst_slist( &list ) ; }
 *
 * Parameter:
 * listname - The name of the list interface. This name is used to access
 *            the generated function with name <next_offset_ ## slist>.
 * listobj  - Pointer to <slist ## _t>.
 * node     - The name of the variable of type typeof(listobj->last) which iterates from first to last stored node. */
#define foreach_generic_slist(listname, listobj, node) \
   for( typeof((listobj)->last) node = first_ ## listname(listobj); node ; node = (node == (listobj)->last) ? 0 : next_ ## listname(node))

/* function: free_generic_slist
 * Generic implementation of <free_slist>. */
extern int free_generic_slist( slist_t * list, callback_param_t * cb, freecb_slist_f free_callback, uint32_t offset_next ) ;

/* function: insertfirst_generic_slist
 * Generic implementation of <insertfirst_slist>. */
extern int insertfirst_generic_slist( slist_t * list, slist_aspect_t * new_node, uint32_t offset_next ) ;

/* function: insertlast_generic_slist
 * Generic implementation of <insertlast_slist>. */
extern int insertlast_generic_slist( slist_t * list, slist_aspect_t * new_node, uint32_t offset_next ) ;

/* function: insertafter_generic_slist
 * Generic implementation of <insertafter_slist>. */
extern int insertafter_generic_slist( slist_t * list, slist_aspect_t * prev_node, slist_aspect_t * new_node, uint32_t offset_next ) ;

/* function: removefirst_generic_slist
 * Generic implementation of <removefirst_slist>. */
extern int removefirst_generic_slist( slist_t * list, slist_aspect_t ** removed_node, uint32_t offset_next ) ;

/* function: removeafter_generic_slist
 * Generic implementation of <removeafter_slist>. */
extern int removeafter_generic_slist( slist_t * list, slist_aspect_t * prev_node, slist_aspect_t ** removed_node, uint32_t offset_next ) ;

/* function: removeall_generic_slist
 * Generic implementation of <removeall_slist>. */
extern int removeall_generic_slist( slist_t * list, callback_param_t * cb, freecb_slist_f remove_callback, uint32_t offset_next ) ;


// section: inline implementations

/* define: free_slist
 * Implements <slist_t.free_slist> with help of <slist_t.free_generic_slist>. */
#define /*int*/ free_slist(list, cb, free_callback) \
   free_generic_slist((list), (cb), (free_callback), 0)

/* define: first_slist
 * Implements <slist_t.first_slist>.
 * > ((list)->last ? next_slist((list)->last) : 0) */
#define /*slist_aspect_t* */ first_slist(/*const slist_t * */list) \
   ((list)->last ? next_slist((list)->last) : 0)

/* define: last_slist
 * Implements <slist_t.last_slist>.
 * > ((list)->last) */
#define /*slist_aspect_t* */ last_slist(/*const slist_t * */list) \
   ((list)->last)

/* define: init_slist
 * Implements <slist_t.init_slist>.
 * > ((list)->last = 0, 0) */
#define /*void*/ init_slist(/*out slist_t * */list) \
   ((list)->last = 0)

/* define: insertafter_slist
 * Implements <slist_t.insertafter_slist> with help of <slist_t.insertafter_generic_slist>. */
#define /*int*/ insertafter_slist(list, prev_node, new_node) \
   insertafter_generic_slist((list), (prev_node), (new_node), 0)

/* define: insertfirst_slist
 * Implements <slist_t.insertfirst_slist> with help of <slist_t.insertfirst_generic_slist>. */
#define /*int*/ insertfirst_slist(list, new_node) \
   insertfirst_generic_slist((list), (new_node), 0)

/* define: insertlast_slist
 * Implements <slist_t.insertlast_slist> with help of <slist_t.insertlast_generic_slist>. */
#define /*int*/ insertlast_slist(list, new_node) \
   insertlast_generic_slist((list), (new_node), 0)

/* define: isempty_slist
 * Implements <slist_t.isempty_slist>.
 * > (0 != (list)->last) */
#define /*int*/ isempty_slist(/*const slist_t * */list) \
   (0 == (list)->last)

/* define: next_slist
 * Implements <slist_t.next_slist>.
 * > (node->next) */
#define next_slist(/*const slist_aspect_t * */node) \
   ((node)->next)

/* define: removeafter_slist
 * Implements <slist_t.removeafter_slist> with help of <slist_t.removeafter_generic_slist>. */
#define /*int*/ removeafter_slist(list, prev_node, removed_node) \
   removeafter_generic_slist((list), (prev_node), (removed_node), 0)

/* define: removeall_slist
 * Implements <slist_t.removeall_slist> with help of <slist_t.removeall_generic_slist>. */
#define /*int*/ removeall_slist(list, cb, remove_callback) \
   removeall_generic_slist((list), (cb), (remove_callback), 0)

/* define: removeall_generic_slist
 * Implements <slist_t.removeall_generic_slist> with help of <slist_t.free_generic_slist>. */
#define /*int*/ removeall_generic_slist(list, cb, remove_callback, offset_next) \
   free_generic_slist((list), (cb), (remove_callback), (offset_next))

/* define: removefirst_slist
 * Implements <slist_t.removefirst_slist> with help of <slist_t.removefirst_generic_slist>. */
#define /*int*/ removefirst_slist(list, removed_node) \
   removefirst_generic_slist((list), (removed_node), 0)

#endif
