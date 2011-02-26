#include "C-kern/konfig.h"
#include "C-kern/api/umgebung.h"
#include "C-kern/api/os/thread.h"

/* dummy project main:
 * Calls a single test function. */

int main(int argc, char * argv[])
{
   (void) argc ;
   (void) argv ;
   int err ;

   err = unittest_umgebung() ;
   if (!err) err = unittest_umgebung_default() ;
   if (!err) err = unittest_umgebung_testproxy() ;
   if (!err) err = unittest_os_thread() ;

   if (err) {
      printf("Test error: %d\n", err) ;
   } else {
      printf("-------\n") ;
      printf("Test OK\n") ;
      printf("-------\n") ;
   }

   return err ;
}
