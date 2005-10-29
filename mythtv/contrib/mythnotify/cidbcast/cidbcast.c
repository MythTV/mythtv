/*
** Caller ID UDP broadcaster
**
** Ken Bass (kbass@kenbass.com) 9/13/03
**
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <termios.h>

typedef struct
{
  char ring_line[5];
  char date[5];
  char time[5];
  char number[20];
  char name[20];
} CID_Info;

int checkfor_cid_info(char *buffer, int len, CID_Info *cid_info);
int checkfor_two_rings(char *buffer, int len);

int verbose = 0;
char xml_filename[255];
char bcast_addr[16];
int bcast_addr_hex=-1;
char serial_port[16];
int udp_port;
char modem_init_string[255];

int ipaddr_to_int(const char *stringIP, unsigned int *ip)
{
  int a,b,c,d;
  
  if (4 != sscanf(stringIP, "%d.%d.%d.%d", &a, &b, &c, &d))
    {
      return -1;
    }
  
  *ip = ((a&0xFF) << 24) +
    ((b&0xFF) << 16) +
    ((c&0xFF) <<  8) +
    (d&0xFF);
  
  return 0;
}

char *get_tail(char *string)
{
  char *c;
  c = string+strlen(string);
  int found = 0;
  while(c!=string)
  {
    if (*c=='/')
    {
      found = 1;
      c++;
      break;
    }
    c--;
  }

  return c;
}

char *replace(char *in_string, char *out_string, char* key_string, char *replace_string)
{
  int str_index, outstring_idx, key_idx, end, replace_len, key_len, cpy_len;
  char *c;
  
  if ((c = (char *) strstr(in_string, key_string)) == NULL)
  {
    strcpy(out_string, in_string);
    return out_string;
  }
  
  replace_len = strlen(replace_string);
  key_len = strlen(key_string);
  end = strlen(in_string) - key_len;
  key_idx = c - in_string;
  
  outstring_idx = 0;
  str_index = 0;
  while(str_index <= end && c != NULL)
  {
    /* Copy characters from the left of matched pattern occurence */
    cpy_len = key_idx-str_index;
    strncpy(out_string+outstring_idx, in_string+str_index, cpy_len);
    outstring_idx += cpy_len;
    str_index += cpy_len;
    
    /* Copy replacement characters instead of matched pattern */
    strcpy(out_string+outstring_idx, replace_string);
    outstring_idx += replace_len;
    str_index += key_len;
    
    /* Check for another pattern match */
    if ((c = (char *) strstr(in_string+str_index, key_string)) != NULL)
    {
      key_idx = c - in_string;
    }
  }

  /* Copy remaining characters from the right of last matched pattern */
  strcpy(out_string+outstring_idx, in_string+str_index);
  
  return out_string; 
}

int open_modem()
{
  int            fd;
  struct termios options;
  
  /* open the port */
  fd = open(serial_port, O_RDWR | O_NOCTTY | O_NDELAY);
  fcntl(fd, F_SETFL, 0);
  
  /* get the current options */
  tcgetattr(fd, &options);
  
  /* set raw input, 1 second timeout */
  cfsetospeed(&options, B38400);
  cfsetispeed(&options, B38400); 
  options.c_cflag     |= (CLOCAL | CREAD);
  options.c_lflag     &= ~(ICANON | ECHO | ECHOE | ISIG);
  options.c_oflag     &= ~OPOST;
  options.c_cc[VMIN]  = 0;
  options.c_cc[VTIME] = 10;
  
  /* set the options */
  tcsetattr(fd, TCSANOW, &options);

  return fd;
}

int issue_modem_cmd(int fd, char *cmd_string)
{
 char buffer[255];  /* Input buffer */
 char *bufptr;      /* Current char in buffer */
 int  nbytes;       /* Number of bytes read */
 int  tries;        /* Number of tries so far */
 
 memset(buffer, 0, 255);

 for (tries = 0; tries < 3; tries ++)
 {

   if (verbose)
   {
     printf("Send %s\n", cmd_string);
   }

   if (write(fd, cmd_string, strlen(cmd_string)) < strlen(cmd_string))
   {
     printf("AT send failed on try %d\n", tries);
     continue;
   }
   
   /* read characters into our string buffer until we get a CR or NL */
   bufptr = buffer;
   while ((nbytes = read(fd, bufptr, buffer + sizeof(buffer) - bufptr - 1)) > 0)
     {
       bufptr += nbytes;
       if (bufptr[-1] == '\n' || bufptr[-1] == '\r')
	 break;
     }

   /* nul terminate the string and see if we got an OK response */
   *bufptr = '\0';

   if (strstr(buffer, "OK") != 0)
     return (0);

   sleep(1);
 }
 
 return (-1);
}

int waitfor_ring(int fd, char *wait_string, CID_Info *cid_info)
{
 char buffer[255];  /* Input buffer */
 char *bufptr;      /* Current char in buffer */
 int  nbytes;       /* Number of bytes read */
 fd_set mrd_set;
 int rv;
 struct timeval tv;
 struct timeval *tv_p;
 memset(buffer, 0, 255);
 int first = 0;

 FD_ZERO(&mrd_set);
 FD_SET (fd, &mrd_set);

 /* read characters into our string buffer until we get a CR or NL */
 bufptr = buffer;

 while (1)
   {
     /* Wait up to ten seconds. */
     if (first == 0)
     {
       tv_p = NULL;
       first = 1;
     }
     else
     {
       tv.tv_sec = 10;
       tv.tv_usec = 0;
       tv_p = &tv;
     }

     /* Keep this in mind - this select will be invoked as characters
     ** are received from the serial device. There is no guarantee how
     ** many characters will be received on each 'read'. Therefore
     ** each time we read characters we append them to the tail end of
     ** the buffer. checkfor_cid_info() below will check the buffer for
     ** complete caller ID info.
     */
     
     rv = select(fd+1, &mrd_set, NULL, NULL, tv_p);
     if (rv < 0)
       {
	 printf("Select failed\n");
	 return -1;
       }
     else if (rv == 0)
       {
	 if (verbose)
	 {
	   printf("Timed out\n");
	 }
	 return -1;
       }
     
     if (FD_ISSET(fd, &mrd_set))
       {
	 while ((nbytes = read(fd, bufptr, buffer + sizeof(buffer) - bufptr - 1)) > 0)
	   {
	     bufptr += nbytes;
	   }
	 
	 /* nul terminate the string and see if we got a complete response */
	 *bufptr = '\0';
	 
	 if (checkfor_cid_info(buffer, 255,
			       cid_info))
	   {
	     return 0;
	   }

	 /* If we encounter 2 rings we never got caller ID info*/
	 if (checkfor_two_rings(buffer, 255))
	   {
	     return -1;
	   }
       }
   }

   return (-1);
}

int checkfor_two_rings(char *buffer, int len)
{
  char *bptr;
  char *c;
  char key[6] = "\nRING";

  if ((c = (char *) strstr(buffer, key)) == NULL)
  {
    if (verbose)
    {
      printf("Found ZERO RINGS\n");
    }
    return 0;
  }

  bptr = c+strlen(key);
  if ((c = (char *) strstr(bptr, key)) == NULL)
  {
    if (verbose)
    {
      printf("Found ONE RINGS\n");
    }
    return 0;
  }
  else
  {
    if (verbose)
    {
      printf("Found TWO RINGS\n");
    }
    return 1;
  }

}

char *extract_cid_field(char *buffer, int len, char *field_name, int *field_len)
{
  char *bptr, *eptr;
  char *c;
  int complete;

  if ((c = (char *) strstr(buffer, field_name)) == NULL)
  {
    if (verbose)
    {
      printf("%s not Found\n", field_name);
    }
    return 0;
  }

  bptr = c+strlen(field_name);
  eptr = bptr;
  complete = 0;
  while (eptr != 0)
  {
    if ((*eptr == '\r' || *eptr == '\n'))
    {
      complete = 1;
      break;
    }
    eptr++;
  }

  if (complete)
  {
    *field_len = eptr - bptr;
    return bptr;
  }
  else
  {
    *field_len = 0;
    return NULL;
  }
}

int getNum(char *dest, int dsize, const char *src, int ssize)
{
  int ret = 0;

  if ( dest && src )
  {
    int i, j;
    memset(dest, 0, dsize);
    for(i=0,j=0; i<ssize && i<dsize; i++)
    {
      if ( isspace(src[i]) )
      {
        break;
      }
      else if ( isdigit(src[i]) )
      {
        dest[j++] = src[i];
      }
    }
    ret = j;
  }

  return ret;
}

/*
** Data comes from modem (when enabled via AT#CID=1) as
** DATE = 0915
** TIME = 1700
** NMBR = 3015551212
** NAME = SCHMOE JOE
**
** For the ZyXel 1496E+
** TIME: 09-15 17:00
** CALLER NAME: SCHMOE JOE
** CALLER NUMBER: 3015551212
**
** init string is: "AT E0 L0 M0 N0 Q0 V1 X7 &C1 &H3 S0=0 S7=45 S13.2=1 S40.2=1 S40.3=1 S40.4=1"
*/

int checkfor_cid_info(char *buffer, int len, CID_Info *cid_info)
{
  char *field_ptr, date[8], time[8];
  int field_len, time_len = 0;

  if ( (field_ptr = extract_cid_field(buffer, len, "NAME = ", &field_len)) 
   || (field_ptr = extract_cid_field(buffer, len, "CALLER NAME: ", &field_len)) )
  {
    memset(cid_info->name, 0, sizeof(cid_info->name));
    memcpy(cid_info->name, field_ptr, field_len);
    if (verbose)
    {
      printf("name %s\n",cid_info->name);
    }
  }
  else
  {
    if (verbose)
    {
      printf("NAME not found\n");
    }
    return 0;
  }

  if ( (field_ptr = extract_cid_field(buffer, len, "NMBR = ", &field_len))
   || (field_ptr = extract_cid_field(buffer, len, "CALLER NUMBER: ", &field_len)) )
  {
    memset(cid_info->number, 0, sizeof(cid_info->number));
    memcpy(cid_info->number, field_ptr, field_len);
    if (verbose)
    {
      printf("number %s\n",cid_info->number);
    }
  }
  else
  {
    if (verbose)
    {
      printf("NMBR not found\n");
    }
    return 0;
  }

  if ( (field_ptr = extract_cid_field(buffer, len, "TIME: ", &field_len)) )
  {
    int date_len;
    date_len = getNum(date, sizeof(date), field_ptr, field_len);
    time_len = getNum(time, sizeof(time), &field_ptr[date_len+2], field_len-date_len-2);
    field_ptr = &date[0];
    field_len = date_len;
    if ( verbose )
    {
      printf( "Date(%d): %s\nTime(%d): %s\nField_ptr: %s\n",
        field_len, date, time_len, time, field_ptr );
    }
  }
  else
  {
    field_ptr = extract_cid_field(buffer, len, "DATE = ", &field_len);
  }
  if (field_ptr)
  {
    memset(cid_info->date, 0, sizeof(cid_info->date));
    memcpy(cid_info->date, field_ptr, field_len);
    if (verbose)
    {
      printf("date %s\n",cid_info->date);
    }
  }
  else
  {
    if (verbose)
    {
      printf("DATE not found\n");
    }
    return 0;
  }

  if (time_len)
  {
    field_ptr = &time[0];
    field_len = time_len;
  }
  else
  {
    field_ptr = extract_cid_field(buffer, len, "TIME = ", &field_len);
  }
  if (field_ptr)
  {
    memset(cid_info->time, 0, sizeof(cid_info->time));
    memcpy(cid_info->time, field_ptr, field_len);
    if (verbose)
    {
      printf("time %s\n",cid_info->time);
    }
  }
  else
  {
    if (verbose)
    {
      printf("TIME not found\n");
    }
    return 0;
  }

  field_ptr = extract_cid_field(buffer, len, "RING", &field_len);
  if (field_ptr)
  {
    memset(cid_info->ring_line, 0, sizeof(cid_info->ring_line));
    memcpy(cid_info->ring_line, field_ptr, field_len);
    if (strcmp(cid_info->ring_line, " A") == 0)
    {
      strcpy(cid_info->ring_line, "1");
    }
    else if (strcmp(cid_info->ring_line, " B") == 0)
    {
      strcpy(cid_info->ring_line, "2");
    }
    else if (strcmp(cid_info->ring_line, " C") == 0)
    {
      strcpy(cid_info->ring_line, "3");
    }
    else if (strcmp(cid_info->ring_line, " D") == 0)
    {
      strcpy(cid_info->ring_line, "4");
    }
    else
    {
      strcpy(cid_info->ring_line, "1");
    }

    if (verbose)
    {
      printf("ring_line %s\n",cid_info->ring_line);
    }
  }
  else
  {
    if (verbose)
    {
      printf("RING not found\n");
    }
    return 0;
  }

  return 1;
}

int close_modem(int fd)
{
  close(fd);

  return 0;

}

int init_modem(int *fd)
{

  *fd = open_modem();

  if (*fd <= 0)
  {
    printf("error open modem\n");
    return -1;
  }

  if (write(*fd, "+++", 3) != 3)
  {
    printf("error init modem\n");
    return -1;
  }

  sleep(2);
  
  if (issue_modem_cmd(*fd, "AT\r") < 0)
  {
    printf("error init modem\n");
    return -1;
  }
  
  if (issue_modem_cmd(*fd, modem_init_string) < 0)
  {
    printf("error init modem\n");
    return -1;
  }

  return 0;  
}

void print_help(char *progname)
{
  printf("\nUsage: %s [--once] --file=xyz.xml [OPTIONS]\n", progname);
  printf("A caller id UDP broadcast utility.\n\n");
  printf("  -d, --device    : serial device (--device=/dev/ttyS0)\n");
  printf("  -u, --udpport   : UDP port to broadcase (--udpport=6947)\n");
  printf("  -b, --bcast     : UDP broadcast address (--bcast=255.255.255.255)\n");
  printf("  -f, --file      : (REQUIRED)XML file to use as a template (--file=cidbcast.xml)\n");
  printf("  -i, --init      : Modem init string (--init=\"AT S7=45 S0=0 L0 V1 X4 &c1 E0 Q0 #CID=1 S41=1\")\n");
  printf("  -v, --verbose   : some debug stuff\n");

  printf("\n  -o, --once      : runs program in 'single shot' mode (see README)\n");
  printf("  (if --once): --file=cidbcast.xml [port] [number] [name] [line]\n");

}

int main(int argc, char *argv[])
{
    struct sockaddr_in addr;
    int fd;
    int yes = 1;
    char* raw_message;
    char* message;
    char* tmp1_message;
    char* tmp2_message;
    struct timeb time;
    struct tm timeValues_r;
    struct tm* timeValues;
    FILE *fptr;
    long fsize;
    CID_Info cid_info;
    int modem_fd;
    int done = 0;
    int rv;
    int option_index = 0, c;
    int file_arg_found = 0;
    int single_shot = 0;
    char string[255];

    static struct option long_options[] = 
      {
	{"device", required_argument, 0, 'd'},
	{"udpport", required_argument, 0, 'u'},
	{"bcast", required_argument, 0, 'b'},
	{"file", required_argument, 0, 'f'},
	{"init", required_argument, 0, 'i'},
	{"help", no_argument, 0, 'h'},
	{"verbose", no_argument, 0, 'v'},
	{"once", no_argument, 0, 'o'},
	{0, 0, 0, 0}
      };

    memset(&cid_info, 0, sizeof(cid_info));

    /* Defaults */
    strcpy(xml_filename, "");
    strcpy(bcast_addr, "255.255.255.255");
    ipaddr_to_int(bcast_addr, &bcast_addr_hex);
    strcpy(serial_port, "/dev/ttyS0");
    strcpy(modem_init_string, "AT S7=45 S0=0 L0 V1 X4 &c1 E0 Q0 #CID=1 S41=1\r");
    udp_port = 6947;

    while (1)
    {
      c = getopt_long (argc, argv, "t:p:b:f:si:hvo",
		       long_options, &option_index);
      if (c == -1)
	break;
      
      switch (c) 
      {

      case 'd':
	strncpy(serial_port, optarg, sizeof(serial_port)-1);
	break;

      case 'i':
	strncpy(modem_init_string, optarg, sizeof(modem_init_string)-2);
	modem_init_string[strlen(modem_init_string)] = '\r';
	break;

      case 'u':
	udp_port = atoi(optarg);
	break;

      case 'b':
	strncpy(bcast_addr, optarg, sizeof(bcast_addr));
	if (ipaddr_to_int(bcast_addr, &bcast_addr_hex) < 0)
	{
	  printf("Error in Broadcast address %s\n",
		 bcast_addr);
	  exit(1);
	}
	break;

      case 'f':
	strncpy(xml_filename, optarg, sizeof(xml_filename)-1);
	file_arg_found = 1;
	break;

      case 'h':
	print_help(argv[0]);
	exit(0);
	break;	

      case 'v':
	verbose = 1;
	printf("Verbose mode enabled\n");
	break;

      case 'o':
	printf("Single shot mode enabled - sending UDP and exiting\n");
	single_shot = 1;
	break;

      default:
	print_help(argv[0]);
	exit(0);
      }
    }

    if (optind < argc) {
      if (single_shot)
	{
	  strcpy(serial_port, argv[optind++]);
	  strcpy(cid_info.number, argv[optind++]);
	  strcpy(cid_info.name, argv[optind++]);
	  strcpy(cid_info.ring_line, argv[optind++]);
	  strcpy(cid_info.date, "");
	  strcpy(cid_info.time, "");
	}
    }

    if (!file_arg_found)
    {
      print_help(argv[0]);
      return -1;
    }

    fptr=fopen(xml_filename, "r");
    if (fptr == NULL)
    {
      printf("Could not open xml file %s - exiting\n", xml_filename);
      exit(1);
    }	

    /* Goto end of file, get file size, rewind */
    (void)fseek(fptr, 0L, SEEK_END);
    fsize = ftell(fptr);
    (void)fseek(fptr, 0L, SEEK_SET);

    /* construct message */
    raw_message = (char *)malloc(fsize+1);
    tmp1_message = (char *)malloc(fsize+1024);
    tmp2_message = (char *)malloc(fsize+1024);
    message = (char *)malloc(fsize+1024);

    fread(raw_message, 1, fsize, fptr);
    if (verbose)
    {
      printf("raw_message (%lu):\n%s", fsize, raw_message);
    }
    fclose(fptr);

    if (!single_shot)
    {
      if (init_modem(&modem_fd) < 0)
      {
	printf("Error initializing modem %s - exiting\n", serial_port);
	exit(1);
      }
      else
      {
	printf("Modem %s initialized OK - waiting for RING\n", serial_port);
      }
    }

    while (!done)
    {
      if (!single_shot)
      {
	rv = waitfor_ring(modem_fd, "RING", &cid_info);
      }
      else
      {
	rv = 0;
      }

      if (rv == 0)
      {

	if (single_shot)
	{
	  done = 1;
	}

	ftime(&time);
	timeValues = localtime_r(&(time.time), &timeValues_r);

	replace(raw_message, tmp2_message, "%cid_number%", cid_info.number);
	replace(tmp2_message, tmp1_message, "%cid_name%", cid_info.name);
	replace(tmp1_message, tmp2_message, "%cid_date%", cid_info.date);
	replace(tmp2_message, tmp1_message, "%cid_time%", cid_info.time);
	replace(tmp1_message, tmp2_message, "%cid_ring_type%", cid_info.ring_line);
	replace(tmp2_message, tmp1_message, "%cid_port%", 
		get_tail(serial_port));
	sprintf(string, "%d", (int)time.time);
	replace(tmp1_message, tmp2_message, "%cid_time_stamp%", string);
	sprintf(string, "%s", tzname[1]);
	replace(tmp2_message, tmp1_message, "%cid_time_zone%", string);
	sprintf(string, "%d", timeValues->tm_isdst);
	replace(tmp1_message, tmp2_message, "%cid_daylight_saving%", string);
	sprintf(string, "%d", timeValues->tm_hour);
	replace(tmp2_message, tmp1_message, "%cid_time_hour%", string);
	sprintf(string, "%d", timeValues->tm_min);
	replace(tmp1_message, tmp2_message, "%cid_time_minute%", string);
	sprintf(string, "%d", timeValues->tm_sec);
	replace(tmp2_message, tmp1_message, "%cid_time_second%", string);
	sprintf(string, "%d", timeValues->tm_mday);
	replace(tmp1_message, tmp2_message, "%cid_time_day%", string);
	sprintf(string, "%d", timeValues->tm_mon + 1);
	replace(tmp2_message, tmp1_message, "%cid_time_month%", string);
	sprintf(string, "%d", timeValues->tm_year + 1900);
	replace(tmp1_message, tmp2_message, "%cid_time_year%", string);
	replace(tmp2_message, tmp1_message, "%cid_time_string%", ctime(&(time.time)));
	strcpy(message, tmp1_message);

	if (verbose)
	{
	  printf("\nmessage (%u):\n%s", strlen(message), message);
	}
	
	/* create what looks like an ordinary UDP socket */
	if ((fd=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP)) < 0) 
	{
	  perror("socket");
	  exit(1);
	}
	
	/* setsockopt */
	if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(int) ) < 0) 
	  {
	    perror("bind");
	    exit(1);
	  }
	
	/* set up destination address */
	memset(&addr,0,sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr=htonl(bcast_addr_hex);
	addr.sin_port=htons(udp_port);
	
	/* now just sendto() our destination! */
	if (sendto(fd,message,strlen(message),0,(struct sockaddr *) &addr,
		   sizeof(addr)) < 0)
	{
	  perror("sendto");
	}
	else
	{
	  printf("Sent UDP/XML packet to IP %s port %d\n", bcast_addr, udp_port);
	}
	close (fd);
      }
      else
	{
	  if (verbose)
	    {
	      printf("Error getting CID_info, dont send anything\n");
	    }
	}
    }

    if (!single_shot)
    {
      close_modem(modem_fd);
    }

    return 0;
}


