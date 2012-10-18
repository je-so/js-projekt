/* title: Patricia-Trie impl
   Implements <Patricia-Trie>.

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
   (C) 2012 Jörg Seebohn

   file: C-kern/api/ds/inmem/patriciatrie.h
    Header file <Patricia-Trie>.

   file: C-kern/ds/inmem/patriciatrie.c
    Implementation file <Patricia-Trie impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/ds/typeadapt.h"
#include "C-kern/api/ds/inmem/patriciatrie.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/ds/foreach.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#include "C-kern/api/test/errortimer.h"
#endif



// section: patriciatrie_t

// group: lifetime

int free_patriciatrie(patriciatrie_t * tree)
{
   int err = removenodes_patriciatrie(tree) ;

   tree->nodeadp = (typeadapt_member_t) typeadapt_member_INIT_FREEABLE ;

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

// group: search

/* define: GETBIT
 * Returns the bit value at bitoffset. Returns (key[0]&0x80) if bitoffset is 0 and (key[0]&0x40) if it is 1 and so on.
 * Every string has a virtual end marker of 0xFF.
 * If bitoffset/8 equals therefore length a value of 1 is returned.
 * If bitoffset is beyond string length and beyond virtual end marker a value of 0 is returned. */
#define GETBIT(key, length, bitoffset)                      \
   ( __extension__ ({                                       \
         size_t _byteoffset = (bitoffset) / 8 ;             \
         (_byteoffset < (length))                           \
         ? (0x80>>((bitoffset)%8)) & ((key)[_byteoffset])   \
         : ((_byteoffset == (length)) ? 1 : 0) ;            \
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
/*out*/size_t        * bit_offset)
{
   size_t length = (length1 < length2 ? length1 : length2) ;
   const uint8_t * k1 = key1 ;
   const uint8_t * k2 = key2 ;
   while (length && *k1 == *k2) {
      -- length ;
      ++ k1 ;
      ++ k2 ;
   }

   uint8_t b1 ;
   uint8_t b2 ;
   size_t  result ;
   if (length) {
      b1 = *k1 ;
      b2 = *k2 ;
      result = 8 * (size_t)(k1 - key1) ;
   } else {
      if (length1 == length2) return EINVAL ;   // both keys are equal
      if (length1 < length2) {
         length = length2 - length1 ;
         if (*k2 == 0xFF/*end marker of k1*/) {
            do {
               ++ k2 ;
               -- length ;
            } while (length && *k2 == 0) ;
            b1 = 0x00 ;
            if (!length) {
               result = 8 * length2 ;
               b2 = 0xFF ;
            } else {
               result = 8 * (size_t)(k2 - key2) ;
               b2 = *k2 ;
            }
         } else {
            b1 = 0xFF ;
            b2 = *k2 ;
            result = 8 * (size_t)(k2 - key2) ;
         }
      } else {
         length = length1 - length2 ;
         if (*k1 == 0xFF/*end marker of k2*/) {
            do {
               ++ k1 ;
               -- length ;
            } while (length && *k1 == 0) ;
            b2 = 0x00 ;
            if (!length) {
               result = 8 * length1 ;
               b1 = 0xFF ;
            } else {
               result = 8 * (size_t)(k1 - key1) ;
               b1 = *k1 ;
            }
         } else {
            b1 = *k1 ;
            b2 = 0xFF ;
            result = 8 * (size_t)(k1 - key1) ;
         }
      }
   }

   // assert(b1 != b2) ;
   b1 ^= b2 ;
   for (uint8_t mask = 0x80; (0 == (b1 & mask)) && mask; mask>>=1) {
      ++ result ;
   }

   *bit_offset = result ;
   return 0 ;
}

static int findnode(patriciatrie_t * tree, size_t keylength, const uint8_t searchkey[keylength], patriciatrie_node_t ** found_parent, patriciatrie_node_t ** found_node)
{
   patriciatrie_node_t * parent ;
   patriciatrie_node_t * node = tree->root ;

   if (!node) return ESRCH ;

   do {
      parent = node ;
      if (GETBIT(searchkey, keylength, node->bit_offset)) {
         node = node->right ;
      } else {
         node = node->left ;
      }
   } while (node->bit_offset > parent->bit_offset) ;

   *found_node   = node ;
   *found_parent = parent ;

   return 0 ;
}

int find_patriciatrie(patriciatrie_t * tree, size_t keylength, const uint8_t searchkey[keylength], patriciatrie_node_t ** found_node)
{
   int err ;
   patriciatrie_node_t * node ;
   patriciatrie_node_t * parent ;

   VALIDATE_INPARAM_TEST((searchkey != 0 || keylength == 0) && keylength < (((size_t)-1)/8), ONABORT, ) ;

   err = findnode(tree, keylength, searchkey, &parent, &node) ;
   if (err) return err ;

   typeadapt_binarykey_t key ;
   callgetbinarykey_typeadaptmember(&tree->nodeadp, memberasobject_typeadaptmember(&tree->nodeadp, node), &key) ;
   if (key.size != keylength || 0 != memcmp(key.addr, searchkey, keylength)) {
      return ESRCH ;
   }

   *found_node = node ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

// group: change

int insert_patriciatrie(patriciatrie_t * tree, patriciatrie_node_t * newnode)
{
   int err ;
   typeadapt_binarykey_t   insertkey ;

   callgetbinarykey_typeadaptmember(&tree->nodeadp, memberasobject_typeadaptmember(&tree->nodeadp, newnode), &insertkey) ;

   VALIDATE_INPARAM_TEST((insertkey.addr != 0 || insertkey.size == 0) && insertkey.size < (((size_t)-1)/8), ONABORT, ) ;

   if (!tree->root) {
      tree->root = newnode ;
      newnode->bit_offset = 0 ;
      newnode->right = newnode ;
      newnode->left  = newnode ;
      return 0 ;
   }

   // search node
   patriciatrie_node_t * parent ;
   patriciatrie_node_t * node ;
   size_t new_bitoffset ;
   (void) findnode(tree, insertkey.size, insertkey.addr, &parent, &node) ;

   if (node == newnode) {
      return EEXIST ;      // found node already inserted
   } else {
      typeadapt_binarykey_t nodek ;
      callgetbinarykey_typeadaptmember(&tree->nodeadp, memberasobject_typeadaptmember(&tree->nodeadp, node), &nodek) ;
      if (first_different_bit(nodek.addr, nodek.size, insertkey.addr, insertkey.size, &new_bitoffset)) {
         return EEXIST ;   // found node has same key
      }
   }

   // search position in tree where new_bitoffset belongs to
   if (parent->bit_offset > new_bitoffset) {
      node   = tree->root ;
      parent = 0 ;
      // search the correct position in the tree
      while (node->bit_offset < new_bitoffset) {
         parent = node ;
         if (GETBIT(insertkey.addr, insertkey.size, node->bit_offset)) {
            node = node->right ;
         } else {
            node = node->left ;
         }
      }
   }

   assert(parent == 0 || parent->bit_offset < new_bitoffset || tree->root == parent) ;

   if (node->right == node->left) {
      // LEAF points to itself (bit_offset is unused) and is at the tree bottom
      assert(node->bit_offset == 0 && node->left == node) ;
      newnode->bit_offset = 0 ;
      newnode->right = newnode ;
      newnode->left  = newnode ;
      node->bit_offset = new_bitoffset ;
      if (GETBIT(insertkey.addr, insertkey.size, new_bitoffset)) {
         node->right = newnode ;
      } else {
         node->left  = newnode ;
      }
   } else {
      newnode->bit_offset = new_bitoffset ;
      if (GETBIT(insertkey.addr, insertkey.size, new_bitoffset)) {
         newnode->right = newnode ;
         newnode->left  = node ;
      } else {
         newnode->right = node ;
         newnode->left  = newnode ;
      }
      if (parent) {
         if (GETBIT(insertkey.addr, insertkey.size, parent->bit_offset)) {
            assert(parent->right == node) ;
            parent->right = newnode ;
         } else {
            assert(parent->left == node) ;
            parent->left = newnode ;
         }
      } else {
         assert(tree->root == node) ;
         tree->root = newnode ;
      }
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int remove_patriciatrie(patriciatrie_t * tree, size_t keylength, const uint8_t searchkey[keylength], patriciatrie_node_t ** removed_node)
{
   int err ;
   patriciatrie_node_t * node ;
   patriciatrie_node_t * parent ;

   VALIDATE_INPARAM_TEST((searchkey != 0 || keylength == 0) && keylength < (((size_t)-1)/8), ONABORT, ) ;

   err = findnode(tree, keylength, searchkey, &parent, &node) ;
   if (err) return err ;

   typeadapt_binarykey_t nodek ;
   callgetbinarykey_typeadaptmember(&tree->nodeadp, memberasobject_typeadaptmember(&tree->nodeadp, node), &nodek) ;

   if (  nodek.size != keylength
         || memcmp(nodek.addr, searchkey, keylength)) {
      return ESRCH ;
   }

   patriciatrie_node_t * const delnode = node ;
   patriciatrie_node_t * replacednode = 0 ;
   patriciatrie_node_t * replacedwith ;

   if (node->right == node->left) {
      // LEAF points to itself (bit_offset is unused) and is at the tree bottom
      assert(node->left == node) ;
      assert(0==node->bit_offset) ;
      if (node == parent) {
         assert(tree->root == node) ;
         tree->root = 0 ;
      } else if ( parent->left == parent
                  || parent->right == parent) {
         // parent is new LEAF
         parent->bit_offset = 0 ;
         parent->left  = parent ;
         parent->right = parent ;
      } else {
         // shift parents other child into parents position
         replacednode = parent ;
         replacedwith = (parent->left == node ? parent->right : parent->left) ;
         assert(replacedwith != node && replacedwith != parent) ;
         parent->bit_offset = 0 ;
         parent->left  = parent ;
         parent->right = parent ;
      }
   } else if ( node->left == node
               || node->right == node) {
      // node can be removed (cause only one child)
      assert(parent == node) ;
      replacednode = node ;
      replacedwith = (node->left == node ? node->right : node->left) ;
      assert(replacedwith != node) ;
   } else {
      // node has two childs
      assert(parent != node) ;
      replacednode = node ;
      replacedwith = parent ;
      // find parent of replacedwith
      do {
         parent = node ;
         if (GETBIT(nodek.addr, nodek.size, node->bit_offset)) {
            node = node->right ;
         } else {
            node = node->left ;
         }
      } while (node != replacedwith) ;
      // now let parent of replacedwith point to other child of replacedwith instead to replacedwith
      if (parent->left == node)
         parent->left = (node->left == replacednode ? node->right : node->left) ;
      else
         parent->right = (node->left == replacednode ? node->right : node->left) ;
      // copy replacednode into replacedwith
      node->bit_offset = replacednode->bit_offset ;
      node->left       = replacednode->left ;
      node->right      = replacednode->right ;
   }

   // find parent of replacednode and let it point to replacedwith
   if (replacednode) {
      node = tree->root ;
      if (node == replacednode) {
         tree->root = replacedwith ;
      } else {
         do {
            parent = node ;
            if (GETBIT(nodek.addr, nodek.size, node->bit_offset)) {
               node = node->right ;
            } else {
               node = node->left ;
            }
         } while (node != replacednode) ;
         if (parent->left == replacednode)
            parent->left = replacedwith ;
         else
            parent->right = replacedwith ;
      }
   }

   delnode->bit_offset = 0 ;
   delnode->left       = 0 ;
   delnode->right      = 0 ;

   *removed_node = delnode ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int removenodes_patriciatrie(patriciatrie_t * tree)
{
   int err ;
   patriciatrie_node_t   * parent = 0 ;
   patriciatrie_node_t   * node   = tree->root ;

   tree->root = 0 ;

   if (node) {

      const bool isDeleteObject = islifetimedelete_typeadapt(tree->nodeadp.typeadp) ;

      err = 0 ;

      for (;;) {
         while (  node->left->bit_offset > node->bit_offset
                  || (node->left != node && node->left->left == node->left && node->left->right == node->left) /*node->left has unused bit_offset*/) {
            patriciatrie_node_t * nodeleft = node->left ;
            node->left = parent ;
            parent = node ;
            node   = nodeleft ;
         }
         if (  node->right->bit_offset > node->bit_offset
               || (node->right != node && node->right->left == node->right && node->right->right == node->right) /*node->right has unused bit_offset*/) {
            patriciatrie_node_t * noderight = node->right ;
            node->left = parent ;
            parent = node ;
            node   = noderight ;
         } else {
            node->bit_offset = 0 ;
            node->left       = 0 ;
            node->right      = 0 ;
            if (isDeleteObject) {
               typeadapt_object_t * object = memberasobject_typeadaptmember(&tree->nodeadp, node) ;
               int err2 = calldelete_typeadaptmember(&tree->nodeadp, &object) ;
               if (err2) err = err2 ;
            }
            if (!parent) break ;
            if (parent->right == node) {
               parent->right = parent ;
            }
            node   = parent ;
            parent = node->left ;
            node->left = node ;
         }
      }

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}


// section: patriciatrie_iterator_t

// group: lifetime

int initfirst_patriciatrieiterator(/*out*/patriciatrie_iterator_t * iter, patriciatrie_t * tree)
{
   patriciatrie_node_t * parent ;
   patriciatrie_node_t * node = tree->root ;

   if (node) {
      do {
         parent = node ;
         node   = node->left ;
      } while (node->bit_offset > parent->bit_offset) ;
   }

   iter->next = node ;
   return 0 ;
}

int initlast_patriciatrieiterator(/*out*/patriciatrie_iterator_t * iter, patriciatrie_t * tree)
{
   patriciatrie_node_t * parent ;
   patriciatrie_node_t * node = tree->root ;

   if (node) {
      do {
         parent = node ;
         node = node->right ;
      } while (node->bit_offset > parent->bit_offset) ;
   }

   iter->next = node ;
   return 0 ;
}

// group: iterate

bool next_patriciatrieiterator(patriciatrie_iterator_t * iter, patriciatrie_t * tree, /*out*/patriciatrie_node_t ** node)
{
   if (!iter->next) return false ;

   *node = iter->next ;

   typeadapt_binarykey_t nextk ;
   callgetbinarykey_typeadaptmember(&tree->nodeadp, memberasobject_typeadaptmember(&tree->nodeadp, iter->next), &nextk) ;

   patriciatrie_node_t * next = tree->root ;
   patriciatrie_node_t * parent ;
   patriciatrie_node_t * higher_branch_parent = 0 ;
   do {
      parent = next ;
      if (GETBIT(nextk.addr, nextk.size, next->bit_offset)) {
         next = next->right ;
      } else {
         higher_branch_parent = parent ;
         next = next->left ;
      }
   } while (next->bit_offset > parent->bit_offset) ;

   if (higher_branch_parent) {
      parent = higher_branch_parent ;
      next = parent->right ;
      while (next->bit_offset > parent->bit_offset) {
         parent = next ;
         next = next->left ;
      }
      iter->next = next ;
   } else {
      iter->next = 0 ;
   }

   return true ;
}

bool prev_patriciatrieiterator(patriciatrie_iterator_t * iter, patriciatrie_t * tree, /*out*/patriciatrie_node_t ** node)
{
   if (!iter->next) return false ;

   *node = iter->next ;

   typeadapt_binarykey_t nextk ;
   callgetbinarykey_typeadaptmember(&tree->nodeadp, memberasobject_typeadaptmember(&tree->nodeadp, iter->next), &nextk) ;

   patriciatrie_node_t * next = tree->root ;
   patriciatrie_node_t * parent ;
   patriciatrie_node_t * lower_branch_parent = 0 ;
   do {
      parent = next ;
      if (GETBIT(nextk.addr, nextk.size, next->bit_offset)) {
         lower_branch_parent = parent ;
         next = next->right ;
      } else {
         next = next->left ;
      }
   } while (next->bit_offset > parent->bit_offset) ;

   if (lower_branch_parent) {
      parent = lower_branch_parent ;
      next = parent->left ;
      while (next->bit_offset > parent->bit_offset) {
         parent = next ;
         next = next->right ;
      }
      iter->next = next ;
   } else {
      iter->next = 0 ;
   }

   return true ;
}

// section: patriciatrie_prefixiter_t

// group: lifetime

int initfirst_patriciatrieprefixiter(/*out*/patriciatrie_prefixiter_t * iter, patriciatrie_t * tree, size_t keylength, const uint8_t prefixkey[keylength])
{
   patriciatrie_node_t * parent ;
   patriciatrie_node_t * node = tree->root ;
   size_t         prefix_bits = 8 * keylength ;

   if (  (prefixkey || !keylength)
         && keylength < (((size_t)-1)/8)
         && node) {
      if (node->bit_offset < prefix_bits) {
         do {
            parent = node ;
            if (GETBIT(prefixkey, keylength, node->bit_offset)) {
               node = node->right ;
            } else {
               node = node->left ;
            }
         } while (node->bit_offset > parent->bit_offset && node->bit_offset < prefix_bits) ;
      } else {
         parent = node ;
         node = node->left ;
      }
      while (node->bit_offset > parent->bit_offset) {
         parent = node ;
         node = node->left ;
      }
      typeadapt_binarykey_t key ;
      callgetbinarykey_typeadaptmember(&tree->nodeadp, memberasobject_typeadaptmember(&tree->nodeadp, node), &key) ;
      bool isPrefix =   key.size >= keylength
                        && ! memcmp(key.addr, prefixkey, keylength) ;
      if (!isPrefix) node = 0 ;
   }

   iter->next        = node ;
   iter->prefix_bits = prefix_bits ;
   return 0 ;
}

// group: iterate

bool next_patriciatrieprefixiter(patriciatrie_prefixiter_t * iter, patriciatrie_t * tree, /*out*/patriciatrie_node_t ** node)
{
   if (!iter->next) return false ;

   *node = iter->next ;

   typeadapt_binarykey_t nextk ;
   callgetbinarykey_typeadaptmember(&tree->nodeadp, memberasobject_typeadaptmember(&tree->nodeadp, iter->next), &nextk) ;

   patriciatrie_node_t * next = tree->root ;
   patriciatrie_node_t * parent ;
   patriciatrie_node_t * higher_branch_parent = 0 ;
   do {
      parent = next ;
      if (GETBIT(nextk.addr, nextk.size, next->bit_offset)) {
         next = next->right ;
      } else {
         higher_branch_parent = parent ;
         next = next->left ;
      }
   } while (next->bit_offset > parent->bit_offset) ;

   if (  higher_branch_parent
         && higher_branch_parent->bit_offset >= iter->prefix_bits) {
      parent = higher_branch_parent ;
      next = parent->right ;
      while (next->bit_offset > parent->bit_offset) {
         parent = next ;
         next = next->left ;
      }
      iter->next = next ;
   } else {
      iter->next = 0 ;
   }

   return true ;
}


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

#define MAX_TREE_NODES        10000

typedef struct testnode_t     testnode_t ;
typedef struct testadapt_t    testadapt_t ;

struct testnode_t {
   patriciatrie_node_t  node ;
   uint8_t  key[4] ;
   size_t   key_len ;
   int      is_freed ;
   int      is_used ;
} ;

struct testadapt_t {
   struct {
      typeadapt_EMBED(testadapt_t, testnode_t, void) ;
   } ;
   test_errortimer_t    errcounter ;
   unsigned             freenode_count ;
   unsigned             getbinkey_count ;
} ;

static int impl_deletenode_testadapt(testadapt_t * testadp, testnode_t ** node)
{
   int err = process_testerrortimer(&testadp->errcounter) ;

   if (!err) {
      ++ testadp->freenode_count ;
      ++ (*node)->is_freed ;
   }

   *node = 0 ;

   return err ;
}

static void impl_getbinarykey_testadapt(testadapt_t * testadp, testnode_t * node, /*out*/typeadapt_binarykey_t * binkey)
{
   int err = process_testerrortimer(&testadp->errcounter) ;

   if (err) {
      binkey->addr = 0 ;
      binkey->size = 1 ;
   } else {
      ++ testadp->getbinkey_count ;
      binkey->addr = node->key ;
      binkey->size = node->key_len ;
   }
}

static int test_firstdiffbit(void)
{
   int err = 1 ;
   uint8_t  key1[256] = {0} ;
   uint8_t  key2[256] = {0} ;
   size_t   bit_offset = 0 ;

   // keys are equal
   TEST(EINVAL == first_different_bit(key1, sizeof(key1), key2, sizeof(key2), &bit_offset)) ;

   // keys are equal until end of key2 + (end marker 0xFF)
   key1[2] = 0xFF ;
   TEST(0==first_different_bit(key1, sizeof(key1), key2, 2, &bit_offset)) ;
   TEST(bit_offset == 8*sizeof(key1)) ;
   for (unsigned i = 1; i < 8; ++i) {
      key1[2] = (uint8_t)(0xFF << i) ;
      TEST(0 == first_different_bit(key1, sizeof(key1), key2, 2, &bit_offset)) ;
      TEST(bit_offset == 24-i) ;
   }
   key1[2] = 0 ;
   key2[2] = 0xFF ;
   TEST(0 == first_different_bit( key1, 2, key2, sizeof(key2),  &bit_offset)) ;
   TEST(bit_offset == 8*sizeof(key2)) ;
   for (unsigned i = 1; i < 8; ++i) {
      key2[2] = (uint8_t)(0xFF << i) ;
      TEST(0 == first_different_bit(key1, 2, key2, sizeof(key2), &bit_offset)) ;
      TEST(bit_offset == 24-i) ;
   }

   // keys differ in second byte
   for (int i = 0; i < 8; ++i) {
      key1[1] = (uint8_t) (0x01 << i) ;
      TEST(0 == first_different_bit(key1, sizeof(key1), key2, 2, &bit_offset)) ;
      TEST(bit_offset == (size_t)(15-i)) ;
      TEST(0 == first_different_bit(key1, 2, key2, sizeof(key2), &bit_offset)) ;
      TEST(bit_offset == (size_t)(15-i)) ;
   }

   // keys differ in first 8 byte
   for (unsigned i = 0; i < sizeof(key1); ++i) {
      key1[i] = (uint8_t)i ;
      key2[i] = (uint8_t)i ;
   }
   TEST(EINVAL==first_different_bit(key1, sizeof(key1), key2, sizeof(key2), &bit_offset)) ;
   TEST(0==first_different_bit(key1, sizeof(key1), key2, sizeof(key2)-1, &bit_offset)) ;
   TEST(bit_offset==8*sizeof(key1)) ;
   for (unsigned i = 0; i < 8; ++i) {
      key2[i] = (uint8_t)(0x80 >> i) ;
      key1[i] = 0 ;
      TEST(0 == first_different_bit(key1, sizeof(key1), key2, sizeof(key2)/3, &bit_offset)) ;
      TEST(bit_offset==8*i+i) ;
      TEST(0 == first_different_bit(key1, sizeof(key1)/3, key2, sizeof(key2), &bit_offset)) ;
      TEST(bit_offset==8*i+i) ;
      key1[i] = (uint8_t)i ;
      key2[i] = (uint8_t)i ;
   }

   err = 0 ;
ONABORT:
   return err ;
}

static int test_initfree(void)
{
   testadapt_t             typeadapt = { typeadapt_INIT_LIFEGETKEY(0, &impl_deletenode_testadapt, &impl_getbinarykey_testadapt), test_errortimer_INIT_FREEABLE, 0, 0 } ;
   typeadapt_member_t      nodeadapt = typeadapt_member_INIT(asgeneric_typeadapt(&typeadapt,testadapt_t,testnode_t,void), offsetof(testnode_t, node)) ;
   typeadapt_member_t emptynodeadapt = typeadapt_member_INIT_FREEABLE ;
   patriciatrie_t          tree = patriciatrie_INIT_FREEABLE ;
   patriciatrie_node_t     node = patriciatrie_node_INIT ;
   testnode_t              nodes[20] ;

   // prepare
   memset(nodes, 0, sizeof(nodes)) ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      nodes[i].key_len = sizeof(nodes[i].key) ;
      nodes[i].key[0] = (uint8_t) i ;
   }

   // TEST patriciatrie_node_INIT
   TEST(0 == node.bit_offset) ;
   TEST(0 == node.left) ;
   TEST(0 == node.right) ;

   // TEST patriciatrie_INIT_FREEABLE
   TEST(0 == tree.root) ;
   TEST(1 == isequal_typeadaptmember(&tree.nodeadp, &emptynodeadapt)) ;

   // TEST patriciatrie_INIT
   tree = (patriciatrie_t) patriciatrie_INIT((void*)7, nodeadapt) ;
   TEST(7 == (uintptr_t) tree.root) ;
   TEST(1 == isequal_typeadaptmember(&tree.nodeadp, &nodeadapt)) ;
   tree = (patriciatrie_t) patriciatrie_INIT((void*)0, emptynodeadapt) ;
   TEST(0 == tree.root) ;
   TEST(1 == isequal_typeadaptmember(&tree.nodeadp, &emptynodeadapt)) ;

   // TEST init_patriciatrie, free_patriciatrie
   tree.root = (void*)1 ;
   init_patriciatrie(&tree, &nodeadapt) ;
   TEST(0 == tree.root) ;
   TEST(1 == isequal_typeadaptmember(&tree.nodeadp, &nodeadapt)) ;
   TEST(0 == free_patriciatrie(&tree)) ;
   TEST(0 == tree.root) ;
   TEST(1 == isequal_typeadaptmember(&tree.nodeadp, &emptynodeadapt)) ;
   TEST(0 == free_patriciatrie(&tree)) ; // double free
   TEST(0 == tree.root) ;
   TEST(1 == isequal_typeadaptmember(&tree.nodeadp, &emptynodeadapt)) ;

   // TEST free_patriciatrie: frees all nodes
   init_patriciatrie(&tree, &nodeadapt) ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == insert_patriciatrie(&tree, &nodes[i].node)) ;
   }
   TEST(0 != tree.root) ;
   TEST(1 == isequal_typeadaptmember(&tree.nodeadp, &nodeadapt)) ;
   typeadapt.freenode_count = 0 ;
   TEST(0 == free_patriciatrie(&tree)) ;
   TEST(nrelementsof(nodes) == typeadapt.freenode_count) ;
   TEST(0 == tree.root) ;
   TEST(1 == isequal_typeadaptmember(&tree.nodeadp, &emptynodeadapt)) ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(1 == nodes[i].is_freed) ;
      TEST(0 == nodes[i].node.bit_offset) ;
      TEST(0 == nodes[i].node.left) ;
      TEST(0 == nodes[i].node.right) ;
      nodes[i].is_freed = 0 ;
   }

   // TEST free_patriciatrie: lifetime.delete_object set to 0
   init_patriciatrie(&tree, &nodeadapt) ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == insert_patriciatrie(&tree, &nodes[i].node)) ;
   }
   TEST(0 != tree.root) ;
   TEST(1 == isequal_typeadaptmember(&tree.nodeadp, &nodeadapt)) ;
   typeadapt.lifetime.delete_object = 0 ;
   typeadapt.freenode_count = 0 ;
   TEST(0 == free_patriciatrie(&tree)) ;
   typeadapt.lifetime.delete_object = &impl_deletenode_testadapt ;
   TEST(0 == typeadapt.freenode_count) ;
   TEST(0 == tree.root) ;
   TEST(1 == isequal_typeadaptmember(&tree.nodeadp, &emptynodeadapt)) ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == nodes[i].is_freed) ;
      TEST(0 == nodes[i].node.bit_offset) ;
      TEST(0 == nodes[i].node.left) ;
      TEST(0 == nodes[i].node.right) ;
   }

   // TEST free_patriciatrie: ERROR
   init_patriciatrie(&tree, &nodeadapt) ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == insert_patriciatrie(&tree, &nodes[i].node)) ;
   }
   TEST(0 != tree.root) ;
   TEST(1 == isequal_typeadaptmember(&tree.nodeadp, &nodeadapt)) ;
   typeadapt.freenode_count = 0 ;
   init_testerrortimer(&typeadapt.errcounter, 4, ENOENT) ;
   TEST(ENOENT == free_patriciatrie(&tree)) ;
   TEST(nrelementsof(nodes)-1 == typeadapt.freenode_count) ;
   TEST(0 == tree.root) ;
   TEST(1 == isequal_typeadaptmember(&tree.nodeadp, &emptynodeadapt)) ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST((i != 2) == nodes[i].is_freed) ;
      TEST(0 == nodes[i].node.bit_offset) ;
      TEST(0 == nodes[i].node.left) ;
      TEST(0 == nodes[i].node.right) ;
   }

   // TEST isempty_patriciatrie
   tree.root = (void*)1 ;
   TEST(0 == isempty_patriciatrie(&tree)) ;
   tree.root = (void*)0 ;
   TEST(1 == isempty_patriciatrie(&tree)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_insertremove(void)
{
   testadapt_t          typeadapt = { typeadapt_INIT_LIFEGETKEY(0, &impl_deletenode_testadapt, &impl_getbinarykey_testadapt), test_errortimer_INIT_FREEABLE, 0, 0 } ;
   typeadapt_member_t   nodeadapt = typeadapt_member_INIT(asgeneric_typeadapt(&typeadapt,testadapt_t,testnode_t,void), offsetof(testnode_t, node)) ;
   patriciatrie_t       tree      = patriciatrie_INIT_FREEABLE ;
   memblock_t           memblock  = memblock_INIT_FREEABLE ;
   testnode_t           * nodes   = 0 ;
   patriciatrie_node_t  * node ;
   unsigned             nodecount ;

   // prepare
   TEST(0 == MM_RESIZE(sizeof(testnode_t) * MAX_TREE_NODES, &memblock)) ;
   nodes = (testnode_t*)memblock.addr ;
   memset(nodes, 0, sizeof(testnode_t) * MAX_TREE_NODES) ;
   for (unsigned i = 0; i < MAX_TREE_NODES; ++i) {
      nodes[i].key_len = sizeof(nodes[i].key) ;
   }

   // TEST insert_patriciatrie: empty key (length == 0)
   init_patriciatrie(&tree, &nodeadapt) ;
   nodes[0].key_len = 0 ;
   TEST(0 == insert_patriciatrie(&tree, &nodes[0].node)) ;
   node = 0 ;
   TEST(0 == find_patriciatrie(&tree, 0, 0, &node)) ;
   TEST(&nodes[0].node == node) ;
   TEST(0 == removenodes_patriciatrie(&tree)) ;
   TEST(1 == nodes[0].is_freed) ;
   nodes[0].key_len = sizeof(nodes[0].key) ;
   nodes[0].is_freed = 0 ;

   // TEST EINVAL
   TEST(EINVAL == find_patriciatrie(&tree, 1, 0, &node)) ;
   TEST(EINVAL == find_patriciatrie(&tree, ((size_t)-1), nodes[0].key, &node)) ;
   TEST(EINVAL == find_patriciatrie(&tree, ((size_t)-1)/8, nodes[0].key, &node)) ;
   TEST(EINVAL == remove_patriciatrie(&tree, 1, 0, &node)) ;
   TEST(EINVAL == remove_patriciatrie(&tree, ((size_t)-1)/8, nodes[0].key, &node)) ;
   init_testerrortimer(&typeadapt.errcounter, 1, ENOMEM) ;
   TEST(EINVAL == insert_patriciatrie(&tree, &nodes[0].node)) ;
   nodes[0].key_len = ((size_t)-1)/8 ;
   TEST(EINVAL == insert_patriciatrie(&tree, &nodes[0].node)) ;
   nodes[0].key_len = sizeof(nodes[0].key) ;

   // TEST insert_patriciatrie, removenodes_patriciatrie: special case
   /*           (0111)
    *       /     ^     \
    *     (0011)  |   > (1111) <
    *      /  ^|__|   |___||___|
    * > (0001)|
    * |__| |__|
    */
   static_assert(sizeof(nodes[0].key) == 4, "code assumes 4 byte keys") ;
   memcpy(nodes[0].key, (uint8_t[4]){ 0, 1, 1, 1 }, 4) ;
   TEST(0 == insert_patriciatrie(&tree, &nodes[0].node)) ;
   memcpy(nodes[1].key, (uint8_t[4]){ 1, 1, 1, 1 }, 4) ;
   TEST(0 == insert_patriciatrie(&tree, &nodes[1].node)) ;
   memcpy(nodes[2].key, (uint8_t[4]){ 0, 0, 1, 1 }, 4) ;
   TEST(0 == insert_patriciatrie(&tree, &nodes[2].node)) ;
   memcpy(nodes[3].key, (uint8_t[4]){ 0, 0, 0, 1 }, 4) ;
   TEST(0 == insert_patriciatrie(&tree, &nodes[3].node)) ;
   TEST(0 == removenodes_patriciatrie(&tree)) ;
   for (int i = 0; i < 4; ++i) {
      TEST(1 == nodes[i].is_freed) ;
      nodes[i].is_freed = 0 ;
   }

   // TEST insert_patriciatrie, removenodes_patriciatrie: special case
   /*           (0111)
    *       /     ^     \
    * --->(0001)  |   > (1111) <
    * |    /   |__|   |___||___|
    * | (0011)<
    * |__| |__|
    */
   static_assert(sizeof(nodes[0].key) == 4, "code assumes 4 byte keys") ;
   memcpy(nodes[0].key, (uint8_t[4]){ 0, 1, 1, 1 }, 4) ;
   TEST(0 == insert_patriciatrie(&tree, &nodes[0].node)) ;
   memcpy(nodes[1].key, (uint8_t[4]){ 1, 1, 1, 1 }, 4) ;
   TEST(0 == insert_patriciatrie(&tree, &nodes[1].node)) ;
   memcpy(nodes[2].key, (uint8_t[4]){ 0, 0, 0, 1 }, 4) ;
   TEST(0 == insert_patriciatrie(&tree, &nodes[2].node)) ;
   memcpy(nodes[3].key, (uint8_t[4]){ 0, 0, 1, 1 }, 4) ;
   TEST(0 == insert_patriciatrie(&tree, &nodes[3].node)) ;
   TEST(0 == removenodes_patriciatrie(&tree)) ;
   for (int i = 0; i < 4; ++i) {
      TEST(1 == nodes[i].is_freed) ;
      nodes[i].is_freed = 0 ;
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
   memcpy(nodes[0].key, (uint8_t[4]){ 1, 1, 1, 1 }, 4) ;
   TEST(0 == insert_patriciatrie(&tree, &nodes[0].node)) ;
   memcpy(nodes[1].key, (uint8_t[4]){ 0, 0, 0, 0 }, 4) ;
   TEST(0 == insert_patriciatrie(&tree, &nodes[1].node)) ;
   memcpy(nodes[2].key, (uint8_t[4]){ 1, 0, 1, 1 }, 4) ;
   TEST(0 == insert_patriciatrie(&tree, &nodes[2].node)) ;
   memcpy(nodes[3].key, (uint8_t[4]){ 1, 0, 0, 0 }, 4) ;
   TEST(0 == insert_patriciatrie(&tree, &nodes[3].node)) ;
   TEST(0 == removenodes_patriciatrie(&tree)) ;
   for (int i = 0; i < 4; ++i) {
      TEST(1 == nodes[i].is_freed) ;
      nodes[i].is_freed = 0 ;
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
   assert(sizeof(nodes[0].key) == 4) ;
   memcpy(nodes[0].key, (uint8_t[4]){ 1, 1, 1, 1 }, 4) ;
   TEST(0 == insert_patriciatrie(&tree, &nodes[0].node)) ;
   memcpy(nodes[1].key, (uint8_t[4]){ 0, 0, 0, 0 }, 4) ;
   TEST(0 == insert_patriciatrie(&tree, &nodes[1].node)) ;
   memcpy(nodes[2].key, (uint8_t[4]){ 1, 0, 1, 1 }, 4) ;
   TEST(0 == insert_patriciatrie(&tree, &nodes[2].node)) ;
   memcpy(nodes[3].key, (uint8_t[4]){ 1, 0, 0, 0 }, 4) ;
   TEST(0 == insert_patriciatrie(&tree, &nodes[3].node)) ;
   memcpy(nodes[4].key, (uint8_t[4]){ 1, 1, 0, 1 }, 4) ;
   TEST(0 == insert_patriciatrie(&tree, &nodes[4].node)) ;
   memcpy(nodes[5].key, (uint8_t[4]){ 1, 1, 0, 0 }, 4) ;
   TEST(0 == insert_patriciatrie(&tree, &nodes[5].node)) ;
   TEST(tree.root == &nodes[0].node) ;
   TEST(nodes[0].node.left == &nodes[1].node) ;
   TEST(nodes[0].node.right == &nodes[2].node) ;
   TEST(nodes[2].node.left == &nodes[3].node) ;
   TEST(nodes[2].node.right == &nodes[4].node) ;
   TEST(nodes[4].node.left == &nodes[5].node) ;
   TEST(nodes[4].node.right == &nodes[0].node) ;
   TEST(0 == remove_patriciatrie(&tree, 4, nodes[0].key, &node)) ;
   TEST(tree.root == &nodes[4].node) ;
   TEST(nodes[4].node.left == &nodes[1].node) ;
   TEST(nodes[4].node.right == &nodes[2].node) ;
   TEST(nodes[2].node.left == &nodes[3].node) ;
   TEST(nodes[2].node.right == &nodes[5].node) ;
   TEST(nodes[5].node.left == &nodes[5].node) ;
   TEST(nodes[5].node.right == &nodes[4].node) ;
   TEST(0 == remove_patriciatrie(&tree, 4, nodes[5].key, &node)) ;
   TEST(tree.root == &nodes[4].node) ;
   TEST(nodes[4].node.left == &nodes[1].node) ;
   TEST(nodes[4].node.right == &nodes[2].node) ;
   TEST(nodes[2].node.left == &nodes[3].node) ;
   TEST(nodes[2].node.right == &nodes[4].node) ;
   TEST(0 == remove_patriciatrie(&tree, 4, nodes[4].key, &node)) ;
   TEST(tree.root == &nodes[2].node) ;
   TEST(nodes[2].node.left == &nodes[1].node) ;
   TEST(nodes[2].node.right == &nodes[3].node) ;
   TEST(nodes[3].node.left == &nodes[3].node) ;
   TEST(nodes[3].node.right == &nodes[2].node) ;
   TEST(0 == remove_patriciatrie(&tree, 4, nodes[1].key, &node)) ;
   TEST(tree.root == &nodes[3].node) ;
   TEST(nodes[3].node.left == &nodes[3].node) ;
   TEST(nodes[3].node.right == &nodes[2].node) ;
   TEST(nodes[2].node.left == &nodes[2].node) ;
   TEST(nodes[2].node.right == &nodes[2].node) ;
   TEST(0 == removenodes_patriciatrie(&tree)) ;
   TEST(1 == nodes[2].is_freed) ;
   TEST(1 == nodes[3].is_freed) ;
   nodes[3].is_freed = 0 ;
   nodes[2].is_freed = 0 ;

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
   assert(sizeof(nodes[0].key) == 4) ;
   memcpy(nodes[0].key, (uint8_t[4]){ 0, 0, 0, 0 }, 4) ;
   TEST(0 == insert_patriciatrie(&tree, &nodes[0].node)) ;
   memcpy(nodes[1].key, (uint8_t[4]){ 1, 1, 1, 1 }, 4) ;
   TEST(0 == insert_patriciatrie(&tree, &nodes[1].node)) ;
   memcpy(nodes[2].key, (uint8_t[4]){ 0, 1, 0, 0 }, 4) ;
   TEST(0 == insert_patriciatrie(&tree, &nodes[2].node)) ;
   memcpy(nodes[3].key, (uint8_t[4]){ 0, 1, 1, 0 }, 4) ;
   TEST(0 == insert_patriciatrie(&tree, &nodes[3].node)) ;
   memcpy(nodes[4].key, (uint8_t[4]){ 0, 0, 1, 0 }, 4) ;
   TEST(0 == insert_patriciatrie(&tree, &nodes[4].node)) ;
   memcpy(nodes[5].key, (uint8_t[4]){ 0, 0, 1, 1 }, 4) ;
   TEST(0 == insert_patriciatrie(&tree, &nodes[5].node)) ;
   TEST(tree.root == &nodes[0].node) ;
   TEST(nodes[0].node.left == &nodes[2].node) ;
   TEST(nodes[0].node.right == &nodes[1].node) ;
   TEST(nodes[2].node.left == &nodes[4].node) ;
   TEST(nodes[2].node.right == &nodes[3].node) ;
   TEST(nodes[4].node.left == &nodes[0].node) ;
   TEST(nodes[4].node.right == &nodes[5].node) ;
   TEST(0 == remove_patriciatrie(&tree, sizeof(nodes[0].key), nodes[0].key, &node)) ;
   TEST(tree.root == &nodes[4].node) ;
   TEST(nodes[4].node.left == &nodes[2].node) ;
   TEST(nodes[4].node.right == &nodes[1].node) ;
   TEST(nodes[2].node.left == &nodes[5].node) ;
   TEST(nodes[2].node.right == &nodes[3].node) ;
   TEST(nodes[5].node.left == &nodes[4].node) ;
   TEST(nodes[5].node.right == &nodes[5].node) ;
   TEST(0 == remove_patriciatrie(&tree, 4, nodes[5].key, &node)) ;
   TEST(tree.root == &nodes[4].node) ;
   TEST(nodes[4].node.left == &nodes[2].node) ;
   TEST(nodes[4].node.right == &nodes[1].node) ;
   TEST(nodes[2].node.left == &nodes[4].node) ;
   TEST(nodes[2].node.right == &nodes[3].node) ;
   TEST(0 == remove_patriciatrie(&tree, 4, nodes[4].key, &node)) ;
   TEST(tree.root == &nodes[2].node) ;
   TEST(nodes[2].node.left == &nodes[3].node) ;
   TEST(nodes[2].node.right == &nodes[1].node) ;
   TEST(nodes[3].node.left == &nodes[2].node) ;
   TEST(nodes[3].node.right == &nodes[3].node) ;
   TEST(0 == remove_patriciatrie(&tree, 4, nodes[1].key, &node)) ;
   TEST(tree.root == &nodes[3].node) ;
   TEST(nodes[3].node.left == &nodes[2].node) ;
   TEST(nodes[3].node.right == &nodes[3].node) ;
   TEST(nodes[2].node.left == &nodes[2].node) ;
   TEST(nodes[2].node.right == &nodes[2].node) ;
   TEST(0 == removenodes_patriciatrie(&tree)) ;
   TEST(1 == nodes[2].is_freed) ;
   TEST(1 == nodes[3].is_freed) ;
   nodes[3].is_freed = 0 ;
   nodes[2].is_freed = 0 ;

   // TEST insert_patriciatrie: three keys, 2nd. bit 0 differs, 3rd. bit 8 differs
   memcpy(nodes[0].key, (uint8_t[4]){ 0, 0, 0, 0 }, 4) ;
   TEST(0 == insert_patriciatrie(&tree, &nodes[0].node)) ;
   memcpy(nodes[1].key, (uint8_t[4]){ 255, 0, 0, 0 }, 4) ;
   TEST(0 == insert_patriciatrie(&tree, &nodes[1].node)) ;
   memcpy(nodes[2].key, (uint8_t[4]){ 255, 255, 0, 0 }, 4) ;
   TEST(0 == insert_patriciatrie(&tree, &nodes[2].node)) ;
   TEST(tree.root == &nodes[0].node) ;
   TEST(tree.root->right == &nodes[1].node) ;
   TEST(nodes[1].node.right == &nodes[2].node) ;

   // TEST remove_patriciatrie
   TEST(0 == remove_patriciatrie(&tree, sizeof(nodes[0].key), nodes[0].key, &node)) ;
   TEST(node == &nodes[0].node) ;
   TEST(0 == remove_patriciatrie(&tree, sizeof(nodes[1].key), nodes[1].key, &node)) ;
   TEST(node == &nodes[1].node) ;
   TEST(0 == remove_patriciatrie(&tree, sizeof(nodes[2].key), nodes[2].key, &node)) ;
   TEST(node == &nodes[2].node) ;
   TEST(0 == tree.root) ;

   // TEST insert_patriciatrie: three keys, 2nd. bit 8 differs, 3rd. bit 0 differs
   memcpy(nodes[0].key, (uint8_t[4]){ 0, 0, 0, 0 }, 4) ;
   TEST(0 == insert_patriciatrie(&tree, &nodes[0].node)) ;
   memcpy(nodes[1].key, (uint8_t[4]){ 0, 255, 0, 0 }, 4) ;
   TEST(0 == insert_patriciatrie(&tree, &nodes[1].node)) ;
   memcpy(nodes[2].key, (uint8_t[4]){ 255, 255, 0, 0 }, 4) ;
   TEST(0 == insert_patriciatrie(&tree, &nodes[2].node)) ;
   TEST(tree.root == &nodes[2].node) ;
   TEST(tree.root->left == &nodes[0].node) ;
   TEST(nodes[0].node.right == &nodes[1].node) ;

   // TEST remove_patriciatrie
   TEST(0 == remove_patriciatrie(&tree, sizeof(nodes[1].key), nodes[1].key, &node)) ;
   TEST(node == &nodes[1].node) ;
   TEST(0 == remove_patriciatrie(&tree, sizeof(nodes[0].key), nodes[0].key, &node)) ;
   TEST(node == &nodes[0].node) ;
   TEST(0 == remove_patriciatrie(&tree, sizeof(nodes[2].key), nodes[2].key, &node)) ;
   TEST(node == &nodes[2].node) ;
   TEST(0 == tree.root) ;

   // TEST insert_patriciatrie, removenodes_patriciatrie
   nodecount = 0 ;
   for (int i1 = 0; i1 < 4; ++i1)
      for (int i2 = 0; i2 < 4; ++i2)
         for (int i3 = 0; i3 < 4; ++i3)
            for (int i4 = 0; i4 < 4; ++i4) {
               uint8_t key[sizeof(nodes[nodecount].key)] = { (uint8_t)i1, (uint8_t)i2, (uint8_t)i3, (uint8_t)i4  } ;
               memcpy(nodes[nodecount].key, key, sizeof(nodes[nodecount].key)) ;
               TEST(0 == insert_patriciatrie(&tree, &nodes[nodecount].node)) ;
               ++ nodecount ;
            }
   TEST(0 == removenodes_patriciatrie(&tree)) ;
   for (unsigned i = 0; i < nodecount; ++i) {
      TEST(1 == nodes[i].is_freed) ;
      nodes[i].is_freed = 0 ;
   }

   // TEST insert_patriciatrie, find_patriciatrie, removenodes_patriciatrie: EEXIST
   nodecount = 0 ;
   for (int i1 = 0; i1 < 4; ++i1)
      for (int i2 = 0; i2 < 4; ++i2)
         for (int i3 = 0; i3 < 4; ++i3)
            for (int i4 = 0; i4 < 4; ++i4) {
               uint8_t key[sizeof(nodes[0].key)] = { (uint8_t)i1, (uint8_t)i2, (uint8_t)i3, (uint8_t)i4  } ;
               memcpy(nodes[nodecount].key, key, sizeof(key)) ;
               TEST(0 == insert_patriciatrie(&tree, &nodes[nodecount].node)) ;
               TEST(EEXIST == insert_patriciatrie(&tree, &nodes[nodecount].node)) ;
               testnode_t nodecopy = { .key_len = sizeof(key) } ;
               memcpy(nodecopy.key, key, sizeof(key)) ;
               TEST(EEXIST == insert_patriciatrie(&tree, &nodecopy.node)) ;
               TEST(0 == find_patriciatrie(&tree, sizeof(key), key, &node)) ;
               TEST(&nodes[nodecount].node == node) ;
               TEST(!nodes[nodecount].is_freed) ;
               ++ nodecount ;
            }
   TEST(0 == removenodes_patriciatrie(&tree)) ;
   for (unsigned i = 0; i < MAX_TREE_NODES; ++i) {
      if (i < nodecount) {
         TEST(nodes[i].is_freed) ;
         nodes[i].is_freed = 0 ;
      }
      TEST(0 == nodes[i].is_freed) ;
   }

   // TEST insert_patriciatrie, remove_patriciatrie
   for (unsigned i = 0; i < MAX_TREE_NODES; ++i) {
      uint32_t key = (uint32_t)i ;
      assert(sizeof(nodes[i].key) == sizeof(key)) ;
      memcpy(nodes[i].key, &key, sizeof(key)) ;
      TEST(0 == insert_patriciatrie(&tree, &nodes[i].node)) ;
   }
   for (unsigned i = 0; i < MAX_TREE_NODES; ++i) {
      uint32_t key = (uint32_t)(MAX_TREE_NODES-1-i) ;
      TEST(0 == remove_patriciatrie(&tree, sizeof(key), (uint8_t*)&key, &node)) ;
      TEST(node == &nodes[MAX_TREE_NODES-1-i].node) ;
      TEST(!nodes[MAX_TREE_NODES-1-i].is_freed) ;
   }
   TEST(0 == removenodes_patriciatrie(&tree)) ;
   for (unsigned i = 0; i < MAX_TREE_NODES; ++i) {
      TEST(0 == nodes[i].is_freed) ;
   }

   // TEST insert_patriciatrie, remove_patriciatrie: random
   for (int i = 0; i < MAX_TREE_NODES; ++i) {
      assert(sizeof(nodes[i].key) == 4) ;
      nodes[i].is_freed = 0 ;
      nodes[i].is_used = 0 ;
      nodes[i].key[0] = (uint8_t)((i/1000) % 10) ;
      nodes[i].key[1] = (uint8_t)((i/100) % 10) ;
      nodes[i].key[2] = (uint8_t)((i/10) % 10) ;
      nodes[i].key[3] = (uint8_t)((i/1) % 10) ;
   }
   srand(100) ;
   for (int i = 0; i < 10*MAX_TREE_NODES; ++i) {
      int id = rand() % MAX_TREE_NODES ;
      assert(id >= 0 && id < MAX_TREE_NODES) ;
      if (nodes[id].is_used) {
         nodes[id].is_used = 0 ;
         TEST(0 == remove_patriciatrie(&tree, sizeof(nodes[id].key), nodes[id].key, &node)) ;
         TEST(&nodes[id].node == node) ;
         TEST(ESRCH == remove_patriciatrie(&tree, sizeof(nodes[id].key), nodes[id].key, &node)) ;
      } else {
         nodes[id].is_used = 1 ;
         TEST(0 == insert_patriciatrie(&tree, &nodes[id].node)) ;
         TEST(0 == find_patriciatrie(&tree, sizeof(nodes[id].key), nodes[id].key, &node)) ;
         TEST(&nodes[id].node == node) ;
         TEST(EEXIST == insert_patriciatrie(&tree, &nodes[id].node)) ;
      }
   }
   removenodes_patriciatrie(&tree) ;
   for (int i = 0; i < MAX_TREE_NODES; ++i) {
      if (nodes[i].is_freed) {
         TEST(nodes[i].is_used) ;
         nodes[i].is_used = 0 ;
         nodes[i].is_freed = 0 ;
      }
      TEST(! nodes[i].is_used) ;
   }

   // unprepare
   TEST(0 == MM_FREE(&memblock)) ;
   nodes = 0 ;

   return 0 ;
ONABORT:
   (void) MM_FREE(&memblock) ;
   return EINVAL ;
}

static int test_iterator(void)
{
   testadapt_t                typeadapt = { typeadapt_INIT_LIFEGETKEY(0, &impl_deletenode_testadapt, &impl_getbinarykey_testadapt), test_errortimer_INIT_FREEABLE, 0, 0 } ;
   typeadapt_member_t         nodeadapt = typeadapt_member_INIT(asgeneric_typeadapt(&typeadapt,testadapt_t,testnode_t,void), offsetof(testnode_t, node)) ;
   patriciatrie_t             tree      = patriciatrie_INIT_FREEABLE ;
   memblock_t                 memblock  = memblock_INIT_FREEABLE ;
   testnode_t                 * nodes   = 0 ;
   patriciatrie_iterator_t    iter      = patriciatrie_iterator_INIT_FREEABLE ;
   patriciatrie_prefixiter_t  preiter   = patriciatrie_prefixiter_INIT_FREEABLE ;
   patriciatrie_node_t        * found_node ;

   // prepare
   TEST(0 == MM_RESIZE(sizeof(testnode_t) * MAX_TREE_NODES, &memblock)) ;
   nodes = (testnode_t*)memblock.addr ;
   memset(nodes, 0, sizeof(testnode_t) * MAX_TREE_NODES) ;
   init_patriciatrie(&tree, &nodeadapt) ;
   for (unsigned i = 0; i < MAX_TREE_NODES; ++i) {
      static_assert(sizeof(nodes[i].key) == 4, "key assignment thinks keysize is 4") ;
      nodes[i].key_len = sizeof(nodes[i].key) ;
      nodes[i].key[0] = (uint8_t)((i/1000) % 10) ;
      nodes[i].key[1] = (uint8_t)((i/100) % 10) ;
      nodes[i].key[2] = (uint8_t)((i/10) % 10) ;
      nodes[i].key[3] = (uint8_t)((i/1) % 10) ;
   }

   // TEST patriciatrie_iterator_INIT_FREEABLE
   TEST(0 == iter.next) ;

   // TEST initfirst_patriciatrieiterator: empty tree
   iter.next = (void*)1 ;
   TEST(0 == initfirst_patriciatrieiterator(&iter, &tree)) ;
   TEST(0 == iter.next) ;

   // TEST initlast_patriciatrieiterator: empty tree
   iter.next = (void*)1 ;
   TEST(0 == initlast_patriciatrieiterator(&iter, &tree)) ;
   TEST(0 == iter.next) ;

   // TEST next_patriciatrieiterator: empty tree
   TEST(0 == initfirst_patriciatrieiterator(&iter, &tree)) ;
   TEST(0 == next_patriciatrieiterator(&iter, 0/*not needed*/, 0/*not used*/)) ;

   // TEST prev_patriciatrieiterator: empty tree
   TEST(0 == initlast_patriciatrieiterator(&iter, &tree)) ;
   TEST(0 == prev_patriciatrieiterator(&iter, 0/*not needed*/, 0/*not used*/)) ;

   // TEST free_patriciatrieiterator
   iter.next = (void*)1 ;
   TEST(0 == free_patriciatrieiterator(&iter)) ;
   TEST(0 == iter.next) ;

   // TEST patriciatrie_prefixiter_INIT_FREEABLE
   TEST(0 == preiter.next) ;
   TEST(0 == preiter.prefix_bits) ;

   // TEST initfirst_patriciatrieprefixiter: empty tree
   preiter.next = (void*)1 ;
   preiter.prefix_bits = 1 ;
   TEST(0 == initfirst_patriciatrieprefixiter(&preiter, &tree, 0, 0)) ;
   TEST(0 == preiter.next) ;
   TEST(0 == preiter.prefix_bits) ;

   // TEST next_patriciatrieprefixiter: empty tree
   TEST(0 == initfirst_patriciatrieprefixiter(&preiter, &tree, 0, 0)) ;
   TEST(0 == next_patriciatrieprefixiter(&preiter, 0/*not needed*/, 0/*not used*/)) ;

   // TEST free_patriciatrieprefixiter
   preiter.next = (void*)1 ;
   TEST(0 == free_patriciatrieprefixiter(&preiter)) ;
   TEST(0 == preiter.next) ;

   // TEST random iterator
   srand(400) ;
   for (int test = 0; test < 20; ++test) {
      typeadapt.getbinkey_count = 0 ;
      for (int i = 0; i < MAX_TREE_NODES; ++i) {
         int id = rand() % MAX_TREE_NODES ;
         if (nodes[id].is_used) {
            nodes[id].is_used = 0 ;
            patriciatrie_node_t * removed_node ;
            TEST(0 == remove_patriciatrie(&tree, sizeof(nodes[id].key), nodes[id].key, &removed_node)) ;
         } else {
            nodes[id].is_used = 1 ;
            TEST(0 == insert_patriciatrie(&tree, &nodes[id].node)) ;
         }
      }
      TEST(typeadapt.getbinkey_count > MAX_TREE_NODES) ;
      TEST(0 == initfirst_patriciatrieiterator(&iter, &tree)) ;
      for (int i = 0; i < MAX_TREE_NODES; ++i) {
         if (! (i % 10)) {
            TEST(0 == initfirst_patriciatrieprefixiter(&preiter, &tree, sizeof(nodes[i].key)-1, nodes[i].key)) ;
         }
         if (nodes[i].is_used) {
            TEST(1 == next_patriciatrieprefixiter(&preiter, &tree, &found_node)) ;
            TEST(found_node == &nodes[i].node) ;
            TEST(1 == next_patriciatrieiterator(&iter, &tree, &found_node)) ;
            TEST(found_node == &nodes[i].node) ;
         }
      }
      found_node = (void*)1 ;
      TEST(false == next_patriciatrieiterator(&iter, &tree, &found_node)) ;
      TEST(found_node == (void*)1/*not changed*/) ;
      TEST(0 == initlast_patriciatrieiterator(&iter, &tree)) ;
      for (int i = MAX_TREE_NODES-1; i >= 0 ; --i) {
         if (nodes[i].is_used) {
            TEST(1 == prev_patriciatrieiterator(&iter, &tree, &found_node)) ;
            TEST(found_node == &nodes[i].node) ;
         }
      }
      found_node = (void*)1 ;
      TEST(false == prev_patriciatrieiterator(&iter, &tree, &found_node)) ;
      TEST(found_node == (void*)1/*not changed*/) ;
   }

   TEST(0 == removenodes_patriciatrie(&tree)) ;
   for (int i = 0; i < MAX_TREE_NODES; ++i) {
      if (nodes[i].is_freed) {
         TEST(nodes[i].is_used) ;
         nodes[i].is_used = 0 ;
         nodes[i].is_freed = 0 ;
      }
      TEST(! nodes[i].is_used) ;
   }

   // TEST prefix iterator
   for (int i = 0; i < MAX_TREE_NODES ; ++i) {
      nodes[i].key[0] = 0 ;
      nodes[i].key[1] = (uint8_t)(i / 256) ;
      nodes[i].key[2] = 0 ;
      nodes[i].key[3] = (uint8_t)(i % 256) ;
   }
   for (int i = 0; i < MAX_TREE_NODES; ++i) {
      TEST(0 == insert_patriciatrie(&tree, &nodes[i].node)) ;
   }
   TEST(0 == initfirst_patriciatrieprefixiter(&preiter, &tree, 1, nodes[0].key)) ;
   for (int i = 0; i < MAX_TREE_NODES; ++i) {
      TEST(1 == next_patriciatrieprefixiter(&preiter, &tree, &found_node)) ;
      TEST(found_node == &nodes[i].node) ;
   }
   found_node = (void*)1 ;
   TEST(false == next_patriciatrieprefixiter(&preiter, &tree, &found_node)) ;
   TEST(found_node == (void*)1/*not changed*/) ;
   for (int i = 0; i < MAX_TREE_NODES-255; i += 256) {
      TEST(0 == initfirst_patriciatrieprefixiter(&preiter, &tree, 2, nodes[i].key)) ;
      for (int g = 0; g <= 255; ++g) {
         TEST(1 == next_patriciatrieprefixiter(&preiter, &tree, &found_node)) ;
         TEST(found_node == &nodes[i+g].node) ;
      }
      found_node = (void*)1 ;
      TEST(false == next_patriciatrieprefixiter(&preiter, &tree, &found_node)) ;
      TEST(found_node == (void*)1/*not changed*/) ;
   }

   // unprepare
   TEST(0 == MM_FREE(&memblock)) ;
   nodes = 0 ;

   return 0 ;
ONABORT:
   (void) MM_FREE(&memblock) ;
   return EINVAL ;
}

int unittest_ds_inmem_patriciatrie()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_firstdiffbit())      goto ONABORT ;
   if (test_initfree())          goto ONABORT ;
   if (test_insertremove())      goto ONABORT ;
   if (test_iterator())          goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
