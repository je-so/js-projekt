/* title: Module

   Supports loading of binary blobs which are the result
   of a extraction of the text section of a shared libraries.

   Certain restrictions apply to what you can do and what not
   in a sahred library to allow it to be converted into a module.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/task/module.h
    Header file <Module>.

   file: C-kern/task/module.c
    Implementation file <Module impl>.
*/
#ifndef CKERN_TASK_MODULE_HEADER
#define CKERN_TASK_MODULE_HEADER

/* typedef: struct module_t
 * Export <module_t> into global namespace. */
typedef struct module_t module_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_task_module
 * Test <module_t> functionality. */
int unittest_task_module(void);
#endif


/* struct: module_t
 * Describes the meory page where the program code is stored.
 * TODO: Support interface/version export */
struct module_t {
   uint8_t *   code_addr;
   size_t      code_size;
};

// group: lifetime

/* define: module_FREE
 * Static initializer. */
#define module_FREE { 0, 0 }

/* function: init_module
 * Maps a file as binary executable into memory whose filename is modulename.
 * The binary must be stored in a directory whose name is configured in
 * resource/config/modulevalues with module="module_t" and name="DIRECTORY*.
 * The binary is mapped as is. No relocation is done and no data segments are
 * supproted. */
int init_module(/*out*/module_t * mod, const char * modulename);

/* function: free_module
 * Unmaps a binary blob from memory. */
int free_module(module_t * mod);

// group: query

/* function: codeaddr_module
 * Returns the start address of the mapped program code. */
uint8_t * codeaddr_module(const module_t * mod);

/* function: codesize_module
 * Returns the size of the mapped program code. */
size_t codesize_module(const module_t * mod);



// section: inline implementation

/* define: codeaddr_module
 * Implements <module_t.codeaddr_module>. */
#define codeaddr_module(mod)              ((mod)->code_addr)

/* define: codesize_module
 * Implements <module_t.codesize_module>. */
#define codesize_module(mod)              ((mod)->code_size)

#endif
