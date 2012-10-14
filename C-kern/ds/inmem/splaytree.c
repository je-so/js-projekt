/* title: Splaytree impl

   Implements <Splaytree>.

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

   file: C-kern/api/ds/inmem/splaytree.h
    Header file of <Splaytree>.

   file: C-kern/ds/inmem/splaytree.c
    Implementation file of <Splaytree impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/ds/typeadapt.h"
#include "C-kern/api/ds/inmem/binarystack.h"
#include "C-kern/api/ds/inmem/splaytree.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/ds/foreach.h"
#include "C-kern/api/math/int/sign.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#include "C-kern/api/test/errortimer.h"
#endif


// section: splaytree_t

// group: helper

/* define: KEYCOMPARE
 * Casts node to type <typeadapt_object_t> and calls <callcmpkeyobj_typeadapt>.
 * This macro expects variable name tree to point to <splaytree_t>. */
#define KEYCOMPARE(key,node)  callcmpkeyobj_typeadaptmember(&tree->nodeadp, key, memberasobject_typeadaptmember(&tree->nodeadp, node))

/* define: OBJCOMPARE
 * Casts node to type <typeadapt_object_t> and calls <callcmpobj_typeadapt>.
 * This macro expects variable name tree to point to <splaytree_t>. */
#define OBJCOMPARE(keyobject,node)  callcmpobj_typeadaptmember(&tree->nodeadp, keyobject, memberasobject_typeadaptmember(&tree->nodeadp, node))

// group: test

/* function: invariant_splaytree
 * Checks that nodes of left subtree are smaller and nodes in right subtree are higher than root node.
 * The check is done recursively. */
int invariant_splaytree(splaytree_t * tree)
{
   int err ;

   struct state_t {
      const splaytree_node_t   * parent ;
      const typeadapt_object_t * lowerbound ;
      const typeadapt_object_t * upperbound ;
   } ;

   binarystack_t  parents = binarystack_INIT_FREEABLE ;
   struct state_t * state ;
   struct state_t * prevstate ;

   if (tree->root) {

      err = init_binarystack(&parents, 64*sizeof(struct state_t)) ;
      if (err) goto ONABORT ;

      err = push_binarystack(&parents, &state) ;
      if (err) goto ONABORT ;

      *state = (struct state_t) { .parent = tree->root, .lowerbound = 0, .upperbound = 0 } ;

      do {

         const splaytree_node_t  * leftchild  = state->parent->left ;
         const splaytree_node_t  * rightchild = state->parent->right ;
         typeadapt_object_t      * stateobj   = memberasobject_typeadaptmember(&tree->nodeadp, state->parent) ;

         if (  (  state->lowerbound
                  && callcmpobj_typeadaptmember(&tree->nodeadp, state->lowerbound, stateobj) >= 0)
               || (  state->upperbound
                     && callcmpobj_typeadaptmember(&tree->nodeadp, state->upperbound, stateobj) <= 0)) {
            err = EINVAL ;
            goto ONABORT ;
         }

         if (  leftchild
               || rightchild) {

            prevstate = state ;

            err = push_binarystack(&parents, &state) ;
            if (err) goto ONABORT ;

            if (leftchild) {
               state->parent     = leftchild ;
               state->lowerbound = prevstate->lowerbound ;
               state->upperbound = stateobj ;
            } else {
               state->parent     = rightchild ;
               state->lowerbound = stateobj ;
               state->upperbound = prevstate->upperbound ;
            }

         } else {
            for (;;) {
               const splaytree_node_t * child = state->parent ;

               err = pop_binarystack(&parents, sizeof(struct state_t)) ;
               if (err) goto ONABORT ;

               if (isempty_binarystack(&parents)) break ;

               state = top_binarystack(&parents) ;
               assert(state->parent->right == child || state->parent->left == child) ;

               if (state->parent->left == child && state->parent->right) {
                  prevstate = state ;
                  err = push_binarystack(&parents, &state) ;
                  if (err) goto ONABORT ;
                  state->parent     = prevstate->parent->right ;
                  state->lowerbound = memberasobject_typeadaptmember(&tree->nodeadp, prevstate->parent) ;
                  state->upperbound = prevstate->upperbound ;
                  break ;
               }
            }
         }

      } while (!isempty_binarystack(&parents)) ;
   }

   err = free_binarystack(&parents) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   (void) free_binarystack(&parents) ;
   return err ;
}

// group: lifetime

int free_splaytree(splaytree_t * tree)
{
   int err = removenodes_splaytree(tree) ;

   tree->nodeadp = (typeadapt_member_t) typeadapt_member_INIT_FREEABLE ;

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

// group: change

#ifdef KONFIG_UNITTEST
/* function: recursivesplay_splaytree
 * Searches for a node X with key 'key' and makes it the new root of the tree.
 * If the node is not in the tree the last node (with no left or right child)
 * is used instead. The rotations done to move X to the top are called 'splaying'.
 * See http://en.wikipedia.org/wiki/Splay_tree for a description of splay trees.
 *
 * This is a straight forward recursive implementation and it used only for test purposes.
 */
static splaytree_node_t * recursivesplay_splaytree(splaytree_t * tree, splaytree_node_t * root, const void * key)
{
   assert(root) ;
   splaytree_node_t * Xroot = root ;

   if (KEYCOMPARE(key, root) > 0 && root->right) {
      if (KEYCOMPARE(key, root->right) > 0 && root->right->right) {
         Xroot = recursivesplay_splaytree(tree, root->right->right, key) ;
         root->right->right = Xroot->left ;
         Xroot->left = root->right ;
         root->right = Xroot->left->left ;
         Xroot->left->left = root ;
      } else if (KEYCOMPARE(key, root->right) < 0 && root->right->left) {
         Xroot = recursivesplay_splaytree(tree, root->right->left, key) ;
         root->right->left = Xroot->right ;
         Xroot->right = root->right ;
         root->right  = Xroot->left ;
         Xroot->left  = root ;
      } else {
         Xroot = root->right ;
         root->right = Xroot->left ;
         Xroot->left = root ;
      }
   } else if (KEYCOMPARE(key, root) < 0 && root->left) {
      if (KEYCOMPARE(key, root->left) < 0 && root->left->left) {
         Xroot = recursivesplay_splaytree(tree, root->left->left, key) ;
         root->left->left = Xroot->right ;
         Xroot->right = root->left ;
         root->left   = Xroot->right->right ;
         Xroot->right->right = root ;
      } else if (KEYCOMPARE(key, root->left) > 0 && root->left->right) {
         Xroot = recursivesplay_splaytree(tree, root->left->right, key) ;
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

/* function: splay_splaytree
 * Searches for a node X with key 'key' and makes it the new root of the tree.
 * If the node is not in the tree the last node (with no left or right child)
 * is used instead. The rotations done to move X to the top are called 'splaying'.
 * See http://en.wikipedia.org/wiki/Splay_tree for a description of splay trees.
 *
 * This version splays from top to down without recursion - it is the production version.
 * The result is compared with <recursivesplay_splaytree>.
 */
static int splay_splaytree(splaytree_t * tree, const void * key)
{
   splaytree_node_t        keyroot = { .left = 0, .right = 0 } ;
   splaytree_node_t        * higherAsKey ;
   splaytree_node_t        * lowerAsKey ;

   higherAsKey = &keyroot ;
   lowerAsKey  = &keyroot ;

   splaytree_node_t * node = tree->root ;
   assert(node) ;

   int cmp = KEYCOMPARE(key, node) ;
   for (;;) {
      if (cmp > 0) {

         splaytree_node_t * rightnode = node->right ;
         if (!rightnode) break ;

         cmp = KEYCOMPARE(key, rightnode) ;
         if (cmp > 0 && rightnode->right) {
            node->right = rightnode->left ;
            rightnode->left = node ;
            node = rightnode ;
            rightnode = node->right ;
            cmp = KEYCOMPARE(key, rightnode) ;
         } else if (cmp < 0 && rightnode->left) {
            higherAsKey->left = rightnode ;
            higherAsKey = rightnode ;
            rightnode = rightnode->left ;
            cmp = KEYCOMPARE(key, rightnode) ;
         }
         lowerAsKey->right = node ;
         lowerAsKey = node ;
         node = rightnode ;

      } else if (cmp < 0) {

         splaytree_node_t * leftnode = node->left ;
         if (!leftnode) break ;

         cmp = KEYCOMPARE(key, leftnode) ;
         if (cmp < 0 && leftnode->left) {
            node->left = leftnode->right ;
            leftnode->right = node ;
            node = leftnode ;
            leftnode = node->left ;
            cmp = KEYCOMPARE(key, leftnode) ;
         } else if (cmp > 0 && leftnode->right) {
            lowerAsKey->right = leftnode ;
            lowerAsKey = leftnode ;
            leftnode = leftnode->right ;
            cmp = KEYCOMPARE(key, leftnode) ;
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

/* function: splay2_splaytree
 * Same as <splay_splaytree> except key is an object. */
static int splay2_splaytree(splaytree_t * tree, const splaytree_node_t * keynode)
{
   typeadapt_object_t      * keyobject = memberasobject_typeadaptmember(&tree->nodeadp, keynode) ;
   splaytree_node_t        keyroot = { .left = 0, .right = 0 } ;
   splaytree_node_t        * higherAsKey ;
   splaytree_node_t        * lowerAsKey ;

   higherAsKey = &keyroot ;
   lowerAsKey  = &keyroot ;

   splaytree_node_t * node = tree->root ;
   assert(node) ;

   int cmp = OBJCOMPARE(keyobject, node) ;
   for (;;) {
      if (cmp > 0) {

         splaytree_node_t * rightnode = node->right ;
         if (!rightnode) break ;

         cmp = OBJCOMPARE(keyobject, rightnode) ;
         if (cmp > 0 && rightnode->right) {
            node->right = rightnode->left ;
            rightnode->left = node ;
            node = rightnode ;
            rightnode = node->right ;
            cmp = OBJCOMPARE(keyobject, rightnode) ;
         } else if (cmp < 0 && rightnode->left) {
            higherAsKey->left = rightnode ;
            higherAsKey = rightnode ;
            rightnode = rightnode->left ;
            cmp = OBJCOMPARE(keyobject, rightnode) ;
         }
         lowerAsKey->right = node ;
         lowerAsKey = node ;
         node = rightnode ;

      } else if (cmp < 0) {

         splaytree_node_t * leftnode = node->left ;
         if (!leftnode) break ;

         cmp = OBJCOMPARE(keyobject, leftnode) ;
         if (cmp < 0 && leftnode->left) {
            node->left = leftnode->right ;
            leftnode->right = node ;
            node = leftnode ;
            leftnode = node->left ;
            cmp = OBJCOMPARE(keyobject, leftnode) ;
         } else if (cmp > 0 && leftnode->right) {
            lowerAsKey->right = leftnode ;
            lowerAsKey = leftnode ;
            leftnode = leftnode->right ;
            cmp = OBJCOMPARE(keyobject, leftnode) ;
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

int insert_splaytree(splaytree_t * tree, const void * new_key, splaytree_node_t * new_node)
{
   int err ;

   if (! tree->root) {

      new_node->left  = 0 ;
      new_node->right = 0 ;

   } else {

      err = splay_splaytree(tree, new_key) ;
      if (err) goto ONABORT ;

      splaytree_node_t * const root = tree->root ;
      const int                 cmp = KEYCOMPARE(new_key, root) ;

      if (cmp == 0)  {
         return EEXIST ;
      }

      if (cmp < 0) {

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
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int remove_splaytree(splaytree_t * tree, const void * key, /*out*/splaytree_node_t ** removed_node)
{
   if (!tree->root) {
      return ESRCH ;
   }

   int err = splay_splaytree(tree, key) ;
   if (err) goto ONABORT ;

   splaytree_node_t * const root = tree->root ;
   if (0 != KEYCOMPARE(key, root)) {
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
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int removenodes_splaytree(splaytree_t * tree)
{
   int err ;
   splaytree_node_t  * parent = 0 ;
   splaytree_node_t  * node   = tree->root ;

   tree->root = 0 ;

   if (node) {

      const bool isDeleteObject = islifetimedelete_typeadapt(tree->nodeadp.typeadp) ;

      err = 0 ;

      do {
         while (node->left) {
            splaytree_node_t * nodeleft = node->left ;
            node->left = parent ;
            parent = node ;
            node   = nodeleft ;
         }
         splaytree_node_t * delnode = node ;
         if (delnode->right) {
            node = delnode->right ;
            delnode->right = 0 ;
         } else {
            node = parent ;
            if (node) {
               parent = node->left ;
               node->left = 0 ;
            }
         }

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
   TRACEABORT_LOG(err) ;
   return err ;
}

// group: search

int find_splaytree(splaytree_t * tree, const void * key, /*out*/splaytree_node_t ** found_node)
{
   if (!tree->root) {
      return ESRCH ;
   }

   int err = splay_splaytree(tree, key) ;
   if (err) goto ONABORT ;

   if (0 != KEYCOMPARE(key, tree->root)) {
      return ESRCH ;
   }

   *found_node = tree->root ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

// group: iterate

int initfirst_splaytreeiterator(/*out*/splaytree_iterator_t * iter, splaytree_t * tree)
{
   splaytree_node_t * node = tree->root ;

   if (node) {
      while (node->left) {
         node = node->left ;
      }
   }

   iter->next = node ;
   return 0 ;
}

int initlast_splaytreeiterator(/*out*/splaytree_iterator_t * iter, splaytree_t * tree)
{
   splaytree_node_t * node = tree->root ;

   if (node) {
      while (node->right) {
         node = node->right ;
      }
   }

   iter->next = node ;
   return 0 ;
}

bool next_splaytreeiterator(splaytree_iterator_t * iter, splaytree_t * tree, /*out*/splaytree_node_t ** node)
{
   if (  !iter->next
         || splay2_splaytree(tree, iter->next)) {
      return false ;
   }

   *node = tree->root ;

   // search next higher
   splaytree_node_t * next = tree->root->right ;

   if (next) {
      while (next->left) {
         next = next->left ;
      }
   }

   iter->next = next ;

   return true ;
}

bool prev_splaytreeiterator(splaytree_iterator_t * iter, splaytree_t * tree, /*out*/splaytree_node_t ** node)
{
   if (  !iter->next
         || splay2_splaytree(tree, iter->next)) {
      return false ;
   }

   *node = tree->root ;

   // search next lower
   splaytree_node_t * next = tree->root->left ;

   if (next) {
      while (next->right) {
         next = next->right ;
      }
   }

   iter->next = next ;

   return true ;
}


// group: test

#ifdef KONFIG_UNITTEST

typedef struct testadapt_t             testadapt_t ;
typedef struct testnode_t              testnode_t ;

struct testnode_t {
   int   is_freed ;
   int   is_inserted ;
   splaytree_node_t   index ;
   int   key ;
} ;

typedef testnode_t                     nodesarray_t[10000] ;

struct testadapt_t {
   struct {
      typeadapt_EMBED(testadapt_t, testnode_t, int) ;
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

static int impl_cmpkeyobj_testadapt(testadapt_t * testadp, const int * key, const testnode_t * rnode)
{
   (void) testadp ;
   static_assert(sizeof(int) <= sizeof(int*), "(int) cast possible") ;
   int lkey = (int)key ;
   int rkey = rnode->key ;
   return sign_int(lkey - rkey) ;
}

static int impl_cmpobj_testadapt(testadapt_t * testadp, const testnode_t * lnode, const testnode_t * rnode)
{
   (void) testadp ;
   int lkey = lnode->key ;
   int rkey = rnode->key ;
   return sign_int(lkey - rkey) ;
}

static splaytree_node_t * build_perfect_tree(int count, testnode_t * nodes)
{
   assert(count > 0 && count < 10000) ;
   assert(0 == ((count + 1) & count)) ;   // count == (2^power)-1
   int root = (count + 1) / 2 ;
   if (root == 1) {
      nodes[root].index.left = nodes[root].index.right = 0 ;
   } else {
      splaytree_node_t * left  = build_perfect_tree(root - 1, nodes) ;
      splaytree_node_t * right = build_perfect_tree(root - 1, nodes+root) ;
      nodes[root].index.left  = left ;
      nodes[root].index.right = right ;
   }
   return &nodes[root].index ;
}

static int test_initfree(void)
{
   testadapt_t                typeadapt = {
      typeadapt_INIT_LIFEKEYCMP(0, &impl_deletenode_testadapt, &impl_cmpkeyobj_testadapt, &impl_cmpobj_testadapt),
      test_errortimer_INIT_FREEABLE, 0
   } ;
   typeadapt_member_t         nodeadapt = typeadapt_member_INIT(asgeneric_typeadapt(&typeadapt, testadapt_t, testnode_t, void), offsetof(testnode_t, index)) ;
   memblock_t                 memblock1 = memblock_INIT_FREEABLE ;
   splaytree_t                tree      = splaytree_INIT_FREEABLE ;
   nodesarray_t               * nodes1 ;
   lrtree_node_t              emptynode = lrtree_node_INIT ;

   // prepare
   TEST(0 == MM_RESIZE(sizeof(nodesarray_t), &memblock1)) ;
   nodes1 = (nodesarray_t*)memblock1.addr ;
   MEMSET0(nodes1) ;
   for (unsigned i = 0; i < nrelementsof(*nodes1); ++i) {
      (*nodes1)[i].key = (int)i ;
   }

   // TEST lrtree_node_INIT
   TEST(0 == emptynode.left) ;
   TEST(0 == emptynode.right) ;

   // TEST splaytree_INIT_FREEABLE
   TEST(tree.root    == 0) ;
   TEST(tree.nodeadp.typeadp == 0) ;
   TEST(tree.nodeadp.typeinfo.memberoffset == 0) ;
   TEST(isempty_splaytree(&tree)) ;

   // TEST splaytree_INIT
   tree = (splaytree_t) splaytree_INIT((splaytree_node_t*)1, typeadapt_member_INIT((void*)2, 3)) ;
   TEST(tree.root == (splaytree_node_t*)1) ;
   TEST(tree.nodeadp.typeadp == (typeadapt_t*)2) ;
   TEST(tree.nodeadp.typeinfo.memberoffset == 3) ;
   TEST(! isempty_splaytree(&tree)) ;
   tree = (splaytree_t) splaytree_INIT((splaytree_node_t*)0, typeadapt_member_INIT(0,0)) ;
   TEST(tree.root == 0) ;
   TEST(tree.nodeadp.typeadp == 0) ;
   TEST(tree.nodeadp.typeinfo.memberoffset == 0) ;

   // TEST init_splaytree, double free_splaytree
   tree = (splaytree_t) splaytree_INIT((splaytree_node_t*)1, typeadapt_member_INIT((void*)2, 3)) ;
   init_splaytree(&tree, &nodeadapt) ;
   TEST(tree.root == 0) ;
   TEST(tree.nodeadp.typeadp == nodeadapt.typeadp) ;
   TEST(tree.nodeadp.typeinfo.memberoffset == nodeadapt.typeinfo.memberoffset) ;
   TEST(0 == free_splaytree(&tree)) ;
   TEST(tree.root == 0) ;
   TEST(tree.nodeadp.typeadp == 0) ;
   TEST(tree.nodeadp.typeinfo.memberoffset == 0) ;
   TEST(0 == free_splaytree(&tree)) ;
   TEST(tree.root == 0) ;

   // TEST free_splaytree
   init_splaytree(&tree, &nodeadapt) ;
   typeadapt.freenode_count = 0 ;
   tree.root      = build_perfect_tree(7, *nodes1) ;
   TEST(tree.root == &(*nodes1)[4].index) ;
   TEST(0 == invariant_splaytree(&tree)) ;
   TEST(0 == free_splaytree(&tree)) ;
   TEST(7 == typeadapt.freenode_count) ;
   TEST(0 == tree.root) ;
   for (int i = 1; i <= 7; ++i) {
      TEST(0 == (*nodes1)[i].index.left) ;
      TEST(0 == (*nodes1)[i].index.right) ;
      TEST(1 == (*nodes1)[i].is_freed) ;
      (*nodes1)[i].is_freed = 0 ;
   }

   // TEST free_splaytree: no delete called if typeadapt_t.lifetime.delete_object set to 0
   init_splaytree(&tree, &nodeadapt) ;
   testadapt_t  old_typeadapt = typeadapt ;
   typeadapt.lifetime.delete_object = 0 ;
   typeadapt.freenode_count = 0 ;
   tree.root      = build_perfect_tree(31, (*nodes1)) ;
   TEST(tree.root == &(*nodes1)[16].index) ;
   TEST(0 == free_splaytree(&tree)) ;
   TEST(0 == typeadapt.freenode_count) ;
   TEST(0 == tree.root) ;
   for (int i = 1; i <= 31; ++i) {
      TEST(0 == (*nodes1)[i].index.left) ;
      TEST(0 == (*nodes1)[i].index.right) ;
      TEST(0 == (*nodes1)[i].is_freed) ;
   }
   typeadapt = old_typeadapt ;

   // TEST free_splaytree: ERROR
   init_splaytree(&tree, &nodeadapt) ;
   typeadapt.freenode_count = 0 ;
   init_testerrortimer(&typeadapt.errcounter, 32, EMFILE) ;
   tree.root      = build_perfect_tree(63, *nodes1) ;
   TEST(tree.root == &(*nodes1)[32].index) ;
   TEST(0 == invariant_splaytree(&tree)) ;
   TEST(EMFILE == free_splaytree(&tree)) ;
   TEST(62 == typeadapt.freenode_count) ;
   TEST(0 == tree.root) ;
   for (int i = 1; i <= 63; ++i) {
      TEST(0 == (*nodes1)[i].index.left) ;
      TEST(0 == (*nodes1)[i].index.right) ;
      TEST((i != 32) == (*nodes1)[i].is_freed) ;
      (*nodes1)[i].is_freed = 0 ;
   }

   // TEST getinistate_splaytree
   splaytree_node_t * saved_root      = (void*) 1 ;
   typeadapt_member_t saved_nodeadapt = nodeadapt ;
   typeadapt_member_t empty_nodeadapt = typeadapt_member_INIT_FREEABLE ;
   tree = (splaytree_t)splaytree_INIT_FREEABLE ;
   getinistate_splaytree(&tree, &saved_root, 0) ;
   TEST(0 == saved_root) ;
   getinistate_splaytree(&tree, &saved_root, &saved_nodeadapt) ;
   TEST(0 == saved_root) ;
   TEST(1 == isequal_typeadaptmember(&saved_nodeadapt, &empty_nodeadapt)) ;
   tree = (splaytree_t)splaytree_INIT(&(*nodes1)[0].index, nodeadapt) ;
   getinistate_splaytree(&tree, &saved_root, 0) ;
   TEST(&(*nodes1)[0].index == saved_root) ;
   getinistate_splaytree(&tree, &saved_root, &saved_nodeadapt) ;
   TEST(&(*nodes1)[0].index == saved_root) ;
   TEST(1 == isequal_typeadaptmember(&saved_nodeadapt, &nodeadapt)) ;

   // TEST isempty_splaytree
   tree = (splaytree_t)splaytree_INIT_FREEABLE ;
   TEST(isempty_splaytree(&tree)) ;
   tree.root = (void*)1 ;
   TEST(! isempty_splaytree(&tree)) ;

   // unprepare
   TEST(0 == MM_FREE(&memblock1)) ;
   nodes1 = 0 ;

   return 0 ;
ONABORT:
   MM_FREE(&memblock1) ;
   return EINVAL ;
}

static int test_insertremove(void)
{
   testadapt_t                typeadapt = {
      typeadapt_INIT_LIFEKEYCMP(0, &impl_deletenode_testadapt, &impl_cmpkeyobj_testadapt, &impl_cmpobj_testadapt),
      test_errortimer_INIT_FREEABLE, 0
   } ;
   typeadapt_member_t         nodeadapt = typeadapt_member_INIT(asgeneric_typeadapt(&typeadapt, testadapt_t, testnode_t, void), offsetof(testnode_t, index)) ;
   memblock_t                 memblock1 = memblock_INIT_FREEABLE ;
   memblock_t                 memblock2 = memblock_INIT_FREEABLE ;
   splaytree_t                tree      = splaytree_INIT(0, nodeadapt) ;
   nodesarray_t               * nodes1 ;
   nodesarray_t               * nodes2 ;
   splaytree_node_t           * treenode ;

   // prepare
   TEST(0 == MM_RESIZE(sizeof(nodesarray_t), &memblock1)) ;
   TEST(0 == MM_RESIZE(sizeof(nodesarray_t), &memblock2)) ;
   nodes1 = (nodesarray_t*)memblock1.addr ;
   nodes2 = (nodesarray_t*)memblock2.addr ;
   MEMSET0(nodes1) ;
   MEMSET0(nodes2) ;
   for (unsigned i = 0; i < nrelementsof(*nodes1); ++i) {
      (*nodes1)[i].key = (int)i ;
      (*nodes2)[i].key = (int)i ;
   }

   // TEST invariant parent buffer allocation (depth is 10000)
   typeadapt.freenode_count = 0 ;
   tree.root = &(*nodes1)[0].index ;
   for (unsigned i = 0; i < nrelementsof((*nodes1)); ++i) {
      (*nodes1)[i].is_freed    = 0 ;
      (*nodes1)[i].index.left  = 0 ;
      (*nodes1)[i].index.right = &(*nodes1)[i+1].index ;
   }
   (*nodes1)[nrelementsof((*nodes1))-1].index.right = 0 ;
   TEST(0 == invariant_splaytree(&tree)) ;
   TEST(0 == free_splaytree(&tree)) ;
   TEST(nrelementsof((*nodes1)) == typeadapt.freenode_count) ;
   for (unsigned i = 0; i < nrelementsof((*nodes1)); ++i) {
      TEST((*nodes1)[i].is_freed    == 1) ;
      TEST((*nodes1)[i].index.right == 0) ;
      (*nodes1)[i].is_freed = 0 ;
   }

   // TEST splay_splaytree, splay2_splaytree: compare it with recursivesplay_splaytree
   init_splaytree(&tree, &nodeadapt) ;
   for (int i = 0; i <= 1024; ++i) {
      tree.root = build_perfect_tree(1023, (*nodes1)) ;
      TEST(tree.root == &(*nodes1)[512].index) ;
      recursivesplay_splaytree(&tree, tree.root, (void*)i) ;
      TEST(tree.root == &(*nodes1)[i?(i<1024?i:1023):1].index) ;

      tree.root = build_perfect_tree(1023, (*nodes2)) ;
      TEST(tree.root == &(*nodes2)[512].index) ;
      TEST(0         == splay_splaytree(&tree, (void*)i)) ;
      TEST(tree.root == &(*nodes2)[i?(i<1024?i:1023):1].index) ;

      for(int i2 = 1; i2 < 1024; ++i2) {
         if ((*nodes1)[i2].index.left) {
            TEST(((*nodes1)[i2].index.left - &(*nodes1)[0].index) == ((*nodes2)[i2].index.left - &(*nodes2)[0].index)) ;
         }
         if ((*nodes1)[i2].index.right) {
            TEST(((*nodes1)[i2].index.right - &(*nodes1)[0].index) == ((*nodes2)[i2].index.right - &(*nodes2)[0].index)) ;
         }
      }

      tree.root = build_perfect_tree(1023, (*nodes2)) ;
      TEST(tree.root == &(*nodes2)[512].index) ;
      testnode_t keynode = { .key = i } ;
      TEST(0         == splay2_splaytree(&tree, &keynode.index)) ;
      TEST(tree.root == &(*nodes2)[i?(i<1024?i:1023):1].index) ;

      for(int i2 = 1; i2 < 1024; ++i2) {
         if ((*nodes1)[i2].index.left) {
            TEST(((*nodes1)[i2].index.left - &(*nodes1)[0].index) == ((*nodes2)[i2].index.left - &(*nodes2)[0].index)) ;
         }
         if ((*nodes1)[i2].index.right) {
            TEST(((*nodes1)[i2].index.right - &(*nodes1)[0].index) == ((*nodes2)[i2].index.right - &(*nodes2)[0].index)) ;
         }
      }
   }

   // TEST insert,remove cycle
   init_splaytree(&tree, &nodeadapt) ;
   TEST(!tree.root) ;
   TEST(0 == insert_splaytree(&tree, 0, &(*nodes1)[0].index)) ;
   TEST(tree.root == &(*nodes1)[0].index) ;
   TEST(0 == remove_splaytree(&tree, 0, &treenode)) ;
   TEST(treenode == &(*nodes1)[0].index) ;
   TEST(!(*nodes1)[0].is_freed) ;
   TEST(!tree.root) ;

   TEST(!tree.root) ;
   TEST(0 == insert_splaytree(&tree, (void*)10, &(*nodes1)[10].index)) ;
   TEST(tree.root == &(*nodes1)[10].index) ;
   TEST(0 == remove_splaytree(&tree, (void*)10, &treenode)) ;
   TEST(treenode == &(*nodes1)[10].index) ;
   TEST(!(*nodes1)[10].is_freed) ;
   TEST(!tree.root) ;

   // TEST insert_splaytree, free_splaytree: ascending order
   for (unsigned i = 0; i < nrelementsof((*nodes1)); ++i) {
      TEST(0 == insert_splaytree(&tree, (void*)(*nodes1)[i].key, &(*nodes1)[i].index)) ;
      (*nodes1)[i].is_inserted = 1 ;
      if (!(i%100)) TEST(0 == invariant_splaytree(&tree)) ;
   }
   TEST(0 == invariant_splaytree(&tree)) ;
   for (unsigned i = 0; i < nrelementsof((*nodes1)); ++i) {
      TEST(0 == find_splaytree(&tree, (void*)(*nodes1)[i].key, &treenode)) ;
      TEST(treenode == &(*nodes1)[i].index) ;
      TEST((*nodes1)[i].is_inserted && !(*nodes1)[i].is_freed) ;
   }
   TEST(0 == free_splaytree(&tree)) ;
   for (unsigned i = 0; i < nrelementsof((*nodes1)); ++i) {
      TEST((*nodes1)[i].is_inserted && (*nodes1)[i].is_freed) ;
      (*nodes1)[i].is_freed    = 0 ;
      (*nodes1)[i].is_inserted = 0 ;
   }

   // TEST insert_splaytree, free_splaytree: descending order
   init_splaytree(&tree, &nodeadapt) ;
   for (int i = nrelementsof((*nodes1))-1; i >= 0; --i) {
      TEST(0 == insert_splaytree(&tree, (void*)(*nodes1)[i].key, &(*nodes1)[i].index)) ;
      (*nodes1)[i].is_inserted = 1 ;
      if (!(i%100)) TEST(0 == invariant_splaytree(&tree)) ;
   }
   TEST(0 == invariant_splaytree(&tree)) ;
   for (unsigned i = 0; i < nrelementsof((*nodes1)); ++i) {
      TEST(0 == find_splaytree(&tree, (void*)(*nodes1)[i].key, &treenode)) ;
      TEST(treenode == &(*nodes1)[i].index) ;
      TEST((*nodes1)[i].is_inserted && !(*nodes1)[i].is_freed) ;
   }
   TEST(0 == free_splaytree(&tree)) ;
   for (unsigned i = 0; i < nrelementsof((*nodes1)); ++i) {
      TEST((*nodes1)[i].is_inserted && (*nodes1)[i].is_freed) ;
      (*nodes1)[i].is_freed    = 0 ;
      (*nodes1)[i].is_inserted = 0 ;
   }

   // TEST insert_splaytree, remove_splaytree: random order
   init_splaytree(&tree, &nodeadapt) ;
   srand(100) ;
   for (unsigned i = 0; i < 10*nrelementsof((*nodes1)); ++i) {
      int id = rand() % (int)nrelementsof((*nodes1)) ;
      if ((*nodes1)[id].is_inserted) continue ;
      TEST(0 == insert_splaytree(&tree, (void*)(*nodes1)[id].key, &(*nodes1)[id].index)) ;
      (*nodes1)[id].is_inserted = 1 ;
   }
   for (unsigned i = 0; i < nrelementsof((*nodes1)); ++i) {
      if ((*nodes1)[i].is_inserted) continue ;
      TEST(0 == insert_splaytree(&tree, (void*)(*nodes1)[i].key, &(*nodes1)[i].index)) ;
      (*nodes1)[i].is_inserted = 1 ;
   }
   for (unsigned i = 0; i < nrelementsof((*nodes1)); ++i) {
      TEST(0 == find_splaytree(&tree, (void*)(*nodes1)[i].key, &treenode)) ;
      TEST(treenode == &(*nodes1)[i].index) ;
   }
   for (unsigned i = 0; i < nrelementsof((*nodes1)); ++i) {
      TEST(0 == remove_splaytree(&tree, (void*)(*nodes1)[i].key, &treenode)) ;
      TEST(treenode == &(*nodes1)[i].index) ;
      TEST(ESRCH == find_splaytree(&tree, (void*)(*nodes1)[i].key, &treenode)) ;
      if (!(i%100)) TEST(0 == invariant_splaytree(&tree)) ;
      TEST((*nodes1)[i].is_inserted) ;
      (*nodes1)[i].is_inserted = 0 ;
   }
   for (unsigned i = 0; i < 10*nrelementsof((*nodes1)); ++i) {
      int id = rand() % (int)nrelementsof((*nodes1)) ;
      if ((*nodes1)[id].is_inserted) continue ;
      TEST(0 == insert_splaytree(&tree, (void*)(*nodes1)[id].key, &(*nodes1)[id].index)) ;
      (*nodes1)[id].is_inserted = 1 ;
   }
   for (unsigned i = 0; i < nrelementsof((*nodes1)); ++i) {
      if ((*nodes1)[i].is_inserted) continue ;
      TEST(0 == insert_splaytree(&tree, (void*)(*nodes1)[i].key, &(*nodes1)[i].index)) ;
      (*nodes1)[i].is_inserted = 1 ;
   }
   for (unsigned i = 0; i < nrelementsof((*nodes1)); ++i) {
      TEST(0 == find_splaytree(&tree, (void*)(*nodes1)[i].key, &treenode)) ;
      TEST(treenode == &(*nodes1)[i].index) ;
      TEST(!(*nodes1)[i].is_freed) ;
   }
   for (unsigned i = 0; i < 10*nrelementsof((*nodes1)); ++i) {
      int id = rand() % (int)nrelementsof((*nodes1)) ;
      if (!(*nodes1)[id].is_inserted) continue ;
      (*nodes1)[id].is_inserted = 0 ;
      TEST(0 == remove_splaytree(&tree, (void*)(*nodes1)[id].key, &treenode)) ;
      TEST(&(*nodes1)[id].index == treenode) ;
      TEST(ESRCH == find_splaytree(&tree, (void*)(*nodes1)[id].key, &treenode)) ;
      if (!(i%100)) TEST(0 == invariant_splaytree(&tree)) ;
   }
   for (int i = nrelementsof((*nodes1))-1; i >= 0; --i) {
      if ((*nodes1)[i].is_inserted) {
         (*nodes1)[i].is_inserted = 0 ;
         TEST(0 == remove_splaytree(&tree, (void*)(*nodes1)[i].key, &treenode)) ;
         TEST(&(*nodes1)[i].index == treenode) ;
         TEST(ESRCH == find_splaytree(&tree, (void*)(*nodes1)[i].key, &treenode)) ;
      }
      if (!(i%100)) TEST(0 == invariant_splaytree(&tree)) ;
      TEST(! (*nodes1)[i].is_freed) ;
      (*nodes1)[i].is_freed = 0 ;
   }
   TEST(! tree.root) ;

   // TEST removenodes_splaytree
   for (unsigned i = 0; i < nrelementsof((*nodes1)); ++i) {
      TEST(0 == insert_splaytree(&tree, (void*)(*nodes1)[i].key, &(*nodes1)[i].index)) ;
   }
   TEST(0 == invariant_splaytree(&tree)) ;
   typeadapt.freenode_count = 0 ;
   TEST(0 == removenodes_splaytree(&tree)) ;
   TEST(nrelementsof((*nodes1)) == typeadapt.freenode_count) ;
   for (unsigned i = 0; i < nrelementsof((*nodes1)); ++i) {
      TEST((*nodes1)[i].is_freed) ;
      (*nodes1)[i].is_freed = 0 ;
   }

   // TEST EEXIST
   TEST(0 == insert_splaytree(&tree, (void*)1, &(*nodes1)[1].index)) ;
   TEST(0 == insert_splaytree(&tree, (void*)2, &(*nodes1)[2].index)) ;
   TEST(0 == insert_splaytree(&tree, (void*)3, &(*nodes1)[3].index)) ;
   TEST(EEXIST == insert_splaytree(&tree, (void*)1, &(*nodes1)[1].index)) ;
   TEST(EEXIST == insert_splaytree(&tree, (void*)2, &(*nodes1)[2].index)) ;
   TEST(EEXIST == insert_splaytree(&tree, (void*)3, &(*nodes1)[3].index)) ;
   typeadapt.freenode_count = 0 ;
   TEST(0 == removenodes_splaytree(&tree)) ;
   TEST(3 == typeadapt.freenode_count) ;
   for (int i = 1; i <= 3; ++i) {
      TEST((*nodes1)[i].is_freed) ;
      (*nodes1)[i].is_freed = 0 ;
   }

   // TEST ESRCH
   TEST(0 == insert_splaytree(&tree, (void*)1, &(*nodes1)[1].index)) ;
   TEST(0 == insert_splaytree(&tree, (void*)2, &(*nodes1)[2].index)) ;
   TEST(0 == insert_splaytree(&tree, (void*)3, &(*nodes1)[3].index)) ;
   TEST(ESRCH == find_splaytree(&tree, (void*)4, &treenode)) ;
   TEST(ESRCH == remove_splaytree(&tree, (void*)5, &treenode)) ;

   // unprepare
   TEST(0 == MM_FREE(&memblock1)) ;
   TEST(0 == MM_FREE(&memblock2)) ;
   nodes1 = 0 ;
   nodes2 = 0 ;

   return 0 ;
ONABORT:
   MM_FREE(&memblock1) ;
   MM_FREE(&memblock2) ;
   return EINVAL ;
}

static int test_iterator(void)
{
   testadapt_t                typeadapt = {
      typeadapt_INIT_LIFEKEYCMP(0, &impl_deletenode_testadapt, &impl_cmpkeyobj_testadapt, &impl_cmpobj_testadapt),
      test_errortimer_INIT_FREEABLE, 0
   } ;
   typeadapt_member_t         nodeadapt = typeadapt_member_INIT(asgeneric_typeadapt(&typeadapt, testadapt_t, testnode_t, void), offsetof(testnode_t, index)) ;
   memblock_t                 memblock1 = memblock_INIT_FREEABLE ;
   splaytree_t                tree      = splaytree_INIT_FREEABLE ;
   splaytree_t                emptytree = splaytree_INIT_FREEABLE ;
   splaytree_iterator_t       iter = splaytree_iterator_INIT_FREEABLE ;
   nodesarray_t               * nodes1 ;

   // prepare
   TEST(0 == MM_RESIZE(sizeof(nodesarray_t), &memblock1)) ;
   nodes1 = (nodesarray_t*)memblock1.addr ;
   MEMSET0(nodes1) ;
   for (unsigned i = 0; i < nrelementsof(*nodes1); ++i) {
      (*nodes1)[i].key = (int)i ;
   }
   init_splaytree(&tree, &nodeadapt) ;
   for (unsigned i = 0; i < nrelementsof((*nodes1)); ++i) {
      testnode_t * node = &(*nodes1)[(11*i) % nrelementsof((*nodes1))] ;
      TEST(0 == insert_splaytree(&tree, (void*)(node->key), &node->index)) ;
      node->is_inserted = 1 ;
      if (!(i%100)) TEST(0 == invariant_splaytree(&tree)) ;
   }
   for (unsigned i = 0; i < nrelementsof((*nodes1)); ++i) {
      TEST(1 == (*nodes1)[i].is_inserted) ;
   }

   // TEST splaytree_iterator_INIT_FREEABLE
   TEST(0 == iter.next) ;

   // TEST initfirst_splaytreeiterator: empty tree
   iter.next = (void*)1 ;
   TEST(0 == initfirst_splaytreeiterator(&iter, &emptytree)) ;
   TEST(0 == iter.next) ;

   // TEST next_splaytreeiterator: empty tree
   TEST(0 == initfirst_splaytreeiterator(&iter, &emptytree)) ;
   TEST(false == next_splaytreeiterator(&iter, 0, 0)) ;

   // TEST initlast_splaytreeiterator: empty tree
   iter.next = (void*)1 ;
   TEST(0 == initlast_splaytreeiterator(&iter, &emptytree)) ;
   TEST(0 == iter.next) ;

   // TEST prev_splaytreeiterator: empty tree
   TEST(0 == initlast_splaytreeiterator(&iter, &emptytree)) ;
   TEST(false == prev_splaytreeiterator(&iter, 0, 0)) ;

   // TEST free_splaytreeiterator
   iter.next = (void*)1 ;
   TEST(0 == free_splaytreeiterator(&iter)) ;
   TEST(0 == iter.next) ;

   for (unsigned itercount = 0; itercount < 3; ++itercount) {
      // TEST initfirst_splaytreeiterator: full tree
      TEST(0 == initfirst_splaytreeiterator(&iter, &tree)) ;
      TEST(&(*nodes1)[0].index == iter.next) ;

      // TEST next_splaytreeiterator: full tree
      for (unsigned i = 0; i < nrelementsof((*nodes1)); ++i) {
         splaytree_node_t * resultnode = &(*nodes1)[i].index ;
         splaytree_node_t * nextnode   = 0 ;
         TEST(next_splaytreeiterator(&iter, &tree, &nextnode)) ;
         TEST(resultnode == nextnode);
      }
      TEST(0 == iter.next) ;
      TEST(! next_splaytreeiterator(&iter, &tree, 0)) ;

      // TEST initlast_splaytreeiterator: full tree
      TEST(0 == initlast_splaytreeiterator(&iter, &tree)) ;
      TEST(&(*nodes1)[nrelementsof((*nodes1))-1].index == iter.next) ;

      // TEST next_splaytreeiterator: full tree
      for (unsigned i = 0; i < nrelementsof((*nodes1)); ++i) {
         splaytree_node_t * resultnode = &(*nodes1)[nrelementsof((*nodes1))-1-i].index ;
         splaytree_node_t * nextnode   = 0 ;
         TEST(prev_splaytreeiterator(&iter, &tree, &nextnode)) ;
         TEST(resultnode == nextnode);
      }
      TEST(0 == iter.next) ;
      TEST(! prev_splaytreeiterator(&iter, &tree, 0)) ;
   }

   for (unsigned i = 0; 0 == i; i = 1) {
      // TEST foreach
      foreach (_splaytree, &tree, node) {
         TEST(node == &(*nodes1)[i++].index) ;
      }
      TEST(i == nrelementsof((*nodes1))) ;

      // TEST foreachReverse
      foreachReverse (_splaytree, &tree, node) {
         TEST(node == &(*nodes1)[--i].index) ;
      }
      TEST(i == 0) ;

      // TEST foreach: remove every second current element
      foreach (_splaytree, &tree, node) {
         TEST(node == &(*nodes1)[i].index) ;
         if ((i % 2)) {
            splaytree_node_t * removed_node = 0 ;
            TEST(0 == remove_splaytree(&tree, (void*)i, &removed_node)) ;
            TEST(node == removed_node) ;
         }
         ++ i ;
      }
      TEST(i == nrelementsof((*nodes1))) ;

      // TEST foreachReverse: remove all elements
      i = nrelementsof((*nodes1)) ;
      foreachReverse (_splaytree, &tree, node) {
         -- i ;
         i -= (i % 2) ;
         TEST(node == &(*nodes1)[i].index) ;
         splaytree_node_t * removed_node = 0 ;
         TEST(0 == remove_splaytree(&tree, (void*)i, &removed_node)) ;
         TEST(node == removed_node) ;
      }
      TEST(i == 0) ;
      TEST(isempty_splaytree(&tree)) ;
   }

   // unprepare
   TEST(0 == MM_FREE(&memblock1)) ;
   nodes1 = 0 ;

   return 0 ;
ONABORT:
   MM_FREE(&memblock1) ;
   return EINVAL ;
}

splaytree_IMPLEMENT(_testtree, testnode_t, int, index)

static int test_generic(void)
{
   testadapt_t                typeadapt = {
      typeadapt_INIT_LIFEKEYCMP(0, &impl_deletenode_testadapt, &impl_cmpkeyobj_testadapt, &impl_cmpobj_testadapt),
      test_errortimer_INIT_FREEABLE, 0
   } ;
   typeadapt_member_t         nodeadapt = typeadapt_member_INIT(asgeneric_typeadapt(&typeadapt, testadapt_t, testnode_t, void), offsetof(testnode_t, index)) ;
   memblock_t                 memblock1 = memblock_INIT_FREEABLE ;
   splaytree_t                tree      = splaytree_INIT_FREEABLE ;
   nodesarray_t               * nodes1 ;

   // prepare
   TEST(0 == MM_RESIZE(sizeof(nodesarray_t), &memblock1)) ;
   nodes1 = (nodesarray_t*)memblock1.addr ;
   MEMSET0(nodes1) ;
   for (unsigned i = 0; i < nrelementsof(*nodes1); ++i) {
      (*nodes1)[i].key = (int)i ;
   }

   // TEST init_splaytree, free_splaytree
   tree.root = (void*)1 ;
   init_testtree(&tree, &nodeadapt) ;
   TEST(0 == tree.root) ;
   TEST(isequal_typeadaptmember(&tree.nodeadp, &nodeadapt)) ;
   init_testtree(&tree, &nodeadapt) ;
   TEST(0 == free_testtree(&tree)) ;
   typeadapt_member_t emptynodeadapt = typeadapt_member_INIT_FREEABLE ;
   TEST(isequal_typeadaptmember(&tree.nodeadp, &emptynodeadapt)) ;

   // TEST getinistate_splaytree
   TEST(0 == free_testtree(&tree)) ;
   TEST(isempty_testtree(&tree)) ;
   testnode_t * root = (void*)1 ;
   typeadapt_member_t nodeadapt2 = nodeadapt ;
   getinistate_testtree(&tree, &root, &nodeadapt2) ;
   TEST(0 == root) ;
   TEST(isequal_typeadaptmember(&nodeadapt2, &emptynodeadapt)) ;
   init_testtree(&tree, &nodeadapt) ;
   tree.root = &(*nodes1)[10].index ;
   getinistate_testtree(&tree, &root, &nodeadapt2) ;
   TEST(&(*nodes1)[10] == root) ;
   TEST(isequal_typeadaptmember(&nodeadapt2, &nodeadapt)) ;

   // TEST isempty_splaytree
   tree.root = (void*)1 ;
   TEST(! isempty_testtree(&tree)) ;
   tree.root = (void*)0 ;
   TEST(isempty_testtree(&tree)) ;

   // TEST insert_splaytree, find_splaytree, remove_splaytree, invariant_splaytree
   init_testtree(&tree, &nodeadapt) ;
   TEST(0 == invariant_splaytree(&tree)) ;
   for (unsigned i = 0; i < nrelementsof((*nodes1)); ++i) {
      TEST(0 == insert_testtree(&tree, (int*)(*nodes1)[i].key, &(*nodes1)[i])) ;
   }
   TEST(0 == invariant_splaytree(&tree)) ;
   for (unsigned i = 0; i < nrelementsof((*nodes1)); ++i) {
      testnode_t * found_node = 0 ;
      TEST(0 == find_testtree(&tree, (int*)(*nodes1)[i].key, &found_node)) ;
      TEST(found_node == &(*nodes1)[i]) ;
   }
   TEST(0 == invariant_splaytree(&tree)) ;
   for (unsigned i = 0; i < nrelementsof((*nodes1)); ++i) {
      testnode_t * removed_node = 0 ;
      TEST(0 == remove_testtree(&tree, (int*)(*nodes1)[i].key, &removed_node)) ;
      TEST(removed_node == &(*nodes1)[i]) ;
      TEST(ESRCH == find_testtree(&tree, (int*)(*nodes1)[i].key, &removed_node)) ;
      if (!(i%100)) TEST(0 == invariant_splaytree(&tree)) ;
   }

   // TEST removenodes_splaytree, free_splaytree
   init_testtree(&tree, &nodeadapt) ;
   for (unsigned i = 0; i < nrelementsof((*nodes1)); ++i) {
      TEST(0 == insert_testtree(&tree, (int*)(*nodes1)[i].key, &(*nodes1)[i])) ;
   }
   TEST(0 == removenodes_testtree(&tree)) ;
   for (unsigned i = 0; i < nrelementsof((*nodes1)); ++i) {
      TEST(1 == (*nodes1)[i].is_freed) ;
   }
   for (unsigned i = 0; i < nrelementsof((*nodes1)); ++i) {
      TEST(0 == insert_testtree(&tree, (int*)(*nodes1)[i].key, &(*nodes1)[i])) ;
   }
   TEST(0 == free_testtree(&tree)) ;
   for (unsigned i = 0; i < nrelementsof((*nodes1)); ++i) {
      TEST(2 == (*nodes1)[i].is_freed) ;
   }

   // TEST foreach, foreachReverse
   init_testtree(&tree, &nodeadapt) ;
   for (unsigned i = 0; i < nrelementsof((*nodes1)); ++i) {
      TEST(0 == insert_testtree(&tree, (int*)(*nodes1)[i].key, &(*nodes1)[i])) ;
   }
   for (unsigned i = 0; 0 == i; i = 1) {
      foreach (_testtree, &tree, node) {
         TEST(node == &(*nodes1)[i++]) ;
      }
      TEST(i == nrelementsof((*nodes1))) ;

      foreachReverse (_testtree, &tree, node) {
         TEST(node == &(*nodes1)[--i]) ;
      }
      TEST(i == 0) ;
   }

   // unprepare
   TEST(0 == MM_FREE(&memblock1)) ;
   nodes1 = 0 ;

   return 0 ;
ONABORT:
   MM_FREE(&memblock1) ;
   return EINVAL ;
}

int unittest_ds_inmem_splaytree()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())       goto ONABORT ;
   if (test_insertremove())   goto ONABORT ;
   if (test_iterator())       goto ONABORT ;
   if (test_generic())        goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif