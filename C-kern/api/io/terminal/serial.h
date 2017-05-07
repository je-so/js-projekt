/* title: Async-Serial-CommPort

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

   file: C-kern/api/io/terminal/serial.h
    Header file <Async-Serial-CommPort>.

   file: C-kern/platform/Linux/io/serial.c
    Implementation file <Async-Serial-CommPort impl>.
*/
#ifndef CKERN_IO_TERMINAL_SERCOM_HEADER
#define CKERN_IO_TERMINAL_SERCOM_HEADER

// === exported types
struct serial_t;
struct serial_config_t;
struct serial_oldconfig_t;

/* enums: serial_config_e
 *
 * serial_config_NOPARITY   - Übertragung benutzt kein Parity Bit am Ende der Daten.
 * serial_config_ODDPARITY  - Übertragung überträgt ein Parity Bit am Ende der Daten,
 *                            wobei die Anzahl übertragener Einsen inklusive Paritätsbit ungerade ist.
 * serial_config_EVENPARITY - Übertragung überträgt ein Parity Bit am Ende der Daten,
 *                            wobei die Anzahl übertragener Einsen inklusive Paritätsbit gerade ist.
 * serial_config_50BPS      - Es werden 50 Bits/pro Sekunde übertragen
 * serial_config_75BPS      - Es werden 75 Bits/pro Sekunde übertragen
 * serial_config_110BPS     - Es werden 110 Bits/pro Sekunde übertragen
 * serial_config_134BPS     - Es werden 134 Bits/pro Sekunde übertragen
 * serial_config_150BPS     - Es werden 150 Bits/pro Sekunde übertragen
 * serial_config_200BPS     - Es werden 200 Bits/pro Sekunde übertragen
 * serial_config_300BPS     - Es werden 300 Bits/pro Sekunde übertragen
 * serial_config_600BPS     - Es werden 600 Bits/pro Sekunde übertragen
 * serial_config_1200BPS    - Es werden 1200 Bits/pro Sekunde übertragen
 * serial_config_1800BPS    - Es werden 1800 Bits/pro Sekunde übertragen
 * serial_config_2400BPS    - Es werden 2400 Bits/pro Sekunde übertragen
 * serial_config_4800BPS    - Es werden 4800 Bits/pro Sekunde übertragen
 * serial_config_9600BPS    - Es werden 9600 Bits/pro Sekunde übertragen
 * serial_config_19200BPS   - Es werden 19200 Bits/pro Sekunde übertragen
 * serial_config_38400BPS   - Es werden 38400 Bits/pro Sekunde übertragen
 * serial_config_57600BPS   - Es werden 57600 Bits/pro Sekunde übertragen
 * serial_config_115200BPS  - Es werden 115200 Bits/pro Sekunde übertragen
 * serial_config_230400BPS  - Es werden 230400 Bits/pro Sekunde übertragen
 * serial_config_460800BPS  - Es werden 460800 Bits/pro Sekunde übertragen
 * serial_config_500000BPS  - Es werden 500000 Bits/pro Sekunde übertragen
 * serial_config_576000BPS  - Es werden 576000 Bits/pro Sekunde übertragen
 * serial_config_921600BPS  - Es werden 921600 Bits/pro Sekunde übertragen
 * serial_config_1000000BPS - Es werden 1000000 Bits/pro Sekunde übertragen
 * serial_config_1152000BPS - Es werden 1152000 Bits/pro Sekunde übertragen
 * serial_config_1500000BPS - Es werden 1500000 Bits/pro Sekunde übertragen
 * serial_config_2000000BPS - Es werden 2000000 Bits/pro Sekunde übertragen
 * serial_config_2500000BPS - Es werden 2500000 Bits/pro Sekunde übertragen
 * serial_config_3000000BPS - Es werden 3000000 Bits/pro Sekunde übertragen
 * serial_config_3500000BPS - Es werden 3500000 Bits/pro Sekunde übertragen
 * serial_config_4000000BPS - Es werden 4000000 Bits/pro Sekunde übertragen
 * */
typedef enum serial_config_e {
   serial_config_NOPARITY   = 0,
   serial_config_ODDPARITY  = 1,
   serial_config_EVENPARITY = 2,
   serial_config_50BPS = 0,
   serial_config_75BPS,
   serial_config_110BPS,
   serial_config_134BPS,
   serial_config_150BPS,
   serial_config_200BPS,
   serial_config_300BPS,
   serial_config_600BPS,
   serial_config_1200BPS,
   serial_config_1800BPS,
   serial_config_2400BPS,
   serial_config_4800BPS,
   serial_config_9600BPS,
   serial_config_19200BPS,
   serial_config_38400BPS,
   serial_config_57600BPS,
   serial_config_115200BPS,
   serial_config_230400BPS,
   serial_config_460800BPS,
   serial_config_500000BPS,
   serial_config_576000BPS,
   serial_config_921600BPS,
   serial_config_1000000BPS,
   serial_config_1152000BPS,
   serial_config_1500000BPS,
   serial_config_2000000BPS,
   serial_config_2500000BPS,
   serial_config_3000000BPS,
   serial_config_3500000BPS,
   serial_config_4000000BPS
} serial_config_e;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_terminal_serial
 * Test <serial_t> functionality. */
int unittest_io_terminal_serial(void);
#endif


/* struct: serial_config_t
 * Speichert die vorherige Konfiguration der seriellen Schnittstelle.
 * Mit <serial_t.restore_serial> kann diese wiederhergestelt werden. */
typedef struct serial_oldconfig_t {
   unsigned sysold[6];
} serial_oldconfig_t;


/* struct: serial_config_t
 * Definiert die Übertragungscharakteristika der seriellen Schnittstelle.
 * Zuerst werden die Anzahl der Datenbits zwischen 5 und 8 Bits pro übertragenem
 * Datenwort festgelegt. Danach das optionale Paritätsbit
 * (siehe <serial_config_NOPARITY>, <serial_config_ODDPARITY> und <serial_config_EVENPARITY>).
 * Danach werden ein oder zwei Stopbits übertragen.
 * Die (asynchrone, d.h. ohne extra Taktleitung) Übertragungsgeschwindigkeit wird
 * mit einer der Konstanten aus <serial_config_e> festgelegt.
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
typedef struct serial_config_t {
   uint8_t nrdatabits;  // values [5..8] supported
   uint8_t parity;      // values [0..2] supported (serial_config_NOPARITY,serial_config_ODDPARITY,serial_config_EVENPARITY)
   uint8_t nrstopbits;  // values [1..2] supported
   uint8_t speed;       // values serial_config_50BPS ... serial_config_4000000BPS supported
} serial_config_t;


/* struct: serial_t
 * Erlaubt den Zugriff auf eine serielle Schnittstelle.
 *
 * Beim öffnen der Schnittstlle mit <init_serial> kann die Konfiguration
 * mit einem Parameter vom Typ <serial_config_t> eingestellt werden.
 *
 * Später kann mit <reconfig_serial> die Konfiguration geändert werden.
 * Die aktuelle Konfiguration kann mit <getconfig_serial> gelesen werden.
 *
 * Vor dem Schließen der Schnittstelle ist es Usus die vorherige Konfiguration
 * wieder mit <restore_serial> herzustellen. Der dazu notwendige Parameter
 * vom Typ <serial_oldconfig_t> wird bei Aufruf von <init_serial> mit den vorherigen
 * Initialisierungswerten gefüllt.
 * */
typedef struct serial_t {
   sys_iochannel_t sysio;
} serial_t;

// group: lifetime

/* define: serial_FREE
 * Static initializer. */
#define serial_FREE \
         { sys_iochannel_FREE }

/* function: init_serial
 * Öffnet die serielle Schnittstelle abgelegt unter dem Pfad devicepath.
 * Die alte Konfiguration wird in oldconfig zurückgeliefert und der Parameter
 * config beschreibt die neue Konfiguration. Beide Parameter können auf 0 gesetzt werdem. */
int init_serial(/*out*/serial_t *comport, /*out*/serial_oldconfig_t *oldconfig/*0==>not returned*/, const char *devicepath, const struct serial_config_t *config/*0==unchanged*/);

/* function: free_serial
 * Schließt Dateideskriptor zur seriellen Schnittstelle. */
int free_serial(serial_t *comport);

// group: query

/* function: getconfig_serial
 * Liefert die vormals gesetzte Übertragungsrate usw. zurück.
 * Falls <init_serial> mit config==0 aufgerufen wurde,
 * wird die zuletzt von einem anderen Prozess gesetzte bzw. Defaultkonfiguration
 * zurückgeliefert. */
int getconfig_serial(const serial_t *comport, /*out*/struct serial_config_t *config);

// group: update

/* function: reconfig_serial
 * Ändert die Übertragungskonfiguration. */
int reconfig_serial(serial_t *comport, const struct serial_config_t *config);

/* function: restore_serial
 * Stellt beim Öffnen der Schnittstelle existente Übertragungskonfiguration wieder her.
 * Der Parameter oldconfig wurde von <init_serial> zurückgeliefert,
 * wenn er nicht gleich 0 gesetzt war. */
int restore_serial(serial_t *comport, const struct serial_oldconfig_t *oldconfig);


// section: inline implementation


#endif
