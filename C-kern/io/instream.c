/* title: InputStream impl

   Implements <InputStream>.

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

#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/instream.h"
#include "C-kern/api/memory/memblock.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


// section: instream_t

/* function: readnextdatablock_instream
 * Calls <instream_t.readnext> to read the next data block.
 * If <instream_t.keepstart> is set or if not all data from the current
 * read buffer has been consumed then part of the read buffer is not overwritten.
 * The implementation of <instream_t.keepstart> is responsible for this. */
int readnextdatablock_instream(instream_t * instr)
{
   int err ;

   if (instr->readerror) {
      // no I/O after an error has occurred
      return instr->readerror ;
   }

   uint8_t ** keepstart  = instr->keepstart ? &instr->keepstart : &instr->nextdata ;
   // instr->keepstart == instr->nextdata - 1 and only nextdata is incremented ==> instr->nextdata > instr->keepstart
   // keepstart == &instr->keepstart ||  keepstart == &instr->nextdata ==> nextoffset > 0 || nextoffset == 0
   size_t     nextoffset = (size_t) (instr->nextdata - *keepstart) ;

   memblock_t datablock = memblock_INIT((size_t)(instr->enddata - instr->startdata), instr->startdata) ;
   err = instr->iimpl->readnext(instr->object, &datablock, keepstart) ;
   if (err) {
      instr->readerror = err ;
      goto ONABORT ;
   }

   instr->nextdata  = *keepstart + nextoffset ;
   instr->enddata   = datablock.addr + datablock.size ;
   instr->startdata = datablock.addr ;

   return datablock.size ? 0 : ENODATA ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

typedef struct instream_testimpl_t     instream_testimpl_t ;

struct instream_testimpl_t {
   /* variable: err
    * Error code return in <readnext_testimpl>. */
   int         err ;
   int         callcount ;
   bool        dataindex ;
   uint8_t     * data[2] ;
   size_t      datasize ;
   size_t      readoffset ;
   memblock_t  olddatablock ;
} ;

instream_it_DECLARE(instream_test_it, instream_testimpl_t) ;

static void init_instreamtestimpl(instream_testimpl_t * testimpl, size_t datasize, uint8_t * buffer1, uint8_t * buffer2)
{
   testimpl->err        = 0 ;
   testimpl->callcount  = 0 ;
   testimpl->dataindex  = 0 ;
   testimpl->data[0]    = buffer1 ;
   testimpl->data[1]    = buffer2 ;
   testimpl->datasize   = datasize ;
   testimpl->readoffset = 0 ;
   testimpl->olddatablock = (memblock_t) memblock_INIT_FREEABLE ;
}

/* function: readnext_testimpl
 * Test implementation of <instream_it.readnext>. */
static int readnext_testimpl(instream_testimpl_t * isimpl, /*inout*/memblock_t * datablock, /*inout*/uint8_t ** startkeep)
{
   int err ;

   ++ isimpl->callcount ;

   if (isimpl->err) return isimpl->err ;

   // test datablock is never changed
   if (datablock->addr != isimpl->olddatablock.addr || datablock->size != isimpl->olddatablock.size) {
      err = EINVAL ;
      goto ONABORT ;
   }

   // read next block
   isimpl->readoffset += datablock->size > 16 ? 16 : datablock->size ;
   if (isimpl->readoffset > isimpl->datasize) isimpl->readoffset = isimpl->datasize ;

   // simulate address change in next call
   isimpl->dataindex = !isimpl->dataindex ;

   uint8_t  * keepaddr = *startkeep ;
   size_t   keepsize   = (size_t) (datablock->size + datablock->addr - keepaddr) ;

   // simulate alignment 16 byte (which is pagesize_vm in a real implementation)
   size_t keepsize_aligned = (keepsize + 0x0F) & (~(size_t)0x0F) ;

   if (keepsize_aligned > datablock->size) {
      if (datablock->size == isimpl->datasize /*for test: all data in a single block*/) {
         keepsize_aligned = datablock->size ;
      } else {
         err = EINVAL ;
         goto ONABORT ;
      }
   }

   if (  isimpl->readoffset == isimpl->datasize
         && !keepsize_aligned) {
      // no more data
      *startkeep = 0 ;
      datablock->addr = 0 ;
      datablock->size = 0 ;
   } else {
      datablock->addr = isimpl->data[isimpl->dataindex] + isimpl->readoffset ;
      datablock->size = isimpl->datasize - isimpl->readoffset ;
      if (datablock->size > 16) datablock->size = 16 ;

      *startkeep = datablock->addr - keepsize ;
      datablock->addr -= keepsize_aligned ;
      datablock->size += keepsize_aligned ;
   }

   isimpl->olddatablock = *datablock ;

   return 0 ;
ONABORT:
   return err ;
}

static int readnext_dummy(instream_impl_t * isimpl, /*inout*/memblock_t * datablock, /*inout*/uint8_t ** startkeep)
{
   (void)isimpl ;
   (void)datablock ;
   (void)startkeep ;
   return 0 ;
}

static int test_interface(void)
{
   instream_it       iinstr = instream_it_INIT_FREEABLE ;
   instream_test_it  itest  = instream_it_INIT_FREEABLE ;

   // TEST instream_it_INIT_FREEABLE
   TEST(0 == iinstr.readnext) ;

   // TEST instream_it_INIT
   iinstr = (instream_it) instream_it_INIT(&readnext_dummy) ;
   TEST(iinstr.readnext == &readnext_dummy) ;

   // TEST instream_it_DECLARE
   TEST(0 == itest.readnext) ;
   itest = (instream_test_it) instream_it_INIT(&readnext_testimpl) ;
   TEST(itest.readnext == &readnext_testimpl) ;

   // TEST asgeneric_instreamit
   TEST((instream_it*)&itest == asgeneric_instreamit(&itest, instream_testimpl_t)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_initfree(void)
{
   instream_testimpl_t  testimpl = { .err = 0 } ;
   instream_test_it     iimpl = instream_it_INIT(&readnext_testimpl) ;
   instream_t           instr = instream_INIT_FREEABLE ;

   // TEST instream_INIT_FREEABLE
   TEST(0 == instr.nextdata) ;
   TEST(0 == instr.enddata) ;
   TEST(0 == instr.keepstart) ;
   TEST(0 == instr.startdata) ;
   TEST(0 == instr.object) ;
   TEST(0 == instr.iimpl) ;
   TEST(0 == instr.readerror) ;

   // TEST instream_INIT
   instr = (instream_t) instream_INIT((instream_impl_t*)1, (instream_it*)2) ;
   TEST(instr.object == (instream_impl_t*)1) ;
   TEST(instr.iimpl  == (instream_it*)2) ;
   TEST(0 == instr.nextdata) ;
   TEST(0 == instr.enddata) ;
   TEST(0 == instr.keepstart) ;
   TEST(0 == instr.startdata) ;
   TEST(0 == instr.readerror) ;

   // TEST init_instream, free_instream
   memset(&instr, -1, sizeof(instr)) ;
   TEST(0 == init_instream(&instr, (instream_impl_t*)&testimpl, asgeneric_instreamit(&iimpl, instream_testimpl_t))) ;
   TEST(instr.object == (instream_impl_t*)&testimpl) ;
   TEST(instr.iimpl  == (instream_it*)&iimpl) ;
   TEST(0 == instr.nextdata) ;
   TEST(0 == instr.enddata) ;
   TEST(0 == instr.keepstart) ;
   TEST(0 == instr.startdata) ;
   TEST(0 == instr.readerror) ;
   TEST(0 == free_instream(&instr)) ;
   TEST(0 == instr.nextdata) ;
   TEST(0 == instr.enddata) ;
   TEST(0 == instr.keepstart) ;
   TEST(0 == instr.startdata) ;
   TEST(0 == instr.object) ;
   TEST(0 == instr.iimpl) ;
   TEST(0 == instr.readerror) ;
   TEST(0 == free_instream(&instr)) ;
   TEST(0 == instr.nextdata) ;
   TEST(0 == instr.enddata) ;
   TEST(0 == instr.keepstart) ;
   TEST(0 == instr.startdata) ;
   TEST(0 == instr.object) ;
   TEST(0 == instr.iimpl) ;
   TEST(0 == instr.readerror) ;

   // TEST keepaddr_instream
   TEST(0 == keepaddr_instream(&instr)) ;
   instr.keepstart = (uint8_t*)1 ;
   TEST((uint8_t*)1 == keepaddr_instream(&instr)) ;
   instr.keepstart = (uint8_t*)2 ;
   TEST((uint8_t*)2 == keepaddr_instream(&instr)) ;

   // TEST startkeep_instream
   instr.nextdata = (uint8_t*)5 ;
   startkeep_instream(&instr) ;
   TEST(instr.keepstart == (uint8_t*)4) ;
   instr.nextdata = (void*)2 ;
   startkeep_instream(&instr) ;
   TEST(instr.keepstart == (uint8_t*)1) ;

   // TEST endkeep_instream
   TEST(instr.keepstart != 0) ;
   endkeep_instream(&instr) ;
   TEST(instr.keepstart == 0) ;

   // TEST readerror_instream
   instr.readerror = EPERM ;
   TEST(EPERM == readerror_instream(&instr)) ;
   instr.readerror = 0 ;
   TEST(0 == readerror_instream(&instr)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_readblock(void)
{
   instream_testimpl_t  testimpl ;
   instream_test_it     iimpl = instream_it_INIT(&readnext_testimpl) ;
   instream_t           instr = instream_INIT((instream_impl_t*)&testimpl, asgeneric_instreamit(&iimpl, instream_testimpl_t)) ;
   uint8_t              data[2][250] ;
   bool                 di ;

   // TEST readnextdatablock_instream: simulate reading byte by byte
   init_instreamtestimpl(&testimpl, sizeof(data[0]), data[0], data[1]) ;
   TEST(0 == instr.nextdata) ;
   TEST(0 == instr.enddata) ;
   TEST(0 == instr.startdata) ;
   di = 1 ;
   for (unsigned i = 0; i < sizeof(data[0]); i += 16, di = !di) {
      TEST(0 == readnextdatablock_instream(&instr)) ;
      const unsigned sz = (i + 16 <= sizeof(data[0])) ? 16 : sizeof(data[0]) - i ;
      TEST(&data[di][i]    == instr.nextdata) ;
      TEST(&data[di][i+sz] == instr.enddata) ;
      TEST(&data[di][i]    == instr.startdata) ;
      TEST(0               == instr.keepstart) ;
      instr.nextdata = instr.enddata ; // simulate reading
   }
   TEST(ENODATA == readnextdatablock_instream(&instr)) ;
   TEST(0 == instr.nextdata) ;
   TEST(0 == instr.enddata) ;
   TEST(0 == instr.startdata) ;

   // TEST readnextdatablock_instream: keeping [15..0] bytes unread data
   init_instreamtestimpl(&testimpl, sizeof(data[0]), data[0], data[1]) ;
   TEST(0 == init_instream(&instr, (instream_impl_t*)&testimpl, asgeneric_instreamit(&iimpl, instream_testimpl_t))) ;
   di = 1 ;
   for (unsigned i = 0; i < sizeof(data[0]); i += 16, di = !di) {
      TEST(0 == readnextdatablock_instream(&instr)) ;
      const unsigned is = i ? i - 16 : i ;
      const unsigned in = is + (i/16) ;
      const unsigned ie = i + ((i + 16 <= sizeof(data[0])) ? 16 : sizeof(data[0]) - i) ;
      TEST(&data[di][in] == instr.nextdata) ;
      TEST(&data[di][ie] == instr.enddata) ;
      TEST(&data[di][is] == instr.startdata) ;
      TEST(0             == instr.keepstart) ;
      instr.nextdata += 1 + (i ? 16 : 0) ; // simulate reading
   }
   TEST(ENODATA == readnextdatablock_instream(&instr)) ;
   TEST(0 == instr.nextdata) ;
   TEST(0 == instr.enddata) ;
   TEST(0 == instr.startdata) ;

   // TEST readnextdatablock_instream: keeping sizeof(data[0]) bytes unread data
   init_instreamtestimpl(&testimpl, sizeof(data[0]), data[0], data[1]) ;
   TEST(0 == init_instream(&instr, (instream_impl_t*)&testimpl, asgeneric_instreamit(&iimpl, instream_testimpl_t))) ;
   di = 1 ;
   for (unsigned i = 0; i < sizeof(data[0]); i += 16, di = !di) {
      TEST(0 == readnextdatablock_instream(&instr)) ;
      const unsigned ie = i + ((i + 16 <= sizeof(data[0])) ? 16 : sizeof(data[0]) - i) ;
      TEST(&data[di][0]  == instr.nextdata) ;
      TEST(&data[di][ie] == instr.enddata) ;
      TEST(&data[di][0]  == instr.startdata) ;
      TEST(0             == instr.keepstart) ;
      // no reading at all
   }
   TEST(0 == readnextdatablock_instream(&instr)) ;
   TEST(&data[di][0] == instr.nextdata) ;
   TEST(&data[di][sizeof(data[0])] == instr.enddata) ;
   TEST(&data[di][0] == instr.startdata) ;

   // TEST readnextdatablock_instream: startkeep will be adapted
   init_instreamtestimpl(&testimpl, sizeof(data[0]), data[0], data[1]) ;
   TEST(0 == init_instream(&instr, (instream_impl_t*)&testimpl, asgeneric_instreamit(&iimpl, instream_testimpl_t))) ;
   di = 1 ;
   for (unsigned i = 0; i < sizeof(data[0]); i += 16, di = !di) {
      TEST(0 == readnextdatablock_instream(&instr)) ;
      const unsigned ie = i + ((i + 16 <= sizeof(data[0])) ? 16 : sizeof(data[0]) - i) ;
      const unsigned is = i ? i - 16 : i ;
      uint8_t      * ks = i ? &data[di][is + (i/16)] : 0 ;
      TEST(&data[di][i ] == instr.nextdata) ;
      TEST(&data[di][ie] == instr.enddata) ;
      TEST(&data[di][is] == instr.startdata) ;
      TEST(ks            == instr.keepstart) ;
      instr.keepstart = &data[di][i] + 1 + (i/16) ;
      if (instr.keepstart > instr.enddata) instr.keepstart = instr.enddata ;
      instr.nextdata  = instr.enddata ; // simulate reading
   }
   TEST(ENODATA == readnextdatablock_instream(&instr)) ;
   TEST(0 == instr.nextdata) ;
   TEST(0 == instr.enddata) ;
   TEST(0 == instr.startdata) ;
   TEST(0 == instr.keepstart) ;

   // TEST readnextdatablock_instream: error prevents calling instream_it.readnext another time
   init_instreamtestimpl(&testimpl, sizeof(data[0]), data[0], data[1]) ;
   TEST(0 == init_instream(&instr, (instream_impl_t*)&testimpl, asgeneric_instreamit(&iimpl, instream_testimpl_t))) ;
   TEST(0 == readnextdatablock_instream(&instr)) ;
   di = 1 ;
   instr.keepstart = data[1] + 1 ;
   instr.nextdata  = data[1] + 2 ;
   TEST(0 == readnextdatablock_instream(&instr)) ;
   TEST(&data[0][2]  == instr.nextdata) ;
   TEST(&data[0][32] == instr.enddata) ;
   TEST(&data[0][0]  == instr.startdata) ;
   TEST(&data[0][1]  == instr.keepstart) ;
   TEST(0            == instr.readerror) ;
   testimpl.err = EIO ;
   testimpl.callcount = 0 ;
   TEST(EIO == readnextdatablock_instream(&instr)) ;
   testimpl.err = 0 ;
   TEST(&data[0][2]  == instr.nextdata) ;
   TEST(&data[0][32] == instr.enddata) ;
   TEST(&data[0][0]  == instr.startdata) ;
   TEST(&data[0][1]  == instr.keepstart) ;
   TEST(EIO          == instr.readerror) ;
   TEST(1            == testimpl.callcount) ;
   TEST(EIO == readnextdatablock_instream(&instr)) ;
   TEST(&data[0][2]  == instr.nextdata) ;
   TEST(&data[0][32] == instr.enddata) ;
   TEST(&data[0][0]  == instr.startdata) ;
   TEST(&data[0][1]  == instr.keepstart) ;
   TEST(EIO          == instr.readerror) ;
   TEST(1            == testimpl.callcount) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_readbyte(void)
{
   instream_testimpl_t  testimpl ;
   instream_test_it     iimpl = instream_it_INIT(&readnext_testimpl) ;
   instream_t           instr = instream_INIT((instream_impl_t*)&testimpl, asgeneric_instreamit(&iimpl, instream_testimpl_t)) ;
   uint8_t              data[2][250] ;
   bool                 di ;
   uint8_t              nb ;

   // prepare
   for (unsigned i = 0; i < sizeof(data[0]); ++i) {
      data[0][i] = (uint8_t)i ;
      data[1][i] = (uint8_t)i ;
   }

   // TEST readnext_instream: read all bytes
   init_instreamtestimpl(&testimpl, sizeof(data[0]), data[0], data[1]) ;
   di = 1 ;
   for (unsigned i = 0; i < sizeof(data[0]); ++i, di = (i % 16) ? di : !di) {
      nb = 255 ;
      TEST(0 == readnext_instream(&instr, &nb)) ;
      TEST(i == nb) ;
      TEST(instr.nextdata == &data[di][i+1]) ;
   }
   TEST(ENODATA == readnext_instream(&instr, &nb)) ;
   TEST(0 == readerror_instream(&instr)) ;
   TEST(instr.nextdata == 0) ;
   TEST(instr.enddata  == 0) ;
   TEST(instr.startdata == 0) ;
   TEST(instr.keepstart == 0) ;

   // TEST readnext_instream: startkeep_instream
   init_instreamtestimpl(&testimpl, sizeof(data[0]), data[0], data[1]) ;
   TEST(0 == init_instream(&instr, (instream_impl_t*)&testimpl, asgeneric_instreamit(&iimpl, instream_testimpl_t))) ;
   di = 1 ;
   for (unsigned i = 0; i < sizeof(data[0]); ++i, di = (i % 16) ? di : !di) {
      nb = 255 ;
      TEST(0 == readnext_instream(&instr, &nb)) ;
      TEST(i == nb) ;
      TEST(instr.nextdata == &data[di][i+1]) ;
      if (0 == (i % 16)) {
         startkeep_instream(&instr) ;
      }
      TEST(keepaddr_instream(&instr) == &data[di][i - i%16]) ;
   }
   endkeep_instream(&instr) ;
   TEST(ENODATA == readnext_instream(&instr, &nb)) ;
   TEST(0 == readerror_instream(&instr)) ;
   TEST(instr.nextdata == 0) ;
   TEST(instr.enddata  == 0) ;
   TEST(instr.startdata == 0) ;
   TEST(instr.keepstart == 0) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_io_instream()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_interface())   goto ONABORT ;
   if (test_initfree())    goto ONABORT ;
   if (test_readblock())   goto ONABORT ;
   if (test_readbyte())    goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif