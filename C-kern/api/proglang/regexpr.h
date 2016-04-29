/* title: RegularExpression

   Erstellt aus einer textuellen Beschreibung eines
   regulären Ausdrucks eine strukturelle Beschreibung <regexpr_t>.

   Siehe <regexpr_t> für eine Definition von regülaren Ausdrücken.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: C-kern/api/proglang/regexpr.h
    Header file <RegularExpression>.

   file: C-kern/proglang/regexpr.c
    Implementation file <RegularExpression impl>.
*/
#ifndef CKERN_PROGLANG_REGEXPR_HEADER
#define CKERN_PROGLANG_REGEXPR_HEADER

/* typedef: struct regexpr_t
 * Export <regexpr_t> into global namespace. */
typedef struct regexpr_t regexpr_t;

/* typedef: struct regexpr_node_t
 * Export <regexpr_node_t> into global namespace. */
typedef struct regexpr_node_t regexpr_node_t;

/* enums: regexpr_type_e
 * TODO: */
typedef enum regexpr_type_e {
   regexpr_type_GROUP,  /* ( followed by anything until end */
   regexpr_type_SET,    /* [ followed by many char or range until end */
   regexpr_type_SWITCH, /* lists start addr of all branches */
   regexpr_type_END,    /* marks ] or | or ) or 'end of string' */
   regexpr_type_NEXTBLOCK, /* Used to link blocks of memory together to simulate one continuous memory block of nodes */
   regexpr_type_REPEAT, /* *, +, {min, max} repeats the following node at least 0, 1, or min times */
   regexpr_type_CHAR,   /* a, b, or c*/
   regexpr_type_RANGE,  /* a-z, A-Z, or 0-9 */
} regexpr_type_e;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_proglang_regexpr
 * Test <regexpr_t> functionality. */
int unittest_proglang_regexpr(void);
#endif


/* struct: regexpr_node_t
 * TODO: */
struct regexpr_node_t {
   // group: public
   /* variable: type
    * Erstes Feld das allen Strukturen, die sich von <regexpr_node_t> ableiten,
    * gemeinsam ist. */
   uint8_t type;
};


/* struct: regexpr_char_t
 * TODO: */
struct regexpr_char_t {
   uint8_t type;
   uint8_t size;
   uint8_t chr[4/*size*/];
};


/* TODO: */
struct regexpr_range_t {
   uint8_t type;
   uint8_t size1;
   uint8_t size2;
   uint8_t chr1[4];
   uint8_t chr2[4];
};


/* struct: regexpr_t
 * TODO: describe type
 *
 * Die Funktion erzeugt ein <regexpr_t>,
 * dessen Struktur durch eine textuelle Beschreibung definiert wird.
 *
 * Der syntaktische Aufbau der textuellen Beschreibung:
 *
 * > re   = seq *( ( '|' | '&' | '&!' ) seq ) ;
 * > seq  = +( ? not ? repeat ? not atom ) ;
 * > not  = '!' ;
 * > repeat = ( '*' | '+' | '?' ) ;
 * > atom = '(' re ')' | char | set ;
 * > set  = '[' + ( char ?( '-' char ) ) ']' ;
 * > char = '.' | no-special-char | '\' ( special-char | control-code ) ;
 * > special-char = '.' | '[' | ']' | '(' | ')' | '*' | '+' | '{' | '}' ;
 * > control-code = 'n' | 'r' | 't' ;
 *
 * Erklärung der Syntax:
 *
 *
 *
 * */
struct regexpr_t {
   regexpr_node_t * root;
};


// group: lifetime

/* define: regexpr_FREE
 * Static initializer. */
#define regexpr_FREE \
         { 0 }

/* function: init_regexpr
 * Initiailisiert regex mit der strukturelle Repräsentation des durch definition in Textform definierten regulären Ausdrucks.
 * Die Syntax eines regulären Ausdruck ist in <regexpr_t> beschrieben.
 *
 * Beispiele für Parameter definition:
 * "[a-zA-Z_]*[0-9a-zA-Z_]" - Beschreibt einen Identifier, der mit einem Buchstaben oder '_' beginnt.
 * "*." - Kein oder beliebig viele Zeichen inklusive Newline.
 * "*!\n"  - Kein oder beliebig viele Zeichen außer Newline. Das '\n' wird vom Compiler
 *           in das Newline Zeichen umesetzt.
 * "*!\\n" - Kein oder beliebig viele Zeichen außer Newline. Das '\\n' wird vom Compiler
 *           in die Zeichenfolge '\' und 'n' umgesetzt. Zeichenfolgen beginnend mit '\'
 *           werden als Controlcodes (\n == Newline, \t == Tab, \r == Carriage Return) bzw.
 *           als Escapesequenz zur MAskierung der Sonderfunktion des Zeichens betrachtet
 *           (z.B. \* == Das '*' Zeichen selbst, wird nicht als Wiederholungsoperator gesehen).
 *
 * */
int init_regexpr(/*out*/regexpr_t* regex, size_t len, const char definition[len]);

/* function: free_regexpr
 * TODO: Describe Frees all associated resources. */
int free_regexpr(regexpr_t* regex);

// group: query

/* function: root_regexpr
 * Gibt Startknotenadresse des regulären Ausdrucks zurück. */
regexpr_node_t* root_regexpr(regexpr_t* regex);

// group: update



// section: inline implementation

/* define: root_regexpr
 * Implements <regexpr_t.root_regexpr>. */
#define root_regexpr(regex) \
         ((regex)->root)


#endif
