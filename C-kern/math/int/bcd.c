/* title: Intop-BCD impl

   Implements <Intop-BCD>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2017 Jörg Seebohn

   file: C-kern/api/math/int/bcd.h
    Header file <Intop-BCD>.

   file: C-kern/math/int/bcd.c
    Implementation file <Intop-BCD impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/math/int/bcd.h"
#include "C-kern/api/math/int/log2.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif

// === private types


// section: int_t

// group: compute

/*
   Wie funktioniert die Implementierung von convert2bcd_int?

   Eine Hex-Ziffer im  Wert von 0 bis 9 ist automatisch im richtigen Format.
   Eine Hex-Ziffer im  Wert von 10 bis 15 (A..F) muss um den Wert 6 erhöht werden,
   damit die korrekte BCD-Ziffernfolge 0x10(==10+6) bis 0x15(==15+6) entsteht.

   Sei nun B eine korrekt BCD kodierte Integerzahl. Die Hexdarstellung verwendet
   also nur Ziffern von 0 bis 9. Der Wert einer BCD Zahl von z.B. 0x123 entspricht
   der Dezimalzahl 123.

   Beobachtung:

   Bei Multiplikation der Zahl B mit 2 werden alle Ziffern von 0 bis 4 danach korrekt
   dargestellt von 0 bis 8.
   Ziffern von 5 bis 9 verlangen eine Korrektur. 5*2 ergibt Dezimal 10 muss aber in
   der BCD Kodierung als 0x10 dargestellt werden, also den dezimalen Wert 16 ergeben.
   Es muss also ein Korrekturwert von 6 addiert werden.

   Um jede BCD-Ziffer der Zahl B vor der Multiplikation zu testen, ob diese korrigiert werden muss,
   wird 0x33333333... dazuaddiert. Ziffern von 0 bis 4 ergeben die Werte 0..7.
   Ziffern jedoch von 5..9 ergeben Werte von 8..12. Diese zu korrigierenden Ziffern haben alle
   das Bit 0x8 gesetzt.

   Bildet man eine Maske, wo dieses Bit gesetzt ist (BCD-Ziffer >= 5) oder nicht (BCD-Ziffer<5),
   dann kann der Korrekturwert 6 mittels dieser Maske addiert werden als (Maske >> 2)+(Maske >> 1).
   Das Bit Acht wird auf die Werte 2 und 4 gbracht und dann addiert.

   Nach der Multiplikation mit 2 ist das niederwertigste Bit von B nicht gesetzt und kann mit
   dem nächsten höchstwertigen Bit des zu konvertierenden Integers befüllt werden.
*/

uint32_t convert2bcd_int32(uint32_t i)
{
   uint32_t bcd = i, tmp, incr, mask;
   if (i>9) {
      incr = 0x33333333;
      mask = 0x88888888;
      unsigned nbit = log2_int(i); // log2(8)==3 && i>9 ==> nbit>=3 (nbit == number_of_bits_needed_to_represent_i-1)
      nbit -= 3;     // (nbit == number_of_bits_needed_to_represent_i-4) >= 0
      if (nbit) {
         bcd >>= nbit;  // bcd contains topmost 4 bits of i in least 4 bits ==> 8<= bcd <=15
         i <<= bitsof(i)-nbit; // topmost bit of i contains next bit
         if (bcd>9) bcd+=6; // bcd now represents the topmost 4 bits of i correctly BCD encoded
         do {
            tmp = bcd+incr; // every digit 0x...5... up to 0x...9... produces 0x...8.... upto 0x...c.... with 0x...8... always set
            bcd <<= 1;     // multiplies every digit by 2 (digit >= 5 are added with 3 ==> digit >= 8 ==> 2*digit >= 16)
            tmp &= mask; // every 0x...8... (upto 0x...c...) is reduced to 0x...8... other digits to 0x...0...
            bcd |= i>>(bitsof(bcd)-1); // shift topmost bit of i into lowest position of bcd
            i <<= 1;
            bcd += tmp>>2; // adds 0x...2... to every bcd digit >= 5
            bcd += tmp>>1; // adds 0x...4... to every bcd digit >= 5 (both together add 0x...6...)
         } while (--nbit>0);
      } else { // bcd >>= 0; ==> bcd = i; && i > 9 ==> bcd > 9
         bcd += 6; // bcd now represents the topmost 4 bits of i correctly BCD encoded
      }
   }
   return bcd;
}

uint64_t convert2bcd_int64(uint64_t i)
{
   uint64_t bcd=i, tmp, incr, mask;
   if (i>9) {
      incr = 0x3333333333333333;
      mask = 0x8888888888888888;
      unsigned nbit = log2_int(i); // log2(8)==3 && i>9 ==> nbit>=3 (nbit == number_of_bits_needed_to_represent_i-1)
      nbit -= 3;     // (nbit == number_of_bits_needed_to_represent_i-4) >= 0
      if (nbit) {
         bcd >>= nbit;  // bcd contains topmost 4 bits of i in least 4 bits ==> 8<= bcd <=15
         i <<= bitsof(i)-nbit; // topmost bit of i contains next bit
         if (bcd>9) bcd+=6; // bcd now represents the topmost 4 bits of i correctly BCD encoded
         do {
            tmp = bcd+incr; // every digit 0x...5... up to 0x...9... produces 0x...8.... upto 0x...c.... with 0x...8... always set
            bcd <<= 1;     // multiplies every digit by 2 (digit >= 5 are added with 3 ==> digit >= 8 ==> 2*digit >= 16)
            tmp &= mask; // every 0x...8... (upto 0x...c...) is reduced to 0x...8... other digits to 0x...0...
            bcd |= i>>(bitsof(bcd)-1); // shift topmost bit of i into lowest position of bcd
            i <<= 1;
            bcd += tmp>>2; // adds 0x...2... to every bcd digit >= 5
            bcd += tmp>>1; // adds 0x...4... to every bcd digit >= 5 (both together add 0x...6...)
         } while (--nbit>0);
      } else { // bcd >>= 0; ==> bcd = i; && i > 9 ==> bcd > 9
         bcd += 6; // bcd now represents the topmost 4 bits of i correctly BCD encoded
      }
   }
   return bcd;
}


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_convert2bcd(void)
{
   // TEST convert2bcd_int: (input: 0<= i <=9) ==> bcd has same representation 0x0..0x9
   for (unsigned i=0; i<=9; ++i) {
      TEST( i==convert2bcd_int(i));
      TEST( i==convert2bcd_int32(i));
      TEST( i==convert2bcd_int64(i));
   }

   // TEST convert2bcd_int: (input: 10<= i <=15 && nrbits(i)==4) ==> 0x10<= bcd <=0x15
   for (unsigned i=10; i<=15; ++i) {
      unsigned e/*expect*/= 0x10-10+i;
      TEST( e==convert2bcd_int(i));
      TEST( e==convert2bcd_int32(i));
      TEST( e==convert2bcd_int64(i));
   }

   // TEST convert2bcd_int: (input: 16<= i <=99 && nrbits(i)<=7) ==> 0x16<= bcd <=0x99
   for (unsigned i=16; i<=99; ++i) {
      unsigned e/*expect*/= (i/10)*16 + i%10;
      TEST( e==convert2bcd_int(i));
      TEST( e==convert2bcd_int32(i));
      TEST( e==convert2bcd_int64(i));
   }

   // TEST convert2bcd_int: (input: 100<= i <=999 && nrbits(i)<=10) ==> 0x100<= bcd <=0x999
   for (unsigned i=100; i<=999; ++i) {
      unsigned e/*expect*/= (i/100)*256 + ((i%100)/10)*16 + i%10;
      TEST( e==convert2bcd_int(i));
      TEST( e==convert2bcd_int32(i));
      TEST( e==convert2bcd_int64(i));
   }

   // TEST convert2bcd_int: (input: 1000<= i <=9999) ==> 0x1000<= bcd <=0x9999
   for (unsigned i=1000; i<=9999; ++i) {
      unsigned e/*expect*/= (i/1000)*4096 + ((i/100)%10)*256 + ((i/10)%10)*16 + i%10;
      TEST( e==convert2bcd_int(i));
      TEST( e==convert2bcd_int32(i));
      TEST( e==convert2bcd_int64(i));
   }

   // TEST convert2bcd_int: all 32 bits with pattern
   for (unsigned i=1; i<=99; ++i) {
      uint64_t e/*expect*/= (i/10)*16 + i%10;
      for (uint32_t i2=i; e <= UINT32_MAX; e*=16, i2*=10) {
         TEST( e==convert2bcd_int(i2));
         TEST( e==convert2bcd_int32(i2));
         TEST( e==convert2bcd_int64(i2));
      }
      e = (i/10)*16 + i%10;
      for (uint32_t i2=i,e2=(uint32_t)e; e <= UINT32_MAX; e*=256, e+=e2, i2*=100, i2+=i) {
         TEST( e==convert2bcd_int(i2));
         TEST( e==convert2bcd_int32(i2));
         TEST( e==convert2bcd_int64(i2));
      }
   }

   // TEST convert2bcd_int: all 64 bits with pattern
   for (unsigned i=1; i<=99; ++i) {
      uint64_t e/*expect*/= (i/10)*16 + i%10;
      for (uint64_t i2=i; e; e*=16, i2*=10) {
         TEST( e==convert2bcd_int(i2));
         TEST( e==convert2bcd_int64(i2));
      }
      e = (i/10)*16 + i%10;
      for (uint64_t i2=i,e2=e; e; e=(e>>56)?0:e*256+e2, i2*=100, i2+=i) {
         TEST( e==convert2bcd_int(i2));
         TEST( e==convert2bcd_int64(i2));
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

int unittest_math_int_bcd()
{
   if (test_convert2bcd())       goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
