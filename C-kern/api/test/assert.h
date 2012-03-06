/* title: AssertTest
   Defines system specific assert macro.

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

   file: C-kern/api/test/assert.h
    Header file of <AssertTest>.
*/
#ifndef CKERN_TEST_ASSERT_HEADER
#define CKERN_TEST_ASSERT_HEADER

/* define: assert
 * Prints »Assertion failed« and aborts process.
 * Uses <assertfail_maincontext> to implement its functionality. */
#define assert(expr) \
   ((expr) ? (void) 0 : assertfail_maincontext(STR(expr), __FILE__, __LINE__, __FUNCTION__))


#endif