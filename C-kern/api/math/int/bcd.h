/* title: Intop-BCD

   Unterstützt Darstellung von Integer als Binary-Coded-Decimal.
   Bei BCD werden die Ziffern 0-9 mittels 4-Bit kodiert.

   Dezimal-   BCD Darstellung als Bits
   Zahl     | [3|2|1|0]
   --------------------
   0        |  0 0 0 0
   1        |  0 0 0 1
   2        |  0 0 1 0
   ...
   7        |  0 1 1 1
   8        |  1 0 0 0
   9        |  1 0 0 1

   Die 4-Bit Kodierungen 1010 bis 1111 der werden nicht verwendet.

   Ein Dezimalzahl 1234567890 wird demnach kodiert in der Form 0x1234567890.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2017 Jörg Seebohn

   file: C-kern/api/math/int/bcd.h
    Header file <Intop-BCD>.

   file: C-kern/math/int/bcd.c
    Implementation file <Intop-BCD impl>.
*/
#ifndef CKERN_MATH_INT_BCD_HEADER
#define CKERN_MATH_INT_BCD_HEADER


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_math_int_bcd
 * Test <int_t> functionality. */
int unittest_math_int_bcd(void);
#endif


// struct: int_t

// group: compute

/* function: convert2bcd_int
 * Konvertiert den Integer i in die BCD-Darstellung.
 * Der Wert von i muss kleiner oder gleich 99999999
 * für 32-Bit bzw. kleiner oder gleich 9999999999999999
 * für 64-Bit Integer sein.
 *
 * Examples:
 * > convert2bcd_int(1234567890123456) == 0x1234567890123456
 * > convert2bcd_int(90123456) == 0x90123456
 * > convert2bcd_int(123) == 0x123
 *
 * Explanation:
 * See "Double dabble" from Wikipedia at https://en.wikipedia.org/wiki/Double_dabble.
 *
 * Returns:
 * Valid BCD representation of i if (i<=9999999999999999). */
unsigned convert2bcd_int(unsigned i);

/* function: convert2bcd_int32
 * Implementiert <convert2bcd_int> für 32-Bit Integer.
 *
 * Returns:
 * Valid BCD representation of i if (i<=99999999). */
uint32_t convert2bcd_int32(uint32_t i);

/* function: convert2bcd_int64
 * Implementiert <convert2bcd_int> für 64-Bit Integer.
 *
 * Returns:
 * Valid BCD representation of i if (i<=9999999999999999). */
uint64_t convert2bcd_int64(uint64_t i);


// section: inline implementation

#define convert2bcd_int(i) \
         (_Generic((i), uint64_t: convert2bcd_int64(i), default: convert2bcd_int32((uint32_t)(i))))


#endif