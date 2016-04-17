/* title: Patricia-Trie

   Patricia Trie Interface.

   Practical Algorithm to Retrieve Information Coded in Alphanumeric:
   The trie stores for every inserted string a node which contains a bit
   offset into the byte encoded string. The bit at this offset differentiates
   the newly inserted string from an already inserted one. If the newly inserted
   string differs in more than one bit the one with the smallest offset from the
   start of the string is chosen.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2012 Jörg Seebohn

   file: C-kern/api/ds/inmem/patriciatrie.h
    Header file <Patricia-Trie>.

   file: C-kern/ds/inmem/patriciatrie.c
    Implementation file <Patricia-Trie impl>.
*/
#ifndef CKERN_DS_INMEM_PATRICIATRIE_HEADER
#define CKERN_DS_INMEM_PATRICIATRIE_HEADER

#include "C-kern/api/ds/inmem/node/patriciatrie_node.h"

// === exported types
struct patriciatrie_t;
struct patriciatrie_iterator_t;
struct patriciatrie_prefixiter_t;
struct getkey_adapter_t;
struct getkey_data_t;

/* typedef: delete_adapter_f
 * Function type to delete a single object stored in a data structure.
 * Parameter obj points to the start address. */
typedef int (* delete_adapter_f) (void * obj);

/* typedef: getkey_adapter_f
 * Function type to return part of binary key. */
typedef void (* getkey_adapter_f) (void * obj, /*out*/struct getkey_data_t * key);


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_ds_inmem_patriciatrie
 * Tests implementation of <patriciatrie_t>. */
int unittest_ds_inmem_patriciatrie(void);
#endif


/* struct: getkey_data_t
 * Describes data block of streamed binary key and the current state of the stream. */
typedef struct getkey_data_t {
   uint8_t const* addr; // start address of partial data (addr[0..size-1] == partial_data[next_offset-size..next_offset-1])
   size_t         size; // size of addr array
} getkey_data_t;


/* struct: getkey_adapter_t
 * Function to get content of binary key at offset in bytes.
 * If key->next != 0 then the next call with offset_new == offset_old+key->size
 * returns the following part. */
typedef struct getkey_adapter_t {
   size_t            nodeoffset;   // used to convert pointer to node_t to pointer start address of object
   getkey_adapter_f  getkey;       // returns part of key
} getkey_adapter_t;

// group: lifetime

/* define: getkey_adapter_INIT
 * Static initializer.
 *
 * Parameter:
 * nodeoffset - Result of offsetof(object_type_t, node)
 * getkey_f   - Function which returns <getkey_data_t> at given offset. */
#define getkey_adapter_INIT(nodeoffset, getkey_f) \
         { (nodeoffset), (getkey_f) }

// group: query

/* function: isequal_getkeyadapter
 * Returns true if both getkey_adapter_t l and r are of equal content. */
static inline bool isequal_getkeyadapter(const getkey_adapter_t* l, const getkey_adapter_t* r)
{
            return   l->nodeoffset == r->nodeoffset
                  && l->getkey     == r->getkey;
}


/* struct: patriciatrie_t
 * Implements a trie with path compression.
 *
 * typeadapt_t:
 * The service <typeadapt_lifetime_it.delete_object> of <typeadapt_t.lifetime> is used in <free_patriciatrie> and <removenodes_patriciatrie>.
 * The service <typeadapt_getkey_it.getbinarykey> of <typeadapt_t.getbinarykey> is used in find, insert and remove functions.
 *
 * Description:
 * A patricia tree is a type of digital tree and manages nodes of type patriciatrie_node_t.
 * Every node contains a bit offset indexing the search key. If the corresponding bit of the searchkey
 * is 0 the left path is taken else the right path.
 * The bit handling is done by this implementation so it is expected that there is a binary key description
 * (<typeadapt_binarykey_t>) associated with each stored node.
 *
 * Performance:
 * If the set of stored strings is prefix-free it has guaranteed O(log n) (n = number of strings)
 * performance for insert and delete. If the strings are prefixes of each other then performance
 * could degrade to O(n).
 *
 * C-Strings:
 * If you manage C-strings and treat the trailing \0 byte as part of the key it is guaranteed that
 * a set of different strings is prefix-free.
 *
 * When to use:
 * Use patricia tries (crit-bit trees) if strings are prefix free and if they are very large.
 * The reason is that even for very large strings only O(log n) bits are compared. And if
 * strings are large (i.e. strlen >> log n) other algorithms like trees or hash tables has
 * a best effort of at least O(strlen).
 * */
typedef struct patriciatrie_t {
   patriciatrie_node_t* root;
   getkey_adapter_t     keyadapt;
} patriciatrie_t;

// group: lifetime

/* define: patriciatrie_FREE
 * Static initializer. */
#define patriciatrie_FREE \
         patriciatrie_INIT(0, getkey_adapter_INIT(0,0))

/* define: patriciatrie_INIT
 * Static initializer. You can use <patriciatrie_INIT> with the returned values prvided by <getinistate_patriciatrie>.
 * Parameter root is a pointer to <patriciatrie_node_t> and nodeadp must be of type <typeadapt_member_t> (no pointer). */
#define patriciatrie_INIT(root, keyadapt) \
         { root, keyadapt }

/* function: init_patriciatrie
 * Inits an empty tree object.
 * The <typeadapt_member_t> is copied but the <typeadapt_t> it references is not.
 * So do not delete <typeadapt_t> as long as this object lives. */
void init_patriciatrie(/*out*/patriciatrie_t * tree, getkey_adapter_t keyadapt);

/* function: free_patriciatrie
 * Frees all resources. Calling it twice is safe. */
int free_patriciatrie(patriciatrie_t * tree, delete_adapter_f delete_f/*0 ==> not called*/);

// group: query

/* function: getinistate_patriciatrie
 * Returns the current state of <patriciatrie_t> for later use in <patriciatrie_INIT>. */
static inline void getinistate_patriciatrie(const patriciatrie_t * tree, /*out*/patriciatrie_node_t ** root, /*out*/getkey_adapter_t * keyadapt/*0=>ignored*/)
{
         *root = tree->root;
         if (0 != keyadapt) *keyadapt = tree->keyadapt;
}


/* function: isempty_patriciatrie
 * Returns true if tree contains no elements. */
bool isempty_patriciatrie(const patriciatrie_t * tree);

// group: foreach-support

/* typedef: iteratortype_patriciatrie
 * Declaration to associate <patriciatrie_iterator_t> with <patriciatrie_t>. */
typedef struct patriciatrie_iterator_t iteratortype_patriciatrie;

/* typedef: iteratedtype_patriciatrie
 * Declaration to associate <patriciatrie_node_t> with <patriciatrie_t>. */
typedef patriciatrie_node_t       *  iteratedtype_patriciatrie;

// group: search

/* function: find_patriciatrie
 * Searches for a node with equal key. If it exists it is returned in found_node else ESRCH is returned. */
int find_patriciatrie(patriciatrie_t * tree, size_t keylength, const uint8_t searchkey[keylength], /*out*/patriciatrie_node_t ** found_node);

// group: change

/* function: insert_patriciatrie
 * Inserts a new node into the tree only if it is unique.
 * If another node exists with the same key nothing is inserted and the function returns EEXIST.
 * The caller has to allocate the new node and has to transfer ownership. */
int insert_patriciatrie(patriciatrie_t * tree, patriciatrie_node_t * newnode);

/* function: remove_patriciatrie
 * Removes a node if its key equals searchkey.
 * The removed node is not freed but a pointer to it is returned in *removed_node* to the caller.
 * ESRCH is returned if no node with searchkey exists. */
int remove_patriciatrie(patriciatrie_t * tree, size_t keylength, const uint8_t searchkey[keylength], patriciatrie_node_t ** removed_node);

/* function: removenodes_patriciatrie
 * Removes all nodes from the tree. For every removed node delete_f is called with the start address of the object.
 *
 * Unchecked Precondition:
 * - nodeoffset == offsetof(struct object_t, node)
 *
 * where object_t is defined as
 * > struct object_t { ...; patriciatrie_node_t node; ... };
 * */
int removenodes_patriciatrie(patriciatrie_t * tree, delete_adapter_f delete_f/*0 ==> not called*/);

// group: generic

/* define: patriciatrie_IMPLEMENT
 * Generates interface of <patriciatrie_t> storing elements of type object_t.
 *
 * Parameter:
 * _fsuffix  - The suffix name of all generated tree interface functions, e.g. "init##_fsuffix".
 * object_t  - The type of object which can be stored and retrieved from this tree.
 *             The object must contain a field of type <patriciatrie_node_t>.
 * nodename  - The access path of the field <patriciatrie_node_t> in type object_t.
 * getkey_f  - Function which returns <getkey_data_t> at given offset - see <getkey_adapter_t>. */
void patriciatrie_IMPLEMENT(IDNAME _fsuffix, TYPENAME object_t, IDNAME nodename, void (*getkey_f)(void*));



/* struct: patriciatrie_iterator_t
 * Iterates over elements contained in <patriciatrie_t>.
 * The iterator supports removing or deleting of the current node. */
typedef struct patriciatrie_iterator_t {
   patriciatrie_node_t * next;
   patriciatrie_t      * tree;
} patriciatrie_iterator_t;

// group: lifetime

/* define: patriciatrie_iterator_FREE
 * Static initializer. */
#define patriciatrie_iterator_FREE { 0, 0 }

/* function: initfirst_patriciatrieiterator
 * Initializes an iterator for <patriciatrie_t>. */
int initfirst_patriciatrieiterator(/*out*/patriciatrie_iterator_t * iter, patriciatrie_t * tree);

/* function: initlast_patriciatrieiterator
 * Initializes an iterator of <patriciatrie_t>. */
int initlast_patriciatrieiterator(/*out*/patriciatrie_iterator_t * iter, patriciatrie_t * tree);

/* function: free_patriciatrieiterator
 * Frees an iterator of <patriciatrie_t>. */
int free_patriciatrieiterator(patriciatrie_iterator_t * iter);

// group: iterate

/* function: next_patriciatrieiterator
 * Returns next node of tree in ascending order.
 * The first call after <initfirst_patriciatrieiterator> returns the node with the lowest key.
 * In case no next node exists false is returned and parameter node is not changed. */
bool next_patriciatrieiterator(patriciatrie_iterator_t * iter, /*out*/patriciatrie_node_t ** node);

/* function: prev_patriciatrieiterator
 * Returns next node of tree in descending order.
 * The first call after <initlast_patriciatrieiterator> returns the node with the highest key.
 * In case no previous node exists false is returned and parameter node is not changed. */
bool prev_patriciatrieiterator(patriciatrie_iterator_t * iter, /*out*/patriciatrie_node_t ** node);


/* struct: patriciatrie_prefixiter_t
 * Iterates over elements contained in <patriciatrie_t>.
 * The iterator supports removing or deleting of the current node. */
typedef struct patriciatrie_prefixiter_t {
   patriciatrie_node_t* next;
   patriciatrie_t     * tree;
   size_t               prefix_bits;
} patriciatrie_prefixiter_t;

// group: lifetime

/* define: patriciatrie_prefixiter_FREE
 * Static initializer. */
#define patriciatrie_prefixiter_FREE \
         { 0, 0, 0 }

/* function: initfirst_patriciatrieprefixiter
 * Initializes an iterator for <patriciatrie_t> for nodes with prefix *prefixkey*. */
int initfirst_patriciatrieprefixiter(/*out*/patriciatrie_prefixiter_t * iter, patriciatrie_t * tree, size_t keylength, const uint8_t prefixkey[keylength]);

/* function: free_patriciatrieprefixiter
 * Frees an prefix iterator of <patriciatrie_t>. */
int free_patriciatrieprefixiter(patriciatrie_iterator_t * iter);

// group: iterate

/* function: next_patriciatrieprefixiter
 * Returns next node with a certain prefix in ascending order.
 * The first call after <initfirst_patriciatrieprefixiter> returns the node with the lowest key.
 * In case no next node exists false is returned and parameter node is not changed. */
bool next_patriciatrieprefixiter(patriciatrie_prefixiter_t * iter, /*out*/patriciatrie_node_t ** node);


// section: inline implementation

/* define: free_patriciatrieiterator
 * Implements <patriciatrie_iterator_t.free_patriciatrieiterator> as NOP. */
#define free_patriciatrieiterator(iter)      \
         ((iter)->next = 0, 0)

/* define: free_patriciatrieprefixiter
 * Implements <patriciatrie_prefixiter_t.free_patriciatrieprefixiter> as NOP. */
#define free_patriciatrieprefixiter(iter)    \
         ((iter)->next = 0, 0)

/* define: init_patriciatrie
 * Implements <patriciatrie_t.init_patriciatrie>. */
#define init_patriciatrie(tree,keyadapt) \
         ((void)(*(tree) = (patriciatrie_t) patriciatrie_INIT(0, (keyadapt))))

/* define: isempty_patriciatrie
 * Implements <patriciatrie_t.isempty_patriciatrie>. */
#define isempty_patriciatrie(tree)           (0 == (tree)->root)

/* define: patriciatrie_IMPLEMENT
 * Implements <patriciatrie_t.patriciatrie_IMPLEMENT>. */
#define patriciatrie_IMPLEMENT(_fsuffix, object_t, nodename, getkey_f)   \
   typedef patriciatrie_iterator_t  iteratortype##_fsuffix;    \
   typedef object_t              *  iteratedtype##_fsuffix;    \
   static inline int  initfirst##_fsuffix##iterator(patriciatrie_iterator_t * iter, patriciatrie_t * tree) __attribute__ ((always_inline)); \
   static inline int  initlast##_fsuffix##iterator(patriciatrie_iterator_t * iter, patriciatrie_t * tree) __attribute__ ((always_inline)); \
   static inline int  free##_fsuffix##iterator(patriciatrie_iterator_t * iter) __attribute__ ((always_inline)); \
   static inline bool next##_fsuffix##iterator(patriciatrie_iterator_t * iter, object_t ** node) __attribute__ ((always_inline)); \
   static inline bool prev##_fsuffix##iterator(patriciatrie_iterator_t * iter, object_t ** node) __attribute__ ((always_inline)); \
   static inline void init##_fsuffix(/*out*/patriciatrie_t * tree) __attribute__ ((always_inline)); \
   static inline int  free##_fsuffix(patriciatrie_t * tree, delete_adapter_f delete_f) __attribute__ ((always_inline)); \
   static inline void getinistate##_fsuffix(const patriciatrie_t * tree, /*out*/patriciatrie_node_t ** root, /*out*/getkey_adapter_t * keyadapt) __attribute__ ((always_inline)); \
   static inline bool isempty##_fsuffix(const patriciatrie_t * tree) __attribute__ ((always_inline)); \
   static inline int  find##_fsuffix(patriciatrie_t * tree, size_t keylength, const uint8_t searchkey[keylength], /*out*/object_t ** found_node) __attribute__ ((always_inline)); \
   static inline int  insert##_fsuffix(patriciatrie_t * tree, object_t * new_node) __attribute__ ((always_inline)); \
   static inline int  remove##_fsuffix(patriciatrie_t * tree, size_t keylength, const uint8_t searchkey[keylength], /*out*/object_t ** removed_node) __attribute__ ((always_inline)); \
   static inline int  removenodes##_fsuffix(patriciatrie_t * tree, delete_adapter_f delete_f) __attribute__ ((always_inline)); \
   static inline size_t nodeoffset##_fsuffix(void) { \
      static_assert(&((object_t*)0)->nodename == (patriciatrie_node_t*)offsetof(object_t, nodename), "correct type"); \
      return offsetof(object_t, nodename); \
   } \
   static inline getkey_adapter_t keyadapt##_fsuffix(void) { \
      return (getkey_adapter_t) getkey_adapter_INIT(nodeoffset##_fsuffix(), (getkey_f)); \
   } \
   static inline patriciatrie_node_t * cast2node##_fsuffix(object_t * object) { \
      return &object->nodename; \
   } \
   static inline object_t * cast2object##_fsuffix(patriciatrie_node_t * node) { \
      return (object_t *) ((uintptr_t)node - nodeoffset##_fsuffix()); \
   } \
   static inline void init##_fsuffix(/*out*/patriciatrie_t * tree) { \
      init_patriciatrie(tree, keyadapt##_fsuffix()); \
   } \
   static inline int  free##_fsuffix(patriciatrie_t * tree, delete_adapter_f delete_f) { \
      return free_patriciatrie(tree, delete_f); \
   } \
   static inline void getinistate##_fsuffix(const patriciatrie_t * tree, /*out*/patriciatrie_node_t ** root, /*out*/getkey_adapter_t * keyadapt) { \
      getinistate_patriciatrie(tree, root, keyadapt); \
   } \
   static inline bool isempty##_fsuffix(const patriciatrie_t * tree) { \
      return isempty_patriciatrie(tree); \
   } \
   static inline int  find##_fsuffix(patriciatrie_t * tree, size_t keylength, const uint8_t searchkey[keylength], /*out*/object_t ** found_node) { \
      patriciatrie_node_t* node; \
      int err = find_patriciatrie(tree, keylength, searchkey, &node); \
      if (!err) *found_node = cast2object##_fsuffix(node); \
      return err; \
   } \
   static inline int  insert##_fsuffix(patriciatrie_t * tree, object_t * new_node) { \
      return insert_patriciatrie(tree, cast2node##_fsuffix(new_node)); \
   } \
   static inline int  remove##_fsuffix(patriciatrie_t * tree, size_t keylength, const uint8_t searchkey[keylength], /*out*/object_t ** removed_node) { \
      patriciatrie_node_t* node; \
      int err = remove_patriciatrie(tree, keylength, searchkey, &node); \
      if (!err) *removed_node = cast2object##_fsuffix(node); \
      return err; \
   } \
   static inline int  removenodes##_fsuffix(patriciatrie_t * tree, delete_adapter_f delete_f) { \
      return removenodes_patriciatrie(tree, delete_f); \
   } \
   static inline int  initfirst##_fsuffix##iterator(patriciatrie_iterator_t * iter, patriciatrie_t * tree) { \
      return initfirst_patriciatrieiterator(iter, tree); \
   } \
   static inline int  initlast##_fsuffix##iterator(patriciatrie_iterator_t * iter, patriciatrie_t * tree) { \
      return initlast_patriciatrieiterator(iter, tree); \
   } \
   static inline int  free##_fsuffix##iterator(patriciatrie_iterator_t * iter) { \
      return free_patriciatrieiterator(iter); \
   } \
   static inline bool next##_fsuffix##iterator(patriciatrie_iterator_t * iter, object_t ** node) { \
      patriciatrie_node_t* next; \
      bool isNext = next_patriciatrieiterator(iter, &next); \
      if (isNext) *node = cast2object##_fsuffix(next); \
      return isNext; \
   } \
   static inline bool prev##_fsuffix##iterator(patriciatrie_iterator_t * iter, object_t ** node) { \
      patriciatrie_node_t* prev; \
      bool isPrev = prev_patriciatrieiterator(iter, &prev);  \
      if (isPrev) *node = cast2object##_fsuffix(prev); \
      return isPrev; \
   }


#endif
