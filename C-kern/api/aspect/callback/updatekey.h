/* title: Updatekey-Callback
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

   file: C-kern/api/aspect/callback/updatekey.h
    Header file of <Updatekey-Callback>.
*/
#ifndef CKERN_ASPECT_CALLBACK_UPDATE_KEY_HEADER
#define CKERN_ASPECT_CALLBACK_UPDATE_KEY_HEADER

// forward
struct callback_param_t ;

/* typedef: updatekey_callback_t typedef
 * Shortcut for <updatekey_callback_t>. */
typedef struct updatekey_callback_t   updatekey_callback_t ;

/* define: updatekey_callback_SIGNATURE
 * Declares signature of function pointer.
 *
 * Parameter:
 * functionname   - Name of function pointer whose signature is declared.
 * cbparam_type   - Generic type first parameter of callback points to.
 * key_type       - Type of key the second parameter points to.
 * object_type    - Type of object third parameter points to.
 * > int (* functionname) (cbparam_type * cb, const key_type * new_key, object_type * object) */
#define updatekey_callback_SIGNATURE(functionname, cbparam_type, key_type, object_type) \
   int (* functionname) (cbparam_type * cb, const key_type * new_key, object_type * object)

/* typedef: updatekey_callback_f
 * Function pointer to any callback which updates a key.
 * The called function should return 0 in case of success.
 * After successfull return the object contains in its key field(s)
 * the new value of new_key. */
typedef updatekey_callback_SIGNATURE(  updatekey_callback_f, struct callback_param_t, void, void) ;

/* define: updatekey_callback_ADAPT
 * Adapt types of callback parameter to more specific ones.
 * This macro constructs two types which are equivalent to
 * <updatekey_callback_f> and <updatekey_callback_t>.
 *
 * Parameter:
 * type_name     - Name prefix of constructed types. The name prefix is suffixed with either "_t" or "_f" to construct
 *                 two types.
 * cbparam_type  - Type of parameter the first parameter of every callback points to.
 *                 This type replaces <callback_param_t>.
 * key_type       - Type of key the second parameter points to.
 * object_type    - Type of object third parameter points to. */
#define updatekey_callback_ADAPT(type_name, cbparam_type, key_type, object_type)  \
   typedef int (* type_name ## _f )    ( cbparam_type * cb, const key_type * new_key, object_type * object) ; \
   typedef struct type_name ## _t      type_name ## _t ; \
   struct type_name ## _t { \
      type_name ## _f   fct ; \
      cbparam_type    * cb_param ; \
   } ;

/* struct: updatekey_callback_t
 * Stores callback function and user param for updating a key. */
struct updatekey_callback_t {
   /* variable: fct
    * The pointer to the called back function. */
   updatekey_callback_f       fct ;
   /* variable: cb_param
    * The pointer to the first parameter of the called back function. */
   struct callback_param_t  * cb_param ;
} ;

#endif
