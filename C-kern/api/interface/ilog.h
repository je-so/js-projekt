/* title: Common-Log-Interface

   Uses <iobj_t> to declare opaque log interface.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/interface/ilog.h
    Header file <Common-Log-Interface>.

   file: C-kern/api/stdtypes/iobj.h
    Header file <InterfaceableObject>.

   file: C-kern/konfig.h
    Included from header <Konfiguration>.
*/
#ifndef CKERN_INTERFACE_ILOG_HEADER
#define CKERN_INTERFACE_ILOG_HEADER

// import
// need to include "C-kern/api/stdtypes/iobj.h" before this header


/* struct: ilog_t
 * Defined as <iobj_T>(log).
 * Type struct log_t and log_it are opaque.
 * struct log_it is defined in file <C-kern/api/io/log/log.h>. */
typedef iobj_T(log) ilog_t;

// group: generic

/* function: cast_ilog
 * Casts parameter iobj to pointer to <ilog_t>.
 * iobj must be a pointer to an anonymous interfaceable log object. */
#define cast_ilog(iobj) \
         cast_iobj(iobj, log)

#endif
