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
#include "C-kern/api/err.h"
#include "C-kern/api/math/int/log2.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/platform/virtmemory.h"
#endif

typedef struct arraysf_imp2_it         arraysf_imp2_it ;

/* struct: arraysf_imp2_it
 * Extends <arraysf_imp_it> with additional information. */
struct arraysf_imp2_it {
   /* variable: impit
    * Base type <arraysf_imp_it>. */
   arraysf_imp_it    impit ;
   /* variable: objectsize
    * Memory size in bytes of object which contains <arraysf_node_t>. */
   size_t            objectsize ;
   /* variable: nodeoffset
    * Offset in bytes from startaddr of allocated object to contained <arraysf_node_t>. */
   size_t            nodeoffset ;
} ;

// group: helper

static int copynode_arraysfimp2(const arraysf_imp_it * impit, const struct arraysf_node_t * node, /*out*/struct arraysf_node_t ** copied_node)
{
   int err ;
   const arraysf_imp2_it * impit2 = (const arraysf_imp2_it *) impit ;
   void                  * memblock ;

   err = impit2->impit.malloc(&impit2->impit, impit2->objectsize, &memblock) ;
   if (err) return err ;

   memcpy(memblock, (((const uint8_t*)node) - impit2->nodeoffset), impit2->objectsize) ;

   *copied_node = (arraysf_node_t*) (((uint8_t*)memblock) + impit2->nodeoffset) ;

   return 0 ;
}

static int freenode_arraysfimp2(const arraysf_imp_it * impit, struct arraysf_node_t * node)
{
   const arraysf_imp2_it * impit2 = (const arraysf_imp2_it *) impit ;

   return impit2->impit.free(&impit2->impit, impit2->objectsize, ((uint8_t*)node) - impit2->nodeoffset) ;
}


// section: arraysf_imp_it

// group: helper

/* function: defaultmalloc_arraysfimpit
 * Default implementation of <arraysf_imp_it.malloc>. */
static int defaultmalloc_arraysfimpit(const arraysf_imp_it * impit, size_t size, /*out*/void ** memblock)
{
   int err ;

   (void) impit ;

   void * addr = malloc(size) ;
   if (!addr) {
      err = ENOMEM ;
      LOG_OUTOFMEMORY(size) ;
      goto ABBRUCH ;
   }

   *(uint8_t**)memblock = (uint8_t*) addr ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

/* function: defaultfree_arraysfimpit
 * Default implementation of <arraysf_imp_it.free>. */
static int defaultfree_arraysfimpit(const arraysf_imp_it * impit, size_t size, void * memblock)
{
   (void) impit ;
   (void) size ;

   free(memblock) ;
   return 0 ;
}

// group: lifetime

arraysf_imp_it * default_arraysfimpit(void)
{
   static arraysf_imp_it impit = {
         .copynode = 0,
         .freenode = 0,
         .malloc   = &defaultmalloc_arraysfimpit,
         .free     = &defaultfree_arraysfimpit
      } ;

   return &impit ;
}

int new_arraysfimp(arraysf_imp_it ** impit, size_t objectsize, size_t nodeoffset)
{
   int err ;
   arraysf_imp2_it * new_obj ;

   VALIDATE_INPARAM_TEST(  objectsize >= sizeof(arraysf_node_t)
                        && nodeoffset <= objectsize-sizeof(arraysf_node_t), ABBRUCH, ) ;

   err = defaultmalloc_arraysfimpit(0, sizeof(arraysf_imp2_it), (void**)&new_obj) ;
   if (err) goto ABBRUCH ;

   new_obj->impit.copynode = &copynode_arraysfimp2 ;
   new_obj->impit.freenode = &freenode_arraysfimp2 ;
   new_obj->impit.malloc   = &defaultmalloc_arraysfimpit ;
   new_obj->impit.free     = &defaultfree_arraysfimpit ;
   new_obj->objectsize = objectsize ;
   new_obj->nodeoffset = nodeoffset ;

   *impit = &new_obj->impit ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int delete_arraysfimp(arraysf_imp_it ** impit)
{
   int err ;
   arraysf_imp_it * del_obj = *impit ;

   if (del_obj) {
      *impit = 0 ;

      err = defaultfree_arraysfimpit(del_obj, sizeof(arraysf_imp2_it), del_obj) ;

      if (err) goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}


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

#define isbranchtype_arraysf(node)     (0 != ((intptr_t)(node) & 0x01))

#define asbranch_arraysf(node)         ((arraysf_mwaybranch_t*) (0x01 ^ (intptr_t)(node)))

#define encodebranch_arraysf(branch)   ((arraysf_node_t*) (0x01 ^ (intptr_t)(branch)))

typedef struct arraysf_findresult_t    arraysf_findresult_t ;

struct arraysf_findresult_t {
   unsigned             rootindex ;
   unsigned             childindex ;
   unsigned             pchildindex ;
   arraysf_mwaybranch_t * parent ;
   arraysf_mwaybranch_t * pparent ;
   arraysf_node_t       * found_node ;
} ;

static int find_arraysf(const arraysf_t * array, size_t pos, /*err;out*/arraysf_findresult_t * result)
{
   int err ;
   unsigned rootindex ;

   switch(array->type) {
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

   arraysf_node_t       * node      = array->root[rootindex] ;
   arraysf_mwaybranch_t * pparent   = 0 ;
   arraysf_mwaybranch_t * parent    = 0 ;
   unsigned             childindex  = 0 ;
   unsigned             pchildindex = 0 ;

   err = ESRCH ;

   for(; node; ) {

      if (isbranchtype_arraysf(node)) {
         pparent = parent ;
         parent  = asbranch_arraysf(node) ;
         pchildindex = childindex   ;
         childindex  = childindex_arraysfmwaybranch(parent, pos) ;
         node  = parent->child[childindex] ;
      } else {
         if (pos == node->pos) {
            err = 0 ;
         }
         break ;
      }
   }

   result->rootindex   = rootindex ;
   result->childindex  = childindex ;
   result->pchildindex = pchildindex ;
   result->parent      = parent ;
   result->pparent     = pparent ;
   result->found_node  = node ;

   return err ;
}

// group: lifetime

int new_arraysf(/*out*/arraysf_t ** array, arraysf_e type, arraysf_imp_it * impit)
{
   int err ;
   arraysf_t   * new_obj ;

   VALIDATE_INPARAM_TEST(*array == 0, ABBRUCH, ) ;

   VALIDATE_INPARAM_TEST(type < nrelementsof(s_arraysf_nrelemroot), ABBRUCH, LOG_INT(type)) ;

   if (! impit) {
      impit = default_arraysfimpit() ;
   }

   VALIDATE_INPARAM_TEST(     impit->malloc
                           && impit->free, ABBRUCH, ) ;

   const size_t objsize = objectsize_arraysf(type) ;

   err = impit->malloc(impit, objsize, (void**)&new_obj) ;
   if (err) goto ABBRUCH ;

   memset(new_obj, 0, objsize) ;
   new_obj->type  = type ;
   new_obj->impit = impit ;

   *array = new_obj ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int delete_arraysf(arraysf_t ** array)
{
   int err = 0 ;
   int err2 ;
   arraysf_t * del_obj = *array ;

   if (del_obj) {
      *array = 0 ;

      typeof(del_obj->impit)           impit      = del_obj->impit ;
      typeof(del_obj->impit->freenode) freenodecb = del_obj->impit->freenode ;
      typeof(del_obj->impit->free)     freecb     = del_obj->impit->free ;

      for(unsigned i = 0; i < nrelemroot_arraysf(del_obj); ++i) {

         if (!del_obj->root[i]) continue ;

         arraysf_node_t * node = del_obj->root[i] ;

         if (!isbranchtype_arraysf(node)) {
            if (freenodecb) {
               err2 = freenodecb(impit, node) ;
               if (err2) err = err2 ;
            }
            continue ;
         }

         arraysf_mwaybranch_t * branch = asbranch_arraysf(node) ;
         node = branch->child[0] ;
         branch->child[0] = 0 ;
         branch->used = nrelementsof(branch->child)-1 ;

         for(;;) {

            for(;;) {
               if (node) {
                  if (isbranchtype_arraysf(node)) {
                     arraysf_mwaybranch_t * parent = branch ;
                     branch = asbranch_arraysf(node) ;
                     node = branch->child[0] ;
                     branch->child[0] = (arraysf_node_t*) parent ;
                     branch->used     = nrelementsof(branch->child)-1 ;
                     continue ;
                  } else if (freenodecb) {
                     err2 = freenodecb(impit, node) ;
                     if (err2) err = err2 ;
                  }
               }

               if (! branch->used)  break ;
               node = branch->child[branch->used --] ;
            }

            do {
               arraysf_mwaybranch_t * parent ;
               parent = (arraysf_mwaybranch_t *) branch->child[0] ;
               err2 = freecb(impit, sizeof(arraysf_mwaybranch_t), branch) ;
               if (err2) err = err2 ;
               branch = parent ;
            } while(branch && !branch->used) ;

            if (!branch) break ;

            node = branch->child[branch->used --] ;
         }

      }

      const size_t objsize = objectsize_arraysf(del_obj->type) ;

      del_obj->impit->free(del_obj->impit, objsize, del_obj) ;
   }

   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}

// group: query

arraysf_node_t * at_arraysf(const arraysf_t * array, size_t pos)
{
   int err ;
   arraysf_findresult_t found ;

   err = find_arraysf(array, pos, &found) ;
   if (err) return 0 ;

   return found.found_node ;
}

// group: change

int tryinsert_arraysf(arraysf_t * array, arraysf_node_t * node, /*err*/arraysf_node_t ** inserted_or_existing_node)
{
   int err ;
   arraysf_findresult_t found ;
   arraysf_node_t       * copied_node = 0 ;
   size_t               pos           = node->pos ;

   err = find_arraysf(array, pos, &found) ;
   if (ESRCH != err) {
      if (inserted_or_existing_node) {
         *inserted_or_existing_node = (0 == err) ? found.found_node : 0 ;
      }
      return (0 == err) ? EEXIST : err ;
   }

   if (array->impit->copynode) {
      err = array->impit->copynode(array->impit, node, &copied_node) ;
      if (err) goto ABBRUCH ;
      node = copied_node ;
   }

   VALIDATE_INPARAM_TEST(0 == ((intptr_t)node&0x01), ABBRUCH, ) ;

   size_t   pos2    = 0 ;
   size_t   posdiff = 0 ;

   if (found.found_node) {

      pos2    = found.found_node->pos ;
      posdiff = (pos ^ pos2) ;

      if (  ! found.parent
         || 0 == (posdiff >> found.parent->shift))  {

   // prefix matches (add new branch layer after found.parent)

         unsigned shift = log2_int(posdiff) & ~0x01u ;

         arraysf_mwaybranch_t * branch ;
         err = array->impit->malloc(array->impit, sizeof(arraysf_mwaybranch_t), (void**)&branch) ;
         if (err) goto ABBRUCH ;

         init_arraysfmwaybranch(branch, shift, found.found_node->pos, found.found_node, pos, node) ;

         if (found.parent) {
            found.parent->child[found.childindex] = encodebranch_arraysf(branch) ;
         } else {
            array->root[found.rootindex] = encodebranch_arraysf(branch) ;
         }
         goto DONE ;
      }

   // fall through to not so simple case

   } else /*(! found.found_node)*/ {

   // simple case (parent is 0)

      if (! found.parent) {
         array->root[found.rootindex] = node ;
         goto DONE ;
      }

   // get pos of already stored node / check prefix match => second simple case

      arraysf_mwaybranch_t * branch = found.parent ;
      for(unsigned i = nrelementsof(branch->child); (i--); ) {
         if (branch->child[i]) {
            if (isbranchtype_arraysf(branch->child[i])) {
               branch = asbranch_arraysf(branch->child[i]) ;
               i = nrelementsof(branch->child) ;
               continue ;
            }
            pos2    = branch->child[i]->pos ;
            posdiff = (pos ^ pos2) ;
            break ;
         }
      }

      unsigned prefix = ~0x03u & (posdiff >> found.parent->shift) ;

      if (0 == prefix) {
         // prefix does match
         found.parent->child[found.childindex] = node ;
         ++ found.parent->used ;
         goto DONE ;
      }

   }

   // not so simple case prefix differs (add new branch layer between root & found.parent)

   assert(found.parent) ;
   arraysf_mwaybranch_t * parent = 0 ;
   arraysf_mwaybranch_t * branch = asbranch_arraysf(array->root[found.rootindex]) ;

   unsigned childindex = 0 ;
   unsigned shift = log2_int(posdiff) & ~0x01u ;

   while(branch->shift > shift) {
      parent = branch ;
      childindex = childindex_arraysfmwaybranch(branch, pos) ;
      arraysf_node_t * child = branch->child[childindex] ;
      assert(child) ;
      assert(isbranchtype_arraysf(child)) ;
      branch = asbranch_arraysf(child) ;
   }

   arraysf_mwaybranch_t * new_branch ;
   err = array->impit->malloc(array->impit, sizeof(arraysf_mwaybranch_t), (void**)&new_branch) ;
   if (err) goto ABBRUCH ;

   init_arraysfmwaybranch(new_branch, shift, pos2, encodebranch_arraysf(branch), pos, node) ;

   if (parent) {
      parent->child[childindex] = encodebranch_arraysf(new_branch) ;
   } else {
      array->root[found.rootindex] = encodebranch_arraysf(new_branch) ;
   }

DONE:

   ++ array->length ;

   if (inserted_or_existing_node) {
      *inserted_or_existing_node = node ;
   }

   return 0 ;
ABBRUCH:
   if (  copied_node
      && array->impit->freenode) {
      (void) array->impit->freenode( array->impit, copied_node) ;
   }
   LOG_ABORT(err) ;
   return err ;
}

int tryremove_arraysf(arraysf_t * array, size_t pos, /*out*/struct arraysf_node_t ** removed_node/*could be 0*/)
{
   int err ;
   int err2 ;
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

         unsigned i ;
         for(i = nrelementsof(found.parent->child); (i --); ) {
            if (  i != found.childindex
               && found.parent->child[i]) {
               arraysf_node_t * other_child = found.parent->child[i] ;
               if (found.pparent) {
                  setchild_arraysfmwaybranch(found.pparent, found.pchildindex, other_child) ;
               } else {
                  array->root[found.rootindex] = other_child ;
               }
               break ;
            }
         }
         assert(i < nrelementsof(found.parent->child)) ;

         err = array->impit->free(array->impit, sizeof(arraysf_mwaybranch_t), found.parent) ;

      }

   }

   assert(array->length > 0) ;
   -- array->length ;

   if (array->impit->freenode) {
      err2 = array->impit->freenode( array->impit, found.found_node) ;
      if (err2) err = err2 ;
      found.found_node = 0 ;
   }

   if (removed_node) {
      *removed_node = found.found_node ;
   }

   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int remove_arraysf(arraysf_t * array, size_t pos, /*out*/struct arraysf_node_t ** removed_node/*could be 0*/)
{
   int err ;

   err = tryremove_arraysf(array, pos, removed_node) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int insert_arraysf(arraysf_t * array, struct arraysf_node_t * node, /*out*/struct arraysf_node_t ** inserted_node/*0 => not returned*/)
{
   int err ;
   arraysf_node_t * inserted_or_existing_node ;

   err = tryinsert_arraysf(array, node, &inserted_or_existing_node) ;
   if (err) goto ABBRUCH ;

   if (inserted_node) {
      *inserted_node = inserted_or_existing_node ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}


// section: arraysf_iterator_t

int init_arraysfiterator(/*out*/arraysf_iterator_t * iter, arraysf_t * array)
{
   int err ;
   size_t        objectsize = sizeof(binarystack_t) ;
   memblock_t    objectmem  = memblock_INIT_FREEABLE ;
   binarystack_t * stack  = 0 ;

   (void) array ;

   err = MM_RESIZE(objectsize, &objectmem) ;
   if (err) goto ABBRUCH ;

   stack  = (binarystack_t *) objectmem.addr ;

   struct {
      arraysf_mwaybranch_t * branch ;
      unsigned             ci ;
   } pos ;

   err = init_binarystack(stack, (/*max depth*/4 * sizeof(size_t)) * /*objectsize*/sizeof(pos) ) ;
   if (err) goto ABBRUCH ;

   iter->stack = stack ;
   iter->ri    = 0 ;

   return 0 ;
ABBRUCH:
   MM_FREE(&objectmem) ;
   LOG_ABORT(err) ;
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

      err2 = MM_FREE(&objectmem) ;
      if (err2) err = err2 ;

      if (err) goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}

bool next_arraysfiterator(arraysf_iterator_t * iter, arraysf_t * array, /*out*/struct arraysf_node_t ** _node)
{
   int err ;
   size_t         nrelemroot = nrelemroot_arraysf(array) ;
   struct {
      arraysf_mwaybranch_t * branch ;
      unsigned             ci ;
   }              pos ;

   for (;;) {

      if (isempty_binarystack(iter->stack)) {

         arraysf_node_t * rootnode ;

         for(;;) {
            if (iter->ri >= nrelemroot) {
               return false ;
            }
            rootnode = array->root[iter->ri ++] ;
            if (rootnode) {
               if (!isbranchtype_arraysf(rootnode)) {
                  *_node = rootnode ;
                  return true ;
               }
               break ;
            }
         }

         pos.branch = asbranch_arraysf(rootnode) ;
         pos.ci     = 0 ;

      } else {

         err = pop_binarystack(iter->stack, sizeof(pos), &pos) ;
         if (err) goto ABBRUCH ;

      }

      for(;;) {
         arraysf_node_t * childnode = pos.branch->child[pos.ci ++] ;

         if (childnode) {

            if (pos.ci < nrelementsof(pos.branch->child)) {
               err = push_binarystack(iter->stack, sizeof(pos), &pos) ;
               if (err) goto ABBRUCH ;
            }

            if (isbranchtype_arraysf(childnode)) {
               pos.branch = asbranch_arraysf(childnode) ;
               pos.ci     = 0 ;
               continue ;
            } else {
               *_node = childnode ;
               return true ;
            }
         }

         if (pos.ci >= nrelementsof(pos.branch->child)) {
            break ;
         }
      }

   }

   return false ;
ABBRUCH:
   // move iterator to end of container
   iter->ri = nrelemroot ;
   pop_binarystack(iter->stack, size_binarystack(iter->stack), 0) ;
   LOG_ABORT(err) ;
   return false ;
}


// group: test

#ifdef KONFIG_UNITTEST

typedef struct testnode_t              testnode_t ;

struct testnode_t {
   arraysf_node_t node ;
   int            copycount ;
   int            freecount ;
} ;

static int test_copynode(const arraysf_imp_it * impit, const arraysf_node_t * node, arraysf_node_t ** copied_node)
{
   (void) impit ;
   ++ ((testnode_t*)(intptr_t)node)->copycount ;
   *copied_node = (arraysf_node_t*) (intptr_t) node ;
   return 0 ;
}

static int test_freenode(const arraysf_imp_it * impit, arraysf_node_t * node)
{
   (void) impit ;
   ++ ((testnode_t*)node)->freecount ;
   return 0 ;
}

static int test_freenodeerr(const arraysf_imp_it * impit, arraysf_node_t * node)
{
   (void) impit ;
   ++ ((testnode_t*)node)->freecount ;
   return 12345 ;
}

static int test_initfree(void)
{
   const size_t   nrnodes  = 100000 ;
   vm_block_t     memblock = vm_block_INIT_FREEABLE ;
   arraysf_t      * array  = 0 ;
   arraysf_imp_it impit    = *default_arraysfimpit() ;
   testnode_t     * nodea ;

   // prepare
   impit.freenode = &test_freenode ;
   impit.copynode = &test_copynode ;
   TEST(0 == init_vmblock(&memblock, (pagesize_vm()-1+nrnodes*sizeof(testnode_t)) / pagesize_vm() )) ;
   nodea = (testnode_t *) memblock.addr ;

   // TEST static init
   TEST(0 == array) ;

   // TEST init, double free
   for(arraysf_e type = arraysf_6BITROOT_UNSORTED; type <= arraysf_8BITROOT24; ++type) {
      TEST(0 == new_arraysf(&array, type, 0)) ;
      TEST(0 != array) ;
      TEST(type == array->type) ;
      TEST(0 != array->impit) ;
      TEST(0 == array->impit->copynode) ;
      TEST(0 == array->impit->freenode) ;
      TEST(0 != array->impit->malloc) ;
      TEST(0 != array->impit->free) ;
      TEST(0 == length_arraysf(array)) ;
      switch(type) {
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
      TEST(0 == delete_arraysf(&array)) ;
      TEST(0 == array) ;
      TEST(0 == delete_arraysf(&array)) ;
      TEST(0 == array) ;
   }

   // TEST root distributions
   for(arraysf_e type = arraysf_6BITROOT_UNSORTED; type <= arraysf_8BITROOT24; ++type) {
      TEST(0 == new_arraysf(&array, type, 0)) ;
      for(size_t pos1 = 0, pos2 = 0; pos1 < 256; ++ pos1, pos2 = (pos2 + (pos2 == 0)) << 1) {
         size_t          pos             = pos1 + pos2 ;
         testnode_t      node            = { .node = arraysf_node_INIT(pos) } ;
         arraysf_node_t  * inserted_node = 0 ;
         TEST(0 == tryinsert_arraysf(array, &node.node, &inserted_node)) ;
         TEST(inserted_node == &node.node) ;
         TEST(1 == array->length) ;
         size_t ri ;
         switch(type) {
         case arraysf_6BITROOT_UNSORTED:     ri = (pos & 63) ; break ;
         case arraysf_8BITROOT_UNSORTED:     ri = (pos & 255) ; break ;
         case arraysf_MSBPOSROOT:   ri = ~0x01u & log2_int(pos) ;
                                    ri = 3 * (ri/2) + ((pos >> ri) & 0x03u) ; break ;
         case arraysf_8BITROOT24:   ri = (pos >> 24) ; break ;
         }
         TEST(ri < nrelemroot_arraysf(array)) ;
         TEST(&node.node == array->root[ri]) ;
         for(unsigned i = 0; i < nrelemroot_arraysf(array); ++i) {
            if (i == ri) continue ;
            TEST(0 == array->root[i]) ;
         }
         TEST(0 == tryremove_arraysf(array, pos, 0)) ;
         TEST(0 == array->length) ;
         TEST(0 == array->root[ri]) ;
      }
      TEST(0 == delete_arraysf(&array)) ;
   }

   // TEST arraysf_8BITROOT24: root distribution
   TEST(0 == new_arraysf(&array, arraysf_8BITROOT24, 0)) ;
   for(size_t pos = 0, ri = 0; (ri == 0) || pos; pos += ((~((~(size_t)0) >> 1)) >> 7), ++ri) {
      testnode_t     node            = { .node = arraysf_node_INIT(pos+ri) } ;
      arraysf_node_t * inserted_node = 0 ;
      TEST(0 == tryinsert_arraysf(array, &node.node, &inserted_node)) ;
      TEST(inserted_node == &node.node) ;
      TEST(1 == length_arraysf(array)) ;
      TEST(pos+ri == node.node.pos) ;
      TEST(ri < nrelemroot_arraysf(array)) ;
      TEST(&node.node == array->root[ri]) ;
      for(unsigned i = 0; i < nrelemroot_arraysf(array); ++i) {
         if (i == ri) continue ;
         TEST(0 == array->root[i]) ;
      }
      arraysf_node_t * removed_node = 0 ;
      TEST(0 == tryremove_arraysf(array, pos+ri, &removed_node)) ;
      TEST(0 == length_arraysf(array)) ;
      TEST(0 == array->root[ri]) ;
      TEST(inserted_node == removed_node) ;
   }
   TEST(0 == delete_arraysf(&array)) ;

   // TEST insert_arraysf (1 level)
   TEST(0 == new_arraysf(&array, arraysf_MSBPOSROOT, &impit)) ;
   nodea[4].node = (arraysf_node_t) arraysf_node_INIT(4) ;
   TEST(0 == tryinsert_arraysf(array, &nodea[4].node, 0))
   TEST(&nodea[4].node == array->root[4]) ;
   for(size_t pos = 5; pos <= 7; ++pos) {
      arraysf_node_t * inserted_node = 0 ;
      nodea[pos] = (testnode_t) { .node = arraysf_node_INIT(pos) } ;
      TEST(0 == tryinsert_arraysf(array, &nodea[pos].node, &inserted_node))
      TEST(inserted_node == &nodea[pos].node) ;
      TEST(0 == nodea[pos].freecount) ;
      TEST(1 == nodea[pos].copycount) ;
      TEST(pos-3 == length_arraysf(array)) ;
      TEST(isbranchtype_arraysf(array->root[4])) ;
      TEST(0 == asbranch_arraysf(array->root[4])->shift) ;
      TEST(pos-3 == asbranch_arraysf(array->root[4])->used) ;
   }
   for(size_t pos = 4; pos <= 7; ++pos) {
      TEST(&nodea[pos].node == asbranch_arraysf(array->root[4])->child[pos-4]) ;
      TEST(&nodea[pos].node == at_arraysf(array, pos)) ;
      TEST(0 == at_arraysf(array, 10*pos+4)) ;
   }

   // TEST remove_arraysf (1 level)
   for(size_t pos = 4; pos <= 7; ++pos) {
      arraysf_node_t * removed_node = (void*) 1 ;
      TEST(0 == tryremove_arraysf(array, pos, &removed_node))
      TEST(1 == nodea[pos].copycount) ;
      TEST(1 == nodea[pos].freecount) ;
      TEST(0 == removed_node) ; // freenode called => removed_node = 0
      TEST(0 == at_arraysf(array, pos)) ;
      if (pos < 6) {
         TEST(array->root[4]) ;
         TEST(isbranchtype_arraysf(array->root[4])) ;
      } else if (pos == 6) {
         TEST(&nodea[7].node == array->root[4]) ;
      } else {
         TEST(! array->root[4]) ;
      }
      TEST(7-pos == length_arraysf(array)) ;
   }

   // TEST insert_arraysf (2 levels)
   arraysf_mwaybranch_t * branch1 = 0 ;
   for(size_t pos = 16; pos <= 31; ++pos) {
      nodea[pos] = (testnode_t) { .node = arraysf_node_INIT(pos) } ;
      TEST(0 == tryinsert_arraysf(array, &nodea[pos].node, 0))
      TEST(pos == nodea[pos].node.pos) ;
      TEST(1 == nodea[pos].copycount) ;
      TEST(0 == nodea[pos].freecount) ;
      TEST(pos-15 == length_arraysf(array)) ;
      if (pos == 16) {
         TEST(&nodea[16].node == array->root[7]) ;
      } else if (pos == 17) {
         TEST(isbranchtype_arraysf(array->root[7])) ;
         branch1 = asbranch_arraysf(array->root[7]) ;
         TEST(0 == branch1->shift) ;
         TEST(&nodea[16].node  == branch1->child[0]) ;
         TEST(&nodea[17].node  == branch1->child[1]) ;
      } else if (pos <= 19) {
         TEST(isbranchtype_arraysf(array->root[7])) ;
         TEST(branch1 == asbranch_arraysf(array->root[7])) ;
         TEST(&nodea[pos].node == branch1->child[pos-16]) ;
      } else if (pos == 20 || pos == 24 || pos == 28) {
         TEST(isbranchtype_arraysf(array->root[7])) ;
         if (pos == 20) {
            arraysf_mwaybranch_t * branch2 = asbranch_arraysf(array->root[7]) ;
            TEST(2 == branch2->shift) ;
            TEST(branch1 == asbranch_arraysf(branch2->child[0])) ;
            branch1 = branch2 ;
         }
         TEST(&nodea[pos].node == branch1->child[(pos-16)/4]) ;
      } else {
         TEST(isbranchtype_arraysf(array->root[7])) ;
         TEST(branch1 == asbranch_arraysf(array->root[7])) ;
         TEST(isbranchtype_arraysf(branch1->child[(pos-16)/4])) ;
         arraysf_mwaybranch_t * branch2 = asbranch_arraysf(branch1->child[(pos-16)/4]) ;
         TEST(&nodea[pos&~0x03u].node == branch2->child[0]) ;
         TEST(&nodea[pos].node == branch2->child[pos&0x03u]) ;
      }
   }

   // TEST remove_arraysf (2 levels)
   for(size_t pos = 16; pos <= 31; ++pos) {
      arraysf_node_t * removed_node = (void*) 1 ;
      TEST(0 == tryremove_arraysf(array, pos, &removed_node)) ;
      TEST(1 == nodea[pos].copycount) ;
      TEST(1 == nodea[pos].freecount) ;
      TEST(0 == removed_node) ; // freenode called => removed_node = 0
      TEST(0 == at_arraysf(array, pos))
      TEST(31-pos == length_arraysf(array)) ;
      if (pos <= 17) {
         TEST(1 == isbranchtype_arraysf(asbranch_arraysf(array->root[7])->child[0])) ;
      } else if (pos == 18) {
         TEST(&nodea[19].node == asbranch_arraysf(array->root[7])->child[0]) ;
      } else if (pos == 19) {
         TEST(0 == asbranch_arraysf(array->root[7])->child[0]) ;
      } else if (pos < 22) {
         TEST(1 == isbranchtype_arraysf(asbranch_arraysf(array->root[7])->child[1])) ;
      } else if (pos == 22) {
         TEST(1 == isbranchtype_arraysf(array->root[7])) ;
         TEST(2 == asbranch_arraysf(array->root[7])->shift) ;
         TEST(&nodea[23].node == asbranch_arraysf(array->root[7])->child[1]) ;
      } else if (pos <= 26) {
         TEST(1 == isbranchtype_arraysf(array->root[7])) ;
         TEST(2 == asbranch_arraysf(array->root[7])->shift) ;
      } else if (pos <= 29) {
         TEST(1 == isbranchtype_arraysf(array->root[7])) ;
         TEST(0 == asbranch_arraysf(array->root[7])->shift) ;
      } else if (pos == 30) {
         TEST(&nodea[31].node == array->root[7]) ;
      } else if (pos == 31) {
         TEST(0 == array->root[7]) ;
      }
   }

   // TEST insert_arraysf at_arraysf remove_arraysf forward
   for(arraysf_e type = arraysf_6BITROOT_UNSORTED; type <= arraysf_8BITROOT24; ++type) {
      TEST(0 == delete_arraysf(&array)) ;
      TEST(0 == new_arraysf(&array, type, 0)) ;
      for(size_t pos = 0; pos < nrnodes; ++pos) {
         arraysf_node_t * inserted_node = (void*) 0 ;
         nodea[pos] = (testnode_t) { .node = arraysf_node_INIT(pos) } ;
         TEST(0 == tryinsert_arraysf(array, &nodea[pos].node, &inserted_node))
         TEST(inserted_node == &nodea[pos].node) ;
         TEST(1+pos == length_arraysf(array)) ;
      }
      for(size_t pos = 0; pos < nrnodes; ++pos) {
         TEST(&nodea[pos].node == at_arraysf(array, pos)) ;
      }
      for(size_t pos = 0; pos < nrnodes; ++pos) {
         TEST(0 != at_arraysf(array, pos))
         arraysf_node_t * removed_node = (void*) 0 ;
         TEST(0 == tryremove_arraysf(array, pos, &removed_node))
         TEST(0 == nodea[pos].copycount) ;
         TEST(0 == nodea[pos].freecount) ;
         TEST(removed_node == &nodea[pos].node) ;
         TEST(0 == at_arraysf(array, pos))
         TEST(nrnodes-1-pos == length_arraysf(array)) ;
      }
   }

   // TEST insert_arraysf at_arraysf remove_arraysf backward
   for(arraysf_e type = arraysf_6BITROOT_UNSORTED; type <= arraysf_8BITROOT24; ++type) {
      TEST(0 == delete_arraysf(&array)) ;
      TEST(0 == new_arraysf(&array, type, &impit)) ;
      for(size_t pos = nrnodes; (pos --); ) {
         nodea[pos] = (testnode_t) { .node = arraysf_node_INIT(pos) } ;
         TEST(0 == tryinsert_arraysf(array, &nodea[pos].node, 0))
         TEST(1 == nodea[pos].copycount) ;
         TEST(0 == nodea[pos].freecount) ;
         TEST(nrnodes-pos == length_arraysf(array)) ;
      }
      for(size_t pos = nrnodes; (pos --); ) {
         TEST(&nodea[pos].node == at_arraysf(array, pos)) ;
      }
      for(size_t pos = nrnodes; (pos --); ) {
         TEST(0 != at_arraysf(array, pos))
         arraysf_node_t * removed_node = (void*) 1 ;
         TEST(0 == tryremove_arraysf(array, pos, &removed_node))
         TEST(1 == nodea[pos].copycount) ;
         TEST(1 == nodea[pos].freecount) ;
         TEST(0 == removed_node) ;
         TEST(0 == at_arraysf(array, pos))
         TEST(pos == length_arraysf(array)) ;
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
            TEST(&nodea[pos].node == at_arraysf(array, pos)) ;
            TEST(0 == tryremove_arraysf(array, pos, 0)) ;
            TEST(1 == nodea[pos].copycount) ;
            TEST(1 == nodea[pos].freecount) ;
            nodea[pos].copycount = 0 ;
            nodea[pos].freecount = 0 ;
         } else {
            nodea[pos] = (testnode_t) { .node = arraysf_node_INIT(pos) } ;
            TEST(0 == tryinsert_arraysf(array, &nodea[pos].node, 0)) ;
            TEST(1 == nodea[pos].copycount) ;
            TEST(0 == nodea[pos].freecount) ;
         }
      }
   }
   TEST(0 == delete_arraysf(&array)) ;

   // TEST delete_arraysf frees memory
   for(arraysf_e type = arraysf_6BITROOT_UNSORTED; type <= arraysf_8BITROOT24; ++type) {
      TEST(0 == new_arraysf(&array, type, 0)) ;
      TEST(0 != impolicy_arraysf(array)) ;
      TEST(0 == impolicy_arraysf(array)->copynode) ;
      TEST(0 == impolicy_arraysf(array)->freenode) ;
      for(size_t pos = nrnodes; (pos --); ) {
         nodea[pos] = (testnode_t) { .node = arraysf_node_INIT(pos) } ;
         TEST(0 == tryinsert_arraysf(array, &nodea[pos].node, 0))
         TEST(nrnodes-pos == length_arraysf(array)) ;
      }
      TEST(0 == delete_arraysf(&array)) ;
      TEST(0 == array) ;
   }

   // TEST delete_arraysf frees also nodes
   impit.copynode = 0;
   impit.freenode = &test_freenode ;
   for(arraysf_e type = arraysf_6BITROOT_UNSORTED; type <= arraysf_8BITROOT24; ++type) {
      memset(nodea, 0, sizeof(testnode_t) * nrnodes) ;
      TEST(0 == new_arraysf(&array, type, &impit)) ;
      unsigned nr = 0 ;
      for(size_t key = 4; key; key <<= 2) {
         for(size_t key2 = 0; key2 <= 11; ++key2) {
            size_t pos = key + key2 ;
            TEST(nr < nrnodes) ;
            nodea[nr] = (testnode_t) { .node = arraysf_node_INIT(pos) } ;
            TEST(0 == tryinsert_arraysf(array, &nodea[nr].node, 0))
            TEST((++nr) == length_arraysf(array)) ;
         }
      }
      TEST(0 == delete_arraysf(&array)) ;
      TEST(0 == array) ;
      for(unsigned pos = 0; pos < nrnodes; ++pos) {
         TEST(0 == nodea[pos].copycount) ;
         if (pos < nr) {
            TEST(1 == nodea[pos].freecount) ;
         } else {
            TEST(0 == nodea[pos].freecount) ;
         }
      }
   }

   // unprepare
   TEST(0 == free_vmblock(&memblock)) ;

   return 0 ;
ABBRUCH:
   (void) delete_arraysf(&array) ;
   free_vmblock(&memblock) ;
   return EINVAL ;
}

static int test_error(void)
{
   const size_t   nrnodes = 100000 ;
   vm_block_t     memblock = vm_block_INIT_FREEABLE ;
   arraysf_imp_it impit    = *default_arraysfimpit() ;
   arraysf_t      * array = 0 ;
   testnode_t     * nodea ;

   // prepare
   impit.freenode = &test_freenode ;
   TEST(0 == init_vmblock(&memblock, (pagesize_vm()-1+nrnodes*sizeof(testnode_t)) / pagesize_vm() )) ;
   nodea = (testnode_t *) memblock.addr ;
   TEST(0 == new_arraysf(&array, arraysf_MSBPOSROOT, &impit)) ;

   // TEST EINVAL
      // (array != 0)
   TEST(EINVAL == new_arraysf(&array, arraysf_MSBPOSROOT, &impit)) ;
   arraysf_t * array2 = 0 ;
      // type invalid
   TEST(EINVAL == new_arraysf(&array2, -1, &impit)) ;

   // TEST EEXIST
   nodea[0] = (testnode_t) { .node = arraysf_node_INIT(0) } ;
   nodea[1] = (testnode_t) { .node = arraysf_node_INIT(0) } ;
   TEST(0 == tryinsert_arraysf(array, &nodea[0].node, 0)) ;
   arraysf_node_t * existing_node = 0 ;
   TEST(EEXIST == tryinsert_arraysf(array, &nodea[1].node, &existing_node)) ;  // no log
   TEST(&nodea[0].node == existing_node) ;
   existing_node = 0 ;
   TEST(EEXIST == insert_arraysf(array, &nodea[1].node, &existing_node)) ;     // log
   TEST(0 == existing_node) ;

   // TEST ESRCH
   arraysf_findresult_t found ;
   TEST(0 == at_arraysf(array, 1)) ;               // no log
   TEST(ESRCH == find_arraysf(array, 1, &found)) ; // no log
   TEST(ESRCH == tryremove_arraysf(array, 1, 0)) ; // no log
   TEST(ESRCH == remove_arraysf(array, 1, 0)) ;    // log
   nodea[0].freecount = 0 ;
   TEST(0 == tryremove_arraysf(array, 0, 0)) ;
   TEST(1 == nodea[0].freecount) ;

   // TEST free memory error
   impit.freenode = &test_freenodeerr ;
   for(size_t pos = 0; pos < nrnodes; ++pos) {
      nodea[pos] = (testnode_t) { .node = arraysf_node_INIT(pos) } ;
      TEST(0 == tryinsert_arraysf(array, &nodea[pos].node, 0))
      nodea[pos].freecount = 0 ;
      TEST(pos == nodea[pos].node.pos) ;
      TEST(1+pos == length_arraysf(array)) ;
   }
   TEST(12345 == delete_arraysf(&array)) ;
   for(unsigned pos = 0; pos < nrnodes; ++pos) {
      TEST(1 == nodea[pos].freecount) ;
   }

   // unprepare
   TEST(0 == free_vmblock(&memblock)) ;

   return 0 ;
ABBRUCH:
   (void) delete_arraysf(&array) ;
   free_vmblock(&memblock) ;
   return EINVAL ;
}

static int test_arraysfimp(void)
{
   arraysf_imp_it * impit = 0 ;
   arraysf_t      * array = 0 ;

   // TEST static init
   TEST(0 == impit) ;

   // TEST init, double free
   TEST(0 == new_arraysfimp(&impit, 32, 16)) ;
   TEST(0 != impit) ;
   TEST(impit->copynode == &copynode_arraysfimp2) ;
   TEST(impit->freenode == &freenode_arraysfimp2) ;
   TEST(impit->malloc   == &defaultmalloc_arraysfimpit) ;
   TEST(impit->free     == &defaultfree_arraysfimpit) ;
   TEST(32 == ((arraysf_imp2_it*)impit)->objectsize) ;
   TEST(16 == ((arraysf_imp2_it*)impit)->nodeoffset) ;
   TEST(0 == delete_arraysfimp(&impit)) ;
   TEST(0 == impit) ;
   TEST(0 == delete_arraysfimp(&impit)) ;
   TEST(0 == impit) ;

   // TEST copynode_arraysfimp2
   TEST(0 == new_arraysfimp(&impit, 64, 32)) ;
   TEST(0 != impit) ;
   TEST(64 == ((arraysf_imp2_it*)impit)->objectsize) ;
   TEST(32 == ((arraysf_imp2_it*)impit)->nodeoffset) ;
   uint8_t memblock[64] ;
   for(uint8_t i = 0; i < 64; ++i) {
      memblock[i] = i ;
   }
   uint8_t * copied_node = 0 ;
   TEST(0 == impit->copynode(impit, (arraysf_node_t*)(32+memblock), (arraysf_node_t**)&copied_node)) ;
   TEST(0 != copied_node) ;
   for(int i = 0; i < 32; ++i) {
      TEST(32+i == copied_node[i]) ;
      TEST(i    == copied_node[i-32]) ;
   }

   // TEST freenode_arraysfimp2
   TEST(0 == impit->freenode(impit, (arraysf_node_t*)copied_node)) ;
   TEST(0 == delete_arraysfimp(&impit)) ;
   TEST(0 == impit) ;

   // TEST with arraysf
   TEST(0 == new_arraysfimp(&impit, sizeof(arraysf_node_t)+16, 4)) ;
   TEST(0 == new_arraysf(&array, arraysf_MSBPOSROOT, impit)) ;
   TEST(0 != array) ;
   TEST(impit == array->impit) ;
   for(size_t i = 0; i < 1000; ++i) {
      arraysf_node_t * node = 0 ;
      *(arraysf_node_t*)(4+memblock) = (arraysf_node_t) arraysf_node_INIT(i) ;
      TEST(0 == insert_arraysf(array, (arraysf_node_t*)(4+memblock), &node)) ;
      TEST(0 != node) ;
      TEST((arraysf_node_t*)(4+memblock) != node) ;
   }
   for(size_t i = 0; i < 1000; ++i) {
      arraysf_node_t * node = at_arraysf(array, i) ;
      TEST(0 != node) ;
      TEST(i == node->pos) ;
      TEST(0 == memcmp(-4 + (uint8_t*)node, memblock, 4)) ;
      TEST(0 == memcmp(sizeof(*node) + (uint8_t*)node, memblock+4+sizeof(*node), 12)) ;
   }
   for(size_t i = 0; i < 1000; ++i) {
      TEST(0 == remove_arraysf(array, i, 0)) ;
   }
   TEST(0 == delete_arraysf(&array)) ;
   TEST(0 == array) ;
   TEST(0 == delete_arraysfimp(&impit)) ;
   TEST(0 == impit) ;

   return 0 ;
ABBRUCH:
   (void) delete_arraysf(&array) ;
   return EINVAL ;
}

static int test_iterator(void)
{
   const size_t   nrnodes  = 30000 ;
   vm_block_t     memblock = vm_block_INIT_FREEABLE ;
   arraysf_t      * array  = 0 ;
   testnode_t     * nodea ;
   size_t         nextpos ;

   // prepare
   TEST(0 == init_vmblock(&memblock, (pagesize_vm()-1+nrnodes*sizeof(testnode_t)) / pagesize_vm() )) ;
   nodea = (testnode_t *) memblock.addr ;

   TEST(0 == new_arraysf(&array, arraysf_MSBPOSROOT, 0)) ;
   for(size_t i = 0; i < nrnodes; ++i) {
      nodea[i] = (testnode_t)  { .node = arraysf_node_INIT(i) } ;
      TEST(0 == insert_arraysf(array, &nodea[i].node, 0)) ;
   }

   // TEST foreach all
   nextpos = 0 ;
   foreach (_arraysf, array, node) {
      TEST(node->pos == nextpos) ;
      ++ nextpos ;
   }
   TEST(nextpos == nrnodes) ;

   // TEST foreach break after nrnodes/2
   nextpos = 0 ;
   foreach (_arraysf, array, node) {
      TEST(node->pos == nextpos) ;
      ++ nextpos ;
      if (nextpos == nrnodes/2) break ;
   }
   TEST(nextpos == nrnodes/2) ;

   // TEST foreach (nrnodes/2 .. nrnodes) after remove
   for(size_t i = 0; i < nrnodes/2; ++i) {
      TEST(0 == remove_arraysf(array, i, 0)) ;
   }
   nextpos = nrnodes/2 ;
   foreach (_arraysf, array, node) {
      TEST(node->pos == nextpos) ;
      ++ nextpos ;
   }
   TEST(nextpos == nrnodes) ;

   // unprepare
   TEST(0 == delete_arraysf(&array)) ;
   TEST(0 == free_vmblock(&memblock)) ;

   return 0 ;
ABBRUCH:
   (void) delete_arraysf(&array) ;
   free_vmblock(&memblock) ;
   return EINVAL ;
}

int unittest_ds_inmem_arraysf()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())       goto ABBRUCH ;
   if (test_error())          goto ABBRUCH ;
   if (test_arraysfimp())     goto ABBRUCH ;
   if (test_iterator())       goto ABBRUCH ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
