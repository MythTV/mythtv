/* Ken Bass (kbass@kenbass.com) 09/13/2003
**
** MythTV OSD I/F utility
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

int verbose = 0;
char xml_filename[255];
char bcast_addr[16];
int bcast_addr_hex=-1;
int udp_port;

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

char *replace(char *in_string, char *out_string, char* key_string, char *replace_string)
{
  int str_index, outstring_idx, key_idx, end, replace_len, key_len, cpy_len;
  char *c;
  
  if ((c = (char *) strstr(in_string, key_string)) == NULL)
  {
    strcpy(out_string,in_string);

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
    if((c = (char *) strstr(in_string+str_index, key_string)) != NULL)
    {
      key_idx = c - in_string;
    }
  }

  /* Copy remaining characters from the right of last matched pattern */
  strcpy(out_string+outstring_idx, in_string+str_index);
  
  return out_string; 
}

void print_help(char *progname)
{
  printf("\nUsage: %s --file xyz.xml [OPTION]\n", progname);
  printf("A utility to put items on the mythTV OSD.\n\n");
  printf("  --file           : (required) XML file to use as a template (--file=cid.xml)\n");
  printf("  --name=\"value\"   : (optional) 'name' can be any word\n");
  printf("  --udpport        : (optional) UDP port to sent to (--udpport=6948)\n");
  printf("  --bcastaddr      : (optional) IP address to send to (--bcastaddr=127.0.0.1)\n");
  printf("  --verbose        : (optional) some debug stuff\n");
  printf("  --help           : (optional) this text\n");
  printf("\nAn XML file is required - it will be read into memory. Then a\n");
  printf("each %%name%% item will be replaced with the given 'value'.\n");
}

int get_arg_pair(char *arg, char *arg_name, char *arg_value)
{

  char *c, *n;

  n = arg_name;

  if (strncmp(arg, "--", 2) != 0)
    {
      return -1;
    }

  c = arg; 
  c+=2; /* Skip leading '--' */

  while (*c && ((*c != '=') && (*c!=' ')))
    {
      *n = *c;
      n++;
      c++;
    }
  *n=0;
  if (*c!='=')
    {
      *arg_value = 0;
	return 0;
    }
  c++; /* Skip = */
  n = arg_value;
  while (*c && (*c != '='))
    {
      *n = *c;
      n++;
      c++;
    }
  *n=0;

  return 0;
}

int main(int argc, char *argv[])
{
  int i;
  int file_arg_found = 0;
  char arg_name[255];
  char arg_value[255];
  char esc_arg_name[257];
  FILE *fptr;
  long fsize;
  char* raw_message;
  char* src_message;
  char* tmp_message;
  struct sockaddr_in addr;
  int fd, rc;
  int yes = 1;

  /* Defaults */
  strcpy(bcast_addr, "255.255.255.255");
  ipaddr_to_int(bcast_addr, &bcast_addr_hex);
  udp_port = 6948;

  /* First pass - get '--file=' argument */
  for(i=1; i < argc; i++)
  {
    if (strncmp(argv[i], "--file=", 7) == 0)
      {
	get_arg_pair(argv[i], arg_name, arg_value);
	strncpy(xml_filename, arg_value, sizeof(xml_filename)-1);
	file_arg_found = 1;
	break;
      }
  }

  if (!file_arg_found)
      {
	printf("--file=xyz.xml argument required\n");
	exit(0);
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
    tmp_message = (char *)malloc(fsize+1024);
    src_message = (char *)malloc(fsize+1024);

    fread(raw_message, 1, fsize, fptr);
    fclose(fptr);
    strncpy(src_message, raw_message, fsize+1024-1);


  for(i=1; i < argc; i++)
  {
    if (verbose)
    {
      printf("arg%d %s\n", i, argv[i]);
    }
    if (strncmp(argv[i], "--", 2) == 0)
      {
	get_arg_pair(argv[i], arg_name, arg_value);
	if (strcmp(arg_name, "help") == 0)
	  {
	    print_help(argv[0]);
	    exit(0);
	  }
	if (strcmp(arg_name, "file") == 0)
	  {
	    continue;
	  }
	if (strcmp(arg_name, "verbose") == 0)
	  {
	    verbose=1;
	    continue;
	  }
	if (strcmp(arg_name, "udpport") == 0)
	  {
	    udp_port = atoi(arg_value);
	    continue;
	  }
	if (strcmp(arg_name, "bcastaddr") == 0)
	  {
	    strcpy(bcast_addr, arg_value);
	    ipaddr_to_int(bcast_addr, &bcast_addr_hex);
	    continue;
	  }

        if (verbose)
        {
	  printf("arg_name %s : arg_value %s\n", arg_name, arg_value);
        }

	sprintf(esc_arg_name, "%%%s%%", arg_name);
	replace(src_message, tmp_message, esc_arg_name, arg_value);
	strncpy(src_message, tmp_message,  fsize+1024-1);
      }
    else
      {
	printf("bad arg\n");
      }
  }

  if (verbose)
  {
    printf("output:\n%s\n", src_message);
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
rc = sendto(fd,src_message,strlen(src_message),0,(struct sockaddr *) &addr,
	     sizeof(addr));
 if (rc < 0)
    {
      perror("sendto");
    }
  else
    {
      printf("Sent UDP/XML packet to IP %s port %d (%d bytes)\n", bcast_addr, udp_port, rc);
    }

  return 0;
}
