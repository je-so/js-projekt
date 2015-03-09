/* title: IOBuffer

   Ausbaufähiges Modul, es soll dereinst die Verwaltung aller
   I/O Buffer übernehmen.

   Momentan unterstützt es nur das Lesen einer Datei von Anfang bis Ende.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2015 Jörg Seebohn

   file: C-kern/api/io/subsys/iobuffer.h
    Header file <IOBuffer>.

   file: C-kern/io/subsys/iobuffer.c
    Implementation file <IOBuffer impl>.
*/
#ifndef CKERN_IO_SUBSYS_IOBUFFER_HEADER
#define CKERN_IO_SUBSYS_IOBUFFER_HEADER

#include "C-kern/api/io/subsys/iothread.h"
#include "C-kern/api/task/itc/itccounter.h"

// forward
struct directory_t;
struct memblock_t;


/* typedef: struct iobuffer_t
 * Export <iobuffer_t> into global namespace. */
typedef struct iobuffer_t iobuffer_t;

/* typedef: struct iobuffer_stream_t
 * Export <iobuffer_stream_t> into global namespace. */
typedef struct iobuffer_stream_t iobuffer_stream_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_subsys_iobuffer
 * Test <iobuffer_t> functionality. */
int unittest_io_subsys_iobuffer(void);
#endif


/* struct: iobuffer_t
 * Verwaltet eine große Ein-Ausgabe Speicherseite.
 * Die Speicherseite ist zum Lesen oder Schreiben von Daten
 * in einen (geräteabhängigen) I/O Kanal geeignet . */
struct iobuffer_t {
   // group: private Felder
   /* variable: addr
    * Startadresse des I/O Buffers. */
   uint8_t* addr;
   /* variable: size
    * Größe des I/O Buffers in Bytes. */
   size_t   size;
};

// group: lifetime

/* define: iobuffer_FREE
 * Static initializer. */
#define iobuffer_FREE \
         { 0, 0 }

/* function: init_iobuffer
 * Allokiert I/O buffer von 1 MB.
 *
 * TODO: 1. support size (4k,1MB,20MB)
 *       2. support I/O devices
 *          (on Linux all(?) page aligned memory could be used for direct I/O)
 */
int init_iobuffer(/*out*/iobuffer_t* iobuf);

/* function: free_iobuffer
 * Gibt allokierten Speicher frei. */
int free_iobuffer(iobuffer_t* iobuf);

// group: query

/* function: addr_iobuffer
 * Liefert Startaddresse des I/O Buffers. */
uint8_t* addr_iobuffer(const iobuffer_t* iobuf);

/* function: size_iobuffer
 * Liefert Größe des I/O Buffers in Bytes. */
size_t size_iobuffer(const iobuffer_t* iobuf);



/* struct: iobuffer_stream_t
 * Verwaltet mehrere <iobuffer_t>, die zum Linearlesen einer Datei gedacht sind. */
struct iobuffer_stream_t {
   // group: Private Felder
   /* variable: iothread
    * Thread für das Hintergrundlesen.
    * TODO: move iothread into process I/O subsystem */
   iothread_t      iothread;
   /* variable: ready
    * Signalisiert, daß ein weiterer <iotask> fertig bearbeitet ist. */
   itccounter_t    ready;
   /* variable: buffer
    * 3 I/O Buffer zum Halten der gelesenen Daten. */
   iobuffer_t      buffer[3];
   /* variable: iotask
    * 3 <iotask_t> zum individuellen Ansprechen jedes <buffer>s. */
   iotask_t        iotask[3];
   /* variable: ioc
    * I/O Kanal, von dem gelesen wird. */
   sys_iochannel_t ioc;
   /* variable: nextbuffer
    * Index in <iotask>, welcher als nächstes von <readnext_iobufferstream> zurückgeliefert wird.
    * Der <iotask> mit dem vorherigen Indexwert nextbuffer-1, wobei -1 dem Wert lengthof(buffer)-1
    * entspricht, ist unbenutzt oder wurde bei einem vorherigen Aufruf von <readnext_iobufferstream>
    * zurückgegeben und der Speicherbereich wird daher noch vom Benutzer verwendet. */
   uint8_t         nextbuffer;
   /* variable: filesize
    * Die Gesamtlänge in Bytes der zu lesenden Daten. */
   off_t           filesize;
   /* variable: readpos
    * Die Position in der Datei <ioc>, ab der mit dem nächsten I/O gelesen werden soll.
    * Dieser Wert wird jedesmal um buffer[i].size oder weniger erhöht, wenn dem <iothread> ein neuer
    * iotask[i] übergeben. Dieser Wert wird bis zum Erreichen des Wertes <filesize> erhöht. */
   off_t           readpos;
};

// group: lifetime

/* define: iobuffer_stream_FREE
 * Static initializer. */
#define iobuffer_stream_FREE \
         {  iothread_FREE, itccounter_FREE, { iobuffer_FREE, iobuffer_FREE, iobuffer_FREE }, \
            { iotask_FREE, iotask_FREE, iotask_FREE }, sys_iochannel_FREE, 0, 0, 0 }

/* function: init_iobufferstream
 * Öffnet eine Datei zum Lesen und allokiert mehrere I/O buffer. */
int init_iobufferstream(/*out*/iobuffer_stream_t* iostream, const char* path, struct directory_t* relative_to/*could be 0*/);

/* function: free_iobufferstream
 * Stoppt das Hintergrundlesen der Datei und gibt alle Ressourcen frei. */
int free_iobufferstream(iobuffer_stream_t* iostream);

// group: I of I/O

/* function: readnext_iobufferstream
 * Gibt den nächsten fertig gelesenen Datenblock in nextbuffer zurück und wartet nötigenfalls.
 * Der durch den vorherigen Aufruf zurückgegebene nextbuffer geht wieder in das Eigentum von iostream
 * über und ein neuer Datenblock der Datei wird darin eingelesen, falls das Dateiende nicht erreicht ist.
 *
 * Returns:
 * 0       - nextbuffer ist gültig und enthält Daten.
 * ENODATA - Es gibt keine weiteren Daten mehr, alle Daten wurden schon gelesen.
 * EIO     - I/O Fehler.
 *
 * Restriktionen:
 * nextbuffer - Nur gültig bis zu nächstem Aufruf von <readnext_iobufferstream> (, <free_iobufferstream>). */
int readnext_iobufferstream(iobuffer_stream_t* iostream, /*out*/struct memblock_t* nextbuffer);



// section: inline implementation

// group: iobuffer_t

/* define: addr_iobuffer
 * Implements <iobuffer_t.addr_iobuffer>. */
#define addr_iobuffer(iobuf) \
         ((iobuf)->addr)

/* define: size_iobuffer
 * Implements <iobuffer_t.size_iobuffer>. */
#define size_iobuffer(iobuf) \
         ((iobuf)->size)

#endif
