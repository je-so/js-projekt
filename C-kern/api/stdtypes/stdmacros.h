/* title: Standard-Macros

   Defines some generic C macros useful during preprocessing time of C files.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2012 Jörg Seebohn

   file: C-kern/api/stdtypes/stdmacros.h
    Header file <Standard-Macros>.

   file: C-kern/konfig.h
    Included from header <Konfiguration>.
*/
#ifndef CKERN_STDTYPES_STDMACROS_HEADER
#define CKERN_STDTYPES_STDMACROS_HEADER


// section: Macros

// group: helper-macros

/* define: MACRO
 * Allows the preprocessor to replace any macro arguments with their value before calling another macro.
 * This macro helps to implement helper macros for which two implementations needs to be provided normally.
 * See CONCAT and CONCAT2 for example.
 *
 * Parameter:
 * _M   - Name of the macro function.
 * ...  - Any parameters, possible macros itself, the function is called with. */
#define MACRO(_M,...)                  _M(__VA_ARGS__)

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
#define CONST_CAST(type_t, ptr) \
         ( __extension__ ({               \
            const type_t * _ptr1 = (ptr); \
            (type_t*) ((uintptr_t)_ptr1); \
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
#define VOLATILE_CAST(type_t,ptr) \
         ( __extension__ ({                  \
            volatile type_t * _ptr1 = (ptr); \
            (type_t*) ((uintptr_t)_ptr1);    \
         }))

// group: function-declaration

/* define: IDNAME
 * Use it to declare a parameter of type name which serves as a valid C identifier.
 * See <dlist_IMPLEMENT> for an example. */
#define IDNAME                         void*

/* define: LABEL
 * Use it to declare a parameter of type code block.
 * The code block { statement1; statement2; ...;} is
 * embedded inside a macro expression.
 * See <end_syncfunc> for an example. */
#define CODEBLOCK                      void*

/* define: LABEL
 * Use it to declare a parameter of type C label.
 * The label serves a target of a goto statement. */
#define LABEL                          void*

/* define: TYPENAME
 * It's a placeholder for any C type in a function declaration.
 * It is used in function declarations which are implemented as macro.
 * A type name is not supported in language C99 or later.
 * See <cast_typeadaptlifetime> for an example. */
#define TYPENAME                       void*

/* define: TYPEQUALIFIER
 * It's a marker in a function declaration.
 * It is used in function declarations which are implemented as macro.
 * Set it to either an empty value ,, or to ,const,.
 * See <memblock_t.cast_memblock> for an example. */
#define TYPEQUALIFIER                  void*

// group: function-call

/* define: nrargsof_get11th
 * Returns the 11th argument of the parameter list.
 * Use MACRO to call this helper to replace macro argument before this macro is called. */
#define nrargsof_get11th(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,...) _11

/* define: nrargsof_helper0
 * Helper function supporting functions up to 9 arguments.
 *
 * Used in nrargsof as:
 * > nrargsof_helper0 __VA_ARGS__ ()
 * This token sequence resolves to nrargsof_helper0() in case __VA_ARGS__ is an empty parameter list.
 * In case __VA_ARGS__ contains one parameter it resolves to a single argument "nrargsof_helper0 ARG1 ()" .
 * In case of two parameters it resolves to two arguments "nrargsof_helper0 ARG1,ARG2 ()" and so on. */
#define nrargsof_helper0()             1,2,3,4,5,6,7,8,9,10

/* define: nrargsof
 * Determines the number of arguments in the given parameter list (...).
 * A maximum number of 9 parameters is supported. */
#define nrargsof(...)                  MACRO(nrargsof_get11th, nrargsof_helper0 __VA_ARGS__ (),0,9,8,7,6,5,4,3,2,1,-1)

/* define: fctname_nrargsof(fctname, _suffix, ...)
 * Rturns function name out of fctname0_suffix,...,fctname9_suffix depending on the number of arguments.
 * The number of arguments supported are between 0 and 9 inclusive.
 *
 * Parameter:
 * fctname   - The name of function
 * _suffix   - The suffix of the function.
 * ...       - The number of parameters the function should be called with.
 *
 * Returns:
 * fctnameNR_suffix where NR is replaced with a single digit from [0..9] depending on the number of parameters. */
#define fctname_nrargsof(fctname, _suffix, ...) \
         CONCAT( fctname, CONCAT(nrargsof(__VA_ARGS__), _suffix))

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
