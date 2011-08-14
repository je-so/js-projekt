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

/* typedef: compare_callback_t typedef
 * Shortcut for <compare_callback_t>. */
typedef struct compare_callback_t      compare_callback_t ;

/* define: compare_callback_SIGNATURE
 * Declares signature of callback function pointer.
 *
 * Parameter:
 * functionname   - Name of function pointer whose signature is declared.
 * callback_type  - Generic type first parameter of callback points to.
 * left_type      - The second parameter of the callback points to left_type.
 * right_type     - The third parameter of the callback points to right_type.
 * > int (* functionname) (callback_type * cb, left_type * left_elemnt, right_type * right_elemnt) */
#define compare_callback_SIGNATURE(functionname, callback_type, left_type, right_type) \
   int (* functionname) (callback_type * cb, left_type * left_elemnt, right_type * right_elemnt)

/* typedef: compare_callback_f
 * Function pointer to any callback which compares two elements.
 * The function should never fail.
 *
 * Returns:
 *  0 - The called function should return 0 in case left equals right element.
 * -1 - It should return -1 in case left is lesser then right element.
 * +1 - It should return +1 in case left is greater then right element. */
typedef compare_callback_SIGNATURE(    compare_callback_f, struct callback_aspect_t, const void, const void ) ;

/* define: compare_callback_ADAPT
 * Adapt types of callback parameter to more specific ones.
 * This macro constructs two types which are equivalent to
 * <compare_callback_f> and <compare_callback_t>.
 *
 * Parameter:
 * type_name     - Name prefix of constructed types. The name prefix is suffixed with either "_t" or "_f" to construct
 *                 two types.
 * callback_type - Type of parameter the first parameter of every callback points to.
 *                 This type replaces <callback_aspect_t>.
 * left_type     - The second parameter of the callback is constructed with type: (left_type *).
 * right_type    - The third parameter of the callback is constructed with type: (right_type *). */
#define compare_callback_ADAPT(type_name, callback_type, left_type, right_type)  \
   typedef compare_callback_SIGNATURE( type_name ## _f, callback_type, const left_type, const right_type) ; \
   typedef struct type_name ## _t      type_name ## _t ; \
   struct type_name ## _t { \
      type_name ## _f   fct ; \
      callback_type   * cb_param ; \
   } ;

/* struct: compare_callback_t
 * Stores callback function and user param for comparing. */
struct compare_callback_t {
   /* variable: fct
    * The pointer to the called back function. */
   compare_callback_f         fct ;
   /* variable: cb_param
    * The pointer to the first parameter of the called back function. */
   struct callback_aspect_t * cb_param ;
} ;

#endif
