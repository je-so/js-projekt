/*
 * Generated from R(esource)TextCompiler
 */
#ifndef CKERN_API_RESOURCE_ERRORLOG_HEADER
#define CKERN_API_RESOURCE_ERRORLOG_HEADER

#define de 1
#define en 2
#if (KONFIG_LANG == de)
#define TEXTRES_ERRORLOG_CONDITION_EXPECTED(sCondition) \
        "In %s:%d: %s\nDie Bedingung (%s) ist nicht wahr (interner Programmfehler)\n", __FILE__, __LINE__, __FUNCTION__, sCondition
#define TEXTRES_ERRORLOG_CONTEXT_INFO(sContext) \
        "Bezogen auf den Kontext '%s'\n", sContext
#define TEXTRES_ERRORLOG_ERROR_LOCATION \
        "%s:%d: Fehler in %s()\n", __FILE__, __LINE__, __FUNCTION__
#define TEXTRES_ERRORLOG_FORMAT_MISSING_ENDOFLINE(name) \
        "'%s' enthält kein Zeilenendezeichen ('\n')\n", name
#define TEXTRES_ERRORLOG_FORMAT_WRONG(name) \
        "'%s' besitzt unbekanntes Format\n", name
#define TEXTRES_ERRORLOG_FUNCTION_ABORT(err) \
        "Funktions-Abbruch (err=%d)\n", err
#define TEXTRES_ERRORLOG_FUNCTION_ABORT_FREE(err) \
        "Resource Freigabefehler (err=%d)\n", err
#define TEXTRES_ERRORLOG_FUNCTION_ERROR(function_name, function_error, function_strerror) \
        "Aufruf '%s' meldet Fehler (err=%d): '%s'\n", function_name, function_error, function_strerror
#define TEXTRES_ERRORLOG_FUNCTION_SYSERR(function_name, function_errno, function_strerrno) \
        "Systemaufruf '%s' meldet Fehler (err=%d): '%s'\n", function_name, function_errno, function_strerrno
#define TEXTRES_ERRORLOG_LOCALE_SETLOCALE \
        "Kann die Lokalisierung mit setlocale nicht setzen\nAbhilfe: Bitte die Umgebungsvariable LC_ALL auf einen vom System unterstützten Wert setzen\n"
#define TEXTRES_ERRORLOG_MEMORY_OUT_OF(size) \
        "Kein Speicher (bytes=%"PRIuSIZE")\n", size
#define TEXTRES_ERRORLOG_PARAMETER_INT_TOO_BIG(parameter_name, parameter_max) \
        "Parameter '%s' > %d\n", parameter_name, parameter_max
#define TEXTRES_ERRORLOG_PATH_CONTAINS_UNSUPPORTED_CHAR(char_string) \
        "Pfad enthält ausgeschlossenes Zeichen ('%s')\n", char_string
#define TEXTRES_ERRORLOG_PATH_CONTAINS_TOO_MANY_LEADING_DOTS(checked_path, basedir) \
        "Pfad '%s' enthält zuviel führende '../'\nrelativ zum Pfad '%s'\n", checked_path, basedir
#define TEXTRES_ERRORLOG_PATH_NOT_WELL_FORMED(checked_path) \
        "Pfad '%s' enthält '/./' oder '//' bzw. '/../'\n", checked_path
#define TEXTRES_ERRORLOG_PROCESS_TERMINATED_ABNORMAL(process_name, exit_code) \
        "Prozess '%s' abnormal beendet (exit wert=%d)\n", process_name, exit_code
#define TEXTRES_ERRORLOG_PROCESS_TERMINATED_WITH_ERROR(process_name, exit_code) \
        "Prozess '%s' beendet mit Fehlercode '%d'\n", process_name, exit_code
#define TEXTRES_ERRORLOG_X11_DISPLAY_NOT_SET \
        "Name des X11 Display-Servers nicht bekannt\nBitte die Umgebungsvariable »DISPLAY« setzen\n"
#define TEXTRES_ERRORLOG_X11_NO_CONNECTION(display_server_name) \
        "Verbindung zu X11 Display-Server '%s' fehlgeschlagen\n", display_server_name

#elif (KONFIG_LANG == en)
#define TEXTRES_ERRORLOG_CONDITION_EXPECTED(sCondition) \
        "In %s:%d: %s\nExpected condition (%s) to be true (internal program error)\n", __FILE__, __LINE__, __FUNCTION__, sCondition
#define TEXTRES_ERRORLOG_CONTEXT_INFO(sContext) \
        "In relation to following context '%s'\n", sContext
#define TEXTRES_ERRORLOG_ERROR_LOCATION \
        "%s:%d: error in %s()\n", __FILE__, __LINE__, __FUNCTION__
#define TEXTRES_ERRORLOG_FORMAT_MISSING_ENDOFLINE(name) \
        "'%s' contains no newline ('\n')\n", name
#define TEXTRES_ERRORLOG_FORMAT_WRONG(name) \
        "'%s' has unknown format\n", name
#define TEXTRES_ERRORLOG_FUNCTION_ABORT(err) \
        "Function aborted (err=%d)\n", err
#define TEXTRES_ERRORLOG_FUNCTION_ABORT_FREE(err) \
        "All resources can not be freed (err=%d)\n", err
#define TEXTRES_ERRORLOG_FUNCTION_ERROR(function_name, function_error, function_strerror) \
        "Call to '%s' returned error (err=%d): '%s'\n", function_name, function_error, function_strerror
#define TEXTRES_ERRORLOG_FUNCTION_SYSERR(function_name, function_errno, function_strerrno) \
        "System call '%s' returned error (err=%d): '%s'\n", function_name, function_errno, function_strerrno
#define TEXTRES_ERRORLOG_LOCALE_SETLOCALE \
        "Cannot change the current locale with setlocale\nRemedy: Please set environment variable LC_ALL to a supported value by the system\n"
#define TEXTRES_ERRORLOG_MEMORY_OUT_OF(size) \
        "Out of memory (bytes=%"PRIuSIZE")\n", size
#define TEXTRES_ERRORLOG_PARAMETER_INT_TOO_BIG(parameter_name, parameter_max) \
        "Parameter '%s' > %d\n", parameter_name, parameter_max
#define TEXTRES_ERRORLOG_PATH_CONTAINS_UNSUPPORTED_CHAR(char_string) \
        "Path contains unsupported character ('%s')\n", char_string
#define TEXTRES_ERRORLOG_PATH_CONTAINS_TOO_MANY_LEADING_DOTS(checked_path, basedir) \
        "path '%s' contains to many leading '../'\nrelative to path '%s'\n", checked_path, basedir
#define TEXTRES_ERRORLOG_PATH_NOT_WELL_FORMED(checked_path) \
        "path '%s' contains '/./' or '//' or '/../'\n", checked_path
#define TEXTRES_ERRORLOG_PROCESS_TERMINATED_ABNORMAL(process_name, exit_code) \
        "Process '%s' terminated abnormal (exit value=%d)\n", process_name, exit_code
#define TEXTRES_ERRORLOG_PROCESS_TERMINATED_WITH_ERROR(process_name, exit_code) \
        "Process '%s' terminated with error code '%d'\n", process_name, exit_code
#define TEXTRES_ERRORLOG_X11_DISPLAY_NOT_SET \
        "Default name of X11 display servers unknown\nPlease set environment variable »DISPLAY« setzen\n"
#define TEXTRES_ERRORLOG_X11_NO_CONNECTION(display_server_name) \
        "Can not connect with X11 display server '%s'\n", display_server_name

#else
#error unsupported language
#endif
#undef de
#undef en

#endif
