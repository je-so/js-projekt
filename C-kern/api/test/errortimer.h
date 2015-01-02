/* title: Test-Errortimer
   Implements simple count down until error code is returned.
   All exported functions are implemented inline.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
int unittest_test_errortimer(void) ;
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

// group: lifetime

/* define: test_errortimer_FREE
 * Static initializer. Initializes timer disarmed. */
#define test_errortimer_FREE { 0, 0 }

/* function: init_testerrortimer
 * Inits errtimer with timercount and errcode.
 *
 * Parameter:
 * errtimer    - Pointer to object of type <test_errortimer_t> which is initialized.
 * timercount  - The number of times after <process_testerrortimer> returns an error.
 *               A value of 0 disables the timer.
 * errcode     - The errorcode <process_testerrortimer> returns in timer has fired.
 * */
void init_testerrortimer(/*out*/test_errortimer_t * errtimer, uint32_t timercount, int errcode) ;

/* function: free_testerrortimer
 * Sets errtimer to <test_errortimer_FREE>. */
void free_testerrortimer(test_errortimer_t * errtimer) ;

// group: query

/* function: isenabled_testerrortimer
 * Returns true if timer has not fired. */
bool isenabled_testerrortimer(const test_errortimer_t * errtimer) ;

/* function: errcode_testerrortimer
 * Returns the error code of the timer.
 * The returned value is the one set with <init_testerrortimer>
 * independent if the timer is enabled or not. */
int errcode_testerrortimer(const test_errortimer_t * errtimer) ;

// group: update

/* function: process_testerrortimer
 * Returns error if timer has elapsed else 0.
 * If <test_errortimer_t.timercount> is 0, nothing is done and 0 is returned.
 * Else timercount is decremented. If timercount is zero after the decrement
 * <test_errortimer_t.errcode> is returned else 0 is returned. */
int process_testerrortimer(test_errortimer_t * errtimer) ;

/* function: ONERROR_testerrortimer
 * No op if KONFIG_UNITTEST is not defined.
 * This function calls <process_testerrortimer>(errtimer) and if it returns 0 it does nothing.
 * Else it sets the variable err to the returned code and jumps to ONERROR_LABEL. */
void ONERROR_testerrortimer(test_errortimer_t* errtimer, /*err*/int* err, IDNAME ONERROR_LABEL);

/* function: PROCESS_testerrortimer
 * This function calls <process_testerrortimer>(errtimer) and returns its value.
 * Returns always 0 if KONFIG_UNITTEST is not defined. */
int PROCESS_testerrortimer(test_errortimer_t * errtimer);

/* function: SETONERROR_testerrortimer
 * No op if KONFIG_UNITTEST is not defined.
 * This function calls <process_testerrortimer>(errtimer) and sets
 * the variable err if the call returned an error else err is not changed. */
void SETONERROR_testerrortimer(test_errortimer_t * errtimer, /*err*/int * err) ;


// section: inline implementation

/* define: errcode_testerrortimer
 * Implements <test_errortimer_t.errcode_testerrortimer>. */
#define errcode_testerrortimer(errtimer) \
         ((errtimer)->errcode)

/* define: free_testerrortimer
 * Implements <test_errortimer_t.free_testerrortimer>. */
#define free_testerrortimer(errtimer)  \
         ((void)(*(errtimer) = (test_errortimer_t) test_errortimer_FREE))

/* define: isenabled_testerrortimer
 * Implements <test_errortimer_t.isenabled_testerrortimer>. */
#define isenabled_testerrortimer(errtimer)   \
         ((errtimer)->timercount > 0)

/* define: init_testerrortimer
 * Implements <test_errortimer_t.init_testerrortimer>. */
#define init_testerrortimer(errtimer, timercount, errcode)  \
         ((void) (*(errtimer) = (test_errortimer_t){ timercount, errcode }))

/* define: process_testerrortimer
 * Implements <test_errortimer_t.process_testerrortimer>. */
#define process_testerrortimer(errtimer)              \
         ( __extension__ ({                           \
            test_errortimer_t * _tm  = (errtimer) ;   \
            int                 _err ;                \
            if (  _tm->timercount                     \
                  && ! (-- _tm->timercount)) {        \
               _err = _tm->errcode ;                  \
            } else {                                  \
               _err = 0 ;                             \
            }                                         \
            _err ;                                    \
         }))

#ifdef KONFIG_UNITTEST

/* define: ONERROR_testerrortimer
 * Implements <test_errortimer_t.ONERROR_testerrortimer>. */
#define ONERROR_testerrortimer(errtimer, err, ONERROR_LABEL)  \
         do {                                            \
            typeof(err) _eret = (err);                   \
            int   _err2;                                 \
            _err2 = process_testerrortimer(errtimer);    \
            if (_err2) {                                 \
               *_eret = _err2;                           \
               goto ONERROR_LABEL;                       \
            }                                            \
         } while(0)

/* define: PROCESS_testerrortimer
 * Implements <test_errortimer_t.PROCESS_testerrortimer>. */
#define PROCESS_testerrortimer(errtimer) \
         (process_testerrortimer(errtimer))

/* define: SETONERROR_testerrortimer
 * Implements <test_errortimer_t.SETONERROR_testerrortimer>. */
#define SETONERROR_testerrortimer(errtimer, err)         \
         do {                                            \
            typeof(err) _eret = (err) ;                  \
            int   _err2 ;                                \
            _err2 = process_testerrortimer(errtimer) ;   \
            if (_err2) *_eret = _err2 ;                  \
         } while(0)

#else

#define ONERROR_testerrortimer(errtimer, err, ONERROR_LABEL) \
         /* no op */

#define PROCESS_testerrortimer(errtimer) \
         (0)

#define SETONERROR_testerrortimer(errtimer, err) \
         /* no op */
#endif


#endif
