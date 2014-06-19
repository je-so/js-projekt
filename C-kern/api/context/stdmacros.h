/* title: Standard-Macros

   Defines some generic C macros useful during preprocessing time of C files.

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

   file: C-kern/api/context/stdmacros.h
    Header file <Standard-Macros>.

   file: C-kern/konfig.h
    Included from header <Konfiguration>.
*/
#ifndef CKERN_CONTEXT_STDMACROS_HEADER
#define CKERN_CONTEXT_STDMACROS_HEADER


// section: Macros

// group: string

/* define: CONCAT
 * Combines two language tokens into one. Calls <CONCAT2> to ensure expansion of arguments.
 *
 * Example:
 * > CONCAT(var,__LINE__)  // generates token var10 if current line number is 10
 * > CONCAT2(var,__LINE__) // generates token var__LINE__ */
#define CONCAT(S1,S2)                  CONCAT2(S1,S2)

/* define: CONCAT2
 * Used by <CONCAT> to ensure expansion of arguments.
 * This macro does the real work - combining two language tokens into one.*/
#define CONCAT2(S1,S2)                 S1 ## S2

/* define: STR
 * Makes string token out of argument. Calls <STR2> to ensure expansion of argument.
 *
 * Example:
 * > STR(__LINE__)  // generates token "10" if current line number is 10
 * > STR2(__LINE__) // generates token "__LINE__" */
#define STR(S1)                        STR2(S1)

/* define: STR2
 * Used by <STR> to ensure expansion of arguments.
 * This macro does the real work - making a string out of its argument.*/
#define STR2(S1)                       # S1

// group: cast

/* define: CONST_CAST
 * Removes the const from ptr.
 * Use this macro to remove a const from a pointer. This macro is safe
 * so if you cast the pointer to a wrong type a warning is issued by the compiler.
 * Parameter *ptr* must be of type
 * > const type_t * ptr ;
 * The returned type is
 * > (type_t *) ptr ; */
#define CONST_CAST(type_t, ptr)              \
         ( __extension__ ({                  \
            const type_t * _ptr1 = (ptr) ;   \
            (type_t*) ((uintptr_t)_ptr1) ;   \
         }))

/* define: structof
 * Converts pointer to member of structure to pointer of containing structure. */
#define structof(struct_t, member, ptrmember)   \
   ( __extension__ ({                           \
      typeof(((struct_t*)0)->member) * _ptr2 ;  \
      _ptr2 = (ptrmember) ;                     \
      (struct_t*)                               \
         ((uintptr_t)_ptr2                      \
         - offsetof(struct_t, member)) ;        \
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

// group: function-declaration

/* define: IDNAME
 * It's a marker in a function declaration.
 * It is used in function declarations which are implemented as macro.
 * An identifier name is not supported in language C99 or later.
 * See <dlist_IMPLEMENT> for an example. */
#define IDNAME                         void*

/* define: TYPENAME
 * It's a marker in a function declaration.
 * It is used in function declarations which are implemented as macro.
 * A type name is not supported in language C99 or later.
 * See <genericcast_typeadaptlifetime> for an example. */
#define TYPENAME                       void*

/* define: TYPEQUALIFIER
 * It's a marker in a function declaration.
 * It is used in function declarations which are implemented as macro.
 * Set it to either an empty value ,, or to ,const,.
 * See <genericcast_mmfile> for an example. */
#define TYPEQUALIFIER                  void*

// group: size-calculations

/* define: bitsof
 * Calculates memory size of a type in number of bits. */
#define bitsof(type_t)                 (8*sizeof(type_t))

/* define: lengthof
 * Returns number of elements of the first dimension of a static array. */
#define lengthof(static_array)         (sizeof(static_array) / sizeof(*(static_array)))

// group: memory

/* define: MEMCOPY
 * Copies memory from source to destination.
 * The values of sizeof(*(source)) and sizeof(*(destination)) must compare equal.
 * This is checked by <static_assert>.
 *
 * Parameter:
 * destination - Pointer to destination address
 * source      - Pointer to source address
 */
#define MEMCOPY(destination, source)   do { static_assert(sizeof(*(destination)) == sizeof(*(source)),"same size") ; memcpy((destination), (source), sizeof(*(destination))) ; } while(0)

/* define: MEMSET0
 * Sets memory of variable to 0.
 *
 * Parameter:
 * pointer - Pointer to the variable which will be cleared.
 *
 * To clear a whole array use &array as parameter:
 * >  int array[100] ;
 * >  MEMSET0(&array) ; */
#define MEMSET0(pointer)               memset((pointer), 0, sizeof(*(pointer)))

#endif
