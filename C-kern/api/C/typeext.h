/* title: C-Type-Extensions

   Offers macros to support type casts, function declarations and size calculations.

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

   file: C-kern/api/C/typeext.h
    Header file <C-Type-Extensions>.
*/
#ifndef CKERN_C_TYPEEXT_HEADER
#define CKERN_C_TYPEEXT_HEADER


// section: Functions

// group: function-declaration

/* define: IDNAME
 * It's a marker in function declaration.
 * It is used in function declarations which are implemented as macro.
 * An identifier name is not supported in language C99 or later.
 * See <dlist_IMPLEMENT> for an example. */
#define IDNAME                         void*

/* define: TYPENAME
 * It's a marker in function declaration.
 * It is used in function declarations which are implemented as macro.
 * A type name is not supported in language C99 or later.
 * See <asgeneric_typeadaptlifetime> for an example. */
#define TYPENAME                       void*

// group: size-calculations

/* define: bitsof
 * Calculates memory size of a type in number of bits. */
#define bitsof(type_t)                 (8*sizeof(type_t))

/* define: nrelementsof
 * Calculates the number of elements of a static array. */
#define nrelementsof(static_array)     ( sizeof(static_array) / sizeof(*(static_array)) )

// group: cast

/* define: CONST_CAST
 * Removes the const from ptr.
 * Use this macro to remove a const from a pointer. This macro is safe
 * so if you cast the pointer to a wrong type a warning is issued by the compiler.
 * Parameter *ptr* must be of type
 * > const type_t * ptr ;
 * The returned type is
 * > (type_t *) ptr ; */
#define CONST_CAST(type_t,ptr)         ( __extension__ ({ const type_t * _ptr = (ptr) ;  (type_t*)((uintptr_t)_ptr) ; }))

/* define: structof
 * Converts pointer to member of structure to pointer of containing structure. */
#define structof(struct_t, member, ptrmember)                              \
   ( __extension__ ({                                                      \
      typeof(((struct_t*)0)->member) * _ptr = (ptrmember) ;                \
      (struct_t*)((uintptr_t)_ptr - offsetof(struct_t, member) ) ;         \
   }))

/* define: VOLATILE_CAST
 * Removes the volatile from ptr.
 * Use this macro to remove a vvolatile from a pointer. This macro is safe
 * so if you cast the pointer to a wrong type a warning is issued by the compiler.
 * Parameter *ptr* must be of type
 * > volatile type_t * ptr ;
 * The returned type is
 * > (type_t*) ptr ; */
#define VOLATILE_CAST(type_t,ptr)      ( __extension__ ({ volatile type_t * _ptr = (ptr) ;  (type_t*)((uintptr_t)_ptr) ; }))

#endif
