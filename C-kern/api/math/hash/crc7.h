/* title: CRC-7

   Berechnet den CRC-7 Wert einer Byte-Sequenz.

   Dieser 7-Bit Cyclic-Redundancy-Check Wert wird zur Fehlererkennung bei I/O
   Übertragungen genutzt. Der Eingabebytestrom wird als große binäre Zahl aufgefasst,
   die durch ein sogenanntes "Generator Polynom" dividiert wird.
   Der sich daraus ergebende 7-Bit Restwert wird dabei als Resultat der Berechnung aufgefasst.

   Funktionsweise:
   Die Eingabedaten werden als eine lange Integerzahl betrachtet,
   etwa so "00001001001001". Diese Zahl wird durch das 8-Bit
   Generatorpolynom "10001001" modulo-2 dividiert, was einer Xor-Operation entspricht.
   Der Divisionsrest beträgt bei einer 8-Bit Xor-Operation maximal 7-Bit, daher CRC-7.
   Vor der Division wird die zu dividierende Zahl mit 7 0en erweitert, um den Divisions-
   rest aufzunehmen.

   Beispiel:
   >                  -------
   >    000010010010010000000
   > ^      10001001
   > == 000000011011010000000
   > ^         10001001
   > == 000000001010011000000
   > ^          10001001
   > == 000000000010111100000
   > ^            10001001
   > == 000000000000110101000
   > ^              10001001
   > == 000000000000010111010
   > ^               10001001
   > == 000000000000000110011
   >                  -------
   > Ergebnis:        0110011

   Diese Art der Berechnung kann für jedes Bit einzeln berechnet werden, da
   nachfolgende Bits zuerst als 0 angenommen werdem und bei bekanntwerden ihres
   Wertes dieser mittels xor-dazuaddiert wird.

   Dies kann man sich zunutze machen und für alle Bytes die Ergebnisse der Division
   in einer Tabelle ablegen. Jedes weitere Eingabebyte wird dann einfach hinzuaddiert
   mittels xor.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: C-kern/api/math/hash/crc7.h
    Header file <CRC-7>.

   file: C-kern/math/hash/crc7.c
    Implementation file <CRC-7 impl>.
*/
#ifndef CKERN_MATH_HASH_CRC7_HEADER
#define CKERN_MATH_HASH_CRC7_HEADER

// === exported types
struct crc7_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_math_hash_crc7
 * Test <crc7_t> functionality. */
int unittest_math_hash_crc7(void);
#endif


/* struct: crc7_t
 * Speichert die partiell berechnete CRC-7 Prüfsumme.
 *
 * Usage:
 * Mit <init_crc7> wird der Typ initialisiert.
 * Mit <update_crc7> wird die Prüfsumme komplett berechnet oder für den nächsten Datenblock weitergeführt.
 * Mit <value_crc37> lest man die bislang berechnete CRC-7 Prüfsumme.
 *
 * */
typedef struct crc7_t {
   // group: private
   /* variable: value
    * The crc32 value calculated from a sequence of bytes.
    * The value will be updated every time <update_crc32> is called. */
   uint8_t value;
} crc7_t;

// group: lifetime

/* define: crc7_INIT
 * Static initializer. */
#define crc7_INIT  \
         { 0 }

/* function: init_crc7
 * Initialisiert Variable des Typs <crc7_t>. */
void init_crc7(/*out*/crc7_t *crc);

// group: query

/* function: value_crc7
 * Liefert die CRC-7 Prüfsumme.
 * Der berechnete Wert ist die Prüfsumme der Zahl, die sich aus den Bytes der in <update_crc7> übergebenen Daten zusammensetzt.
 * Direkt nach Aufruf von <init_crc7> liefert diese Funktion 0 zurück. */
uint8_t value_crc7(const crc7_t *crc);

// group: update

/* function: update_crc7
 * Passt die schon berechnete CRC-7 Prüfsumme an verlängerte Eingabedaten an.
 * Um eine neue Prüfsumme über einen neuen Datenbestand zu berechnen,
 * muss vorher die Funktion <init_crc7> aufgerufen werden,
 * damit die interne Prüfsumme auf 0 zurückgesetzt wird. */
static inline void update_crc7(crc7_t *crc, size_t blocksize, const void *datablock/*[blocksize]*/);

/* function: update_crc7
 * Interne von <update_crc7> benutzte Funktion. */
uint8_t update2_crc7(uint8_t crcvalue, size_t blocksize, const void *datablock/*[blocksize]*/);


// section: inline implementation

/* define: init_crc7
 * Implements <crc7_t.init_crc7>. */
#define init_crc7(crc) \
         ((void)(*(crc)=(crc7_t)crc7_INIT))

/* define: update_crc7
 * Implements <crc7_t.update_crc7>. */
static inline void update_crc7(crc7_t *crc, size_t blocksize, const void *datablock/*[blocksize]*/)
{
         crc->value = update2_crc7(crc->value, blocksize, datablock);
}

/* define: value_crc7
 * Implements <crc7_t.value_crc7>. */
#define value_crc7(crc) \
         ((uint8_t) (((crc)->value) >> 1))

#endif
