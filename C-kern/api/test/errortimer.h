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

/* typedef: test_errortimer_t
 * Export <test_errortimer_t>. */
typedef struct test_errortimer_t    test_errortimer_t ;

// section: Functions

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

/* define: test_errortimer_INIT
 * Static initializer. */
#define test_errortimer_INIT    { 0, 0 }

/* function: init_testerrortimer
 * Inits <test_errortimer_t> with timercount and errcode.
 *
 * Parameter:
 * errtimer    - Pointer to object of type <test_errortimer_t> which is initialized.
 * timercount  - The number of times <process_testerrortimer> returns success (0).
 * errcode     - The errorcode <process_testerrortimer> returns in case timercount
 *               has reached zero.
 * */
extern int init_testerrortimer(/*out*/test_errortimer_t * errtimer, uint32_t timercount, int errcode) ;

/* function: process_testerrortimer
 * Returns 0 if <test_errortimer_t.timercount> in has not reached zero.
 * And decrements timercount.
 * Returns <test_errortimer_t.errcode> if timercount has reached zero.
 * In this case timercount is not changed.
 * The timer is in the fired state as long as it not reinitalized. */
extern int process_testerrortimer(test_errortimer_t * errtimer) ;


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
      test_errortimer_t * _tm = (errtimer) ; \
      int                 _err ;             \
      if (_tm->timercount) {                 \
      -- _tm->timercount;                    \
         _err = 0 ;                          \
      } else {                               \
         _err = _tm->errcode ;               \
      }                                      \
                                             \
      _err ;                                 \
   }))

#endif
