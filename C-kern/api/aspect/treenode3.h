/* title: Treenode3-Aspect
   Storage overhead needed to manage
   object nodes in a memory tree structure
   with 3 pointers.

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

   file: C-kern/api/aspect/treenode3.h
    Header file of <Treenode3-Aspect>.
*/
#ifndef CKERN_ASPECT_TREENODE3_HEADER
#define CKERN_ASPECT_TREENODE3_HEADER

/* typedef: treenode3_aspect_t typedef
 * Shortcut for <treenode3_aspect_t>. */
typedef struct treenode3_aspect_t   treenode3_aspect_t ;

/* struct: treenode3_aspect_t
 * Is used to reserve storage for a data structure which
 * allows for fast indexing of objects stored in memory.
 * An object must inherit from <treenode3_aspect_t>
 * to be manageable by a generic tree index. The relationship
 * between a node and its left or right subtree is determined
 * by the implementation. */
 struct treenode3_aspect_t {
   /* variable: left
    * Points to left subtree. If there is no subtree this value is set to NULL. */
   treenode3_aspect_t * left ;
   /* variable: left
    * Points to right subtree. If there is no subtree this value is set to NULL. */
   treenode3_aspect_t * right ;
   /* variable: parent
    * Points to parent node. If this is the root node there is no parent and
    * this value is set to NULL. */
   treenode3_aspect_t * parent ;
} ;

#endif
