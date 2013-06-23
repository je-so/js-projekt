/*
 * C header generated by textresource compiler v3
 *
 * Do not edit this file -- instead edit 'C-kern/resource/errlog.text'
 *
 */

int ABORT_FATAL_ERRLOG(log_channel_e channel) ;
int ABORT_ASSERT_FAILED_ERRLOG(log_channel_e channel, const char * wrong_condition) ;
int ERROR_LOCATION_ERRLOG(log_channel_e channel, size_t thread_id, const char * funcname, const char * filename, int linenr, int err) ;
int FILE_FORMAT_MISSING_ENDOFLINE_ERRLOG(log_channel_e channel, const char * filename) ;
int FILE_FORMAT_WRONG_ERRLOG(log_channel_e channel, const char * filename) ;
int FUNCTION_ABORT_ERRLOG(log_channel_e channel) ;
int FUNCTION_ABORT_FREE_ERRLOG(log_channel_e channel) ;
int FUNCTION_ERROR_ERRLOG(log_channel_e channel, const char * funcname, const char * errstr) ;
int FUNCTION_SYSERR_ERRLOG(log_channel_e channel, const char * funcname, const char * errstr) ;
int FUNCTION_WRONG_RETURNVALUE_ERRLOG(log_channel_e channel, const char * funcname, const char * wrong_value) ;
int LOCALE_SETLOCALE_ERRLOG(log_channel_e channel) ;
int LOG_ENTRY_TRUNCATED_ERRLOG(log_channel_e channel, int before_size, int after_size) ;
int MEMORY_OUT_OF_ERRLOG(log_channel_e channel, size_t size) ;
int PARSEERROR_EXPECTCHAR_ERRLOG(log_channel_e channel, size_t linenr, size_t colnr, const char * chr) ;
int PARSEERROR_EXPECTNEWLINE_ERRLOG(log_channel_e channel, size_t linenr, size_t colnr) ;
int RESOURCE_USAGE_DIFFERENT_ERRLOG(log_channel_e channel) ;
int TEST_INPARAM_FALSE_ERRLOG(log_channel_e channel, const char * violated_condition) ;
int TEST_INVARIANT_FALSE_ERRLOG(log_channel_e channel, const char * violated_condition) ;
int TEST_OUTPARAM_FALSE_ERRLOG(log_channel_e channel, const char * violated_condition) ;
int TEST_STATE_FALSE_ERRLOG(log_channel_e channel, const char * violated_condition) ;
int X11_DISPLAY_NOT_SET_ERRLOG(log_channel_e channel) ;
int X11_NO_CONNECTION_ERRLOG(log_channel_e channel, const char * display_server_name) ;
