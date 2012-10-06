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
#include "C-kern/api/ds/inmem/arraysf.h"
#include "C-kern/api/ds/inmem/binarystack.h"
#include "C-kern/api/ds/inmem/node/arraysf_node.h"
#include "C-kern/api/ds/foreach.h"
#include "C-kern/api/ds/typeadapter.h"
#include "C-kern/api/err.h"
#include "C-kern/api/math/int/log2.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/platform/virtmemory.h"
#endif


typedef struct arraysf_pos_t           arraysf_pos_t ;


struct arraysf_pos_t {
   arraysf_mwaybranch_t * branch ;
   unsigned             ci ;
} ;


// section: arraysf_t

// group: helper

static uint16_t s_arraysf_nrelemroot[4] = {
    [arraysf_6BITROOT_UNSORTED] = 64
   ,[arraysf_8BITROOT_UNSORTED] = 256
   ,[arraysf_MSBPOSROOT] = 1/*0 value*/ + (8 * sizeof(size_t)/*in bits*/) / 2 * 3   // per 2 bit 3 entries (1 bit always set)
   ,[arraysf_8BITROOT24] = 256
} ;

#define nrelemroot_arraysf(array)      (s_arraysf_nrelemroot[(array)->type])

#define objectsize_arraysf(type)       (sizeof(arraysf_t) + sizeof(arraysf_node_t*) * s_arraysf_nrelemroot[type])

typedef struct arraysf_findresult_t    arraysf_findresult_t ;

struct arraysf_findresult_t {
   unsigned             rootindex ;
   unsigned             childindex ;
   unsigned             pchildindex ;
   arraysf_mwaybranch_t * parent ;
   arraysf_mwaybranch_t * pparent ;
   arraysf_unode_t      * found_node ;
   size_t               found_pos ;
} ;

static int find_arraysf(const arraysf_t * array, size_t pos, /*err;out*/arraysf_findresult_t * result, size_t offset_node)
{
   int err ;
   unsigned rootindex ;

   switch (array->type) {
   case arraysf_6BITROOT_UNSORTED:  rootindex = (0x3F & pos) ;
                                    break ;
   case arraysf_8BITROOT_UNSORTED:  rootindex = (0xFF & pos) ;
                                    break ;
   case arraysf_MSBPOSROOT:         rootindex  = (log2_int(pos) & ~0x01u) ;
                                    rootindex += (0x03u & (pos >> rootindex)) + (rootindex >> 1) ;
                                    break ;
   case arraysf_8BITROOT24:         rootindex = (0xFF & (pos >> 24)) ;
                                    break ;
   default:                         return EINVAL ;
   }

   arraysf_unode_t      * node      = array->root[rootindex] ;
   arraysf_mwaybranch_t * pparent   = 0 ;
   arraysf_mwaybranch_t * parent    = 0 ;
   unsigned             childindex  = 0 ;
   unsigned             pchildindex = 0 ;

   err = ESRCH ;

   for(; node; ) {

      if (isbranchtype_arraysfunode(node)) {
         pparent = parent ;
         parent  = asbranch_arraysfunode(node) ;
         pchildindex = childindex   ;
         childindex  = childindex_arraysfmwaybranch(parent, pos) ;
         node        = parent->child[childindex] ;
      } else {
         result->found_pos = asnode_arraysfunode(node, offset_node)->pos ;
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

int new_arraysf(/*out*/arraysf_t ** array, arraysf_e type)
{
   int err ;
   memblock_t  new_obj = memblock_INIT_FREEABLE ;

   VALIDATE_INPARAM_TEST(*array == 0, ONABORT, ) ;

   VALIDATE_INPARAM_TEST(type < nrelementsof(s_arraysf_nrelemroot), ONABORT, PRINTINT_LOG(type)) ;

   const size_t objsize = objectsize_arraysf(type) ;

   err = MM_RESIZE(objsize, &new_obj) ;
   if (err) goto ONABORT ;

   memset(new_obj.addr, 0, objsize) ;
   ((arraysf_t*)(new_obj.addr))->type  = type ;

   *array = ((arraysf_t*)(new_obj.addr)) ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int delete_arraysf(arraysf_t ** array, struct typeadapter_iot * typeadp)
{
   int err = 0 ;
   int err2 ;
   arraysf_t * del_obj = *array ;

   if (del_obj) {
      *array = 0 ;

      for(unsigned i = 0; i < nrelemroot_arraysf(del_obj); ++i) {

         if (!del_obj->root[i]) continue ;

         arraysf_unode_t * node = del_obj->root[i] ;

         if (!isbranchtype_arraysfunode(node)) {
            if (typeadp) {
               err2 = execfree_typeadapteriot(typeadp, asgeneric_arraysfunode(node)) ;
               if (err2) err = err2 ;
            }
            continue ;
         }

         arraysf_mwaybranch_t * branch = asbranch_arraysfunode(node) ;
         node = branch->child[0] ;
         branch->child[0] = 0 ;
         branch->used = nrelementsof(branch->child)-1 ;

         for(;;) {

            for(;;) {
               if (node) {
                  if (isbranchtype_arraysfunode(node)) {
                     arraysf_mwaybranch_t * parent = branch ;
                     branch = asbranch_arraysfunode(node) ;
                     node = branch->child[0] ;
                     branch->child[0] = (arraysf_unode_t*) parent ;
                     branch->used     = nrelementsof(branch->child)-1 ;
                     continue ;
                  } else if (typeadp) {
                     err2 = execfree_typeadapteriot(typeadp, asgeneric_arraysfunode(node)) ;
                     if (err2) err = err2 ;
                  }
               }

               if (! branch->used)  break ;
               node = branch->child[branch->used --] ;
            }

            do {
               arraysf_mwaybranch_t * parent = (arraysf_mwaybranch_t *) branch->child[0] ;
               memblock_t           mblock   = memblock_INIT(sizeof(arraysf_mwaybranch_t), (uint8_t*)branch) ;
               err2 = MM_FREE(&mblock) ;
               if (err2) err = err2 ;
               branch = parent ;
            } while(branch && !branch->used) ;

            if (!branch) break ;

            node = branch->child[branch->used --] ;
         }

      }

      const size_t objsize = objectsize_arraysf(del_obj->type) ;

      memblock_t mblock = memblock_INIT(objsize, (uint8_t*)del_obj) ;
      err2 = MM_FREE(&mblock) ;
      if (err2) err = err2 ;
   }

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

// group: query

struct generic_object_t * at_arraysf(const arraysf_t * array, size_t pos, size_t offset_node)
{
   int err ;
   arraysf_findresult_t found ;

   err = find_arraysf(array, pos, &found, offset_node) ;
   if (err) return 0 ;

   return asgeneric_arraysfunode(found.found_node) ;
}

// group: change

int tryinsert_arraysf(arraysf_t * array, struct generic_object_t * node, /*out;err*/struct generic_object_t ** inserted_or_existing_node, struct typeadapter_iot * typeadp/*0=>no copy is made*/, size_t offset_node)
{
   int err ;
   arraysf_findresult_t found ;
   typeof(node)         copied_node = 0 ;
   size_t               pos         = asnode_arraysfunode(fromgeneric_arraysfunode(node), offset_node)->pos ;

   err = find_arraysf(array, pos, &found, offset_node) ;
   if (ESRCH != err) {
      if (inserted_or_existing_node) {
         *inserted_or_existing_node = (0 == err) ? asgeneric_arraysfunode(found.found_node) : 0 ;
      }
      return (0 == err) ? EEXIST : err ;
   }

   if (typeadp) {
      err = execcopy_typeadapteriot(typeadp, &copied_node, node) ;
      if (err) goto ONABORT ;
      node = copied_node ;
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
         err = MM_RESIZE(sizeof(arraysf_mwaybranch_t), &mblock) ;
         if (err) goto ONABORT ;

         arraysf_mwaybranch_t * new_branch = (arraysf_mwaybranch_t *) mblock.addr ;

         init_arraysfmwaybranch(new_branch, shift, pos2, found.found_node, pos, fromgeneric_arraysfunode(node)) ;

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
         array->root[found.rootindex] = fromgeneric_arraysfunode(node) ;
         goto DONE ;
      }

   // get pos of already stored node / check prefix match => second simple case

      arraysf_mwaybranch_t * branch = found.parent ;
      for(unsigned i = nrelementsof(branch->child); (i--); ) {
         if (branch->child[i]) {
            if (isbranchtype_arraysfunode(branch->child[i])) {
               branch = asbranch_arraysfunode(branch->child[i]) ;
               i = nrelementsof(branch->child) ;
               continue ;
            }
            pos2    = asnode_arraysfunode(branch->child[i], offset_node)->pos ;
            posdiff = (pos ^ pos2) ;
            break ;
         }
      }

      unsigned prefix = ~0x03u & (posdiff >> found.parent->shift) ;

      if (0 == prefix) {
         // prefix does match
         found.parent->child[found.childindex] = fromgeneric_arraysfunode(node) ;
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

   while(branch->shift > shift) {
      parent = branch ;
      childindex = childindex_arraysfmwaybranch(branch, pos) ;
      arraysf_unode_t * child = branch->child[childindex] ;
      assert(child) ;
      assert(isbranchtype_arraysfunode(child)) ;
      branch = asbranch_arraysfunode(child) ;
   }

   memblock_t mblock = memblock_INIT_FREEABLE ;
   err = MM_RESIZE(sizeof(arraysf_mwaybranch_t), &mblock) ;
   if (err) goto ONABORT ;

   arraysf_mwaybranch_t * new_branch = (arraysf_mwaybranch_t*) mblock.addr ;

   init_arraysfmwaybranch(new_branch, shift, pos2, asunode_arraysfmwaybranch(branch), pos, fromgeneric_arraysfunode(node)) ;

   if (parent) {
      parent->child[childindex] = asunode_arraysfmwaybranch(new_branch) ;
   } else {
      array->root[found.rootindex] = asunode_arraysfmwaybranch(new_branch) ;
   }

DONE:

   ++ array->length ;

   if (inserted_or_existing_node) {
      *inserted_or_existing_node = node ;
   }

   return 0 ;
ONABORT:
   if (copied_node) {
      (void) execfree_typeadapteriot(typeadp, copied_node) ;
   }
   TRACEABORT_LOG(err) ;
   return err ;
}

int tryremove_arraysf(arraysf_t * array, size_t pos, /*out*/struct generic_object_t ** removed_node/*could be 0*/, size_t offset_node)
{
   int err ;
   arraysf_findresult_t found ;

   err = find_arraysf(array, pos, &found, offset_node) ;
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

         for(unsigned i = nrelementsof(found.parent->child)-1; ; ) {
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
         err = MM_FREE(&mblock) ;
         (void) err /*IGNORE*/ ;
      }

   }

   assert(array->length > 0) ;
   -- array->length ;

   if (removed_node) {
      *removed_node = asgeneric_arraysfunode(found.found_node) ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int remove_arraysf(arraysf_t * array, size_t pos, /*out*/struct generic_object_t ** removed_node/*could be 0*/, size_t offset_node)
{
   int err ;

   err = tryremove_arraysf(array, pos, removed_node, offset_node) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int insert_arraysf(arraysf_t * array, struct generic_object_t * node, /*out*/struct generic_object_t ** inserted_node/*0=>not returned*/, struct typeadapter_iot * typeadp/*0=>no copy is made*/, size_t offset_node)
{
   int err ;
   struct generic_object_t * inserted_or_existing_node ;

   err = tryinsert_arraysf(array, node, &inserted_or_existing_node, typeadp, offset_node) ;
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

int initfirst_arraysfiterator(/*out*/arraysf_iterator_t * iter, arraysf_t * array)
{
   int err ;
   size_t        objectsize = sizeof(binarystack_t) ;
   memblock_t    objectmem  = memblock_INIT_FREEABLE ;
   binarystack_t * stack  = 0 ;

   (void) array ;

   err = MM_RESIZE(objectsize, &objectmem) ;
   if (err) goto ONABORT ;

   stack  = (binarystack_t *) objectmem.addr ;

   err = init_binarystack(stack, (/*max depth*/4 * sizeof(size_t)) * /*objectsize*/sizeof(arraysf_pos_t) ) ;
   if (err) goto ONABORT ;

   iter->stack = stack ;
   iter->ri    = 0 ;

   return 0 ;
ONABORT:
   MM_FREE(&objectmem) ;
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

      err2 = MM_FREE(&objectmem) ;
      if (err2) err = err2 ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

bool next_arraysfiterator(arraysf_iterator_t * iter, arraysf_t * array, /*out*/struct generic_object_t ** node)
{
   int err ;
   size_t         nrelemroot = nrelemroot_arraysf(array) ;
   arraysf_pos_t  * pos ;

   for (;;) {

      if (isempty_binarystack(iter->stack)) {

         arraysf_unode_t * rootnode ;

         for(;;) {
            if (iter->ri >= nrelemroot) {
               return false ;
            }
            rootnode = array->root[iter->ri ++] ;
            if (rootnode) {
               if (!isbranchtype_arraysfunode(rootnode)) {
                  *node = asgeneric_arraysfunode(rootnode) ;
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
               *node = asgeneric_arraysfunode(childnode) ;
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

struct testnode_t {
   arraysf_node_t node ;
   int            copycount ;
   arraysf_node_EMBED(pos2)
   int            freecount ;
} ;

static int test_copynode(typeadapter_t * typeimpl, testnode_t ** copied_node, testnode_t * node)
{
   (void) typeimpl ;
   ++ node->copycount ;
   *copied_node = node ;
   return 0 ;
}

static int test_freenode(typeadapter_t * typeimpl, testnode_t * node)
{
   (void) typeimpl ;
   ++ node->freecount ;
   return 0 ;
}

static int test_freenodeerr(typeadapter_t * typeimpl, testnode_t * node)
{
   (void) typeimpl ;
   ++ node->freecount ;
   return 12345 ;
}

arraysf_IMPLEMENT(_tarraysf, testnode_t, node.pos)

typeadapter_it_DECLARE(testnode_typeadapt_it, typeadapter_t, testnode_t)

static int test_initfree(void)
{
   const size_t    nrnodes    = 100000 ;
   vm_block_t      memblock   = vm_block_INIT_FREEABLE ;
   arraysf_t       * array    = 0 ;
   testnode_typeadapt_it  typeadt_ft = typeadapter_it_INIT(&test_copynode, &test_freenode) ;
   typeadapter_iot        typeadt    = typeadapter_iot_INIT(0, &typeadt_ft.generic) ;
   testnode_t      * nodea ;

   // prepare
   TEST(0 == init_vmblock(&memblock, (pagesize_vm()-1+nrnodes*sizeof(testnode_t)) / pagesize_vm() )) ;
   nodea = (testnode_t *) memblock.addr ;

   // TEST static init
   TEST(0 == array) ;

   // TEST init, double free
   for(arraysf_e type = arraysf_6BITROOT_UNSORTED; type <= arraysf_8BITROOT24; ++type) {
      TEST(0 == new_arraysf(&array, type)) ;
      TEST(0 != array) ;
      TEST(type == array->type) ;
      TEST(0 == length_arraysf(array)) ;
      switch (type) {
      case arraysf_6BITROOT_UNSORTED:  TEST(nrelemroot_arraysf(array) == 64) ;
                                       break ;
      case arraysf_8BITROOT_UNSORTED:
      case arraysf_8BITROOT24:         TEST(nrelemroot_arraysf(array) == 256) ;
                                       break ;
      case arraysf_MSBPOSROOT:         TEST(nrelemroot_arraysf(array) == 1+sizeof(size_t)*12) ;
                                       break ;
      }
      for(unsigned i = 0; i < nrelemroot_arraysf(array); ++i) {
         TEST(0 == array->root[i]) ;
      }
      TEST(0 == delete_arraysf(&array, 0)) ;
      TEST(0 == array) ;
      TEST(0 == delete_arraysf(&array, 0)) ;
      TEST(0 == array) ;
   }

   // TEST root distributions
   for(arraysf_e type = arraysf_6BITROOT_UNSORTED; type <= arraysf_8BITROOT24; ++type) {
      TEST(0 == new_arraysf(&array, type)) ;
      for(size_t pos1 = 0, pos2 = 0; pos1 < 256; ++ pos1, pos2 = (pos2 + (pos2 == 0)) << 1) {
         size_t      pos             = pos1 + pos2 ;
         testnode_t  node            = { .node = arraysf_node_INIT(pos) } ;
         testnode_t  * inserted_node = 0 ;
         TEST(0 == tryinsert_tarraysf(array, &node, &inserted_node, 0)) ;
         TEST(inserted_node == &node) ;
         TEST(1 == array->length) ;
         size_t ri ;
         switch (type) {
         case arraysf_6BITROOT_UNSORTED:     ri = (pos & 63) ; break ;
         case arraysf_8BITROOT_UNSORTED:     ri = (pos & 255) ; break ;
         case arraysf_MSBPOSROOT:   ri = ~0x01u & log2_int(pos) ;
                                    ri = 3 * (ri/2) + ((pos >> ri) & 0x03u) ; break ;
         case arraysf_8BITROOT24:   ri = (pos >> 24) ; break ;
         }
         TEST(ri < nrelemroot_arraysf(array)) ;
         TEST(&node.node == asnode_arraysfunode(array->root[ri], offsetof(testnode_t,node))) ;
         for(unsigned i = 0; i < nrelemroot_arraysf(array); ++i) {
            if (i == ri) continue ;
            TEST(0 == array->root[i]) ;
         }
         TEST(0 == tryremove_tarraysf(array, pos, 0)) ;
         TEST(0 == array->length) ;
         TEST(0 == array->root[ri]) ;
      }
      TEST(0 == delete_arraysf(&array, 0)) ;
   }

   // TEST arraysf_8BITROOT24: root distribution
   TEST(0 == new_arraysf(&array, arraysf_8BITROOT24)) ;
   for(size_t pos = 0, ri = 0; (ri == 0) || pos; pos += ((~(SIZE_MAX >> 1)) >> 7), ++ri) {
      testnode_t  node            = { .node = arraysf_node_INIT(pos+ri) } ;
      testnode_t  * inserted_node = 0 ;
      TEST(0 == tryinsert_tarraysf(array, &node, &inserted_node, 0)) ;
      TEST(inserted_node == &node) ;
      TEST(1 == length_tarraysf(array)) ;
      TEST(pos+ri == node.node.pos) ;
      TEST(ri < nrelemroot_arraysf(array)) ;
      TEST(&node.node == asnode_arraysfunode(array->root[ri], offsetof(testnode_t,node))) ;
      for(unsigned i = 0; i < nrelemroot_arraysf(array); ++i) {
         if (i == ri) continue ;
         TEST(0 == array->root[i]) ;
      }
      testnode_t * removed_node = 0 ;
      TEST(0 == tryremove_tarraysf(array, pos+ri, &removed_node)) ;
      TEST(0 == length_arraysf(array)) ;
      TEST(0 == array->root[ri]) ;
      TEST(inserted_node == removed_node) ;
   }
   TEST(0 == delete_arraysf(&array, 0)) ;

   // TEST insert_arraysf (1 level)
   TEST(0 == new_arraysf(&array, arraysf_MSBPOSROOT)) ;
   nodea[4].node = (arraysf_node_t) arraysf_node_INIT(4) ;
   TEST(0 == tryinsert_tarraysf(array, &nodea[4], 0, &typeadt))
   TEST(&nodea[4].node == asnode_arraysfunode(array->root[4], offsetof(testnode_t,node))) ;
   for(size_t pos = 5; pos <= 7; ++pos) {
      testnode_t * inserted_node = 0 ;
      nodea[pos] = (testnode_t) { .node = arraysf_node_INIT(pos) } ;
      TEST(0 == tryinsert_tarraysf(array, &nodea[pos], &inserted_node, &typeadt))
      TEST(inserted_node == &nodea[pos]) ;
      TEST(0 == nodea[pos].freecount) ;
      TEST(1 == nodea[pos].copycount) ;
      TEST(pos-3 == length_arraysf(array)) ;
      TEST(isbranchtype_arraysfunode(array->root[4])) ;
      TEST(0 == asbranch_arraysfunode(array->root[4])->shift) ;
      TEST(pos-3 == asbranch_arraysfunode(array->root[4])->used) ;
   }
   for(size_t pos = 4; pos <= 7; ++pos) {
      TEST(&nodea[pos].node == asnode_arraysfunode(asbranch_arraysfunode(array->root[4])->child[pos-4], offsetof(testnode_t,node))) ;
      TEST(&nodea[pos]      == at_tarraysf(array, pos)) ;
      TEST(0                == at_tarraysf(array, 10*pos+4)) ;
   }

   // TEST remove_arraysf (1 level)
   for(size_t pos = 4; pos <= 7; ++pos) {
      testnode_t * removed_node = 0 ;
      TEST(0 == tryremove_tarraysf(array, pos, &removed_node))
      TEST(&nodea[pos] == removed_node) ;
      TEST(1 == nodea[pos].copycount) ;
      TEST(0 == nodea[pos].freecount) ;
      TEST(0 == at_tarraysf(array, pos)) ;
      if (pos < 6) {
         TEST(array->root[4]) ;
         TEST(isbranchtype_arraysfunode(array->root[4])) ;
      } else if (pos == 6) {
         TEST(&nodea[7].node == asnode_arraysfunode(array->root[4], offsetof(testnode_t,node))) ;
      } else {
         TEST(! array->root[4]) ;
      }
      TEST(7-pos == length_tarraysf(array)) ;
   }

   // TEST insert_arraysf (2 levels)
   arraysf_mwaybranch_t * branch1 = 0 ;
   for(size_t pos = 16; pos <= 31; ++pos) {
      nodea[pos] = (testnode_t) { .node = arraysf_node_INIT(pos) } ;
      TEST(0 == tryinsert_tarraysf(array, &nodea[pos], 0, &typeadt))
      TEST(1 == nodea[pos].copycount) ;
      TEST(0 == nodea[pos].freecount) ;
      TEST(pos-15 == length_tarraysf(array)) ;
      if (pos == 16) {
         TEST(&nodea[16].node == asnode_arraysfunode(array->root[7], offsetof(testnode_t,node))) ;
      } else if (pos == 17) {
         TEST(isbranchtype_arraysfunode(array->root[7])) ;
         branch1 = asbranch_arraysfunode(array->root[7]) ;
         TEST(0 == branch1->shift) ;
         TEST(&nodea[16].node  == asnode_arraysfunode(branch1->child[0], offsetof(testnode_t,node))) ;
         TEST(&nodea[17].node  == asnode_arraysfunode(branch1->child[1], offsetof(testnode_t,node))) ;
      } else if (pos <= 19) {
         TEST(isbranchtype_arraysfunode(array->root[7])) ;
         TEST(branch1 == asbranch_arraysfunode(array->root[7])) ;
         TEST(&nodea[pos].node == asnode_arraysfunode(branch1->child[pos-16], offsetof(testnode_t,node))) ;
      } else if (pos == 20 || pos == 24 || pos == 28) {
         TEST(isbranchtype_arraysfunode(array->root[7])) ;
         if (pos == 20) {
            arraysf_mwaybranch_t * branch2 = asbranch_arraysfunode(array->root[7]) ;
            TEST(2 == branch2->shift) ;
            TEST(branch1 == asbranch_arraysfunode(branch2->child[0])) ;
            branch1 = branch2 ;
         }
         TEST(&nodea[pos].node == asnode_arraysfunode(branch1->child[(pos-16)/4], offsetof(testnode_t,node))) ;
      } else {
         TEST(isbranchtype_arraysfunode(array->root[7])) ;
         TEST(branch1 == asbranch_arraysfunode(array->root[7])) ;
         TEST(isbranchtype_arraysfunode(branch1->child[(pos-16)/4])) ;
         arraysf_mwaybranch_t * branch2 = asbranch_arraysfunode(branch1->child[(pos-16)/4]) ;
         TEST(&nodea[pos&~0x03u].node == asnode_arraysfunode(branch2->child[0], offsetof(testnode_t,node))) ;
         TEST(&nodea[pos].node        == asnode_arraysfunode(branch2->child[pos&0x03u], offsetof(testnode_t,node))) ;
      }
   }

   // TEST remove_arraysf (2 levels)
   for(size_t pos = 16; pos <= 31; ++pos) {
      testnode_t * removed_node = 0 ;
      TEST(0 == tryremove_tarraysf(array, pos, &removed_node)) ;
      TEST(&nodea[pos] == removed_node) ;
      TEST(1 == nodea[pos].copycount) ;
      TEST(0 == nodea[pos].freecount) ;
      TEST(0 == at_tarraysf(array, pos))
      TEST(31-pos == length_tarraysf(array)) ;
      if (pos <= 17) {
         TEST(1 == isbranchtype_arraysfunode(asbranch_arraysfunode(array->root[7])->child[0])) ;
      } else if (pos == 18) {
         TEST(&nodea[19].node == asnode_arraysfunode(asbranch_arraysfunode(array->root[7])->child[0], offsetof(testnode_t,node))) ;
      } else if (pos == 19) {
         TEST(0 == asbranch_arraysfunode(array->root[7])->child[0]) ;
      } else if (pos < 22) {
         TEST(1 == isbranchtype_arraysfunode(asbranch_arraysfunode(array->root[7])->child[1])) ;
      } else if (pos == 22) {
         TEST(1 == isbranchtype_arraysfunode(array->root[7])) ;
         TEST(2 == asbranch_arraysfunode(array->root[7])->shift) ;
         TEST(&nodea[23].node == asnode_arraysfunode(asbranch_arraysfunode(array->root[7])->child[1], offsetof(testnode_t,node))) ;
      } else if (pos <= 26) {
         TEST(1 == isbranchtype_arraysfunode(array->root[7])) ;
         TEST(2 == asbranch_arraysfunode(array->root[7])->shift) ;
      } else if (pos <= 29) {
         TEST(1 == isbranchtype_arraysfunode(array->root[7])) ;
         TEST(0 == asbranch_arraysfunode(array->root[7])->shift) ;
      } else if (pos == 30) {
         TEST(&nodea[31].node == asnode_arraysfunode(array->root[7], offsetof(testnode_t,node))) ;
      } else if (pos == 31) {
         TEST(0 == array->root[7]) ;
      }
   }

   // TEST insert_arraysf at_arraysf remove_arraysf forward
   for(arraysf_e type = arraysf_6BITROOT_UNSORTED; type <= arraysf_8BITROOT24; ++type) {
      TEST(0 == delete_arraysf(&array, 0)) ;
      TEST(0 == new_arraysf(&array, type)) ;
      for(size_t pos = 0; pos < nrnodes; ++pos) {
         testnode_t * inserted_node = 0 ;
         nodea[pos] = (testnode_t) { .node = arraysf_node_INIT(pos) } ;
         TEST(0 == tryinsert_tarraysf(array, &nodea[pos], &inserted_node, 0))
         TEST(inserted_node == &nodea[pos]) ;
         TEST(1+pos == length_arraysf(array)) ;
      }
      for(size_t pos = 0; pos < nrnodes; ++pos) {
         TEST(&nodea[pos] == at_tarraysf(array, pos)) ;
      }
      for(size_t pos = 0; pos < nrnodes; ++pos) {
         TEST(0 != at_tarraysf(array, pos))
         testnode_t * removed_node = 0 ;
         TEST(0 == tryremove_tarraysf(array, pos, &removed_node))
         TEST(0 == nodea[pos].copycount) ;
         TEST(0 == nodea[pos].freecount) ;
         TEST(removed_node == &nodea[pos]) ;
         TEST(0 == at_tarraysf(array, pos))
         TEST(nrnodes-1-pos == length_tarraysf(array)) ;
      }
   }

   // TEST insert_arraysf at_arraysf remove_arraysf backward
   for(arraysf_e type = arraysf_6BITROOT_UNSORTED; type <= arraysf_8BITROOT24; ++type) {
      TEST(0 == delete_tarraysf(&array, 0)) ;
      TEST(0 == new_tarraysf(&array, type)) ;
      for(size_t pos = nrnodes; (pos --); ) {
         nodea[pos] = (testnode_t) { .node = arraysf_node_INIT(pos) } ;
         TEST(0 == tryinsert_tarraysf(array, &nodea[pos], 0, &typeadt))
         TEST(1 == nodea[pos].copycount) ;
         TEST(0 == nodea[pos].freecount) ;
         TEST(nrnodes-pos == length_tarraysf(array)) ;
      }
      for(size_t pos = nrnodes; (pos --); ) {
         TEST(&nodea[pos] == at_tarraysf(array, pos)) ;
      }
      for(size_t pos = nrnodes; (pos --); ) {
         TEST(0 != at_tarraysf(array, pos))
         testnode_t * removed_node = 0 ;
         TEST(0 == tryremove_tarraysf(array, pos, &removed_node))
         TEST(removed_node == &nodea[pos]) ;
         TEST(1 == nodea[pos].copycount) ;
         TEST(0 == nodea[pos].freecount) ;
         TEST(0 == at_tarraysf(array, pos))
         TEST(pos == length_tarraysf(array)) ;
         nodea[pos].copycount = 0 ;
         nodea[pos].freecount = 0 ;
      }
   }

   // TEST random elements (insert_arraysf, remove_arraysf)
   memset(nodea, 0, sizeof(testnode_t) * nrnodes) ;
   srand(99999) ;
   for(size_t count2 = 0; count2 < 10; ++count2) {
      for(size_t count = 0; count < nrnodes; ++count) {
         size_t pos = ((unsigned)rand() % nrnodes) ;
         if (nodea[pos].copycount) {
            testnode_t * removed_node = (void*) 0 ;
            TEST(&nodea[pos] == at_tarraysf(array, pos)) ;
            TEST(0 == tryremove_tarraysf(array, pos, &removed_node)) ;
            TEST(removed_node == &nodea[pos]) ;
            TEST(1 == nodea[pos].copycount) ;
            TEST(0 == nodea[pos].freecount) ;
            nodea[pos].copycount = 0 ;
         } else {
            testnode_t * inserted_node = (void*) 0 ;
            nodea[pos] = (testnode_t) { .node = arraysf_node_INIT(pos) } ;
            TEST(0 == tryinsert_tarraysf(array, &nodea[pos], &inserted_node, &typeadt)) ;
            TEST(inserted_node == &nodea[pos]) ;
            TEST(1 == nodea[pos].copycount) ;
            TEST(0 == nodea[pos].freecount) ;
         }
      }
   }
   TEST(0 == delete_tarraysf(&array, 0)) ;
   for(size_t pos = nrnodes; (pos --); ) {
      TEST(0 == nodea[pos].freecount) ;
   }

   // TEST delete_arraysf frees memory and no nodes
   for(arraysf_e type = arraysf_6BITROOT_UNSORTED; type <= arraysf_8BITROOT24; ++type) {
      TEST(0 == new_tarraysf(&array, type)) ;
      for(size_t pos = nrnodes; (pos --); ) {
         nodea[pos] = (testnode_t) { .node = arraysf_node_INIT(pos) } ;
         TEST(0 == tryinsert_tarraysf(array, &nodea[pos], 0, 0))
         TEST(nrnodes-pos == length_tarraysf(array)) ;
      }
      TEST(0 == delete_tarraysf(&array, &typeadt)) ;
      TEST(0 == array) ;
      for(size_t pos = nrnodes; (pos --); ) {
         TEST(0 == nodea[pos].copycount) ;
         TEST(1 == nodea[pos].freecount) ;
      }
   }

   // TEST delete_arraysf frees memory and nodes
   for(arraysf_e type = arraysf_6BITROOT_UNSORTED; type <= arraysf_8BITROOT24; ++type) {
      memset(nodea, 0, sizeof(testnode_t) * nrnodes) ;
      TEST(0 == new_tarraysf(&array, type)) ;
      unsigned nr = 0 ;
      for(size_t key = 4; key; key <<= 2) {
         for(size_t key2 = 0; key2 <= 11; ++key2) {
            size_t pos = key + key2 ;
            TEST(nr < nrnodes) ;
            nodea[nr] = (testnode_t) { .node = arraysf_node_INIT(pos) } ;
            TEST(0 == tryinsert_tarraysf(array, &nodea[nr], 0, &typeadt))
            TEST((++nr) == length_tarraysf(array)) ;
         }
      }
      TEST(0 == delete_tarraysf(&array, &typeadt)) ;
      TEST(0 == array) ;
      for(unsigned pos = 0; pos < nrnodes; ++pos) {
         if (pos < nr) {
            TEST(1 == nodea[pos].copycount) ;
            TEST(1 == nodea[pos].freecount) ;
         } else {
            TEST(0 == nodea[pos].copycount) ;
            TEST(0 == nodea[pos].freecount) ;
         }
      }
   }

   // unprepare
   TEST(0 == free_vmblock(&memblock)) ;

   return 0 ;
ONABORT:
   (void) delete_tarraysf(&array, 0) ;
   free_vmblock(&memblock) ;
   return EINVAL ;
}

static int test_error(void)
{
   const size_t    nrnodes    = 100000 ;
   vm_block_t      memblock   = vm_block_INIT_FREEABLE ;
   testnode_typeadapt_it  typeadt_ft = typeadapter_it_INIT_FREEABLE ;
   typeadapter_iot        typeadt    = typeadapter_iot_INIT(0, &typeadt_ft.generic) ;
   arraysf_t       * array    = 0 ;
   testnode_t      * nodea ;
   char            * logbuffer ;
   size_t          logbufsize1 ;
   size_t          logbufsize2 ;

   // prepare
   TEST(0 == init_vmblock(&memblock, (pagesize_vm()-1+nrnodes*sizeof(testnode_t)) / pagesize_vm() )) ;
   nodea = (testnode_t *) memblock.addr ;
   TEST(0 == new_tarraysf(&array, arraysf_MSBPOSROOT)) ;

   // TEST EINVAL
      // (array != 0)
   GETBUFFER_LOG(&logbuffer, &logbufsize1) ;
   TEST(EINVAL == new_tarraysf(&array, arraysf_MSBPOSROOT)) ;
   GETBUFFER_LOG(&logbuffer, &logbufsize2) ;
   TEST(logbufsize1 < logbufsize2) ;

   arraysf_t * array2 = 0 ;
      // type invalid
   GETBUFFER_LOG(&logbuffer, &logbufsize1) ;
   TEST(EINVAL == new_tarraysf(&array2, -1)) ;
   GETBUFFER_LOG(&logbuffer, &logbufsize2) ;
   TEST(logbufsize1 < logbufsize2) ;

   // TEST EEXIST
   nodea[0] = (testnode_t) { .node = arraysf_node_INIT(0) } ;
   nodea[1] = (testnode_t) { .node = arraysf_node_INIT(0) } ;
   testnode_t * inserted_node = 0 ;
   TEST(0 == insert_tarraysf(array, &nodea[0], &inserted_node, 0)) ;
   TEST(&nodea[0] == inserted_node ) ;
   testnode_t * existing_node = 0 ;
   GETBUFFER_LOG(&logbuffer, &logbufsize1) ;
   TEST(EEXIST == tryinsert_tarraysf(array, &nodea[1], &existing_node, 0)) ;  // no log
   GETBUFFER_LOG(&logbuffer, &logbufsize2) ;
   TEST(logbufsize1 == logbufsize2) ;
   TEST(&nodea[0] == existing_node) ;
   inserted_node = 0 ;
   TEST(EEXIST == insert_tarraysf(array, &nodea[1], &inserted_node, 0)) ;     // log
   GETBUFFER_LOG(&logbuffer, &logbufsize2) ;
   TEST(logbufsize1 < logbufsize2) ;
   TEST(0 == inserted_node) ;

   // TEST ESRCH
   testnode_t * removed_node = 0 ;
   GETBUFFER_LOG(&logbuffer, &logbufsize1) ;
   arraysf_findresult_t found ;
   TEST(0 == at_tarraysf(array, 1)) ;                             // no log
   TEST(ESRCH == find_arraysf(array, 1, &found, 0)) ;             // no log
   TEST(ESRCH == tryremove_tarraysf(array, 1, &removed_node)) ;   // no log
   GETBUFFER_LOG(&logbuffer, &logbufsize2) ;
   TEST(logbufsize1 == logbufsize2) ;
   TEST(ESRCH == remove_tarraysf(array, 1, &removed_node)) ;      // log
   GETBUFFER_LOG(&logbuffer, &logbufsize2) ;
   TEST(logbufsize1 < logbufsize2) ;
   nodea[0].freecount = 0 ;
   TEST(0 == tryremove_tarraysf(array, 0, &removed_node)) ;
   TEST(removed_node == &nodea[0]) ;
   TEST(0 == nodea[0].freecount) ;

   // TEST free memory error
   typeadt_ft.freeobj = &test_freenodeerr ;
   for(size_t pos = 0; pos < nrnodes; ++pos) {
      nodea[pos] = (testnode_t) { .node = arraysf_node_INIT(pos) } ;
      TEST(0 == tryinsert_tarraysf(array, &nodea[pos], 0, 0))
      nodea[pos].freecount = 0 ;
      TEST(pos == nodea[pos].node.pos) ;
      TEST(1+pos == length_tarraysf(array)) ;
   }
   TEST(12345 == delete_tarraysf(&array, &typeadt)) ;
   for(unsigned pos = 0; pos < nrnodes; ++pos) {
      TEST(1 == nodea[pos].freecount) ;
   }

   // unprepare
   TEST(0 == free_vmblock(&memblock)) ;

   return 0 ;
ONABORT:
   (void) delete_tarraysf(&array, 0) ;
   free_vmblock(&memblock) ;
   return EINVAL ;
}

static int test_iterator(void)
{
   const size_t   nrnodes  = 30000 ;
   vm_block_t     memblock = vm_block_INIT_FREEABLE ;
   arraysf_iterator_t iter = arraysf_iterator_INIT_FREEABLE ;
   arraysf_t      * array  = 0 ;
   testnode_t     * nodea ;
   testnode_t     * removed_node ;
   size_t         nextpos ;

   // prepare
   TEST(0 == init_vmblock(&memblock, (pagesize_vm()-1+nrnodes*sizeof(testnode_t)) / pagesize_vm() )) ;
   nodea = (testnode_t *) memblock.addr ;
   TEST(0 == new_tarraysf(&array, arraysf_MSBPOSROOT)) ;
   for(size_t i = 0; i < nrnodes; ++i) {
      nodea[i] = (testnode_t)  { .node = arraysf_node_INIT(i) } ;
      TEST(0 == insert_tarraysf(array, &nodea[i], 0, 0)) ;
   }

   // TEST static init
   TEST(0 == iter.stack) ;
   TEST(0 == iter.ri) ;

   // TEST init, double free
   iter.ri = 1 ;
   TEST(0 == initfirst_tarraysfiterator(&iter, array)) ;
   TEST(0 != iter.stack) ;
   TEST(0 == iter.ri) ;
   TEST(0 == free_tarraysfiterator(&iter)) ;
   TEST(0 == iter.stack) ;
   TEST(0 == iter.ri) ;
   TEST(0 == free_tarraysfiterator(&iter)) ;
   TEST(0 == iter.stack) ;
   TEST(0 == iter.ri) ;

   // TEST next_arraysfiterator
   TEST(0 == initfirst_tarraysfiterator(&iter, array)) ;
   nextpos = 0 ;
   for (iteratedtype_tarraysf * node = 0; !node; node = (void*)1) {
      while (next_tarraysfiterator(&iter, array, &node)) {
         TEST(node->node.pos == nextpos) ;
         ++ nextpos ;
      }
   }
   TEST(iter.ri == nrelemroot_arraysf(array)) ;
   TEST(nextpos == nrnodes) ;
   TEST(0 == free_tarraysfiterator(&iter)) ;

   // TEST foreach all
   nextpos = 0 ;
   foreach (_arraysf, array, node) {
      TEST(((arraysf_node_t*)node)->pos == nextpos) ;
      ++ nextpos ;
   }
   TEST(nextpos == nrnodes) ;

   // TEST foreach_arraysf break after nrnodes/2
   nextpos = 0 ;
   foreach (_tarraysf, array, node) {
      TEST(node->node.pos == nextpos) ;
      ++ nextpos ;
      if (nextpos == nrnodes/2) break ;
   }
   TEST(nextpos == nrnodes/2) ;

   // TEST foreach_tarraysf (nrnodes/2 .. nrnodes) after remove
   for(size_t i = 0; i < nrnodes/2; ++i) {
      TEST(0 == remove_tarraysf(array, i, &removed_node)) ;
      TEST(removed_node == &nodea[i]) ;
   }
   nextpos = nrnodes/2 ;
   foreach (_tarraysf, array, node) {
      TEST(node->node.pos == nextpos) ;
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

arraysf_IMPLEMENT(_t2arraysf, testnode_t, pos2)

static int test_generic(void)
{
   const size_t   nrnodes     = 300 ;
   vm_block_t     memblock    = vm_block_INIT_FREEABLE ;
   arraysf_t      * array     = 0 ;
   arraysf_t      * array2    = 0 ;
   testnode_typeadapt_it   typeadt_ft = typeadapter_it_INIT(&test_copynode, &test_freenode) ;
   typeadapter_iot         typeadt    = typeadapter_iot_INIT(0, &typeadt_ft.generic) ;
   testnode_t     * nodea ;
   size_t         nextpos ;


   // prepare
   TEST(0 == init_vmblock(&memblock, (pagesize_vm()-1+nrnodes*sizeof(testnode_t)) / pagesize_vm() )) ;
   nodea = (testnode_t *) memblock.addr ;
   TEST(0 == new_tarraysf(&array, arraysf_8BITROOT24)) ;
   TEST(0 == new_t2arraysf(&array2, arraysf_8BITROOT24)) ;

   // TEST insert_arraysf
   for(size_t i = 0; i < nrnodes; ++i) {
      nodea[i] = (testnode_t)  { .node = arraysf_node_INIT(i), .pos2 = (100000u + i) } ;
      TEST(0 == insert_tarraysf(array, &nodea[i], 0, &typeadt)) ;
      TEST(0 == insert_t2arraysf(array2, &nodea[i], 0, &typeadt)) ;
      TEST(2 == nodea[i].copycount) ;
   }

   // TEST at_arraysf
   for(size_t i = 0; i < nrnodes; ++i) {
      TEST(nodea[i].node.pos == i) ;
      TEST(nodea[i].pos2     == (100000u + i)) ;
      TEST(&nodea[i] == at_tarraysf(array, i)) ;
      TEST(&nodea[i] == at_t2arraysf(array2, 100000u + i)) ;
      TEST(2 == nodea[i].copycount) ;
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
   for(size_t i = 0; i < nrnodes; ++i) {
      testnode_t * removed_node = 0 ;
      TEST(0 == remove_tarraysf(array, i, &removed_node)) ;
      TEST(&nodea[i] == removed_node) ;
      removed_node = 0 ;
      TEST(0 == remove_t2arraysf(array2, 100000u + i, &removed_node)) ;
      TEST(&nodea[i] == removed_node) ;
      TEST(nrnodes-1-i == length_tarraysf(array)) ;
      TEST(nrnodes-1-i == length_t2arraysf(array2)) ;
   }

   // TEST delete_arraysf
   for(size_t i = nrnodes; (i--); ) {
      nodea[i] = (testnode_t)  { .node = arraysf_node_INIT(i), .pos2 = (100000u + i) } ;
      TEST(0 == insert_tarraysf(array, &nodea[i], 0, &typeadt)) ;
      TEST(0 == insert_t2arraysf(array2, &nodea[i], 0, &typeadt)) ;
      TEST(2 == nodea[i].copycount) ;
      TEST(&nodea[i] == at_tarraysf(array, i)) ;
      TEST(&nodea[i] == at_t2arraysf(array2, 100000u + i)) ;
      TEST(2 == nodea[i].copycount) ;
   }
   TEST(0 == delete_arraysf(&array, &typeadt)) ;
   TEST(0 == delete_arraysf(&array2, &typeadt)) ;
   for(size_t i = 0; i < nrnodes; ++i) {
      TEST(2 == nodea[i].freecount) ;
   }

   // unprepare
   TEST(0 == free_vmblock(&memblock)) ;

   return 0 ;
ONABORT:
   TEST(0 == delete_arraysf(&array, 0)) ;
   TEST(0 == delete_arraysf(&array2, 0)) ;
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
