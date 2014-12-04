/* title: SearchString

   Imlementiert Suchen eines Teilstrings in einem größeren String,
   z.B. einer Textdatei.

   Implements searching of substring in a larger string.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/string/strsearch.h
    Header file <SearchString>.

   file: C-kern/string/strsearch.c
    Implementation file <SearchString impl>.
*/
#ifndef CKERN_STRING_STRSEARCH_HEADER
#define CKERN_STRING_STRSEARCH_HEADER

/* typedef: struct strsearch_t
 * Export <strsearch_t> into global namespace. */
typedef struct strsearch_t strsearch_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_string_strsearch
 * Test <strsearch_t> functionality. */
int unittest_string_strsearch(void);
#endif


/* struct: strsearch_t
 * Erlaubt das wiederholte Suchen eines Strings in Texten.
 * Mittels <init_strsearch> wird eine aus dem Suchstring berechnete
 * Hilfstabelle einmalig aufgebaut und mit <find_strsearch> kann
 * dieser String wiederholt in verschiedenen Daten gesucht werden.
 *
 * TODO: add bad chr heuristic (simple bit field if first char compare fails)
 *
 * Vereinfachter Boyer-Moore Algorithmus:
 * Der Suchalgorithmus ist ein vereinfachter Boyer-Moore,
 * der ohne Bad-Character-Heuristic implementiert ist.
 *
 * Verglichen wird zuerst der letzte Buchstaben des Suchstrings,
 * schlägt dieser Vergleich fehl, wird der Suchstring um eine Stelle
 * weiter bewegt und wieder der letzte Buchstaben verglichen.
 *
 * Ist der letzte Buchstabe des Suchstrings gleich dem darunterliegenden
 * des Textes, wird der vorletzte verglichen usw. bis zum ersten. Schlägt
 * eine Vergliech fehl, wird der Suchstring soweit weiterbewegt, bis er
 * sich wieder mit den bereits verglichenen Buchstaben überdeckt. Geht dies
 * nicht, wird der Suchstring um seine eigene Länge weiterbewegt.
 *
 * Um wieviel der Suchstring weiterbewegt werden muss, wird einmalig berechnet
 * und in einer Hilfstabelle abegelgt.
 *
 * */
struct strsearch_t {
   // group: private fields
   /* variable: shift
    * Points to array which contains the number of positions findstr has to be shifted right. */
   uint8_t*       shift;/*[findsize]*/
   /* variable: findstr
    * Points to substring which is searched for. */
   const uint8_t* findstr;/*[findsize]*/
   /* variable: findsize
    * Length of shift and findsize. */
   uint8_t        findsize;
};

// group: lifetime

/* define: strsearch_FREE
 * Static initializer. */
#define strsearch_FREE \
         { 0, 0, 0 }

/* function: init_strsearch
 * Initialisiert strsrch mit findsize, findstr und shift.
 * Das Array findstr beinhaltet den zu suchenden String und das Array shift bekommt
 * berechnete Werte zugewiesen, die in <find_strsearch> benötigt werden.
 *
 * Zeit:
 * O(findsize)
 *
 * Parameter-Array-Lebensdauer:
 * Die Arrays findstr und shift dürfen nicht freigegeben werden oder deren Inhalte geändert werden,
 * solange strsrch verwendet wird.
 *
 * Freigabe:
 * Da der Aufrufer den Speicher allokiert, muss strsrch auch nicht freigegeben werden. */
void init_strsearch(/*out*/strsearch_t* strsrch, uint8_t findsize, const uint8_t findstr[findsize], uint8_t shift[findsize]);

// group: query

/* function: find_strsearch
 * Sucht in data nach findstr, der als Parameter in <init_strsearch> übergeben wurde.
 * strsrch muss zuvor mittels <init_strsearch> initialisiert worden sein.
 * Wenn mehrfach nach demselben String in einem Text gesucht werden soll,
 * spart diese Funktion verglichen mit <strsearch> Zeit. Die Initialisierung der Such-Hilfstabelle
 * wird in <init_strsearch> nur einmal getan, im Gegensatz dazu initialisiert <strsearch> die
 * Hilfstabelle jedesmal aufs Neue.
 *
 * Siehe auch <strsearch> für eine weitergehende Beschreibung. */
const uint8_t* find_strsearch(strsearch_t* strsrch, size_t size, const uint8_t data[size]);

/* function: strsearch
 * Suche Teilstring findstr im Text data von vorn nach hinten.
 * Falls ("findstr is not found in data" || size == 0 || findsize == 0) wird der Wert
 * 0 zurückgegeben.
 * Wenn findstr mehr als einmal in vorkommt, wird immer das erste Auftreten mit
 * der niedrigsten Speicheradresse gemeldet.
 *
 * Used algorithmus:
 * Simplified Boyer-Moore without Bad-Character-Heuristic.
 * Like KMP but comparing starts from end of findstr (findstr[findsize-1]).
 *
 * Worst case:
 * O(size)
 */
const uint8_t* strsearch(size_t size, const uint8_t data[size], uint8_t findsize, const uint8_t findstr[findsize]);



// section: inline implementation

#endif
