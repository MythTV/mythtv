/* === === === === === === === === === === === === === === === === === === ===
 * 
 * Copyright (C) 2006 Michael Thomson <redeye@m-thomson.net>
 * 
 * Based on red_eye.c by Malcolm Smith distributed with no copyright or license
 * from http://www.redremote.co.uk/serial/resdown.html
 * 
 * The original software may still be available from:
 * http://www.redremote.co.uk/serial/red_eye.tgz
 * 
 * This version was modified to compile under gcc 4.x on Fedora Core 4, other
 * platforms may also work. Additional debugging controls were added.
 * 
 * === === === === === === === === === === === === === === === === === === ===
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 * 
 * === === === === === === === === === === === === === === === === === === ===
 */

#include <stdlib.h>	/* standard library definitions			*/
#include <stdio.h>	/* standard buffered input/output		*/
#include <fcntl.h>	/* file control options				*/
#include <unistd.h>	/* standard symbolic constants and types	*/
#include <termios.h>	/* define values for termios			*/
#include <sys/ioctl.h>	/* define values for ioctl operations		*/
#include <string.h>	/* string operations				*/

#define COM_PORT	"/dev/ttyS0";
			/* default port, can be overridden in arguments	*/

int	debug = 0;		/* global debug level				*/

int open_port(char * com_port) {
  int			fd;
  int			err;
  struct termios	options;

  if ( debug >= 1 ) {
    printf("Entering function open_port()\n");
  }
  if ( debug >= 2 ) {
    printf("  Opening port...\n");
  }

  fd = open(com_port, O_WRONLY | O_NOCTTY | O_NDELAY);
  if ( fd == -1 ) {
    perror("  open: Unable to open com port - ");
    exit(EXIT_FAILURE);
  }

  if ( debug >= 2 ) {
    printf("  Opening port succeeded!\n");
    printf("  Setting file options...\n");
  }

  err = fcntl(fd, F_SETFL, 0);
  if ( err != 0 ) {
    perror("  fcntl: Setting file options failed - ");
    exit(EXIT_FAILURE);
  }

  if ( debug >= 2 ) {
    printf("  Setting file options suceeded!\n");
    printf("  Getting current options for the port...\n");
  }

  err = tcgetattr(fd, &options);
  if ( err != 0 ) {
    perror("  tcgetattr: Getting file options failed - ");
    exit(EXIT_FAILURE);
  } 

  if ( debug >= 2 ) {
    printf("  Getting current options for the port succeeded!\n");
    printf("  Setting baud rates for the port...\n");
  }

  err = cfsetispeed(&options, B9600);
  if ( err != 0 ) {
    perror("  cfsetispeed: Setting input baud rate failed - ");
    exit(EXIT_FAILURE);
  } 

  err = cfsetospeed(&options, B9600);
  if ( err != 0 ) {
    perror("  cfsetospeed: Setting output baud rate failed - ");
    exit(EXIT_FAILURE);
  } 

  if ( debug >= 2 ) {
    printf("  Setting baud rates for the port succeeded!\n");
    printf("  Setting local mode...\n");
  }

  /* Enable the receiver and set local mode... */
  options.c_cflag |= (CLOCAL );
  options.c_cflag &= ~PARENB;
  options.c_cflag &= ~CSTOPB;
  options.c_cflag &= ~CSIZE;
  options.c_cflag |= CS8;
  options.c_cflag &= ~CREAD;
  options.c_cflag &= ~CRTSCTS;
  options.c_iflag &= ~(IXON | IXOFF | IXANY);

  err = tcsetattr(fd, TCSANOW, &options);
  if ( err != 0 ) {
    perror("  tcsetattr: Setting local mode failed - ");
    exit(EXIT_FAILURE);
  }

  if ( debug >= 2 ) {
    printf("  Setting local mode suceeded!\n");
  }
  if ( debug >= 1 ) {
    printf("Leaving function open_port()\n");
  }

  return(fd);
}

void set_rts(int fd) {
  /* It appears that the "RTS" line is actually the DTR line in Linux!!! */
  int	bitset = TIOCM_DTR;
  int	status;
  int	err;

  if ( debug >= 1 ) {
    printf("Entering function set_rts()\n");
  }
  if ( debug >= 2 ) {
    printf("  Checking current state of RTS line...\n");
  }

  err = ioctl(fd, TIOCMGET, &status);
  if ( err != 0 ) {
    perror("  ioctl: Checking RTS line failed - ");
    exit(EXIT_FAILURE);
  }

  if ( debug >= 2 ) {
    if ( status & bitset ) {
      printf("    RTS bit is set \n");
    } else {
      printf("    RTS bit is unset\n");
    }
    printf("  Checking current state of RTS line suceeded\n");
    printf("  Setting RTS line...\n");
  }

  status |= bitset;
  err = ioctl(fd, TIOCMSET, &status);
  if ( err != 0 ) {
    perror("  ioctl: Setting RTS line failed - ");
    exit(EXIT_FAILURE);
  }

  if ( debug >= 2 ) {
    printf("  Setting RTS line succeeded\n");
    printf("  Checking new state of RTS line...\n");
  }

  err = ioctl(fd, TIOCMGET, &status);
  if ( err != 0 ) {
    perror("  ioctl: Checking RTS line failed - ");
    exit(EXIT_FAILURE);
  }

  if ( debug >= 2 ) {
    if ( status & bitset ) {
      printf("    RTS bit was set correctly \n");
    } else {
      printf("    FAILED to set RTS bit\n");
      exit(EXIT_FAILURE);
    }
    printf("  Checking new state of RTS line suceeded\n");
  }
  if ( debug >= 1 ) {
    printf("Leaving function set_rts()\n");
  }
}

int main (int argc, char **argv) {
  int	fd;			/* File descriptor for access to serial port */
  char	*com_port = COM_PORT;
  char	*data;
  char	*message;
  int	wait_time;
  int	n;

  debug = 0;

  /* Very crude argv / argc handling as yet */
  switch (argc) {
    case 5: {
      com_port = argv[1];
      data = argv[2];
      wait_time = atoi(argv[3]);
      debug = atoi(argv[4]);
      break;
    }; 
    case 4: {
      com_port = argv[1];
      data = argv[2];
      wait_time = atoi(argv[3]);
      break;
    }; 
    case 3: {
      com_port = COM_PORT;
      data = argv[1];
      debug = atoi(argv[2]);
      wait_time = 1;
      break;
    };
    case 2: {
      com_port = COM_PORT;
      data = argv[1];
      wait_time = 1;
      break;
    };
    default: {
      fputs("usage: red_eye deviceComPort sendString waitSecs [debug]\n",stderr);
      fputs(" -or-  red_eye sendString [debug]\n",stderr);
      fputs("\n",stderr);
      fputs(" where deviceComPort   is e.g. /dev/ttyS0\n",stderr);
      fputs("       sendString      is your desired command to the RedEye Serial\n",stderr);
      fputs("       waitSecs        is pause time after command (in seconds 0-9)\n",stderr);
      fputs("       debug           is debug level (0=off, 9=max)\n",stderr);
      exit(EXIT_FAILURE);
    };
  }

  if ( 0 > wait_time ) wait_time = 0;
  if ( 9 < wait_time ) wait_time = 9;

  if ( 0 > debug ) debug = 0;
  if ( 9 < debug ) debug = 9;

  if ( debug >= 1 ) {
    printf("Opening port for %s\n", com_port);
  }

  fd = open_port(com_port) ;

  if ( debug >= 1 ) {
    printf("Setting RTS for %s\n", com_port);
  }

  set_rts(fd);

  if ( debug >= 1 ) {
    printf("Pausing for 2 seconds to allow IR system to power up\n");
  }

  sleep(2);

  if ( debug >= 1 ) {
    printf("Writing data (\"%s\") now\n",data);
  }

  n = write(fd, strcat(data,"O"), strlen(data)+1);
  if (n < 0) {
    perror("  write: Writing data failed - ");
    exit(EXIT_FAILURE);
  }
  if ( debug >= 1 ) {
    printf("Writing data suceeded\n");
  }

  tcdrain(fd);

  if ( debug >= 1 ) {
    printf("Sleeping for %d seconds ....\n",wait_time);
  }

  sleep(wait_time);

  if ( debug >= 1 ) {
    printf("Start close... ");
  }

  close(fd);

  if ( debug >= 1 ) {
    printf("finished\n");
  }

  exit(EXIT_SUCCESS);
}
