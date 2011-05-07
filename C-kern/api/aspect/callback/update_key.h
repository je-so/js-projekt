/* title: Update-Key-Callback
   Type of callback which updates a key
   of an object like a tree node.

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

   file: /jsprojekt/JS/C-kern/api/aspect/callback/update_key.h
    Header file of <Update-Key-Callback>.
*/
#ifndef CKERN_ASPECT_CALLBACK_UPDATE_KEY_HEADER
#define CKERN_ASPECT_CALLBACK_UPDATE_KEY_HEADER

// forward
struct callback_aspect_t ;

/* typedef: callback_update_key_t typedef
 * Shortcut for <callback_update_key_t>. */
typedef struct callback_update_key_t   callback_update_key_t ;

/* typedef: callback_update_key_f
 * Function pointer to any callback which updates a key.
 * The called function should return 0 in case of success.
 * After successfull return the object contains in its key field(s)
 * the new value of new_key. */
typedef int                         (* callback_update_key_f ) (struct callback_aspect_t * cb, const void * new_key, void * object) ;

/* define: callback_update_key_ADAPT
 * Adapt types of callback parameter to more specific ones.
 * This macro constructs two types which are equivalent to
 * <callback_update_key_f> and <callback_update_key_t>.
 *
 * Parameter:
 * type_name    - Name prefix of constructed types. The name prefix is suffixed with either "_t" or "_f" to construct
 *                two types.
 * key_type     - The second parameter of the callback is constructed with type: (const key_type *).
 * object_type  - The third parameter of the callback is constructed with type: (object_type *). */
#define callback_update_key_ADAPT(type_name, key_type, object_type)  \
   typedef int (* type_name ## _f )    ( struct callback_aspect_t * cb, const key_type * new_key, object_type * object) ; \
   typedef struct type_name ## _t      type_name ## _t ; \
   struct type_name ## _t { \
      type_name ## _f            fct ; \
      struct callback_aspect_t * cb_param ; \
   } ;

/* struct: callback_update_key_t
 * Stores callback function and user param for updating a key. */
struct callback_update_key_t {
   /* variable: fct
    * The pointer to the called back function. */
   callback_update_key_f      fct ;
   /* variable: cb_param
    * The pointer to the first parameter of the called back function. */
   struct callback_aspect_t * cb_param ;
} ;

#endif
