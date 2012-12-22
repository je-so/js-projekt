/* title: InputStream

   Offers interface to stream data from files (or network sockets)
   to reader components. Readers could be parsers or image loaders or other.

   about: Copyright
   This program is free software.
   You can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   Author:
   (C) 2012 Jörg Seebohn

   file: C-kern/api/io/instream.h
    Header file <InputStream>.

   file: C-kern/io/instream.c
    Implementation file <InputStream impl>.
*/
#ifndef CKERN_IO_INSTREAM_HEADER
#define CKERN_IO_INSTREAM_HEADER

// forward
struct memblock_t ;

/* typedef: struct instream_it
 * Export interface <instream_it> into global namespace. */
typedef struct instream_it             instream_it ;

/* typedef: struct instream_t
 * Export abstract object <instream_t> into global namespace. */
typedef struct instream_t              instream_t ;

/* typedef: instream_impl_t
 * Define abstract <instream_impl_t> implementing <instream_t>. */
typedef struct instream_impl_t         instream_impl_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_instream
 * Test <instream_t> functionality. */
int unittest_io_instream(void) ;
#endif


/* struct: instream_it
 * Defines interface a type must implement to be used by <instream_t>. */
struct instream_it {
   // group: variables
   /* variable: readnext
    * Pointer to function which reads the next datablock of an input stream.
    *
    * After return datablock points to a newly read data block. The returned
    * memory address may change from call to call or not - consider old addresses
    * to become invalid.
    *
    * If you want to keep data of a previously read data block set keepstart to
    * the start address of the data within datablock you want to keep
    * else set it to datablock->addr + datablock->size.
    * After return keepstart points to the beginning of the same data but adapted
    * to the memory frame of the newly returned datablock. In case keepstart was set
    * to old value of (datablock->addr + datablock->size) it is set to new value of
    * datablock->addr.
    *
    * After return datablock contains data of the next data block plus (in case keepstart != 0)
    * (<memblock_t.size> - (keepstart - <memblock_t.addr>) + alignment) bytes of the end of the
    * previous read data block. The kept data is aligned to some unknown value
    * which depends on the implementation.
    *
    * For the first call parameters datablock and keepstart must be set to
    * <memblock_INIT_FREEABLE> and 0 respectively. For all other calls datablock must contain
    * the unmodified value returned from a previous call and keepstart must always point within
    * address space of datablock.
    *
    * In case there is no more data the returncode is 0 and datablock is either set to <memblock_INIT_FREEABLE>
    * or contains the data beginning with (keepstart - alignment). */
   int (* readnext)  (struct instream_impl_t * isimpl, /*inout*/struct memblock_t * datablock, /*inout*/uint8_t ** keepstart) ;
} ;

// group: lifetime

#define instream_it_INIT_FREEABLE         instream_it_INIT(0)

/* define: instream_it_INIT
 * Static initializer.
 *
 * Parameters:
 * readnext     - Set <readnext> to the value of this parameter.
 * */
#define instream_it_INIT(readnext)        { readnext }

// group: generic

/* function: asgeneric_instreamit
 * Casts parameter iinstr into pointer to <instream_it>.
 * The parameter *iinstr* has to be of type "pointer to type" which was
 * declared with . */
instream_it * asgeneric_instreamit(void * iinstr, TYPENAME instream_impl_t) ;

/* function: typeadapt_DECLARE
 * Declares a derived interface from generic <instream_it>.
 * See <instream_it> for a list of functions.
 *
 * Parameter:
 * declared_it     - The name of the structure which is declared as the interface.
 * instream_impl_t - The type of the implementation object.
 *                   The first parameter of every function is a pointer to this type.
 */
void instream_it_DECLARE(TYPENAME declared_it, TYPENAME instream_impl_t) ;


/* struct: instream_t
 * Abstract object to read from data from a stream.
 * The stream implementation must export an interface of type <instream_it>
 * to be used by this object. */
struct instream_t {
   // group: variables
   /* variable: nextdata
    * Points to next unread data byte. Only valid if this value is not equal to <enddata>. */
   uint8_t        * nextdata ;
   /* variable: enddata
    * Points to end address of data block read from stream.
    * The end address points to the address after the last data byte.
    * <enddata> - <nextdata> gives the number of unread bytes.
    * <enddata> - <startdata> gives the size of the data block read from underlying stream. */
   uint8_t        * enddata ;
   /* variable: keepstart
    * Marks start of data to be kept in buffer.
    * A value of 0 means that no data should be kept in the buffer
    * if a new datablock is read with <instream_it.readnext>. */
   uint8_t        * keepstart ;
   /* variable: startdata
    * Points to start address of data block read from data stream. */
   uint8_t        * startdata ;
   /* variable: object
    * A pointer to the object which is manipulated by the interface <instream_it>. */
   instream_impl_t * object ;
   /* variable: iimpl
    * A pointer to an implementation of interface <instream_it>. */
   instream_it    * iimpl ;
   /* variable: readerror
    * Safes status of last call to <instream_it.readnext>.
    * In case <readerror> is not 0 no more call to <instream_it.readnext> is made. */
   int            readerror ;
} ;

// group: lifetime

/* define: instream_INIT_FREEABLE
 * Static initializer. */
#define instream_INIT_FREEABLE            instream_INIT(0, 0)

/* define: instream_INIT
 * Static initializer. */
#define instream_INIT(obj, iimpl)         { 0, 0, 0, 0, obj, iimpl, 0 }

/* function: init_instream
 * Initialize object. The object of type <instream_impl_t> and its interface
 * given in the two parameter must live at least as long as this object lives.
 * No copy is made only pointers are stored. */
int init_instream(/*out*/instream_t * instr, instream_impl_t * obj, instream_it * iimpl) ;

/* function: free_instream
 * Free resources but associated <instream_impl_t> is not freed.
 * Only the stored pointers are set to 0.
 * You have to free the object given in <init_instream> after calling this destructor. */
int free_instream(instream_t * instr) ;

// group: query

/* function: keepaddr_instream
 * Returns start address of data which must be kept in the buffer.
 * See <startkeep_instream>. A value of 0 is returned if no data
 * is kept in the buffer. See <endkeep_instream>. */
uint8_t * keepaddr_instream(const instream_t * instr) ;

/* function: readerror_instream
 * Returns the last read error of the input stream.
 * A value != 0 indicates an error. Once an error
 * has occurred the input stream is no more accessed. */
int readerror_instream(const instream_t * instr) ;

// group: read

/* function: readnext_instream
 * Returns the next read data byte.
 *
 * This function can fail if either an I/O error occurred or if
 * <startkeep_instream> was called (<keepaddr_instream> returns != 0)
 * and the buffer has to grow in size to keep all data but there is no
 * more memory available.
 *
 * The function returns ENODATA if there is no more data to read. */
int readnext_instream(instream_t * instr, /*out*/uint8_t * databyte) ;

// group: buffer

/* function: startkeep_instream
 * Keeps data in the read buffer starting from the last read byte.
 * If you call this function twice only data associated with the last call is remembered. */
void startkeep_instream(instream_t * instr) ;

/* function: endkeep_instream
 * Stops keeping data in the buffer. This allows the read buffer to shrink. */
void endkeep_instream(instream_t * instr) ;

/* function: readnextdatablock_instream
 * Do not call this function. It is called from <readnext_instream> or other reading functions.
 *
 * If <readerror_instream> returns a value != 0 this function does nothing else it
 * calls <instream_it.readnext> to read the next data block into the read buffer.
 * The read buffer is grown if <keepaddr_instream> returns a value != 0 or
 * if the read buffer contains unread data.
 *
 * If <instream_it.readnext> returns an error it is stored internally and also returnd.
 * In case of an error nothing is changed. The stored read error code can be queried with
 * a call to <readerror_instream>.
 *
 * Returns ENODATA if the read buffer contains no more data. */
int readnextdatablock_instream(instream_t * instr) ;


// section: inline implementation

/* define: asgeneric_instreamit
 * Implements <instream_it.asgeneric_instreamit>. */
#define asgeneric_instreamit(iinstr, instream_impl_t)                                           \
   ( __extension__ ({                                                                           \
      static_assert(                                                                            \
         offsetof(typeof(*(iinstr)), readnext) == offsetof(instream_it, readnext),              \
         "ensure same structure") ;                                                             \
      if (0) {                                                                                  \
         int _err = 0 ;                                                                         \
         _err += (iinstr)->readnext((instream_impl_t*)0, (struct memblock_t*)0, (uint8_t**)0) ; \
         (void) _err ;                                                                          \
      }                                                                                         \
      (instream_it*) (iinstr) ;                                                                 \
   }))

/* define: endkeep_instream
 * Implements <instream_t.endkeep_instream>. */
#define endkeep_instream(instr)           do { (instr)->keepstart = 0 ; } while (0)

/* define: free_instream
 * Implements <instream_t.free_instream>. */
#define free_instream(instr)              (*(instr) = (instream_t) instream_INIT_FREEABLE, 0)

/* define: init_instream
 * Implements <instream_t.init_instream>. */
#define init_instream(instr, obj, iimpl)  (*(instr) = (instream_t) instream_INIT(obj, iimpl), 0)

/* define: keepaddr_instream
 * Implements <instream_t.keepaddr_instream>. */
#define keepaddr_instream(instr)          ((instr)->keepstart)

/* define: readerror_instream
 * Implements <instream_t.readerror_instream>. */
#define readerror_instream(instr)         ((instr)->readerror)

/* define: readnext_instream
 * Implements <instream_t.readnext_instream>. */
#define readnext_instream(instr, databyte)                     \
   ( __extension__ ({                                          \
      int _err ;                                               \
      typeof(instr) _istr = (instr) ;                          \
      if (  _istr->nextdata != _istr->enddata                  \
            || !(_err = readnextdatablock_instream(_istr))) {  \
         *(databyte) = * (_istr->nextdata ++) ;                \
         _err = 0 ;                                            \
      }                                                        \
      _err ;                                                   \
   }))

/* define: startkeep_instream
 * Implements <instream_t.startkeep_instream>. */
#define startkeep_instream(instr)         do { (instr)->keepstart = (instr)->nextdata - 1 ; } while (0)

/* define: instream_it_DECLARE
 * Implements <instream_it.instream_it_DECLARE>. */
#define instream_it_DECLARE(declared_it, instream_impl_t)         \
   typedef struct declared_it          declared_it ;              \
   struct declared_it {                                           \
      int (* readnext)  (instream_impl_t * isimpl, /*inout*/struct memblock_t * datablock, /*inout*/uint8_t ** keepstart) ;   \
   }


#endif
