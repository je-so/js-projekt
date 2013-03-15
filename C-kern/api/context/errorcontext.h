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
 * Called from <init_maincontext>. */
int initonce_errorcontext(/*support generic type*/void * errcontext) ;

/* function: freeonce_locale
 * Called from <free_maincontext>. */
int freeonce_errorcontext(/*support generic type*/void * errcontext) ;

// group: lifetime

/* define: errorcontext_INIT_FREEABLE
 * Static initializer. */
#define errorcontext_INIT_FREEABLE      { 0, 0 }

/* function: init_errorcontext
 * Initializes errcontext with static system error string table. */
int init_errorcontext(/*out*/errorcontext_t * errcontext) ;

/* function: free_errorcontext
 * Sets members of errcontext to 0. */
int free_errorcontext(errorcontext_t * errcontext) ;

// group: query

/* function: maxsyserrnum_errorcontext
 * Returns the number of entries*/
uint16_t maxsyserrnum_errorcontext(void) ;

/* function: string_errorcontext
 * Returns the error description as string for error errnum.
 * The value of errnum is set to the value returned in errno. */
const uint8_t * string_errorcontext(const errorcontext_t * errcontext, int errnum) ;

// group: generic

/* function: genericcast_errorcontext
 * Casts a pointer to generic object into pointer to <errorcontext_t>.
 * The object must have members with the same name and type as
 * <errorcontext_t> and in the same order. */
errorcontext_t * genericcast_errorcontext(void * object) ;


// section: inline implementation

/* define: initonce_errorcontext
 * Implements <errorcontext_t.initonce_errorcontext>. */
#define initonce_errorcontext(errcontext) \
         (init_errorcontext(genericcast_errorcontext(errcontext)))

/* define: freeonce_errorcontext
 * Implements <errorcontext_t.freeonce_errorcontext>. */
#define freeonce_errorcontext(errcontext) \
         (free_errorcontext(genericcast_errorcontext(errcontext)))

/* define: freeonce_errorcontext
 * Implements <errorcontext_t.freeonce_errorcontext>. */
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

/* define: string_errorcontext
 * Implements <errorcontext_t.string_errorcontext>. */
#define string_errorcontext(errcontext, errnum)                \
         ( __extension__ ({                                    \
            const errorcontext_t * _errcontext = (errcontext); \
            unsigned _errnum = (unsigned) (errnum) ;           \
            _errcontext->strdata                               \
               + _errcontext->stroffset[                       \
                     (uint8_t)(_errnum)                        \
                     | (_errnum >= 255 ? 255 : 0)] ;           \
         }))

#endif