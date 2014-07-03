/* title: SyncQueue

   Adapts type <queue_t> for use in <syncrun_t>.

   Includes:
   You need to include types <queue_t> and <dlist_t> before calling the inline functions.

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
   (C) 2013 Jörg Seebohn

   file: C-kern/api/task/syncqueue.h
    Header file <SyncQueue>.

   file: C-kern/task/syncqueue.c
    Implementation file <SyncQueue impl>.
*/
#ifndef CKERN_TASK_SYNCRUN_QUEUE_HEADER
#define CKERN_TASK_SYNCRUN_QUEUE_HEADER

// forward
struct dlist_t ;
struct dlist_node_t ;

/* typedef: struct syncqueue_t
 * Export <syncqueue_t> into global namespace. */
typedef struct syncqueue_t                syncqueue_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_task_syncqueue
 * Test <syncqueue_t> functionality. */
int unittest_task_syncqueue(void) ;
#endif


/* struct: syncqueue_t
 * Internal type to store zero or more <syncthread_t> + additional information.
 * This object uses <queue_t> to implement its functionality.
 *
 * Extension of queue_t:
 * Use <genericcast_queue> to cast pointer to <syncqueue_t> into pointer to
 * <queue_t> if you want to read the content of the queue. Use only functions
 * of this interface if you want to update the content.
 *
 * List of Extensions:
 * o The queue counts the number of stored elements.
 * o It offers functionality to remove any element in the queue by overwriting
 *   it with the last and remove the last instead.
 * o It offers functionality to remember nodes which should be removed in
 *   a linked list and allows to compact the queue after it has been iterated over.
 * */
struct syncqueue_t {
   /* variable: last
    * This variables makes <syncqueue_t> convertible to type <queue_t>. */
   struct dlist_node_t *   last ;
   /* variable: nrelements
    * Number of stored elements. */
   size_t                  nrelements ;
} ;

// group: lifetime

/* define: syncqueue_FREE
 * Static initializer. */
#define syncqueue_FREE syncqueue_INIT

/* define: syncqueue_INIT
 * Static initializer. */
#define syncqueue_INIT { 0, 0 }

/* function: init_syncqueue
 * Initializes syncqueue. */
void init_syncqueue(/*out*/syncqueue_t * syncqueue) ;

/* function: free_syncqueue
 * Frees all allocated memory pages and clears syncqueue.
 * For contained elements no free operation is called. */
int free_syncqueue(syncqueue_t * syncqueue) ;

// group: query

/* function: isfree_syncqueue
 * Returns true if syncqueue is equal to <syncqueue_FREE>. */
bool isfree_syncqueue(const syncqueue_t * syncqueue) ;

/* function: len_syncqueue
 * Returns the number of elements stored in the queue. */
size_t len_syncqueue(const syncqueue_t * syncqueue) ;

/* function: queuefromaddr_syncqueue
 * See <queuefromaddr_queue>. */
syncqueue_t * queuefromaddr_syncqueue(void * nodeaddr) ;

// group: update

/* function: insert_syncqueue
 * Add new element of type IDNAME to end of queue (last).
 * In case of no memory ENOMEM is returned and nothing is done. */
int insert_syncqueue(syncqueue_t * syncqueue, IDNAME ** newelem) ;

/* function: insert2_syncqueue
 * Add new element to end of queue (last).
 * The type is determined by second parameter elem_t and its size with parameter elemsize.
 * The initialization function init_elem is called for the newly allocated element
 * which is returned in out parameter newaddr.
 * In case of no memory ENOMEM is returned and nothing is done.
 *
 * Precondition:
 * o elemsize >= sizeof(elem_t) */
int insert2_syncqueue(syncqueue_t * syncqueue, uint8_t elemsize, IDNAME ** newelem) ;

/* function: remove_syncqueue
 * Removes elem of type IDNAME from the queue by overwriting it with the last.
 * The second parameter elem must be a pointer to type elem_t.
 * The function initmove_elem is called with src set to the last element and
 * dest set to elem. If elem points to the last element in the queue
 * function initmove_elem is not called. */
int remove_syncqueue(syncqueue_t * syncqueue, IDNAME * elem, void (*initmove_elem)(IDNAME * dest, IDNAME * src)) ;

/* function: removefirst_syncqueue
 * Removes an element from the queue by freeing syncqueue memory.
 * Parameter elemsize must be the size of the memory at the begin of syncqueue
 * which is removed. No free function is called for the removed object. */
int removefirst_syncqueue(syncqueue_t * syncqueue, uint16_t elemsize) ;

/* function: removelast_syncqueue
 * Removes an element from the queue by freeing syncqueue memory.
 * Parameter elemsize must be the size of the memory at the end of syncqueue
 * which is removed. No free function is called for the removed object. */
int removelast_syncqueue(syncqueue_t * syncqueue, uint16_t elemsize) ;

/* function: addtofreelist_syncqueue
 * Adds elem to freelist. The function is implemented as macro
 * and the pointer elem has to point to an element of size > sizeof(dlist_node_t).
 *
 * Precondition:
 * o sizeof(*elem) >= sizeof(dlist_node_t)
 * o Elements must be added to freelist in the order from first to last in queue (unchecked). */
void addtofreelist_syncqueue(syncqueue_t * syncqueue, struct dlist_t * freelist, IDNAME * elem) ;

/* function: compact_syncqueue
 * Moves elements from the end of queue to elements stored in freelist.
 * For every moved element function initmove_elem is called. If an element in freelist
 * equals to the last element function initmove_elem is not called. */
int compact_syncqueue(syncqueue_t * syncqueue, IDNAME elem_t, struct dlist_t * freelist, void (*initmove_elem)(void * dest, void * src)) ;

/* function: compact2_syncqueue
 * Called from macro <compact_syncqueue>.
 * Do not call this function directly. Use the macro to ensure type safety. */
int compact2_syncqueue(syncqueue_t * syncqueue, uint16_t elemsize, struct dlist_t * freelist, void (*initmove_elem)(void * dest, void * src)) ;



// section: inline implementation

/* define: insert_syncqueue
 * Implements <syncqueue_t.insert_syncqueue>. */
#define insert_syncqueue(syncqueue, newelem)                   \
         ( __extension__ ({                                    \
            syncqueue_t *           _sq   = (syncqueue) ;      \
            typeof(**(newelem)) **  _elem = (newelem) ;        \
            int _err = insertlast_queue(                       \
                     genericcast_queue(_sq),                   \
                     (void**)_elem, sizeof(**_elem)) ;         \
            _sq->nrelements += (_err == 0) ;                   \
            _err ;                                             \
         }))

/* define: insert2_syncqueue
 * Implements <syncqueue_t.insert2_syncqueue>. */
#define insert2_syncqueue(syncqueue, elemsize, newelem)        \
         ( __extension__ ({                                    \
            syncqueue_t *           _sq   = (syncqueue) ;      \
            typeof(**(newelem)) **  _elem = (newelem) ;        \
            int _err = insertlast_queue(                       \
                     genericcast_queue(_sq),                   \
                     (void**)_elem, elemsize) ;                \
            _sq->nrelements += (_err == 0) ;                   \
            _err ;                                             \
         }))

/* define: addtofreelist_syncqueue
 * Implements <syncqueue_t.addtofreelist_syncqueue>. */
#define addtofreelist_syncqueue(syncqueue, freelist, elem)     \
         do {                                                  \
            static_assert(                                     \
               sizeof(*(elem)) >= sizeof(dlist_node_t),        \
               "Check that element can hold a dlist_node_t") ; \
            insertfirst_dlist(                                 \
                  freelist, (dlist_node_t*)elem) ;             \
         } while (0)

/* define: compact_syncqueue
 * Implements <syncqueue_t.compact_syncqueue>. */
#define compact_syncqueue(syncqueue, elem_t, freelist, initmove_elem)   \
         ( __extension__ ({                                    \
            void (*_im) (elem_t * dest, elem_t * src) ;        \
            _im = (initmove_elem) ;                            \
            compact2_syncqueue(  (syncqueue), sizeof(elem_t),  \
                                 (freelist),                   \
                                 (void(*)(void*,void*))_im) ;  \
         }))

/* define: init_syncqueue
 * Implements <syncqueue_t.init_syncqueue>. */
#define init_syncqueue(syncqueue)   \
         ((void)(*(syncqueue) = (syncqueue_t) syncqueue_INIT))

/* define: len_syncqueue
 * Implements <syncqueue_t.len_syncqueue>. */
#define len_syncqueue(syncqueue)    \
         ((syncqueue)->nrelements)

/* define: queuefromaddr_syncqueue
 * Implements <syncqueue_t.queuefromaddr_syncqueue>. */
#define queuefromaddr_syncqueue(nodeaddr) \
         ((syncqueue_t*)queuefromaddr_queue(nodeaddr))

/* define: remove_syncqueue
 * Implements <syncqueue_t.remove_syncqueue>. */
#define remove_syncqueue(syncqueue, elem, initmove_elem)             \
         ( __extension__ ({                                          \
            syncqueue_t *  _sq        = (syncqueue) ;                \
            typeof(elem)   _elem      = (elem) ;                     \
            typeof(elem)   _lastentry = last_queue(                  \
                           genericcast_queue(_sq), sizeof(*_elem)) ; \
            if (_lastentry && _lastentry != _elem) {                 \
               (initmove_elem)(_elem, _lastentry) ;                  \
            }                                                        \
            int _err = removelast_queue(                             \
                        genericcast_queue(_sq), sizeof(*_elem)) ;    \
            _sq->nrelements -= (_err == 0) ;                         \
            _err ;                                                   \
         }))

/* define: removefirst_syncqueue
 * Implements <syncqueue_t.removefirst_syncqueue>. */
#define removefirst_syncqueue(syncqueue, elemsize)                   \
         ( __extension__ ({                                          \
            syncqueue_t * _sq = (syncqueue) ;                        \
            int _err = removefirst_queue(                            \
                        genericcast_queue(_sq), elemsize) ;          \
            _sq->nrelements -= (_err == 0) ;                         \
            _err ;                                                   \
         }))

/* define: removelast_syncqueue
 * Implements <syncqueue_t.removelast_syncqueue>. */
#define removelast_syncqueue(syncqueue, elemsize)                    \
         ( __extension__ ({                                          \
            syncqueue_t * _sq = (syncqueue) ;                        \
            int _err = removelast_queue(                             \
                        genericcast_queue(_sq), elemsize) ;          \
            _sq->nrelements -= (_err == 0) ;                         \
            _err ;                                                   \
         }))

#endif
