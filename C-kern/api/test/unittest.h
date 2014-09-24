/* title: Unit-Test

   Defines a functions to execute a single unit test.
   Defines a macro for implementing a unit test and to report any errors.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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

// group: log
// Called from thread which executes the unit test.

/* function: logf_unittest
 * Logs a formatted string. The output is truncated if the output exceeds 256 bytes. */
void logf_unittest(const char * format, ...) __attribute__ ((__format__ (__printf__, 1, 2)));

/* function: logfailed_unittest
 * Logs "<filename>:<line_number>: TEST FAILED\n".
 * This is a thread-safe function!
 * Called from <TEST> macro. */
void logfailed_unittest(const char * filename, unsigned line_number);

/* function: logfailedf_unittest
 * Logs "<filename>:<line_number>: TEST FAILED\n<filename>:<line_number>: <format>\n".
 * The format description of the value(s) is expected in format. The value(s) is expected as last argument.
 * This is a thread-safe function! */
void logfailedf_unittest(const char * filename, unsigned line_number, const char * format, ...) __attribute__ ((__format__ (__printf__, 3, 4)));

/* function: logwarning_unittest
 * Logs "** <reason> ** " which should warn the user.
 * Warnings are used to indicate wrong environment conditions for example. */
void logwarning_unittest(const char * reason);

// group: reporting
// Called from unit-test engine.

/* function: logrun_unittest
 * Logs "RUN %s: ".
 * Is called from <execsingle_unittest>. */
void logrun_unittest(const char * testname);

/* function: logresult_unittest
 * Logs "OK\n" or "FAILED\n".
 * This is a thread-safe function!
 * Is called from <execsingle_unittest>. */
void logresult_unittest(bool isFailed);

/* function: logsummary_unittest
 * Logs how many tests passed and how many failed.
 * Must be called from the unit-test execution engine. */
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
 * Tests CONDITION and jumps to label ONERR in case of false.
 * If CONDITION is false an error is printed and computation continues
 * at label ONERR.
 * In case of CONDITION true nothing is done.
 *
 * Parameters:
 * CONDITION - Condition which is tested to be true.
 *
 * Usage:
 * The following demonstrates how this macro is used:
 *
 * > int unittest_module()
 * > {
 * >    type_t type = type_FREE;
 * >    TEST(0 == init_type(&type));
 * >    TEST(0 == free_type(&type));
 * >    return 0; // success
 * > ONERR:
 * >    free_type(&type);
 * >    return EINVAL; // any error code
 * > }
 * */
#define TEST(CONDITION)  \
         if ( !(CONDITION) ) {      \
            logfailed_unittest(     \
               __FILE__, __LINE__); \
            goto ONERR;             \
         }

/* define: TESTP
 * Tests CONDITION and jumps to label ONERR in case of false.
 * If CONDITION is false an error is printed and computation continues
 * at label ONERR. The printed error contains the formatted output string
 * In case of CONDITION true nothing is done.
 *
 * Parameters:
 * CONDITION - Condition which is tested to be true.
 * FORMAT    - printf format type string of the values to be printed.
 * ARGS      - Parameter values matching the types in the format string.
 *
 * Usage:
 * The following demonstrates how this macro is used:
 *
 * > int unittest_module()
 * > {
 * >    type_t type = type_FREE;
 * >    int r;
 * >    TESTP(0 == (r = init_type(&type)), "%d", r);
 * >    TESTP(0 == (r = free_type(&type)), "%d", r);
 * >    return 0; // success
 * > ONERR:
 * >    free_type(&type);
 * >    return EINVAL;
 * > }
 * */
#define TESTP(CONDITION, FORMAT, ARGS) \
         if ( !(CONDITION) ) {         \
            logfailedf_unittest(       \
            __FILE__, __LINE__,        \
            FORMAT, ARGS);             \
            goto ONERR;                \
         }                             \

#endif
