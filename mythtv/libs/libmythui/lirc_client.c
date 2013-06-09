/* NOTE: Extracted from LIRC release 0.8.4a -- dtk */
/*       Updated to LIRC release 0.8.6 */

/****************************************************************************
 ** lirc_client.c ***********************************************************
 ****************************************************************************
 *
 * lirc_client - common routines for lircd clients
 *
 * Copyright (C) 1998 Trent Piepho <xyzzy@u.washington.edu>
 * Copyright (C) 1998 Christoph Bartelmus <lirc@bartelmus.de>
 *
 * System wide LIRCRC support by Michal Svec <rebel@atrey.karlin.mff.cuni.cz>
 */ 

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "lirc_client.h"


/* internal defines */
#define MAX_INCLUDES 10
#define LIRC_READ 255
#define LIRC_PACKET_SIZE 255
/* three seconds */
#define LIRC_TIMEOUT 3

/* internal data structures */
struct filestack_t {
	FILE *file;
	char *name;
	int line;
	struct filestack_t *parent;
};

enum packet_state
{
	P_BEGIN,
	P_MESSAGE,
	P_STATUS,
	P_DATA,
	P_N,
	P_DATA_N,
	P_END
};

/* internal functions */
static void lirc_printf(const struct lirc_state*, const char *format_str, ...);
static void lirc_perror(const struct lirc_state*, const char *s);
static int lirc_readline(const struct lirc_state *state, char **line,FILE *f);
static char *lirc_trim(char *s);
static char lirc_parse_escape(const struct lirc_state *state, char **s,const char *name,int line);
static void lirc_parse_string(const struct lirc_state *state, char *s,const char *name,int line);
static void lirc_parse_include(char *s,const char *name,int line);
static int lirc_mode(
             const struct lirc_state *state,
             const char *token,const char *token2,char **mode,
		     struct lirc_config_entry **new_config,
		     struct lirc_config_entry **first_config,
		     struct lirc_config_entry **last_config,
		     int (check)(char *s),
		     const char *name,int line);
/*
  lircrc_config relies on this function, hence don't make it static
  but it's not part of the official interface, so there's no guarantee
  that it will stay available in the future
*/
static unsigned int lirc_flags(const struct lirc_state *state, char *string);
static char *lirc_getfilename(const struct lirc_state *state,
							  const char *file,
							  const char *current_file);
static FILE *lirc_open(const struct lirc_state *state,
					   const char *file, const char *current_file,
					   char **full_name);
static struct filestack_t *stack_push(const struct lirc_state *state, struct filestack_t *parent);
static struct filestack_t *stack_pop(struct filestack_t *entry);
static void stack_free(struct filestack_t *entry);
static int lirc_readconfig_only_internal(const struct lirc_state *state,
                                         const char *file,
                                         struct lirc_config **config,
                                         int (check)(char *s),
                                         char **full_name,
                                         char **sha_bang);
static char *lirc_startupmode(const struct lirc_state *state,
							  struct lirc_config_entry *first);
static void lirc_freeconfigentries(struct lirc_config_entry *first);
static void lirc_clearmode(struct lirc_config *config);
static char *lirc_execute(const struct lirc_state *state,
			  struct lirc_config *config,
			  struct lirc_config_entry *scan);
static int lirc_iscode(struct lirc_config_entry *scan, char *remote,
		       char *button,int rep);
static int lirc_code2char_internal(const struct lirc_state *state,
				   struct lirc_config *config,char *code,
				   char **string, char **prog);
static const char *lirc_read_string(const struct lirc_state *state, int fd);
static int lirc_identify(const struct lirc_state *state, int sockfd);

static int lirc_send_command(const struct lirc_state *state, int sockfd, const char *command, char *buf, size_t *buf_len, int *ret_status);

static void lirc_printf(const struct lirc_state *state, const char *format_str, ...)
{
	va_list ap;  
	
	if(state && !state->lirc_verbose) return;
	
	va_start(ap,format_str);
	vfprintf(stderr,format_str,ap);
	va_end(ap);
}

static void lirc_perror(const struct lirc_state *state, const char *s)
{
	if(!state->lirc_verbose) return;

	perror(s);
}

struct lirc_state *lirc_init(const char *lircrc_root_file,
							 const char *lircrc_user_file,
							 const char *prog,
							 const char *lircd,
							 int verbose)
{
	struct sockaddr_un addr;

	/* connect to lircd */

	if(lircrc_root_file==NULL || lircrc_user_file == NULL || prog==NULL)
	{
		lirc_printf(NULL, "%s: lirc_init invalid params\n",prog);
		return(NULL);
	}

	struct lirc_state *state = (struct lirc_state *) calloc(1, sizeof(struct lirc_state));
	if(state==NULL)
	{
		lirc_printf(NULL, "%s: out of memory\n",prog);
		return(NULL);
	}
	state->lirc_lircd=-1;
	state->lirc_verbose=verbose;

	state->lircrc_root_file=strdup(lircrc_root_file);
	if(state->lircrc_root_file==NULL)
	{
		lirc_printf(state, "%s: out of memory\n",prog);
		lirc_deinit(state);
		return(NULL);
	}
	
	state->lircrc_user_file=strdup(lircrc_user_file);
	if(state->lircrc_user_file==NULL)
	{
		lirc_printf(state, "%s: out of memory\n",prog);
		lirc_deinit(state);
		return(NULL);
	}
	
	state->lirc_prog=strdup(prog);
	if(state->lirc_prog==NULL)
	{
		lirc_printf(state, "%s: out of memory\n",prog);
		lirc_deinit(state);
		return(NULL);
	}

	if (lircd)
	{
		addr.sun_family=AF_UNIX;
		strncpy(addr.sun_path,lircd,sizeof(addr.sun_path)-1);
		state->lirc_lircd=socket(AF_UNIX,SOCK_STREAM,0);
		if(state->lirc_lircd==-1)
		{
			lirc_printf(state, "%s: could not open socket\n",state->lirc_prog);
			lirc_perror(state, state->lirc_prog);
			lirc_deinit(state);
			return(NULL);
		}
		if(connect(state->lirc_lircd,(struct sockaddr *)&addr,sizeof(addr))==-1)
		{
			close(state->lirc_lircd);
			lirc_printf(state, "%s: could not connect to socket\n",state->lirc_prog);
			lirc_perror(state, state->lirc_prog);
			lirc_deinit(state);
			return(NULL);
		}
	}

	return(state);
}

int lirc_deinit(struct lirc_state *state)
{
	int ret = LIRC_RET_SUCCESS;
	if (state==NULL)
		return ret;
	if(state->lircrc_root_file!=NULL)
	{
		free(state->lircrc_root_file);
		state->lircrc_root_file=NULL;
	}
	if(state->lircrc_user_file!=NULL)
	{
		free(state->lircrc_user_file);
		state->lircrc_user_file=NULL;
	}
	if(state->lirc_prog!=NULL)
	{
		free(state->lirc_prog);
		state->lirc_prog=NULL;
	}
	if(state->lirc_buffer!=NULL)
	{
		free(state->lirc_buffer);
		state->lirc_buffer=NULL;
	}
	if (state->lirc_lircd!=-1)
		ret = close(state->lirc_lircd);
	free(state);
	return ret;
}

static int lirc_readline(const struct lirc_state *state, char **line,FILE *f)
{
	char *newline,*ret,*enlargeline;
	int len;
	
	newline=(char *) malloc(LIRC_READ+1);
	if(newline==NULL)
	{
		lirc_printf(state, "%s: out of memory\n",state->lirc_prog);
		return(-1);
	}
	len=0;
	while(1)
	{
		ret=fgets(newline+len,LIRC_READ+1,f);
		if(ret==NULL)
		{
			if(feof(f) && len>0)
			{
				*line=newline;
			}
			else
			{
				free(newline);
				*line=NULL;
			}
			return(0);
		}
		len=strlen(newline);
		if(newline[len-1]=='\n')
		{
			newline[len-1]=0;
			*line=newline;
			return(0);
		}
		
		enlargeline=(char *) realloc(newline,len+1+LIRC_READ);
		if(enlargeline==NULL)
		{
			free(newline);
			lirc_printf(state, "%s: out of memory\n",state->lirc_prog);
			return(-1);
		}
		newline=enlargeline;
	}
}

static char *lirc_trim(char *s)
{
	int len;
	
	while(s[0]==' ' || s[0]=='\t') s++;
	len=strlen(s);
	while(len>0)
	{
		len--;
		if(s[len]==' ' || s[len]=='\t') s[len]=0;
		else break;
	}
	return(s);
}

/* parse standard C escape sequences + \@,\A-\Z is ^@,^A-^Z */

static char lirc_parse_escape(const struct lirc_state *state, char **s,const char *name,int line)
{

	char c;
	unsigned int i,overflow,count;
	int digits_found,digit;

	c=**s;
	(*s)++;
	switch(c)
	{
	case 'a':
		return('\a');
	case 'b':
		return('\b');
	case 'e':
#if 0
	case 'E': /* this should become ^E */
#endif
		return(033);
	case 'f':
		return('\f');
	case 'n':
		return('\n');
	case 'r':
		return('\r');
	case 't':
		return('\t');
	case 'v':
		return('\v');
	case '\n':
		return(0);
	case 0:
		(*s)--;
		return 0;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
		i=c-'0';
		count=0;
		
		while(++count<3)
		{
			c=*(*s)++;
			if(c>='0' && c<='7')
			{
				i=(i << 3)+c-'0';
			}
			else
			{
				(*s)--;
				break;
			}
		}
		if(i>(1<<CHAR_BIT)-1)
		{
			i&=(1<<CHAR_BIT)-1;
			lirc_printf(state, "%s: octal escape sequence "
				    "out of range in %s:%d\n",state->lirc_prog,
				    name,line);
		}
		return((char) i);
	case 'x':
		{
			i=0;
			overflow=0;
			digits_found=0;
			for (;;)
			{
				c = *(*s)++;
				if(c>='0' && c<='9')
					digit=c-'0';
				else if(c>='a' && c<='f')
					digit=c-'a'+10;
				else if(c>='A' && c<='F')
					digit=c-'A'+10;
				else
				{
					(*s)--;
					break;
				}
				overflow|=i^(i<<4>>4);
				i=(i<<4)+digit;
				digits_found=1;
			}
			if(!digits_found)
			{
				lirc_printf(state, "%s: \\x used with no "
					    "following hex digits in %s:%d\n",
					    state->lirc_prog,name,line);
			}
			if(overflow || i>(1<<CHAR_BIT)-1)
			{
				i&=(1<<CHAR_BIT)-1;
				lirc_printf(state, "%s: hex escape sequence out "
					    "of range in %s:%d\n",
					    state->lirc_prog,name,line);
			}
			return((char) i);
		}
	default:
		if(c>='@' && c<='Z') return(c-'@');
		return(c);
	}
}

static void lirc_parse_string(const struct lirc_state *state, char *s,const char *name,int line)
{
	char *t;

	t=s;
	while(*s!=0)
	{
		if(*s=='\\')
		{
			s++;
			*t=lirc_parse_escape(state, &s,name,line);
			t++;
		}
		else
		{
			*t=*s;
			s++;
			t++;
		}
	}
	*t=0;
}

static void lirc_parse_include(char *s,const char *name,int line)
{
	char last;
	size_t len;
	
	len=strlen(s);
	if(len<2)
	{
		return;
	}
	last=s[len-1];
	if(*s!='"' && *s!='<')
	{
		return;
	}
	if(*s=='"' && last!='"')
	{
		return;
	}
	else if(*s=='<' && last!='>')
	{
		return;
	}
	s[len-1]=0;
	memmove(s, s+1, len-2+1); /* terminating 0 is copied */
}

int lirc_mode(const struct lirc_state *state,
          const char *token,const char *token2,char **mode,
	      struct lirc_config_entry **new_config,
	      struct lirc_config_entry **first_config,
	      struct lirc_config_entry **last_config,
	      int (check)(char *s),
	      const char *name,int line)
{
	struct lirc_config_entry *new_entry;
	
	new_entry=*new_config;
	if(strcasecmp(token,"begin")==0)
	{
		if(token2==NULL)
		{
			if(new_entry==NULL)
			{
				new_entry=(struct lirc_config_entry *)
				malloc(sizeof(struct lirc_config_entry));
				if(new_entry==NULL)
				{
					lirc_printf(state, "%s: out of memory\n",
						    state->lirc_prog);
					return(-1);
				}
				else
				{
					new_entry->prog=NULL;
					new_entry->code=NULL;
					new_entry->rep_delay=0;
					new_entry->rep=0;
					new_entry->config=NULL;
					new_entry->change_mode=NULL;
					new_entry->flags=none;
					new_entry->mode=NULL;
					new_entry->next_config=NULL;
					new_entry->next_code=NULL;
					new_entry->next=NULL;

					*new_config=new_entry;
				}
			}
			else
			{
				lirc_printf(state, "%s: bad file format, "
					    "%s:%d\n",state->lirc_prog,name,line);
				return(-1);
			}
		}
		else
		{
			if(new_entry==NULL && *mode==NULL)
			{
				*mode=strdup(token2);
				if(*mode==NULL)
				{
					return(-1);
				}
			}
			else
			{
				lirc_printf(state, "%s: bad file format, "
					    "%s:%d\n",state->lirc_prog,name,line);
				return(-1);
			}
		}
	}
	else if(strcasecmp(token,"end")==0)
	{
		if(token2==NULL)
		{
			if(new_entry!=NULL)
			{
#if 0
				if(new_entry->prog==NULL)
				{
					lirc_printf(state, "%s: prog missing in "
						    "config before line %d\n",
						    state->lirc_prog,line);
					lirc_freeconfigentries(new_entry);
					*new_config=NULL;
					return(-1);
				}
				if(strcasecmp(new_entry->prog,state->lirc_prog)!=0)
				{
					lirc_freeconfigentries(new_entry);
					*new_config=NULL;
					return(0);
				}
#endif
				new_entry->next_code=new_entry->code;
				new_entry->next_config=new_entry->config;
				if(*last_config==NULL)
				{
					*first_config=new_entry;
					*last_config=new_entry;
				}
				else
				{
					(*last_config)->next=new_entry;
					*last_config=new_entry;
				}
				*new_config=NULL;

				if(*mode!=NULL) 
				{
					new_entry->mode=strdup(*mode);
					if(new_entry->mode==NULL)
					{
						lirc_printf(state, "%s: out of "
							    "memory\n",
							    state->lirc_prog);
						return(-1);
					}
				}

				if(check!=NULL &&
				   new_entry->prog!=NULL &&
				   strcasecmp(new_entry->prog,state->lirc_prog)==0)
				{					
					struct lirc_list *list;

					list=new_entry->config;
					while(list!=NULL)
					{
						if(check(list->string)==-1)
						{
							return(-1);
						}
						list=list->next;
					}
				}
				
				if (new_entry->rep_delay==0 &&
				    new_entry->rep>0)
				{
					new_entry->rep_delay=new_entry->rep-1;
				}
			}
			else
			{
				lirc_printf(state, "%s: %s:%d: 'end' without "
					    "'begin'\n",state->lirc_prog,name,line);
				return(-1);
			}
		}
		else
		{
			if(*mode!=NULL)
			{
				if(new_entry!=NULL)
				{
					lirc_printf(state, "%s: %s:%d: missing "
						    "'end' token\n",state->lirc_prog,
						    name,line);
					return(-1);
				}
				if(strcasecmp(*mode,token2)==0)
				{
					free(*mode);
					*mode=NULL;
				}
				else
				{
					lirc_printf(state, "%s: \"%s\" doesn't "
						    "match mode \"%s\"\n",
						    state->lirc_prog,token2,*mode);
					return(-1);
				}
			}
			else
			{
				lirc_printf(state, "%s: %s:%d: 'end %s' without "
					    "'begin'\n",state->lirc_prog,name,line,
					    token2);
				return(-1);
			}
		}
	}
	else
	{
		lirc_printf(state, "%s: unknown token \"%s\" in %s:%d ignored\n",
			    state->lirc_prog,token,name,line);
	}
	return(0);
}

unsigned int lirc_flags(const struct lirc_state *state, char *string)
{
	char *s, *strtok_state = NULL;
	unsigned int flags;

	flags=none;
	s=strtok_r(string," \t|",&strtok_state);
	while(s)
	{
		if(strcasecmp(s,"once")==0)
		{
			flags|=once;
		}
		else if(strcasecmp(s,"quit")==0)
		{
			flags|=quit;
		}
		else if(strcasecmp(s,"mode")==0)
		{
			flags|=mode;
		}
		else if(strcasecmp(s,"startup_mode")==0)
		{
			flags|=startup_mode;
		}
		else if(strcasecmp(s,"toggle_reset")==0)
		{
			flags|=toggle_reset;
		}
		else
		{
			lirc_printf(state, "%s: unknown flag \"%s\"\n",state->lirc_prog,s);
		}
		s=strtok_r(NULL," \t",&strtok_state);
	}
	return(flags);
}

static char *lirc_getfilename(const struct lirc_state *state,
							  const char *file,
							  const char *current_file)
{
	const char *home;
	char *filename;

	if(file==NULL)
	{
		home=getenv("HOME");
		if(home==NULL)
		{
			home="/";
		}
		filename=(char *) malloc(strlen(home)+1+
					 strlen(state->lircrc_user_file)+1);
		if(filename==NULL)
		{
			lirc_printf(state, "%s: out of memory\n",state->lirc_prog);
			return NULL;
		}
		strcpy(filename,home);
		if(strlen(home)>0 && filename[strlen(filename)-1]!='/')
		{
			strcat(filename,"/");
		}
		strcat(filename,state->lircrc_user_file);
	}
	else if(strncmp(file, "~/", 2)==0)
	{
		home=getenv("HOME");
		if(home==NULL)
		{
			home="/";
		}
		filename=(char *) malloc(strlen(home)+strlen(file)+1);
		if(filename==NULL)
		{
			lirc_printf(state, "%s: out of memory\n",state->lirc_prog);
			return NULL;
		}
		strcpy(filename,home);
		strcat(filename,file+1);
	}
	else if(file[0]=='/' || current_file==NULL)
	{
		/* absulute path or root */
		filename=strdup(file);
		if(filename==NULL)
		{
			lirc_printf(state, "%s: out of memory\n",state->lirc_prog);
			return NULL;
		}
	}
	else
	{
		/* get path from parent filename */
		int pathlen = strlen(current_file);
		while (pathlen>0 && current_file[pathlen-1]!='/')
			pathlen--;
		filename=(char *) malloc(pathlen+strlen(file)+1);
		if(filename==NULL)
		{
			lirc_printf(state, "%s: out of memory\n",state->lirc_prog);
			return NULL;
		}
		memcpy(filename,current_file,pathlen);
		filename[pathlen]=0;
		strcat(filename,file);
	}
	return filename;
}

static FILE *lirc_open(const struct lirc_state *state,
					   const char *file, const char *current_file,
                       char **full_name)
{
	FILE *fin;
	char *filename;
	
	filename=lirc_getfilename(state, file, current_file);
	if(filename==NULL)
	{
		return NULL;
	}

	fin=fopen(filename,"r");
	if(fin==NULL && (file!=NULL || errno!=ENOENT))
	{
		lirc_printf(state, "%s: could not open config file %s\n",
			    state->lirc_prog,filename);
		lirc_perror(state, state->lirc_prog);
	}
	else if(fin==NULL)
	{
		fin=fopen(state->lircrc_root_file,"r");
		if(fin==NULL && errno!=ENOENT)
		{
			lirc_printf(state, "%s: could not open config file %s\n",
				    state->lirc_prog,state->lircrc_root_file);
			lirc_perror(state, state->lirc_prog);
		}
		else if(fin==NULL)
		{
			lirc_printf(state, "%s: could not open config files "
				    "%s and %s\n",
				    state->lirc_prog,filename,state->lircrc_root_file);
			lirc_perror(state, state->lirc_prog);
		}
		else
		{
			free(filename);
			filename = strdup(state->lircrc_root_file);
			if(filename==NULL)
			{
				fclose(fin);
				lirc_printf(state, "%s: out of memory\n",state->lirc_prog);
				return NULL;
			}
		}
	}
	if(full_name && fin!=NULL)
	{
		*full_name = filename;
	}
	else
	{
		free(filename);
	}
	return fin;
}

static struct filestack_t *stack_push(const struct lirc_state *state, struct filestack_t *parent)
{
	struct filestack_t *entry;
	entry = malloc(sizeof(struct filestack_t));
	if (entry == NULL)
	{
		lirc_printf(state, "%s: out of memory\n",state->lirc_prog);
		return NULL;
	}
	entry->file = NULL;
	entry->name = NULL;
	entry->line = 0;
	entry->parent = parent;
	return entry;
}

static struct filestack_t *stack_pop(struct filestack_t *entry)
{
	struct filestack_t *parent = NULL;
	if (entry)
	{
		parent = entry->parent;
		if (entry->name)
			free(entry->name);
		free(entry);
	}
	return parent;
}

static void stack_free(struct filestack_t *entry)
{
	while (entry)
	{
		entry = stack_pop(entry);
	}
}

int lirc_readconfig(const struct lirc_state *state,
                    const char *file,
                    struct lirc_config **config,
                    int (check)(char *s))
{
	struct sockaddr_un addr;
	int sockfd = -1;
	char *sha_bang, *filename;
	const char *sha_bang2;
	char *command;
	unsigned int ret;
	
	filename = NULL;
	sha_bang = NULL;
	if(lirc_readconfig_only_internal(state,file,config,check,&filename,&sha_bang)==-1)
	{
		return -1;
	}
	
	if(sha_bang == NULL)
	{
		goto lirc_readconfig_compat;
	}
	
	/* connect to lircrcd */

	addr.sun_family=AF_UNIX;
	if(lirc_getsocketname(filename, addr.sun_path, sizeof(addr.sun_path))>sizeof(addr.sun_path))
	{
		lirc_printf(state, "%s: WARNING: file name too long\n", state->lirc_prog);
		goto lirc_readconfig_compat;
	}
	sockfd=socket(AF_UNIX,SOCK_STREAM,0);
	if(sockfd==-1)
	{
		lirc_printf(state, "%s: WARNING: could not open socket\n",state->lirc_prog);
		lirc_perror(state, state->lirc_prog);
		goto lirc_readconfig_compat;
	}
	if(connect(sockfd, (struct sockaddr *)&addr, sizeof(addr))!=-1)
	{
		if(sha_bang!=NULL) free(sha_bang);
		(*config)->sockfd=sockfd;
		free(filename);
		
		/* tell daemon state->lirc_prog */
		if(lirc_identify(state, sockfd) == LIRC_RET_SUCCESS)
		{
			/* we're connected */
			return 0;
		}
		close(sockfd);
		lirc_freeconfig(*config);
		return -1;
	}
	close(sockfd);
	sockfd = -1;
	
	/* launch lircrcd */
	sha_bang2=sha_bang!=NULL ? sha_bang:"lircrcd";
	
	command=malloc(strlen(sha_bang2)+1+strlen(filename)+1);
	if(command==NULL)
	{
		goto lirc_readconfig_compat;
	}
	strcpy(command, sha_bang2);
	strcat(command, " ");
	strcat(command, filename);
	
	ret = system(command);
	free(command);
	
	if(ret!=EXIT_SUCCESS)
	{
		goto lirc_readconfig_compat;
	}
	
	if(sha_bang!=NULL) { free(sha_bang); sha_bang = NULL; }
	free(filename); filename = NULL;
	
	sockfd=socket(AF_UNIX,SOCK_STREAM,0);
	if(sockfd==-1)
	{
		lirc_printf(state, "%s: WARNING: could not open socket\n",state->lirc_prog);
		lirc_perror(state, state->lirc_prog);
		goto lirc_readconfig_compat;
	}
	if(connect(sockfd, (struct sockaddr *)&addr, sizeof(addr))!=-1)
	{
		if(lirc_identify(state, sockfd) == LIRC_RET_SUCCESS)
		{
			(*config)->sockfd=sockfd;
			return 0;
		}
	}
	close(sockfd);
	lirc_freeconfig(*config);
	return -1;
	
 lirc_readconfig_compat:
	/* compat fallback */
	if(sockfd != -1) close(sockfd);
	if(sha_bang!=NULL) free(sha_bang);
	free(filename);
	return 0;
}

int lirc_readconfig_only(const struct lirc_state *state,
                         const char *file,
                         struct lirc_config **config,
                         int (check)(char *s))
{
	return lirc_readconfig_only_internal(state, file, config, check, NULL, NULL);
}

static int lirc_readconfig_only_internal(const struct lirc_state *state,
                                         const char *file,
                                         struct lirc_config **config,
                                         int (check)(char *s),
                                         char **full_name,
                                         char **sha_bang)
{
	char *string,*eq,*token,*token2,*token3,*strtok_state = NULL;
	struct filestack_t *filestack, *stack_tmp;
	int open_files;
	struct lirc_config_entry *new_entry,*first,*last;
	char *mode,*remote;
	int ret=0;
	int firstline=1;
	char *save_full_name = NULL;
	
	filestack = stack_push(state, NULL);
	if (filestack == NULL)
	{
		return -1;
	}
	filestack->file = lirc_open(state, file, NULL, &(filestack->name));
	if (filestack->file == NULL)
	{
		stack_free(filestack);
		return -1;
	}
	filestack->line = 0;
	open_files = 1;

	first=new_entry=last=NULL;
	mode=NULL;
	remote=LIRC_ALL;
	while (filestack)
	{
		if((ret=lirc_readline(state,&string,filestack->file))==-1 ||
		   string==NULL)
		{
			fclose(filestack->file);
			if(open_files == 1 && full_name != NULL)
			{
				save_full_name = filestack->name;
				filestack->name = NULL;
			}
			filestack = stack_pop(filestack);
			open_files--;
			continue;
		}
		/* check for sha-bang */
		if(firstline && sha_bang)
		{
			firstline = 0;
			if(strncmp(string, "#!", 2)==0)
			{
				*sha_bang=strdup(string+2);
				if(*sha_bang==NULL)
				{
					lirc_printf(state, "%s: out of memory\n",
						    state->lirc_prog);
					ret=-1;
					free(string);
					break;
				}
			}
		}
		filestack->line++;
		eq=strchr(string,'=');
		if(eq==NULL)
		{
			token=strtok_r(string," \t",&strtok_state);
			if(token==NULL)
			{
				/* ignore empty line */
			}
			else if(token[0]=='#')
			{
				/* ignore comment */
			}
			else if(strcasecmp(token, "include") == 0)
			{
				if (open_files >= MAX_INCLUDES)
				{
					lirc_printf(state, "%s: too many files "
						    "included at %s:%d\n",
						    state->lirc_prog,
						    filestack->name,
						    filestack->line);
					ret=-1;
				}
				else
				{
					token2 = strtok_r(NULL, "", &strtok_state);
					token2 = lirc_trim(token2);
					lirc_parse_include
						(token2, filestack->name,
						 filestack->line);
					stack_tmp = stack_push(state, filestack);
					if (stack_tmp == NULL)
					{
						ret=-1;
					}
					else
					{
						stack_tmp->file = lirc_open(state, token2, filestack->name, &(stack_tmp->name));
						stack_tmp->line = 0;
						if (stack_tmp->file)
						{
							open_files++;
							filestack = stack_tmp;
						}
						else
						{
							stack_pop(stack_tmp);
							ret=-1;
						}
					}
				}
			}
			else
			{
				token2=strtok_r(NULL," \t",&strtok_state);
				if(token2!=NULL && 
				   (token3=strtok_r(NULL," \t",&strtok_state))!=NULL)
				{
					lirc_printf(state, "%s: unexpected token in line %s:%d\n",
						    state->lirc_prog,filestack->name,filestack->line);
				}
				else
				{
					ret=lirc_mode(state, token,token2,&mode,
						      &new_entry,&first,&last,
						      check,
						      filestack->name,
						      filestack->line);
					if(ret==0)
					{
						if(remote!=LIRC_ALL)
							free(remote);
						remote=LIRC_ALL;
					}
					else
					{
						if(mode!=NULL)
						{
							free(mode);
							mode=NULL;
						}
						if(new_entry!=NULL)
						{
							lirc_freeconfigentries
								(new_entry);
							new_entry=NULL;
						}
					}
				}
			}
		}
		else
		{
			eq[0]=0;
			token=lirc_trim(string);
			token2=lirc_trim(eq+1);
			if(token[0]=='#')
			{
				/* ignore comment */
			}
			else if(new_entry==NULL)
			{
				lirc_printf(state, "%s: bad file format, %s:%d\n",
					state->lirc_prog,filestack->name,filestack->line);
				ret=-1;
			}
			else
			{
				token2=strdup(token2);
				if(token2==NULL)
				{
					lirc_printf(state, "%s: out of memory\n",
						    state->lirc_prog);
					ret=-1;
				}
				else if(strcasecmp(token,"prog")==0)
				{
					if(new_entry->prog!=NULL) free(new_entry->prog);
					new_entry->prog=token2;
				}
				else if(strcasecmp(token,"remote")==0)
				{
					if(remote!=LIRC_ALL)
						free(remote);
					
					if(strcasecmp("*",token2)==0)
					{
						remote=LIRC_ALL;
						free(token2);
					}
					else
					{
						remote=token2;
					}
				}
				else if(strcasecmp(token,"button")==0)
				{
					struct lirc_code *code;
					
					code=(struct lirc_code *)
					malloc(sizeof(struct lirc_code));
					if(code==NULL)
					{
						free(token2);
						lirc_printf(state, "%s: out of "
							    "memory\n",
							    state->lirc_prog);
						ret=-1;
					}
					else
					{
						code->remote=remote;
						if(strcasecmp("*",token2)==0)
						{
							code->button=LIRC_ALL;
							free(token2);
						}
						else
						{
							code->button=token2;
						}
						code->next=NULL;

						if(new_entry->code==NULL)
						{
							new_entry->code=code;
						}
						else
						{
							new_entry->next_code->next
							=code;
						}
						new_entry->next_code=code;
						if(remote!=LIRC_ALL)
						{
							remote=strdup(remote);
							if(remote==NULL)
							{
								lirc_printf(state, "%s: out of memory\n",state->lirc_prog);
								ret=-1;
							}
						}
					}
				}
				else if(strcasecmp(token,"delay")==0)
				{
					char *end;

					errno=ERANGE+1;
					new_entry->rep_delay=strtoul(token2,&end,0);
					if((new_entry->rep_delay==UINT_MAX 
					    && errno==ERANGE)
					   || end[0]!=0
					   || strlen(token2)==0)
					{
						lirc_printf(state, "%s: \"%s\" not"
							    " a  valid number for "
							    "delay\n",state->lirc_prog,
							    token2);
					}
					free(token2);
				}
				else if(strcasecmp(token,"repeat")==0)
				{
					char *end;

					errno=ERANGE+1;
					new_entry->rep=strtoul(token2,&end,0);
					if((new_entry->rep==UINT_MAX
					    && errno==ERANGE)
					   || end[0]!=0
					   || strlen(token2)==0)
					{
						lirc_printf(state, "%s: \"%s\" not"
							    " a  valid number for "
							    "repeat\n",state->lirc_prog,
							    token2);
					}
					free(token2);
				}
				else if(strcasecmp(token,"config")==0)
				{
					struct lirc_list *new_list;

					new_list=(struct lirc_list *) 
					malloc(sizeof(struct lirc_list));
					if(new_list==NULL)
					{
						free(token2);
						lirc_printf(state, "%s: out of "
							    "memory\n",
							    state->lirc_prog);
						ret=-1;
					}
					else
					{
						lirc_parse_string(state,token2,filestack->name,filestack->line);
						new_list->string=token2;
						new_list->next=NULL;
						if(new_entry->config==NULL)
						{
							new_entry->config=new_list;
						}
						else
						{
							new_entry->next_config->next
							=new_list;
						}
						new_entry->next_config=new_list;
					}
				}
				else if(strcasecmp(token,"mode")==0)
				{
					if(new_entry->change_mode!=NULL) free(new_entry->change_mode);
					new_entry->change_mode=token2;
				}
				else if(strcasecmp(token,"flags")==0)
				{
					new_entry->flags=lirc_flags(state, token2);
					free(token2);
				}
				else
				{
					free(token2);
					lirc_printf(state, "%s: unknown token \"%s\" in %s:%d ignored\n",
						    state->lirc_prog,token,filestack->name,filestack->line);
				}
			}
		}
		free(string);
		if(ret==-1) break;
	}
	if(remote!=LIRC_ALL)
		free(remote);
	if(new_entry!=NULL)
	{
		if(ret==0)
		{
			ret=lirc_mode(state, "end",NULL,&mode,&new_entry,
				      &first,&last,check,"",0);
			lirc_printf(state, "%s: warning: end token missing at end "
				    "of file\n",state->lirc_prog);
		}
		else
		{
			lirc_freeconfigentries(new_entry);
			new_entry=NULL;
		}
	}
	if(mode!=NULL)
	{
		if(ret==0)
		{
			lirc_printf(state, "%s: warning: no end token found for mode "
				    "\"%s\"\n",state->lirc_prog,mode);
		}
		free(mode);
	}
	if(ret==0)
	{
		char *startupmode;
		
		*config=(struct lirc_config *)
			malloc(sizeof(struct lirc_config));
		if(*config==NULL)
		{
			lirc_printf(state, "%s: out of memory\n",state->lirc_prog);
			lirc_freeconfigentries(first);
			return(-1);
		}
		(*config)->first=first;
		(*config)->next=first;
		startupmode = lirc_startupmode(state, (*config)->first);
		(*config)->current_mode=startupmode ? strdup(startupmode):NULL;
		(*config)->sockfd=-1;
		if(full_name != NULL)
		{
			*full_name = save_full_name;
			save_full_name = NULL;
		}
	}
	else
	{
		*config=NULL;
		lirc_freeconfigentries(first);
		if(sha_bang && *sha_bang!=NULL)
		{
			free(*sha_bang);
			*sha_bang=NULL;
		}
	}
	if(filestack)
	{
		stack_free(filestack);
	}
	if(save_full_name)
	{
		free(save_full_name);
	}
	return(ret);
}

static char *lirc_startupmode(const struct lirc_state *state, struct lirc_config_entry *first)
{
	struct lirc_config_entry *scan;
	char *startupmode;

	startupmode=NULL;
	scan=first;
	/* Set a startup mode based on flags=startup_mode */
	while(scan!=NULL)
	{
		if(scan->flags&startup_mode) {
			if(scan->change_mode!=NULL) {
				startupmode=scan->change_mode;
				/* Remove the startup mode or it confuses lirc mode system */
				scan->change_mode=NULL;
				break;
			}
			else {
				lirc_printf(state, "%s: startup_mode flags requires 'mode ='\n",
					    state->lirc_prog);
			}
		}
		scan=scan->next;
	}

	/* Set a default mode if we find a mode = client app name */
	if(startupmode==NULL) {
		scan=first;
		while(scan!=NULL)
		{
			if(scan->mode!=NULL && strcasecmp(state->lirc_prog,scan->mode)==0)
			{
				startupmode=state->lirc_prog;
				break;
			}
			scan=scan->next;
		}
	}

	if(startupmode==NULL) return(NULL);
	scan=first;
	while(scan!=NULL)
	{
		if(scan->change_mode!=NULL && scan->flags&once &&
		   strcasecmp(startupmode,scan->change_mode)==0)
		{
			scan->flags|=ecno;
		}
		scan=scan->next;
	}
	return(startupmode);
}

void lirc_freeconfig(struct lirc_config *config)
{
	if(config!=NULL)
	{
		if(config->sockfd!=-1)
		{
			(void) close(config->sockfd);
			config->sockfd=-1;
		}
		lirc_freeconfigentries(config->first);
		free(config->current_mode);
		free(config);
	}
}

static void lirc_freeconfigentries(struct lirc_config_entry *first)
{
	struct lirc_config_entry *c,*config_temp;
	struct lirc_list *list,*list_temp;
	struct lirc_code *code,*code_temp;

	c=first;
	while(c!=NULL)
	{
		if(c->prog) free(c->prog);
		if(c->change_mode) free(c->change_mode);
		if(c->mode) free(c->mode);

		code=c->code;
		while(code!=NULL)
		{
			if(code->remote!=NULL && code->remote!=LIRC_ALL)
				free(code->remote);
			if(code->button!=NULL && code->button!=LIRC_ALL)
				free(code->button);
			code_temp=code->next;
			free(code);
			code=code_temp;
		}

		list=c->config;
		while(list!=NULL)
		{
			if(list->string) free(list->string);
			list_temp=list->next;
			free(list);
			list=list_temp;
		}
		config_temp=c->next;
		free(c);
		c=config_temp;
	}
}

static void lirc_clearmode(struct lirc_config *config)
{
	struct lirc_config_entry *scan;

	if(config->current_mode==NULL)
	{
		return;
	}
	scan=config->first;
	while(scan!=NULL)
	{
		if(scan->change_mode!=NULL)
		{
			if(strcasecmp(scan->change_mode,config->current_mode)==0)
			{
				scan->flags&=~ecno;
			}
		}
		scan=scan->next;
	}
	free(config->current_mode);
	config->current_mode=NULL;
}

static char *lirc_execute(const struct lirc_state *state,
			  struct lirc_config *config,
			  struct lirc_config_entry *scan)
{
	char *s;
	int do_once=1;
	
	if(scan->flags&mode)
	{
		lirc_clearmode(config);
	}
	if(scan->change_mode!=NULL)
	{
		free(config->current_mode);
		config->current_mode=strdup(scan->change_mode);
		if(scan->flags&once)
		{
			if(scan->flags&ecno)
			{
				do_once=0;
			}
			else
			{
				scan->flags|=ecno;
			}
		}
	}
	if(scan->next_config!=NULL &&
	   scan->prog!=NULL &&
	   (state->lirc_prog == NULL || strcasecmp(scan->prog,state->lirc_prog)==0) &&
	   do_once==1)
	{
		s=scan->next_config->string;
		scan->next_config=scan->next_config->next;
		if(scan->next_config==NULL)
			scan->next_config=scan->config;
		return(s);
	}
	return(NULL);
}

static int lirc_iscode(struct lirc_config_entry *scan, char *remote,
		       char *button,int rep)
{
	struct lirc_code *codes;
	
	/* no remote/button specified */
	if(scan->code==NULL)
	{
		return rep==0 ||
			(scan->rep>0 && rep>scan->rep_delay &&
			 ((rep-scan->rep_delay-1)%scan->rep)==0);
	}
	
	/* remote/button match? */
	if(scan->next_code->remote==LIRC_ALL || 
	   strcasecmp(scan->next_code->remote,remote)==0)
	{
		if(scan->next_code->button==LIRC_ALL || 
		   strcasecmp(scan->next_code->button,button)==0)
		{
			int iscode=0;
			/* button sequence? */
			if(scan->code->next==NULL || rep==0)
			{
				scan->next_code=scan->next_code->next;
				if(scan->code->next != NULL)
				{
					iscode=1;
				}
			}
			/* sequence completed? */
			if(scan->next_code==NULL)
			{
				scan->next_code=scan->code;
				if(scan->code->next!=NULL || rep==0 ||
				   (scan->rep>0 && rep>scan->rep_delay &&
				    ((rep-scan->rep_delay-1)%scan->rep)==0))
					iscode=2;
                        }
			return iscode;
		}
	}
	
        if(rep!=0) return(0);
	
	/* handle toggle_reset */
	if(scan->flags & toggle_reset)
	{
		scan->next_config = scan->config;
	}
	
	codes=scan->code;
        if(codes==scan->next_code) return(0);
	codes=codes->next;
	/* rebase code sequence */
	while(codes!=scan->next_code->next)
	{
                struct lirc_code *prev,*next;
                int flag=1;

                prev=scan->code;
                next=codes;
                while(next!=scan->next_code)
                {
                        if(prev->remote==LIRC_ALL ||
                           strcasecmp(prev->remote,next->remote)==0)
                        {
                                if(prev->button==LIRC_ALL ||
                                   strcasecmp(prev->button,next->button)==0)
                                {
                                        prev=prev->next;
                                        next=next->next;
                                }
                                else
                                {
                                        flag=0;break;
                                }
                        }
                        else
                        {
                                flag=0;break;
                        }
                }
                if(flag==1)
                {
                        if(prev->remote==LIRC_ALL ||
                           strcasecmp(prev->remote,remote)==0)
                        {
                                if(prev->button==LIRC_ALL ||
                                   strcasecmp(prev->button,button)==0)
                                {
                                        if(rep==0)
                                        {
                                                scan->next_code=prev->next;
                                                return(0);
                                        }
                                }
                        }
                }
                codes=codes->next;
	}
	scan->next_code=scan->code;
	return(0);
}

#if 0
char *lirc_ir2char(const struct lirc_state *state,struct lirc_config *config,char *code)
{
	static int warning=1;
	char *string;
	
	if(warning)
	{
		fprintf(stderr,"%s: warning: lirc_ir2char() is obsolete\n",
			state->lirc_prog);
		warning=0;
	}
	if(lirc_code2char(state,config,code,&string)==-1) return(NULL);
	return(string);
}
#endif

int lirc_code2char(const struct lirc_state *state, struct lirc_config *config,char *code,char **string)
{
	if(config->sockfd!=-1)
	{
		char* command = malloc((10+strlen(code)+1+1) * sizeof(char));
		if (command == NULL)
			return LIRC_RET_ERROR;
		static char buf[LIRC_PACKET_SIZE];
		size_t buf_len = LIRC_PACKET_SIZE;
		int success;
		int ret;
		
		sprintf(command, "CODE %s\n", code);
		
		ret = lirc_send_command(state, config->sockfd, command,
					buf, &buf_len, &success);
		if(success == LIRC_RET_SUCCESS)
		{
			if(ret > 0)
			{
				*string = buf;
			}
			else
			{
				*string = NULL;
			}
			free(command);
			return LIRC_RET_SUCCESS;
		}
		free(command);
		return LIRC_RET_ERROR;
	}
	return lirc_code2char_internal(state, config, code, string, NULL);
}

int lirc_code2charprog(struct lirc_state *state,struct lirc_config *config,char *code,char **string,
		       char **prog)
{
	char *backup;
	int ret;
	
	backup = state->lirc_prog;
	state->lirc_prog = NULL;
	
	ret = lirc_code2char_internal(state,config, code, string, prog);
	
	state->lirc_prog = backup;
	return ret;
}

static int lirc_code2char_internal(const struct lirc_state *state,
								   struct lirc_config *config,char *code,
								   char **string, char **prog)
{
	int rep;
	char *backup, *strtok_state = NULL;
	char *remote,*button;
	char *s=NULL;
	struct lirc_config_entry *scan;
	int exec_level;
	int quit_happened;

	*string=NULL;
	if(sscanf(code,"%*20x %20x %*5000s %*5000s\n",&rep)==1)
	{
		backup=strdup(code);
		if(backup==NULL) return(-1);

		strtok_r(backup," ",&strtok_state);
		strtok_r(NULL," ",&strtok_state);
		button=strtok_r(NULL," ",&strtok_state);
		remote=strtok_r(NULL,"\n",&strtok_state);

		if(button==NULL || remote==NULL)
		{
			free(backup);
			return(0);
		}
		
		scan=config->next;
		quit_happened=0;
		while(scan!=NULL)
		{
			exec_level = lirc_iscode(scan,remote,button,rep);
			if(exec_level > 0 &&
			   (scan->mode==NULL ||
			    (scan->mode!=NULL && 
			     config->current_mode!=NULL &&
			     strcasecmp(scan->mode,config->current_mode)==0)) &&
			   quit_happened==0
			   )
			{
				if(exec_level > 1)
				{
					s=lirc_execute(state,config,scan);
					if(s != NULL && prog != NULL)
					{
						*prog = scan->prog;
					}
				}
				else
				{
					s = NULL;
				}
				if(scan->flags&quit)
				{
					quit_happened=1;
					config->next=NULL;
					scan=scan->next;
					continue;
				}
				else if(s!=NULL)
				{
					config->next=scan->next;
					break;
				}
			}
			scan=scan->next;
		}
		free(backup);
		if(s!=NULL)
		{
			*string=s;
			return(0);
		}
	}
	config->next=config->first;
	return(0);
}

#define PACKET_SIZE 100

#if 0
char *lirc_nextir(struct lirc_state *state)
{
	static int warning=1;
	char *code;
	int ret;
	
	if(warning)
	{
		fprintf(stderr,"%s: warning: lirc_nextir() is obsolete\n",
			state->lirc_prog);
		warning=0;
	}
	ret=lirc_nextcode(state, &code);
	if(ret==-1) return(NULL);
	return(code);
}
#endif

int lirc_nextcode(struct lirc_state *state, char **code)
{
	static int packet_size=PACKET_SIZE;
	static int end_len=0;
	ssize_t len=0;
	char *end,c;

	*code=NULL;
	if(state->lirc_buffer==NULL)
	{
		state->lirc_buffer=(char *) malloc(packet_size+1);
		if(state->lirc_buffer==NULL)
		{
			lirc_printf(state, "%s: out of memory\n",state->lirc_prog);
			return(-1);
		}
		state->lirc_buffer[0]=0;
	}
	while((end=strchr(state->lirc_buffer,'\n'))==NULL)
	{
		if(end_len>=packet_size)
		{
			char *new_buffer;

			packet_size+=PACKET_SIZE;
			new_buffer=(char *) realloc(state->lirc_buffer,packet_size+1);
			if(new_buffer==NULL)
			{
				return(-1);
			}
			state->lirc_buffer=new_buffer;
		}
		len=read(state->lirc_lircd,state->lirc_buffer+end_len,packet_size-end_len);
		if(len<=0)
		{
			if(len==-1 && errno==EAGAIN) return(0);
			else return(-1);
		}
		end_len+=len;
		state->lirc_buffer[end_len]=0;
		/* return if next code not yet available completely */
		if(strchr(state->lirc_buffer,'\n')==NULL)
		{
			return(0);
		}
	}
	/* copy first line to buffer (code) and move remaining chars to
	   state->lirc_buffers start */
	end++;
	end_len=strlen(end);
	c=end[0];
	end[0]=0;
	*code=strdup(state->lirc_buffer);
	end[0]=c;
	memmove(state->lirc_buffer,end,end_len+1);
	if(*code==NULL) return(-1);
	return(0);
}

size_t lirc_getsocketname(const char *filename, char *buf, size_t size)
{
	if(strlen(filename)+2<=size)
	{
		strcpy(buf, filename);
		strcat(buf, "d");
	}
	return strlen(filename)+2;
}

const char *lirc_getmode(const struct lirc_state *state, struct lirc_config *config)
{
	if(config->sockfd!=-1)
	{
		static char buf[LIRC_PACKET_SIZE];
		size_t buf_len = LIRC_PACKET_SIZE;
		int success;
		int ret;
		
		ret = lirc_send_command(state, config->sockfd, "GETMODE\n",
					buf, &buf_len, &success);
		if(success == LIRC_RET_SUCCESS)
		{
			if(ret > 0)
			{
				return buf;
			}
			else
			{
				return NULL;
			}
		}
		return NULL;
	}
	return config->current_mode;
}

const char *lirc_setmode(const struct lirc_state *state, struct lirc_config *config, const char *mode)
{
	if(config->sockfd!=-1)
	{
		static char buf[LIRC_PACKET_SIZE];
		size_t buf_len = LIRC_PACKET_SIZE;
		int success;
		int ret;
		char cmd[LIRC_PACKET_SIZE];
		if(snprintf(cmd, LIRC_PACKET_SIZE, "SETMODE%s%s\n",
			    mode ? " ":"",
			    mode ? mode:"")
		   >= LIRC_PACKET_SIZE)
		{
			return NULL;
		}
		
		ret = lirc_send_command(state, config->sockfd, cmd,
					buf, &buf_len, &success);
		if(success == LIRC_RET_SUCCESS)
		{
			if(ret > 0)
			{
				return buf;
			}
			else
			{
				return NULL;
			}
		}
		return NULL;
	}
	
	free(config->current_mode);
	config->current_mode = mode ? strdup(mode) : NULL;
	return config->current_mode;
}

static const char *lirc_read_string(const struct lirc_state *state, int fd)
{
	static char buffer[LIRC_PACKET_SIZE+1]="";
	char *end;
	static int head=0, tail=0;
	int ret;
	ssize_t n;
	fd_set fds;
	struct timeval tv;
	
	if(head>0)
	{
		memmove(buffer,buffer+head,tail-head+1);
		tail-=head;
		head=0;
		end=strchr(buffer,'\n');
	}
	else
	{
		end=NULL;
	}
	if(strlen(buffer)!=tail)
	{
		lirc_printf(state, "%s: protocol error\n", state->lirc_prog);
		goto lirc_read_string_error;
	}
	
	while(end==NULL)
	{
		if(LIRC_PACKET_SIZE<=tail)
		{
			lirc_printf(state, "%s: bad packet\n", state->lirc_prog);
			goto lirc_read_string_error;
		}
		
		FD_ZERO(&fds);
		FD_SET(fd,&fds);
		tv.tv_sec=LIRC_TIMEOUT;
		tv.tv_usec=0;
		do
		{
			ret=select(fd+1,&fds,NULL,NULL,&tv);
		}
		while(ret==-1 && errno==EINTR);
		if(ret==-1)
		{
			lirc_printf(state, "%s: select() failed\n", state->lirc_prog);
			lirc_perror(state, state->lirc_prog);
			goto lirc_read_string_error;
		}
		else if(ret==0)
		{
			lirc_printf(state, "%s: timeout\n", state->lirc_prog);
			goto lirc_read_string_error;
		}
		
		n=read(fd, buffer+tail, LIRC_PACKET_SIZE-tail);
		if(n<=0)
		{
			lirc_printf(state, "%s: read() failed\n", state->lirc_prog);
			lirc_perror(state, state->lirc_prog);			
			goto lirc_read_string_error;
		}
		buffer[tail+n]=0;
		tail+=n;
		end=strchr(buffer,'\n');
	}
	
	end[0]=0;
	head=strlen(buffer)+1;
	return(buffer);

 lirc_read_string_error:
	head=tail=0;
	buffer[0]=0;
	return(NULL);
}

int lirc_send_command(const struct lirc_state *lstate, int sockfd, const char *command, char *buf, size_t *buf_len, int *ret_status)
{
	int done,todo;
	const char *string,*data;
	char *endptr;
	enum packet_state state;
	int status,n;
	unsigned long data_n=0;
	size_t written=0, max=0, len;

	if(buf_len!=NULL)
	{
		max=*buf_len;
	}
	todo=strlen(command);
	data=command;
	while(todo>0)
	{
		done=write(sockfd,(const void *) data,todo);
		if(done<0)
		{
			lirc_printf(lstate, "%s: could not send packet\n",
				    lstate->lirc_prog);
			lirc_perror(lstate, lstate->lirc_prog);
			return(-1);
		}
		data+=done;
		todo-=done;
	}

	/* get response */
	status=LIRC_RET_SUCCESS;
	state=P_BEGIN;
	n=0;
	while(1)
	{
		string=lirc_read_string(lstate, sockfd);
		if(string==NULL) return(-1);
		switch(state)
		{
		case P_BEGIN:
			if(strcasecmp(string,"BEGIN")!=0)
			{
				continue;
			}
			state=P_MESSAGE;
			break;
		case P_MESSAGE:
			if(strncasecmp(string,command,strlen(string))!=0 ||
			   strlen(string)+1!=strlen(command))
			{
				state=P_BEGIN;
				continue;
			}
			state=P_STATUS;
			break;
		case P_STATUS:
			if(strcasecmp(string,"SUCCESS")==0)
			{
				status=LIRC_RET_SUCCESS;
			}
			else if(strcasecmp(string,"END")==0)
			{
				status=LIRC_RET_SUCCESS;
				goto good_packet;
			}
			else if(strcasecmp(string,"ERROR")==0)
			{
				lirc_printf(lstate, "%s: command failed: %s",
					    lstate->lirc_prog, command);
				status=LIRC_RET_ERROR;
			}
			else
			{
				goto bad_packet;
			}
			state=P_DATA;
			break;
		case P_DATA:
			if(strcasecmp(string,"END")==0)
			{
				goto good_packet;
			}
			else if(strcasecmp(string,"DATA")==0)
			{
				state=P_N;
				break;
			}
			goto bad_packet;
		case P_N:
			errno=0;
			data_n=strtoul(string,&endptr,0);
			if(!*string || *endptr)
			{
				goto bad_packet;
			}
			if(data_n==0)
			{
				state=P_END;
			}
			else
			{
				state=P_DATA_N;
			}
			break;
		case P_DATA_N:
			len=strlen(string);
			if(buf!=NULL && written+len+1<max)
			{
				memcpy(buf+written, string, len+1);
			}
			written+=len+1;
			n++;
			if(n==data_n) state=P_END;
			break;
		case P_END:
			if(strcasecmp(string,"END")==0)
			{
				goto good_packet;
			}
			goto bad_packet;
			break;
		}
	}
	
	/* never reached */
	
 bad_packet:
	lirc_printf(lstate, "%s: bad return packet\n", lstate->lirc_prog);
	return(-1);
	
 good_packet:
	if(ret_status!=NULL)
	{
		*ret_status=status;
	}
	if(buf_len!=NULL)
	{
		*buf_len=written;
	}
	return (int) data_n;
}

int lirc_identify(const struct lirc_state *state, int sockfd)
{
	char* command = malloc((10+strlen(state->lirc_prog)+1+1) * sizeof(char));
	if (command == NULL)
		return LIRC_RET_ERROR;
	int success;

	sprintf(command, "IDENT %s\n", state->lirc_prog);

	(void) lirc_send_command(state, sockfd, command, NULL, NULL, &success);
	free(command);
	return success;
}
