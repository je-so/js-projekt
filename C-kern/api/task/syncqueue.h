/* title: SyncQueue

   Adapts type <queue_t> for use in <syncrun_t>.

   Includes:
   You need to include "C-kern/api/ds/inmem/queue.h" before calling the inline functions.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/task/syncqueue.h
    Header file <SyncQueue>.

   file: C-kern/task/syncqueue.c
    Implementation file <SyncQueue impl>.
*/
#ifndef CKERN_TASK_SYNCQUEUE_HEADER
#define CKERN_TASK_SYNCQUEUE_HEADER

// forward
struct dlist_t;
struct dlist_node_t;

/* typedef: struct syncqueue_t
 * Export <syncqueue_t> into global namespace. */
typedef struct syncqueue_t syncqueue_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_task_syncqueue
 * Test <syncqueue_t> functionality. */
int unittest_task_syncqueue(void);
#endif


/* struct: syncqueue_t
 * Internal type to store zero or more <syncthread_t> + additional information.
 * This object uses <queue_t> to implement its functionality.
 *
 * Extension of queue_t:
 * Use <cast_queue> to cast pointer to <syncqueue_t> into pointer to
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
   struct dlist_node_t *   last;
   /* variable: pagesize
    * This variables makes <syncqueue_t> convertible to type <queue_t>. */
   uint8_t                 pagesize;
   /* variable: qidx
    * Kennung der Queue. Wird vom Aufrufer zur Identifierung der Queue verwendet. */
   uint8_t                 qidx;
   /* variable: elemsize
    * Größe in Bytes der verwalteten Elemente. */
   uint16_t                elemsize;
   /* variable: size
    * Anzahl gespeicherter Elemente. */
   size_t                  size;
};

// group: lifetime

/* define: syncqueue_FREE
 * Static initializer. */
#define syncqueue_FREE \
         { 0, 0, 0, 0, 0 }

/* function: init_syncqueue
 * Initializes syncqueue. */
int init_syncqueue(/*out*/syncqueue_t * syncqueue, uint16_t elemsize, uint8_t qidx);

/* function: free_syncqueue
 * Frees all allocated memory pages and clears syncqueue.
 * For contained elements no free operation is called. */
int free_syncqueue(syncqueue_t * syncqueue);

// group: query

/* function: syncqueue_PAGESIZE
 * Definiert Bytegröße der Speicherseiten der Queue. */
#define syncqueue_PAGESIZE 1024

/* function: isfree_syncqueue
 * Returns true if syncqueue is equal to <syncqueue_FREE>. */
bool isfree_syncqueue(const syncqueue_t * syncqueue);

/* function: elemsize_syncqueue
 * Returns size of stored elements. */
uint16_t elemsize_syncqueue(const syncqueue_t * syncqueue);

/* function: idx_syncqueue
 * Returns queue index set during initialization with <init_syncqueue>. */
uint16_t idx_syncqueue(const syncqueue_t * syncqueue);

/* function: size_syncqueue
 * Returns the number of elements stored in the queue. */
size_t size_syncqueue(const syncqueue_t * syncqueue);

/* function: castPaddr_syncqueue
 * See <castPaddr_queue>. */
syncqueue_t * castPaddr_syncqueue(void * nodeaddr);

// group: update

/* function: preallocate_syncqueue
 * Preallocate an element which is returned in node.
 * The size of the queue is incremented by one.
 * */
int preallocate_syncqueue(syncqueue_t* syncqueue, /*out*/void** node);

/* function: removefirst_syncqueue
 * Removes an element from the queue by freeing syncqueue memory.
 * The size of the queue is decremented by one.
 * */
int removefirst_syncqueue(syncqueue_t * syncqueue);

/* function: removelast_syncqueue
 * Removes an element from the queue by freeing syncqueue memory.
 * The size of the queue is decremented by one.
 * */
int removelast_syncqueue(syncqueue_t * syncqueue);



// section: inline implementation

/* define: elemsize_syncqueue
 * Implements <syncqueue_t.elemsize_syncqueue>. */
#define elemsize_syncqueue(syncqueue)    \
         ((syncqueue)->elemsize)

/* define: idx_syncqueue
 * Implements <syncqueue_t.idx_syncqueue>. */
#define idx_syncqueue(syncqueue) \
         ((syncqueue)->qidx)

/* define: preallocate_syncqueue
 * Implements <syncqueue_t.preallocate_syncqueue>. */
#define preallocate_syncqueue(syncqueue, node) \
         ( __extension__ ({                  \
            int _err;                        \
            syncqueue_t * _sq = (syncqueue); \
            _err = insertlast_queue(         \
                        cast_queue(_sq),     \
                        _sq->elemsize,       \
                        node);               \
            _sq->size += (_err == 0);        \
            _err;                            \
         }))

/* define: castPaddr_syncqueue
 * Implements <syncqueue_t.castPaddr_syncqueue>. */
#define castPaddr_syncqueue(nodeaddr) \
         ((syncqueue_t*)castPaddr_queue(nodeaddr, syncqueue_PAGESIZE))

/* define: removefirst_syncqueue
 * Implements <syncqueue_t.removefirst_syncqueue>. */
#define removefirst_syncqueue(syncqueue) \
         ( __extension__ ({                  \
            syncqueue_t * _sq = (syncqueue); \
            int _err = removefirst_queue(    \
                        cast_queue(_sq),     \
                        _sq->elemsize);      \
            _sq->size -= (_err == 0);        \
            _err;                            \
         }))

/* define: removelast_syncqueue
 * Implements <syncqueue_t.removelast_syncqueue>. */
#define removelast_syncqueue(syncqueue) \
         ( __extension__ ({                  \
            syncqueue_t * _sq = (syncqueue); \
            int _err = removelast_queue(     \
                        cast_queue(_sq),     \
                        _sq->elemsize);      \
            _sq->size -= (_err == 0);        \
            _err;                            \
         }))

/* define: size_syncqueue
 * Implements <syncqueue_t.size_syncqueue>. */
#define size_syncqueue(syncqueue) \
         ((syncqueue)->size)

#endif
