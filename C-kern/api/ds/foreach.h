/* title: Foreach-Iterator
   Allows to iterate over content of container
   with simple <foreach> macro.
   Using the iterator interface directly is also possible

   > int  init_containeriterator (container_iterator_t * iter, container_t * container) ;
   > int  free_containeriterator (container_iterator_t * iter) ;
   > bool next_containeriterator (container_iterator_t * iter, container_t * container, container_node_t ** node) ;

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

   file: /jsprojekt/JS/C-kern/api/ds/foreach.h
    Header file <Foreach-Iterator>.
*/
#ifndef CKERN_DS_FOREACH_HEADER
#define CKERN_DS_FOREACH_HEADER


// section: Functions

/* define: foreach
 * Iterates over all elements from a container.
 * During iteration do not change the content of the container
 * except if the iterator allows it.
 * It is possible to exit <foreach> loop by using *break*.
 * The initialized iterator is also freed in this case.
 * If you exit the loop by use of goto or any other exception handling
 * mechanism the iterator is *not* freed which may result in memory leaks.
 *
 * Parameter:
 * _fctsuffix - The suffix of the container interface functions.
 *              This name is used to access the container functions offering suuporting an iterator interface.
 * container  - Pointer to container which contains all elements.
 * varname    - The name of the variable which iterates over all contained elements.
 *
 * Explanation:
 * A container type which wants to offer <foreach> functionality must implement the
 * following iterator interface:
 *
 * (start code)
 * iterator_t * iteratortype##_fctsuffix (void) ;
 * node_t     * iteratedtype##_fctsuffix (void) ;
 * int  init##_fctsuffix##iterator (iterator_t * iter, container_t * container) ;
 * int  free##_fctsuffix##iterator (iterator_t * iter) ;
 * bool next##_fctsuffix##iterator (iterator_t * iter, container_t * container, node_t ** node) ;
 * (end code)
 *
 * Example implementation:
 * - <slist_iterator_t>    Type of iterator.
 * - <init_slistiterator>  Initializes <slist_iterator_t> and allocates necessary resources.
 * - <free_slistiterator>  Frees resources associated with <slist_iterator_t>. This is called
 *                         after leaving the foreach loop automatically with either break or
 *                         if there is no more element in the container.
 * - <next_slistiterator>  Returns next element stored in container.
 * - <iteratortype_slist>  Connects <slist_t> with function suffix "_slist" to <slist_iterator_t>.
 * - <iteratedtype_slist>  Type of element (<slist_node_t>*) which is returned from call <next_slistiterator>.
 * */
#define foreach(_fctsuffix, container, varname)                               \
   for( int varname ## _once_ = 1; varname ## _once_; varname ## _once_ = 0)  \
   for( typeof(* iteratortype ## _fctsuffix ()) varname ## _iter_ ;           \
        varname ## _once_ &&                                                  \
        (0 == init ## _fctsuffix ## iterator (                                \
                     &varname ## _iter_, (container))) ;                      \
        (void) free ## _fctsuffix ## iterator (                               \
                     &varname ## _iter_), varname ## _once_ = 0)              \
   for(  typeof(iteratedtype ## _fctsuffix ()) varname;                       \
         next ## _fctsuffix ## iterator (                                     \
                  &varname ## _iter_, (container), & varname); )


#endif
