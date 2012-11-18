/* title: C-Preprocessor-macros

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

   file: C-kern/api/C/ppmacros.h
    Header file <C-Preprocessor-macros>.
*/
#ifndef CKERN_C_PPMACROS_HEADER
#define CKERN_C_PPMACROS_HEADER


// section: Functions

// group: macros

/* define: CONCAT
 * Combines two language tokens into one. Calls <CONCAT_> to ensure expansion of arguments.
 *
 * Example:
 * > CONCAT(var,__LINE__)  // generates token var10 if current line number is 10
 * > CONCAT_(var,__LINE__) // generates token var__LINE__ */
#define CONCAT(S1,S2)                  CONCAT_(S1,S2)

/* define: CONCAT_
 * Used by <CONCAT> to ensure expansion of arguments.
 * This macro does the real work - combining two language tokens into one.*/
#define CONCAT_(S1,S2)                 S1 ## S2

/* define: STR
 * Makes string token out of argument. Calls <STR_> to ensure expansion of argument.
 *
 * Example:
 * > STR(__LINE__)  // generates token "10" if current line number is 10
 * > STR_(__LINE__) // generates token "__LINE__" */
#define STR(S1)                        STR_(S1)

/* define: STR_
 * Used by <STR> to ensure expansion of arguments.
 * This macro does the real work - making a string out of its argument.*/
#define STR_(S1)                       # S1

/* define: SWAP
 * Swaps content of two local variables.
 * This works only for simple types. */
#define SWAP(var1,var2)                { typeof(var1) _temp ; _temp = (var1), (var1) = (var2), (var2) = _temp ; }

#endif
