/* title: Free-Callback
   Type of callback which frees resources
   associated with an object like a tree node.

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

   file: C-kern/api/aspect/callback/free.h
    Header file of <Free-Callback>.
*/
#ifndef CKERN_ASPECT_CALLBACK_FREE_HEADER
#define CKERN_ASPECT_CALLBACK_FREE_HEADER

// forward
struct callback_aspect_t ;

/* typedef: free_callback_t typedef
 * Shortcut for <free_callback_t>. */
typedef struct free_callback_t         free_callback_t ;

/* define: free_callback_SIGNATURE
 * Declares signature of function pointer.
 *
 * Parameter:
 * functionname   - Name of function pointer whose signature is declared.
 * callback_type  - Generic type first parameter of callback points to.
 * object_type    - Type of object second parameter points to.
 * > int (* functionname) (callback_type * cb, object_type * object) */
#define free_callback_SIGNATURE(functionname, callback_type, object_type) \
   int (* functionname) (callback_type * cb, object_type * object)

/* typedef: free_callback_f
 * Function pointer to any callback which frees resources.
 * The called function should return 0 in case of success.
 * After successfull return the object has freed all its internal resources.
 * The memory *object* points to is never freed. */
typedef  free_callback_SIGNATURE(      free_callback_f, struct callback_aspect_t, void ) ;

/* define: free_callback_ADAPT
 * Adapt types of callback parameter to more specific ones.
 * This macro constructs two types which are equivalent to
 * <free_callback_f> and <free_callback_t>.
 *
 * Parameter:
 * type_name     - Name prefix of constructed types. The name prefix is suffixed with either "_t" or "_f" to construct
 *                 two types.
 * callback_type - Type of parameter the first parameter of every callback points to.
 *                 This type replaces <callback_aspect_t>.
 * object_type   - The second parameter of the callback is constructed with type: (object_type *).
 *                 This type replaces (void*). */
#define free_callback_ADAPT(type_name, callback_type, object_type)  \
   typedef free_callback_SIGNATURE(    type_name ## _f , callback_type, object_type) ; \
   typedef struct type_name ## _t      type_name ## _t ; \
   struct type_name ## _t { \
      type_name ## _f   fct ; \
      callback_type   * cb_param ; \
   } ;

/* struct: free_callback_t
 * Stores callback function and user param for freeing resources. */
struct free_callback_t {
   /* variable: fct
    * The pointer to the called back function. */
   free_callback_f            fct ;
   /* variable: cb_param
    * The pointer to the first parameter of the called back function. */
   struct callback_aspect_t * cb_param ;
} ;

#endif
