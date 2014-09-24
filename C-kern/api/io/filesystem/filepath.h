/* title: Filesystem-Path

   Offers helper objects to print the file system path of a file or directory object.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/io/filesystem/filepath.h
    Header file <Filesystem-Path>.

   file: C-kern/platform/Linux/io/filepath.c
    Implementation file <Filesystem-Path impl>.
*/
#ifndef CKERN_IO_FILESYSTEM_FILEPATH_HEADER
#define CKERN_IO_FILESYSTEM_FILEPATH_HEADER

// forward
struct directory_t ;

/* typedef: struct filepath_static_t
 * Export <filepath_static_t> into global namespace. */
typedef struct filepath_static_t          filepath_static_t ;



// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_filepath
 * Test <filepath_static_t> functionality. */
int unittest_io_filepath(void) ;
#endif


/* struct: filepath_static_t
 * Allocates static memory for a working directory and filename combination.
 * The path of the working directory always ends in the path separator '/'.
 * So printing workdir and then filename results in a valid path name. */
struct filepath_static_t {
   char           workdir[sys_path_MAXSIZE+1] ;
   const char *   filename ;
} ;

// group: lifetime

/* function: init_filepathstatic
 * Initializes fpath with the path of working directory workdir and filename.
 * The parameter filename is no copied. So fpath is only valid as long as filename
 * is not changed or deleted.
 * The function calls <path_directory> to determine the path of the working directory.
 * If an error occurrs the path of the working directory is set to "???ERR/".
 *
 * It is allowed to set either workdir or filename to 0. If both parameter are set to 0
 * the resulting path is the empty string.
 *
 * You do not need to free fpath cause no extra memory is allocated. */
void init_filepathstatic(/*out*/filepath_static_t * fpath, const struct directory_t * workdir, const char * filename) ;

// group: query

/* function: STRPARAM_filepathstatic
 * This macro returns two string parameter describing path of working directory and filename.
 * MAke sure that the expression which evaluates to fpath has no side effects cause fpath is
 * evaluated twice in the macro.  Use this macro to print the full path name as
 * > printf("%s%s\n", STRPARAM_filepathstatic(fpath)) ; */
void STRPARAM_filepathstatic(const filepath_static_t * fpath) ;



// section: inline implementation

/* define: STRPARAM_filepathstatic
 * Implements <filepath_static_t.STRPARAM_filepathstatic>. */
#define STRPARAM_filepathstatic(fpath) \
         (fpath)->workdir, (fpath)->filename


#endif
