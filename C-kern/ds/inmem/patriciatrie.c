/* title: Patricia-Trie impl
   Implements <Patricia-Trie>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2012 Jörg Seebohn

   file: C-kern/api/ds/inmem/patriciatrie.h
    Header file <Patricia-Trie>.

   file: C-kern/ds/inmem/patriciatrie.c
    Implementation file <Patricia-Trie impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/ds/inmem/patriciatrie.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/ds/foreach.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#include "C-kern/api/test/errortimer.h"
#endif



// section: patriciatrie_t

// group: lifetime

int free_patriciatrie(patriciatrie_t *tree, delete_adapter_f delete_f)
{
   int err = removenodes_patriciatrie(tree, delete_f);

   tree->keyadapt = (getkey_adapter_t) getkey_adapter_INIT(0,0);

   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: helper

static inline void * cast_object(patriciatrie_node_t *node, patriciatrie_t *tree)
{
   return ((uint8_t*)node - tree->keyadapt.nodeoffset);
}

// group: search

/* function: getbitinit
 * Ensures that precondition of getbit is met. */
static inline void getbitinit(patriciatrie_t *tree, getkey_data_t *key, size_t bitoffset)
{
   size_t byteoffset = bitoffset / 8;
   if (byteoffset < key->offset) {
      // PRECONDITION of getbit violated ==> go back in stream
      tree->keyadapt.getkey(key, byteoffset);
   }
}

/* function: getbit
 * Returns the bit value at bitoffset if bitoffset is in range [0..key->streamsize*8-1].
 * Every key has a virtual end marker byte 0xFF at offset key->streamsize.
 * Therefore a bitoffset in range [key->streamsize*8..key->streamsize*8+7]
 * always results into the value 1. For every bitoffset >= (key->streamsize+1)*8
 * a value of 0 is returned.
 *
 * Unchecked Precondition:
 * - key->offset <= bitoffset/8
 *
 * */
static inline int getbit(patriciatrie_t *tree, getkey_data_t *key, size_t bitoffset)
{
   size_t byteoffset = bitoffset / 8;
   if (byteoffset >= key->endoffset) {
      // end of streamed key ?
      if (byteoffset >= key->streamsize) {
         return (byteoffset == key->streamsize);
      }
      // go forward in stream
      tree->keyadapt.getkey(key, byteoffset);
   }
   return key->addr[byteoffset - key->offset] & (0x80>>(bitoffset%8));
}

/* function: get_first_different_bit
 * Determines the bit with the smallest offset which differs in foundkey and newkey.
 * Virtual end markers of 0xFF at end of each key are used to ensure that keys of different
 * length are always different.
 * If both keys are equal the function returns the error code EEXIST.
 * On success 0 is returned and *newkey_bit_offset contains the bit index
 * of the first differing bit starting from bitoffset 0.
 * The value *newkey_bit_value contains the value of the bit
 * from newkey at offset *newkey_bit_offset.
 *
 * Unchecked Precondition:
 * - foundobj == cast_object(foundnode, tree)
 * - newobj   == cast_object(newnode, tree)
 * */
static inline int get_first_different_bit(
      patriciatrie_t*tree,
      getkey_data_t *foundkey,
      getkey_data_t *newkey,
      /*out*/size_t *newkey_bit_offset,
      /*out*/uint8_t*newkey_bit_value)
{
   uint8_t bf;
   uint8_t bn;

   // reset both keys to offset 0
   ////
   if (foundkey->offset) {
      tree->keyadapt.getkey(foundkey, 0);
   }
   if (newkey->offset) {
      tree->keyadapt.getkey(newkey, 0);
   }

   // compare keys
   ////
   size_t offset = 0;
   uint8_t const *keyf = foundkey->addr;
   uint8_t const *keyn = newkey->addr;
   for (;;) {
      size_t endoffset = foundkey->endoffset < newkey->endoffset ? foundkey->endoffset : newkey->endoffset;
      for (; offset < endoffset; ++offset) {
         if (*keyf != *keyn) {
            bf = *keyf;
            bn = *keyn;
            goto FOUND_DIFFERENCE;
         }
         ++ keyf;
         ++ keyn;
      }
      if (offset == newkey->endoffset) {
         if (offset == newkey->streamsize) {
            if (offset == foundkey->endoffset) {
               if (offset == foundkey->streamsize) return EEXIST;
               tree->keyadapt.getkey(foundkey, offset);
               keyf = foundkey->addr;
            }
            if (*keyf != 0xFF/*end marker of newkey*/) {
               bf = *keyf;
               bn = 0xFF;
               goto FOUND_DIFFERENCE;
            }
            bn = 0; // artificial extension of newkey is 0
            ++offset;
            ++keyf;
            for (;;) {
               for (; offset < foundkey->endoffset; ++offset) {
                  if (*keyf != 0) {
                     bf = *keyf;
                     goto FOUND_DIFFERENCE;
                  }
                  ++ keyf;
               }
               if (offset == foundkey->streamsize) {
                  bf = 0xFF;/*end marker of foundkey*/
                  goto FOUND_DIFFERENCE;
               }
               tree->keyadapt.getkey(foundkey, offset);
               keyf = foundkey->addr;
            }
         }
         tree->keyadapt.getkey(newkey, offset);
         keyn = newkey->addr;
      }
      // == assert(offset < newkey->endoffset) ==
      if (offset == foundkey->endoffset) {
         if (offset == foundkey->streamsize) {
            if (*keyn != 0xFF/*end marker of foundkey*/) {
               bf = 0xFF;
               bn = *keyn;
               goto FOUND_DIFFERENCE;
            }
            bf = 0; // artificial extension of foundkey is 0
            ++offset;
            ++keyn;
            for (;;) {
               for (; offset < newkey->endoffset; ++offset) {
                  if (*keyn != 0) {
                     bn = *keyn;
                     goto FOUND_DIFFERENCE;
                  }
                  ++ keyn;
               }
               if (offset == newkey->streamsize) {
                  bn = 0xFF;/*end marker of newkey*/
                  goto FOUND_DIFFERENCE;
               }
               tree->keyadapt.getkey(newkey, offset);
               keyn = newkey->addr;
            }
         }
         tree->keyadapt.getkey(foundkey, offset);
         keyf = foundkey->addr;
      }
   }

FOUND_DIFFERENCE: // == (bf != bn) ==
   offset *= 8;
   bf ^= bn;
   unsigned mask = 0x80;
   for (; 0 == (mask & bf); mask >>= 1) {
      ++ offset;
   }

   *newkey_bit_offset = offset;
   *newkey_bit_value  = (uint8_t) (bn & mask);
   return 0;
}

static inline int is_key_equal(
      patriciatrie_t      *tree,
      patriciatrie_node_t *foundnode,
      getkey_data_t       *cmpkey)
{
   getkey_data_t foundkey;
   init1_getkeydata(&foundkey, tree->keyadapt.getkey, cast_object(foundnode, tree));

   if (foundkey.streamsize == cmpkey->streamsize) {
      if (cmpkey->offset) {
         tree->keyadapt.getkey(cmpkey, 0);
      }
      size_t offset = 0;
      uint8_t const *keyf = foundkey.addr;
      uint8_t const *keyc = cmpkey->addr;
      if (cmpkey->endoffset == cmpkey->streamsize) {
         for (;;) {
            for (; offset < foundkey.endoffset ; ++offset) {
               if (keyc[offset] != *keyf++) {
                  return false;
               }
            }
            if (offset == foundkey.streamsize) {
               return true/*equal*/;
            }
            tree->keyadapt.getkey(&foundkey, offset);
            keyf = foundkey.addr;
         }

      } else {
         for (;;) {
            size_t endoffset = foundkey.endoffset <= cmpkey->endoffset ? foundkey.endoffset : cmpkey->endoffset;
            for (; offset < endoffset; ++offset) {
               if (*keyc++ != *keyf++) {
                  return false;
               }
            }
            if (offset == foundkey.endoffset) {
               if (offset == foundkey.streamsize) {
                  return true/*equal*/;
               }
               tree->keyadapt.getkey(&foundkey, offset);
               keyf = foundkey.addr;
            }
            if (offset == cmpkey->endoffset) {
               tree->keyadapt.getkey(cmpkey, offset);
               keyc = cmpkey->addr;
            }
         }
      }
   }

   return false;
}

/* function: findnode
 * Returns found_parent and found_node, both !=0.
 * In case of a single node *found_node == *found_parent.
 * In case of an empty trie ESRCH is returned else 0.
 * Either (*found_node)->bit_offset < (*found_parent)->bit_offset),
 * or (*found_node)->bit_offset == (*found_parent)->bit_offset) in case of a single node.
 *
 * Unchecked Precondition:
 * - tree->root  != 0
 * - key->offset == 0
 * */
static void findnode(patriciatrie_t *tree, getkey_data_t *key, patriciatrie_node_t ** found_parent, patriciatrie_node_t ** found_node)
{
   patriciatrie_node_t *parent;
   patriciatrie_node_t *node = tree->root;

   do {
      parent = node;
      if (getbit(tree, key, node->bit_offset)) {
         node = node->right;
      } else {
         node = node->left;
      }
   } while (node->bit_offset > parent->bit_offset);

   *found_node   = node;
   *found_parent = parent;
}

int find_patriciatrie(patriciatrie_t *tree, size_t len, const uint8_t key[len], /*out*/patriciatrie_node_t ** found_node)
{
   int err;
   patriciatrie_node_t *node;
   patriciatrie_node_t *parent;

   VALIDATE_INPARAM_TEST((key != 0 || len == 0) && len < (((size_t)-1)/8), ONERR, );

   if (!tree->root) return ESRCH;

   getkey_data_t fullkey;
   initfullkey_getkeydata(&fullkey, len, key);
   findnode(tree, &fullkey, &parent, &node);

   if (! is_key_equal(tree, node, &fullkey)) {
      return ESRCH;
   }

   *found_node = node;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: change

int insert_patriciatrie(patriciatrie_t *tree, patriciatrie_node_t * newnode, /*err*/ patriciatrie_node_t** existing_node/*0 ==> not returned*/)
{
   int err;
   getkey_data_t newkey;
   init1_getkeydata(&newkey, tree->keyadapt.getkey, cast_object(newnode, tree));

   VALIDATE_INPARAM_TEST((newkey.addr != 0 || newkey.streamsize == 0) && newkey.streamsize < (((size_t)-1)/8), ONERR, );

   if (!tree->root) {
      tree->root = newnode;
      newnode->bit_offset = 0;
      newnode->right = newnode;
      newnode->left  = newnode;
      return 0;
   }

   // search node
   patriciatrie_node_t *parent;
   patriciatrie_node_t *node;
   findnode(tree, &newkey, &parent, &node);

   size_t  new_bitoffset;
   uint8_t new_bitvalue;
   {
      getkey_data_t foundkey;
      init1_getkeydata(&foundkey, tree->keyadapt.getkey, cast_object(node, tree));
      if (node == newnode || get_first_different_bit(tree, &foundkey, &newkey, &new_bitoffset, &new_bitvalue)) {
         if (existing_node) *existing_node = node;
         return EEXIST;   // found node has same key
      }
      // new_bitvalue == getbit(tree, &newkey, new_bitoffset)
   }

   // search position in tree where new_bitoffset belongs to
   if (new_bitoffset < parent->bit_offset) {
      node   = tree->root;
      parent = 0;
      getbitinit(tree, &newkey, node->bit_offset);
      while (node->bit_offset < new_bitoffset) {
         parent = node;
         if (getbit(tree, &newkey, node->bit_offset)) {
            node = node->right;
         } else {
            node = node->left;
         }
      }
   }

   // == assert(parent == 0 || parent->bit_offset < new_bitoffset || (tree->root == parent && parent == node)/*single node case)*/) ==

   if (node->right == node->left) {
      // node points to itself (bit_offset is unused) and is at the tree bottom (LEAF)
      // == assert(node->bit_offset == 0 && node->left == node) ==
      newnode->bit_offset = 0;
      newnode->right = newnode;
      newnode->left  = newnode;
      node->bit_offset = new_bitoffset;
      if (new_bitvalue) {
         node->right = newnode;
      } else {
         node->left  = newnode;
      }
   } else {
      newnode->bit_offset = new_bitoffset;
      if (new_bitvalue) {
         newnode->right = newnode;
         newnode->left  = node;
      } else {
         newnode->right = node;
         newnode->left  = newnode;
      }
      if (parent) {
         // == (tree->root == node) is possible therefore check for parent != 0 ==
         if (parent->right == node) {
            parent->right = newnode;
         } else {
            parent->left = newnode;
         }
      } else {
         // == (parent == 0) ==> (tree->root == node) ==
         tree->root = newnode;
      }
   }

   return 0;
ONERR:
   if (existing_node) *existing_node = 0; // err param
   TRACEEXIT_ERRLOG(err);
   return err;
}

int remove_patriciatrie(patriciatrie_t *tree, size_t len, const uint8_t key[len], /*out*/patriciatrie_node_t ** removed_node)
{
   int err;
   patriciatrie_node_t * node;
   patriciatrie_node_t * parent;

   VALIDATE_INPARAM_TEST((key != 0 || len == 0) && len < (((size_t)-1)/8), ONERR, );

   if (!tree->root) return ESRCH;

   getkey_data_t fullkey;
   initfullkey_getkeydata(&fullkey, len, key);
   findnode(tree, &fullkey, &parent, &node);

   if (! is_key_equal(tree, node, &fullkey)) {
      return ESRCH;
   }

   patriciatrie_node_t * const delnode = node;
   patriciatrie_node_t * replacednode = 0;
   patriciatrie_node_t * replacedwith;

   if (node->right == node->left) {
      // node points to itself (bit_offset is unused) and is at the tree bottom (LEAF)
      // == assert(node->left == node && 0 == node->bit_offset) ==
      if (tree->root == node) {
         tree->root = 0;
      } else if ( parent->left == parent
                  || parent->right == parent) {
         // parent is new LEAF
         parent->bit_offset = 0;
         parent->left  = parent;
         parent->right = parent;
      } else {
         // shift parents other child into parents position
         // other child or any child of this child points back to parent
         replacednode = parent;
         replacedwith = (parent->left == node ? parent->right : parent->left);
         // and make parent the new LEAF (only one such node in the whole trie)
         parent->bit_offset = 0;
         parent->left  = parent;
         parent->right = parent;
      }
   } else if (node->left == node || node->right == node) {
      // node points to itself
      // ==> (parent == node) && "node has only one child which does not point back to node"
      // node can be replaced by single child
      replacednode = node;
      replacedwith = (node->left == node ? node->right : node->left);
   } else {
      // node has two childs ==> one of the childs of node points back to node
      // ==> (parent != node)
      replacednode = node;
      replacedwith = parent;
      // find parent of replacedwith
      // node->bit_offset < parent->bit_offset ==> node higher in hierarchy than parent
      // NOT NEEDED(only single data block): getbitinit(tree, &fullkey, node->bit_offset)
      do {
         parent = node;
         if (getbit(tree, &fullkey, node->bit_offset)) {
            node = node->right;
         } else {
            node = node->left;
         }
      } while (node != replacedwith);
      // now let parent of replacedwith point to other child of replacedwith instead to replacedwith
      if (parent->left == node)
         parent->left = (node->left == replacednode ? node->right : node->left);
      else
         parent->right = (node->left == replacednode ? node->right : node->left);
      // copy replacednode into replacedwith
      node->bit_offset = replacednode->bit_offset;
      node->left       = replacednode->left;
      node->right      = replacednode->right;
   }

   // find parent of replacednode and let it point to replacedwith
   if (replacednode) {
      node = tree->root;
      if (node == replacednode) {
         tree->root = replacedwith;
      } else {
         // NOT NEEDED(only single data block): getbitinit(tree, &fullkey, node->bit_offset)
         do {
            parent = node;
            if (getbit(tree, &fullkey, node->bit_offset)) {
               node = node->right;
            } else {
               node = node->left;
            }
         } while (node != replacednode);
         if (parent->left == replacednode)
            parent->left = replacedwith;
         else
            parent->right = replacedwith;
      }
   }

   delnode->bit_offset = 0;
   delnode->left       = 0;
   delnode->right      = 0;

   *removed_node = delnode;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int removenodes_patriciatrie(patriciatrie_t *tree, delete_adapter_f delete_f)
{
   int err;
   patriciatrie_node_t * parent = 0;
   patriciatrie_node_t * node   = tree->root;

   tree->root = 0;

   if (node) {

      const bool isDelete = (delete_f != 0);

      err = 0;

      for (;;) {
         while (  node->left->bit_offset > node->bit_offset
                  || (node->left != node && node->left->left == node->left && node->left->right == node->left) /*node->left has unused bit_offset*/) {
            patriciatrie_node_t * nodeleft = node->left;
            node->left = parent;
            parent = node;
            node   = nodeleft;
         }
         if (  node->right->bit_offset > node->bit_offset
               || (node->right != node && node->right->left == node->right && node->right->right == node->right) /*node->right has unused bit_offset*/) {
            patriciatrie_node_t * noderight = node->right;
            node->left = parent;
            parent = node;
            node   = noderight;
         } else {
            node->bit_offset = 0;
            node->left       = 0;
            node->right      = 0;
            if (isDelete) {
               void * object = cast_object(node, tree);
               int err2 = delete_f(object);
               if (err2) err = err2;
            }
            if (!parent) break;
            if (parent->right == node) {
               parent->right = parent;
            }
            node   = parent;
            parent = node->left;
            node->left = node;
         }
      }

      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}


// section: patriciatrie_iterator_t

// group: lifetime

int initfirst_patriciatrieiterator(/*out*/patriciatrie_iterator_t *iter, patriciatrie_t *tree)
{
   patriciatrie_node_t * parent;
   patriciatrie_node_t * node = tree->root;

   if (node) {
      do {
         parent = node;
         node   = node->left;
      } while (node->bit_offset > parent->bit_offset);
   }

   iter->next = node;
   iter->tree = tree;
   return 0;
}

int initlast_patriciatrieiterator(/*out*/patriciatrie_iterator_t *iter, patriciatrie_t *tree)
{
   patriciatrie_node_t * parent;
   patriciatrie_node_t * node = tree->root;

   if (node) {
      do {
         parent = node;
         node = node->right;
      } while (node->bit_offset > parent->bit_offset);
   }

   iter->next = node;
   iter->tree = tree;
   return 0;
}

// group: iterate

bool next_patriciatrieiterator(patriciatrie_iterator_t *iter, /*out*/patriciatrie_node_t ** node)
{
   if (!iter->next) return false;

   *node = iter->next;

   getkey_data_t nextk;
   init1_getkeydata(&nextk, iter->tree->keyadapt.getkey, cast_object(iter->next, iter->tree));

   patriciatrie_node_t * next = iter->tree->root;
   patriciatrie_node_t * parent;
   patriciatrie_node_t * higher_branch_parent = 0;
   do {
      parent = next;
      if (getbit(iter->tree, &nextk, next->bit_offset)) {
         next = next->right;
      } else {
         higher_branch_parent = parent;
         next = next->left;
      }
   } while (next->bit_offset > parent->bit_offset);

   if (higher_branch_parent) {
      parent = higher_branch_parent;
      next = parent->right;
      while (next->bit_offset > parent->bit_offset) {
         parent = next;
         next = next->left;
      }
      iter->next = next;
   } else {
      iter->next = 0;
   }

   return true;
}

bool prev_patriciatrieiterator(patriciatrie_iterator_t *iter, /*out*/patriciatrie_node_t ** node)
{
   if (!iter->next) return false;

   *node = iter->next;

   getkey_data_t nextk;
   init1_getkeydata(&nextk, iter->tree->keyadapt.getkey, cast_object(iter->next, iter->tree));

   patriciatrie_node_t * next = iter->tree->root;
   patriciatrie_node_t * parent;
   patriciatrie_node_t * lower_branch_parent = 0;
   do {
      parent = next;
      if (getbit(iter->tree, &nextk, next->bit_offset)) {
         lower_branch_parent = parent;
         next = next->right;
      } else {
         next = next->left;
      }
   } while (next->bit_offset > parent->bit_offset);

   if (lower_branch_parent) {
      parent = lower_branch_parent;
      next = parent->left;
      while (next->bit_offset > parent->bit_offset) {
         parent = next;
         next = next->right;
      }
      iter->next = next;
   } else {
      iter->next = 0;
   }

   return true;
}

// section: patriciatrie_prefixiter_t

// group: lifetime

int initfirst_patriciatrieprefixiter(/*out*/patriciatrie_prefixiter_t *iter, patriciatrie_t *tree, size_t len, const uint8_t prefixkey[len])
{
   patriciatrie_node_t * parent;
   patriciatrie_node_t * node = tree->root;
   size_t         prefix_bits = 8 * len;

   if (  (prefixkey || !len)
         && len < (((size_t)-1)/8)
         && node) {
      getkey_data_t prefk;
      initfullkey_getkeydata(&prefk, len, prefixkey);
      if (node->bit_offset < prefix_bits) {
         do {
            parent = node;
            if (getbit(tree, &prefk, node->bit_offset)) {
               node = node->right;
            } else {
               node = node->left;
            }
         } while (node->bit_offset > parent->bit_offset && node->bit_offset < prefix_bits);
      } else {
         parent = node;
         node = node->left;
      }
      while (node->bit_offset > parent->bit_offset) {
         parent = node;
         node = node->left;
      }
      getkey_data_t key;
      init1_getkeydata(&key, tree->keyadapt.getkey, cast_object(node, tree));
      if (key.streamsize >= prefk.streamsize) {
         uint8_t const *key2 = key.addr;
         for (size_t offset = 0; offset < len; ++offset) {
            if (offset == key.endoffset) {
               tree->keyadapt.getkey(&key, offset);
               key2 = key.addr;
            }
            if (prefixkey[offset] != *key2++) goto NO_PREFIX;
         }

      } else { NO_PREFIX:
         node = 0;
      }
   }

   iter->next        = node;
   iter->tree        = tree;
   iter->prefix_bits = prefix_bits;
   return 0;
}

// group: iterate

bool next_patriciatrieprefixiter(patriciatrie_prefixiter_t *iter, /*out*/patriciatrie_node_t ** node)
{
   if (!iter->next) return false;

   *node = iter->next;

   getkey_data_t nextk;
   init1_getkeydata(&nextk, iter->tree->keyadapt.getkey, cast_object(iter->next, iter->tree));

   patriciatrie_node_t * next = iter->tree->root;
   patriciatrie_node_t * parent;
   patriciatrie_node_t * higher_branch_parent = 0;
   do {
      parent = next;
      if (getbit(iter->tree, &nextk, next->bit_offset)) {
         next = next->right;
      } else {
         higher_branch_parent = parent;
         next = next->left;
      }
   } while (next->bit_offset > parent->bit_offset);

   if (  higher_branch_parent
         && higher_branch_parent->bit_offset >= iter->prefix_bits) {
      parent = higher_branch_parent;
      next = parent->right;
      while (next->bit_offset > parent->bit_offset) {
         parent = next;
         next = next->left;
      }
      iter->next = next;
   } else {
      iter->next = 0;
   }

   return true;
}


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

#define MAX_TREE_NODES 10000

typedef struct testnode_t {
   int      is_freed;
   int      is_used;
   patriciatrie_node_t  node;
   uint8_t  key[4];
   size_t   key_len;
   uint8_t  streamed_key[8/*2*sizeof(key)*/];
} testnode_t;

test_errortimer_t    s_errcounter;
unsigned             s_freenode_count;
unsigned             s_getbinkey_count;

static int impl_deletenode(void * obj)
{
   int err = 0;
   testnode_t * node = obj;

   if (! process_testerrortimer(&s_errcounter, &err)) {
      ++ s_freenode_count;
      ++ node->is_freed;
   }

   return err;
}

static void impl_gettestkey(/*inout*/getkey_data_t* key, size_t offset)
{
   int err;
   testnode_t *node = key->object;

   if (process_testerrortimer(&s_errcounter, &err)) {
      // only used for input param check in insert
      key->addr = 0;
      key->streamsize = 1;
   } else {
      ++ s_getbinkey_count;
      if (offset == 0) {   // init
         for (size_t i = 0; i < sizeof(node->key); ++i) {
            node->streamed_key[2*i+0] = node->key[i];
            node->streamed_key[2*i+1] = (uint8_t) ~ node->key[i];
         }
         init2_getkeydata(key, node->key_len, node->key_len ? 1 : 0, node->streamed_key);
      } else {
         assert(offset < node->key_len);
         update_getkeydata(key, offset, 1, node->streamed_key + 2 * offset);
      }
   }
}

typedef struct diffbitnode_t {
   size_t  keysize;
   size_t  nextsize;
   uint8_t key[256];
   getkey_data_t keydata;
   patriciatrie_node_t node;
} diffbitnode_t;

static void set_keyandnextsize(diffbitnode_t *node, size_t keysize, size_t nextsize)
{
   node->keysize  = keysize;
   node->nextsize = nextsize >= keysize ? keysize : nextsize + (nextsize == 0);
}

static void impl_getdiffbitkey(/*inout*/getkey_data_t* key, size_t offset)
{
   diffbitnode_t *diffbit = key->object;
   if (offset == 0) {
      init2_getkeydata(key, diffbit->keysize, diffbit->nextsize, diffbit->key);
      key->impl_ptr = diffbit;
   } else {
      assert(offset < key->streamsize && key->endoffset <= key->streamsize);
      offset -= (offset % diffbit->nextsize);
      size_t nextsize = offset + diffbit->nextsize <= key->streamsize
                      ? diffbit->nextsize : key->streamsize - offset;
      update_getkeydata(key, offset, nextsize, offset + diffbit->key);
   }
   assert(key->impl_ptr == diffbit);
}

static int test_searchhelper(void)
{
   patriciatrie_t tree;
   diffbitnode_t  node[2] = { { 0, 0, {0}, {0}, patriciatrie_node_INIT } };
   size_t         bitoff = 0;
   uint8_t        bitval = 0;

   // prepare
   init_patriciatrie(&tree, (getkey_adapter_t)getkey_adapter_INIT(offsetof(diffbitnode_t, node), &impl_getdiffbitkey));
   for (size_t i = 0; i < lengthof(node[0].key); ++i) {
      node[0].key[i] = (uint8_t)i;
      node[1].key[i] = (uint8_t)i;
   }

   // TEST is_key_equal: keylen == 0
   set_keyandnextsize(&node[0], 0, 0);
   initfullkey_getkeydata(&node[1].keydata, 0, 0);
   TEST(1 == is_key_equal(&tree, &node[0].node, &node[1].keydata));
   TEST(1 == is_key_equal(&tree, &node[0].node, &node[1].keydata));

   // TEST is_key_equal: equal keys with different streaming patterns
   for (size_t keysize = 1, nextsize = 1; keysize <= lengthof(node[0].key); ++keysize, ++nextsize) {
      // test
      set_keyandnextsize(&node[0], keysize, nextsize/3);
      set_keyandnextsize(&node[1], keysize, nextsize/2);
      init1_getkeydata(&node[1].keydata, &impl_getdiffbitkey, &node[1]);
      TEST( 1 == is_key_equal(&tree, &node[0].node, &node[1].keydata));
      // test
      set_keyandnextsize(&node[0], keysize, nextsize/2);
      set_keyandnextsize(&node[1], keysize, 1);
      init1_getkeydata(&node[1].keydata, &impl_getdiffbitkey, &node[1]);
      TEST( 1 == is_key_equal(&tree, &node[0].node, &node[1].keydata));
      // test
      set_keyandnextsize(&node[0], keysize, 3);
      set_keyandnextsize(&node[1], keysize, keysize);
      init1_getkeydata(&node[1].keydata, &impl_getdiffbitkey, &node[1]);
      TEST( 1 == is_key_equal(&tree, &node[0].node, &node[1].keydata));
   }

   // TEST is_key_equal: unequal keys with different streaming patterns
   for (size_t keysize = 1, nextsize = 1; keysize <= lengthof(node[0].key); ++keysize, ++nextsize) {
      // prepare
      node[0].key[keysize/3] ^= 0xff;
      // test
      set_keyandnextsize(&node[0], keysize, nextsize/3);
      set_keyandnextsize(&node[1], keysize, nextsize/2);
      init1_getkeydata(&node[1].keydata, &impl_getdiffbitkey, &node[1]);
      TEST( 0 == is_key_equal(&tree, &node[0].node, &node[1].keydata));
      // test
      set_keyandnextsize(&node[0], keysize, nextsize/2);
      set_keyandnextsize(&node[1], keysize, 1);
      init1_getkeydata(&node[1].keydata, &impl_getdiffbitkey, &node[1]);
      TEST( 0 == is_key_equal(&tree, &node[0].node, &node[1].keydata));
      // test
      set_keyandnextsize(&node[0], keysize, 3);
      set_keyandnextsize(&node[1], keysize, keysize);
      init1_getkeydata(&node[1].keydata, &impl_getdiffbitkey, &node[1]);
      TEST( 0 == is_key_equal(&tree, &node[0].node, &node[1].keydata));
      // reset
      node[0].key[keysize/3] ^= 0xff;
   }

   // TEST get_first_different_bit: keylen == 0
   memset(&node, 0, sizeof(node));
   for (unsigned i = 0; i < 2; ++i) {
      init1_getkeydata(&node[i].keydata, &impl_getdiffbitkey, &node[i]);
   }
   TEST(EEXIST == get_first_different_bit(&tree, &node[0].keydata, &node[1].keydata, &bitoff, &bitval));

   // TEST get_first_different_bit: equal keys with different streaming patterns
   for (size_t keysize = 1, nextsize = 1; keysize <= 256; keysize *= 2, ++nextsize) {
      set_keyandnextsize(&node[0], keysize, nextsize);
      set_keyandnextsize(&node[1], keysize, nextsize/2);
      init1_getkeydata(&node[0].keydata, &impl_getdiffbitkey, &node[0]);
      init1_getkeydata(&node[1].keydata, &impl_getdiffbitkey, &node[1]);
      TEST(EEXIST == get_first_different_bit(&tree, &node[0].keydata, &node[1].keydata, &bitoff, &bitval));
      // keydata is reset if offset > 0
      TEST(EEXIST == get_first_different_bit(&tree, &node[1].keydata, &node[0].keydata, &bitoff, &bitval));
      node[0].nextsize = keysize;
      TEST(EEXIST == get_first_different_bit(&tree, &node[0].keydata, &node[1].keydata, &bitoff, &bitval));
      TEST(EEXIST == get_first_different_bit(&tree, &node[1].keydata, &node[0].keydata, &bitoff, &bitval));
   }

   // TEST get_first_different_bit: keys are equal until end marker 0xFF of node[0].key
   memset(node[0].key, 0, sizeof(node[0].key));
   memset(node[1].key, 0, sizeof(node[0].key));
   node[0].key[2] = 0xFF; // end marker of node[1].key
   set_keyandnextsize(&node[1], 2, 2);
   init1_getkeydata(&node[1].keydata, &impl_getdiffbitkey, &node[1]);
   for (unsigned len = sizeof(node[0].key)-4; len < sizeof(node[0].key); ++len) {
      set_keyandnextsize(&node[0], len, 1);
      init1_getkeydata(&node[0].keydata, &impl_getdiffbitkey, &node[0]);
      for (unsigned tc = 0; tc < 2; ++tc) {
         // test
         TEST( 0 == get_first_different_bit(&tree, &node[tc].keydata, &node[!tc].keydata, &bitoff, &bitval));
         // check out param
         TEST( bitval == (tc ? 0x80 : 0));
         TEST( bitoff == 8*len);
      }
   }

   // TEST get_first_different_bit: keys differ in end marker of node[1].key
   set_keyandnextsize(&node[0], sizeof(node[0].key), sizeof(node[0].key));
   set_keyandnextsize(&node[1], 2, 1);
   init1_getkeydata(&node[0].keydata, &impl_getdiffbitkey, &node[0]);
   init1_getkeydata(&node[1].keydata, &impl_getdiffbitkey, &node[1]);
   for (unsigned i = 1; i < 8; ++i) {
      for (unsigned tc = 0; tc < 2; ++tc) {
         node[0].key[2] = (uint8_t)(0xFF << i);
         TEST( 0 == get_first_different_bit(&tree, &node[tc].keydata, &node[!tc].keydata, &bitoff, &bitval));
         // check out param
         TEST( bitval == (tc ? 0 : 0x01 << (i-1)));
         TEST( bitoff == 24u-i);
      }
   }
   // reset
   node[0].key[2] = 0;

   // TEST get_first_different_bit: keys differ in 0 extension of node[1].key
   set_keyandnextsize(&node[0], sizeof(node[0].key), sizeof(node[0].key)/3);
   init1_getkeydata(&node[0].keydata, &impl_getdiffbitkey, &node[0]);
   for (unsigned len = 0; len < 16; ++len) {
      node[0].key[len] = 0xFF; // end marker of node[1].key
      set_keyandnextsize(&node[1], len, len/2);
      init1_getkeydata(&node[1].keydata, &impl_getdiffbitkey, &node[1]);
      for (unsigned doff = len+1; doff < sizeof(node[0].key); doff *= 2) {
         node[0].key[doff] = (uint8_t) (1u << (doff&7));
         for (unsigned tc = 0; tc < 2; ++tc) {
            // test
            TEST( 0 == get_first_different_bit(&tree, &node[tc].keydata, &node[!tc].keydata, &bitoff, &bitval));
            // check out param
            TEST( bitval == (tc ? 0x01 << (doff&7) : 0));
            TEST( bitoff == 8u*doff + 7 - (doff&7));
         }
         node[0].key[doff] = 0;
      }
      // reset
      node[0].key[len] = 0;
   }

   // TEST get_first_different_bit: keys differ in different pos
   for (size_t i = 0; i < lengthof(node[0].key); ++i) {
      node[0].key[i] = (uint8_t)i;
      node[1].key[i] = (uint8_t)i;
   }
   set_keyandnextsize(&node[0], sizeof(node[0].key), sizeof(node[0].key)/3);
   set_keyandnextsize(&node[1], sizeof(node[1].key), sizeof(node[1].key)/4);
   init1_getkeydata(&node[0].keydata, &impl_getdiffbitkey, &node[0]);
   init1_getkeydata(&node[1].keydata, &impl_getdiffbitkey, &node[1]);
   for (unsigned pos = 0; pos < sizeof(node[0].key)-1; ++pos) {
      for (unsigned bit = 0; bit < 8; ++bit) {
         node[0].key[pos]   ^= (uint8_t) (0xff >> bit);
         node[0].key[pos+1] ^= (uint8_t) 0xff;
         for (unsigned tc = 0; tc < 2; ++tc) {
            // test
            TEST( 0 == get_first_different_bit(&tree, &node[tc].keydata, &node[!tc].keydata, &bitoff, &bitval));
            // check out param
            TEST( bitval == ((tc ? ~node[1].key[pos] : node[1].key[pos]) & (0x80>>bit)));
            TEST( bitoff == 8u*pos + bit);
         }
         // reset
         node[0].key[pos]   = (uint8_t) pos;
         node[0].key[pos+1] = (uint8_t) (pos+1);
      }
   }

   // TEST get_first_different_bit: keys differ in length
   set_keyandnextsize(&node[0], sizeof(node[0].key), sizeof(node[0].key)/3);
   init1_getkeydata(&node[0].keydata, &impl_getdiffbitkey, &node[0]);
   for (unsigned len = 0; len < sizeof(node[0].key)-1; ++len) {
      set_keyandnextsize(&node[1], len, len/4);
      init1_getkeydata(&node[1].keydata, &impl_getdiffbitkey, &node[1]);
      unsigned nrbits = 0;
      while ((0x80 >> nrbits) & node[1].key[len]) ++ nrbits;
      for (unsigned tc = 0; tc < 2; ++tc) {
         // test
         TEST( 0 == get_first_different_bit(&tree, &node[tc].keydata, &node[!tc].keydata, &bitoff, &bitval));
         // check out param
         TEST( bitval == (tc ? 0 : (0x80 >> nrbits)));
         TEST( bitoff == 8u*len + nrbits);
      }
   }

   // TEST get_first_different_bit: equal keys
   for (unsigned len = 0; len < sizeof(node[0].key)-1; ++len) {
      set_keyandnextsize(&node[0], len, len%5);
      set_keyandnextsize(&node[1], len, len%7);
      init1_getkeydata(&node[0].keydata, &impl_getdiffbitkey, &node[0]);
      init1_getkeydata(&node[1].keydata, &impl_getdiffbitkey, &node[1]);
      for (unsigned tc = 0; tc < 2; ++tc) {
         // test
         TEST( EEXIST == get_first_different_bit(&tree, &node[tc].keydata, &node[!tc].keydata, &bitoff, &bitval));
      }
   }

   // TEST getbitinit: offset == 0 ==> does not change
   set_keyandnextsize(&node[0], sizeof(node[0].key), 1);
   init1_getkeydata(&node[0].keydata, &impl_getdiffbitkey, &node[0]);
   for (size_t i = 0; i < node[0].keysize; ++i) {
      getbitinit(&tree, &node[0].keydata, 8*i);
      TEST( 0 == node[0].keydata.offset);
   }

   // TEST getbitinit: offset set to bitoffset
   set_keyandnextsize(&node[0], sizeof(node[0].key), 1);
   init1_getkeydata(&node[0].keydata, &impl_getdiffbitkey, &node[0]);
   for (size_t i = node[0].keysize; i-- > 0; ) {
      for (size_t i2 = 0; i2 < 8; ++i2) {
         // prepare
         impl_getdiffbitkey(&node[0].keydata, node[0].keysize-1);
         TEST(node[0].keysize-1 == node[0].keydata.offset);
         // test
         getbitinit(&tree, &node[0].keydata, 8*i + 7 - i2);
         // check node[0].keydata
         TEST( i   == node[0].keydata.offset);
         TEST( i+1 == node[0].keydata.endoffset);
      }
   }

   // TEST getbit: 0 <= bitoffset < keysize*8
   set_keyandnextsize(&node[0], sizeof(node[0].key), 1);
   init1_getkeydata(&node[0].keydata, &impl_getdiffbitkey, &node[0]);
   for (size_t i = 0; i < 8*node[0].keysize; ++i) {
      uint8_t b = (uint8_t) ((i/8) & ((size_t)0x80 >> (i%8)));
      // test
      TEST( b     == getbit(&tree, &node[0].keydata, i));
      // check node[0].keydata
      TEST( i/8   == node[0].keydata.offset);
      TEST( i/8+1 == node[0].keydata.endoffset);
   }

   // TEST getbit: keysize*8 <= bitoffset < (keysize+1)*8
   set_keyandnextsize(&node[0], sizeof(node[0].key), 1);
   init1_getkeydata(&node[0].keydata, &impl_getdiffbitkey, &node[0]);
   for (size_t i = 0; i < 8; ++i) {
      // test
      TEST( 1 == getbit(&tree, &node[0].keydata, 8*node[0].keysize + i));
      // check node[0].keydata
      TEST( 0 == node[0].keydata.offset);
      TEST( 1 == node[0].keydata.endoffset);
   }

   // TEST getbit: bitoffset >= (keysize+1)*8
   for (size_t i = 0; i < 32; ++i) {
      // test
      TEST( 0 == getbit(&tree, &node[0].keydata, 8*(node[0].keysize+1) + i));
      // check node[0].keydata
      TEST( 0 == node[0].keydata.offset);
      TEST( 1 == node[0].keydata.endoffset);
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_initfree(void)
{
   size_t const            NODEOFFSET = offsetof(testnode_t, node);
   getkey_adapter_t        keyadapt = getkey_adapter_INIT(NODEOFFSET, &impl_gettestkey);
   patriciatrie_t          tree = patriciatrie_FREE;
   patriciatrie_node_t     node = patriciatrie_node_INIT;
   testnode_t              nodes[20];

   // prepare
   free_testerrortimer(&s_errcounter);
   memset(nodes, 0, sizeof(nodes));
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      nodes[i].key_len = sizeof(nodes[i].key);
      nodes[i].key[0] = (uint8_t) i;
   }

   // TEST patriciatrie_node_INIT
   TEST(0 == node.bit_offset);
   TEST(0 == node.left);
   TEST(0 == node.right);

   // TEST patriciatrie_FREE
   TEST(0 == tree.root);
   TEST(0 == tree.keyadapt.nodeoffset);
   TEST(0 == tree.keyadapt.getkey);

   // TEST patriciatrie_INIT
   tree = (patriciatrie_t) patriciatrie_INIT((void*)7, keyadapt);
   TEST(tree.root                == (void*)7);
   TEST(tree.keyadapt.nodeoffset == keyadapt.nodeoffset);
   TEST(tree.keyadapt.getkey     == keyadapt.getkey);
   tree = (patriciatrie_t) patriciatrie_INIT((void*)0, getkey_adapter_INIT(0,0));
   TEST(0 == tree.root);
   TEST(0 == tree.keyadapt.nodeoffset);
   TEST(0 == tree.keyadapt.getkey);

   // TEST init_patriciatrie, free_patriciatrie
   tree.root = (void*)1;
   init_patriciatrie(&tree, keyadapt);
   TEST(0 == tree.root);
   TEST(1 == isequal_getkeyadapter(&tree.keyadapt, &keyadapt));
   TEST(0 == free_patriciatrie(&tree, 0));
   TEST(0 == tree.root);
   TEST(1 == isequal_getkeyadapter(&tree.keyadapt, &(getkey_adapter_t) getkey_adapter_INIT(0,0)));
   TEST(0 == free_patriciatrie(&tree, 0)); // double free
   TEST(0 == tree.root);
   TEST(1 == isequal_getkeyadapter(&tree.keyadapt, &(getkey_adapter_t) getkey_adapter_INIT(0,0)));

   // TEST free_patriciatrie: frees all nodes
   init_patriciatrie(&tree, keyadapt);
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(0 == insert_patriciatrie(&tree, &nodes[i].node, 0));
   }
   TEST(0 != tree.root);
   TEST(1 == isequal_getkeyadapter(&tree.keyadapt, &keyadapt));
   s_freenode_count = 0;
   TEST(0 == free_patriciatrie(&tree, &impl_deletenode));
   TEST(lengthof(nodes) == s_freenode_count);
   TEST(0 == tree.root);
   TEST(1 == isequal_getkeyadapter(&tree.keyadapt, &(getkey_adapter_t) getkey_adapter_INIT(0,0)));
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(1 == nodes[i].is_freed);
      TEST(0 == nodes[i].node.bit_offset);
      TEST(0 == nodes[i].node.left);
      TEST(0 == nodes[i].node.right);
      nodes[i].is_freed = 0;
   }

   // TEST free_patriciatrie: lifetime.delete_object set to 0
   init_patriciatrie(&tree, keyadapt);
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(0 == insert_patriciatrie(&tree, &nodes[i].node, 0));
   }
   TEST(0 != tree.root);
   TEST(1 == isequal_getkeyadapter(&tree.keyadapt, &keyadapt));
   s_freenode_count = 0;
   TEST(0 == free_patriciatrie(&tree, 0));
   TEST(0 == s_freenode_count);
   TEST(0 == tree.root);
   TEST(1 == isequal_getkeyadapter(&tree.keyadapt, &(getkey_adapter_t) getkey_adapter_INIT(0,0)));
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(0 == nodes[i].is_freed);
      TEST(0 == nodes[i].node.bit_offset);
      TEST(0 == nodes[i].node.left);
      TEST(0 == nodes[i].node.right);
   }

   // TEST free_patriciatrie: ERROR
   init_patriciatrie(&tree, keyadapt);
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(0 == insert_patriciatrie(&tree, &nodes[i].node, 0));
   }
   TEST(0 != tree.root);
   TEST(1 == isequal_getkeyadapter(&tree.keyadapt, &keyadapt));
   s_freenode_count = 0;
   init_testerrortimer(&s_errcounter, 4, ENOENT);
   TEST(ENOENT == free_patriciatrie(&tree, &impl_deletenode));
   TEST(lengthof(nodes)-1 == s_freenode_count);
   TEST(0 == tree.root);
   TEST(1 == isequal_getkeyadapter(&tree.keyadapt, &(getkey_adapter_t) getkey_adapter_INIT(0,0)));
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST((i != 2) == nodes[i].is_freed);
      TEST(0 == nodes[i].node.bit_offset);
      TEST(0 == nodes[i].node.left);
      TEST(0 == nodes[i].node.right);
   }

   // TEST getinistate_patriciatrie
   patriciatrie_node_t * saved_root   = (void*) 1;
   getkey_adapter_t saved_keyadapt = keyadapt;
   tree = (patriciatrie_t)patriciatrie_FREE;
   getinistate_patriciatrie(&tree, &saved_root, 0);
   TEST(0 == saved_root);
   getinistate_patriciatrie(&tree, &saved_root, &saved_keyadapt);
   TEST(0 == saved_root);
   TEST(1 == isequal_getkeyadapter(&saved_keyadapt, &(getkey_adapter_t) getkey_adapter_INIT(0,0)));
   tree = (patriciatrie_t)patriciatrie_INIT(&nodes[0].node, keyadapt);
   getinistate_patriciatrie(&tree, &saved_root, 0);
   TEST(&nodes[0].node == saved_root);
   getinistate_patriciatrie(&tree, &saved_root, &saved_keyadapt);
   TEST(&nodes[0].node == saved_root);
   TEST(1 == isequal_getkeyadapter(&tree.keyadapt, &keyadapt));

   // TEST isempty_patriciatrie
   tree.root = (void*)1;
   TEST(0 == isempty_patriciatrie(&tree));
   tree.root = (void*)0;
   TEST(1 == isempty_patriciatrie(&tree));

   return 0;
ONERR:
   return EINVAL;
}

static int test_insertremove(void)
{
   size_t const         NODEOFFSET = offsetof(testnode_t, node);
   getkey_adapter_t     keyadapt  = getkey_adapter_INIT(NODEOFFSET, &impl_gettestkey);
   patriciatrie_t       tree      = patriciatrie_FREE;
   memblock_t           memblock  = memblock_FREE;
   testnode_t           * nodes   = 0;
   patriciatrie_node_t  * node;
   patriciatrie_node_t  * existing_node;
   unsigned             nodecount;

   // prepare
   free_testerrortimer(&s_errcounter);
   TEST(0 == RESIZE_MM(sizeof(testnode_t) * MAX_TREE_NODES, &memblock));
   nodes = (testnode_t*)memblock.addr;
   memset(nodes, 0, sizeof(testnode_t) * MAX_TREE_NODES);
   for (unsigned i = 0; i < MAX_TREE_NODES; ++i) {
      nodes[i].key_len = sizeof(nodes[i].key);
   }
   existing_node = (void*)1;

   // TEST insert_patriciatrie: empty key (length == 0)
   init_patriciatrie(&tree, keyadapt);
   nodes[0].key_len = 0;
   TEST( 0 == insert_patriciatrie(&tree, &nodes[0].node, &existing_node));
   // check existing_node (not changed)
   TEST( 1 == (uintptr_t)existing_node);
   // check tree
   node = 0;
   TEST(0 == find_patriciatrie(&tree, 0, 0, &node));
   TEST(&nodes[0].node == node);
   TEST(0 == removenodes_patriciatrie(&tree, &impl_deletenode));
   TEST(1 == nodes[0].is_freed);
   nodes[0].key_len = sizeof(nodes[0].key);
   nodes[0].is_freed = 0;

   // TEST insert_patriciatrie: differ in bit 0 (single node case)
   init_patriciatrie(&tree, keyadapt);
   TEST(0 == insert_patriciatrie(&tree, &nodes[0].node, 0));
   // test
   nodes[1].key[0] = 0x80;
   TEST( 0 == insert_patriciatrie(&tree, &nodes[1].node, &existing_node));
   // check existing_node (not changed)
   TEST( 1 == (uintptr_t)existing_node);
   // check tree
   TEST( tree.root == &nodes[0].node);
   // check nodes[0]
   TEST( nodes[0].node.bit_offset == 0);
   TEST( nodes[0].node.left       == &nodes[0].node);
   TEST( nodes[0].node.right      == &nodes[1].node);
   TEST( 0 == find_patriciatrie(&tree, 4, (uint8_t[]) { 0, 0, 0, 0 }, &node));
   TEST( node == &nodes[0].node)
   // check nodes[1]
   TEST( nodes[1].node.bit_offset == 0);
   TEST( nodes[1].node.left       == &nodes[1].node);
   TEST( nodes[1].node.right      == &nodes[1].node);
   TEST( 0 == find_patriciatrie(&tree, 4, (uint8_t[]) { 0x80, 0, 0, 0 }, &node));
   TEST( node == &nodes[1].node)
   // reset
   s_freenode_count = 0;
   TEST(0 == removenodes_patriciatrie(&tree, &impl_deletenode));
   TEST(2 == s_freenode_count);
   for (unsigned i = 0; i < 2; ++i) {
      TEST(1 == nodes[i].is_freed);
      nodes[i].is_freed = 0;
      nodes[i].key[0] = 0;
   }

   // TEST insert_patriciatrie: differ in bit 0 (two node case)
   init_patriciatrie(&tree, keyadapt);
   TEST(0 == insert_patriciatrie(&tree, &nodes[0].node, 0));
   nodes[1].key[0] = 0x01;
   TEST(0 == insert_patriciatrie(&tree, &nodes[1].node, 0));
   // test
   nodes[2].key[0] = 0x81;
   TEST( 0 == insert_patriciatrie(&tree, &nodes[2].node, &existing_node));
   // check existing_node (not changed)
   TEST( 1 == (uintptr_t)existing_node);
   // check tree
   TEST( tree.root == &nodes[2].node);
   // check nodes[0]
   TEST( nodes[0].node.bit_offset == 7);
   TEST( nodes[0].node.left       == &nodes[0].node);
   TEST( nodes[0].node.right      == &nodes[1].node);
   TEST( 0 == find_patriciatrie(&tree, 4, (uint8_t[]) { 0, 0, 0, 0 }, &node));
   TEST( node == &nodes[0].node)
   // check nodes[1]
   TEST( nodes[1].node.bit_offset == 0);
   TEST( nodes[1].node.left       == &nodes[1].node);
   TEST( nodes[1].node.right      == &nodes[1].node);
   TEST( 0 == find_patriciatrie(&tree, 4, (uint8_t[]) { 0x01, 0, 0, 0 }, &node));
   TEST( node == &nodes[1].node)
   // check nodes[2]
   TEST( nodes[2].node.bit_offset == 0);
   TEST( nodes[2].node.left       == &nodes[0].node);
   TEST( nodes[2].node.right      == &nodes[2].node);
   TEST( 0 == find_patriciatrie(&tree, 4, (uint8_t[]) { 0x81, 0, 0, 0 }, &node));
   TEST( node == &nodes[2].node)
   // reset
   s_freenode_count = 0;
   TEST(0 == removenodes_patriciatrie(&tree, &impl_deletenode));
   TEST(3 == s_freenode_count);
   for (unsigned i = 0; i < 3; ++i) {
      TEST(1 == nodes[i].is_freed);
      nodes[i].is_freed = 0;
      nodes[i].key[0] = 0;
   }

   // TEST insert_patriciatrie: case parent == node == tree.root
   init_patriciatrie(&tree, keyadapt);
   TEST(0 == insert_patriciatrie(&tree, &nodes[0].node, 0));
   nodes[1].key[0] = 0x80;
   TEST(0 == insert_patriciatrie(&tree, &nodes[1].node, 0));
   // test
   nodes[2].key[0] = 0x01;
   TEST( 0 == insert_patriciatrie(&tree, &nodes[2].node, &existing_node));
   // check existing_node (not changed)
   TEST( 1 == (uintptr_t)existing_node);
   // check tree
   TEST( tree.root == &nodes[0].node);
   // check nodes[0]
   TEST( nodes[0].node.bit_offset == 0);
   TEST( nodes[0].node.left       == &nodes[2].node);
   TEST( nodes[0].node.right      == &nodes[1].node);
   // check nodes[1]
   TEST( nodes[1].node.bit_offset == 0);
   TEST( nodes[1].node.left       == &nodes[1].node);
   TEST( nodes[1].node.right      == &nodes[1].node);
   // check nodes[2]
   TEST( nodes[2].node.bit_offset == 7);
   TEST( nodes[2].node.left       == &nodes[0].node);
   TEST( nodes[2].node.right      == &nodes[2].node);
   TEST( 0 == find_patriciatrie(&tree, 4, (uint8_t[]) { 0x01, 0, 0, 0 }, &node));
   TEST( node == &nodes[2].node)
   // reset
   s_freenode_count = 0;
   TEST(0 == removenodes_patriciatrie(&tree, &impl_deletenode));
   TEST(3 == s_freenode_count);
   for (unsigned i = 0; i < 3; ++i) {
      TEST(1 == nodes[i].is_freed);
      nodes[i].is_freed = 0;
      nodes[i].key[0] = 0;
   }

   // TEST insert_patriciatrie: EINVAL
   for (unsigned tc = 0; tc <= 3; ++tc) {
      if (tc < 2) {
         nodes[0].key_len = ((size_t)-1)/8;
      } else {
         init_testerrortimer(&s_errcounter, 1, EPIPE);
      }
      existing_node = (tc & 1) ? (void*)1 : (void*)0;
      TEST( EINVAL == insert_patriciatrie(&tree, &nodes[0].node, (tc & 1) ? &existing_node : 0/*not returned*/));
      // check existing_node
      TEST( 0 == existing_node);
      // reset
      nodes[0].key_len = sizeof(nodes[0].key);
   }

   // TEST insert_patriciatrie: EEXIST
   TEST(0 == insert_patriciatrie(&tree, &nodes[0].node, 0));
   for (uintptr_t tc = 0; tc <= 1; ++tc) {
      existing_node = tc ? (patriciatrie_node_t*)0 : &nodes[0].node;
      TEST( EEXIST == insert_patriciatrie(&tree, &nodes[0].node, tc ? &existing_node : 0/*not returned*/));
      // check existing_node
      TEST( &nodes[0].node == existing_node);
   }
   // reset
   init_patriciatrie(&tree, keyadapt);

   // TEST EINVAL
   TEST(EINVAL == find_patriciatrie(&tree, 1, 0, &node));
   TEST(EINVAL == find_patriciatrie(&tree, ((size_t)-1), nodes[0].key, &node));
   TEST(EINVAL == find_patriciatrie(&tree, ((size_t)-1)/8, nodes[0].key, &node));
   TEST(EINVAL == remove_patriciatrie(&tree, 1, 0, &node));
   TEST(EINVAL == remove_patriciatrie(&tree, ((size_t)-1)/8, nodes[0].key, &node));

   // TEST insert_patriciatrie, removenodes_patriciatrie: special case
   /*           (0111)
    *       /     ^     \
    *     (0011)  |   > (1111) <
    *      /  ^|__|   |___||___|
    * > (0001)|
    * |__| |__|
    */
   static_assert(sizeof(nodes[0].key) == 4, "code assumes 4 byte keys");
   memcpy(nodes[0].key, (uint8_t[4]){ 0, 1, 1, 1 }, 4);
   TEST(0 == insert_patriciatrie(&tree, &nodes[0].node, 0));
   memcpy(nodes[1].key, (uint8_t[4]){ 1, 1, 1, 1 }, 4);
   TEST(0 == insert_patriciatrie(&tree, &nodes[1].node, 0));
   memcpy(nodes[2].key, (uint8_t[4]){ 0, 0, 1, 1 }, 4);
   TEST(0 == insert_patriciatrie(&tree, &nodes[2].node, 0));
   memcpy(nodes[3].key, (uint8_t[4]){ 0, 0, 0, 1 }, 4);
   TEST(0 == insert_patriciatrie(&tree, &nodes[3].node, 0));
   TEST(0 == removenodes_patriciatrie(&tree, &impl_deletenode));
   for (int i = 0; i < 4; ++i) {
      TEST(1 == nodes[i].is_freed);
      nodes[i].is_freed = 0;
   }

   // TEST insert_patriciatrie, removenodes_patriciatrie: special case
   /*           (0111)
    *       /     ^     \
    * --->(0001)  |   > (1111) <
    * |    /   |__|   |___||___|
    * | (0011)<
    * |__| |__|
    */
   static_assert(sizeof(nodes[0].key) == 4, "code assumes 4 byte keys");
   memcpy(nodes[0].key, (uint8_t[4]){ 0, 1, 1, 1 }, 4);
   TEST(0 == insert_patriciatrie(&tree, &nodes[0].node, 0));
   memcpy(nodes[1].key, (uint8_t[4]){ 1, 1, 1, 1 }, 4);
   TEST(0 == insert_patriciatrie(&tree, &nodes[1].node, 0));
   memcpy(nodes[2].key, (uint8_t[4]){ 0, 0, 0, 1 }, 4);
   TEST(0 == insert_patriciatrie(&tree, &nodes[2].node, 0));
   memcpy(nodes[3].key, (uint8_t[4]){ 0, 0, 1, 1 }, 4);
   TEST(0 == insert_patriciatrie(&tree, &nodes[3].node, 0));
   TEST(0 == removenodes_patriciatrie(&tree, &impl_deletenode));
   for (int i = 0; i < 4; ++i) {
      TEST(1 == nodes[i].is_freed);
      nodes[i].is_freed = 0;
   }

   // TEST removenodes_patriciatrie: special dual case
   /*           (1111)  <-------
    *       /         \        |
    *   > (0000) <      (1011)--
    *   |___||___|      /  ^
    *             > (1000) |
    *             |___||___|
    *
    *
    */
   memcpy(nodes[0].key, (uint8_t[4]){ 1, 1, 1, 1 }, 4);
   TEST(0 == insert_patriciatrie(&tree, &nodes[0].node, 0));
   memcpy(nodes[1].key, (uint8_t[4]){ 0, 0, 0, 0 }, 4);
   TEST(0 == insert_patriciatrie(&tree, &nodes[1].node, 0));
   memcpy(nodes[2].key, (uint8_t[4]){ 1, 0, 1, 1 }, 4);
   TEST(0 == insert_patriciatrie(&tree, &nodes[2].node, 0));
   memcpy(nodes[3].key, (uint8_t[4]){ 1, 0, 0, 0 }, 4);
   TEST(0 == insert_patriciatrie(&tree, &nodes[3].node, 0));
   TEST(0 == removenodes_patriciatrie(&tree, &impl_deletenode));
   for (int i = 0; i < 4; ++i) {
      TEST(1 == nodes[i].is_freed);
      nodes[i].is_freed = 0;
   }

   // TEST remove_patriciatrie: special case
   /*           (1111)  <--------------
    *       /         \               |
    *   > (0000) <      (1011)        |
    *   |___||___|      /  ^  \       |
    *             > (1000) |   (1101) |
    *             |___||___|    / ^ |_|
    *                     > (1100)|
    *                     |___||__|
    */
   assert(sizeof(nodes[0].key) == 4);
   memcpy(nodes[0].key, (uint8_t[4]){ 1, 1, 1, 1 }, 4);
   TEST(0 == insert_patriciatrie(&tree, &nodes[0].node, 0));
   memcpy(nodes[1].key, (uint8_t[4]){ 0, 0, 0, 0 }, 4);
   TEST(0 == insert_patriciatrie(&tree, &nodes[1].node, 0));
   memcpy(nodes[2].key, (uint8_t[4]){ 1, 0, 1, 1 }, 4);
   TEST(0 == insert_patriciatrie(&tree, &nodes[2].node, 0));
   memcpy(nodes[3].key, (uint8_t[4]){ 1, 0, 0, 0 }, 4);
   TEST(0 == insert_patriciatrie(&tree, &nodes[3].node, 0));
   memcpy(nodes[4].key, (uint8_t[4]){ 1, 1, 0, 1 }, 4);
   TEST(0 == insert_patriciatrie(&tree, &nodes[4].node, 0));
   memcpy(nodes[5].key, (uint8_t[4]){ 1, 1, 0, 0 }, 4);
   TEST(0 == insert_patriciatrie(&tree, &nodes[5].node, 0));
   TEST(tree.root == &nodes[0].node);
   TEST(nodes[0].node.left == &nodes[1].node);
   TEST(nodes[0].node.right == &nodes[2].node);
   TEST(nodes[2].node.left == &nodes[3].node);
   TEST(nodes[2].node.right == &nodes[4].node);
   TEST(nodes[4].node.left == &nodes[5].node);
   TEST(nodes[4].node.right == &nodes[0].node);
   TEST(0 == remove_patriciatrie(&tree, 4, nodes[0].key, &node));
   TEST(tree.root == &nodes[4].node);
   TEST(nodes[4].node.left == &nodes[1].node);
   TEST(nodes[4].node.right == &nodes[2].node);
   TEST(nodes[2].node.left == &nodes[3].node);
   TEST(nodes[2].node.right == &nodes[5].node);
   TEST(nodes[5].node.left == &nodes[5].node);
   TEST(nodes[5].node.right == &nodes[4].node);
   TEST(0 == remove_patriciatrie(&tree, 4, nodes[5].key, &node));
   TEST(tree.root == &nodes[4].node);
   TEST(nodes[4].node.left == &nodes[1].node);
   TEST(nodes[4].node.right == &nodes[2].node);
   TEST(nodes[2].node.left == &nodes[3].node);
   TEST(nodes[2].node.right == &nodes[4].node);
   TEST(0 == remove_patriciatrie(&tree, 4, nodes[4].key, &node));
   TEST(tree.root == &nodes[2].node);
   TEST(nodes[2].node.left == &nodes[1].node);
   TEST(nodes[2].node.right == &nodes[3].node);
   TEST(nodes[3].node.left == &nodes[3].node);
   TEST(nodes[3].node.right == &nodes[2].node);
   TEST(0 == remove_patriciatrie(&tree, 4, nodes[1].key, &node));
   TEST(tree.root == &nodes[3].node);
   TEST(nodes[3].node.left == &nodes[3].node);
   TEST(nodes[3].node.right == &nodes[2].node);
   TEST(nodes[2].node.left == &nodes[2].node);
   TEST(nodes[2].node.right == &nodes[2].node);
   TEST(0 == removenodes_patriciatrie(&tree, &impl_deletenode));
   TEST(1 == nodes[2].is_freed);
   TEST(1 == nodes[3].is_freed);
   nodes[3].is_freed = 0;
   nodes[2].is_freed = 0;

   // TEST remove_patriciatrie: special dual case
   /*   ------->  (0000)
    *   |       /        \
    *   |    (0100)      (1111)
    *   |   /     ^ \
    *   | (0010)  | (0110) <
    *   |_| ^ \   |___||___|
    *       |(0011) <
    *       |__||___|
    */
   assert(sizeof(nodes[0].key) == 4);
   memcpy(nodes[0].key, (uint8_t[4]){ 0, 0, 0, 0 }, 4);
   TEST(0 == insert_patriciatrie(&tree, &nodes[0].node, 0));
   memcpy(nodes[1].key, (uint8_t[4]){ 1, 1, 1, 1 }, 4);
   TEST(0 == insert_patriciatrie(&tree, &nodes[1].node, 0));
   memcpy(nodes[2].key, (uint8_t[4]){ 0, 1, 0, 0 }, 4);
   TEST(0 == insert_patriciatrie(&tree, &nodes[2].node, 0));
   memcpy(nodes[3].key, (uint8_t[4]){ 0, 1, 1, 0 }, 4);
   TEST(0 == insert_patriciatrie(&tree, &nodes[3].node, 0));
   memcpy(nodes[4].key, (uint8_t[4]){ 0, 0, 1, 0 }, 4);
   TEST(0 == insert_patriciatrie(&tree, &nodes[4].node, 0));
   memcpy(nodes[5].key, (uint8_t[4]){ 0, 0, 1, 1 }, 4);
   TEST(0 == insert_patriciatrie(&tree, &nodes[5].node, 0));
   TEST(tree.root == &nodes[0].node);
   TEST(nodes[0].node.left == &nodes[2].node);
   TEST(nodes[0].node.right == &nodes[1].node);
   TEST(nodes[2].node.left == &nodes[4].node);
   TEST(nodes[2].node.right == &nodes[3].node);
   TEST(nodes[4].node.left == &nodes[0].node);
   TEST(nodes[4].node.right == &nodes[5].node);
   TEST(0 == remove_patriciatrie(&tree, sizeof(nodes[0].key), nodes[0].key, &node));
   TEST(tree.root == &nodes[4].node);
   TEST(nodes[4].node.left == &nodes[2].node);
   TEST(nodes[4].node.right == &nodes[1].node);
   TEST(nodes[2].node.left == &nodes[5].node);
   TEST(nodes[2].node.right == &nodes[3].node);
   TEST(nodes[5].node.left == &nodes[4].node);
   TEST(nodes[5].node.right == &nodes[5].node);
   TEST(0 == remove_patriciatrie(&tree, 4, nodes[5].key, &node));
   TEST(tree.root == &nodes[4].node);
   TEST(nodes[4].node.left == &nodes[2].node);
   TEST(nodes[4].node.right == &nodes[1].node);
   TEST(nodes[2].node.left == &nodes[4].node);
   TEST(nodes[2].node.right == &nodes[3].node);
   TEST(0 == remove_patriciatrie(&tree, 4, nodes[4].key, &node));
   TEST(tree.root == &nodes[2].node);
   TEST(nodes[2].node.left == &nodes[3].node);
   TEST(nodes[2].node.right == &nodes[1].node);
   TEST(nodes[3].node.left == &nodes[2].node);
   TEST(nodes[3].node.right == &nodes[3].node);
   TEST(0 == remove_patriciatrie(&tree, 4, nodes[1].key, &node));
   TEST(tree.root == &nodes[3].node);
   TEST(nodes[3].node.left == &nodes[2].node);
   TEST(nodes[3].node.right == &nodes[3].node);
   TEST(nodes[2].node.left == &nodes[2].node);
   TEST(nodes[2].node.right == &nodes[2].node);
   TEST(0 == removenodes_patriciatrie(&tree, &impl_deletenode));
   TEST(1 == nodes[2].is_freed);
   TEST(1 == nodes[3].is_freed);
   nodes[3].is_freed = 0;
   nodes[2].is_freed = 0;

   // TEST insert_patriciatrie: three keys, 2nd. bit 0 differs, 3rd. bit 8 differs
   memcpy(nodes[0].key, (uint8_t[4]){ 0, 0, 0, 0 }, 4);
   TEST(0 == insert_patriciatrie(&tree, &nodes[0].node, 0));
   memcpy(nodes[1].key, (uint8_t[4]){ 255, 0, 0, 0 }, 4);
   TEST(0 == insert_patriciatrie(&tree, &nodes[1].node, 0));
   memcpy(nodes[2].key, (uint8_t[4]){ 255, 255, 0, 0 }, 4);
   TEST(0 == insert_patriciatrie(&tree, &nodes[2].node, 0));
   TEST(tree.root == &nodes[0].node);
   TEST(tree.root->right == &nodes[1].node);
   TEST(nodes[1].node.right == &nodes[2].node);

   // TEST remove_patriciatrie
   TEST(0 == remove_patriciatrie(&tree, sizeof(nodes[0].key), nodes[0].key, &node));
   TEST(node == &nodes[0].node);
   TEST(0 == remove_patriciatrie(&tree, sizeof(nodes[1].key), nodes[1].key, &node));
   TEST(node == &nodes[1].node);
   TEST(0 == remove_patriciatrie(&tree, sizeof(nodes[2].key), nodes[2].key, &node));
   TEST(node == &nodes[2].node);
   TEST(0 == tree.root);

   // TEST insert_patriciatrie: three keys, 2nd. bit 8 differs, 3rd. bit 0 differs
   memcpy(nodes[0].key, (uint8_t[4]){ 0, 0, 0, 0 }, 4);
   TEST(0 == insert_patriciatrie(&tree, &nodes[0].node, 0));
   memcpy(nodes[1].key, (uint8_t[4]){ 0, 255, 0, 0 }, 4);
   TEST(0 == insert_patriciatrie(&tree, &nodes[1].node, 0));
   memcpy(nodes[2].key, (uint8_t[4]){ 255, 255, 0, 0 }, 4);
   TEST(0 == insert_patriciatrie(&tree, &nodes[2].node, 0));
   TEST(tree.root == &nodes[2].node);
   TEST(tree.root->left == &nodes[0].node);
   TEST(nodes[0].node.right == &nodes[1].node);

   // TEST remove_patriciatrie
   TEST(0 == remove_patriciatrie(&tree, sizeof(nodes[1].key), nodes[1].key, &node));
   TEST(node == &nodes[1].node);
   TEST(0 == remove_patriciatrie(&tree, sizeof(nodes[0].key), nodes[0].key, &node));
   TEST(node == &nodes[0].node);
   TEST(0 == remove_patriciatrie(&tree, sizeof(nodes[2].key), nodes[2].key, &node));
   TEST(node == &nodes[2].node);
   TEST(0 == tree.root);

   // TEST insert_patriciatrie, removenodes_patriciatrie
   nodecount = 0;
   for (int i1 = 0; i1 < 4; ++i1)
      for (int i2 = 0; i2 < 4; ++i2)
         for (int i3 = 0; i3 < 4; ++i3)
            for (int i4 = 0; i4 < 4; ++i4) {
               uint8_t key[sizeof(nodes[nodecount].key)] = { (uint8_t)i1, (uint8_t)i2, (uint8_t)i3, (uint8_t)i4  };
               memcpy(nodes[nodecount].key, key, sizeof(nodes[nodecount].key));
               TEST(0 == insert_patriciatrie(&tree, &nodes[nodecount].node, 0));
               ++ nodecount;
            }
   TEST(0 == removenodes_patriciatrie(&tree, &impl_deletenode));
   for (unsigned i = 0; i < nodecount; ++i) {
      TEST(1 == nodes[i].is_freed);
      nodes[i].is_freed = 0;
   }

   // TEST insert_patriciatrie, find_patriciatrie, removenodes_patriciatrie: EEXIST
   nodecount = 0;
   for (int i1 = 0; i1 < 4; ++i1)
      for (int i2 = 0; i2 < 4; ++i2)
         for (int i3 = 0; i3 < 4; ++i3)
            for (int i4 = 0; i4 < 4; ++i4) {
               uint8_t key[sizeof(nodes[0].key)] = { (uint8_t)i1, (uint8_t)i2, (uint8_t)i3, (uint8_t)i4  };
               memcpy(nodes[nodecount].key, key, sizeof(key));
               TEST(0 == insert_patriciatrie(&tree, &nodes[nodecount].node, 0));
               TEST( EEXIST == insert_patriciatrie(&tree, &nodes[nodecount].node, 0));
               TEST( EEXIST == insert_patriciatrie(&tree, &nodes[nodecount].node, &existing_node));
               TEST( existing_node == &nodes[nodecount].node);
               testnode_t nodecopy = { .key_len = sizeof(key) };
               memcpy(nodecopy.key, key, sizeof(key));
               TEST( EEXIST == insert_patriciatrie(&tree, &nodecopy.node, 0));
               existing_node = 0;
               TEST( EEXIST == insert_patriciatrie(&tree, &nodecopy.node, &existing_node));
               TEST( existing_node == &nodes[nodecount].node);
               TEST(0 == find_patriciatrie(&tree, sizeof(key), key, &node));
               TEST(&nodes[nodecount].node == node);
               TEST(!nodes[nodecount].is_freed);
               ++ nodecount;
            }
   TEST(0 == removenodes_patriciatrie(&tree, &impl_deletenode));
   for (unsigned i = 0; i < MAX_TREE_NODES; ++i) {
      if (i < nodecount) {
         TEST(nodes[i].is_freed);
         nodes[i].is_freed = 0;
      }
      TEST(0 == nodes[i].is_freed);
   }

   // TEST insert_patriciatrie, remove_patriciatrie
   for (unsigned i = 0; i < MAX_TREE_NODES; ++i) {
      uint32_t key = (uint32_t)i;
      assert(sizeof(nodes[i].key) == sizeof(key));
      memcpy(nodes[i].key, &key, sizeof(key));
      TEST(0 == insert_patriciatrie(&tree, &nodes[i].node, 0));
   }
   for (unsigned i = 0; i < MAX_TREE_NODES; ++i) {
      uint32_t key = (uint32_t)(MAX_TREE_NODES-1-i);
      TEST(0 == remove_patriciatrie(&tree, sizeof(key), (uint8_t*)&key, &node));
      TEST(node == &nodes[MAX_TREE_NODES-1-i].node);
      TEST(!nodes[MAX_TREE_NODES-1-i].is_freed);
   }
   TEST(0 == removenodes_patriciatrie(&tree, &impl_deletenode));
   for (unsigned i = 0; i < MAX_TREE_NODES; ++i) {
      TEST(0 == nodes[i].is_freed);
   }

   // TEST insert_patriciatrie, remove_patriciatrie: random
   for (int i = 0; i < MAX_TREE_NODES; ++i) {
      assert(sizeof(nodes[i].key) == 4);
      nodes[i].is_freed = 0;
      nodes[i].is_used = 0;
      nodes[i].key[0] = (uint8_t)((i/1000) % 10);
      nodes[i].key[1] = (uint8_t)((i/100) % 10);
      nodes[i].key[2] = (uint8_t)((i/10) % 10);
      nodes[i].key[3] = (uint8_t)((i/1) % 10);
   }
   srand(100);
   for (int i = 0; i < 10*MAX_TREE_NODES; ++i) {
      int id = rand() % MAX_TREE_NODES;
      assert(id >= 0 && id < MAX_TREE_NODES);
      if (nodes[id].is_used) {
         nodes[id].is_used = 0;
         TEST(0 == remove_patriciatrie(&tree, sizeof(nodes[id].key), nodes[id].key, &node));
         TEST(&nodes[id].node == node);
         TEST(ESRCH == remove_patriciatrie(&tree, sizeof(nodes[id].key), nodes[id].key, &node));
      } else {
         nodes[id].is_used = 1;
         TEST(0 == insert_patriciatrie(&tree, &nodes[id].node, 0));
         TEST(0 == find_patriciatrie(&tree, sizeof(nodes[id].key), nodes[id].key, &node));
         TEST(&nodes[id].node == node);
         TEST(EEXIST == insert_patriciatrie(&tree, &nodes[id].node, 0));
      }
   }
   removenodes_patriciatrie(&tree, &impl_deletenode);
   for (int i = 0; i < MAX_TREE_NODES; ++i) {
      if (nodes[i].is_freed) {
         TEST(nodes[i].is_used);
         nodes[i].is_used  = 0;
         nodes[i].is_freed = 0;
      }
      TEST(! nodes[i].is_used);
   }

   // unprepare
   TEST(0 == FREE_MM(&memblock));
   nodes = 0;

   return 0;
ONERR:
   (void) FREE_MM(&memblock);
   return EINVAL;
}

static int test_iterator(void)
{
   size_t const               NODEOFFSET = offsetof(testnode_t, node);
   getkey_adapter_t           keyadapt  = getkey_adapter_INIT(NODEOFFSET, &impl_gettestkey);
   patriciatrie_t             tree      = patriciatrie_FREE;
   memblock_t                 memblock  = memblock_FREE;
   testnode_t               * nodes   = 0;
   patriciatrie_iterator_t    iter      = patriciatrie_iterator_FREE;
   patriciatrie_prefixiter_t  preiter   = patriciatrie_prefixiter_FREE;
   patriciatrie_node_t      * found_node;

   // prepare
   free_testerrortimer(&s_errcounter);
   TEST(0 == RESIZE_MM(sizeof(testnode_t) * MAX_TREE_NODES, &memblock));
   nodes = (testnode_t*)memblock.addr;
   memset(nodes, 0, sizeof(testnode_t) * MAX_TREE_NODES);
   init_patriciatrie(&tree, keyadapt);
   for (unsigned i = 0; i < MAX_TREE_NODES; ++i) {
      static_assert(sizeof(nodes[i].key) == 4, "key assignment thinks keysize is 4");
      nodes[i].key_len = sizeof(nodes[i].key);
      nodes[i].key[0] = (uint8_t)((i/1000) % 10);
      nodes[i].key[1] = (uint8_t)((i/100) % 10);
      nodes[i].key[2] = (uint8_t)((i/10) % 10);
      nodes[i].key[3] = (uint8_t)((i/1) % 10);
   }

   // TEST patriciatrie_iterator_FREE
   TEST(0 == iter.next);
   TEST(0 == iter.tree);

   // TEST initfirst_patriciatrieiterator: empty tree
   iter.next = (void*)1;
   iter.tree = 0;
   TEST(0 == initfirst_patriciatrieiterator(&iter, &tree));
   TEST(iter.next == 0);
   TEST(iter.tree == &tree);

   // TEST initlast_patriciatrieiterator: empty tree
   iter.next = (void*)1;
   iter.tree = 0;
   TEST(0 == initlast_patriciatrieiterator(&iter, &tree));
   TEST(iter.next == 0);
   TEST(iter.tree == &tree);

   // TEST next_patriciatrieiterator: empty tree
   TEST(0 == initfirst_patriciatrieiterator(&iter, &tree));
   TEST(0 == next_patriciatrieiterator(&iter, 0/*not used*/));

   // TEST prev_patriciatrieiterator: empty tree
   TEST(0 == initlast_patriciatrieiterator(&iter, &tree));
   TEST(0 == prev_patriciatrieiterator(&iter, 0/*not used*/));

   // TEST free_patriciatrieiterator
   iter.next = (void*)1;
   iter.tree = &tree;
   TEST(0 == free_patriciatrieiterator(&iter));
   TEST(0 == iter.next);
   TEST(0 != iter.tree);

   // TEST patriciatrie_prefixiter_FREE
   TEST(0 == preiter.next);
   TEST(0 == preiter.tree);
   TEST(0 == preiter.prefix_bits);

   // TEST initfirst_patriciatrieprefixiter: empty tree
   preiter.next = (void*)1;
   preiter.tree = 0;
   preiter.prefix_bits = 1;
   TEST(0 == initfirst_patriciatrieprefixiter(&preiter, &tree, 0, 0));
   TEST(preiter.next        == 0);
   TEST(preiter.tree        == &tree);
   TEST(preiter.prefix_bits == 0);

   // TEST next_patriciatrieprefixiter: empty tree
   TEST(0 == initfirst_patriciatrieprefixiter(&preiter, &tree, 0, 0));
   TEST(0 == next_patriciatrieprefixiter(&preiter, 0/*not used*/));

   // TEST free_patriciatrieprefixiter
   preiter.next = (void*)1;
   preiter.tree = &tree;
   preiter.prefix_bits = 1;
   TEST(0 == free_patriciatrieprefixiter(&preiter));
   TEST(0 == preiter.next);
   TEST(0 != preiter.tree);
   TEST(0 != preiter.prefix_bits);

   // TEST both iterator
   srand(400);
   for (unsigned test = 0; test < 20; ++test) {
      s_getbinkey_count = 0;
      for (unsigned i = 0; i < MAX_TREE_NODES; ++i) {
         int id = rand() % MAX_TREE_NODES;
         if (nodes[id].is_used) {
            nodes[id].is_used = 0;
            patriciatrie_node_t * removed_node;
            TEST(0 == remove_patriciatrie(&tree, sizeof(nodes[id].key), nodes[id].key, &removed_node));
         } else {
            nodes[id].is_used = 1;
            TEST(0 == insert_patriciatrie(&tree, &nodes[id].node, 0));
         }
      }
      TEST(s_getbinkey_count > MAX_TREE_NODES);
      TEST(0 == initfirst_patriciatrieiterator(&iter, &tree));
      for (unsigned i = 0; i < MAX_TREE_NODES; ++i) {
         if (! (i % 10)) {
            TEST(0 == initfirst_patriciatrieprefixiter(&preiter, &tree, sizeof(nodes[i].key)-1, nodes[i].key));
         }
         if (nodes[i].is_used) {
            TEST(1 == next_patriciatrieprefixiter(&preiter, &found_node));
            TEST(found_node == &nodes[i].node);
            TEST(1 == next_patriciatrieiterator(&iter, &found_node));
            TEST(found_node == &nodes[i].node);
         }
      }
      found_node = (void*)1;
      TEST(false == next_patriciatrieiterator(&iter, &found_node));
      TEST(found_node == (void*)1/*not changed*/);
      TEST(0 == initlast_patriciatrieiterator(&iter, &tree));
      for (unsigned i = MAX_TREE_NODES-1; i < MAX_TREE_NODES; --i) {
         if (nodes[i].is_used) {
            TEST(1 == prev_patriciatrieiterator(&iter, &found_node));
            TEST(found_node == &nodes[i].node);
         }
      }
      found_node = (void*)1;
      TEST(false == prev_patriciatrieiterator(&iter, &found_node));
      TEST(found_node == (void*)1/*not changed*/);
   }

   TEST(0 == removenodes_patriciatrie(&tree, &impl_deletenode));
   for (unsigned i = 0; i < MAX_TREE_NODES; ++i) {
      if (nodes[i].is_freed) {
         TEST(nodes[i].is_used);
         nodes[i].is_used = 0;
         nodes[i].is_freed = 0;
      }
      TEST(! nodes[i].is_used);
   }

   // TEST prefix iterator
   for (unsigned i = 0; i < MAX_TREE_NODES; ++i) {
      nodes[i].key[0] = 0;
      nodes[i].key[1] = (uint8_t)(i / 256);
      nodes[i].key[2] = 0;
      nodes[i].key[3] = (uint8_t)(i % 256);
   }
   for (unsigned i = 0; i < MAX_TREE_NODES; ++i) {
      TEST(0 == insert_patriciatrie(&tree, &nodes[i].node, 0));
   }
   TEST(0 == initfirst_patriciatrieprefixiter(&preiter, &tree, 1, nodes[0].key));
   for (unsigned i = 0; i < MAX_TREE_NODES; ++i) {
      TEST(1 == next_patriciatrieprefixiter(&preiter, &found_node));
      TEST(found_node == &nodes[i].node);
   }
   found_node = (void*)1;
   TEST(false == next_patriciatrieprefixiter(&preiter, &found_node));
   TEST(found_node == (void*)1/*not changed*/);
   for (unsigned i = 0; i < MAX_TREE_NODES-255; i += 256) {
      TEST(0 == initfirst_patriciatrieprefixiter(&preiter, &tree, 2, nodes[i].key));
      for (unsigned g = 0; g <= 255; ++g) {
         TEST(1 == next_patriciatrieprefixiter(&preiter, &found_node));
         TEST(found_node == &nodes[i+g].node);
      }
      found_node = (void*)1;
      TEST(false == next_patriciatrieprefixiter(&preiter, &found_node));
      TEST(found_node == (void*)1/*not changed*/);
   }

   // unprepare
   TEST(0 == FREE_MM(&memblock));
   nodes = 0;

   return 0;
ONERR:
   (void) FREE_MM(&memblock);
   return EINVAL;
}

patriciatrie_IMPLEMENT(_testtree, testnode_t, node, &impl_gettestkey)

static int test_generic(void)
{
   size_t const         NODEOFFSET = offsetof(testnode_t, node);
   getkey_adapter_t     keyadapt = getkey_adapter_INIT(NODEOFFSET, &impl_gettestkey);
   patriciatrie_t       tree = patriciatrie_FREE;
   testnode_t           nodes[50];
   testnode_t         * existing_node = 0;

   // prepare
   free_testerrortimer(&s_errcounter);
   memset(nodes, 0, sizeof(nodes));
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      static_assert(sizeof(nodes[i].key) == 4, "key assignment thinks keysize is 4");
      nodes[i].key_len = sizeof(nodes[i].key);
      nodes[i].key[0] = (uint8_t)((i/1000) % 10);
      nodes[i].key[1] = (uint8_t)((i/100) % 10);
      nodes[i].key[2] = (uint8_t)((i/10) % 10);
      nodes[i].key[3] = (uint8_t)((i/1) % 10);
   }

   // TEST init_patriciatrie, free_patriciatrie
   tree.root = (void*)1;
   init_testtree(&tree);
   TEST(tree.root                == 0);
   TEST(tree.keyadapt.nodeoffset == keyadapt.nodeoffset);
   TEST(tree.keyadapt.getkey     == keyadapt.getkey);
   TEST(0 == free_testtree(&tree, &impl_deletenode));
   TEST(0 == tree.root);
   TEST(0 == tree.keyadapt.nodeoffset);
   TEST(0 == tree.keyadapt.getkey);

   // TEST getinistate_patriciatrie
   patriciatrie_node_t * root = 0;
   getkey_adapter_t keyadapt2 = getkey_adapter_INIT(0,0);
   init_testtree(&tree);
   tree.root = &nodes[10].node;
   getinistate_testtree(&tree, &root, &keyadapt2);
   TEST( &nodes[10].node == root);
   TEST( isequal_getkeyadapter(&keyadapt2, &keyadapt2));
   tree = (patriciatrie_t) patriciatrie_FREE;
   keyadapt2 = keyadapt;
   getinistate_testtree(&tree, &root, &keyadapt2);
   TEST(0 == root);
   TEST(0 == keyadapt2.nodeoffset);
   TEST(0 == keyadapt2.getkey);

   // TEST isempty_patriciatrie
   tree.root = (void*)1;
   TEST(0 == isempty_testtree(&tree));
   tree.root = (void*)0;
   TEST(1 == isempty_testtree(&tree));

   // TEST insert_patriciatrie: EINVAL
   init_testtree(&tree);
   {
      testnode_t errnode;
      errnode.key_len = (size_t)-1;
      existing_node = (void*)1;
      // test
      TEST( EINVAL == insert_testtree(&tree, &errnode, 0/*not set*/));
      // check existing_node
      TEST( 1 == (uintptr_t)existing_node);
      // test
      TEST( EINVAL == insert_testtree(&tree, &errnode, &existing_node));
      // check existing_node
      TEST( 0 == existing_node);
   }

   // TEST insert_patriciatrie, find_patriciatrie, remove_patriciatrie
   init_testtree(&tree);
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(0 == insert_testtree(&tree, &nodes[i], 0));
      TEST(EEXIST == insert_testtree(&tree, &nodes[i], 0));
      TEST(EEXIST == insert_testtree(&tree, &nodes[i], &existing_node));
      TEST(existing_node == &nodes[i])
   }
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      testnode_t * found_node = 0;
      TEST(0 == find_testtree(&tree, sizeof(nodes[i].key), nodes[i].key, &found_node));
      TEST(found_node == &nodes[i]);
   }
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      testnode_t * removed_node = 0;
      TEST(0 == remove_testtree(&tree, sizeof(nodes[i].key), nodes[i].key, &removed_node));
      TEST(removed_node == &nodes[i]);
      TEST(ESRCH == find_testtree(&tree, sizeof(nodes[i].key), nodes[i].key, &removed_node));
   }

   // TEST removenodes_patriciatrie, free_patriciatrie
   init_testtree(&tree);
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(0 == insert_testtree(&tree, &nodes[i], 0));
   }
   TEST(0 == removenodes_testtree(&tree, &impl_deletenode));
   TEST(1 == isequal_getkeyadapter(&tree.keyadapt, &keyadapt));
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(1 == nodes[i].is_freed);
   }
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(0 == insert_testtree(&tree, &nodes[i], 0));
   }
   TEST(0 == free_testtree(&tree, &impl_deletenode));
   TEST(1 == isequal_getkeyadapter(&tree.keyadapt, &(getkey_adapter_t) getkey_adapter_INIT(0,0)));
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(2 == nodes[i].is_freed);
   }

   // TEST foreach, foreachReverse
   init_testtree(&tree);
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(0 == insert_testtree(&tree, &nodes[i], 0));
   }
   for (unsigned i = 0; 0 == i; i = 1) {
      foreach (_testtree, node, &tree) {
         TEST(node == &nodes[i++]);
      }
      TEST(i == lengthof(nodes));

      foreachReverse (_testtree, node, &tree) {
         TEST(node == &nodes[--i]);
      }
      TEST(i == 0);
   }

   return 0;
ONERR:
   return EINVAL;
}

int unittest_ds_inmem_patriciatrie()
{
   if (test_searchhelper())      goto ONERR;
   if (test_initfree())          goto ONERR;
   if (test_insertremove())      goto ONERR;
   if (test_iterator())          goto ONERR;
   if (test_generic())           goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
