/* title: Argumentcheck
   Test that input parameter are valid.

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

   file: C-kern/api/test/argument.h
    Header file of <Argumentcheck>.
*/
#ifndef CKERN_TEST_ARGUMENT_HEADER
#define CKERN_TEST_ARGUMENT_HEADER

/* define: CHECK_INARG
 * Tests condition on input parameter.
 * In case of condition is wrong the error variable *err* is set to
 * EINVAL and execution continues at the given label *_ONERROR_LABEL*.
 *
 * Parameter:
 * _CONDITION        - The condition to check for (example: x > 0 )
 * _ONERROR_LABEL    - The label to jump to if condition is wrong.
 * _LOG_VALUE        - The log command to log the value of the wrong parameter.
 * */
#define CHECK_INARG(_CONDITION,_ONERROR_LABEL,_LOG_VALUE)  \
   if (!(_CONDITION)) {                                  \
      err = EINVAL ;                                     \
      LOG_ERRTEXT(FUNCTION_WRONG_INPUT(#_CONDITION)) ;   \
      _LOG_VALUE ;                                       \
      goto _ONERROR_LABEL ;                              \
   }


#endif
