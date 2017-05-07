/* title: Async-Serial-CommPort impl

   Implements <Async-Serial-CommPort>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: C-kern/api/io/terminal/serial.h
    Header file <Async-Serial-CommPort>.

   file: C-kern/platform/Linux/io/serial.c
    Implementation file <Async-Serial-CommPort impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/io/terminal/serial.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif


// section: serial_t

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_serial_errtimer
 * Simulates an error in different functions. */
static test_errortimer_t   s_serial_errtimer = test_errortimer_FREE;
#endif

static const unsigned s_serial_speed[serial_config_4000000BPS+1] = {
   [serial_config_50BPS] = B50,
   [serial_config_75BPS] = B75,
   [serial_config_110BPS] = B110,
   [serial_config_134BPS] = B134,
   [serial_config_150BPS] = B150,
   [serial_config_200BPS] = B200,
   [serial_config_300BPS] = B300,
   [serial_config_600BPS] = B600,
   [serial_config_1200BPS] = B1200,
   [serial_config_1800BPS] = B1800,
   [serial_config_2400BPS] = B2400,
   [serial_config_4800BPS] = B4800,
   [serial_config_9600BPS] = B9600,
   [serial_config_19200BPS] = B19200,
   [serial_config_38400BPS] = B38400,
   [serial_config_57600BPS] = B57600,
   [serial_config_115200BPS] = B115200,
   [serial_config_230400BPS] = B230400,
   [serial_config_460800BPS] = B460800,
   [serial_config_500000BPS] = B500000,
   [serial_config_576000BPS] = B576000,
   [serial_config_921600BPS] = B921600,
   [serial_config_1000000BPS] = B1000000,
   [serial_config_1152000BPS] = B1152000,
   [serial_config_1500000BPS] = B1500000,
   [serial_config_2000000BPS] = B2000000,
   [serial_config_2500000BPS] = B2500000,
   [serial_config_3000000BPS] = B3000000,
   [serial_config_3500000BPS] = B3500000,
   [serial_config_4000000BPS] = B4000000
};

// group: lifetime

int init_serial(/*out*/serial_t *comport, /*out*/serial_oldconfig_t *oldconfig/*0==>not returned*/, const char *devicepath, const struct serial_config_t *config)
{
   int err;
   int fd = open(devicepath, O_RDWR|O_NOCTTY|O_NONBLOCK|O_CLOEXEC);
   struct termios sysconfig;

   if (fd == -1) {
      err = errno;
      TRACESYSCALL_ERRLOG("open", err);
      goto ONERR;
   }

   if (!isatty(fd)) {
      err = ENOTTY;
      TRACESYSCALL_ERRLOG("isatty", err);
      goto ONERR;
   }

   if (tcgetattr(fd, &sysconfig)) {
      err = errno;
      TRACESYSCALL_ERRLOG("tcgetattr", err);
      goto ONERR;
   }

   if (oldconfig) {
      oldconfig->sysold[0] = sysconfig.c_iflag;
      oldconfig->sysold[1] = sysconfig.c_oflag;
      oldconfig->sysold[2] = sysconfig.c_cflag;
      oldconfig->sysold[3] = sysconfig.c_lflag;
      oldconfig->sysold[4] = cfgetispeed(&sysconfig);
      oldconfig->sysold[5] = cfgetospeed(&sysconfig);
   }

   if (config) {
      serial_t dummy = { .sysio = fd };
      err = reconfig_serial(&dummy, config);
      if (err) {
         TRACECALL_ERRLOG("reconfig_serial", err);
         goto ONERR;
      }
   }

   // set out
   comport->sysio = fd;

   return 0;
ONERR:
   PRINTCSTR_ERRLOG(devicepath);
   free_iochannel(&fd);
   TRACEEXIT_ERRLOG(err);
   return err;
}

int free_serial(serial_t *comport)
{
   int err;

   err = free_iochannel(&comport->sysio);
   (void) PROCESS_testerrortimer(&s_serial_errtimer, &err);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: query

int getconfig_serial(const serial_t *comport, /*out*/struct serial_config_t *config)
{
   int err;
   struct termios sysconfig;

   if (tcgetattr(comport->sysio, &sysconfig)) {
      err = errno;
      TRACESYSCALL_ERRLOG("tcgetattr", err);
      goto ONERR;
   }

   if ((sysconfig.c_cflag & CSIZE) == CS5) config->nrdatabits = 5;
   else if ((sysconfig.c_cflag & CSIZE) == CS6) config->nrdatabits = 6;
   else if ((sysconfig.c_cflag & CSIZE) == CS7) config->nrdatabits = 7;
   else if ((sysconfig.c_cflag & CSIZE) == CS8) config->nrdatabits = 8;

   config->parity = (sysconfig.c_cflag & PARENB) == 0 ? serial_config_NOPARITY
                  : (sysconfig.c_cflag & PARODD) == 0 ? serial_config_EVENPARITY
                  : serial_config_ODDPARITY;

   config->nrstopbits = 1 + ((sysconfig.c_cflag & CSTOPB) != 0);

   config->speed = 0;
   unsigned speed = cfgetospeed(&sysconfig);
   for (size_t i = 0; i < lengthof(s_serial_speed); ++i) {
      if (speed == s_serial_speed[i]) {
         config->speed = (uint8_t) i;
      }
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: update

int reconfig_serial(serial_t* comport, const struct serial_config_t *config)
{
   int err;
   struct termios sysconfig;

   if (  (unsigned)config->nrdatabits-5 > 3
         || config->parity > 2 || (unsigned)config->nrstopbits-1u > 1
         || config->speed >= lengthof(s_serial_speed)) {
      err = EINVAL;
      goto ONERR;
   }

   if (tcgetattr(comport->sysio, &sysconfig)) {
      err = errno;
      TRACESYSCALL_ERRLOG("tcgetattr", err);
      goto ONERR;
   }

/*
   IGNBRK : ignore BREAK condition on input
   IGNPAR : ignore framing errors and parity errors
   PARMRK : not set ==> byte with parity error or framing error is read as '\0' else it is marked with prefix sequence \377 \0
   INPCK  : enable input parity check
   IXON   : enable XON/XOFF flow control on output
   IXOFF  : enable XON/XOFF flow control on input
   ISTRIP : strip off eighth bit
   OPOST  : enable implementation-defined output processing
   OFILL  : send fill characters for a delay delay
   CSIZE  : Character size mask
   CSTOPB : set two stop bits
   CREAD  : enable receiver
   PARENB : enable parity generation on output and parity checking for input
   PARODD : parity for input and output is odd
   CLOCAL : ignore modem control lines
   ISIG   : received characters  INTR,  QUIT,  SUSP, or DSUSP are
            generate the corresponding signal.
   ICANON : enable canonical mode (otherwise single raw character mode)
   ECHO   : echo input characters
*/

   sysconfig.c_iflag |= (IGNBRK|INPCK);
   sysconfig.c_iflag &= ~(unsigned)(IGNPAR|PARMRK|IXON|IXOFF|INLCR|IGNCR|ICRNL|ISTRIP);
   sysconfig.c_oflag &= ~(unsigned)(OPOST|ONLCR|OCRNL|ONLRET|OFILL);
   sysconfig.c_cflag |= (CLOCAL|CREAD);
   sysconfig.c_cflag &= ~(unsigned)(PARENB|PARODD|CSTOPB|CSIZE);
   sysconfig.c_lflag &= ~(unsigned)(ISIG|ICANON|ECHO);
   cfsetispeed(&sysconfig, s_serial_speed[config->speed]);
   cfsetospeed(&sysconfig, s_serial_speed[config->speed]);
   sysconfig.c_cflag |= config->nrdatabits == 5 ? CS5
                      : config->nrdatabits == 6 ? CS6
                      : config->nrdatabits == 7 ? CS7
                      : CS8;
   sysconfig.c_cflag |= (config->parity != serial_config_NOPARITY ? PARENB : 0);
   sysconfig.c_cflag |= (config->parity == serial_config_ODDPARITY ? PARODD : 0);
   sysconfig.c_cflag |= (config->nrstopbits == 2 ? CSTOPB : 0);

   if (tcsetattr(comport->sysio, TCSAFLUSH, &sysconfig)) {
      err = errno;
      TRACESYSCALL_ERRLOG("tcsetattr", err);
      goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int restore_serial(serial_t *comport, const struct serial_oldconfig_t *oldconfig)
{
   int err;
   struct termios sysconfig;

   if (tcgetattr(comport->sysio, &sysconfig)) {
      err = errno;
      TRACESYSCALL_ERRLOG("tcgetattr", err);
      goto ONERR;
   }

   sysconfig.c_iflag = oldconfig->sysold[0];
   sysconfig.c_oflag = oldconfig->sysold[1];
   sysconfig.c_cflag = oldconfig->sysold[2];
   sysconfig.c_lflag = oldconfig->sysold[3];
   cfsetispeed(&sysconfig, (speed_t) oldconfig->sysold[4]);
   cfsetospeed(&sysconfig, (speed_t) oldconfig->sysold[5]);

   if (tcsetattr(comport->sysio, TCSAFLUSH, &sysconfig)) {
      err = errno;
      TRACESYSCALL_ERRLOG("tcsetattr", err);
      goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST


static int create_pseudoserial(sys_iochannel_t* sysio, char devicepath[256])
{
   *sysio = posix_openpt(O_RDWR|O_CLOEXEC|O_NOCTTY);
   TEST(0 <  *sysio);
   TEST(0 == grantpt(*sysio));
   TEST(0 == unlockpt(*sysio));
   TEST(0 != ptsname(*sysio));
   TEST(256 > strlen(ptsname(*sysio)));
   strcpy(devicepath, ptsname(*sysio));

   return 0;
ONERR:
   return EINVAL;
}

static int test_initfree(void)
{
   serial_t        comport = serial_FREE;
   sys_iochannel_t master  = sys_iochannel_FREE;
   sys_iochannel_t sysio   = sys_iochannel_FREE;
   char            devicepath[256];
   const char      *serdev;
   struct termios  sysconfig;
   serial_oldconfig_t oldconfig;

   // prepare
   serdev = access("/dev/ttyS0", O_RDWR) == 0 ? "/dev/ttyS0"
          : access("/dev/tts/0", O_RDWR) == 0 ? "/dev/tts/0"
          : 0;
   TEST(0 == create_pseudoserial(&master, devicepath));
   sysio = open(devicepath, O_RDWR|O_CLOEXEC|O_NONBLOCK|O_NOCTTY);
   TEST(0 < sysio);
   TEST(0 == tcgetattr(sysio, &sysconfig));
   TEST(0 == free_iochannel(&sysio));

   // TEST serial_FREE
   TEST( isfree_iochannel(comport.sysio));

   for (unsigned i = 0; i < 3; ++i) {
      struct termios config = sysconfig;
      switch (i) {
         case 0:  config.c_iflag |= (IGNBRK|INPCK);
                  config.c_oflag &= ~(unsigned)(OPOST|ONLCR|OCRNL|ONLRET|OFILL);
                  config.c_iflag &= ~(unsigned)(IGNPAR|PARMRK|IXON|IXOFF|INLCR|IGNCR|ICRNL|ISTRIP);
                  config.c_cflag &= ~(unsigned)(CLOCAL);
                  cfsetispeed(&config, B4800);
                  cfsetospeed(&config, B9600);
                  break;
         case 1:  config.c_cflag |= (CLOCAL|CREAD);
                  config.c_lflag &= ~(unsigned)(ISIG|ICANON|ECHO);
                  cfsetispeed(&config, B50);
                  cfsetospeed(&config, B134);
                  break;
         default: break;
      }
      sysio = open(devicepath, O_RDWR|O_CLOEXEC|O_NONBLOCK|O_NOCTTY);
      TEST(0 == tcsetattr(sysio, TCSANOW, &config));
      TEST(0 == free_iochannel(&sysio));

      // TEST init_serial: oldconfig is set
      memset(&oldconfig, 0, sizeof(oldconfig));
      TEST( 0 == init_serial(&comport, &oldconfig, devicepath, 0));
      // check comport
      TEST( 0 <  comport.sysio);
      // check oldconfig
      TEST( oldconfig.sysold[0] == config.c_iflag);
      TEST( oldconfig.sysold[1] == config.c_oflag);
      TEST( oldconfig.sysold[2] == config.c_cflag);
      TEST( oldconfig.sysold[3] == config.c_lflag);
      TEST( oldconfig.sysold[4] == cfgetispeed(&config));
      TEST( oldconfig.sysold[5] == cfgetospeed(&config));

      // TEST free_serial
      TEST( 0 == free_serial(&comport));
      // check comport
      TEST( isfree_iochannel(comport.sysio));
   }

   // TEST init_serial: no oldconfig, no config
   {
      struct termios config;
      TEST( 0 == init_serial(&comport, 0, devicepath, 0));
      // check comport
      TEST( 0 <  comport.sysio);
      // check comport.sysio config not changed
      TEST( 0 == tcgetattr(comport.sysio, &config));
      TEST( config.c_iflag == sysconfig.c_iflag);
      TEST( config.c_oflag == sysconfig.c_oflag);
      TEST( config.c_cflag == sysconfig.c_cflag);
      TEST( config.c_lflag == sysconfig.c_lflag);
      TEST( cfgetispeed(&config) == cfgetispeed(&sysconfig))
      TEST( cfgetospeed(&config) == cfgetospeed(&sysconfig))
      // reset
      TEST(0 == free_serial(&comport));
   }

   if (serdev != 0) {
      struct termios oldsysconfig;
      sysio = open(serdev, O_RDWR|O_CLOEXEC|O_NONBLOCK|O_NOCTTY);
      TEST(0 == tcsetattr(sysio, TCSANOW, &oldsysconfig));
      TEST(0 == free_iochannel(&sysio));
      for (unsigned tc = 0; tc < 5; ++tc) {
         serial_config_t config[5] = {
            { .nrdatabits = 5, .parity = 0, .nrstopbits = 1, .speed = serial_config_134BPS },
            { .nrdatabits = 6, .parity = 1, .nrstopbits = 1, .speed = serial_config_9600BPS },
            { .nrdatabits = 7, .parity = 2, .nrstopbits = 1, .speed = serial_config_19200BPS },
            { .nrdatabits = 8, .parity = 1, .nrstopbits = 2, .speed = serial_config_57600BPS },
            { .nrdatabits = 8, .parity = 0, .nrstopbits = 2, .speed = serial_config_115200BPS }
         };

         // TEST init_serial: config is set
         TEST( 0 == init_serial(&comport, &oldconfig, serdev, &config[tc]));
         // check comport
         TEST( 0 <  comport.sysio);
         // check config
         TEST( 0 == tcgetattr(comport.sysio, &sysconfig));
         if (config[tc].nrdatabits == 5) {
            TEST( (sysconfig.c_cflag & CSIZE) == CS5);
         } else if (config[tc].nrdatabits == 6) {
            TEST( (sysconfig.c_cflag & CSIZE) == CS6);
         } else if (config[tc].nrdatabits == 7) {
            TEST( (sysconfig.c_cflag & CSIZE) == CS7);
         } else {
            TEST( (sysconfig.c_cflag & CSIZE) == CS8);
         }
         if (config[tc].parity == serial_config_NOPARITY) {
            TEST( (sysconfig.c_cflag & PARENB) == 0);
         } else {
            TEST( (sysconfig.c_cflag & PARENB) != 0);
            if (config[tc].parity == serial_config_ODDPARITY) {
               TEST( (sysconfig.c_cflag & PARODD) != 0);
            } else {
               TEST( (sysconfig.c_cflag & PARODD) == 0);
            }
         }
         if (config[tc].nrstopbits == 1) {
            TEST( (sysconfig.c_cflag & CSTOPB) == 0);
         } else {
            TEST( (sysconfig.c_cflag & CSTOPB) != 0);
         }
         TEST( cfgetispeed(&sysconfig) == s_serial_speed[config[tc].speed]);
         TEST( cfgetospeed(&sysconfig) == s_serial_speed[config[tc].speed]);

         // TEST getconfig_serial
         {
            serial_config_t conf;
            TEST( 0 == getconfig_serial(&comport, &conf));
            // check conf
            TEST( conf.nrdatabits == config[tc].nrdatabits);
            TEST( conf.parity     == config[tc].parity);
            TEST( conf.nrstopbits == config[tc].nrstopbits);
            TEST( conf.speed      == config[tc].speed);
         }

         // TEST restore_serial
         TEST( 0 == restore_serial(&comport, &oldconfig));
         // check restored config
         TEST( 0 == tcgetattr(comport.sysio, &sysconfig));
         TEST( oldsysconfig.c_iflag == sysconfig.c_iflag);
         TEST( oldsysconfig.c_oflag == sysconfig.c_oflag);
         TEST( oldsysconfig.c_cflag == sysconfig.c_cflag);
         TEST( oldsysconfig.c_lflag == sysconfig.c_lflag);
         TEST( cfgetispeed(&oldsysconfig) == cfgetispeed(&sysconfig))
         TEST( cfgetospeed(&oldsysconfig) == cfgetospeed(&sysconfig))

         // TEST free_serial
         TEST( 0 == free_serial(&comport));
         // check comport
         TEST( isfree_iochannel(comport.sysio));
      }
   }

   if (serdev != 0) {
      struct termios oldsysconfig;
      sysio = open(serdev, O_RDWR|O_CLOEXEC|O_NONBLOCK|O_NOCTTY);
      TEST(0 == tcsetattr(sysio, TCSANOW, &oldsysconfig));
      TEST(0 == free_iochannel(&sysio));
      for (unsigned tc = 0; tc < 5; ++tc) {
         serial_config_t config[5] = {
            { .nrdatabits = 5, .parity = 0, .nrstopbits = 1, .speed = serial_config_134BPS },
            { .nrdatabits = 6, .parity = 1, .nrstopbits = 1, .speed = serial_config_9600BPS },
            { .nrdatabits = 7, .parity = 2, .nrstopbits = 1, .speed = serial_config_19200BPS },
            { .nrdatabits = 8, .parity = 1, .nrstopbits = 2, .speed = serial_config_57600BPS },
            { .nrdatabits = 8, .parity = 0, .nrstopbits = 2, .speed = serial_config_115200BPS }
         };

         // TEST init_serial: config == 0 ==> not changed
         TEST( 0 == init_serial(&comport, &oldconfig, serdev, 0));
         // check config
         TEST( 0 == tcgetattr(comport.sysio, &sysconfig));
         TEST( oldsysconfig.c_iflag == sysconfig.c_iflag);
         TEST( oldsysconfig.c_oflag == sysconfig.c_oflag);
         TEST( oldsysconfig.c_cflag == sysconfig.c_cflag);
         TEST( oldsysconfig.c_lflag == sysconfig.c_lflag);
         TEST( cfgetispeed(&oldsysconfig) == cfgetispeed(&sysconfig))
         TEST( cfgetospeed(&oldsysconfig) == cfgetospeed(&sysconfig))

         // TEST reconfig_serial
         TEST( 0 == reconfig_serial(&comport, &config[tc]));
         // check comport
         TEST( 0 <  comport.sysio);
         // check config
         TEST( 0 == tcgetattr(comport.sysio, &sysconfig));
         if (config[tc].nrdatabits == 5) {
            TEST( (sysconfig.c_cflag & CSIZE) == CS5);
         } else if (config[tc].nrdatabits == 6) {
            TEST( (sysconfig.c_cflag & CSIZE) == CS6);
         } else if (config[tc].nrdatabits == 7) {
            TEST( (sysconfig.c_cflag & CSIZE) == CS7);
         } else {
            TEST( (sysconfig.c_cflag & CSIZE) == CS8);
         }
         if (config[tc].parity == serial_config_NOPARITY) {
            TEST( (sysconfig.c_cflag & PARENB) == 0);
         } else {
            TEST( (sysconfig.c_cflag & PARENB) != 0);
            if (config[tc].parity == serial_config_ODDPARITY) {
               TEST( (sysconfig.c_cflag & PARODD) != 0);
            } else {
               TEST( (sysconfig.c_cflag & PARODD) == 0);
            }
         }
         if (config[tc].nrstopbits == 1) {
            TEST( (sysconfig.c_cflag & CSTOPB) == 0);
         } else {
            TEST( (sysconfig.c_cflag & CSTOPB) != 0);
         }
         TEST( cfgetispeed(&sysconfig) == s_serial_speed[config[tc].speed]);
         TEST( cfgetospeed(&sysconfig) == s_serial_speed[config[tc].speed]);

         // TEST getconfig_serial
         {
            serial_config_t conf;
            TEST( 0 == getconfig_serial(&comport, &conf));
            // check conf
            TEST( conf.nrdatabits == config[tc].nrdatabits);
            TEST( conf.parity     == config[tc].parity);
            TEST( conf.nrstopbits == config[tc].nrstopbits);
            TEST( conf.speed      == config[tc].speed);
         }

         // TEST restore_serial
         TEST( 0 == restore_serial(&comport, &oldconfig));
         // check restored config
         TEST( 0 == tcgetattr(comport.sysio, &sysconfig));
         TEST( oldsysconfig.c_iflag == sysconfig.c_iflag);
         TEST( oldsysconfig.c_oflag == sysconfig.c_oflag);
         TEST( oldsysconfig.c_cflag == sysconfig.c_cflag);
         TEST( oldsysconfig.c_lflag == sysconfig.c_lflag);
         TEST( cfgetispeed(&oldsysconfig) == cfgetispeed(&sysconfig))
         TEST( cfgetospeed(&oldsysconfig) == cfgetospeed(&sysconfig))

         // TEST free_serial
         TEST( 0 == free_serial(&comport));
         // check comport
         TEST( isfree_iochannel(comport.sysio));
      }
   }

   // TEST init_serial: EINVAL
   for (unsigned tc = 0; tc < 5; ++tc) {
      serial_config_t errconfig[5] = {
         { .nrdatabits = 4, .parity = 0, .nrstopbits = 1, .speed = serial_config_9600BPS },
         { .nrdatabits = 9, .parity = 0, .nrstopbits = 1, .speed = serial_config_9600BPS },
         { .nrdatabits = 8, .parity = 3, .nrstopbits = 1, .speed = serial_config_9600BPS },
         { .nrdatabits = 8, .parity = 0, .nrstopbits = 3, .speed = serial_config_9600BPS },
         { .nrdatabits = 8, .parity = 0, .nrstopbits = 1, .speed = serial_config_4000000BPS+1 }
      };
      // test
      TEST( EINVAL == init_serial(&comport, &oldconfig, devicepath, &errconfig[tc]));
      // check comport
      TEST( isfree_iochannel(comport.sysio));
   }

   // TEST init_serial: ENOTTY no terminal
   TEST( ENOTTY == init_serial(&comport, &oldconfig, "/dev/zero", 0));
   // check comport
   TEST( isfree_iochannel(comport.sysio));

   // adapt log
   size_t   len = strlen(devicepath);
   size_t   logsize;
   uint8_t *logbuf;
   GETBUFFER_ERRLOG(&logbuf, &logsize);
   for (uint8_t* pos=logbuf; (pos=(uint8_t*)strstr((char*)pos, devicepath)) != 0; ) {
      pos[0] = 'X';
      memmove(pos+1, pos + len, (size_t) (logbuf + logsize - (pos + len)));
      logsize -= (len-1);
      logbuf[logsize] = 0;
   }
   TRUNCATEBUFFER_ERRLOG(logsize);

   // free resources
   TEST(0 == free_iochannel(&sysio));
   TEST(0 == free_iochannel(&master));

   return 0;
ONERR:
   free_iochannel(&sysio);
   free_iochannel(&master);
   free_serial(&comport);
   return EINVAL;
}

int unittest_io_terminal_serial()
{
   if (test_initfree())       goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
