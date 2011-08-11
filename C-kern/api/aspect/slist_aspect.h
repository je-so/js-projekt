/* title: SingleLinkedList-Aspect
   Storage overhead needed by objects which
   wants to be accessed by a single linked list.

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

   file: C-kern/api/aspect/slist_aspect.h
    Header file of <SingleLinkedList-Aspect>.
*/
#ifndef CKERN_ASPECT_SINGLELINKEDLIST_HEADER
#define CKERN_ASPECT_SINGLELINKEDLIST_HEADER

/* typedef: slist_aspect_t typedef
 * Shortcut for <slist_aspect_t>. */
typedef struct slist_aspect_t             slist_aspect_t ;

/* struct: slist_aspect_t
 * Is used to reserve storage in an object to be manageable by a single linked list data structure.
 * The list allows to access or search nodes in sequential order in O(n).
 * An object which wants to be member of a list must inherit from <slist_aspect_t>.
 * The relationship between a node and its left or right subtree is determined
 * by the implementation. */
 struct slist_aspect_t {
   /* variable: next
    * Points to next node in the list. If there is no next node this value is set to NULL.
    * It is possible that the last element points to the first element of the list
    * if the list is circularly linked. */
   slist_aspect_t  * next ;
} ;

// group: lifetime

/* define: slist_aspect_INIT
 * Static initializer: Sets next pointer to NULL.
 * Before inserting a new node do not forget to initialize the next pointer with NULL.
 *
 * Note:
 * The next pointer is checked against NULL in the precondition of every insert function
 * of every list implementation.
 * This ensures that a node is not inserted in more than one list by mistake. */
#define slist_aspect_INIT     { 0 }

// group: type adaption

/* define: slist_aspect_EMBED
 * Allows to embed content of <slist_aspect_t> into another structure.
 *
 * Parameter:
 * object_type - The type of object the list aspect should point to.
 * nameprefix  - All embedded fields are prefix with this name.
 *
 * Your object must inherit or embed <slist_aspect_t> to be manageable by a single linked list.
 * With the <slist_aspect_EMBED> macro you can do:
 * > struct object_t {
 * >    int          dummy ;
 * >    object_t   * nameprefix ## next ;
 * >    int          dummy2 ;
 * > }
 *
 * Instead of doing:
 * > struct object_t {
 * >    int             dummy ;
 * >    slist_aspect_t  listaspect ;
 * >    int             dummy2 ;
 * > } */
#define slist_aspect_EMBED(object_type,nameprefix) \
   struct_type * nameprefix ## next

#endif
