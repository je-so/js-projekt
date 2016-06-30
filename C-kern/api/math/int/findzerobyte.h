/* title: Intop-FindZeroByte

   Sucht ein 0-Byte in einem Multibyte-Wort.

   Funktionsweise:
   Der Wert val sei vom Typ uint32_t.
   Dann kann mittels folgender Formel berechnet werden,
   ob val ein 0-Byte enthält. Der Wert der Formel ist != 0,
   wenn val ein 0-Byte enthält.

   Formel:
   (val-0x01010101) & (~val) & 0x80808080

   Erklärung:
   1. Die Berechnung von 0xXX-0x01 ergibt einen Wert kleiner 0 (Bit 0x80 gesetzt),
      wenn der Wert 0xXX aus der Menge { 0x00, 0x81, 0x82, ..., 0xFF } stammt.
      Wenn 0xXX also entweder negativ (außer 0x80) bzw. 0 ist.
   2. Die Berechnung von ~0xXX ergibt einen Wert kleiner 0 (Bit 0x80 gesetzt),
      wenn der Wert 0xXX aus der Menge { 0x00, 0x01, 0x02, ..., 0x7F } stammt.
      Wenn 0xXX also entweder positiv bzw. 0 ist.
   3. Die Berechnung (0xXX-0x01) & ~0xXX ergibt einen Wert kleiner 0 (Bit 0x80 gesetzt),
      wenn der Wert 0xXX aus der Schnittmenge beider in Schritt 1 und 2 berechneten
      Mengen ist. D.h. nur der Wert 0 erfüllt diese Bedingung.
   4. Mittels der Berechnung (...) & 0x80808080 wird das Vorzeichenbit für jedes
      Byte herausgefiltert. Ist das Ergebnis != 0, dann besitzt val ein 0-Byte.
   5. Hinweis: Nur das Vorzeichenbit (0x80) des niederwertigsten Bytes ist immer korrekt.
      Beispiel: Sei val der Wert 0x01010100.
      ==> (val-0x01010101) & (~val) & 0x80808080 == (0xffffffff) & (0xf7f7f7ff) & 0x80808080
      ==> 0xf7f7f7ff & 0x80808080 == 0x80808080
      D.h. alle Vorzeichenbits sind im berechneten Ergebnis gesetzt, aber nur nur das niederwertigste
      Byte von val ist 0. Die Erklärung ist, dass 0x00-0x01 den Wert 0xff ergibt mit einem negativen
      Übertrag. Der negative Übertrag wirkt sich auf das nächsthhöhere Byte aus und kann dort
      zu einem falschen Positiv führen.
   6. Fertig.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: C-kern/api/math/int/findzerobyte.h
    Header file <Intop-FindZeroByte>.

   file: C-kern/math/int/findzerobyte.c
    Implementation file <Intop-FindZeroByte impl>.
*/
#ifndef CKERN_MATH_INT_FINDZEROBYTE_HEADER
#define CKERN_MATH_INT_FINDZEROBYTE_HEADER



// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_math_int_findzerobyte
 * Test searching 0 byte in multibyte integer values. */
int unittest_math_int_findzerobyte(void);
#endif


/* struct: int_t
 * Dummy Type to mark integer functions. */
struct int_t;

// group: byte-operations

/* function: findzerobyte_int
 * Sucht nach Werten in val von der Form 0xXX00XXXXXX, ..., 0xXXXXXX00.
 * Es wird die Position des gefundenen \0 Bytes beginnend mit dem
 * niederwertigsten Byte zurückgegeben. Das niederwertigste Byte hat
 * die Position 1, das Byte 0x00 in 0x..00XX die Position 2 usw.
 * Sollte val mehrere 0-Bytes beinhalten, dann wird immer das Byte mit
 * der kleinsten Nummer bzw. das am niederwertigste Byte zurückgegeben.
 *
 * Angestellte Berechnung:
 * > if ((val & 0x000000FF) == 0) return 1;
 * > if ((val & 0x0000FF00) == 0) return 2;
 * > if ((val & 0x00FF0000) == 0) return 3;
 * > if ((val & 0xFF000000) == 0) return 4;
 * > return 0;
 *
 * Verwendung:
 * Wird eine Bytesuche im Speicher durchgeführt und mittels
 * > val = *(uint64_t*)str
 * ein neuer Wert geladen, so muss darauf geachtt werden, dass
 * str[0] das niederwertigste Byte in val ist.
 * Ist der Prozessor in Bigendian-Typ, dann muss vorher die Funktion
 * val mit der Funktion <htole_int> transformiert werden.
 *
 * Suche nach nicht 0-Bytes:
 * Wenn nach anderen Werten als 0 gesucht werden soll, dann ist dieser
 * Wert per Xor-Operation
 * > val ^= 0xAAAAAAAA
 * von val abzuziehen. Enthählt val den Wert 0xAA, so produziert 0xAA^0xAA
 * den Wert 0x00.
 *
 * Return:
 * 0 - Alle 0xXX im Wort sind != 0x00
 * i > 0 - Das i-te Byte in val hat den Wert 0x00 (0xXX..XX00YY..YY) und YY != 0x00
 *         und Bytelänge aller YY ist (i-1). */
uint8_t findzerobyte_int(unsigned val);

/* function: findzerobyte_int32
 * Implementiert findzerobyte_int für Wortlänge uint32_t. */
static inline unsigned findzerobyte_int32(uint32_t val);

/* function: findzerobyte_int64
 * Implementiert findzerobyte_int für Wortlänge uint64_t. */
static inline unsigned findzerobyte_int64(uint64_t val);


// section: inline implementation

/* define: findzerobyte_int
 * Implements <int_t.findzerobyte_int>. */
#define findzerobyte_int(val) \
         ( __extension__ ({                                    \
            unsigned _r;                                       \
            typeof(val) _v = (val);                            \
            static_assert(sizeof(_v) >= sizeof(uint32_t), );   \
            static_assert(sizeof(_v) <= sizeof(uint64_t), );   \
            if (sizeof(_v) == sizeof(uint32_t)) {              \
               _r = findzerobyte_int32((uint32_t)_v);          \
            } else if (sizeof(_v) == sizeof(uint64_t)) {       \
               _r = findzerobyte_int64((uint64_t)_v);          \
            }                                                  \
            _r;                                                \
         }))

/* define: findzerobyte_int32
 * Implements <int_t.findzerobyte_int32>. */
static inline unsigned findzerobyte_int32(uint32_t val)
{
         val = (~ val) & (val - 0x01010101) & 0x80808080;
         if (!val) return 0;
         for (unsigned s = 0; s < 3; ++s, val >>= 8) {
            if (val & 0xff) return 1+s;
         }
         return 4;
}

/* define: findzerobyte_int64
 * Implements <int_t.findzerobyte_int64>. */
static inline unsigned findzerobyte_int64(uint64_t val)
{
         val = (~ val) & (val - 0x0101010101010101) & 0x8080808080808080;
         if (!val) return 0;
         for (unsigned s = 0; s < 7; ++s, val >>= 8) {
            if (val & 0xff) return 1+s;
         }
         return 8;
}


#endif
