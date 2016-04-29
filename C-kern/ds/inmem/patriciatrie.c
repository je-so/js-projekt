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

// group: helper

static inline void * cast_object(patriciatrie_node_t* node, size_t nodeoffset)
{
   return ((uint8_t*)node - nodeoffset);
}

// group: lifetime

int free_patriciatrie(patriciatrie_t * tree, delete_adapter_f delete_f)
{
   int err = removenodes_patriciatrie(tree, delete_f);

   tree->keyadapt = (getkey_adapter_t) getkey_adapter_INIT(0,0);

   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: search

/* define: GETBIT
 * Returns the bit value at bitoffset. Returns (key[0]&0x80) if bitoffset is 0 and (key[0]&0x40) if it is 1 and so on.
 * Every string has a virtual end marker of 0xFF.
 * If bitoffset/8 equals therefore length a value of 1 is returned.
 * If bitoffset is beyond string length and beyond virtual end marker a value of 0 is returned. */
#define GETBIT(key, length, bitoffset) \
   ( __extension__ ({                                       \
         size_t _byteoffset = (bitoffset) / 8;              \
         (_byteoffset < (length))                           \
         ? (0x80>>((bitoffset)%8)) & ((key)[_byteoffset])   \
         : ((_byteoffset == (length)) ? 1 : 0);             \
   }))

/* function: first_different_bit
 * Determines the first bit which differs in key1 and key2. Virtual end markers of 0xFF
 * at end of each string are considered. If both keys are equal the functions returns
 * the error code EINVAL. On success 0 is returned and *bit_offset contains the bit index
 * of the first differing bit beginning with 0. */
static int first_different_bit(
       const uint8_t * const key1,
       size_t        const length1,
       const uint8_t * const key2,
       size_t        const length2,
/*out*/size_t *      key2_bit_offset,
/*out*/uint8_t*      key2_bit_value)
{
   size_t length = (length1 < length2 ? length1 : length2);
   const uint8_t * k1 = key1;
   const uint8_t * k2 = key2;
   while (length && *k1 == *k2) {
      -- length;
      ++ k1;
      ++ k2;
   }

   uint8_t b1;
   uint8_t b2;
   size_t  result;
   if (length) {
      b1 = *k1;
      b2 = *k2;
      result = 8 * (size_t)(k1 - key1);
   } else {
      if (length1 == length2) return EINVAL;   // both keys are equal
      if (length1 < length2) {
         length = length2 - length1;
         b1 = 0xFF/*end marker of k1*/;
         b2 = *k2;
         if (b2 == b1) {
            b1 = 0x00; // artificial extension of k1
            do {
               ++ k2;
               -- length;
               if (! length) { b2 = 0xFF/*end marker*/; break; }
               b2 = *k2;
            } while (b2 == b1);
         }
         result = 8 * (size_t)(k2 - key2);
      } else {
         length = length1 - length2;
         b2 = 0xFF/*end marker of k2*/;
         b1 = *k1;
         if (b1 == b2) {
            b2 = 0x00; // artificial extension of k2
            do {
               ++ k1;
               -- length;
               if (! length) { b1 = 0xFF/*end marker*/; break; }
               b1 = *k1;
            } while (b1 == b2);
         }
         result = 8 * (size_t)(k1 - key1);
      }
   }

   // assert(b1 != b2);
   b1 ^= b2;
   unsigned mask = 0x80;
   if (!b1) return EINVAL; // should never happen (b1 != b2)
   for (; 0 == (mask & b1); mask >>= 1) {
      ++ result;
   }

   *key2_bit_offset = result;
   *key2_bit_value  = (uint8_t) (b2 & mask);
   return 0;
}

static int findnode(patriciatrie_t * tree, size_t keylength, const uint8_t searchkey[keylength], patriciatrie_node_t ** found_parent, patriciatrie_node_t ** found_node)
{
   patriciatrie_node_t * parent;
   patriciatrie_node_t * node = tree->root;

   if (!node) return ESRCH;

   do {
      parent = node;
      if (GETBIT(searchkey, keylength, node->bit_offset)) {
         node = node->right;
      } else {
         node = node->left;
      }
   } while (node->bit_offset > parent->bit_offset);

   *found_node   = node;
   *found_parent = parent;

   return 0;
}

int find_patriciatrie(patriciatrie_t * tree, size_t keylength, const uint8_t searchkey[keylength], patriciatrie_node_t ** found_node)
{
   int err;
   patriciatrie_node_t * node;
   patriciatrie_node_t * parent;

   VALIDATE_INPARAM_TEST((searchkey != 0 || keylength == 0) && keylength < (((size_t)-1)/8), ONERR, );

   err = findnode(tree, keylength, searchkey, &parent, &node);
   if (err) return err;

   getkey_data_t key;
   tree->keyadapt.getkey(cast_object(node, tree->keyadapt.nodeoffset), &key);
   if (key.size != keylength || 0 != memcmp(key.addr, searchkey, keylength)) {
      return ESRCH;
   }

   *found_node = node;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: change

int insert_patriciatrie(patriciatrie_t * tree, patriciatrie_node_t * newnode, /*err*/ patriciatrie_node_t** existing_node/*0 ==> not returned*/)
{
   int err;
   getkey_data_t key;
   tree->keyadapt.getkey(cast_object(newnode, tree->keyadapt.nodeoffset), &key);

   VALIDATE_INPARAM_TEST((key.addr != 0 || key.size == 0) && key.size < (((size_t)-1)/8), ONERR, );

   if (!tree->root) {
      tree->root = newnode;
      newnode->bit_offset = 0;
      newnode->right = newnode;
      newnode->left  = newnode;
      return 0;
   }

   // search node
   patriciatrie_node_t * parent;
   patriciatrie_node_t * node;
   (void) findnode(tree, key.size, key.addr, &parent, &node);

   size_t  new_bitoffset;
   uint8_t new_bitvalue;
   {
      getkey_data_t nodek;
      tree->keyadapt.getkey(cast_object(node, tree->keyadapt.nodeoffset), &nodek);
      if (node == newnode || first_different_bit(nodek.addr, nodek.size, key.addr, key.size, &new_bitoffset, &new_bitvalue)) {
         if (existing_node) *existing_node = node;
         return EEXIST;   // found node has same key
      }
      // new_bitvalue == GETBIT(key.addr, key.size, new_bitoffset)
   }

   // search position in tree where new_bitoffset belongs to
   if (new_bitoffset < parent->bit_offset) {
      node   = tree->root;
      parent = 0;
      while (node->bit_offset < new_bitoffset) {
         parent = node;
         if (GETBIT(key.addr, key.size, node->bit_offset)) {
            node = node->right;
         } else {
            node = node->left;
         }
      }
   }

   assert(parent == 0 || parent->bit_offset < new_bitoffset || tree->root == parent);

   if (node->right == node->left) {
      // LEAF points to itself (bit_offset is unused) and is at the tree bottom
      assert(node->bit_offset == 0 && node->left == node);
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
         if (parent->right == node) {
            parent->right = newnode;
         } else {
            assert(parent->left == node);
            parent->left = newnode;
         }
      } else {
         assert(tree->root == node);
         tree->root = newnode;
      }
   }

   return 0;
ONERR:
   if (existing_node) *existing_node = 0; // err param
   TRACEEXIT_ERRLOG(err);
   return err;
}

int remove_patriciatrie(patriciatrie_t * tree, size_t keylength, const uint8_t searchkey[keylength], /*out*/patriciatrie_node_t ** removed_node)
{
   int err;
   patriciatrie_node_t * node;
   patriciatrie_node_t * parent;

   VALIDATE_INPARAM_TEST((searchkey != 0 || keylength == 0) && keylength < (((size_t)-1)/8), ONERR, );

   err = findnode(tree, keylength, searchkey, &parent, &node);
   if (err) return err;

   getkey_data_t nodek;
   tree->keyadapt.getkey(cast_object(node, tree->keyadapt.nodeoffset), &nodek);

   if (nodek.size != keylength || memcmp(nodek.addr, searchkey, keylength)) {
      return ESRCH;
   }

   patriciatrie_node_t * const delnode = node;
   patriciatrie_node_t * replacednode = 0;
   patriciatrie_node_t * replacedwith;

   if (node->right == node->left) {
      // LEAF points to itself (bit_offset is unused) and is at the tree bottom
      assert(node->left == node);
      assert(0==node->bit_offset);
      if (node == parent) {
         assert(tree->root == node);
         tree->root = 0;
      } else if ( parent->left == parent
                  || parent->right == parent) {
         // parent is new LEAF
         parent->bit_offset = 0;
         parent->left  = parent;
         parent->right = parent;
      } else {
         // shift parents other child into parents position
         replacednode = parent;
         replacedwith = (parent->left == node ? parent->right : parent->left);
         assert(replacedwith != node && replacedwith != parent);
         parent->bit_offset = 0;
         parent->left  = parent;
         parent->right = parent;
      }
   } else if ( node->left == node
               || node->right == node) {
      // node can be removed (cause only one child)
      assert(parent == node);
      replacednode = node;
      replacedwith = (node->left == node ? node->right : node->left);
      assert(replacedwith != node);
   } else {
      // node has two childs
      assert(parent != node);
      replacednode = node;
      replacedwith = parent;
      // find parent of replacedwith
      do {
         parent = node;
         if (GETBIT(nodek.addr, nodek.size, node->bit_offset)) {
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
         do {
            parent = node;
            if (GETBIT(nodek.addr, nodek.size, node->bit_offset)) {
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

int removenodes_patriciatrie(patriciatrie_t * tree, delete_adapter_f delete_f)
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
               void * object = cast_object(node, tree->keyadapt.nodeoffset);
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

int initfirst_patriciatrieiterator(/*out*/patriciatrie_iterator_t * iter, patriciatrie_t * tree)
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

int initlast_patriciatrieiterator(/*out*/patriciatrie_iterator_t * iter, patriciatrie_t * tree)
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

bool next_patriciatrieiterator(patriciatrie_iterator_t * iter, /*out*/patriciatrie_node_t ** node)
{
   if (!iter->next) return false;

   *node = iter->next;

   getkey_data_t nextk;
   iter->tree->keyadapt.getkey(cast_object(iter->next, iter->tree->keyadapt.nodeoffset), &nextk);

   patriciatrie_node_t * next = iter->tree->root;
   patriciatrie_node_t * parent;
   patriciatrie_node_t * higher_branch_parent = 0;
   do {
      parent = next;
      if (GETBIT(nextk.addr, nextk.size, next->bit_offset)) {
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

bool prev_patriciatrieiterator(patriciatrie_iterator_t * iter, /*out*/patriciatrie_node_t ** node)
{
   if (!iter->next) return false;

   *node = iter->next;

   getkey_data_t nextk;
   iter->tree->keyadapt.getkey(cast_object(iter->next, iter->tree->keyadapt.nodeoffset), &nextk);

   patriciatrie_node_t * next = iter->tree->root;
   patriciatrie_node_t * parent;
   patriciatrie_node_t * lower_branch_parent = 0;
   do {
      parent = next;
      if (GETBIT(nextk.addr, nextk.size, next->bit_offset)) {
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

int initfirst_patriciatrieprefixiter(/*out*/patriciatrie_prefixiter_t * iter, patriciatrie_t * tree, size_t keylength, const uint8_t prefixkey[keylength])
{
   patriciatrie_node_t * parent;
   patriciatrie_node_t * node = tree->root;
   size_t         prefix_bits = 8 * keylength;

   if (  (prefixkey || !keylength)
         && keylength < (((size_t)-1)/8)
         && node) {
      if (node->bit_offset < prefix_bits) {
         do {
            parent = node;
            if (GETBIT(prefixkey, keylength, node->bit_offset)) {
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
      tree->keyadapt.getkey(cast_object(node, tree->keyadapt.nodeoffset), &key);
      bool isPrefix =   key.size >= keylength
                        && ! memcmp(key.addr, prefixkey, keylength);
      if (!isPrefix) node = 0;
   }

   iter->next        = node;
   iter->tree        = tree;
   iter->prefix_bits = prefix_bits;
   return 0;
}

// group: iterate

bool next_patriciatrieprefixiter(patriciatrie_prefixiter_t * iter, /*out*/patriciatrie_node_t ** node)
{
   if (!iter->next) return false;

   *node = iter->next;

   getkey_data_t nextk;
   iter->tree->keyadapt.getkey(cast_object(iter->next, iter->tree->keyadapt.nodeoffset), &nextk);

   patriciatrie_node_t * next = iter->tree->root;
   patriciatrie_node_t * parent;
   patriciatrie_node_t * higher_branch_parent = 0;
   do {
      parent = next;
      if (GETBIT(nextk.addr, nextk.size, next->bit_offset)) {
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

static void impl_getstreamkey(void * obj, /*out*/getkey_data_t* key)
{
   int err;
   testnode_t * node = obj;

   if (process_testerrortimer(&s_errcounter, &err)) {
      // only used for input param check in insert
      key->addr = 0;
      key->size = 1;
   } else {
      ++ s_getbinkey_count;
      key->addr = node->key;
      key->size = node->key_len;
   }
}

static int test_firstdiffbit(void)
{
   uint8_t  key1[256] = {0};
   uint8_t  key2[256] = {0};
   size_t   bit_offset = 0;
   uint8_t  bit_value  = 0;

   // TEST first_different_bit: keys are equal
   TEST(EINVAL == first_different_bit(key1, sizeof(key1), key2, sizeof(key2), &bit_offset, &bit_value));

   // TEST first_different_bit: keys are equal until end of key2(end marker 0xFF)
   key1[2] = 0xFF; // end markter of key2
   TEST( 0 == first_different_bit(key1, sizeof(key1), key2, 2, &bit_offset, &bit_value));
   // check out param
   TEST( bit_value  == 0);
   TEST( bit_offset == bitsof(key1));
   // test parameter reversed
   TEST( 0 == first_different_bit(key2, 2, key1, sizeof(key1), &bit_offset, &bit_value));
   // check out param
   TEST( bit_value  != 0);
   TEST( bit_offset == bitsof(key1));

   // TEST first_different_bit: keys differ in end marker of key2
   for (unsigned i = 1; i < 8; ++i) {
      key1[2] = (uint8_t)(0xFF << i);
      TEST( 0 == first_different_bit(key1, sizeof(key1), key2, 2, &bit_offset, &bit_value));
      // check out param
      TEST( bit_value  != 0);
      TEST( bit_offset == 24-i);
      // test parameter reversed
      TEST( 0 == first_different_bit(key2, 2, key1, sizeof(key1), &bit_offset, &bit_value));
      // check out param
      TEST( bit_value  == 0);
      TEST( bit_offset == 24-i);
   }

   // TEST first_different_bit: keys differ in second byte
   for (int i = 0; i < 8; ++i) {
      key1[1] = (uint8_t) (0x01 << i);
      TEST( 0 == first_different_bit(key1, sizeof(key1), key2, 2, &bit_offset, &bit_value));
      // check out param
      TEST( bit_value  == 0);
      TEST( bit_offset == (size_t)(15-i));
      // test parameter reversed
      TEST( 0 == first_different_bit(key2, 2, key1, sizeof(key1), &bit_offset, &bit_value));
      // check out param
      TEST( bit_value  != 0);
      TEST( bit_offset == (size_t)(15-i));
   }

   // prepare
   static_assert(lengthof(key1) == lengthof(key2), "check for overflow");
   for (unsigned i = 0; i < lengthof(key1); ++i) {
      key1[i] = (uint8_t)i;
      key2[i] = (uint8_t)i;
   }

   // TEST first_different_bit: keys are equal
   TEST(EINVAL == first_different_bit(key1, sizeof(key1), key2, sizeof(key2), &bit_offset, &bit_value));
   TEST(EINVAL == first_different_bit(key2, sizeof(key2), key1, sizeof(key1), &bit_offset, &bit_value));

   // TEST first_different_bit: keys differ in length
   TEST(0 == first_different_bit(key1, sizeof(key1), key2, sizeof(key2)-1, &bit_offset, &bit_value));
   // check out param
   TEST( bit_value  == 0);
   TEST( bit_offset == bitsof(key1));
   // test parameter reversed
   TEST(0 == first_different_bit(key2, sizeof(key2)-1, key1, sizeof(key1), &bit_offset, &bit_value));
   // check out param
   TEST( bit_value  != 0);
   TEST( bit_offset == bitsof(key1));

   // TEST first_different_bit: keys differ in first 8 byte
   for (unsigned i = 0; i < 8; ++i) {
      key1[i] = 0;
      key2[i] = (uint8_t)(0x80 >> i);
      TEST( 0 == first_different_bit(key1, sizeof(key1), key2, sizeof(key2)/3, &bit_offset, &bit_value));
      // check out param
      TEST( bit_value  != 0);
      TEST( bit_offset == 8*i+i);
      // test parameter reversed
      TEST( 0 == first_different_bit(key2, sizeof(key2)/3, key1, sizeof(key1), &bit_offset, &bit_value));
      // check out param
      TEST( bit_value  == 0);
      TEST( bit_offset == 8*i+i);
      // reset
      key1[i] = (uint8_t)i;
      key2[i] = (uint8_t)i;
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_initfree(void)
{
   size_t const            NODEOFFSET = offsetof(testnode_t, node);
   getkey_adapter_t        keyadapt = getkey_adapter_INIT(NODEOFFSET, &impl_getstreamkey);
   patriciatrie_t          tree = patriciatrie_FREE;
   patriciatrie_node_t     node = patriciatrie_node_INIT;
   testnode_t              nodes[20];

   // prepare
   free_testerrortimer(&s_errcounter);
   s_freenode_count  = 0;
   s_getbinkey_count = 0;
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
   getkey_adapter_t     keyadapt  = getkey_adapter_INIT(NODEOFFSET, &impl_getstreamkey);
   patriciatrie_t       tree      = patriciatrie_FREE;
   memblock_t           memblock  = memblock_FREE;
   testnode_t           * nodes   = 0;
   patriciatrie_node_t  * node;
   patriciatrie_node_t  * existing_node;
   unsigned             nodecount;

   // prepare
   free_testerrortimer(&s_errcounter);
   s_freenode_count  = 0;
   s_getbinkey_count = 0;
   TEST(0 == RESIZE_MM(sizeof(testnode_t) * MAX_TREE_NODES, &memblock));
   nodes = (testnode_t*)memblock.addr;
   memset(nodes, 0, sizeof(testnode_t) * MAX_TREE_NODES);
   for (unsigned i = 0; i < MAX_TREE_NODES; ++i) {
      nodes[i].key_len = sizeof(nodes[i].key);
   }

   // TEST insert_patriciatrie: empty key (length == 0)
   init_patriciatrie(&tree, keyadapt);
   nodes[0].key_len = 0;
   existing_node = (void*)1;
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

   // TEST insert_patriciatrie: differ in bit 0
   init_patriciatrie(&tree, keyadapt);
   TEST(0 == insert_patriciatrie(&tree, &nodes[0].node, 0));
   // test
   nodes[1].key[0] = 0x80;
   TEST( 0 == insert_patriciatrie(&tree, &nodes[1].node, &existing_node));
   // check existing_node (not changed)
   TEST( 1 == (uintptr_t)existing_node);
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
   TEST(0 == removenodes_patriciatrie(&tree, &impl_deletenode));
   for (unsigned i = 0; i < 2; ++i) {
      TEST(1 == nodes[i].is_freed);
      nodes[i].is_freed = 0;
      nodes[i].key[0] = 0;
   }
   nodes[0].is_freed = 0;

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
   getkey_adapter_t           keyadapt  = getkey_adapter_INIT(NODEOFFSET, &impl_getstreamkey);
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

patriciatrie_IMPLEMENT(_testtree, testnode_t, node, &impl_getstreamkey)

static int test_generic(void)
{
   size_t const         NODEOFFSET = offsetof(testnode_t, node);
   getkey_adapter_t     keyadapt = getkey_adapter_INIT(NODEOFFSET, &impl_getstreamkey);
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
   if (test_firstdiffbit())      goto ONERR;
   if (test_initfree())          goto ONERR;
   if (test_insertremove())      goto ONERR;
   if (test_iterator())          goto ONERR;
   if (test_generic())           goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
