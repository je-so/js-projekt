/* title: Unit-Test

   Defines a functions to execute a single unit test.
   Defines a macro for implementing a unit test and to report any errors.

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

   file: C-kern/api/test/unittest.h
    Header file of <Unit-Test>.

   file: C-kern/test/unittest.c
    Implementation file <Unit-Test impl>.
*/
#ifndef CKERN_API_TEST_UNITTEST_HEADER
#define CKERN_API_TEST_UNITTEST_HEADER


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_test_unittest
 * Unittest for <TEST> macro and log functions. */
int unittest_test_unittest(void);
#endif


// struct: unittest_t
struct unittest_t;

// group: lifetime

/* function: initsingleton_unittest
 * Prepares the single instance of <unittest_t> to execute tests.
 * Parameter log_files_directory contains the directory path where all of the log files are stored.
 * The stored log files are compared to the generated log during test execution.
 * The adapter interface which is pointed to by parameter adapter has to be valid until
 * <freesingleton_unittest> is called.
 *
 * All test results (logs) are written to STDOUT.
 */
int initsingleton_unittest(const char * log_files_directory);

/* function: freesingleton_unittest
 * Frees any resources allocated with the single object of type <unittest_t>. */
int freesingleton_unittest(void);

// group: report

/* function: logf_unittest
 * Logs a formatted string. The output is truncated if the output exceeds 256 bytes. */
void logf_unittest(const char * format, ...) __attribute__ ((__format__ (__printf__, 1, 2)));

/* function: logfailed_unittest
 * Logs "<filename>:<line_number>: <msg>\n".
 * This is a thread-safe function -- all other are not !
 * If msg is set to 0 the msg is set to its default value "TEST FAILED". */
void logfailed_unittest(const char * filename, unsigned line_number, const char * msg);

/* function: logresult_unittest
 * Logs "OK\n" or "FAILED\n". */
void logresult_unittest(bool isFailed);

/* function: logrun_unittest
 * Logs "RUN %s: ". */
void logrun_unittest(const char * testname);

/* function: logsummary_unittest
 * Logs how many tests passed and how many failed. */
void logsummary_unittest(void);

// group: execute

/* function: execsingle_unittest
 * Runs a single unit test.
 * A return value of 0 indicates success, a return value != 0 indicates the test failed. */
int execsingle_unittest(const char * testname, int (*test_f)(void));

/* function: execasprocess_unittest
 * Forks a child process which runs the test function.
 * The parameter retcode is set to the value returned by test_f.
 * If test_t exits abnormally with a signal retcode is set to EINTR.
 * Also the content of the buffered errorlog is transfered via pipe
 * at the end of test_f to the calling process and printed to its errorlog.
 *
 * Use this function only within the execution of a unittest. */
int execasprocess_unittest(int (*test_f)(void), /*out*/int * retcode);

// group: macros

/* define: TEST
 * Tests CONDITION and jumps to label ONABORT in case of error.
 * If CONDITION fails an error is printed and computation continues
 * at the label ONABORT.
 *
 * Parameters:
 * CONDITION - Condition which is tested to be true.
 *
 * Usage:
 * The following demonstrates how this macro is used:
 *
 * > int unittest_module()
 * > {
 * >    type_t type = type_INIT_FREEABLE;
 * >    TEST(0 == init_type(&type));
 * >    TEST(0 == free_type(&type));
 * >    return 0; // success
 * > ONABORT:
 * >    free_type(&type);
 * >    return EINVAL; // any error code
 * > }
 * */
#define TEST(CONDITION)  \
         if ( !(CONDITION) ) {                           \
            logfailed_unittest(__FILE__, __LINE__, 0);   \
            goto ONABORT;                                \
         }


#endif
