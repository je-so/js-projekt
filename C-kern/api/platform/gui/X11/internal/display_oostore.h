/* title: X11-Display OOSTORE
   Defines types which are managed by an
   internal object store.

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

   file: C-kern/api/platform/gui/X11/internal/display_oostore.h
    Header file of <X11-Display OOSTORE>.
*/
#ifndef CKERN_PLATFORM_GUI_X11_DISPLAY_OOSTORE_HEADER
#define CKERN_PLATFORM_GUI_X11_DISPLAY_OOSTORE_HEADER

#include "C-kern/api/oostore.h"
#include "C-kern/api/platform/index/splaytree.h"

typedef struct x11display_objectid_t   x11display_objectid_t ;

/* struct: x11display_objectid_t
 * Associates Xlib object id (XID) with object pointer.
 *
 * An <oostore_memindex_t> is included to allow this
 * type of structure to be stored in an index of type tree
 * for faster searching.
 * */
struct x11display_objectid_t {
   /* variable: index1
    * First index defined on <id>. */
   splaytree_node_t  index1/*index(id)*/ ;
   /* variable: id
    * The identification (a 32 bit number) of an object. */
   uint32_t    id ;
   /* variable: object
    * The pointer to the identified object. */
   void        * object ;
} ;

static inline int free_x11displayobjectid(x11display_objectid_t * obj)
{
   (void) obj ;
   return 0 ;
}

// OOSTORE:GENERATE(new,delete,find,update[object])FOR(x11display_objectid_t)*/

/* typedef: x11display_objectid_root_t
 * Root pointer to collection of <x11display_objectid_t>.
 * The content is indexed by <x11display_objectid_t.id>. */
typedef x11display_objectid_t * x11display_objectid_root_t ;

static inline x11display_objectid_t *
fromindex1_x11displayobjectid(const splaytree_node_t * indexaspect) ;

static inline splaytree_node_t *
toindex1_x11displayobjectid(x11display_objectid_t * object) ;

static inline int
comparecb_x11displayobjectid(callback_param_t * cb, const void * key, const splaytree_node_t * indexaspect) ;

static inline int
freecb_x11displayobjectid(callback_param_t * cb, splaytree_node_t * indexaspect) ;

static inline int
new_x11displayobjectid(x11display_objectid_root_t * rootobj, uint32_t key_id, void * value_object) ;

static inline int
delete_x11displayobjectid(x11display_objectid_root_t * rootobj, uint32_t key_id) ;

static inline int
find_x11displayobjectid(x11display_objectid_root_t * rootobj, uint32_t key_id, /*out*/x11display_objectid_t ** object) ;



static inline x11display_objectid_t * fromindex1_x11displayobjectid(const splaytree_node_t * indexaspect)
{
   return (x11display_objectid_t *) ((intptr_t)(indexaspect) - (intptr_t)(&((x11display_objectid_t*)0)->index1)) ;
}

static inline splaytree_node_t * toindex1_x11displayobjectid(x11display_objectid_t * object)
{
   return (& object->index1) ;
}

static inline int comparecb_x11displayobjectid(callback_param_t * cb, const void * key, const splaytree_node_t * indexaspect)
{
   (void) cb ;
   static_assert( sizeof(uint32_t) <= sizeof(void*), "key value is cast into void*" ) ;
   x11display_objectid_t * object = fromindex1_x11displayobjectid(indexaspect) ;
   uint32_t key1 = (uint32_t) key ;
   uint32_t key2 = object->id ;
   return (key1 < key2 ? -1 : (key1 > key2 ? +1 : 0)) ;
}

static inline int freecb_x11displayobjectid(callback_param_t * cb, splaytree_node_t * indexaspect)
{
   int err ;
   oostore_t * oostore = (oostore_t *) cb ;
   x11display_objectid_t * object = fromindex1_x11displayobjectid(indexaspect) ;
   err = mfree_oostore(oostore, (void**)&object) ;
   return err ;
}

static inline int new_x11displayobjectid(x11display_objectid_root_t * rootobj, uint32_t key_id, void * value_object)
{
   int err ;
   oostore_t              * oostore    = 0 ;
   splaytree_compare_t      compare_cb = { .fct = &comparecb_x11displayobjectid, .cb_param = (callback_param_t*)0 } ;
   x11display_objectid_t  * new_object ;
   err = malloc_oostore( oostore, sizeof(x11display_objectid_t), (void**)&new_object) ;
   if (err) return err ;
   new_object->id     = key_id ;
   new_object->object = value_object ;
   err = insert_splaytree((splaytree_t*)rootobj, (void *)key_id, toindex1_x11displayobjectid(new_object), &compare_cb) ;
   if (err) {
      (void) mfree_oostore( oostore, (void**)&new_object) ;
      return err ;
   }
   return 0 ;
}

static inline int delete_x11displayobjectid(x11display_objectid_root_t * rootobj, uint32_t key_id)
{
   int err ;
   int err2 ;
   oostore_t              * oostore    = 0 ;
   splaytree_compare_t      compare_cb = { .fct = &comparecb_x11displayobjectid, .cb_param = (callback_param_t*)0 } ;
   splaytree_node_t       * removed_obj ;
   x11display_objectid_t  * del_object ;
   err = remove_splaytree((splaytree_t*)rootobj, (void *)key_id, &removed_obj, &compare_cb) ;
   if (err) return err ;
   del_object = fromindex1_x11displayobjectid(removed_obj) ;
   err = free_x11displayobjectid(del_object) ;
   err2 = mfree_oostore( oostore, (void**)&del_object) ;
   if (err2) err = err2 ;
   return err ;
}

static inline int find_x11displayobjectid(x11display_objectid_root_t * rootobj, uint32_t key_id, /*out*/x11display_objectid_t ** object)
{
   int err ;
   splaytree_compare_t  compare_cb = { .fct = &comparecb_x11displayobjectid, .cb_param = (callback_param_t*)0 } ;
   splaytree_node_t   * found_object ;
   err = find_splaytree((splaytree_t*)rootobj, (void *)key_id, &found_object, &compare_cb) ;
   if (err) return err ;
   *object = fromindex1_x11displayobjectid(found_object) ;
   return 0 ;
}

static inline int update_x11displayobjectid(x11display_objectid_root_t * rootobj, uint32_t key_id, void * value_object)
{
   int err ;
   splaytree_compare_t     compare_cb = { .fct = &comparecb_x11displayobjectid, .cb_param = (callback_param_t*)0 } ;
   splaytree_node_t      * found_object ;
   x11display_objectid_t * update_object ;
   err = find_splaytree((splaytree_t*)rootobj, (void *)key_id, &found_object, &compare_cb) ;
   if (err) return err ;
   update_object = fromindex1_x11displayobjectid(found_object) ;
   update_object->object = value_object ;
   return 0 ;
}

static inline int deleteset_x11displayobjectid(x11display_objectid_root_t * rootobj)
{
   int err ;
   splaytree_free_t  free_cb = { .fct = &freecb_x11displayobjectid, .cb_param = (callback_param_t*)0 } ;
   err = free_splaytree((splaytree_t*)rootobj, &free_cb) ;
   return err ;
}

// OOSTORE:END

#endif
