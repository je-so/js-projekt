/* title: Free-Resource-Callback
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

   file: C-kern/api/aspect/callback/free_resource.h
    Header file of <Free-Resource-Callback>.
*/
#ifndef CKERN_ASPECT_CALLBACK_FREE_RESOURCE_HEADER
#define CKERN_ASPECT_CALLBACK_FREE_RESOURCE_HEADER

// forward
struct callback_aspect_t ;

/* typedef: callback_free_resource_t typedef
 * Shortcut for <callback_free_resource_t>. */
typedef struct callback_free_resource_t   callback_free_resource_t ;

/* typedef: callback_free_resource_f
 * Function pointer to any callback which frees resources.
 * The called function should return 0 in case of success.
 * After successfull return the object has freed all its internal resources.
 * The memory *object* points to is never freed. */
typedef int                            (* callback_free_resource_f ) (struct callback_aspect_t * cb, void * object) ;

/* define: callback_free_resource_ADAPT
 * Adapt types of callback parameter to more specific ones.
 * This macro constructs two types which are equivalent to
 * <callback_free_resource_f> and <callback_free_resource_t>.
 *
 * Parameter:
 * type_name    - Name prefix of constructed types. The name prefix is suffixed with either "_t" or "_f" to construct
 *                two types.
 * object_type  - The second parameter of the callback is constructed with type: (object_type *). */
#define callback_free_resource_ADAPT(type_name, object_type)  \
   typedef int (* type_name ## _f )    ( struct callback_aspect_t * cb, object_type * object) ; \
   typedef struct type_name ## _t      type_name ## _t ; \
   struct type_name ## _t { \
      type_name ## _f            fct ; \
      struct callback_aspect_t * cb_param ; \
   } ;

/* struct: callback_free_resource_t
 * Stores callback function and user param for freeing resources. */
struct callback_free_resource_t {
   /* variable: fct
    * The pointer to the called back function. */
   callback_free_resource_f   fct ;
   /* variable: cb_param
    * The pointer to the first parameter of the called back function. */
   struct callback_aspect_t * cb_param ;
} ;

#endif
