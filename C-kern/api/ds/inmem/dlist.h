/* title: DoubleLinkedList

   Manages a double linked circular list of objects.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2012 Jörg Seebohn

   file: C-kern/api/ds/inmem/dlist.h
    Header file <DoubleLinkedList>.

   file: C-kern/ds/inmem/dlist.c
    Implementation file <DoubleLinkedList impl>.
*/
#ifndef CKERN_DS_INMEM_DLIST_HEADER
#define CKERN_DS_INMEM_DLIST_HEADER

#include "C-kern/api/ds/inmem/node/dlist_node.h"

// forward
struct typeadapt_t;

// === exported types
struct dlist_t;
struct dlist_iterator_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_ds_inmem_dlist
 * Test <dlist_t> functionality. */
int unittest_ds_inmem_dlist(void);
#endif


/* struct: dlist_iterator_t
 * Iterates over elements contained in <dlist_t>.
 * The iterator supports removing or deleting of the current node.
 *
 * Example:
 * > dlist_t list;
 * > fill_list(&list);
 * >  foreach (_dlist, node, &list) {
 * >     if (need_to_remove(node)) {
 * >        err = remove_dlist(&list, node);
 * >     }
 * >  }
 */
typedef struct dlist_iterator_t {
   dlist_node_t  *next;
   struct dlist_t*list;
} dlist_iterator_t;

// group: lifetime

/* define: dlist_iterator_FREE
 * Static initializer. */
#define dlist_iterator_FREE { 0, 0 }

/* function: initfirst_dlistiterator
 * Initializes an iterator for <dlist_t>.
 * Returns ENODATA if list is empty. */
int initfirst_dlistiterator(/*out*/dlist_iterator_t *iter, struct dlist_t *list);

/* function: initlast_dlistiterator
 * Initializes an iterator for <dlist_t>.
 * Returns ENODATA if list is empty. */
int initlast_dlistiterator(/*out*/dlist_iterator_t *iter, struct dlist_t *list);

/* function: free_dlistiterator
 * Frees an iterator for <dlist_t>. */
int free_dlistiterator(dlist_iterator_t *iter);

// group: iterate

/* function: next_dlistiterator
 * Returns all elements from first to last node of list.
 * The first call after <initfirst_dlistiterator> returns the
 * first list element if list is not empty.
 *
 * Returns:
 * true  - node contains a pointer to the next valid node in the list.
 * false - There is no next node. The last element was already returned or the list is empty. */
bool next_dlistiterator(dlist_iterator_t *iter, /*out*/dlist_node_t ** node);

/* function: prev_dlistiterator
 * Returns all elements from last to first node of list.
 * The first call after <initlast_dlistiterator> returns the
 * last list element if list is not empty.
 *
 * Returns:
 * true  - node contains a pointer to the next valid node in the list.
 * false - There is no next node. The last element was already returned or the list is empty. */
bool prev_dlistiterator(dlist_iterator_t *iter, /*out*/dlist_node_t ** node);


/* struct: dlist_t
 * Double linked circular list.
 * The last node's next pointer points to the first node.
 * The first node's previous pointer points to the last node.
 * If the list contains only one node the node's next and prev
 * pointers point to this node.
 *
 * Make sure that any inserted node is not already inserted in another
 * list before calling any insert function!
 *
 * */
typedef struct dlist_t {
   dlist_node_t  *last;
} dlist_t;

// group: lifetime

/* define: dlist_FREE
 * Statischer Initialisierer. */
#define dlist_FREE \
         { 0 }

/* define: dlist_INIT
 * Static initializer. You can use it instead of <init_dlist>.
 * After assigning you can call <free_dlist> or any other function safely. */
#define dlist_INIT \
         { (void*)0 }

/* define: dlist_INIT_LAST
 * Static initializer. Sets the last pointer in <dlist_t> to lastnode. */
#define dlist_INIT_LAST(lastnode) \
         { (lastnode) }

/* function: init_dlist
 * Initializes a <dlist_t> object. Same as assigning <dlist_INIT>. */
void init_dlist(/*out*/dlist_t *list);

/* function: free_dlist
 * Frees all resources.
 * For every removed node the typeadapter callback <typeadapt_lifetime_it.delete_object> is called.
 * See <typeadapt_t> how to construct a typeadapter.
 * Set typeadp to 0 if you do not want to call a free memory on every node. */
int free_dlist(dlist_t *list, uint16_t nodeoffset, struct typeadapt_t * typeadp/*0=>no free called*/);

// group: query

/* function: isempty_dlist
 * Returns true if list contains no elements else false. */
int isempty_dlist(const dlist_t *list);

/* function: first_dlist
 * Returns the first element in the list.
 * Returns NULL in case list contains no elements. */
dlist_node_t * first_dlist(const dlist_t *list);

/* function: last_dlist
 * Returns the last element in the list.
 * Returns NULL in case list contains no elements. */
dlist_node_t * last_dlist(const dlist_t *list);

/* function: next_dlist
 * Returns the node coming after this node.
 * If node is the last node in the list the first is returned instead. */
dlist_node_t * next_dlist(dlist_node_t * node);

/* function: prev_dlist
 * Returns the node coming before this node.
 * If node is the first node in the list the last is returned instead. */
dlist_node_t * prev_dlist(dlist_node_t * node);

/* function: isinlist_dlist
 * Returns true if node is stored in a list else false. */
bool isinlist_dlist(const dlist_node_t * node);

// group: foreach-support

/* typedef: iteratortype_dlist
 * Declaration to associate <dlist_iterator_t> with <dlist_t>. */
typedef dlist_iterator_t iteratortype_dlist;

/* typedef: iteratedtype_dlist
 * Declaration to associate <dlist_node_t> with <dlist_t>. */
typedef dlist_node_t *   iteratedtype_dlist;

// group: change

/* function: insertfirst_dlist
 * Makes new_node the new first element of list.
 * Ownership is transfered from caller to <dlist_t>. */
void insertfirst_dlist(dlist_t *list, dlist_node_t * new_node);

/* function: insertlast_dlist
 * Makes new_node the new last element of list.
 * Ownership is transfered from caller to <dlist_t>. */
void insertlast_dlist(dlist_t *list, dlist_node_t * new_node);

/* function: insertafter_dlist
 * Inserts new_node after prev_node into the list.
 * Ownership is transfered from caller to <dlist_t>.
 * new_node will be made the new last node if prev_node is the <last_dlist>(list) node. */
void insertafter_dlist(dlist_t *list, dlist_node_t * prev_node, dlist_node_t * new_node);

/* function: insertbefore_dlist
 * Inserts new_node before next_node into the list.
 * Ownership is transfered from caller to <dlist_t>.
 * new_node will be made the new first node if next_node is the <first_dlist>(list) node. */
void insertbefore_dlist(dlist_node_t * next_node, dlist_node_t * new_node);

/* function: removefirst_dlist
 * Removes the first node from the list.
 * Ownership is transfered from <dlist_t> to caller.
 * removed_node points to the formerly first element of the list.
 *
 * Ungeprüfte Precondition:
 * o ! isempty_dlist(list) */
dlist_node_t* removefirst_dlist(dlist_t *list);

/* function: removelast_dlist
 * Removes the last node from the list.
 * Ownership is transfered from <dlist_t> to caller.
 * removed_node points to the formerly last element of the list.
 *
 * Ungeprüfte Precondition:
 * o ! isempty_dlist(list) */
void removelast_dlist(dlist_t *list, dlist_node_t ** removed_node);

/* function: remove_dlist
 * Removes the node from the list.
 * Ownership is transfered from <dlist_t> to caller.
 *
 * Ungeprüfte Precondition:
 * o node ist Teil von list && ! isempty_dlist(list) */
void remove_dlist(dlist_t *list, dlist_node_t * node);

/* function: replacenode_dlist
 * Removes oldnode from list and replaces it with newnode.
 * Ownership of oldnode is transfered back to caller.
 * Ownership of newnode is transfered from caller to <dlist_t>.
 * oldnode's prev and next pointer are cleared.
 *
 * Precondition:
 * o Make sure that newnode is not part of any list else undefined behaviour will happen.
 * o Make sure that oldnode is part of list and not any other else
 *   last pointer of list will not be updated if oldnode is the last node. */
void replacenode_dlist(dlist_t *list, dlist_node_t * oldnode, dlist_node_t * newnode);

// group: set-ops

/* function: removeall_dlist
 * Removes all nodes from the list.
 * For every removed node <typeadapt_lifetime_it.delete_object> is called.
 * Set typeadp to 0 if you do not want to call a free memory on every node. */
int removeall_dlist(dlist_t *list, uint16_t nodeoffset, struct typeadapt_t * typeadp/*0=>no free called*/);

/* function: insertlastPlist_dlist
 * Transfer ownership of all nodes from nodes to list.
 * After this operation nodes is empty and list contains all nodes.
 * The nodes are appended at the end of list. */
void insertlastPlist_dlist(dlist_t *list, dlist_t * nodes);

// group: generic

/* function: cast_dlist
 * Cast list into <dlist_t> if that is possible.
 * The generic object list must have a last pointer
 * as first member. */
dlist_t* cast_dlist(void*list);

/* function: castconst_dlist
 * Casts list into <dlist_t> if that is possible.
 * The generic object list must have a last pointer
 * as first member. */
const dlist_t* castconst_dlist(const void*list);

/* define: dlist_IMPLEMENT
 * Generates interface of double linked list storing elements of type object_t.
 *
 * Parameter:
 * _fsuffix   - The suffix of the generated list interface functions, e.g. "init##_fsuffix".
 * object_t   - The type of object which can be stored and retrieved from this list.
 *              The object must contain a field of type <dlist_node_t>.
 * nodeprefix - The access path of the field <dlist_node_t> in type object_t including ".".
 *              if next and prev pointers are part of object_t leave this field emtpy.
 * */
void dlist_IMPLEMENT(IDNAME _fsuffix, TYPENAME object_t, IDNAME nodeprefix);


// section: inline implementation

// group: dlist_iterator_t

/* define: free_dlistiterator
 * Implements <dlist_iterator_t.free_dlistiterator>. */
#define free_dlistiterator(iter) \
         (((iter)->next = 0), 0)

/* define: initfirst_dlistiterator
 * Implements <dlist_iterator_t.initfirst_dlistiterator>. */
#define initfirst_dlistiterator(iter, list) \
         ( __extension__ ({                              \
            int _err;                                    \
            typeof(iter) _iter = (iter);                 \
            typeof(list) _list = (list);                 \
            if (_list->last) {                           \
               *_iter = (typeof(*_iter))                 \
                        { first_dlist(_list), _list };   \
               _err = 0;                                 \
            } else {                                     \
               _err = ENODATA;                           \
            }                                            \
            _err;                                        \
         }))

/* define: initlast_dlistiterator
 * Implements <dlist_iterator_t.initlast_dlistiterator>. */
#define initlast_dlistiterator(iter, list) \
         ( __extension__ ({                              \
            int _err;                                    \
            typeof(iter) _iter = (iter);                 \
            typeof(list) _list = (list);                 \
            if (_list->last) {                           \
               *_iter = (typeof(*_iter))                 \
                        { last_dlist(_list), _list };    \
               _err = 0;                                 \
            } else {                                     \
               _err = ENODATA;                           \
            }                                            \
            _err;                                        \
         }))

/* define: next_dlistiterator
 * Implements <dlist_iterator_t.next_dlistiterator>. */
#define next_dlistiterator(iter, node)                      \
         ( __extension__ ({                                 \
            typeof(iter) _iter = (iter);                    \
            bool _isNext = (0 != _iter->next);              \
            if (_isNext) {                                  \
               *(node) = _iter->next;                       \
               if (_iter->list->last == _iter->next)        \
                  _iter->next = 0;                          \
               else                                         \
                  _iter->next = next_dlist(_iter->next);    \
            }                                               \
            _isNext;                                        \
         }))

/* define: prev_dlistiterator
 * Implements <dlist_iterator_t.prev_dlistiterator>. */
#define prev_dlistiterator(iter, node)                      \
         ( __extension__ ({                                 \
            typeof(iter) _iter = (iter);                    \
            bool _isNext = (0 != _iter->next);              \
            if (_isNext) {                                  \
               *(node)     = _iter->next;                   \
               _iter->next = prev_dlist(_iter->next);       \
               if (_iter->list->last == _iter->next) {      \
                  _iter->next = 0;                          \
               }                                            \
            }                                               \
            _isNext;                                        \
         }))

// group: dlist_t

/* define: cast_dlist
 * Implements <dlist_t.cast_dlist>. */
#define cast_dlist(list) \
         ( __extension__ ({                \
            typeof(list) _l2 = (list);     \
            static_assert(                 \
               &(_l2->last)                \
               == &((dlist_t*) (uintptr_t) \
                     _l2)->last,           \
                  "compatible structure"   \
            );                             \
            (dlist_t*) _l2;                \
         }))

/* define: castconst_dlist
 * Implements <dlist_t.castconst_dlist>. */
#define castconst_dlist(list) \
         ( __extension__ ({                \
            typeof(list) _l2 = (list);     \
            static_assert(                 \
               &(_l2->last)                \
               == &((dlist_t*) (uintptr_t) \
                     _l2)->last,           \
                  "compatible structure"   \
            );                             \
            (const dlist_t*) _l2;          \
         }))

/* define: first_dlist
 * Implements <dlist_t.first_dlist>. */
#define first_dlist(list) \
         ( __extension__ ({                    \
            typeof(list) _l;                   \
            _l = (list);                       \
            ( _l->last ? _l->last->next : 0 ); \
         }))

/* define: last_dlist
 * Implements <dlist_t.last_dlist>. */
#define last_dlist(list) \
         ((list)->last)

/* define: init_dlist
 * Implements <dlist_t.init_dlist>. */
#define init_dlist(list) \
         ((void)(*(list) = (dlist_t)dlist_INIT))

/* define: isempty_dlist
 * Implements <dlist_t.isempty_dlist>. */
#define isempty_dlist(list) \
         (0 == (list)->last)

/* define: isinlist_dlist
 * Implements <dlist_t.isinlist_dlist>. */
#define isinlist_dlist(node) \
         (0 != (node)->next)

/* define: next_dlist
 * Implements <dlist_t.next_dlist>. */
#define next_dlist(node)                     ((node)->next)

/* define: prev_dlist
 * Implements <dlist_t.prev_dlist>. */
#define prev_dlist(node)                     ((node)->prev)

/* define: removeall_dlist
 * Implements <dlist_t.removeall_dlist> with a call to <dlist_t.free_dlist>. */
#define removeall_dlist(list, nodeoffset, typeadp) \
         (free_dlist((list), (nodeoffset), (typeadp)))

/* define: dlist_IMPLEMENT
 * Implements <dlist_t.dlist_IMPLEMENT>. */
#define dlist_IMPLEMENT(_fsuffix, object_t, nodeprefix)     \
   typedef dlist_iterator_t   iteratortype##_fsuffix;      \
   typedef object_t *         iteratedtype##_fsuffix;      \
   static inline uint16_t nodeoffset##_fsuffix(void) { \
      static_assert(UINT16_MAX > (uintptr_t) & (((object_t*)0)->nodeprefix next), "offset fits in uint16_t"); \
      return (uint16_t) (uintptr_t) & (((object_t*)0)->nodeprefix next); \
   }  \
   static inline dlist_node_t * cast2node##_fsuffix(object_t * object) { \
      static_assert(&(((object_t*)0)->nodeprefix next) == (dlist_node_t**)((uintptr_t)nodeoffset##_fsuffix()), "correct type"); \
      static_assert(&(((object_t*)0)->nodeprefix prev) == (dlist_node_t**)(nodeoffset##_fsuffix() + sizeof(dlist_node_t*)), "correct type and offset"); \
      return (dlist_node_t *) ((uintptr_t)object + nodeoffset##_fsuffix()); \
   } \
   static inline object_t * cast2object##_fsuffix(dlist_node_t * node) { \
      return (object_t *) ((uintptr_t)node - nodeoffset##_fsuffix()); \
   } \
   static inline object_t * castnull2object##_fsuffix(dlist_node_t * node) { \
      return node ? (object_t *) ((uintptr_t)node - nodeoffset##_fsuffix()) : 0; \
   } \
   static inline void init##_fsuffix(dlist_t *list) { \
      init_dlist(list); \
   } \
   static inline int free##_fsuffix(dlist_t *list, struct typeadapt_t * typeadp) { \
      return free_dlist(list, nodeoffset##_fsuffix(), typeadp); \
   } \
   static inline int isempty##_fsuffix(const dlist_t *list) { \
      return isempty_dlist(list); \
   } \
   static inline object_t * first##_fsuffix(const dlist_t *list) { \
      return castnull2object##_fsuffix(first_dlist(list)); \
   } \
   static inline object_t * last##_fsuffix(const dlist_t *list) { \
      return castnull2object##_fsuffix(last_dlist(list)); \
   } \
   static inline object_t * next##_fsuffix(object_t * node) { \
      return cast2object##_fsuffix(next_dlist(cast2node##_fsuffix(node))); \
   } \
   static inline object_t * prev##_fsuffix(object_t * node) { \
      return cast2object##_fsuffix(prev_dlist(cast2node##_fsuffix(node))); \
   } \
   static inline bool isinlist##_fsuffix(object_t * node) { \
      return isinlist_dlist(cast2node##_fsuffix(node)); \
   } \
   static inline void insertfirst##_fsuffix(dlist_t *list, object_t * new_node) { \
      insertfirst_dlist(list, cast2node##_fsuffix(new_node)); \
   } \
   static inline void insertlast##_fsuffix(dlist_t *list, object_t * new_node) { \
      insertlast_dlist(list, cast2node##_fsuffix(new_node)); \
   } \
   static inline void insertafter##_fsuffix(dlist_t *list, object_t * prev_node, object_t * new_node) { \
      insertafter_dlist(list, cast2node##_fsuffix(prev_node), cast2node##_fsuffix(new_node)); \
   } \
   static inline void insertbefore##_fsuffix(object_t * next_node, object_t * new_node) { \
      insertbefore_dlist(cast2node##_fsuffix(next_node), cast2node##_fsuffix(new_node)); \
   } \
   static inline object_t* removefirst##_fsuffix(dlist_t *list) { \
      return cast2object##_fsuffix(removefirst_dlist(list)); \
   } \
   static inline void removelast##_fsuffix(dlist_t *list, object_t ** removed_node) { \
      dlist_node_t* _n = 0; \
      removelast_dlist(list, &_n); \
      *removed_node = cast2object##_fsuffix(_n); \
   } \
   static inline void remove##_fsuffix(dlist_t *list, object_t * node) { \
      remove_dlist(list, cast2node##_fsuffix(node)); \
   } \
   static inline void replacenode##_fsuffix(dlist_t *list, object_t * oldnode, object_t * newnode) { \
      replacenode_dlist(list, cast2node##_fsuffix(oldnode), cast2node##_fsuffix(newnode)); \
   } \
   static inline int removeall##_fsuffix(dlist_t *list, struct typeadapt_t * typeadp) { \
      return removeall_dlist(list, nodeoffset##_fsuffix(), typeadp); \
   } \
   static inline void insertlastPlist##_fsuffix(dlist_t *list, dlist_t * nodes) { \
      insertlastPlist_dlist(list, nodes); \
   } \
   static inline int initfirst##_fsuffix##iterator(dlist_iterator_t *iter, dlist_t *list) { \
      return initfirst_dlistiterator(iter, list); \
   } \
   static inline int initlast##_fsuffix##iterator(dlist_iterator_t *iter, dlist_t *list) { \
      return initlast_dlistiterator(iter, list); \
   } \
   static inline int free##_fsuffix##iterator(dlist_iterator_t *iter) { \
      return free_dlistiterator(iter); \
   } \
   static inline bool next##_fsuffix##iterator(dlist_iterator_t *iter, object_t ** node) { \
      bool isNext = next_dlistiterator(iter, (dlist_node_t**)node); \
      if (isNext) *node = cast2object##_fsuffix(*(dlist_node_t**)node); \
      return isNext; \
   } \
   static inline bool prev##_fsuffix##iterator(dlist_iterator_t *iter, object_t ** node) { \
      bool isNext = prev_dlistiterator(iter, (dlist_node_t**)node); \
      if (isNext) *node = cast2object##_fsuffix(*(dlist_node_t**)node); \
      return isNext; \
   }

#endif
