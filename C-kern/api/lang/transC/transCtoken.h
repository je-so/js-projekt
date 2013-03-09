/* title: TransC-Token

   Lists all token IDs and their categorisation into classes.

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

   file: C-kern/api/lang/transC/transCtoken.h
    Header file <TransC-Token>.

   file: C-kern/lang/transC/transCtoken.c
    Implementation file <TransC-Token impl>.
*/
#ifndef CKERN_LANG_TRANSC_TRANSCTOKEN_HEADER
#define CKERN_LANG_TRANSC_TRANSCTOKEN_HEADER

/* typedef: struct transCtoken_t
 * Export <transCtoken_t> into global namespace. */
typedef struct transCtoken_t              transCtoken_t ;

/* enums: transCtoken_e
 * The different token types (categories).
 * Operators have for their own class ordered by precedence.
 * The higher the class number the higher their precedence.
 * Every precedence level is associated with a fixed order of evaluation
 * used between operators of the same precedence.
 *
 * The cast operator '(type)' is recognized only by the parser.
 *
 * TODO: */
enum transCtoken_e {
   transCtoken_UNKNOWN = 0,
   transCtoken_BLOCK,      // '{', '}'
   transCtoken_SEMICOLON,  // ';'
   transCtoken_NEWSYMBOL,  // new symbol whose identifier is not known
   transCtoken_SYMBOL,     // known variable, constant or type
   transCtoken_KEYWORD,    // break, case, continue, default, else, enum, for, goto, if, return, struct, switch, typedef, union, assert, static_assert, while, do,
   transCtoken_CONSTANT,   // 0b11, 077, 99, 0xff, 1.0, "xxx"
   transCtoken_STORAGE,    // auto, extern, register, static
   transCtoken_TYPE,       // built-in type bit, bool, byte, intXX, intmax, uintXX, uintmax, void
   transCtoken_TYPEMOD,    // const, volatile, unsigned, signed,
   transCtoken_OPERATOR
} ;

typedef enum transCtoken_e          transCtoken_e ;

/* enums: transCtoken_id_e
 * The token ID (unique number) which identifies a specific string or a class of strings.
 * Strings in this case are also called lexeme.
 * A string literal for example has many representations. An additional value attribute
 * will provide the concrete lexeme.
 *
 * TODO: */
enum transCtoken_id_e {
   transCtoken_id_UNKNOWN = 0,
   transCtoken_id_OPEN_CURLY,    // '{'
   transCtoken_id_CLOSE_CURLY,   // '}'
   // operator
   transCtoken_id_OPEN_ROUND,    // '('
   transCtoken_id_CLOSE_ROUND,   // ')'
   transCtoken_id_OPEN_SQUARE,   // '['
   transCtoken_id_CLOSE_SQUARE,  // ']'
   transCtoken_id_POINT,         // '.'
   transCtoken_id_FOLLOW,        // '->'
   transCtoken_id_CAST,          // (type)
   transCtoken_id_COMMA,         // ','
   transCtoken_id_EQUAL,         // '=='
   transCtoken_id_NOTEQUAL,      // '!='
   transCtoken_id_LESSEQUAL,     // '<='
   transCtoken_id_GREATEQUAL,    // '>='
   transCtoken_id_LESS,          // '<'
   transCtoken_id_GREATER,       // '>'
   transCtoken_id_INCREMENT,     // '++'
   transCtoken_id_DECREMENT,     // '--'
   transCtoken_id_NOT,           // '!'
   transCtoken_id_PLUS,          // '+'
   transCtoken_id_PLUS_SIGN,     // '+'
   transCtoken_id_MINUS,         // '-'
   transCtoken_id_MINUS_SIGN,    // '-'
   transCtoken_id_MULT,          // '*'
   transCtoken_id_POINTER,       // '*'
   transCtoken_id_DIV,           // '/'
   transCtoken_id_MOD,           // '%'
   transCtoken_id_AND,           // '&'
   transCtoken_id_ADDRESS,       // '&'
   transCtoken_id_XOR,           // '^'
   transCtoken_id_OR,            // '|'
   transCtoken_id_INVERS,        // '~'
   transCtoken_id_LSHIFT,        // '<<'
   transCtoken_id_RSHIFT,        // '>>'
   transCtoken_id_QUESTMARK,     // '?'
   transCtoken_id_COLON,         // ':'
   transCtoken_id_LOGICAND,      // '&&'
   transCtoken_id_LOGICOR,       // '||'
   transCtoken_id_ASSIGN,        // '='
   transCtoken_id_ASSIGN_PLUS,   // '+='
   transCtoken_id_ASSIGN_MINUS,  // '-='
   transCtoken_id_ASSIGN_MULT,   // '*='
   transCtoken_id_ASSIGN_DIV,    // '/='
   transCtoken_id_ASSIGN_MOD,    // '%='
   transCtoken_id_ASSIGN_AND,    // '&='
   transCtoken_id_ASSIGN_XOR,    // '^='
   transCtoken_id_ASSIGN_OR,     // '|='
   transCtoken_id_ASSIGN_INVERS, // '~='
   transCtoken_id_ASSIGN_LSHIFT, // '<<='
   transCtoken_id_ASSIGN_RSHIFT, // '>>='
   transCtoken_id_SIZEOF,        // sizeof
   transCtoken_id_LENGTHOF,      // lengthof
   // keywords
   transCtoken_id_ASSERT,        // assert
   transCtoken_id_BREAK,         // break
   transCtoken_id_CASE,          // case
   transCtoken_id_CONTINUE,      // continue
   transCtoken_id_DEFAULT,       // default
   transCtoken_id_DO,            // do
   transCtoken_id_ELSE,          // else
   transCtoken_id_ENUM,          // enum
   transCtoken_id_FOR,           // for
   transCtoken_id_GOTO,          // goto
   transCtoken_id_IF,            // if
   transCtoken_id_RETURN,        // return
   transCtoken_id_STATICASSERT,  // staticassert
   transCtoken_id_STRUCT,        // struct
   transCtoken_id_SWITCH,        // switch
   transCtoken_id_TYPEDEF,       // typedef
   transCtoken_id_UNION,         // union
   transCtoken_id_WHILE,         // while
   // constants, literals
   transCtoken_id_UCHAR,         // '\n', '\U01FFFFFF', '\uFFFF'
   transCtoken_id_NUMBER02,      // 0b111
   transCtoken_id_NUMBER08,      // 0777
   transCtoken_id_NUMBER10,      // 999
   transCtoken_id_NUMBER16,      // 0xFFF
   transCtoken_id_STRING,        // "..."
   // storage class
   transCtoken_id_AUTO,          // auto
   transCtoken_id_EXTERN,        // extern
   transCtoken_id_REGISTER,      // register
   transCtoken_id_STATIC,        // static
   // built-in types
   transCtoken_id_BIT,           // bit
   transCtoken_id_BOOL,          // bool
   transCtoken_id_BYTE,          // byte
   transCtoken_id_DOUBLE,        // double
   transCtoken_id_FLOAT,         // float
   transCtoken_id_INT99,         // int32, int64
   transCtoken_id_UINT99,        // uint32, uint64
   transCtoken_id_VOID,          // void
   // type modifiers
   transCtoken_id_CONST,         // const
   transCtoken_id_SIGNED,        // signed
   transCtoken_id_UNSIGNED,      // unsigned
   transCtoken_id_VOLATILE,      // volatile
} ;

typedef enum transCtoken_id_e             transCtoken_id_e ;

/* enums: transCtoken_precedence_e
 * TODO: */
enum transCtoken_precedence_e {
   // evaluation order (associativity): left to right
   // ','
   transCtoken_precedence_COMMA,
   // evaluation order (associativity): right to left
   // '=', '+=', '-=', '*=', '/=', '%=', '<<=', '>>=', '&=', '^=', '|=', '~='
   transCtoken_precedence_ASSIGN,
   // evaluation order (associativity): right to left
   // '?', ':'
   transCtoken_precedence_CONDITIONAL,
   // evaluation order (associativity): left to right
   // '||'
   transCtoken_precedence_LOGICOR,
   // evaluation order (associativity): left to right
   // '&&'
   transCtoken_precedence_LOGICAND,
   // evaluation order (associativity): left to right
   // '==', '!='
   transCtoken_precedence_COMPARE,
   // evaluation order (associativity): left to right
   // '<', '<=', '>=', '>'
   transCtoken_precedence_COMPARE2,
   // evaluation order (associativity): left to right (binary operator / determined by parsing context)
   // '+', '-'
   transCtoken_precedence_PLUS,
   // evaluation order (associativity): left to right
   // '*', '/', '%'
   transCtoken_precedence_MULT,
   // evaluation order (associativity): left to right
   // '|'
   transCtoken_precedence_OR,
   // evaluation order (associativity): left to right
   // '^'
   transCtoken_precedence_XOR,
   // evaluation order (associativity): left to right
   // '&'
   transCtoken_precedence_AND,
   // evaluation order (associativity): left to right
   // '<<', '>>'
   transCtoken_precedence_SHIFT,
   // evaluation order (associativity): right to left (unary operator / determined by parsing context)
   // '!', '~', '++', '--', '+', '-', '(type)', '*', '&', 'sizeof', 'lengthof'
   transCtoken_precedence_UNARY,
   // evaluation order (associativity): left to right
   // '[', ']', '(', ')', '.', '->'
   transCtoken_precedence_SUBSCRIPT,

   ////////////////////////////////////////
   // tokenID <-> tokenClass assignments //
   ////////////////////////////////////////

   transCtoken_id_COMMA_PRECEDENCE         = transCtoken_precedence_COMMA,

   transCtoken_id_ASSIGN_PRECEDENCE        = transCtoken_precedence_ASSIGN,
   transCtoken_id_ASSIGN_PLUS_PRECEDENCE   = transCtoken_precedence_ASSIGN,
   transCtoken_id_ASSIGN_MINUS_PRECEDENCE  = transCtoken_precedence_ASSIGN,
   transCtoken_id_ASSIGN_MULT_PRECEDENCE   = transCtoken_precedence_ASSIGN,
   transCtoken_id_ASSIGN_DIV_PRECEDENCE    = transCtoken_precedence_ASSIGN,
   transCtoken_id_ASSIGN_MOD_PRECEDENCE    = transCtoken_precedence_ASSIGN,
   transCtoken_id_ASSIGN_AND_PRECEDENCE    = transCtoken_precedence_ASSIGN,
   transCtoken_id_ASSIGN_XOR_PRECEDENCE    = transCtoken_precedence_ASSIGN,
   transCtoken_id_ASSIGN_OR_PRECEDENCE     = transCtoken_precedence_ASSIGN,
   transCtoken_id_ASSIGN_INVERS_PRECEDENCE = transCtoken_precedence_ASSIGN,
   transCtoken_id_ASSIGN_LSHIFT_PRECEDENCE = transCtoken_precedence_ASSIGN,
   transCtoken_id_ASSIGN_RSHIFT_PRECEDENCE = transCtoken_precedence_ASSIGN,

   transCtoken_id_QUESTMARK_PRECEDENCE     = transCtoken_precedence_CONDITIONAL,
   transCtoken_id_COLON_PRECEDENCE         = transCtoken_precedence_CONDITIONAL,

   transCtoken_id_LOGICOR_PRECEDENCE       = transCtoken_precedence_LOGICOR,

   transCtoken_id_LOGICAND_PRECEDENCE      =  transCtoken_precedence_LOGICAND,

   transCtoken_id_EQUAL_PRECEDENCE         = transCtoken_precedence_COMPARE,
   transCtoken_id_NOTEQUAL_PRECEDENCE      = transCtoken_precedence_COMPARE,

   transCtoken_id_LESSEQUAL_PRECEDENCE     = transCtoken_precedence_COMPARE2,
   transCtoken_id_GREATEQUAL_PRECEDENCE    = transCtoken_precedence_COMPARE2,
   transCtoken_id_LESS_PRECEDENCE          = transCtoken_precedence_COMPARE2,
   transCtoken_id_GREATER_PRECEDENCE       = transCtoken_precedence_COMPARE2,

   transCtoken_id_PLUS_PRECEDENCE          = transCtoken_precedence_PLUS,
   transCtoken_id_MINUS_PRECEDENCE         = transCtoken_precedence_PLUS,

   transCtoken_id_MULT_PRECEDENCE          = transCtoken_precedence_MULT,
   transCtoken_id_DIV_PRECEDENCE           = transCtoken_precedence_MULT,
   transCtoken_id_MOD_PRECEDENCE           = transCtoken_precedence_MULT,

   transCtoken_id_OR_PRECEDENCE            = transCtoken_precedence_OR,

   transCtoken_id_XOR_PRECEDENCE           = transCtoken_precedence_XOR,

   transCtoken_id_AND_PRECEDENCE           = transCtoken_precedence_AND,

   transCtoken_id_LSHIFT_PRECEDENCE        = transCtoken_precedence_SHIFT,
   transCtoken_id_RSHIFT_PRECEDENCE        = transCtoken_precedence_SHIFT,


   transCtoken_id_NOT_PRECEDENCE           = transCtoken_precedence_UNARY,
   transCtoken_id_INVERS_PRECEDENCE        = transCtoken_precedence_UNARY,
   transCtoken_id_INCREMENT_PRECEDENCE     = transCtoken_precedence_UNARY,
   transCtoken_id_DECREMENT_PRECEDENCE     = transCtoken_precedence_UNARY,
   transCtoken_id_PLUS_SIGN_PRECEDENCE     = transCtoken_precedence_UNARY,
   transCtoken_id_MINUS_SIGN_PRECEDENCE    = transCtoken_precedence_UNARY,
   transCtoken_id_CAST_PRECEDENCE          = transCtoken_precedence_UNARY,
   transCtoken_id_POINTER_PRECEDENCE       = transCtoken_precedence_UNARY,
   transCtoken_id_ADDRESS_PRECEDENCE       = transCtoken_precedence_UNARY,
   transCtoken_id_SIZEOF_PRECEDENCE        = transCtoken_precedence_UNARY,
   transCtoken_id_LENGTHOF_PRECEDENCE      = transCtoken_precedence_UNARY,
   transCtoken_id_OPEN_SQUARE_PRECEDENCE   = transCtoken_precedence_SUBSCRIPT,
   transCtoken_id_CLOSE_SQUARE_PRECEDENCE  = transCtoken_precedence_SUBSCRIPT,
   transCtoken_id_OPEN_ROUND_PRECEDENCE    = transCtoken_precedence_SUBSCRIPT,
   transCtoken_id_CLOSE_ROUND_PRECEDENCE   = transCtoken_precedence_SUBSCRIPT,
   transCtoken_id_POINT_PRECEDENCE         = transCtoken_precedence_SUBSCRIPT,
   transCtoken_id_FOLLOW_PRECEDENCE        = transCtoken_precedence_SUBSCRIPT,
} ;

typedef enum transCtoken_precedence_e     transCtoken_precedence_e ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_lang_transc_transCtoken
 * Test <transCtoken_t> functionality. */
int unittest_lang_transc_transCtoken(void) ;
#endif


/* struct: transCtoken_t
 * TODO: describe type */
struct transCtoken_t {
   transCtoken_e        type ;
   struct {
      transCtoken_id_e  id ;
      union {
         uint8_t        precedence ;
      } ;
   } attr ;
} ;

// group: lifetime

/* define: transCtoken_INIT_FREEABLE
 * Static initializer. */
#define transCtoken_INIT_FREEABLE \
         { .type = transCtoken_UNKNOWN, { .id = transCtoken_id_UNKNOWN } }

/* define: transCtoken_INIT_ID
 * Static initializer.
 *
 * Parameters:
 * tokentype  - The classifying type of the token (<transCtoken_e>).
 * tokenid    - The unique id of the token (<transCtoken_id_e>). */
#define transCtoken_INIT_ID(tokentype, tokenid) \
         { .type = tokentype, .attr = { tokenid, { 0 } } }

/* define: transCtoken_INIT_OPERATOR
 * Static initializer.
 *
 * Parameters:
 * tokenid    - The unique id of the token (<transCtoken_id_e>).
 * precedence - The precedence of an operator (<transCtoken_precedence_e>). */
#define transCtoken_INIT_OPERATOR(tokenid, precedence) \
         { .type = transCtoken_OPERATOR, .attr = { tokenid, { precedence } } }


// group: query

/* function: type_transctoken
 * TODO: */
transCtoken_e type_transctoken(const transCtoken_t * token) ;

/* function: idattr_transctoken
 * TODO: */
transCtoken_id_e idattr_transctoken(const transCtoken_t * token) ;


// section: inline implementation

/* define: type_transctoken
 * Implements <transCtoken_t.type_transctoken>. */
#define type_transctoken(token) \
         ((token)->type)

/* define: idattr_transctoken
 * Implements <transCtoken_t.idattr_transctoken>. */
#define idattr_transctoken(token) \
         ((token)->attr.id)

#endif
