#include "C-kern/konfig.h"
#include "C-kern/api/umgebung.h"

/* dummy project main:
 * Calls a single test function. */

int main(int argc, char * argv[])
{
   (void) argc ;
   (void) argv ;
   int err ;

   err = unittest_umgebung_initprocess() ;
   if (!err) err = unittest_umgebung_testproxy() ;

   if (err) {
      printf("Test error: %d\n", err) ;
   } else {
      printf("Test OK\n") ;
   }

   return err ;
}
