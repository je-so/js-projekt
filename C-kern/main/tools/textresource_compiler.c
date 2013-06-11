/* title: TextResource-Compiler

   Translates text resource descriptions into C files.

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

   file: C-kern/main/tools/textresource_compiler.c
    Implementation file <TextResource-Compiler>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/ds/foreach.h"
#include "C-kern/api/ds/typeadapt.h"
#include "C-kern/api/ds/typeadapt/typeadapt_impl.h"
#include "C-kern/api/ds/inmem/arraystf.h"
#include "C-kern/api/ds/inmem/slist.h"
#include "C-kern/api/io/filesystem/directory.h"
#include "C-kern/api/io/filesystem/file.h"
#include "C-kern/api/io/filesystem/mmfile.h"
#include "C-kern/api/io/reader/utf8reader.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#include "C-kern/api/platform/startup.h"
#include "C-kern/api/string/cstring.h"
#include "C-kern/api/string/string.h"
#include "C-kern/api/string/stringstream.h"


typedef struct outconfig_t             outconfig_t ;

typedef struct textresource_t          textresource_t ;

typedef struct textresource_reader_t   textresource_reader_t ;

typedef struct textresource_writer_t   textresource_writer_t ;

typedef struct textresource_text_t     textresource_text_t ;

typedef struct textresource_parameter_t      textresource_parameter_t ;

typedef struct textresource_langref_t        textresource_langref_t ;

typedef struct textresource_condition_t      textresource_condition_t ;

typedef struct textresource_textatom_t       textresource_textatom_t ;

typedef struct textresource_paramtype_t      textresource_paramtype_t ;

typedef struct textresource_language_t       textresource_language_t ;

typedef struct xmlattribute_t                xmlattribute_t ;

enum xmltag_openclose_e {
   xmltag_OPEN,
   xmltag_CLOSE,
   xmltag_OPEN_OR_CLOSE,
} ;

typedef enum xmltag_openclose_e        xmltag_openclose_e ;

enum typemodifier_e {
   typemodifier_PLAIN    = 0,
   typemodifier_POINTER  = 1,
   typemodifier_CONST    = 2,
   typemodifier_RESERVED = 4
} ;

typedef enum typemodifier_e            typemodifier_e ;

enum textresource_textatom_e {
   textresource_textatom_STRING,
   textresource_textatom_PARAMETER
} ;

typedef enum textresource_textatom_e   textresource_textatom_e ;


// section: Helper

// group: constants

#define VERSION   "v3"

// group: log

static void print_error(const char * format, ...) __attribute__ ((__format__ (__printf__, 1, 2))) ;

/* function: print_version
 * Prints version information */
static void print_version(void)
{
   printf("Text resource " VERSION " compiler\n") ;
}

/* function: print_usage
 * Prints usage information. */
static void print_usage(void)
{
   printf("Usage: %.20s <textresource-filename>\n", progname_maincontext()) ;
}

/* function: print_error
 * Prints formatted error string. */
static void print_error(const char * format, ...)
{
   va_list vargs ;
   va_start(vargs, format) ;
   dprintf(STDERR_FILENO, "\n%s: ", progname_maincontext()) ;
   vdprintf(STDERR_FILENO, format, vargs) ;
   dprintf(STDERR_FILENO, "\n") ;
   va_end(vargs) ;
}

/* struct: xmlattribute_t
 * Name and value of an xml attribute.
 * > <entity attr_name="attr_value" ...>. */
struct xmlattribute_t {
   const char  * name ;
   string_t    value ;
} ;

// group: lifetime

/* define: xmlattribute_INIT
 * Static initializer. Sets <xmlattribute_t> name to parameter value name and
 * its value to undefined. */
#define xmlattribute_INIT(name)        { (name), string_INIT_FREEABLE }


/* struct: outconfig_t
 * Contains control information how the generated output is to be formatted. */
struct outconfig_t {
   string_t    type ;
   union {
      struct {
         string_t    cfilename;
         string_t    hfilename ;
         string_t    firstparam ;
         string_t    nameprefix ;
         string_t    namesuffix ;
         string_t    printf ;
      } C ;
      struct {
         string_t    cfilename ;
         string_t    strdata ;
         string_t    stroffset ;
      } Ctable ;
   } ;
} ;

#define outconfig_INIT_FREEABLE        { string_INIT_FREEABLE, { .C = { string_INIT_FREEABLE, string_INIT_FREEABLE, string_INIT_FREEABLE, string_INIT_FREEABLE, string_INIT_FREEABLE, string_INIT_FREEABLE } } }


/* struct: textresource_language_t
 * A language selector for <textresource_text_t>.
 * */
struct textresource_language_t {
   string_t          name ;
   slist_node_EMBED  (next) ;
} ;

static typeadapt_impl_t    g_textreslang_adapter     = typeadapt_impl_INIT(sizeof(textresource_language_t)) ;

slist_IMPLEMENT(_languagelist, textresource_language_t, next)

// group: lifetime

/* define: textresource_language_INIT
 * Static initializer. */
#define textresource_language_INIT(name)     { string_INIT(name->size, name->addr), 0 }


/* struct: textresource_parameter_t
 * A parameter for <textresource_text_t>.
 * */
struct textresource_parameter_t {
   arraystf_node_t            name ;
   textresource_paramtype_t   * type ;
   slist_node_EMBED           (next) ;
   typemodifier_e             typemod ;
} ;

arraystf_IMPLEMENT(_arrayparam, textresource_parameter_t, name)
slist_IMPLEMENT(_paramlist, textresource_parameter_t, next)

static typeadapt_impl_t    g_parameter_adapter     = typeadapt_impl_INIT(sizeof(textresource_parameter_t)) ;
static typeadapt_member_t  g_parameter_nodeadapter = typeadapt_member_INIT((typeadapt_t*)&g_parameter_adapter, offsetof(textresource_parameter_t, name)) ;

// group: lifetime

/* define: textresource_parameter_INIT_FREEABLE
 * Static initializer. */
#define textresource_parameter_INIT_FREEABLE    { arraystf_node_INIT(0,0), 0, 0, typemodifier_PLAIN }


/* struct: textresource_textatom_t
 * An atomic text element string content referenced by <textresource_condition_t>.
 * It describes either a parameter reference and its formatting or a simple text string. */
struct textresource_textatom_t {
   slist_node_EMBED           (next) ;
   textresource_textatom_e    type ;
   union {
      string_t                string ;
      struct {
         textresource_parameter_t   * ref ;
         int                        maxlen ;
      }                       param ;
   } ;
} ;

static typeadapt_impl_t    g_textatom_adapter     = typeadapt_impl_INIT(sizeof(textresource_textatom_t)) ;


// group: lifetime

/* define: textresource_textatom_INIT
 * Static initializer. Initializes <textresource_textatom_t> as type textresource_textatom_STRING. */
#define textresource_textatom_INIT(string)                        { 0, textresource_textatom_STRING, { string } }

/* define: textresource_textatom_INIT
 * Static initializer. Initializes <textresource_textatom_t> as type textresource_textatom_PARAMETER. */
#define textresource_textatom_INIT_PARAM(parameter)               { 0, textresource_textatom_PARAMETER, { .param = { parameter, 0 } } }


slist_IMPLEMENT(_textatomlist, textresource_textatom_t, next)


/* struct: textresource_condition_t
 * A condition which selects or deselects the contained <textresource_textatom_t>.
 * The <condition> indicates a conditional string if it is not empty.
 * If <condition> contains "else" it is the last entry of a switch
 * > ((condition1): "str1" (cond2): ... else: "strX")
 * Every switch ends in an else. If the user does not supply one an artifical
 * <textresource_condition_t> is created with <condition> set to "else" and an empty atomlist. */
struct textresource_condition_t {
   slist_t                    atomlist ;
   slist_node_t               * next ;
   /* variable: condition
    * The C99-condition which is checked in the generated source code.
    * If the condition is true the text is used and all other conditional texts until after
    * the next encountered "else" are skipped. A <condition> set to "else" marks
    * the last entry of a conditional sequence. */
   string_t                   condition ;
} ;

typedef struct textresource_condition_adapt_t   textresource_condition_adapt_t ;

struct textresource_condition_adapt_t {
   typeadapt_EMBED(textresource_condition_adapt_t, textresource_condition_t, void*) ;
} ;

static int copyobj_textresourcecondition(textresource_condition_adapt_t * typeadp, /*out*/textresource_condition_t ** condcopy, const textresource_condition_t * cond) ;
static int freeobj_textresourcecondition(textresource_condition_adapt_t * typeadp, textresource_condition_t ** cond) ;

static textresource_condition_adapt_t  g_condition_adapter     = typeadapt_INIT_LIFETIME(&copyobj_textresourcecondition, &freeobj_textresourcecondition) ;

slist_IMPLEMENT(_conditionlist, textresource_condition_t, next)

// group: lifetime

#define textresource_condition_INIT(condition)     { slist_INIT, 0, condition }

static int copyobj_textresourcecondition(textresource_condition_adapt_t * typeadp, /*out*/textresource_condition_t ** condcopy, const textresource_condition_t * cond)
{
   int err ;
   memblock_t  mblock = memblock_INIT_FREEABLE ;

   (void) typeadp ;

   err = RESIZE_MM(sizeof(textresource_condition_t), &mblock) ;
   if (err) return err ;

   textresource_condition_t * newcond = (textresource_condition_t *) mblock.addr ;
   *newcond = (textresource_condition_t) textresource_condition_INIT(cond->condition) ;

   *condcopy = newcond ;

   return 0 ;
}

static int freeobj_textresourcecondition(textresource_condition_adapt_t * typeadp, textresource_condition_t ** cond)
{
   int err ;
   int err2 ;

   (void) typeadp ;

   if (*cond) {
      textresource_condition_t * delobj = *cond ;

      *cond = 0 ;

      err = free_textatomlist(&delobj->atomlist, (typeadapt_t*)&g_textatom_adapter) ;

      memblock_t  mblock = memblock_INIT(sizeof(textresource_condition_t), (uint8_t*)delobj) ;
      err2 = FREE_MM(&mblock) ;
      if (err2) err = err2 ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

// group: add

static int addtextatom_textresourcecondition(textresource_condition_t * cond, textresource_textatom_t * textatom)
{
   int err ;
   textresource_textatom_t * textatomcopy ;

   err = callnewcopy_typeadapt(&g_textatom_adapter, (struct typeadapt_object_t**)&textatomcopy, (struct typeadapt_object_t*)textatom) ;
   if (err) goto ONABORT ;

   insertlast_textatomlist(&cond->atomlist, textatomcopy) ;

   return 0 ;
ONABORT:
   return err ;
}


/* struct: textresource_langref_t
 * A reference to a language and container for one or more <textresource_condition_t>. */
struct textresource_langref_t {
   slist_t                    condlist ;
   slist_node_t               * next ;
   textresource_language_t    * lang ;
} ;

typedef struct textresource_langref_adapt_t     textresource_langref_adapt_t ;

struct textresource_langref_adapt_t {
   typeadapt_EMBED(textresource_langref_adapt_t, textresource_langref_t, void*) ;
} ;

static int copyobj_textresourcelangref(textresource_langref_adapt_t * typeadp, /*out*/textresource_langref_t ** langcopy, const textresource_langref_t * lang) ;
static int freeobj_textresourcelangref(textresource_langref_adapt_t * typeadp, textresource_langref_t ** lang) ;

static textresource_langref_adapt_t    g_langref_adapter     = typeadapt_INIT_LIFETIME(&copyobj_textresourcelangref, &freeobj_textresourcelangref) ;

slist_IMPLEMENT(_langreflist, textresource_langref_t, next)

// group: lifetime

/* define: textresource_langref_INIT
 * Static initializer. */
#define textresource_langref_INIT(language)   { slist_INIT, 0, (language) }

static int copyobj_textresourcelangref(textresource_langref_adapt_t * typeadp, /*out*/textresource_langref_t ** langcopy, const textresource_langref_t * lang)
{
   int err ;
   memblock_t  mblock = memblock_INIT_FREEABLE ;

   (void) typeadp ;

   err = RESIZE_MM(sizeof(textresource_langref_t), &mblock) ;
   if (err) return err ;

   textresource_langref_t * newlang = (textresource_langref_t *) mblock.addr ;
   *newlang = (textresource_langref_t) textresource_langref_INIT(lang->lang) ;

   *langcopy = newlang ;

   return 0 ;
}

static int freeobj_textresourcelangref(textresource_langref_adapt_t * typeadp, textresource_langref_t ** lang)
{
   int err ;
   int err2 ;

   (void) typeadp ;

   if (*lang) {
      textresource_langref_t * delobj = *lang ;

      *lang = 0 ;

      err = free_conditionlist(&delobj->condlist, (typeadapt_t*)&g_condition_adapter) ;

      memblock_t  mblock = memblock_INIT(sizeof(textresource_langref_t), (uint8_t*)delobj) ;
      err2 = FREE_MM(&mblock) ;
      if (err2) err = err2 ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

// group: add

static int addcondition_textresourcelangref(textresource_langref_t * lang, textresource_condition_t * condition, /*out*/textresource_condition_t ** copy)
{
   int err ;
   textresource_condition_t * conditioncopy ;

   err = callnewcopy_typeadapt(&g_condition_adapter, &conditioncopy, condition) ;
   if (err) goto ONABORT ;

   insertlast_conditionlist(&lang->condlist, conditioncopy) ;

   if (copy) *copy = conditioncopy ;

   return 0 ;
ONABORT:
   return err ;
}


/* struct: textresource_text_t
 * A single text resource description.
 * The set of all defined text resources is stored in <textresource_t>.
 *
 * A <textresource_text_t> has 0 or more <textresource_parameter_t> stored in paramlist.
 * It contains one or more language specific text definitions stored as <textresource_langref_t> in langlist.
 * A <textresource_langref_t> contains one or more <textresource_condition_t>
 * which define a condition under which a string has to be used or not. */
struct textresource_text_t {
   arraystf_node_t   name ;
   slist_node_t *    next ;
   arraystf_t   *    params ;
   slist_t           paramlist ;
   slist_t           langlist ;
   textresource_text_t * textref ;  // used in outconfig C-table
   size_t            tableoffset ;  // used in outconfig C-table
} ;

typedef struct textresource_text_adapt_t   textresource_text_adapt_t ;

struct textresource_text_adapt_t {
   typeadapt_EMBED(textresource_text_adapt_t, textresource_text_t, void*) ;
} ;

static int copyobj_textresourcetext(textresource_text_adapt_t * typeadt, /*out*/textresource_text_t ** textcopy, const textresource_text_t * text) ;
static int freeobj_textresourcetext(textresource_text_adapt_t * typeadt, textresource_text_t ** text) ;

static textresource_text_adapt_t    g_textrestext_adapter     = typeadapt_INIT_LIFETIME(&copyobj_textresourcetext, &freeobj_textresourcetext) ;
static typeadapt_member_t           g_textrestext_nodeadapter = typeadapt_member_INIT((typeadapt_t*)&g_textrestext_adapter, offsetof(textresource_text_t, name)) ;

arraystf_IMPLEMENT(_arraytname, textresource_text_t, name)
slist_IMPLEMENT(_textlist, textresource_text_t, next)

// group: lifetime

static int init_textresourcetext(/*out*/textresource_text_t * text, const string_t * name)
{
   int err ;
   arraystf_t * textparams = 0 ;
   err = new_arrayparam(&textparams, 16) ;
   if (err) return err ;
   text->name   = (arraystf_node_t) arraystf_node_INIT(name->size, name->addr) ;
   text->next   = 0 ;
   text->params = textparams ;
   init_paramlist(&text->paramlist);
   init_langreflist(&text->langlist) ;
   text->textref = 0 ;
   text->tableoffset = 0 ;
   return 0 ;
}

static int copyobj_textresourcetext(textresource_text_adapt_t * typeadt, /*out*/textresource_text_t ** textcopy, const textresource_text_t * text)
{
   int err ;
   memblock_t  mblock = memblock_INIT_FREEABLE ;

   (void) typeadt ;

   err = RESIZE_MM(sizeof(textresource_text_t), &mblock) ;
   if (err) return err ;

   textresource_text_t * newtext = (textresource_text_t *) mblock.addr ;
   (void) init_textresourcetext(newtext, genericcast_string(&text->name)) ;

   *textcopy = newtext ;

   return 0 ;
}

static int freeobj_textresourcetext(textresource_text_adapt_t * typeadt, textresource_text_t ** text)
{
   int err ;
   int err2 ;
   textresource_text_t * delobj = *text ;

   (void) typeadt ;

   if (delobj) {
      *text = 0 ;

      memblock_t  mblock = memblock_INIT(sizeof(textresource_text_t), (uint8_t*)delobj) ;

      err = free_paramlist(&delobj->paramlist, 0) ; // do not delete the nodes

      err2 = delete_arrayparam(&delobj->params, &g_parameter_nodeadapter) ;  // delete nodes
      if (err2) err = err2 ;

      err2 = free_langreflist(&delobj->langlist, (typeadapt_t*)&g_langref_adapter) ;
      if (err2) err = err2 ;

      err2 = FREE_MM(&mblock) ;
      if (err2) err = err2 ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

// group: update

static int copyfromref_textresourcetext(textresource_text_t * text, textresource_text_t * textref)
{
   int err ;

   VALIDATE_INPARAM_TEST(isempty_langreflist(&text->langlist), ONABORT, ) ;

   text->textref = textref ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}


/* struct: textresource_paramtype_t
 * */
struct textresource_paramtype_t {
   arraystf_node_t   name ;
   typemodifier_e    typemod ;
   const char        * format ;
   const char        * ptrformat ;
} ;

// group: lifetime

/* define: textresource_paramtype_INIT
 * Static initializer.
 *
 * Parameter:
 * cstr     - const C string.
 * modifier - Bit flags from <typemodifier_e> added together. */
#define textresource_paramtype_INIT(cstr,modifier,format)            \
      textresource_paramtype_INIT2(cstr,modifier,format,"")

#define textresource_paramtype_INIT2(cstr,modifier,format,ptrformat) \
      { arraystf_node_INIT_CSTR(cstr), (modifier), (format), (ptrformat) }


/* struct: textresource_t
 * Defines memory representation of parsed text resource description.
 */
struct textresource_t {
   const char        * read_from_filename ;
   arraystf_t        * textnames ;
   arraystf_t        * paramtypes ;
   /* variable: languages
    * Contains list of <textresource_language_t>. */
   slist_t           languages ;
   /* variable: textlist
    * Contains list of <textresource_text_t>. */
   slist_t           textlist ;
   outconfig_t       outconfig ;
} ;

arraystf_IMPLEMENT(_arrayptype, textresource_paramtype_t, name.addr)

// group: lifetime

/* define: textresource_INIT_FREEABLE
 * Static initializer. */
#define textresource_INIT_FREEABLE     { 0, 0, 0, slist_INIT, slist_INIT, outconfig_INIT_FREEABLE }

static int free_textresource(textresource_t * textres)
{
   int err ;
   int err2 ;

   textres->read_from_filename = 0 ;

   err = delete_arrayptype(&textres->paramtypes, 0) ;

   err2 = free_textlist(&textres->textlist, 0 /*freed in delete_arraytname*/) ;
   if (err) err = err2 ;

   err2 = delete_arraytname(&textres->textnames, &g_textrestext_nodeadapter) ;
   if (err) err = err2 ;

   err2 = free_languagelist(&textres->languages, (typeadapt_t*)&g_textreslang_adapter) ;
   if (err) err = err2 ;

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

/* function: init_textresource
 * Initializes <textresource_t> object. */
static int init_textresource(/*out*/textresource_t * textres, const char * read_from_filename)
{
   int err ;
   static textresource_paramtype_t  knowntypes[] = {
          textresource_paramtype_INIT("const",    typemodifier_CONST, "")
         ,textresource_paramtype_INIT("size_t",   typemodifier_PLAIN, "zu")
         ,textresource_paramtype_INIT("ssize_t",  typemodifier_PLAIN, "zd")
         ,textresource_paramtype_INIT("int8_t",   typemodifier_PLAIN, PRId8)
         ,textresource_paramtype_INIT2("uint8_t",  typemodifier_POINTER, PRIu8, "s")
         ,textresource_paramtype_INIT("int16_t",  typemodifier_PLAIN, PRId16)
         ,textresource_paramtype_INIT("uint16_t", typemodifier_PLAIN, PRIu16)
         ,textresource_paramtype_INIT("int32_t",  typemodifier_PLAIN, PRId32)
         ,textresource_paramtype_INIT("uint32_t", typemodifier_PLAIN, PRIu32)
         ,textresource_paramtype_INIT("int64_t",  typemodifier_PLAIN, PRId64)
         ,textresource_paramtype_INIT("uint64_t", typemodifier_PLAIN, PRIu64)
         ,textresource_paramtype_INIT2("char",     typemodifier_POINTER, "c", "s")
         ,textresource_paramtype_INIT("int",      typemodifier_PLAIN, "d")
         ,textresource_paramtype_INIT("unsigned", typemodifier_PLAIN, "u")
         ,textresource_paramtype_INIT("long",     typemodifier_PLAIN, "ld")
         ,textresource_paramtype_INIT("float",    typemodifier_PLAIN, "g")
         ,textresource_paramtype_INIT("double",   typemodifier_PLAIN, "g")
         ,textresource_paramtype_INIT("_PRINTF_", typemodifier_RESERVED, "")
         ,textresource_paramtype_INIT("_err_", typemodifier_RESERVED, "")
   } ;

   *textres = (textresource_t) textresource_INIT_FREEABLE ;

   textres->read_from_filename = read_from_filename ;

   err = new_arraytname(&textres->textnames, 256) ;
   if (err) goto ONABORT ;

   err = new_arrayptype(&textres->paramtypes, 16) ;
   if (err) goto ONABORT ;

   init_slist(&textres->languages) ;

   for (unsigned i = 0; i < lengthof(knowntypes); ++i) {
      err = insert_arrayptype(textres->paramtypes, &knowntypes[i], 0, 0) ;
      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   free_textresource(textres) ;
   TRACEABORT_LOG(err) ;
   return err ;
}

// group: add

static int addlanguage_textresource(textresource_t * textres, string_t * langid)
{
   int err ;
   textresource_language_t    lang = textresource_language_INIT(langid) ;
   textresource_language_t *  langcopy ;

   err = callnewcopy_typeadapt(&g_textreslang_adapter, (struct typeadapt_object_t**)&langcopy, (struct typeadapt_object_t*)&lang) ;
   if (err) goto ONABORT ;

   insertlast_languagelist(&textres->languages, langcopy) ;

   return 0 ;
ONABORT:
   return err ;
}

static int addtext_textresource(textresource_t * textres, textresource_text_t * text)
{
   insertlast_textlist(&textres->textlist, text) ;
   return 0 ;
}


/* struct: textresource_reader_t
 * Reads textual representation of text resource.
 * The content of the textfile is stored into
 * a structure of type <textresource_t>. */
struct textresource_reader_t {
   textresource_t    txtres ;
   utf8reader_t      txtpos ;
} ;

// group: helper

static void report_parseerror(textresource_reader_t * reader, const char * format, ...) __attribute__ ((__format__ (__printf__, 2, 3))) ;


static void report_errorposition(const char * filename, size_t column, size_t line)
{
   dprintf(STDERR_FILENO, "%s: line:%zu col:%zu\n", filename, line, column) ;
}

static void report2_parseerror(textresource_reader_t * reader, const textpos_t * pos, const char * format, ...)
{
   print_error("Syntax error") ;
   report_errorposition(reader->txtres.read_from_filename, column_textpos(pos), line_textpos(pos)) ;
   va_list vargs ;
   va_start(vargs, format) ;
   vdprintf(STDERR_FILENO, format, vargs) ;
   dprintf(STDERR_FILENO, "\n") ;
   va_end(vargs) ;
}

static void report_parseerror(textresource_reader_t * reader, const char * format, ...)
{
   print_error("Syntax error") ;
   report_errorposition(reader->txtres.read_from_filename, column_utf8reader(&reader->txtpos), line_utf8reader(&reader->txtpos)) ;
   va_list vargs ;
   va_start(vargs, format) ;
   vdprintf(STDERR_FILENO, format, vargs) ;
   dprintf(STDERR_FILENO, "\n") ;
   va_end(vargs) ;
}

static void report_unexpectedendofinput(textresource_reader_t * reader)
{
   char32_t ch ;
   if (EILSEQ == nextchar_utf8reader(&reader->txtpos, &ch)) {
      skipascii_utf8reader(&reader->txtpos) ;
      print_error("Wrong UTF-8 character encoding") ;
   } else {
      print_error("Unexpected end of input") ;
   }
   report_errorposition(reader->txtres.read_from_filename, column_utf8reader(&reader->txtpos), line_utf8reader(&reader->txtpos)) ;
}

/* function: skip_spaceandcomment
 * Skips comments until next non space character.
 * The readoffset is moved forward until end of input. */
static int skip_spaceandcomment(textresource_reader_t * reader)
{
   int err ;
   for (uint8_t ch; 0 == peekascii_utf8reader(&reader->txtpos, &ch); ) {
      if ('#' == ch) {
         err = skipline_utf8reader(&reader->txtpos) ;
         if (err && err != ENODATA) return err ;
      } else {
         if (     ' '  != ch
               && '\t' != ch
               && '\n' != ch) {
            break ;
         }
         skipascii_utf8reader(&reader->txtpos) ;
      }
   }

   return 0 ;
}

static int match_unsigned(textresource_reader_t * reader, int * number)
{
   int err ;
   uint8_t  ch ;
   int      value = 0 ;

   err = skip_spaceandcomment(reader) ;
   if (err) goto ONABORT ;

   if (  0 != nextbyte_utf8reader(&reader->txtpos, &ch)
         || !('0' <= ch && ch <= '9')) {
      report_parseerror(reader, "expected to read a number") ;
      err = EINVAL ;
      goto ONABORT ;
   }

   value = (ch - '0') ;

   while (  0 == peekascii_utf8reader(&reader->txtpos, &ch)
            && ('0' <= ch && ch <= '9')) {
      skipascii_utf8reader(&reader->txtpos) ;
      if (value >= (INT_MAX/10 - 1)) {
         report_parseerror(reader, "number too big") ;
         err = EINVAL ;
         goto ONABORT ;
      }
      value *= 10 ;
      value += (ch - '0') ;
   }

   *number = value ;

   return 0 ;
ONABORT:
   return err ;
}

static int match_string(textresource_reader_t * reader, const char * string)
{
   int err ;
   size_t len = strlen(string) ;
   size_t matchlen ;

   err = skip_spaceandcomment(reader) ;
   if (err) goto ONABORT ;

   // only ascii string are supported cause of nrbytes == columnIncrement
   err = matchbytes_utf8reader(&reader->txtpos, len, len, (const uint8_t*)string, &matchlen) ;
   if (err) {
      incrcolumn_textpos(textpos_utf8reader(&reader->txtpos)) ;
      report_parseerror(reader, "expected to read »%s«", string) ;
      err = EINVAL ;
      goto ONABORT ;
   }

   return 0 ;
ONABORT:
   return err ;
}

static int match_stringandspace(textresource_reader_t * reader, const char * string)
{
   int err ;
   uint8_t     ch ;

   err = match_string(reader, string) ;
   if (err) return err ;

   if (  0 != nextbyte_utf8reader(&reader->txtpos, &ch)
         || (  ' '  != ch
               && '\t' != ch
               && '\n' != ch)) {
      goto ONABORT ;
   }

   return 0 ;
ONABORT:
   report_parseerror(reader, "expected to read » «") ;
   return EINVAL ;
}

static int match_identifier(textresource_reader_t * reader, /*out*/string_t * idname)
{
   int err ;
   uint32_t ch ;

   err = skip_spaceandcomment(reader) ;
   if (err) goto ONABORT ;

   const uint8_t * idstart = unread_utf8reader(&reader->txtpos) ;

   err = nextchar_utf8reader(&reader->txtpos, &ch) ;
   if (err) {
      report_unexpectedendofinput(reader) ;
      goto ONABORT ;
   }

   if (  !( ('a' <= ch && ch <= 'z')
         || ('A' <= ch && ch <= 'Z')
         || ('0' <= ch && ch <= '9')
         || '_' == ch)) {
      report_parseerror(reader, "expected identifier but read unsupported character") ;
      err = EINVAL ;
      goto ONABORT ;
   }

   while (0 == peekascii_utf8reader(&reader->txtpos, &ch)) {

      if (  '\t' == ch
         || ' ' == ch
         || '\n' == ch
         || '"' == ch
         || '[' == ch
         || '(' == ch
         || ')' == ch
         || ',' == ch
         || '=' == ch
         || '*' == ch
         || '<' == ch
         || ':' == ch) {
         break ;
      }

      if (  !( ('a' <= ch && ch <= 'z')
            || ('A' <= ch && ch <= 'Z')
            || ('0' <= ch && ch <= '9')
            || '_' == ch)) {
         skipchar_utf8reader(&reader->txtpos) ;
         report_parseerror(reader, "expected identifier but read unsupported character") ;
         err = EINVAL ;
         goto ONABORT ;
      }

      skipascii_utf8reader(&reader->txtpos) ;
   }

   err = initse_string(idname, idstart, unread_utf8reader(&reader->txtpos)) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   return err ;
}

static int match_quotedcstring(textresource_reader_t * reader, string_t * cstring)
{
   int err ;

   err = match_string(reader, "\"") ;
   if (err) goto ONABORT ;

   const uint8_t * startstr = unread_utf8reader(&reader->txtpos) ;
   const uint8_t * endstr   = startstr ;

   for (uint8_t ch, isEscape = 0, isClosingQuote = 0; !isClosingQuote;) {

      if (0 != peekascii_utf8reader(&reader->txtpos, &ch)) break ;
      skipchar_utf8reader(&reader->txtpos) ;

      if (isEscape) {
         isEscape = false ;
         switch (ch) {
         case '\\':     break ;
         case 'n':      break ;
         case 't':      break ;
         case '"':      break ;
         default:       report_parseerror(reader, "unsupported escape sequence '\\%c'", (ch&0x7f)) ;
                        err = EINVAL ;
                        goto ONABORT ;
         }
      } else {
         switch (ch) {
         case '\\':     isEscape = true ; break ;
         case '"':      endstr = unread_utf8reader(&reader->txtpos) - 1 ;
                        isClosingQuote = true ;
                        break ;
         }
      }
   }

   initse_string(cstring, startstr, endstr) ;

   return 0 ;
ONABORT:
   return err ;
}

static int match_ifcondition(textresource_reader_t * reader, string_t * ifcondition)
{
   int err ;

   err = match_string(reader, "(") ;
   if (err) goto ONABORT ;

   const uint8_t * startstr = unread_utf8reader(&reader->txtpos) - 1 ;
   const uint8_t * endstr   = startstr ;

   for (uint8_t ch, depth = 1; depth;) {

      if (0 != peekascii_utf8reader(&reader->txtpos, &ch)) break ;
      skipchar_utf8reader(&reader->txtpos) ;

      if ('(' == ch) {
         ++ depth ;
         if (10 <= depth) {
            report_parseerror(reader, "too deeply nested parentheses '('") ;
            err = EINVAL ;
            goto ONABORT ;
         }
      } else if (')' == ch) {
         -- depth ;
         endstr = unread_utf8reader(&reader->txtpos) ;
      }
   }

   if (startstr + 2 == endstr) {
      report_parseerror(reader, "empty '()' not allowed") ;
      err = EINVAL ;
      goto ONABORT ;
   }

   initse_string(ifcondition, startstr, endstr) ;

   return 0 ;
ONABORT:
   return err ;
}

static int match_formatdescription(textresource_reader_t * reader, textresource_textatom_t * param)
{
   int err ;

   err = match_string(reader, "[") ;
   if (err) goto ONABORT ;

   for (uint8_t ch;;) {

      if (  0 == peekascii_utf8reader(&reader->txtpos, &ch)
         && ']' == ch) {
         break ;
      }

      string_t formatid ;
      err = match_identifier(reader, &formatid) ;
      if (err) goto ONABORT ;

      if (  6 == formatid.size
            && 0 == strncmp((const char*)formatid.addr, "maxlen", 6)) {
         err = match_string(reader, "=") ;
         if (err) goto ONABORT ;
         err = match_unsigned(reader, &param->param.maxlen) ;
         if (err) goto ONABORT ;
      } else {
         report_parseerror(reader, "unknown format specifier '%.*s'", (int)formatid.size, formatid.addr) ;
         err = EINVAL ;
         goto ONABORT ;
      }

   }

   err = match_string(reader, "]") ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   return err ;
}

// group: parser

static int parse_parameterlist(textresource_reader_t * reader, textresource_text_t * text)
{
   int err ;
   uint8_t ch ;

   err = match_string(reader, "(") ;
   if (err) return err ;

   err = skip_spaceandcomment(reader) ;
   if (err) goto ONABORT ;

   if (  0 == peekascii_utf8reader(&reader->txtpos, &ch)
      && ')' != ch) {

      for (;;) {

         string_t                   name_type ;
         textresource_parameter_t   textparam = textresource_parameter_INIT_FREEABLE ;

         do {
            err = match_identifier(reader, &name_type) ;
            if (err) goto ONABORT ;

            textparam.type = at_arrayptype(reader->txtres.paramtypes, name_type.size, name_type.addr) ;
            if (  !textparam.type
               || (textparam.type->typemod & typemodifier_RESERVED)) {
               report_parseerror(reader, "unknown parameter type '%.*s'", (int)name_type.size, (const char*)name_type.addr) ;
               err = EINVAL ;
               goto ONABORT ;
            }

            if (0 != (textparam.type->typemod & typemodifier_CONST)) {
               if (0 != (textparam.typemod & typemodifier_CONST)) {
                  report_parseerror(reader, "more than one const not supported in parameter type") ;
                  err = EINVAL ;
                  goto ONABORT ;
               }
               textparam.typemod |= typemodifier_CONST ;
            }

         } while (0 != (textparam.type->typemod & (typemodifier_CONST))) ;

         err = skip_spaceandcomment(reader) ;
         if (err) goto ONABORT ;

         if (  0 == peekascii_utf8reader(&reader->txtpos, &ch)
            && '*' == ch) {
            skipascii_utf8reader(&reader->txtpos) ;
            if (0 == (textparam.type->typemod & typemodifier_POINTER)) {
               report_parseerror(reader, "parameter type '%.*s' does not support '*'", (int)name_type.size, name_type.addr) ;
               err = EINVAL ;
               goto ONABORT ;
            }

            textparam.typemod |= typemodifier_POINTER ;
         }

         err = match_identifier(reader, CONST_CAST(string_t,genericcast_string(&textparam.name))) ;
         if (err) goto ONABORT ;

         if (at_arrayptype(reader->txtres.paramtypes, textparam.name.size, textparam.name.addr)) {
            report_parseerror(reader, "parameter name '%.*s' reserved", (int)textparam.name.size, textparam.name.addr) ;
            err = EINVAL ;
            goto ONABORT ;
         }

         textresource_parameter_t * textparamcopy ;

         err = insert_arrayparam(text->params, &textparam, &textparamcopy, &g_parameter_nodeadapter) ;
         if (!err) insertlast_paramlist(&text->paramlist, textparamcopy) ;
         if (err) {
            if (EEXIST == err) {
               report_parseerror(reader, "parameter name '%.*s' is not unique", (int)textparam.name.size, (const char*)textparam.name.addr) ;
            }
            goto ONABORT ;
         }


         err = skip_spaceandcomment(reader) ;
         if (err) goto ONABORT ;

         if (  0 != peekascii_utf8reader(&reader->txtpos, &ch)
            || ',' != ch)   break ;

         skipascii_utf8reader(&reader->txtpos) ;
      }
   }

   err = match_string(reader, ")") ;
   if (err) return err ;

   return 0 ;
ONABORT:
   return err ;
}

static int parse_textatomline(textresource_reader_t * reader, textresource_text_t * text, textresource_condition_t * condition)
{
   int err ;
   bool isLineEnding = false ;

   for (uint8_t ch;;) {
      if (0 != peekascii_utf8reader(&reader->txtpos, &ch)) break ;

      if ('\n' == ch) {
         isLineEnding = true ;
         skipascii_utf8reader(&reader->txtpos) ;
         continue ;
      }

      if (  ' '  == ch
         || '\t' == ch) {
         skipascii_utf8reader(&reader->txtpos) ;
         continue ;
      }

      if ('"' == ch) {
         isLineEnding = false ;  // " character lets the line continue

         string_t cstring ;

         err = match_quotedcstring(reader, &cstring) ;
         if (err) goto ONABORT ;

         textresource_textatom_t  textstring = textresource_textatom_INIT(cstring) ;

         err = addtextatom_textresourcecondition(condition, &textstring) ;
         if (err) goto ONABORT ;

      } else if (isLineEnding) {
         break ;
      } else {
         bool isParam = false ;

         for (size_t i = 0, endi = unreadsize_utf8reader(&reader->txtpos); i < endi; ++i) {
            if (0 != peekasciiatoffset_utf8reader(&reader->txtpos, i, &ch)) break ;
            if (  ('a' <= ch && ch <= 'z')
                  || ('A' <= ch && ch <= 'Z')
                  || ('0' <= ch && ch <= '9')
                  ||  '_' == ch) {
               isParam = true ;
               continue ;
            }
            if (  ':' == ch
                  || (  i == 4
                        && 0 == strncmp((const char*)unread_utf8reader(&reader->txtpos), "else", 4))) {
               isParam = false ;
            }
            break ;
         }

         if (!isParam) break ;

         string_t paramname ;

         err = match_identifier(reader, &paramname) ;
         if (err) goto ONABORT ;

         textresource_parameter_t * param = at_arrayparam(text->params, paramname.size, paramname.addr) ;

         if (!param) {
            report_parseerror(reader, "Unknown parameter '%.*s'", (int)paramname.size, paramname.addr) ;
            err = EINVAL ;
            goto ONABORT ;
         }

         while (  0 == peekascii_utf8reader(&reader->txtpos, &ch)
               && (' '  == ch || '\t' == ch)) {
               skipascii_utf8reader(&reader->txtpos) ;
         }

         textresource_textatom_t  textparam = textresource_textatom_INIT_PARAM(param) ;

         if ('[' == ch) {
            err = match_formatdescription(reader, &textparam) ;
            if (err) goto ONABORT ;
         }

         err = addtextatom_textresourcecondition(condition, &textparam) ;
         if (err) goto ONABORT ;
      }
   }

   return 0 ;
ONABORT:
   return err ;
}

static int parse_conditional_textatoms(textresource_reader_t * reader, textresource_text_t * text, textresource_langref_t * lang)
{
   int err ;

   err = match_string(reader, "(") ;
   if (err) goto ONABORT ;

   bool isElseAllowed = false ;

   for (uint8_t ch;;) {

      err = skip_spaceandcomment(reader) ;
      if (err) goto ONABORT ;

      if (  0 != peekascii_utf8reader(&reader->txtpos, &ch)
         || '<' == ch) {
         break ;
      }

      if (')' == ch) {
         if (!isElseAllowed) {
            report_parseerror(reader, "Need at least one (CONDITION) before ')'") ;
            err = EINVAL ;
            goto ONABORT ;
         }
         textresource_condition_t elsecond = textresource_condition_INIT(string_INIT_CSTR("else")) ;
         err = addcondition_textresourcelangref(lang, &elsecond, 0) ;
         if (err) goto ONABORT ;
         break ;
      }

      if ('e' == ch) {
         // match else: "<string>"
         err = match_string(reader, "else") ;
         if (err) goto ONABORT ;

         if (!isElseAllowed) {
            report_parseerror(reader, "Need at least one (CONDITION) before 'else:'") ;
            err = EINVAL ;
            goto ONABORT ;
         }

         textresource_condition_t   elsecond   = textresource_condition_INIT(string_INIT_CSTR("else")) ;
         textresource_condition_t   * elsecopy = 0 ;

         err = addcondition_textresourcelangref(lang, &elsecond, &elsecopy) ;
         if (err) goto ONABORT ;

         err = parse_textatomline(reader, text, elsecopy) ;
         if (err) goto ONABORT ;
         break ;
      }

      if ('(' == ch) {
         // match (x == y && ...) "<string>"
         string_t boolstring ;
         err = match_ifcondition(reader, &boolstring) ;
         if (err) goto ONABORT ;

         textresource_condition_t   ifcond   = textresource_condition_INIT(boolstring) ;
         textresource_condition_t   * ifcopy = 0 ;

         err = addcondition_textresourcelangref(lang, &ifcond, &ifcopy) ;
         if (err) goto ONABORT ;

         err = parse_textatomline(reader, text, ifcopy) ;
         if (err) goto ONABORT ;

         isElseAllowed = true ;

      } else {
         skipchar_utf8reader(&reader->txtpos) ;
         report_parseerror(reader, "expected 'else' or '(' or ')'") ;
         err = EINVAL ;
         goto ONABORT ;
      }
   }

   err = match_string(reader, ")") ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   return err ;
}

static int parse_unconditional_textatoms(textresource_reader_t * reader, textresource_text_t * text, textresource_langref_t * lang)
{
   int err ;

   for (uint8_t ch;;) {
      err = skip_spaceandcomment(reader) ;
      if (err) goto ONABORT ;

      if (  0 != peekascii_utf8reader(&reader->txtpos, &ch)
         || (  '"' != ch
            && '(' != ch)) {
         break ;
      }

      if ('"' == ch) {
         textresource_condition_t * condition ;

         {
            textresource_condition_t  emptycond = textresource_condition_INIT(string_INIT_FREEABLE) ;
            err = addcondition_textresourcelangref(lang, &emptycond, &condition) ;
            if (err) goto ONABORT ;
         }

         err = parse_textatomline(reader, text, condition) ;
         if (err) goto ONABORT ;

      } else {
         err = parse_conditional_textatoms(reader, text, lang) ;
         if (err) goto ONABORT ;

         err = skip_spaceandcomment(reader) ;
         if (err) goto ONABORT ;
      }

   }

   return 0 ;
ONABORT:
   return err ;
}

static int parse_textdefinitions_textresourcereader(textresource_reader_t * reader)
{
   int err ;
   string_t                name ;
   textresource_text_t     text ;
   textresource_text_t     * textcopy = 0 ;

   for (uint8_t ch;;) {

      err = match_identifier(reader, &name) ;
      if (err) return err ;

      err = init_textresourcetext(&text, &name) ;
      if (err) return err ;

      err = insert_arraytname(reader->txtres.textnames, &text, &textcopy, &g_textrestext_nodeadapter) ;
      if (err) {
         if (EEXIST == err) {
            err = EINVAL ;
            report_parseerror(reader, "double defined identifier '%.*s'", (int)name.size, name.addr) ;
         }
         return err ;
      }

      err = addtext_textresource(&reader->txtres, textcopy) ;
      if (err) return err ;

      err = skip_spaceandcomment(reader) ;
      if (err) return err ;

      if (  reader->txtres.outconfig.type.size == 7 /*outconfig C-table*/
            && 0 == peekascii_utf8reader(&reader->txtpos, &ch)
            && '-' == ch) {
         err = match_string(reader, "->") ;
         if (err) return err ;
         err = match_identifier(reader, &name) ;
         if (err) return err ;
         textresource_text_t * textref = at_arraytname(reader->txtres.textnames, name.size, name.addr) ;
         if (! textref) {
            report_parseerror(reader, "undefined identifier '%.*s'", (int)name.size, name.addr) ;
            return EINVAL ;
         }
         err = copyfromref_textresourcetext(textcopy, textref) ;
         if (err) return err ;
         err = skip_spaceandcomment(reader) ;
         if (err) return err ;

      } else {
         if (reader->txtres.outconfig.type.size != 7 /*outconfig C*/) {
            err = parse_parameterlist(reader, textcopy) ;
            if (err) return err ;
         }

         foreach(_languagelist, language, &reader->txtres.languages) {

            err = match_identifier(reader, &name) ;
            if (err) return err ;

            err = match_string(reader, ":") ;
            if (err) return err ;

            if (  language->name.size != name.size
                  || 0 != memcmp(language->name.addr, name.addr, name.size)) {
               report_parseerror(reader, "expected language definition '%.*s'", (int)language->name.size, language->name.addr) ;
               return EINVAL ;
            }

            textresource_langref_t langref       = textresource_langref_INIT(language) ;
            textresource_langref_t * langrefcopy = 0 ;

            err = callnewcopy_typeadapt(&g_langref_adapter, &langrefcopy, &langref) ;
            if (err) return err ;

            insertlast_langreflist(&textcopy->langlist, langrefcopy) ;

            err = parse_unconditional_textatoms(reader, textcopy, langrefcopy) ;
            if (err) return err ;
         }
      }

      if (  0 != peekascii_utf8reader(&reader->txtpos, &ch)
         || '<' == ch) {
         break ;
      }
   }

   return 0 ;
}

static int parse_xmlattribute_textresourcereader(textresource_reader_t * reader, /*out*/string_t * name, /*out*/string_t * value)
{
   int err ;
   uint32_t ch ;

   err = skip_spaceandcomment(reader) ;
   if (err) return err ;

   const uint8_t * name_start = unread_utf8reader(&reader->txtpos) ;
   const uint8_t * name_end   = name_start ;

   while (0 == nextchar_utf8reader(&reader->txtpos, &ch)) {
      if (!('a' <= ch && ch <= 'z')) break ;
   }

   name_end = unread_utf8reader(&reader->txtpos) - 1 ;
   if (name_end == name_start) {
      report_parseerror(reader, "expect non empty attribute name") ;
      return EINVAL ;
   }

   if ('\t' == ch || ' ' == ch) {
      err = skip_spaceandcomment(reader) ;
      if (err) return err ;
      (void) nextchar_utf8reader(&reader->txtpos, &ch) ;
   }

   if ('=' != ch) {
      report_parseerror(reader, "expect '=' after attribute name") ;
      return EINVAL ;
   }

   err = skip_spaceandcomment(reader) ;
   if (err) return err ;

   uint32_t closing_quote = 0 ;
   if (     0 == peekascii_utf8reader(&reader->txtpos, &ch)
         && ('\'' == ch || '"' == ch)) {
      closing_quote = ch ;
      skipascii_utf8reader(&reader->txtpos) ;
   }

   const uint8_t * value_start = unread_utf8reader(&reader->txtpos) ;
   const uint8_t * value_end   = value_start ;

   if (closing_quote) {
      ch = 0 ;
      while (0 == peekascii_utf8reader(&reader->txtpos, &ch)) {
         if (closing_quote == ch) {
            skipascii_utf8reader(&reader->txtpos) ;
            break ;
         }
         skipchar_utf8reader(&reader->txtpos) ;
      }
      if (closing_quote != ch) {
         report_parseerror(reader, "missing '%c' in attribute value", closing_quote) ;
         return EINVAL ;
      }
      value_end = unread_utf8reader(&reader->txtpos) - 1 ;
   } else {
      while (0 == peekascii_utf8reader(&reader->txtpos, &ch)) {
         if (' ' == ch || '\t' == ch || '/' == ch || '>' == ch) break ;
         skipchar_utf8reader(&reader->txtpos) ;
      }
      value_end = unread_utf8reader(&reader->txtpos) ;
   }

   *name  = (string_t) string_INIT((size_t)(name_end-name_start),name_start) ;
   *value = (string_t) string_INIT((size_t)(value_end-value_start),value_start) ;

   return 0 ;
}

static int parse_xmlattributes_textresourcereader(textresource_reader_t * reader, uint8_t nr_of_attribs, xmlattribute_t attribs[nr_of_attribs], /*inout*/xmltag_openclose_e * opclose)
{
   int err ;
   string_t       name ;
   string_t       value ;
   textpos_t      closepos ;
   bool           isOpenElement = true ;

   for (uint8_t ch;;) {
      err = parse_xmlattribute_textresourcereader(reader, &name, &value) ;
      if (err) return err ;

      for (unsigned i = 0 ; i < nr_of_attribs; ++i) {
         if (  0 == strncmp(attribs[i].name, (const char*)name.addr, name.size)
            && 0 == attribs[i].name[name.size]) {
            attribs[i].value = value ;
            break ;
         }
      }

      err = skip_spaceandcomment(reader) ;
      if (err) return err ;

      if (     0 == peekascii_utf8reader(&reader->txtpos, &ch)
            && '/' == ch) {
         skipascii_utf8reader(&reader->txtpos) ;
         closepos = *textpos_utf8reader(&reader->txtpos) ;
         isOpenElement = false ;
      }

      if (  0 != peekascii_utf8reader(&reader->txtpos, &ch)
         || (!isOpenElement && ('>' != ch))) {
         skipchar_utf8reader(&reader->txtpos) ;
         report_parseerror(reader, "Expected closing '>'") ;
         return EINVAL ;
      }

      if ('>' == ch) {
         skipascii_utf8reader(&reader->txtpos) ;
         switch (*opclose) {
         case xmltag_OPEN:    if (!isOpenElement) {
                                 report2_parseerror(reader, &closepos, "Expected no closing '/>' ") ;
                                 return EINVAL ;
                              }
                              break ;
         case xmltag_CLOSE:   if (isOpenElement) {
                                 report_parseerror(reader, "Expected closing '/>' ") ;
                                 return EINVAL ;
                              }
                              break ;
         case xmltag_OPEN_OR_CLOSE:
                              *opclose = isOpenElement ? xmltag_OPEN : xmltag_CLOSE ;
                              break ;
         }
         break ;
      }
   }

   return 0 ;
}

/* function: parse_version_textresourcereader
 * Skips comments until "<textresource version='3'>" is found.
 * The whole header tag is consumed. */
static int parse_version_textresourcereader(textresource_reader_t * reader)
{
   int err ;
   const char *         expectversion = "3" ;
   xmltag_openclose_e   opclose       = xmltag_OPEN ;
   xmlattribute_t       version       = xmlattribute_INIT("version") ;

   err = match_stringandspace(reader, "<textresource") ;
   if (err) return err ;

   err = parse_xmlattributes_textresourcereader(reader, 1, &version, &opclose) ;
   if (err) return err ;

   if (     strlen(expectversion) != version.value.size
         || 0 != strncmp(expectversion, (const char*)version.value.addr, version.value.size)) {
      report_parseerror(reader, "Expected version '%s'", expectversion) ;
      return EINVAL ;
   }

   return 0 ;
}

static int parse_languages_utf8reader(textresource_reader_t * reader)
{
   int err ;
   string_t langid ;
   uint8_t  ch ;

   err = match_string(reader, "languages>") ;
   if (err) goto ONABORT ;

   for (;;) {
      err = match_identifier(reader, &langid) ;
      if (err) goto ONABORT ;

      err = addlanguage_textresource(&reader->txtres, &langid) ;
      if (err) goto ONABORT ;

      err = skip_spaceandcomment(reader) ;
      if (err) goto ONABORT ;

      if (  0 == peekascii_utf8reader(&reader->txtpos, &ch)
            && ch == '<') {
         break ;
      }

      err = match_string(reader, ",") ;
      if (err) goto ONABORT ;
   }

   err = match_string(reader, "</languages>") ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   return err ;
}

/* function: parse_outconfigC_utf8reader
 * Parses control information for generated output. */
static int parse_outconfigC_utf8reader(textresource_reader_t * reader)
{
   int err ;
   xmltag_openclose_e   closetag   = xmltag_CLOSE ;
   xmlattribute_t       value      = xmlattribute_INIT("value") ;
   xmlattribute_t       genattr[2] = { xmlattribute_INIT("header"), xmlattribute_INIT("source") } ;
   xmlattribute_t       firstattr[2] = { xmlattribute_INIT("value"), xmlattribute_INIT("header") } ;

   for (uint8_t ch;;) {
      err = skip_spaceandcomment(reader) ;
      if (err) goto ONABORT ;

      err = match_string(reader, "<") ;
      if (err) goto ONABORT ;

      if (0 != peekascii_utf8reader(&reader->txtpos, &ch)) break ;

      switch (ch) {
      case 'f':   err = match_stringandspace(reader, "firstparam") ;
                  if (err) goto ONABORT ;
                  err = parse_xmlattributes_textresourcereader(reader, lengthof(firstattr), firstattr, &closetag) ;
                  if (err) goto ONABORT ;
                  static_assert(lengthof(firstattr) == 2, "assume one value") ;
                  reader->txtres.outconfig.C.firstparam = firstattr[0].value ;
                  break ;
      case 'g':   err = match_stringandspace(reader, "generate") ;
                  if (err) goto ONABORT ;
                  err = parse_xmlattributes_textresourcereader(reader, lengthof(genattr), genattr, &closetag) ;
                  if (err) goto ONABORT ;
                  static_assert(lengthof(genattr) == 2, "assume two values") ;
                  reader->txtres.outconfig.C.hfilename = genattr[0].value ;
                  reader->txtres.outconfig.C.cfilename = genattr[1].value ;
                  break ;
      case 'l':   err = parse_languages_utf8reader(reader) ;
                  if (err) goto ONABORT ;
                  break ;
      case 'n':   if (  0 == peekasciiatoffset_utf8reader(&reader->txtpos, 4, &ch)
                        && 's' == ch) {
                     err = match_stringandspace(reader, "namesuffix") ;
                     if (err) goto ONABORT ;
                     err = parse_xmlattributes_textresourcereader(reader, 1, &value, &closetag) ;
                     if (err) goto ONABORT ;
                     reader->txtres.outconfig.C.namesuffix = value.value ;
                  } else {
                     err = match_stringandspace(reader, "nameprefix") ;
                     if (err) goto ONABORT ;
                     err = parse_xmlattributes_textresourcereader(reader, 1, &value, &closetag) ;
                     if (err) goto ONABORT ;
                     reader->txtres.outconfig.C.nameprefix = value.value ;
                  }
                  break ;
      case 'p':   err = match_stringandspace(reader, "printf") ;
                  if (err) goto ONABORT ;
                  err = parse_xmlattributes_textresourcereader(reader, 1, &value, &closetag) ;
                  if (err) goto ONABORT ;
                  reader->txtres.outconfig.C.printf = value.value ;
                  break ;
      default:    return 0 ;
      }
   }

   return 0 ;
ONABORT:
   return err ;
}

/* function: parse_outconfigCtable_utf8reader
 * Parses control information for generated output. */
static int parse_outconfigCtable_utf8reader(textresource_reader_t * reader)
{
   int err ;
   xmltag_openclose_e   closetag   = xmltag_CLOSE ;
   xmlattribute_t       genattr[1] = { xmlattribute_INIT("source") } ;
   xmlattribute_t       tabattr[2] = { xmlattribute_INIT("strdata"), xmlattribute_INIT("stroffset") } ;

   for (uint8_t ch;;) {
      err = skip_spaceandcomment(reader) ;
      if (err) goto ONABORT ;

      err = match_string(reader, "<") ;
      if (err) goto ONABORT ;

      if (0 != peekascii_utf8reader(&reader->txtpos, &ch)) break ;

      switch (ch) {
      case 'g':   err = match_stringandspace(reader, "generate") ;
                  if (err) goto ONABORT ;
                  err = parse_xmlattributes_textresourcereader(reader, lengthof(genattr), genattr, &closetag) ;
                  if (err) goto ONABORT ;
                  static_assert(lengthof(genattr) == 1, "assume one value") ;
                  reader->txtres.outconfig.Ctable.cfilename = genattr[0].value ;
                  break ;
      case 'l':   err = parse_languages_utf8reader(reader) ;
                  if (err) goto ONABORT ;
                  break ;
      case 't':   err = match_stringandspace(reader, "tablename") ;
                  if (err) goto ONABORT ;
                  err = parse_xmlattributes_textresourcereader(reader, lengthof(tabattr), tabattr, &closetag) ;
                  if (err) goto ONABORT ;
                  static_assert(lengthof(tabattr) == 2, "assume two values") ;
                  reader->txtres.outconfig.Ctable.strdata   = tabattr[0].value ;
                  reader->txtres.outconfig.Ctable.stroffset = tabattr[1].value ;
                  break ;
      default:    return 0 ;
      }
   }

   return 0 ;
ONABORT:
   return err ;
}

/* function: parse_header_textresourcereader
 * Parses all header information.
 * First <parse_version_textresourcereader> is called to find the beginning of the header.
 * Then all additional xml-header-tags are parsed. */
static int parse_header_textresourcereader(textresource_reader_t * reader)
{
   int err ;
   xmltag_openclose_e   opclose  = xmltag_OPEN ;
   xmlattribute_t       typeattr = xmlattribute_INIT("type") ;

   err = parse_version_textresourcereader(reader) ;
   if (err) goto ONABORT ;

   err = skip_spaceandcomment(reader) ;
   if (err) goto ONABORT ;

   err = match_stringandspace(reader, "<outconfig") ;
   if (err) goto ONABORT ;

   err = parse_xmlattributes_textresourcereader(reader, 1, &typeattr, &opclose) ;
   if (err) goto ONABORT ;

   if (  (  1 != typeattr.value.size
            || 0 != memcmp("C", typeattr.value.addr, typeattr.value.size))
         && (  7 != typeattr.value.size
               || 0 != memcmp("C-table", typeattr.value.addr, typeattr.value.size))) {
      report_parseerror(reader, "Only output configurations 'C' and 'C-table' are supported at the moment") ;
      err = EINVAL ;
      goto ONABORT ;
   }

   reader->txtres.outconfig.type = typeattr.value ;

   if (1 == typeattr.value.size) {
      err = parse_outconfigC_utf8reader(reader) ;
      if (err) goto ONABORT ;
   } else {
      err = parse_outconfigCtable_utf8reader(reader) ;
      if (err) goto ONABORT ;
   }

   err = match_string(reader, "/outconfig>") ;
   if (err) goto ONABORT ;

   if (isempty_languagelist(&reader->txtres.languages)) {
      report_parseerror(reader, "<languages>de, en, ...</languages> not defined in <outconfig>") ;
      err = EINVAL ;
      goto ONABORT ;
   }

   return 0 ;
ONABORT:
   return err ;
}

/* function: parse_contentversion3_textresourcereader
 * Parses content of "<textresource version='3'> </textresource>". */
static int parse_contentversion3_textresourcereader(textresource_reader_t * reader)
{
   int err ;

   err = skip_spaceandcomment(reader) ;
   if (err) goto ONABORT ;

   for (uint8_t ch;;) {
      if (  0 != peekascii_utf8reader(&reader->txtpos, &ch)
         || '<' == ch) {
         break ;
      }

      err = parse_textdefinitions_textresourcereader(reader) ;
      if (err) goto ONABORT ;
   }

   err = match_string(reader, "</textresource>") ;
   if (err) goto ONABORT ;

   err = skip_spaceandcomment(reader) ;
   if (err) goto ONABORT ;

   if (isnext_utf8reader(&reader->txtpos)) {
      report_parseerror(reader, "expected to read nothing after '</textresource>'") ;
      err = EINVAL ;
      goto ONABORT ;
   }

   return 0 ;
ONABORT:
   return err ;
}

// group: lifetime

/* define: textresource_reader_INIT_FREEABLE
 * Static initializer. */
#define textresource_reader_INIT_FREEABLE       { textresource_INIT_FREEABLE, utf8reader_INIT_FREEABLE } ;

/* function: free_textresourcereader
 * Closes file and frees memory of <textresource_t>. */
static int free_textresourcereader(textresource_reader_t * reader)
{
   int err ;
   int err2 ;

   err = free_utf8reader(&reader->txtpos) ;
   err2 = free_textresource(&reader->txtres) ;
   if (err2) err = err2 ;

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

/* function: init_textresourcereader
 * Opens file and reads header for the version information.
 * The parameter filename is stored in <textresource_reader_t> as a reference
 * so do not delete filename as long as reader is alive. */
static int init_textresourcereader(/*out*/textresource_reader_t * reader, const char * filename)
{
   int err ;
   textresource_reader_t new_reader = textresource_reader_INIT_FREEABLE ;

   err = init_textresource(&new_reader.txtres, filename) ;
   if (err) goto ONABORT ;

   err = init_utf8reader(&new_reader.txtpos, filename, 0) ;
   if (err) {
      print_error("Can not open file »%s«", filename) ;
      goto ONABORT ;
   }

   err = parse_header_textresourcereader(&new_reader) ;
   if (err) goto ONABORT ;

   err = parse_contentversion3_textresourcereader(&new_reader) ;
   if (err) goto ONABORT ;

   *reader = new_reader ;
   return 0 ;
ONABORT:
   free_textresourcereader(&new_reader) ;
   return err ;
}


/* struct: textresource_writer_t
 * Writes programming language representation of text resource.
 * This writer implements the C language output. */
struct textresource_writer_t {
   textresource_t  * txtres ;
   file_t          outfile ;
} ;

static int writeCconfig_textresourcewriter(textresource_writer_t * writer) ;
static int writeCtableconfig_textresourcewriter(textresource_writer_t * writer) ;

// group: lifetime

/* define: textresource_writer_INIT_FREEABLE
 * Static initializer. */
#define textresource_writer_INIT_FREEABLE       { 0, file_INIT_FREEABLE }

static int free_textresourcewriter(textresource_writer_t * writer)
{
   int err ;

   writer->txtres = 0 ;

   err = free_file(&writer->outfile) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

static int init_textresourcewriter(textresource_writer_t * writer, textresource_t * txtres)
{
   int err ;

   *writer = (textresource_writer_t) textresource_writer_INIT_FREEABLE ;
   writer->txtres = txtres ;

   if (1 == size_string(&txtres->outconfig.type)) {
      err = writeCconfig_textresourcewriter(writer) ;
   } else {
      err = writeCtableconfig_textresourcewriter(writer) ;
   }

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   free_textresourcewriter(writer) ;
   return err ;
}

// group: write

static int writeCfctdeclaration_textresourcewriter(textresource_writer_t * writer, textresource_text_t * text)
{
   typeof(((outconfig_t*)0)->C) * progC = &writer->txtres->outconfig.C ;

   dprintf(writer->outfile, "int %.*s%.*s%.*s(", (int)progC->nameprefix.size, progC->nameprefix.addr,
                                                 (int)text->name.size, text->name.addr,
                                                 (int)progC->namesuffix.size, progC->namesuffix.addr
                                                ) ;

   bool first_param = true ;

   if (progC->firstparam.size) {
      first_param = false ;
      dprintf(writer->outfile, "%.*s", (int)progC->firstparam.size, progC->firstparam.addr) ;
   }

   foreach (_paramlist, param, &text->paramlist) {
      if (first_param) {
         first_param = false ;
      } else {
         dprintf(writer->outfile, ", ") ;
      }
      dprintf(writer->outfile, "%s%.*s %s%.*s",  (param->typemod & typemodifier_CONST) ? "const " : "",
                                             (int)param->type->name.size, param->type->name.addr,
                                             (param->typemod & typemodifier_POINTER) ? "* " : "",
                                             (int)param->name.size, param->name.addr) ;
   }

   if (first_param) {
      dprintf(writer->outfile, "void)") ;
   } else {
      dprintf(writer->outfile, ")") ;
   }

   return 0 ;
}

static int writeCheader_textresourcewriter(textresource_writer_t * writer)
{
   int err ;

   dprintf(writer->outfile, "/*\n * C header generated by textresource compiler "VERSION"\n *\n") ;
   dprintf(writer->outfile, " * Do not edit this file -- instead edit '%s'\n *\n */\n\n", writer->txtres->read_from_filename) ;

   foreach (_textlist, text, &writer->txtres->textlist) {
      err = writeCfctdeclaration_textresourcewriter(writer, text) ;
      if (err) goto ONABORT ;
      dprintf(writer->outfile, " ;\n") ;
   }

   return 0 ;
ONABORT:
   return err ;
}

static int writeCprintf_textresourcewriter(textresource_writer_t * writer, slist_t * atomlist)
{
   int err = EIO ;
   ssize_t                        bytes ;
   typeof(((outconfig_t*)0)->C) * progC = &writer->txtres->outconfig.C ;

   dprintf(writer->outfile, "%.*s(\"", (int)progC->printf.size, progC->printf.addr) ;

   // write format string

   foreach(_textatomlist, textatom, atomlist) {
      if (textresource_textatom_STRING == textatom->type) {

         for (size_t i = 0; i < textatom->string.size; ++i) {
            if ('%' == textatom->string.addr[i]) {
               bytes = write(writer->outfile, "%%", 2) ;
               if (2 != bytes) goto ONABORT ;
            } else {
               bytes = write(writer->outfile, &textatom->string.addr[i], 1) ;
               if (1 != bytes) goto ONABORT ;
            }
         }

      } else if (textresource_textatom_PARAMETER == textatom->type) {
         textresource_parameter_t * param = textatom->param.ref ;
         if (!(param->typemod & typemodifier_POINTER)) {
            bytes = dprintf(writer->outfile, "%%%s", param->type->format) ;
            if (bytes < 0) goto ONABORT ;
         } else {
            bytes = dprintf(writer->outfile, "%%%s%s", (textatom->param.maxlen ? ".*" : ""), param->type->ptrformat) ;
            if (bytes < 0) goto ONABORT ;
         }
      } else {
         assert(0) ;
      }
   }

   bytes = dprintf(writer->outfile, "%s", "\"") ;
   if (bytes < 0) goto ONABORT ;

   // write parameter string

   foreach(_textatomlist, textatom, atomlist) {

      if (textresource_textatom_PARAMETER == textatom->type) {
         textresource_parameter_t * param = textatom->param.ref ;
         if (!(param->typemod & typemodifier_POINTER)) {
            bytes = dprintf(writer->outfile, ", %.*s", (int)param->name.size, param->name.addr) ;
            if (bytes < 0) goto ONABORT ;
         } else {
            if (textatom->param.maxlen)
               bytes = dprintf(writer->outfile, ", %d, %.*s", textatom->param.maxlen, (int)param->name.size, param->name.addr) ;
            else
               bytes = dprintf(writer->outfile, ", %.*s", (int)param->name.size, param->name.addr) ;
            if (bytes < 0) goto ONABORT ;
         }
      }

   }

   bytes = dprintf(writer->outfile, "%s", ") ;\n") ;
   if (bytes < 0) goto ONABORT ;

   return 0 ;
ONABORT:
   return err ;
}

static int writeCfunction_textresourcewriter(textresource_writer_t * writer, textresource_text_t * text, textresource_language_t * lang)
{
   int err ;

   dprintf(writer->outfile, "\n") ;

   err = writeCfctdeclaration_textresourcewriter(writer, text) ;
   if (err) goto ONABORT ;

   dprintf(writer->outfile, "\n{\n") ;

   textresource_langref_t * langref = 0 ;

   foreach(_langreflist, langref2, &text->langlist) {
      if (lang == langref2->lang) {
         langref = langref2 ;
         break ;
      }
   }

   if (langref) {
      bool isIfElse = false ;
      foreach(_conditionlist, condition, &langref->condlist) {
         if (condition->condition.size) {
            if (  4 == condition->condition.size
               && 0 == strcmp("else", (const char*)condition->condition.addr)) {
               isIfElse = false ;
               if (!isempty_textatomlist(&condition->atomlist)) dprintf(writer->outfile, "   else ") ;
            } else {
               dprintf(writer->outfile, "   ") ;
               if (isIfElse)
                  dprintf(writer->outfile, "else ") ;
               else
                  isIfElse = true ;
               dprintf(writer->outfile, "if %.*s ", (int)condition->condition.size, condition->condition.addr) ;
            }
         } else if (!isempty_textatomlist(&condition->atomlist)) {
            dprintf(writer->outfile, "   ") ;
         }

         if (!isempty_textatomlist(&condition->atomlist))
            writeCprintf_textresourcewriter(writer, &condition->atomlist) ;
         else if (isIfElse)
            dprintf(writer->outfile, "/* EMPTY */ ;\n") ;
      }
   }

   dprintf(writer->outfile, "   return 0;\n}\n") ;

   return 0 ;
ONABORT:
   return err ;
}

static int writeCsource_textresourcewriter(textresource_writer_t * writer, textresource_language_t * lang)
{
   int err ;

   dprintf(writer->outfile, "/*\n * C source code generated by textresource compiler "VERSION"\n *\n") ;
   dprintf(writer->outfile, " * Do not edit this file -- instead edit '%s'\n *\n */\n", writer->txtres->read_from_filename) ;

   foreach (_textlist, text, &writer->txtres->textlist) {

      err = writeCfunction_textresourcewriter(writer, text, lang) ;
      if (err) goto ONABORT ;

   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

static int writeCconfig_textresourcewriter(textresource_writer_t * writer)
{
   int err ;
   cstring_t                      filename = cstring_INIT_FREEABLE ;
   typeof(((outconfig_t*)0)->C) * progC    = &writer->txtres->outconfig.C ;

   // generate C source file

   foreach(_languagelist, lang, &writer->txtres->languages) {

      err = initfromstring_cstring(&filename, &progC->cfilename) ;
      if (err) goto ONABORT ;

      if (lang->name.size > INT_MAX) {
         err = ENOMEM ;
         goto ONABORT ;
      }

      err = printfappend_cstring(&filename, ".%.*s", (int)lang->name.size, (const char*)lang->name.addr) ;
      if (err) goto ONABORT ;

      if (0 == checkpath_directory(0, str_cstring(&filename))) {
         (void) removefile_directory(0, str_cstring(&filename)) ;
      }

      err = initcreate_file(&writer->outfile, str_cstring(&filename), 0) ;
      if (err) {
         print_error("Can not create file »%s«", str_cstring(&filename)) ;
         goto ONABORT ;
      }

      err = writeCsource_textresourcewriter(writer, lang) ;
      if (err) goto ONABORT ;

      err = free_file(&writer->outfile) ;
      if (err) goto ONABORT ;

      err = free_cstring(&filename) ;
      if (err) goto ONABORT ;
   }

   // generate C header file

   err = initfromstring_cstring(&filename, &progC->hfilename) ;
   if (err) goto ONABORT ;

   if (0 == checkpath_directory(0, str_cstring(&filename))) {
      (void) removefile_directory(0, str_cstring(&filename)) ;
   }

   err = initcreate_file(&writer->outfile, str_cstring(&filename), 0) ;
   if (err) {
      print_error("Can not create file »%s«", str_cstring(&filename)) ;
      goto ONABORT ;
   }

   err = writeCheader_textresourcewriter(writer) ;
   if (err) goto ONABORT ;

   err = free_cstring(&filename) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   free_cstring(&filename) ;
   TRACEABORT_LOG(err) ;
   return err ;
}

static int writeCtable_textresourcewriter(textresource_writer_t * writer, textresource_language_t * lang)
{
   int err ;
   typeof(((outconfig_t*)0)->Ctable) * Ctable = &writer->txtres->outconfig.Ctable ;


   dprintf(writer->outfile, "%.*s[] = {\n", (int)Ctable->stroffset.size, Ctable->stroffset.addr) ;

   size_t tableoffset = 0 ;

   foreach (_textlist, text, &writer->txtres->textlist) {

      if (text->textref) continue ;

      text->tableoffset = tableoffset ;

      foreach (_langreflist, langref, &text->langlist) {
         if (lang == langref->lang) {
            textresource_condition_t * condition = first_conditionlist(&langref->condlist) ;
            if (  condition->condition.size
                  || last_conditionlist(&langref->condlist) != condition) {
               print_error("type 'C-table' does not support conditional strings") ;
               err = EINVAL ;
               goto ONABORT ;
            }
            foreach (_textatomlist, textatom, &condition->atomlist) {
               if (textresource_textatom_STRING == textatom->type) {
                  tableoffset += textatom->string.size ;
               } else {
                  print_error("type 'C-table' does not support parameter values") ;
                  err = EINVAL ;
                  goto ONABORT ;
               }
            }
            tableoffset += 1 /*0 byte*/ ;
            break ;
         }
      }
   }

   bool isPrev = false ;

   foreach (_textlist, text, &writer->txtres->textlist) {

      if (isPrev) dprintf(writer->outfile, ",\n") ;

      dprintf(writer->outfile, "   [%.*s] = ", (int)text->name.size, text->name.addr) ;

      if (text->textref)
         dprintf(writer->outfile, "%zu /*same as %.*s*/", text->textref->tableoffset, (int)text->textref->name.size, text->textref->name.addr) ;
      else
         dprintf(writer->outfile, "%zu", text->tableoffset) ;

      isPrev = true ;
   }

   dprintf(writer->outfile, "\n} ;\n\n%.*s[%zu] = {\n", (int)Ctable->strdata.size, Ctable->strdata.addr, tableoffset) ;

   foreach (_textlist, text, &writer->txtres->textlist) {

      if (text->textref) continue ;

      foreach (_langreflist, langref, &text->langlist) {
         if (lang == langref->lang) {
            textresource_condition_t * condition = first_conditionlist(&langref->condlist) ;
            dprintf(writer->outfile, "   \"") ;
            foreach (_textatomlist, textatom, &condition->atomlist) {
               dprintf(writer->outfile, "%.*s", (int)textatom->string.size, textatom->string.addr) ;
            }
            dprintf(writer->outfile, "\\0\"\n") ;
            break ;
         }
      }
   }

   dprintf(writer->outfile, "} ;\n") ;

   return 0 ;
ONABORT:
   return err ;
}

static int writeCtableconfig_textresourcewriter(textresource_writer_t * writer)
{
   int err ;
   cstring_t                           filename = cstring_INIT_FREEABLE ;
   typeof(((outconfig_t*)0)->Ctable) * Ctable   = &writer->txtres->outconfig.Ctable ;

   // generate C source file

   foreach (_languagelist, lang, &writer->txtres->languages) {

      err = initfromstring_cstring(&filename, &Ctable->cfilename) ;
      if (err) goto ONABORT ;

      if (lang->name.size > INT_MAX) {
         err = ENOMEM ;
         goto ONABORT ;
      }

      err = printfappend_cstring(&filename, ".%.*s", (int)lang->name.size, (const char*)lang->name.addr) ;
      if (err) goto ONABORT ;

      if (0 == checkpath_directory(0, str_cstring(&filename))) {
         (void) removefile_directory(0, str_cstring(&filename)) ;
      }

      err = initcreate_file(&writer->outfile, str_cstring(&filename), 0) ;
      if (err) {
         print_error("Can not create file »%s«", str_cstring(&filename)) ;
         goto ONABORT ;
      }

      err = writeCtable_textresourcewriter(writer, lang) ;
      if (err) goto ONABORT ;

      err = free_file(&writer->outfile) ;
      if (err) goto ONABORT ;

      err = free_cstring(&filename) ;
      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   free_cstring(&filename) ;
   return err ;
}


static int main_thread(int argc, const char * argv[])
{
   int err ;
   textresource_reader_t   reader = textresource_reader_INIT_FREEABLE ;
   textresource_writer_t   writer = textresource_writer_INIT_FREEABLE ;
   const char              * infile ;

   err = init_maincontext(maincontext_DEFAULT, argc, argv) ;
   if (err) goto ONABORT ;

   if (argc != 2) {
      goto PRINT_USAGE ;
   }
   infile = argv[1] ;

   err = init_textresourcereader(&reader, infile) ;
   if (err) goto ONABORT ;

   err = init_textresourcewriter(&writer, &reader.txtres) ;
   if (err) goto ONABORT ;

ONABORT:
   free_textresourcewriter(&writer) ;
   free_textresourcereader(&reader) ;
   free_maincontext() ;
   return err ;
PRINT_USAGE:
   print_version() ;
   print_usage() ;
   err = EINVAL ;
   goto ONABORT ;
}

int main(int argc, const char * argv[])
{
   int err ;

   err = startup_platform(argc, argv, &main_thread) ;

   return err ;
}
