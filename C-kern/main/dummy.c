#include <stdio.h>

/* dummy project main:
 * Calls a single test function. */

int main(int argc, char * argv[])
{
   (void) argc ;
   (void) argv ;
   int err ;

   err = 0 ;

   if (err) {
      printf("Test error: %d\n", err) ;
   } else {
      printf("Test OK\n") ;
   }

   return err ;
}
