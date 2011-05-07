/* title: Compare-Callback
   Type of callback which compares two tree nodes
   or a key with a tree node.

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

   file: /jsprojekt/JS/C-kern/api/aspect/callback/compare.h
    Header file of <Compare-Callback>.
*/
#ifndef CKERN_ASPECT_CALLBACK_COMPARE_HEADER
#define CKERN_ASPECT_CALLBACK_COMPARE_HEADER

// forward
struct callback_aspect_t ;

/* typedef: callback_compare_t typedef
 * Shortcut for <callback_compare_t>. */
typedef struct callback_compare_t   callback_compare_t ;

/* typedef: callback_compare_f
 * Function pointer to any callback which compares two elements.
 * The function should never fail.
 *
 * Returns:
 *  0 - The called function should return 0 in case left equals right element.
 * -1 - It should return -1 in case left is lesser then right element.
 * +1 - It should return +1 in case left is greater then right element. */
typedef int                      (* callback_compare_f ) (struct callback_aspect_t * cb, const void * left_elemnt, const void * right_elemnt) ;

/* define: callback_compare_ADAPT
 * Adapt types of callback parameter to more specific ones.
 * This macro constructs two types which are equivalent to
 * <callback_compare_f> and <callback_compare_t>.
 *
 * Parameter:
 * type_name   - Name prefix of constructed types. The name prefix is suffixed with either "_t" or "_f" to construct
 *               two types.
 * left_type   - The second parameter of the callback is constructed with type: (left_type *).
 * right_type  - The third parameter of the callback is constructed with type: (right_type *). */
#define callback_compare_ADAPT(type_name, left_type, right_type)  \
   typedef int (* type_name ## _f )    ( struct callback_aspect_t * cb, const left_type * left_elemnt, const right_type * right_elemnt) ; \
   typedef struct type_name ## _t      type_name ## _t ; \
   struct type_name ## _t { \
      type_name ## _f            fct ; \
      struct callback_aspect_t * cb_param ; \
   } ;

/* struct: callback_compare_t
 * Stores callback function and user param for comparing. */
struct callback_compare_t {
   /* variable: fct
    * The pointer to the called back function. */
   callback_compare_f         fct ;
   /* variable: cb_param
    * The pointer to the first parameter of the called back function. */
   struct callback_aspect_t * cb_param ;
} ;

#endif
