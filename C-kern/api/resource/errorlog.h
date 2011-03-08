/*
 * Generated from R(esource)TextCompiler
 */
#ifndef CKERN_API_RESOURCE_ERRORLOG_HEADER
#define CKERN_API_RESOURCE_ERRORLOG_HEADER

#define de 1
#define en 2
#if (KONFIG_LANG == de)
#define CONDITION_EXPECTED(sCondition) \
        "In %s:%d: %s\nDie Bedingung (%s) ist nicht wahr (interner Programmfehler)\n", __FILE__, __LINE__, __FUNCTION__, sCondition
#define CONTEXT_INFO(sContext) \
        "Bezogen auf den Kontext '%s'\n", sContext
#define DIRECTORY_ERROR_TOO_MANY_LEADING_DOTS(checked_path, basedir) \
        "Pfad '%s' enthält zuviel führende '../'\nrelativ zum Pfad '%s'\n", checked_path, basedir
#define DIRECTORY_ERROR_CONTAINS_BAD_DOTS(checked_path) \
        "Pfad '%s' enthält '.', '//' bzw. '..'\n", checked_path
#define FORMAT_MISSING_ENDOFLINE(name) \
        "'%s' enthält kein Zeilenendezeichen ('\n')\n", name
#define FORMAT_WRONG(name) \
        "'%s' besitzt unbekanntes Format\n", name
#define FUNCTION_ABORT(file, function, err) \
        "%s: Funktions-Abbruch '%s' (err=%d)\n", file, function, err
#define FUNCTION_ABORT_FREE(err) \
        "%s:%d: %s\nResource Freigabefehler (err=%d)\n", __FILE__, __LINE__, __FUNCTION__, err
#define FUNCTION_SYSERROR(function_name, function_errno, function_strerrno) \
        "Funktion '%s' erzeugte Fehler(%d): '%s'\n", function_name, function_errno, function_strerrno
#define LOCALE_SETLOCALE \
        "Kann die Lokalisierung mit setlocale nicht setzen\nAbhilfe: Bitte die Umgebungsvariable LC_ALL auf einen vom System unterstützten Wert setzen\n"
#define MEMORY_ERROR_OUT_OF(size) \
        "Kein Speicher (%"PRIuSIZE" bytes)!\n", size
#define PARAMETER_INT_TOO_BIG(parameter_name, parameter_max) \
        "Parameter '%s' > %d\n", parameter_name, parameter_max
#define PROCESS_TERMINATED_ABNORMAL(process_name, exit_code) \
        "Prozess '%s' abnormal beendet (exit wert=%d)\n", process_name, exit_code
#define PROCESS_TERMINATED_WITH_ERROR(process_name, exit_code) \
        "Prozess '%s' beendet mit Fehlercode '%d'\n", process_name, exit_code
#define QUEUE_BUSY(module_name) \
        "Anfrage-Zwischenspeicher voll (in Modul='%s')\n", module_name
#define QUEUE_NOT_ALL_CANCELED(module_name) \
        "Nicht alle Anfragen konnten erfolgreich aus dem System entfernt werden (in Modul='%s')\n", module_name

#elif (KONFIG_LANG == en)
#define CONDITION_EXPECTED(sCondition) \
        "In %s:%d: %s\nExpected condition (%s) to be true (internal program error)\n", __FILE__, __LINE__, __FUNCTION__, sCondition
#define CONTEXT_INFO(sContext) \
        "In relation to following context '%s'\n", sContext
#define DIRECTORY_ERROR_TOO_MANY_LEADING_DOTS(checked_path, basedir) \
        "path '%s' contains to many leading '../'\nrelative to path '%s'\n", checked_path, basedir
#define DIRECTORY_ERROR_CONTAINS_BAD_DOTS(checked_path) \
        "path '%s' contains '.', '//' or '..'\n", checked_path
#define FORMAT_MISSING_ENDOFLINE(name) \
        "'%s' contains no newline ('\n')\n", name
#define FORMAT_WRONG(name) \
        "'%s' has unknown format\n", name
#define FUNCTION_ABORT(file, function, err) \
        "%s: Function '%s' aborted (err=%d)\n", file, function, err
#define FUNCTION_ABORT_FREE(err) \
        "%s:%d: %s\nResource can not be freed (err=%d)\n", __FILE__, __LINE__, __FUNCTION__, err
#define FUNCTION_SYSERROR(function_name, function_errno, function_strerrno) \
        "Function '%s' returned error(%d): '%s'\n", function_name, function_errno, function_strerrno
#define LOCALE_SETLOCALE \
        "Cannot change the current locale with setlocale\nRemedy: Please set environment variable LC_ALL to a supported value by the system\n"
#define MEMORY_ERROR_OUT_OF(size) \
        "Out of memory (%"PRIuSIZE" bytes)!\n", size
#define PARAMETER_INT_TOO_BIG(parameter_name, parameter_max) \
        "Parameter '%s' > %d\n", parameter_name, parameter_max
#define PROCESS_TERMINATED_ABNORMAL(process_name, exit_code) \
        "Process '%s' terminated abnormal (exit value=%d)\n", process_name, exit_code
#define PROCESS_TERMINATED_WITH_ERROR(process_name, exit_code) \
        "Process '%s' terminated with error code '%d'\n", process_name, exit_code
#define QUEUE_BUSY(module_name) \
        "Request-Queue is busy (in module='%s')\n", module_name
#define QUEUE_NOT_ALL_CANCELED(module_name) \
        "Not all requests could be canceled from queue (in module='%s')\n", module_name

#else
#error unsupported language
#endif
#undef de
#undef en

#endif
