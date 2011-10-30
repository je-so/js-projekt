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

   file: C-kern/api/aspect/callback/compare.h
    Header file of <Compare-Callback>.
*/
#ifndef CKERN_ASPECT_CALLBACK_COMPARE_HEADER
#define CKERN_ASPECT_CALLBACK_COMPARE_HEADER

// forward
struct callback_param_t ;

/* typedef: struct compare_callback_t
 * Shortcut for <compare_callback_t>. */
typedef struct compare_callback_t      compare_callback_t ;

/* typedef: compare_callback_f
 * Function pointer to a function which compares two elements.
 * The function should never fail.
 *
 * Parameter:
 * cb    - The opaque callback parameter to supply the callback function with context information.
 * left  - The left element of the comparison.
 * right - The right element of the comparison.
 *
 * Returns:
 *  0 - The called function should return 0 in case left equals right element.
 * -1 - It should return -1 in case left is lesser then right element.
 * +1 - It should return +1 in case left is greater then right element. */
typedef int                         (* compare_callback_f) (struct callback_param_t * cb, const void * left, const void * right) ;

/* define: compare_callback_f_DECLARE
 * Declares signature of callback function pointer.
 *
 * Parameter:
 * declared_type_f - Name of function pointer whose signature is declared.
 * cb_param_t      - Generic type of first parameter of callback function.
 * left_t          - The type of the second parameter of the callback.
 * right_t         - The type of the 3d parameter of the callback.
 * > int (* declared_type_f) (cb_param_t * cb, left_t left, right_t right) */
#define compare_callback_f_DECLARE(declared_type_f, cb_param_t, left_t, right_t) \
   typedef int (* declared_type_f) (cb_param_t * cb, left_t left, right_t right) ;

/* define: compare_callback_ADAPT
 * Adapt types of callback parameter to more specific ones.
 * This macro constructs two types which are equivalent to
 * <compare_callback_f> and <compare_callback_t>.
 *
 * Parameter:
 * declared_type - Name prefix of constructed types. The name prefix is suffixed with either "_t" or "_f" to construct
 *                 two types.
 * cb_param_t    - Type of first parameter the first parameter of every callback points to.
 *                 This type replaces <callback_param_t>.
 * left_t        - The type of object the second parameter of the callback points to.
 * right_t       - The type of object the 3d parameter of the callback points to. */
#define compare_callback_ADAPT(declared_type, cb_param_t, left_t, right_t)  \
   compare_callback_f_DECLARE( declared_type ## _f, cb_param_t, const left_t *, const right_t *) \
   typedef struct declared_type ## _t      declared_type ## _t ; \
   struct declared_type ## _t {         \
      declared_type ## _f   fct ;       \
      cb_param_t          * cb_param ;  \
   } ;


/* struct: compare_callback_t
 * Stores pointer to comparison function and opaque data pointer.
 * The opaque pointer is used as first parameter of the called function. */
struct compare_callback_t {
   // group: public Variables
   /* variable: fct
    * The pointer to the called back function. */
   compare_callback_f         fct ;
   /* variable: cb_param
    * The pointer to the first parameter of the called back function. */
   struct callback_param_t  * cb_param ;
} ;

#endif
