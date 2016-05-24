/* title: Async-Serial-Communication

   Die serielle Schnittstelle überträgt Daten serielle
   auf einer Daten-Leitung zwischen Computern und externen
   Systemen (Peripheriegeräten). Allgemein wird damit die
   asynchrone serielle Kommunikation bezeichnet, die von
   einem UART-Baustein, einem Universal Asynchronous Receiver Transmitter,
   implementiert wird.

   Unter Linux/Unix sind die seriellen Schnittstelle zumeist als "/dev/ttyXXX"
   im Verzeichnisbaum sichtbar. Die TTY-Schnittstelle ist eine alte serielle
   Schnittstelle zur Verbindung von Fernschreibern (englisch Teletypewrite).
   Aus der englischen Bezeichnung leitet sich ihr Name ab.

   Die serielle Schnittstelle wird von Terminaltreibern genutzt, die einerseits
   mit einem Prozess verbunden, etwa einer Loginshell, und auf der anderen Seite
   mit einer seriellen Schnittstelle bzw. mit einer virtuellen Systemkonsole kommunizieren.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: C-kern/api/io/terminal/sercom.h
    Header file <Async-Serial-Communication>.

   file: C-kern/platform/Linux/io/sercom.c
    Implementation file <Async-Serial-Communication impl>.
*/
#ifndef CKERN_IO_TERMINAL_SERCOM_HEADER
#define CKERN_IO_TERMINAL_SERCOM_HEADER

// === exported types
struct sercom_t;
struct sercom_config_t;
struct sercom_oldconfig_t;

/* enums: sercom_config_e
 *
 * sercom_config_NOPARITY   - Übertragung benutzt kein Parity Bit am Ende der Daten.
 * sercom_config_ODDPARITY  - Übertragung überträgt ein Parity Bit am Ende der Daten,
 *                            wobei die Anzahl übertragener Einsen inklusive Paritätsbit ungerade ist.
 * sercom_config_EVENPARITY - Übertragung überträgt ein Parity Bit am Ende der Daten,
 *                            wobei die Anzahl übertragener Einsen inklusive Paritätsbit gerade ist.
 * sercom_config_50BPS      - Es werden 50 Bits/pro Sekunde übertragen
 * sercom_config_75BPS      - Es werden 75 Bits/pro Sekunde übertragen
 * sercom_config_110BPS     - Es werden 110 Bits/pro Sekunde übertragen
 * sercom_config_134BPS     - Es werden 134 Bits/pro Sekunde übertragen
 * sercom_config_150BPS     - Es werden 150 Bits/pro Sekunde übertragen
 * sercom_config_200BPS     - Es werden 200 Bits/pro Sekunde übertragen
 * sercom_config_300BPS     - Es werden 300 Bits/pro Sekunde übertragen
 * sercom_config_600BPS     - Es werden 600 Bits/pro Sekunde übertragen
 * sercom_config_1200BPS    - Es werden 1200 Bits/pro Sekunde übertragen
 * sercom_config_1800BPS    - Es werden 1800 Bits/pro Sekunde übertragen
 * sercom_config_2400BPS    - Es werden 2400 Bits/pro Sekunde übertragen
 * sercom_config_4800BPS    - Es werden 4800 Bits/pro Sekunde übertragen
 * sercom_config_9600BPS    - Es werden 9600 Bits/pro Sekunde übertragen
 * sercom_config_19200BPS   - Es werden 19200 Bits/pro Sekunde übertragen
 * sercom_config_38400BPS   - Es werden 38400 Bits/pro Sekunde übertragen
 * sercom_config_57600BPS   - Es werden 57600 Bits/pro Sekunde übertragen
 * sercom_config_115200BPS  - Es werden 115200 Bits/pro Sekunde übertragen
 * sercom_config_230400BPS  - Es werden 230400 Bits/pro Sekunde übertragen
 * sercom_config_460800BPS  - Es werden 460800 Bits/pro Sekunde übertragen
 * sercom_config_500000BPS  - Es werden 500000 Bits/pro Sekunde übertragen
 * sercom_config_576000BPS  - Es werden 576000 Bits/pro Sekunde übertragen
 * sercom_config_921600BPS  - Es werden 921600 Bits/pro Sekunde übertragen
 * sercom_config_1000000BPS - Es werden 1000000 Bits/pro Sekunde übertragen
 * sercom_config_1152000BPS - Es werden 1152000 Bits/pro Sekunde übertragen
 * sercom_config_1500000BPS - Es werden 1500000 Bits/pro Sekunde übertragen
 * sercom_config_2000000BPS - Es werden 2000000 Bits/pro Sekunde übertragen
 * sercom_config_2500000BPS - Es werden 2500000 Bits/pro Sekunde übertragen
 * sercom_config_3000000BPS - Es werden 3000000 Bits/pro Sekunde übertragen
 * sercom_config_3500000BPS - Es werden 3500000 Bits/pro Sekunde übertragen
 * sercom_config_4000000BPS - Es werden 4000000 Bits/pro Sekunde übertragen
 * */
typedef enum sercom_config_e {
   sercom_config_NOPARITY   = 0,
   sercom_config_ODDPARITY  = 1,
   sercom_config_EVENPARITY = 2,
   sercom_config_50BPS = 0,
   sercom_config_75BPS,
   sercom_config_110BPS,
   sercom_config_134BPS,
   sercom_config_150BPS,
   sercom_config_200BPS,
   sercom_config_300BPS,
   sercom_config_600BPS,
   sercom_config_1200BPS,
   sercom_config_1800BPS,
   sercom_config_2400BPS,
   sercom_config_4800BPS,
   sercom_config_9600BPS,
   sercom_config_19200BPS,
   sercom_config_38400BPS,
   sercom_config_57600BPS,
   sercom_config_115200BPS,
   sercom_config_230400BPS,
   sercom_config_460800BPS,
   sercom_config_500000BPS,
   sercom_config_576000BPS,
   sercom_config_921600BPS,
   sercom_config_1000000BPS,
   sercom_config_1152000BPS,
   sercom_config_1500000BPS,
   sercom_config_2000000BPS,
   sercom_config_2500000BPS,
   sercom_config_3000000BPS,
   sercom_config_3500000BPS,
   sercom_config_4000000BPS
} sercom_config_e;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_terminal_sercom
 * Test <sercom_t> functionality. */
int unittest_io_terminal_sercom(void);
#endif


/* struct: sercom_config_t
 * Speichert die vorherige Konfiguration der seriellen Schnittstelle.
 * Mit <sercom_t.restore_sercom> kann diese wiederhergestelt werden. */
typedef struct sercom_oldconfig_t {
   unsigned sysold[6];
} sercom_oldconfig_t;


/* struct: sercom_config_t
 * Definiert die Übertragungscharakteristika der seriellen Schnittstelle.
 * Zuerst werden die Anzahl der Datenbits zwischen 5 und 8 Bits pro übertragenem
 * Datenwort festgelegt. Danach das optionale Paritätsbit
 * (siehe <sercom_config_NOPARITY>, <sercom_config_ODDPARITY> und <sercom_config_EVENPARITY>).
 * Danach werden ein oder zwei Stopbits übertragen.
 * Die (asynchrone, d.h. ohne extra Taktleitung) Übertragungsgeschwindigkeit wird
 * mit einer der Konstanten aus <sercom_config_e> festgelegt.
 *
 * Parity-Bit:
 * Ist die Summe der gesendeten Einsen eines Informationsworts gerade,
 * dann ist der Wert des Paritätsbits bei Even-Parity 0 und bei Odd-Parity
 * ist diesert Wert 1.
 * Ist die Summe der gesendeten Einsen eines Informationsworts ungerade,
 * dann ist der Wert des Paritätsbits bei Even-Parity 1 und bei Odd-Parity
 * ist diesert Wert 0.
 *
 * Werte auf Leitung:
 *
 * >            ________         _______                 _______________          ______
 * > logisch 1:   Ruhe  | Start | Bit 0 | Bit 1 | Bit 2 | Bit 3 | Bit 4 | Parity | Stop |
 * > logisch 0:         |__Bit__|       |_______|_______|       |       |  Bit   |  Bit |
 * >                        0       1       0       0       1       1    E:1, O:0    1
 *
 * */
typedef struct sercom_config_t {
   uint8_t nrdatabits;  // values [5..8] supported
   uint8_t parity;      // values [0..2] supported (sercom_config_NOPARITY,sercom_config_ODDPARITY,sercom_config_EVENPARITY)
   uint8_t nrstopbits;  // values [1..2] supported
   uint8_t speed;       // values sercom_config_50BPS ... sercom_config_4000000BPS supported
} sercom_config_t;


/* struct: sercom_t
 * Erlaubt den Zugriff auf eine serielle Schnittstelle.
 *
 * Beim öffnen der Schnittstlle mit <init_sercom> kann die Konfiguration
 * mit einem Parameter vom Typ <sercom_config_t> eingestellt werden.
 *
 * Später kann mit <reconfig_sercom> die Konfiguration geändert werden.
 * Die aktuelle Konfiguration kann mit <getconfig_sercom> gelesen werden.
 *
 * Vor dem Schließen der Schnittstelle ist es Usus die vorherige Konfiguration
 * wieder mit <restore_sercom> herzustellen. Der dazu notwendige Parameter
 * vom Typ <sercom_oldconfig_t> wird bei Aufruf von <init_sercom> mit den vorherigen
 * Initialisierungswerten gefüllt.
 * */
typedef struct sercom_t {
   sys_iochannel_t sysio;
} sercom_t;

// group: lifetime

/* define: sercom_FREE
 * Static initializer. */
#define sercom_FREE \
         { sys_iochannel_FREE }

/* function: init_sercom
 * Öffnet die serielle Schnittstelle abgelegt unter dem Pfad devicepath.
 * Die alte Konfiguration wird in oldconfig zurückgeliefert und der Parameter
 * config beschreibt die neue Konfiguration. Beide Parameter können auf 0 gesetzt werdem. */
int init_sercom(/*out*/sercom_t *comport, /*out*/sercom_oldconfig_t *oldconfig/*0==>not returned*/, const char *devicepath, const struct sercom_config_t *config/*0==unchanged*/);

/* function: free_sercom
 * Schließt Dateideskriptor zur seriellen Schnittstelle. */
int free_sercom(sercom_t *comport);

// group: query

/* function: getconfig_sercom
 * Liefert die vormals gesetzte Übertragungsrate usw. zurück.
 * Falls <init_sercom> mit config==0 aufgerufen wurde,
 * wird die zuletzt von einem anderen Prozess gesetzte bzw. Defaultkonfiguration
 * zurückgeliefert. */
int getconfig_sercom(const sercom_t *comport, /*out*/struct sercom_config_t *config);

// group: update

/* function: reconfig_sercom
 * Ändert die Übertragungskonfiguration. */
int reconfig_sercom(sercom_t *comport, const struct sercom_config_t *config);

/* function: restore_sercom
 * Stellt beim Öffnen der Schnittstelle existente Übertragungskonfiguration wieder her.
 * Der Parameter oldconfig wurde von <init_sercom> zurückgeliefert,
 * wenn er nicht gleich 0 gesetzt war. */
int restore_sercom(sercom_t *comport, const struct sercom_oldconfig_t *oldconfig);


// section: inline implementation


#endif
