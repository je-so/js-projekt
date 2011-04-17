/*
   C-System-Layer: C-kern/api/test.h
   Copyright (C) 2010 Jörg Seebohn

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
*/
#ifndef CKERN_API_TEST_HEADER
#define CKERN_API_TEST_HEADER

// section: Test

// group: Macro

/* define: TEST_ONERROR_GOTO
 * Tests CONDITION and exits on error.
 * If CONDITION fails an error is printed and further tests are skipped cause it jumps to
 * ERROR_LABEL with help of goto.
 *
 * Parameters:
 * CONDITION          - Condition which is tested to be true.
 * TEST_FUNCTION_NAME - Name of the unittest function which is printed in case of error.
 * ERROR_LABEL        - Name of label the test macro jumps in case of error.
 *
 * Usage:
 * TODO describe usage
 * */
#define TEST_ONERROR_GOTO(CONDITION,TEST_FUNCTION_NAME,ERROR_LABEL) \
   if ( !(CONDITION) ) {\
      dprintf( STDERR_FILENO, "%s:%d: %s():\n FAILED TEST (%s)\n", __FILE__, __LINE__, #TEST_FUNCTION_NAME, #CONDITION) ;\
      goto ERROR_LABEL ;\
   }

// group: malloc

/* function: prepare_malloctest
 * Inits test of system-malloc.
 * The GNU C library offers some
 * a query function for malloc so
 * this function does 2 steps:
 * 1. Call system functions which
 *    allocate system memory (strerror).
 * 2. Free all allocated but unused memory. */
extern void prepare_malloctest(void) ;

/* function: trimmemory_malloctest
 * Frees preallocated memory which is not in use. */
extern void trimmemory_malloctest(void) ;

/* function: usedbytes_malloctest
 * Returns alloctaed number of bytes which are not freed. */
extern size_t usedbytes_malloctest(void) ;


#endif
