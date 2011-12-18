/* title: Test-Errortimer
   Implements simple count down until error code is returned.
   All exported functions are implemented inline.

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

   file: C-kern/api/test/errortimer.h
    Header file of <Test-Errortimer>.
*/
#ifndef CKERN_TEST_ERRORTIMER_HEADER
#define CKERN_TEST_ERRORTIMER_HEADER

/* typedef: struct test_errortimer_t
 * Export <test_errortimer_t>. */
typedef struct test_errortimer_t       test_errortimer_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_test_errortimer
 * Unittest for <test_errortimer_t>. */
extern int unittest_test_errortimer(void) ;
#endif

/* struct: test_errortimer_t
 * Holds a timer value and an error code.
 * The function <process_testerrortimer> returns
 * a stored error code if timercount has reached zero. */
struct test_errortimer_t {
   /* variable: timercount
    * The number of times <process_testerrortimer> returns success. */
   uint32_t    timercount ;
   /* variable: errcode
    * The error code which is returned by <process_testerrortimer>. */
   int         errcode ;
} ;

/* define: test_errortimer_INIT_FREEABLE
 * Static initializer. Initializes timer disarmed. */
#define test_errortimer_INIT_FREEABLE    { 0, 0 }

/* function: init_testerrortimer
 * Inits <test_errortimer_t> with timercount and errcode.
 *
 * Parameter:
 * errtimer    - Pointer to object of type <test_errortimer_t> which is initialized.
 * timercount  - The number of times after <process_testerrortimer> returns an error.
 * errcode     - The errorcode <process_testerrortimer> returns in timer has fired.
 * */
extern int init_testerrortimer(/*out*/test_errortimer_t * errtimer, uint32_t timercount, int errcode) ;

/* function: process_testerrortimer
 * Returns error if timer has elapsed else 0.
 * If <test_errortimer_t.timercount> is 0 the timer is disarmed, nothing is done and 0 is returned.
 * Else timercount is decremented.
 * <test_errortimer_t.errcode> is returned if timercount has reached zero else 0. */
extern int process_testerrortimer(test_errortimer_t * errtimer) ;

/* function: ONERROR_testerrortimer
 * No op if KONFIG_UNITTEST is not defined.
 * In case KONFIG_UNITTEST is defined it is implemented as macro.
 * This function calls <process_testerrortimer>(errtimer) sets the variable err
 * as a result of this call and jumps to ONERROR_LABEL in case of an error. */
extern void ONERROR_testerrortimer(test_errortimer_t * errtimer, void ** ONERROR_LABEL) ;


// section: inline implementations

/* define: init_testerrortimer
 * Implements <test_errortimer_t.init_testerrortimer>. */
#define init_testerrortimer(errtimer, timercount, errcode)        \
   ( __extension__ ({                                             \
      *(errtimer) = (test_errortimer_t){ timercount, errcode } ;  \
      0 ;                                                         \
   }))


/* define: process_testerrortimer
 * Implements <test_errortimer_t.process_testerrortimer>. */
#define process_testerrortimer(/*test_errortimer_t * */errtimer)  \
   ( __extension__ ({                        \
      test_errortimer_t * _tm  = (errtimer); \
      int                 _err = 0 ;         \
      if (    _tm->timercount                \
         && ! (-- _tm->timercount)) {        \
         _err = _tm->errcode ;               \
      }                                      \
                                             \
      _err ;                                 \
   }))

/* define: ONERROR_testerrortimer
 * Implements <test_errortimer_t.ONERROR_testerrortimer>.
 * This function is a no op in case KONFIG_UNITTEST is not defined. */
#ifdef KONFIG_UNITTEST
#define ONERROR_testerrortimer(errtimer, ONERROR_LABEL)     \
      do {                                                  \
         err = process_testerrortimer(errtimer) ;           \
         if (err) goto ONERROR_LABEL ;                      \
      } while(0)
#else
#define ONERROR_testerrortimer(errtimer, ONERROR_LABEL)     /* no op */
#endif

#endif
