/* title: AssertTest

   Defines system specific <assert> and standard <static_assert>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/test/assert.h
    Header file of <AssertTest>.
*/
#ifndef CKERN_TEST_ASSERT_HEADER
#define CKERN_TEST_ASSERT_HEADER


// section: Functions

// group: test

/* define: assert
 * Prints »Assertion failed« and aborts process.
 * Uses <assertfail_maincontext> to implement its functionality. */
#define assert(expr) \
   ((expr) ? (void) 0 : assertfail_maincontext(STR(expr), __FILE__, __LINE__, __FUNCTION__))

/* define: static_assert
 * Checks condition to be true during compilation. No runtime code is generated.
 * Can only be used in function context.
 *
 * Paramters:
 *  C - Condition which must hold true
 *  S - human readable explanation (ignored) */
#define static_assert(C,S) \
         ((void)(sizeof(char[(C)?1:-1])))

#endif
