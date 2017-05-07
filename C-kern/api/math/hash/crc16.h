/* title: CRC-16

   Berechnet den CRC-16 Wert einer Byte-Sequenz.

   Dieser 16-Bit Cyclic-Redundancy-Check Wert wird zur Fehlererkennung bei I/O
   Übertragungen genutzt. Der Eingabebytestrom wird als große binäre Zahl aufgefasst,
   die durch ein sogenanntes "Generator Polynom" dividiert wird.
   Der sich daraus ergebende 16-Bit Restwert wird dabei als Resultat der Berechnung aufgefasst.

   Funktionsweise:
   Die Eingabedaten werden als eine lange Integerzahl betrachtet,
   etwa so "00001001001001". Diese Zahl wird durch das 17-Bit Generatorpolynom
   "10001000000100001" (0x11021) modulo-2 dividiert, was einer Xor-Operation entspricht.
   Der Divisionsrest beträgt bei einer 17-Bit Xor-Operation maximal 16-Bit, daher CRC-16.
   Vor der Division wird die zu dividierende Zahl mit 16 0en erweitert, um den Divisions-
   rest aufzunehmen.

   Beispiel:
   >                  ----------------
   >    000010010010010000000000000000
   > ^      10001000000100001
   > == 000000011010010100001000000000
   > ^         10001000000100001
   > == 000000001011010100101001000000
   > ^          10001000000100001
   > == 000000000011110100111001100000
   > ^            10001000000100001
   > == 000000000001111100111101101000
   > ^             10001000000100001
   > == 000000000000111000111111101100
   > ^              10001000000100001
   > == 000000000000011010111110101110
   > ^               10001000000100001
   > == 000000000000001011111110001111
   >                  ----------------
   > Ergebnis:        1011111110001111

   Diese Art der Berechnung kann für jedes Bit einzeln berechnet werden, da
   nachfolgende Bits zuerst als 0 angenommen werdem und bei Bekanntwerden ihres
   Wertes dieser mittels Xor dazuaddiert wird.

   Dies kann man sich zunutze machen und für alle Bytes die Ergebnisse der Division
   in einer Tabelle ablegen. Jedes weitere Eingabebyte wird dann einfach mittels Xor
   hinzuaddiert.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: C-kern/api/math/hash/crc16.h
    Header file <CRC-16>.

   file: C-kern/math/hash/crc16.c
    Implementation file <CRC-16 impl>.
*/
#ifndef CKERN_MATH_HASH_CRC16_HEADER
#define CKERN_MATH_HASH_CRC16_HEADER

// === exported types
struct crc16_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_math_hash_crc16
 * Test <crc16_t> functionality. */
int unittest_math_hash_crc16(void);
#endif


/* struct: crc16_t
 * Speichert die partiell berechnete CRC-16 Prüfsumme.
 *
 * Usage:
 * Mit <init_crc16> wird der Typ initialisiert.
 * Mit <update_crc16> wird die Prüfsumme komplett berechnet oder für den nächsten Datenblock weitergeführt.
 * Mit <value_crc16> liest man die bislang berechnete CRC-16 Prüfsumme.
 * */
typedef struct crc16_t {
   // group: private
   /* variable: value
    * Der über einen Datenstrom berechnete CRC-16 Wert.
    * Der Wert wird mit Aufruf der Funktion <update_crc16> angepasst. */
   uint16_t value;
} crc16_t;

// group: lifetime

/* define: crc16_INIT
 * Static initializer. */
#define crc16_INIT  \
         { 0 }

/* function: init_crc16
 * Initialisiert Variable des Typs <crc16_t>. */
void init_crc16(/*out*/crc16_t *crc);

// group: query

/* function: value_crc16
 * Liefert die CRC-16 Prüfsumme.
 * Der berechnete Wert ist die Prüfsumme der Zahl, die sich aus den Bytes der in <update_crc16> übergebenen Daten zusammensetzt.
 * Direkt nach Aufruf von <init_crc16> liefert diese Funktion 0 zurück. */
uint16_t value_crc16(const crc16_t *crc);

// group: update

/* function: update_crc16
 * Passt die schon berechnete CRC-16 Prüfsumme an verlängerte Eingabedaten an.
 * Um eine neue Prüfsumme über einen neuen Datenbestand zu berechnen,
 * muss vorher die Funktion <init_crc16> aufgerufen werden,
 * damit die interne Prüfsumme auf 0 zurückgesetzt wird. */
static inline void update_crc16(crc16_t *crc, size_t blocksize, const void *datablock/*[blocksize]*/);

/* function: update_crc16
 * Interne von <update_crc16> benutzte Funktion. */
uint16_t update2_crc16(uint16_t crcvalue, size_t blocksize, const void *datablock/*[blocksize]*/);

// group: update



// section: inline implementation

/* define: init_crc16
 * Implements <crc16_t.init_crc16>. */
#define init_crc16(crc) \
         ((void)(*(crc)=(crc16_t)crc16_INIT))

/* define: update_crc16
 * Implements <crc16_t.update_crc16>. */
static inline void update_crc16(crc16_t *crc, size_t blocksize, const void *datablock/*[blocksize]*/)
{
         crc->value = update2_crc16(crc->value, blocksize, datablock);
}

/* define: value_crc16
 * Implements <crc16_t.value_crc16>. */
#define value_crc16(crc) \
         ((crc)->value)


#endif
