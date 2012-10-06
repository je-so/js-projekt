/* title: FloatingPointUnit
   Allows to control the global floating point unit configuration (fp environment).
   A thread inherits the configuration from the creator thread at start up.
   Changes to the fpu settings are local to this thread.

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

   file: C-kern/api/math/fpu.h
    Header file <FloatingPointUnit>.

   file: C-kern/math/fpu.c
    Implementation file <FloatingPointUnit impl>.
*/
#ifndef CKERN_MATH_FPU_HEADER
#define CKERN_MATH_FPU_HEADER

/* enums: fpu_except_e
 * Exceptions a standard conforming fpu supports.
 * Exceptions are thrown by a system specific signaling mechanism.
 * Under Linux (Posix) the signal SIGFPE is generated if any <fpu_except_e>
 * occurs. At process startup the fpu configured to be silent, i.e. to ignore any
 * exceptional conditions and only set flags which can be queried.
 *
 * fpu_except_INVALID   - The invalid exception is signaled if there is no well-defined result, such as zero divided by zero, infinity minus infinity, or sqrt(-1).
 *                        The default result is a NaN (Not a Number).
 * fpu_except_DIVBYZERO - The divide-by-zero exception is signaled if a number is divided by zero.
 *                        The result is a correctly signed infinity.
 * fpu_except_OVERFLOW  - The overflow exception is signaled if the result has a larger value than the largest floating-point number that is representable.
 *                        The result is a correctly signed infinity.
 * fpu_except_UNDERFLOW - The underflow exception is signaled if the result is smaller in absolute value than the smallest
                          positive normalized floating-point number (and would lose much accuracy when represented as a denormalized number).
                          The result is a rounded number.
 * fpu_except_INEXACT   - The inexact this is signaled if the result of an operation is not exact and is a rounded.
 *                        The default result is the rounded result.
 * fpu_except_MASK_ALL  - The values of all other bit values added together.
 * */
enum fpu_except_e {
    fpu_except_INVALID   = FE_INVALID
   ,fpu_except_DIVBYZERO = FE_DIVBYZERO
   ,fpu_except_OVERFLOW  = FE_OVERFLOW
   ,fpu_except_UNDERFLOW = FE_UNDERFLOW
   ,fpu_except_INEXACT   = FE_INEXACT
   ,fpu_except_MASK_ALL  = fpu_except_INVALID|fpu_except_DIVBYZERO|fpu_except_OVERFLOW|fpu_except_UNDERFLOW|fpu_except_INEXACT
   ,fpu_except_MASK_ERR  = fpu_except_INVALID|fpu_except_DIVBYZERO|fpu_except_OVERFLOW
} ;

typedef enum fpu_except_e              fpu_except_e ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_math_fpu
 * Tests configuration interface of floating point unit. */
int unittest_math_fpu(void) ;
#endif


// section: fpuexcept

// group: query

/* function: getsignaled_fpuexcept
 * Returns signaled interrupt flags fpu register value.
 * For every raised exception the corresponding bit in the return value is set.
 * See <fpu_except_e> for a list of all possible bits.
 * For every enumerated value in <fpu_except_e> this function can return its value only
 * if it was disabled (not throwing exception). For enabled exceptions an interrupt is generated.
 * Once an exception was signaled the bit is sticky and can only be cleared with a call to <clear_fpuexcept>. */
fpu_except_e getsignaled_fpuexcept(fpu_except_e exception_mask) ;

/* function: getenabled_fpuexcept
 * Returns interrupt mask fpu register value.
 * For every enabled exception the corresponding bit is set in the return value of this function.
 * If an enabled exception occurs an interrupt is generated and the thread is stopped.
 * Exceptions can be enabled with a call to <enable_fpuexcept> and disabled with a call to <disable_fpuexcept>.
 * Disabled exceptions do not generate any interrupt but silently set a signaled bit in case they occured.
 * The bits can be read with a call to <getsignaled_fpuexcept>. */
fpu_except_e getenabled_fpuexcept(void) ;

// group: enable

/* function: enable_fpuexcept
 * Enables a fpu exception to generate an interrupt.
 * The parameter *exception_flags* can be used to enable more than one exception
 * by oring together the values from <fpu_except_e>. Once enabled this status for an exception
 * can only be changed by calling <disable_fpuexcept>.
 * If an enabled exception occurs an interrupt is generated and the thread is stopped. */
int enable_fpuexcept(fpu_except_e exception_flags) ;

/* function: disable_fpuexcept
 * Disables a fpu exception to be silent.
 * Once disabled this status for an exception can only be changed by calling <enable_fpuexcept>.
 * Disabled exceptions do not generate any interrupt but silently set a signaling bit in case they occur.
 * The signaled bits can be read with a call to <getsignaled_fpuexcept>. */
int disable_fpuexcept(fpu_except_e exception_flags) ;

// group: signal

/* function: signal_fpuexcept
 * Signals an exceptional condition.
 * More than one exception can be signaled by oring together different <fpu_except_e>.
 * In case of an enabled exception an interrupt is generated else the corresponding signaled bit
 * is set which can be read by calling <getsignaled_fpuexcept>. */
int signal_fpuexcept(fpu_except_e exception_flags) ;

/* function: clear_fpuexcept
 * Clears the flags indicating an exceptional condition.
 * More than one exceptional state can be cleared by oring together different <fpu_except_e>.
 * This function is only useful for disabled exceptions to clear the bits returned by a call to
 * <getsignaled_fpuexcept>. */
int clear_fpuexcept(fpu_except_e exception_flags) ;


// section: inline implementation

/* define: getenabled_fpuexcept
 * Implements <fpuexcept.getenabled_fpuexcept>. */
#define  getenabled_fpuexcept()                 ((fpu_except_e) fegetexcept())

/* define: getsignaled_fpuexcept
 * Implements <fpuexcept.getsignaled_fpuexcept>. */
#define getsignaled_fpuexcept(exception_mask)   ((fpu_except_e) fetestexcept(exception_mask))


#endif
