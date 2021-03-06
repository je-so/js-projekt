/* title: Queue

   Offers data structure to store fixed size elements in
   LIFO or FIFO order.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
struct dlist_node_t;

// === exported types
struct queue_t;
struct queue_iterator_t;
struct queue_page_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_ds_inmem_queue
 * Test <queue_t> functionality. */
int unittest_ds_inmem_queue(void);
#endif


/* struct: queue_iterator_t
 * Iterates over elements contained in <queue_t>.
 * The iterator allows to remove and insert before the current position, i.e.
 * the forward iterator <initfirst_queueiterator> supports <removefirst_queue> and
 * <insertfirst_queue>, the backward iterator <initlast_queueiterator> supports <removelast_queue> and
 * <insertlast_queue>.
 *
 * Removing the current element is supported by the iterator.
 *
 * Never try to remove elements which are iterted over in the future.
 * The behaviour is undefined. Inserting "future" elements are possibly not iterated over. */
typedef struct queue_iterator_t {
   /* variable: first
    * Zeigt auf erstes Element. */
   uint8_t *      first;
   /* variable: last
    * Zeigt auf letztes Element. */
   uint8_t *      last;
   /* variable: fpage
    * Die Speicherseite, welche <page> folgt. Je nach Laufrichtung des Iterators die
    * vorherige oder nächste Seite. */
   struct queue_page_t * fpage;
   /* variable: endpage
    * Letzte zu iterierende Seite der Queue. */
   struct queue_page_t * endpage;
   /* variable: nodesize
    * The size of the returned node. */
   uint16_t       nodesize;
} queue_iterator_t;

// group: lifetime

/* define: queue_iterator_FREE
 * Static initializer. */
#define queue_iterator_FREE { 0, 0, 0, 0, 0 }

/* function: initfirst_queueiterator
 * Initializes an iterator for <queue_t>.
 * Returns ENODATA in case parameter nodesize is 0, or queue is empty, or
 * first page of the queue contains less than nodesize bytes. */
static inline int initfirst_queueiterator(/*out*/queue_iterator_t * iter, struct queue_t *queue, uint16_t nodesize);

/* function: initlast_queueiterator
 * Initializes a reverse iterator for <queue_t>.
 * Returns ENODATA in case parameter nodesize is 0, or queue is empty, or
 * last page of the queue contains less than nodesize bytes. */
static inline int initlast_queueiterator(/*out*/queue_iterator_t * iter, struct queue_t *queue, uint16_t nodesize);

/* function: free_queueiterator
 * Does nothing! There are no allocated resources */
int free_queueiterator(queue_iterator_t * iter);

// group: iterate

/* function: next_queueiterator
 * Returns next iterated node.
 * The first call after a valid call to <initfirst_queueiterator> returns the first queue element.
 *
 * The iterator stops if it encounters a page with less than nodesize bytes (see <initfirst_queueiterator>).
 *
 * Returns:
 * true  - node contains a pointer to the next valid node in the queue.
 * false - There is no next node. The last element was already returned. */
bool next_queueiterator(queue_iterator_t * iter, /*out*/void ** node);

/* function: nextskip_queueiterator
 * Skips extrasize bytes after the current node and returns true if possible.
 * The extrasize bytes must be on the same memory page as the current iterator
 * cause a node can not be split between pages.
 * Call this function if nodes stored in the queue are of different sizes.
 * A return value of false indicates an error and nothing is done.
 * An error occurs only if there are no more extrasize bytes left
 * after the current node on the same memory page. */
bool nextskip_queueiterator(queue_iterator_t * iter, uint16_t extrasize);

/* function: prev_queueiterator
 * Returns previous iterated node.
 * The first call after a valid call to <initlast_queueiterator> returns the last queue element.
 *
 * The iterator stops if it encounters a page with less than nodesize bytes (see <initlast_queueiterator>).
 *
 * Returns:
 * true  - node contains a pointer to the prev. valid node in the queue.
 * false - There is no previous node. The first element was already returned. */
bool prev_queueiterator(queue_iterator_t * iter, /*out*/void ** node);


/* struct: queue_t
 * Supports stacking of objects in FIFO or LIFO order at the same time.
 * You can insert new nodes as first or last element and remove nodes
 * from the front (first) or back (last) of the queue.
 *
 * The queue maintains a list of memory pages. On every memory page
 * several nodes are stored. The queue uses a fixed pagesize of 4096. */
typedef struct queue_t {
   struct
   dlist_node_t * last;
   /* variable: pagesize
    * Kodiert Größe in Bytes einer <queue_page_t> als enum. */
   uint8_t        pagesize;
} queue_t;

// group: lifetime

/* define: queue_FREE
 * Static initializer. */
#define queue_FREE { 0,  0 }

/* define: queue_INIT
 * Static initializer. */
#define queue_INIT { 0,  4/*4096 bytes pagesize*/ }

/* function: init_queue
 * Initialisiert queue. Kein Speicher wird allokiert.
 *
 * Returns:
 * 0      - pagesize supported.
 * EINVAL - pagesize not in [256, 1024, 4096, 16384] */
int init_queue(/*out*/queue_t *queue, size_t pagesize);

/* function: initmove_queue
 * Moves the object to another memory address.
 * After successful return dest is a copy of the old value of src
 * and src is set to <queue_INIT>.
 * This function has runtime O(n/NB) where NB is the number of nodes per memory page.
 * (You could copy queue_t directly but then <castPaddr_queue> returns the old
 * address. If you can live with that then copy directly). */
void initmove_queue(/*out*/queue_t * dest, queue_t * src);

/* function: free_queue
 * Free all memory pages even if they are not empty. */
int free_queue(queue_t *queue);

// group: query

/* function: pagesize_queue
 * Return size in bytes of internal used memory pages. See <queue_page_t>. */
uint16_t pagesize_queue(const queue_t *queue);

/* function: maxelemsize_queue
 * Returns maximum element size in bytes of supported by queues which uses pagesize as its a page size.
 * Each element with maximum size needs to be stored on a separate memory page. */
static inline uint16_t maxelemsize_queue(const uint16_t pagesize);

/* function: isempty_queue
 * Return true if queue contains no elements. */
bool isempty_queue(const queue_t *queue);

/* function: isfree_queue
 * Return true if queue is freed. */
bool isfree_queue(const queue_t *queue);

/* function: first_queue
 * Returns the first element or 0.
 * In case of 0 the queue is either empty or the first
 * memory page contains less than nodesize bytes. */
void * first_queue(const queue_t *queue, uint16_t nodesize);

/* function: last_queue
 * Returns the last element or 0.
 * In case of 0 the queue is either empty or the last
 * memory page contains less than nodesize bytes. */
void * last_queue(const queue_t *queue, uint16_t nodesize);

/* function: sizefirst_queue
 * Returns the number of bytes allocated on the first memory page.
 * Its the size of all nodes stored on the first memory page. */
size_t sizefirst_queue(const queue_t *queue);

/* function: sizelast_queue
 * Returns the number of bytes allocated on the last memory page.
 * Its the size of all nodes stored on the last memory page.
 * If only one memory page is allocated this value equals to <sizenodesfirstpage_queue>. */
size_t sizelast_queue(const queue_t *queue);

/* function: sizebytes_queue
 * Returns the number of bytes allocated on all memory pages.
 * O(n): This operation needs to iterate over the list of all allocated pages. */
size_t sizebytes_queue(const queue_t *queue);

/* function: castPaddr_queue
 * Converts address of an inserted node into queue it belongs to. */
queue_t * castPaddr_queue(void * nodeaddr, uint16_t pagesize);

/* function: defaultpagesize_queue
 * Returns the default size of a memory page the queue uses.
 * Use it as argument in the constructor. */
uint16_t defaultpagesize_queue(void);

// group: foreach-support

/* typedef: iteratortype_queue
 * Declaration to associate <queue_iterator_t> with <queue_t>. */
typedef queue_iterator_t      iteratortype_queue;

/* typedef: iteratedtype_queue
 * Declaration to associate (void*) with <queue_t>. */
typedef void *                iteratedtype_queue;

// group: update

/* function: insertfirst_queue
 * Allocates nodesize bytes on the first memory page.
 * The start address of the allocated node is returned in nodeaddr.
 * If the queue is empty or the first memory page does not have at least nodesize bytes free
 * a new memory page is allocated and made the new first one.
 * The error ENOMEM is returned in case no new memory page could be allocated.
 * The error EINVAL is returned in case nodesize > 512. */
int insertfirst_queue(queue_t *queue, uint16_t nodesize, /*out*/void ** nodeaddr);

/* function: insertlast_queue
 * Allocates nodesize bytes on the last memory page.
 * The start address of the allocated node is returned in nodeaddr.
 * If the queue is empty or the last memory page does not have at least nodesize bytes free
 * a new memory page is allocated and made the new last one.
 * The error ENOMEM is returned in case no new memory page could be allocated.
 * The error EINVAL is returned in case nodesize > 512. */
int insertlast_queue(queue_t *queue, uint16_t nodesize, /*out*/void ** nodeaddr);

/* function: removefirst_queue
 * Removes nodesize bytes from the first memory page.
 * If the queue is empty ENODATA is returned and nothing is done.
 * If the first memory page contains less than nodesize bytes
 * EOVERFLOW is returned and nothing is done. */
int removefirst_queue(queue_t *queue, uint16_t nodesize);

/* function: removelast_queue
 * Removes nodesize bytes from the last memory page.
 * If the queue is empty ENODATA is returned and nothing is done.
 * If the last memory page contains less than nodesize bytes
 * EOVERFLOW is returned and nothing is done. */
int removelast_queue(queue_t *queue, uint16_t nodesize);

/* function: removeall_queue
 * Removes all stored nodes at once.
 * If the queue is empty nothing is done. */
int removeall_queue(queue_t*queue);

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
int resizelast_queue(queue_t *queue, /*out*/void ** nodeaddr, uint16_t oldsize, uint16_t newsize);

// group: generic

/* function: cast_queue
 * Tries to cast generic object queue into pointer to <queue_t>.
 * The generic object must have a last pointer as first member. */
queue_t * cast_queue(void *queue);

/* define: queue_IMPLEMENT
 * Generates interface of queue_t storing elements of type object_t.
 *
 * Parameter:
 * _fsuffix  - It is the suffix of the generated list interface functions, e.g. "init##_fsuffix".
 * object_t  - The type of object which can be stored and retrieved from the queue. */
void queue_IMPLEMENT(IDNAME _fsuffix, TYPENAME object_t);



/* struct: queue_page_t
 * Internal type used as header for memory pages. */
typedef struct queue_page_t {
   /* variable: next
    * Next memory page in list of pages. */
   struct dlist_node_t *next;
   /* variable: prev
    * PRev memory page in list of pages. */
   struct dlist_node_t *prev;
   /* variable: queue
    * queue this page belongs to. */
   queue_t             *queue;
   /* variable: end_offset
    * Offset of end of last node relative to start address of this object.
    * Bytes from <end_offset> up to <pagesize>-1 are unused. */
   uint16_t             end_offset;
   /* variable: start_offset
    * Offset of first node relative to start address of this object.
    * Bytes from start_offset up to <end_offset>-1 are in use.
    * Bytes from sizeof(queue_page_t) up to <start_offset>-1 are unused. */
   uint16_t             start_offset;
} queue_page_t;


// section: inline implementation

// group: queue_iterator_t

/* define: free_queueiterator
 * Implements <queue_iterator_t.free_queueiterator>. */
#define free_queueiterator(iter) \
         (0)

/* define: initfirst_queueiterator
 * Implements <queue_iterator_t.initfirst_queueiterator>. */
static inline int initfirst_queueiterator(/*out*/queue_iterator_t * iter, struct queue_t *queue, uint16_t nodesize)
{
         queue_page_t* lp = (queue_page_t*)queue->last;
         queue_page_t* fp;
         if (queue->last && nodesize
            && (fp = (queue_page_t*)lp->next)
            && nodesize <= (fp->end_offset - fp->start_offset)) {
               *iter = (queue_iterator_t) {
                     (uint8_t*)((uintptr_t)fp + fp->start_offset),
                     (uint8_t*)((uintptr_t)fp + fp->end_offset - nodesize),
                     fp == lp ? 0 : (queue_page_t*)fp->next,
                     lp,
                     nodesize
               };
               return 0;
         } else {
            return ENODATA;
         }
}

/* define: initlast_queueiterator
 * Implements <queue_iterator_t.initlast_queueiterator>. */
static inline int initlast_queueiterator(/*out*/queue_iterator_t * iter, struct queue_t *queue, uint16_t nodesize)
{
         queue_page_t* lp = (queue_page_t*)queue->last;
         if (queue->last && nodesize
            && nodesize <= (lp->end_offset - lp->start_offset)) {
            queue_page_t* fp = (queue_page_t*)lp->next;
            *iter = (queue_iterator_t) {
                  (uint8_t*)((uintptr_t)lp + lp->start_offset + nodesize),
                  (uint8_t*)((uintptr_t)lp + lp->end_offset),
                  fp == lp ? 0 : (queue_page_t*)lp->prev,
                  fp,
                  nodesize
            };
            return 0;
         } else {
            return ENODATA;
         }
}

/* define: next_queueiterator
 * Implements <queue_iterator_t.next_queueiterator>. */
#define next_queueiterator(iter, node) \
         ( __extension__ ({                              \
               bool         _isnext;                     \
               typeof(iter) _it = (iter);                \
               typeof(node) _nd = (node);                \
               for (;;) {                                \
                  if (_it->last >= _it->first) {         \
                     *_nd = (void*) _it->first;          \
                     _it->first += _it->nodesize;        \
                     _isnext = true;                     \
                     break;                              \
                  }                                      \
                  queue_page_t* _pg = _it->fpage;        \
                  if ( ! _pg                             \
                       || (_it->nodesize > _pg->end_offset -_pg->start_offset)) { \
                     _isnext = false;                    \
                     break;                              \
                  }                                      \
                  _it->first = (uint8_t*)((uintptr_t)_pg + _pg->start_offset); \
                  _it->last  = (uint8_t*)((uintptr_t)_pg + _pg->end_offset - _it->nodesize); \
                  _it->fpage = _pg==_it->endpage ? 0 : (queue_page_t*)_pg->next; \
               }                                         \
               _isnext;                                  \
         }))

/* define: nextskip_queueiterator
 * Implements <queue_iterator_t.nextskip_queueiterator>. */
#define nextskip_queueiterator(iter, extrasize)    \
         ( __extension__ ({                        \
               bool         _isskip;               \
               typeof(iter) _it = (iter);          \
               uint16_t     _es = (extrasize);     \
               if (_it->last - _it->first          \
                   + _it->nodesize >= _es) {       \
                  _it->first += _es;               \
                  _isskip = true;                  \
               } else {                            \
                  _isskip = false;                 \
               }                                   \
               _isskip;                            \
         }))

/* define: prev_queueiterator
 * Implements <queue_iterator_t.prev_queueiterator>. */
#define prev_queueiterator(iter, node) \
         ( __extension__ ({                              \
               bool         _isprev;                     \
               typeof(iter) _it = (iter);                \
               typeof(node) _nd = (node);                \
               for (;;) {                                \
                  if (_it->last >= _it->first) {         \
                     _it->last -= _it->nodesize;         \
                     *_nd = (void*) _it->last;           \
                     _isprev = true;                     \
                     break;                              \
                  }                                      \
                  queue_page_t* _pg = _it->fpage;        \
                  if ( ! _pg                             \
                       || (_it->nodesize > _pg->end_offset -_pg->start_offset)) { \
                     _isprev = false;                    \
                     break;                              \
                  }                                      \
                  _it->first = (uint8_t*)((uintptr_t)_pg + _pg->start_offset + _it->nodesize); \
                  _it->last  = (uint8_t*)((uintptr_t)_pg + _pg->end_offset); \
                  _it->fpage = _pg==_it->endpage ? 0 : (queue_page_t*)_pg->prev; \
               }                                         \
               _isprev;                                  \
         }))

// group: queue_t

/* define: first_queue
 * Implements <queue_t.first_queue>. */
#define first_queue(queue, nodesize) \
         ( __extension__ ({                              \
               typeof(queue)  _qu   = (queue);           \
               uint16_t       _ns   = (nodesize);        \
               void *         _node = 0;                 \
               queue_page_t * _first;                    \
               if (_qu->last) {                          \
                  _first = (queue_page_t*)               \
                     ((queue_page_t*)_qu->last)->next;   \
                  if ((_first->end_offset                \
                       - _first->start_offset) >= _ns) { \
                     _node = (uint8_t*)_first            \
                           + _first->start_offset;       \
                  }                                      \
               }                                         \
               _node;                                    \
         }))

/* define: cast_queue
 * Implements <queue_t.cast_queue>. */
#define cast_queue(queue) \
         ( __extension__ ({                      \
            typeof(queue) _q = (queue);          \
            static_assert(                       \
               &(_q->last)                       \
               == &((queue_t*) _q)->last         \
               && &(_q->pagesize)                \
                  == &((queue_t*) _q)->pagesize, \
                  "ensure compatible structure"  \
            );                                   \
            (queue_t*) _q;                       \
         }))

/* define: isempty_queue
 * Implements <queue_t.isempty_queue>. */
#define isempty_queue(queue) \
         (0 == (queue)->last)

/* define: isfree_queue
 * Implements <queue_t.isfree_queue>. */
#define isfree_queue(queue) \
         (0 == (queue)->last)

/* define: last_queue
 * Implements <queue_t.last_queue>. */
#define last_queue(queue, nodesize) \
         ( __extension__ ({                              \
               typeof(queue)  _qu   = (queue);           \
               uint16_t       _ns   = (nodesize);        \
               void *         _node = 0;                 \
               queue_page_t * _last;                     \
               if (_qu->last) {                          \
                  _last = (queue_page_t*)_qu->last;      \
                  if ((_last->end_offset                 \
                       - _last->start_offset) >= _ns) {  \
                     _node = (uint8_t*)_last             \
                           + _last->end_offset           \
                           - _ns;                        \
                  }                                      \
               }                                         \
               _node;                                    \
         }))

/* define: pagesize_queue
 * Implements <queue_t.pagesize_queue>. */
#define pagesize_queue(queue) \
         ((uint16_t)(256u << (queue)->pagesize))

/* define: maxelemsize_queue
 * Implements <queue_t.maxelemsize_queue>. */
static inline uint16_t maxelemsize_queue(const uint16_t pagesize)
{
         return (uint16_t) (pagesize - sizeof(queue_page_t));
}

/* define: defaultpagesize_queue
 * Implements <queue_t.defaultpagesize_queue>. */
#define defaultpagesize_queue()  \
         ((uint16_t)4096)

/* define: castPaddr_queue
 * Implements <queue_t.castPaddr_queue>. */
#define castPaddr_queue(nodeaddr, pagesize) \
         (((queue_page_t*)((uintptr_t)(nodeaddr) & ~((uintptr_t)(pagesize)-1u)))->queue)

/* define: sizefirst_queue
 * Implements <queue_t.sizefirst_queue>. */
#define sizefirst_queue(queue) \
         ( __extension__ ({                              \
               typeof(queue)  _qu   = (queue);           \
               size_t         _size;                     \
               queue_page_t * _first;                    \
               if (_qu->last) {                          \
                  _first = (queue_page_t*)               \
                     ((queue_page_t*)_qu->last)->next;   \
                  _size  = (size_t)_first->end_offset    \
                         - _first->start_offset;         \
               } else {                                  \
                  _size = 0;                             \
               }                                         \
               _size;                                    \
         }))

/* define: sizelast_queue
 * Implements <queue_t.sizelast_queue>. */
#define sizelast_queue(queue) \
         ( __extension__ ({                           \
               typeof(queue)  _qu   = (queue);        \
               size_t         _size;                  \
               queue_page_t * _last;                  \
               if (_qu->last) {                       \
                  _last = (queue_page_t*)_qu->last;   \
                  _size = (size_t)_last->end_offset   \
                        - _last->start_offset;        \
               } else {                               \
                  _size = 0;                          \
               }                                      \
               _size;                                 \
         }))

/* define: queue_IMPLEMENT
 * Implements <queue_t.queue_IMPLEMENT>. */
#define queue_IMPLEMENT(_fsuffix, object_t) \
   typedef queue_iterator_t   iteratortype##_fsuffix;       \
   typedef object_t *         iteratedtype##_fsuffix;       \
   static inline int init##_fsuffix(/*out*/queue_t *queue, uint32_t pagesize) { \
      return init_queue(queue, pagesize); \
   } \
   static inline void initmove##_fsuffix(/*out*/queue_t * dest, queue_t * src) { \
      initmove_queue(dest, src); \
   } \
   static inline int free##_fsuffix(queue_t *queue) { \
      return free_queue(queue); \
   } \
   static inline uint16_t pagesize##_fsuffix(const queue_t *queue) { \
      return pagesize_queue(queue); \
   } \
   static inline bool isempty##_fsuffix(const queue_t *queue) { \
      return isempty_queue(queue); \
   } \
   static inline bool isfree##_fsuffix(const queue_t *queue) { \
      return isfree_queue(queue); \
   } \
   static inline object_t * first##_fsuffix(const queue_t *queue) { \
      return first_queue(queue, sizeof(object_t)); \
   } \
   static inline object_t * last##_fsuffix(const queue_t *queue) { \
      return last_queue(queue, sizeof(object_t)); \
   } \
   static inline size_t sizefirst##_fsuffix(const queue_t *queue) { \
      return sizefirst_queue(queue); \
   } \
   static inline size_t sizelast##_fsuffix(const queue_t *queue) { \
      return sizelast_queue(queue); \
   } \
   static inline size_t sizebytes##_fsuffix(const queue_t *queue) { \
      return sizebytes_queue(queue); \
   } \
   static inline int insertfirst##_fsuffix(queue_t *queue,/*out*/object_t ** new_node) { \
      return insertfirst_queue(queue, sizeof(object_t), (void**)new_node); \
   } \
   static inline int insertlast##_fsuffix(queue_t *queue,/*out*/object_t ** new_node) { \
      return insertlast_queue(queue, sizeof(object_t), (void**)new_node); \
   } \
   static inline int removefirst##_fsuffix(queue_t *queue) { \
      return removefirst_queue(queue, sizeof(object_t)); \
   } \
   static inline int removelast##_fsuffix(queue_t *queue) { \
      return removelast_queue(queue, sizeof(object_t)); \
   } \
   static inline int removeall##_fsuffix(queue_t *queue) { \
      return removeall_queue(queue); \
   } \
   static inline int initfirst##_fsuffix##iterator(/*out*/queue_iterator_t * iter, queue_t *queue) { \
      return initfirst_queueiterator(iter, queue, sizeof(object_t)); \
   } \
   static inline int initlast##_fsuffix##iterator(/*out*/queue_iterator_t * iter, queue_t *queue) { \
      return initlast_queueiterator(iter, queue, sizeof(object_t)); \
   } \
   static inline int free##_fsuffix##iterator(queue_iterator_t * iter) { \
      (void) iter; \
      return free_queueiterator(iter); \
   } \
   static inline bool next##_fsuffix##iterator(queue_iterator_t * iter, object_t ** node) { \
      return next_queueiterator(iter, (void**)node); \
   } \
   static inline bool prev##_fsuffix##iterator(queue_iterator_t * iter, object_t ** node) { \
      return prev_queueiterator(iter, (void**)node); \
   }


#endif
