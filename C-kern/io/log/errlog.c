/* title: Errorlog-Writer

   Writes error log text. This is only a wrapper module
   which includes the source file generated with text resource compiler.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2012 Jörg Seebohn

   file: C-kern/api/io/log/errlog_macros.h
    Header file of <ErrlogMacros>.

   file: C-kern/io/log/errlog.c
    Implementation file <Errorlog-Writer>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/log/logbuffer.h"
#include "C-kern/api/io/log/logcontext.h"

#define PRINTF(...)   do { (void) logcontext; printf_logbuffer(logbuffer, __VA_ARGS__); } while(0)

// generated by text resource compiler

#include STR(C-kern/resource/generated/errlog.KONFIG_LANG)
