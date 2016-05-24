/* title: PseudoTerminal

   Erlaubt Erzeugen eines Pseudoterminals.

   Das Pseudoterminal wird wie eine Pipe benutzt.

   Es besteht aus zwei Endgeräten, einem Master und einem Slave.
   Das "Slave"-Ende wird mit einem terminalorientierten
   Programm verbunden, das "Master"-Ende ist mittels Pipe mit
   dem Slave verbunden und kann alle Ein- und Ausgaben
   mitschneiden, weiterleiten oder auch nur simulieren.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/io/terminal/pseudoterm.h
    Header file <PseudoTerminal>.

   file: C-kern/platform/Linux/io/pseudoterm.c
    Implementation file <PseudoTerminal impl>.
*/
#ifndef CKERN_IO_TERMINAL_PSEUDOTERM_HEADER
#define CKERN_IO_TERMINAL_PSEUDOTERM_HEADER

/* typedef: struct pseudoterm_t
 * Export <pseudoterm_t> into global namespace. */
typedef struct pseudoterm_t pseudoterm_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_terminal_pseudoterm
 * Test <pseudoterm_t> functionality. */
int unittest_io_terminal_pseudoterm(void);
#endif


/* struct: pseudoterm_t
 * Dieser Typ erlaubt das Erzeugen eines Pseudoterminals.
 * Beim Initialisieren (<init_pseudoterm>) wird ein freies
 * Master-Device geöffnet und mit einem Slave-Device verbunden.
 *
 * Das Slave-Device implementiert die Schnittstelle eines Terminals,
 * die mit <terminal_t> konfiguriert werden kann.
 *
 * Der Name des Slave-Pseudoterminals kann mit <pathname_pseudoterm>
 * erfragt werden.
 *
 * Jetzt kann das Programm mittels fork() einen Kindprozess von sich
 * erzeugen, der eine neue Sitzung öffnet und damit die Verbindung
 * zu seinem bisherigen Controlling-Terminal verliert. Danach öffnet der Kindprozess
 * das Slave-Pseudoterminal, kopiert den Filedescriptor nach std-in, std-out und std-err
 * und hat somit ein Pseudoterminal als neues Controlling Terminal eingerichtet.
 * Die vom Kinprozess aufzurufende Funktion <switchcontrolling_terminal> erledigt dies.
 *
 * Das eigene Programm kann dann mittels des durch <io_pseudoterm> ermittelten
 * <sys_iochannel_t> alle Ausgaben des Kindprozesses lesen und simulierte
 * Tastatureingaben schreiben – oder die gesamte Terminal-E/A über ein Netzwerk
 * weiterleiten.
 *
 * Die Konfiguration des Terminals und die Größe des Fensters werden vom Master und Slave
 * geteilt, beide können sie lesen und änderen.
 *
 * Architektur:
 *
 * Der Typ <pseudoterm_t> zeigt auf das Masterdevice und
 * mittels <pathname_pseudoterm> kann der Pfad zum Slavedevice
 * ermittelt werden.
 *
 * Der Slavedevice Pfad existiert aber nur so lange, wie das Master-Terminal
 * – also dieses <pseudoterm_t> Objekt – nicht geschlossen wird.
 *
 * >
 * > ╭──────────╮  fork()       ╭──────────╮
 * > │ Dieses   │  -----------> │ Terminal │
 * > │ Programm │  ( + exec())  │ Programm │
 * > ╰──────────╯               ╰──────────╯
 * >     | ▴               std- in ▴ | -out
 * >     | |   (user space)        | | -err
 * > ----|-|-----------------------|-|------------
 * >     | |   (kernel space)      | |
 * >     ▾ |                       | ▾
 * > ┌───────────────┐        ┌──────────────┐
 * > │ Master-Device │ <-  -> │ Slave-Device │
 * > └─────────────┬─┘        └┬─────────────┘
 * >             ┌─┴───────────┴─┐
 * >             │ Shared Config │
 * >             └───────────────┘
 *
 * */
struct pseudoterm_t {
   sys_iochannel_t master_device;
};

// group: lifetime

/* define: pseudoterm_FREE
 * Static initializer. */
#define pseudoterm_FREE \
         { sys_iochannel_FREE }

/* function: init_pseudoterm
 * Legt ein neues Master-Slave Pseudoterminalpaar an.
 * Nach Erfolg zeigt pty (siehe <io_pseudoterm>) auf das Mastergerät.
 * Der Pfadname für das Slavegerät kann mit <pathname_pseudoterm> erfragt
 * und dann mit dem Typ <terminal_t> geöffnet werden.
 * Ein neu erzeugter Kindprozess sollte <switchcontrolling_terminal> aufrufen,
 * um das Slave-Pseudoterminal zu seinem neuen Controlling Terminal zu machen. */
int init_pseudoterm(/*out*/pseudoterm_t* pty);

/* function: free_pseudoterm
 * Schliesst das Master-Pseudoterminal.
 * Das Slave-Terminal (<pathname_pseudoterm>) wird aber erst ungültig,
 * falls das letzte <pseudoterm_t> geschlossen wird.
 * D.h. falls der Kindprozess pty schliesst, der Elternprozess es aber
 * weiterhin geöffnet hält, ist das Slave-Pseudoterminal weiterhin gültig. */
int free_pseudoterm(pseudoterm_t* pty);

// group: query

/* function: io_pseudoterm
 * Gibt den IO-Kanal (Dateideskriptor) des Masterpseudoterminals zurück.
 * Falls das Slavepseudoterminal noch nicht geöffnet wurde, gibt es keine
 * Fehlermeldung (unter Linux). Lesen gibt EAGAIN zurück und geschriebene Daten werden
 * in einem internen Speicher (4K) abgelegt. Wird das Slaveterminal nach dem Öffnen
 * wieder geschlossen, gibt poll von <io_pseudoterm> POLLHUP zurück und
 * Lesen von <io_pseudoterm> ergibt die Fehlermeldung EIO. */
sys_iochannel_t io_pseudoterm(const pseudoterm_t* pty);

/* function: pathname_pseudoterm
 * Gibt den \0 terminierten Pfadnamen des Slavepseudoterminals in name zurück.
 * Das Öffnen dieses Pfads mit dem Objekttyp <terminal_t> erlaubt das Setzen
 * eines neuen Controlling Terminals, das mit dem Masterterminal pty verbunden
 * ist.
 *
 * Return:
 * 0       - OK. name enthält den \0 terminierten Pfad und falls namesize != 0, dann
 *           wurde es auf die Anzahl Bytes des Pfadstrings *mit* \0 Byte gesetzt.
 * ENOBUFS - Der Pfad ist länger als len Bytes. Falls namesize != 0, dann wurde es
 *           die Anzahl Bytes des Pfadstrings *mit* \0 Byte gesetzt.
 * ENOTTY  - pty enthält einen ungültigen <sys_iochannel_t>. Weder name noch namesize wurden geändert. */
int pathname_pseudoterm(const pseudoterm_t* pty, size_t len, /*out*/uint8_t name[len], /*err;out*/size_t* namesize);



// section: inline implementation

/* define: io_pseudoterm
 * Implements <pseudoterm_t.io_pseudoterm>. */
#define io_pseudoterm(pty) \
         ((pty)->master_device)

#endif
