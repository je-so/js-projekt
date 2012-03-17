/* title: Convert-wchar
   Converts multi byte strings (mbs) into wide character strings.

   TODO: remove module; either replace it with another implementation (see iconv(3))
         or with an external process which converts local character sets into UTF-8.

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
   (C) 2011 Jörg Seebohn

   file: C-kern/api/string/convertwchar.h
    Header file of <Convert-wchar>.

   file: C-kern/string/convertwchar.c
    Implementation file <Convert-wchar impl>.
*/
#ifndef CKERN_API_STRING_CONVERTWCHAR_HEADER
#define CKERN_API_STRING_CONVERTWCHAR_HEADER

/* typedef: struct convert_wchar_t
 * Exports <convert_wchar_t>. */
typedef struct convert_wchar_t            convert_wchar_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_string_convertwchar
 * Tests <convert_wchar_t>. */
extern int unittest_string_convertwchar(void) ;
#endif


/* struct: convert_wchar_t
 * Support conversion into wchar_t from multibyte characters.
 * This object holds the necessary state used for conversion of a multibyte character
 * sequence into the corresponding sequence of wide characters. */
struct convert_wchar_t
{
   /* variable: len
    * Stores size (in bytes) of the unconverted mb character sequence. */
   size_t         len ;
   /* variable: next
    * Points to unconverted mb character sequence of <len> bytes. */
   const char  *  next ;
   /* variable: internal_state
    * Internal state used in conversion. */
   mbstate_t      internal_state ;
} ;

// group: lifetime

/* define: convert_wchar_INIT
 * Static initializer, same as <init_convertwchar>.
 *
 * Parameter:
 * string_len - Describes length in bytes of string. It is of type (size_t).
 * string     - Points to multibyte character sequence of type (const char*).
 *
 * Example usage:
 * > const char * mbs = "..." ;
 * > wchar_t      wchar ;
 * > convert_wchar_t wconv = convert_wchar_INIT(strlen(mbs), mbs) ;
 * > while(0 == next_convertwchar(&wconv, &wchar)) { ... } ; */
#define convert_wchar_INIT(string_len, string)     { .len = (string_len), .next = (string) }

/* define: convert_wchar_INIT_FREEABLE
 * Static initializer. */
#define convert_wchar_INIT_FREEABLE                convert_wchar_INIT(0,0)

/* function: init_convertwchar
 * Inits <convert_wchar_t> with a pointer to a mbs string. */
extern int init_convertwchar(/*out*/convert_wchar_t * conv, size_t input_len, const char * input_string) ;

/* function: initcopy_convertwchar
 * Copies state from *source* to *dest*.
 * This works only if destination memory does not overlap with source memory. */
extern int initcopy_convertwchar(/*out*/convert_wchar_t * restrict dest, const convert_wchar_t * restrict source) ;

/* function: free_convertwchar
 * Frees resources associated with <convert_wchar_t>. */
extern int free_convertwchar(convert_wchar_t * conv) ;

// group: query

/* function: currentpos_convertwchar
 * Returns next character position of multibyte string where conversion starts. */
extern const char * currentpos_convertwchar(convert_wchar_t * conv) ;

// group: read

/* function: next_convertwchar
 * Converts next complete mb character sequence into a wide character.
 * The resulting wide character is stored at *next_wchar. In case of an error
 * no more conversion should be done cause internal state is undefined.
 * The end of input is signaled by returning 0 and *next_wchar set to 0.
 *
 * Returns:
 * 0      - Conversion was successfull and *next_wchar contains a valid wide character.
 * EILSEQ - Conversion encountered an illegal or incomplete mb character sequence. */
extern int next_convertwchar(convert_wchar_t * conv, wchar_t * next_wchar) ;

/* function: skip_convertwchar
 * Skips the next char_count characters. A character may be represented as more than one byte.
 *
 * Returns:
 * 0       - The input pointer was moved forward until char_count characters were skipped.
 * EILSEQ  - Conversion encountered an illegal or incomplete mb character sequence.
 * ENODATA - The input pointer was moved forward until end of input was reached. But the number of skipped characters
 *           was less then char_count. */
extern int skip_convertwchar(convert_wchar_t * conv, size_t char_count) ;

/* function: peek_convertwchar
 * Converts the next *char_count* characters without modifying *conv*.
 * If an error is encountered before all chars could be converted wchar_array is initialized partially.
 * Before return *conv* is restored to the state it was before the call to this function.
 *
 * Returns:
 * 0       - wchar_array contains char_count valid characters.
 *           If input string contains less then char_count characters the missing characters are set to L'\0'.
 * EILSEQ  - Conversion encountered an illegal or incomplete mb character sequence. */
extern int peek_convertwchar(const convert_wchar_t * conv, size_t char_count, wchar_t * wchar_array) ;


// section: inline implementations

/* define: free_convertwchar
 * Implements <convert_wchar_t.free_convertwchar> as a no op. */
#define free_convertwchar(conv)        (0)

/* define: currentpos_convertwchar
 * Implements <convert_wchar_t.currentpos_convertwchar>. */
#define currentpos_convertwchar(conv)  ((conv)->next)

/* define: next_convertwchar
 * Implements <convert_wchar_t.next_convertwchar>.
 * > #define next_convertwchar( conv, next_wchar ) \
 * >    (__extension__ ({ size_t bytes = (size_t)((conv)->input_len ? mbrtowc( next_wchar, (conv)->next_input_char, (conv)->input_len, &(conv)->internal_state) : (unsigned)(*(next_wchar) = 0)) ; int result ; if (bytes > (conv)->input_len) { result = EILSEQ ; } else { (conv)->input_len -= bytes ; (conv)->next_input_char += bytes ; result = 0 ; } result ; })) */
#define next_convertwchar(conv, next_wchar) \
   (__extension__ ({ size_t bytes = (size_t)((conv)->len ? mbrtowc( next_wchar, (conv)->next, (conv)->len, &(conv)->internal_state) : (unsigned)(*(next_wchar) = 0)) ; int _result_ ; if (bytes > (conv)->len) { _result_ = EILSEQ ; } else { (conv)->len -= bytes ; (conv)->next += bytes ; _result_ = 0 ; } _result_ ; }))

#endif
