/* title: TextPosition

   Exports <textpos_t> which describes position in text as column and line number.
   A text is a big string which contains special formatting characters.
   The only formatting character which is supported by any text function handling
   UTF-8 encoded strings is the '\n' newline characters which marks the end of a
   text line.

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

   file: C-kern/api/string/textpos.h
    Header file <TextPosition>.

   file: C-kern/string/textpos.c
    Implementation file <TextPosition impl>.
*/
#ifndef CKERN_STRING_TEXTPOS_HEADER
#define CKERN_STRING_TEXTPOS_HEADER

/* typedef: struct textpos_t
 * Export <textpos_t> into global namespace. */
typedef struct textpos_t         textpos_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_string_textpos
 * Test <textpos_t> functionality. */
int unittest_string_textpos(void) ;
#endif


/* struct: textpos_t
 * Describes position in text as column and line number.
 * The first line has the line number 1 and the beginning of
 * the text or the beginning of a new line has the column number 0.
 * After reading a newline '\n' the column number is reset to 0 and the line number is incremented.
 * Reading another character increments the column number but does not change the line number.
 * A text reader should call the functions */
struct textpos_t {
   /* variable: column
    * Column number of the last read character. This value is initialized to 0.
    * The reading of a newline character '\n' resets this value to 0 to reflect the beginning of a line. */
   size_t         column ;
   /* variable: line
    * Line number of the next unread character. This value is initialized to 1.
    * It is incremented every time a newline character '\n' is encountered. */
   size_t         line ;
   /* variable: prevlastcolumn
    * Column number of the last character on the previous line.
    * This value is a copy of <column> before it is set to 0 in function <incrline_textpos>. */
   size_t         prevlastcolumn ;
} ;

// group: lifetime

/* define: textpos_INIT
 * Static initializer. Initializes position to start of text. */
#define textpos_INIT                { 0, 1, 0 }

/* define: textpos_INIT_FREEABLE
 * Static initializer. */
#define textpos_INIT_FREEABLE       { 0, 0, 0 }

/* function: init_textpos
 * Sets column and line numbers to arbitrary values. */
void init_textpos(/*out*/textpos_t * txtpos, size_t colnr, size_t linenr) ;

/* function: free_textpos
 * Sets pos to <textpos_INIT_FREEABLE>. */
void free_textpos(textpos_t * txtpos) ;

// group: query

/* function: column_textpos
 * Returns the current column number beginning from 0. */
size_t column_textpos(const textpos_t * txtpos) ;

/* function: line_textpos
 * Returns the current line number beginning from 1. */
size_t line_textpos(const textpos_t * txtpos) ;

/* function: prevlastcolumn_textpos
 * Returns the column number of the last character of the previous line (number). */
size_t prevlastcolumn_textpos(const textpos_t * txtpos) ;


// group: change

/* function: addcolumn_textpos
 * Adds increment to the column number.
 * The incremented column number is returned. */
size_t addcolumn_textpos(textpos_t * txtpos, size_t increment) ;

/* function: incrcolumn_textpos
 * Increments the column number. */
void incrcolumn_textpos(textpos_t * txtpos) ;

/* function: incrline_textpos
 * Increments the line number. */
void incrline_textpos(textpos_t * txtpos) ;


// section: inline implementation

// group: textpos_t

/* define: addcolumn_textpos
 * Implements <textpos_t.addcolumn_textpos>. */
#define addcolumn_textpos(txtpos, increment)       ((txtpos)->column += (increment))

/* define: column_textpos
 * Implements <textpos_t.column_textpos>. */
#define column_textpos(txtpos)                     ((txtpos)->column)

/* define: free_textpos
 * Implements <textpos_t.free_textpos>. */
#define free_textpos(txtpos)                       ((void)(*(txtpos) = (textpos_t)textpos_INIT_FREEABLE))

/* define: init_textpos
 * Implements <textpos_t.init_textpos>. */
#define init_textpos(txtpos, colnr, linenr)        ((void)(*(txtpos) = (textpos_t) { colnr, linenr, 0 }))

/* define: line_textpos
 * Implements <textpos_t.line_textpos>. */
#define line_textpos(txtpos)                       ((txtpos)->line)

/* define: incrcolumn_textpos
 * Implements <textpos_t.incrcolumn_textpos>. */
#define incrcolumn_textpos(txtpos)                 ((void)(++(txtpos)->column))

/* define: incrline_textpos
 * Implements <textpos_t.incrline_textpos>. */
#define incrline_textpos(txtpos)                   \
   do {                                            \
         textpos_t * _tpos = (txtpos) ;            \
         _tpos->prevlastcolumn = _tpos->column ;   \
         _tpos->column = 0 ;                       \
         ++ _tpos->line ;                          \
   } while (0)

/* define: prevlastcolumn_textpos
 * Implements <textpos_t.prevlastcolumn_textpos>. */
#define prevlastcolumn_textpos(txtpos)             ((txtpos)->prevlastcolumn)

#endif
