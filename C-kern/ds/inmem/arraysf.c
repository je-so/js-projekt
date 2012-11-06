/* title: ArraySF impl
   Implements array which supports non-continuous index values.

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

   file: C-kern/api/ds/inmem/arraysf.h
    Header file <ArraySF>.

   file: C-kern/ds/inmem/arraysf.c
    Implementation file <ArraySF impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/ds/foreach.h"
#include "C-kern/api/ds/typeadapt.h"
#include "C-kern/api/ds/inmem/arraysf.h"
#include "C-kern/api/ds/inmem/binarystack.h"
#include "C-kern/api/math/int/log2.h"
#include "C-kern/api/math/int/power2.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/platform/virtmemory.h"
#include "C-kern/api/test/errortimer.h"
#include "C-kern/api/test/testmm.h"
#endif


// section: arraysf_t

// group: helper

#define toplevelsize_arraysf(array)       ((array)->toplevelsize)

#define posshift_arraysf(array)           ((array)->posshift)

#define objectsize_arraysf(toplevelsize)  (sizeof(arraysf_t) + sizeof(arraysf_node_t*) * (toplevelsize))

typedef struct arraysf_findresult_t       arraysf_findresult_t ;

struct arraysf_findresult_t {
   unsigned             rootindex ;
   unsigned             childindex ;
   unsigned             pchildindex ;
   arraysf_mwaybranch_t * parent ;
   arraysf_mwaybranch_t * pparent ;
   arraysf_unode_t      * found_node ;
   size_t               found_pos ;
} ;

static int find_arraysf(const arraysf_t * array, size_t pos, /*err;out*/arraysf_findresult_t * result)
{
   int err ;
   uint32_t rootindex = (uint32_t)(pos >> posshift_arraysf(array)) & (uint32_t)(toplevelsize_arraysf(array)-1) ;

   arraysf_unode_t      * node      = array->root[rootindex] ;
   arraysf_mwaybranch_t * pparent   = 0 ;
   arraysf_mwaybranch_t * parent    = 0 ;
   unsigned             childindex  = 0 ;
   unsigned             pchildindex = 0 ;

   err = ESRCH ;

   for (;node;) {

      if (isbranchtype_arraysfunode(node)) {
         pparent = parent ;
         parent  = asbranch_arraysfunode(node) ;
         pchildindex = childindex   ;
         childindex  = childindex_arraysfmwaybranch(parent, pos) ;
         node        = parent->child[childindex] ;
      } else {
         result->found_pos = asnode_arraysfunode(node)->pos ;
         if (pos == result->found_pos) err = 0 ;
         break ;
      }
   }

   result->rootindex   = rootindex ;
   result->childindex  = childindex ;
   result->pchildindex = pchildindex ;
   result->parent      = parent ;
   result->pparent     = pparent ;
   result->found_node  = node ;
   // result->found_pos  ! already set in case node != 0

   return err ;
}

// group: lifetime

int new_arraysf(/*out*/arraysf_t ** array, uint32_t toplevelsize, uint8_t posshift)
{
   int err ;
   memblock_t new_obj = memblock_INIT_FREEABLE ;

   toplevelsize += (0 == toplevelsize) ;
   toplevelsize = makepowerof2_int(toplevelsize) ;

   VALIDATE_INPARAM_TEST(toplevelsize <= 0x00800000, ONABORT, PRINTUINT32_LOG(toplevelsize)) ;
   VALIDATE_INPARAM_TEST(posshift <= 8*sizeof(size_t) - log2_int(toplevelsize < 2 ? 2 : toplevelsize), ONABORT, PRINTUINT32_LOG(posshift)) ;

   const size_t objsize = objectsize_arraysf(toplevelsize) ;

   err = RESIZE_MM(objsize, &new_obj) ;
   if (err) goto ONABORT ;

   memset(new_obj.addr, 0, objsize) ;
   ((arraysf_t*)(new_obj.addr))->toplevelsize = 0xffffff & toplevelsize ;
   ((arraysf_t*)(new_obj.addr))->posshift     = posshift ;

   *array = ((arraysf_t*)(new_obj.addr)) ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int delete_arraysf(arraysf_t ** array, struct typeadapt_member_t * nodeadp)
{
   int err = 0 ;
   int err2 ;
   arraysf_t * del_obj = *array ;

   if (del_obj) {
      *array = 0 ;

      const bool isDelete = (nodeadp && islifetimedelete_typeadapt(nodeadp->typeadp)) ;

      for (unsigned i = 0; i < toplevelsize_arraysf(del_obj); ++i) {

         if (!del_obj->root[i]) continue ;

         arraysf_unode_t * node = del_obj->root[i] ;

         if (!isbranchtype_arraysfunode(node)) {
            if (isDelete) {
               typeadapt_object_t * delobj = memberasobject_typeadaptmember(nodeadp, node) ;
               err2 = calldelete_typeadaptmember(nodeadp, &delobj) ;
               if (err2) err = err2 ;
            }
            continue ;
         }

         arraysf_mwaybranch_t * branch = asbranch_arraysfunode(node) ;
         node = branch->child[0] ;
         branch->child[0] = 0 ;
         branch->used = nrelementsof(branch->child)-1 ;

         for (;;) {

            for (;;) {
               if (node) {
                  if (isbranchtype_arraysfunode(node)) {
                     arraysf_mwaybranch_t * parent = branch ;
                     branch = asbranch_arraysfunode(node) ;
                     node = branch->child[0] ;
                     branch->child[0] = (arraysf_unode_t*) parent ;
                     branch->used     = nrelementsof(branch->child)-1 ;
                     continue ;
                  } else if (isDelete) {
                     typeadapt_object_t * delobj = memberasobject_typeadaptmember(nodeadp, node) ;
                     err2 = calldelete_typeadaptmember(nodeadp, &delobj) ;
                     if (err2) err = err2 ;
                  }
               }

               if (! branch->used)  break ;
               node = branch->child[branch->used --] ;
            }

            do {
               arraysf_mwaybranch_t * parent = (arraysf_mwaybranch_t *) branch->child[0] ;
               memblock_t           mblock   = memblock_INIT(sizeof(arraysf_mwaybranch_t), (uint8_t*)branch) ;
               err2 = FREE_MM(&mblock) ;
               if (err2) err = err2 ;
               branch = parent ;
            } while (branch && !branch->used) ;

            if (!branch) break ;

            node = branch->child[branch->used --] ;
         }

      }

      const size_t objsize = objectsize_arraysf(toplevelsize_arraysf(del_obj)) ;

      memblock_t mblock = memblock_INIT(objsize, (uint8_t*)del_obj) ;
      err2 = FREE_MM(&mblock) ;
      if (err2) err = err2 ;
   }

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

// group: query

struct arraysf_node_t * at_arraysf(const arraysf_t * array, size_t pos)
{
   int err ;
   arraysf_findresult_t found ;

   err = find_arraysf(array, pos, &found) ;

   return err ? 0 : asnode_arraysfunode(found.found_node) ;
}

// group: change

int tryinsert_arraysf(arraysf_t * array, struct arraysf_node_t * node, /*out;err*/struct arraysf_node_t ** inserted_or_existing_node, struct typeadapt_member_t * nodeadp/*0=>no copy is made*/)
{
   int err ;
   arraysf_findresult_t found ;
   typeadapt_object_t   * copied_node = 0 ;
   size_t               pos           = node->pos ;

   err = find_arraysf(array, pos, &found) ;
   if (ESRCH != err) {
      *inserted_or_existing_node = (0 == err) ? asnode_arraysfunode(found.found_node) : 0 ;
      return (0 == err) ? EEXIST : err ;
   }

   if (nodeadp) {
      err = callnewcopy_typeadaptmember(nodeadp, &copied_node, memberasobject_typeadaptmember(nodeadp, node)) ;
      if (err) goto ONABORT ;
      node = objectasmember_typeadaptmember(nodeadp, copied_node) ;
   }

   VALIDATE_INPARAM_TEST(0 == ((uintptr_t)node&0x01), ONABORT, ) ;

   size_t   pos2    = 0 ;
   size_t   posdiff = 0 ;

   if (found.found_node) {

      pos2    = found.found_pos ;
      posdiff = (pos ^ pos2) ;

      if (  ! found.parent
         || 0 == (posdiff >> found.parent->shift))  {

   // prefix matches (add new branch layer after found.parent)

         unsigned shift = log2_int(posdiff) & ~0x01u ;

         memblock_t mblock = memblock_INIT_FREEABLE ;
         err = RESIZE_MM(sizeof(arraysf_mwaybranch_t), &mblock) ;
         if (err) goto ONABORT ;

         arraysf_mwaybranch_t * new_branch = (arraysf_mwaybranch_t *) mblock.addr ;

         init_arraysfmwaybranch(new_branch, shift, pos2, found.found_node, pos, asunode_arraysfnode(node)) ;

         if (found.parent) {
            found.parent->child[found.childindex] = asunode_arraysfmwaybranch(new_branch) ;
         } else {
            array->root[found.rootindex] = asunode_arraysfmwaybranch(new_branch) ;
         }
         goto DONE ;
      }

   // fall through to not so simple case

   } else /*(! found.found_node)*/ {

   // simple case (parent is 0)

      if (! found.parent) {
         array->root[found.rootindex] = asunode_arraysfnode(node) ;
         goto DONE ;
      }

   // get pos of already stored node / check prefix match => second simple case

      arraysf_mwaybranch_t * branch = found.parent ;
      for (unsigned i = nrelementsof(branch->child); (i--); ) {
         if (branch->child[i]) {
            if (isbranchtype_arraysfunode(branch->child[i])) {
               branch = asbranch_arraysfunode(branch->child[i]) ;
               i = nrelementsof(branch->child) ;
               continue ;
            }
            pos2    = asnode_arraysfunode(branch->child[i])->pos ;
            posdiff = (pos ^ pos2) ;
            break ;
         }
      }

      unsigned prefix = ~0x03u & (posdiff >> found.parent->shift) ;

      if (0 == prefix) {
         // prefix does match
         found.parent->child[found.childindex] = asunode_arraysfnode(node) ;
         ++ found.parent->used ;
         goto DONE ;
      }

   }

   // not so simple case prefix differs (add new branch layer between root & found.parent)

   assert(found.parent) ;
   arraysf_mwaybranch_t * parent = 0 ;
   arraysf_mwaybranch_t * branch = asbranch_arraysfunode(array->root[found.rootindex]) ;

   unsigned childindex = 0 ;
   unsigned shift = log2_int(posdiff) & ~0x01u ;

   while (branch->shift > shift) {
      parent = branch ;
      childindex = childindex_arraysfmwaybranch(branch, pos) ;
      arraysf_unode_t * child = branch->child[childindex] ;
      assert(child) ;
      assert(isbranchtype_arraysfunode(child)) ;
      branch = asbranch_arraysfunode(child) ;
   }

   memblock_t mblock = memblock_INIT_FREEABLE ;
   err = RESIZE_MM(sizeof(arraysf_mwaybranch_t), &mblock) ;
   if (err) goto ONABORT ;

   arraysf_mwaybranch_t * new_branch = (arraysf_mwaybranch_t*) mblock.addr ;

   init_arraysfmwaybranch(new_branch, shift, pos2, asunode_arraysfmwaybranch(branch), pos, asunode_arraysfnode(node)) ;

   if (parent) {
      parent->child[childindex] = asunode_arraysfmwaybranch(new_branch) ;
   } else {
      array->root[found.rootindex] = asunode_arraysfmwaybranch(new_branch) ;
   }

DONE:

   ++ array->length ;

   *inserted_or_existing_node = node ;

   return 0 ;
ONABORT:
   *inserted_or_existing_node = 0 ;
   if (copied_node) {
      (void) calldelete_typeadaptmember(nodeadp, &copied_node) ;
   }
   TRACEABORT_LOG(err) ;
   return err ;
}

int tryremove_arraysf(arraysf_t * array, size_t pos, /*out*/struct arraysf_node_t ** removed_node)
{
   int err ;
   arraysf_findresult_t found ;

   err = find_arraysf(array, pos, &found) ;
   if (err) return err ;

   if (! found.parent) {

   // simple case

      array->root[found.rootindex] = 0 ;

   } else /*0 != found.parent*/ {

   // check for simple case 2

      if (found.parent->used > 2) {

         -- found.parent->used ;
         found.parent->child[found.childindex] = 0 ;

      } else {

   // delete parent (only one more entry) and adapt parent of parent

         for (unsigned i = nrelementsof(found.parent->child)-1; ; ) {
            if (  i != found.childindex
               && found.parent->child[i]) {
               arraysf_unode_t * other_child = found.parent->child[i] ;
               if (found.pparent) {
                  setchild_arraysfmwaybranch(found.pparent, found.pchildindex, other_child) ;
               } else {
                  array->root[found.rootindex] = other_child ;
               }
               break ;
            }
            if (!(i--)) {
               err = EINVAL ;
               goto ONABORT ;
            }
         }

         memblock_t mblock = memblock_INIT(sizeof(arraysf_mwaybranch_t), (uint8_t*)found.parent) ;
         err = FREE_MM(&mblock) ;
         (void) err /*IGNORE*/ ;
      }

   }

   assert(array->length > 0) ;
   -- array->length ;

   *removed_node = asnode_arraysfunode(found.found_node) ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int remove_arraysf(arraysf_t * array, size_t pos, /*out*/struct arraysf_node_t ** removed_node)
{
   int err ;

   err = tryremove_arraysf(array, pos, removed_node) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int insert_arraysf(arraysf_t * array, struct arraysf_node_t * node, /*out*/struct arraysf_node_t ** inserted_node/*0=>not returned*/, struct typeadapt_member_t * nodeadp/*0=>no copy is made*/)
{
   int err ;
   struct arraysf_node_t * inserted_or_existing_node ;

   err = tryinsert_arraysf(array, node, &inserted_or_existing_node, nodeadp) ;
   if (err) goto ONABORT ;

   if (inserted_node) {
      *inserted_node = inserted_or_existing_node ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}


// section: arraysf_iterator_t

typedef struct arraysf_pos_t           arraysf_pos_t ;

struct arraysf_pos_t {
   arraysf_mwaybranch_t * branch ;
   unsigned             ci ;
} ;

int initfirst_arraysfiterator(/*out*/arraysf_iterator_t * iter, arraysf_t * array)
{
   int err ;
   size_t        objectsize = sizeof(binarystack_t) ;
   memblock_t    objectmem  = memblock_INIT_FREEABLE ;
   binarystack_t * stack  = 0 ;

   (void) array ;

   err = RESIZE_MM(objectsize, &objectmem) ;
   if (err) goto ONABORT ;

   stack  = (binarystack_t *) objectmem.addr ;

   err = init_binarystack(stack, (/*max depth*/4 * sizeof(size_t)) * /*objectsize*/sizeof(arraysf_pos_t) ) ;
   if (err) goto ONABORT ;

   iter->stack = stack ;
   iter->ri    = 0 ;

   return 0 ;
ONABORT:
   FREE_MM(&objectmem) ;
   TRACEABORT_LOG(err) ;
   return err ;
}

int free_arraysfiterator(arraysf_iterator_t * iter)
{
   int err ;
   int err2 ;

   if (iter->stack) {
      size_t        objectsize = sizeof(binarystack_t) ;
      memblock_t    objectmem  = memblock_INIT(objectsize, (uint8_t*)iter->stack) ;

      err = free_binarystack(iter->stack) ;
      iter->stack = 0 ;

      err2 = FREE_MM(&objectmem) ;
      if (err2) err = err2 ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

bool next_arraysfiterator(arraysf_iterator_t * iter, arraysf_t * array, /*out*/struct arraysf_node_t ** node)
{
   int err ;
   size_t         nrelemroot = toplevelsize_arraysf(array) ;
   arraysf_pos_t  * pos ;

   for (;;) {

      if (isempty_binarystack(iter->stack)) {

         arraysf_unode_t * rootnode ;

         for (;;) {
            if (iter->ri >= nrelemroot) {
               return false ;
            }
            rootnode = array->root[iter->ri ++] ;
            if (rootnode) {
               if (!isbranchtype_arraysfunode(rootnode)) {
                  *node = asnode_arraysfunode(rootnode) ;
                  return true ;
               }
               break ;
            }
         }

         err = push_binarystack(iter->stack, &pos) ;
         if (err) goto ONABORT ;

         pos->branch = asbranch_arraysfunode(rootnode) ;
         pos->ci     = 0 ;

      } else {
         pos = top_binarystack(iter->stack) ;
      }

      for (;;) {
         arraysf_unode_t * childnode = pos->branch->child[pos->ci ++] ;

         if (pos->ci >= nrelementsof(pos->branch->child)) {
            // pos becomes invalid
            err = pop_binarystack(iter->stack, sizeof(arraysf_pos_t)) ;
            if (err) goto ONABORT ;

            if (!childnode) break ;
         }

         if (childnode) {
            if (isbranchtype_arraysfunode(childnode)) {
               err = push_binarystack(iter->stack, &pos) ;
               if (err) goto ONABORT ;
               pos->branch = asbranch_arraysfunode(childnode) ;
               pos->ci     = 0 ;
               continue ;
            } else {
               *node = asnode_arraysfunode(childnode) ;
               return true ;
            }
         }
      }

   }

   return false ;
ONABORT:
   // move iterator to end of container
   iter->ri = nrelemroot ;
   pop_binarystack(iter->stack, size_binarystack(iter->stack)) ;
   TRACEABORT_LOG(err) ;
   return false ;
}


// group: test

#ifdef KONFIG_UNITTEST

typedef struct testnode_t              testnode_t ;
typedef struct testnode_adapt_t        testnode_adapt_t ;

struct testnode_t {
   arraysf_node_t node ;
   int            copycount ;
   arraysf_node_EMBED(pos2) ;
   int            freecount ;
} ;

struct testnode_adapt_t {
   struct {
      typeadapt_EMBED(testnode_adapt_t, testnode_t, void*) ;
   } ;
   test_errortimer_t    errcounter ;
} ;

static int copynode_testnodeadapt(testnode_adapt_t * typeadp, testnode_t ** copied_node, const testnode_t * node)
{
   (void) typeadp ;
   ++ CONST_CAST(testnode_t,node)->copycount ;
   *copied_node = CONST_CAST(testnode_t,node) ;
   return 0 ;
}

static int freenode_testnodeadapt(testnode_adapt_t * typeadp, testnode_t ** node)
{
   int err = process_testerrortimer(&typeadp->errcounter) ;

   if (!err && *node) {
      ++ (*node)->freecount ;
   }

   *node = 0 ;
   return err ;
}

static int test_initfree(void)
{
   const size_t      nrnodes   = 100000 ;
   vm_block_t        memblock  = vm_block_INIT_FREEABLE ;
   arraysf_t         * array   = 0 ;
   testnode_adapt_t  typeadapt = { typeadapt_INIT_LIFETIME(&copynode_testnodeadapt, &freenode_testnodeadapt), test_errortimer_INIT_FREEABLE } ;
   typeadapt_member_t nodeadp  = typeadapt_member_INIT(asgeneric_typeadapt(&typeadapt,testnode_adapt_t,testnode_t,void*), offsetof(testnode_t,node)) ;
   testnode_t        * nodes ;
   arraysf_node_t    * inserted_node ;
   arraysf_node_t    * removed_node ;

   // prepare
   TEST(0 == init_vmblock(&memblock, (pagesize_vm()-1+nrnodes*sizeof(testnode_t)) / pagesize_vm() )) ;
   nodes = (testnode_t *) memblock.addr ;

   // TEST arraysf_node_INIT
   for (unsigned i = 1; i; i <<= 1) {
      nodes[0].node = (arraysf_node_t) arraysf_node_INIT(i) ;
      TEST(i == nodes[0].node.pos) ;
   }
   nodes[0].node = (arraysf_node_t) arraysf_node_INIT(0) ;
   TEST(0 == nodes[0].node.pos) ;

   // TEST new_arraysf, delete_arraysf
   for (unsigned topsize = 0, expectsize=1; topsize <= 256; ++topsize) {
      if (topsize > expectsize)  expectsize <<= 1 ;
      for (uint8_t posshift = 0; posshift <= 8*sizeof(size_t)-log2_int(expectsize < 2 ? 2 : expectsize); ++posshift) {
         TEST(0 == new_arraysf(&array, topsize, posshift)) ;
         TEST(0 != array) ;
         TEST(0 == length_arraysf(array)) ;
         TEST(expectsize == toplevelsize_arraysf(array)) ;
         TEST(posshift   == posshift_arraysf(array)) ;
         for (unsigned i = 0; i < toplevelsize_arraysf(array); ++i) {
            TEST(0 == array->root[i]) ;
         }
         TEST(0 == delete_arraysf(&array, 0)) ;
         TEST(0 == array) ;
         TEST(0 == delete_arraysf(&array, 0)) ;
         TEST(0 == array) ;
      }
   }

   // TEST root distributions
   for (unsigned rootsize = 1; rootsize <= 256; rootsize *= 2) {
      for (uint8_t posshift = 0; posshift <= 8*sizeof(size_t)-log2_int(rootsize < 2 ? 2 : rootsize); ++posshift) {
         TEST(0 == new_arraysf(&array, rootsize, posshift)) ;
         for (size_t pos = 0; pos < 512; ++ pos) {
            testnode_t  node = { .node = arraysf_node_INIT(pos << posshift) } ;
            size_t      ri   = (pos & (rootsize-1)) ;
            TEST(ri < toplevelsize_arraysf(array)) ;
            TEST(0 == tryinsert_arraysf(array, &node.node, &inserted_node, 0)) ;
            TEST(1 == length_arraysf(array)) ;
            TEST(0 != array->root[ri]) ;
            TEST(inserted_node == &node.node) ;
            TEST(&node.node == asnode_arraysfunode(array->root[ri])) ;
            for (unsigned i = 0; i < toplevelsize_arraysf(array); ++i) {
               if (i == ri) continue ;
               TEST(0 == array->root[i]) ;
            }
            TEST(0 == tryremove_arraysf(array, pos << posshift, &removed_node)) ;
            TEST(0 == length_arraysf(array)) ;
            TEST(0 == array->root[ri]) ;
            TEST(removed_node == &node.node) ;
         }
         TEST(0 == delete_arraysf(&array, 0)) ;
         TEST(0 == array) ;
      }
   }

   // TEST insert_arraysf (1 level)
   TEST(0 == new_arraysf(&array, 16, 8)) ;
   nodes[4].node = (arraysf_node_t) arraysf_node_INIT(4) ;
   TEST(0 == tryinsert_arraysf(array, &nodes[4].node, &inserted_node, &nodeadp))
   TEST(&nodes[4].node == asnode_arraysfunode(array->root[0])) ;
   for (size_t pos = 5; pos <= 7; ++pos) {
      nodes[pos] = (testnode_t) { .node = arraysf_node_INIT(pos) } ;
      inserted_node = 0 ;
      TEST(0 == tryinsert_arraysf(array, &nodes[pos].node, &inserted_node, &nodeadp))
      TEST(inserted_node == &nodes[pos].node) ;
      TEST(0 == nodes[pos].freecount) ;
      TEST(1 == nodes[pos].copycount) ;
      TEST(pos-3 == length_arraysf(array)) ;
      TEST(isbranchtype_arraysfunode(array->root[0])) ;
      TEST(0 == asbranch_arraysfunode(array->root[0])->shift) ;
      TEST(pos-3 == asbranch_arraysfunode(array->root[0])->used) ;
   }
   for (size_t pos = 4; pos <= 7; ++pos) {
      TEST(&nodes[pos].node == asnode_arraysfunode(asbranch_arraysfunode(array->root[0])->child[pos-4])) ;
      TEST(&nodes[pos].node == at_arraysf(array, pos)) ;
      TEST(0                == at_arraysf(array, 10*pos+4)) ;
   }

   // TEST tryremove_arraysf (1 level)
   for (size_t pos = 4; pos <= 7; ++pos) {
      TEST(0 == tryremove_arraysf(array, pos, &removed_node))
      TEST(&nodes[pos].node == removed_node) ;
      TEST(1 == nodes[pos].copycount) ;
      TEST(0 == nodes[pos].freecount) ;
      TEST(0 == at_arraysf(array, pos)) ;
      if (pos < 6) {
         TEST(array->root[0]) ;
         TEST(isbranchtype_arraysfunode(array->root[0])) ;
      } else if (pos == 6) {
         TEST(&nodes[7].node == asnode_arraysfunode(array->root[0])) ;
      } else {
         TEST(! array->root[0]) ;
      }
      TEST(7-pos == length_arraysf(array)) ;
   }

   // TEST insert_arraysf (2 levels)
   arraysf_mwaybranch_t * branch1 = 0 ;
   for (size_t pos = 16; pos <= 31; ++pos) {
      nodes[pos] = (testnode_t) { .node = arraysf_node_INIT(pos) } ;
      TEST(0 == tryinsert_arraysf(array, &nodes[pos].node, &inserted_node, &nodeadp))
      TEST(1 == nodes[pos].copycount) ;
      TEST(0 == nodes[pos].freecount) ;
      TEST(pos-15 == length_arraysf(array)) ;
      if (pos == 16) {
         TEST(&nodes[16].node == asnode_arraysfunode(array->root[0])) ;
      } else if (pos == 17) {
         TEST(isbranchtype_arraysfunode(array->root[0])) ;
         branch1 = asbranch_arraysfunode(array->root[0]) ;
         TEST(0 == branch1->shift) ;
         TEST(&nodes[16].node  == asnode_arraysfunode(branch1->child[0])) ;
         TEST(&nodes[17].node  == asnode_arraysfunode(branch1->child[1])) ;
      } else if (pos <= 19) {
         TEST(isbranchtype_arraysfunode(array->root[0])) ;
         TEST(branch1 == asbranch_arraysfunode(array->root[0])) ;
         TEST(&nodes[pos].node == asnode_arraysfunode(branch1->child[pos-16])) ;
      } else if (pos == 20 || pos == 24 || pos == 28) {
         TEST(isbranchtype_arraysfunode(array->root[0])) ;
         if (pos == 20) {
            arraysf_mwaybranch_t * branch2 = asbranch_arraysfunode(array->root[0]) ;
            TEST(2 == branch2->shift) ;
            TEST(branch1 == asbranch_arraysfunode(branch2->child[0])) ;
            branch1 = branch2 ;
         }
         TEST(&nodes[pos].node == asnode_arraysfunode(branch1->child[(pos-16)/4])) ;
      } else {
         TEST(isbranchtype_arraysfunode(array->root[0])) ;
         TEST(branch1 == asbranch_arraysfunode(array->root[0])) ;
         TEST(isbranchtype_arraysfunode(branch1->child[(pos-16)/4])) ;
         arraysf_mwaybranch_t * branch2 = asbranch_arraysfunode(branch1->child[(pos-16)/4]) ;
         TEST(&nodes[pos&~0x03u].node == asnode_arraysfunode(branch2->child[0])) ;
         TEST(&nodes[pos].node        == asnode_arraysfunode(branch2->child[pos&0x03u])) ;
      }
   }

   // TEST remove_arraysf (2 levels)
   for (size_t pos = 16; pos <= 31; ++pos) {
      TEST(0 == tryremove_arraysf(array, pos, &removed_node)) ;
      TEST(&nodes[pos].node == removed_node) ;
      TEST(1 == nodes[pos].copycount) ;
      TEST(0 == nodes[pos].freecount) ;
      TEST(0 == at_arraysf(array, pos))
      TEST(31-pos == length_arraysf(array)) ;
      if (pos <= 17) {
         TEST(1 == isbranchtype_arraysfunode(asbranch_arraysfunode(array->root[0])->child[0])) ;
      } else if (pos == 18) {
         TEST(&nodes[19].node == asnode_arraysfunode(asbranch_arraysfunode(array->root[0])->child[0])) ;
      } else if (pos == 19) {
         TEST(0 == asbranch_arraysfunode(array->root[0])->child[0]) ;
      } else if (pos < 22) {
         TEST(1 == isbranchtype_arraysfunode(asbranch_arraysfunode(array->root[0])->child[1])) ;
      } else if (pos == 22) {
         TEST(1 == isbranchtype_arraysfunode(array->root[0])) ;
         TEST(2 == asbranch_arraysfunode(array->root[0])->shift) ;
         TEST(&nodes[23].node == asnode_arraysfunode(asbranch_arraysfunode(array->root[0])->child[1])) ;
      } else if (pos <= 26) {
         TEST(1 == isbranchtype_arraysfunode(array->root[0])) ;
         TEST(2 == asbranch_arraysfunode(array->root[0])->shift) ;
      } else if (pos <= 29) {
         TEST(1 == isbranchtype_arraysfunode(array->root[0])) ;
         TEST(0 == asbranch_arraysfunode(array->root[0])->shift) ;
      } else if (pos == 30) {
         TEST(&nodes[31].node == asnode_arraysfunode(array->root[0])) ;
      } else if (pos == 31) {
         TEST(0 == array->root[0]) ;
      }
   }

   // TEST insert_arraysf, at_arraysf, remove_arraysf: ascending
   for (unsigned rootsize = 512; rootsize <= 1024; rootsize *= 2) {
      for (uint8_t posshift = 0; posshift < 8*sizeof(size_t); posshift = (uint8_t)(posshift+16)) {
         TEST(0 == delete_arraysf(&array, 0)) ;
         TEST(0 == new_arraysf(&array, rootsize, posshift)) ;
         for (size_t pos = 0; pos < nrnodes; ++pos) {
            nodes[pos] = (testnode_t) { .node = arraysf_node_INIT(pos) } ;
            TEST(0 == tryinsert_arraysf(array, &nodes[pos].node, &inserted_node, 0))
            TEST(inserted_node == &nodes[pos].node) ;
            TEST(1+pos == length_arraysf(array)) ;
         }
         for (size_t pos = 0; pos < nrnodes; ++pos) {
            TEST(&nodes[pos].node == at_arraysf(array, pos)) ;
         }
         for (size_t pos = 0; pos < nrnodes; ++pos) {
            TEST(0 != at_arraysf(array, pos))
            TEST(0 == tryremove_arraysf(array, pos, &removed_node))
            TEST(0 == nodes[pos].copycount) ;
            TEST(0 == nodes[pos].freecount) ;
            TEST(removed_node == &nodes[pos].node) ;
            TEST(0 == at_arraysf(array, pos))
            TEST(nrnodes-1-pos == length_arraysf(array)) ;
         }
      }
   }

   // TEST insert_arraysf, at_arraysf, remove_arraysf: descending
   for (unsigned rootsize = 512; rootsize <= 1024; rootsize *= 2) {
      for (uint8_t posshift = 8*sizeof(size_t)-10; posshift >= 16; posshift=(uint8_t)(posshift-16)) {
         TEST(0 == delete_arraysf(&array, 0)) ;
         TEST(0 == new_arraysf(&array, rootsize, posshift)) ;
         for (size_t pos = nrnodes; (pos --); ) {
            nodes[pos] = (testnode_t) { .node = arraysf_node_INIT(pos) } ;
            TEST(0 == tryinsert_arraysf(array, &nodes[pos].node, &inserted_node, &nodeadp))
            TEST(1 == nodes[pos].copycount) ;
            TEST(0 == nodes[pos].freecount) ;
            TEST(nrnodes-pos == length_arraysf(array)) ;
         }
         for (size_t pos = nrnodes; (pos --); ) {
            TEST(&nodes[pos].node == at_arraysf(array, pos)) ;
         }
         for (size_t pos = nrnodes; (pos --); ) {
            TEST(0 != at_arraysf(array, pos))
            TEST(0 == tryremove_arraysf(array, pos, &removed_node))
            TEST(removed_node == &nodes[pos].node) ;
            TEST(1 == nodes[pos].copycount) ;
            TEST(0 == nodes[pos].freecount) ;
            TEST(0 == at_arraysf(array, pos))
            TEST(pos == length_arraysf(array)) ;
            nodes[pos].copycount = 0 ;
            nodes[pos].freecount = 0 ;
         }
      }
   }

   // TEST random elements (insert_arraysf, remove_arraysf)
   TEST(0 == delete_arraysf(&array, 0)) ;
   TEST(0 == new_arraysf(&array, 2048, 1)) ;
   memset(nodes, 0, sizeof(testnode_t) * nrnodes) ;
   srand(99999) ;
   for (size_t count2 = 0; count2 < 10; ++count2) {
      for (size_t count = 0; count < nrnodes; ++count) {
         size_t pos = ((unsigned)rand() % nrnodes) ;
         if (nodes[pos].copycount) {
            TEST(&nodes[pos].node == at_arraysf(array, pos)) ;
            TEST(0 == tryremove_arraysf(array, pos, &removed_node)) ;
            TEST(removed_node == &nodes[pos].node) ;
            TEST(1 == nodes[pos].copycount) ;
            TEST(0 == nodes[pos].freecount) ;
            nodes[pos].copycount = 0 ;
         } else {
            inserted_node = 0 ;
            nodes[pos] = (testnode_t) { .node = arraysf_node_INIT(pos) } ;
            TEST(0 == tryinsert_arraysf(array, &nodes[pos].node, &inserted_node, &nodeadp)) ;
            TEST(inserted_node == &nodes[pos].node) ;
            TEST(1 == nodes[pos].copycount) ;
            TEST(0 == nodes[pos].freecount) ;
         }
      }
   }
   TEST(0 == delete_arraysf(&array, 0)) ;
   for (size_t pos = nrnodes; (pos --); ) {
      TEST(0 == nodes[pos].freecount) ;
   }

   // TEST delete_arraysf
   for (uint8_t posshift = 11; posshift < 18; posshift=(uint8_t)(posshift+3)) {
      TEST(0 == delete_arraysf(&array, 0)) ;
      TEST(0 == new_arraysf(&array, 16384, posshift)) ;
      memset(nodes, 0, sizeof(testnode_t) * nrnodes) ;
      unsigned nr = 0 ;
      for (size_t key = 4; key; key <<= 2) {
         for (size_t key2 = 0; key2 <= 11; ++key2) {
            size_t pos = key + key2 ;
            TEST(nr < nrnodes) ;
            nodes[nr] = (testnode_t) { .node = arraysf_node_INIT(pos) } ;
            TEST(0 == tryinsert_arraysf(array, &nodes[nr].node, &inserted_node, &nodeadp))
            TEST((++nr) == length_arraysf(array)) ;
         }
      }
      TEST(0 == delete_arraysf(&array, &nodeadp)) ;
      TEST(0 == array) ;
      for (unsigned pos = 0; pos < nrnodes; ++pos) {
         if (pos < nr) {
            TEST(1 == nodes[pos].copycount) ;
            TEST(1 == nodes[pos].freecount) ;
         } else {
            TEST(0 == nodes[pos].copycount) ;
            TEST(0 == nodes[pos].freecount) ;
         }
      }
   }

   // TEST delete_arraysf: lifetime.delete_object set to 0
   TEST(0 == new_arraysf(&array, 128, 0)) ;
   for (size_t pos = nrnodes; (pos --); ) {
      nodes[pos] = (testnode_t) { .node = arraysf_node_INIT(pos) } ;
      TEST(0 == tryinsert_arraysf(array, &nodes[pos].node, &inserted_node, 0))
      TEST(nrnodes-pos == length_arraysf(array)) ;
   }
   typeadapt.lifetime.delete_object = 0 ;
   TEST(0 == delete_arraysf(&array, &nodeadp)) ;
   typeadapt.lifetime.delete_object = &freenode_testnodeadapt ;
   TEST(0 == array) ;
   for (size_t pos = nrnodes; (pos --); ) {
      TEST(0 == nodes[pos].copycount) ;
      TEST(0 == nodes[pos].freecount) ;
   }

   // TEST delete_arraysf: nodeadp set to 0
   TEST(0 == new_arraysf(&array, 128, 0)) ;
   for (size_t pos = nrnodes; (pos --); ) {
      nodes[pos] = (testnode_t) { .node = arraysf_node_INIT(pos) } ;
      TEST(0 == tryinsert_arraysf(array, &nodes[pos].node, &inserted_node, 0))
      TEST(nrnodes-pos == length_arraysf(array)) ;
   }
   typeadapt.lifetime.delete_object = 0 ;
   TEST(0 == delete_arraysf(&array, 0)) ;
   typeadapt.lifetime.delete_object = &freenode_testnodeadapt ;
   TEST(0 == array) ;
   for (size_t pos = nrnodes; (pos --); ) {
      TEST(0 == nodes[pos].copycount) ;
      TEST(0 == nodes[pos].freecount) ;
   }

   // unprepare
   TEST(0 == free_vmblock(&memblock)) ;

   return 0 ;
ONABORT:
   (void) delete_arraysf(&array, 0) ;
   free_vmblock(&memblock) ;
   return EINVAL ;
}

static int test_error(void)
{
   const size_t    nrnodes  = 100000 ;
   vm_block_t      memblock = vm_block_INIT_FREEABLE ;
   arraysf_t       * array  = 0 ;
   arraysf_t       * array2 = 0 ;
   testnode_adapt_t  typeadapt = { typeadapt_INIT_LIFETIME(&copynode_testnodeadapt, &freenode_testnodeadapt), test_errortimer_INIT_FREEABLE } ;
   typeadapt_member_t nodeadp  = typeadapt_member_INIT(asgeneric_typeadapt(&typeadapt,testnode_adapt_t,testnode_t,void*), offsetof(testnode_t,node)) ;
   testnode_t      * nodes ;
   arraysf_node_t  * removed_node  = 0 ;
   arraysf_node_t  * inserted_node = 0 ;
   arraysf_node_t  * existing_node = 0 ;
   char            * logbuffer ;
   size_t          logbufsize1 ;
   size_t          logbufsize2 ;

   // prepare
   TEST(0 == init_vmblock(&memblock, (pagesize_vm()-1+nrnodes*sizeof(testnode_t)) / pagesize_vm() )) ;
   nodes = (testnode_t *) memblock.addr ;
   TEST(0 == new_arraysf(&array, 256, 0)) ;

   // TEST EINVAL
   TEST(EINVAL == new_arraysf(&array2, 0x800001/*too big*/, 0)) ;
   TEST(EINVAL == new_arraysf(&array2, 256, 8*sizeof(size_t)-8+1/*too big*/)) ;

   // TEST EEXIST
   nodes[0] = (testnode_t) { .node = arraysf_node_INIT(0) } ;
   nodes[1] = (testnode_t) { .node = arraysf_node_INIT(0) } ;
   TEST(0 == insert_arraysf(array, &nodes[0].node, &inserted_node, 0)) ;
   TEST(&nodes[0].node == inserted_node) ;
   GETBUFFER_LOG(&logbuffer, &logbufsize1) ;
   TEST(EEXIST == tryinsert_arraysf(array, &nodes[1].node, &existing_node, 0)) ;  // no log
   GETBUFFER_LOG(&logbuffer, &logbufsize2) ;
   TEST(logbufsize1 == logbufsize2) ;
   TEST(&nodes[0].node == existing_node) ;
   inserted_node = 0 ;
   TEST(EEXIST == insert_arraysf(array, &nodes[1].node, &inserted_node, 0)) ;     // log
   GETBUFFER_LOG(&logbuffer, &logbufsize2) ;
   TEST(logbufsize1 < logbufsize2) ;
   TEST(0 == inserted_node) ;

   // TEST ESRCH
   GETBUFFER_LOG(&logbuffer, &logbufsize1) ;
   arraysf_findresult_t found ;
   TEST(0 == at_arraysf(array, 1)) ;                              // no log
   TEST(ESRCH == find_arraysf(array, 1, &found)) ;                // no log
   TEST(ESRCH == tryremove_arraysf(array, 1, &removed_node)) ;    // no log
   GETBUFFER_LOG(&logbuffer, &logbufsize2) ;
   TEST(logbufsize1 == logbufsize2) ;
   TEST(ESRCH == remove_arraysf(array, 1, &removed_node)) ;       // log
   GETBUFFER_LOG(&logbuffer, &logbufsize2) ;
   TEST(logbufsize1 < logbufsize2) ;
   nodes[0].freecount = 0 ;
   TEST(0 == tryremove_arraysf(array, 0, &removed_node)) ;
   TEST(removed_node == &nodes[0].node) ;
   TEST(0 == nodes[0].freecount) ;

   // TEST delete_arraysf: ERROR
   for (size_t pos = 0; pos < nrnodes; ++pos) {
      nodes[pos] = (testnode_t) { .node = arraysf_node_INIT(pos) } ;
      TEST(0 == tryinsert_arraysf(array, &nodes[pos].node, &inserted_node, 0))
      nodes[pos].freecount = 0 ;
      TEST(pos == nodes[pos].node.pos) ;
      TEST(1+pos == length_arraysf(array)) ;
   }
   init_testerrortimer(&typeadapt.errcounter, 1, 12345) ;
   TEST(12345 == delete_arraysf(&array, &nodeadp)) ;
   for (unsigned pos = 0; pos < nrnodes; ++pos) {
      TEST((pos != 0) == nodes[pos].freecount) ;
   }

   // unprepare
   TEST(0 == free_vmblock(&memblock)) ;

   return 0 ;
ONABORT:
   (void) delete_arraysf(&array, 0) ;
   free_vmblock(&memblock) ;
   return EINVAL ;
}

static int test_iterator(void)
{
   const size_t   nrnodes  = 30000 ;
   vm_block_t     memblock = vm_block_INIT_FREEABLE ;
   arraysf_iterator_t iter = arraysf_iterator_INIT_FREEABLE ;
   arraysf_t      * array  = 0 ;
   testnode_t     * nodes ;
   arraysf_node_t * removed_node ;
   size_t         nextpos ;

   // prepare
   TEST(0 == init_vmblock(&memblock, (pagesize_vm()-1+nrnodes*sizeof(testnode_t)) / pagesize_vm() )) ;
   nodes = (testnode_t *) memblock.addr ;
   TEST(0 == new_arraysf(&array, 256, 8*sizeof(size_t)-8)) ;
   for (size_t i = 0; i < nrnodes; ++i) {
      nodes[i] = (testnode_t)  { .node = arraysf_node_INIT(i) } ;
      TEST(0 == insert_arraysf(array, &nodes[i].node, 0, 0)) ;
   }

   // TEST arraysf_iterator_INIT_FREEABLE
   TEST(0 == iter.stack) ;
   TEST(0 == iter.ri) ;

   // TEST initfirst_arraysfiterator, free_arraysfiterator
   iter.ri = 1 ;
   TEST(0 == initfirst_arraysfiterator(&iter, array)) ;
   TEST(0 != iter.stack) ;
   TEST(0 == iter.ri) ;
   TEST(0 == free_arraysfiterator(&iter)) ;
   TEST(0 == iter.stack) ;
   TEST(0 == iter.ri) ;
   TEST(0 == free_arraysfiterator(&iter)) ;
   TEST(0 == iter.stack) ;
   TEST(0 == iter.ri) ;

   // TEST next_arraysfiterator
   TEST(0 == initfirst_arraysfiterator(&iter, array)) ;
   nextpos = 0 ;
   for (iteratedtype_arraysf * node = 0; !node; node = (void*)1) {
      while (next_arraysfiterator(&iter, array, &node)) {
         TEST(node->pos == nextpos) ;
         ++ nextpos ;
      }
   }
   TEST(iter.ri == toplevelsize_arraysf(array)) ;
   TEST(nextpos == nrnodes) ;
   TEST(0 == free_arraysfiterator(&iter)) ;

   // TEST foreach all
   nextpos = 0 ;
   foreach (_arraysf, array, node) {
      TEST(((arraysf_node_t*)node)->pos == nextpos) ;
      ++ nextpos ;
   }
   TEST(nextpos == nrnodes) ;

   // TEST foreach_arraysf break after nrnodes/2
   nextpos = 0 ;
   foreach (_arraysf, array, node) {
      TEST(node->pos == nextpos) ;
      ++ nextpos ;
      if (nextpos == nrnodes/2) break ;
   }
   TEST(nextpos == nrnodes/2) ;

   // TEST foreach_arraysf (nrnodes/2 .. nrnodes) after remove
   for (size_t i = 0; i < nrnodes/2; ++i) {
      TEST(0 == remove_arraysf(array, i, &removed_node)) ;
      TEST(removed_node == &nodes[i].node) ;
   }
   nextpos = nrnodes/2 ;
   foreach (_arraysf, array, node) {
      TEST(node->pos == nextpos) ;
      ++ nextpos ;
   }
   TEST(nextpos == nrnodes) ;

   // unprepare
   TEST(0 == delete_arraysf(&array, 0)) ;
   TEST(0 == free_vmblock(&memblock)) ;

   return 0 ;
ONABORT:
   (void) delete_arraysf(&array, 0) ;
   free_vmblock(&memblock) ;
   return EINVAL ;
}

arraysf_IMPLEMENT(_tarraysf, testnode_t, node)
arraysf_IMPLEMENT(_t2arraysf, testnode_t, pos2)

static int test_generic(void)
{
   const size_t      nrnodes   = 300 ;
   vm_block_t        memblock  = vm_block_INIT_FREEABLE ;
   arraysf_t         * array   = 0 ;
   arraysf_t         * array2  = 0 ;
   testnode_adapt_t  typeadapt = { typeadapt_INIT_LIFETIME(&copynode_testnodeadapt, &freenode_testnodeadapt), test_errortimer_INIT_FREEABLE } ;
   typeadapt_member_t nodeadp1 = typeadapt_member_INIT(asgeneric_typeadapt(&typeadapt,testnode_adapt_t,testnode_t,void*), offsetof(testnode_t,node)) ;
   typeadapt_member_t nodeadp2 = typeadapt_member_INIT(asgeneric_typeadapt(&typeadapt,testnode_adapt_t,testnode_t,void*), offsetof(testnode_t,pos2)) ;
   test_errortimer_t memerror ;
   testnode_t        * nodes ;
   testnode_t        * inserted_node ;
   size_t            nextpos ;

   // prepare
   TEST(0 == init_vmblock(&memblock, (pagesize_vm()-1+nrnodes*sizeof(testnode_t)) / pagesize_vm() )) ;
   nodes = (testnode_t *) memblock.addr ;
   TEST(0 == new_tarraysf(&array, 256, 8*sizeof(size_t)-8)) ;
   TEST(0 == new_t2arraysf(&array2, 256, 8*sizeof(size_t)-8)) ;

   // TEST insert_arraysf: inserted_node parameter set to 0
   nodes[0] = (testnode_t)  { .node = arraysf_node_INIT(0), .pos2 = (100000u + 0) } ;
   TEST(0 == insert_tarraysf(array, &nodes[0], 0, &nodeadp1)) ;
   TEST(0 == insert_t2arraysf(array2, &nodes[0], 0, &nodeadp2)) ;

   // TEST tryinsert_arraysf: ENOMEM => inserted_node is set to 0
   nodes[1] = (testnode_t)  { .node = arraysf_node_INIT(1), .pos2 = (100000u + 1) } ;
   init_testerrortimer(&memerror, 1, ENOMEM) ;
   setresizeerr_testmm(mmcontext_testmm(), &memerror) ;
   inserted_node = &nodes[1] ;
   TEST(ENOMEM == tryinsert_tarraysf(array, &nodes[1], &inserted_node, &nodeadp1)) ;
   TEST(0 == inserted_node) ;
   TEST(1 == nodes[1].copycount) ;
   TEST(1 == nodes[1].freecount) ;
   init_testerrortimer(&memerror, 1, ENOMEM) ;
   setresizeerr_testmm(mmcontext_testmm(), &memerror) ;
   inserted_node = &nodes[1] ;
   TEST(ENOMEM == tryinsert_t2arraysf(array2, &nodes[1], &inserted_node, &nodeadp2)) ;
   TEST(0 == inserted_node) ;
   TEST(2 == nodes[1].copycount) ;
   TEST(2 == nodes[1].freecount) ;

   // TEST insert_arraysf
   for (size_t i = 1; i < nrnodes; i += 2) {
      nodes[i] = (testnode_t)  { .node = arraysf_node_INIT(i), .pos2 = (100000u + i) } ;
      inserted_node = 0 ;
      TEST(0 == insert_tarraysf(array, &nodes[i], &inserted_node, &nodeadp1)) ;
      TEST(inserted_node == &nodes[i]) ;
      inserted_node = 0 ;
      TEST(0 == insert_t2arraysf(array2, &nodes[i], &inserted_node, &nodeadp2)) ;
      TEST(inserted_node == &nodes[i]) ;
      TEST(2 == nodes[i].copycount) ;
      TEST(i/2+2 == length_tarraysf(array)) ;
      TEST(i/2+2 == length_t2arraysf(array2)) ;
   }

   // TEST tryinsert_arraysf
   for (size_t i = 2; i < nrnodes; i += 2) {
      nodes[i] = (testnode_t)  { .node = arraysf_node_INIT(i), .pos2 = (100000u + i) } ;
      inserted_node = 0 ;
      TEST(0 == tryinsert_tarraysf(array, &nodes[i], &inserted_node, &nodeadp1)) ;
      TEST(inserted_node == &nodes[i]) ;
      inserted_node = 0 ;
      TEST(0 == tryinsert_t2arraysf(array2, &nodes[i], &inserted_node, &nodeadp2)) ;
      TEST(inserted_node == &nodes[i]) ;
      TEST(2 == nodes[i].copycount) ;
      TEST((nrnodes+i)/2+1 == length_tarraysf(array)) ;
      TEST((nrnodes+i)/2+1 == length_t2arraysf(array2)) ;
   }

   // TEST tryinsert_arraysf: EEXIST
   for (size_t i = 1; i < nrnodes; ++i) {
      inserted_node = 0 ;
      TEST(EEXIST == tryinsert_tarraysf(array, &nodes[i], &inserted_node, &nodeadp1)) ;
      TEST(inserted_node == &nodes[i]) ;
      inserted_node = 0 ;
      TEST(EEXIST == tryinsert_t2arraysf(array2, &nodes[i], &inserted_node, &nodeadp2)) ;
      TEST(inserted_node == &nodes[i]) ;
      TEST(nrnodes == length_tarraysf(array)) ;
      TEST(nrnodes == length_t2arraysf(array2)) ;
   }

   // TEST at_arraysf: return value 0
   TEST(0 == at_tarraysf(array, nrnodes)) ;
   TEST(0 == at_t2arraysf(array2, 100000u + nrnodes)) ;

   // TEST at_arraysf
   for (size_t i = 0; i < nrnodes; ++i) {
      TEST(nodes[i].node.pos == i) ;
      TEST(nodes[i].pos2     == (100000u + i)) ;
      TEST(&nodes[i] == at_tarraysf(array, i)) ;
      TEST(&nodes[i] == at_t2arraysf(array2, 100000u + i)) ;
      TEST(2 == nodes[i].copycount) ;
   }

   // TEST foreach all
   nextpos = 0 ;
   foreach (_tarraysf, array, node) {
      TEST(node->node.pos == nextpos) ;
      ++ nextpos ;
   }
   TEST(nextpos == nrnodes) ;
   nextpos = 100000 ;
   foreach (_t2arraysf, array2, node) {
      TEST(node->pos2 == nextpos) ;
      ++ nextpos ;
   }
   TEST(nextpos == 100000+nrnodes) ;

   // TEST remove_arraysf
   for (size_t i = 0; i < nrnodes; ++i) {
      testnode_t * removed_node = 0 ;
      TEST(0 == remove_tarraysf(array, i, &removed_node)) ;
      TEST(&nodes[i] == removed_node) ;
      removed_node = 0 ;
      TEST(0 == remove_t2arraysf(array2, 100000u + i, &removed_node)) ;
      TEST(&nodes[i] == removed_node) ;
      TEST(nrnodes-1-i == length_tarraysf(array)) ;
      TEST(nrnodes-1-i == length_t2arraysf(array2)) ;
   }

   // TEST delete_arraysf
   for (size_t i = nrnodes; (i--); ) {
      nodes[i] = (testnode_t)  { .node = arraysf_node_INIT(i), .pos2 = (100000u + i) } ;
      TEST(0 == insert_tarraysf(array, &nodes[i], 0, &nodeadp1)) ;
      TEST(0 == insert_t2arraysf(array2, &nodes[i], 0, &nodeadp2)) ;
      TEST(2 == nodes[i].copycount) ;
      TEST(&nodes[i] == at_tarraysf(array, i)) ;
      TEST(&nodes[i] == at_t2arraysf(array2, 100000u + i)) ;
      TEST(2 == nodes[i].copycount) ;
   }
   TEST(0 == delete_tarraysf(&array, &nodeadp1)) ;
   TEST(0 == delete_t2arraysf(&array2, &nodeadp2)) ;
   for (size_t i = 0; i < nrnodes; ++i) {
      TEST(2 == nodes[i].freecount) ;
   }

   // unprepare
   TEST(0 == free_vmblock(&memblock)) ;

   return 0 ;
ONABORT:
   TEST(0 == delete_tarraysf(&array, 0)) ;
   TEST(0 == delete_t2arraysf(&array2, 0)) ;
   free_vmblock(&memblock) ;
   return EINVAL ;
}

int unittest_ds_inmem_arraysf()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())       goto ONABORT ;
   if (test_error())          goto ONABORT ;
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
