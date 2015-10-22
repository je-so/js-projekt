/*
 * C header generated by textresource compiler v5
 *
 * Do not edit this file -- instead edit 'C-kern/resource/errlog.text'
 *
 */

#define _format_string_ASSERT_FAILED_ERRLOG "%s"
void _param_types_ASSERT_FAILED_ERRLOG(struct logbuffer_t * logbuffer, const char * wrong_condition);
void ASSERT_FAILED_ERRLOG(struct logbuffer_t * logbuffer, va_list vargs);
#define _format_string_ERROR_DESCRIPTION_ERRLOG "%d"
void _param_types_ERROR_DESCRIPTION_ERRLOG(struct logbuffer_t * logbuffer, int err);
void ERROR_DESCRIPTION_ERRLOG(struct logbuffer_t * logbuffer, va_list vargs);
#define _format_string_ERROR_IGNORED_ERRLOG ""
void _param_types_ERROR_IGNORED_ERRLOG(struct logbuffer_t * logbuffer);
void ERROR_IGNORED_ERRLOG(struct logbuffer_t * logbuffer, va_list vargs);
#define _format_string_FILE_FORMAT_MISSING_ENDOFLINE_ERRLOG "%s"
void _param_types_FILE_FORMAT_MISSING_ENDOFLINE_ERRLOG(struct logbuffer_t * logbuffer, const char * filename);
void FILE_FORMAT_MISSING_ENDOFLINE_ERRLOG(struct logbuffer_t * logbuffer, va_list vargs);
#define _format_string_FILE_FORMAT_WRONG_ERRLOG "%s"
void _param_types_FILE_FORMAT_WRONG_ERRLOG(struct logbuffer_t * logbuffer, const char * filename);
void FILE_FORMAT_WRONG_ERRLOG(struct logbuffer_t * logbuffer, va_list vargs);
#define _format_string_FILE_NAME_ERRLOG "%s%s"
void _param_types_FILE_NAME_ERRLOG(struct logbuffer_t * logbuffer, const char * workdir, const char * filename);
void FILE_NAME_ERRLOG(struct logbuffer_t * logbuffer, va_list vargs);
#define _format_string_FILE_CREATE_ERRLOG "%d%s%s"
void _param_types_FILE_CREATE_ERRLOG(struct logbuffer_t * logbuffer, int err, const char * workdir, const char * filename);
void FILE_CREATE_ERRLOG(struct logbuffer_t * logbuffer, va_list vargs);
#define _format_string_FILE_OPEN_ERRLOG "%d%s%s"
void _param_types_FILE_OPEN_ERRLOG(struct logbuffer_t * logbuffer, int err, const char * workdir, const char * filename);
void FILE_OPEN_ERRLOG(struct logbuffer_t * logbuffer, va_list vargs);
#define _format_string_FILE_REMOVE_ERRLOG "%d%s%s"
void _param_types_FILE_REMOVE_ERRLOG(struct logbuffer_t * logbuffer, int err, const char * workdir, const char * filename);
void FILE_REMOVE_ERRLOG(struct logbuffer_t * logbuffer, va_list vargs);
#define _format_string_FUNCTION_CALL_ERRLOG "%s%d"
void _param_types_FUNCTION_CALL_ERRLOG(struct logbuffer_t * logbuffer, const char * funcname, int err);
void FUNCTION_CALL_ERRLOG(struct logbuffer_t * logbuffer, va_list vargs);
#define _format_string_FUNCTION_EXIT_ERRLOG "%d"
void _param_types_FUNCTION_EXIT_ERRLOG(struct logbuffer_t * logbuffer, int err);
void FUNCTION_EXIT_ERRLOG(struct logbuffer_t * logbuffer, va_list vargs);
#define _format_string_FUNCTION_EXIT_FREE_RESOURCE_ERRLOG "%d"
void _param_types_FUNCTION_EXIT_FREE_RESOURCE_ERRLOG(struct logbuffer_t * logbuffer, int err);
void FUNCTION_EXIT_FREE_RESOURCE_ERRLOG(struct logbuffer_t * logbuffer, va_list vargs);
#define _format_string_FUNCTION_SYSCALL_ERRLOG "%s%d"
void _param_types_FUNCTION_SYSCALL_ERRLOG(struct logbuffer_t * logbuffer, const char * funcname, int err);
void FUNCTION_SYSCALL_ERRLOG(struct logbuffer_t * logbuffer, va_list vargs);
#define _format_string_FUNCTION_WRONG_RETURNVALUE_ERRLOG "%s%s"
void _param_types_FUNCTION_WRONG_RETURNVALUE_ERRLOG(struct logbuffer_t * logbuffer, const char * funcname, const char * wrong_value);
void FUNCTION_WRONG_RETURNVALUE_ERRLOG(struct logbuffer_t * logbuffer, va_list vargs);
#define _format_string_LOCALE_SETLOCALE_ERRLOG ""
void _param_types_LOCALE_SETLOCALE_ERRLOG(struct logbuffer_t * logbuffer);
void LOCALE_SETLOCALE_ERRLOG(struct logbuffer_t * logbuffer, va_list vargs);
#define _format_string_LOGENTRY_HEADER_ERRLOG "%zu%llu%u%s%s%d"
void _param_types_LOGENTRY_HEADER_ERRLOG(struct logbuffer_t * logbuffer, size_t threadid, uint64_t sec, uint32_t usec, const char * funcname, const char * filename, int linenr);
void LOGENTRY_HEADER_ERRLOG(struct logbuffer_t * logbuffer, va_list vargs);
#define _format_string_MEMORY_OUT_OF_ERRLOG "%zu%d"
void _param_types_MEMORY_OUT_OF_ERRLOG(struct logbuffer_t * logbuffer, size_t size, int err);
void MEMORY_OUT_OF_ERRLOG(struct logbuffer_t * logbuffer, va_list vargs);
#define _format_string_PARSEERROR_EXPECTCHAR_ERRLOG "%zu%zu%s"
void _param_types_PARSEERROR_EXPECTCHAR_ERRLOG(struct logbuffer_t * logbuffer, size_t linenr, size_t colnr, const char * chr);
void PARSEERROR_EXPECTCHAR_ERRLOG(struct logbuffer_t * logbuffer, va_list vargs);
#define _format_string_PARSEERROR_EXPECTNEWLINE_ERRLOG "%zu%zu"
void _param_types_PARSEERROR_EXPECTNEWLINE_ERRLOG(struct logbuffer_t * logbuffer, size_t linenr, size_t colnr);
void PARSEERROR_EXPECTNEWLINE_ERRLOG(struct logbuffer_t * logbuffer, va_list vargs);
#define _format_string_PROGRAM_ABORT_ERRLOG "%d"
void _param_types_PROGRAM_ABORT_ERRLOG(struct logbuffer_t * logbuffer, int err);
void PROGRAM_ABORT_ERRLOG(struct logbuffer_t * logbuffer, va_list vargs);
#define _format_string_RESOURCE_USAGE_DIFFERENT_ERRLOG ""
void _param_types_RESOURCE_USAGE_DIFFERENT_ERRLOG(struct logbuffer_t * logbuffer);
void RESOURCE_USAGE_DIFFERENT_ERRLOG(struct logbuffer_t * logbuffer, va_list vargs);
#define _format_string_STATE_WRONG_SIGHANDLER_DEFINED_ERRLOG "%s"
void _param_types_STATE_WRONG_SIGHANDLER_DEFINED_ERRLOG(struct logbuffer_t * logbuffer, const char * signame);
void STATE_WRONG_SIGHANDLER_DEFINED_ERRLOG(struct logbuffer_t * logbuffer, va_list vargs);
#define _format_string_TEST_INPARAM_FALSE_ERRLOG "%s"
void _param_types_TEST_INPARAM_FALSE_ERRLOG(struct logbuffer_t * logbuffer, const char * violated_condition);
void TEST_INPARAM_FALSE_ERRLOG(struct logbuffer_t * logbuffer, va_list vargs);
#define _format_string_TEST_INVARIANT_FALSE_ERRLOG "%s"
void _param_types_TEST_INVARIANT_FALSE_ERRLOG(struct logbuffer_t * logbuffer, const char * violated_condition);
void TEST_INVARIANT_FALSE_ERRLOG(struct logbuffer_t * logbuffer, va_list vargs);
#define _format_string_TEST_OUTPARAM_FALSE_ERRLOG "%s"
void _param_types_TEST_OUTPARAM_FALSE_ERRLOG(struct logbuffer_t * logbuffer, const char * violated_condition);
void TEST_OUTPARAM_FALSE_ERRLOG(struct logbuffer_t * logbuffer, va_list vargs);
#define _format_string_TEST_STATE_FALSE_ERRLOG "%s"
void _param_types_TEST_STATE_FALSE_ERRLOG(struct logbuffer_t * logbuffer, const char * violated_condition);
void TEST_STATE_FALSE_ERRLOG(struct logbuffer_t * logbuffer, va_list vargs);
#define _format_string_X11_DISPLAY_NOT_SET_ERRLOG ""
void _param_types_X11_DISPLAY_NOT_SET_ERRLOG(struct logbuffer_t * logbuffer);
void X11_DISPLAY_NOT_SET_ERRLOG(struct logbuffer_t * logbuffer, va_list vargs);
#define _format_string_X11_NO_CONNECTION_ERRLOG "%s"
void _param_types_X11_NO_CONNECTION_ERRLOG(struct logbuffer_t * logbuffer, const char * display_server_name);
void X11_NO_CONNECTION_ERRLOG(struct logbuffer_t * logbuffer, va_list vargs);
