/* title: Extendible-Hashing impl

   Implements <Extendible-Hashing>.

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

   file: C-kern/api/ds/inmem/exthash.h
    Header file <Extendible-Hashing>.

   file: C-kern/ds/inmem/exthash.c
    Implementation file <Extendible-Hashing impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/ds/foreach.h"
#include "C-kern/api/ds/typeadapt.h"
#include "C-kern/api/ds/inmem/exthash.h"
#include "C-kern/api/ds/inmem/redblacktree.h"
#include "C-kern/api/math/int/log2.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


// section: exthash_t

// group: description

/* about: Algorithm
 *
 * Hash Table:
 * The container of <exthash_t> holds all entries in an array of size of a power of two.
 * If too many entries are hashed to the same array index the table is doubled in size.
 * The current size of the table is stored as exponent in <exthash_t.level>.
 * The maximum value for <exthash_t.level> is stored in <exthash_t.maxlevel>. Therefore
 * the the table can grow to the maximum size pow(2, <exthash_t.maxlevel>).
 *
 * Hashing:
 * Before inserting, removing, or finding an element in the hash tbale its hash value
 * (a 32 or 64 bit value of type size_t) is calculated. The computed value is taken
 * modulo pow(2, <exthash_t.level>) (the size of the hash table). The hash table contains
 * only a pointer to the bucket where elements are stored. The reason is that extendible
 * hashing can be used to hash on external storage. In this implementation the table contains
 * a pointer to the root node of a tree. The tree manages all elements stores all elements
 * hashed to the same index.
 *
 * Growing the Table:
 * Consider a hash table of size 2 and inserting an element with hash value 0x83290a13.
 * > ---------------
 * > | index[0b00] | --> (tree with 1 entries)
 * > ---------------
 * > | index[0b01] | --> (tree with 3 entry)
 * > ---------------
 * The new element is hashed to index 1 (== 0x83290a13 % 2). The tree at index 1 contains
 * at least 3 elements therefore the implementation choses to double the table size.
 * > ---------------
 * > | index[0b00] | --> (tree)    <---
 * > ---------------                  |
 * > | index[0b01] | --> (tree)    <--|---   (level 1 / 2 == pow(2, 1))
 * > ---------------                  |  |
 * > | index[0b10] | --> (shared)  ---|  |
 * > ---------------                     |
 * > | index[0b11] | --> (shared)  -------   (level 2 / 4 == pow(2, 2))
 * > ---------------
 * The new entries in the table are marked with the special value (void*)(uintptr_t)-1.
 * This value indicates that the table entry does not point to an allocated bucket (tree)
 * but shares its content with an entry at a lower level.
 *
 * Now all entries in index 1 have to be rehashed. The index is calculated as
 * (hash value % 4) instead of (hash value % 2). A modulo operation of a power of 2
 * is the same as masking the hash value with (size-1). Therefore instead of masking
 * the hash value with 0b0...001 it is masked with 0xb0...011. So the bit pow(2, <exthash_t.level>-1)
 * decides between keeping the entry at index 1 or moving it to the newly allocated bucket at index 3.
 * The new element with hash value 0x83290a13 hashes then to index 3 (== 0x83290a13 % 4) and is stored
 * in this bucket.
 * > ---------------
 * > | index[0b00] | --> (tree      <-----------
 * > ---------------                           |
 * > | index[0b01] | --> (tree with 2 entries) |
 * > ---------------                           |
 * > | index[0b10] | --> (shared)  ------------|
 * > ---------------
 * > | index[0b11] | --> (tree with 1 rehashed old entry + new entry)
 * > ---------------
 *
 * Doubling th table again would produce the following
 * > ----------------
 * > | index[0b000] | --> (tree)    <--------- <-
 * > ----------------                        |  |
 * > | index[0b001] | --> (tree 2 entries) <-|--|--        (level 1 / 2 == pow(2, 1))
 * > ----------------                        |  | |
 * > | index[0b010] | --> (shared)  ---------|  | | <-
 * > ----------------                           | |  |
 * > | index[0b011] | --> (tree 2 entries)      | |  | <-  (level 2 / 4 == pow(2, 2))
 * > ----------------                           | |  |  |
 * > | index[0b100] | --> (shared)  ------------| |  |  |
 * > ----------------                             |  |  |
 * > | index[0b101] | --> (shared)  ---------------  |  |
 * > ----------------                                |  |
 * > | index[0b110] | --> (shared)  ------------------  |
 * > ----------------                                   |
 * > | index[0b111] | --> (shared)  ---------------------  (level 3 / 8 == pow(2, 3))
 * > ----------------
 *
 * Inserting a new element with hash value 0x11223342 hash to index 2 (modulo 8).
 * The bucket at index 2 is shares its content with the bucket at index 0.
 * Therefore the algorithm chooses to rehash the nodes stored at index 0 to make
 * index 2 unshared and then inserts the new element at index 2.
 * > ----------------
 * > | index[0b000] | --> (tree)    <--------- <-
 * > ----------------                        |  |
 * > | index[0b001] | --> (tree 2 entries) <-|--|--        (level 1 / 2 == pow(2, 1))
 * > ----------------                        |  | |
 * > | index[0b010] | --> (tree with new entry) | | <-
 * > ----------------                           | |  |
 * > | index[0b011] | --> (tree 2 entries)      | |  | <-  (level 2 / 4 == pow(2, 2))
 * > ----------------                           | |  |  |
 * > | index[0b100] | --> (shared)  ------------| |  |  |
 * > ----------------                             |  |  |
 * > | index[0b101] | --> (shared)  ---------------  |  |
 * > ----------------                                |  |
 * > | index[0b110] | --> (shared)  ------------------  |
 * > ----------------                                   |
 * > | index[0b111] | --> (shared)  ---------------------  (level 3 / 8 == pow(2, 3))
 * > ----------------
 *
 * Nothing else has to be changed. Cause index 6 at the next level shares its content
 * already with index 2. This means that a find operation has to follow the chain
 * of shared entries in the table which makes it an operation having a time complexity
 * of O(level) in the worst case.
 * */

// group: lifetime

static inline size_t lengthoftable_exthash(uint8_t level)
{
   return ((size_t)1 << level) ;
}

static inline size_t sizeoftable_exthash(uint8_t level)
{
   return lengthoftable_exthash(level) * sizeof(void*) ;
}

int init_exthash(/*out*/exthash_t * htable, size_t initial_size, size_t max_size, const typeadapt_member_t * nodeadp)
{
   int err ;
   uint8_t     level ;
   uint8_t     maxlevel ;
   memblock_t  mem = memblock_INIT_FREEABLE ;

   VALIDATE_INPARAM_TEST(initial_size <= max_size && max_size < ((size_t)-1)/sizeof(void*), ONABORT, ) ;

   static_assert(sizeof(size_t) <= 8, "uint8_t supports 128 bits") ;
   level    = (uint8_t) log2_int(initial_size) ;
   maxlevel = (uint8_t) log2_int(max_size) ;

   err = RESIZE_MM(sizeoftable_exthash(level), &mem) ;
   if (err) goto ONABORT ;

   clear_memblock(&mem) ;  // no sharing of entries

   htable->hashtable = (exthash_node_t**) addr_memblock(&mem) ;
   htable->nr_nodes = 0 ;
   htable->nodeadp  = *nodeadp ;
   htable->level    = level ;
   htable->maxlevel = maxlevel ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int free_exthash(exthash_t * htable)
{
   int err ;

   if (htable->hashtable) {

      err = removenodes_exthash(htable) ;

      memblock_t  mem = memblock_INIT(sizeoftable_exthash(htable->level), (uint8_t*)htable->hashtable) ;
      int err2 = FREE_MM(&mem) ;
      if (err2) err = err2 ;

      htable->hashtable = 0 ;
      htable->nodeadp  = (typeadapt_member_t) typeadapt_member_INIT_FREEABLE ;
      htable->level    = 0 ;
      htable->maxlevel = 0 ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

// group: search

static inline size_t hashobject_exthash(exthash_t * htable, const exthash_node_t * node)
{
   size_t hashvalue ;

   hashvalue = callhashobject_typeadaptmember(&htable->nodeadp, memberasobject_typeadaptmember(&htable->nodeadp, node)) ;

   return hashvalue ;
}

static inline size_t hashkey_exthash(exthash_t * htable, const void * key)
{
   size_t hashvalue ;

   hashvalue = callhashkey_typeadaptmember(&htable->nodeadp, key) ;

   return hashvalue ;
}

static inline size_t tableindex_exthash(exthash_t * htable, const exthash_node_t * node, const void * key)
{
   size_t hashvalue ;
   if (node) {
      hashvalue = hashobject_exthash(htable, node) ;
   } else {
      hashvalue = hashkey_exthash(htable, key) ;
   }

   // modulo to the size of hash table
   size_t mask   = lengthoftable_exthash(htable->level) - 1 ;
   size_t tabidx = hashvalue & mask ;

   return tabidx ;
}

static inline size_t unsharedtableindex_exthash(exthash_t * htable, const exthash_node_t * node, const void * key, size_t * sharedidx)
{
   size_t tabidx = tableindex_exthash(htable, node, key) ;
   size_t shridx = 0 ;

   while (0 == ~(uintptr_t)htable->hashtable[tabidx]) {
      // now go down to next entry at next lower level if it is marked with (uintptr_t)-1
      // instead of shifting the mask to the right we clear the highest bit of tabidx which skips all the 0 bits
      shridx  = tabidx ;
      // (htable->hashtable[0] != (uintptr_t)-1) ==> (tabidx != 0) !
      tabidx ^= ((size_t)1 << log2_int(tabidx)) ;
   }

   *sharedidx = shridx ;

   return tabidx ;
}

int find_exthash(exthash_t * htable, const void * key, /*out*/exthash_node_t ** found_node)
{
   int err ;
   size_t tabidx ;
   size_t sharedidx ;

   tabidx = unsharedtableindex_exthash(htable, 0, key, &sharedidx) ;

   redblacktree_t tree = redblacktree_INIT(htable->hashtable[tabidx], htable->nodeadp) ;
   err = find_redblacktree(&tree, key, found_node) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   if (err != ESRCH) {
      TRACEABORT_LOG(err) ;
   }
   return err ;
}


// group: change

static int doubletablesize_exthash(exthash_t * htable)
{
   int err ;

   if (htable->level < htable->maxlevel) {
      size_t      tablesize    = sizeoftable_exthash(htable->level) ;
      size_t      newtablesize = sizeoftable_exthash((uint8_t)(htable->level+1)) ;
      memblock_t  mem = memblock_INIT(tablesize, (uint8_t*)htable->hashtable) ;

      err = RESIZE_MM(newtablesize, &mem) ;
      if (err) goto ONABORT ;

      ++ htable->level ;
      htable->hashtable = (exthash_node_t**) addr_memblock(&mem) ;

      // set new table entries to special value (uintptr_t)-1 which indicates that
      // these entries share the same root node with corresponding table entries at level-1
      memset(addr_memblock(&mem) + tablesize, 0xFF, newtablesize - tablesize) ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

/* function: unsharebucket_exthash
 * Makes a shared bucket unshared.
 * Splits the content of the bucket <exthash_t.hashtable>[tabidx] into two buckets.
 * The second bucket index is the next higher index which points to a bucket which shares its
 * content with bucket tabidx.
 *
 * If the function succeeds tabidx is no more shared on the next higher level.
 * It is possible that another table entry shares its content with tabidx. Another call to
 * this function would then distribute the keys to these two buckets.
 *
 * (unchecked) Preconditions:
 * 1. - <tableindexfromkeyAndFollow_exthash> must have indicated that a bucket with higher index
 *      exists which shares its content.
 * */
static int unsharebucket_exthash(exthash_t * htable, size_t tabidx)
{
   int err ;
   size_t highbit  = tabidx ? ((size_t)2 << (log2_int(tabidx))) : 1 ;
   size_t splitidx = tabidx | highbit ;

   do {
      // see tableindexfromkeyAndFollow_exthash: tabidx ^= ((size_t)1 << log2_int(tabidx)) ;
      // now define sharedidx == tabidx && tabidx ^= ((size_t)1 << log2_int(sharedidx))
      // ==> htable->hashtable[sharedidx] == (uintptr_t)-1 && ispowerof2_int(sharedidx ^ tabidx)
      // ==> "the loop ends" && splitidx <= sharedidx
      splitidx  = tabidx | highbit ;
      highbit <<= 1 ;
   } while (0 != ~(uintptr_t)htable->hashtable[splitidx]) ;
   highbit >>= 1 ;

   // distribute keys to the two buckets !!
   redblacktree_t tree  = redblacktree_INIT(htable->hashtable[tabidx], htable->nodeadp) ;
   redblacktree_t tree2 = redblacktree_INIT(0/*empty tree*/, htable->nodeadp) ;
   err = 0 ;
   foreach (_redblacktree, node, &tree) {
      size_t hashvalue = hashobject_exthash(htable, node) ;

      if ((hashvalue & highbit)) {
         int err2 ;
         err2 = remove_redblacktree(&tree, node) ;
         if (!err2) err2 = insert_redblacktree(&tree2, node) ;
         if (err2) err = err2 ;
      }
   }

   // make splitidx entry unshared
   getinistate_redblacktree(&tree, &htable->hashtable[tabidx], 0) ;
   getinistate_redblacktree(&tree2, &htable->hashtable[splitidx], 0) ;

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int insert_exthash(exthash_t * htable, exthash_node_t * new_node)
{
   int err ;
   size_t tabidx ;
   size_t sharedidx ;

   tabidx = unsharedtableindex_exthash(htable, new_node, 0, &sharedidx) ;

   const bool isEntryShared = (sharedidx != 0) ;

   if (isEntryShared) {
      err = unsharebucket_exthash(htable, tabidx) ;
      if (err) goto ONABORT ;
      if (0 != ~(uintptr_t)htable->hashtable[sharedidx]) {
         // sharedidx is unshared ==> it was the bucket at the next higher level
         tabidx = sharedidx ;
      }
   } else if (htable->hashtable[tabidx] && htable->hashtable[tabidx]->left && htable->hashtable[tabidx]->right/*>= 3 elements*/) {
      err = doubletablesize_exthash(htable) ;
      if (err) goto ONABORT ;
   }

   redblacktree_t tree = redblacktree_INIT(htable->hashtable[tabidx], htable->nodeadp) ;
   err = insert_redblacktree(&tree, new_node) ;
   if (err) goto ONABORT ;
   getinistate_redblacktree(&tree, &htable->hashtable[tabidx], 0) ;

   ++ htable->nr_nodes ;

   return 0 ;
ONABORT:
   if (err != EEXIST) {
      TRACEABORT_LOG(err) ;
   }
   return err ;
}

int remove_exthash(exthash_t * htable, exthash_node_t * node)
{
   int err ;
   size_t tabidx ;
   size_t sharedidx ;

   tabidx = unsharedtableindex_exthash(htable, node, 0, &sharedidx) ;

   redblacktree_t tree = redblacktree_INIT(htable->hashtable[tabidx], htable->nodeadp) ;
   err = remove_redblacktree(&tree, node) ;
   if (err) goto ONABORT ;
   getinistate_redblacktree(&tree, &htable->hashtable[tabidx], 0) ;

   -- htable->nr_nodes ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int removenodes_exthash(exthash_t * htable)
{
   int err = 0 ;

   size_t endindex = lengthoftable_exthash(htable->level) ;

   for (size_t i = 0; i < endindex; ++i) {
      if (htable->hashtable[i]) {
         if (0 != ~(uintptr_t)htable->hashtable[i]) {
            redblacktree_t tree = redblacktree_INIT(htable->hashtable[i], htable->nodeadp) ;
            int err2 = free_redblacktree(&tree) ;
            if (err2) err = err2 ;
         }
         htable->hashtable[i] = 0 ;
      }
   }

   htable->nr_nodes = 0 ;

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

// group: test

int invariant_exthash(const exthash_t * htable)
{
   int err = 0 ;

   size_t         endindex = lengthoftable_exthash(htable->level) ;
   redblacktree_t tree = redblacktree_INIT(0, htable->nodeadp) ;

   for (size_t i = 0; i < endindex; ++i) {
      if (  htable->hashtable[i]
            && 0 != ~(uintptr_t)htable->hashtable[i]) {
         tree.root = htable->hashtable[i] ;
         err = invariant_redblacktree(&tree) ;
         if (err) goto ONABORT ;
      }
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}



// section: exthash_iterator_t

// group: lifetime

int initfirst_exthashiterator(/*out*/exthash_iterator_t * iter, exthash_t * htable)
{
   size_t endindex = lengthoftable_exthash(htable->level) ;

   static_assert( sizeof(void*) == sizeof(redblacktree_iterator_t)
                  && sizeof(void*) == sizeof(iter->next)
                  && 0 == offsetof(redblacktree_iterator_t, next)
                  && 0 == offsetof(exthash_iterator_t, next)
                  , "(redblacktree_iterator_t*)iter works") ;

   for (size_t i = 0; i < endindex; ++i) {
      if (  htable->hashtable[i]
            && 0 != ~(uintptr_t)htable->hashtable[i]) {
            redblacktree_t tree = redblacktree_INIT(htable->hashtable[i], htable->nodeadp) ;
         int err = initfirst_redblacktreeiterator((redblacktree_iterator_t*)iter, &tree) ;
         if (err) return err ;

         iter->htable     = htable ;
         iter->tableindex = i ;
         break ;
      }
   }

   return 0 ;
}

// group: iterate

bool next_exthashiterator(exthash_iterator_t * iter, /*out*/exthash_node_t ** node)
{
   if (!next_redblacktreeiterator((redblacktree_iterator_t*)iter, node)) {
      return false ;
   }

   if (!iter->next) {
      size_t endindex = lengthoftable_exthash(iter->htable->level) ;

      for (size_t i = iter->tableindex+1; i < endindex; ++i) {
         if (  iter->htable->hashtable[i]
               && 0 != ~(uintptr_t)iter->htable->hashtable[i]) {
            redblacktree_t tree = redblacktree_INIT(iter->htable->hashtable[i], iter->htable->nodeadp) ;
            (void) initfirst_redblacktreeiterator((redblacktree_iterator_t*)iter, &tree) ;
            iter->tableindex = i ;
            break ;
         }
      }
   }

   return true ;
}


// section: exthash_t

// group: test

#ifdef KONFIG_UNITTEST

typedef struct testobject_t      testobject_t ;

typeadapt_DECLARE(testadapt_t, testobject_t, uintptr_t) ;

struct testobject_t {
   size_t         deletecount ;
   size_t         key ;
   exthash_node_t node ;
} ;

static int impl_cmpkeyobj_testadapt(testadapt_t * typeadp, const uintptr_t lkey, const struct testobject_t * robject)
{
   assert(typeadp) ;
   return lkey == robject->key ? 0 : lkey < robject->key ? -1 : +1 ;
}

static int impl_cmpobj_testadapt(testadapt_t * typeadp, const struct testobject_t * lobject, const struct testobject_t * robject)
{
   assert(typeadp) ;
   return lobject->key == robject->key ? 0 : lobject->key < robject->key ? -1 : +1 ;
}

static size_t impl_hashobj_testadapt(testadapt_t * typeadp, const struct testobject_t * object)
{
   assert(typeadp) ;
   return object->key ;
}

static size_t impl_hashkey_testadapt(testadapt_t * typeadp, const uintptr_t key)
{
   assert(typeadp) ;
   return (size_t) key ;
}

static int impl_delete_testadapt(testadapt_t * typeadp, struct testobject_t ** object)
{
   int err = 0 ;
   assert(*object && typeadp) ;
   if ((*object)->deletecount == (size_t)-1) {
      err = EINVAL ;
   } else {
      ++ (*object)->deletecount ;
   }
   (*object) = 0 ;
   return err ;
}

static int test_initfree(void)
{
   exthash_t            htable    = exthash_INIT_FREEABLE ;
   typeadapt_member_t   emptyadp  = typeadapt_member_INIT_FREEABLE ;
   testadapt_t          typeadapt = typeadapt_INIT_LIFECMPHASH(0, &impl_delete_testadapt, &impl_cmpkeyobj_testadapt, &impl_cmpobj_testadapt, &impl_hashobj_testadapt, &impl_hashkey_testadapt) ;
   typeadapt_member_t   nodeadp   = typeadapt_member_INIT(genericcast_typeadapt(&typeadapt, testadapt_t, testobject_t, intptr_t), offsetof(testobject_t, node)) ;
   exthash_node_t       node      = exthash_node_INIT ;
   exthash_iterator_t   iter      = exthash_iterator_INIT_FREEABLE ;
   testobject_t         nodes[256] ;

   // prepare
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      nodes[i].deletecount = 0 ;
      nodes[i].key  = i ;
      nodes[i].node = (exthash_node_t) exthash_node_INIT ;
   }

   // TEST exthash_iterator_INIT_FREEABLE
   TEST(0 == iter.next) ;
   TEST(0 == iter.htable) ;
   TEST(0 == iter.tableindex) ;

   // TEST exthash_node_INIT
   TEST(0 == node.parent) ;
   TEST(0 == node.left) ;
   TEST(0 == node.right) ;

   // TEST exthash_INIT_FREEABLE
   TEST(0 == htable.hashtable) ;
   TEST(0 == htable.nr_nodes) ;
   TEST(1 == isequal_typeadaptmember(&htable.nodeadp, &emptyadp)) ;
   TEST(0 == htable.level) ;
   TEST(0 == htable.maxlevel) ;

   // TEST init_exthash, free_exthash
   for (unsigned i = 0, level=0; i < 1024; level+=(i!=0), i*=2, i+=(i==0)) {
      TEST(0 == init_exthash(&htable, i, 4*(i+(i==0)), &nodeadp)) ;
      TEST(0 != htable.hashtable) ;
      TEST(0 == htable.nr_nodes) ;
      TEST(1 == isequal_typeadaptmember(&htable.nodeadp, &nodeadp)) ;
      TEST(level   == htable.level) ;
      TEST(level+2 == htable.maxlevel) ;
      for (size_t ti = 0; ti < lengthoftable_exthash((uint8_t)level); ++ti) {
         TEST(0 == htable.hashtable[ti]) ;
      }
      TEST(0 == free_exthash(&htable)) ;
      TEST(0 == htable.hashtable) ;
      TEST(0 == htable.nr_nodes) ;
      TEST(1 == isequal_typeadaptmember(&htable.nodeadp, &emptyadp)) ;
      TEST(0 == htable.level) ;
      TEST(0 == htable.maxlevel) ;
      TEST(0 == free_exthash(&htable)) ;
      TEST(0 == htable.hashtable) ;
      TEST(0 == htable.nr_nodes) ;
      TEST(1 == isequal_typeadaptmember(&htable.nodeadp, &emptyadp)) ;
      TEST(0 == htable.level) ;
      TEST(0 == htable.maxlevel) ;
      TEST(0 == free_exthash(&htable)) ;
   }

   // TEST free_exthash: free all nodes in tree
   TEST(0 == init_exthash(&htable, 1, 1, &nodeadp)) ;
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      redblacktree_t tree = redblacktree_INIT(htable.hashtable[0], nodeadp) ;
      TEST(0 == insert_redblacktree(&tree, &nodes[i].node)) ;
      getinistate_redblacktree(&tree, &htable.hashtable[0], 0) ;
   }
   htable.nr_nodes = lengthof(nodes) ;
   TEST(0 == free_exthash(&htable)) ;
   TEST(0 == htable.hashtable) ;
   TEST(0 == htable.nr_nodes) ;
   TEST(1 == isequal_typeadaptmember(&htable.nodeadp, &emptyadp)) ;
   TEST(0 == htable.level) ;
   TEST(0 == htable.maxlevel) ;
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(1 == nodes[i].deletecount) ;
      nodes[i].deletecount = 0 ;
      TEST(0 == nodes[i].node.parent) ;
      TEST(0 == nodes[i].node.left) ;
      TEST(0 == nodes[i].node.right) ;
   }

   // TEST free_exthash: free all nodes in table
   TEST(0 == init_exthash(&htable, 131072, 131072, &nodeadp)) ;
   TEST(131072 == lengthoftable_exthash(htable.level)) ;
   for (unsigned i = 0; i < 131072; ++i) {
      htable.hashtable[i] = &nodes[i % lengthof(nodes)].node ;
   }
   TEST(0 == free_exthash(&htable)) ;
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(131072/lengthof(nodes) == nodes[i].deletecount) ;
      nodes[i].deletecount = 0 ;
   }

   // TEST init_exthash: EINVAL
   TEST(EINVAL == init_exthash(&htable, 2, 1, &nodeadp)) ;
   TEST(EINVAL == init_exthash(&htable, 1, 1u+((size_t)-1)/sizeof(void*), &nodeadp)) ;
   TEST(0 == htable.hashtable) ;
   TEST(0 == htable.nr_nodes) ;
   TEST(1 == isequal_typeadaptmember(&htable.nodeadp, &emptyadp)) ;
   TEST(0 == htable.level) ;
   TEST(0 == htable.maxlevel) ;

   // TEST nrelements_exthash, isempty_exthash
   for (unsigned i = 0; i < 256; ++i) {
      htable.nr_nodes = i ;
      TEST(i == nrelements_exthash(&htable)) ;
      TEST((i == 0) == isempty_exthash(&htable)) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_privquery(void)
{
   exthash_t            htable    = exthash_INIT_FREEABLE ;
   testadapt_t          typeadapt = typeadapt_INIT_LIFECMPHASH(0, &impl_delete_testadapt, &impl_cmpkeyobj_testadapt, &impl_cmpobj_testadapt, &impl_hashobj_testadapt, &impl_hashkey_testadapt) ;
   typeadapt_member_t   nodeadp   = typeadapt_member_INIT(genericcast_typeadapt(&typeadapt, testadapt_t, testobject_t, intptr_t), offsetof(testobject_t, node)) ;
   testobject_t         node      = { 0, 0, exthash_node_INIT } ;

   // prepare
   TEST(0 == init_exthash(&htable, 65536, 65536, &nodeadp)) ;

   // TEST lengthoftable_exthash
   for (size_t level = 0, result = 1; level < sizeof(size_t)*8; ++level, result <<= 1) {
      TEST(result == lengthoftable_exthash((uint8_t)level)) ;
   }

   // TEST sizeoftable_exthash
   for (size_t level = 0, result = sizeof(void*); level < sizeof(size_t)*8; ++level, result <<= 1) {
      TEST(result == sizeoftable_exthash((uint8_t)level)) ;
   }

   // TEST hashobject_exthash, hashkey_exthash
   for (unsigned i = 0; i < 65535; ++i) {
      node.key = i ;
      TEST(node.key == hashobject_exthash(&htable, &node.node)) ;
      TEST(node.key == hashkey_exthash(&htable, (void*)node.key)) ;
      node.key = ((size_t)-1) - i ;
      TEST(node.key == hashobject_exthash(&htable, &node.node)) ;
      TEST(node.key == hashkey_exthash(&htable, (void*)node.key)) ;
   }

   // TEST tableindex_exthash
   TEST(16 == htable.level) ;
   for (size_t level = 0, mask = 0; level < sizeof(size_t)*8; ++level, mask*=2, mask+=1) {
      htable.level = (uint8_t) level ;
      node.key = (size_t)-1 ;
      TEST(mask == tableindex_exthash(&htable, &node.node, 0)) ;
      TEST(mask == tableindex_exthash(&htable, 0, (void*)node.key)) ;
      node.key = (size_t)0 ;
      TEST(0 == tableindex_exthash(&htable, &node.node, 0)) ;
      TEST(0 == tableindex_exthash(&htable, 0, (void*)node.key)) ;
      node.key = (size_t)1 ;
      TEST((mask&1) == tableindex_exthash(&htable, &node.node, 0)) ;
      TEST((mask&1) == tableindex_exthash(&htable, 0, (void*)node.key)) ;
      node.key = (size_t)56 ;
      TEST((mask&56) == tableindex_exthash(&htable, &node.node, 0)) ;
      TEST((mask&56) == tableindex_exthash(&htable, 0, (void*)node.key)) ;
      node.key = (size_t)0xff000000 ;
      TEST((mask&0xff000000) == tableindex_exthash(&htable, &node.node, 0)) ;
      TEST((mask&0xff000000) == tableindex_exthash(&htable, 0, (void*)node.key)) ;
   }
   htable.level = 16 ;

   // TEST unsharedtableindex_exthash: index 0 is where it all ends + unshared buckets grow
   TEST(65536 == lengthoftable_exthash(htable.level)) ;
   memset(htable.hashtable, 255, sizeoftable_exthash(htable.level)) ;
   for (unsigned unshared = 1; unshared <= 256; unshared *= 2) {
      memset(htable.hashtable, 0, unshared * sizeof(htable.hashtable[0])) ;
      for (unsigned i = 0; i < 65536; ++i) {
         size_t sharedindex = 65536 ;
         node.key = i ;
         TEST((i & (unshared-1)) == unsharedtableindex_exthash(&htable, &node.node, 0, &sharedindex)) ;
         TEST((i & (unshared-1)) == unsharedtableindex_exthash(&htable, 0, (const void*)node.key, &sharedindex)) ;
         size_t expect = 0 ;
         if (i >= unshared) {
            expect = unshared ;
            while (!(i & expect)) {
               expect <<= 1 ;
            }
            expect |= (i & (unshared-1)) ;
         }
         TEST(expect == sharedindex) ;
      }
   }

   // unprepare
   TEST(0 == free_exthash(&htable)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_privchange(void)
{
   exthash_t            htable     = exthash_INIT_FREEABLE ;
   testadapt_t          typeadapt  = typeadapt_INIT_LIFECMPHASH(0, &impl_delete_testadapt, &impl_cmpkeyobj_testadapt, &impl_cmpobj_testadapt, &impl_hashobj_testadapt, &impl_hashkey_testadapt) ;
   typeadapt_member_t   nodeadp    = typeadapt_member_INIT(genericcast_typeadapt(&typeadapt, testadapt_t, testobject_t, intptr_t), offsetof(testobject_t, node)) ;
   testobject_t         nodes[256] = { { 0, 0, exthash_node_INIT } } ;

   // prepare
   TEST(0 == init_exthash(&htable, 1, 65536, &nodeadp)) ;

   // TEST doubletablesize_exthash
   for (uint8_t level = 0; level < 16; ++level) {
      TEST(level == htable.level) ;
      for (size_t ti = 0; ti < lengthoftable_exthash(level); ++ti) {
         htable.hashtable[ti] = (exthash_node_t*) ti ;
      }
      TEST(0 == doubletablesize_exthash(&htable)) ;
      TEST(level+1 == htable.level) ;
      TEST(16      == htable.maxlevel) ;
      for (size_t ti = 0; ti < lengthoftable_exthash(level); ++ti) {
         TEST(htable.hashtable[ti] == (exthash_node_t*)ti) ;
      }
      for (size_t ti = lengthoftable_exthash(level); ti < lengthoftable_exthash((uint8_t)(level+1)); ++ti) {
         TEST(htable.hashtable[ti] == (exthash_node_t*)(uintptr_t)-1) ;
      }
   }

   // TEST unsharebucket_exthash: finds shared bucket with higher index
   TEST(65536 == lengthoftable_exthash(htable.level)) ;
   memset(htable.hashtable, 255, sizeoftable_exthash(htable.level)) ;
   htable.hashtable[0] = 0 ; // this value is always unshared
   for (unsigned level = 0; level < 15; ++level) {
      for (unsigned i = 0; i < (1u << level); ++i) {
         size_t nexti = i | (1u << level) ;
         TEST(nexti < 65536) ;
         TEST(0 == ~(uintptr_t)htable.hashtable[nexti]) ;
         TEST(0 == unsharebucket_exthash(&htable, i)) ;
         TEST(0 == htable.hashtable[i]) ;
         TEST(0 == htable.hashtable[nexti]) ;
      }
   }

   // TEST unsharebucket_exthash: distributes all nodes between bucket [0] and shared bucket [1]
   TEST(1 <= htable.level) ;
   redblacktree_t tree = redblacktree_INIT(0, nodeadp) ;
   for (unsigned i = 0; i < 256; ++i) {
      nodes[i].key = (i << 8) | (i&1) ;
      TEST(0 == insert_redblacktree(&tree, &nodes[i].node)) ;
   }
   htable.hashtable[0] = tree.root ;
   htable.hashtable[1] = (void*) (uintptr_t)-1 ;
   TEST(0 == unsharebucket_exthash(&htable, 0)) ;
   tree.root = htable.hashtable[0] ;
   unsigned count = 0 ;
   foreach (_redblacktree, node, &tree) {
      TEST((count << 8) == ((testobject_t*)memberasobject_typeadaptmember(&nodeadp, node))->key) ;
      count += 2 ;
   }
   TEST(count == 256) ;
   tree.root = htable.hashtable[1] ;
   count = 1 ;
   foreach (_redblacktree, node, &tree) {
      TEST(((count << 8)+1) == ((testobject_t*)memberasobject_typeadaptmember(&nodeadp, node))->key) ;
      count += 2 ;
   }
   TEST(count == 257) ;

   // unprepare
   TEST(0 == free_exthash(&htable)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_findinsertremove(void)
{
   exthash_t            htable    = exthash_INIT_FREEABLE ;
   testadapt_t          typeadapt = typeadapt_INIT_LIFECMPHASH(0, &impl_delete_testadapt, &impl_cmpkeyobj_testadapt, &impl_cmpobj_testadapt, &impl_hashobj_testadapt, &impl_hashkey_testadapt) ;
   typeadapt_member_t   nodeadp   = typeadapt_member_INIT(genericcast_typeadapt(&typeadapt, testadapt_t, testobject_t, intptr_t), offsetof(testobject_t, node)) ;
   const size_t         MAXNODES  = 524288 ;
   memblock_t           mem       = memblock_INIT_FREEABLE ;
   testobject_t         * nodes ;
   exthash_node_t       * found_node ;

   // prepare
   TEST(0 == RESIZE_MM(MAXNODES * sizeof(testobject_t), &mem)) ;
   nodes = (testobject_t*) mem.addr ;
   clear_memblock(&mem) ;
   for (size_t i = 0; i < MAXNODES; ++i) {
      nodes[i].key = i ;
   }

   // TEST insert_exthash
   TEST(0 == init_exthash(&htable, MAXNODES, MAXNODES, &nodeadp)) ;
   TEST(0 == invariant_exthash(&htable)) ;
   for (size_t i = 0; i < MAXNODES; ++i) {
      TEST(0      == insert_exthash(&htable, &nodes[i].node)) ;
      TEST(EEXIST == insert_exthash(&htable, &nodes[i].node)) ;
      TEST(i+1 == nrelements_exthash(&htable)) ;
   }
   for (size_t i = 0; i < MAXNODES; ++i) {
      TEST(0 == find_exthash(&htable, (const void*)(uintptr_t)i, &found_node)) ;
      TEST(found_node == &nodes[i].node) ;
   }
   TEST(0 == invariant_exthash(&htable)) ;

   // TEST remove_exthash
   for (size_t i = 0; i < MAXNODES; ++i) {
      TEST(0     == remove_exthash(&htable, &nodes[i].node)) ;
      TEST(0     == nodes[i].deletecount) ;
      TEST(MAXNODES-1-i == nrelements_exthash(&htable)) ;
   }
   for (size_t i = 0; i < MAXNODES; ++i) {
      TEST(ESRCH == find_exthash(&htable, (const void*)(uintptr_t)i, &found_node)) ;
   }
   TEST(0 == invariant_exthash(&htable)) ;

   // TEST removenodes_exthash: empty table
   TEST(0 == nrelements_exthash(&htable)) ;
   TEST(0 == removenodes_exthash(&htable)) ;
   TEST(0 == free_exthash(&htable)) ;

   // TEST insert_exthash: table grows until max level
   TEST(0 == init_exthash(&htable, 1, MAXNODES/8, &nodeadp)) ;
   for (size_t i = 0; i < MAXNODES; ++i) {
      TEST(0      == insert_exthash(&htable, &nodes[i].node)) ;
      TEST(EEXIST == insert_exthash(&htable, &nodes[i].node)) ;
      TEST(i+1 == nrelements_exthash(&htable)) ;
   }
   TEST(MAXNODES/8 == lengthoftable_exthash(htable.level)) ;
   for (size_t i = 0; i < MAXNODES; ++i) {
      TEST(0 == find_exthash(&htable, (const void*)(uintptr_t)i, &found_node)) ;
      TEST(found_node == &nodes[i].node) ;
   }

   // TEST removenodes_exthash
   TEST(MAXNODES == nrelements_exthash(&htable)) ;
   TEST(0 == removenodes_exthash(&htable)) ;
   TEST(0 == nrelements_exthash(&htable)) ;
   for (size_t i = 0; i < MAXNODES; ++i) {
      TEST(ESRCH == find_exthash(&htable, (const void*)(uintptr_t)i, &found_node)) ;
      TEST(1 == nodes[i].deletecount) ;
      nodes[i].deletecount = 0 ;
   }

   // TEST remove_nodes: ERROR
   for (size_t i = 0; i < 256; ++i) {
      TEST(0 == insert_exthash(&htable, &nodes[i].node)) ;
   }
   nodes[0].deletecount = (size_t)-1 ;
   TEST(EINVAL == removenodes_exthash(&htable)) ;
   TEST(0 == nrelements_exthash(&htable)) ;
   TEST((size_t)-1 == nodes[0].deletecount) ;
   nodes[0].deletecount = 0 ;
   for (size_t i = 1; i < 256; ++i) {
      TEST(1 == nodes[i].deletecount) ;
      nodes[i].deletecount = 0 ;
   }
   TEST(0 == free_exthash(&htable)) ;

   // TEST insert_exthash, remove_exthash: random
   static_assert(MAXNODES >= 16*1024, "") ;
   TEST(0 == init_exthash(&htable, 1, 1024, &nodeadp)) ;
   srand(123) ;
   for (size_t i = 0; i < 64*1024; ++i) {
      size_t id = (size_t)rand() % (16*1024) ;
      if (0 == find_exthash(&htable, (const void*)id, &found_node)) {
         TEST(found_node == &nodes[id].node) ;
         TEST(0 == remove_exthash(&htable, found_node)) ;
      } else {
         TEST(0 == insert_exthash(&htable, &nodes[id].node)) ;
      }
   }
   TEST(0 == invariant_exthash(&htable)) ;
   TEST(0 == free_exthash(&htable)) ;

   // TEST foreach
   static_assert(MAXNODES >= 512, "") ;
   TEST(0 == init_exthash(&htable, 512, 512, &nodeadp)) ;
   for (size_t i = 0; i < 1024; ++i) {
      TEST(0 == insert_exthash(&htable, &nodes[i].node)) ;
   }
   for (size_t i = 0; i == 0; i = 1) {
      foreach (_exthash, node, &htable) {
         TEST(node == &nodes[i/2 + 512*(i&1)].node)
         ++ i ;
      }
      TEST(i == 1024) ;
   }

   // TEST initfirst_exthashiterator
   exthash_iterator_t iter = exthash_iterator_INIT_FREEABLE ;
   exthash_node_t * old = htable.hashtable[0] ;
   htable.hashtable[0] = 0 ;
   TEST(0 == initfirst_exthashiterator(&iter, &htable)) ;
   TEST(iter.next   != 0) ;
   TEST(iter.htable == &htable) ;
   TEST(iter.tableindex == 1) ;
   htable.hashtable[0] = (void*)(uintptr_t)-1 ;
   iter = (exthash_iterator_t) exthash_iterator_INIT_FREEABLE ;
   TEST(0 == initfirst_exthashiterator(&iter, &htable)) ;
   TEST(iter.next   != 0) ;
   TEST(iter.htable == &htable) ;
   TEST(iter.tableindex == 1) ;
   htable.hashtable[0] = old ;
   iter = (exthash_iterator_t) { .next = 0, .tableindex = 1 } ;
   TEST(0 == initfirst_exthashiterator(&iter, &htable)) ;
   TEST(iter.next   != 0) ;
   TEST(iter.htable == &htable) ;
   TEST(iter.tableindex == 0) ;

   // TEST next_exthashiterator
   for (size_t i = 0; i == 0; i = 1) {
      while (next_exthashiterator(&iter, &found_node)) {
         TEST(found_node == &nodes[i/2 + 512*(i&1)].node)
         if (i&1 && iter.next) {
            TEST(iter.tableindex == (i+1)/2) ;
         }
         ++ i ;
      }
      TEST(i == 1024) ;
      TEST(0 == iter.next) ;
      TEST(511 == iter.tableindex) ;
      TEST(0 == next_exthashiterator(&iter, &found_node)) ;
      TEST(0 == iter.next) ;
      TEST(511 == iter.tableindex) ;
   }

   // TEST free_exthashiterator
   iter.next   = (void*) 1 ;
   iter.htable = &htable ;
   iter.tableindex = 3 ;
   TEST(0 == free_exthashiterator(&iter)) ;
   TEST(iter.next       == 0) ;
   TEST(iter.htable     == &htable) ;
   TEST(iter.tableindex == 3) ;

   // unprepare
   TEST(0 == free_exthash(&htable)) ;
   TEST(0 == FREE_MM(&mem)) ;

   return 0 ;
ONABORT:
   FREE_MM(&mem) ;
   return EINVAL ;
}

exthash_IMPLEMENT(_testhash, testobject_t, uintptr_t, node)

static int test_generic(void)
{
   exthash_t            htable    = exthash_INIT_FREEABLE ;
   testadapt_t          typeadapt = typeadapt_INIT_LIFECMPHASH(0, &impl_delete_testadapt, &impl_cmpkeyobj_testadapt, &impl_cmpobj_testadapt, &impl_hashobj_testadapt, &impl_hashkey_testadapt) ;
   typeadapt_member_t   nodeadp   = typeadapt_member_INIT(genericcast_typeadapt(&typeadapt, testadapt_t, testobject_t, intptr_t), offsetof(testobject_t, node)) ;
   testobject_t         nodes[256] ;

   // prepare
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      nodes[i].deletecount = 0 ;
      nodes[i].key  = i ;
      nodes[i].node = (exthash_node_t) exthash_node_INIT ;
   }

   // TEST init_exthash
   TEST(0 == init_testhash(&htable, 1, lengthof(nodes), &nodeadp)) ;
   TEST(0 != htable.hashtable) ;
   TEST(0 == htable.nr_nodes) ;
   TEST(1 == isequal_typeadaptmember(&htable.nodeadp, &nodeadp)) ;
   TEST(0 == htable.level) ;
   TEST(8 == htable.maxlevel) ;
   TEST(0 == nrelements_testhash(&htable)) ;
   TEST(1 == isempty_testhash(&htable)) ;

   // TEST free_exthash
   typeadapt_member_t emptyadp = typeadapt_member_INIT_FREEABLE ;
   TEST(0 == free_testhash(&htable)) ;
   TEST(0 == htable.hashtable) ;
   TEST(0 == htable.nr_nodes) ;
   TEST(1 == isequal_typeadaptmember(&htable.nodeadp, &emptyadp)) ;

   // TEST insert_exthash
   TEST(0 == init_testhash(&htable, lengthof(nodes), lengthof(nodes), &nodeadp)) ;
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(0 == insert_testhash(&htable, &nodes[i])) ;
      TEST(i+1 == nrelements_testhash(&htable)) ;
      TEST(0 == isempty_testhash(&htable)) ;
   }

   // TEST foreach
   for (unsigned i = 0; !i; i = 1) {
      foreach (_testhash, node, &htable) {
         TEST(node == &nodes[i]) ;
         ++ i ;
      }
      TEST(lengthof(nodes) == i) ;
   }

   // TEST find_exthash
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      testobject_t * found_node ;
      TEST(0 == find_testhash(&htable, i, &found_node)) ;
      TEST(found_node == &nodes[i]) ;
   }

   // TEST remove_exthash
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      testobject_t * found_node ;
      TEST(0 == isempty_testhash(&htable)) ;
      TEST(0 == remove_testhash(&htable, &nodes[i])) ;
      TEST(ESRCH == find_testhash(&htable, i, &found_node)) ;
      TEST(lengthof(nodes)-1-i == nrelements_testhash(&htable)) ;
   }
   TEST(1 == isempty_testhash(&htable)) ;

   // TEST removenodes_exthash
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(0 == nodes[i].deletecount) ;
      TEST(0 == insert_testhash(&htable, &nodes[i])) ;
   }
   TEST(0 == removenodes_testhash(&htable)) ;
   TEST(0 == nrelements_testhash(&htable)) ;
   TEST(1 == isempty_testhash(&htable)) ;
   for (unsigned i = 0; i < lengthof(nodes); ++i) {
      TEST(1 == nodes[i].deletecount) ;
      nodes[i].deletecount = 0 ;
   }

   // unprepare
   TEST(0 == free_testhash(&htable)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_ds_inmem_exthash()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())          goto ONABORT ;
   if (test_privquery())         goto ONABORT ;
   if (test_privchange())        goto ONABORT ;
   if (test_findinsertremove())  goto ONABORT ;
   if (test_generic())           goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
