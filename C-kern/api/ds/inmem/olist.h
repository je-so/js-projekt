/* title: DoubleLinkeolistUsingOffsets

   Implementiert eine doppelt verkettete Liste
   mit Offsets anstatt Zeigern.

   Momentan wird nur der Typ uint16_t als Offset unterstützt.

   > struct {
   >   olist_node_t node;
   > } array[MAX_NODE_COUNT];
   >
   > olist_t:
   >    ---------          array[last].node
   >    | last  |-------------------------------╮
   >    ---------                               |
   >                                            |
   >                                            |
   >     (first)    olist_node_t:        (last) ↓
   >    --------     --------              --------
   > ╭->| next | --> | next | --> ... -->  | next |-╮
   > |╭-| prev | <-- | prev | <-- ... <--  | prev | |
   > || --------     --------              -------- |
   > |╰---------------array[prev].node----------^   |
   > ╰----------------array[next].node--------------╯

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2017 Jörg Seebohn

   file: C-kern/api/ds/inmem/olist.h
    Header file <DoubleLinkeolistUsingOffsets>.

   file: C-kern/ds/inmem/olist.c
    Implementation file <DoubleLinkeolistUsingOffsets impl>.
*/
#ifndef CKERN_DS_INMEM_OLIST_HEADER
#define CKERN_DS_INMEM_OLIST_HEADER

// exported types
struct olist_t;
struct olist_node_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_ds_inmem_olist
 * Test <olist_t> functionality. */
int unittest_ds_inmem_olist(void);
#endif

/* struct: olist_node_t
 * Datentyp, der von einer <olist_t> verwaltet wird.
 * en: Every type which wants to be managed by an <olist_t> must embed such a node. */
typedef struct olist_node_t {
   uint16_t next;
   uint16_t prev;
} olist_node_t;


/* struct: olist_t
 * Doppelt verkettete Liste, die anstatt mit Pointern mit Offsets (2er Potenz) arbeitet.
 *
 * Common Parameters:
 * node   - The index of the nodes (starts at 0) to be accessed.
 * first  - The start address of the first array element plus offset to embedded olist_node_t.
 * shift  - The size of one object in the array. This is the stride to the next element in the array
 *          given in number of shifts. A shift of 0 means 1 byte elements,
 *          a shift of n means (1<<n) byte elements.
 */
typedef struct olist_t {
   uint16_t last;
} olist_t;

// group: lifetime

/* define: olist_FREE
 * Statischer Initialisierer. */
#define olist_FREE \
         { 0 }

/* define: olist_INIT
 * Static initializer. You can use it instead of <init_olist>. */
#define olist_INIT \
         { 0 }

/* define: olist_INIT_LAST
 * Static initializer. Sets offset of <olist_t> pointing to last node. */
#define olist_INIT_LAST(lastnode) \
         { (lastnode) }

/* function: init_olist
 * Initializes lsit as empty. Same as assigning <olist_INIT> to list. */
void init_olist(/*out*/olist_t* list);

// group: private

static inline olist_node_t* access_olist(size_t node, olist_node_t* first, size_t shift);

// group: query

/* function: isempty_olist
 * Returns true if list contains no elements else false. */
int isempty_olist(const olist_t* list);

/* function: first_olist
 * Returns the first element in the list.
 *
 * Unchecked precondition:
 * - !isempty_olist(list) */
uint16_t first_olist(const olist_t* list, olist_node_t* first, size_t shift);

/* function: last_olist
 * Returns the last element in the list.
 *
 * Unchecked precondition:
 * - !isempty_olist(list) */
uint16_t last_olist(const olist_t* list, olist_node_t* first, size_t shift);

// group: change

/* function: insertfirst_olist
 * Makes array[node] the new first element of list.
 * Ownership is transfered from caller to <olist_t>. */
void insertfirst_olist(olist_t* list, uint16_t node, olist_node_t* first, size_t shift);

/* function: insertlast_olist
 * Makes array[node] the new last element of list.
 * Ownership is transfered from caller to <olist_t>. */
void insertlast_olist(olist_t* list, uint16_t node, olist_node_t* first, size_t shift);

/* function: remove_olist
 * Removes array[node] from the list.
 * Ownership is transfered from <dlist_t> to caller.
 *
 * Ungeprüfte Precondition:
 * o node ist Teil von list && ! isempty_dlist(list) */
void remove_olist(olist_t* list, uint16_t node, olist_node_t* first, size_t shift);

// group: generic

// ...



// section: inline implementation

/* define: access_olist
 * Implements <olist_t.access_olist>. */
static inline olist_node_t* access_olist(size_t node, olist_node_t* first, size_t shift)
{
         return (olist_node_t*) ((uintptr_t)first+(node<<shift));
}

/* define: first_olist
 * Implements <olist_t.first_olist>. */
#define first_olist(list, first, stride) \
         (access_olist((size_t)((list)->last)-1, first, stride)->next)

/* define: init_olist
 * Implements <olist_t.init_olist>. */
#define init_olist(list) \
         ((void)((list)->last=0))

/* define: insertfirst_olist
 * Implements <olist_t.insertfirst_olist>. */
#define insertfirst_olist(list, node, first, shift) \
         ( __extension__ ({                                          \
            olist_node_t* _first=(first);                            \
            typeof(list) _list=(list);                               \
            typeof((list)->last) _n=(node);                          \
            olist_node_t* _nnode;                                    \
            olist_node_t* _lnode;                                    \
            olist_node_t* _fnode;                                    \
            unsigned _shift=(shift);                                 \
            _nnode = access_olist(_n,_first,_shift);                 \
            if (_list->last) {                                       \
               size_t _last=(size_t)(_list->last-1);                 \
               _lnode = access_olist(_last,_first,_shift);           \
               _nnode->next = _lnode->next;                          \
               _nnode->prev = (typeof((list)->last)) _last;          \
               _fnode = access_olist(_lnode->next,_first,_shift);    \
               _fnode->prev = _n;                                    \
               _lnode->next = _n;                                    \
            } else {                                                 \
               _nnode->next = _n;                                    \
               _nnode->prev = _n;                                    \
               _list->last  = (typeof(_list->last))(_n+1);           \
            }                                                        \
         }))

/* define: insertlast_olist
 * Implements <olist_t.insertlast_olist>. */
#define insertlast_olist(list, node, first, shift) \
         ( __extension__ ({                                          \
            olist_node_t* _first=(first);                            \
            typeof(list) _list=(list);                               \
            typeof((list)->last) _n=(node);                          \
            olist_node_t* _nnode;                                    \
            olist_node_t* _lnode;                                    \
            olist_node_t* _fnode;                                    \
            unsigned _shift=(shift);                                 \
            _nnode = access_olist(_n,_first,_shift);                 \
            if (_list->last) {                                       \
               size_t _last=(size_t)(_list->last-1);                 \
               _lnode = access_olist(_last,_first,_shift);           \
               _nnode->next = _lnode->next;                          \
               _nnode->prev = (typeof((list)->last)) _last;          \
               _fnode = access_olist(_lnode->next,_first,_shift);    \
               _fnode->prev = _n;                                    \
               _lnode->next = _n;                                    \
            } else {                                                 \
               _nnode->next = _n;                                    \
               _nnode->prev = _n;                                    \
            }                                                        \
            _list->last  = (typeof(_list->last))(_n+1);              \
         }))

/* define: remove_olist
 * Implements <olist_t.remove_olist>. */
#define remove_olist(list, node, first, shift) \
         ( __extension__ ({                                             \
            olist_node_t* _first=(first);                               \
            typeof(list) _list=(list);                                  \
            typeof((list)->last) _n=(node);                             \
            olist_node_t* _nnode;                                       \
            unsigned _shift=(shift);                                    \
            _nnode = access_olist(_n,_first,_shift);                    \
            if (_list->last == _n+1) {                                  \
               _list->last = (typeof(_list->last))                      \
                             (_n==_nnode->prev ? 0 : _nnode->prev+1);   \
            }                                                           \
            olist_node_t _temp = *_nnode;                               \
            access_olist(_temp.next,_first,_shift)->prev = _temp.prev;  \
            access_olist(_temp.prev,_first,_shift)->next = _temp.next;  \
         }))

/* define: isempty_olist
 * Implements <olist_t.isempty_olist>. */
#define isempty_olist(list) \
         (0 == (list)->last)

/* define: last_olist
 * Implements <olist_t.last_olist>. */
#define last_olist(list, first, shift) \
         ((typeof((list)->last)) ((list)->last-1))


#endif
