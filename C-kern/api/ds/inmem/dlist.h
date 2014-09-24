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
struct typeadapt_t ;

/* typedef: struct dlist_t
 * Export <dlist_t> into global namespace. */
typedef struct dlist_t                 dlist_t ;

/* typedef: struct dlist_iterator_t
 * Export <dlist_iterator_t>. */
typedef struct dlist_iterator_t        dlist_iterator_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_ds_inmem_dlist
 * Test <dlist_t> functionality. */
int unittest_ds_inmem_dlist(void) ;
#endif


/* struct: dlist_iterator_t
 * Iterates over elements contained in <dlist_t>.
 * The iterator supports removing or deleting of the current node.
 *
 * Example:
 * > dlist_t list ;
 * > fill_list(&list) ;
 * >  foreach (_dlist, node, &list) {
 * >     if (need_to_remove(node)) {
 * >        err = remove_dlist(&list, node) ;
 * >     }
 * >  }
 */
struct dlist_iterator_t {
   struct dlist_node_t * next ;
   dlist_t             * list ;
} ;

// group: lifetime

/* define: dlist_iterator_FREE
 * Static initializer. */
#define dlist_iterator_FREE { 0, 0 }

/* function: initfirst_dlistiterator
 * Initializes an iterator for <dlist_t>.
 * Returns ENODATA if list is empty. */
int initfirst_dlistiterator(/*out*/dlist_iterator_t * iter, dlist_t * list) ;

/* function: initlast_dlistiterator
 * Initializes an iterator for <dlist_t>.
 * Returns ENODATA if list is empty. */
int initlast_dlistiterator(/*out*/dlist_iterator_t * iter, dlist_t * list) ;

/* function: free_dlistiterator
 * Frees an iterator for <dlist_t>. */
int free_dlistiterator(dlist_iterator_t * iter) ;

// group: iterate

/* function: next_dlistiterator
 * Returns all elements from first to last node of list.
 * The first call after <initfirst_dlistiterator> returns the
 * first list element if list is not empty.
 *
 * Returns:
 * true  - node contains a pointer to the next valid node in the list.
 * false - There is no next node. The last element was already returned or the list is empty. */
bool next_dlistiterator(dlist_iterator_t * iter, /*out*/struct dlist_node_t ** node) ;

/* function: prev_dlistiterator
 * Returns all elements from last to first node of list.
 * The first call after <initlast_dlistiterator> returns the
 * last list element if list is not empty.
 *
 * Returns:
 * true  - node contains a pointer to the next valid node in the list.
 * false - There is no next node. The last element was already returned or the list is empty. */
bool prev_dlistiterator(dlist_iterator_t * iter, /*out*/struct dlist_node_t ** node) ;


/* struct: dlist_t
 * Double linked circular list.
 * The last node's next pointer points to the first node.
 * The first node's previous pointer points to the last node.
 * If the list contains only one node the node's next and prev
 * pointers point to this node.
 *
 * Error codes:
 * EINVAL - Either the inserted nodes <dlist_node_t.next> pointer is not 0.
 *          Or <insertafter_dlist>, <insertbefore_dlist>, <removefirst_dlist>,
 *          or <removelast_dlist> is called for an empty list.
 * */
struct dlist_t {
   struct dlist_node_t  * last ;
} ;

// group: lifetime

/* define: dlist_INIT
 * Static initializer. You can use it instead of <init_dlist>.
 * After assigning you can call <free_dlist> or any other function safely. */
#define dlist_INIT { (void*)0 }

/* define: dlist_INIT_LAST
 * Static initializer. Sets the last pointer in <dlist_t> to lastnode. */
#define dlist_INIT_LAST(lastnode)      { (lastnode) }

/* function: init_dlist
 * Initializes a <dlist_t> object. Same as assigning <dlist_INIT>. */
void init_dlist(/*out*/dlist_t *list) ;

/* function: free_dlist
 * Frees all resources.
 * For every removed node the typeadapter callback <typeadapt_lifetime_it.delete_object> is called.
 * See <typeadapt_t> how to construct a typeadapter.
 * Set typeadp to 0 if you do not want to call a free memory on every node. */
int free_dlist(dlist_t * list, uint16_t nodeoffset, struct typeadapt_t * typeadp/*0=>no free called*/) ;

// group: query

/* function: isempty_dlist
 * Returns true if list contains no elements else false. */
int isempty_dlist(const dlist_t * list) ;

/* function: first_dlist
 * Returns the first element in the list.
 * Returns NULL in case list contains no elements. */
struct dlist_node_t * first_dlist(dlist_t * list) ;

/* function: last_dlist
 * Returns the last element in the list.
 * Returns NULL in case list contains no elements. */
struct dlist_node_t * last_dlist(dlist_t * list) ;

/* function: next_dlist
 * Returns the node coming after this node.
 * If node is the last node in the list the first is returned instead. */
struct dlist_node_t * next_dlist(struct dlist_node_t * node) ;

/* function: prev_dlist
 * Returns the node coming before this node.
 * If node is the first node in the list the last is returned instead. */
struct dlist_node_t * prev_dlist(struct dlist_node_t * node) ;

/* function: isinlist_dlist
 * Returns true if node is stored in a list else false. */
bool isinlist_dlist(const struct dlist_node_t * node) ;

// group: foreach-support

/* typedef: iteratortype_dlist
 * Declaration to associate <dlist_iterator_t> with <dlist_t>. */
typedef dlist_iterator_t      iteratortype_dlist ;

/* typedef: iteratedtype_dlist
 * Declaration to associate <dlist_node_t> with <dlist_t>. */
typedef struct dlist_node_t * iteratedtype_dlist ;

// group: change

/* function: insertfirst_dlist
 * Makes new_node the new first element of list.
 * Ownership is transfered from caller to <dlist_t>. */
void insertfirst_dlist(dlist_t * list, struct dlist_node_t * new_node) ;

/* function: insertlast_dlist
 * Makes new_node the new last element of list.
 * Ownership is transfered from caller to <dlist_t>. */
void insertlast_dlist(dlist_t * list, struct dlist_node_t * new_node) ;

/* function: insertafter_dlist
 * Inserts new_node after prev_node into the list.
 * Ownership is transfered from caller to <dlist_t>.
 * new_node will be made the new last node if prev_node is the <last_dlist>(list) node. */
void insertafter_dlist(dlist_t * list, struct dlist_node_t * prev_node, struct dlist_node_t * new_node) ;

/* function: insertbefore_dlist
 * Inserts new_node before next_node into the list.
 * Ownership is transfered from caller to <dlist_t>.
 * new_node will be made the new first node if next_node is the <first_dlist>(list) node. */
void insertbefore_dlist(struct dlist_node_t * next_node, struct dlist_node_t * new_node) ;

/* function: removefirst_dlist
 * Removes the first node from the list.
 * Ownership is transfered from <dlist_t> to caller.
 * If the list contains no elements EINVAL is returned else
 * removed_node points to the formerly first element of the list. */
int removefirst_dlist(dlist_t * list, struct dlist_node_t ** removed_node) ;

/* function: removelast_dlist
 * Removes the last node from the list.
 * Ownership is transfered from <dlist_t> to caller.
 * If the list contains no elements EINVAL is returned else
 * removed_node points to the formerly last element of the list. */
int removelast_dlist(dlist_t * list, struct dlist_node_t ** removed_node) ;

/* function: remove_dlist
 * Removes the node from the list.
 * Ownership is transfered from <dlist_t> to caller.
 *
 * Precondition:
 * Make sure that node is part of this list and not any other else undefined behaviour could happen. */
int remove_dlist(dlist_t * list, struct dlist_node_t * node) ;

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
void replacenode_dlist(dlist_t * list, struct dlist_node_t * newnode, struct dlist_node_t * oldnode) ;

// group: set-ops

/* function: removeall_dlist
 * Removes all nodes from the list.
 * For every removed node <typeadapt_lifetime_it.delete_object> is called.
 * Set typeadp to 0 if you do not want to call a free memory on every node. */
int removeall_dlist(dlist_t * list, uint16_t nodeoffset, struct typeadapt_t * typeadp/*0=>no free called*/) ;

/* function: transfer_dlist
 * Transfer ownership of all nodes from fromlist to tolist.
 * After this operation fromlist is empty and tolist contains all nodes.
 * The nodes are appended at the end of tolist. */
void transfer_dlist(dlist_t * tolist, dlist_t * fromlist) ;

// group: generic

/* function: genericcast_dlist
 * Casts list into <dlist_t> if that is possible.
 * The generic object list must have a last pointer
 * as first member. */
dlist_t * genericcast_dlist(void * list) ;

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
void dlist_IMPLEMENT(IDNAME _fsuffix, TYPENAME object_t, IDNAME nodeprefix) ;


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

/* define: genericcast_dlist
 * Implements <dlist_t.genericcast_dlist>. */
#define genericcast_dlist(list)                             \
         ( __extension__ ({                                 \
            static_assert(                                  \
                  sizeof((list)->last)                      \
                  == sizeof(((dlist_t*)0)->last)            \
                  && offsetof(typeof(*(list)), last)        \
                  == offsetof(dlist_t, last)                \
                  && (typeof((list)->last))0                \
                     == (dlist_node_t*)0,                   \
                  "ensure compatible structure"             \
            ) ;                                             \
            (dlist_t*) (list) ;                             \
         }))

/* define: first_dlist
 * Implements <dlist_t.first_dlist>. */
#define first_dlist(list)                    ((list)->last ? (list)->last->next : (struct dlist_node_t*)0)

/* define: last_dlist
 * Implements <dlist_t.last_dlist>. */
#define last_dlist(list)                     ((list)->last)

/* define: init_dlist
 * Implements <dlist_t.init_dlist>. */
#define init_dlist(list)                     ((void)(*(list) = (dlist_t)dlist_INIT))

/* define: isempty_dlist
 * Implements <dlist_t.isempty_dlist>. */
#define isempty_dlist(list)                  (0 == (list)->last)

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
   typedef dlist_iterator_t   iteratortype##_fsuffix ;      \
   typedef object_t *         iteratedtype##_fsuffix ;      \
   static inline int  initfirst##_fsuffix##iterator(dlist_iterator_t * iter, dlist_t * list) __attribute__ ((always_inline)) ;   \
   static inline int  initlast##_fsuffix##iterator(dlist_iterator_t * iter, dlist_t * list) __attribute__ ((always_inline)) ;    \
   static inline int  free##_fsuffix##iterator(dlist_iterator_t * iter) __attribute__ ((always_inline)) ; \
   static inline bool next##_fsuffix##iterator(dlist_iterator_t * iter, object_t ** node) __attribute__ ((always_inline)) ; \
   static inline bool prev##_fsuffix##iterator(dlist_iterator_t * iter, object_t ** node) __attribute__ ((always_inline)) ; \
   static inline void init##_fsuffix(dlist_t * list) __attribute__ ((always_inline)) ; \
   static inline int  free##_fsuffix(dlist_t * list, struct typeadapt_t * typeadp) __attribute__ ((always_inline)) ; \
   static inline int  isempty##_fsuffix(const dlist_t * list) __attribute__ ((always_inline)) ; \
   static inline object_t * first##_fsuffix(const dlist_t * list) __attribute__ ((always_inline)) ;  \
   static inline object_t * last##_fsuffix(const dlist_t * list) __attribute__ ((always_inline)) ;   \
   static inline object_t * next##_fsuffix(object_t * node) __attribute__ ((always_inline)) ; \
   static inline object_t * prev##_fsuffix(object_t * node) __attribute__ ((always_inline)) ; \
   static inline bool isinlist##_fsuffix(object_t * node) __attribute__ ((always_inline)) ; \
   static inline void insertfirst##_fsuffix(dlist_t * list, object_t * new_node) __attribute__ ((always_inline)) ;   \
   static inline void insertlast##_fsuffix(dlist_t * list, object_t * new_node) __attribute__ ((always_inline)) ;    \
   static inline void insertafter##_fsuffix(dlist_t * list, object_t * prev_node, object_t * new_node) __attribute__ ((always_inline)) ; \
   static inline void insertbefore##_fsuffix(object_t* next_node, object_t * new_node) __attribute__ ((always_inline)) ; \
   static inline int removefirst##_fsuffix(dlist_t * list, object_t ** removed_node) __attribute__ ((always_inline)) ;  \
   static inline int removelast##_fsuffix(dlist_t * list, object_t ** removed_node) __attribute__ ((always_inline)) ;   \
   static inline int remove##_fsuffix(dlist_t * list, object_t * node) __attribute__ ((always_inline)) ; \
   static inline void replacenode##_fsuffix(dlist_t * list, object_t * newnode, object_t * oldnode) __attribute__ ((always_inline)) ; \
   static inline int removeall##_fsuffix(dlist_t * list, struct typeadapt_t * typeadp) __attribute__ ((always_inline)) ; \
   static inline void transfer##_fsuffix(dlist_t * tolist, dlist_t * fromlist) __attribute__ ((always_inline)) ; \
   static inline uint16_t nodeoffset##_fsuffix(void) __attribute__ ((always_inline)) ; \
   static inline uint16_t nodeoffset##_fsuffix(void) { \
      static_assert(UINT16_MAX > (uintptr_t) & (((object_t*)0)->nodeprefix next), "offset fits in uint16_t") ; \
      return (uint16_t) (uintptr_t) & (((object_t*)0)->nodeprefix next) ; \
   }  \
   static inline dlist_node_t * asnode##_fsuffix(object_t * object) { \
      static_assert(&(((object_t*)0)->nodeprefix next) == (dlist_node_t**)((uintptr_t)nodeoffset##_fsuffix()), "correct type") ; \
      static_assert(&(((object_t*)0)->nodeprefix prev) == (dlist_node_t**)(nodeoffset##_fsuffix() + sizeof(dlist_node_t*)), "correct type and offset") ; \
      return (dlist_node_t *) ((uintptr_t)object + nodeoffset##_fsuffix()) ; \
   } \
   static inline object_t * asobject##_fsuffix(dlist_node_t * node) { \
      return (object_t *) ((uintptr_t)node - nodeoffset##_fsuffix()) ; \
   } \
   static inline object_t * asobjectnull##_fsuffix(dlist_node_t * node) { \
      return node ? (object_t *) ((uintptr_t)node - nodeoffset##_fsuffix()) : 0 ; \
   } \
   static inline void init##_fsuffix(dlist_t * list) { \
      init_dlist(list) ; \
   } \
   static inline int free##_fsuffix(dlist_t * list, struct typeadapt_t * typeadp) { \
      return free_dlist(list, nodeoffset##_fsuffix(), typeadp) ; \
   } \
   static inline int isempty##_fsuffix(const dlist_t * list) { \
      return isempty_dlist(list) ; \
   } \
   static inline object_t * first##_fsuffix(const dlist_t * list) { \
      return asobjectnull##_fsuffix(first_dlist(list)) ; \
   } \
   static inline object_t * last##_fsuffix(const dlist_t * list) { \
      return asobjectnull##_fsuffix(last_dlist(list)) ; \
   } \
   static inline object_t * next##_fsuffix(object_t * node) { \
      return asobject##_fsuffix(next_dlist(asnode##_fsuffix(node))) ; \
   } \
   static inline object_t * prev##_fsuffix(object_t * node) { \
      return asobject##_fsuffix(prev_dlist(asnode##_fsuffix(node))) ; \
   } \
   static inline bool isinlist##_fsuffix(object_t * node) { \
      return isinlist_dlist(asnode##_fsuffix(node)) ; \
   } \
   static inline void insertfirst##_fsuffix(dlist_t * list, object_t * new_node) { \
      insertfirst_dlist(list, asnode##_fsuffix(new_node)) ; \
   } \
   static inline void insertlast##_fsuffix(dlist_t * list, object_t * new_node) { \
      insertlast_dlist(list, asnode##_fsuffix(new_node)) ; \
   } \
   static inline void insertafter##_fsuffix(dlist_t * list, object_t * prev_node, object_t * new_node) { \
      insertafter_dlist(list, asnode##_fsuffix(prev_node), asnode##_fsuffix(new_node)) ; \
   } \
   static inline void insertbefore##_fsuffix(object_t * next_node, object_t * new_node) { \
      insertbefore_dlist(asnode##_fsuffix(next_node), asnode##_fsuffix(new_node)) ; \
   } \
   static inline int removefirst##_fsuffix(dlist_t * list, object_t ** removed_node) { \
      int err = removefirst_dlist(list, (dlist_node_t**)removed_node) ; \
      if (!err) *removed_node = asobject##_fsuffix(*(dlist_node_t**)removed_node) ; \
      return err ; \
   } \
   static inline int removelast##_fsuffix(dlist_t * list, object_t ** removed_node) { \
      int err = removelast_dlist(list, (dlist_node_t**)removed_node) ; \
      if (!err) *removed_node = asobject##_fsuffix(*(dlist_node_t**)removed_node) ; \
      return err ; \
   } \
   static inline int remove##_fsuffix(dlist_t * list, object_t * node) { \
      return remove_dlist(list, asnode##_fsuffix(node)) ; \
   } \
   static inline void replacenode##_fsuffix(dlist_t * list, object_t * newnode, object_t * oldnode) { \
      replacenode_dlist(list, asnode##_fsuffix(newnode), asnode##_fsuffix(oldnode)) ; \
   } \
   static inline int removeall##_fsuffix(dlist_t * list, struct typeadapt_t * typeadp) { \
      return removeall_dlist(list, nodeoffset##_fsuffix(), typeadp) ; \
   } \
   static inline void transfer##_fsuffix(dlist_t * tolist, dlist_t * fromlist) { \
      transfer_dlist(tolist, fromlist) ; \
   } \
   static inline int initfirst##_fsuffix##iterator(dlist_iterator_t * iter, dlist_t * list) { \
      return initfirst_dlistiterator(iter, list) ; \
   } \
   static inline int initlast##_fsuffix##iterator(dlist_iterator_t * iter, dlist_t * list) { \
      return initlast_dlistiterator(iter, list) ; \
   } \
   static inline int free##_fsuffix##iterator(dlist_iterator_t * iter) { \
      return free_dlistiterator(iter) ; \
   } \
   static inline bool next##_fsuffix##iterator(dlist_iterator_t * iter, object_t ** node) { \
      bool isNext = next_dlistiterator(iter, (dlist_node_t**)node) ; \
      if (isNext) *node = asobject##_fsuffix(*(dlist_node_t**)node) ; \
      return isNext ; \
   } \
   static inline bool prev##_fsuffix##iterator(dlist_iterator_t * iter, object_t ** node) { \
      bool isNext = prev_dlistiterator(iter, (dlist_node_t**)node) ; \
      if (isNext) *node = asobject##_fsuffix(*(dlist_node_t**)node) ; \
      return isNext ; \
   }

#endif
