/* Ken Bass (kbass@kenbass.com) 09/13/2003
**
** MythTV UDP Relay
**
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <libxml/debugXML.h>
#include <libxml/HTMLtree.h>
#include <libxml/xmlIO.h>
#include <libxml/DOCBparser.h>
#include <libxml/xinclude.h>
#include <libxml/catalog.h>
#include <libxslt/xslt.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>

#define UDP_RCV_BUFSIZE (64*1024)

int verbose = 0;
char xml_filename[255];

typedef struct 
{
  int fd;
  int port;
  struct sockaddr_in addr;
  char bcast_addr[16];
  int bcast_addr_hex;
} SOCKET_HANDLE;

typedef struct 
{

  SOCKET_HANDLE rcv_sock_handle;
  SOCKET_HANDLE snd_sock_handle;
  char udp_rcv_buffer[UDP_RCV_BUFSIZE];
  char xml_output_buffer[UDP_RCV_BUFSIZE];
  char xslt_filename[255];
} GLOBALS;

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

int setup_rcv_socket(SOCKET_HANDLE* sock, int port)
{
  int yes = 1;
  sock->port = port;
  
  /* create what looks like an ordinary UDP socket */
  if ( (sock->fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0 )
    {
      perror("socket");
      return -1;
    }
  
  /* set up destination address */
  sock->addr.sin_family = AF_INET;
  sock->addr.sin_addr.s_addr = htonl(INADDR_ANY);
  sock->addr.sin_port = htons(port);
  
  /* set up for reusable broadcast port */
  if (setsockopt(sock->fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int) ) < 0)
    {
      perror("setsockopt");
      return -2;
    }
  
  /* bind to receive address */
  if ( bind(sock->fd, (struct sockaddr*) &(sock->addr), sizeof(sock->addr)) < 0 )
    {
      perror("bind");
      return -3;
    }
  
  return 0;
}

int setup_snd_socket(SOCKET_HANDLE* sock, int port, char *addr)
{
  int yes = 1;

  strcpy(sock->bcast_addr, addr);
  if (ipaddr_to_int(sock->bcast_addr, &sock->bcast_addr_hex) < 0)
  {
    printf("Error in Broadcast address %s\n",
	   sock->bcast_addr);
    return -1;
  }

  sock->port = port;

  /* create what looks like an ordinary UDP socket */
  if ( (sock->fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0 )
    {
      perror("socket");
      return -1;
    }
  
  /* set up destination address */
  sock->addr.sin_family = AF_INET;
  sock->addr.sin_addr.s_addr = htonl(sock->bcast_addr_hex);
  sock->addr.sin_port = htons(sock->port);
  
  /* set up for reusable broadcast port */
  if (setsockopt(sock->fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int) ) < 0)
    {
      perror("setsockopt");
      return -2;
    }

  /* setsockopt */
  if (setsockopt(sock->fd, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(int) ) < 0) 
    {
      perror("so_broadcast");
      return -3;
    }
 
  return 0;
}

int send_xml_output(GLOBALS *globals, SOCKET_HANDLE* sock)
{
  int rc;

  /* now just sendto() our destination! */
  rc = sendto(sock->fd,
	      globals->xml_output_buffer,
	      strlen(globals->xml_output_buffer),0,(struct sockaddr *) &sock->addr,
	      sizeof(sock->addr));
  if (rc < 0)
    {
      perror("sendto");
      return -1;
    }
  else
    {
      if (verbose)
      {
	printf("Sent UDP/XML packet to IP %s port %d (%d bytes)\n", 
	       sock->bcast_addr, sock->port, rc);
      }
    }

  return 0;
  
} 

/* Transfrom received XML data using XSL */
int process_udp_xml(GLOBALS *globals)
{
  xsltStylesheetPtr cur = NULL;
  xmlDocPtr doc, res;
  xmlChar *xml_output;
  int xml_output_len;
  int rc;

  xmlSubstituteEntitiesDefault(1);
  xmlLoadExtDtdDefaultValue = 1;
  cur = xsltParseStylesheetFile((const xmlChar *)globals->xslt_filename);
  if (cur == NULL)
    {
      xsltCleanupGlobals();
      xmlCleanupParser();
      return -1;
    }

  doc = xmlParseMemory(globals->udp_rcv_buffer, UDP_RCV_BUFSIZE);
  if (doc == NULL)
    {
      xsltFreeStylesheet(cur);
      xsltCleanupGlobals();
      xmlCleanupParser();
      return -1;
    }
  res = xsltApplyStylesheet(cur, doc, NULL);
  if (res == NULL)
    {
      xmlFreeDoc(res);
      xsltFreeStylesheet(cur);
      xsltCleanupGlobals();
      xmlCleanupParser();
      return -1;
    }

  rc = xsltSaveResultToString(&xml_output,
				&xml_output_len,
				res,
				cur);

  /* If tranformation is empty - error. Its possible the incoming
  ** XML was not an event the XSLT handles */
  if ((rc < 0) || (xml_output_len == 0))
  {
    
    xsltFreeStylesheet(cur);
    xmlFreeDoc(res);
    xmlFreeDoc(doc);
    
    xsltCleanupGlobals();
    xmlCleanupParser();
    
    return -1;
  }

  strncpy(globals->xml_output_buffer, xml_output, UDP_RCV_BUFSIZE-1);
  
  xmlFree(xml_output);

  xsltFreeStylesheet(cur);
  xmlFreeDoc(res);
  xmlFreeDoc(doc);
  
  xsltCleanupGlobals();
  xmlCleanupParser();

  return 0;
}

int waitfor_udp(GLOBALS *globals, SOCKET_HANDLE* sock)
{
  int  nbytes;       /* Number of bytes read */
  fd_set udp_rd_set;
  int rv;

  FD_ZERO(&udp_rd_set);
  FD_SET (sock->fd, &udp_rd_set);

  memset (globals->udp_rcv_buffer, 0, UDP_RCV_BUFSIZE);

  rv = select(sock->fd+1, &udp_rd_set, NULL, NULL, NULL);
  if (rv < 0)
  {
    printf("Select failed\n");
    return -1;
  }

  if (FD_ISSET(sock->fd, &udp_rd_set))
    {
      nbytes = read(sock->fd, globals->udp_rcv_buffer, UDP_RCV_BUFSIZE);
      if (nbytes <= 0)
      {
	return -1;
      }
      else
      {
	if (verbose)
	{
	  printf("got UDP data (%d bytes)\n", nbytes);
	}
      }
    }

  return 0;
}

void print_help(char *progname)
{
  printf("\nUsage: %s [OPTION]\n", progname);
  printf("A caller id UDP broadcast utility for MythTV notify.\n\n");
  printf("  -i, --udpport_in  : UDP port to monitor (--udpport_in=6947)\n");
  printf("  -o, --udpport_out : UDP port to monitor (--udpport_out=6948)\n");
  printf("  -b, --bcast     : UDP broadcast address (--bcast=255.255.255.255)\n");
  printf("  -x, --xslfile   : XSL file to use as a template (--xslfile=cid.xml)\n");
  printf("  -v, --verbose   : some verbose debug stuff\n");
  printf("\nAn XSL (XSLT) file is required - it is used to transfrom the incoming\n");
  printf("XML to the output XML (mythnotify)\n");
}

int main(int argc, char *argv[])
{
  int file_arg_found = 0;
  static GLOBALS globals;
  int done = 0;
  int option_index = 0, c;
  int rcv_udp_port;
  int snd_udp_port;
  char bcast_addr[16];
  int bcast_addr_hex;

  static struct option long_options[] = 
  {
    {"udpport_in", optional_argument, 0, 'i'},
    {"udpport_out", optional_argument, 0, 'o'},
    {"bcast", optional_argument, 0, 'b'},
    {"xslfile", required_argument, 0, 'x'},
    {"help", no_argument, 0, 'h'},
    {"verbose", no_argument, 0, 'v'},
    {0, 0, 0, 0}
  };

  /* Defaults */
  strcpy(bcast_addr, "255.255.255.255");
  ipaddr_to_int(bcast_addr, &bcast_addr_hex);
  rcv_udp_port = 6947;
  snd_udp_port = 6948;
  strcpy(globals.xslt_filename, "");

  while (1) 
  {
    c = getopt_long (argc, argv, "i:o:b:x:hv",
		     long_options, &option_index);
    if (c == -1)
      break;
    
    switch (c) 
    {
      
      case 'i':
	rcv_udp_port = atoi(optarg);
	break;

      case 'o':
	snd_udp_port = atoi(optarg);
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

      case 'x':
	strncpy(globals.xslt_filename, optarg, sizeof(globals.xslt_filename)-1);
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

      default:
	print_help(argv[0]);
	exit(0);
    }
  }

  if (!file_arg_found)
  {
    printf("No xslfile provided; --xslfile=file.xsl arg required\n");
    return -1;
  }

  if (rcv_udp_port == snd_udp_port)
    {
      printf("UDP input and output ports cannot be the same\n");
      return -1;
    }

  if (setup_rcv_socket(&globals.rcv_sock_handle, rcv_udp_port) < 0)
  {
    printf("error in setup_rcv_sockets\n");
    return -1;
  }

  if (setup_snd_socket(&globals.snd_sock_handle, 
		       snd_udp_port, bcast_addr) < 0)
  {
    printf("error in setup_snd_sockets\n");
    return -1;
  }

  done = 0;
  while (!done)
  {
    if (waitfor_udp(&globals, &globals.rcv_sock_handle) >= 0)
      {

	if (process_udp_xml(&globals) >= 0)
	{

	  if (verbose)
	  {
	    printf("%s", globals.xml_output_buffer);
	  }
	  
	  send_xml_output(&globals, &globals.snd_sock_handle);
	}
      }
  }

  return 0;
}
