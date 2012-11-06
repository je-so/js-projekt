/* title: RedBlacktree-Index impl

   Implement <RedBlacktree-Index>.

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
   (C) 2011 Jörg Seebohn

   file: C-kern/api/ds/inmem/redblacktree.h
    Header file of <RedBlacktree-Index>.

   file: C-kern/ds/inmem/redblacktree.c
    Implementation file of <RedBlacktree-Index impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/ds/typeadapt.h"
#include "C-kern/api/ds/inmem/redblacktree.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/ds/foreach.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#include "C-kern/api/test/errortimer.h"
#endif


// section: redblacktree_t

// group: internal macros

/* define: COLOR
 * Returns the color bit of the node.
 *
 * Returns:
 * 1 - color is black
 * 0 - color is red */
#define COLOR(node)        (((uintptr_t)1) & (uintptr_t)((node)->parent))

/* define: ISBLACK
 * Returns true if color is black. */
#define ISBLACK(node)      (0 != COLOR(node))

/* define: ISRED
 * Returns true if color is red. */
#define ISRED(node)        (0 == COLOR(node))

/* define: PARENT
 * Access parent pointer from node.
 * The reason to use this this macro is that the color of a node
 * is encoded in the lowest bit of the parent pointer. */
#define PARENT(node)       ((redblacktree_node_t*) (((uintptr_t)-2) & (uintptr_t)((node)->parent)))

/* define: SETBLACK
 * Sets the color of node to black. */
#define SETBLACK(node)     (node)->parent = (redblacktree_node_t*) (((uintptr_t)1) | (uintptr_t)((node)->parent))

/* define: SETRED
 * Sets the color of node to red. */
#define SETRED(node)       (node)->parent = PARENT(node)

/* define: SETPARENT
 * Sets the new parent of node and keeps the encoded color.
 * The least bit of newparent must be cleared. */
#define SETPARENT(node, newparent) \
         (node)->parent = (redblacktree_node_t*) (COLOR(node) | (uintptr_t)newparent)

/* define: SETPARENTRED
 * Sets a new parent for node and sets its color to red.
 * The least bit of newparent must be cleared. */
#define SETPARENTRED(node, newparent) \
         (node)->parent = newparent

/* define: SETPARENTBLACK
 * Sets a new parent for node and sets its color to black.
 * The least bit of newparent must be cleared. */
#define SETPARENTBLACK(node, newparent) \
         (node)->parent = (redblacktree_node_t*) (((uintptr_t)1) | (uintptr_t)newparent)

/* define: EVENADDRESS
 * Test for node having an even address (bit 0 is clear). */
#define EVENADDRESS(node)        ((1u & (uintptr_t)(node)) == 0)

/* define: KEYCOMPARE
 * Casts node to type <typeadapt_object_t> and calls <callcmpkeyobj_typeadapt>.
 * This macro expects variable name tree to point to <redblacktree_t>. */
#define KEYCOMPARE(key,node)     callcmpkeyobj_typeadaptmember(&tree->nodeadp, key, memberasobject_typeadaptmember(&tree->nodeadp, node))

/* define: NODECOMPARE
 * Casts both nodes to type <typeadapt_object_t> and calls <callcmpobj_typeadapt>.
 * This macro expects variable name tree to point to <redblacktree_t>. */
#define NODECOMPARE(lnode,rnode) callcmpobj_typeadaptmember(&tree->nodeadp, memberasobject_typeadaptmember(&tree->nodeadp, lnode), memberasobject_typeadaptmember(&tree->nodeadp, rnode))

// group: test

int invariant_redblacktree(redblacktree_t * tree)
{
   redblacktree_node_t  * prev = 0 ;
   redblacktree_node_t  * node = tree->root ;

   if (!node) {
      return 0 ;
   }

   if (  !ISBLACK(node)
         || PARENT(node) != 0) {
      goto ONABORT ;
   }

   // determine height of tree

   size_t height = 1 ;  // black height of root node
   while (node->left) {
      if (PARENT(node->left) != node) goto ONABORT ;
      node = node->left ;
      if (ISBLACK(node)) {
         ++ height ;
      }
   }

   const size_t const_height = height ;

   do {

      if (node->left  && NODECOMPARE(node->left,  node) >= 0) goto ONABORT ;
      if (node->right && NODECOMPARE(node->right, node) <= 0) goto ONABORT ;

      if (ISRED(node)) {
         if (node->left  && ISRED(node->left))  goto ONABORT ;
         if (node->right && ISRED(node->right)) goto ONABORT ;
      }

      if (prev) {
         if (NODECOMPARE(node, prev) <= 0) goto ONABORT ;
         if (NODECOMPARE(prev, node) >= 0) goto ONABORT ;
      }

      prev = node ;

      if (  (!node->left || !node->right) // is leaf
            && const_height != height) {
         goto ONABORT ;
      }

      if (!node->right) {
         redblacktree_node_t * parent ;
         for (;;) {
            if (ISBLACK(node)) -- height ;
            parent = PARENT(node) ;
            if (  !parent
                  || parent->left == node) {
               break ;
            }
            node = parent ;
         }
         node = parent ;
      } else {
         if (PARENT(node->right) != node) goto ONABORT ;
         node = node->right ;
         if (ISBLACK(node)) ++ height ;

         while (node->left) {
            if (PARENT(node->left) != node) goto ONABORT ;
            node = node->left ;
            if (ISBLACK(node)) {
               ++ height ;
            }
         }
      }

   } while (node) ;

   if (height != 0) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(EINVAL) ;
   return EINVAL ;
}

// group: lifetime

int free_redblacktree(redblacktree_t * tree)
{
   int err = removenodes_redblacktree(tree) ;

   tree->nodeadp = (typeadapt_member_t) typeadapt_member_INIT_FREEABLE ;

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

// group: search

int find_redblacktree(redblacktree_t * tree, const void * key, /*out*/redblacktree_node_t ** found_node)
{
   redblacktree_node_t * node = tree->root ;

   while (node) {
      int cmp = KEYCOMPARE(key, node) ;
      if (cmp == 0) {
         *found_node = node ;
         return 0 ;
      }
      if (cmp < 0) {
         node = node->left ;
      } else {
         node = node->right ;
      }
   }

   return ESRCH ;
}

// group: change

static redblacktree_node_t * rotateLeft(redblacktree_t * tree, redblacktree_node_t * node)
{
   redblacktree_node_t * parent = PARENT(node) ;
   redblacktree_node_t * right  = node->right ;
   assert(right) ;

   // left child of right child becomes right child of node
   node->right = right->left ;
   if (right->left) SETPARENT(right->left, node) ;

   // node is now left child of former right child
   right->left = node ;
   SETPARENT(node, right) ;

   // former parent of node is now parent of former right child
   SETPARENT(right, parent) ;
   if (parent) {
      if (parent->left == node)
         parent->left = right ;
      else
         parent->right = right ;
   } else {
      tree->root = right ;
   }

   // right replaced node
   return right ;
}

static redblacktree_node_t * rotateRight(redblacktree_t * tree, redblacktree_node_t * node)
{
   redblacktree_node_t * parent = PARENT(node) ;
   redblacktree_node_t * left   = node->left ;
   assert(left) ;

   // right child of left becomes left child of node
   node->left = left->right ;
   if (left->right) SETPARENT(left->right, node) ;

   // node is now right child of former left child
   left->right = node ;
   SETPARENT(node, left) ;

   // former parent of node is now parent of former left child
   SETPARENT(left, parent) ;
   if (parent) {
      if (parent->left == node)
         parent->left  = left ;
      else
         parent->right = left ;
   } else {
      tree->root = left ;
   }

   // left replaced node
   return left ;
}

static void rebalanceAfterInsert(redblacktree_t * tree, redblacktree_node_t * const inserted_node)
{
   assert(inserted_node) ;
   assert(ISRED(inserted_node)) ;

   redblacktree_node_t * child = inserted_node ;
   redblacktree_node_t * node  = PARENT(inserted_node) ;

   assert(node) ;
   assert(ISRED(node)) ;

   /*    I. case        II. dual case
    *        parent       parent
    *       /       \    /      \
    *      node (RED) uncle    node (RED)
    *     /                       \
    *    child (RED)           child (RED)
    */

   // repair all RED-RED conflicts
   for (;;) {

      // ISRED(node) => node != root => parent != NULL
      redblacktree_node_t  * parent = PARENT(node) ;
      if (node == parent->left)
      {  //  I.
         redblacktree_node_t * right = parent->right ;
         if (!right || ISBLACK(right)) {
            // uncle is BLACK => rotation
            if (child == node->right)
            {  // make child a left child
               child =  node ;
               node  =  rotateLeft (tree, node) ;
            }
            SETBLACK(node) ;
            SETRED(parent) ;
            rotateRight (tree, parent) ;
            return ;  // ready
         }
         // uncle is RED => propagate red
         SETBLACK(node) ;
         SETBLACK(right) ;
         SETRED(parent) ;
         child =  parent ;
         node  =  PARENT(parent) ;
     } else {
         //  II.
         redblacktree_node_t * left = parent->left ;
         if (!left || ISBLACK(left)) {
            // uncle is BLACK => rotation
            if (child == node->left)
            {  // make child a right child
               child =  node ;
               node  =  rotateRight (tree, node) ;
            }
            SETBLACK(node) ;
            SETRED(parent) ;
            rotateLeft (tree, parent) ;
            return ; // ready
         }
         // uncle is RED => propagate red
         SETBLACK(node) ;
         SETBLACK(left) ;
         SETRED(parent) ;
         child =  parent ;
         node  =  PARENT(parent) ;
      }

      if (!node) { // child is root !
         SETBLACK(tree->root) ;
         return ;
      } else if (ISBLACK(node)) {
         return ;  // NO RED-RED conflict, DONE
      }

   }

}


static void rebalanceAfterRemove(redblacktree_t * tree, const bool isNodeLeft, redblacktree_node_t * const parent_node)
{
   // height is reduced (removed node was black)

   assert(parent_node) ;

   redblacktree_node_t * parent = parent_node ;
   bool                  isLeft = isNodeLeft ;

   /*    I. case right is RED (no dual case drawn)
    *            parent (BLACK)
    *       /             \
    *     node (removed)   right (RED)
    *                      /    \
    *                     RL     RR (both BLACK + != NULL)
    *    => (make right BLACK)
    *           right (BLACK)
    *       /             \
    *     parent (RED)     RR (BLACK)
    *     /           \
    *  node (removed)  RL (BLACK)
    *                  (RL is the new right)
    *
    *   II. case (children of right are BLACK or NULL)
    *            parent (?)
    *       /             \
    *     node (removed)   right (BLACK)
    *                      /    \
    *                     RL     RR (both BLACK or NULL)
    *    => (make right RED) =>  node subtree has same height as right subtree (both reduced by one)
    *            parent (?)
    *       /             \
    *     node (removed)   right (RED)
    *                      /    \
    *                     RL     RR (both BLACK or NULL)
    *
    *   II.1 subcase (parent is RED)
    *            parent (RED)
    *    => (make parent BLACK) =>  both subtrees has increased in height => DONE
    *            parent (BLACK)
    *
    *   II.2 subcase (parent is BLACK)
    *            parent (BLACK)
    *    => (make parent BLACK) =>  both subtrees has decreased in height =>
    *            propagate correction up one node
    *
    *   III. case (children of right are RED (left) and BLACK (right) (changed in dual case))
    *          parent (?)
    *       /             \
    *     node (removed)   right (BLACK)
    *                      /        \
    *                     RL (RED)   RR (BLACK)
    *    => convert case III. into case IV.
    *          parent (?)
    *       /             \
    *     node (removed)   RL (BLACK)
    *                            \
    *                            right (RED)
    *                              \
    *                              RR (BLACK)
    *
    *   IV. case (right children of right is RED (changed in dual case))
    *          parent (?)
    *       /             \
    *     node (removed)   right (BLACK)
    *                      /       \
    *                     RL (?)   RR (RED)
    *    => rotate left (RR subtree has same height, RL also, node subtree is increased by one) => DONE
    *         right (same as parent(?) from above)
    *       /                \
    *     parent (BLACK)      RR (BLACK)
    *    /             \
    *   node (removed)  RL(?)
    */
   for (;;)  {

      if (isLeft) {
         // node is left child
         redblacktree_node_t * right = parent->right ; // right != NULL
         if (ISRED(right)) {
            // case I.
            SETBLACK(right) ;
            SETRED(parent) ;
            rotateLeft(tree, parent) ;
            right = parent->right ;   // ISBLACK(right) && right != NULL
         }

         if (  (!right->left || ISBLACK(right->left))
               && (!right->right || ISBLACK(right->right))) {
            // case II.
            SETRED(right) ;  // reduce height of right side

         } else {
            // case III.
            if (!right->right || ISBLACK(right->right)) {
               // ISBLACK(right->right) => right->left != NULL  !+! ISRED(right->left)
               SETBLACK(right->left) ;
               // not necessary ! SETRED(right) ;
               right = rotateRight(tree, right) ; // right != NULL && ISBLACK(right)
            }
            // case IV.
            if (ISRED(parent)) {
               SETRED(right) ;
               SETBLACK(parent) ;
            }
            SETBLACK(right->right) ;
            rotateLeft(tree, parent) ;
            return ; // DONE
         }
      } else {
         // node is right child
         redblacktree_node_t * left = parent->left ;  // left != NULL
         if (ISRED(left)) {
            // case I.
            SETBLACK(left) ;
            SETRED(parent) ;
            rotateRight(tree, parent) ;
            left = parent->left ;   // ISBLACK(left) && left != NULL
         }

         if (  (!left->left || ISBLACK(left->left))
               && (!left->right || ISBLACK(left->right))) {
            // case II.
            SETRED(left) ;  // reduce height of left side

         } else {
            // case III.
            if (!left->left || ISBLACK(left->left)) {
               // ISBLACK(left->left) => left->right != NULL  !+! ISRED(left->right)
               SETBLACK(left->right) ;
               // not necessary ! SETRED(left) ;
               left = rotateLeft(tree, left) ; // left != NULL && ISBLACK(left)
            }
            // case IV.
            if (ISRED(parent)) {
               SETRED(left) ;
               SETBLACK(parent) ;
            }
            SETBLACK(left->left) ;
            rotateRight(tree, parent) ;
            return ; // DONE
         }
      }

      if (ISRED(parent)) {
         // case II.1
         SETBLACK(parent) ;
         return ; // done, cause right side keeps same height and left is made one higher
      }
      // case II.2
      redblacktree_node_t * pparent = PARENT(parent) ;
      if (!pparent) return ; // done, cause whole tree is reduced one in depth
      // propagate height reduction up
      isLeft = (pparent->left == parent) ;
      parent = pparent ;
   }
}

int insert_redblacktree(redblacktree_t * tree, const void * new_key, redblacktree_node_t * new_node)
{
   int err ;

   VALIDATE_INPARAM_TEST(EVENADDRESS(new_node), ONABORT, ) ;

   if (!tree->root) {

      // first node
      tree->root = new_node ;

      new_node->left  = 0 ;
      new_node->right = 0 ;
      SETPARENTBLACK(new_node, 0) ;  /* setup new node */

   } else {

      // search insert position in tree; result == (pInsertPos,isKeyLower)

      redblacktree_node_t * parent ;
      for (parent = tree->root ;;) {
         int cmp = KEYCOMPARE(new_key, parent) ;
         if (cmp == 0) {
            return EEXIST ;
         }
         if (cmp < 0) {
            if (parent->left) {
               parent = parent->left ;
               continue ;
            }
            parent->left = new_node ;
            break ;
         } else {
            if (parent->right) {
               parent = parent->right ;
               continue ;
            }
            parent->right = new_node ;
            break ;
         }
      }

      new_node->left  = 0 ;
      new_node->right = 0 ;
      SETPARENTRED(new_node, parent) ; /* setup new node */

      if (ISRED(parent)) {
         rebalanceAfterInsert(tree, new_node) ;
      }

   }

   return  0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int remove_redblacktree(redblacktree_t * tree, const void * key, /*out*/redblacktree_node_t ** removed_node)
{
   int err ;
   redblacktree_node_t * node ;

   err = find_redblacktree(tree, key, &node) ;
   if (err) return err ;

   redblacktree_node_t * node_parent ;
   redblacktree_node_t * node_child ;
   bool  isNodeBlack ;
   bool  isNodeLeft ;

   if (!node->left) {
      /* node needs not to be replaced by successor */
      node_parent = PARENT(node) ;
      node_child  = node->right ;  // possibly == NULL
      isNodeBlack = ISBLACK(node) ;
      isNodeLeft  = node_parent ? (node_parent->left == node) : false ;
   } else if (!node->right) {
      /* node needs not to be replaced by successor. */
      node_parent = PARENT(node) ;
      node_child  = node->left ;   // always != NULL
      isNodeBlack = ISBLACK(node) ;
      isNodeLeft  = node_parent ? (node_parent->left == node) : false ;
   } else {
      /* find successor which has a NULL left child. */
      redblacktree_node_t * replace_node = node->right ;
      while (replace_node->left) {
         replace_node = replace_node->left ;
      }
      /* move replace_node into position of node */
      node_parent = PARENT(node) ;
      if (node_parent) {
         if (node_parent->left == node)
            node_parent->left = replace_node ;
         else
            node_parent->right = replace_node ;
      } else {
         tree->root = replace_node ;
      }
      node_parent = PARENT(replace_node) ;
      node_child  = replace_node->right ;  // possibly == NULL
      isNodeBlack = ISBLACK(replace_node) ;
      isNodeLeft  = (node_parent->left == replace_node) ;
      replace_node->parent = node->parent ; // copy also COLOR
      replace_node->left   = node->left ;
      if (replace_node->left) SETPARENT(replace_node->left, replace_node) ;
      if (node_parent == node) {
         // replace_node is right child of node
         node_parent = replace_node ;
      } else {
         replace_node->right = node->right ;
         if (replace_node->right) SETPARENT(replace_node->right, replace_node) ;
      }
   }

   /* remove node (or successor) from tree */
   if (node_parent) {
      if (isNodeLeft)
         node_parent->left = node_child ;
      else
         node_parent->right = node_child ;

      if (isNodeBlack) {
         // changed black height
         if (node_child) {
            assert(ISRED(node_child)) ; // else BLACK node depth would be violated
            SETPARENTBLACK(node_child, node_parent) ; // restored
         } else {
            rebalanceAfterRemove(tree, isNodeLeft, node_parent) ;
         }
      } else {
         // a RED node can only have two BLACK childs, but the removed/replaced node has only 0 or 1 child
         assert(node_child == 0) ;
      }

   } else {
      tree->root = node_child ;
      if (node_child) SETPARENTBLACK(node_child, 0) ;
   }

   MEMSET0(node) ;
   *removed_node = node ;
   return 0 ;
}

int removenodes_redblacktree(redblacktree_t * tree)
{
   int err ;
   redblacktree_node_t * node = tree->root ;

   tree->root = 0 ;

   if (node) {

      const bool isDeleteObject = islifetimedelete_typeadapt(tree->nodeadp.typeadp) ;

      err = 0 ;

      do {
         while (node->left) {
            redblacktree_node_t * leftnode = node->left ;
            node->left = 0 ;
            node = leftnode ;
         }
         redblacktree_node_t * delnode = node ;
         if (delnode->right) {
            node = delnode->right ;
            delnode->right = 0 ;
            SETPARENTRED(node, PARENT(delnode)) ;
         } else {
            node = PARENT(delnode) ;
         }
         delnode->parent = 0 ;

         if (isDeleteObject) {
            typeadapt_object_t * object = memberasobject_typeadaptmember(&tree->nodeadp, delnode) ;
            int err2 = calldelete_typeadaptmember(&tree->nodeadp, &object) ;
            if (err2) err = err2 ;
         }

      } while (node) ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

// group: iterate

int initfirst_redblacktreeiterator(/*out*/redblacktree_iterator_t * iter, redblacktree_t * tree)
{
   redblacktree_node_t * node = tree->root ;

   if (node) {
      while (node->left) {
         node = node->left ;
      }
   }

   iter->next = node ;
   return 0 ;
}

int initlast_redblacktreeiterator(/*out*/redblacktree_iterator_t * iter, redblacktree_t * tree)
{
   redblacktree_node_t * node = tree->root ;

   if (node) {
      while (node->right) {
         node = node->right ;
      }
   }

   iter->next = node ;
   return 0 ;
}

bool next_redblacktreeiterator(redblacktree_iterator_t * iter, redblacktree_t * tree, /*out*/redblacktree_node_t ** node)
{
   (void) tree ;

   if (!iter->next) {
      return false ;
   }

   *node = iter->next ;

   // search next higher
   redblacktree_node_t * next = iter->next ;

   if (next->right) {
      next = next->right ;
      while (next->left) {
         next = next->left ;
      }
   } else {
      redblacktree_node_t * child ;
      do {
         child = next ;
         next  = PARENT(next) ;
      } while (next && next->right == child/*child is bigger than parent*/) ;
   }

   iter->next = next ;

   return true ;
}

bool prev_redblacktreeiterator(redblacktree_iterator_t * iter, redblacktree_t * tree, /*out*/redblacktree_node_t ** node)
{
   (void) tree ;

   if (!iter->next) {
      return false ;
   }

   *node = iter->next ;

   // search next lower
   redblacktree_node_t * next = iter->next ;

   if (next->left) {
      next = next->left ;
      while (next->right) {
         next = next->right ;
      }
   } else {
      redblacktree_node_t * child ;
      do {
         child = next ;
         next  = PARENT(next) ;
      } while (next && next->left == child/*child is lower than parent*/) ;
   }

   iter->next = next ;

   return true ;
}


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

typedef struct testadapt_t             testadapt_t ;
typedef struct testnode_t              testnode_t ;

struct testnode_t {
   unsigned             key ;
   redblacktree_node_t  node ;
   int                  is_freed ;
   int                  is_inserted ;
} ;

struct testadapt_t {
   struct {
      typeadapt_EMBED(testadapt_t, testnode_t, uintptr_t) ;
   } ;
   test_errortimer_t    errcounter ;
   unsigned             freenode_count ;
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

static int impl_cmpkeyobj_testadapt(testadapt_t * testadp, const uintptr_t lkey, const testnode_t * rnode)
{
   (void) testadp ;
   unsigned rkey = rnode->key ;
   return lkey < rkey ? -1 : (lkey > rkey ? +1 : 0) ;
}

static int impl_cmpobj_testadapt(testadapt_t * testadp, const testnode_t * lnode, const testnode_t * rnode)
{
   (void) testadp ;
   unsigned lkey = lnode->key ;
   unsigned rkey = rnode->key ;
   return lkey < rkey ? -1 : (lkey > rkey ? +1 : 0) ;
}

static redblacktree_node_t * build_perfect_tree(unsigned count, testnode_t * nodes)
{
   assert(count < 10000) ;
   assert(0 == ((count + 1) & count)) ;   // count == (2^power)-1
   unsigned root = (count + 1) / 2 ;
   if (root == 1) {
      nodes[root].node.left = nodes[root].node.right = 0 ;
   } else {
      redblacktree_node_t * left  = build_perfect_tree(root - 1, nodes) ;
      redblacktree_node_t * right = build_perfect_tree(root - 1, nodes+root) ;
      nodes[root].node.left  = left ;
      nodes[root].node.right = right ;
      left->parent  = (redblacktree_node_t*)((uintptr_t)1 | (uintptr_t)&nodes[root].node) ;
      right->parent = (redblacktree_node_t*)((uintptr_t)1 | (uintptr_t)&nodes[root].node) ;
   }
   nodes[root].node.parent = (redblacktree_node_t*) (uintptr_t)1 ;
   return &nodes[root].node;
}

static int test_initfree(void)
{
   testnode_t           nodes[100] ;
   testadapt_t          typeadapt = {
                           typeadapt_INIT_LIFEKEYCMP(0, &impl_deletenode_testadapt, &impl_cmpkeyobj_testadapt, &impl_cmpobj_testadapt),
                           test_errortimer_INIT_FREEABLE, 0
                        } ;
   typeadapt_member_t   nodeadapt = typeadapt_member_INIT(asgeneric_typeadapt(&typeadapt, testadapt_t, testnode_t, uintptr_t), offsetof(testnode_t, node)) ;
   redblacktree_t       tree      = redblacktree_INIT_FREEABLE ;
   lrptree_node_t       emptynode = lrptree_node_INIT ;

   // prepare
   MEMSET0(&nodes) ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      nodes[i].key = i ;
   }

   // TEST lrptree_node_INIT
   TEST(0 == emptynode.left) ;
   TEST(0 == emptynode.right) ;
   TEST(0 == emptynode.parent) ;

   // TEST redblacktree_INIT_FREEABLE
   typeadapt_member_t emptyadapt = typeadapt_member_INIT_FREEABLE ;
   TEST(0 == tree.root) ;
   TEST(isequal_typeadaptmember(&emptyadapt, &tree.nodeadp)) ;

   // TEST redblacktree_INIT
   tree = (redblacktree_t) redblacktree_INIT((void*)1, nodeadapt) ;
   TEST((void*)1 == tree.root) ;
   TEST(isequal_typeadaptmember(&nodeadapt, &tree.nodeadp)) ;
   tree = (redblacktree_t) redblacktree_INIT((void*)0, emptyadapt) ;
   TEST(0 == tree.root) ;
   TEST(isequal_typeadaptmember(&emptyadapt, &tree.nodeadp)) ;

   // TEST init_redblacktree, double free_redblacktree
   tree.root = &nodes[0].node ;
   init_redblacktree(&tree, &nodeadapt) ;
   TEST(0 == tree.root) ;
   TEST(isequal_typeadaptmember(&nodeadapt, &tree.nodeadp)) ;
   TEST(0 == free_redblacktree(&tree)) ;
   TEST(0 == tree.root) ;
   TEST(isequal_typeadaptmember(&emptyadapt, &tree.nodeadp)) ;
   TEST(0 == free_redblacktree(&tree)) ;
   TEST(0 == tree.root) ;
   TEST(isequal_typeadaptmember(&emptyadapt, &tree.nodeadp)) ;

   // TEST free_redblacktree
   init_redblacktree(&tree, &nodeadapt) ;
   typeadapt.freenode_count = 0 ;
   tree.root = build_perfect_tree(7, nodes) ;
   TEST(tree.root == &nodes[4].node) ;
   TEST(0 == invariant_redblacktree(&tree)) ;
   TEST(0 == free_redblacktree(&tree)) ;
   TEST(7 == typeadapt.freenode_count) ;
   TEST(0 == tree.root) ;
   TEST(1 == isequal_typeadaptmember(&emptyadapt, &tree.nodeadp)) ;
   for (int i = 1; i <= 7; ++i) {
      TEST(0 == nodes[i].node.left) ;
      TEST(0 == nodes[i].node.right) ;
      TEST(0 == nodes[i].node.parent) ;
      TEST(1 == nodes[i].is_freed) ;
      nodes[i].is_freed = 0 ;
   }

   // TEST free_redblacktree: lifetime.delete_object set to 0
   init_redblacktree(&tree, &nodeadapt) ;
   typeadapt.freenode_count = 0 ;
   tree.root = build_perfect_tree(15, nodes) ;
   TEST(tree.root == &nodes[8].node) ;
   TEST(0 == invariant_redblacktree(&tree)) ;
   typeadapt.lifetime.delete_object = 0 ;
   TEST(0 == free_redblacktree(&tree)) ;
   typeadapt.lifetime.delete_object = &impl_deletenode_testadapt ;
   TEST(0 == typeadapt.freenode_count) ;
   TEST(0 == tree.root) ;
   TEST(1 == isequal_typeadaptmember(&emptyadapt, &tree.nodeadp)) ;
   for (int i = 1; i <= 15; ++i) {
      TEST(0 == nodes[i].node.left) ;
      TEST(0 == nodes[i].node.right) ;
      TEST(0 == nodes[i].node.parent) ;
      TEST(0 == nodes[i].is_freed) ;
   }

   // TEST free_redblacktree: ERROR
   init_redblacktree(&tree, &nodeadapt) ;
   typeadapt.freenode_count = 0 ;
   tree.root = build_perfect_tree(31, nodes) ;
   TEST(tree.root == &nodes[16].node) ;
   TEST(0 == invariant_redblacktree(&tree)) ;
   init_testerrortimer(&typeadapt.errcounter, 16, ENOEXEC) ;
   TEST(ENOEXEC == free_redblacktree(&tree)) ;
   TEST(30 == typeadapt.freenode_count) ;
   TEST(0 == tree.root) ;
   TEST(1 == isequal_typeadaptmember(&emptyadapt, &tree.nodeadp)) ;
   for (int i = 1; i <= 31; ++i) {
      TEST(0 == nodes[i].node.left) ;
      TEST(0 == nodes[i].node.right) ;
      TEST(0 == nodes[i].node.parent) ;
      TEST((i != 16) == nodes[i].is_freed) ;
      nodes[i].is_freed = 0 ;
   }

   // TEST getinistate_redblacktree
   redblacktree_node_t * saved_root   = (void*) 1 ;
   typeadapt_member_t saved_nodeadapt = nodeadapt ;
   typeadapt_member_t empty_nodeadapt = typeadapt_member_INIT_FREEABLE ;
   tree = (redblacktree_t)redblacktree_INIT_FREEABLE ;
   getinistate_redblacktree(&tree, &saved_root, 0) ;
   TEST(0 == saved_root) ;
   getinistate_redblacktree(&tree, &saved_root, &saved_nodeadapt) ;
   TEST(0 == saved_root) ;
   TEST(1 == isequal_typeadaptmember(&saved_nodeadapt, &empty_nodeadapt)) ;
   tree = (redblacktree_t)redblacktree_INIT(&nodes[0].node, nodeadapt) ;
   getinistate_redblacktree(&tree, &saved_root, 0) ;
   TEST(&nodes[0].node == saved_root) ;
   getinistate_redblacktree(&tree, &saved_root, &saved_nodeadapt) ;
   TEST(&nodes[0].node == saved_root) ;
   TEST(1 == isequal_typeadaptmember(&saved_nodeadapt, &nodeadapt)) ;

   // TEST isempty_redblacktree
   tree.root = (void*)1 ;
   TEST(0 == isempty_redblacktree(&tree)) ;
   tree.root = (void*)0 ;
   TEST(1 == isempty_redblacktree(&tree)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_insertconditions(void)
{
   testnode_t           nodes[20] ;
   testadapt_t          typeadapt = {
                           typeadapt_INIT_LIFEKEYCMP(0, &impl_deletenode_testadapt, &impl_cmpkeyobj_testadapt, &impl_cmpobj_testadapt),
                           test_errortimer_INIT_FREEABLE, 0
                        } ;
   typeadapt_member_t   nodeadapt = typeadapt_member_INIT(asgeneric_typeadapt(&typeadapt, testadapt_t, testnode_t, uintptr_t), offsetof(testnode_t, node)) ;
   redblacktree_t       tree      = redblacktree_INIT(0, nodeadapt) ;

   // prepare
   MEMSET0(&nodes) ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      nodes[i].key = i ;
   }

   // TEST: root == NULL (insert clears left/right/parent fields)
   for (int i = 0; i <= 2; ++i) {
      nodes[i].node.parent = &nodes[10].node ;
      nodes[i].node.left   = &nodes[10].node ;
      nodes[i].node.right  = &nodes[10].node ;
   }
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[1].key, &nodes[1].node)) ;
   TEST(0 == invariant_redblacktree(&tree)) ;
   TEST(tree.root         == &nodes[1].node) ;
   TEST(tree.root->left   == 0) ;
   TEST(tree.root->right  == 0) ;
   TEST(tree.root->parent == (redblacktree_node_t*)0x01/*color black*/) ;
   // TEST: parent (BLACK) (insert clears left/right fields)
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[0].key, &nodes[0].node)) ;
   TEST(0 == invariant_redblacktree(&tree)) ;
   TEST(tree.root         == &nodes[1].node) ;
   TEST(tree.root->left   == &nodes[0].node) ;
   TEST(tree.root->right  == 0) ;
   TEST(tree.root->parent == (redblacktree_node_t*)0x01/*color black*/) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[2].key, &nodes[2].node)) ;
   TEST(0 == invariant_redblacktree(&tree)) ;
   TEST(tree.root         == &nodes[1].node) ;
   TEST(tree.root->left   == &nodes[0].node) ;
   TEST(tree.root->right  == &nodes[2].node) ;
   TEST(tree.root->parent == (redblacktree_node_t*)0x01/*color black*/) ;
   TEST(nodes[0].node.left   == 0) ;
   TEST(nodes[0].node.right  == 0) ;
   TEST(nodes[0].node.parent == &nodes[1].node/*color is RED*/) ;
   TEST(nodes[2].node.left   == 0) ;
   TEST(nodes[2].node.right  == 0) ;
   TEST(nodes[2].node.parent == &nodes[1].node/*color is RED*/) ;
   TEST(0 == removenodes_redblacktree(&tree)) ;

   // TEST: parent (RED) uncle(RED)
   for (int i = 2; i <= 4; ++i) {
      nodes[i].node.parent = &nodes[10].node ;
      nodes[i].node.left   = &nodes[10].node ;
      nodes[i].node.right  = &nodes[10].node ;
   }
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[3].key, &nodes[3].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[4].key, &nodes[4].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[2].key, &nodes[2].node)) ;
   TEST(tree.root == &nodes[3].node) ;
   TEST(0         == PARENT(&nodes[3].node)) ;
   TEST(nodes[3].node.left  == &nodes[2].node) ;
   TEST(nodes[3].node.right == &nodes[4].node) ;
   TEST(PARENT(&nodes[2].node) == &nodes[3].node) ;
   TEST(ISRED(&nodes[2].node)) ;
   TEST(nodes[2].node.left  == 0) ;
   TEST(nodes[2].node.right == 0) ;
   TEST(PARENT(&nodes[4].node) == &nodes[3].node) ;
   TEST(ISRED(&nodes[4].node)) ;
   TEST(nodes[4].node.left  == 0) ;
   TEST(nodes[4].node.right == 0) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[1].key, &nodes[1].node)) ;
   TEST(0 == invariant_redblacktree(&tree)) ;
   TEST(PARENT(&nodes[3].node) == 0) ;
   TEST(tree.root == &nodes[3].node) ;
   TEST(nodes[3].node.left  == &nodes[2].node) ;
   TEST(nodes[3].node.right == &nodes[4].node) ;
   TEST(PARENT(&nodes[2].node) == &nodes[3].node) ;
   TEST(ISBLACK(&nodes[2].node)) ;
   TEST(nodes[2].node.left  == &nodes[1].node) ;
   TEST(nodes[2].node.right == 0) ;
   TEST(ISRED(&nodes[1].node)) ;
   TEST(PARENT(&nodes[4].node) == &nodes[3].node) ;
   TEST(ISBLACK(&nodes[4].node)) ;
   TEST(nodes[4].node.left  == 0) ;
   TEST(nodes[4].node.right == 0) ;
   TEST(0 == removenodes_redblacktree(&tree)) ;
   // DUAL
   for (int i = 2; i <= 5; ++i) {
      nodes[i].node.parent = &nodes[10].node ;
      nodes[i].node.left   = &nodes[10].node ;
      nodes[i].node.right  = &nodes[10].node ;
   }
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[3].key, &nodes[3].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[5].key, &nodes[5].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[2].key, &nodes[2].node)) ;
   TEST(tree.root == &nodes[3].node) ;
   TEST(0 == invariant_redblacktree(&tree)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[4].key, &nodes[4].node)) ;
   TEST(0 == invariant_redblacktree(&tree)) ;
   TEST(0         == PARENT(&nodes[3].node)) ;
   TEST(tree.root == &nodes[3].node) ;
   TEST(nodes[3].node.left  == &nodes[2].node) ;
   TEST(nodes[3].node.right == &nodes[5].node) ;
   TEST(PARENT(&nodes[2].node) == &nodes[3].node) ;
   TEST(ISBLACK(&nodes[2].node)) ;
   TEST(nodes[2].node.left  == 0) ;
   TEST(nodes[2].node.right == 0) ;
   TEST(PARENT(&nodes[5].node) == &nodes[3].node) ;
   TEST(ISBLACK(&nodes[5].node)) ;
   TEST(nodes[5].node.left  == &nodes[4].node) ;
   TEST(nodes[5].node.right == 0) ;
   TEST(ISRED(&nodes[4].node)) ;
   TEST(0 == removenodes_redblacktree(&tree)) ;

   // TEST: parent (RED)  uncle(NULL)
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[3].key, &nodes[3].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[1].key, &nodes[1].node)) ;
   TEST(tree.root == &nodes[3].node) ;
   TEST(0         == PARENT(&nodes[3].node)) ;
   TEST(nodes[3].node.left  == &nodes[1].node) ;
   TEST(nodes[3].node.right == 0) ;
   TEST(PARENT(&nodes[1].node) == &nodes[3].node) ;
   TEST(ISRED(&nodes[1].node)) ;
   TEST(nodes[1].node.left  == 0) ;
   TEST(nodes[1].node.right == 0) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[2].key, &nodes[2].node)) ;
   TEST(0 == invariant_redblacktree(&tree)) ;
   TEST(PARENT(&nodes[2].node) == 0) ;
   TEST(tree.root == &nodes[2].node) ;
   TEST(nodes[2].node.left  == &nodes[1].node) ;
   TEST(nodes[2].node.right == &nodes[3].node) ;
   TEST(PARENT(&nodes[3].node) == &nodes[2].node) ;
   TEST(ISRED(&nodes[3].node)) ;
   TEST(nodes[3].node.left  == 0) ;
   TEST(nodes[3].node.right == 0) ;
   TEST(PARENT(&nodes[1].node) == &nodes[2].node) ;
   TEST(ISRED(&nodes[1].node)) ;
   TEST(nodes[1].node.left  == 0) ;
   TEST(nodes[1].node.right == 0) ;
   TEST(0 == removenodes_redblacktree(&tree)) ;
   // DUAL
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[3].key, &nodes[3].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[4].key, &nodes[4].node)) ;
   TEST(tree.root == &nodes[3].node) ;
   TEST(0 == invariant_redblacktree(&tree)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[5].key, &nodes[5].node)) ;
   TEST(0 == invariant_redblacktree(&tree)) ;
   TEST(0 == PARENT(&nodes[4].node)) ;
   TEST(tree.root == &nodes[4].node) ;
   TEST(nodes[4].node.left  == &nodes[3].node) ;
   TEST(nodes[4].node.right == &nodes[5].node) ;
   TEST(PARENT(&nodes[3].node) == &nodes[4].node) ;
   TEST(ISRED(&nodes[3].node)) ;
   TEST(nodes[3].node.left  == 0) ;
   TEST(nodes[3].node.right == 0) ;
   TEST(PARENT(&nodes[5].node) == &nodes[4].node) ;
   TEST(ISRED(&nodes[5].node)) ;
   TEST(nodes[5].node.left  == 0) ;
   TEST(nodes[5].node.right == 0) ;
   TEST(0 == removenodes_redblacktree(&tree)) ;

   // TEST: parent (RED)  uncle(BLACK) / propagates
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[7].key, &nodes[7].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[5].key, &nodes[5].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[9].key, &nodes[9].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[3].key, &nodes[3].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[6].key, &nodes[6].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[8].key, &nodes[8].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[10].key, &nodes[10].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[2].key, &nodes[2].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[4].key, &nodes[4].node)) ;
   TEST(0 == invariant_redblacktree(&tree)) ;
   /*       7
    *     5     9
    *   3   6   8  10
    *  2 4             */
   TEST(tree.root == &nodes[7].node) ;
   TEST(nodes[7].node.left  == &nodes[5].node) ;
   TEST(nodes[7].node.right == &nodes[9].node) ;
   TEST(ISRED(&nodes[5].node)) ;
   TEST(nodes[5].node.left  == &nodes[3].node) ;
   TEST(nodes[5].node.right == &nodes[6].node) ;
   TEST(ISBLACK(&nodes[9].node)) ;
   TEST(nodes[9].node.left  == &nodes[8].node) ;
   TEST(nodes[9].node.right == &nodes[10].node) ;
   TEST(ISBLACK(&nodes[3].node)) ;
   TEST(ISBLACK(&nodes[6].node)) ;
   TEST(ISRED(&nodes[8].node)) ;
   TEST(ISRED(&nodes[10].node)) ;
   TEST(ISRED(&nodes[2].node)) ;
   TEST(ISRED(&nodes[4].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[1].key, &nodes[1].node)) ;
   TEST(0 == invariant_redblacktree(&tree)) ;
   TEST(tree.root == &nodes[5].node) ;
   TEST(nodes[5].node.left  == &nodes[3].node) ;
   TEST(nodes[5].node.right == &nodes[7].node) ;
   TEST(nodes[7].node.left  == &nodes[6].node) ;
   TEST(nodes[7].node.right == &nodes[9].node) ;
   TEST(nodes[3].node.left  == &nodes[2].node) ;
   TEST(nodes[3].node.right == &nodes[4].node) ;
   TEST(nodes[2].node.left  == &nodes[1].node) ;
   TEST(nodes[2].node.right == 0) ;
   TEST(ISBLACK(&nodes[9].node)) ;
   TEST(ISRED(&nodes[8].node)) ;
   TEST(ISRED(&nodes[10].node)) ;
   TEST(ISRED(&nodes[7].node)) ;
   TEST(ISRED(&nodes[3].node)) ;
   TEST(ISBLACK(&nodes[2].node)) ;
   TEST(ISBLACK(&nodes[4].node)) ;
   TEST(ISRED(&nodes[1].node)) ;
   TEST(0 == removenodes_redblacktree(&tree)) ;
   // DUAL
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[4].key, &nodes[4].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[2].key, &nodes[2].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[9].key, &nodes[9].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[1].key, &nodes[1].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[3].key, &nodes[3].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[7].key, &nodes[7].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[10].key, &nodes[10].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[5].key, &nodes[5].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[8].key, &nodes[8].node)) ;
   TEST(0 == invariant_redblacktree(&tree)) ;
   /*        4
    *     2       9
    *   1   3   7   10
    *          5 8     */
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[6].key, &nodes[6].node)) ;
   TEST(0 == invariant_redblacktree(&tree)) ;
   TEST(tree.root == &nodes[7].node) ;
   TEST(nodes[7].node.left == &nodes[4].node) ;
   TEST(nodes[7].node.right== &nodes[9].node) ;
   TEST(nodes[9].node.left == &nodes[8].node) ;
   TEST(nodes[9].node.right== &nodes[10].node) ;
   TEST(nodes[4].node.left == &nodes[2].node) ;
   TEST(nodes[4].node.right== &nodes[5].node) ;
   TEST(ISRED(&nodes[9].node)) ;
   TEST(ISBLACK(&nodes[10].node)) ;
   TEST(ISBLACK(&nodes[7].node)) ;
   TEST(ISBLACK(&nodes[8].node)) ;
   TEST(ISBLACK(&nodes[5].node)) ;
   TEST(ISRED(&nodes[4].node)) ;
   TEST(ISBLACK(&nodes[2].node)) ;
   TEST(ISRED(&nodes[1].node)) ;
   TEST(ISRED(&nodes[3].node)) ;
   TEST(0 == removenodes_redblacktree(&tree)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_removeconditions(void)
{
   testnode_t           nodes[20] ;
   testadapt_t          typeadapt = {
                           typeadapt_INIT_LIFEKEYCMP(0, &impl_deletenode_testadapt, &impl_cmpkeyobj_testadapt, &impl_cmpobj_testadapt),
                           test_errortimer_INIT_FREEABLE, 0
                        } ;
   typeadapt_member_t   nodeadapt = typeadapt_member_INIT(asgeneric_typeadapt(&typeadapt, testadapt_t, testnode_t, uintptr_t), offsetof(testnode_t, node)) ;
   redblacktree_t       tree      = redblacktree_INIT(0, nodeadapt) ;
   redblacktree_node_t  * node    = 0 ;

   // prepare
   MEMSET0(&nodes) ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      nodes[i].key = i ;
   }

   // TEST: remove successor (is directly right of node + RED child of black node)
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[7].key, &nodes[7].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[4].key, &nodes[4].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[9].key, &nodes[9].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[3].key, &nodes[3].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[5].key, &nodes[5].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[6].key, &nodes[6].node)) ;
   TEST(0 == invariant_redblacktree(&tree)) ;
   /*        7
    *     4      9
    *   3   5
    *         6      */
   TEST(tree.root           == &nodes[7].node) ;
   TEST(nodes[7].node.left  == &nodes[4].node) ;
   TEST(nodes[7].node.right == &nodes[9].node) ;
   TEST(nodes[4].node.left  == &nodes[3].node) ;
   TEST(nodes[4].node.right == &nodes[5].node) ;
   TEST(nodes[5].node.left  == 0) ;
   TEST(nodes[5].node.right == &nodes[6].node) ;
   TEST(ISBLACK(&nodes[9].node)) ;
   TEST(ISRED(&nodes[6].node)) ;
   TEST(ISRED(&nodes[4].node)) ;
   TEST(0 == remove_redblacktree(&tree, (void*)nodes[4].key, &node)) ;
   TEST(0 == invariant_redblacktree(&tree)) ;
   TEST(node == &nodes[4].node) ;
   TEST(tree.root           == &nodes[7].node) ;
   TEST(nodes[7].node.left  == &nodes[5].node) ;
   TEST(nodes[7].node.right == &nodes[9].node) ;
   TEST(nodes[5].node.left  == &nodes[3].node) ;
   TEST(nodes[5].node.right == &nodes[6].node) ;
   TEST(ISBLACK(&nodes[6].node)) ;
   TEST(0 == removenodes_redblacktree(&tree)) ;

   // TEST: remove successor (root)
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[7].key, &nodes[7].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[5].key, &nodes[5].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[9].key, &nodes[9].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[3].key, &nodes[3].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[6].key, &nodes[6].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[8].key, &nodes[8].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[10].key, &nodes[10].node)) ;
   TEST(0 == invariant_redblacktree(&tree)) ;
   /*        7
    *     5      9
    *   3   6  8  10  */
   TEST(tree.root           == &nodes[7].node) ;
   TEST(nodes[7].node.left  == &nodes[5].node) ;
   TEST(nodes[7].node.right == &nodes[9].node) ;
   TEST(nodes[9].node.left  == &nodes[8].node) ;
   TEST(nodes[9].node.right == &nodes[10].node) ;
   TEST(ISBLACK(&nodes[9].node)) ;
   TEST(ISRED(&nodes[8].node)) ;
   TEST(ISRED(&nodes[10].node)) ;
   TEST(0 == remove_redblacktree(&tree, (void*)nodes[7].key, &node)) ;
   TEST(0 == invariant_redblacktree(&tree)) ;
   TEST(node == &nodes[7].node) ;
   TEST(tree.root           == &nodes[8].node) ;
   TEST(nodes[8].node.left  == &nodes[5].node) ;
   TEST(nodes[8].node.right == &nodes[9].node) ;
   TEST(nodes[9].node.left  == 0 /*successor removed*/) ;
   TEST(nodes[9].node.right == &nodes[10].node) ;
   TEST(0 == removenodes_redblacktree(&tree)) ;

   // TEST: uncle of removed is RED
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[7].key, &nodes[7].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[5].key, &nodes[5].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[11].key, &nodes[11].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[3].key, &nodes[3].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[6].key, &nodes[6].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[9].key, &nodes[9].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[13].key, &nodes[13].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[8].key, &nodes[8].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[10].key, &nodes[10].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[12].key, &nodes[12].node)) ;
   TEST(0 == insert_redblacktree(&tree, (void*)nodes[14].key, &nodes[14].node)) ;
   SETBLACK(&nodes[5].node) ;
   SETBLACK(&nodes[3].node) ;
   SETBLACK(&nodes[6].node) ;
   SETRED(&nodes[11].node) ;
   SETBLACK(&nodes[8].node) ;
   SETBLACK(&nodes[9].node) ;
   SETBLACK(&nodes[10].node) ;
   SETBLACK(&nodes[12].node) ;
   SETBLACK(&nodes[13].node) ;
   SETBLACK(&nodes[14].node) ;
   TEST(0 == invariant_redblacktree(&tree)) ;
   /*        7
    *     5       11
    *   3   6  9      13
    *         8 10  12  14 */
   TEST(tree.root           == &nodes[7].node) ;
   TEST(nodes[7].node.left  == &nodes[5].node) ;
   TEST(nodes[7].node.right == &nodes[11].node) ;
   TEST(nodes[5].node.left  == &nodes[3].node) ;
   TEST(nodes[5].node.right == &nodes[6].node) ;
   TEST(nodes[11].node.left == &nodes[9].node) ;
   TEST(nodes[11].node.right== &nodes[13].node) ;
   TEST(nodes[9].node.left  == &nodes[8].node) ;
   TEST(nodes[9].node.right == &nodes[10].node) ;
   TEST(nodes[13].node.left == &nodes[12].node) ;
   TEST(nodes[13].node.right== &nodes[14].node) ;
   TEST(0 == remove_redblacktree(&tree, (void*)nodes[3].key, &node)) ;
   TEST(0 == invariant_redblacktree(&tree)) ;
   TEST(node == &nodes[3].node) ;
   TEST(tree.root            == &nodes[11].node) ;
   TEST(nodes[11].node.left  == &nodes[7].node) ;
   TEST(nodes[11].node.right == &nodes[13].node) ;
   TEST(nodes[7].node.left   == &nodes[5].node) ;
   TEST(nodes[7].node.right  == &nodes[9].node) ;
   TEST(nodes[5].node.left   == 0) ;
   TEST(nodes[5].node.right  == &nodes[6].node) ;
   TEST(nodes[9].node.left   == &nodes[8].node) ;
   TEST(nodes[9].node.right  == &nodes[10].node) ;
   TEST(ISRED(&nodes[9].node)) ;
   TEST(ISRED(&nodes[6].node)) ;
   TEST(ISBLACK(&nodes[12].node)) ;
   TEST(ISBLACK(&nodes[13].node)) ;
   TEST(ISBLACK(&nodes[14].node)) ;
   TEST(0 == removenodes_redblacktree(&tree)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_insertremove(void)
{
   typedef testnode_t   NODES[10000] ;
   NODES                * nodes ;
   memblock_t           memblock  = memblock_INIT_FREEABLE ;
   testadapt_t          typeadapt = {
                           typeadapt_INIT_LIFEKEYCMP(0, &impl_deletenode_testadapt, &impl_cmpkeyobj_testadapt, &impl_cmpobj_testadapt),
                           test_errortimer_INIT_FREEABLE, 0
                        } ;
   typeadapt_member_t   nodeadapt = typeadapt_member_INIT(asgeneric_typeadapt(&typeadapt, testadapt_t, testnode_t, uintptr_t), offsetof(testnode_t, node)) ;
   redblacktree_t       tree      = redblacktree_INIT(0, nodeadapt) ;
   redblacktree_node_t  * treenode ;

   // prepare
   TEST(0 == RESIZE_MM(sizeof(NODES), &memblock)) ;
   nodes = (NODES*)memblock.addr ;
   MEMSET0(nodes) ;
   for (unsigned i = 0; i < nrelementsof(*nodes); ++i) {
      (*nodes)[i].key = i ;
   }

   // TEST insert_redblacktree: EINVAL (uneven address)
   TEST(EINVAL == insert_redblacktree(&tree, (void*)0, (redblacktree_node_t*)(1+(uintptr_t)&(*nodes)[0].node))) ;
   TEST(0 == tree.root) ;

   // TEST find_redblacktree: empty tree
   treenode = (void*)1 ;
   TEST(ESRCH == find_redblacktree(&tree, (void*)(*nodes)[0].key, &treenode)) ;
   TEST(0 == tree.root) ;
   TEST(treenode == (void*)1) ;

   // TEST insert_redblacktree: single node
   (*nodes)[0].node.left   = (void*)1 ;
   (*nodes)[0].node.right  = (void*)1 ;
   (*nodes)[0].node.parent = (void*)1 ;
   TEST(0 == insert_redblacktree(&tree, (void*)(*nodes)[0].key, &(*nodes)[0].node)) ;
   TEST(tree.root == &(*nodes)[0].node) ;
   TEST(0 == (*nodes)[0].is_freed) ;
   TEST(0 == (*nodes)[0].node.left) ;
   TEST(0 == (*nodes)[0].node.right) ;
   TEST(0 == PARENT(&(*nodes)[0].node)) ;

   // TEST find_redblacktree: single node
   treenode = 0 ;
   TEST(0 == find_redblacktree(&tree, (void*)(*nodes)[0].key, &treenode)) ;
   TEST(treenode == &(*nodes)[0].node) ;

   // TEST remove_redblacktree: single node
   TEST(tree.root == &(*nodes)[0].node) ;
   treenode = 0 ;
   TEST(0 == remove_redblacktree(&tree, (void*)(*nodes)[0].key, &treenode)) ;
   TEST(0 == tree.root) ;
   TEST(0 == (*nodes)[0].is_freed) ;
   TEST(treenode == &(*nodes)[0].node) ;

   // TEST removenodes_redblacktree: single node
   TEST(0 == tree.root) ;
   TEST(0 == insert_redblacktree(&tree, (void*)(*nodes)[10].key, &(*nodes)[10].node)) ;
   TEST(tree.root == &(*nodes)[10].node) ;
   TEST(0 == removenodes_redblacktree(&tree)) ;
   TEST(1 == (*nodes)[10].is_freed) ;
   TEST(0 == tree.root) ;
   (*nodes)[10].is_freed = 0 ;

   // TEST insert_redblacktree: ascending order
   for (unsigned i = 0; i < nrelementsof(*nodes); ++i) {
      TEST(0 == insert_redblacktree(&tree, (void*)(*nodes)[i].key, &(*nodes)[i].node)) ;
      if (!(i%100)) TEST(0 == invariant_redblacktree(&tree)) ;
   }
   TEST(0 == invariant_redblacktree(&tree)) ;

   // TEST find_redblacktree: ascending order
   for (unsigned i = 0; i < nrelementsof(*nodes); ++i) {
      TEST(0 == find_redblacktree(&tree, (void*)(*nodes)[i].key, &treenode)) ;
      TEST(treenode == &(*nodes)[i].node) ;
   }
   TEST(0 == invariant_redblacktree(&tree)) ;

   // TEST remove_redblacktree: ascending order
   for (unsigned i = 0; i < nrelementsof(*nodes); ++i) {
      TEST(0 == remove_redblacktree(&tree, (void*)(*nodes)[i].key, &treenode)) ;
      TEST(0 == (*nodes)[i].is_freed) ;
      TEST(treenode == &(*nodes)[i].node) ;
      if (!(i%100)) TEST(0 == invariant_redblacktree(&tree)) ;
   }
   TEST(0 == tree.root) ;
   TEST(0 == invariant_redblacktree(&tree)) ;

   // TEST insert_redblacktree: descending order
   for (int i = (int)nrelementsof(*nodes)-1; i >= 0; --i) {
      TEST(0 == insert_redblacktree(&tree, (void*)(*nodes)[i].key, &(*nodes)[i].node)) ;
      if (!(i%100)) TEST(0 == invariant_redblacktree(&tree)) ;
   }
   TEST(0 == invariant_redblacktree(&tree)) ;

   // TEST find_redblacktree: descending order
   for (int i = (int)nrelementsof(*nodes)-1; i >= 0; --i) {
      TEST(0 == find_redblacktree(&tree, (void*)(*nodes)[i].key, &treenode)) ;
      TEST(treenode == &(*nodes)[i].node) ;
   }
   TEST(0 == invariant_redblacktree(&tree)) ;

   // TEST remove_redblacktree: descending order
   for (int i = (int)nrelementsof(*nodes)-1; i >= 0; --i) {
      TEST(0 == remove_redblacktree(&tree, (void*)(*nodes)[i].key, &treenode)) ;
      TEST(0 == (*nodes)[i].is_freed) ;
      TEST(treenode == &(*nodes)[i].node) ;
      if (!(i%100)) TEST(0 == invariant_redblacktree(&tree)) ;
   }
   TEST(0 == tree.root) ;
   TEST(0 == invariant_redblacktree(&tree)) ;

   // TEST insert_redblacktree,find_redblacktree,remove_redblacktree: random order
   srand(100) ;
   for (unsigned i = 0; i < 4*nrelementsof(*nodes); ++i) {
      int id = rand() % (int)nrelementsof(*nodes) ;
      if ((*nodes)[id].is_inserted) {
         TEST(0 == find_redblacktree(&tree, (void*)(*nodes)[id].key, &treenode)) ;
         TEST(treenode == &(*nodes)[id].node) ;
         (*nodes)[id].is_inserted = 0 ;
         treenode = 0 ;
         TEST(0 == remove_redblacktree(&tree, (void*)(*nodes)[id].key, &treenode)) ;
         TEST(treenode == &(*nodes)[id].node) ;
      } else {
         TEST(ESRCH == find_redblacktree(&tree, (void*)(*nodes)[id].key, &treenode)) ;
         (*nodes)[id].is_inserted = 1 ;
         TEST(0 == insert_redblacktree(&tree, (void*)(*nodes)[id].key, &(*nodes)[id].node)) ;
      }
   }

   // TEST find_redblacktree, removenodes_redblacktree: random order
   typeadapt.freenode_count = 0 ;
   for (unsigned i = 0; i < nrelementsof(*nodes); ++i) {
      if ((*nodes)[i].is_inserted) {
         TEST(0 == find_redblacktree(&tree, (void*)(*nodes)[i].key, &treenode)) ;
         TEST(treenode == &(*nodes)[i].node) ;
      } else {
         TEST(ESRCH == find_redblacktree(&tree, (void*)(*nodes)[i].key, &treenode)) ;
         ++ typeadapt.freenode_count ;
      }
      TEST(0 == (*nodes)[i].is_freed) ;
   }
   TEST(0 == removenodes_redblacktree(&tree)) ;
   TEST(nrelementsof(*nodes) == typeadapt.freenode_count) ;
   for (unsigned i = 0; i < nrelementsof(*nodes); ++i) {
      TEST(0 == (*nodes)[i].node.left) ;
      TEST(0 == (*nodes)[i].node.right) ;
      TEST(0 == (*nodes)[i].node.parent) ;
      TEST((*nodes)[i].is_inserted == (*nodes)[i].is_freed) ;
      (*nodes)[i].is_freed    = 0 ;
      (*nodes)[i].is_inserted = 0 ;
   }

   // TEST removenodes_redblacktree
   for (unsigned i = 0; i < nrelementsof(*nodes); ++i) {
      TEST(0 == insert_redblacktree(&tree, (void*)(*nodes)[i].key, &(*nodes)[i].node)) ;
   }
   typeadapt.freenode_count = 0 ;
   TEST(0 == removenodes_redblacktree(&tree)) ;
   TEST(nrelementsof(*nodes) == typeadapt.freenode_count) ;
   for (unsigned i = 0; i < nrelementsof(*nodes); ++i) {
      TEST(0 == (*nodes)[i].node.left) ;
      TEST(0 == (*nodes)[i].node.right) ;
      TEST(0 == (*nodes)[i].node.parent) ;
      TEST(1 == (*nodes)[i].is_freed) ;
      (*nodes)[i].is_freed = 0 ;
   }

   // TEST removenodes_redblacktree: lifetime.delete_object set to 0
   for (unsigned i = 0; i < nrelementsof(*nodes); ++i) {
      TEST(0 == insert_redblacktree(&tree, (void*)(*nodes)[i].key, &(*nodes)[i].node)) ;
   }
   typeadapt.freenode_count = 0 ;
   typeadapt.lifetime.delete_object = 0 ;
   TEST(0 == removenodes_redblacktree(&tree)) ;
   TEST(0 == typeadapt.freenode_count) ;
   for (unsigned i = 0; i < nrelementsof(*nodes); ++i) {
      TEST(0 == (*nodes)[i].node.left) ;
      TEST(0 == (*nodes)[i].node.right) ;
      TEST(0 == (*nodes)[i].node.parent) ;
      TEST(0 == (*nodes)[i].is_freed) ;
   }

   // unprepare
   TEST(0 == FREE_MM(&memblock)) ;
   nodes = 0 ;

   return 0 ;
ONABORT:
   (void) FREE_MM(&memblock) ;
   return EINVAL ;
}

static int test_iterator(void)
{
   testnode_t           nodes[100] ;
   testadapt_t          typeadapt = {
                           typeadapt_INIT_LIFEKEYCMP(0, &impl_deletenode_testadapt, &impl_cmpkeyobj_testadapt, &impl_cmpobj_testadapt),
                           test_errortimer_INIT_FREEABLE, 0
                        } ;
   typeadapt_member_t   nodeadapt = typeadapt_member_INIT(asgeneric_typeadapt(&typeadapt, testadapt_t, testnode_t, uintptr_t), offsetof(testnode_t, node)) ;
   redblacktree_t       tree      = redblacktree_INIT(0, nodeadapt) ;
   redblacktree_t       emptytree = redblacktree_INIT_FREEABLE ;
   redblacktree_iterator_t iter   = redblacktree_iterator_INIT_FREEABLE ;

   // prepare
   MEMSET0(&nodes) ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      nodes[i].key = i ;
   }
   init_redblacktree(&tree, &nodeadapt) ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      testnode_t * node = &nodes[(13*i) % nrelementsof(nodes)] ;
      TEST(0 == insert_redblacktree(&tree, (void*)(node->key), &node->node)) ;
   }
   TEST(0 == invariant_redblacktree(&tree)) ;

   // TEST redblacktree_iterator_INIT_FREEABLE
   TEST(0 == iter.next) ;

   // TEST initfirst_redblacktreeiterator: empty tree
   iter.next = (void*)1 ;
   TEST(0 == initfirst_redblacktreeiterator(&iter, &emptytree)) ;
   TEST(0 == iter.next) ;

   // TEST next_redblacktreeiterator: empty tree
   TEST(0 == initfirst_redblacktreeiterator(&iter, &emptytree)) ;
   TEST(false == next_redblacktreeiterator(&iter, 0/*not needed*/, 0/*not needed*/)) ;

   // TEST initlast_redblacktreeiterator: empty tree
   iter.next = (void*)1 ;
   TEST(0 == initlast_redblacktreeiterator(&iter, &emptytree)) ;
   TEST(0 == iter.next) ;

   // TEST prev_redblacktreeiterator: empty tree
   TEST(0 == initlast_redblacktreeiterator(&iter, &emptytree)) ;
   TEST(false == prev_redblacktreeiterator(&iter, 0/*not needed*/, 0/*not needed*/)) ;

   // TEST free_redblacktreeiterator
   iter.next = (void*)1 ;
   TEST(0 == free_redblacktreeiterator(&iter)) ;
   TEST(0 == iter.next) ;

   // TEST initfirst_redblacktreeiterator: full tree
   TEST(0 == initfirst_redblacktreeiterator(&iter, &tree)) ;
   TEST(&nodes[0].node == iter.next) ;

   // TEST next_redblacktreeiterator: full tree
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      redblacktree_node_t * nextnode = 0 ;
      TEST(next_redblacktreeiterator(&iter, 0/*not needed*/, &nextnode)) ;
      TEST(&nodes[i].node == nextnode);
   }
   TEST(0 == iter.next) ;
   TEST(! next_redblacktreeiterator(&iter, 0/*not needed*/, 0/*not needed*/)) ;

   // TEST initlast_redblacktreeiterator: full tree
   TEST(0 == initlast_redblacktreeiterator(&iter, &tree)) ;
   TEST(&nodes[nrelementsof(nodes)-1].node == iter.next) ;

   // TEST next_redblacktreeiterator: full tree
   for (unsigned i = nrelementsof(nodes); (i--) > 0;) {
      redblacktree_node_t * nextnode = 0 ;
      TEST(prev_redblacktreeiterator(&iter, 0/*not needed*/, &nextnode)) ;
      TEST(&nodes[i].node == nextnode);
   }
   TEST(0 == iter.next) ;
   TEST(! prev_redblacktreeiterator(&iter, 0/*not needed*/, 0/*not needed*/)) ;

   // TEST foreach
   unsigned i = 0 ;
   foreach (_redblacktree, &tree, node) {
      TEST(node == &nodes[i++].node) ;
   }
   TEST(i == nrelementsof(nodes)) ;

   // TEST foreachReverse
   foreachReverse (_redblacktree, &tree, node) {
      TEST(node == &nodes[--i].node) ;
   }
   TEST(i == 0) ;

   // TEST foreach: remove every second current element
   foreach (_redblacktree, &tree, node) {
      TEST(node == &nodes[i].node) ;
      if ((i % 2)) {
         redblacktree_node_t * removed_node = 0 ;
         TEST(0 == remove_redblacktree(&tree, (void*)i, &removed_node)) ;
         TEST(node == removed_node) ;
      }
      ++ i ;
   }
   TEST(i == nrelementsof(nodes)) ;

   // TEST foreachReverse: remove all elements
   i = nrelementsof(nodes) ;
   foreachReverse (_redblacktree, &tree, node) {
      -- i ;
      i -= (i % 2) ;
      TEST(node == &nodes[i].node) ;
      redblacktree_node_t * removed_node = 0 ;
      TEST(0 == remove_redblacktree(&tree, (void*)i, &removed_node)) ;
      TEST(node == removed_node) ;
   }
   TEST(i == 0) ;
   TEST(isempty_redblacktree(&tree)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

redblacktree_IMPLEMENT(_testtree, testnode_t, uintptr_t, node)

static int test_generic(void)
{
   testnode_t           nodes[100] ;
   testadapt_t          typeadapt = {
                           typeadapt_INIT_LIFEKEYCMP(0, &impl_deletenode_testadapt, &impl_cmpkeyobj_testadapt, &impl_cmpobj_testadapt),
                           test_errortimer_INIT_FREEABLE, 0
                        } ;
   typeadapt_member_t   emptynodeadapt = typeadapt_member_INIT_FREEABLE ;
   typeadapt_member_t   nodeadapt = typeadapt_member_INIT(asgeneric_typeadapt(&typeadapt, testadapt_t, testnode_t, uintptr_t), offsetof(testnode_t, node)) ;
   redblacktree_t       tree      = redblacktree_INIT(0, nodeadapt) ;

   // prepare
   MEMSET0(&nodes) ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      nodes[i].key = i ;
   }

   // TEST init_redblacktree, free_redblacktree
   tree.root = (void*)1 ;
   init_testtree(&tree, &nodeadapt) ;
   TEST(0 == tree.root) ;
   TEST(isequal_typeadaptmember(&tree.nodeadp, &nodeadapt)) ;
   init_testtree(&tree, &nodeadapt) ;
   TEST(0 == free_testtree(&tree)) ;
   TEST(isequal_typeadaptmember(&tree.nodeadp, &emptynodeadapt)) ;

   // TEST getinistate_redblacktree
   TEST(0 == free_testtree(&tree)) ;
   TEST(isempty_testtree(&tree)) ;
   testnode_t * root = (void*)1 ;
   typeadapt_member_t nodeadapt2 = nodeadapt ;
   getinistate_testtree(&tree, &root, &nodeadapt2) ;
   TEST(0 == root) ;
   TEST(isequal_typeadaptmember(&nodeadapt2, &emptynodeadapt)) ;
   init_testtree(&tree, &nodeadapt) ;
   tree.root = &nodes[10].node ;
   getinistate_testtree(&tree, &root, &nodeadapt2) ;
   TEST(&nodes[10] == root) ;
   TEST(isequal_typeadaptmember(&nodeadapt2, &nodeadapt)) ;

   // TEST isempty_redblacktree
   tree.root = (void*)1 ;
   TEST(0 == isempty_testtree(&tree)) ;
   tree.root = (void*)0 ;
   TEST(1 == isempty_testtree(&tree)) ;

   // TEST insert_redblacktree, find_redblacktree, remove_redblacktree, invariant_redblacktree
   init_testtree(&tree, &nodeadapt) ;
   TEST(0 == invariant_redblacktree(&tree)) ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == insert_testtree(&tree, nodes[i].key, &nodes[i])) ;
   }
   TEST(0 == invariant_redblacktree(&tree)) ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      testnode_t * found_node = 0 ;
      TEST(0 == find_testtree(&tree, nodes[i].key, &found_node)) ;
      TEST(found_node == &nodes[i]) ;
   }
   TEST(0 == invariant_redblacktree(&tree)) ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      testnode_t * removed_node = 0 ;
      TEST(0 == remove_testtree(&tree, nodes[i].key, &removed_node)) ;
      TEST(removed_node == &nodes[i]) ;
      TEST(ESRCH == find_testtree(&tree, nodes[i].key, &removed_node)) ;
      if (!(i%100)) TEST(0 == invariant_redblacktree(&tree)) ;
   }

   // TEST removenodes_redblacktree, free_redblacktree
   init_testtree(&tree, &nodeadapt) ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == insert_testtree(&tree, nodes[i].key, &nodes[i])) ;
   }
   TEST(0 == removenodes_testtree(&tree)) ;
   TEST(0 == isequal_typeadaptmember(&tree.nodeadp, &emptynodeadapt)) ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(1 == nodes[i].is_freed) ;
   }
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == insert_testtree(&tree, nodes[i].key, &nodes[i])) ;
   }
   TEST(0 == free_testtree(&tree)) ;
   TEST(1 == isequal_typeadaptmember(&tree.nodeadp, &emptynodeadapt)) ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(2 == nodes[i].is_freed) ;
   }

   // TEST foreach, foreachReverse
   init_testtree(&tree, &nodeadapt) ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == insert_testtree(&tree, nodes[i].key, &nodes[i])) ;
   }
   for (unsigned i = 0; 0 == i; i = 1) {
      foreach (_testtree, &tree, node) {
         TEST(node == &nodes[i++]) ;
      }
      TEST(i == nrelementsof(nodes)) ;

      foreachReverse (_testtree, &tree, node) {
         TEST(node == &nodes[--i]) ;
      }
      TEST(i == 0) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_ds_inmem_redblacktree()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())          goto ONABORT ;
   if (test_insertconditions())  goto ONABORT ;
   if (test_removeconditions())  goto ONABORT ;
   if (test_insertremove())      goto ONABORT ;
   if (test_iterator())          goto ONABORT ;
   if (test_generic())           goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
