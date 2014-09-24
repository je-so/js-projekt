/* title: ValidationTest
   Test for input and output parameters, for object state, which is only internal
   or external visible.

   Add these tests to your implementation so that software defects are detected
   before they can do any harm to your persistent data.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2012 Jörg Seebohn

   file: C-kern/api/test/validate.h
    Header file <ValidationTest>.
*/
#ifndef CKERN_TEST_VALIDATE_HEADER
#define CKERN_TEST_VALIDATE_HEADER


/* define: VALIDATE_INPARAM_TEST
 * Validates condition on input parameter.
 * In case of condition is wrong the error variable *err* is set to
 * EINVAL and execution continues at the given label *_ONERROR_LABEL*.
 *
 * Parameter:
 * _CONDITION        - The condition to check for. Example "only positive numbers allowed as input": (x > 0)
 * _ONERROR_LABEL    - The label to jump to if condition is wrong.
 * _LOG_VALUE        - The log command to log the value of the wrong parameter.
 * */
#define VALIDATE_INPARAM_TEST(_CONDITION,_ONERROR_LABEL,_LOG_VALUE)  \
         if (!(_CONDITION)) {                                        \
            err = EINVAL ;                                           \
            TRACE_ERRLOG(  log_flags_START, TEST_INPARAM_FALSE,      \
                           err, #_CONDITION) ;                       \
            _LOG_VALUE ;                                             \
            goto _ONERROR_LABEL ;                                    \
         }

/* define: VALIDATE_OUTPARAM_TEST
 * Validates values returned as return stmt or as out param.
 * In case of condition is wrong the error variable *err* is set to
 * EINVAL and execution continues at the given label *_ONERROR_LABEL*.
 *
 * Parameter:
 * _CONDITION        - The condition to check for. Example: (isvalid_file(&file))
 * _ONERROR_LABEL    - The label to jump to if condition is wrong.
 * _LOG_VALUE        - The log command to log the value of the wrong parameter.
 * */
#define VALIDATE_OUTPARAM_TEST(_CONDITION,_ONERROR_LABEL,_LOG_VALUE) \
         if (!(_CONDITION)) {                                        \
            err = EINVAL ;                                           \
            TRACE_ERRLOG(  log_flags_START, TEST_OUTPARAM_FALSE,     \
                           err, #_CONDITION) ;                       \
            _LOG_VALUE ;                                             \
            goto _ONERROR_LABEL ;                                    \
         }

/* define: VALIDATE_INVARIANT_TEST
 * Validates that internal invariant object state is valid.
 * In case of wrong _CONDITION the error variable *err* is set to
 * EINVARIANT and execution continues at *_ONERROR_LABEL*.
 *
 * This test ensures that internal data structures are valid and not corrupted.
 * For example a binary tree data structure defines the invariant condition
 * that for any node in the tree all nodes in the right subtree are greater than
 * and all nodes in the left subtree are smaller than itself. A violated invariant
 * is a strong inidcation for a software defect.
 *
 * Parameter:
 * _CONDITION        - The condition to check for. Example: (0 == invariant_tree(&tree))
 * _ONERROR_LABEL    - The label to jump to if condition is wrong.
 * _LOG_VALUE        - The log command to log the value of the wrong parameter.
 * */
#define VALIDATE_INVARIANT_TEST(_CONDITION,_ONERROR_LABEL,_LOG_VALUE)   \
         if (!(_CONDITION)) {                                     \
            err = EINVARIANT ;                                    \
            TRACE_ERRLOG(  log_flags_START, TEST_INVARIANT_FALSE, \
                           err, #_CONDITION);                     \
            _LOG_VALUE ;                                          \
            goto _ONERROR_LABEL ;                                 \
         }

/* define: VALIDATE_STATE_TEST
 * Validates that the object state is valid.
 * In case of wrong _CONDITION the error variable *err* is set to
 * EPROTO and execution continues at label *_ONERROR_LABEL*.
 *
 * For any external visible state this test ensures that a component
 * is in a state in which an operation can by executed. For example reading
 * is only allowed on files being opened for reading.
 *
 * Parameter:
 * _CONDITION        - The condition to check for. Example: (isvalid_file(&file))
 * _ONERROR_LABEL    - The label to jump to if condition is wrong.
 * _LOG_VALUE        - The log command to log the value of the wrong parameter.
 * */
#define VALIDATE_STATE_TEST(_CONDITION,_ONERROR_LABEL,_LOG_VALUE) \
         if (!(_CONDITION)) {                                  \
            err = EPROTO ;                                     \
            TRACE_ERRLOG(  log_flags_START, TEST_STATE_FALSE,  \
                           err, #_CONDITION) ;                 \
            _LOG_VALUE ;                                       \
            goto _ONERROR_LABEL ;                              \
         }

#endif
