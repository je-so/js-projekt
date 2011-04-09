/* title: Unittest impl
   Implements <run_unittest>.

   Ruft nacheinander alle Unittests auf.

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

   file: C-kern/api/test/unittest.h
    Header file of <Unittest>.

   file: C-kern/test/run_unittest.c
    Implements <run_unittest>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/umgebung.h"
#include "C-kern/api/umgebung/log.h"
#include "C-kern/api/umgebung/object_cache.h"
#include "C-kern/api/math/int/signum.h"
#include "C-kern/api/math/int/power2.h"
#include "C-kern/api/os/filesystem/directory.h"
#include "C-kern/api/os/filesystem/mmfile.h"
#include "C-kern/api/os/thread.h"
#include "C-kern/api/os/virtmemory.h"



typedef char RESULT_STRING[20] ;


/* Ruft einen Unittest auf und notiert das Ergebnis. */
#define RUN(FCT) \
   LOG_CLEARBUFFER() ; \
   err = FCT() ; \
   if (!err && isGenerateLogResource) generate_logresource(#FCT) ; \
   if (!err && isCheckLogResource) err = check_logresource(#FCT) ; \
   ++total_count ; if (err) ++err_count ; \
   print_result( err, &progress, &progress_count) ;

#define GENERATED_LOGRESOURCE_DIR   "C-kern/resource/unittest.log/"


/* */
static void generate_logresource(const char * test_name)
{
   int     err = EINVAL ;
   int     fd = -1 ;
   char   resource_path[sizeof(GENERATED_LOGRESOURCE_DIR) + strlen(test_name)] ;

   strcpy( resource_path, GENERATED_LOGRESOURCE_DIR ) ;
   strcpy( resource_path + sizeof(GENERATED_LOGRESOURCE_DIR)-1, test_name ) ;

   fd = open( resource_path, O_CREAT|O_EXCL|O_RDWR|O_CLOEXEC, S_IRUSR|S_IWUSR ) ;
   if (fd < 0) {
      err = errno ;
      goto ABBRUCH ;
   }

   char   * logbuffer ;
   size_t logbuffer_size ;
   LOG_GETBUFFER( &logbuffer, &logbuffer_size ) ;

   if (logbuffer_size) {
      int logsize = write( fd, logbuffer, logbuffer_size ) ;
      if (logbuffer_size != (unsigned)logsize) {
         dprintf( STDERR_FILENO, "logbuffer_size = %d, logsize = %d\n", logbuffer_size, logsize) ;
         goto ABBRUCH ;
      }
   }

   close(fd) ;
   return ;
ABBRUCH:
   if (err != EEXIST) {
      dprintf( STDERR_FILENO, "%s: %s:\n", __FILE__, __FUNCTION__ ) ;
      dprintf( STDERR_FILENO, "ERROR(%d:%s): '" GENERATED_LOGRESOURCE_DIR "%s'\n", err, strerror(err), test_name ) ;
   }
   if (fd >= 0) close(fd) ;
   return ;
}


static int check_logresource(const char * test_name)
{
   int              err ;
   mmfile_t     logfile = mmfile_INIT_FREEABLE ;
   off_t   logfile_size ;
   char   resource_path[sizeof(GENERATED_LOGRESOURCE_DIR) + strlen(test_name)] ;

   strcpy( resource_path, GENERATED_LOGRESOURCE_DIR ) ;
   strcpy( resource_path + sizeof(GENERATED_LOGRESOURCE_DIR)-1, test_name ) ;

   err = filesize_directory(resource_path, 0, &logfile_size ) ;
   if (err) goto ABBRUCH ;

   if (logfile_size != 0) {
      err = init_mmfile( &logfile, resource_path, 0, 0, 0, mmfile_openmode_RDONLY ) ;
      if (err) goto ABBRUCH ;
      logfile_size = sizeinbytes_mmfile(&logfile) ;
   }

   char * logbuffer ;
   size_t logbuffer_size ;
   LOG_GETBUFFER( &logbuffer, &logbuffer_size ) ;

   if (logbuffer_size != logfile_size) {
      dprintf( STDERR_FILENO, "logbuffer_size = %d, logfile_size = %d\n", (int)logbuffer_size, (int)logfile_size) ;
      err = EINVAL ;
      goto ABBRUCH ;
   }

   if (logbuffer_size && memcmp( memstart_mmfile(&logfile), logbuffer, logbuffer_size )) {
      dprintf( STDERR_FILENO, "Content of read logbuffer:\n%*s\n", logbuffer_size, logbuffer) ;
      err = EINVAL ;
      goto ABBRUCH ;
   }

   err = free_mmfile(&logfile) ;
   if (err) goto ABBRUCH ;

   return 0 ;
ABBRUCH:
   dprintf( STDERR_FILENO, "%s: %s:\n", __FILE__, __FUNCTION__ ) ;
   dprintf( STDERR_FILENO, "ERROR(%d:%s): '" GENERATED_LOGRESOURCE_DIR "%s'\n", err, strerror(err), test_name ) ;
   free_mmfile(&logfile) ;
   return err ;
}

static void print_result(int err, RESULT_STRING * progress, unsigned * progress_count)
{
   assert(*progress_count < sizeof(*progress)) ;
   if (err) {
      (*progress)[*progress_count] = '!' ;
   } else {
      (*progress)[*progress_count] = '.' ;
   }
   ++ (*progress_count) ;
   printf( "UNITTEST: %.*s\n", (*progress_count), (*progress)) ;
   if (*progress_count == sizeof(*progress)) {
      (*progress_count) = 0 ;
   }
}

int run_unittest(void)
{
   int err ;
   unsigned      err_count = 0 ;
   unsigned    total_count = 0 ;
   unsigned progress_count = 0 ;
   const bool        oldlog_onstate = LOG_ISON() ;
   const bool  oldlog_bufferedstate = LOG_ISBUFFERED() ;
   const bool isGenerateLogResource = true ;   // SET !true! if unittests has changed !!
   const bool    isCheckLogResource = true ;
   RESULT_STRING           progress ;
#define     test_umgebung_type_SIZE /*=*/ 2 /*;*/
   const umgebung_type_e test_umgebung_type[test_umgebung_type_SIZE] = {
       umgebung_type_TEST
      ,umgebung_type_DEFAULT
   } ;

   // prepare
   {
      vm_mappedregions_t mappedregions  = vm_mappedregions_INIT_FREEABLE ;
      vm_mappedregions_t mappedregions2 = vm_mappedregions_INIT_FREEABLE ;
      init_vmmappedregions(&mappedregions) ;
      init_vmmappedregions(&mappedregions2) ;
      free_vmmappedregions(&mappedregions) ;
      free_vmmappedregions(&mappedregions2) ;
   }

   LOG_TURNON() ;
   if (  isGenerateLogResource
         || isCheckLogResource) {
      LOG_CONFIG_BUFFERED(true) ;
   }

   // before init
   RUN(unittest_umgebung) ;
   RUN(unittest_umgebung_default) ;
   RUN(unittest_umgebung_testproxy) ;
   RUN(unittest_umgebung_log) ;
   RUN(unittest_umgebung_objectcache) ;

for(int type_nr = 0; type_nr < test_umgebung_type_SIZE ; ++type_nr) {

   // init
   if (init_process_umgebung(test_umgebung_type[type_nr])) {
      dprintf( STDERR_FILENO, "%s: %s:\n", __FILE__, __FUNCTION__ ) ;
      dprintf( STDERR_FILENO, "%s\n", "Abort reason: init_process_umgebung failed" ) ;
      goto ABBRUCH ;
   }

   // prepare
   (void) strerror(ENOMEM) ;

   // current development
   // assert(0) ;

//{ string unittest
//}

//{ generic unittest
   // bitmanipulations
   RUN(unittest_math_int_signum) ;
   RUN(unittest_math_int_power2) ;
   // list
   // set / dictionary
   // trie (string dictionary)
//}

//{ compiler unittest
//}

//{ os unittest
   // filesystem
   RUN(unittest_os_directory) ;
   RUN(unittest_os_memorymappedfile) ;
   // other
   RUN(unittest_os_thread) ;
   RUN(unittest_os_virtualmemory) ;
#define X11 1
#if (KONFIG_GRAPHIK==X11)
   // graphik subsystem
   RUN(unittest_os_X11) ;
   RUN(unittest_os_X11_display) ;
   RUN(unittest_os_X11_glxwindow) ;
#endif
#undef X11
//}

   LOG_CLEARBUFFER() ;

   if (free_process_umgebung()) {
      dprintf( STDERR_FILENO, "%s: %s:\n", __FILE__, __FUNCTION__ ) ;
      dprintf( STDERR_FILENO, "%s\n", "Abort reason: free_process_umgebung failed" ) ;
      goto ABBRUCH ;
   }

   break ; // TODO remove line (execute tests only once)
}

ABBRUCH:

   if (!oldlog_onstate) LOG_TURNOFF() ;
   LOG_CONFIG_BUFFERED(oldlog_bufferedstate) ;

   if (!err_count) {
      printf( "\nALL UNITTEST(%d): OK\n", total_count) ;
   } else if (err_count == total_count) {
      printf( "\nALL UNITTEST(%d): FAILED\n", total_count) ;
   } else {
      printf( "\n%d UNITTEST: OK\n%d UNITTEST: FAILED\n", total_count-err_count, err_count) ;
   }

   free_process_umgebung() ;

   return err_count > 0 ;
}
