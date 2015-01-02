/* title: IOPipe

   TODO: describe module interface

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/io/pipe.h
    Header file <IOPipe>.

   file: C-kern/platform/Linux/io/pipe.c
    Implementation file <IOPipe impl>.
*/
#ifndef CKERN_IO_PIPE_HEADER
#define CKERN_IO_PIPE_HEADER

/* typedef: struct pipe_t
 * Export <pipe_t> into global namespace. */
typedef struct pipe_t pipe_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_pipe
 * Test <pipe_t> functionality. */
int unittest_io_pipe(void);
#endif


/* struct: pipe_t
 * Unidirektionaler Kommunikationskanal.
 * Erlaubt das Lesen der über das Kommunikationsende <write> geschriebenen
 * Daten an dem Kommunikationsende <read>. */
struct pipe_t {
   // group: Datenfelder
   /* variable: read
    * Ein lesender Zugriff erlaubender <sys_iochannel_t>.
    * <read> ist direkt verbunden mit <write>. */
   sys_iochannel_t read;
   /* variable: write
    * Ein schreibender Zugriff erlaubender <sys_iochannel_t>.
    * <write> ist direkt verbunden mit <read>. */
   sys_iochannel_t write;
};

// group: lifetime

/* define: pipe_FREE
 * Statische Zuweisung um pipe_t als uninitialisiert zu markieren. */
#define pipe_FREE \
         { sys_iochannel_FREE, sys_iochannel_FREE }

/* function: init_pipe
 * Initialisert pipe als nicht blockierender unidirektionaler Kommunikationskanal. */
int init_pipe(/*out*/pipe_t* pipe);

/* function: free_pipe
 * Gibt die Ressourcen (Dateideskriptoren) wieder frei.
 * Der Aufruf von <free_pipe> funktioniert auch dann, wenn zuvor
 * ein Kommunikationsende – <read> oder <write> – durch <free_iochannel>
 * geschlossen wurde. */
int free_pipe(pipe_t* pipe);

// group: read-write

/* function: readall_pipe
 * Wartet maximal msec_timeout Millisekunden, bis alle size Datenbytes gelesen werden konnten.
 * Im Fehlerfall werden schon gelesene Daten verworfen und
 * stattdessen der Fehlercode zurückegegeben. Der Inhalt von data wird auch im Fehlerfall
 * möglicherweise verändert. */
int readall_pipe(pipe_t* pipe, size_t size, void* data/*uint8_t[size]*/, int32_t msec_timeout/*<0: infinite timeout*/);

/* function: writeall_pipe
 * Wartet maximal msec_timeout Millisekunden, bis alle size Datenbytes geschrieben werden konnten.
 * Im Fehlerfall wird trotz teilweise geschriebener Daten der Fehlercode zurückegegeben, die Länge
 * der schon geschr. Daten wird verworfen. Teilweise geschriebene Daten verbleiben im internen Pipepuffer. */
int writeall_pipe(pipe_t* pipe, size_t size, void* data/*uint8_t[size]*/, int32_t msec_timeout/*<0: infinite timeout*/);

// group: generic-cast

/* function: cast_pipe
 * TODO: */
pipe_t* cast_pipe(sys_iochannel_t* read, sys_iochannel_t* write);



// section: inline implementation

/* define: cast_pipe
 * Implements <pipe_t.cast_pipe>. */
#define cast_pipe(_read, _write) \
         ( __extension__ ({                    \
            sys_iochannel_t* _r = (_read);     \
            sys_iochannel_t* _w = (_write);    \
            static_assert(                     \
               _r == &((pipe_t*)_r)->read      \
               && _w == &((pipe_t*)_r)->write, \
               "compatible struct");           \
            (pipe_t*) _r;                      \
         }))



#endif
