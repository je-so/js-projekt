/* title: RedBlacktree-Index impl
   Implement interface of <redblacktree_t>.

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

   file: C-kern/api/platform/index/redblacktree.h
    Header file of <RedBlacktree-Index>.

   file: C-kern/platform/shared/index/redblacktree.c
    Implementation file of <RedBlacktree-Index impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/platform/index/redblacktree.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif

// group: internal macros

/* define: PARENT
 * Access parent pointer from node.
 * The reason to use this this macro is that the color of a node
 * is encoded in the lowest bit of the parent pointer. */
#define PARENT(node)       ((redblacktree_node_t*) (((uintptr_t)-2) & (uintptr_t)((node)->parent)))

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



int invariant_redblacktree( redblacktree_t * tree, const redblacktree_compare_nodes_t * compare_callback )
{
   if (     !tree
         || !compare_callback ) {
      goto ONABORT ;
   }

   redblacktree_node_t                 * prev     = 0 ;
   redblacktree_node_t                 * node     = tree->root ;
   const redblacktree_compare_nodes_f  compare    = compare_callback->fct ;
   callback_param_t                    * const cb = compare_callback->cb_param ;

   if (!node) {
      return 0 ;
   }

   if (     !ISBLACK(node)
         || 0 != PARENT(node) ) {
      goto ONABORT ;
   }

   // determine height of tree

   size_t height = 1 ;  // black height of root node
   while( node->left ) {
      if (PARENT(node->left) != node) goto ONABORT ;
      node = node->left ;
      if (ISBLACK(node)) {
         ++ height ;
      }
   }

   const size_t const_height = height ;

   do {

      if (node->left  && compare( cb, node->left,  node) >= 0) goto ONABORT ;
      if (node->right && compare( cb, node->right, node) <= 0) goto ONABORT ;

      if (ISRED(node)) {
         if (node->left  && ISRED(node->left))  goto ONABORT ;
         if (node->right && ISRED(node->right)) goto ONABORT ;
      }

      if (prev) {
         if (compare( cb, node, prev) <= 0) goto ONABORT ;
         if (compare( cb, prev, node) >= 0) goto ONABORT ;
      }

      prev = node ;

      if (     (!node->left || !node->right) // is leaf
            && const_height != height) {
         goto ONABORT ;
      }

      if (!node->right) {
         redblacktree_node_t * parent ;
         for(;;) {
            if (ISBLACK(node)) -- height ;
            parent = PARENT(node) ;
            if (     !parent
                  || parent->left == node ) {
               break ;
            }
            node = parent ;
         }
         node = parent ;
      } else {
         if (PARENT(node->right) != node) goto ONABORT ;
         node = node->right ;
         if (ISBLACK(node)) ++ height ;

         while( node->left ) {
            if (PARENT(node->left) != node) goto ONABORT ;
            node = node->left ;
            if (ISBLACK(node)) {
               ++ height ;
            }
         }
      }

   } while( node ) ;

   if (0 != height) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(EINVAL) ;
   return EINVAL ;
}

int free_redblacktree( redblacktree_t * tree, const redblacktree_free_t * free_callback )
{
   int err ;
   err = freenodes_redblacktree(tree, free_callback) ;
   return err ;
}

int init_redblacktree( /*out*/redblacktree_t * tree )
{
   tree->root = 0 ;
   return 0 ;
}

int find_redblacktree( redblacktree_t * tree, const void * key, /*out*/redblacktree_node_t ** found_node, const redblacktree_compare_t * compare_callback )
{
   redblacktree_node_t           * node = tree->root ;
   const redblacktree_compare_f compare = compare_callback->fct ;
   callback_param_t          * const cb = compare_callback->cb_param ;

   while (node) {
      int cmp = compare( cb, key, node) ;
      if (!cmp) {
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

static redblacktree_node_t * rotateLeft( redblacktree_t * tree, redblacktree_node_t * node )
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

static redblacktree_node_t * rotateRight( redblacktree_t * tree, redblacktree_node_t * node )
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

static void rebalanceAfterInsert( redblacktree_t * tree, redblacktree_node_t * const inserted_node )
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
   for(;;) {

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


static void rebalanceAfterRemove( redblacktree_t * tree, const bool isNodeLeft, redblacktree_node_t * const parent_node)
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
   for(;;)  {

      if (isLeft) {
         // node is left child
         redblacktree_node_t * right = parent->right ; // right != NULL
         if (ISRED(right)) {
            // case I.
            SETBLACK(right) ;
            SETRED(parent) ;
            rotateLeft( tree, parent ) ;
            right = parent->right ;   // ISBLACK(right) && right != NULL
         }

         if (  (!right->left || ISBLACK(right->left) )
               && (!right->right || ISBLACK(right->right) )) {
            // case II.
            SETRED(right) ;  // reduce height of right side

         } else {
            // case III.
            if ( !right->right || ISBLACK(right->right)) {
               // ISBLACK(right->right) => right->left != NULL  !+! ISRED(right->left)
               SETBLACK(right->left) ;
               // not necessary ! SETRED(right) ;
               right = rotateRight( tree, right) ; // right != NULL && ISBLACK(right)
            }
            // case IV.
            if (ISRED(parent)) {
               SETRED(right) ;
               SETBLACK(parent) ;
            }
            SETBLACK(right->right) ;
            rotateLeft( tree, parent ) ;
            return ; // DONE
         }
      } else {
         // node is right child
         redblacktree_node_t * left = parent->left ;  // left != NULL
         if (ISRED(left)) {
            // case I.
            SETBLACK(left) ;
            SETRED(parent) ;
            rotateRight( tree, parent ) ;
            left = parent->left ;   // ISBLACK(left) && left != NULL
         }

         if (  (!left->left || ISBLACK(left->left) )
               && (!left->right || ISBLACK(left->right) )) {
            // case II.
            SETRED(left) ;  // reduce height of left side

         } else {
            // case III.
            if ( !left->left || ISBLACK(left->left)) {
               // ISBLACK(left->left) => left->right != NULL  !+! ISRED(left->right)
               SETBLACK(left->right) ;
               // not necessary ! SETRED(left) ;
               left = rotateLeft( tree, left ) ; // left != NULL && ISBLACK(left)
            }
            // case IV.
            if (ISRED(parent)) {
               SETRED(left) ;
               SETBLACK(parent) ;
            }
            SETBLACK(left->left) ;
            rotateRight( tree, parent ) ;
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

int insert_redblacktree( redblacktree_t * tree, const void * new_key, redblacktree_node_t * new_node, const redblacktree_compare_t * compare_callback )
{
   int err ;

   if ( (((uintptr_t)1) & (uintptr_t)(new_node)) /*uneven address*/ ) {
      err = EINVAL ;
      goto ONABORT ;
   }

   if (!tree->root) {

      // first node
      tree->root = new_node ;

      new_node->left  = 0 ;
      new_node->right = 0 ;
      SETPARENTBLACK(new_node, 0) ;  /* setup new node */

   } else {

      // search insert position in tree; result == (pInsertPos,isKeyLower)
      const redblacktree_compare_f compare = compare_callback->fct ;
      callback_param_t         * const cb  = compare_callback->cb_param ;

      redblacktree_node_t * parent ;
      for(parent = tree->root ;;)
      {
         int cmp = compare(cb, new_key, parent) ;
         if (!cmp) {
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
         rebalanceAfterInsert( tree, new_node ) ;
      }

   }

   return  0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int remove_redblacktree( redblacktree_t * tree, const void * key, /*out*/redblacktree_node_t ** removed_node, const redblacktree_compare_t * compare_callback )
{
   int err ;
   redblacktree_node_t * node ;

   err = find_redblacktree( tree, key, &node, compare_callback ) ;
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
      while( replace_node->left ) {
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
            assert( ISRED(node_child) ) ; // else BLACK node depth would be violated
            SETPARENTBLACK(node_child, node_parent) ; // restored
         } else {
            rebalanceAfterRemove( tree, isNodeLeft, node_parent ) ;
         }
      } else {
         // a RED node can only have two BLACK childs, but the removed/replaced node has only 0 or 1 child
         assert(node_child == 0) ;
      }

   } else {
      tree->root = node_child ;
      if (node_child) SETPARENTBLACK(node_child, 0) ;
   }

   MEMSET0( node ) ;
   *removed_node = node ;
   return 0 ;
}

int updatekey_redblacktree( redblacktree_t * tree, const void * old_key, const void * new_key, const redblacktree_update_key_t * update_key, const redblacktree_compare_t * compare_callback )
{
   int err ;
   redblacktree_node_t * updated_node ;

   err = remove_redblacktree( tree, old_key, &updated_node, compare_callback ) ;
   if (err) return err ;

   err = update_key->fct(update_key->cb_param, new_key, updated_node) ;
   if (err) {
      int err2 = insert_redblacktree( tree, old_key, updated_node, compare_callback ) ;
      assert(!err2) ;
      TRACECALLERR_LOG("redblacktree_update_key_t callback", err) ;
      goto ONABORT ;
   }

   err = insert_redblacktree( tree, new_key, updated_node, compare_callback ) ;
   if (err) {
      int err2 ;
      err2 = update_key->fct(update_key->cb_param, old_key, updated_node) ;
      assert(!err2) ;
      err2 = insert_redblacktree( tree, old_key, updated_node, compare_callback ) ;
      assert(!err2) ;
      goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int freenodes_redblacktree( redblacktree_t * tree, const redblacktree_free_t * free_callback )
{
   int err ;
   redblacktree_node_t * parent = 0 ;
   redblacktree_node_t * node   = tree->root ;

   tree->root = 0 ;

   if (     node
         && free_callback ) {

      err = 0 ;

      for (;;) {
         while ( node->left ) {
            redblacktree_node_t * nodeleft = node->left ;
            node->left = parent ;
            parent = node ;
            node   = nodeleft ;
         }
         if (node->right) {
            redblacktree_node_t * noderight = node->right ;
            node->left = parent ;
            parent = node ;
            node   = noderight ;
         } else {
            assert( node->left == 0 && node->right == 0 ) ;
            node->parent = 0 ;
            int err2 = free_callback->fct( free_callback->cb_param, node ) ;
            if (err2) err = err2 ;
            if (!parent) break ;
            if (parent->right == node) {
               node   = parent ;
               parent = node->left ;
               node->left  = 0 ;
               node->right = 0 ;
            } else {
               node   = parent ;
               parent = node->left ;
               node->left = 0 ;
            }
         }
      }

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}


#ifdef KONFIG_UNITTEST

struct TreeNode {
   redblacktree_node_t  aspect ;
   unsigned key ;
   int      is_freed ;
   int      is_inserted ;
} ;

static int adapter_compare_nodes( callback_param_t * cb, const redblacktree_node_t * node1, const redblacktree_node_t * node2)
{
   static_assert( sizeof(void*) >= sizeof(unsigned ), "void* is not big enough to carry search key" ) ;
   assert(cb == (callback_param_t *)1) ;
   unsigned key1 = ((const struct TreeNode*)node1)->key ;
   unsigned key2 = ((const struct TreeNode*)node2)->key ;
   return key1 < key2 ? -1 : (key1 > key2 ? +1 : 0) ;
}

static int adapter_compare_key_node( callback_param_t * cb, const void * key, const redblacktree_node_t * node)
{
   assert(cb == (callback_param_t *)2) ;
   unsigned key1 = (unsigned )key ;
   unsigned key2 = ((const struct TreeNode*)node)->key ;
   return key1 < key2 ? -1 : (key1 > key2 ? +1 : 0) ;
}

static int adapter_updatekey( callback_param_t * cb, const void * new_key, redblacktree_node_t * node)
{
   assert(cb == (callback_param_t *)3) ;
   ((struct TreeNode*)node)->key = (unsigned) new_key ;
   return 0 ;
}

static int adapter_updatekey_enomem( callback_param_t * cb, const void * new_key, redblacktree_node_t * node)
{
   assert(cb == (callback_param_t *)3) ;
   (void) new_key ;
   (void) node ;
   return ENOMEM ;
}

static int freenode_count = 0 ;

static int adapter_freenode( callback_param_t * cb, redblacktree_node_t * node )
{
   assert(cb == (callback_param_t*)4) ;
   ++ freenode_count ;
   ((struct TreeNode*)node)->is_freed = 1 ;
   return 0 ;
}

static redblacktree_node_t * build_perfect_tree( unsigned count, struct TreeNode * nodes )
{
   assert( count < 10000 ) ;
   assert( 0 == ((count + 1) & count) ) ;   // count == (2^power)-1
   unsigned root = (count + 1) / 2 ;
   if (root == 1) {
      nodes[root].aspect.left = nodes[root].aspect.right = 0 ;
   } else {
      redblacktree_node_t * left  = build_perfect_tree( root - 1, nodes ) ;
      redblacktree_node_t * right = build_perfect_tree( root - 1, nodes+root ) ;
      nodes[root].aspect.left  = left ;
      nodes[root].aspect.right = right ;
      left->parent  = (redblacktree_node_t*)((uintptr_t)1 | (uintptr_t)&nodes[root].aspect) ;
      right->parent = (redblacktree_node_t*)((uintptr_t)1 | (uintptr_t)&nodes[root].aspect) ;
   }
   nodes[root].aspect.parent = (redblacktree_node_t*) (uintptr_t)1 ;
   return &nodes[root].aspect;
}

static int test_insertconditions(void)
{
   redblacktree_t                tree             = redblacktree_INIT_FREEABLE ;
   redblacktree_compare_nodes_t  compare_nodes_cb = { .fct = &adapter_compare_nodes, .cb_param = (callback_param_t*)1 } ;
   redblacktree_compare_t        compare_cb       = { .fct = &adapter_compare_key_node, .cb_param = (callback_param_t*)2 } ;
   redblacktree_free_t           free_cb          = { .fct = &adapter_freenode, .cb_param = (callback_param_t*)4 } ;
   struct TreeNode               nodes[20] ;

   MEMSET0( &nodes ) ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      nodes[i].key = i ;
   }

   // TEST: root == NULL (insert clears left/right/parent fields)
   for(int i = 0; i <= 2; ++i) {
      nodes[i].aspect.parent = &nodes[10].aspect ;
      nodes[i].aspect.left   = &nodes[10].aspect ;
      nodes[i].aspect.right  = &nodes[10].aspect ;
   }
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[1].key, &nodes[1].aspect, &compare_cb)) ;
   TEST(0 == invariant_redblacktree( &tree, &compare_nodes_cb)) ;
   TEST(tree.root         == &nodes[1].aspect) ;
   TEST(tree.root->left   == 0) ;
   TEST(tree.root->right  == 0) ;
   TEST(tree.root->parent == (redblacktree_node_t*)0x01/*color black*/) ;
   // TEST: parent (BLACK) (insert clears left/right fields)
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[0].key, &nodes[0].aspect, &compare_cb )) ;
   TEST(0 == invariant_redblacktree( &tree, &compare_nodes_cb)) ;
   TEST(tree.root         == &nodes[1].aspect) ;
   TEST(tree.root->left   == &nodes[0].aspect) ;
   TEST(tree.root->right  == 0) ;
   TEST(tree.root->parent == (redblacktree_node_t*)0x01/*color black*/) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[2].key, &nodes[2].aspect, &compare_cb )) ;
   TEST(0 == invariant_redblacktree( &tree, &compare_nodes_cb )) ;
   TEST(tree.root         == &nodes[1].aspect) ;
   TEST(tree.root->left   == &nodes[0].aspect) ;
   TEST(tree.root->right  == &nodes[2].aspect) ;
   TEST(tree.root->parent == (redblacktree_node_t*)0x01/*color black*/) ;
   TEST(nodes[0].aspect.left   == 0) ;
   TEST(nodes[0].aspect.right  == 0) ;
   TEST(nodes[0].aspect.parent == &nodes[1].aspect/*color is RED*/) ;
   TEST(nodes[2].aspect.left   == 0) ;
   TEST(nodes[2].aspect.right  == 0) ;
   TEST(nodes[2].aspect.parent == &nodes[1].aspect/*color is RED*/) ;
   TEST(0 == freenodes_redblacktree( &tree, &free_cb )) ;

   // TEST: parent (RED) uncle(RED)
   for(int i = 2; i <= 4; ++i) {
      nodes[i].aspect.parent = &nodes[10].aspect ;
      nodes[i].aspect.left   = &nodes[10].aspect ;
      nodes[i].aspect.right  = &nodes[10].aspect ;
   }
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[3].key, &nodes[3].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[4].key, &nodes[4].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[2].key, &nodes[2].aspect, &compare_cb )) ;
   TEST(tree.root == &nodes[3].aspect) ;
   TEST(0         == PARENT(&nodes[3].aspect)) ;
   TEST(nodes[3].aspect.left  == &nodes[2].aspect) ;
   TEST(nodes[3].aspect.right == &nodes[4].aspect) ;
   TEST(PARENT(&nodes[2].aspect) == &nodes[3].aspect) ;
   TEST(ISRED(&nodes[2].aspect)) ;
   TEST(nodes[2].aspect.left  == 0) ;
   TEST(nodes[2].aspect.right == 0) ;
   TEST(PARENT(&nodes[4].aspect) == &nodes[3].aspect) ;
   TEST(ISRED(&nodes[4].aspect)) ;
   TEST(nodes[4].aspect.left  == 0) ;
   TEST(nodes[4].aspect.right == 0) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[1].key, &nodes[1].aspect, &compare_cb )) ;
   TEST(0 == invariant_redblacktree( &tree, &compare_nodes_cb )) ;
   TEST(PARENT(&nodes[3].aspect) == 0) ;
   TEST(tree.root == &nodes[3].aspect) ;
   TEST(nodes[3].aspect.left  == &nodes[2].aspect) ;
   TEST(nodes[3].aspect.right == &nodes[4].aspect) ;
   TEST(PARENT(&nodes[2].aspect) == &nodes[3].aspect) ;
   TEST(ISBLACK(&nodes[2].aspect)) ;
   TEST(nodes[2].aspect.left  == &nodes[1].aspect) ;
   TEST(nodes[2].aspect.right == 0) ;
   TEST(ISRED(&nodes[1].aspect)) ;
   TEST(PARENT(&nodes[4].aspect) == &nodes[3].aspect) ;
   TEST(ISBLACK(&nodes[4].aspect)) ;
   TEST(nodes[4].aspect.left  == 0) ;
   TEST(nodes[4].aspect.right == 0) ;
   TEST(0 == freenodes_redblacktree( &tree, &free_cb )) ;
   // DUAL
   for(int i = 2; i <= 5; ++i) {
      nodes[i].aspect.parent = &nodes[10].aspect ;
      nodes[i].aspect.left   = &nodes[10].aspect ;
      nodes[i].aspect.right  = &nodes[10].aspect ;
   }
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[3].key, &nodes[3].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[5].key, &nodes[5].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[2].key, &nodes[2].aspect, &compare_cb )) ;
   TEST(tree.root == &nodes[3].aspect) ;
   TEST(0 == invariant_redblacktree( &tree, &compare_nodes_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[4].key, &nodes[4].aspect, &compare_cb )) ;
   TEST(0 == invariant_redblacktree( &tree, &compare_nodes_cb )) ;
   TEST(0         == PARENT(&nodes[3].aspect)) ;
   TEST(tree.root == &nodes[3].aspect) ;
   TEST(nodes[3].aspect.left  == &nodes[2].aspect) ;
   TEST(nodes[3].aspect.right == &nodes[5].aspect) ;
   TEST(PARENT(&nodes[2].aspect) == &nodes[3].aspect) ;
   TEST(ISBLACK(&nodes[2].aspect)) ;
   TEST(nodes[2].aspect.left  == 0) ;
   TEST(nodes[2].aspect.right == 0) ;
   TEST(PARENT(&nodes[5].aspect) == &nodes[3].aspect) ;
   TEST(ISBLACK(&nodes[5].aspect)) ;
   TEST(nodes[5].aspect.left  == &nodes[4].aspect) ;
   TEST(nodes[5].aspect.right == 0) ;
   TEST(ISRED(&nodes[4].aspect)) ;
   TEST(0 == freenodes_redblacktree( &tree, &free_cb )) ;

   // TEST: parent (RED)  uncle(NULL)
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[3].key, &nodes[3].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[1].key, &nodes[1].aspect, &compare_cb )) ;
   TEST(tree.root == &nodes[3].aspect) ;
   TEST(0         == PARENT(&nodes[3].aspect)) ;
   TEST(nodes[3].aspect.left  == &nodes[1].aspect) ;
   TEST(nodes[3].aspect.right == 0) ;
   TEST(PARENT(&nodes[1].aspect) == &nodes[3].aspect) ;
   TEST(ISRED(&nodes[1].aspect)) ;
   TEST(nodes[1].aspect.left  == 0) ;
   TEST(nodes[1].aspect.right == 0) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[2].key, &nodes[2].aspect, &compare_cb)) ;
   TEST(0 == invariant_redblacktree( &tree, &compare_nodes_cb )) ;
   TEST(PARENT(&nodes[2].aspect) == 0) ;
   TEST(tree.root == &nodes[2].aspect) ;
   TEST(nodes[2].aspect.left  == &nodes[1].aspect) ;
   TEST(nodes[2].aspect.right == &nodes[3].aspect) ;
   TEST(PARENT(&nodes[3].aspect) == &nodes[2].aspect) ;
   TEST(ISRED(&nodes[3].aspect)) ;
   TEST(nodes[3].aspect.left  == 0) ;
   TEST(nodes[3].aspect.right == 0) ;
   TEST(PARENT(&nodes[1].aspect) == &nodes[2].aspect) ;
   TEST(ISRED(&nodes[1].aspect)) ;
   TEST(nodes[1].aspect.left  == 0) ;
   TEST(nodes[1].aspect.right == 0) ;
   TEST(0 == freenodes_redblacktree( &tree, &free_cb )) ;
   // DUAL
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[3].key, &nodes[3].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[4].key, &nodes[4].aspect, &compare_cb )) ;
   TEST(tree.root == &nodes[3].aspect) ;
   TEST(0 == invariant_redblacktree( &tree, &compare_nodes_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[5].key, &nodes[5].aspect, &compare_cb )) ;
   TEST(0 == invariant_redblacktree( &tree, &compare_nodes_cb )) ;
   TEST(0 == PARENT(&nodes[4].aspect)) ;
   TEST(tree.root == &nodes[4].aspect) ;
   TEST(nodes[4].aspect.left  == &nodes[3].aspect) ;
   TEST(nodes[4].aspect.right == &nodes[5].aspect) ;
   TEST(PARENT(&nodes[3].aspect) == &nodes[4].aspect) ;
   TEST(ISRED(&nodes[3].aspect)) ;
   TEST(nodes[3].aspect.left  == 0) ;
   TEST(nodes[3].aspect.right == 0) ;
   TEST(PARENT(&nodes[5].aspect) == &nodes[4].aspect) ;
   TEST(ISRED(&nodes[5].aspect)) ;
   TEST(nodes[5].aspect.left  == 0) ;
   TEST(nodes[5].aspect.right == 0) ;
   TEST(0 == freenodes_redblacktree( &tree, &free_cb )) ;

   // TEST: parent (RED)  uncle(BLACK) / propagates
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[7].key, &nodes[7].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[5].key, &nodes[5].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[9].key, &nodes[9].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[3].key, &nodes[3].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[6].key, &nodes[6].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[8].key, &nodes[8].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[10].key, &nodes[10].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[2].key, &nodes[2].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[4].key, &nodes[4].aspect, &compare_cb )) ;
   TEST(0 == invariant_redblacktree( &tree, &compare_nodes_cb )) ;
   /*       7
    *     5     9
    *   3   6   8  10
    *  2 4             */
   TEST(tree.root == &nodes[7].aspect) ;
   TEST(nodes[7].aspect.left  == &nodes[5].aspect) ;
   TEST(nodes[7].aspect.right == &nodes[9].aspect) ;
   TEST(ISRED(&nodes[5].aspect)) ;
   TEST(nodes[5].aspect.left  == &nodes[3].aspect) ;
   TEST(nodes[5].aspect.right == &nodes[6].aspect) ;
   TEST(ISBLACK(&nodes[9].aspect)) ;
   TEST(nodes[9].aspect.left  == &nodes[8].aspect) ;
   TEST(nodes[9].aspect.right == &nodes[10].aspect) ;
   TEST(ISBLACK(&nodes[3].aspect)) ;
   TEST(ISBLACK(&nodes[6].aspect)) ;
   TEST(ISRED(&nodes[8].aspect)) ;
   TEST(ISRED(&nodes[10].aspect)) ;
   TEST(ISRED(&nodes[2].aspect)) ;
   TEST(ISRED(&nodes[4].aspect)) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[1].key, &nodes[1].aspect, &compare_cb )) ;
   TEST(0 == invariant_redblacktree( &tree, &compare_nodes_cb )) ;
   TEST(tree.root == &nodes[5].aspect) ;
   TEST(nodes[5].aspect.left  == &nodes[3].aspect) ;
   TEST(nodes[5].aspect.right == &nodes[7].aspect) ;
   TEST(nodes[7].aspect.left  == &nodes[6].aspect) ;
   TEST(nodes[7].aspect.right == &nodes[9].aspect) ;
   TEST(nodes[3].aspect.left  == &nodes[2].aspect) ;
   TEST(nodes[3].aspect.right == &nodes[4].aspect) ;
   TEST(nodes[2].aspect.left  == &nodes[1].aspect) ;
   TEST(nodes[2].aspect.right == 0) ;
   TEST(ISBLACK(&nodes[9].aspect)) ;
   TEST(ISRED(&nodes[8].aspect)) ;
   TEST(ISRED(&nodes[10].aspect)) ;
   TEST(ISRED(&nodes[7].aspect)) ;
   TEST(ISRED(&nodes[3].aspect)) ;
   TEST(ISBLACK(&nodes[2].aspect)) ;
   TEST(ISBLACK(&nodes[4].aspect)) ;
   TEST(ISRED(&nodes[1].aspect)) ;
   TEST(0 == freenodes_redblacktree( &tree, &free_cb )) ;
   // DUAL
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[4].key, &nodes[4].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[2].key, &nodes[2].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[9].key, &nodes[9].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[1].key, &nodes[1].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[3].key, &nodes[3].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[7].key, &nodes[7].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[10].key, &nodes[10].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[5].key, &nodes[5].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[8].key, &nodes[8].aspect, &compare_cb )) ;
   TEST(0 == invariant_redblacktree( &tree, &compare_nodes_cb )) ;
   /*        4
    *     2       9
    *   1   3   7   10
    *          5 8     */
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[6].key, &nodes[6].aspect, &compare_cb )) ;
   TEST(0 == invariant_redblacktree( &tree, &compare_nodes_cb )) ;
   TEST(tree.root == &nodes[7].aspect) ;
   TEST(nodes[7].aspect.left == &nodes[4].aspect) ;
   TEST(nodes[7].aspect.right== &nodes[9].aspect) ;
   TEST(nodes[9].aspect.left == &nodes[8].aspect) ;
   TEST(nodes[9].aspect.right== &nodes[10].aspect) ;
   TEST(nodes[4].aspect.left == &nodes[2].aspect) ;
   TEST(nodes[4].aspect.right== &nodes[5].aspect) ;
   TEST(ISRED(&nodes[9].aspect)) ;
   TEST(ISBLACK(&nodes[10].aspect)) ;
   TEST(ISBLACK(&nodes[7].aspect)) ;
   TEST(ISBLACK(&nodes[8].aspect)) ;
   TEST(ISBLACK(&nodes[5].aspect)) ;
   TEST(ISRED(&nodes[4].aspect)) ;
   TEST(ISBLACK(&nodes[2].aspect)) ;
   TEST(ISRED(&nodes[1].aspect)) ;
   TEST(ISRED(&nodes[3].aspect)) ;
   TEST(0 == freenodes_redblacktree( &tree, &free_cb )) ;

   return 0 ;
ONABORT:
   return 1 ;
}

static int test_removeconditions(void)
{
   redblacktree_t                tree             = redblacktree_INIT_FREEABLE ;
   redblacktree_compare_nodes_t  compare_nodes_cb = { .fct = &adapter_compare_nodes, .cb_param = (callback_param_t*)1 } ;
   redblacktree_compare_t        compare_cb       = { .fct = &adapter_compare_key_node, .cb_param = (callback_param_t*)2 } ;
   redblacktree_free_t           free_cb          = { .fct = &adapter_freenode, .cb_param = (callback_param_t*)4 } ;
   struct TreeNode               nodes[20] ;
   redblacktree_node_t           * node = 0 ;

   MEMSET0( &nodes ) ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      nodes[i].key = i ;
   }

   // TEST: remove successor (is directly right of node + RED child of black node)
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[7].key, &nodes[7].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[4].key, &nodes[4].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[9].key, &nodes[9].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[3].key, &nodes[3].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[5].key, &nodes[5].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[6].key, &nodes[6].aspect, &compare_cb )) ;
   TEST(0 == invariant_redblacktree( &tree, &compare_nodes_cb )) ;
   /*        7
    *     4      9
    *   3   5
    *         6      */
   TEST(tree.root             == &nodes[7].aspect) ;
   TEST(nodes[7].aspect.left  == &nodes[4].aspect) ;
   TEST(nodes[7].aspect.right == &nodes[9].aspect) ;
   TEST(nodes[4].aspect.left  == &nodes[3].aspect) ;
   TEST(nodes[4].aspect.right == &nodes[5].aspect) ;
   TEST(nodes[5].aspect.left  == 0) ;
   TEST(nodes[5].aspect.right == &nodes[6].aspect) ;
   TEST(ISBLACK(&nodes[9].aspect)) ;
   TEST(ISRED(&nodes[6].aspect)) ;
   TEST(ISRED(&nodes[4].aspect)) ;
   TEST(0 == remove_redblacktree( &tree, (void*)nodes[4].key, &node, &compare_cb)) ;
   TEST(0 == invariant_redblacktree( &tree, &compare_nodes_cb )) ;
   TEST(node == &nodes[4].aspect) ;
   TEST(tree.root             == &nodes[7].aspect) ;
   TEST(nodes[7].aspect.left  == &nodes[5].aspect) ;
   TEST(nodes[7].aspect.right == &nodes[9].aspect) ;
   TEST(nodes[5].aspect.left  == &nodes[3].aspect) ;
   TEST(nodes[5].aspect.right == &nodes[6].aspect) ;
   TEST(ISBLACK(&nodes[6].aspect)) ;
   TEST(0 == freenodes_redblacktree( &tree, &free_cb )) ;

   // TEST: remove successor (root)
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[7].key, &nodes[7].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[5].key, &nodes[5].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[9].key, &nodes[9].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[3].key, &nodes[3].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[6].key, &nodes[6].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[8].key, &nodes[8].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[10].key, &nodes[10].aspect, &compare_cb )) ;
   TEST(0 == invariant_redblacktree( &tree, &compare_nodes_cb )) ;
   /*        7
    *     5      9
    *   3   6  8  10  */
   TEST(tree.root             == &nodes[7].aspect) ;
   TEST(nodes[7].aspect.left  == &nodes[5].aspect) ;
   TEST(nodes[7].aspect.right == &nodes[9].aspect) ;
   TEST(nodes[9].aspect.left  == &nodes[8].aspect) ;
   TEST(nodes[9].aspect.right == &nodes[10].aspect) ;
   TEST(ISBLACK(&nodes[9].aspect)) ;
   TEST(ISRED(&nodes[8].aspect)) ;
   TEST(ISRED(&nodes[10].aspect)) ;
   TEST(0 == remove_redblacktree( &tree, (void*)nodes[7].key, &node, &compare_cb)) ;
   TEST(0 == invariant_redblacktree( &tree, &compare_nodes_cb )) ;
   TEST(node == &nodes[7].aspect) ;
   TEST(tree.root             == &nodes[8].aspect) ;
   TEST(nodes[8].aspect.left  == &nodes[5].aspect) ;
   TEST(nodes[8].aspect.right == &nodes[9].aspect) ;
   TEST(nodes[9].aspect.left  == 0 /*successor removed*/) ;
   TEST(nodes[9].aspect.right == &nodes[10].aspect) ;
   TEST(0 == freenodes_redblacktree( &tree, &free_cb )) ;

   // TEST: uncle of removed is RED
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[7].key, &nodes[7].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[5].key, &nodes[5].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[11].key, &nodes[11].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[3].key, &nodes[3].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[6].key, &nodes[6].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[9].key, &nodes[9].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[13].key, &nodes[13].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[8].key, &nodes[8].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[10].key, &nodes[10].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[12].key, &nodes[12].aspect, &compare_cb )) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[14].key, &nodes[14].aspect, &compare_cb )) ;
   SETBLACK(&nodes[5].aspect) ;
   SETBLACK(&nodes[3].aspect) ;
   SETBLACK(&nodes[6].aspect) ;
   SETRED(&nodes[11].aspect) ;
   SETBLACK(&nodes[8].aspect) ;
   SETBLACK(&nodes[9].aspect) ;
   SETBLACK(&nodes[10].aspect) ;
   SETBLACK(&nodes[12].aspect) ;
   SETBLACK(&nodes[13].aspect) ;
   SETBLACK(&nodes[14].aspect) ;
   TEST(0 == invariant_redblacktree( &tree, &compare_nodes_cb )) ;
   /*        7
    *     5       11
    *   3   6  9      13
    *         8 10  12  14 */
   TEST(tree.root             == &nodes[7].aspect) ;
   TEST(nodes[7].aspect.left  == &nodes[5].aspect) ;
   TEST(nodes[7].aspect.right == &nodes[11].aspect) ;
   TEST(nodes[5].aspect.left  == &nodes[3].aspect) ;
   TEST(nodes[5].aspect.right == &nodes[6].aspect) ;
   TEST(nodes[11].aspect.left == &nodes[9].aspect) ;
   TEST(nodes[11].aspect.right== &nodes[13].aspect) ;
   TEST(nodes[9].aspect.left  == &nodes[8].aspect) ;
   TEST(nodes[9].aspect.right == &nodes[10].aspect) ;
   TEST(nodes[13].aspect.left == &nodes[12].aspect) ;
   TEST(nodes[13].aspect.right== &nodes[14].aspect) ;
   TEST(0 == remove_redblacktree( &tree, (void*)nodes[3].key, &node, &compare_cb )) ;
   TEST(0 == invariant_redblacktree( &tree, &compare_nodes_cb )) ;
   TEST(node == &nodes[3].aspect) ;
   TEST(tree.root              == &nodes[11].aspect) ;
   TEST(nodes[11].aspect.left  == &nodes[7].aspect) ;
   TEST(nodes[11].aspect.right == &nodes[13].aspect) ;
   TEST(nodes[7].aspect.left   == &nodes[5].aspect) ;
   TEST(nodes[7].aspect.right  == &nodes[9].aspect) ;
   TEST(nodes[5].aspect.left   == 0) ;
   TEST(nodes[5].aspect.right  == &nodes[6].aspect) ;
   TEST(nodes[9].aspect.left   == &nodes[8].aspect) ;
   TEST(nodes[9].aspect.right  == &nodes[10].aspect) ;
   TEST(ISRED(&nodes[9].aspect)) ;
   TEST(ISRED(&nodes[6].aspect)) ;
   TEST(ISBLACK(&nodes[12].aspect)) ;
   TEST(ISBLACK(&nodes[13].aspect)) ;
   TEST(ISBLACK(&nodes[14].aspect)) ;
   TEST(0 == freenodes_redblacktree( &tree, &free_cb )) ;

   return 0 ;
ONABORT:
   return 1 ;
}

int unittest_platform_index_redblacktree()
{
   redblacktree_t                tree             = redblacktree_INIT_FREEABLE ;
   redblacktree_compare_nodes_t  compare_nodes_cb = { .fct = &adapter_compare_nodes, .cb_param = (callback_param_t*)1 } ;
   redblacktree_compare_t        compare_cb       = { .fct = &adapter_compare_key_node, .cb_param = (callback_param_t*)2 } ;
   redblacktree_update_key_t     update_key_cb    = { .fct = &adapter_updatekey, .cb_param = (callback_param_t*)3 } ;
   redblacktree_update_key_t     update_key_err   = { .fct = &adapter_updatekey_enomem, .cb_param = (callback_param_t*)3 } ;
   redblacktree_free_t           free_cb          = { .fct = &adapter_freenode, .cb_param = (callback_param_t*)4 } ;
   struct TreeNode               nodes[10000] ;
   redblacktree_node_t           * treenode ;

   assert( (void*)&nodes == (void*)nodes ) ;
   MEMSET0( &nodes ) ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      nodes[i].key = i ;
   }

   // TEST init, double free
   TEST(! tree.root) ;
   tree.root = &nodes[0].aspect ;
   TEST(0 == init_redblacktree( &tree )) ;
   TEST(! tree.root) ;
   tree.root = &nodes[0].aspect ;
   TEST(0 == free_redblacktree( &tree, 0)) ;
   TEST(! tree.root ) ;
   TEST(0 == free_redblacktree( &tree, &free_cb)) ;
   TEST(! tree.root ) ;

   // TEST free_redblacktree
   freenode_count = 0 ;
   tree.root = build_perfect_tree( 7, nodes ) ;
   TEST( tree.root == &nodes[4].aspect ) ;
   TEST(0 == invariant_redblacktree( &tree, &compare_nodes_cb )) ;
   TEST(0 == free_redblacktree( &tree, &free_cb )) ;
   TEST(7 == freenode_count) ;
   TEST(0 == tree.root) ;
   for (int i = 1; i <= 7; ++i) {
      TEST(0 == nodes[i].aspect.left) ;
      TEST(0 == nodes[i].aspect.right) ;
      TEST(0 == nodes[i].aspect.parent) ;
      TEST(1 == nodes[i].is_freed) ;
      nodes[i].is_freed = 0 ;
   }
   TEST(0 == init_redblacktree( &tree )) ;

   if (test_insertconditions()) goto ONABORT ;
   if (test_removeconditions()) goto ONABORT ;

   // TEST insert uneven address
   TEST(EINVAL == insert_redblacktree( &tree, (void*)0, (redblacktree_node_t*)(1+(uintptr_t)&nodes[0].aspect), &compare_cb )) ;

   // TEST insert,remove cycle
   TEST(tree.root == 0) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[0].key, &nodes[0].aspect, &compare_cb )) ;
   TEST(0 == nodes[0].is_freed) ;
   TEST(tree.root == &nodes[0].aspect) ;
   TEST(0 == remove_redblacktree( &tree, (void*)0, &treenode, &compare_cb)) ;
   TEST(0 == nodes[0].is_freed) ;
   TEST(0 == nodes[0].aspect.parent) ;
   TEST(treenode  == &nodes[0].aspect) ;
   TEST(tree.root == 0) ;

   // TEST insert, freenode cycle
   TEST(tree.root == 0) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[10].key, &nodes[10].aspect, &compare_cb )) ;
   TEST(0 == nodes[10].is_freed) ;
   TEST(tree.root == &nodes[10].aspect) ;
   TEST(0 == freenodes_redblacktree( &tree, &free_cb)) ;
   TEST(1 == nodes[10].is_freed) ;
   TEST(tree.root == 0) ;
   nodes[10].is_freed = 0 ;

   // TEST insert,free cycle (all nodes are freed)
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == insert_redblacktree( &tree, (void*)nodes[i].key, &nodes[i].aspect, &compare_cb)) ;
      if (!(i%100)) TEST(0 == invariant_redblacktree( &tree, &compare_nodes_cb )) ;
   }
   TEST(0 == invariant_redblacktree( &tree, &compare_nodes_cb )) ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == find_redblacktree( &tree, (void*)nodes[i].key, &treenode, &compare_cb)) ;
      TEST(treenode == &nodes[i].aspect) ;
      TEST(0        == nodes[i].is_freed) ;
   }
   TEST(0 == free_redblacktree( &tree, &free_cb )) ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == nodes[i].aspect.left) ;
      TEST(0 == nodes[i].aspect.right) ;
      TEST(0 == nodes[i].aspect.parent) ;
      TEST(1 == nodes[i].is_freed) ;
      nodes[i].is_freed = 0 ;
   }
   TEST(0 == init_redblacktree( &tree )) ;
   for (int i = (int)nrelementsof(nodes)-1; i >= 0; --i) {
      TEST(0 == insert_redblacktree( &tree, (void*)nodes[i].key, &nodes[i].aspect, &compare_cb)) ;
      if (!(i%100)) TEST(0 == invariant_redblacktree( &tree, &compare_nodes_cb )) ;
   }
   TEST(0 == invariant_redblacktree( &tree, &compare_nodes_cb )) ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == find_redblacktree( &tree, (void*)nodes[i].key, &treenode, &compare_cb)) ;
      TEST(treenode == &nodes[i].aspect) ;
      TEST(0        == nodes[i].is_freed) ;
   }
   TEST(0 == free_redblacktree( &tree, &free_cb )) ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == nodes[i].aspect.left) ;
      TEST(0 == nodes[i].aspect.right) ;
      TEST(0 == nodes[i].aspect.parent) ;
      TEST(1 == nodes[i].is_freed) ;
      nodes[i].is_freed = 0 ;
   }
   TEST(0 == init_redblacktree( &tree )) ;

   // TEST insert,remove
   srand(100) ;
   for (unsigned i = 0; i < 10*nrelementsof(nodes); ++i) {
      int id = rand() % (int)nrelementsof(nodes) ;
      if (nodes[id].is_inserted) continue ;
      nodes[id].is_inserted = 1 ;
      TEST(0 == insert_redblacktree( &tree, (void*)nodes[id].key, &nodes[id].aspect, &compare_cb)) ;
   }
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      if (nodes[i].is_inserted) continue ;
      nodes[i].is_inserted = 1 ;
      TEST(0 == insert_redblacktree( &tree, (void*)nodes[i].key, &nodes[i].aspect, &compare_cb)) ;
   }
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == find_redblacktree( &tree, (void*)nodes[i].key, &treenode, &compare_cb)) ;
      TEST(treenode == &nodes[i].aspect) ;
      TEST(0        == nodes[i].is_freed) ;
   }
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      nodes[i].is_inserted = 0 ;
      TEST(0 == remove_redblacktree( &tree, (void*)nodes[i].key, &treenode, &compare_cb)) ;
      TEST(treenode == &nodes[i].aspect) ;
      TEST(ESRCH    == find_redblacktree( &tree, (void*)nodes[i].key, &treenode, &compare_cb)) ;
      if (!(i%100)) TEST(0 == invariant_redblacktree( &tree, &compare_nodes_cb )) ;
   }
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == nodes[i].aspect.left) ;
      TEST(0 == nodes[i].aspect.right) ;
      TEST(0 == nodes[i].aspect.parent) ;
      TEST(0 == nodes[i].is_freed) ;
   }

   // TEST insert, freenodes
   for (unsigned i = 0; i < 10 * nrelementsof(nodes); ++i) {
      int id = rand() % (int)nrelementsof(nodes) ;
      if (nodes[id].is_inserted) continue ;
      nodes[id].is_inserted = 1 ;
      TEST(0 == insert_redblacktree( &tree, (void*)nodes[id].key, &nodes[id].aspect, &compare_cb)) ;
   }
   freenode_count = 0 ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      if (nodes[i].is_inserted) {
         TEST(0 == find_redblacktree( &tree, (void*)nodes[i].key, &treenode, &compare_cb)) ;
         TEST(treenode == &nodes[i].aspect) ;
         -- freenode_count ;
      } else {
         TEST(ESRCH == find_redblacktree( &tree, (void*)nodes[i].key, &treenode, &compare_cb)) ;
      }
      TEST(0 == nodes[i].is_freed) ;
   }
   TEST(0 == freenodes_redblacktree(&tree, &free_cb)) ;
   TEST(0 == freenode_count) ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == nodes[i].aspect.left) ;
      TEST(0 == nodes[i].aspect.right) ;
      TEST(0 == nodes[i].aspect.parent) ;
      TEST(nodes[i].is_inserted == nodes[i].is_freed) ;
      nodes[i].is_freed    = 0 ;
      nodes[i].is_inserted = 0 ;
   }

   // TEST freenodes
   freenode_count = 0 ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == insert_redblacktree( &tree, (void*)nodes[i].key, &nodes[i].aspect, &compare_cb)) ;
   }
   TEST(0 == freenode_count) ;
   TEST(0 == freenodes_redblacktree( &tree, &free_cb)) ;
   TEST(nrelementsof(nodes) == freenode_count) ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == nodes[i].aspect.left) ;
      TEST(0 == nodes[i].aspect.right) ;
      TEST(0 == nodes[i].aspect.parent) ;
      TEST(1 == nodes[i].is_freed) ;
      nodes[i].is_freed = 0 ;
   }

   // TEST updatekey
   freenode_count = 0 ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == insert_redblacktree( &tree, (void*)nodes[i].key, &nodes[i].aspect, &compare_cb)) ;
   }
   for (unsigned i = 0; i < 10 * nrelementsof(nodes); ++i) {
      int id = rand() % (int)nrelementsof(nodes) ;
      if (nodes[id].is_inserted) continue ;
      nodes[id].is_inserted = 1 ;
      TEST(0 == updatekey_redblacktree( &tree, (void*)nodes[id].key, (void*)(nrelementsof(nodes) + nodes[id].key), &update_key_cb, &compare_cb)) ;
   }
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      if (nodes[i].is_inserted) continue ;
      nodes[i].is_inserted = 1 ;
      TEST(0 == updatekey_redblacktree( &tree, (void*)nodes[i].key, (void*)(nrelementsof(nodes) + nodes[i].key), &update_key_cb, &compare_cb)) ;
   }
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(nodes[i].is_inserted) ;
      TEST((i + nrelementsof(nodes)) == nodes[i].key) ;
   }
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == find_redblacktree( &tree, (void*)(i+nrelementsof(nodes)), &treenode, &compare_cb)) ;
      TEST(treenode == &nodes[i].aspect) ;
   }
   TEST(0 == freenode_count) ;
   TEST(0 == freenodes_redblacktree( &tree, &free_cb)) ;
   TEST(nrelementsof(nodes) == freenode_count) ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == nodes[i].aspect.left) ;
      TEST(0 == nodes[i].aspect.right) ;
      TEST(0 == nodes[i].aspect.parent) ;
      TEST(1 == nodes[i].is_freed) ;
      nodes[i].is_freed = 0 ;
      nodes[i].key      = i ;
   }

   // TEST updatekey to itself
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == insert_redblacktree( &tree, (void*)nodes[i].key, &nodes[i].aspect, &compare_cb)) ;
   }
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == updatekey_redblacktree( &tree, (void*)nodes[i].key, (void*)nodes[i].key, &update_key_cb, &compare_cb)) ;
   }
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == find_redblacktree( &tree, (void*)i, &treenode, &compare_cb)) ;
      TEST(treenode == &nodes[i].aspect) ;
   }
   TEST(0 == freenodes_redblacktree( &tree, 0)) ;

   // TEST updatekey: ESRCH
   TEST(!tree.root) ;
   TEST(ESRCH == updatekey_redblacktree( &tree, (void*)nodes[0].key, (void*)nodes[1].key, &update_key_cb, &compare_cb)) ;

   // TEST updatekey: EEXIST
   freenode_count = 0 ;
   TEST(!tree.root) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[0].key, &nodes[0].aspect, &compare_cb)) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[1].key, &nodes[1].aspect, &compare_cb)) ;
   TEST(EEXIST == updatekey_redblacktree( &tree, (void*)nodes[0].key, (void*)nodes[1].key, &update_key_cb, &compare_cb)) ;
   for (unsigned i = 0; i < 2; ++i) {
      TEST(0 == find_redblacktree( &tree, (void*)i, &treenode, &compare_cb)) ;
      TEST(treenode == &nodes[i].aspect) ;
   }
   TEST(0 == freenode_count) ;
   TEST(0 == freenodes_redblacktree( &tree, &free_cb)) ;
   TEST(2 == freenode_count) ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST((i < 2) == nodes[i].is_freed) ;
      nodes[i].is_freed = 0 ;
   }

   // TEST updatekey: update_key_cb return ENOMEM
   freenode_count = 0 ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[0].key, &nodes[0].aspect, &compare_cb)) ;
   TEST(0 == insert_redblacktree( &tree, (void*)nodes[1].key, &nodes[1].aspect, &compare_cb)) ;
   TEST(ENOMEM == updatekey_redblacktree( &tree, (void*)nodes[0].key, (void*)nodes[1].key, &update_key_err, &compare_cb)) ;
   for (unsigned i = 0; i < 2; ++i) {
      TEST(0 == find_redblacktree( &tree, (void*)i, &treenode, &compare_cb)) ;
      TEST(treenode == &nodes[i].aspect) ;
   }
   TEST(0 == freenode_count) ;
   TEST(0 == freenodes_redblacktree( &tree, &free_cb)) ;
   TEST(2 == freenode_count) ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST((i < 2) == nodes[i].is_freed) ;
      nodes[i].is_freed = 0 ;
   }

   return 0 ;
ONABORT:
   (void) free_redblacktree( &tree, &free_cb ) ;
   return 1 ;
}

#endif
