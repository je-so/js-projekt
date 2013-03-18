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
#include "C-kern/api/err.h"
#include "C-kern/api/ds/foreach.h"
#include "C-kern/api/ds/typeadapt.h"
#include "C-kern/api/ds/inmem/arraystf.h"
#include "C-kern/api/ds/inmem/binarystack.h"
#include "C-kern/api/math/int/log2.h"
#include "C-kern/api/math/int/power2.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#include "C-kern/api/string/string.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/test/testmm.h"
#include "C-kern/api/test/errortimer.h"
#include "C-kern/api/memory/vm.h"
#endif


// section: arraystf_node_t

// group: helper

/* function: compiletime_assert
 * Checks that <arraystf_node_t> can be castet into <string_t>. */
static inline void compiletime_assert(void)
{
   string_t          str ;
   arraystf_node_t   node ;

   // TEST string_t has only members: addr & size
   static_assert(sizeof(node) == sizeof(str), "considered same number of members in string_t") ;

   // TEST arraystf_node_t.addr & arraystf_node_t.size compatible with string_t.addr & string_t.size
   static_assert(offsetof(arraystf_node_t, addr) == offsetof(string_t, addr), "arraystf_node_t compatible with string_t") ;
   static_assert(sizeof(node.addr) == sizeof(str.addr), "arraystf_node_t compatible with string_t") ;
   static_assert(offsetof(arraystf_node_t, size) == offsetof(string_t, size), "arraystf_node_t compatible with string_t") ;
   static_assert(sizeof(node.size) == sizeof(str.size), "arraystf_node_t compatible with string_t") ;
}

typedef struct arraystf_keyval_t       arraystf_keyval_t ;

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

   for (offset = 0; offset < size; ++offset ) {
      if (addr1[offset] == addr2[offset]) continue ;
      keyval->data = (size_t) (addr1[offset] ^ addr2[offset]) ;
      goto DONE ;
   }

   for (; offset < size1; ++offset) {
      if (0 == addr1[offset]) continue ;
      keyval->data = addr1[offset] ;
      goto DONE ;
   }

   for (; offset < size2; ++offset) {
      if (0 == addr2[offset]) continue ;
      keyval->data = addr2[offset] ;
      goto DONE ;
   }

   VALIDATE_INPARAM_TEST(size1 != size2, ONABORT, ) ;

   offset       = SIZE_MAX ;
   keyval->data = (size1 ^ size2) ;

DONE:
   keyval->offset = offset ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}


// section: arraystf_t

// group: helper

#define toplevelsize_arraystf(array)         ((array)->toplevelsize)

#define objectsize_arraystf(toplevelsize)    (sizeof(arraystf_t) + sizeof(arraystf_node_t*) * (toplevelsize))

typedef struct arraystf_findresult_t         arraystf_findresult_t ;

struct arraystf_findresult_t {
   unsigned                rootindex ;
   unsigned                childindex ;
   unsigned                pchildindex ;
   arraystf_mwaybranch_t   * parent ;
   arraystf_mwaybranch_t   * pparent ;
   arraystf_unode_t        * found_node ;
   arraystf_node_t         * found_key ;
} ;

static int find_arraystf(const arraystf_t * array, arraystf_node_t * keynode, /*err;out*/arraystf_findresult_t * result)
{
   int err ;
   arraystf_keyval_t keyval ;

   VALIDATE_INPARAM_TEST(keynode->size < SIZE_MAX, ONABORT, ) ;

   uint32_t rootindex = 0 ;

   keyval.offset = 0 ;
   keyval.data   = 0 ;

   if (keynode->size > 2) {
      keyval.data = keynode->addr[0] ;
      rootindex = keynode->addr[0] ;
      rootindex <<= 8 ;
      rootindex += keynode->addr[1] ;
      rootindex <<= 8 ;
      rootindex += keynode->addr[2] ;
   } else if (keynode->size > 1) {
      keyval.data = keynode->addr[0] ;
      rootindex = keynode->addr[0] ;
      rootindex <<= 8 ;
      rootindex += keynode->addr[1] ;
      rootindex <<= 8 ;
   } else if (keynode->size > 0) {
      keyval.data = keynode->addr[0] ;
      rootindex = keynode->addr[0] ;
      rootindex <<= 16 ;
   }

   rootindex = (rootindex >> array->rootidxshift) ;

   arraystf_unode_t      * node      = array->root[rootindex] ;
   arraystf_mwaybranch_t * pparent   = 0 ;
   arraystf_mwaybranch_t * parent    = 0 ;
   unsigned              childindex  = 0 ;
   unsigned              pchildindex = 0 ;

   err = ESRCH ;

   while (node) {

      if (isbranchtype_arraystfunode(node)) {
         pparent = parent ;
         parent  = branch_arraystfunode(node) ;
         if (parent->offset > keyval.offset) {
            init_arraystfkeyval(&keyval, parent->offset, keynode) ;
         }
         pchildindex = childindex ;
         childindex  = childindex_arraystfmwaybranch(parent, keyval.data) ;
         node        = parent->child[childindex] ;
      } else {
         result->found_key = node_arraystfunode(node) ;
         if (  keynode->size == result->found_key->size
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
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

// group: lifetime

int new_arraystf(/*out*/arraystf_t ** array, uint32_t toplevelsize)
{
   int err ;
   memblock_t  new_obj = memblock_INIT_FREEABLE ;

   toplevelsize += (0 == toplevelsize) ;
   toplevelsize = makepowerof2_int(toplevelsize) ;

   VALIDATE_INPARAM_TEST(toplevelsize <= 0x00800000, ONABORT, PRINTUINT32_LOG(toplevelsize)) ;

   const size_t objsize = objectsize_arraystf(toplevelsize) ;

   err = RESIZE_MM(objsize, &new_obj) ;
   if (err) goto ONABORT ;

   memset(new_obj.addr, 0, objsize) ;
   ((arraystf_t*)(new_obj.addr))->toplevelsize = 0xffffff & toplevelsize ;
   ((arraystf_t*)(new_obj.addr))->rootidxshift = (uint8_t) (24 - log2_int(toplevelsize)) ;

   *array = ((arraystf_t*)(new_obj.addr)) ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int delete_arraystf(arraystf_t ** array, typeadapt_member_t * nodeadp)
{
   int err ;
   int err2 ;
   arraystf_t * del_obj = *array ;

   if (del_obj) {
      *array = 0 ;

      err = 0 ;

      const bool isDelete = (nodeadp && iscalldelete_typeadapt(nodeadp->typeadp)) ;

      for (unsigned i = 0; i < toplevelsize_arraystf(del_obj); ++i) {

         if (!del_obj->root[i]) continue ;

         arraystf_unode_t * node = del_obj->root[i] ;

         if (!isbranchtype_arraystfunode(node)) {
            if (isDelete) {
               typeadapt_object_t * delobj = memberasobject_typeadaptmember(nodeadp, node) ;
               err2 = calldelete_typeadaptmember(nodeadp, &delobj) ;
               if (err2) err = err2 ;
            }
            continue ;
         }

         arraystf_mwaybranch_t * branch = branch_arraystfunode(node) ;
         node = branch->child[0] ;
         branch->child[0] = 0 ;
         branch->used = lengthof(branch->child)-1 ;

         for (;;) {

            for (;;) {
               if (node) {
                  if (isbranchtype_arraystfunode(node)) {
                     arraystf_mwaybranch_t * parent = branch ;
                     branch = branch_arraystfunode(node) ;
                     node = branch->child[0] ;
                     branch->child[0] = (arraystf_unode_t*) parent ;
                     branch->used     = lengthof(branch->child)-1 ;
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
               arraystf_mwaybranch_t   * parent = (arraystf_mwaybranch_t *) branch->child[0] ;
               memblock_t              mblock   = memblock_INIT(sizeof(arraystf_mwaybranch_t), (uint8_t*)branch) ;
               err2 = FREE_MM(&mblock) ;
               if (err2) err = err2 ;
               branch = parent ;
            } while (branch && !branch->used) ;

            if (!branch) break ;

            node = branch->child[branch->used --] ;
         }

      }

      const size_t objsize = objectsize_arraystf(toplevelsize_arraystf(del_obj)) ;

      memblock_t mblock = memblock_INIT(objsize, (uint8_t*)del_obj) ;
      err2 = FREE_MM(&mblock) ;
      if (err2) err = err2 ;

      if (err) goto ONABORT ;
   }


   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

// group: query

struct arraystf_node_t * at_arraystf(const arraystf_t * array, size_t size, const uint8_t keydata[size])
{
   int err ;
   arraystf_findresult_t found ;
   arraystf_node_t       key   = arraystf_node_INIT(size, &keydata[0]) ;

   err = find_arraystf(array, &key, &found) ;
   if (err) return 0 ;

   return node_arraystfunode(found.found_node) ;
}

// group: change

int tryinsert_arraystf(arraystf_t * array, struct arraystf_node_t * node, /*out;err*/struct arraystf_node_t ** inserted_or_existing_node, struct typeadapt_member_t * nodeadp/*0=>no copy is made*/)
{
   int err ;
   arraystf_findresult_t   found ;
   arraystf_keyval_t       keydiff ;
   typeadapt_object_t      * copied_node ;
   unsigned                shift ;

   err = find_arraystf(array, node, &found) ;
   if (ESRCH != err) {
      if (inserted_or_existing_node) {
         *inserted_or_existing_node = (0 == err) ? node_arraystfunode(found.found_node) : 0 ;
      }
      return (0 == err) ? EEXIST : err ;
   }

   if (nodeadp) {
      err = callnewcopy_typeadaptmember(nodeadp, &copied_node, memberasobject_typeadaptmember(nodeadp, node)) ;
      if (err) goto ONABORT ;
      node = objectasmember_typeadaptmember(nodeadp, copied_node) ;
   }

   VALIDATE_INPARAM_TEST(0 == ((uintptr_t)node&0x01), ONABORT, ) ;

   if (found.found_node) {

      err = initdiff_arraystfkeyval(&keydiff, found.found_key, node) ;
      if (err) goto ONABORT ;

      shift = log2_int(keydiff.data) & ~0x01u ;

      if (  ! found.parent
         || found.parent->offset < keydiff.offset
         || (     found.parent->offset == keydiff.offset
               && found.parent->shift  >  shift))  {

   // prefix matches (add new branch layer after found.parent)

         memblock_t mblock = memblock_INIT_FREEABLE ;
         err = RESIZE_MM(sizeof(arraystf_mwaybranch_t), &mblock) ;
         if (err) goto ONABORT ;

         arraystf_mwaybranch_t * new_branch = (arraystf_mwaybranch_t *) mblock.addr ;

         arraystf_keyval_t keynode ;
         init_arraystfkeyval(&keynode, keydiff.offset, node) ;
         keydiff.data ^= keynode.data ;

         init_arraystfmwaybranch(new_branch, keydiff.offset, shift, keydiff.data, found.found_node, keynode.data, nodecast_arraystfunode(node)) ;

         if (found.parent) {
            found.parent->child[found.childindex] = branchcast_arraystfunode(new_branch) ;
         } else {
            array->root[found.rootindex] = branchcast_arraystfunode(new_branch) ;
         }
         goto DONE ;
      }

   // fall through to not so simple case

   } else /*(! found.found_node)*/ {

   // simple case (parent is 0)

      if (! found.parent) {
         array->root[found.rootindex] = nodecast_arraystfunode(node) ;
         goto DONE ;
      }

   // get pos of already stored node / check prefix match => second simple case

      arraystf_mwaybranch_t * branch = found.parent ;
      for (unsigned i = lengthof(branch->child)-1; ;) {
         if (branch->child[i]) {
            if (isbranchtype_arraystfunode(branch->child[i])) {
               branch = branch_arraystfunode(branch->child[i]) ;
               i = lengthof(branch->child)-1;
               continue ;
            }

            err = initdiff_arraystfkeyval(&keydiff, node_arraystfunode(branch->child[i]), node) ;
            if (err) goto ONABORT ;
            break ;
         }
         if (!(i--)) {
            err = EINVAL ;
            goto ONABORT ;
         }
      }

      shift = log2_int(keydiff.data) & ~0x01u ;

      if (     found.parent->offset == keydiff.offset
            && found.parent->shift  == shift)  {
         // prefix does match
         found.parent->child[found.childindex] = nodecast_arraystfunode(node) ;
         ++ found.parent->used ;
         goto DONE ;
      }

   }

   // not so simple case prefix differs (add new branch layer between root & found.parent)

   assert(found.parent) ;
   arraystf_mwaybranch_t   * parent = 0 ;
   arraystf_mwaybranch_t   * branch = branch_arraystfunode(array->root[found.rootindex]) ;
   arraystf_keyval_t       keynode ;

   unsigned childindex = 0 ;

   while (     branch->offset < keydiff.offset
            || (     branch->offset == keydiff.offset
                  && branch->shift  > shift)) {
      parent = branch ;
      init_arraystfkeyval(&keynode, branch->offset, node) ;
      childindex = childindex_arraystfmwaybranch(branch, keynode.data) ;
      arraystf_unode_t * child = branch->child[childindex] ;
      assert(child) ;
      assert(isbranchtype_arraystfunode(child)) ;
      branch = branch_arraystfunode(child) ;
   }

   memblock_t mblock = memblock_INIT_FREEABLE ;
   err = RESIZE_MM(sizeof(arraystf_mwaybranch_t), &mblock) ;
   if (err) goto ONABORT ;

   arraystf_mwaybranch_t * new_branch = (arraystf_mwaybranch_t*) mblock.addr ;

   init_arraystfkeyval(&keynode, keydiff.offset, node) ;
   keydiff.data ^= keynode.data ;

   init_arraystfmwaybranch(new_branch, keydiff.offset, shift, keydiff.data, branchcast_arraystfunode(branch), keynode.data, nodecast_arraystfunode(node)) ;

   if (parent) {
      parent->child[childindex] = branchcast_arraystfunode(new_branch) ;
   } else {
      array->root[found.rootindex] = branchcast_arraystfunode(new_branch) ;
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

int tryremove_arraystf(arraystf_t * array, size_t size, const uint8_t keydata[size], /*out*/struct arraystf_node_t ** removed_node)
{
   int err ;
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

         for (unsigned i = lengthof(found.parent->child)-1; ; ) {
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
               goto ONABORT ;
            }
         }

         memblock_t mblock = memblock_INIT(sizeof(arraystf_mwaybranch_t), (uint8_t*)found.parent) ;
         err = FREE_MM(&mblock) ;
         (void) err /*IGNORE*/ ;
      }

   }

   assert(array->length > 0) ;
   -- array->length ;

   *removed_node = node_arraystfunode(found.found_node) ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int remove_arraystf(arraystf_t * array, size_t size, const uint8_t keydata[size], /*out*/struct arraystf_node_t ** removed_node)
{
   int err ;

   err = tryremove_arraystf(array, size, keydata, removed_node) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int insert_arraystf(arraystf_t * array, struct arraystf_node_t * node, /*out*/struct arraystf_node_t ** inserted_node/*0=>copy not returned*/, struct typeadapt_member_t * nodeadp/*0=>no copy is made*/)
{
   int err ;
   struct arraystf_node_t * inserted_or_existing_node ;

   err = tryinsert_arraystf(array, node, &inserted_or_existing_node, nodeadp) ;
   if (err) goto ONABORT ;

   if (inserted_node) {
      *inserted_node = inserted_or_existing_node ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}


// section: arraystf_iterator_t

typedef struct arraystf_pos_t          arraystf_pos_t ;

struct arraystf_pos_t {
   arraystf_mwaybranch_t * branch ;
   unsigned              ci ;
} ;

int initfirst_arraystfiterator(/*out*/arraystf_iterator_t * iter, arraystf_t * array)
{
   int err ;
   size_t        objectsize = sizeof(binarystack_t) ;
   memblock_t    objectmem  = memblock_INIT_FREEABLE ;
   binarystack_t * stack  = 0 ;

   (void) array ;

   err = RESIZE_MM(objectsize, &objectmem) ;
   if (err) goto ONABORT ;

   stack  = (binarystack_t *) objectmem.addr ;

   err = init_binarystack(stack, (/*guessed depth*/16) * /*objectsize*/sizeof(arraystf_pos_t) ) ;
   if (err) goto ONABORT ;

   iter->stack = stack ;
   iter->ri    = 0 ;

   return 0 ;
ONABORT:
   FREE_MM(&objectmem) ;
   TRACEABORT_LOG(err) ;
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

      err2 = FREE_MM(&objectmem) ;
      if (err2) err = err2 ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

bool next_arraystfiterator(arraystf_iterator_t * iter, arraystf_t * array, /*out*/struct arraystf_node_t ** node)
{
   int err ;
   size_t         nrelemroot = toplevelsize_arraystf(array) ;
   arraystf_pos_t * pos ;

   for (;;) {

      if (isempty_binarystack(iter->stack)) {

         arraystf_unode_t * rootnode ;

         for (;;) {
            if (iter->ri >= nrelemroot) {
               return false ;
            }
            rootnode = array->root[iter->ri ++] ;
            if (rootnode) {
               if (!isbranchtype_arraystfunode(rootnode)) {
                  *node = node_arraystfunode(rootnode) ;
                  return true ;
               }
               break ;
            }
         }

         err = push_binarystack(iter->stack, &pos) ;
         if (err) goto ONABORT ;

         pos->branch = branch_arraystfunode(rootnode) ;
         pos->ci     = 0 ;

      } else {
         pos = top_binarystack(iter->stack) ;
      }

      for (;;) {
         arraystf_unode_t * childnode = pos->branch->child[pos->ci ++] ;

         if (pos->ci >= lengthof(pos->branch->child)) {
            // pos becomes invalid
            err = pop_binarystack(iter->stack, sizeof(arraystf_pos_t)) ;
            if (err) goto ONABORT ;

            if (!childnode) break ;
         }

         if (childnode) {
            if (isbranchtype_arraystfunode(childnode)) {
               err = push_binarystack(iter->stack, &pos) ;
               if (err) goto ONABORT ;
               pos->branch = branch_arraystfunode(childnode) ;
               pos->ci     = 0 ;
               continue ;
            } else {
               *node = node_arraystfunode(childnode) ;
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

static int test_arraystfnode(void)
{
   arraystf_node_t         node = arraystf_node_INIT(0,0) ;
   arraystf_mwaybranch_t   branch ;
   arraystf_unode_t        * unode ;

   // TEST arraystf_node_INIT(0,0)
   TEST(0 == node.addr) ;
   TEST(0 == node.size) ;

   // TEST arraystf_node_INIT
   for (unsigned i = 0; i < 1000; i += 100) {
      node = (arraystf_node_t) arraystf_node_INIT(i+1,(const uint8_t*)i) ;
      TEST(i   == (uintptr_t)node.addr) ;
      TEST(i+1 == node.size) ;
   }

   // TEST stringcast_arraystfnode
   for (unsigned i = 0; i < 1000; i += 100) {
      TEST(stringcast_arraystfnode((string_t*)i) == (arraystf_node_t*)i) ;
   }

   // TEST childindex_arraystfmwaybranch
   for (uint8_t i = 0; i < bitsof(size_t)-1; ++i) {
      branch.shift = i ;
      TEST(0 == childindex_arraystfmwaybranch(&branch, (size_t)0)) ;
      TEST(1 == childindex_arraystfmwaybranch(&branch, (size_t)1 << i)) ;
      TEST(2 == childindex_arraystfmwaybranch(&branch, (size_t)2 << i)) ;
      TEST(3 == childindex_arraystfmwaybranch(&branch, (size_t)3 << i)) ;
      TEST(3 == childindex_arraystfmwaybranch(&branch, SIZE_MAX)) ;
   }

   // TEST init_arraystfmwaybranch
   init_arraystfmwaybranch(&branch, 5, 3, (size_t)1 << 3, (arraystf_unode_t*)1, (size_t)3 << 3, (arraystf_unode_t*)2) ;
   TEST(branch.child[0] == 0) ;
   TEST(branch.child[1] == (arraystf_unode_t*)1) ;
   TEST(branch.child[2] == 0) ;
   TEST(branch.child[3] == (arraystf_unode_t*)2) ;
   TEST(branch.offset   == 5) ;
   TEST(branch.shift    == 3) ;
   TEST(branch.used     == 2) ;

   // TEST setchild_arraystfmwaybranch
   for (unsigned i = 0; i < lengthof(branch.child); ++i) {
      unode = (arraystf_unode_t*) 1 ;
      setchild_arraystfmwaybranch(&branch, i, unode) ;
      TEST(unode == branch.child[i]) ;
      unode = (arraystf_unode_t*) 0 ;
      setchild_arraystfmwaybranch(&branch, i, unode) ;
      TEST(unode == branch.child[i]) ;
   }

   // TEST nodecast_arraystfunode, node_arraystfunode
   unode = nodecast_arraystfunode(&node) ;
   TEST(&node == &unode->node) ;
   TEST(&node == node_arraystfunode(unode)) ;

   // TEST branchcast_arraystfunode, branch_arraystfunode
   unode = branchcast_arraystfunode(&branch) ;
   TEST(&branch == (arraystf_mwaybranch_t*)(0x01 ^ (uintptr_t)&unode->branch)) ;
   TEST(&branch == branch_arraystfunode(unode)) ;

   // TEST isbranchtype_arraystfunode
   unode = nodecast_arraystfunode(&node) ;
   TEST(0 == isbranchtype_arraystfunode(unode)) ;
   unode = branchcast_arraystfunode(&branch) ;
   TEST(1 == isbranchtype_arraystfunode(unode)) ;

   return 0 ;
ONABORT:
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
   for (unsigned i = 0; i < 4; ++i) {
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
   for (unsigned i = 0; i < 17; ++i) {
      keyval.data   = 0 ;
      keyval.offset = i+1 ;
      init_arraystfkeyval(&keyval, i, &node1) ;
      TEST(key1[i] == keyval.data) ;
      TEST(i       == keyval.offset) ;
   }

   // TEST init_arraystfkeyval (offset higher key size)
   key1 = (const uint8_t*) "0123456789ABCDEF" ;
   node1 = (arraystf_node_t) arraystf_node_INIT(17, key1) ;
   for (unsigned i = 17; i < SIZE_MAX; i <<= 1, i++) {
      keyval.data   = 0 ;
      keyval.offset = i+1 ;
      init_arraystfkeyval(&keyval, i, &node1) ;
      TEST(0 == keyval.data /*always 0*/) ;
      TEST(i == keyval.offset) ;
   }

   // TEST init_arraystfkeyval (special value -1)
   key1 = (const uint8_t*) "0123456789ABCDEF" ;
   for (unsigned i = 1; i <= 17; i++) {
      node1 = (arraystf_node_t) arraystf_node_INIT(i, key1) ;
      keyval.data   = 0 ;
      keyval.offset = 0 ;
      init_arraystfkeyval(&keyval, SIZE_MAX, &node1) ;
      TEST(i        == keyval.data/*always key size*/) ;
      TEST(SIZE_MAX == keyval.offset) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

typedef struct testnode_t              testnode_t ;
typedef struct testnode_adapt_t        testnode_adapt_t ;

struct testnode_t {
   arraystf_node_t node ;
   uint8_t         copycount ;
   uint8_t         freecount ;
   uint8_t         key[40] ;
   struct {
      arraystf_node_EMBED(addr, size) ;
   }               node2 ;
   uint8_t         key2[40] ;
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
   const size_t      nrnodes    = 100000 ;
   vm_block_t        memblock   = vm_block_INIT_FREEABLE ;
   arraystf_t        * array    = 0 ;
   testnode_adapt_t  typeadapt = { typeadapt_INIT_LIFETIME(&copynode_testnodeadapt, &freenode_testnodeadapt), test_errortimer_INIT_FREEABLE } ;
   typeadapt_member_t nodeadp  = typeadapt_member_INIT(genericcast_typeadapt(&typeadapt,testnode_adapt_t,testnode_t,void*), offsetof(testnode_t, node)) ;
   testnode_t        * nodes ;
   arraystf_node_t   * inserted_node ;
   arraystf_node_t   * removed_node ;

   // prepare
   TEST(0 == init_vmblock(&memblock, (pagesize_vm()-1+nrnodes*sizeof(testnode_t)) / pagesize_vm() )) ;
   nodes = (testnode_t *) memblock.addr ;

   // TEST arraystf_node_EMBED
   static_assert(sizeof(nodes->node) == sizeof(nodes->node2), "arraystf_node_EMBED creates same structure")
   static_assert(offsetof(typeof(nodes->node),addr) == offsetof(typeof(nodes->node2),addr), "arraystf_node_EMBED creates same structure")
   static_assert(offsetof(typeof(nodes->node),size) == offsetof(typeof(nodes->node2),size), "arraystf_node_EMBED creates same structure")

   // TEST new_arraystf, delete_arraystf
   for (unsigned topsize = 0, expectsize=1, expectshift=24; topsize <= 512; ++topsize) {
      if (topsize > expectsize) {
         expectsize <<= 1 ;
         expectshift -= 1 ;
      }
      TEST(0 == new_arraystf(&array, topsize)) ;
      TEST(0 != array) ;
      TEST(0 == length_arraystf(array)) ;
      TEST(expectshift == array->rootidxshift) ;
      TEST(expectsize  == toplevelsize_arraystf(array)) ;
      for (unsigned i = 0; i < toplevelsize_arraystf(array); ++i) {
         TEST(0 == array->root[i]) ;
      }
      TEST(0 == delete_arraystf(&array, 0)) ;
      TEST(0 == array) ;
      TEST(0 == delete_arraystf(&array, 0)) ;
      TEST(0 == array) ;
   }

   // TEST root distributions
   for (uint32_t topsize = 1, rshift = 24; topsize < 65536*4; topsize *= 2, rshift -= 1) {
      TEST(0 == new_arraystf(&array, topsize)) ;
      uint8_t keyvalue[256] = { 0 } ;
      for (size_t byte = 0; byte < 256; ++byte) {
         testnode_t  node  = { .node = arraystf_node_INIT(byte, keyvalue) } ;
         keyvalue[0] = (uint8_t) byte ;
         keyvalue[1] = (uint8_t) byte ;
         keyvalue[2] = (uint8_t) byte ;
         size_t ri = byte * (size_t)65536 + byte * 256 + byte ;
         if (byte == 0) ri = 0 ;
         else if (byte == 1) ri = byte * (size_t)65536 ;
         else if (byte == 2) ri = byte * (size_t)65536 + byte * (size_t)256 ;
         else                ri = byte * (size_t)65536 + byte * (size_t)256 + byte ;
         ri >>= rshift ;
         TEST(ri < toplevelsize_arraystf(array)) ;
         TEST(0 == tryinsert_arraystf(array, &node.node, &inserted_node, 0)) ;
         TEST(0 != array->root[ri]) ;
         TEST(1 == length_arraystf(array)) ;
         TEST(inserted_node == &node.node) ;
         TEST(&node.node == node_arraystfunode(array->root[ri])) ;
         for (unsigned i = 0; i < toplevelsize_arraystf(array); ++i) {
            if (i == ri) continue ;
            TEST(0 == array->root[i]) ;
         }
         TEST(0 == tryremove_arraystf(array, node.node.size, node.node.addr, &removed_node)) ;
         TEST(0 == length_arraystf(array)) ;
         TEST(0 == array->root[ri]) ;
         TEST(removed_node == &node.node) ;
      }
      TEST(0 == delete_arraystf(&array, 0)) ;
      TEST(0 == array) ;
   }

   // TEST insert_arraystf (1 level)
   TEST(0 == new_arraystf(&array, 64)) ;
   nodes[4] = (testnode_t) { .node = arraystf_node_INIT(1, nodes[4].key), .key = { 4 } } ;
   TEST(0 == tryinsert_arraystf(array, &nodes[4].node, &inserted_node, &nodeadp))
   TEST(&nodes[4].node == node_arraystfunode(array->root[1])) ;
   for (uint8_t pos = 5; pos <= 7; ++pos) {
      inserted_node = 0 ;
      nodes[pos] = (testnode_t) { .node = arraystf_node_INIT(1, nodes[pos].key), .key = { pos } } ;
      TEST(0 == tryinsert_arraystf(array, &nodes[pos].node, &inserted_node, &nodeadp)) ;
      TEST(inserted_node == &nodes[pos].node) ;
      TEST(0 == nodes[pos].freecount) ;
      TEST(1 == nodes[pos].copycount) ;
      TEST(pos-3u == length_arraystf(array)) ;
      TEST(isbranchtype_arraystfunode(array->root[1])) ;
      TEST(0 == branch_arraystfunode(array->root[1])->shift) ;
      TEST(pos-3u == branch_arraystfunode(array->root[1])->used) ;
   }
   for (uint8_t pos = 4; pos <= 7; ++pos) {
      TEST(&nodes[pos].node == node_arraystfunode(branch_arraystfunode(array->root[1])->child[pos-4])) ;
      TEST(&nodes[pos].node == at_arraystf(array, 1, &pos)) ;
   }
   TEST(0 == at_arraystf(array, 0, 0)) ;
   TEST(0 == at_arraystf(array, 5, (const uint8_t*)"00000")) ;

   // TEST remove_arraystf (1 level)
   for (uint8_t pos = 4; pos <= 7; ++pos) {
      removed_node = (void*)1 ;
      TEST(0 == tryremove_arraystf(array, 1, &pos, &removed_node))
      TEST(&nodes[pos].node == removed_node) ;
      TEST(1 == nodes[pos].copycount) ;
      TEST(0 == nodes[pos].freecount) ;
      TEST(0 == at_arraystf(array, 1, &pos)) ;
      if (pos < 6) {
         TEST(array->root[1]) ;
         TEST(isbranchtype_arraystfunode(array->root[1])) ;
      } else if (pos == 6) {
         TEST(&nodes[7].node == node_arraystfunode(array->root[1])) ;
      } else {
         TEST(! array->root[1]) ;
      }
      TEST(7u-pos == length_arraystf(array)) ;
   }

   // TEST insert_arraystf (2 levels)
   TEST(0 == delete_arraystf(&array, 0)) ;
   TEST(0 == new_arraystf(&array, 1)) ;
   arraystf_mwaybranch_t * branch1 = 0 ;
   for (uint8_t pos = 16; pos <= 31; ++pos) {
      nodes[pos] = (testnode_t) { .node = arraystf_node_INIT(1, nodes[pos].key), .key = { pos } } ;
      TEST(0 == tryinsert_arraystf(array, &nodes[pos].node, &inserted_node, &nodeadp))
      TEST(1 == nodes[pos].copycount) ;
      TEST(0 == nodes[pos].freecount) ;
      TEST(pos-15u == length_arraystf(array)) ;
      if (pos == 16) {
         TEST(&nodes[16].node == node_arraystfunode( array->root[0])) ;
      } else if (pos == 17) {
         TEST(isbranchtype_arraystfunode( array->root[0])) ;
         branch1 = branch_arraystfunode( array->root[0]) ;
         TEST(0 == branch1->shift) ;
         TEST(&nodes[16].node  == node_arraystfunode(branch1->child[0])) ;
         TEST(&nodes[17].node  == node_arraystfunode(branch1->child[1])) ;
      } else if (pos <= 19) {
         TEST(isbranchtype_arraystfunode( array->root[0])) ;
         TEST(branch1 == branch_arraystfunode( array->root[0])) ;
         TEST(&nodes[pos].node == node_arraystfunode(branch1->child[pos-16])) ;
      } else if (pos == 20 || pos == 24 || pos == 28) {
         TEST(isbranchtype_arraystfunode( array->root[0])) ;
         if (pos == 20) {
            arraystf_mwaybranch_t * branch2 = branch_arraystfunode( array->root[0]) ;
            TEST(2 == branch2->shift) ;
            TEST(branch1 == branch_arraystfunode(branch2->child[0])) ;
            branch1 = branch2 ;
         }
         TEST(&nodes[pos].node == node_arraystfunode(branch1->child[(pos-16)/4])) ;
      } else {
         TEST(isbranchtype_arraystfunode( array->root[0])) ;
         TEST(branch1 == branch_arraystfunode( array->root[0])) ;
         TEST(isbranchtype_arraystfunode(branch1->child[(pos-16)/4])) ;
         arraystf_mwaybranch_t * branch2 = branch_arraystfunode(branch1->child[(pos-16)/4]) ;
         TEST(&nodes[pos&~0x03u].node == node_arraystfunode(branch2->child[0])) ;
         TEST(&nodes[pos].node        == node_arraystfunode(branch2->child[pos&0x03u])) ;
      }
   }

   // TEST remove_arraystf (2 levels)
   for (uint8_t pos = 16; pos <= 31; ++pos) {
      removed_node = 0 ;
      TEST(0 == tryremove_arraystf(array, 1, &pos, &removed_node)) ;
      TEST(&nodes[pos].node == removed_node) ;
      TEST(1 == nodes[pos].copycount) ;
      TEST(0 == nodes[pos].freecount) ;
      TEST(0 == at_arraystf(array, 1, &pos)) ;
      TEST(31u-pos == length_arraystf(array)) ;
      if (pos <= 17) {
         TEST(1 == isbranchtype_arraystfunode(branch_arraystfunode( array->root[0])->child[0])) ;
      } else if (pos == 18) {
         TEST(&nodes[19].node == node_arraystfunode(branch_arraystfunode( array->root[0])->child[0])) ;
      } else if (pos == 19) {
         TEST(0 == branch_arraystfunode( array->root[0])->child[0]) ;
      } else if (pos < 22) {
         TEST(1 == isbranchtype_arraystfunode(branch_arraystfunode( array->root[0])->child[1])) ;
      } else if (pos == 22) {
         TEST(1 == isbranchtype_arraystfunode( array->root[0])) ;
         TEST(2 == branch_arraystfunode( array->root[0])->shift) ;
         TEST(&nodes[23].node == node_arraystfunode(branch_arraystfunode( array->root[0])->child[1])) ;
      } else if (pos <= 26) {
         TEST(1 == isbranchtype_arraystfunode( array->root[0])) ;
         TEST(2 == branch_arraystfunode( array->root[0])->shift) ;
      } else if (pos <= 29) {
         TEST(1 == isbranchtype_arraystfunode( array->root[0])) ;
         TEST(0 == branch_arraystfunode( array->root[0])->shift) ;
      } else if (pos == 30) {
         TEST(&nodes[31].node == node_arraystfunode( array->root[0])) ;
      } else if (pos == 31) {
         TEST(0 ==  array->root[0]) ;
      }
   }

   // TEST insert_arraystf, at_arraystf, remove_arraystf: ascending
   for (uint32_t topsize = 2048; topsize <= 4096; topsize *= 2) {
      TEST(0 == delete_arraystf(&array, 0)) ;
      TEST(0 == new_arraystf(&array, topsize)) ;
      for (size_t pos = 0; pos < nrnodes; ++pos) {
         inserted_node = 0 ;
         nodes[pos] = (testnode_t) { .node = arraystf_node_INIT(4, nodes[pos].key), .key = { 0, (uint8_t)(pos/65536), (uint8_t)(pos/256), (uint8_t)pos } } ;
         TEST(0 == tryinsert_arraystf(array, &nodes[pos].node, &inserted_node, 0))
         TEST(inserted_node == &nodes[pos].node) ;
         TEST(1+pos == length_arraystf(array)) ;
      }
      for (size_t pos = 0; pos < nrnodes; ++pos) {
         TEST(&nodes[pos].node == at_arraystf(array, 4, nodes[pos].key)) ;
      }
      for (size_t pos = 0; pos < nrnodes; ++pos) {
         removed_node = 0 ;
         TEST(0 == tryremove_arraystf(array, 4, nodes[pos].key, &removed_node)) ;
         TEST(&nodes[pos].node == removed_node) ;
         TEST(0 == nodes[pos].copycount) ;
         TEST(0 == nodes[pos].freecount) ;
         TEST(0 == at_arraystf(array, 4, nodes[pos].key))
         TEST(nrnodes-1-pos == length_arraystf(array)) ;
      }
   }

   // TEST insert_arraystf, at_arraystf, remove_arraystf: descending
   for (uint32_t topsize = 4096; topsize <= 8192; topsize *= 2) {
      TEST(0 == delete_arraystf(&array, 0)) ;
      TEST(0 == new_arraystf(&array, topsize)) ;
      for (size_t pos = nrnodes; (pos --); ) {
         nodes[pos] = (testnode_t) { .node = arraystf_node_INIT(4, nodes[pos].key), .key = { 0, (uint8_t)(pos/65536), (uint8_t)(pos/256), (uint8_t)pos } } ;
         TEST(0 == tryinsert_arraystf(array, &nodes[pos].node, &inserted_node, &nodeadp))
         TEST(1 == nodes[pos].copycount) ;
         TEST(0 == nodes[pos].freecount) ;
         TEST(nrnodes-pos == length_arraystf(array)) ;
      }
      for (size_t pos = nrnodes; (pos --); ) {
         TEST(&nodes[pos].node == at_arraystf(array, 4, nodes[pos].key)) ;
      }
      for (size_t pos = nrnodes; (pos --); ) {
         removed_node = 0 ;
         TEST(0 != at_arraystf(array, 4, nodes[pos].key)) ;
         TEST(0 == tryremove_arraystf(array, 4, nodes[pos].key, &removed_node)) ;
         TEST(&nodes[pos].node == removed_node) ;
         TEST(1 == nodes[pos].copycount) ;
         TEST(0 == nodes[pos].freecount) ;
         TEST(0 == at_arraystf(array, 4, nodes[pos].key))
         TEST(pos == length_arraystf(array)) ;
         nodes[pos].copycount = 0 ;
         nodes[pos].freecount = 0 ;
      }
   }

   // TEST insert_arraystf, remove_arraystf: random
   srand(99999) ;
   for (size_t count2 = 0; count2 < 10; ++count2) {
      for (size_t count = 0; count < nrnodes; ++count) {
         size_t pos = ((unsigned)rand() % nrnodes) ;
         if (nodes[pos].copycount) {
            removed_node = 0 ;
            TEST(&nodes[pos].node == at_arraystf(array, 4, nodes[pos].key)) ;
            TEST(0 == tryremove_arraystf(array, 4, nodes[pos].key, &removed_node)) ;
            TEST(&nodes[pos].node == removed_node) ;
            TEST(1 == nodes[pos].copycount) ;
            TEST(0 == nodes[pos].freecount) ;
            nodes[pos].copycount = 0 ;
         } else {
            TEST(0 == tryinsert_arraystf(array, &nodes[pos].node, &inserted_node, &nodeadp)) ;
            TEST(1 == nodes[pos].copycount) ;
            TEST(0 == nodes[pos].freecount) ;
         }
      }
   }
   TEST(0 == delete_arraystf(&array, 0)) ;
   for (size_t pos = nrnodes; (pos --); ) {
      TEST(0 == nodes[pos].freecount) ;
   }

   // TEST delete_arraystf
   TEST(0 == new_arraystf(&array, 16384)) ;
   for (size_t pos = nrnodes; (pos --); ) {
      TEST(0 == tryinsert_arraystf(array, &nodes[pos].node, &inserted_node, 0))
      TEST(nrnodes-pos == length_arraystf(array)) ;
   }
   TEST(0 == delete_arraystf(&array, &nodeadp)) ;
   TEST(0 == array) ;
   for (size_t pos = nrnodes; (pos --); ) {
      TEST(1 == nodes[pos].freecount) ;
      nodes[pos].freecount = 0 ;
   }

   // TEST delete_arraystf: lifetime.delete_object set to 0
   TEST(0 == new_arraystf(&array, 16384)) ;
   for (size_t pos = nrnodes; (pos --); ) {
      TEST(0 == tryinsert_arraystf(array, &nodes[pos].node, &inserted_node, 0))
      TEST(nrnodes-pos == length_arraystf(array)) ;
   }
   typeadapt.lifetime.delete_object = 0 ;
   TEST(0 == delete_arraystf(&array, &nodeadp)) ;
   typeadapt.lifetime.delete_object = &freenode_testnodeadapt ;
   TEST(0 == array) ;
   for (size_t pos = nrnodes; (pos --); ) {
      TEST(0 == nodes[pos].freecount) ;
   }

   // TEST delete_arraystf: nodeadp set to 0
   TEST(0 == new_arraystf(&array, 16384)) ;
   for (size_t pos = nrnodes; (pos --); ) {
      TEST(0 == tryinsert_arraystf(array, &nodes[pos].node, &inserted_node, 0))
      TEST(nrnodes-pos == length_arraystf(array)) ;
   }
   TEST(0 == delete_arraystf(&array, 0)) ;
   TEST(0 == array) ;
   for (size_t pos = nrnodes; (pos --); ) {
      TEST(0 == nodes[pos].freecount) ;
   }

   // unprepare
   TEST(0 == free_vmblock(&memblock)) ;

   return 0 ;
ONABORT:
   (void) delete_arraystf(&array, 0) ;
   free_vmblock(&memblock) ;
   return EINVAL ;
}

static int test_error(void)
{
   const size_t      nrnodes   = 10000 ;
   vm_block_t        memblock  = vm_block_INIT_FREEABLE ;
   testnode_adapt_t  typeadapt = { typeadapt_INIT_LIFETIME(&copynode_testnodeadapt, &freenode_testnodeadapt), test_errortimer_INIT_FREEABLE } ;
   typeadapt_member_t nodeadp  = typeadapt_member_INIT(genericcast_typeadapt(&typeadapt,testnode_adapt_t,testnode_t,void*), offsetof(testnode_t, node)) ;
   arraystf_t        * array   = 0 ;
   testnode_t        * nodes ;
   arraystf_node_t   * removed_node  = 0 ;
   arraystf_node_t   * inserted_node = 0 ;
   arraystf_node_t   * existing_node = 0 ;
   char              * logbuffer ;
   size_t            logbufsize1 ;
   size_t            logbufsize2 ;

   // prepare
   TEST(0 == init_vmblock(&memblock, (pagesize_vm()-1+nrnodes*sizeof(testnode_t)) / pagesize_vm() )) ;
   nodes = (testnode_t *) memblock.addr ;
   TEST(0 == new_arraystf(&array, 256)) ;

   // TEST EINVAL
   TEST(EINVAL == new_arraystf(&array, 0x800001/*too big*/)) ;
      // key has length SIZE_MAX
   nodes[0] = (testnode_t) { .node = arraystf_node_INIT(SIZE_MAX, nodes[0].key), .key = { 0 } } ;
   TEST(EINVAL == tryinsert_arraystf(array, &nodes[0].node, &inserted_node, 0)) ;

   // TEST EEXIST
   nodes[0] = (testnode_t) { .node = arraystf_node_INIT(1, nodes[0].key), .key = { 0 } } ;
   TEST(0 == tryinsert_arraystf(array, &nodes[0].node, &inserted_node, 0)) ;
   nodes[1] = (testnode_t) { .node = arraystf_node_INIT(1, nodes[1].key), .key = { 0 } } ;
   GETBUFFER_LOG(&logbuffer, &logbufsize1) ;
   TEST(EEXIST == tryinsert_arraystf(array, &nodes[1].node, &existing_node, 0)) ;  // no log
   GETBUFFER_LOG(&logbuffer, &logbufsize2) ;
   TEST(logbufsize1 == logbufsize2) ;
   TEST(&nodes[0].node == existing_node) ;
   existing_node = 0 ;
   TEST(EEXIST == insert_arraystf(array, &nodes[1].node, &existing_node, 0)) ;     // log
   GETBUFFER_LOG(&logbuffer, &logbufsize2) ;
   TEST(logbufsize1 < logbufsize2) ;
   TEST(0 == existing_node) ;

   // TEST ESRCH
   arraystf_findresult_t found ;
   nodes[1] = (testnode_t) { .node = arraystf_node_INIT(1, nodes[1].key), .key = { 1 } } ;
   GETBUFFER_LOG(&logbuffer, &logbufsize1) ;
   TEST(0     == at_arraystf(array, 1, nodes[1].key)) ;           // no log
   TEST(ESRCH == find_arraystf(array, &nodes[1].node, &found)) ;  // no log
   TEST(ESRCH == tryremove_arraystf(array, 1, nodes[1].key, 0)) ; // no log
   GETBUFFER_LOG(&logbuffer, &logbufsize2) ;
   TEST(logbufsize1 == logbufsize2) ;
   TEST(ESRCH == remove_arraystf(array, 1, nodes[1].key, 0)) ;    // log
   GETBUFFER_LOG(&logbuffer, &logbufsize2) ;
   TEST(logbufsize1 < logbufsize2) ;
   nodes[0].freecount = 0 ;
   TEST(0 == tryremove_arraystf(array, 1, nodes[0].key, &removed_node)) ;
   TEST(&nodes[0].node == removed_node) ;

   // TEST delete_arraystf: ERROR
   for (size_t pos = 0; pos < nrnodes; ++pos) {
      nodes[pos] = (testnode_t) { .node = arraystf_node_INIT(2, nodes[pos].key), .key = { (uint8_t)(pos/256), (uint8_t)pos } } ;
      TEST(0 == tryinsert_arraystf(array, &nodes[pos].node, &inserted_node, 0))
      TEST(1+pos == length_arraystf(array)) ;
   }
   init_testerrortimer(&typeadapt.errcounter, 1, 12345) ;
   TEST(12345 == delete_arraystf(&array, &nodeadp)) ;
   for (unsigned pos = 0; pos < nrnodes; ++pos) {
      TEST((pos != 0) == nodes[pos].freecount) ;
   }

   // unprepare
   TEST(0 == free_vmblock(&memblock)) ;

   return 0 ;
ONABORT:
   (void) delete_arraystf(&array, 0) ;
   free_vmblock(&memblock) ;
   return EINVAL ;
}

static int test_iterator(void)
{
   const size_t      nrnodes  = 30000 ;
   vm_block_t        memblock = vm_block_INIT_FREEABLE ;
   arraystf_iterator_t iter   = arraystf_iterator_INIT_FREEABLE ;
   arraystf_t        * array  = 0 ;
   testnode_t        * nodes ;
   arraystf_node_t   * removed_node ;
   size_t            nextpos ;

   // prepare
   TEST(0 == init_vmblock(&memblock, (pagesize_vm()-1+nrnodes*sizeof(testnode_t)) / pagesize_vm() )) ;
   nodes = (testnode_t *) memblock.addr ;
   TEST(0 == new_arraystf(&array, 256)) ;
   for (size_t i = 0; i < nrnodes; ++i) {
      snprintf((char*)nodes[i].key, sizeof(nodes[i].key), "%05zu", i) ;
      nodes[i].node = (arraystf_node_t) arraystf_node_INIT(5, nodes[i].key) ;
      TEST(0 == insert_arraystf(array, &nodes[i].node, 0, 0)) ;
   }

   // TEST arraystf_iterator_INIT_FREEABLE
   TEST(0 == iter.stack) ;
   TEST(0 == iter.ri) ;

   // TEST initfirst_arraystfiterator, free_arraystfiterator
   iter.ri = 1 ;
   TEST(0 == initfirst_arraystfiterator(&iter, array)) ;
   TEST(0 != iter.stack) ;
   TEST(0 == iter.ri) ;
   TEST(0 == free_arraystfiterator(&iter)) ;
   TEST(0 == iter.stack) ;
   TEST(0 == iter.ri) ;
   TEST(0 == free_arraystfiterator(&iter)) ;
   TEST(0 == iter.stack) ;
   TEST(0 == iter.ri) ;

   // TEST next_arraystfiterator
   TEST(0 == initfirst_arraystfiterator(&iter, array)) ;
   nextpos = 0 ;
   for (iteratedtype_arraystf node = 0; !node; node = (void*)1) {
      while (next_arraystfiterator(&iter, array, &node)) {
         char nextnr[6] ;
         snprintf(nextnr, sizeof(nextnr), "%05zu", nextpos ++) ;
         TEST(0 == strcmp((const char*)node->addr, nextnr)) ;
      }
   }
   TEST(iter.ri == toplevelsize_arraystf(array)) ;
   TEST(nextpos == nrnodes) ;
   TEST(0 == free_arraystfiterator(&iter)) ;

   // TEST foreach all
   nextpos = 0 ;
   foreach (_arraystf, node, array) {
      char nextnr[6] ;
      snprintf(nextnr, sizeof(nextnr), "%05zu", nextpos ++) ;
      TEST(0 == strcmp((const char*)((arraystf_node_t*)node)->addr, nextnr)) ;
   }
   TEST(nextpos == nrnodes) ;

   // TEST foreach break after nrnodes/2
   nextpos = 0 ;
   foreach (_arraystf, node, array) {
      char nextnr[6] ;
      snprintf(nextnr, sizeof(nextnr), "%05zu", nextpos ++) ;
      TEST(0 == strcmp((const char*)node->addr, nextnr)) ;
      if (nextpos == nrnodes/2) break ;
   }
   TEST(nextpos == nrnodes/2) ;

   // TEST foreach (nrnodes/2 .. nrnodes) after remove
   for (size_t i = 0; i < nrnodes/2; ++i) {
      uint8_t key[10] ;
      sprintf((char*)key, "%05ld", (long)i) ;
      TEST(0 == remove_arraystf(array, 5, key, &removed_node)) ;
   }
   nextpos = nrnodes/2 ;
   foreach (_arraystf, node, array) {
      char nextnr[6] ;
      snprintf(nextnr, sizeof(nextnr), "%05zu", nextpos ++) ;
      TEST(0 == strcmp((const char*)node->addr, nextnr)) ;
   }
   TEST(nextpos == nrnodes) ;

   // unprepare
   TEST(0 == delete_arraystf(&array, 0)) ;
   TEST(0 == free_vmblock(&memblock)) ;

   return 0 ;
ONABORT:
   (void) delete_arraystf(&array, 0) ;
   free_vmblock(&memblock) ;
   return EINVAL ;
}

static int test_zerokey(void)
{
   arraystf_t        * array  = 0 ;
   vm_block_t        memblock = vm_block_INIT_FREEABLE ;
   const uint16_t    nrkeys   = 256 ;
   const uint16_t    keylen   = 1024 ;
   testnode_t        * nodes ;
   arraystf_node_t   * removed_node ;
   uint8_t           * keys ;

   // prepare
   TEST(0 == init_vmblock(&memblock, (pagesize_vm()-1 + nrkeys*25u*sizeof(testnode_t)+(size_t)nrkeys*keylen) / pagesize_vm() )) ;
   keys  = memblock.addr ;
   nodes = (testnode_t*) &memblock.addr[nrkeys*keylen] ;

   // TEST insert_arraystf: same keys with different length
   memset(memblock.addr, 0, memblock.size) ;
   for (unsigned i = 0; i < nrkeys; ++i) {
      keys[i*keylen] = (uint8_t) i;
   }
   TEST(0 == new_arraystf(&array, 256)) ;
   for (unsigned i = 0; i < nrkeys; ++i) {
      for (unsigned l = 1; l <= 25; ++l) {
         // same key with different length
         nodes[25*i+l-1].node = (arraystf_node_t) arraystf_node_INIT(keylen/l, &keys[i*keylen]) ;
         TEST(0 == insert_arraystf(array, &nodes[25*i+l-1].node, 0, 0)) ;
      }
   }
   for (unsigned i = 0; i < nrkeys; ++i) {
      for (unsigned l = 1; l <= 25; ++l) {
         arraystf_node_t * node = at_arraystf(array, keylen/l, &keys[i*keylen]) ;
         TEST(node       == &nodes[25*i+l-1].node) ;
         TEST(node->addr == &keys[i*keylen]) ;
         TEST(node->size == keylen/l) ;
      }
   }
   for (unsigned i = 0; i < nrkeys; ++i) {
      for (unsigned l = 1; l <= 25; ++l) {
         TEST(0 == remove_arraystf(array, keylen/l, &keys[i*keylen], &removed_node)) ;
      }
   }
   TEST(0 == length_arraystf(array)) ;
   TEST(0 == delete_arraystf(&array, 0)) ;

   // unprepare
   TEST(0 == free_vmblock(&memblock)) ;

   return 0 ;
ONABORT:
   (void) delete_arraystf(&array, 0) ;
   free_vmblock(&memblock) ;
   return EINVAL ;
}

arraystf_IMPLEMENT(_arraytest, testnode_t, node)
arraystf_IMPLEMENT(_arraytest2, testnode_t, node2)

static int test_generic(void)
{
   const size_t      nrnodes    = bitsof(((testnode_t*)0)->key) ;
   vm_block_t        memblock   = vm_block_INIT_FREEABLE ;
   arraystf_t        * array    = 0 ;
   arraystf_t        * array2   = 0 ;
   testnode_adapt_t  typeadapt = { typeadapt_INIT_LIFETIME(&copynode_testnodeadapt, &freenode_testnodeadapt), test_errortimer_INIT_FREEABLE } ;
   typeadapt_member_t nodeadp1 = typeadapt_member_INIT(genericcast_typeadapt(&typeadapt,testnode_adapt_t,testnode_t,void*), offsetof(testnode_t, node)) ;
   typeadapt_member_t nodeadp2 = typeadapt_member_INIT(genericcast_typeadapt(&typeadapt,testnode_adapt_t,testnode_t,void*), offsetof(testnode_t, node2)) ;
   test_errortimer_t memerror ;
   testnode_t        * nodes ;
   testnode_t        * inserted_node ;
   size_t            nextpos ;

   // prepare
   TEST(0 == init_vmblock(&memblock, (pagesize_vm()-1+nrnodes*sizeof(testnode_t)) / pagesize_vm() )) ;
   nodes = (testnode_t *) memblock.addr ;
   TEST(0 == new_arraytest(&array, 256)) ;
   TEST(0 == new_arraytest2(&array2, 256)) ;

   // TEST insert_arraystf: inserted_node parameter set to 0
   nodes[0] = (testnode_t) { .node = arraystf_node_INIT(sizeof(nodes[0].key), nodes[0].key), .node2 = arraystf_node_INIT(sizeof(nodes[0].key2), nodes[0].key2) } ;
   nodes[0].key[0 / 8] = (uint8_t) (0x80u >> (0%8)) ;
   nodes[0].key2[(nrnodes-1-0) / 8] = (uint8_t) (0x80u >> ((nrnodes-1-0)%8)) ;
   TEST(0 == insert_arraytest(array, &nodes[0], 0, &nodeadp1)) ;
   TEST(0 == insert_arraytest2(array2, &nodes[0], 0, &nodeadp2)) ;

   // TEST tryinsert_arraystf: ENOMEM => inserted_node is set to 0
   nodes[1] = (testnode_t) { .node = arraystf_node_INIT(sizeof(nodes[1].key), nodes[1].key), .node2 = arraystf_node_INIT(sizeof(nodes[1].key2), nodes[1].key2) } ;
   nodes[1].key[0 / 8] = (uint8_t) (0x80u >> (0%8)) ;
   nodes[1].key[1] = 1 ;
   nodes[1].key2[(nrnodes-1-0) / 8] = (uint8_t) (1 + (0x80u >> ((nrnodes-1-0)%8))) ;
   init_testerrortimer(&memerror, 1, ENOMEM) ;
   setresizeerr_testmm(mmcontext_testmm(), &memerror) ;
   inserted_node = &nodes[1] ;
   TEST(ENOMEM == tryinsert_arraytest(array, &nodes[1], &inserted_node, &nodeadp1)) ;
   TEST(0 == inserted_node) ;
   TEST(1 == nodes[1].copycount) ;
   TEST(1 == nodes[1].freecount) ;
   init_testerrortimer(&memerror, 1, ENOMEM) ;
   setresizeerr_testmm(mmcontext_testmm(), &memerror) ;
   inserted_node = &nodes[1] ;
   TEST(ENOMEM == tryinsert_arraytest2(array2, &nodes[1], &inserted_node, &nodeadp2)) ;
   TEST(0 == inserted_node) ;
   TEST(2 == nodes[1].copycount) ;
   TEST(2 == nodes[1].freecount) ;

   // TEST insert_arraystf
   for (size_t i = 1; i < nrnodes; i += 2) {
      nodes[i] = (testnode_t) { .node = arraystf_node_INIT(sizeof(nodes[i].key), nodes[i].key), .node2 = arraystf_node_INIT(sizeof(nodes[i].key2), nodes[i].key2) } ;
      nodes[i].key[i / 8] = (uint8_t) (0x80u >> (i%8)) ;
      nodes[i].key2[(nrnodes-1-i) / 8] = (uint8_t) (0x80u >> ((nrnodes-1-i)%8)) ;
      inserted_node = 0 ;
      TEST(0 == insert_arraytest(array, &nodes[i], &inserted_node, &nodeadp1)) ;
      TEST(inserted_node == &nodes[i]) ;
      inserted_node = 0 ;
      TEST(0 == insert_arraytest2(array2, &nodes[i], &inserted_node, &nodeadp2)) ;
      TEST(inserted_node == &nodes[i]) ;
      TEST(2 == nodes[i].copycount) ;
      TEST(i/2+2 == length_arraytest(array)) ;
      TEST(i/2+2 == length_arraytest2(array2)) ;
   }

   // TEST tryinsert_arraystf
   for (size_t i = 2; i < nrnodes; i += 2) {
      nodes[i] = (testnode_t) { .node = arraystf_node_INIT(sizeof(nodes[i].key), nodes[i].key), .node2 = arraystf_node_INIT(sizeof(nodes[i].key2), nodes[i].key2) } ;
      nodes[i].key[i / 8] = (uint8_t) (0x80u >> (i%8)) ;
      nodes[i].key2[(nrnodes-1-i) / 8] = (uint8_t) (0x80u >> ((nrnodes-1-i)%8)) ;
      inserted_node = 0 ;
      TEST(0 == tryinsert_arraytest(array, &nodes[i], &inserted_node, &nodeadp1)) ;
      TEST(inserted_node == &nodes[i]) ;
      inserted_node = 0 ;
      TEST(0 == tryinsert_arraytest2(array2, &nodes[i], &inserted_node, &nodeadp2)) ;
      TEST(inserted_node == &nodes[i]) ;
      TEST(2 == nodes[i].copycount) ;
      TEST((nrnodes+i)/2+1 == length_arraytest(array)) ;
      TEST((nrnodes+i)/2+1 == length_arraytest2(array2)) ;
   }

   // TEST tryinsert_arraystf: EEXIST => inserted_node is set to existing node
   for (size_t i = 1; i < nrnodes; ++ i) {
      inserted_node = 0 ;
      TEST(EEXIST == tryinsert_arraytest(array, &nodes[i], &inserted_node, &nodeadp1)) ;
      TEST(inserted_node == &nodes[i]) ;
      inserted_node = 0 ;
      TEST(EEXIST == tryinsert_arraytest2(array2, &nodes[i], &inserted_node, &nodeadp2)) ;
      TEST(inserted_node == &nodes[i]) ;
      TEST(nrnodes == length_arraytest(array)) ;
      TEST(nrnodes == length_arraytest2(array2)) ;
   }

   {
      // TEST at_arraystf: return value 0
      testnode_t node = { .node = arraystf_node_INIT(sizeof(node.key), node.key), .node2 = arraystf_node_INIT(sizeof(node.key2), node.key2) } ;
      node.key[0 / 8] = (uint8_t) (0x80u >> (0%8)) ;
      node.key[0 / 8 + 1] = 1 ;
      node.key2[(nrnodes-1-0) / 8] = (uint8_t) (0x80u >> ((nrnodes-1-0)%8)) ;
      node.key2[(nrnodes-1-0) / 8 - 1] = 1 ;
      TEST(0 == at_arraytest(array, node.node.size, node.node.addr)) ;
      TEST(0 == at_arraytest2(array2, node.node2.size, node.node2.addr)) ;
   }

   // TEST at_arraystf
   for (size_t i = 0; i < nrnodes; ++i) {
      testnode_t node = { .node = arraystf_node_INIT(sizeof(node.key), node.key), .node2 = arraystf_node_INIT(sizeof(node.key2), node.key2) } ;
      node.key[i / 8] = (uint8_t) (0x80u >> (i%8)) ;
      node.key2[(nrnodes-1-i) / 8] = (uint8_t) (0x80u >> ((nrnodes-1-i)%8)) ;
      TEST(&nodes[i] == at_arraytest(array, node.node.size, node.node.addr)) ;
      TEST(&nodes[i] == at_arraytest2(array2, node.node2.size, node.node2.addr)) ;
      TEST(2 == nodes[i].copycount) ;
   }

   // TEST foreach all
   nextpos = nrnodes ;
   foreach (_arraytest, node, array) {
      -- nextpos ;
      TEST(node == &nodes[nextpos]) ;
   }
   TEST(nextpos == 0) ;
   nextpos = 0 ;
   foreach (_arraytest2, node, array2) {
      TEST(node == &nodes[nextpos]) ;
      ++ nextpos ;
   }
   TEST(nextpos == nrnodes) ;

   // TEST remove_arraystf
   for (size_t i = 0; i < nrnodes; ++i) {
      testnode_t  * removed_node = 0 ;
      testnode_t  node = { .node = arraystf_node_INIT(sizeof(node.key), node.key), .node2 = arraystf_node_INIT(sizeof(node.key2), node.key2) } ;
      node.key[i / 8] = (uint8_t) (0x80u >> (i%8)) ;
      node.key2[(nrnodes-1-i) / 8] = (uint8_t) (0x80u >> ((nrnodes-1-i)%8)) ;
      TEST(0 == remove_arraytest(array, node.node.size, node.node.addr, &removed_node)) ;
      TEST(&nodes[i] == removed_node) ;
      removed_node = 0 ;
      TEST(0 == remove_arraytest2(array2, node.node2.size, node.node2.addr, &removed_node)) ;
      TEST(&nodes[i] == removed_node) ;
      TEST(nrnodes-1-i == length_arraytest(array)) ;
      TEST(nrnodes-1-i == length_arraytest2(array2)) ;
   }

   // TEST delete_arraystf
   for (size_t i = nrnodes; (i--); ) {
      nodes[i] = (testnode_t) { .node = arraystf_node_INIT(sizeof(nodes[i].key), nodes[i].key), .node2 = arraystf_node_INIT(sizeof(nodes[i].key2), nodes[i].key2) } ;
      nodes[i].key[i / 8] = (uint8_t) (0x80u >> (i%8)) ;
      nodes[i].key2[(nrnodes-1-i) / 8] = (uint8_t) (0x80u >> ((nrnodes-1-i)%8)) ;
      TEST(0 == insert_arraytest(array, &nodes[i], 0, &nodeadp1)) ;
      TEST(0 == insert_arraytest2(array2, &nodes[i], 0, &nodeadp2)) ;
      TEST(2 == nodes[i].copycount) ;
      TEST(&nodes[i] == at_arraytest(array, nodes[i].node.size, nodes[i].node.addr)) ;
      TEST(&nodes[i] == at_arraytest2(array2, nodes[i].node2.size, nodes[i].node2.addr)) ;
      TEST(2 == nodes[i].copycount) ;
   }
   TEST(0 == delete_arraystf(&array, &nodeadp1)) ;
   TEST(0 == delete_arraystf(&array2, &nodeadp2)) ;
   for (size_t i = 0; i < nrnodes; ++i) {
      TEST(2 == nodes[i].freecount) ;
   }

   // unprepare
   TEST(0 == free_vmblock(&memblock)) ;

   return 0 ;
ONABORT:
   TEST(0 == delete_arraystf(&array, 0)) ;
   TEST(0 == delete_arraystf(&array2, 0)) ;
   free_vmblock(&memblock) ;
   return EINVAL ;
}

int unittest_ds_inmem_arraystf()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   for (int i = 0; i < 2; ++i) {
   TEST(0 == free_resourceusage(&usage)) ;
   TEST(0 == init_resourceusage(&usage)) ;

   if (test_arraystfnode())   goto ONABORT ;
   if (test_arraystfkeyval()) goto ONABORT ;
   if (test_initfree())       goto ONABORT ;
   if (test_error())          goto ONABORT ;
   if (test_iterator())       goto ONABORT ;
   if (test_zerokey())        goto ONABORT ;
   if (test_generic())        goto ONABORT ;

   if (0 == same_resourceusage(&usage)) break ;
   CLEARBUFFER_LOG() ;
   }
   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
