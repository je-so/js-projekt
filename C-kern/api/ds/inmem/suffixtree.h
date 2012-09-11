/* title: Suffix-Tree

   A suffix tree is a tree which stores all suffixes of a given string.
   For "ABABC" all suffixes are: ABABC,BABC,ABC,BC,C and the empty string.
   This implementation considers the empty string as no suffix in contrast to
   the standard definition.

   Time in O(n):
   The construction of a suffix tree runs in time O(n) for
   a given input string of length n.

   If you want to know if a certain substring of length s
   is contained in a suffix tree you can do so in time O(s).

   Reference:
   The algorithm for constructing a suffix tree is from E. Ukkonen (1995).
   See <paper at http://www.cs.helsinki.fi/u/ukkonen/SuffixT1withFigs.pdf> for
   a thorough description.

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
#ifndef CKERN_STRING_SUFFIX_TREE_HEADER
#define CKERN_STRING_SUFFIX_TREE_HEADER

// forward
struct cstring_t ;
struct suffixtree_node_t ;

/* typedef: struct suffixtree_t
 * Exports <suffixtree_t>. Implementation of a suffix tree. */
typedef struct suffixtree_t            suffixtree_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_ds_inmem_suffixtree
 * Test suffix tree with content if a C-source file. */
int unittest_ds_inmem_suffixtree(void) ;
#endif


/* struct: suffixtree_t
 * A suffix tree contains a set of connected nodes and a reference to the input text.
 * The root node is represented with the special value NULL.
 * Child nodes are managed by a single linked list. The first character of the substring of every child
 * is unique between the childs.
 *
 * This implementation considers the empty string as an invalid suffix in contrast to
 * the standard definition. To query for an empty string with <isstring_suffixtree> always returns false.
 * */
struct suffixtree_t {
   /* variable: childs
    * Points to root node with all childs of root node. */
   struct suffixtree_node_t   * childs ;

   /* variable: maxlength
    * Max length (in bytes) of all added strings. */
   size_t                     maxlength ;
} ;

// group: lifetime

/* define: suffixtree_INIT_FREEABLE
 * Static initializer. Sets all fields to 0. */
#define suffixtree_INIT_FREEABLE            { (void*)0, 0 }

/* fucntion: init_suffixtree
 * This function initializes the tree object with an empty tree. The adapter must live at
 * least as long as the tree object - only a reference to it is stored (no copy is made).*/
int init_suffixtree(/*out*/suffixtree_t * tree) ;

/* function: free_suffixtree
 * Frees all memory of the allocated nodes.
 * The input string can also be freed if it is no longer needed.
 * TODO: Implement free memory group - allow to mark the nodes as aprt of a group ! */
int free_suffixtree(suffixtree_t * tree) ;

// group: query

/* function: dump_suffixtree
 * Writes a simple ascii representation of all nodes into a <cstring_t>.
 * The first dumped node is root
 * > node(0):
 * >  childs:
 * >  A -> node(bffff17c)
 * >  B -> node(bffff168)
 * > node(bffff168): 'B'
 * > suffix->node(0), parent->node(0), childs:
 * >  ::-> leaf: ''
 * >  A -> node(bffff17c)"
 * >  C -> leaf: 'CXX'
 * > ...
 * A node has a number (its internal memory address). Root has number 0.
 * It matches also a string except root.
 * It has a list of childs indexed by the first matching character of the child.
 * Other nodes have also a suffix pointer and a parent pointer.
 * The suffix pointer points to the node which matches the suffix of this node.
 * The string a node matches are all string concatenated on path from root to this
 * node (including the node).
 *
 * A leaf is only listed as child of a node. It matches only a string.
 * A leaf marked with '::' is an end marker to store only positional information.
 * */
int dump_suffixtree(suffixtree_t * tree, struct cstring_t * cstr) ;

/* function: isstring_suffixtree
 * Returns true if at least one suffix begins with searchstr.
 * Use this function to check if a certain substring is contained in the string
 * for which this suffix tree was built.
 *
 * Returns:
 * true  - searched string is contained in input text
 * false - searched string is not contained in input text */
bool isstring_suffixtree(suffixtree_t * tree, size_t length, const uint8_t searchstr[length]) ;

/* function: matchall_suffixtree
 * Returns number of times and start addresses of suffixes which contain searchstr.
 * Use this function to search for all occurrences of a certain substring in the string
 * for which this suffix tree was built.
 *
 * The number of valid positions in array matchedpos can be computed with
 * > min((matched_count > skip_count ? matched_count-skip_count : 0), maxmatchcount)
 *
 * Parameter:
 * tree       - Suffix tree of the string which is scanned for searchstr.
 * length     - The length of searchstr.
 * searchstr  - The content of the search string.
 * skip_count - The first skip_count number of found occurrences are not returned in matchedpos array.
 * matched_count - Returns the number of all found occurrences. This number is independent of skip_count and maxmatchcount.
 *                 Set maxmatchcount to 0 to count all occurences without returning them.
 * maxmatchcount - Contains the size of the array matchedpos.
 * matchedpos - Returns the positions searchstr is found in the string suffix tree was built for.
 *              The number of positions returned is less or equal than maxmatchcount.
 *              The first skip_count positions are not contained.
 *
 * TODO: rewrite interface to use danymic array as result
 *       (implement it part static allocation + dynamic allocation; same as wbuffer, but possible mixed at the same time) !
 *
 * */
int matchall_suffixtree(suffixtree_t * tree, size_t length, const uint8_t searchstr[length], size_t skip_count, /*out*/size_t * matched_count, size_t maxmatchcount, /*out*/const uint8_t * matchedpos[maxmatchcount]) ;

// group: build

/* function: build_suffixtree
 * This function constructs a suffix tree from the given input string.
 * After return the caller is not allowed to free the text.
 * The input text must live at least as long as the constructed tree cause it stores
 * references to *input_string* to describe string contents instead of making copies.
 * Memory of any previously build tree is freed before a new one is built. */
int build_suffixtree(suffixtree_t * tree, size_t length, const uint8_t input_string[length]) ;

/* function: clear_suffixtree
 * All contained suffixes are removed and all internal memory is freed.
 * After return no more references to any added input strings are held so they can be safely freed. */
int clear_suffixtree(suffixtree_t * tree) ;



#endif
