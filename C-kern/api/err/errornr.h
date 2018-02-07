/* title: ErrorNumbers

   Contains list of application specific error numbers.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/err/errornr.h
    Header file <ErrorNumbers>.

   file: C-kern/err/errornr.c
    Implementation file <ErrorNumbers impl>.
*/
#ifndef CKERN_ERR_ERRORNR_HEADER
#define CKERN_ERR_ERRORNR_HEADER

/* enums: errornr_e
 * Application specific error codes.
 *
 * ENOTINIT   - Object not initialized.
 * EINVARIANT - Invariant violated. This means for example corrupt memory, a software bug.
 * ERESET     - A power management event has occurred. The application must destroy all contexts and reinitialise OpenGL ES state and objects to continue rendering.
 * EALLOC     - Allocation of (window, printer or other) resource failed.
 * ELEAK      - Resource leak. This means for example memory not freed, file descriptors not closed and so on.
 * ESYNTAX    - General syntax error during parsing
 * */
typedef enum errornr_e {
   errornr_STATE_NOTINIT = 256,
   errornr_STATE_INVARIANT,
   errornr_STATE_RESET,
   errornr_RESOURCE_ALLOCATE,
   errornr_RESOURCE_LEAK,
   errornr_RESOURCE_LEAK_MEMORY,
   errornr_PARSER_SYNTAX,
   errornr_NEXTERRORCODE
} errornr_e;

#define ENOTINIT     errornr_STATE_NOTINIT
#define EINVARIANT   errornr_STATE_INVARIANT
#define ERESET       errornr_STATE_RESET
#define EALLOC       errornr_RESOURCE_ALLOCATE
#define ELEAK        errornr_RESOURCE_LEAK
#define EMEMLEAK     errornr_RESOURCE_LEAK_MEMORY
#define ESYNTAX      errornr_PARSER_SYNTAX


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_err_errornr
 * Test assignment of <errornr_e>. */
int unittest_err_errornr(void);
#endif


#endif
