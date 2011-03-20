#include "C-kern/konfig.h"
#include "C-kern/api/umgebung.h"
#include "C-kern/api/umgebung/log.h"
#include "C-kern/api/umgebung/object_cache.h"
#include "C-kern/api/generic/integer.h"
#include "C-kern/api/os/filesystem/directory.h"
#include "C-kern/api/os/filesystem/mmfile.h"
#include "C-kern/api/os/thread.h"
#include "C-kern/api/os/virtmemory.h"

/* dummy project main:
 * Calls a single test function. */

#define RUN(test) \
   if (!err && test()) { \
      err = 1 ; \
      printf("Failed: " #test "\n") ; \
   }

int main(int argc, char * argv[])
{
   (void) argc ;
   (void) argv ;
   int err = 0 ;

   RUN(unittest_umgebung) ;
   RUN(unittest_umgebung_default) ;
   RUN(unittest_umgebung_testproxy) ;
   RUN(unittest_umgebung_log) ;
   RUN(unittest_umgebung_objectcache) ;
   RUN(unittest_os_thread) ;
   RUN(unittest_os_memorymappedfile) ;
   RUN(unittest_os_virtualmemory) ;
   RUN(unittest_directory) ;
   RUN(unittest_generic_integer) ;

   if (err) {
      printf("----------\n") ;
      printf("Test ERROR\n") ;
      printf("----------\n") ;
   } else {
      printf("-------\n") ;
      printf("Test OK\n") ;
      printf("-------\n") ;
   }

   return err ;
}
