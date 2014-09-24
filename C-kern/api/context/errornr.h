/* title: ErrorNumbers

   Contains list of application specific error numbers.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/context/errornr.h
    Header file <ErrorNumbers>.

   file: C-kern/context/errornr.c
    Implementation file <ErrorNumbers impl>.
*/
#ifndef CKERN_CONTEXT_ERRORNR_HEADER
#define CKERN_CONTEXT_ERRORNR_HEADER

/* enums: errornr_e
 * Application specific error codes.
 *
 * EINVARIANT - Invariant violated. This means for example corrupt memory, a software bug.
 * ELEAK      - Resource leak. This means for example memory not freed, file descriptors not closed and so on.
 * */
enum errornr_e {
   errornr_FIRSTERRORCODE  = 256,
   errornr_STATE           = errornr_FIRSTERRORCODE,
   errornr_STATE_INVARIANT,
   errornr_STATE_RESET,
   errornr_RESOURCE_ALLOCATE,
   errornr_RESOURCE_LEAK,
   errornr_NEXTERRORCODE
} ;

typedef enum errornr_e  errornr_e;

#define ESTATE       errornr_STATE
#define EINVARIANT   errornr_STATE_INVARIANT
#define ERESET       errornr_STATE_RESET
#define EALLOC       errornr_RESOURCE_ALLOCATE
#define ELEAK        errornr_RESOURCE_LEAK


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_context_errornr
 * Test assignment of <errornr_e>. */
int unittest_context_errornr(void) ;
#endif


#endif
