/* title: Interface-Aspect
   Describes types used by every interface.

   * Define type of object accessed by means of an interface.

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

   file: /jsprojekt/JS/C-kern/api/aspect/interface.h
    Header file of <Interface-Aspect>.
*/
#ifndef CKERN_ASPECT_INTERFACE_HEADER
#define CKERN_ASPECT_INTERFACE_HEADER

/* typedef: interface_it typedef
 * Export <interface_it>. */
typedef struct interface_it               interface_it ;

/* typedef: interface_object_it typedef
 * Export <interface_object_it>. */
typedef struct interface_object_it        interface_object_it ;

/* define: interface_object_DECLARE
 *
 * Parameter:
 * is_typedef        - Set it to 1 if you want to also emit this code:
 *                     "typedef struct declared_type_it declared_type_it ;"
 * declared_type_t   - The name of the structure which is declared as the interfaceable object.
 * interface_type_it - The type of the service interface the stored object offers.
 *                     A pointer to this service interface is stored in the declared structure.
 * object_type_t     - The type of the object which is the first parameter of the service interface functions.
 *                     A pointer to this object is stored in the declared structure.
 *
 * (start code)
 * #define interface_object_DECLARE(is_typedef, declared_type_t, interface_type_it, object_type_t) \
 *    CONCAT(EMITCODE_, is_typedef)(typedef struct declared_type_t declared_type_t ;)  \
 *    struct declared_type_t {            \
 *       object_type_t      * object ;    \
 *       interface_type_it  * functable ; \
 *    } ;
 * (end code) */
#define interface_object_DECLARE(is_typedef, declared_type_t, interface_type_it, object_type_t) \
   CONCAT(EMITCODE_, is_typedef)(typedef struct declared_type_t declared_type_t ;)  \
   struct declared_type_t {            \
      object_type_t      * object ;    \
      interface_type_it  * functable ; \
   } ;


/* struct: interface_object_it
 * A generic interfaceable object.
 *
 * The structure contains a pointer to the object
 * and a pointer to a table of pointers to functions.
 *
 * With help of the function table the exported interface of the object can be accessed.
 *
 * This structure serves as an example of the structure of interfaceable objects. */
struct interface_object_it {
   /* variable: object
    * A pointer to the object which is operated on by interface <interface_it>. */
   void         * object ;
   /* variable: functable
    * A pointer to a function table interface <interface_it> which operates on <object>. */
   interface_it * functable ;
} ;


/* struct: interface_it
 * Table of pointers to functions which constitute the interface. */
struct interface_it {
      int (*free) (void * object) ;
} ;

#endif