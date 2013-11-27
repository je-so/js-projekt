/* title: ErrorContext

   Manages string table of system errors.

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
   (C) 2013 Jörg Seebohn

   file: C-kern/api/context/errorcontext.h
    Header file <ErrorContext>.

   file: C-kern/context/errorcontext.c
    Implementation file <ErrorContext impl>.
*/
#ifndef CKERN_CONTEXT_ERRORCONTEXT_HEADER
#define CKERN_CONTEXT_ERRORCONTEXT_HEADER

/* typedef: struct errorcontext_t
 * Export <errorcontext_t> into global namespace. */
typedef struct errorcontext_t             errorcontext_t ;

/* variable: g_errorcontext_stroffset
 * Used in <processcontext_INIT_STATIC> to initialize static <processcontext_t>. */
extern uint16_t                           g_errorcontext_stroffset[] ;

/* variable: g_errorcontext_strdata
 * Used in <processcontext_INIT_STATIC> to initialize static <processcontext_t>. */
extern uint8_t                            g_errorcontext_strdata[] ;


/* enums: ckern_api_error_e
 * Application specifix error codes.
 *
 * EINVARIANT - Invariant violated. This means for example corrupt memory, a software bug.
 *
 * */
enum ckern_api_error_e {
   ckern_api_error_FIRSTERRORCODE = 256,
#define EINVARIANT   ckern_api_error_INVARIANT
   ckern_api_error_INVARIANT      = ckern_api_error_FIRSTERRORCODE,

   ckern_api_error_NEXTERRORCODE
} ;

typedef enum ckern_api_error_e            ckern_api_error_e ;

// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_context_errorcontext
 * Test <errorcontext_t> functionality. */
int unittest_context_errorcontext(void) ;
#endif


/* struct: errorcontext_t
 * Stores description of system errors in a string table.
 * Used as replacement for strerror. */
struct errorcontext_t {
   /* variable: stroffset
    * Table with offset values relative to <strdata>.
    * The tables length is equal to 256 but all entries with index > <maxsyserrnum_errorcontext>
    * share the same offset to string "Unknown error". */
   uint16_t *  stroffset ;
   /* variable: strdata
    * String table with system error descriptions in english.
    * Last byte of string is a '\0' byte (C string). */
   uint8_t  *  strdata ;
} ;

// group: initonce

/* function: initonce_errorcontext
 * Called from <init_maincontext>.
 * The parameter errcontext supports any generic object type which
 * has the same structure as <errorcontext_t>. */
int initonce_errorcontext(/*out*/errorcontext_t * error) ;

/* function: freeonce_errorcontext
 * Called from <free_maincontext>.
 * The parameter errcontext supports any generic object type which
 * has the same structure as <errorcontext_t>.
 * This operation is a no op. This ensures that errorcontext_t works
 * in an uninitialized (static) context. */
int freeonce_errorcontext(errorcontext_t * error) ;

// group: lifetime

/* define: errorcontext_INIT_FREEABLE
 * Static initializer. */
#define errorcontext_INIT_FREEABLE        { 0, 0 }

/* define: errorcontext_INIT_STATIC
 * Static initializer used in <processcontext_INIT_STATIC>. */
#define errorcontext_INIT_STATIC          { g_errorcontext_stroffset, g_errorcontext_strdata }

/* function: init_errorcontext
 * Initializes errcontext with static system error string table. */
int init_errorcontext(/*out*/errorcontext_t * errcontext) ;

/* function: free_errorcontext
 * Sets members of errcontext to 0. */
int free_errorcontext(errorcontext_t * errcontext) ;

// group: query

/* function: maxsyserrnum_errorcontext
 * Returns the number of system error entries. */
uint16_t maxsyserrnum_errorcontext(void) ;

/* function: str_errorcontext
 * Returns the error description of errnum as a null terminated C string.
 * The value of errnum should be set to the value returned in errno.
 * If errnum is < 0 or errnum > <maxsyserrnum_errorcontext> then string "Unknown error"
 * is returned. */
const uint8_t * str_errorcontext(const errorcontext_t errcontext, int errnum) ;

// group: generic

/* function: genericcast_errorcontext
 * Casts a pointer to generic object into pointer to <errorcontext_t>.
 * The object must have members with the same name and type as
 * <errorcontext_t> and in the same order. */
errorcontext_t * genericcast_errorcontext(void * object) ;


// section: inline implementation

/* define: initonce_errorcontext
 * Implements <errorcontext_t.initonce_errorcontext>. */
#define initonce_errorcontext(error) \
         (init_errorcontext(genericcast_errorcontext(error)))

/* define: freeonce_errorcontext
 * Implements <errorcontext_t.freeonce_errorcontext>. */
#define freeonce_errorcontext(error) \
         (0)

/* define: genericcast_errorcontext
 * Implements <errorcontext_t.genericcast_errorcontext>. */
#define genericcast_errorcontext(object)                       \
         ( __extension__ ({                                    \
            typeof(object) _obj = (object) ;                   \
            errorcontext_t * _errcon = 0 ;                     \
            static_assert(                                     \
               sizeof(_errcon->stroffset)                      \
               == sizeof(_obj->stroffset)                      \
               && sizeof((_errcon->stroffset)[0])              \
                  == sizeof((_obj->stroffset)[0])              \
               && offsetof(errorcontext_t, stroffset)          \
                  == offsetof(typeof(*_obj), stroffset)        \
               && sizeof(_errcon->strdata)                     \
                  == sizeof(_obj->strdata)                     \
               && sizeof(_errcon->strdata[0])                  \
                  == sizeof(_obj->strdata[0])                  \
               && offsetof(errorcontext_t, strdata)            \
                  == offsetof(typeof(*_obj), strdata),         \
               "members are compatible") ;                     \
            if (0) {                                           \
               volatile uint16_t _off ;                        \
               volatile uint8_t  _str ;                        \
               _off = _obj->stroffset[0] ;                     \
               _str = _obj->strdata[_off] ;                    \
               (void) _str ;                                   \
            }                                                  \
            (void) _errcon ;                                   \
            (errorcontext_t *)(_obj) ;                         \
         }))

/* define: maxsyserrnum_errorcontext
 * Implements <errorcontext_t.maxsyserrnum_errorcontext>. */
#define maxsyserrnum_errorcontext() \
         (132)

/* define: str_errorcontext
 * Implements <errorcontext_t.str_errorcontext>. */
#define str_errorcontext(errcontext, errnum)                   \
         ( __extension__ ({                                    \
            unsigned _errnum = (unsigned) (errnum) ;           \
            (errcontext).strdata                               \
               + (errcontext).stroffset[                       \
                     (_errnum > 511 ? 511 : _errnum)] ;        \
         }))

#endif
