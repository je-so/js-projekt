/*
 * Generated from R(esource)TextCompiler
 */
#ifndef CKERN_API_RESOURCE_ERRORLOG_HEADER
#define CKERN_API_RESOURCE_ERRORLOG_HEADER

#define de 1
#define en 2
#if (KONFIG_LANG == de)
#define TEXTRES_ERRORLOG_ABORT_FATAL(err) \
        "Programmabbruch Fehler (err=%d)\n", err
#define TEXTRES_ERRORLOG_ABORT_ASSERT_FAILED(sWrongCondition) \
        "Assertion '%s' fehlgeschlagen.\n", sWrongCondition
#define TEXTRES_ERRORLOG_ERROR_LOCATION(sFile, uLine, sFunction) \
        "%s:%u: %s(): Fehler: ", sFile, uLine, sFunction
#define TEXTRES_ERRORLOG_FORMAT_MISSING_ENDOFLINE(name) \
        "'%s' enthält kein Zeilenendezeichen ('\n')\n", name
#define TEXTRES_ERRORLOG_FORMAT_WRONG(name) \
        "'%s' besitzt unbekanntes Format\n", name
#define TEXTRES_ERRORLOG_FUNCTION_ABORT(err) \
        "Funktions-Abbruch (err=%d)\n", err
#define TEXTRES_ERRORLOG_FUNCTION_ABORT_FREE(err) \
        "Resource Freigabefehler (err=%d)\n", err
#define TEXTRES_ERRORLOG_FUNCTION_ERROR(sFunctionname, error, sError) \
        "Aufruf '%s' meldet Fehler (err=%d): '%s'\n", sFunctionname, error, sError
#define TEXTRES_ERRORLOG_FUNCTION_SYSERR(sFunctionname, sys_errno, sError) \
        "Systemaufruf '%s' meldet Fehler (err=%d): '%s'\n", sFunctionname, sys_errno, sError
#define TEXTRES_ERRORLOG_FUNCTION_WRONG_RETURNVALUE(sFunctionname, sWrongValue) \
        "Funktion '%s' liefert falschen Rückgabewert (%s)\n", sFunctionname, sWrongValue
#define TEXTRES_ERRORLOG_LOCALE_SETLOCALE \
        "Kann die Lokalisierung mit setlocale nicht setzen\nAbhilfe: Bitte die Umgebungsvariable LC_ALL auf einen vom System unterstützten Wert setzen\n"
#define TEXTRES_ERRORLOG_LOG_ENTRY_TRUNCATED(entry_size, trunc_size) \
        "Vorheriger Logeintrag wurde gekürzt von %d auf %d Bytes\n", entry_size, trunc_size
#define TEXTRES_ERRORLOG_MEMORY_OUT_OF(size) \
        "Kein Speicher (bytes=%"PRIuSIZE")\n", size
#define TEXTRES_ERRORLOG_PARAMETER_INT_TOO_BIG(parameter_name, parameter_max) \
        "Parameter '%s' > %d\n", parameter_name, parameter_max
#define TEXTRES_ERRORLOG_RESOURCE_USAGE_DIFFERENT \
        "Ungleiche Anzahl benutzter Ressourcen\n"
#define TEXTRES_ERRORLOG_TEST_INPARAM_FALSE(sViolatedCondition) \
        "Eingabewert verletzt Bedingung (%s)\n", sViolatedCondition
#define TEXTRES_ERRORLOG_TEST_INVARIANT_FALSE(sViolatedCondition) \
        "Interne Zustandsvariablen verletzen Bedingung (%s)\n", sViolatedCondition
#define TEXTRES_ERRORLOG_TEST_OUTPARAM_FALSE(sViolatedCondition) \
        "Ausgabewert verletzt Bedingung (%s)\n", sViolatedCondition
#define TEXTRES_ERRORLOG_TEST_STATE_FALSE(sViolatedCondition) \
        "Operation nur erlaubt im Zustand (%s)\n", sViolatedCondition
#define TEXTRES_ERRORLOG_X11_DISPLAY_NOT_SET \
        "Name des X11 Display-Servers nicht bekannt\nBitte die Umgebungsvariable »DISPLAY« setzen\n"
#define TEXTRES_ERRORLOG_X11_NO_CONNECTION(display_server_name) \
        "Verbindung zu X11 Display-Server '%s' fehlgeschlagen\n", display_server_name

#elif (KONFIG_LANG == en)
#define TEXTRES_ERRORLOG_ABORT_FATAL(err) \
        "Abort process with fatal error (err=%d)\n", err
#define TEXTRES_ERRORLOG_ABORT_ASSERT_FAILED(sWrongCondition) \
        "Assertion '%s' failed.\n", sWrongCondition
#define TEXTRES_ERRORLOG_ERROR_LOCATION(sFile, uLine, sFunction) \
        "%s:%u: %s(): error: ", sFile, uLine, sFunction
#define TEXTRES_ERRORLOG_FORMAT_MISSING_ENDOFLINE(name) \
        "'%s' contains no newline ('\n')\n", name
#define TEXTRES_ERRORLOG_FORMAT_WRONG(name) \
        "'%s' has unknown format\n", name
#define TEXTRES_ERRORLOG_FUNCTION_ABORT(err) \
        "Function aborted (err=%d)\n", err
#define TEXTRES_ERRORLOG_FUNCTION_ABORT_FREE(err) \
        "All resources can not be freed (err=%d)\n", err
#define TEXTRES_ERRORLOG_FUNCTION_ERROR(sFunctionname, error, sError) \
        "Call to '%s' returned error (err=%d): '%s'\n", sFunctionname, error, sError
#define TEXTRES_ERRORLOG_FUNCTION_SYSERR(sFunctionname, sys_errno, sError) \
        "System call '%s' returned error (err=%d): '%s'\n", sFunctionname, sys_errno, sError
#define TEXTRES_ERRORLOG_FUNCTION_WRONG_RETURNVALUE(sFunctionname, sWrongValue) \
        "Function '%s' returned wrong value (%s)\n", sFunctionname, sWrongValue
#define TEXTRES_ERRORLOG_LOCALE_SETLOCALE \
        "Cannot change the current locale with setlocale\nRemedy: Please set environment variable LC_ALL to a supported value by the system\n"
#define TEXTRES_ERRORLOG_LOG_ENTRY_TRUNCATED(entry_size, trunc_size) \
        "Previous log entry truncated from %d to %d bytes\n", entry_size, trunc_size
#define TEXTRES_ERRORLOG_MEMORY_OUT_OF(size) \
        "Out of memory (bytes=%"PRIuSIZE")\n", size
#define TEXTRES_ERRORLOG_PARAMETER_INT_TOO_BIG(parameter_name, parameter_max) \
        "Parameter '%s' > %d\n", parameter_name, parameter_max
#define TEXTRES_ERRORLOG_RESOURCE_USAGE_DIFFERENT \
        "The number of used resources is different\n"
#define TEXTRES_ERRORLOG_TEST_INPARAM_FALSE(sViolatedCondition) \
        "Function input violates condition (%s)\n", sViolatedCondition
#define TEXTRES_ERRORLOG_TEST_INVARIANT_FALSE(sViolatedCondition) \
        "Internal state variables violate condition (%s)\n", sViolatedCondition
#define TEXTRES_ERRORLOG_TEST_OUTPARAM_FALSE(sViolatedCondition) \
        "Function output violates condition (%s)\n", sViolatedCondition
#define TEXTRES_ERRORLOG_TEST_STATE_FALSE(sViolatedCondition) \
        "Operation allowed only in state (%s)\n", sViolatedCondition
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
