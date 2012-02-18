/* title: ArraySTF impl
   Implements array which supports strings as index values.

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

   file: C-kern/api/ds/inmem/arraystf.h
    Header file <ArraySTF>.

   file: C-kern/ds/inmem/arraystf.c
    Implementation file <ArraySTF impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/ds/inmem/arraystf.h"
#include "C-kern/api/ds/inmem/binarystack.h"
#include "C-kern/api/ds/arraystf_node.h"
#include "C-kern/api/ds/foreach.h"
#include "C-kern/api/err.h"
#include "C-kern/api/math/int/log2.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/platform/virtmemory.h"
#endif

typedef struct arraystf_imp2_it        arraystf_imp2_it ;

typedef struct arraystf_keyval_t       arraystf_keyval_t ;

/* struct: arraystf_keyval_t
 * Describes the value of a key string at a certain memory offset. */
struct arraystf_keyval_t {
   size_t   data ;
   size_t   offset ;
} ;

static int initdiff_arraystfkeyval(/*out*/arraystf_keyval_t * keyval, arraystf_node_t * node, arraystf_node_t * key)
{
   int err ;
   size_t         size1   = node->size ;
   size_t         size2   = key->size ;
   const uint8_t  * addr1 = node->addr ;
   const uint8_t  * addr2 = key->addr ;
   size_t         size    = (size1 <= size2) ? size1 : size2 ;
   size_t         offset ;

   for(offset = 0; offset < size; ++offset ) {
      if (addr1[offset] == addr2[offset]) continue ;
      keyval->data = (size_t) (addr1[offset] ^ addr2[offset]) ;
      goto DONE ;
   }

   for( ; offset < size1; ++offset) {
      if (0 == addr1[offset]) continue ;
      keyval->data = addr1[offset] ;
      goto DONE ;
   }

   for( ; offset < size2; ++offset) {
      if (0 == addr2[offset]) continue ;
      keyval->data = addr2[offset] ;
      goto DONE ;
   }

   VALIDATE_INPARAM_TEST(size1 != size2, ABBRUCH, ) ;

   offset       = ~(size_t)0 ;
   keyval->data = (size1 ^ size2) ;

DONE:
   keyval->offset = offset ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

static inline void setdata_arraystfkeyval(arraystf_keyval_t * keyval, arraystf_node_t * node)
{
   if (keyval->offset < node->size) {
      keyval->data = node->addr[keyval->offset] ;
   } else if (0 == (~ keyval->offset)) {
      keyval->data = node->size ;
   } else {
      keyval->data = 0 ;
   }
}

/* struct: arraystf_imp2_it
 * Extends <arraystf_imp_it> with additional information. */
struct arraystf_imp2_it {
   /* variable: impit
    * Base type <arraystf_imp_it>. */
   arraystf_imp_it   impit ;
   /* variable: objectsize
    * Memory size in bytes of object which contains <arraystf_node_t>. */
   size_t            objectsize ;
   /* variable: nodeoffset
    * Offset in bytes from startaddr of allocated object to contained <arraystf_node_t>. */
   size_t            nodeoffset ;
} ;

// group: helper

static int copynode_arraystfimp2(const arraystf_imp_it * impit, const struct arraystf_node_t * node, /*out*/struct arraystf_node_t ** copied_node)
{
   int err ;
   const arraystf_imp2_it * impit2 = (const arraystf_imp2_it *) impit ;
   void                   * memblock ;

   err = impit2->impit.malloc(&impit2->impit, impit2->objectsize, &memblock) ;
   if (err) return err ;

   memcpy(memblock, (((const uint8_t*)node) - impit2->nodeoffset), impit2->objectsize) ;

   *copied_node = (arraystf_node_t*) (((uint8_t*)memblock) + impit2->nodeoffset) ;

   return 0 ;
}

static int freenode_arraystfimp2(const arraystf_imp_it * impit, struct arraystf_node_t * node)
{
   const arraystf_imp2_it * impit2 = (const arraystf_imp2_it *) impit ;

   return impit2->impit.free(&impit2->impit, impit2->objectsize, ((uint8_t*)node) - impit2->nodeoffset) ;
}


// section: arraystf_imp_it

// group: helper

/* function: defaultmalloc_arraystfimpit
 * Default implementation of <arraystf_imp_it.malloc>. */
static int defaultmalloc_arraystfimpit(const arraystf_imp_it * impit, size_t size, /*out*/void ** memblock)
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

/* function: defaultfree_arraystfimpit
 * Default implementation of <arraystf_imp_it.free>. */
static int defaultfree_arraystfimpit(const arraystf_imp_it * impit, size_t size, void * memblock)
{
   (void) impit ;
   (void) size ;

   free(memblock) ;
   return 0 ;
}

// group: lifetime

arraystf_imp_it * default_arraystfimpit(void)
{
   static arraystf_imp_it impit = {
         .copynode = 0,
         .freenode = 0,
         .malloc   = &defaultmalloc_arraystfimpit,
         .free     = &defaultfree_arraystfimpit
      } ;

   return &impit ;
}

int new_arraystfimp(arraystf_imp_it ** impit, size_t objectsize, size_t nodeoffset)
{
   int err ;
   arraystf_imp2_it * new_obj ;

   VALIDATE_INPARAM_TEST(  objectsize >= sizeof(arraystf_node_t)
                        && nodeoffset <= objectsize-sizeof(arraystf_node_t), ABBRUCH, ) ;

   err = defaultmalloc_arraystfimpit(0, sizeof(arraystf_imp2_it), (void**)&new_obj) ;
   if (err) goto ABBRUCH ;

   new_obj->impit.copynode = &copynode_arraystfimp2 ;
   new_obj->impit.freenode = &freenode_arraystfimp2 ;
   new_obj->impit.malloc   = &defaultmalloc_arraystfimpit ;
   new_obj->impit.free     = &defaultfree_arraystfimpit ;
   new_obj->objectsize = objectsize ;
   new_obj->nodeoffset = nodeoffset ;

   *impit = &new_obj->impit ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int delete_arraystfimp(arraystf_imp_it ** impit)
{
   int err ;
   arraystf_imp_it * del_obj = *impit ;

   if (del_obj) {
      *impit = 0 ;

      err = defaultfree_arraystfimpit(del_obj, sizeof(arraystf_imp2_it), del_obj) ;

      if (err) goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}


// section: arraystf_t

// group: helper

static uint16_t s_arraystf_nrelemroot[6] = {
    [arraystf_4BITROOT_UNSORTED]  = 16
   ,[arraystf_6BITROOT_UNSORTED]  = 64
   ,[arraystf_MSBROOT]   = 8
   ,[arraystf_4BITROOT]  = 16
   ,[arraystf_6BITROOT]  = 64
   ,[arraystf_8BITROOT]  = 256
} ;

#define nrelemroot_arraystf(array)     (s_arraystf_nrelemroot[(array)->type])

#define objectsize_arraystf(type)      (sizeof(arraystf_t) + sizeof(arraystf_node_t*) * s_arraystf_nrelemroot[type])

#define isbranchtype_arraystf(node)    (0 != ((intptr_t)(node) & 0x01))

#define asbranch_arraystf(node)        ((arraystf_mwaybranch_t*) (0x01 ^ (intptr_t)(node)))

#define encodebranch_arraystf(branch)  ((arraystf_node_t*) (0x01 ^ (intptr_t)(branch)))

typedef struct arraystf_findresult_t   arraystf_findresult_t ;

struct arraystf_findresult_t {
   unsigned                rootindex ;
   unsigned                childindex ;
   unsigned                pchildindex ;
   arraystf_mwaybranch_t   * parent ;
   arraystf_mwaybranch_t   * pparent ;
   arraystf_node_t         * found_node ;
} ;

static int find_arraystf(const arraystf_t * array, arraystf_node_t * keynode, /*err;out*/arraystf_findresult_t * result)
{
   int err ;
   unsigned             rootindex ;
   arraystf_keyval_t    keyval ;
   uint8_t              rootbyte ;

   VALIDATE_INPARAM_TEST(keynode->size < ~(size_t)0, ABBRUCH, ) ;

   rootbyte = (uint8_t) ((keynode->size) ? keynode->addr[0] : 0) ;
   keyval.offset = 0 ;
   keyval.data   = rootbyte ;

   switch(array->type) {
   case arraystf_4BITROOT_UNSORTED:  rootindex = (keynode->size & 0x0f) ;
                              break ;
   case arraystf_6BITROOT_UNSORTED:  rootindex = (keynode->size & 0x3f) ;
                              break ;
   case arraystf_MSBROOT:     rootindex = log2_int(rootbyte) ;
                              break ;
   case arraystf_4BITROOT:    rootindex = (rootbyte >> 4) ;
                              break ;
   case arraystf_6BITROOT:    rootindex = (rootbyte >> 2) ;
                              break ;
   case arraystf_8BITROOT:    rootindex = rootbyte ;
                              break ;
   default:                   return EINVAL ;
   }

   arraystf_node_t       * node      = array->root[rootindex] ;
   arraystf_mwaybranch_t * pparent   = 0 ;
   arraystf_mwaybranch_t * parent    = 0 ;
   unsigned              childindex  = 0 ;
   unsigned              pchildindex = 0 ;

   err = ESRCH ;

   while (node) {

      if (isbranchtype_arraystf(node)) {
         pparent = parent ;
         parent  = asbranch_arraystf(node) ;
         if (parent->offset > keyval.offset) {
            keyval.offset = parent->offset ;
            setdata_arraystfkeyval(&keyval, keynode) ;
         }
         pchildindex = childindex ;
         childindex  = childindex_arraystfmwaybranch(parent, keyval.data) ;
         node  = parent->child[childindex] ;
      } else {
         if (     keynode->size == node->size
               && 0 == memcmp(node->addr, keynode->addr, keynode->size)) {
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
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

// group: lifetime

int new_arraystf(/*out*/arraystf_t ** array, arraystf_e type, arraystf_imp_it * impit)
{
   int err ;
   arraystf_t  * new_obj ;

   VALIDATE_INPARAM_TEST(*array == 0, ABBRUCH, ) ;

   VALIDATE_INPARAM_TEST(type < nrelementsof(s_arraystf_nrelemroot), ABBRUCH, LOG_INT(type)) ;

   if (! impit) {
      impit = default_arraystfimpit() ;
   }

   VALIDATE_INPARAM_TEST(     impit->malloc
                           && impit->free, ABBRUCH, ) ;

   const size_t objsize = objectsize_arraystf(type) ;

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

int delete_arraystf(arraystf_t ** array)
{
   int err = 0 ;
   int err2 ;
   arraystf_t * del_obj = *array ;

   if (del_obj) {
      *array = 0 ;

      typeof(del_obj->impit)           impit      = del_obj->impit ;
      typeof(del_obj->impit->freenode) freenodecb = del_obj->impit->freenode ;
      typeof(del_obj->impit->free)     freecb     = del_obj->impit->free ;

      for(unsigned i = 0; i < nrelemroot_arraystf(del_obj); ++i) {

         if (!del_obj->root[i]) continue ;

         arraystf_node_t * node = del_obj->root[i] ;

         if (!isbranchtype_arraystf(node)) {
            if (freenodecb) {
               err2 = freenodecb(impit, node) ;
               if (err2) err = err2 ;
            }
            continue ;
         }

         arraystf_mwaybranch_t * branch = asbranch_arraystf(node) ;
         node = branch->child[0] ;
         branch->child[0] = 0 ;
         branch->used = nrelementsof(branch->child)-1 ;

         for(;;) {

            for(;;) {
               if (node) {
                  if (isbranchtype_arraystf(node)) {
                     arraystf_mwaybranch_t * parent = branch ;
                     branch = asbranch_arraystf(node) ;
                     node = branch->child[0] ;
                     branch->child[0] = (arraystf_node_t*) parent ;
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
               arraystf_mwaybranch_t * parent ;
               parent = (arraystf_mwaybranch_t *) branch->child[0] ;
               err2 = freecb(impit, sizeof(arraystf_mwaybranch_t), branch) ;
               if (err2) err = err2 ;
               branch = parent ;
            } while(branch && !branch->used) ;

            if (!branch) break ;

            node = branch->child[branch->used --] ;
         }

      }

      const size_t objsize = objectsize_arraystf(del_obj->type) ;

      del_obj->impit->free(del_obj->impit, objsize, del_obj) ;
   }

   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}

// group: query

arraystf_node_t * at_arraystf(const arraystf_t * array, size_t size, const uint8_t keydata[size])
{
   int err ;
   arraystf_findresult_t found ;
   arraystf_node_t       key   = arraystf_node_INIT(size, &keydata[0]) ;

   err = find_arraystf(array, &key, &found) ;
   if (err) return 0 ;

   return found.found_node ;
}

// group: change

int tryinsert_arraystf(arraystf_t * array, arraystf_node_t * node, /*err*/struct arraystf_node_t ** inserted_or_existing_node)
{
   int err ;
   arraystf_findresult_t   found ;
   arraystf_keyval_t       keydiff ;
   unsigned                shift ;
   arraystf_node_t         * copied_node = 0 ;

   err = find_arraystf(array, node, &found) ;
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

   if (found.found_node) {

      err = initdiff_arraystfkeyval(&keydiff, found.found_node, node) ;
      if (err) goto ABBRUCH ;

      shift = log2_int(keydiff.data) & ~0x01u ;

      if (  ! found.parent
         || found.parent->offset < keydiff.offset
         || (     found.parent->offset == keydiff.offset
               && found.parent->shift  >  shift))  {

   // prefix matches (add new branch layer after found.parent)

         arraystf_mwaybranch_t * new_branch ;
         err = array->impit->malloc(array->impit, sizeof(arraystf_mwaybranch_t), (void**)&new_branch) ;
         if (err) goto ABBRUCH ;

         arraystf_keyval_t keynode ;
         keynode.offset = keydiff.offset ;
         setdata_arraystfkeyval(&keynode, node) ;
         keydiff.data ^= keynode.data ;

         init_arraystfmwaybranch(new_branch, keydiff.offset, shift, keydiff.data, found.found_node, keynode.data, node) ;

         if (found.parent) {
            found.parent->child[found.childindex] = encodebranch_arraystf(new_branch) ;
         } else {
            array->root[found.rootindex] = encodebranch_arraystf(new_branch) ;
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

      arraystf_mwaybranch_t * branch = found.parent ;
      for (unsigned i = nrelementsof(branch->child)-1; ;) {
         if (branch->child[i]) {
            if (isbranchtype_arraystf(branch->child[i])) {
               branch = asbranch_arraystf(branch->child[i]) ;
               i = nrelementsof(branch->child)-1;
               continue ;
            }

            err = initdiff_arraystfkeyval(&keydiff, branch->child[i], node) ;
            if (err) goto ABBRUCH ;
            break ;
         }
         if (!(i--)) {
            err = EINVAL ;
            goto ABBRUCH ;
         }
      }

      shift = log2_int(keydiff.data) & ~0x01u ;

      if (     found.parent->offset == keydiff.offset
            && found.parent->shift  == shift)  {
         // prefix does match
         found.parent->child[found.childindex] = node ;
         ++ found.parent->used ;
         goto DONE ;
      }

   }

   // not so simple case prefix differs (add new branch layer between root & found.parent)

   assert(found.parent) ;
   arraystf_mwaybranch_t   * parent = 0 ;
   arraystf_mwaybranch_t   * branch = asbranch_arraystf(array->root[found.rootindex]) ;
   arraystf_keyval_t       keynode ;

   unsigned childindex = 0 ;

   while (     branch->offset < keydiff.offset
            || (     branch->offset == keydiff.offset
                  && branch->shift  > shift)) {
      parent = branch ;
      keynode.offset = branch->offset ;
      setdata_arraystfkeyval(&keynode, node) ;
      childindex = childindex_arraystfmwaybranch(branch, keynode.data) ;
      arraystf_node_t * child = branch->child[childindex] ;
      assert(child) ;
      assert(isbranchtype_arraystf(child)) ;
      branch = asbranch_arraystf(child) ;
   }

   arraystf_mwaybranch_t * new_branch ;
   err = array->impit->malloc(array->impit, sizeof(arraystf_mwaybranch_t), (void**)&new_branch) ;
   if (err) goto ABBRUCH ;

   keynode.offset = keydiff.offset ;
   setdata_arraystfkeyval(&keynode, node) ;
   keydiff.data ^= keynode.data ;

   init_arraystfmwaybranch(new_branch, keydiff.offset, shift, keydiff.data, encodebranch_arraystf(branch), keynode.data, node) ;

   if (parent) {
      parent->child[childindex] = encodebranch_arraystf(new_branch) ;
   } else {
      array->root[found.rootindex] = encodebranch_arraystf(new_branch) ;
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

int tryremove_arraystf(arraystf_t * array, size_t size, const uint8_t keydata[size], /*out*/struct arraystf_node_t ** removed_node/*could be 0*/)
{
   int err ;
   int err2 ;
   arraystf_findresult_t found ;
   arraystf_node_t       key = arraystf_node_INIT(size, keydata) ;

   err = find_arraystf(array, &key, &found) ;
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
               arraystf_node_t * other_child = found.parent->child[i] ;
               if (found.pparent) {
                  setchild_arraystfmwaybranch(found.pparent, found.pchildindex, other_child) ;
               } else {
                  array->root[found.rootindex] = other_child ;
               }
               break ;
            }
            if (!(i--)) {
               err = EINVAL ;
               goto ABBRUCH ;
            }
         }

         err = array->impit->free(array->impit, sizeof(arraystf_mwaybranch_t), found.parent) ;

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

int remove_arraystf(arraystf_t * array, size_t size, const uint8_t keydata[size], /*out*/struct arraystf_node_t ** removed_node/*could be 0*/)
{
   int err ;

   err = tryremove_arraystf(array, size, keydata, removed_node) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int insert_arraystf(arraystf_t * array, struct arraystf_node_t * node, /*out*/struct arraystf_node_t ** inserted_node/*0 => not returned*/)
{
   int err ;
   arraystf_node_t * inserted_or_existing_node ;

   err = tryinsert_arraystf(array, node, &inserted_or_existing_node) ;
   if (err) goto ABBRUCH ;

   if (inserted_node) {
      *inserted_node = inserted_or_existing_node ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}


// section: arraystf_iterator_t

int init_arraystfiterator(/*out*/arraystf_iterator_t * iter, arraystf_t * array)
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
      arraystf_mwaybranch_t * branch ;
      unsigned              ci ;
   } pos ;

   err = init_binarystack(stack, (/*guessed depth*/16) * /*objectsize*/sizeof(pos) ) ;
   if (err) goto ABBRUCH ;

   iter->stack = stack ;
   iter->ri    = 0 ;

   return 0 ;
ABBRUCH:
   MM_FREE(&objectmem) ;
   LOG_ABORT(err) ;
   return err ;
}

int free_arraystfiterator(arraystf_iterator_t * iter)
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

bool next_arraystfiterator(arraystf_iterator_t * iter, arraystf_t * array, /*out*/struct arraystf_node_t ** _node)
{
   int err ;
   size_t         nrelemroot = nrelemroot_arraystf(array) ;
   struct {
      arraystf_mwaybranch_t * branch ;
      unsigned              ci ;
   }              pos ;

   for (;;) {

      if (isempty_binarystack(iter->stack)) {

         arraystf_node_t * rootnode ;

         for(;;) {
            if (iter->ri >= nrelemroot) {
               return false ;
            }
            rootnode = array->root[iter->ri ++] ;
            if (rootnode) {
               if (!isbranchtype_arraystf(rootnode)) {
                  *_node = rootnode ;
                  return true ;
               }
               break ;
            }
         }

         pos.branch = asbranch_arraystf(rootnode) ;
         pos.ci     = 0 ;

      } else {

         err = pop_binarystack(iter->stack, sizeof(pos), &pos) ;
         if (err) goto ABBRUCH ;

      }

      for(;;) {
         arraystf_node_t * childnode = pos.branch->child[pos.ci ++] ;

         if (childnode) {

            if (pos.ci < nrelementsof(pos.branch->child)) {
               err = push_binarystack(iter->stack, sizeof(pos), &pos) ;
               if (err) goto ABBRUCH ;
            }

            if (isbranchtype_arraystf(childnode)) {
               pos.branch = asbranch_arraystf(childnode) ;
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

#define TEST(CONDITION) TEST_ONERROR_GOTO(CONDITION, ABBRUCH)

static int test_arraystfkeyval(void)
{
   arraystf_keyval_t    keyval ;
   arraystf_node_t      node1 ;
   arraystf_node_t      node2 ;
   const uint8_t        * key1 ;

   // TEST initdiff_arraystfkeyval
   key1 = (const uint8_t*) "1234" ;
   for(unsigned i = 0; i < 4; ++i) {
      node1 = (arraystf_node_t) arraystf_node_INIT(i, key1) ;
      node2 = (arraystf_node_t) arraystf_node_INIT(i+1, key1) ;
      memset(&keyval, 0, sizeof(keyval)) ;
      TEST(0 == initdiff_arraystfkeyval(&keyval, &node1, &node2)) ;
      if (i < 4) {
         TEST(i == keyval.offset) ;
         TEST(key1[i] == keyval.data) ;
      } else {
         // compare size
         TEST(0 == ~ keyval.offset) ;
         TEST((5^4) == keyval.data) ;
      }
   }
   node1 = (arraystf_node_t) arraystf_node_INIT(3, (const uint8_t*) "12\x0f") ;
   node2 = (arraystf_node_t) arraystf_node_INIT(3, (const uint8_t*) "12\xf0") ;
   memset(&keyval, 0, sizeof(keyval)) ;
   TEST(0 == initdiff_arraystfkeyval(&keyval, &node1, &node2)) ;
   TEST(2 == keyval.offset) ;
   TEST(255 == keyval.data) ;
   node1 = (arraystf_node_t) arraystf_node_INIT(7, (const uint8_t*) "124444\x1f") ;
   node2 = (arraystf_node_t) arraystf_node_INIT(10, (const uint8_t*) "124444\x2fxxx") ;
   memset(&keyval, 0, sizeof(keyval)) ;
   TEST(0 == initdiff_arraystfkeyval(&keyval, &node1, &node2)) ;
   TEST(6 == keyval.offset) ;
   TEST(0x30 == keyval.data) ;

   // TEST EINVAL initdiff_arraystfkeyval
   key1 = (const uint8_t*) "1234" ;
   node1 = (arraystf_node_t) arraystf_node_INIT(5, key1) ;
   node2 = (arraystf_node_t) arraystf_node_INIT(5, key1) ;
   TEST(EINVAL == initdiff_arraystfkeyval(&keyval, &node1, &node2)) ;

   // TEST setdata_arraystfkeyval (offset lower key size)
   key1 = (const uint8_t*) "0123456789ABCDEF" ;
   node1 = (arraystf_node_t) arraystf_node_INIT(17, key1) ;
   for(unsigned i = 0; i < 17; ++i) {
      keyval.data   = ~(size_t)0 ;
      keyval.offset = i ;
      setdata_arraystfkeyval(&keyval, &node1) ;
      TEST(key1[i] == keyval.data /*not changed*/) ;
   }

   // TEST setdata_arraystfkeyval (offset higher key size)
   key1 = (const uint8_t*) "0123456789ABCDEF" ;
   node1 = (arraystf_node_t) arraystf_node_INIT(17, key1) ;
   for(unsigned i = 17; i < ~(size_t)0; i <<= 1, i++) {
      keyval.data   = ~(size_t)0 ;
      keyval.offset = i ;
      setdata_arraystfkeyval(&keyval, &node1) ;
      TEST(keyval.offset == i /*not changed*/) ;
      TEST(keyval.data   == 0 /*always 0*/) ;
   }

   // TEST setdata_arraystfkeyval (special value -1)
   key1 = (const uint8_t*) "0123456789ABCDEF" ;
   for(unsigned i = 1; i <= 17; i++) {
      node1 = (arraystf_node_t) arraystf_node_INIT(i, key1) ;
      keyval.data   = ~(size_t)0 ;
      keyval.offset = ~(size_t)0 ;
      setdata_arraystfkeyval(&keyval, &node1) ;
      TEST(keyval.offset == ~(size_t)0 /*not changed*/) ;
      TEST(keyval.data   == i /*always key size*/) ;
   }

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

typedef struct testnode_t              testnode_t ;

struct testnode_t {
   arraystf_node_t node ;
   uint8_t         copycount ;
   uint8_t         freecount ;
   uint8_t         key[6] ;
} ;

static int test_copynode(const arraystf_imp_it * impit, const arraystf_node_t * node, arraystf_node_t ** copied_node)
{
   (void) impit ;
   ++ ((testnode_t*)(intptr_t)node)->copycount ;
   *copied_node = (arraystf_node_t*) (intptr_t) node ;
   return 0 ;
}

static int test_freenode(const arraystf_imp_it * impit, arraystf_node_t * node)
{
   (void) impit ;
   ++ ((testnode_t*)node)->freecount ;
   return 0 ;
}

static int test_freenodeerr(const arraystf_imp_it * impit, arraystf_node_t * node)
{
   (void) impit ;
   ++ ((testnode_t*)node)->freecount ;
   return 12345 ;
}

static int test_initfree(void)
{
   const size_t      nrnodes  = 100000 ;
   vm_block_t        memblock = vm_block_INIT_FREEABLE ;
   arraystf_t        * array  = 0 ;
   arraystf_imp_it   impit    = *default_arraystfimpit() ;
   testnode_t        * nodea ;

   // prepare
   impit.freenode = &test_freenode ;
   impit.copynode = &test_copynode ;
   TEST(0 == init_vmblock(&memblock, (pagesize_vm()-1+nrnodes*sizeof(testnode_t)) / pagesize_vm() )) ;
   nodea = (testnode_t *) memblock.addr ;

   // TEST static init
   TEST(0 == array) ;

   // TEST init, double free
   for(arraystf_e type = arraystf_4BITROOT_UNSORTED; type <= arraystf_8BITROOT; ++type) {
      TEST(0 == new_arraystf(&array, type, 0)) ;
      TEST(0 != array) ;
      TEST(type == array->type) ;
      TEST(0 != array->impit) ;
      TEST(0 == array->impit->copynode) ;
      TEST(0 == array->impit->freenode) ;
      TEST(0 != array->impit->malloc) ;
      TEST(0 != array->impit->free) ;
      TEST(0 == length_arraystf(array)) ;
      switch(type) {
      case arraystf_4BITROOT_UNSORTED:  TEST(nrelemroot_arraystf(array) == 16) ; break ;
      case arraystf_6BITROOT_UNSORTED:  TEST(nrelemroot_arraystf(array) == 64) ; break ;
      case arraystf_MSBROOT:     TEST(nrelemroot_arraystf(array) == 8) ; break ;
      case arraystf_4BITROOT:    TEST(nrelemroot_arraystf(array) == 16) ; break ;
      case arraystf_6BITROOT:    TEST(nrelemroot_arraystf(array) == 64) ; break ;
      case arraystf_8BITROOT:    TEST(nrelemroot_arraystf(array) == 256) ; break ;
      }
      for(unsigned i = 0; i < nrelemroot_arraystf(array); ++i) {
         TEST(0 == array->root[i]) ;
      }
      TEST(0 == delete_arraystf(&array)) ;
      TEST(0 == array) ;
      TEST(0 == delete_arraystf(&array)) ;
      TEST(0 == array) ;
   }

   // TEST root distributions
   for(arraystf_e type = arraystf_4BITROOT_UNSORTED; type <= arraystf_8BITROOT; ++type) {
      TEST(0 == new_arraystf(&array, type, 0)) ;
      uint8_t keyvalue[256] = { 0 } ;
      for(size_t byte = 0; byte < 256; ++byte) {
         testnode_t      node            = { .node = arraystf_node_INIT(byte, keyvalue) } ;
         arraystf_node_t * inserted_node = 0 ;
         keyvalue[0] = (uint8_t) byte ;
         TEST(0 == tryinsert_arraystf(array, &node.node, &inserted_node)) ;
         TEST(inserted_node == &node.node) ;
         TEST(1 == length_arraystf(array)) ;
         size_t ri ;
         switch(type) {
         case arraystf_4BITROOT_UNSORTED:  ri = (byte & 15) ; break ;
         case arraystf_6BITROOT_UNSORTED:  ri = (byte & 63) ; break ;
         case arraystf_MSBROOT:     ri = log2_int(byte) ; break ;
         case arraystf_4BITROOT:    ri = (byte >> 4) ; break ;
         case arraystf_6BITROOT:    ri = (byte >> 2) ; break ;
         case arraystf_8BITROOT:    ri = byte ; break ;
         }
         TEST(ri < nrelemroot_arraystf(array)) ;
         TEST(&node.node == array->root[ri]) ;
         for(unsigned i = 0; i < nrelemroot_arraystf(array); ++i) {
            if (i == ri) continue ;
            TEST(0 == array->root[i]) ;
         }
         TEST(0 == tryremove_arraystf(array, node.node.size, node.node.addr, 0)) ;
         TEST(0 == length_arraystf(array)) ;
         TEST(0 == array->root[ri]) ;
      }
      TEST(0 == delete_arraystf(&array)) ;
   }

   // TEST insert_arraystf (1 level)
   TEST(0 == new_arraystf(&array, arraystf_MSBROOT, &impit)) ;
   nodea[4] = (testnode_t) { .node = arraystf_node_INIT(1, nodea[4].key), .key = { 4 } } ;
   TEST(0 == tryinsert_arraystf(array, &nodea[4].node, 0))
   TEST(&nodea[4].node == array->root[2]) ;
   for(uint8_t pos = 5; pos <= 7; ++pos) {
      arraystf_node_t * inserted_node = 0 ;
      nodea[pos] = (testnode_t) { .node = arraystf_node_INIT(1, nodea[pos].key), .key = { pos } } ;
      TEST(0 == tryinsert_arraystf(array, &nodea[pos].node, &inserted_node)) ;
      TEST(inserted_node == &nodea[pos].node) ;
      TEST(0 == nodea[pos].freecount) ;
      TEST(1 == nodea[pos].copycount) ;
      TEST(pos-3u == length_arraystf(array)) ;
      TEST(isbranchtype_arraystf(array->root[2])) ;
      TEST(0 == asbranch_arraystf(array->root[2])->shift) ;
      TEST(pos-3u == asbranch_arraystf(array->root[2])->used) ;
   }
   for(uint8_t pos = 4; pos <= 7; ++pos) {
      TEST(&nodea[pos].node == asbranch_arraystf(array->root[2])->child[pos-4]) ;
      TEST(&nodea[pos].node == at_arraystf(array, 1, &pos)) ;
   }
   TEST(0 == at_arraystf(array, 0, 0)) ;
   TEST(0 == at_arraystf(array, 5, (const uint8_t*)"00000")) ;

   // TEST remove_arraystf (1 level)
   for(uint8_t pos = 4; pos <= 7; ++pos) {
      arraystf_node_t * removed_node = (void*)1 ;
      TEST(0 == tryremove_arraystf(array, 1, &pos, &removed_node))
      TEST(0 == removed_node) ;
      TEST(1 == nodea[pos].copycount) ;
      TEST(1 == nodea[pos].freecount) ;
      TEST(0 == at_arraystf(array, 1, &pos)) ;
      if (pos < 6) {
         TEST(array->root[2]) ;
         TEST(isbranchtype_arraystf(array->root[2])) ;
      } else if (pos == 6) {
         TEST(&nodea[7].node == array->root[2]) ;
      } else {
         TEST(! array->root[2]) ;
      }
      TEST(7u-pos == length_arraystf(array)) ;
   }

   // TEST insert_arraystf (2 levels)
   arraystf_mwaybranch_t * branch1 = 0 ;
   for(uint8_t pos = 16; pos <= 31; ++pos) {
      nodea[pos] = (testnode_t) { .node = arraystf_node_INIT(1, nodea[pos].key), .key = { pos } } ;
      TEST(0 == tryinsert_arraystf(array, &nodea[pos].node, 0))
      TEST(1 == nodea[pos].copycount) ;
      TEST(0 == nodea[pos].freecount) ;
      TEST(pos-15u == length_arraystf(array)) ;
      if (pos == 16) {
         TEST(&nodea[16].node == array->root[4]) ;
      } else if (pos == 17) {
         TEST(isbranchtype_arraystf(array->root[4])) ;
         branch1 = asbranch_arraystf(array->root[4]) ;
         TEST(0 == branch1->shift) ;
         TEST(&nodea[16].node  == branch1->child[0]) ;
         TEST(&nodea[17].node  == branch1->child[1]) ;
      } else if (pos <= 19) {
         TEST(isbranchtype_arraystf(array->root[4])) ;
         TEST(branch1 == asbranch_arraystf(array->root[4])) ;
         TEST(&nodea[pos].node == branch1->child[pos-16]) ;
      } else if (pos == 20 || pos == 24 || pos == 28) {
         TEST(isbranchtype_arraystf(array->root[4])) ;
         if (pos == 20) {
            arraystf_mwaybranch_t * branch2 = asbranch_arraystf(array->root[4]) ;
            TEST(2 == branch2->shift) ;
            TEST(branch1 == asbranch_arraystf(branch2->child[0])) ;
            branch1 = branch2 ;
         }
         TEST(&nodea[pos].node == branch1->child[(pos-16)/4]) ;
      } else {
         TEST(isbranchtype_arraystf(array->root[4])) ;
         TEST(branch1 == asbranch_arraystf(array->root[4])) ;
         TEST(isbranchtype_arraystf(branch1->child[(pos-16)/4])) ;
         arraystf_mwaybranch_t * branch2 = asbranch_arraystf(branch1->child[(pos-16)/4]) ;
         TEST(&nodea[pos&~0x03u].node == branch2->child[0]) ;
         TEST(&nodea[pos].node == branch2->child[pos&0x03u]) ;
      }
   }

   // TEST remove_arraystf (2 levels)
   for(uint8_t pos = 16; pos <= 31; ++pos) {
      arraystf_node_t * removed_node = (void*)1 ;
      TEST(0 == tryremove_arraystf(array, 1, &pos, &removed_node)) ;
      TEST(0 == removed_node) ;
      TEST(1 == nodea[pos].copycount) ;
      TEST(1 == nodea[pos].freecount) ;
      TEST(0 == at_arraystf(array, 1, &pos)) ;
      TEST(31u-pos == length_arraystf(array)) ;
      if (pos <= 17) {
         TEST(1 == isbranchtype_arraystf(asbranch_arraystf(array->root[4])->child[0])) ;
      } else if (pos == 18) {
         TEST(&nodea[19].node == asbranch_arraystf(array->root[4])->child[0]) ;
      } else if (pos == 19) {
         TEST(0 == asbranch_arraystf(array->root[4])->child[0]) ;
      } else if (pos < 22) {
         TEST(1 == isbranchtype_arraystf(asbranch_arraystf(array->root[4])->child[1])) ;
      } else if (pos == 22) {
         TEST(1 == isbranchtype_arraystf(array->root[4])) ;
         TEST(2 == asbranch_arraystf(array->root[4])->shift) ;
         TEST(&nodea[23].node == asbranch_arraystf(array->root[4])->child[1]) ;
      } else if (pos <= 26) {
         TEST(1 == isbranchtype_arraystf(array->root[4])) ;
         TEST(2 == asbranch_arraystf(array->root[4])->shift) ;
      } else if (pos <= 29) {
         TEST(1 == isbranchtype_arraystf(array->root[4])) ;
         TEST(0 == asbranch_arraystf(array->root[4])->shift) ;
      } else if (pos == 30) {
         TEST(&nodea[31].node == array->root[4]) ;
      } else if (pos == 31) {
         TEST(0 == array->root[4]) ;
      }
   }

   // TEST insert_arraystf at_arraystf remove_arraystf forward
   for(arraystf_e type = arraystf_4BITROOT_UNSORTED; type <= arraystf_8BITROOT; type+=4) {
      TEST(0 == delete_arraystf(&array)) ;
      TEST(0 == new_arraystf(&array, type, &impit)) ;
      for(size_t pos = 0; pos < nrnodes; ++pos) {
         nodea[pos] = (testnode_t) { .node = arraystf_node_INIT(4, nodea[pos].key), .key = { 0, (uint8_t)(pos/65536), (uint8_t)(pos/256), (uint8_t)pos } } ;
         TEST(0 == tryinsert_arraystf(array, &nodea[pos].node, 0))
         TEST(1 == nodea[pos].copycount) ;
         TEST(0 == nodea[pos].freecount) ;
         TEST(1+pos == length_arraystf(array)) ;
      }
      for(size_t pos = 0; pos < nrnodes; ++pos) {
         TEST(&nodea[pos].node == at_arraystf(array, 4, nodea[pos].key)) ;
      }
      for(size_t pos = 0; pos < nrnodes; ++pos) {
         TEST(0 == tryremove_arraystf(array, 4, nodea[pos].key, 0)) ;
         TEST(1 == nodea[pos].copycount) ;
         TEST(1 == nodea[pos].freecount) ;
         TEST(0 == at_arraystf(array,4, nodea[pos].key))
         TEST(nrnodes-1-pos == length_arraystf(array)) ;
      }
   }

   // TEST insert_arraystf at_arraystf remove_arraystf backward
   for(arraystf_e type = arraystf_4BITROOT_UNSORTED+1; type <= arraystf_8BITROOT; type+=4) {
      TEST(0 == delete_arraystf(&array)) ;
      TEST(0 == new_arraystf(&array, type, &impit)) ;
      for(size_t pos = nrnodes; (pos --); ) {
         nodea[pos] = (testnode_t) { .node = arraystf_node_INIT(4, nodea[pos].key), .key = { 0, (uint8_t)(pos/65536), (uint8_t)(pos/256), (uint8_t)pos } } ;
         TEST(0 == tryinsert_arraystf(array, &nodea[pos].node, 0))
         TEST(1 == nodea[pos].copycount) ;
         TEST(0 == nodea[pos].freecount) ;
         TEST(nrnodes-pos == length_arraystf(array)) ;
      }
      for(size_t pos = nrnodes; (pos --); ) {
         TEST(&nodea[pos].node == at_arraystf(array, 4, nodea[pos].key)) ;
      }
      for(size_t pos = nrnodes; (pos --); ) {
         TEST(0 != at_arraystf(array, 4, nodea[pos].key)) ;
         TEST(0 == tryremove_arraystf(array, 4, nodea[pos].key, 0)) ;
         TEST(1 == nodea[pos].copycount) ;
         TEST(1 == nodea[pos].freecount) ;
         TEST(0 == at_arraystf(array, 4, nodea[pos].key))
         TEST(pos == length_arraystf(array)) ;
         nodea[pos].copycount = 0 ;
         nodea[pos].freecount = 0 ;
      }
   }

   // TEST random elements (insert_arraystf, remove_arraystf)
   srand(99999) ;
   for(size_t count2 = 0; count2 < 10; ++count2) {
      for(size_t count = 0; count < nrnodes; ++count) {
         size_t pos = ((unsigned)rand() % nrnodes) ;
         if (nodea[pos].copycount) {
            TEST(&nodea[pos].node == at_arraystf(array, 4, nodea[pos].key)) ;
            TEST(0 == tryremove_arraystf(array, 4, nodea[pos].key, 0)) ;
            TEST(1 == nodea[pos].copycount) ;
            TEST(1 == nodea[pos].freecount) ;
            nodea[pos].copycount = 0 ;
            nodea[pos].freecount = 0 ;
         } else {
            TEST(0 == tryinsert_arraystf(array, &nodea[pos].node, 0)) ;
            TEST(1 == nodea[pos].copycount) ;
            TEST(0 == nodea[pos].freecount) ;
         }
      }
   }
   TEST(0 == delete_arraystf(&array)) ;

   // TEST delete_arraystf frees memory
   for(arraystf_e type = arraystf_4BITROOT_UNSORTED+2; type <= arraystf_8BITROOT; type += 4) {
      TEST(0 == new_arraystf(&array, type, 0)) ;
      TEST(0 != impolicy_arraystf(array)) ;
      TEST(0 == impolicy_arraystf(array)->copynode) ;
      TEST(0 == impolicy_arraystf(array)->freenode) ;
      for(size_t pos = nrnodes; (pos --); ) {
         TEST(0 == tryinsert_arraystf(array, &nodea[pos].node, 0))
         TEST(nrnodes-pos == length_arraystf(array)) ;
      }
      TEST(0 == delete_arraystf(&array)) ;
      TEST(0 == array) ;
   }

   // TEST delete_arraystf frees also nodes
   impit.copynode = 0 ;
   impit.freenode = &test_freenode ;
   for(arraystf_e type = arraystf_4BITROOT_UNSORTED+3; type <= arraystf_8BITROOT; type += 4) {
      memset(nodea, 0, sizeof(testnode_t) * nrnodes) ;
      TEST(0 == new_arraystf(&array, type, &impit)) ;
      unsigned nr = 0 ;
      for(uint32_t key = 4; key; key <<= 2) {
         for(uint32_t key2 = 0; key2 <= 11; ++key2) {
            uint32_t keysum = key + key2 ;
            TEST(nr < nrnodes) ;
            nodea[nr] = (testnode_t) { .node = arraystf_node_INIT(4, nodea[nr].key), .key = { (uint8_t)(keysum/(256*65536)), (uint8_t)(keysum/65536), (uint8_t)(keysum/256), (uint8_t)keysum } } ;
            TEST(0 == tryinsert_arraystf(array, &nodea[nr].node, 0))
            TEST((++nr) == length_arraystf(array)) ;
         }
      }
      TEST(0 == delete_arraystf(&array)) ;
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
   (void) delete_arraystf(&array) ;
   free_vmblock(&memblock) ;
   return EINVAL ;
}

static int test_error(void)
{
   const size_t      nrnodes  = 10000 ;
   vm_block_t        memblock = vm_block_INIT_FREEABLE ;
   arraystf_imp_it   impit    = *default_arraystfimpit() ;
   arraystf_t        * array  = 0 ;
   testnode_t        * nodea ;

   // prepare
   impit.freenode = &test_freenode ;
   TEST(0 == init_vmblock(&memblock, (pagesize_vm()-1+nrnodes*sizeof(testnode_t)) / pagesize_vm() )) ;
   nodea = (testnode_t *) memblock.addr ;
   TEST(0 == new_arraystf(&array, arraystf_4BITROOT_UNSORTED, &impit)) ;

   // TEST EINVAL
   nodea[0] = (testnode_t) { .node = arraystf_node_INIT(~(size_t)0, nodea[0].key), .key = { 0 } } ;
      // key has length ~(size_t)0
   TEST(EINVAL == tryinsert_arraystf(array, &nodea[0].node, 0)) ;
      // (array != 0)
   TEST(EINVAL == new_arraystf(&array, arraystf_4BITROOT_UNSORTED, &impit)) ;
   arraystf_t * array2 = 0 ;
      // type invalid
   TEST(EINVAL == new_arraystf(&array2, -1, &impit)) ;

   // TEST EEXIST
   nodea[0] = (testnode_t) { .node = arraystf_node_INIT(1, nodea[0].key), .key = { 0 } } ;
   TEST(0 == tryinsert_arraystf(array, &nodea[0].node, 0)) ;
   arraystf_node_t * existing_node = 0 ;
   nodea[1] = (testnode_t) { .node = arraystf_node_INIT(1, nodea[1].key), .key = { 0 } } ;
   TEST(EEXIST == tryinsert_arraystf(array, &nodea[1].node, &existing_node)) ;  // no log
   TEST(&nodea[0].node == existing_node) ;
   existing_node = 0 ;
   TEST(EEXIST == insert_arraystf(array, &nodea[1].node, &existing_node)) ;     // log
   TEST(0 == existing_node) ;

   // TEST ESRCH
   arraystf_findresult_t found ;
   nodea[1] = (testnode_t) { .node = arraystf_node_INIT(1, nodea[1].key), .key = { 1 } } ;
   TEST(0 == at_arraystf(array, 1, nodea[1].key)) ;               // no log
   TEST(ESRCH == find_arraystf(array, &nodea[1].node, &found)) ;  // no log
   TEST(ESRCH == tryremove_arraystf(array, 1, nodea[1].key, 0)) ; // no log
   TEST(ESRCH == remove_arraystf(array, 1, nodea[1].key, 0)) ;    // log
   nodea[0].freecount = 0 ;
   TEST(0 == tryremove_arraystf(array, 1, nodea[0].key, 0)) ;
   TEST(1 == nodea[0].freecount) ;

   // TEST free memory error
   impit.freenode = &test_freenodeerr ;
   for(size_t pos = 0; pos < nrnodes; ++pos) {
      nodea[pos] = (testnode_t) { .node = arraystf_node_INIT(2, nodea[pos].key), .key = { (uint8_t)(pos/256), (uint8_t)pos } } ;
      TEST(0 == tryinsert_arraystf(array, &nodea[pos].node, 0))
      TEST(1+pos == length_arraystf(array)) ;
   }
   TEST(12345 == delete_arraystf(&array)) ;
   for(unsigned pos = 0; pos < nrnodes; ++pos) {
      TEST(1 == nodea[pos].freecount) ;
   }

   // unprepare
   TEST(0 == free_vmblock(&memblock)) ;

   return 0 ;
ABBRUCH:
   (void) delete_arraystf(&array) ;
   free_vmblock(&memblock) ;
   return EINVAL ;
}

static int test_arraystfimp(void)
{
   arraystf_imp_it * impit = 0 ;
   arraystf_t      * array = 0 ;
   uint16_t        keys[1000] ;

   // TEST static init
   TEST(0 == impit) ;

   // TEST init, double free
   TEST(0 == new_arraystfimp(&impit, 32, 16)) ;
   TEST(0 != impit) ;
   TEST(impit->copynode == &copynode_arraystfimp2) ;
   TEST(impit->freenode == &freenode_arraystfimp2) ;
   TEST(impit->malloc   == &defaultmalloc_arraystfimpit) ;
   TEST(impit->free     == &defaultfree_arraystfimpit) ;
   TEST(32 == ((arraystf_imp2_it*)impit)->objectsize) ;
   TEST(16 == ((arraystf_imp2_it*)impit)->nodeoffset) ;
   TEST(0 == delete_arraystfimp(&impit)) ;
   TEST(0 == impit) ;
   TEST(0 == delete_arraystfimp(&impit)) ;
   TEST(0 == impit) ;

   // TEST copynode_arraystfimp2
   TEST(0 == new_arraystfimp(&impit, 64, 32)) ;
   TEST(0 != impit) ;
   TEST(64 == ((arraystf_imp2_it*)impit)->objectsize) ;
   TEST(32 == ((arraystf_imp2_it*)impit)->nodeoffset) ;
   uint8_t memblock[64] ;
   for(uint8_t i = 0; i < 64; ++i) {
      memblock[i] = i ;
   }
   uint8_t * copied_node = 0 ;
   TEST(0 == impit->copynode(impit, (arraystf_node_t*)(32+memblock), (arraystf_node_t**)&copied_node)) ;
   TEST(0 != copied_node) ;
   for(int i = 0; i < 32; ++i) {
      TEST(32+i == copied_node[i]) ;
      TEST(i    == copied_node[i-32]) ;
   }

   // TEST freenode_arraystfimp2
   TEST(0 == impit->freenode(impit, (arraystf_node_t*)copied_node)) ;
   TEST(0 == delete_arraystfimp(&impit)) ;
   TEST(0 == impit) ;

   // TEST with arraystf
   TEST(0 == new_arraystfimp(&impit, sizeof(arraystf_node_t)+16, 4)) ;
   TEST(0 == new_arraystf(&array, arraystf_6BITROOT_UNSORTED, impit)) ;
   TEST(0 != array) ;
   TEST(impit == array->impit) ;
   for(uint16_t i = 0; i < nrelementsof(keys); ++i) {
      arraystf_node_t * node = 0 ;
      keys[i] = i ;
      *(arraystf_node_t*)(4+memblock) = (arraystf_node_t) arraystf_node_INIT(2, (uint8_t*)&keys[i]) ;
      TEST(0 == insert_arraystf(array, (arraystf_node_t*)(4+memblock), &node)) ;
      TEST(0 != node) ;
      TEST((arraystf_node_t*)(4+memblock) != node) ;
   }
   for(uint16_t i = 0; i < nrelementsof(keys); ++i) {
      arraystf_node_t * node = at_arraystf(array, 2, (uint8_t*)&i) ;
      TEST(0 != node) ;
      TEST(&keys[i] == (const void*)node->addr) ;
      TEST(0 == memcmp(-4 + (uint8_t*)node, memblock, 4)) ;
      TEST(0 == memcmp(sizeof(*node) + (uint8_t*)node, memblock+4+sizeof(*node), 12)) ;
   }
   for(uint16_t i = 0; i < nrelementsof(keys); ++i) {
      TEST(0 == remove_arraystf(array, 2, (uint8_t*)&i, 0)) ;
   }
   TEST(0 == delete_arraystf(&array)) ;
   TEST(0 == array) ;
   TEST(0 == delete_arraystfimp(&impit)) ;
   TEST(0 == impit) ;

   return 0 ;
ABBRUCH:
   (void) delete_arraystf(&array) ;
   return EINVAL ;
}

static int test_iterator(void)
{
   const size_t   nrnodes  = 30000 ;
   vm_block_t     memblock = vm_block_INIT_FREEABLE ;
   arraystf_t     * array  = 0 ;
   testnode_t     * nodea ;
   size_t         nextpos ;

   // prepare
   TEST(0 == init_vmblock(&memblock, (pagesize_vm()-1+nrnodes*sizeof(testnode_t)) / pagesize_vm() )) ;
   nodea = (testnode_t *) memblock.addr ;

   TEST(0 == new_arraystf(&array, arraystf_8BITROOT, 0)) ;
   for(size_t i = 0; i < nrnodes; ++i) {
      snprintf((char*)nodea[i].key, sizeof(nodea[i].key), "%05zu", i) ;
      nodea[i].node = (arraystf_node_t) arraystf_node_INIT(5, nodea[i].key) ;
      TEST(0 == insert_arraystf(array, &nodea[i].node, 0)) ;
   }

   // TEST foreach all
   nextpos = 0 ;
   foreach (_arraystf, array, node) {
      char nextnr[6] ;
      snprintf(nextnr, sizeof(nextnr), "%05zu", nextpos ++) ;
      TEST(0 == strcmp((const char*)node->addr, nextnr)) ;
   }
   TEST(nextpos == nrnodes) ;

   // TEST foreach break after nrnodes/2
   nextpos = 0 ;
   foreach (_arraystf, array, node) {
      char nextnr[6] ;
      snprintf(nextnr, sizeof(nextnr), "%05zu", nextpos ++) ;
      TEST(0 == strcmp((const char*)node->addr, nextnr)) ;
      if (nextpos == nrnodes/2) break ;
   }
   TEST(nextpos == nrnodes/2) ;

   // TEST foreach (nrnodes/2 .. nrnodes) after remove
   for(size_t i = 0; i < nrnodes/2; ++i) {
      uint8_t key[10] ;
      sprintf((char*)key, "%05ld", (long)i) ;
      TEST(0 == remove_arraystf(array, 5, key, 0)) ;
   }
   nextpos = nrnodes/2 ;
   foreach (_arraystf, array, node) {
      char nextnr[6] ;
      snprintf(nextnr, sizeof(nextnr), "%05zu", nextpos ++) ;
      TEST(0 == strcmp((const char*)node->addr, nextnr)) ;
   }
   TEST(nextpos == nrnodes) ;

   // unprepare
   TEST(0 == delete_arraystf(&array)) ;
   TEST(0 == free_vmblock(&memblock)) ;

   return 0 ;
ABBRUCH:
   (void) delete_arraystf(&array) ;
   free_vmblock(&memblock) ;
   return EINVAL ;
}

static int test_zerokey(void)
{
   arraystf_imp_it * impit = 0 ;
   arraystf_t      * array = 0 ;
   vm_block_t     memblock = vm_block_INIT_FREEABLE ;
   const uint16_t nrkeys   = 256 ;
   const uint16_t keylen   = 1024 ;
   uint8_t         * keys ;

   // prepare
   TEST(0 == new_arraystfimp(&impit, sizeof(arraystf_node_t), 0)) ;
   TEST(0 != impit) ;
   TEST(0 == init_vmblock(&memblock, (pagesize_vm()-1 + (size_t)nrkeys*keylen) / pagesize_vm() )) ;
   keys = memblock.addr ;

   // TEST with arraystf
   memset(memblock.addr, 0, memblock.size) ;
   for(unsigned i = 0; i < nrkeys; ++i) {
      keys[i*keylen] = (uint8_t) i;
   }
   TEST(0 == new_arraystf(&array, arraystf_6BITROOT, impit)) ;
   TEST(0 != array) ;
   TEST(impit == array->impit) ;
   for(unsigned i = 0; i < nrkeys; ++i) {
      for(unsigned l = 1; l <= 25; ++l) {
         arraystf_node_t node = (arraystf_node_t) arraystf_node_INIT(keylen/l, &keys[i*keylen]) ;
         TEST(0 == insert_arraystf(array, &node, 0)) ;
      }
   }
   for(unsigned i = 0; i < nrkeys; ++i) {
      for(unsigned l = 1; l <= 25; ++l) {
         arraystf_node_t * node = at_arraystf(array, keylen/l, &keys[i*keylen]) ;
         TEST(0 != node) ;
         TEST(node->addr == &keys[i*keylen]) ;
         TEST(node->size == keylen/l) ;
      }
   }
   for(unsigned i = 0; i < nrkeys; ++i) {
      for(unsigned l = 1; l <= 25; ++l) {
         TEST(0 == remove_arraystf(array, keylen/l, &keys[i*keylen], 0)) ;
      }
   }
   TEST(0 == length_arraystf(array)) ;
   TEST(0 == delete_arraystf(&array)) ;
   TEST(0 == delete_arraystfimp(&impit)) ;

   // unprepare
   TEST(0 == free_vmblock(&memblock)) ;

   return 0 ;
ABBRUCH:
   (void) delete_arraystf(&array) ;
   free_vmblock(&memblock) ;
   return EINVAL ;
}

int unittest_ds_inmem_arraystf()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   for(int i = 0; i < 2; ++i) {
   TEST(0 == free_resourceusage(&usage)) ;
   TEST(0 == init_resourceusage(&usage)) ;

   if (test_arraystfkeyval()) goto ABBRUCH ;
   if (test_initfree())       goto ABBRUCH ;
   if (test_error())          goto ABBRUCH ;
   if (test_arraystfimp())    goto ABBRUCH ;
   if (test_iterator())       goto ABBRUCH ;
   if (test_zerokey())        goto ABBRUCH ;

   if (0 == same_resourceusage(&usage)) break ;
   LOG_CLEARBUFFER() ;
   }
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
