/* title: Async-Serial-Communication impl

   Implements <Async-Serial-Communication>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: C-kern/api/io/terminal/sercom.h
    Header file <Async-Serial-Communication>.

   file: C-kern/platform/Linux/io/sercom.c
    Implementation file <Async-Serial-Communication impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/io/terminal/sercom.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif


// section: sercom_t

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_sercom_errtimer
 * Simulates an error in different functions. */
static test_errortimer_t   s_sercom_errtimer = test_errortimer_FREE;
#endif

static const unsigned s_sercom_speed[sercom_config_4000000BPS+1] = {
   [sercom_config_50BPS] = B50,
   [sercom_config_75BPS] = B75,
   [sercom_config_110BPS] = B110,
   [sercom_config_134BPS] = B134,
   [sercom_config_150BPS] = B150,
   [sercom_config_200BPS] = B200,
   [sercom_config_300BPS] = B300,
   [sercom_config_600BPS] = B600,
   [sercom_config_1200BPS] = B1200,
   [sercom_config_1800BPS] = B1800,
   [sercom_config_2400BPS] = B2400,
   [sercom_config_4800BPS] = B4800,
   [sercom_config_9600BPS] = B9600,
   [sercom_config_19200BPS] = B19200,
   [sercom_config_38400BPS] = B38400,
   [sercom_config_57600BPS] = B57600,
   [sercom_config_115200BPS] = B115200,
   [sercom_config_230400BPS] = B230400,
   [sercom_config_460800BPS] = B460800,
   [sercom_config_500000BPS] = B500000,
   [sercom_config_576000BPS] = B576000,
   [sercom_config_921600BPS] = B921600,
   [sercom_config_1000000BPS] = B1000000,
   [sercom_config_1152000BPS] = B1152000,
   [sercom_config_1500000BPS] = B1500000,
   [sercom_config_2000000BPS] = B2000000,
   [sercom_config_2500000BPS] = B2500000,
   [sercom_config_3000000BPS] = B3000000,
   [sercom_config_3500000BPS] = B3500000,
   [sercom_config_4000000BPS] = B4000000
};

// group: lifetime

int init_sercom(/*out*/sercom_t *comport, /*out*/sercom_oldconfig_t *oldconfig/*0==>not returned*/, const char *devicepath, const struct sercom_config_t *config)
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
      sercom_t dummy = { .sysio = fd };
      err = reconfig_sercom(&dummy, config);
      if (err) {
         TRACECALL_ERRLOG("reconfig_sercom", err);
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

int free_sercom(sercom_t *comport)
{
   int err;

   err = free_iochannel(&comport->sysio);
   (void) PROCESS_testerrortimer(&s_sercom_errtimer, &err);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: query

int getconfig_sercom(const sercom_t *comport, /*out*/struct sercom_config_t *config)
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

   config->parity = (sysconfig.c_cflag & PARENB) == 0 ? sercom_config_NOPARITY
                  : (sysconfig.c_cflag & PARODD) == 0 ? sercom_config_EVENPARITY
                  : sercom_config_ODDPARITY;

   config->nrstopbits = 1 + ((sysconfig.c_cflag & CSTOPB) != 0);

   config->speed = 0;
   unsigned speed = cfgetospeed(&sysconfig);
   for (size_t i = 0; i < lengthof(s_sercom_speed); ++i) {
      if (speed == s_sercom_speed[i]) {
         config->speed = (uint8_t) i;
      }
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: update

int reconfig_sercom(sercom_t* comport, const struct sercom_config_t *config)
{
   int err;
   struct termios sysconfig;

   if (  (unsigned)config->nrdatabits-5 > 3
         || config->parity > 2 || (unsigned)config->nrstopbits-1u > 1
         || config->speed >= lengthof(s_sercom_speed)) {
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
   cfsetispeed(&sysconfig, s_sercom_speed[config->speed]);
   cfsetospeed(&sysconfig, s_sercom_speed[config->speed]);
   sysconfig.c_cflag |= config->nrdatabits == 5 ? CS5
                      : config->nrdatabits == 6 ? CS6
                      : config->nrdatabits == 7 ? CS7
                      : CS8;
   sysconfig.c_cflag |= (config->parity != sercom_config_NOPARITY ? PARENB : 0);
   sysconfig.c_cflag |= (config->parity == sercom_config_ODDPARITY ? PARODD : 0);
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

int restore_sercom(sercom_t *comport, const struct sercom_oldconfig_t *oldconfig)
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
   sercom_t        comport = sercom_FREE;
   sys_iochannel_t master  = sys_iochannel_FREE;
   sys_iochannel_t sysio   = sys_iochannel_FREE;
   char            devicepath[256];
   const char      *serdev;
   struct termios  sysconfig;
   sercom_oldconfig_t oldconfig;

   // prepare
   serdev = access("/dev/ttyS0", O_RDWR) == 0 ? "/dev/ttyS0"
          : access("/dev/tts/0", O_RDWR) == 0 ? "/dev/tts/0"
          : 0;
   TEST(0 == create_pseudoserial(&master, devicepath));
   sysio = open(devicepath, O_RDWR|O_CLOEXEC|O_NONBLOCK|O_NOCTTY);
   TEST(0 < sysio);
   TEST(0 == tcgetattr(sysio, &sysconfig));
   TEST(0 == free_iochannel(&sysio));

   // TEST sercom_FREE
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

      // TEST init_sercom: oldconfig is set
      memset(&oldconfig, 0, sizeof(oldconfig));
      TEST( 0 == init_sercom(&comport, &oldconfig, devicepath, 0));
      // check comport
      TEST( 0 <  comport.sysio);
      // check oldconfig
      TEST( oldconfig.sysold[0] == config.c_iflag);
      TEST( oldconfig.sysold[1] == config.c_oflag);
      TEST( oldconfig.sysold[2] == config.c_cflag);
      TEST( oldconfig.sysold[3] == config.c_lflag);
      TEST( oldconfig.sysold[4] == cfgetispeed(&config));
      TEST( oldconfig.sysold[5] == cfgetospeed(&config));

      // TEST free_sercom
      TEST( 0 == free_sercom(&comport));
      // check comport
      TEST( isfree_iochannel(comport.sysio));
   }

   // TEST init_sercom: no oldconfig, no config
   {
      struct termios config;
      TEST( 0 == init_sercom(&comport, 0, devicepath, 0));
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
      TEST(0 == free_sercom(&comport));
   }

   if (serdev != 0) {
      struct termios oldsysconfig;
      sysio = open(serdev, O_RDWR|O_CLOEXEC|O_NONBLOCK|O_NOCTTY);
      TEST(0 == tcsetattr(sysio, TCSANOW, &oldsysconfig));
      TEST(0 == free_iochannel(&sysio));
      for (unsigned tc = 0; tc < 5; ++tc) {
         sercom_config_t config[5] = {
            { .nrdatabits = 5, .parity = 0, .nrstopbits = 1, .speed = sercom_config_134BPS },
            { .nrdatabits = 6, .parity = 1, .nrstopbits = 1, .speed = sercom_config_9600BPS },
            { .nrdatabits = 7, .parity = 2, .nrstopbits = 1, .speed = sercom_config_19200BPS },
            { .nrdatabits = 8, .parity = 1, .nrstopbits = 2, .speed = sercom_config_57600BPS },
            { .nrdatabits = 8, .parity = 0, .nrstopbits = 2, .speed = sercom_config_115200BPS }
         };

         // TEST init_sercom: config is set
         TEST( 0 == init_sercom(&comport, &oldconfig, serdev, &config[tc]));
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
         if (config[tc].parity == sercom_config_NOPARITY) {
            TEST( (sysconfig.c_cflag & PARENB) == 0);
         } else {
            TEST( (sysconfig.c_cflag & PARENB) != 0);
            if (config[tc].parity == sercom_config_ODDPARITY) {
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
         TEST( cfgetispeed(&sysconfig) == s_sercom_speed[config[tc].speed]);
         TEST( cfgetospeed(&sysconfig) == s_sercom_speed[config[tc].speed]);

         // TEST getconfig_sercom
         {
            sercom_config_t conf;
            TEST( 0 == getconfig_sercom(&comport, &conf));
            // check conf
            TEST( conf.nrdatabits == config[tc].nrdatabits);
            TEST( conf.parity     == config[tc].parity);
            TEST( conf.nrstopbits == config[tc].nrstopbits);
            TEST( conf.speed      == config[tc].speed);
         }

         // TEST restore_sercom
         TEST( 0 == restore_sercom(&comport, &oldconfig));
         // check restored config
         TEST( 0 == tcgetattr(comport.sysio, &sysconfig));
         TEST( oldsysconfig.c_iflag == sysconfig.c_iflag);
         TEST( oldsysconfig.c_oflag == sysconfig.c_oflag);
         TEST( oldsysconfig.c_cflag == sysconfig.c_cflag);
         TEST( oldsysconfig.c_lflag == sysconfig.c_lflag);
         TEST( cfgetispeed(&oldsysconfig) == cfgetispeed(&sysconfig))
         TEST( cfgetospeed(&oldsysconfig) == cfgetospeed(&sysconfig))

         // TEST free_sercom
         TEST( 0 == free_sercom(&comport));
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
         sercom_config_t config[5] = {
            { .nrdatabits = 5, .parity = 0, .nrstopbits = 1, .speed = sercom_config_134BPS },
            { .nrdatabits = 6, .parity = 1, .nrstopbits = 1, .speed = sercom_config_9600BPS },
            { .nrdatabits = 7, .parity = 2, .nrstopbits = 1, .speed = sercom_config_19200BPS },
            { .nrdatabits = 8, .parity = 1, .nrstopbits = 2, .speed = sercom_config_57600BPS },
            { .nrdatabits = 8, .parity = 0, .nrstopbits = 2, .speed = sercom_config_115200BPS }
         };

         // TEST init_sercom: config == 0 ==> not changed
         TEST( 0 == init_sercom(&comport, &oldconfig, serdev, 0));
         // check config
         TEST( 0 == tcgetattr(comport.sysio, &sysconfig));
         TEST( oldsysconfig.c_iflag == sysconfig.c_iflag);
         TEST( oldsysconfig.c_oflag == sysconfig.c_oflag);
         TEST( oldsysconfig.c_cflag == sysconfig.c_cflag);
         TEST( oldsysconfig.c_lflag == sysconfig.c_lflag);
         TEST( cfgetispeed(&oldsysconfig) == cfgetispeed(&sysconfig))
         TEST( cfgetospeed(&oldsysconfig) == cfgetospeed(&sysconfig))

         // TEST reconfig_sercom
         TEST( 0 == reconfig_sercom(&comport, &config[tc]));
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
         if (config[tc].parity == sercom_config_NOPARITY) {
            TEST( (sysconfig.c_cflag & PARENB) == 0);
         } else {
            TEST( (sysconfig.c_cflag & PARENB) != 0);
            if (config[tc].parity == sercom_config_ODDPARITY) {
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
         TEST( cfgetispeed(&sysconfig) == s_sercom_speed[config[tc].speed]);
         TEST( cfgetospeed(&sysconfig) == s_sercom_speed[config[tc].speed]);

         // TEST getconfig_sercom
         {
            sercom_config_t conf;
            TEST( 0 == getconfig_sercom(&comport, &conf));
            // check conf
            TEST( conf.nrdatabits == config[tc].nrdatabits);
            TEST( conf.parity     == config[tc].parity);
            TEST( conf.nrstopbits == config[tc].nrstopbits);
            TEST( conf.speed      == config[tc].speed);
         }

         // TEST restore_sercom
         TEST( 0 == restore_sercom(&comport, &oldconfig));
         // check restored config
         TEST( 0 == tcgetattr(comport.sysio, &sysconfig));
         TEST( oldsysconfig.c_iflag == sysconfig.c_iflag);
         TEST( oldsysconfig.c_oflag == sysconfig.c_oflag);
         TEST( oldsysconfig.c_cflag == sysconfig.c_cflag);
         TEST( oldsysconfig.c_lflag == sysconfig.c_lflag);
         TEST( cfgetispeed(&oldsysconfig) == cfgetispeed(&sysconfig))
         TEST( cfgetospeed(&oldsysconfig) == cfgetospeed(&sysconfig))

         // TEST free_sercom
         TEST( 0 == free_sercom(&comport));
         // check comport
         TEST( isfree_iochannel(comport.sysio));
      }
   }

   // TEST init_sercom: EINVAL
   for (unsigned tc = 0; tc < 5; ++tc) {
      sercom_config_t errconfig[5] = {
         { .nrdatabits = 4, .parity = 0, .nrstopbits = 1, .speed = sercom_config_9600BPS },
         { .nrdatabits = 9, .parity = 0, .nrstopbits = 1, .speed = sercom_config_9600BPS },
         { .nrdatabits = 8, .parity = 3, .nrstopbits = 1, .speed = sercom_config_9600BPS },
         { .nrdatabits = 8, .parity = 0, .nrstopbits = 3, .speed = sercom_config_9600BPS },
         { .nrdatabits = 8, .parity = 0, .nrstopbits = 1, .speed = sercom_config_4000000BPS+1 }
      };
      // test
      TEST( EINVAL == init_sercom(&comport, &oldconfig, devicepath, &errconfig[tc]));
      // check comport
      TEST( isfree_iochannel(comport.sysio));
   }

   // TEST init_sercom: ENOTTY no terminal
   TEST( ENOTTY == init_sercom(&comport, &oldconfig, "/dev/zero", 0));
   // check comport
   TEST( isfree_iochannel(comport.sysio));

   // free resources
   TEST(0 == free_iochannel(&sysio));
   TEST(0 == free_iochannel(&master));

   return 0;
ONERR:
   free_iochannel(&sysio);
   free_iochannel(&master);
   free_sercom(&comport);
   return EINVAL;
}

int unittest_io_terminal_sercom()
{
   if (test_initfree())       goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
