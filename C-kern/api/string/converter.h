/* title: String-Converter
   Converts multi byte strings (mbs) into wide character strings.

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

   file: C-kern/api/string/converter.h
    Header file of <String-Converter>.

   file: C-kern/string/conversion.c
    Implementation file <String-Converter impl>.
*/
#ifndef CKERN_API_STRING_CONVERTER_HEADER
#define CKERN_API_STRING_CONVERTER_HEADER

/* typedef: typedef wstring_converter_t
 * Shortcut for <wstring_converter_t>. */
typedef struct wstring_converter_t        wstring_converter_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_string_converter
 * Tests <wstring_converter_t>. */
extern int unittest_string_converter(void) ;
#endif


/* struct: wstring_converter_t
 * Object state for conversion of multibyte sequence into wide character sequence. */
struct wstring_converter_t
{
   mbstate_t   internal_state ;
   const char  * next_input_char ;
   size_t      input_len ;
} ;

// group: lifetime

/* define: wstring_converter_INIT
 * Static initializer without need to call <init_wstringconverter>.
 * > const char * mbs = "..." ;
 * > wchar_t wchar ;
 * > wstring_converter_t wconv = wstring_converter_INIT(mbs,strlen(mbs)) ;
 * > while(0 == nextwchar_wstringconverter(&wconv, &wchar)) { ... } ; */
#define wstring_converter_INIT(/*const char* */string, /*size_t*/string_len)     { .next_input_char = (string), .input_len = (string_len) }

/* define: wstring_converter_INIT_FREEABLE
 * Static initializer. After assigning it is safe to call <free_wstringconverter>. */
#define wstring_converter_INIT_FREEABLE                                          wstring_converter_INIT(0,0)

/* function: init_wstringconverter
 * Inits <wstring_converter_t> with a pointer to a mbs string. */
extern int init_wstringconverter(/*out*/wstring_converter_t * conv, size_t input_len, const char * input_string ) ;

/* function: copy_wstringconverter
 * Copies state from *source* to *dest*.
 * Make sure that dest and source points to different addresses. */
extern int copy_wstringconverter(/*out*/wstring_converter_t * restrict dest, const wstring_converter_t * restrict source ) ;

/* function: free_wstringconverter
 * Frees resources associated with <wstring_converter_t>. */
extern int free_wstringconverter( wstring_converter_t * conv ) ;

// group: query

/* function: nextinputchar_wstringconverter
 * Returns next character position of multibyte string where conversion starts. */
extern const char * nextinputchar_wstringconverter( wstring_converter_t * conv ) ;

// group: read

/* function: nextwchar_wstringconverter
 * Converts next complete mb character sequence into a wide character.
 * The resulting wide character is stored at *next_wchar. In case of an error
 * no more conversion should be done cause internal state is undefined.
 * The end of input is signaled by returning 0 and *next_wchar set to 0.
 *
 * Returns:
 * 0      - Conversion was successfull and *next_wchar contains a valid wide character.
 * EILSEQ - Conversion encountered an illegal or incomplete mb character sequence. */
extern int nextwchar_wstringconverter( wstring_converter_t * conv, wchar_t * next_wchar ) ;

/* function: skip_wstringconverter
 * Skips the next char_count characters.
 *
 * Returns:
 * 0       - The input pointer was moved forward until char_count characters were skipped.
 * EILSEQ  - Conversion encountered an illegal or incomplete mb character sequence.
 * ENODATA - The input pointer was moved forward until end of input was reached. But the number of skipped characters
 *           was less then char_count. */
extern int skip_wstringconverter( wstring_converter_t * conv, size_t char_count ) ;

/* function: peek_wstringconverter
 * Converts the next *char_count* characters without modifying *conv*.
 * If an error is encountered before all chars could be converted wchar_array is initialized partially.
 * Before return *conv* is restored to the state it was before the call to this function.
 *
 * Returns:
 * 0       - wchar_array contains char_count valid characters.
 *           If input string contains less then char_count characters the missing characters are set to L'\0'.
 * EILSEQ  - Conversion encountered an illegal or incomplete mb character sequence. */
extern int peek_wstringconverter( const wstring_converter_t * conv, size_t char_count, wchar_t * wchar_array ) ;


// section: inline implementations

/* define: free_wstringconverter
 * Implements <wstring_converter_t.free_wstringconverter>.
 * > #define free_wstringconverter(conv) \
 * >    (0) // do nothing */
#define /*int*/ free_wstringconverter(conv) \
   (0) /*do nothing*/

/* define: nextinputchar_wstringconverter
 * Implements <wstring_converter_t.nextinputchar_wstringconverter>.
 * > #define nextinputchar_wstringconverter(conv)
 * >    (conv->next_input_char) */
#define /*char **/ nextinputchar_wstringconverter(conv) \
   ((conv)->next_input_char)

/* define: nextwchar_wstringconverter
 * Implements <wstring_converter_t.nextwchar_wstringconverter>.
 * > #define nextwchar_wstringconverter( conv, next_wchar ) \
 * >    (__extension__ ({ size_t bytes = (size_t)((conv)->input_len ? mbrtowc( next_wchar, (conv)->next_input_char, (conv)->input_len, &(conv)->internal_state) : (unsigned)(*(next_wchar) = 0)) ; int result ; if (bytes > (conv)->input_len) { result = EILSEQ ; } else { (conv)->input_len -= bytes ; (conv)->next_input_char += bytes ; result = 0 ; } result ; })) */
#define /*int*/ nextwchar_wstringconverter(conv, next_wchar) \
   (__extension__ ({ size_t bytes = (size_t)((conv)->input_len ? mbrtowc( next_wchar, (conv)->next_input_char, (conv)->input_len, &(conv)->internal_state) : (unsigned)(*(next_wchar) = 0)) ; int result ; if (bytes > (conv)->input_len) { result = EILSEQ ; } else { (conv)->input_len -= bytes ; (conv)->next_input_char += bytes ; result = 0 ; } result ; }))

#endif
