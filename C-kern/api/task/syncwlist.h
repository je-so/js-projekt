/* title: SyncWaitlist

   Exports type <syncwlist_t> which implements a list of
   <syncevent_t> to support more than one waiting <syncthread_t>.

   >           ╭─────────╮
   >           | dlist_t | // double linked list
   >           ├─────────┤ // only used as dummy in implementation
   >       ┌───┼ last    |
   >       |   ╰─────────╯
   >       |              ┌──────────────────────┬┐
   >       |   ┌──────────or─────────┐           ||
   >  ╭────↓───↓──────╮      ╭───────↓─────────╮ ||
   >  | syncwlist_t   |      | wlistentry_t    | ||
   >  ├───────────────┤      ├─────────────────┤ ||
   >  | next         ─┼─────▸| next           ─┼─┘|
   >  | prev         ─┼─────▸| prev           ─┼──┘
   >  ╰───────────────╯      | syncevent_t     |
   >                         ╰─────────────────╯
   >  invariant:
   >  for all wlistentry_t x, y :
   >  (x.next==y) <==> (y.prev==x)
   >
   >  for all syncwlist_t w; wlistentry_t x, y :
   >  (w.next==x) <==> (x.prev==w)
   >  (w.prev==y) <==> (y.next==w)

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

   file: C-kern/api/task/syncwlist.h
    Header file <SyncWaitlist>.

   file: C-kern/task/syncwlist.c
    Implementation file <SyncWaitlist impl>.
*/
#ifndef CKERN_TASK_SYNCWLIST_HEADER
#define CKERN_TASK_SYNCWLIST_HEADER

// forward
struct dlist_node_t ;
struct syncevent_t ;
struct syncqueue_t ;

/* typedef: struct syncwlist_t
 * Export <syncwlist_t> into global namespace. */
typedef struct syncwlist_t                syncwlist_t ;

/* typedef: struct syncwlist_iterator_t
 * Export <syncwlist_iterator_t> into global namespace. */
typedef struct syncwlist_iterator_t       syncwlist_iterator_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_task_syncwlist
 * Test <syncwlist_t> functionality. */
int unittest_task_syncwlist(void) ;
#endif

/* struct: syncwlist_iterator_t
 * Offers iteration over content of <syncwlist_t>.
 * Does *not* support removal operation (<remove_syncwlist>) of any element.
 * The reason is that calling <remove_syncwlist> moves a single node
 * in the queue to fill the gap which may be the next node returned by this iterator. */
struct syncwlist_iterator_t {
   struct dlist_node_t *   next ;
   syncwlist_t *           wlist ;
} ;

// group: lifetime

/* define: syncwlist_iterator_INIT_FREEABLE
 * Static initializer. */
#define syncwlist_iterator_INIT_FREEABLE  { 0, 0 }

/* function: initfirst_syncwlistiterator
 * Initializes an iterator for <syncwlist_t>. */
int initfirst_syncwlistiterator(/*out*/syncwlist_iterator_t * iter, syncwlist_t * wlist) ;

/* function: free_syncwlistiterator
 * Frees an iterator of <syncwlist_t>. This is a no-op. */
int free_syncwlistiterator(syncwlist_iterator_t * iter) ;

// group: iterate

/* function: next_syncwlistiterator
 * Returns next contained event.
 * The first call after <initfirst_syncwlistiterator> returns the first contained event if the list is not empty.
 *
 * Returns:
 * true  - event contains a pointer to the next valid <syncevent_t> in <syncwlist_t>.
 * false - There is no next event. The last element was already returned or the list is empty. */
bool next_syncwlistiterator(syncwlist_iterator_t * iter, /*out*/struct syncevent_t ** event) ;


/* struct: syncwlist_t
 * Implements a double linked list of <syncevent_t> and stores them in a <syncqueue_t>.
 * Every <syncevent_t> can be connected to a waiting <syncthread_t>.
 * The nr of <syncevent_t> stored in the list is held in variable nrnodes.
 *
 * Invariant:
 * Some functions expect a pointer to <syncqueue_t> as a parameter.
 * This pointer should always point to the same queue and only nodes of list type <syncwlist_t>
 * should be stored in this queue. If a node is removed from a <syncwlist_t> the last node in the
 * queue is copied to the removed node and then the last node is removed instead. */
struct syncwlist_t {
   struct dlist_node_t *   next ;
   struct dlist_node_t *   prev ;
   size_t                  nrnodes ;
} ;

// group: lifetime

/* define: syncwlist_INIT_FREEABLE
 * Static initializer. */
#define syncwlist_INIT_FREEABLE           { 0, 0, 0 }

/* function: init_syncwlist
 * Initializes wlist to an empty list. */
void init_syncwlist(/*out*/syncwlist_t * wlist) ;

/* function: initmove_syncwlist
 * Initializes destwlist with content of scrwlist and clears srcwlist.
 * After the call destwilist contains all elements of srcwlist and srcwlist
 * is set to the empty list (<init_syncwlist>). */
void initmove_syncwlist(/*out*/syncwlist_t * destwlist, syncwlist_t * srcwlist) ;

/* function: free_syncwlist
 * Marks all listed nodes as free and removes them from the queue.
 * Waiting <syncwait_t> referenced by any stored <syncevent_t> are not changed.
 * After this operation all referenced <syncwait_t> contain invalid
 * <syncwait_t.event>. Therefore make sure that wlist is empty befroe calling free. */
int free_syncwlist(syncwlist_t * wlist, struct syncqueue_t * queue) ;

// group: query

/* function: isempty_syncwlist
 * Returns true if wlist contains no elements. */
bool isempty_syncwlist(const syncwlist_t * wlist) ;

/* function: isfree_syncwlist
 * Returns true if wlist equals <syncwlist_INIT_FREEABLE>. */
bool isfree_syncwlist(const syncwlist_t * wlist) ;

/* function: len_syncwlist
 * Returns the number of inserted events. */
size_t len_syncwlist(const syncwlist_t * wlist) ;

/* function: queue_syncwlist
 * Returns the queue of the first node.
 * If the list is empty 0 is returned.
 * Parameter queue is used to determine the pagesize of the queue.
 * Do not forget to include type <syncqueue_t> if you call this function - it is implemented as macro. */
struct syncqueue_t * queue_syncwlist(const syncwlist_t * wlist) ;

/* function: last_syncwlist
 * Returns pointer to last contained <syncevent_t>.
 * In case list is empty 0 is returned. */
struct syncevent_t * last_syncwlist(const syncwlist_t * wlist) ;

// group: foreach-support

/* typedef: iteratortype_syncwlist
 * Declaration to associate <syncwlist_iterator_t> with <syncwlist_t>. */
typedef syncwlist_iterator_t     iteratortype_syncwlist ;

/* typedef: iteratedtype_syncwlist
 * Declaration to associate <syncevent_t> with <syncwlist_t>. */
typedef struct syncevent_t *     iteratedtype_syncwlist ;

// group: update

/* function: insert_syncwlist
 * Allocates new wlist node and inserts it into wlist.
 * The memory is allocated from queue and the new node is inserted as last element into wlist.
 * The <syncevent_t> member of the node is initialized to <syncevent_INIT_FREEABLE> and a
 * reference to it is returned in newevent. The function returns ENOMEM in case of out of memory. */
int insert_syncwlist(syncwlist_t * wlist, struct syncqueue_t * queue, /*out*/struct syncevent_t ** newevent) ;

/* function: remove_syncwlist
 * Removes the first node in wlist.
 * The node is also removed from queue.
 * The error ENODATA (no error log) is returned in case wlist is empty.
 * The content of the removed <syncevent_t> is copied into removedevent
 * without accessing or changing the referenced <syncwait_t>.
 * After successful return the referenced <syncwait_t> contains an invalid reference
 * to a removed <syncevent_t>. */
int remove_syncwlist(syncwlist_t * wlist, struct syncqueue_t * queue, /*out*/struct syncevent_t * removedevent) ;

/* function: removeempty_syncwlist
 * Removes the last node if its <syncevent_t> has not waiting thread. */
int removeempty_syncwlist(syncwlist_t * wlist, struct syncqueue_t * queue) ;

/* function: transferfirst_syncwlist
 * Transfers the first node from fromwlist to towlist.
 * The first node of fromwlist is removed and inserted as last into towlist.
 * In case fromwlist is empty no error is returned and nothing is done. */
int transferfirst_syncwlist(syncwlist_t * towlist, syncwlist_t * fromwlist) ;

/* function: transferall_syncwlist
 * Transfers all nodes from fromwlist to towlist.
 * All nodes from fromwlist are removed and appended to towlist
 * starting at first node of fromwlist. This function is much faster
 * than repeatedly calling <transferfirst_syncwlist> until fromwlist is empty.
 * In case fromwlist is empty no error is returned and nothing is done. */
int transferall_syncwlist(syncwlist_t * towlist, syncwlist_t * fromwlist) ;



// section: inline implementation

// group: syncwlist_iterator_t

/* define: free_syncwlistiterator
 * Implements <syncwlist_iterator_t.free_syncwlistiterator>. */
#define free_syncwlistiterator(iter)   \
         (*(iter) = (syncwlist_iterator_t) syncwlist_iterator_INIT_FREEABLE, 0)

// group: syncwlist_t

/* define: isempty_syncwlist
 * Implements <syncwlist_t.isempty_syncwlist>. */
#define isempty_syncwlist(wlist) \
         (0 == (wlist)->nrnodes)

/* define: len_syncwlist
 * Implements <syncwlist_t.len_syncwlist>. */
#define len_syncwlist(wlist)     \
         ((size_t)(wlist)->nrnodes)

/* define: queue_syncwlist
 * Implements <syncwlist_t.queue_syncwlist>. */
#define queue_syncwlist(wlist)                  \
         ( __extension__ ({                     \
            syncwlist_t * _wl = (wlist) ;       \
            syncqueue_t * _qu = 0 ;             \
            if (_wl->nrnodes) {                 \
               _qu = queuefromaddr_syncqueue(   \
                        _wl->next) ;            \
            }                                   \
            _qu ;                               \
         }))

#endif
