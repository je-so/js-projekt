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
#include "C-kern/api/ds/inmem/node/arraystf_node.h"
#include "C-kern/api/ds/foreach.h"
#include "C-kern/api/ds/typeadapter.h"
#include "C-kern/api/err.h"
#include "C-kern/api/math/int/log2.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#include "C-kern/api/string/string.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/platform/virtmemory.h"
#endif


typedef struct arraystf_keyval_t       arraystf_keyval_t ;

typedef struct arraystf_pos_t          arraystf_pos_t ;


struct arraystf_pos_t {
   arraystf_mwaybranch_t * branch ;
   unsigned              ci ;
} ;


// section: arraystf_node_t

// group: helper

/* function: compiletime_assert
 * Checks that <arraystf_node_t> can be castet into <conststring_t>. */
static inline void compiletime_assert(void)
{
   conststring_t     cnststr ;
   arraystf_node_t   node ;

   // TEST conststring_t has only members: addr & size
   static_assert(sizeof(node)   == sizeof(cnststr), "considered same number of members in conststring_t") ;

   // TEST arraystf_node_t.addr & arraystf_node_t.size compatible with conststring_t.addr & conststring_t.size
   static_assert(offsetof(arraystf_node_t, addr) == offsetof(conststring_t, addr), "arraystf_node_t compatible with conststring_t") ;
   static_assert(sizeof(node.addr) == sizeof(cnststr.addr), "arraystf_node_t compatible with conststring_t") ;
   static_assert(offsetof(arraystf_node_t, size) == offsetof(conststring_t, size), "arraystf_node_t compatible with conststring_t") ;
   static_assert(sizeof(node.size) == sizeof(cnststr.size), "arraystf_node_t compatible with conststring_t") ;
}

/* struct: arraystf_keyval_t
 * Describes the value of a key string at a certain memory offset. */
struct arraystf_keyval_t {
   size_t   data ;
   size_t   offset ;
} ;

/* function: init_arraystfkeyval
 * Loads next key data byte at offset from node into keyval.
 * The key is defined to be 0 at offsets beyond <arraystf_node_t.size>.
 * The special offset (-1) returns in keyval the length of the key.
 * This special key encoding ensures that no key is a prefix of another (longer) key. */
static void init_arraystfkeyval(/*out*/arraystf_keyval_t * keyval, size_t offset, arraystf_node_t * node)
{
   if (offset < node->size) {
      keyval->data = node->addr[offset] ;
   } else if (0 == (~ offset)) {
      keyval->data = node->size ;
   } else {
      keyval->data = 0 ;
   }
   keyval->offset = offset ;
}

/* function: initdiff_arraystfkeyval
 * Searches the first position where the two keys differ.
 * This position is returned in keyval as offset and data value.
 * The returned data value is result of the the xor operation
 * between the two key values at the computed offset.
 *
 * The xor operation is chosen so that the first differing bit
 * generates a set bit in <arraystf_keyval_t.data>. With <log2_int>
 * it is possible to  compute the bit index and to determine the value
 * of <arraystf_mwaybranch_t.shift>. */
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

   offset       = SIZE_MAX ;
   keyval->data = (size1 ^ size2) ;

DONE:
   keyval->offset = offset ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
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

typedef struct arraystf_findresult_t   arraystf_findresult_t ;

struct arraystf_findresult_t {
   unsigned                rootindex ;
   unsigned                childindex ;
   unsigned                pchildindex ;
   arraystf_mwaybranch_t   * parent ;
   arraystf_mwaybranch_t   * pparent ;
   arraystf_unode_t        * found_node ;
   arraystf_node_t         * found_key ;
} ;

static int find_arraystf(const arraystf_t * array, arraystf_node_t * keynode, /*err;out*/arraystf_findresult_t * result, size_t offset_node)
{
   int err ;
   unsigned             rootindex ;
   arraystf_keyval_t    keyval ;
   uint8_t              rootbyte ;

   VALIDATE_INPARAM_TEST(keynode->size < SIZE_MAX, ABBRUCH, ) ;

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

   arraystf_unode_t      * node      = array->root[rootindex] ;
   arraystf_mwaybranch_t * pparent   = 0 ;
   arraystf_mwaybranch_t * parent    = 0 ;
   unsigned              childindex  = 0 ;
   unsigned              pchildindex = 0 ;

   err = ESRCH ;

   while (node) {

      if (isbranchtype_arraystfunode(node)) {
         pparent = parent ;
         parent  = asbranch_arraystfunode(node) ;
         if (parent->offset > keyval.offset) {
            init_arraystfkeyval(&keyval, parent->offset, keynode) ;
         }
         pchildindex = childindex ;
         childindex  = childindex_arraystfmwaybranch(parent, keyval.data) ;
         node        = parent->child[childindex] ;
      } else {
         result->found_key = asnode_arraystfunode(node, offset_node) ;
         if (     keynode->size == result->found_key->size
               && 0 == memcmp(keynode->addr, result->found_key->addr, keynode->size)) {
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
   // result->found_key  ! already set in case node != 0

   return err ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

// group: lifetime

int new_arraystf(/*out*/arraystf_t ** array, arraystf_e type)
{
   int err ;
   memblock_t  new_obj = memblock_INIT_FREEABLE ;

   VALIDATE_INPARAM_TEST(*array == 0, ABBRUCH, ) ;

   VALIDATE_INPARAM_TEST(type < nrelementsof(s_arraystf_nrelemroot), ABBRUCH, LOG_INT(type)) ;

   const size_t objsize = objectsize_arraystf(type) ;

   err = MM_RESIZE(objsize, &new_obj) ;
   if (err) goto ABBRUCH ;

   memset(new_obj.addr, 0, objsize) ;
   ((arraystf_t*)(new_obj.addr))->type  = type ;

   *array = ((arraystf_t*)(new_obj.addr)) ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int delete_arraystf(arraystf_t ** array, typeadapter_iot * typeadp)
{
   int err = 0 ;
   int err2 ;
   arraystf_t * del_obj = *array ;

   if (del_obj) {
      *array = 0 ;

      for(unsigned i = 0; i < nrelemroot_arraystf(del_obj); ++i) {

         if (!del_obj->root[i]) continue ;

         arraystf_unode_t * node = del_obj->root[i] ;

         if (!isbranchtype_arraystfunode(node)) {
            if (typeadp) {
               err2 = execfree_typeadapteriot(typeadp, asgeneric_arraystfunode(node)) ;
               if (err2) err = err2 ;
            }
            continue ;
         }

         arraystf_mwaybranch_t * branch = asbranch_arraystfunode(node) ;
         node = branch->child[0] ;
         branch->child[0] = 0 ;
         branch->used = nrelementsof(branch->child)-1 ;

         for(;;) {

            for(;;) {
               if (node) {
                  if (isbranchtype_arraystfunode(node)) {
                     arraystf_mwaybranch_t * parent = branch ;
                     branch = asbranch_arraystfunode(node) ;
                     node = branch->child[0] ;
                     branch->child[0] = (arraystf_unode_t*) parent ;
                     branch->used     = nrelementsof(branch->child)-1 ;
                     continue ;
                  } else if (typeadp) {
                     err2 = execfree_typeadapteriot(typeadp, asgeneric_arraystfunode(node)) ;
                     if (err2) err = err2 ;
                  }
               }

               if (! branch->used)  break ;
               node = branch->child[branch->used --] ;
            }

            do {
               arraystf_mwaybranch_t   * parent = (arraystf_mwaybranch_t *) branch->child[0] ;
               memblock_t              mblock   = memblock_INIT(sizeof(arraystf_mwaybranch_t), (uint8_t*)branch) ;
               err2 = MM_FREE(&mblock) ;
               if (err2) err = err2 ;
               branch = parent ;
            } while(branch && !branch->used) ;

            if (!branch) break ;

            node = branch->child[branch->used --] ;
         }

      }

      const size_t objsize = objectsize_arraystf(del_obj->type) ;

      memblock_t mblock = memblock_INIT(objsize, (uint8_t*)del_obj) ;
      err2 = MM_FREE(&mblock) ;
      if (err2) err = err2 ;
   }

   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}

// group: query

struct generic_object_t * at_arraystf(const arraystf_t * array, size_t size, const uint8_t keydata[size], size_t offset_node)
{
   int err ;
   arraystf_findresult_t found ;
   arraystf_node_t       key   = arraystf_node_INIT(size, &keydata[0]) ;

   err = find_arraystf(array, &key, &found, offset_node) ;
   if (err) return 0 ;

   return asgeneric_arraystfunode(found.found_node) ;
}

// group: change

int tryinsert_arraystf(arraystf_t * array, struct generic_object_t * node, /*out,err*/struct generic_object_t ** inserted_or_existing_node, struct typeadapter_iot * typeadp, size_t offset_node)
{
   int err ;
   arraystf_findresult_t   found ;
   arraystf_keyval_t       keydiff ;
   arraystf_node_t         * nodekey   = asnode_arraystfunode(fromgeneric_arraystfunode(node), offset_node) ;
   typeof(node)            copied_node = 0 ;
   unsigned                shift ;

   err = find_arraystf(array, nodekey, &found, offset_node) ;
   if (ESRCH != err) {
      if (inserted_or_existing_node) {
         *inserted_or_existing_node = (0 == err) ? asgeneric_arraystfunode(found.found_node) : 0 ;
      }
      return (0 == err) ? EEXIST : err ;
   }

   if (typeadp) {
      err = execcopy_typeadapteriot(typeadp, &copied_node, node) ;
      if (err) goto ABBRUCH ;
      node    = copied_node ;
      nodekey = asnode_arraystfunode(fromgeneric_arraystfunode(node), offset_node) ;
   }

   VALIDATE_INPARAM_TEST(0 == ((uintptr_t)node&0x01), ABBRUCH, ) ;

   if (found.found_node) {

      err = initdiff_arraystfkeyval(&keydiff, found.found_key, nodekey) ;
      if (err) goto ABBRUCH ;

      shift = log2_int(keydiff.data) & ~0x01u ;

      if (  ! found.parent
         || found.parent->offset < keydiff.offset
         || (     found.parent->offset == keydiff.offset
               && found.parent->shift  >  shift))  {

   // prefix matches (add new branch layer after found.parent)

         memblock_t mblock = memblock_INIT_FREEABLE ;
         err = MM_RESIZE(sizeof(arraystf_mwaybranch_t), &mblock) ;
         if (err) goto ABBRUCH ;

         arraystf_mwaybranch_t * new_branch = (arraystf_mwaybranch_t *) mblock.addr ;

         arraystf_keyval_t keynode ;
         init_arraystfkeyval(&keynode, keydiff.offset, nodekey) ;
         keydiff.data ^= keynode.data ;

         init_arraystfmwaybranch(new_branch, keydiff.offset, shift, keydiff.data, found.found_node, keynode.data, fromgeneric_arraystfunode(node)) ;

         if (found.parent) {
            found.parent->child[found.childindex] = asunode_arraystfmwaybranch(new_branch) ;
         } else {
            array->root[found.rootindex] = asunode_arraystfmwaybranch(new_branch) ;
         }
         goto DONE ;
      }

   // fall through to not so simple case

   } else /*(! found.found_node)*/ {

   // simple case (parent is 0)

      if (! found.parent) {
         array->root[found.rootindex] = fromgeneric_arraystfunode(node) ;
         goto DONE ;
      }

   // get pos of already stored node / check prefix match => second simple case

      arraystf_mwaybranch_t * branch = found.parent ;
      for (unsigned i = nrelementsof(branch->child)-1; ;) {
         if (branch->child[i]) {
            if (isbranchtype_arraystfunode(branch->child[i])) {
               branch = asbranch_arraystfunode(branch->child[i]) ;
               i = nrelementsof(branch->child)-1;
               continue ;
            }

            err = initdiff_arraystfkeyval(&keydiff, asnode_arraystfunode(branch->child[i], offset_node), nodekey) ;
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
         found.parent->child[found.childindex] = fromgeneric_arraystfunode(node) ;
         ++ found.parent->used ;
         goto DONE ;
      }

   }

   // not so simple case prefix differs (add new branch layer between root & found.parent)

   assert(found.parent) ;
   arraystf_mwaybranch_t   * parent = 0 ;
   arraystf_mwaybranch_t   * branch = asbranch_arraystfunode(array->root[found.rootindex]) ;
   arraystf_keyval_t       keynode ;

   unsigned childindex = 0 ;

   while (     branch->offset < keydiff.offset
            || (     branch->offset == keydiff.offset
                  && branch->shift  > shift)) {
      parent = branch ;
      init_arraystfkeyval(&keynode, branch->offset, nodekey) ;
      childindex = childindex_arraystfmwaybranch(branch, keynode.data) ;
      arraystf_unode_t * child = branch->child[childindex] ;
      assert(child) ;
      assert(isbranchtype_arraystfunode(child)) ;
      branch = asbranch_arraystfunode(child) ;
   }

   memblock_t mblock = memblock_INIT_FREEABLE ;
   err = MM_RESIZE(sizeof(arraystf_mwaybranch_t), &mblock) ;
   if (err) goto ABBRUCH ;

   arraystf_mwaybranch_t * new_branch = (arraystf_mwaybranch_t*) mblock.addr ;

   init_arraystfkeyval(&keynode, keydiff.offset, nodekey) ;
   keydiff.data ^= keynode.data ;

   init_arraystfmwaybranch(new_branch, keydiff.offset, shift, keydiff.data, asunode_arraystfmwaybranch(branch), keynode.data, fromgeneric_arraystfunode(node)) ;

   if (parent) {
      parent->child[childindex] = asunode_arraystfmwaybranch(new_branch) ;
   } else {
      array->root[found.rootindex] = asunode_arraystfmwaybranch(new_branch) ;
   }

DONE:

   ++ array->length ;

   if (inserted_or_existing_node) {
      *inserted_or_existing_node = node ;
   }

   return 0 ;
ABBRUCH:
   if (copied_node) {
      (void) execfree_typeadapteriot(typeadp, copied_node) ;
   }
   LOG_ABORT(err) ;
   return err ;
}

int tryremove_arraystf(arraystf_t * array, size_t size, const uint8_t keydata[size], /*out*/struct generic_object_t ** removed_node/*could be 0*/, size_t offset_node)
{
   int err ;
   arraystf_findresult_t found ;
   arraystf_node_t       key = arraystf_node_INIT(size, keydata) ;

   err = find_arraystf(array, &key, &found, offset_node) ;
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
               arraystf_unode_t * other_child = found.parent->child[i] ;
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

         memblock_t mblock = memblock_INIT(sizeof(arraystf_mwaybranch_t), (uint8_t*)found.parent) ;
         err = MM_FREE(&mblock) ;
         (void) err /*IGNORE*/ ;
      }

   }

   assert(array->length > 0) ;
   -- array->length ;

   if (removed_node) {
      *removed_node = asgeneric_arraystfunode(found.found_node) ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int remove_arraystf(arraystf_t * array, size_t size, const uint8_t keydata[size], /*out*/struct generic_object_t ** removed_node/*could be 0*/, size_t offset_node)
{
   int err ;

   err = tryremove_arraystf(array, size, keydata, removed_node, offset_node) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

int insert_arraystf(arraystf_t * array, struct generic_object_t * node, /*out*/struct generic_object_t ** inserted_node, struct typeadapter_iot * typeadp, size_t offset_node)
{
   int err ;
   struct generic_object_t * inserted_or_existing_node ;

   err = tryinsert_arraystf(array, node, &inserted_or_existing_node, typeadp, offset_node) ;
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

   err = init_binarystack(stack, (/*guessed depth*/16) * /*objectsize*/sizeof(arraystf_pos_t) ) ;
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
      iter->stack = 0 ;

      err2 = MM_FREE(&objectmem) ;
      if (err2) err = err2 ;

      if (err) goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}

bool next_arraystfiterator(arraystf_iterator_t * iter, arraystf_t * array, /*out*/struct generic_object_t ** node)
{
   int err ;
   size_t         nrelemroot = nrelemroot_arraystf(array) ;
   arraystf_pos_t * pos ;

   for (;;) {

      pos = (arraystf_pos_t*) lastpushed_binarystack(iter->stack, sizeof(arraystf_pos_t)) ;

      if (0 == pos/*isempty_binarystack(iter->stack)*/) {

         arraystf_unode_t * rootnode ;

         for(;;) {
            if (iter->ri >= nrelemroot) {
               return false ;
            }
            rootnode = array->root[iter->ri ++] ;
            if (rootnode) {
               if (!isbranchtype_arraystfunode(rootnode)) {
                  *node = asgeneric_arraystfunode(rootnode) ;
                  return true ;
               }
               break ;
            }
         }

         err = pushgeneric_binarystack(iter->stack, &pos) ;
         if (err) goto ABBRUCH ;

         pos->branch = asbranch_arraystfunode(rootnode) ;
         pos->ci     = 0 ;

      }

      for (;;) {
         arraystf_unode_t * childnode = pos->branch->child[pos->ci ++] ;

         if (pos->ci >= nrelementsof(pos->branch->child)) {
            // pos becomes invalid
            err = pop_binarystack(iter->stack, sizeof(arraystf_pos_t)) ;
            if (err) goto ABBRUCH ;

            if (!childnode) break ;
         }

         if (childnode) {
            if (isbranchtype_arraystfunode(childnode)) {
               err = pushgeneric_binarystack(iter->stack, &pos) ;
               if (err) goto ABBRUCH ;
               pos->branch = asbranch_arraystfunode(childnode) ;
               pos->ci     = 0 ;
               continue ;
            } else {
               *node = asgeneric_arraystfunode(childnode) ;
               return true ;
            }
         }
      }

   }

   return false ;
ABBRUCH:
   // move iterator to end of container
   iter->ri = nrelemroot ;
   pop_binarystack(iter->stack, size_binarystack(iter->stack)) ;
   LOG_ABORT(err) ;
   return false ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_arraystfnode(void)
{
   arraystf_node_t node = arraystf_node_INIT(0,0) ;

   // TEST arraystf_node_INIT(0,0)
   TEST(0 == node.addr) ;
   TEST(0 == node.size) ;

   // TEST arraystf_node_INIT
   for(unsigned i = 0; i < 1000; i += 100) {
      node = (arraystf_node_t) arraystf_node_INIT(i+1,(const uint8_t*)i) ;
      TEST(i   == (uintptr_t)node.addr) ;
      TEST(i+1 == node.size) ;
   }

   // TEST asconststring_arraystfnode, fromconststring_arraystfnode
   for(unsigned i = 0; i < 1000; i += 100) {
      TEST((conststring_t*)i   == asconststring_arraystfnode((arraystf_node_t*)i)) ;
      TEST((arraystf_node_t*)i == fromconststring_arraystfnode((conststring_t*)i)) ;
   }

   return 0 ;
ABBRUCH:
   return EINVAL ;
}

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

   // TEST init_arraystfkeyval (offset lower key size)
   key1 = (const uint8_t*) "0123456789ABCDEF" ;
   node1 = (arraystf_node_t) arraystf_node_INIT(17, key1) ;
   for(unsigned i = 0; i < 17; ++i) {
      keyval.data   = 0 ;
      keyval.offset = i+1 ;
      init_arraystfkeyval(&keyval, i, &node1) ;
      TEST(key1[i] == keyval.data) ;
      TEST(i       == keyval.offset) ;
   }

   // TEST init_arraystfkeyval (offset higher key size)
   key1 = (const uint8_t*) "0123456789ABCDEF" ;
   node1 = (arraystf_node_t) arraystf_node_INIT(17, key1) ;
   for(unsigned i = 17; i < SIZE_MAX; i <<= 1, i++) {
      keyval.data   = 0 ;
      keyval.offset = i+1 ;
      init_arraystfkeyval(&keyval, i, &node1) ;
      TEST(0 == keyval.data /*always 0*/) ;
      TEST(i == keyval.offset) ;
   }

   // TEST init_arraystfkeyval (special value -1)
   key1 = (const uint8_t*) "0123456789ABCDEF" ;
   for(unsigned i = 1; i <= 17; i++) {
      node1 = (arraystf_node_t) arraystf_node_INIT(i, key1) ;
      keyval.data   = 0 ;
      keyval.offset = 0 ;
      init_arraystfkeyval(&keyval, SIZE_MAX, &node1) ;
      TEST(i        == keyval.data/*always key size*/) ;
      TEST(SIZE_MAX == keyval.offset) ;
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
   uint8_t         key[40] ;
   arraystf_node_t node2 ;
   uint8_t         key2[40] ;
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

typedef struct testnode_typeadapter_it    testnode_typeadapter_it ;

typeadapter_it_DECLARE(testnode_typeadapter_it, typeadapter_t, testnode_t)

arraystf_IMPLEMENT(testnode_t, _arraytest, node.addr)

static int test_initfree(void)
{
   const size_t      nrnodes    = 100000 ;
   vm_block_t        memblock   = vm_block_INIT_FREEABLE ;
   arraystf_t        * array    = 0 ;
   testnode_typeadapter_it
                     typeadt_ft = typeadapter_it_INIT(&test_copynode, &test_freenode) ;
   typeadapter_iot   typeadt    = typeadapter_iot_INIT(0, &typeadt_ft.generic) ;
   testnode_t        * nodea ;

   // prepare
   TEST(0 == init_vmblock(&memblock, (pagesize_vm()-1+nrnodes*sizeof(testnode_t)) / pagesize_vm() )) ;
   nodea = (testnode_t *) memblock.addr ;

   // TEST static init
   TEST(0 == array) ;

   // TEST init, double free
   for(arraystf_e type = arraystf_4BITROOT_UNSORTED; type <= arraystf_8BITROOT; ++type) {
      TEST(0 == new_arraytest(&array, type)) ;
      TEST(0 != array) ;
      TEST(type == array->type) ;
      TEST(0 == length_arraytest(array)) ;
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
      TEST(0 == delete_arraytest(&array, 0)) ;
      TEST(0 == array) ;
      TEST(0 == delete_arraytest(&array, 0)) ;
      TEST(0 == array) ;
   }

   // TEST root distributions
   for(arraystf_e type = arraystf_4BITROOT_UNSORTED; type <= arraystf_8BITROOT; ++type) {
      TEST(0 == new_arraytest(&array, type)) ;
      uint8_t keyvalue[256] = { 0 } ;
      for(size_t byte = 0; byte < 256; ++byte) {
         testnode_t  node            = { .node = arraystf_node_INIT(byte, keyvalue) } ;
         testnode_t  * inserted_node = 0 ;
         keyvalue[0] = (uint8_t) byte ;
         TEST(0 == tryinsert_arraytest(array, &node, &inserted_node, 0)) ;
         TEST(inserted_node == &node) ;
         TEST(1 == length_arraytest(array)) ;
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
         TEST(&node.node == asnode_arraystfunode(array->root[ri], offsetof(testnode_t,node))) ;
         for(unsigned i = 0; i < nrelemroot_arraystf(array); ++i) {
            if (i == ri) continue ;
            TEST(0 == array->root[i]) ;
         }
         TEST(0 == tryremove_arraytest(array, node.node.size, node.node.addr, 0)) ;
         TEST(0 == length_arraytest(array)) ;
         TEST(0 == array->root[ri]) ;
      }
      TEST(0 == delete_arraytest(&array, 0)) ;
   }

   // TEST insert_arraystf (1 level)
   TEST(0 == new_arraytest(&array, arraystf_MSBROOT)) ;
   nodea[4] = (testnode_t) { .node = arraystf_node_INIT(1, nodea[4].key), .key = { 4 } } ;
   TEST(0 == tryinsert_arraytest(array, &nodea[4], 0, &typeadt))
   TEST(&nodea[4].node == asnode_arraystfunode(array->root[2], offsetof(testnode_t,node))) ;
   for(uint8_t pos = 5; pos <= 7; ++pos) {
      testnode_t * inserted_node = 0 ;
      nodea[pos] = (testnode_t) { .node = arraystf_node_INIT(1, nodea[pos].key), .key = { pos } } ;
      TEST(0 == tryinsert_arraytest(array, &nodea[pos], &inserted_node, &typeadt)) ;
      TEST(inserted_node == &nodea[pos]) ;
      TEST(0 == nodea[pos].freecount) ;
      TEST(1 == nodea[pos].copycount) ;
      TEST(pos-3u == length_arraytest(array)) ;
      TEST(isbranchtype_arraystfunode(array->root[2])) ;
      TEST(0 == asbranch_arraystfunode(array->root[2])->shift) ;
      TEST(pos-3u == asbranch_arraystfunode(array->root[2])->used) ;
   }
   for(uint8_t pos = 4; pos <= 7; ++pos) {
      TEST(&nodea[pos].node == asnode_arraystfunode(asbranch_arraystfunode(array->root[2])->child[pos-4], offsetof(testnode_t,node))) ;
      TEST(&nodea[pos]      == at_arraytest(array, 1, &pos)) ;
   }
   TEST(0 == at_arraytest(array, 0, 0)) ;
   TEST(0 == at_arraytest(array, 5, (const uint8_t*)"00000")) ;

   // TEST remove_arraystf (1 level)
   for(uint8_t pos = 4; pos <= 7; ++pos) {
      testnode_t * removed_node = (void*)1 ;
      TEST(0 == tryremove_arraytest(array, 1, &pos, &removed_node))
      TEST(&nodea[pos] == removed_node) ;
      TEST(1 == nodea[pos].copycount) ;
      TEST(0 == nodea[pos].freecount) ;
      TEST(0 == at_arraytest(array, 1, &pos)) ;
      if (pos < 6) {
         TEST(array->root[2]) ;
         TEST(isbranchtype_arraystfunode(array->root[2])) ;
      } else if (pos == 6) {
         TEST(&nodea[7].node == asnode_arraystfunode(array->root[2], offsetof(testnode_t,node))) ;
      } else {
         TEST(! array->root[2]) ;
      }
      TEST(7u-pos == length_arraytest(array)) ;
   }

   // TEST insert_arraystf (2 levels)
   arraystf_mwaybranch_t * branch1 = 0 ;
   for(uint8_t pos = 16; pos <= 31; ++pos) {
      nodea[pos] = (testnode_t) { .node = arraystf_node_INIT(1, nodea[pos].key), .key = { pos } } ;
      TEST(0 == tryinsert_arraytest(array, &nodea[pos], 0, &typeadt))
      TEST(1 == nodea[pos].copycount) ;
      TEST(0 == nodea[pos].freecount) ;
      TEST(pos-15u == length_arraytest(array)) ;
      if (pos == 16) {
         TEST(&nodea[16].node == asnode_arraystfunode(array->root[4], offsetof(testnode_t,node))) ;
      } else if (pos == 17) {
         TEST(isbranchtype_arraystfunode(array->root[4])) ;
         branch1 = asbranch_arraystfunode(array->root[4]) ;
         TEST(0 == branch1->shift) ;
         TEST(&nodea[16].node  == asnode_arraystfunode(branch1->child[0], offsetof(testnode_t,node))) ;
         TEST(&nodea[17].node  == asnode_arraystfunode(branch1->child[1], offsetof(testnode_t,node))) ;
      } else if (pos <= 19) {
         TEST(isbranchtype_arraystfunode(array->root[4])) ;
         TEST(branch1 == asbranch_arraystfunode(array->root[4])) ;
         TEST(&nodea[pos].node == asnode_arraystfunode(branch1->child[pos-16], offsetof(testnode_t,node))) ;
      } else if (pos == 20 || pos == 24 || pos == 28) {
         TEST(isbranchtype_arraystfunode(array->root[4])) ;
         if (pos == 20) {
            arraystf_mwaybranch_t * branch2 = asbranch_arraystfunode(array->root[4]) ;
            TEST(2 == branch2->shift) ;
            TEST(branch1 == asbranch_arraystfunode(branch2->child[0])) ;
            branch1 = branch2 ;
         }
         TEST(&nodea[pos].node == asnode_arraystfunode(branch1->child[(pos-16)/4], offsetof(testnode_t,node))) ;
      } else {
         TEST(isbranchtype_arraystfunode(array->root[4])) ;
         TEST(branch1 == asbranch_arraystfunode(array->root[4])) ;
         TEST(isbranchtype_arraystfunode(branch1->child[(pos-16)/4])) ;
         arraystf_mwaybranch_t * branch2 = asbranch_arraystfunode(branch1->child[(pos-16)/4]) ;
         TEST(&nodea[pos&~0x03u].node == asnode_arraystfunode(branch2->child[0], offsetof(testnode_t,node))) ;
         TEST(&nodea[pos].node        == asnode_arraystfunode(branch2->child[pos&0x03u], offsetof(testnode_t,node))) ;
      }
   }

   // TEST remove_arraystf (2 levels)
   for(uint8_t pos = 16; pos <= 31; ++pos) {
      testnode_t * removed_node = 0 ;
      TEST(0 == tryremove_arraytest(array, 1, &pos, &removed_node)) ;
      TEST(&nodea[pos] == removed_node) ;
      TEST(1 == nodea[pos].copycount) ;
      TEST(0 == nodea[pos].freecount) ;
      TEST(0 == at_arraytest(array, 1, &pos)) ;
      TEST(31u-pos == length_arraytest(array)) ;
      if (pos <= 17) {
         TEST(1 == isbranchtype_arraystfunode(asbranch_arraystfunode(array->root[4])->child[0])) ;
      } else if (pos == 18) {
         TEST(&nodea[19].node == asnode_arraystfunode(asbranch_arraystfunode(array->root[4])->child[0], offsetof(testnode_t,node))) ;
      } else if (pos == 19) {
         TEST(0 == asbranch_arraystfunode(array->root[4])->child[0]) ;
      } else if (pos < 22) {
         TEST(1 == isbranchtype_arraystfunode(asbranch_arraystfunode(array->root[4])->child[1])) ;
      } else if (pos == 22) {
         TEST(1 == isbranchtype_arraystfunode(array->root[4])) ;
         TEST(2 == asbranch_arraystfunode(array->root[4])->shift) ;
         TEST(&nodea[23].node == asnode_arraystfunode(asbranch_arraystfunode(array->root[4])->child[1], offsetof(testnode_t,node))) ;
      } else if (pos <= 26) {
         TEST(1 == isbranchtype_arraystfunode(array->root[4])) ;
         TEST(2 == asbranch_arraystfunode(array->root[4])->shift) ;
      } else if (pos <= 29) {
         TEST(1 == isbranchtype_arraystfunode(array->root[4])) ;
         TEST(0 == asbranch_arraystfunode(array->root[4])->shift) ;
      } else if (pos == 30) {
         TEST(&nodea[31].node == asnode_arraystfunode(array->root[4], offsetof(testnode_t,node))) ;
      } else if (pos == 31) {
         TEST(0 == array->root[4]) ;
      }
   }

   // TEST insert_arraystf at_arraystf remove_arraystf forward
   for(arraystf_e type = arraystf_4BITROOT_UNSORTED; type <= arraystf_8BITROOT; type+=4) {
      TEST(0 == delete_arraytest(&array, 0)) ;
      TEST(0 == new_arraytest(&array, type)) ;
      for(size_t pos = 0; pos < nrnodes; ++pos) {
         testnode_t * inserted_node = 0 ;
         nodea[pos] = (testnode_t) { .node = arraystf_node_INIT(4, nodea[pos].key), .key = { 0, (uint8_t)(pos/65536), (uint8_t)(pos/256), (uint8_t)pos } } ;
         TEST(0 == tryinsert_arraytest(array, &nodea[pos], &inserted_node , 0))
         TEST(inserted_node == &nodea[pos]) ;
         TEST(1+pos == length_arraytest(array)) ;
      }
      for(size_t pos = 0; pos < nrnodes; ++pos) {
         TEST(&nodea[pos] == at_arraytest(array, 4, nodea[pos].key)) ;
      }
      for(size_t pos = 0; pos < nrnodes; ++pos) {
         testnode_t * removed_node = 0 ;
         TEST(0 == tryremove_arraytest(array, 4, nodea[pos].key, &removed_node)) ;
         TEST(&nodea[pos] == removed_node) ;
         TEST(0 == nodea[pos].copycount) ;
         TEST(0 == nodea[pos].freecount) ;
         TEST(0 == at_arraytest(array,4, nodea[pos].key))
         TEST(nrnodes-1-pos == length_arraytest(array)) ;
      }
   }

   // TEST insert_arraystf at_arraystf remove_arraystf backward
   for(arraystf_e type = arraystf_4BITROOT_UNSORTED+1; type <= arraystf_8BITROOT; type+=4) {
      TEST(0 == delete_arraytest(&array, 0)) ;
      TEST(0 == new_arraytest(&array, type)) ;
      for(size_t pos = nrnodes; (pos --); ) {
         nodea[pos] = (testnode_t) { .node = arraystf_node_INIT(4, nodea[pos].key), .key = { 0, (uint8_t)(pos/65536), (uint8_t)(pos/256), (uint8_t)pos } } ;
         TEST(0 == tryinsert_arraytest(array, &nodea[pos], 0, &typeadt))
         TEST(1 == nodea[pos].copycount) ;
         TEST(0 == nodea[pos].freecount) ;
         TEST(nrnodes-pos == length_arraytest(array)) ;
      }
      for(size_t pos = nrnodes; (pos --); ) {
         TEST(&nodea[pos] == at_arraytest(array, 4, nodea[pos].key)) ;
      }
      for(size_t pos = nrnodes; (pos --); ) {
         testnode_t * removed_node = 0 ;
         TEST(0 != at_arraytest(array, 4, nodea[pos].key)) ;
         TEST(0 == tryremove_arraytest(array, 4, nodea[pos].key, &removed_node)) ;
         TEST(&nodea[pos] == removed_node) ;
         TEST(1 == nodea[pos].copycount) ;
         TEST(0 == nodea[pos].freecount) ;
         TEST(0 == at_arraytest(array, 4, nodea[pos].key))
         TEST(pos == length_arraytest(array)) ;
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
            testnode_t * removed_node = 0 ;
            TEST(&nodea[pos] == at_arraytest(array, 4, nodea[pos].key)) ;
            TEST(0 == tryremove_arraytest(array, 4, nodea[pos].key, &removed_node)) ;
            TEST(&nodea[pos] == removed_node) ;
            TEST(1 == nodea[pos].copycount) ;
            TEST(0 == nodea[pos].freecount) ;
            nodea[pos].copycount = 0 ;
         } else {
            TEST(0 == tryinsert_arraytest(array, &nodea[pos], 0, &typeadt)) ;
            TEST(1 == nodea[pos].copycount) ;
            TEST(0 == nodea[pos].freecount) ;
         }
      }
   }
   TEST(0 == delete_arraytest(&array, 0)) ;
   for(size_t pos = nrnodes; (pos --); ) {
      TEST(0 == nodea[pos].freecount) ;
   }

   // TEST delete_arraystf frees nodes & memory (1)
   for(arraystf_e type = arraystf_4BITROOT_UNSORTED+2; type <= arraystf_8BITROOT; type += 4) {
      TEST(0 == new_arraytest(&array, type)) ;
      for(size_t pos = nrnodes; (pos --); ) {
         nodea[pos].copycount = 0 ;
         nodea[pos].freecount = 0 ;
         TEST(0 == tryinsert_arraytest(array, &nodea[pos], 0, 0))
         TEST(nrnodes-pos == length_arraytest(array)) ;
      }
      TEST(0 == delete_arraytest(&array, &typeadt)) ;
      TEST(0 == array) ;
      for(size_t pos = nrnodes; (pos --); ) {
         TEST(0 == nodea[pos].copycount) ;
         TEST(1 == nodea[pos].freecount) ;
      }
   }

   // TEST delete_arraystf frees nodes & memory (2)
   for(arraystf_e type = arraystf_4BITROOT_UNSORTED+3; type <= arraystf_8BITROOT; type += 4) {
      memset(nodea, 0, sizeof(testnode_t) * nrnodes) ;
      TEST(0 == new_arraytest(&array, type)) ;
      unsigned nr = 0 ;
      for(uint32_t key = 4; key; key <<= 2) {
         for(uint32_t key2 = 0; key2 <= 11; ++key2) {
            uint32_t keysum = key + key2 ;
            TEST(nr < nrnodes) ;
            nodea[nr] = (testnode_t) { .node = arraystf_node_INIT(4, nodea[nr].key), .key = { (uint8_t)(keysum/(256*65536)), (uint8_t)(keysum/65536), (uint8_t)(keysum/256), (uint8_t)keysum } } ;
            TEST(0 == tryinsert_arraytest(array, &nodea[nr], 0, &typeadt))
            TEST((++nr) == length_arraytest(array)) ;
         }
      }
      TEST(0 == delete_arraytest(&array, &typeadt)) ;
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
ABBRUCH:
   (void) delete_arraytest(&array, 0) ;
   free_vmblock(&memblock) ;
   return EINVAL ;
}

static int test_error(void)
{
   const size_t      nrnodes    = 10000 ;
   vm_block_t        memblock   = vm_block_INIT_FREEABLE ;
   testnode_typeadapter_it
                     typeadt_ft = typeadapter_it_INIT_FREEABLE ;
   typeadapter_iot   typeadt    = typeadapter_iot_INIT(0, &typeadt_ft.generic) ;
   arraystf_t        * array    = 0 ;
   testnode_t        * nodea ;
   char              * logbuffer ;
   size_t            logbufsize1 ;
   size_t            logbufsize2 ;

   // prepare
   TEST(0 == init_vmblock(&memblock, (pagesize_vm()-1+nrnodes*sizeof(testnode_t)) / pagesize_vm() )) ;
   nodea = (testnode_t *) memblock.addr ;
   TEST(0 == new_arraytest(&array, arraystf_4BITROOT_UNSORTED)) ;

   // TEST EINVAL
   nodea[0] = (testnode_t) { .node = arraystf_node_INIT(SIZE_MAX, nodea[0].key), .key = { 0 } } ;
      // key has length SIZE_MAX
   LOG_GETBUFFER(&logbuffer, &logbufsize1) ;
   TEST(EINVAL == tryinsert_arraytest(array, &nodea[0], 0, 0)) ;
   LOG_GETBUFFER(&logbuffer, &logbufsize2) ;
   TEST(logbufsize1 < logbufsize2) ;
      // (array != 0)
   LOG_GETBUFFER(&logbuffer, &logbufsize1) ;
   TEST(EINVAL == new_arraytest(&array, arraystf_4BITROOT_UNSORTED)) ;
   LOG_GETBUFFER(&logbuffer, &logbufsize2) ;
   TEST(logbufsize1 < logbufsize2) ;
   arraystf_t * array2 = 0 ;
      // type invalid
   LOG_GETBUFFER(&logbuffer, &logbufsize1) ;
   TEST(EINVAL == new_arraytest(&array2, -1)) ;
   LOG_GETBUFFER(&logbuffer, &logbufsize2) ;
   TEST(logbufsize1 < logbufsize2) ;

   // TEST EEXIST
   nodea[0] = (testnode_t) { .node = arraystf_node_INIT(1, nodea[0].key), .key = { 0 } } ;
   TEST(0 == tryinsert_arraytest(array, &nodea[0], 0, 0)) ;
   testnode_t * existing_node = 0 ;
   nodea[1] = (testnode_t) { .node = arraystf_node_INIT(1, nodea[1].key), .key = { 0 } } ;
   LOG_GETBUFFER(&logbuffer, &logbufsize1) ;
   TEST(EEXIST == tryinsert_arraytest(array, &nodea[1], &existing_node, 0)) ;  // no log
   LOG_GETBUFFER(&logbuffer, &logbufsize2) ;
   TEST(logbufsize1 == logbufsize2) ;
   TEST(&nodea[0]   == existing_node) ;
   existing_node = 0 ;
   TEST(EEXIST == insert_arraytest(array, &nodea[1], &existing_node, 0)) ;     // log
   LOG_GETBUFFER(&logbuffer, &logbufsize2) ;
   TEST(logbufsize1 < logbufsize2) ;
   TEST(0 == existing_node) ;

   // TEST ESRCH
   arraystf_findresult_t found ;
   nodea[1] = (testnode_t) { .node = arraystf_node_INIT(1, nodea[1].key), .key = { 1 } } ;
   LOG_GETBUFFER(&logbuffer, &logbufsize1) ;
   TEST(0     == at_arraytest(array, 1, nodea[1].key)) ;           // no log
   TEST(ESRCH == find_arraystf(array, &nodea[1].node, &found, offsetof(testnode_t,node))) ;   // no log
   TEST(ESRCH == tryremove_arraytest(array, 1, nodea[1].key, 0)) ; // no log
   LOG_GETBUFFER(&logbuffer, &logbufsize2) ;
   TEST(logbufsize1 == logbufsize2) ;
   TEST(ESRCH == remove_arraytest(array, 1, nodea[1].key, 0)) ;    // log
   LOG_GETBUFFER(&logbuffer, &logbufsize2) ;
   TEST(logbufsize1 < logbufsize2) ;
   nodea[0].freecount = 0 ;
   testnode_t * removed_node = 0 ;
   TEST(0 == tryremove_arraytest(array, 1, nodea[0].key, &removed_node)) ;
   TEST(&nodea[0] == removed_node) ;

   // TEST free memory error
   typeadt_ft.freeobj = &test_freenodeerr ;
   for(size_t pos = 0; pos < nrnodes; ++pos) {
      nodea[pos] = (testnode_t) { .node = arraystf_node_INIT(2, nodea[pos].key), .key = { (uint8_t)(pos/256), (uint8_t)pos } } ;
      TEST(0 == tryinsert_arraytest(array, &nodea[pos], 0, 0))
      TEST(1+pos == length_arraytest(array)) ;
   }
   TEST(12345 == delete_arraytest(&array, &typeadt)) ;
   for(unsigned pos = 0; pos < nrnodes; ++pos) {
      TEST(1 == nodea[pos].freecount) ;
   }

   // unprepare
   TEST(0 == free_vmblock(&memblock)) ;

   return 0 ;
ABBRUCH:
   (void) delete_arraytest(&array, 0) ;
   free_vmblock(&memblock) ;
   return EINVAL ;
}

static int test_iterator(void)
{
   const size_t   nrnodes   = 30000 ;
   vm_block_t     memblock  = vm_block_INIT_FREEABLE ;
   arraystf_iterator_t iter = arraystf_iterator_INIT_FREEABLE ;
   arraystf_t     * array   = 0 ;
   testnode_t     * nodea ;
   size_t         nextpos ;

   // prepare
   TEST(0 == init_vmblock(&memblock, (pagesize_vm()-1+nrnodes*sizeof(testnode_t)) / pagesize_vm() )) ;
   nodea = (testnode_t *) memblock.addr ;
   TEST(0 == new_arraytest(&array, arraystf_8BITROOT)) ;
   for(size_t i = 0; i < nrnodes; ++i) {
      snprintf((char*)nodea[i].key, sizeof(nodea[i].key), "%05zu", i) ;
      nodea[i].node = (arraystf_node_t) arraystf_node_INIT(5, nodea[i].key) ;
      TEST(0 == insert_arraytest(array, &nodea[i], 0, 0)) ;
   }

   // TEST static init
   TEST(0 == iter.stack) ;
   TEST(0 == iter.ri) ;

   // TEST init, double free
   iter.ri = 1 ;
   TEST(0 == init_arraytestiterator(&iter, array)) ;
   TEST(0 != iter.stack) ;
   TEST(0 == iter.ri) ;
   TEST(0 == free_arraytestiterator(&iter)) ;
   TEST(0 == iter.stack) ;
   TEST(0 == iter.ri) ;
   TEST(0 == free_arraytestiterator(&iter)) ;
   TEST(0 == iter.stack) ;
   TEST(0 == iter.ri) ;

   // TEST next_arraysfiterator
   TEST(0 == init_arraytestiterator(&iter, array)) ;
   nextpos = 0 ;
   {
      typeof(iteratedtype_arraytest()) node ;
      while(next_arraytestiterator(&iter, array, &node)) {
         char nextnr[6] ;
         snprintf(nextnr, sizeof(nextnr), "%05zu", nextpos ++) ;
         TEST(0 == strcmp((const char*)node->node.addr, nextnr)) ;
      }
   }
   TEST(iter.ri == nrelemroot_arraystf(array)) ;
   TEST(nextpos == nrnodes) ;
   TEST(0 == free_arraytestiterator(&iter)) ;

   // TEST foreach all
   nextpos = 0 ;
   foreach (_arraystf, array, node) {
      char nextnr[6] ;
      snprintf(nextnr, sizeof(nextnr), "%05zu", nextpos ++) ;
      TEST(0 == strcmp((const char*)((arraystf_node_t*)node)->addr, nextnr)) ;
   }
   TEST(nextpos == nrnodes) ;

   // TEST foreach break after nrnodes/2
   nextpos = 0 ;
   foreach (_arraytest, array, node) {
      char nextnr[6] ;
      snprintf(nextnr, sizeof(nextnr), "%05zu", nextpos ++) ;
      TEST(0 == strcmp((const char*)node->node.addr, nextnr)) ;
      if (nextpos == nrnodes/2) break ;
   }
   TEST(nextpos == nrnodes/2) ;

   // TEST foreach (nrnodes/2 .. nrnodes) after remove
   for(size_t i = 0; i < nrnodes/2; ++i) {
      uint8_t key[10] ;
      sprintf((char*)key, "%05ld", (long)i) ;
      TEST(0 == remove_arraytest(array, 5, key, 0)) ;
   }
   nextpos = nrnodes/2 ;
   foreach (_arraytest, array, node) {
      char nextnr[6] ;
      snprintf(nextnr, sizeof(nextnr), "%05zu", nextpos ++) ;
      TEST(0 == strcmp((const char*)node->node.addr, nextnr)) ;
   }
   TEST(nextpos == nrnodes) ;

   // unprepare
   TEST(0 == delete_arraytest(&array, 0)) ;
   TEST(0 == free_vmblock(&memblock)) ;

   return 0 ;
ABBRUCH:
   (void) delete_arraytest(&array, 0) ;
   free_vmblock(&memblock) ;
   return EINVAL ;
}

static int test_zerokey(void)
{
   arraystf_t     * array  = 0 ;
   vm_block_t     memblock = vm_block_INIT_FREEABLE ;
   const uint16_t nrkeys   = 256 ;
   const uint16_t keylen   = 1024 ;
   testnode_t     * nodes ;
   uint8_t        * keys ;

   // prepare
   TEST(0 == init_vmblock(&memblock, (pagesize_vm()-1 + nrkeys*25u*sizeof(testnode_t)+(size_t)nrkeys*keylen) / pagesize_vm() )) ;
   keys  = memblock.addr ;
   nodes = (testnode_t*) &memblock.addr[nrkeys*keylen] ;

   // TEST with arraystf
   memset(memblock.addr, 0, memblock.size) ;
   for(unsigned i = 0; i < nrkeys; ++i) {
      keys[i*keylen] = (uint8_t) i;
   }
   TEST(0 == new_arraytest(&array, arraystf_6BITROOT)) ;
   TEST(0 != array) ;
   for(unsigned i = 0; i < nrkeys; ++i) {
      for(unsigned l = 1; l <= 25; ++l) {
         nodes[25*i+l-1].node = (arraystf_node_t) arraystf_node_INIT(keylen/l, &keys[i*keylen]) ;
         TEST(0 == insert_arraytest(array, &nodes[25*i+l-1], 0, 0)) ;
      }
   }
   for(unsigned i = 0; i < nrkeys; ++i) {
      for(unsigned l = 1; l <= 25; ++l) {
         testnode_t * node = at_arraytest(array, keylen/l, &keys[i*keylen]) ;
         TEST(&nodes[25*i+l-1] == node) ;
         TEST(node->node.addr == &keys[i*keylen]) ;
         TEST(node->node.size == keylen/l) ;
      }
   }
   for(unsigned i = 0; i < nrkeys; ++i) {
      for(unsigned l = 1; l <= 25; ++l) {
         TEST(0 == remove_arraytest(array, keylen/l, &keys[i*keylen], 0)) ;
      }
   }
   TEST(0 == length_arraytest(array)) ;
   TEST(0 == delete_arraytest(&array, 0)) ;

   // unprepare
   TEST(0 == free_vmblock(&memblock)) ;

   return 0 ;
ABBRUCH:
   (void) delete_arraytest(&array, 0) ;
   free_vmblock(&memblock) ;
   return EINVAL ;
}

arraystf_IMPLEMENT(testnode_t, _arraytest2, node2.addr)

static int test_generic(void)
{
   const size_t      nrnodes    = 8 * sizeof(((testnode_t*)0)->key) ;
   vm_block_t        memblock   = vm_block_INIT_FREEABLE ;
   arraystf_t        * array    = 0 ;
   arraystf_t        * array2   = 0 ;
   testnode_typeadapter_it
                     typeadt_ft = typeadapter_it_INIT(&test_copynode, &test_freenode) ;
   typeadapter_iot   typeadt    = typeadapter_iot_INIT(0, (typeadapter_it*) &typeadt_ft) ;
   testnode_t        * nodea ;
   size_t            nextpos ;


   // prepare
   TEST(0 == init_vmblock(&memblock, (pagesize_vm()-1+nrnodes*sizeof(testnode_t)) / pagesize_vm() )) ;
   nodea = (testnode_t *) memblock.addr ;
   TEST(0 == new_arraytest(&array, arraystf_8BITROOT)) ;
   TEST(0 == new_arraytest2(&array2, arraystf_8BITROOT)) ;

   // TEST insert_arraystf
   for(size_t i = 0; i < nrnodes; ++i) {
      nodea[i] = (testnode_t)  { .node = arraystf_node_INIT(sizeof(nodea[i].key), nodea[i].key), .node2 = arraystf_node_INIT(sizeof(nodea[i].key2), nodea[i].key2) } ;
      nodea[i].key[i / 8] = (uint8_t) (0x80u >> (i%8)) ;
      nodea[i].key2[(nrnodes-1-i) / 8] = (uint8_t) (0x80u >> ((nrnodes-1-i)%8)) ;
      TEST(0 == insert_arraytest(array, &nodea[i], 0, &typeadt)) ;
      TEST(0 == insert_arraytest2(array2, &nodea[i], 0, &typeadt)) ;
      TEST(2 == nodea[i].copycount) ;
   }

   // TEST at_arraystf
   for(size_t i = 0; i < nrnodes; ++i) {
      testnode_t  node = { .node = arraystf_node_INIT(sizeof(node.key), node.key), .node2 = arraystf_node_INIT(sizeof(node.key2), node.key2) } ;
      node.key[i / 8] = (uint8_t) (0x80u >> (i%8)) ;
      node.key2[(nrnodes-1-i) / 8] = (uint8_t) (0x80u >> ((nrnodes-1-i)%8)) ;
      TEST(&nodea[i] == at_arraytest(array, node.node.size, node.node.addr)) ;
      TEST(&nodea[i] == at_arraytest2(array2, node.node2.size, node.node2.addr)) ;
      TEST(2 == nodea[i].copycount) ;
   }

   // TEST foreach all
   nextpos = nrnodes ;
   foreach (_arraytest, array, node) {
      -- nextpos ;
      TEST(node == &nodea[nextpos]) ;
   }
   TEST(nextpos == 0) ;
   nextpos = 0 ;
   foreach (_arraytest2, array2, node) {
      TEST(node == &nodea[nextpos]) ;
      ++ nextpos ;
   }
   TEST(nextpos == nrnodes) ;

   // TEST remove_arraystf
   for(size_t i = 0; i < nrnodes; ++i) {
      testnode_t  * removed_node = 0 ;
      testnode_t  node = { .node = arraystf_node_INIT(sizeof(node.key), node.key), .node2 = arraystf_node_INIT(sizeof(node.key2), node.key2) } ;
      node.key[i / 8] = (uint8_t) (0x80u >> (i%8)) ;
      node.key2[(nrnodes-1-i) / 8] = (uint8_t) (0x80u >> ((nrnodes-1-i)%8)) ;
      TEST(0 == remove_arraytest(array, node.node.size, node.node.addr, &removed_node)) ;
      TEST(&nodea[i] == removed_node) ;
      removed_node = 0 ;
      TEST(0 == remove_arraytest2(array2, node.node2.size, node.node2.addr, &removed_node)) ;
      TEST(&nodea[i] == removed_node) ;
      TEST(nrnodes-1-i == length_arraytest(array)) ;
      TEST(nrnodes-1-i == length_arraytest2(array2)) ;
   }

   // TEST delete_arraystf
   for(size_t i = nrnodes; (i--); ) {
      nodea[i] = (testnode_t) { .node = arraystf_node_INIT(sizeof(nodea[i].key), nodea[i].key), .node2 = arraystf_node_INIT(sizeof(nodea[i].key2), nodea[i].key2) } ;
      nodea[i].key[i / 8] = (uint8_t) (0x80u >> (i%8)) ;
      nodea[i].key2[(nrnodes-1-i) / 8] = (uint8_t) (0x80u >> ((nrnodes-1-i)%8)) ;
      TEST(0 == insert_arraytest(array, &nodea[i], 0, &typeadt)) ;
      TEST(0 == insert_arraytest2(array2, &nodea[i], 0, &typeadt)) ;
      TEST(2 == nodea[i].copycount) ;
      TEST(&nodea[i] == at_arraytest(array, nodea[i].node.size, nodea[i].node.addr)) ;
      TEST(&nodea[i] == at_arraytest2(array2, nodea[i].node2.size, nodea[i].node2.addr)) ;
      TEST(2 == nodea[i].copycount) ;
   }
   TEST(0 == delete_arraystf(&array, &typeadt)) ;
   TEST(0 == delete_arraystf(&array2, &typeadt)) ;
   for(size_t i = 0; i < nrnodes; ++i) {
      TEST(2 == nodea[i].freecount) ;
   }

   // unprepare
   TEST(0 == free_vmblock(&memblock)) ;

   return 0 ;
ABBRUCH:
   TEST(0 == delete_arraystf(&array, 0)) ;
   TEST(0 == delete_arraystf(&array2, 0)) ;
   free_vmblock(&memblock) ;
   return EINVAL ;
}

int unittest_ds_inmem_arraystf()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   for(int i = 0; i < 2; ++i) {
   TEST(0 == free_resourceusage(&usage)) ;
   TEST(0 == init_resourceusage(&usage)) ;

   if (test_arraystfnode())   goto ABBRUCH ;
   if (test_arraystfkeyval()) goto ABBRUCH ;
   if (test_initfree())       goto ABBRUCH ;
   if (test_error())          goto ABBRUCH ;
   if (test_iterator())       goto ABBRUCH ;
   if (test_zerokey())        goto ABBRUCH ;
   if (test_generic())        goto ABBRUCH ;

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
