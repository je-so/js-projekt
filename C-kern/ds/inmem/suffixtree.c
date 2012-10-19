/* title: Suffix-Tree impl

   Implements <Suffix-Tree>.

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

   file: C-kern/api/ds/inmem/suffixtree.h
    Header file of <Suffix-Tree>.

   file: C-kern/ds/inmem/suffixtree.c
    Implementation file of <Suffix-Tree impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/ds/typeadapt.h"
#include "C-kern/api/ds/inmem/slist.h"
#include "C-kern/api/ds/inmem/suffixtree.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#include "C-kern/api/string/cstring.h"
#include "C-kern/api/string/string.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/io/filesystem/mmfile.h"
#endif

/* typedef: struct suffixtree_iterator_t
 * Shortcut for <suffixtree_iterator_t>. Holds position in <suffixtree_t> used in iteration. */
typedef struct suffixtree_iterator_t      suffixtree_iterator_t ;

/* typedef: struct suffixtree_addstate_t
 * Shortcut for <suffixtree_addstate_t>. Holds state information used in the operation which adds the next character of the input string. */
typedef struct suffixtree_addstate_t      suffixtree_addstate_t ;

/* typedef: struct suffixtree_leaf_t
 * Shortcut for <suffixtree_leaf_t>. Holds node of <suffixtree_t> without any children and no suffix link. */
typedef struct suffixtree_leaf_t          suffixtree_leaf_t ;

/* typedef: struct suffixtree_node_t
 * Shortcut for <suffixtree_node_t>. Holds node of <suffixtree_t> with children and a suffix link. */
typedef struct suffixtree_node_t          suffixtree_node_t ;

/* typedef: struct suffixtree_pos_t
 * Shortcut for <suffixtree_pos_t>. Stores position in suffix tree. */
typedef struct suffixtree_pos_t           suffixtree_pos_t ;


/* struct: suffixtree_iterator_t
 * Stores position in <suffixtree_t> which has to be visited next.
 * A position consists of the next to be visited child of type <suffixtree_node_t>.
 * The length of string which is matched on the path leading up to <next_child>
 * is stored in <prefixlen>.
 * */
struct suffixtree_iterator_t {
   /* variable: next
    * Used in <suffixtreeiterator_list_t> to connect all contained <suffixtree_iterator_t>. */
   slist_node_t         * next ;
   /* variable: prefixlen
    * Sum of length of strings on the path from root up to <next_child>. */
   size_t               prefixlen ;
   /* variable: next_child
    * Pointer to <suffixtree_node_t> which has to be visited next on this level of hierachy. */
   suffixtree_node_t    * next_child ;
} ;

// group: lifetime

/* function: new_suffixtreeiterator
 * Allocates a new <suffixtree_iterator_t> object. */
static inline int new_suffixtreeiterator(/*out*/suffixtree_iterator_t ** iter)
{
   int err ;
   memblock_t objmem = memblock_INIT_FREEABLE ;

   err = MM_RESIZE(sizeof(suffixtree_iterator_t), &objmem) ;
   if (err) return err ;

   *iter = (suffixtree_iterator_t*) objmem.addr ;
   return 0 ;
}

/* function: delete_suffixtreeiterator
 * Frees all memory allocated for <suffixtree_iterator_t> object. */
static int delete_suffixtreeiterator(suffixtree_iterator_t ** iter)
{
   int err = 0 ;

   if (*iter) {
      memblock_t objmem = memblock_INIT(sizeof(suffixtree_iterator_t), (uint8_t*)*iter) ;
      *iter = 0 ;

      err = MM_FREE(&objmem) ;
   }

   return err ;
}


typedef struct suffixtreeiterator_adapter_t  suffixtreeiterator_adapter_t ;

/* struct: suffixtreeiterator_adapter_it
 * Defines <typeadapt_t> to manage memory of <suffixtree_iterator_t>.
 * Used in <slist_t> (see <slist_IMPLEMENT_iterlist>) storing <suffixtree_iterator_t>. */
struct suffixtreeiterator_adapter_t {
   typeadapt_EMBED(suffixtreeiterator_adapter_t, suffixtree_iterator_t, void*) ;
} ;

/* function: freenode_suffixtreeiteratoradapter
 * Frees memory of object type <suffixtree_iterator_t>.
 * Used to adapt <suffixtreeiterator_list_t> to object type <suffixtree_iterator_t>. */
static int freenode_suffixtreeiteratoradapter(suffixtreeiterator_adapter_t * typeadp, suffixtree_iterator_t ** iter)
{
   (void) typeadp ;
   return delete_suffixtreeiterator(iter) ;
}


/* define: slist_IMPLEMENT_iterlist
 * Generates list managing <suffixtree_iterator_t> -- see <slist_IMPLEMENT>. */
slist_IMPLEMENT(_iterlist, suffixtree_iterator_t, next)

/* function: pushnew_iterlist
 * Creates new <suffixtree_iterator_t> and pushes onto <suffixtreeiterator_list_t>. */
static suffixtree_iterator_t * pushnew_iterlist(slist_t * stack)
{
   int err ;
   suffixtree_iterator_t * iter ;

   err = new_suffixtreeiterator(&iter) ;
   if (err) goto ONABORT ;
   iter->next = 0 ;

   if (0 != insertfirst_iterlist(stack, iter)) {
      delete_suffixtreeiterator(&iter) ;
      goto ONABORT ;
   }

   return iter ;
ONABORT:
   return 0 ;
}


/* struct: suffixtree_leaf_t
 * A node in <suffixtree_t> which has no childs.
 * A leaf indicates a position in the tree and contains a matching string.
 * The matching string extends alwayas until the end of the string for which
 * the suffix tree was built. A leaf is created for every suffix of
 * the input string for which the tree is built. Therefore the start address
 * of a leaf subtracted with the length of all strings of all nodes on the path
 * from root to leaf define the start address of a suffix.
 *
 * To distinguish between <suffixtree_node_t> and <suffixtree_leaf_t> bit 0x01 of <str_start> is used.
 *
 * Attention:
 * Do no forget to adapt <suffixtree_leaf_EMBED> after a change of sthis structure.
 * */
struct suffixtree_leaf_t {
   /* variable: next_child
    * Used to manage childs in a single linked list. */
   suffixtree_node_t * next_child ;
   /* variable: str_start
    * Start address of matched string. */
   const uint8_t     * str_start ;
   /* variable: str_size
    * Length in bytes of <str_start>.
    * This value can be 0 in case of an (end of input) marker. */
   size_t            str_size ;
} ;

// group: lifetime

/* function: new_suffixtreeleaf
 * Allocates new leaf of suffix tree. */
static inline int new_suffixtreeleaf(/*out*/suffixtree_leaf_t ** leaf)
{
   int err ;
   memblock_t objmem = memblock_INIT_FREEABLE ;

   err = MM_RESIZE(sizeof(suffixtree_leaf_t), &objmem) ;
   if (err) return err ;

   *leaf = (suffixtree_leaf_t*) objmem.addr ;
   return 0 ;
}

/* function: delete_suffixtreeleaf
 * Frees resources associated with <suffixtree_leaf_t>. */
static int delete_suffixtreeleaf(suffixtree_leaf_t ** leaf)
{
   int err = 0 ;

   if (*leaf) {
      memblock_t objmem = memblock_INIT(sizeof(suffixtree_leaf_t), (uint8_t*)*leaf) ;
      *leaf = 0 ;

      err = MM_FREE(&objmem) ;
   }

   return err ;
}

// group: generic

/* define: suffixtree_leaf_EMBED
 * Allows to embed all data fields of <suffixtree_leaf_t> into another structure. */
#define suffixtree_leaf_EMBED()     \
   suffixtree_node_t * next_child ; \
   const uint8_t     * str_start ;  \
   size_t            str_size       \

// group: query

/* define: ISLEAF
 * Returns true if parameter points to node of type <suffixtree_leaf_t>. */
#define ISLEAF(leaf)                      ((leaf)->str_size & 0x01)

/* define: STR_SIZE
 * Returns the size of the string this node or leaf matches. */
#define STR_SIZE(leaf)                    ((leaf)->str_size >> 1)

/* define: STR_START
 * Returns the start address of the string this node or leaf matches. */
#define STR_START(leaf)                   ((leaf)->str_start)

// group: change

/* define: INIT_AS_NODE
 * Inits the string description fields and sets the type to <suffixtree_node_t>. */
#define INIT_AS_NODE(leaf, _len, _start)  do { (leaf)->str_start = (_start) ; (leaf)->str_size = ((_len) << 1) ; } while (0)

/* define: INIT_AS_LEAF
 * Inits the string description fields and sets the type to <suffixtree_leaf_t>. */
#define INIT_AS_LEAF(leaf, _len, _start)  do { (leaf)->str_start = (_start) ; (leaf)->str_size = (0x01 | (_len) << 1) ; } while (0)

/* define: SKIP_STR_BYTES
 * Increments <suffixtree_leaf_t.str_start> and decrements <suffixtree_leaf_t.str_size> with _add. */
#define SKIP_STR_BYTES(leaf, _add)         do { (leaf)->str_start += (_add) ; (leaf)->str_size -= ((_add) << 1) ; } while (0)



/* struct: suffixtree_node_t
 * A node in <suffixtree_t> which has childs.
 * A node is also linked to a another node with its <suffix_link>.
 * A node indicates a position in the tree and contains a matching string.
 * If a node matches then its contained string is matched and also
 * all strings encountered along the path from root to this node.
 * Every node has at least two childs.
 * This ensures that <suffixtree_t> contains no more than (n-1) <suffixtree_node_t>
 * where n is the length of the input string for which the tree was built. */
struct suffixtree_node_t {
   /* variable: suffixtree_leaf_EMBED
    * Inherit all data fields from <suffixtree_leaf_t>. */
   suffixtree_leaf_EMBED() ;
   /* variable: childs
    * List of all children. */
   suffixtree_node_t * childs ;
   /* variable: suffix_link
    * Points to node which matches same string with this node except without the first character.
    * If this node matches (path from root to this node + contained string) string "c₀c₁...cₘ"
    * then the node suffix_lkinks points to matches "c₁...cₘ".
    * If this node is a child of root and matches only one character then suffix_link points to root and has
    * value NULL. */
   suffixtree_node_t * suffix_link ;
} ;

// group: lifetime

static inline int new_suffixtreenode(/*out*/suffixtree_node_t ** node)
{
   int err ;
   memblock_t objmem = memblock_INIT_FREEABLE ;

   err = MM_RESIZE(sizeof(suffixtree_node_t), &objmem) ;
   if (err) return err ;

   *node = (suffixtree_node_t*) objmem.addr ;
   return 0 ;
}

static inline int delete_suffixtreenode(suffixtree_node_t ** node)
{
   int err = 0 ;

   if (*node) {
      memblock_t objmem = memblock_INIT(sizeof(suffixtree_node_t), (uint8_t*)*node) ;
      *node = 0 ;

      err = MM_FREE(&objmem) ;
   }

   return err ;
}


/* struct: suffixtree_pos_t
 * Describes a position in <suffixtree_t>.
 * >      root
 * >       ↓
 * >      ...
 * >       ↓
 * >     parent
 * >       ↓
 * >      node    // matches matched_len characters
 * */
struct suffixtree_pos_t {
   /* variable: matched_len
    * The number of characters matched in node.
    * The following condition is always true:
    * > matched_len < (node ? STR_SIZE(node) : 1).
    * If (matched_len == 0) then node has fully matched and the processing step
    * assigns node to parent and a child of node to node. */
   size_t            matched_len ;
   /* variable: node
    * The current node indicates the position in the tree.
    * All parent nodes encountered has been matched fully and this node accounts for <matched_len> characters.
    * If this value is NULL it points to the root and <matched_len> is also 0. */
   suffixtree_node_t * node ;
   /* variable: parent
    * The parent of <node>. If <node> is NULL this value is also NULL and has no meaning. */
   suffixtree_node_t * parent ;
} ;

// group: lifetime

/* define: suffixtree_pos_INIT
 * Static initializer. */
#define suffixtree_pos_INIT            { 0, NULL, NULL }


/* struct: suffixtree_addstate_t
 * Holds the position in <suffixtree_t> and the next character to be added. */
struct suffixtree_addstate_t {
   /* variable: pos
    * Position in <suffixtree_t> where the next character is added. */
   suffixtree_pos_t  pos ;
   /* variable: suffix
    * The string which describes the current suffix.
    * The first character of the suffix is read suffix.addr[0] if (suffix.size > 0).
    * A special end-marker is also allowed with a suffix.size of 0.
    * This is translated internal into the character code 256 which does not exist in any string. */
   conststring_t     suffix ;
} ;

// group: lifetime

/* define: suffixtree_addstate_INIT
 * Static initializer. */
#define suffixtree_addstate_INIT(strsize, straddr)   { suffixtree_pos_INIT, conststring_INIT(strsize,straddr) }


// section: suffixtree_t

// group: description

/* about: Build a Suffix Tree
 *
 * A suffix tree is built from an input string and it stores all suffixes.
 *
 * The tree is initialized as empty where the root is denoted by "*".
 * > [0] empty tree
 * >     *
 *
 * Now we build a suffix from the input string "ccxccxccc".
 * It is built character by character starting from left until the end of string is reached.
 * In following diagram an arrow top to bottom is a pointer from parent to child.
 * A node matches its contained character. No two childs of the same parent can match
 * the same character.
 *
 * An end node matches a suffix. It is marked with a subscript ₀,₁,...
 * An input string of length n has n matching end nodes.
 * The value of the subscript is the start offset of a matched suffix.
 *
 * An arrow from child to parent is called the suffix link.
 * The suffix link of a node points to the node which matches the next substring
 * without the first character. For example the child
 *
 * A possible suffix tree may look like this:
 * > [1] add "ccxccxccc" │  [2] add "ccxccxccc" │  [3] add "ccxccxccc"
 * >          ^          │            ^         │             ^
 * >     *               │      *               │      * ──────╮
 * >     ↓               │      ↓               │      ↓       ↓
 * >     c₀              │   ╭> c₁              │   ╭> c ──╮ ╭>x₂
 * >                     │   │  ↓               │   │  ↓   ↓ │
 * >                     │   ╰─ c₀              │   ╰─ c ╭>x₁╯
 * >                     │                      │      ↓ │
 * >                     │                      │      x₀╯
 *
 * For example the child node "x₀" matches the string "ccx". Its suffix link points to "x₁" which matches "cx" the next
 * smaller suffix of "ccx" (without the first character). The suffix links which point to the root
 * (root matches empty string) are omitted.
 *
 * > [4] add "ccxccxccc" │  [5] add "ccxccxccc" │  [6] add "ccxccxccc"
 * >             ^       │               ^      │                ^
 * >      * ─────╮       │       * ──────╮      │       * ────────╮
 * >      ↓      ↓       │   ╭───↓───────↓──╮   │   ╭───↓─────────↓──╮
 * >  ╭>╭>c₃ ─╮╭>x       │   ╰>╭>c₃ ─╮╭─>x  │   │   ╰>╭>c ──╮ ╭─> x₅ │
 * >  │ │ ↓   ↓│ ↓       │     │ ↓   ↓│  ↓  │   │     │ ↓   ↓ │   ↓  │
 * >  │ ╰ c╭─>x╯ c₂<╮    │   ╭>╰ c₄╭>x╯  c<╮╯   │   ╭>╰ c ╭>x₃╯╭─>c ─╯
 * >  ╰───↓│──↓──╯  │    │   ╰───↓╭╯─↓───↓─│─╮  │   ╰───↓╭╯──↓─┼──↓──╮
 * >      x╯╭>c₁────╯    │       x╯╭>c╮╭>c₂┼─╯  │       x₄╭> c ╯╭>c──╯
 * >      ↓ │            │       ↓╭╯ ↓╰┼───╯    │       ↓╭╯  ↓ ╭╯ ↓
 * >      c₀╯            │       c╯╭>c₁╯        │       c╯╭> c ╯╭>x₂
 * >                     │       ↓ │            │       ↓ │  ↓ ╭╯
 * >                     │       c₀╯            │       c ╯╭>x₁╯
 * >                     │                      │       ↓  │
 * >                     │                      │       x₀ ╯
 *
 * As you can see the construction takes all end nodes with a subscript and append
 * a new node matching the next character. The subscript value is moved to the appended node.
 * If a child already exists with the same matching character this child becomes the
 * new end node with the subscript value.
 *
 * All nodes which matches a suffix (end nodes) lie on a (the outermost) suffix link chain as you can see in
 * the above diagram. So appending a new character becomes following the suffix link chain
 * which is O(i) for a suffix tree which matches i suffixes. To construct the whole tree for
 * input length n we need the time O(1+2+3+...+n) which is O(n*n).
 *
 * */

/* about: Optimize Tree Construction
 *
 * 1. Node matches x characters:
 * To be able to optimize the building process we introduce nodes which can store more than one character.
 * This allows us to forbid "characters chains" c->c->x where a node has only one child. Every node has therefore
 * at least two childs or it is a leaf (with no childs).
 *
 * The characters are not stored in the nodes only start and end pointers into to the input string.
 * Therefore creating a node is possible in constant time and not dependant on the length of contained characters.
 *
 * 2. End nodes become leaves and always match a whole suffix:
 * A leaf matches all characters from its first matching character (offset i) until the end of string
 * This allows us to ignore end nodes after creation except if we need to split them.
 * Also a leaf node does not have a suffix pointer cause you never follow it.
 *
 * 3. To understand the optimized algorithm from Ukkonen we need to understand that only
 * nodes (not leaves) of the unoptimized version needs to be updated. The optimized version
 * does no create a node for every single character. Therefore a node can implicitly be
 * represented as a prefix of length p from a node or leave which matches more than p characters.
 *
 * Such nodes (or leaves) must be split so that only the prefix becomes represented in a newly
 * created node and the remainder of the string is matched in the old (splitted) node.
 *
 * 4. Every added character now checks a node if either the next character of the matching string
 * in the node equals the new added character or if it has a child matching this character.
 * If not the node is split if necessary and a leave is added. Noew the next suffix is checked
 * by following the suffix link and the whole step is repeated until root is reached or a node
 * found which already has a child matching the added character.
 *
 * 5. Our tree only grows by a possible splitting of a node and creating a new leaf.
 * So having a leaf we never have to update it except if it has to be splitted and a prefix of
 * becomes a node. It suffices for the algorithm the remember only the last position (possibly root)
 * where it stopped and begin from this position again adding the following character.
 *
 * 6. For every node created through splitting its suffix pointer is computed.
 * Which is done in the next iteration of the loop in step 4.
 *
 * In the following diagram the sequence '--^' marks the position in the tree
 * where the next step will insert the next character.
 *
 *
 * > [1] add "ccxccxccc" │  [2] add "ccxccxccc" │  [3] add "ccxccxccc"
 * >          ^          │            ^         │             ^
 * >     *╮              │      *               │      *╮──────────╮
 * >   --^↓              │      ↓               │    --^↓          ↓
 * >      ccxccxccc₀     │      ccxccxccc₀      │    ╭─>c─────────╮xccxccc₂<╮
 * >                     │    --^               │    │  ↓         ↓         │
 * >                     │                      │    ╰  cxccxccc₀ xccxccc₁──╯
 *
 * The first step adds the whole string which is a suffix of itself as leaf to the tree.
 * Cause we have added it to the root we are ready. In the next step we add "c" which is
 * is the first character of the already added leaf and therefore we need to do nothing
 * except to move our position '--^' to this first character.
 * In the third step we add x. The following character of our current position is 'c' so
 * we have to split the leaf into node 'c' and leaf 'cxccxccc₀'. Then we add a new leaf
 * 'xccxccc₁' and follow the suffix link of the parent of node 'c' (node 'c' has an empty suffix link
 * after the split operation). The parent of 'c' is the root node so it has no suffix link and therefore
 * we skip the first character from our current position 'c' and follow root with '' which is root.
 * Then we check if root contains a child beginning with 'x'. It does not so we add a new leaf to root
 * (the same as before except it matches at position 2). The position is root so we are ready.
 *
 * > [4] add "ccxccxccc"      │  [5] add "ccxccxccc"     │  [6] add "ccxccxccc"
 * >             ^            │               ^          │                ^
 * >     *───────────╮        │      *──────────╮        │      *──────────╮
 * >     ↓           ↓        │      ↓          ↓        │      ↓          ↓
 * >     c╮─────────╮xccxccc₂ │      c─────────╮xccxccc₂ │      c─────────╮xccxccc₂
 * >   --^↓         ↓         │      ↓         ↓         │      ↓         ↓
 * >      cxccxccc₀ xccxccc₁  │      cxccxccc₀ xccxccc₁  │      cxccxccc₀ xccxccc₁
 * >                          │    --^                   │     --^
 *
 * The steps 4 up to 6 are simple they move only the position in the tree.
 *
 * > [7] add "ccxccxccc"      │  [8] add "ccxccxccc"     │  [9] add "ccxccxccc"
 * >                ^         │                  ^       │                   ^
 * >     *───────────╮        │      *──────────╮        │    *───────────────╮
 * >     ↓           ↓        │      ↓          ↓        │    ↓               ↓
 * >     c─────────╮ xccxccc₂ │      c─────────╮xccxccc₂ │    c───────╮  ╭──> xcc───╮
 * >     ↓         ↓          │      ↓         ↓         │    ↓       ↓ ╭╯    ↓     ↓
 * >     cxccxccc₀ xccxccc₁   │      cxccxccc₀ xccxccc₁  │ ╭──c───╮╭─>xcc───╮ xccc₂ c₅
 * >     --^                  │       --^                │ ↓--^╭──↓╯  ↓     ↓
 * >                          │                          │ xcc─╯─╮c₆  xccc₁ c₄
 * >                          │                          │ ↓     ↓
 * >                          │                          │ xccc₀ c₃
 *
 * Steps 7 and 8 only move the position in the tree. Step 9 is more interesting.
 * It is shown in the next diagrams splitted into 5 substeps.
 *
 * > [9.1] add "ccxccxccc"    │  [9.2] add "ccxccxccc"   │  [9.3] add "ccxccxccc"
 * >                    ^     │                     ^    │                     ^
 * >     *──────────╮         │      *──────────╮        │      *──────────────╮
 * >     ↓          ↓         │      ↓          ↓        │      ↓              ↓
 * >     c─────────╮xccxccc₂  │      c──────╮   xccxccc₂ │      c──────╮  ╭──> xcc───╮
 * >     ↓         ↓          │      ↓      ↓   --^      │      ↓  ╭─╮ ↓ ╭╯    ↓     ↓
 * >     cxcc──╮   xccxccc₁   │   ╭──cxcc──>xcc───╮      │   ╭─ cxcc ╰>xcc───╮ xccc₂ c₅
 * >     ↓     ↓   --^        │   ↓     ↓   ↓     ↓      │   ↓--^  ↓   ↓     ↓
 * >     xccc₀ c₃             │   xccc₀ c₃  xccc₁ c₄     │   xccc₀ c₃  xccc₁ c₄
 *
 * The first substep splits the node where the currrent position is set in step 8.
 * A new leaf is added. The suffix "cxcc" is followed which defines the new position in the tree.
 * Substep 2 splits the node the current position points to into node "xcc" and leave "xccc".
 * A new leaf is added to "xcc". The suffix "xcc" is followed which defines the new position in the tree.
 * Substep 3 splits the node the current position points to into node "xcc" and leave "xccc".
 * A new leaf is added to "xcc". Then the suffix pointer from the created in node in step 2
 * is set to the new created node. The suffix "cc" is followed which defines the new position in the tree.
 *
 * > [9.4] add "ccxccxccc"       │  [9.5] add "ccxccxccc"
 * >                    ^        │                     ^
 * >    *───────────────╮        │    *───────────────╮
 * >    ↓╭─────────────╮↓        │    ↓               ↓
 * >    c┼──────╮  ╭──>xcc ───╮  │  ╭>c───────╮  ╭──> xcc───╮
 * > --^↓↓      ↓ ╭╯    ↓     ↓  │  ╰╮↓       ↓ ╭╯    ↓     ↓
 * > ╭─ c───╮╭─>xcc───╮ xccc₂ c₅ │ ╭─╰c───╮╭─>xcc───╮ xccc₂ c₅
 * > ↓ ╭────↓╯  ↓     ↓          │ ↓--^╭──↓╯  ↓     ↓
 * > xcc───╮c₆  xccc₁ c₄         │ xcc─╯─╮c₆  xccc₁ c₄
 * > ↓     ↓                     │ ↓     ↓
 * > xccc₀ c₃                    │ xccc₀ c₃
 *
 * Substep 4 split node "cxcc" into nodes "c" and "xcc". A new leaf is added to "c" and the suffix pointer
 * from the node created in step 3 is set to point to node "c". The suffix "c" is followed which defines the new position
 * in the tree.
 * Substep 5 need not to split the node and checks its childs. A child with "c" exists so the alogithm ends here.
 * The suffix pointer from the node created in step 4 is set to point to "c".
 *
 * The algorithm has created only leaves 0-6. The leaves matching position 7 and 8 were not created.
 * To create them an explicit end-marker (empty string '') is added in step 10.
 *
 * > [10] add "ccxccxccc"
 * >                    ^
 * >           *───────────────╮
 * >        --^↓               ↓
 * > ╭───────╭>c───────╮  ╭──> xcc───╮
 * > ↓       ╰╮↓       ↓ ╭╯    ↓     ↓
 * > ''₈╭───╭─╰c───╮╭─>xcc───╮ xccc₂ c₅
 * >    ↓   ↓   ╭──↓╯  ↓     ↓
 * >    ''₇ xcc─╯─╮c₆  xccc₁ c₄
 * >        ↓     ↓
 * >       xccc₀ c₃
 *
 * The end-marker which never occurs in the string adds a new leave to the node pointed to by the current
 * position (see step 9.5). Then it follow the suffix "c" and adds another leave. The suffix "" is the
 * root but no end-marker is added to the root. Having reached the root ends the algorithm.
 *
 * */

// group: helper

/* function: compiletime_tests
 * Tests that <suffixtree_node_t> inherits from <suffixtree_leaf_t>. */
static inline void compiletime_tests(void)
{
   struct dummy_leaf_t {
      suffixtree_leaf_EMBED() ;
   } ;

   static_assert( 0 == &((suffixtree_node_t*)0)->next_child
                  && 0 == &((suffixtree_leaf_t*)0)->next_child
                  && &((suffixtree_node_t*)0)->str_start  == &((suffixtree_leaf_t*)0)->str_start
                  && &((suffixtree_node_t*)0)->str_size   == &((suffixtree_leaf_t*)0)->str_size
                  && sizeof(struct dummy_leaf_t) == sizeof(suffixtree_leaf_t)
                  , "suffixtree_node_t and suffixtree_leaf_t are compatible") ;
}

// group: lifetime

int init_suffixtree(/*out*/suffixtree_t * tree)
{
   tree->childs    = 0 ;
   tree->maxlength = 0 ;

   return 0 ;
}

int free_suffixtree(suffixtree_t * tree)
{
   int err ;

   err = clear_suffixtree(tree) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

// group: build

static int findchild_suffixtree(suffixtree_t * tree, suffixtree_node_t * parent, uint16_t c, /*out*/suffixtree_node_t ** child)
{
   suffixtree_node_t * childs = parent ? parent->childs : tree->childs ;
   for (; childs; childs = childs->next_child) {
      uint16_t c2 = (uint16_t) (STR_SIZE(childs) ? STR_START(childs)[0] : 256 /*end-marker*/) ;
      if (c == c2) {
         *child = childs ;
         return 0 ;
      }
   }

   return ESRCH ;
}

static int insertchild_suffixtree(suffixtree_t * tree, suffixtree_node_t * parent, suffixtree_node_t * node)
{
   suffixtree_node_t ** childs = parent ? &parent->childs : &tree->childs ;

   node->next_child = (*childs) ;
   *childs = node ;
   return 0 ;
}

/* function: replacechild_suffixtree
 * Replaces old_child of parent with new_child.
 *
 * Before return <suffixtree_node_t.next_child> of old_child is cleared. */
static int replacechild_suffixtree(suffixtree_t * tree, suffixtree_node_t * parent, suffixtree_node_t * const old_child,  suffixtree_node_t * const new_child)
{
   suffixtree_node_t ** childs = parent ? &parent->childs : &tree->childs ;
   suffixtree_node_t  * node   = *childs ;

   if (node == old_child) {
      *childs = new_child ;
   } else {
      while (node->next_child != old_child) {
         node = node->next_child ;
         if (!node) goto ONABORT ;
      }

      node->next_child = new_child ;
   }

   new_child->next_child = old_child->next_child ;
   old_child->next_child = 0 ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(EINVAL) ;
   return EINVAL ;
}

/* function: splitnode_suffixtree
 * The node in pos (see <suffixtree_pos_t.node>) is splitted into two nodes.
 * For example consider a node which matches the string "ab" and pos->matched_len == 1.
 * After split a newly created node which replaces the position of the old matches "a"
 * and its child which is the old node now matches "b".
 * Therfore suffix pointers to node are valid furthermore.
 *
 * (unchecked) Preconditions:
 * 1. - pos->matched_len >= 1
 * 2. - STR_LEN(pos->node) > pos->matched_len
 *
 * The preconditions ensure that after the split both nodes match a string of length >= 1.
 * */
static int splitnode_suffixtree(suffixtree_t * tree, suffixtree_pos_t * pos)
{
   int err ;
   suffixtree_node_t * node ;

   err = new_suffixtreenode(&node) ;
   if (err) goto ONABORT ;

   INIT_AS_NODE(node, pos->matched_len, STR_START(pos->node)) ;
   node->childs      = pos->node ;
   node->suffix_link = 0 ;

   err = replacechild_suffixtree(tree, pos->parent, /*old_child*/pos->node, /*new_child*/node) ;
   if (err) {
      delete_suffixtreenode(&node) ;
      goto ONABORT ;
   }

   SKIP_STR_BYTES(pos->node, pos->matched_len) ;
   pos->matched_len = 0 ; // matched fully
   pos->node        = node ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

/* function: addchar_suffixtree
 * This function process the next input character in state->suffix.
 * See <suffixtree_t.Optimize Tree Construction> for a description of the algorithm
 * and <build_suffixtree> for how this function is driven.
 *
 * INFO: extension needed for adding multiple strings
 * ========
 * node and leaf are used interchangeable.
 * This makes no problem cause a suffix will never match fully a leaf.
 * But if more than one string is added to suffix tree then this could become a problem !
 * The reason is that a string previously added could be a prefix of any later added string.
 * So leaves could become parents which is an error.
 * Before this could happen they must be splitted and converted to a node and an empty leaf (end marker) !!
 * This INFO is only useful if adding more than one string will be supported by some future changes.
 * ========
 * */
static int addchar_suffixtree(suffixtree_t * tree, suffixtree_addstate_t * state)
{
   int err ;

   suffixtree_node_t dummy_node ;
   suffixtree_node_t * last_created_node = &dummy_node ;

   uint16_t next_char = (uint16_t) (state->suffix.size ? state->suffix.addr[0] : 256 /*end-marker*/) ;

   for (;;) {

      if (! state->pos.matched_len) {
         // full node matched (root always fully matched in case node == 0) => search next child
         state->pos.parent = state->pos.node ;
         last_created_node->suffix_link = state->pos.node ;
         last_created_node = &dummy_node ;
         if (0 == findchild_suffixtree(tree, state->pos.node, next_char, &state->pos.node)) {
            // check if full node matched => the next time search child
            state->pos.matched_len = (1 < STR_SIZE(state->pos.node)) ;  /* INFO: applies here (1) */
            break ; // done
         }

         // no child ! => create leaf
         // parent is wrong ==> clear it; this works cause:
         // (state->pos.node->suffix_link != NULL) || (STR_SIZE(state->pos.node) == 1 && _parent_of_(state->pos.node) == NULL)
         state->pos.parent = 0 ;

      } else {
         // pos.node matches matched_len characters, now try next character of node
         if (next_char == STR_START(state->pos.node)[state->pos.matched_len]) {
            ++ state->pos.matched_len ;
            if (state->pos.matched_len == STR_SIZE(state->pos.node)) {
               // full node matched => the next time search child
               state->pos.matched_len = 0 ;  /* INFO: applies here (2) */
            }
            break ; // done
         }

         // next character does not match => split node + create leaf
         err = splitnode_suffixtree(tree, &state->pos) ;
         if (err) goto ONABORT ;
         // if a previous split goes into this else part
         // this code to set the suffix pointer is always reached
         // cause the previous split node has at least two different childs so this node must also be split
         last_created_node->suffix_link = state->pos.node ;
         last_created_node = state->pos.node ;
      }
      // assert(state->pos.matched_len == 0) ;

      // create leaf (except if _is_root_ && _is_end-marker)
      if (state->pos.node || state->suffix.size) {
         suffixtree_leaf_t * leaf ;
         err = new_suffixtreeleaf(&leaf) ;
         if (err) goto ONABORT ;
         INIT_AS_LEAF(leaf, state->suffix.size, state->suffix.addr) ;
         err = insertchild_suffixtree(tree, state->pos.node, (suffixtree_node_t*)leaf) ;
         if (err) {
            delete_suffixtreeleaf(&leaf) ;
            goto ONABORT ;
         }
      }

      // goto next suffix and repeat the whole loop
      // except we have already reached root

      if (0 == state->pos.node) {   // reached root ?
         break ; // done
      }

      if (state->pos.node->suffix_link) {
         state->pos.node = state->pos.node->suffix_link ;
         // state->pos.parent is wrong now but is not needed
         // (state->pos.matched_len == 0) ==> the next iteration overwrites state->pos.parent with state->pos.node.

      } else {
         conststring_t match = (conststring_t) conststring_INIT(STR_SIZE(state->pos.node), STR_START(state->pos.node)) ;

         if (state->pos.parent) {
            state->pos.parent = state->pos.parent->suffix_link ; // skipped first character
         } else {
            // search from root
            skipbyte_string(&match) ;  // build suffix
            if (isempty_string(&match)) {
               state->pos.node = 0 ;
               continue ;
            }
         }

         // go down tree until match.size bytes matched (if no child exists ! internal error !)
         for (;;) {
            err = findchild_suffixtree(tree, state->pos.parent, match.addr[0], &state->pos.node) ;
            if (err) goto ONABORT ;

            size_t nodelen = STR_SIZE(state->pos.node) ;
            if (  ISLEAF(state->pos.node) /* INFO: applies here (3) */
                  ||  nodelen > match.size) {
               // in the middle of a node
               state->pos.matched_len = match.size ;
               break ;
            } else if (nodelen == match.size) {
               // matched until end of node (state->pos.matched_len == 0)
               break ; // done
            } else {
               // step down
               skipbytes_string(&match, nodelen) ;
               state->pos.parent = state->pos.node ;
            }
         }

      }
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int build_suffixtree(suffixtree_t * tree, size_t length, const uint8_t input_string[length])
{
   int err ;

   VALIDATE_INPARAM_TEST(  input_string
                           && length
                           /* One bit in <suffixtree_lead_t.str_size> is reserved
                            * (to discriminate between type LEAF or NODE). */
                           && length <= ((size_t)-1)/2, ONABORT, ) ;

   err = clear_suffixtree(tree) ;
   if (err) goto ONABORT ;

   suffixtree_addstate_t addstate = suffixtree_addstate_INIT(length, input_string) ;

   while (!isempty_string(&addstate.suffix)) {

      err = addchar_suffixtree(tree, &addstate) ;
      if (err) goto ONABORT ;

      skipbyte_string(&addstate.suffix) ;
   }

   if (addstate.pos.node) {
      // add (end-marker) so all positions of suffixes are remembered in leaf nodes
      err = addchar_suffixtree(tree, &addstate) ;
      if (err) goto ONABORT ;
   }

   tree->maxlength = length ;

   return 0 ;
ONABORT:
   clear_suffixtree(tree) ;
   TRACEABORT_LOG(err) ;
   return err ;
}

int clear_suffixtree(suffixtree_t * tree)
{
   int err = 0 ;
   int err2 ;
   suffixtree_node_t ** childs = &tree->childs ;
   suffixtree_node_t *  parent = 0 ;
   suffixtree_node_t * node ;

   for (;;) {
      if (!*childs) {
         if (!parent) break ; // freed all childs of root

         node = parent ;
         parent = parent->suffix_link ;
         if (parent) {
            childs = &parent->childs ;
         } else {
            childs = &tree->childs ;
         }
         err2 = delete_suffixtreenode(&node) ;
         if (err2) err = err2 ;
      } else {
         node = *childs ;
         *childs = node->next_child ;
         if (ISLEAF(node)) {
            err2 = delete_suffixtreeleaf((suffixtree_leaf_t**)&node) ;
            if (err2) err = err2 ;
         } else {
            // step down
            node->suffix_link = parent ;
            childs = &node->childs ;
            parent = node ;
         }
      }
   }

   tree->maxlength = 0 ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

// group: query

static int dumpnode_stree(suffixtree_t * tree, cstring_t * cstr, suffixtree_node_t * parent, suffixtree_node_t * node)
{
   if (!node || !ISLEAF(node)) {
      suffixtree_node_t * childs = node ? node->childs : tree->childs ;
      if (node) {
         size_t maxlen = (20 < STR_SIZE(node) ? 20 : STR_SIZE(node)) ;
         printfappend_cstring(cstr, "node(%"PRIxPTR"): '%.*s'\n", (uintptr_t)node, maxlen, STR_START(node)) ;
         printfappend_cstring(cstr, " suffix->node(%"PRIxPTR"), parent->node(%"PRIxPTR"), childs:\n", (uintptr_t)node->suffix_link, (uintptr_t)parent) ;
      } else {
         printfappend_cstring(cstr, "node(0):\n childs:\n") ;
      }
      for (suffixtree_node_t * child = childs; child; child = child->next_child) {
         if (ISLEAF(child)) {
            size_t maxlen = (20 < STR_SIZE(child) ? 20 : STR_SIZE(child)) ;
            if (maxlen) printfappend_cstring(cstr, " %c -> leaf: '%.*s'\n", *STR_START(child), maxlen, STR_START(child)) ;
            else        printfappend_cstring(cstr, " ::-> leaf: ''\n") ;
         } else {
            printfappend_cstring(cstr, " %c -> node(%"PRIxPTR")\n", *STR_START(child), (uintptr_t)child) ;
         }
      }
      for (suffixtree_node_t * child = childs; child; child = child->next_child) {
         if (!ISLEAF(child)) {
            dumpnode_stree(tree, cstr, node, child) ;
         }
      }
   }

   return 0 ;
}

int dump_suffixtree(suffixtree_t * tree, cstring_t * cstr)
{
   int err ;

   clear_cstring(cstr) ;

   err = dumpnode_stree(tree, cstr, 0, 0) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

/* function: findstring_suffixtree
 * Searches beginning from root the node which searchstring.
 * The out parameter pos contains after successul return the
 * found node, its parent, and the number of bytes which are
 * matched from the beginning of the string in node.
 *
 * Nodes can match a string of characters. Therefore
 * the returned number in pos->matched_len (see <suffixtree_pos_t.matched_len>)
 * gives the number of matched characters (number of bytes) matched from the
 * returned node - its value is always > 0.
 *
 * */
static int findstring_suffixtree(
   suffixtree_t   * tree,
   conststring_t  * searchstring,
   /*out*/suffixtree_pos_t * pos)
{
   int err ;
   // conststring_t     match = (conststring_t) conststring_INIT(length, searchstring) ;
   suffixtree_node_t * parent = 0/*root*/ ;
   suffixtree_node_t * node ;

   if (  isempty_string(searchstring)
         || searchstring->size > tree->maxlength) {
      return ESRCH ;
   }

   for (;;) {

      err = findchild_suffixtree(tree, parent, searchstring->addr[0], &node) ;
      if (err) return err ;   // not found

      size_t node_len = STR_SIZE(node) ;
      if (node_len >= searchstring->size) {
         if (strncmp((const char*)searchstring->addr, (const char*)STR_START(node), searchstring->size)) {
            return ESRCH ; // not found
         }
         break ; // found
      } else if (ISLEAF(node)) {
         return ESRCH ; // not found
      } else if (node_len > 1) {
         if (strncmp((const char*)searchstring->addr, (const char*)STR_START(node), node_len)) {
            return ESRCH ; // not found
         }
      }

      skipbytes_string(searchstring, node_len) ;
      parent = node ;
   }

   pos->matched_len = searchstring->size ;
   pos->node        = node ;
   pos->parent      = parent ;
   return 0 ;
}

bool isstring_suffixtree(suffixtree_t * tree, size_t length, const uint8_t searchstr[length])
{
   bool isStrContained ;

   suffixtree_pos_t pos ;
   conststring_t    searchstring = conststring_INIT(length, searchstr);
   isStrContained = (0 == findstring_suffixtree(tree, &searchstring, &pos)) ;

   return isStrContained ;
}

int matchall_suffixtree(
   suffixtree_t   * tree,
   size_t         length,
   const uint8_t  searchstr[length],
   size_t         skip_count,
   /*out*/size_t  * matched_count,
   size_t         maxmatchcount,
   /*out*/const uint8_t * matchedpos[maxmatchcount])
{
   int err ;
   suffixtreeiterator_adapter_t typeadapt = typeadapt_INIT_LIFETIME(0, &freenode_suffixtreeiteratoradapter) ;
   typeadapt_member_t           nodeadapt = typeadapt_member_INIT((typeadapt_t*)&typeadapt, offsetof(suffixtree_iterator_t, next)) ;
   suffixtree_node_t * leaf = 0 ;
   suffixtree_node_t * node = 0 ;
   slist_t  posstack = slist_INIT ;
   size_t   prefixlen ;

   {  /* find position in tree */
      suffixtree_pos_t pos ;
      conststring_t    searchstring = conststring_INIT(length, searchstr);
      err = findstring_suffixtree(tree, &searchstring, &pos) ;
      if (err) return err ;

      /* calculate prefixlen and check if node is a leaf */
      prefixlen = length ;
      if (pos.matched_len) {
         if (ISLEAF(pos.node)) {
            leaf = pos.node ;
            prefixlen -= pos.matched_len ;
         } else {
            node = pos.node ;
            prefixlen += STR_SIZE(pos.node) - pos.matched_len ;
         }
      }
   }

   /* find all positions */
   size_t leaf_count = 0 ;

   for (;;) {
      if (leaf) {
         /* found leaf */
         if (  skip_count <= leaf_count
               && (leaf_count - skip_count) < maxmatchcount) {
            const uint8_t * match_strstart = STR_START(leaf) - prefixlen ;
            matchedpos[leaf_count - skip_count] = match_strstart ;
         }
         ++ leaf_count ;
      } else {
         /* step down the tree */
         suffixtree_iterator_t * nextpos = pushnew_iterlist(&posstack) ;
         if (!nextpos) {
            err = ENOMEM ;
            goto ONABORT ;
         }
         nextpos->prefixlen  = prefixlen ;
         nextpos->next_child = node->childs ;
      }

      for (;;) {
         suffixtree_iterator_t * nextpos = first_iterlist(&posstack) ;
         if (!nextpos) {
            node = 0 ;
            break ;
         }
         node = nextpos->next_child ;
         if (!node) {
            removefirst_iterlist(&posstack, &nextpos) ;
            err = delete_suffixtreeiterator(&nextpos) ;
            if (err) goto ONABORT ;
         } else {
            prefixlen = nextpos->prefixlen ;
            nextpos->next_child = node->next_child ;
            break ;
         }
      }

      if (!node) break ; // whole tree searched

      if (ISLEAF(node)) {
         leaf = node ;
      } else {
         leaf = 0 ;
         prefixlen += STR_SIZE(node) ;
      }

   }

   *matched_count = leaf_count ;
   err = free_iterlist(&posstack, &nodeadapt) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   (void) free_iterlist(&posstack, &nodeadapt) ;
   TRACEABORT_LOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int compare_size_f(const void * number1, const void * number2)
{
   return (int)*(const size_t*)number1 - (int)*(const size_t*)number2 ;
}

static int test_initfree(void)
{
   suffixtree_t   tree = suffixtree_INIT_FREEABLE ;
   const uint8_t  * teststr ;

   // TEST suffixtree_INIT_FREEABLE
   TEST(0 == tree.childs) ;
   TEST(0 == tree.maxlength) ;

   // TEST init_suffixtree, double free_suffixtree
   TEST(0 == init_suffixtree(&tree)) ;
   TEST(0 == tree.childs) ;
   TEST(0 == tree.maxlength) ;
   teststr = (const uint8_t*)"12345" ;
   TEST(0 == build_suffixtree(&tree, strlen((const char*)teststr), teststr)) ;
   TEST(0 != tree.childs) ;
   TEST(5 == tree.maxlength) ;
   TEST(0 == free_suffixtree(&tree)) ;
   TEST(0 == tree.childs) ;
   TEST(0 == tree.maxlength) ;
   TEST(0 == free_suffixtree(&tree)) ;
   TEST(0 == tree.childs) ;
   TEST(0 == tree.maxlength) ;

   return 0 ;
ONABORT:
   free_suffixtree(&tree) ;
   return EINVAL ;
}

static int test_suffixtree(void)
{
   suffixtree_t      tree = suffixtree_INIT_FREEABLE ;
   suffixtree_pos_t  found_pos ;
   suffixtree_node_t * found_node ;
   const uint8_t     * teststr ;

   // TEST simple suffix tree (only explicit nodes, no splitting)
   TEST(0 == init_suffixtree(&tree)) ;
   teststr = (const uint8_t*)"12345" ;
   TEST(0 == build_suffixtree(&tree, strlen((const char*)teststr), teststr)) ;
   TEST(0 == findchild_suffixtree(&tree, (void*)0/*root*/, '1', &found_node)) ;
   TEST(ISLEAF(found_node)) ;
   TEST(0 == STR_START(found_node) - teststr) ;
   TEST(5 == STR_SIZE(found_node)) ;
   TEST(0 == findchild_suffixtree(&tree, (void*)0/*root*/, '2', &found_node)) ;
   TEST(ISLEAF(found_node)) ;
   TEST(1 == STR_START(found_node) - teststr) ;
   TEST(4 == STR_SIZE(found_node)) ;
   TEST(0 == findchild_suffixtree(&tree, (void*)0/*root*/, '3', &found_node)) ;
   TEST(ISLEAF(found_node)) ;
   TEST(2 == STR_START(found_node) - teststr) ;
   TEST(3 == STR_SIZE(found_node)) ;
   TEST(0 == findchild_suffixtree(&tree, (void*)0/*root*/, '4', &found_node)) ;
   TEST(ISLEAF(found_node)) ;
   TEST(3 == STR_START(found_node) - teststr) ;
   TEST(2 == STR_SIZE(found_node)) ;
   TEST(0 == findchild_suffixtree(&tree, (void*)0/*root*/, '5', &found_node)) ;
   TEST(ISLEAF(found_node)) ;
   TEST(4 == STR_START(found_node) - teststr) ;
   TEST(1 == STR_SIZE(found_node)) ;

   // TEST suffix tree with splitting
   teststr = (const uint8_t*)"ABAC" ;
   TEST(0 == build_suffixtree(&tree, strlen((const char*)teststr), teststr)) ;
   TEST(0 == findchild_suffixtree(&tree, (void*)0/*root*/, 'A', &found_node)) ;
   TEST(!ISLEAF(found_node)) ;
   TEST(0 == STR_START(found_node) - teststr) ;
   TEST(1 == STR_SIZE(found_node)) ;
   TEST(found_node->suffix_link == 0) ;
   TEST(0 == findchild_suffixtree(&tree, found_node/*A*/, 'C', &found_node)) ;
   TEST(ISLEAF(found_node)) ;
   TEST(3 == STR_START(found_node) - teststr) ;
   TEST(0 == findchild_suffixtree(&tree, (void*)0/*root*/, 'B', &found_node)) ;
   TEST(ISLEAF(found_node)) ;
   TEST(1 == STR_START(found_node) - teststr) ;
   TEST(3 == STR_SIZE(found_node)) ;
   TEST(0 == findchild_suffixtree(&tree, (void*)0/*root*/, 'C', &found_node)) ;
   TEST(ISLEAF(found_node)) ;
   TEST(3 == STR_START(found_node) - teststr) ;
   TEST(1 == STR_SIZE(found_node)) ;

   // TEST suffix tree with splitting
   teststr = (const uint8_t*)"ABABCABCD" ;
   suffixtree_node_t * node_B ;
   suffixtree_node_t * node_C ;
   suffixtree_node_t * node_BC ;
   TEST(0 == build_suffixtree(&tree, strlen((const char*)teststr), teststr)) ;
   TEST(0 == findchild_suffixtree(&tree, (void*)0/*root*/, 'C', &found_node)) ;
   TEST(4 == STR_START(found_node) - teststr) ;
   TEST(1 == STR_SIZE(found_node)) ;
   TEST(found_node->suffix_link == 0) ;
   node_C = found_node ;
   TEST(0 == findchild_suffixtree(&tree, (void*)0/*root*/, 'B', &found_node)) ;
   TEST(1 == STR_START(found_node) - teststr) ;
   TEST(1 == STR_SIZE(found_node)) ;
   TEST(found_node->suffix_link == 0) ;
   node_B = found_node ;
   TEST(0 == findchild_suffixtree(&tree, node_B/*B*/, 'C', &found_node)) ;
   TEST(4 == STR_START(found_node) - teststr) ;
   TEST(1 == STR_SIZE(found_node)) ;
   TEST(found_node->suffix_link == node_C) ;
   node_BC = found_node ;
   TEST(0 == findchild_suffixtree(&tree, (void*)0/*root*/, 'A', &found_node)) ;
   TEST(0 == STR_START(found_node) - teststr) ;
   TEST(2 == STR_SIZE(found_node)) ;
   TEST(found_node->suffix_link == node_B) ;
   TEST(0 == findchild_suffixtree(&tree, found_node/*AB*/, 'C', &found_node)) ;
   TEST(4 == STR_START(found_node) - teststr) ;
   TEST(1 == STR_SIZE(found_node)) ;
   TEST(found_node->suffix_link == node_BC) ;
   TEST(0 == findchild_suffixtree(&tree, found_node/*ABC*/, 'D', &found_node)) ;
   TEST(1 == ISLEAF(found_node)) ;
   TEST(8 == STR_START(found_node) - teststr) ;
   TEST(0 == findchild_suffixtree(&tree, node_BC/*BC*/, 'D', &found_node)) ;
   TEST(1 == ISLEAF(found_node)) ;
   TEST(8 == STR_START(found_node) - teststr) ;
   TEST(0 == findchild_suffixtree(&tree, (void*)0/*root*/, 'D', &found_node)) ;
   TEST(ISLEAF(found_node)) ;
   TEST(8 == STR_START(found_node) - teststr) ;

   // TEST suffix tree with splitting
   teststr = (const uint8_t*)"ABABCABCDCABCDX" ;
   TEST(0 == build_suffixtree(&tree, strlen((const char*)teststr), teststr)) ;
   TEST(0 == findstring_suffixtree(&tree, &(conststring_t)conststring_INIT(5, (const uint8_t*)"CABCD"), &found_pos)) ;
   TEST(5 == STR_START(found_pos.node) - teststr) ;
   TEST(4 == STR_SIZE(found_pos.node)) ;
   TEST(0 == findchild_suffixtree(&tree, found_pos.node/*CABCD*/, 'X', &found_node)) ;
   TEST(ISLEAF(found_node)) ;
   TEST(14 == STR_START(found_node) - teststr) ;
   TEST(0 == findstring_suffixtree(&tree, &(conststring_t)conststring_INIT(4, (const uint8_t*)"ABCD"), &found_pos)) ;
   TEST(8 == STR_START(found_pos.node) - teststr) ;
   TEST(1 == STR_SIZE(found_pos.node)) ;
   TEST(0 == findchild_suffixtree(&tree, found_pos.node/*ABCD*/, 'X', &found_node)) ;
   TEST(ISLEAF(found_node)) ;
   TEST(14 == STR_START(found_node) - teststr) ;

   // TEST suffix tree with splitting
   teststr = (const uint8_t*)"ABABCABCDCABCX" ;
   TEST(0 == build_suffixtree(&tree, strlen((const char*)teststr), teststr)) ;
   TEST(0 == findstring_suffixtree(&tree, &(conststring_t)conststring_INIT(4, (const uint8_t*)"CABC"), &found_pos)) ;
   TEST(5 == STR_START(found_pos.node) - teststr) ;
   TEST(3 == STR_SIZE(found_pos.node)) ;
   TEST(0 == findchild_suffixtree(&tree, found_pos.node/*CABC*/, 'X', &found_node)) ;
   TEST(1 == ISLEAF(found_node)) ;
   TEST(13 == STR_START(found_node) - teststr) ;
   TEST(0 == findstring_suffixtree(&tree, &(conststring_t)conststring_INIT(3, (const uint8_t*)"ABC"), &found_pos)) ;
   TEST(4 == STR_START(found_pos.node) - teststr) ;
   TEST(1 == STR_SIZE(found_pos.node)) ;
   TEST(0 == findchild_suffixtree(&tree, found_pos.node/*ABC*/, 'X', &found_node)) ;
   TEST(1 == ISLEAF(found_node)) ;
   TEST(13 == STR_START(found_node) - teststr) ;

   // TEST isstring_suffixtree
   const char * test_string[] = {
      "mississippi", "12345", "ABAC"
      , "ABABABC", "ABABCABC", "ABABCABCD"
      ,"ABABCABCDCABCDX", "rittrtrrirtritrx", "rittrtrriptrieptptriept*trx"
      , "trptiptrptx",
   } ;
   for (unsigned i = 0; i < nrelementsof(test_string); ++i) {
      const uint8_t  * text = (const uint8_t*) test_string[i] ;
      const size_t text_len = strlen(test_string[i]) ;
      TEST(0 == build_suffixtree(&tree, text_len, text)) ;
      TEST(true == isstring_suffixtree(&tree, text_len, text)) ;
      TEST(false == isstring_suffixtree(&tree, text_len+1, text)) ;
      for (size_t suffix = 1; suffix < text_len; ++suffix) {
         TEST(false == isstring_suffixtree(&tree, text_len-suffix+1, text+suffix)) ;
         for (size_t substr_len = text_len-suffix; substr_len; --substr_len) {
            TEST(true == isstring_suffixtree(&tree, substr_len, text+suffix)) ;
         }
      }
   }

   // TEST matchall
   size_t matched_count ;
   const uint8_t * matched_pos[30] = {0} ;
   for (unsigned i = 0; i < nrelementsof(test_string); ++i) {
      const uint8_t  * text = (const uint8_t*) test_string[i] ;
      const size_t text_len = strlen(test_string[i]) ;
      TEST(0 == build_suffixtree(&tree, text_len, text)) ;
      TEST(ESRCH == matchall_suffixtree(&tree, text_len+1, text, 0, &matched_count, 10, matched_pos)) ;
      for (size_t suffix = 0; suffix < text_len; ++suffix) {
         TEST(0 == matchall_suffixtree(&tree, text_len-suffix, text+suffix, 0, &matched_count, 10, matched_pos)) ;
         qsort(matched_pos, matched_count, sizeof(size_t), &compare_size_f) ;
         if (i==0 && suffix==10) {
            TEST(matched_count == 4) ;
            TEST(matched_pos[0] == &text[1]) ;
            TEST(matched_pos[1] == &text[4]) ;
            TEST(matched_pos[2] == &text[7]) ;
            TEST(matched_pos[3] == &text[10]) ;
         } else {
            TEST(matched_count == (size_t)(1+(i==4&&suffix>=5))) ;
            TEST(matched_pos[matched_count-1] == &text[suffix]) ;
         }
      }
   }

   // TEST matchall "A"
   teststr = (const uint8_t*)"A" ;
   TEST(0 == build_suffixtree(&tree, strlen((const char*)teststr), teststr)) ;
   TEST(0 == build_suffixtree(&tree, 1, (const uint8_t*)"A")) ;
   TEST(0 == matchall_suffixtree(&tree, 1, (const uint8_t*)"A", 0, &matched_count, 10, matched_pos))
   TEST(1 == matched_count) ;
   TEST(0 == matched_pos[0] - teststr) ;

   // TEST matchall "AA" needs special end-marker (256) to make it a leaf
   teststr = (const uint8_t*)"AAAAA" ;
   TEST(0 == build_suffixtree(&tree, strlen((const char*)teststr), teststr)) ;
   TEST(0 == matchall_suffixtree(&tree, 1, (const uint8_t*)"A", 0, &matched_count, 10, matched_pos))
   TEST(5 == matched_count) ;
   qsort(matched_pos, matched_count, sizeof(size_t), &compare_size_f) ;
   for (unsigned i = 0; i < matched_count; ++i) TEST(&teststr[i] == matched_pos[i]) ;
   TEST(0 == matchall_suffixtree(&tree, 4, (const uint8_t*)"AAAA", 0, &matched_count, 10, matched_pos))
   TEST(2 == matched_count) ;
   qsort(matched_pos, matched_count, sizeof(size_t), &compare_size_f) ;
   for (unsigned i = 0; i < matched_count; ++i) TEST(&teststr[i] == matched_pos[i]) ;
   TEST(0 == matchall_suffixtree(&tree, 5, (const uint8_t*)"AAAAA", 0, &matched_count, 10, matched_pos))
   TEST(1 == matched_count) ;
   TEST(0 == matched_pos[0] - teststr) ;

   // TEST matchall "CXCXCXZXCYCXCY"
   teststr = (const uint8_t*)"CXCXCXZXCYCXCY" ;
   TEST(0 == build_suffixtree(&tree, strlen((const char*)teststr), teststr)) ;
   TEST(0 == matchall_suffixtree(&tree, 3, (const uint8_t*)"XCY", 0, &matched_count, 10, matched_pos))
   TEST(2 == matched_count) ;
   qsort(matched_pos, matched_count, sizeof(size_t), &compare_size_f) ;
   TEST(&teststr[7] == matched_pos[0] && &teststr[11] == matched_pos[1]) ;
   TEST(0 == matchall_suffixtree(&tree, 2, (const uint8_t*)"CX", 0, &matched_count, 10, matched_pos))
   TEST(4 == matched_count) ;
   qsort(matched_pos, matched_count, sizeof(size_t), &compare_size_f) ;
   TEST(&teststr[0] == matched_pos[0] && &teststr[2] == matched_pos[1]) ;
   TEST(&teststr[4] == matched_pos[2] && &teststr[10] == matched_pos[3]) ;

   // TEST matchall "ccxccxccc"
   teststr = (const uint8_t*)"ccxccxccc" ;
   TEST(0 == build_suffixtree(&tree, strlen((const char*)teststr), teststr)) ;
   TEST(0 == matchall_suffixtree(&tree, 3, (const uint8_t*)"ccc", 0, &matched_count, 10, matched_pos))
   TEST(1 == matched_count) ;
   TEST(&teststr[6] == matched_pos[0]) ;
   TEST(0 == matchall_suffixtree(&tree, 2, (const uint8_t*)"cc", 0, &matched_count, 10, matched_pos))
   TEST(4 == matched_count) ;
   qsort(matched_pos, 4, sizeof(size_t), &compare_size_f) ;
   TEST(&teststr[0] == matched_pos[0] && &teststr[3] == matched_pos[1]) ;
   TEST(&teststr[6] == matched_pos[2] && &teststr[7] == matched_pos[3]) ;
   TEST(0 == matchall_suffixtree(&tree, 1, (const uint8_t*)"x", 0, &matched_count, 10, matched_pos))
   TEST(2 == matched_count) ;
   qsort(matched_pos, 4, sizeof(size_t), &compare_size_f) ;
   TEST(&teststr[2] == matched_pos[0] && &teststr[5] == matched_pos[1]) ;

   // TEST matchall "ABCABC"
   teststr = (const uint8_t*)"ABABABCABC" ;
   TEST(0 == build_suffixtree(&tree, strlen((const char*)teststr), teststr)) ;
   TEST(0 == matchall_suffixtree(&tree, 3, (const uint8_t*)"ABC", 0, &matched_count, 10, matched_pos))
   TEST(2 == matched_count) ;
   qsort(matched_pos, 2, sizeof(size_t), &compare_size_f) ;
   TEST(&teststr[4] == matched_pos[0] && &teststr[7] == matched_pos[1]) ;
   TEST(0 == matchall_suffixtree(&tree, 2, (const uint8_t*)"AB", 0, &matched_count, 10, matched_pos))
   TEST(4 == matched_count) ;
   qsort(matched_pos, matched_count, sizeof(size_t), &compare_size_f) ;
   TEST(&teststr[0] == matched_pos[0] && &teststr[2] == matched_pos[1]) ;
   TEST(&teststr[4] == matched_pos[2] && &teststr[7] == matched_pos[3]) ;
   TEST(0 == matchall_suffixtree(&tree, 4, (const uint8_t*)"ABAB", 0, &matched_count, 10, matched_pos))
   TEST(2 == matched_count) ;
   qsort(matched_pos, matched_count, sizeof(size_t), &compare_size_f) ;
   TEST(&teststr[0] == matched_pos[0] && &teststr[2] == matched_pos[1]) ;
   TEST(0 == matchall_suffixtree(&tree, 10, (const uint8_t*)"ABABABCABC", 0, &matched_count, 10, matched_pos))
   TEST(1 == matched_count) ;
   TEST(0 == matched_pos[0] - teststr) ;
   TEST(ESRCH == matchall_suffixtree(&tree, 11, (const uint8_t*)"ABABABCABC", 0, &matched_count, 10, matched_pos))

   // TEST matchall (ignore_count + maxmatchcount < matched_count)
   teststr = (const uint8_t*)"AAAAAAAAAAAAAAA" ;
   TEST(0 == build_suffixtree(&tree, strlen((const char*)teststr), teststr)) ;
   for (unsigned ignore_count = 0; ignore_count < 15; ++ignore_count) {
      const uint8_t * matched_pos_ref[15] ;
      TEST(0 == matchall_suffixtree(&tree, 1, (const uint8_t*)"A", 0, &matched_count, 15, matched_pos_ref))
      TEST(15 == matched_count) ;
      TEST(0 == matchall_suffixtree(&tree, 1, (const uint8_t*)"A", ignore_count, &matched_count, 10, matched_pos))
      TEST(15 == matched_count) ;
      unsigned matched_pos_size = ignore_count <= 5 ? 10 : 15 - ignore_count ;
      for (unsigned i = 0; i < matched_pos_size; ++i) TEST(matched_pos_ref[i+ignore_count] == matched_pos[i]) ;
   }

   // unprepare
   TEST(0 == free_suffixtree(&tree)) ;

   return 0 ;
ONABORT:
   free_suffixtree(&tree) ;
   return EINVAL ;
}

static int test_matchfile(void)
{
   suffixtree_t   tree          = suffixtree_INIT_FREEABLE ;
   mmfile_t       sourcefile    = mmfile_INIT_FREEABLE ;
   const uint8_t  * iterposstr  = (const uint8_t*) "suffixtree_iterator_t" ;
   // BUILD compare_pos[] with bash script (all in one line after the ">" prompt)
   /* > grep -ob suffixtree_iterator_t C-kern/ds/inmem/suffixtree.c |
    * > while read ; do echo -n "${REPLY%%:*}," ; x=${REPLY%suffixtree_iterator_t*}; x=${x#*:} ;
    * > if [ "${x/suffixtree_iterator_t/}" != "$x" ]; then i=$((${REPLY%%:*}+${#x})); echo -n "$i,"; fi; done ; echo */
   size_t         compare_pos[] = {1245,1284,1378,1405,2272,2567,2680,3121,3203,3324,3397,3526,3597,3711,3994,4081,4195,4312,4397,4510,4675,4754,4830,4908,4984,43162,44365,44643,60204,60328,60441,60496} ;
   const uint8_t  * matched_pos[1+nrelementsof(compare_pos)] ;
   size_t         matched_count ;
   const uint8_t  * teststring ;

   // prepare
   // open and read this source file
   TEST(0 == init_mmfile(&sourcefile, __FILE__, 0, 0, mmfile_openmode_RDONLY, 0)) ;
   uint8_t   * buffer = addr_mmfile(&sourcefile) ;
   size_t buffer_size = size_mmfile(&sourcefile) ;

   // TEST matchall_suffixtree with source file
   TEST(0 == init_suffixtree(&tree)) ;
   TEST(0 == build_suffixtree(&tree, buffer_size, buffer)) ;
   teststring = (const uint8_t*) "static inline int new_suffixtreeleaf(/*out*/suffixtree_leaf_t ** leaf)\n" ;
   TEST(1 == isstring_suffixtree(&tree, strlen((const char*)teststring), teststring)) ;
   teststring = (const uint8_t*) "xxx_special_xxx" ;
   TEST(0 == matchall_suffixtree(&tree, strlen((const char*)teststring), teststring, 0, &matched_count, 10, matched_pos)) ;
   TEST(1 == matched_count) ;
   teststring = iterposstr ;
   TEST(0 == matchall_suffixtree(&tree, strlen((const char*)teststring), teststring, 0, &matched_count, nrelementsof(matched_pos), matched_pos)) ;
   TEST(nrelementsof(compare_pos) == matched_count) ;
   qsort(matched_pos, matched_count, sizeof(size_t), &compare_size_f) ;
   for (unsigned i = 0; i < matched_count; ++i) {
      TEST(&buffer[compare_pos[i]] == matched_pos[i]) ;
   }

   // unprepare
   TEST(0 == free_mmfile(&sourcefile)) ;
   TEST(0 == free_suffixtree(&tree)) ;

   return 0 ;
ONABORT:
   free_mmfile(&sourcefile) ;
   free_suffixtree(&tree) ;
   return EINVAL ;
}

static int test_dump(void)
{
   suffixtree_t   tree = suffixtree_INIT_FREEABLE ;
   cstring_t      cstr = cstring_INIT ;
   suffixtree_node_t nodes[20] ;
   char           expectresult[600] ;
   const uint8_t  * teststr   = (const uint8_t*)"ABABAB" ;
   const size_t   len_teststr = strlen((const char*)teststr) ;

   // prepare
   memset(nodes, 0, sizeof(nodes)) ;
   INIT_AS_NODE(&nodes[0], 2, &teststr[0]) ;                // '<AB>ABAB'
   nodes[0].suffix_link = &nodes[1] ;
   INIT_AS_NODE(&nodes[1], 1, &teststr[1]) ;                // 'A<B>ABAB'
   TEST(0 == insertchild_suffixtree(&tree, 0, &nodes[0])) ;
   TEST(0 == insertchild_suffixtree(&tree, 0, &nodes[1])) ;
   INIT_AS_NODE(&nodes[2], 2, &teststr[2]) ;                // 'AB<AB>AB'
   nodes[2].suffix_link = &nodes[0] ;
   INIT_AS_LEAF(&nodes[3], 0, &teststr[len_teststr]) ;      // end-marker
   TEST(0 == insertchild_suffixtree(&tree, &nodes[1], &nodes[2])) ;
   TEST(0 == insertchild_suffixtree(&tree, &nodes[1], &nodes[3])) ;
   INIT_AS_LEAF(&nodes[4], 2, &teststr[len_teststr-2]) ;    // 'ABAB<AB>'
   INIT_AS_LEAF(&nodes[5], 0, &teststr[len_teststr]) ;      // end-marker
   TEST(0 == insertchild_suffixtree(&tree, &nodes[2], &nodes[4])) ;
   TEST(0 == insertchild_suffixtree(&tree, &nodes[2], &nodes[5])) ;
   INIT_AS_NODE(&nodes[6], 2, &teststr[2]) ;                // 'AB<AB>AB'
   nodes[6].suffix_link = &nodes[2] ;
   INIT_AS_LEAF(&nodes[7], 0, &teststr[len_teststr]) ;      // end-marker
   TEST(0 == insertchild_suffixtree(&tree, &nodes[0], &nodes[6])) ;
   TEST(0 == insertchild_suffixtree(&tree, &nodes[0], &nodes[7])) ;
   INIT_AS_LEAF(&nodes[8], 2, &teststr[len_teststr-2]) ;    // 'ABAB<AB>'
   INIT_AS_LEAF(&nodes[9], 0, &teststr[len_teststr]) ;      // end-marker
   TEST(0 == insertchild_suffixtree(&tree, &nodes[6], &nodes[8])) ;
   TEST(0 == insertchild_suffixtree(&tree, &nodes[6], &nodes[9])) ;

   snprintf(expectresult, sizeof(expectresult),
      "node(0):\n childs:\n B -> node(%"PRIxPTR")\n A -> node(%"PRIxPTR")\n"
      "node(%"PRIxPTR"): 'B'\n suffix->node(0), parent->node(0), childs:\n ::-> leaf: ''\n A -> node(%"PRIxPTR")\n"
      "node(%"PRIxPTR"): 'AB'\n suffix->node(%"PRIxPTR"), parent->node(%"PRIxPTR"), childs:\n ::-> leaf: ''\n A -> leaf: 'AB'\n"
      "node(%"PRIxPTR"): 'AB'\n suffix->node(%"PRIxPTR"), parent->node(0), childs:\n ::-> leaf: ''\n A -> node(%"PRIxPTR")\n"
      "node(%"PRIxPTR"): 'AB'\n suffix->node(%"PRIxPTR"), parent->node(%"PRIxPTR"), childs:\n ::-> leaf: ''\n A -> leaf: 'AB'\n",
      (uintptr_t)&nodes[1], (uintptr_t)&nodes[0],
      (uintptr_t)&nodes[1], (uintptr_t)&nodes[2],
      (uintptr_t)&nodes[2], (uintptr_t)&nodes[0], (uintptr_t)&nodes[1],
      (uintptr_t)&nodes[0], (uintptr_t)&nodes[1], (uintptr_t)&nodes[6],
      (uintptr_t)&nodes[6], (uintptr_t)&nodes[2], (uintptr_t)&nodes[0]
      ) ;

   TEST(0 == dump_suffixtree(&tree, &cstr)) ;
   TEST(0 == strcmp((const char*)str_cstring(&cstr), expectresult)) ;

   // unprepare
   TEST(0 == free_cstring(&cstr)) ;

   return 0 ;
ONABORT:
   free_cstring(&cstr) ;
   return EINVAL ;
}

int unittest_ds_inmem_suffixtree()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   if (test_dump())           goto ONABORT ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())       goto ONABORT ;
   if (test_suffixtree())     goto ONABORT ;
   if (test_matchfile())      goto ONABORT ;
   if (test_dump())           goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
