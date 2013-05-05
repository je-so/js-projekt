/* title: Queue

   Offers data structure to store fixed size elements in
   LIFO or FIFO order.

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

   file: C-kern/api/ds/inmem/queue.h
    Header file <Queue>.

   file: C-kern/ds/inmem/queue.c
    Implementation file <Queue impl>.
*/
#ifndef CKERN_DS_INMEM_QUEUE_HEADER
#define CKERN_DS_INMEM_QUEUE_HEADER

// forward
struct dlist_node_t ;

/* typedef: struct queue_t
 * Export <queue_t> into global namespace. */
typedef struct queue_t              queue_t ;

/* typedef: struct queue_iterator_t
 * Export <queue_iterator_t> into global namespace. */
typedef struct queue_iterator_t     queue_iterator_t ;

/* typedef: struct queue_page_t
 * Export <queue_page_t> into global namespace.
 * This is an opaque type which describes a block of memory
 * holding multiple number of elements. */
typedef struct queue_page_t         queue_page_t ;

// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_ds_inmem_queue
 * Test <queue_t> functionality. */
int unittest_ds_inmem_queue(void) ;
#endif


/* struct: queue_iterator_t
 * Iterates over elements contained in <queue_t>.
 * The iterator does not support removing or inserting during iteration.
 * If elements are inserted as last then they are iterated if they fit on
 * the last page else not. */
struct queue_iterator_t {
   /* variable: lastpage
    * The last memory page in the list of pages. */
   queue_page_t * lastpage ;
   /* variable: nextpage
    * The memory page which is iterated. */
   queue_page_t * nextpage ;
   /* variable: next_offset
    * The offset into <nextpage> which is the start address of the next node. */
   uint32_t       next_offset ;
   /* variable: end_offset
    * Last offset into <nextpage> which is start address of unused memory. */
   uint32_t       end_offset ;
   /* variable: nodesize
    * The size of the returned node. */
   uint16_t       nodesize ;
} ;

// group: lifetime

/* define: queue_iterator_INIT_FREEABLE
 * Static initializer. */
#define queue_iterator_INIT_FREEABLE   { 0, 0, 0, 0, 0 }

/* function: initfirst_queueiterator
 * Initializes an iterator for <queue_t>.
 * A nodesize of 0 results always in an iterator for an empty queue. */
int initfirst_queueiterator(/*out*/queue_iterator_t * iter, queue_t * queue, uint16_t nodesize) ;

/* function: free_queueiterator
 * Frees an iterator of <queue_t>. All members are set to 0. */
int free_queueiterator(queue_iterator_t * iter) ;

// group: iterate

/* function: next_queueiterator
 * Returns next iterated node.
 * The first call after <initfirst_queueiterator> returns the first queue element
 * if it is not empty.
 *
 * Returns:
 * true  - node contains a pointer to the next valid node in the queue.
 * false - There is no next node. The last element was already returned or the queue is empty. */
bool next_queueiterator(queue_iterator_t * iter, /*out*/void ** node) ;

/* function: nextskip_queueiterator
 * Skips extrasize bytes of the current node and returns true if possible.
 * The extrasize bytes must be on the same memory page as the current iterator
 * cause a node can not be split between pages.
 * Call this function if nodes stored in the queue are of different sizes.
 * A return value of false inidicates an error and nothing is done.
 * An error occurs only if on the current memory page are no more extrasize
 * bytes left after the current iterated node. */
bool nextskip_queueiterator(queue_iterator_t * iter, uint16_t extrasize) ;


/* struct: queue_t
 * Supports stacking of objects in FIFO or LIFO order at the same time.
 * You can insert new nodes as first or last element and remove nodes
 * from the front (first) or back (last) of the queue.
 *
 * The queue maintains a list of memory pages. On every memory page
 * several nodes are stored. The queue uses a fixed pagesize of 4096. */
struct queue_t {
   struct dlist_node_t * last ;
} ;

// group: lifetime

/* define: queue_INIT_FREEABLE
 * Static initializer. */
#define queue_INIT_FREEABLE      queue_INIT

/* define: queue_INIT
 * Static initializer. */
#define queue_INIT               { 0 }

/* function: init_queue
 * Sets al fields to 0. Even if it can never fail check return code in case
 * some preallocation is implemented. */
int init_queue(/*out*/queue_t * queue) ;

/* function: initmove_queue
 * Moves the object to another memory address.
 * After successful return dest is a copy of the old value of src
 * and src is set to <queue_INIT>.
 * This function has runtime O(n/NB) where NB is the number of nodes per memory page.
 * (You could copy queue_t directly but then <queuefromaddr_queue> returns the old
 * address. If you can live with that then copy directly). */
void initmove_queue(/*out*/queue_t * dest, queue_t * src) ;

/* function: free_queue
 * Free all memory pages even if they are not empty. */
int free_queue(queue_t * queue) ;

// group: query

/* function: isempty_queue
 * Returns true if queue contains no elements. */
bool isempty_queue(const queue_t * queue) ;

/* function: first_queue
 * Returns the first element or 0.
 * In case of 0 the queue is either empty or the first
 * memory page contains less than nodesize bytes. */
void * first_queue(const queue_t * queue, uint16_t nodesize) ;

/* function: last_queue
 * Returns the last element or 0.
 * In case of 0 the queue is either empty or the last
 * memory page contains less than nodesize bytes. */
void * last_queue(const queue_t * queue, uint16_t nodesize) ;

/* function: sizefirst_queue
 * Returns the number of bytes allocated on the first memory page.
 * Its the size of all nodes stored on the first memory page. */
size_t sizefirst_queue(const queue_t * queue) ;

/* function: sizelast_queue
 * Returns the number of bytes allocated on the last memory page.
 * Its the size of all nodes stored on the last memory page.
 * If only one memory page is allocated this value equals to <sizenodesfirstpage_queue>. */
size_t sizelast_queue(const queue_t * queue) ;

/* function: queuefromaddr_queue
 * Returns queue an inserted node with address nodeaddr belongs to. */
queue_t * queuefromaddr_queue(void * nodeaddr) ;

/* function: pagesizeinbytes_queue
 * Returns the static size of a memory page the queue uses. */
uint32_t pagesizeinbytes_queue(void) ;

// group: foreach-support

/* typedef: iteratortype_queue
 * Declaration to associate <queue_iterator_t> with <queue_t>. */
typedef queue_iterator_t      iteratortype_queue ;

/* typedef: iteratedtype_queue
 * Declaration to associate (void*) with <queue_t>. */
typedef void *                iteratedtype_queue ;

// group: update

/* function: insertfirst_queue
 * Allocates nodesize bytes on the first memory page.
 * The start address of the allocated node is returned in nodeaddr.
 * If the queue is empty or the first memory page does not have at least nodesize bytes free
 * a new memory page is allocated and made the new first one.
 * The error ENOMEM is returned in case no new memory page could be allocated.
 * The error EINVAL is returned in case nodesize > 512. */
int insertfirst_queue(queue_t * queue, /*out*/void ** nodeaddr, uint16_t nodesize) ;

/* function: insertlast_queue
 * Allocates nodesize bytes on the last memory page.
 * The start address of the allocated node is returned in nodeaddr.
 * If the queue is empty or the last memory page does not have at least nodesize bytes free
 * a new memory page is allocated and made the new last one.
 * The error ENOMEM is returned in case no new memory page could be allocated.
 * The error EINVAL is returned in case nodesize > 512. */
int insertlast_queue(queue_t * queue, /*out*/void ** nodeaddr, uint16_t nodesize) ;

/* function: removefirst_queue
 * Removes nodesize bytes from the first memory page.
 * If the queue is empty ENODATA is returned and nothing is done.
 * If the first memory page contains less than nodesize bytes
 * EOVERFLOW is returned and nothing is done. */
int removefirst_queue(queue_t * queue, uint16_t nodesize) ;

/* function: removelast_queue
 * Removes nodesize bytes from the last memory page.
 * If the queue is empty ENODATA is returned and nothing is done.
 * If the last memory page contains less than nodesize bytes
 * EOVERFLOW is returned and nothing is done. */
int removelast_queue(queue_t * queue, uint16_t nodesize) ;

/* function: resizelast_queue
 * Removes oldsize bytes from the last memory page and add newsize bytes.
 * After return nodeaddr contains the start address of the resized last entry.
 * If the queue is empty ENODATA is returned and nothing is done.
 * If the last memory page contains less than oldsize bytes
 * EOVERFLOW is returned and nothing is done.
 * The error EINVAL is returned in case newsize > 512.
 * If newsize does not fit on the last page a new page is allocated and nodeaddr
 * will contain a changed value else it keeps the same as the value returned in
 * the last call to <insertlast_queue>. The first min(oldsize, newsize) bytes
 * keep their value. If nodeaddr changed the old content has been copied to the new page. */
int resizelast_queue(queue_t * queue, /*out*/void ** nodeaddr, uint16_t oldsize, uint16_t newsize) ;

// group: generic

/* function: genericcast_queue
 * Tries to cast generic object queue into pointer to <queue_t>.
 * The generic object must have a last pointer as first member. */
queue_t * genericcast_queue(void * queue) ;

/* define: queue_IMPLEMENT
 * Generates interface of queue_t storing elements of type object_t.
 *
 * Parameter:
 * _fsuffix  - It is the suffix of the generated list interface functions, e.g. "init##_fsuffix".
 * object_t  - The type of object which can be stored and retrieved from the queue. */
void queue_IMPLEMENT(IDNAME _fsuffix, TYPENAME object_t) ;



/* struct: queue_page_t
 * Internal type used as header for memory pages. */
struct queue_page_t {
   /* variable: next
    * Next memory page in list of pages. */
   struct dlist_node_t * next ;
   /* variable: prev
    * PRev memory page in list of pages. */
   struct dlist_node_t * prev ;
   /* variable: queue
    * Queue this page belongs to. */
   queue_t *      queue ;
   /* variable: end_offset
    * Offset of end of last node relative to start address of this object.
    * Bytes from <end_offset> up to <pagesize>-1 are unused. */
   uint32_t       end_offset ;
   /* variable: start_offset
    * Offset of first node relative to start address of this object.
    * Bytes from start_offset up to <end_offset>-1 are in use.
    * Bytes from sizeof(queue_page_t) up to <start_offset>-1 are unused. */
   uint32_t       start_offset ;
} ;


// section: inline implementation

// group: queue_iterator_t

/* define: free_queueiterator
 * Implements <queue_iterator_t.free_queueiterator>. */
#define free_queueiterator(iter) \
         (*(iter) = (queue_iterator_t) queue_iterator_INIT_FREEABLE, 0)

/* define: initfirst_queueiterator
 * Implements <queue_iterator_t.initfirst_queueiterator>. */
#define initfirst_queueiterator(iter, queue, nodesize)   \
         ( __extension__ ({                              \
               typeof(iter)  _it = (iter) ;              \
               typeof(queue) _qu = (queue) ;             \
               uint16_t      _ns = (nodesize) ;          \
               if (_qu->last && _ns) {                   \
                  *_it = (typeof(*_it)) {                \
                        (queue_page_t*)                  \
                        _qu->last,                       \
                        (queue_page_t*)((queue_page_t*)  \
                        _qu->last)->next,                \
                        ((queue_page_t*)((queue_page_t*) \
                        _qu->last)->next)->start_offset, \
                        ((queue_page_t*)((queue_page_t*) \
                        _qu->last)->next)->end_offset,   \
                        _ns                              \
                     } ;                                 \
               } else {                                  \
                  *_it = (typeof(*_it)) {                \
                        0, 0, 0, 0, 1                    \
                     } ;                                 \
               }                                         \
               0 ;                                       \
         }))

/* define: next_queueiterator
 * Implements <queue_iterator_t.next_queueiterator>. */
#define next_queueiterator(iter, node)                   \
         ( __extension__ ({                              \
               bool         _isnext ;                    \
               typeof(iter) _it = (iter) ;               \
               typeof(node) _nd = (node) ;               \
               for (;;) {                                \
                  uint32_t _nextoff = _it->next_offset   \
                                    + _it->nodesize ;    \
                  if (_nextoff <= _it->end_offset) {     \
                     *_nd = (uint8_t*)_it->nextpage      \
                          + _it->next_offset ;           \
                     _it->next_offset = _nextoff ;       \
                     _isnext = true ;                    \
                     break ;                             \
                  }                                      \
                  if (_it->nextpage == _it->lastpage) {  \
                     _isnext = false ;                   \
                     break ;                             \
                  }                                      \
                  _it->nextpage    = (queue_page_t*)     \
                        _it->nextpage->next ;            \
                  _it->next_offset =                     \
                        _it->nextpage->start_offset ;    \
                  _it->end_offset  =                     \
                        _it->nextpage->end_offset ;      \
               }                                         \
               _isnext ;                                 \
         }))

/* define: nextskip_queueiterator
 * Implements <queue_iterator_t.nextskip_queueiterator>. */
#define nextskip_queueiterator(iter, extrasize)          \
         ( __extension__ ({                              \
               bool         _isskip ;                    \
               typeof(iter) _it = (iter) ;               \
               uint16_t     _es = (extrasize) ;          \
               uint32_t _nextoff = _it->next_offset      \
                                 + _es ;                 \
               if (_nextoff <= _it->end_offset) {        \
                  _it->next_offset = _nextoff ;          \
                  _isskip = true ;                       \
               } else {                                  \
                  _isskip = false ;                      \
               }                                         \
               _isskip ;                                 \
         }))

// group: queue_t

/* define: first_queue
 * Implements <queue_t.first_queue>. */
#define first_queue(queue, nodesize) \
         ( __extension__ ({                              \
               typeof(queue)  _qu   = (queue) ;          \
               uint16_t       _ns   = (nodesize) ;       \
               void *         _node = 0 ;                \
               queue_page_t * _first ;                   \
               if (_qu->last) {                          \
                  _first = (queue_page_t*)               \
                     ((queue_page_t*)_qu->last)->next ;  \
                  if ((_first->end_offset                \
                       - _first->start_offset) >= _ns) { \
                     _node = (uint8_t*)_first            \
                           + _first->start_offset ;      \
                  }                                      \
               }                                         \
               _node ;                                   \
         }))

/* define: genericcast_queue
 * Implements <queue_t.genericcast_queue>. */
#define genericcast_queue(queue)                         \
         ( __extension__ ({                              \
            static_assert(                               \
                  sizeof((queue)->last)                  \
                  == sizeof(((queue_t*)0)->last)         \
                  && offsetof(typeof(*(queue)), last)    \
                     == offsetof(queue_t, last)          \
                  && (typeof((queue)->last))0            \
                  == (struct dlist_node_t*)0,            \
                  "ensure compatible structure"          \
            ) ;                                          \
            (queue_t*) (queue) ;                         \
         }))

/* define: init_queue
 * Implements <queue_t.init_queue>. */
#define init_queue(queue) \
         (*(queue) = (queue_t) queue_INIT, 0)

/* define: isempty_queue
 * Implements <queue_t.isempty_queue>. */
#define isempty_queue(queue) \
         (0 == (queue)->last)

/* define: last_queue
 * Implements <queue_t.last_queue>. */
#define last_queue(queue, nodesize) \
         ( __extension__ ({                              \
               typeof(queue)  _qu   = (queue) ;          \
               uint16_t       _ns   = (nodesize) ;       \
               void *         _node = 0 ;                \
               queue_page_t * _last ;                    \
               if (_qu->last) {                          \
                  _last = (queue_page_t*)_qu->last ;     \
                  if ((_last->end_offset                 \
                       - _last->start_offset) >= _ns) {  \
                     _node = (uint8_t*)_last             \
                           + _last->end_offset           \
                           - _ns ;                       \
                  }                                      \
               }                                         \
               _node ;                                   \
         }))

/* define: pagesizeinbytes_queue
 * Implements <queue_t.pagesizeinbytes_queue>. */
#define pagesizeinbytes_queue()  \
         (4096u)

/* define: queuefromaddr_queue
 * Implements <queue_t.queuefromaddr_queue>. */
#define queuefromaddr_queue(nodeaddr) \
         (((queue_page_t*)((uintptr_t)(nodeaddr) & ~((uintptr_t)pagesizeinbytes_queue()-1u)))->queue)

/* define: sizefirst_queue
 * Implements <queue_t.sizefirst_queue>. */
#define sizefirst_queue(queue)                           \
         ( __extension__ ({                              \
               typeof(queue)  _qu   = (queue) ;          \
               size_t         _size ;                    \
               queue_page_t * _first ;                   \
               if (_qu->last) {                          \
                  _first = (queue_page_t*)               \
                     ((queue_page_t*)_qu->last)->next ;  \
                  _size  = _first->end_offset            \
                         - _first->start_offset ;        \
               } else {                                  \
                  _size = 0 ;                            \
               }                                         \
               _size ;                                   \
         }))

/* define: sizelast_queue
 * Implements <queue_t.sizelast_queue>. */
#define sizelast_queue(queue)                            \
         ( __extension__ ({                              \
               typeof(queue)  _qu   = (queue) ;          \
               size_t         _size ;                    \
               queue_page_t * _last ;                    \
               if (_qu->last) {                          \
                  _last = (queue_page_t*)_qu->last ;     \
                  _size = _last->end_offset              \
                        - _last->start_offset ;          \
               } else {                                  \
                  _size = 0 ;                            \
               }                                         \
               _size ;                                   \
         }))

/* define: queue_IMPLEMENT
 * Implements <queue_t.queue_IMPLEMENT>. */
#define queue_IMPLEMENT(_fsuffix, object_t) \
   typedef queue_iterator_t   iteratortype##_fsuffix ;       \
   typedef object_t *         iteratedtype##_fsuffix ;       \
   static inline int  initfirst##_fsuffix##iterator(/*out*/queue_iterator_t * iter, queue_t * queue) __attribute__ ((always_inline)) ; \
   static inline int  free##_fsuffix##iterator(queue_iterator_t * iter) __attribute__ ((always_inline)) ; \
   static inline bool next##_fsuffix##iterator(queue_iterator_t * iter, object_t ** node) __attribute__ ((always_inline)) ; \
   static inline int  init##_fsuffix(/*out*/queue_t * queue) __attribute__ ((always_inline)) ; \
   static inline void initmove##_fsuffix(/*out*/queue_t * dest, queue_t * src) __attribute__ ((always_inline)) ; \
   static inline int  free##_fsuffix(queue_t * queue) __attribute__ ((always_inline)) ; \
   static inline bool isempty##_fsuffix(const queue_t * queue) __attribute__ ((always_inline)) ; \
   static inline object_t * first##_fsuffix(const queue_t * queue) __attribute__ ((always_inline)) ; \
   static inline object_t * last##_fsuffix(const queue_t * queue) __attribute__ ((always_inline)) ; \
   static inline size_t sizefirst##_fsuffix(const queue_t * queue) __attribute__ ((always_inline)) ; \
   static inline size_t sizelast##_fsuffix(const queue_t * queue) __attribute__ ((always_inline)) ; \
   static inline int insertfirst##_fsuffix(queue_t * queue,/*out*/object_t ** new_node) __attribute__ ((always_inline)) ; \
   static inline int insertlast##_fsuffix(queue_t * queue,/*out*/object_t ** new_node) __attribute__ ((always_inline)) ; \
   static inline int removefirst##_fsuffix(queue_t * queue) __attribute__ ((always_inline)) ; \
   static inline int removelast##_fsuffix(queue_t * queue) __attribute__ ((always_inline)) ; \
   static inline int init##_fsuffix(/*out*/queue_t * queue) { \
      return init_queue(queue) ; \
   } \
   static inline void initmove##_fsuffix(/*out*/queue_t * dest, queue_t * src) { \
      initmove_queue(dest, src) ; \
   } \
   static inline int free##_fsuffix(queue_t * queue) { \
      return free_queue(queue) ; \
   } \
   static inline bool isempty##_fsuffix(const queue_t * queue) { \
      return isempty_queue(queue) ; \
   } \
   static inline object_t * first##_fsuffix(const queue_t * queue) { \
      return first_queue(queue, sizeof(object_t)) ; \
   } \
   static inline object_t * last##_fsuffix(const queue_t * queue) { \
      return last_queue(queue, sizeof(object_t)) ; \
   } \
   static inline size_t sizefirst##_fsuffix(const queue_t * queue) { \
      return sizefirst_queue(queue) ; \
   } \
   static inline size_t sizelast##_fsuffix(const queue_t * queue) { \
      return sizelast_queue(queue) ; \
   } \
   static inline int insertfirst##_fsuffix(queue_t * queue,/*out*/object_t ** new_node) { \
      return insertfirst_queue(queue, (void**)new_node, sizeof(object_t)) ; \
   } \
   static inline int insertlast##_fsuffix(queue_t * queue,/*out*/object_t ** new_node) { \
      return insertlast_queue(queue, (void**)new_node, sizeof(object_t)) ; \
   } \
   static inline int removefirst##_fsuffix(queue_t * queue) { \
      return removefirst_queue(queue, sizeof(object_t)) ; \
   } \
   static inline int removelast##_fsuffix(queue_t * queue) { \
      return removelast_queue(queue, sizeof(object_t)) ; \
   } \
   static inline int initfirst##_fsuffix##iterator(/*out*/queue_iterator_t * iter, queue_t * queue) { \
      return initfirst_queueiterator(iter, queue, sizeof(object_t)) ; \
   } \
   static inline int free##_fsuffix##iterator(queue_iterator_t * iter) { \
      return free_queueiterator(iter) ; \
   } \
   static inline bool next##_fsuffix##iterator(queue_iterator_t * iter, object_t ** node) { \
      return next_queueiterator(iter, (void**)node) ; \
   }


#endif
