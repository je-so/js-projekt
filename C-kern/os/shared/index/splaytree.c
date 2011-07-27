/* title: Splaytree-Index impl
   Implements memory tree index with
   help of splay tree algorithm.

   See <http://en.wikipedia.org/wiki/Splay_tree> for a description.

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

   file: C-kern/api/os/index/splaytree.h
    Header file of <Splaytree-Index>.

   file: C-kern/os/shared/index/splaytree.c
    Implementation file of <Splaytree-Index impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/os/index/splaytree.h"
#include "C-kern/api/aspect/callback.h"
#include "C-kern/api/errlog.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/math/int/signum.h"
#endif


int invariant_splaytree( splaytree_t * tree, const splaytree_compare_nodes_t * compare_callback )
{
   int err ;
   const splaytree_node_t ** parents = 0 ;
   const splaytree_node_t     * node = tree->root ;

   if (!compare_callback) {
      err = EINVAL ;
      goto ABBRUCH ;
   }

   if (node) {

      size_t depth = 64 ;
      parents = (const splaytree_node_t**) malloc( depth * sizeof(splaytree_node_t*)) ;
      if (!parents) {
         err = ENOMEM ;
         goto ABBRUCH ;
      }

      size_t parent_idx = 0 ;
      for (;;) {

         const splaytree_node_t  * nodeleft  = node->left ;
         const splaytree_node_t  * noderight = node->right ;
         if (nodeleft && compare_callback->fct(compare_callback->cb_param, node, nodeleft) <= 0) {
            err = EINVAL ;
            goto ABBRUCH ;
         }
         if (noderight && compare_callback->fct(compare_callback->cb_param, noderight, node) <= 0) {
            err = EINVAL ;
            goto ABBRUCH ;
         }

         if (nodeleft) {

            if (parent_idx == depth) {
               const splaytree_node_t ** parents2 = 0 ;
               depth *= 2 ;
               if (depth < ((size_t)-1) / sizeof(splaytree_node_t*)) {
                  parents2 = (const splaytree_node_t**) realloc( parents, depth * sizeof(splaytree_node_t*)) ;
               }
               if (!parents2) {
                  err = ENOMEM ;
                  goto ABBRUCH ;
               }
               parents = parents2 ;
            }

            parents[parent_idx] = node ;
            ++parent_idx ;
            node = nodeleft ;

         } else if (noderight) {

            if (parent_idx == depth) {
               const splaytree_node_t ** parents2 = 0 ;
               depth *= 2 ;
               if (depth < ((size_t)-1) / sizeof(splaytree_node_t*)) {
                  parents2 = (const splaytree_node_t**) realloc( parents, depth * sizeof(splaytree_node_t*)) ;
               }
               if (!parents2) {
                  err = ENOMEM ;
                  goto ABBRUCH ;
               }
               parents = parents2 ;
            }

            parents[parent_idx] = node ;
            ++parent_idx ;
            node = noderight ;

         } else {

            while (parent_idx) {
               const splaytree_node_t * parent = parents[--parent_idx] ;
               assert(parent->right == node || parent->left == node) ;
               if (parent->left == node && parent->right) {
                  ++ parent_idx ;
                  node = parent->right ;
                  break ;
               }
               node = parent ;
            }
            if (!parent_idx) break ;

         }
      }
   }

   free(parents) ;
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   free(parents) ;
   return err ;
}

int free_splaytree( splaytree_t * tree, const splaytree_free_t * free_callback )
{
   int err = freenodes_splaytree( tree, free_callback ) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}


#ifdef KONFIG_UNITTEST
/* Searches for a node X with key 'key' and makes it the new root of the tree.
 * If the node is not in the tree the last node (with no left or right child)
 * is used instead. The rotations done to move X to the top are called 'splaying'.
 * See http://en.wikipedia.org/wiki/Splay_tree for a description of splay trees.
 * This is a straight forward recursive implementation.
 */
static splaytree_node_t * simple_splay_key( splaytree_t * tree, splaytree_node_t * root, const void * key, const splaytree_compare_t * compare_callback )
{
   assert(root) ;
   splaytree_node_t * Xroot = root ;

   if (compare_callback->fct(compare_callback->cb_param, key, root) > 0 && root->right) {
      if (compare_callback->fct(compare_callback->cb_param, key, root->right) > 0 && root->right->right) {
         Xroot = simple_splay_key( tree, root->right->right, key, compare_callback) ;
         root->right->right = Xroot->left ;
         Xroot->left = root->right ;
         root->right = Xroot->left->left ;
         Xroot->left->left = root ;
      } else if (compare_callback->fct(compare_callback->cb_param, key, root->right) < 0 && root->right->left) {
         Xroot = simple_splay_key( tree, root->right->left, key, compare_callback) ;
         root->right->left = Xroot->right ;
         Xroot->right = root->right ;
         root->right  = Xroot->left ;
         Xroot->left  = root ;
      } else {
         Xroot = root->right ;
         root->right = Xroot->left ;
         Xroot->left = root ;
      }
   } else if (compare_callback->fct(compare_callback->cb_param, key, root) < 0 && root->left) {
      if (compare_callback->fct(compare_callback->cb_param, key, root->left) < 0 && root->left->left) {
         Xroot = simple_splay_key( tree, root->left->left, key, compare_callback) ;
         root->left->left = Xroot->right ;
         Xroot->right = root->left ;
         root->left   = Xroot->right->right ;
         Xroot->right->right = root ;
      } else if (compare_callback->fct(compare_callback->cb_param, key, root->left) > 0 && root->left->right) {
         Xroot = simple_splay_key( tree, root->left->right, key, compare_callback) ;
         root->left->right = Xroot->left ;
         Xroot->left  = root->left ;
         root->left   = Xroot->right ;
         Xroot->right = root ;
      } else {
         Xroot = root->left ;
         root->left   = Xroot->right ;
         Xroot->right = root ;
      }
   }

   if (tree->root == root) {
      tree->root = Xroot ;
   }

   return Xroot ;
}
#endif

/* Searches for a node X with key 'key' and makes it the new root of the tree.
 * If the node is not in the tree the last node (with no left or right child)
 * is used instead. The rotations done to move X to the top are called 'splaying'.
 * See http://en.wikipedia.org/wiki/Splay_tree for a description of splay trees.
 * This version splays from top to down without recursion.
 */
static int splay_key( splaytree_t * tree, const void * key, const splaytree_compare_t * compare_callback )
{
   splaytree_node_t        keyroot = { .left = 0, .right = 0 } ;
   splaytree_node_t        * higherAsKey ;
   splaytree_node_t        * lowerAsKey ;

   higherAsKey = &keyroot ;
   lowerAsKey  = &keyroot ;

   splaytree_node_t * node = tree->root ;
   assert(node) ;

   int cmp = compare_callback->fct(compare_callback->cb_param, key, node) ;
   for (;;) {
      if (cmp > 0) {

         splaytree_node_t * rightnode = node->right ;
         if (!rightnode) break ;

         cmp = compare_callback->fct(compare_callback->cb_param, key, rightnode) ;
         if (cmp > 0 && rightnode->right) {
            node->right = rightnode->left ;
            rightnode->left = node ;
            node = rightnode ;
            rightnode = node->right ;
            cmp = compare_callback->fct(compare_callback->cb_param, key, rightnode) ;
         } else if (cmp < 0 && rightnode->left) {
            higherAsKey->left = rightnode ;
            higherAsKey = rightnode ;
            rightnode = rightnode->left ;
            cmp = compare_callback->fct(compare_callback->cb_param, key, rightnode) ;
         }
         lowerAsKey->right = node ;
         lowerAsKey = node ;
         node = rightnode ;

      } else if (cmp < 0) {

         splaytree_node_t * leftnode = node->left ;
         if (!leftnode) break ;

         cmp = compare_callback->fct(compare_callback->cb_param, key, leftnode) ;
         if (cmp < 0 && leftnode->left) {
            node->left = leftnode->right ;
            leftnode->right = node ;
            node = leftnode ;
            leftnode = node->left ;
            cmp = compare_callback->fct(compare_callback->cb_param, key, leftnode) ;
         } else if (cmp > 0 && leftnode->right) {
            lowerAsKey->right = leftnode ;
            lowerAsKey = leftnode ;
            leftnode = leftnode->right ;
            cmp = compare_callback->fct(compare_callback->cb_param, key, leftnode) ;
         }
         higherAsKey->left = node ;
         higherAsKey = node ;
         node = leftnode ;

      } else {
         /* found node */
         break ;
      }
   }

   tree->root        = node ;
   higherAsKey->left = node->right ;
   lowerAsKey->right = node->left ;
   node->left  = keyroot.right ;
   node->right = keyroot.left ;

   return 0 ;
}

int insert_splaytree( splaytree_t * tree, const void * new_key, splaytree_node_t * new_node, const splaytree_compare_t * compare_callback)
{
   int err ;

   if ( ! tree->root ) {

      new_node->left  = 0 ;
      new_node->right = 0 ;

   } else {

      err = splay_key( tree, new_key, compare_callback) ;
      if (err) goto ABBRUCH ;

      splaytree_node_t * const root = tree->root ;
      const int                 cmp = compare_callback->fct(compare_callback->cb_param, new_key, root) ;

      if (cmp == 0)  {
         return EEXIST ;
      }

      if ( cmp < 0 ) {

         new_node->left  = root->left ;
         new_node->right = root ;
         root->left = 0 ;

      } else {

         new_node->right = root->right ;
         new_node->left  = root ;
         root->right = 0 ;

      }
   }

   tree->root = new_node ;
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int remove_splaytree( splaytree_t * tree, const void * key, /*out*/splaytree_node_t ** removed_node, const splaytree_compare_t * compare_callback )
{
   if (!tree->root) {
      return ESRCH ;
   }

   int err = splay_key( tree, key, compare_callback ) ;
   if (err) goto ABBRUCH ;

   splaytree_node_t * const root  = tree->root ;
   if (0 != compare_callback->fct(compare_callback->cb_param, key, root)) {
      return ESRCH ;
   }

   if (! root->left) {

      tree->root = root->right ;

   } else if (! root->right) {

      tree->root = root->left ;

   } else {

      splaytree_node_t * node = root->right ;
      if (!node->left) {
         node->left = root->left ;
      } else {
         splaytree_node_t * parent ;
         do {
            parent = node ;
            node   = node->left ;
         } while (node->left) ;
         parent->left = node->right ;
         node->left  = root->left ;
         node->right = root->right ;
      }
      tree->root = node ;
   }

   root->left  = 0 ;
   root->right = 0 ;
   *removed_node = root ;
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int updatekey_splaytree( splaytree_t * tree, const void * old_key, const void * new_key, const splaytree_update_key_t * update_key, const splaytree_compare_t * compare_callback )
{
   if (!tree->root) {
      return ESRCH ;
   }

   int err = splay_key( tree, old_key, compare_callback) ;
   if (err) goto ABBRUCH ;

   splaytree_node_t * const root  = tree->root ;
   if (0 != compare_callback->fct(compare_callback->cb_param, old_key, root)) {
      return ESRCH ;
   }

   err = update_key->fct(update_key->cb_param, new_key, root) ;
   if (err) {
      LOG_CALLERR("splaytree_update_key_t callback", err) ;
      goto ABBRUCH ;    // update failed => nothing done => return
   }

   // remove node
   if (! root->left) {

      tree->root = root->right ;

   } else if (! root->right) {

      tree->root = root->left ;

   } else {

      splaytree_node_t * node = root->right ;
      if (!node->left) {
         node->left = root->left ;
      } else {
         splaytree_node_t * parent ;
         do {
            parent = node ;
            node   = node->left ;
         } while (node->left) ;
         parent->left = node->right ;
         node->left  = root->left ;
         node->right = root->right ;
      }
      tree->root = node ;
   }

   root->left  = 0 ;
   root->right = 0 ;

   // insert node
   err = insert_splaytree(tree, new_key, root, compare_callback) ;
   if (err) {
      int err2 = update_key->fct(update_key->cb_param, old_key, root) ;
      assert(!err2) ;
      err2 = insert_splaytree(tree, old_key, root, compare_callback) ;
      assert(!err2) ;
      goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int freenodes_splaytree( splaytree_t * tree, const splaytree_free_t * removed_callback )
{
   int err ;
   splaytree_node_t * parent = 0 ;
   splaytree_node_t   * node = tree->root ;

   tree->root = 0 ;

   if (     node
         && removed_callback ) {

      err = 0 ;

      for (;;) {
         while ( node->left ) {
            splaytree_node_t * nodeleft = node->left ;
            node->left = parent ;
            parent = node ;
            node   = nodeleft ;
         }
         if (node->right) {
            splaytree_node_t * noderight = node->right ;
            node->left = parent ;
            parent = node ;
            node   = noderight ;
         } else {
            assert( node->left == 0 && node->right == 0 ) ;
            int err2 = removed_callback->fct(removed_callback->cb_param, node) ;
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

      if (err) goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int find_splaytree( splaytree_t * tree, const void * key, /*out*/splaytree_node_t ** found_node, const splaytree_compare_t * compare_callback )
{
   if (!tree->root) {
      return ESRCH ;
   }

   int err = splay_key( tree, key, compare_callback) ;
   if (err) goto ABBRUCH ;

   if (0 != compare_callback->fct(compare_callback->cb_param, key, tree->root)) {
      return ESRCH ;
   }

   *found_node = tree->root ;
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}


#ifdef KONFIG_UNITTEST

#define TEST(ARG) TEST_ONERROR_GOTO(ARG,unittest_os_index_splaytree,ABBRUCH)

typedef struct node_t node_t ;

struct node_t {
   splaytree_node_t   index ;
   int   key ;
   int   is_freed ;
   int   is_inserted ;
} ;

static int adapter_compare_key_node( callback_aspect_t * cb, const void * key_node1, const splaytree_node_t * node2)
{
   static_assert( sizeof(int) <= sizeof(void*), "integer key can be cast to void *" ) ;
   assert( (void*)17 == (void*)cb ) ;
   int key1 = (int) key_node1 ;
   int key2 = ((const node_t*)node2)->key ;
   return signum(key1 - key2) ;
}

static int adapter_compare_nodes( callback_aspect_t * cb, const splaytree_node_t * node1, const splaytree_node_t * node2)
{
   assert( (void*)14 == (void*)cb ) ;
   int key1 = ((const node_t*)node1)->key ;
   int key2 = ((const node_t*)node2)->key ;
   return signum(key1 - key2) ;
}

static int adapter_updatekey( callback_aspect_t * cb, const void * new_key, splaytree_node_t * node )
{
   assert( (void*)13 == (void*)cb ) ;
   ((node_t*)node)->key = (int)new_key ;
   return 0 ;
}

static int adapter_updatekey_enomem( callback_aspect_t * cb, const void * new_key, splaytree_node_t * node )
{
   assert( (void*)11 == (void*)cb ) ;
   (void) new_key ;
   (void) node ;
   return ENOMEM ;
}

static int freenode_count = 0 ;

static int adapter_freenode( callback_aspect_t * cb, splaytree_node_t * node )
{
   assert( (void*)9 == (void*)cb ) ;
   ++ freenode_count ;
   ((node_t*)node)->is_freed = 1 ;
   return 0 ;
}

static splaytree_node_t * build_perfect_tree( int count, node_t * nodes )
{
   assert( count > 0 && count < 10000 ) ;
   assert( 0 == ((count + 1) & count) ) ;   // count == (2^power)-1
   int root = (count + 1) / 2 ;
   if (root == 1) {
      nodes[root].index.left = nodes[root].index.right = 0 ;
   } else {
      splaytree_node_t * left  = build_perfect_tree( root - 1, nodes ) ;
      splaytree_node_t * right = build_perfect_tree( root - 1, nodes+root ) ;
      nodes[root].index.left  = left ;
      nodes[root].index.right = right ;
   }
   return &nodes[root].index ;
}

int unittest_os_index_splaytree()
{
   node_t                     nodes  [10000] ;
   node_t                     nodes2 [nrelementsof(nodes)] ;
   splaytree_t                tree             = splaytree_INIT(0) ;
   resourceusage_t            usage            = resourceusage_INIT_FREEABLE ;
   splaytree_free_t           free_cb          = { .fct = &adapter_freenode, .cb_param = (callback_aspect_t*)9 } ;
   splaytree_update_key_t     update_key_cb    = { .fct = &adapter_updatekey, .cb_param = (callback_aspect_t*)13 } ;
   splaytree_compare_nodes_t  compare_nodes_cb = { .fct = &adapter_compare_nodes, .cb_param = (callback_aspect_t*)14 } ;
   splaytree_compare_t        compare_cb       = { .fct = &adapter_compare_key_node, .cb_param = (callback_aspect_t*)17 } ;
   splaytree_node_t        *  treenode ;

   TEST(0 == init_resourceusage(&usage)) ;

   MEMSET0(&nodes) ;
   MEMSET0(&nodes2) ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      nodes[i].key  = (int)i ;
      nodes2[i].key = (int)i ;
   }

   // TEST init,free
   TEST(!tree.root) ;
   tree.root = (splaytree_node_t*)1 ;
   TEST(0 == init_splaytree( &tree )) ;
   TEST(!tree.root) ;
   TEST(0 == free_splaytree( &tree, &free_cb )) ;
   TEST(!tree.root) ;

   // TEST free_splaytree
   freenode_count = 0 ;
   tree.root      = build_perfect_tree( 7, nodes ) ;
   TEST( tree.root == &nodes[4].index ) ;
   TEST(0 == invariant_splaytree( &tree, &compare_nodes_cb )) ;
   TEST(0 == free_splaytree( &tree, &free_cb )) ;
   for( int i = 1; i <= 7; ++i) {
      TEST(ESRCH == find_splaytree( &tree, (void*)i, &treenode, &compare_cb)) ;
   }
   TEST(freenode_count == 7) ;
   TEST(0 == tree.root) ;
   for (int i = 1; i <= 7; ++i) {
      TEST(0 == nodes[i].index.left) ;
      TEST(0 == nodes[i].index.right) ;
      TEST(1 == nodes[i].is_freed) ;
      nodes[i].is_freed = 0 ;
   }
   TEST(0 == init_splaytree(&tree)) ;

   // TEST free_splaytree( NULL )
   freenode_count = 0 ;
   tree.root      = build_perfect_tree( 7, nodes ) ;
   TEST( tree.root == &nodes[4].index ) ;
   TEST(0 == free_splaytree( &tree, 0 )) ;
   TEST(0 == freenode_count) ;
   TEST(0 == tree.root) ;
   TEST(0 != nodes[4].index.left) ;
   TEST(0 != nodes[4].index.right) ;
   for (int i = 1; i <= 7; ++i) {
      TEST(0 == nodes[i].is_freed) ;
   }
   TEST(0 == init_splaytree(&tree)) ;

   // TEST invariant parent buffer allocation (depth is 10000)
   freenode_count = 0 ;
   tree.root      = &nodes[0].index ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      nodes[i].is_freed    = 0 ;
      nodes[i].index.left  = 0 ;
      nodes[i].index.right = &nodes[i+1].index ;
   }
   nodes[nrelementsof(nodes)-1].index.right = 0 ;
   TEST(0 == invariant_splaytree( &tree, &compare_nodes_cb)) ;
   TEST(0 == free_splaytree( &tree, &free_cb )) ;
   TEST(nrelementsof(nodes) == freenode_count) ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(nodes[i].is_freed    == 1) ;
      TEST(nodes[i].index.right == 0) ;
      nodes[i].is_freed = 0 ;
   }

   // TEST splay operation (compare it with simple_splay)
   for(int i = 0; i <= 1024; ++i) {
      tree.root = build_perfect_tree( 1023, nodes ) ;
      TEST( tree.root == &nodes[512].index ) ;
      simple_splay_key( &tree, tree.root, (void*)i, &compare_cb ) ;
      TEST( tree.root == &nodes[i?(i<1024?i:1023):1].index ) ;

      tree.root = build_perfect_tree( 1023, nodes2 ) ;
      TEST( tree.root == &nodes2[512].index ) ;
      TEST( 0         == splay_key( &tree, (void*)i, &compare_cb) ) ;
      TEST( tree.root == &nodes2[i?(i<1024?i:1023):1].index ) ;

      for(int i2 = 1; i2 < 1024; ++i2) {
         if (nodes[i2].index.left) {
            TEST( (nodes[i2].index.left - &nodes[0].index) == (nodes2[i2].index.left - &nodes2[0].index) ) ;
         }
         if (nodes[i2].index.right) {
            TEST( (nodes[i2].index.right - &nodes[0].index) == (nodes2[i2].index.right - &nodes2[0].index) ) ;
         }
      }
   }
   TEST(0 == init_splaytree(&tree)) ;

   // TEST insert,remove cycle
   TEST(!tree.root) ;
   TEST(0 == insert_splaytree( &tree, 0, &nodes[0].index, &compare_cb) ) ;
   TEST(tree.root == &nodes[0].index) ;
   TEST(0 == remove_splaytree( &tree, 0, &treenode, &compare_cb)) ;
   TEST(treenode == &nodes[0].index) ;
   TEST(!nodes[0].is_freed) ;
   TEST(!tree.root) ;

   TEST(!tree.root) ;
   TEST(0 == insert_splaytree( &tree, (void*)10, &nodes[10].index, &compare_cb )) ;
   TEST(tree.root == &nodes[10].index) ;
   TEST(0 == remove_splaytree( &tree, (void*)10, &treenode, &compare_cb)) ;
   TEST(treenode == &nodes[10].index) ;
   TEST(!nodes[10].is_freed) ;
   TEST(!tree.root) ;

   // TEST insert,free cycle (all nodes are freed)
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == insert_splaytree( &tree, (void*)nodes[i].key, &nodes[i].index, &compare_cb)) ;
      nodes[i].is_inserted = 1 ;
      if (!(i%100)) TEST(0 == invariant_splaytree( &tree, &compare_nodes_cb)) ;
   }
   TEST(0 == invariant_splaytree( &tree, &compare_nodes_cb)) ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == find_splaytree( &tree, (void*)nodes[i].key, &treenode, &compare_cb)) ;
      TEST(treenode == &nodes[i].index) ;
      TEST(nodes[i].is_inserted && !nodes[i].is_freed) ;
   }
   TEST(0 == free_splaytree( &tree, &free_cb )) ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(nodes[i].is_inserted && nodes[i].is_freed) ;
      nodes[i].is_freed    = 0 ;
      nodes[i].is_inserted = 0 ;
   }
   TEST(0 == init_splaytree(&tree)) ;
   for (int i = nrelementsof(nodes)-1; i >= 0; --i) {
      TEST(0 == insert_splaytree( &tree, (void*)nodes[i].key, &nodes[i].index, &compare_cb)) ;
      nodes[i].is_inserted = 1 ;
      if (!(i%100)) TEST(0 == invariant_splaytree( &tree, &compare_nodes_cb)) ;
   }
   TEST(0 == invariant_splaytree( &tree, &compare_nodes_cb)) ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == find_splaytree( &tree, (void*)nodes[i].key, &treenode, &compare_cb)) ;
      TEST(treenode == &nodes[i].index) ;
      TEST(nodes[i].is_inserted && !nodes[i].is_freed) ;
   }
   TEST(0 == free_splaytree( &tree, &free_cb )) ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(nodes[i].is_inserted && nodes[i].is_freed) ;
      nodes[i].is_freed    = 0 ;
      nodes[i].is_inserted = 0 ;
   }
   TEST(0 == init_splaytree(&tree)) ;

   // TEST insert,remove
   srand(100) ;
   for (unsigned i = 0; i < 10*nrelementsof(nodes); ++i) {
      int id = rand() % (int)nrelementsof(nodes) ;
      if (nodes[id].is_inserted) continue ;
      TEST(0 == insert_splaytree( &tree, (void*)nodes[id].key, &nodes[id].index, &compare_cb )) ;
      nodes[id].is_inserted = 1 ;
   }
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      if (nodes[i].is_inserted) continue ;
      TEST(0 == insert_splaytree( &tree, (void*)nodes[i].key, &nodes[i].index, &compare_cb )) ;
      nodes[i].is_inserted = 1 ;
   }
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == find_splaytree( &tree, (void*)nodes[i].key, &treenode, &compare_cb)) ;
      TEST(treenode == &nodes[i].index) ;
   }
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == remove_splaytree( &tree, (void*)nodes[i].key, &treenode, &compare_cb)) ;
      TEST(treenode == &nodes[i].index) ;
      TEST(ESRCH == find_splaytree( &tree, (void*)nodes[i].key, &treenode, &compare_cb)) ;
      if (!(i%100)) TEST(0 == invariant_splaytree( &tree, &compare_nodes_cb)) ;
      TEST(nodes[i].is_inserted) ;
      nodes[i].is_inserted = 0 ;
   }
   for (unsigned i = 0; i < 10*nrelementsof(nodes); ++i) {
      int id = rand() % (int)nrelementsof(nodes) ;
      if (nodes[id].is_inserted) continue ;
      TEST(0 == insert_splaytree( &tree, (void*)nodes[id].key, &nodes[id].index, &compare_cb)) ;
      nodes[id].is_inserted = 1 ;
   }
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      if (nodes[i].is_inserted) continue ;
      TEST(0 == insert_splaytree( &tree, (void*)nodes[i].key, &nodes[i].index, &compare_cb)) ;
      nodes[i].is_inserted = 1 ;
   }
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == find_splaytree( &tree, (void*)nodes[i].key, &treenode, &compare_cb)) ;
      TEST(treenode == &nodes[i].index) ;
      TEST(!nodes[i].is_freed) ;
   }
   for (unsigned i = 0; i < 10*nrelementsof(nodes); ++i) {
      int id = rand() % (int)nrelementsof(nodes) ;
      if (!nodes[id].is_inserted) continue ;
      nodes[id].is_inserted = 0 ;
      TEST(0 == remove_splaytree( &tree, (void*)nodes[id].key, &treenode, &compare_cb)) ;
      TEST(&nodes[id].index == treenode) ;
      TEST(ESRCH == find_splaytree( &tree, (void*)nodes[id].key, &treenode, &compare_cb)) ;
      if (!(i%100)) TEST(0 == invariant_splaytree( &tree, &compare_nodes_cb)) ;
   }
   for (int i = nrelementsof(nodes)-1; i >= 0; --i) {
      if (nodes[i].is_inserted) {
         nodes[i].is_inserted = 0 ;
         TEST(0 == remove_splaytree( &tree, (void*)nodes[i].key, &treenode, &compare_cb)) ;
         TEST(&nodes[i].index == treenode) ;
         TEST(ESRCH == find_splaytree( &tree, (void*)nodes[i].key, &treenode, &compare_cb)) ;
      }
      if (!(i%100)) TEST(0 == invariant_splaytree( &tree, &compare_nodes_cb)) ;
      TEST(! nodes[i].is_freed) ;
      nodes[i].is_freed = 0 ;
   }
   TEST(! tree.root) ;

   // TEST updatekey
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(0 == insert_splaytree( &tree, (void*)nodes[i].key, &nodes[i].index, &compare_cb )) ;
   }
   for (int i = nrelementsof(nodes)-1; i >= 0; --i) {
      TEST(0 == updatekey_splaytree( &tree, (void*)i, (void*)(1+i), &update_key_cb, &compare_cb )) ;
   }
   for (int i = nrelementsof(nodes)-1; i >= 0; --i) {
      TEST(0 == find_splaytree( &tree, (void*)(1+i), &treenode, &compare_cb )) ;
      TEST(&nodes[i].index == treenode) ;
   }
   for (int i = nrelementsof(nodes)-1; i >= 0; --i) {
      TEST(nodes[i].key == (i+1)) ;
      nodes[i].key      = i ;
   }
   for (int i = nrelementsof(nodes)-1; i >= 0; --i) {
      TEST(0 == find_splaytree( &tree, (void*)i, &treenode, &compare_cb )) ;
      TEST(&nodes[i].index == treenode) ;
   }

   // TEST updatekey returns error ENOMEM
   for (unsigned i = nrelementsof(nodes)-1; i >= nrelementsof(nodes)-5; --i) {
      splaytree_update_key_t enomem_cb = { .fct = &adapter_updatekey_enomem, .cb_param = (callback_aspect_t*)11 } ;
      TEST(ENOMEM == updatekey_splaytree( &tree, (void*)i, (void*)(1+i), &enomem_cb, &compare_cb )) ;
      TEST(nodes[i].key == (int)i) ;
   }
   for (int i = nrelementsof(nodes)-1; i >= 0; --i) {
      TEST(0 == find_splaytree( &tree, (void*)i, &treenode, &compare_cb )) ;
      TEST(&nodes[i].index == treenode) ;
   }

   // TEST freenodes
   freenode_count = 0 ;
   TEST(0 == freenodes_splaytree( &tree, &free_cb)) ;
   TEST(nrelementsof(nodes) == freenode_count) ;
   for (unsigned i = 0; i < nrelementsof(nodes); ++i) {
      TEST(nodes[i].is_freed) ;
      nodes[i].is_freed = 0 ;
   }

   // TEST EEXIST
   freenode_count = 0 ;
   TEST(0 == insert_splaytree( &tree, (void*)1, &nodes[1].index, &compare_cb )) ;
   TEST(0 == insert_splaytree( &tree, (void*)2, &nodes[2].index, &compare_cb )) ;
   TEST(0 == insert_splaytree( &tree, (void*)3, &nodes[3].index, &compare_cb )) ;
   TEST(EEXIST == updatekey_splaytree( &tree, (void*)1, (void*)3, &update_key_cb, &compare_cb )) ;
   TEST(EEXIST == updatekey_splaytree( &tree, (void*)2, (void*)1, &update_key_cb, &compare_cb )) ;
   TEST(EEXIST == updatekey_splaytree( &tree, (void*)3, (void*)2, &update_key_cb, &compare_cb )) ;
   TEST(EEXIST == insert_splaytree( &tree, (void*)1, &nodes[1].index, &compare_cb )) ;
   TEST(EEXIST == insert_splaytree( &tree, (void*)2, &nodes[2].index, &compare_cb )) ;
   TEST(EEXIST == insert_splaytree( &tree, (void*)3, &nodes[3].index, &compare_cb )) ;
   TEST(0 == freenode_count) ;
   TEST(0 == freenodes_splaytree( &tree, &free_cb)) ;
   TEST(3 == freenode_count) ;
   for (int i = 1; i < 4; ++i) {
      TEST(nodes[i].is_freed) ;
      nodes[i].is_freed = 0 ;
   }

   // TEST ESRCH
   TEST(0 == insert_splaytree( &tree, (void*)1, &nodes[1].index, &compare_cb )) ;
   TEST(0 == insert_splaytree( &tree, (void*)2, &nodes[2].index, &compare_cb )) ;
   TEST(0 == insert_splaytree( &tree, (void*)3, &nodes[3].index, &compare_cb )) ;
   TEST(ESRCH == find_splaytree( &tree, (void*)4, &treenode, &compare_cb )) ;
   TEST(ESRCH == remove_splaytree( &tree, (void*)5, &treenode, &compare_cb )) ;
   TEST(ESRCH == updatekey_splaytree( &tree, (void*)6, (void*)7, &update_key_cb, &compare_cb )) ;

   // TEST all resources are freed
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
