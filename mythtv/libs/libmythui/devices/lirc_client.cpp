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
#include <array>
#include <cerrno>
#include <climits>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include "lirc_client.h"

// clazy:excludeall=raw-environment-function
// NOLINTBEGIN(performance-no-int-to-ptr)
// This code uses -1 throughout as the equivalent of nullptr.

/* internal defines */
static constexpr int8_t MAX_INCLUDES     {  10 };
static constexpr size_t LIRC_READ        { 255 };
static constexpr size_t LIRC_PACKET_SIZE { 255 };
/* three seconds */
static constexpr int8_t LIRC_TIMEOUT     {   3 };

/* internal data structures */
struct filestack_t {
	FILE               *m_file;
	char               *m_name;
	int                 m_line;
	struct filestack_t *m_parent;
};

enum packet_state : std::uint8_t
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
static void lirc_printf(const struct lirc_state* /*state*/, const char *format_str, ...);
static void lirc_perror(const struct lirc_state* /*state*/, const char *s);
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
                                         std::string& full_name,
                                         std::string& sha_bang);
static char *lirc_startupmode(const struct lirc_state *state,
							  struct lirc_config_entry *first);
static void lirc_freeconfigentries(struct lirc_config_entry *first);
static void lirc_clearmode(struct lirc_config *config);
static char *lirc_execute(const struct lirc_state *state,
			  struct lirc_config *config,
			  struct lirc_config_entry *scan);
static int lirc_iscode(struct lirc_config_entry *scan, char *remote,
		       char *button,unsigned int rep);
static int lirc_code2char_internal(const struct lirc_state *state,
				   struct lirc_config *config,const char *code,
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
	struct sockaddr_un addr {};

	/* connect to lircd */

	if(lircrc_root_file==nullptr || lircrc_user_file == nullptr || prog==nullptr)
	{
		lirc_printf(nullptr, "%s: lirc_init invalid params\n",prog);
		return nullptr;
	}

	auto *state = (struct lirc_state *) calloc(1, sizeof(struct lirc_state));
	if(state==nullptr)
	{
		lirc_printf(nullptr, "%s: out of memory\n",prog);
		return nullptr;
	}
	state->lirc_lircd=-1;
	state->lirc_verbose=verbose;

	state->lircrc_root_file=strdup(lircrc_root_file);
	if(state->lircrc_root_file==nullptr)
	{
		lirc_printf(state, "%s: out of memory\n",prog);
		lirc_deinit(state);
		return nullptr;
	}
	
	state->lircrc_user_file=strdup(lircrc_user_file);
	if(state->lircrc_user_file==nullptr)
	{
		lirc_printf(state, "%s: out of memory\n",prog);
		lirc_deinit(state);
		return nullptr;
	}
	
	state->lirc_prog=strdup(prog);
	if(state->lirc_prog==nullptr)
	{
		lirc_printf(state, "%s: out of memory\n",prog);
		lirc_deinit(state);
		return nullptr;
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
			return nullptr;
		}
		if(connect(state->lirc_lircd,(struct sockaddr *)&addr,sizeof(addr))==-1)
		{
			close(state->lirc_lircd);
			lirc_printf(state, "%s: could not connect to socket\n",state->lirc_prog);
			lirc_perror(state, state->lirc_prog);
			lirc_deinit(state);
			return nullptr;
		}
	}

	return(state);
}

int lirc_deinit(struct lirc_state *state)
{
	int ret = LIRC_RET_SUCCESS;
	if (state==nullptr)
		return ret;
	if(state->lircrc_root_file!=nullptr)
	{
		free(state->lircrc_root_file);
		state->lircrc_root_file=nullptr;
	}
	if(state->lircrc_user_file!=nullptr)
	{
		free(state->lircrc_user_file);
		state->lircrc_user_file=nullptr;
	}
	if(state->lirc_prog!=nullptr)
	{
		free(state->lirc_prog);
		state->lirc_prog=nullptr;
	}
	if(state->lirc_buffer!=nullptr)
	{
		free(state->lirc_buffer);
		state->lirc_buffer=nullptr;
	}
	if (state->lirc_lircd!=-1)
		ret = close(state->lirc_lircd);
	free(state);
	return ret;
}

static int lirc_readline(const struct lirc_state *state, char **line,FILE *f)
{
	char *newline=(char *) malloc(LIRC_READ+1);
	if(newline==nullptr)
	{
		lirc_printf(state, "%s: out of memory\n",state->lirc_prog);
		return(-1);
	}
	int len=0;
	while(true)
	{
		char *ret=fgets(newline+len,LIRC_READ+1,f);
		if(ret==nullptr)
		{
			if(feof(f) && len>0)
			{
				*line=newline;
			}
			else
			{
				free(newline);
				*line=nullptr;
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
		
		char *enlargeline=(char *) realloc(newline,len+1+LIRC_READ);
		if(enlargeline==nullptr)
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
	while(s[0]==' ' || s[0]=='\t') s++;
	int len=strlen(s);
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

	unsigned int i = 0;
	unsigned int count = 0;

	char c=**s;
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
			uint overflow=0;
			int digits_found=0;
			for (;;)
			{
                                int digit = 0;
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
	char *t=s;
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

static void lirc_parse_include(char *s,
                               [[maybe_unused]] const char *name,
                               [[maybe_unused]] int line)
{
	size_t len=strlen(s);
	if(len<2)
	{
		return;
	}
	char last=s[len-1];
	if(*s!='"' && *s!='<')
	{
		return;
	}
	if(*s=='"' && last!='"')
	{
		return;
	}
	if(*s=='<' && last!='>')
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
	struct lirc_config_entry *new_entry=*new_config;
	if(strcasecmp(token,"begin")==0)
	{
		if(token2==nullptr)
		{
			if(new_entry==nullptr)
			{
				new_entry=(struct lirc_config_entry *)
				malloc(sizeof(struct lirc_config_entry));
				if(new_entry==nullptr)
				{
					lirc_printf(state, "%s: out of memory\n",
						    state->lirc_prog);
					return(-1);
				}

                                new_entry->prog=nullptr;
                                new_entry->code=nullptr;
                                new_entry->rep_delay=0;
                                new_entry->rep=0;
                                new_entry->config=nullptr;
                                new_entry->change_mode=nullptr;
                                new_entry->flags=none;
                                new_entry->mode=nullptr;
                                new_entry->next_config=nullptr;
                                new_entry->next_code=nullptr;
                                new_entry->next=nullptr;

                                *new_config=new_entry;
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
			if(new_entry==nullptr && *mode==nullptr)
			{
				*mode=strdup(token2);
				if(*mode==nullptr)
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
		if(token2==nullptr)
		{
			if(new_entry!=nullptr)
			{
#if 0
				if(new_entry->prog==nullptr)
				{
					lirc_printf(state, "%s: prog missing in "
						    "config before line %d\n",
						    state->lirc_prog,line);
					lirc_freeconfigentries(new_entry);
					*new_config=nullptr;
					return(-1);
				}
				if(strcasecmp(new_entry->prog,state->lirc_prog)!=0)
				{
					lirc_freeconfigentries(new_entry);
					*new_config=nullptr;
					return(0);
				}
#endif
				new_entry->next_code=new_entry->code;
				new_entry->next_config=new_entry->config;
				if(*last_config==nullptr)
				{
					*first_config=new_entry;
					*last_config=new_entry;
				}
				else
				{
					(*last_config)->next=new_entry;
					*last_config=new_entry;
				}
				*new_config=nullptr;

				if(*mode!=nullptr)
				{
					new_entry->mode=strdup(*mode);
					if(new_entry->mode==nullptr)
					{
						lirc_printf(state, "%s: out of "
							    "memory\n",
							    state->lirc_prog);
						return(-1);
					}
				}

				if(check!=nullptr &&
				   new_entry->prog!=nullptr &&
				   strcasecmp(new_entry->prog,state->lirc_prog)==0)
				{					
					struct lirc_list *list=new_entry->config;
					while(list!=nullptr)
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
			if(*mode!=nullptr)
			{
				if(new_entry!=nullptr)
				{
					lirc_printf(state, "%s: %s:%d: missing "
						    "'end' token\n",state->lirc_prog,
						    name,line);
					return(-1);
				}
				if(strcasecmp(*mode,token2)==0)
				{
					free(*mode);
					*mode=nullptr;
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
	char *strtok_state = nullptr;
	unsigned int flags=none;
	char *s=strtok_r(string," \t|",&strtok_state);
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
			flags|=modex;
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
		s=strtok_r(nullptr," \t",&strtok_state);
	}
	return(flags);
}

static char *lirc_getfilename(const struct lirc_state *state,
							  const char *file,
							  const char *current_file)
{
	char *filename = nullptr;

	if(file==nullptr)
	{
		const char *home=getenv("HOME");
		if(home==nullptr)
		{
			home="/";
		}
		filename=(char *) malloc(strlen(home)+1+
					 strlen(state->lircrc_user_file)+1);
		if(filename==nullptr)
		{
			lirc_printf(state, "%s: out of memory\n",state->lirc_prog);
			return nullptr;
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
		const char *home=getenv("HOME");
		if(home==nullptr)
		{
			home="/";
		}
		filename=(char *) malloc(strlen(home)+strlen(file)+1);
		if(filename==nullptr)
		{
			lirc_printf(state, "%s: out of memory\n",state->lirc_prog);
			return nullptr;
		}
		strcpy(filename,home);
		strcat(filename,file+1);
	}
	else if(file[0]=='/' || current_file==nullptr)
	{
		/* absulute path or root */
		filename=strdup(file);
		if(filename==nullptr)
		{
			lirc_printf(state, "%s: out of memory\n",state->lirc_prog);
			return nullptr;
		}
	}
	else
	{
		/* get path from parent filename */
		int pathlen = strlen(current_file);
		while (pathlen>0 && current_file[pathlen-1]!='/')
			pathlen--;
		filename=(char *) malloc(pathlen+strlen(file)+1);
		if(filename==nullptr)
		{
			lirc_printf(state, "%s: out of memory\n",state->lirc_prog);
			return nullptr;
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
	char *filename=lirc_getfilename(state, file, current_file);
	if(filename==nullptr)
	{
		return nullptr;
	}

	FILE *fin=fopen(filename,"r");
	if(fin==nullptr && (file!=nullptr || errno!=ENOENT))
	{
		lirc_printf(state, "%s: could not open config file %s\n",
			    state->lirc_prog,filename);
		lirc_perror(state, state->lirc_prog);
	}
	else if(fin==nullptr)
	{
		fin=fopen(state->lircrc_root_file,"r");
		if(fin==nullptr && errno!=ENOENT)
		{
			lirc_printf(state, "%s: could not open config file %s\n",
				    state->lirc_prog,state->lircrc_root_file);
			lirc_perror(state, state->lirc_prog);
		}
		else if(fin==nullptr)
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
			if(filename==nullptr)
			{
				fclose(fin);
				lirc_printf(state, "%s: out of memory\n",state->lirc_prog);
				return nullptr;
			}
		}
	}
	if(full_name && fin!=nullptr)
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
	auto *entry = static_cast<struct filestack_t *>(malloc(sizeof(struct filestack_t)));
	if (entry == nullptr)
	{
		lirc_printf(state, "%s: out of memory\n",state->lirc_prog);
		return nullptr;
	}
	entry->m_file = nullptr;
	entry->m_name = nullptr;
	entry->m_line = 0;
	entry->m_parent = parent;
	return entry;
}

static struct filestack_t *stack_pop(struct filestack_t *entry)
{
	struct filestack_t *parent = nullptr;
	if (entry)
	{
		parent = entry->m_parent;
		if (entry->m_name)
			free(entry->m_name);
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
	struct sockaddr_un addr {};
	int sockfd = -1;
	unsigned int ret = 0;

	std::string filename;
	std::string sha_bang;
	if(lirc_readconfig_only_internal(state,file,config,check,filename,sha_bang)==-1)
	{
		return -1;
	}
	
	if(sha_bang.empty())
		return 0;
	
	/* connect to lircrcd */

	addr.sun_family=AF_UNIX;
	if(lirc_getsocketname(filename.data(), addr.sun_path, sizeof(addr.sun_path))>sizeof(addr.sun_path))
	{
		lirc_printf(state, "%s: WARNING: file name too long\n", state->lirc_prog);
		return 0;
	}
	sockfd=socket(AF_UNIX,SOCK_STREAM,0);
	if(sockfd==-1)
	{
		lirc_printf(state, "%s: WARNING: could not open socket\n",state->lirc_prog);
		lirc_perror(state, state->lirc_prog);
		return 0;
	}
	if(connect(sockfd, (struct sockaddr *)&addr, sizeof(addr))!=-1)
	{
		(*config)->sockfd=sockfd;
		
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
	
	/* launch lircrcd */
	std::string command = sha_bang + " " + filename;
	ret = system(command.data());
	
	if(ret!=EXIT_SUCCESS)
		return 0;
	
	sockfd=socket(AF_UNIX,SOCK_STREAM,0);
	if(sockfd==-1)
	{
		lirc_printf(state, "%s: WARNING: could not open socket\n",state->lirc_prog);
		lirc_perror(state, state->lirc_prog);
		return 0;
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
}

int lirc_readconfig_only(const struct lirc_state *state,
                         const char *file,
                         struct lirc_config **config,
                         int (check)(char *s))
{
	std::string filename;
	std::string sha_bang;
	return lirc_readconfig_only_internal(state, file, config, check, filename, sha_bang);
}

static int lirc_readconfig_only_internal(const struct lirc_state *state,
                                         const char *file,
                                         struct lirc_config **config,
                                         int (check)(char *s),
                                         std::string& full_name,
                                         std::string& sha_bang)
{
	int ret=0;
	int firstline=1;
	
	struct filestack_t *filestack = stack_push(state, nullptr);
	if (filestack == nullptr)
	{
		return -1;
	}
	filestack->m_file = lirc_open(state, file, nullptr, &(filestack->m_name));
	if (filestack->m_file == nullptr)
	{
		stack_free(filestack);
		return -1;
	}
	filestack->m_line = 0;
	int open_files = 1;

	struct lirc_config_entry *new_entry = nullptr;
	struct lirc_config_entry *first = nullptr;
	struct lirc_config_entry *last = nullptr;
	char *mode=nullptr;
	char *remote=LIRC_ALL;
	while (filestack)
	{
		char *string = nullptr;
		ret=lirc_readline(state,&string,filestack->m_file);
		if(ret==-1 || string==nullptr)
		{
			fclose(filestack->m_file);
			if(open_files == 1)
			{
				full_name = filestack->m_name;
			}
			filestack = stack_pop(filestack);
			open_files--;
			continue;
		}
		/* check for sha-bang */
		if(firstline)
		{
			firstline = 0;
			if(strncmp(string, "#!", 2)==0)
			{
				sha_bang=string+2;
			}
		}
		filestack->m_line++;
		char *eq=strchr(string,'=');
		if(eq==nullptr)
		{
			char *strtok_state = nullptr;
			char *token=strtok_r(string," \t",&strtok_state);
			if ((token==nullptr) || (token[0]=='#'))
			{
				/* ignore empty line or comment */
			}
			else if(strcasecmp(token, "include") == 0)
			{
				if (open_files >= MAX_INCLUDES)
				{
					lirc_printf(state, "%s: too many files "
						    "included at %s:%d\n",
						    state->lirc_prog,
						    filestack->m_name,
						    filestack->m_line);
					ret=-1;
				}
				else
				{
					char *token2 = strtok_r(nullptr, "", &strtok_state);
					token2 = lirc_trim(token2);
					lirc_parse_include
						(token2, filestack->m_name,
						 filestack->m_line);
					struct filestack_t *stack_tmp =
					    stack_push(state, filestack);
					if (stack_tmp == nullptr)
					{
						ret=-1;
					}
					else
					{
						stack_tmp->m_file = lirc_open(state, token2, filestack->m_name, &(stack_tmp->m_name));
						stack_tmp->m_line = 0;
						if (stack_tmp->m_file)
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
				char *token2=strtok_r(nullptr," \t",&strtok_state);
				if(token2!=nullptr &&
				   strtok_r(nullptr," \t",&strtok_state)!=nullptr)
				{
					lirc_printf(state, "%s: unexpected token in line %s:%d\n",
						    state->lirc_prog,filestack->m_name,filestack->m_line);
				}
				else
				{
					ret=lirc_mode(state, token,token2,&mode,
						      &new_entry,&first,&last,
						      check,
						      filestack->m_name,
						      filestack->m_line);
					if(ret==0)
					{
						if(remote!=LIRC_ALL)
							free(remote);
						remote=LIRC_ALL;
					}
					else
					{
						if(mode!=nullptr)
						{
							free(mode);
							mode=nullptr;
						}
						if(new_entry!=nullptr)
						{
							lirc_freeconfigentries
								(new_entry);
							new_entry=nullptr;
						}
					}
				}
			}
		}
		else
		{
			eq[0]=0;
			char *token=lirc_trim(string);
			char *token2=lirc_trim(eq+1);
			if(token[0]=='#')
			{
				/* ignore comment */
			}
			else if(new_entry==nullptr)
			{
				lirc_printf(state, "%s: bad file format, %s:%d\n",
					state->lirc_prog,filestack->m_name,filestack->m_line);
				ret=-1;
			}
			else
			{
				token2=strdup(token2);
				if(token2==nullptr)
				{
					lirc_printf(state, "%s: out of memory\n",
						    state->lirc_prog);
					ret=-1;
				}
				else if(strcasecmp(token,"prog")==0)
				{
					if(new_entry->prog!=nullptr) free(new_entry->prog);
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
					auto *code=
                                            (struct lirc_code *)malloc(sizeof(struct lirc_code));
					if(code==nullptr)
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
						code->next=nullptr;

						if(new_entry->code==nullptr)
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
							if(remote==nullptr)
							{
								lirc_printf(state, "%s: out of memory\n",state->lirc_prog);
								ret=-1;
							}
						}
					}
				}
				else if(strcasecmp(token,"delay")==0)
				{
					char *end = nullptr;

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
					char *end = nullptr;

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
					auto *new_list =
                                            (struct lirc_list *)malloc(sizeof(struct lirc_list));
					if(new_list==nullptr)
					{
						free(token2);
						lirc_printf(state, "%s: out of "
							    "memory\n",
							    state->lirc_prog);
						ret=-1;
					}
					else
					{
						lirc_parse_string(state,token2,filestack->m_name,filestack->m_line);
						new_list->string=token2;
						new_list->next=nullptr;
						if(new_entry->config==nullptr)
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
					if(new_entry->change_mode!=nullptr) free(new_entry->change_mode);
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
						    state->lirc_prog,token,filestack->m_name,filestack->m_line);
				}
			}
		}
		free(string);
		if(ret==-1) break;
	}
	if(remote!=LIRC_ALL)
		free(remote);
	if(new_entry!=nullptr)
	{
		if(ret==0)
		{
			ret=lirc_mode(state, "end",nullptr,&mode,&new_entry,
				      &first,&last,check,"",0);
			lirc_printf(state, "%s: warning: end token missing at end "
				    "of file\n",state->lirc_prog);
		}
		else
		{
			lirc_freeconfigentries(new_entry);
			new_entry=nullptr;
		}
	}
	if(mode!=nullptr)
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
		*config=(struct lirc_config *)
			malloc(sizeof(struct lirc_config));
		if(*config==nullptr)
		{
			lirc_printf(state, "%s: out of memory\n",state->lirc_prog);
			lirc_freeconfigentries(first);
			return(-1);
		}
		(*config)->first=first;
		(*config)->next=first;
		char *startupmode = lirc_startupmode(state, (*config)->first);
		(*config)->current_mode=startupmode ? strdup(startupmode):nullptr;
		(*config)->sockfd=-1;
	}
	else
	{
		*config=nullptr;
		lirc_freeconfigentries(first);
		sha_bang.clear();
	}
	if(filestack)
	{
		stack_free(filestack);
	}
	return(ret);
}

static char *lirc_startupmode(const struct lirc_state *state, struct lirc_config_entry *first)
{
	char *startupmode=nullptr;
	struct lirc_config_entry *scan=first;

	/* Set a startup mode based on flags=startup_mode */
	while(scan!=nullptr)
	{
		if(scan->flags&startup_mode) {
			if(scan->change_mode!=nullptr) {
				startupmode=scan->change_mode;
				/* Remove the startup mode or it confuses lirc mode system */
				scan->change_mode=nullptr;
				break;
			}
                        lirc_printf(state, "%s: startup_mode flags requires 'mode ='\n",
                                    state->lirc_prog);
		}
		scan=scan->next;
	}

	/* Set a default mode if we find a mode = client app name */
	if(startupmode==nullptr) {
		scan=first;
		while(scan!=nullptr)
		{
			if(scan->mode!=nullptr && strcasecmp(state->lirc_prog,scan->mode)==0)
			{
				startupmode=state->lirc_prog;
				break;
			}
			scan=scan->next;
		}
	}

	if(startupmode==nullptr) return nullptr;
	scan=first;
	while(scan!=nullptr)
	{
		if(scan->change_mode!=nullptr && scan->flags&once &&
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
	if(config!=nullptr)
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
	struct lirc_config_entry *c=first;
	while(c!=nullptr)
	{
		if(c->prog) free(c->prog);
		if(c->change_mode) free(c->change_mode);
		if(c->mode) free(c->mode);

		struct lirc_code *code=c->code;
		while(code!=nullptr)
		{
			if(code->remote!=nullptr && code->remote!=LIRC_ALL)
				free(code->remote);
			if(code->button!=nullptr && code->button!=LIRC_ALL)
				free(code->button);
			struct lirc_code *code_temp=code->next;
			free(code);
			code=code_temp;
		}

                struct lirc_list *list=c->config;
		while(list!=nullptr)
		{
			if(list->string) free(list->string);
			struct lirc_list *list_temp=list->next;
			free(list);
			list=list_temp;
		}
		struct lirc_config_entry *config_temp=c->next;
		free(c);
		c=config_temp;
	}
}

static void lirc_clearmode(struct lirc_config *config)
{
	if(config->current_mode==nullptr)
	{
		return;
	}
	struct lirc_config_entry *scan=config->first;
	while(scan!=nullptr)
	{
		if(scan->change_mode!=nullptr)
		{
			if(strcasecmp(scan->change_mode,config->current_mode)==0)
			{
				scan->flags&=~ecno;
			}
		}
		scan=scan->next;
	}
	free(config->current_mode);
	config->current_mode=nullptr;
}

static char *lirc_execute(const struct lirc_state *state,
			  struct lirc_config *config,
			  struct lirc_config_entry *scan)
{
	int do_once=1;
	
	if(scan->flags&modex)
	{
		lirc_clearmode(config);
	}
	if(scan->change_mode!=nullptr)
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
	if(scan->next_config!=nullptr &&
	   scan->prog!=nullptr &&
	   (state->lirc_prog == nullptr || strcasecmp(scan->prog,state->lirc_prog)==0) &&
	   do_once==1)
	{
                char *s=scan->next_config->string;
		scan->next_config=scan->next_config->next;
		if(scan->next_config==nullptr)
			scan->next_config=scan->config;
		return(s);
	}
	return nullptr;
}

static int lirc_iscode(struct lirc_config_entry *scan, char *remote,
		       char *button,unsigned int rep)
{
	/* no remote/button specified */
	if(scan->code==nullptr)
	{
		return static_cast<int>(rep==0 ||
                                        (scan->rep>0 && rep>scan->rep_delay &&
                                         ((rep-scan->rep_delay-1)%scan->rep)==0));
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
			if(scan->code->next==nullptr || rep==0)
			{
				scan->next_code=scan->next_code->next;
				if(scan->code->next != nullptr)
				{
					iscode=1;
				}
			}
			/* sequence completed? */
			if(scan->next_code==nullptr)
			{
				scan->next_code=scan->code;
				if(scan->code->next!=nullptr || rep==0 ||
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
	
	struct lirc_code *codes=scan->code;
        if(codes==scan->next_code) return(0);
	codes=codes->next;
	/* rebase code sequence */
	while(codes!=scan->next_code->next)
	{
                int flag=1;
                struct lirc_code *prev=scan->code;
                struct lirc_code *next=codes;
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
	if(lirc_code2char(state,config,code,&string)==-1) return nullptr;
	return(string);
}
#endif

int lirc_code2char(const struct lirc_state *state, struct lirc_config *config,const char *code,char **string)
{
	if(config->sockfd!=-1)
	{
		char* command = static_cast<char*>(malloc((10+strlen(code)+1+1) * sizeof(char)));
		if (command == nullptr)
			return LIRC_RET_ERROR;
		static std::array<char,LIRC_PACKET_SIZE> s_buf;
		size_t buf_len = s_buf.size();
		int success = LIRC_RET_ERROR;
		
		sprintf(command, "CODE %s\n", code);
		
		int ret = lirc_send_command(state, config->sockfd, command,
					s_buf.data(), &buf_len, &success);
		if(success == LIRC_RET_SUCCESS)
		{
			if(ret > 0)
			{
				*string = s_buf.data();
			}
			else
			{
				*string = nullptr;
			}
			free(command);
			return LIRC_RET_SUCCESS;
		}
		free(command);
		return LIRC_RET_ERROR;
	}
	return lirc_code2char_internal(state, config, code, string, nullptr);
}

int lirc_code2charprog(struct lirc_state *state,struct lirc_config *config,char *code,char **string,
		       char **prog)
{
	char *backup = state->lirc_prog;
	state->lirc_prog = nullptr;
	
	int ret = lirc_code2char_internal(state,config, code, string, prog);
	
	state->lirc_prog = backup;
	return ret;
}

static int lirc_code2char_internal(const struct lirc_state *state,
                                   struct lirc_config *config, const char *code,
                                   char **string, char **prog)
{
	unsigned int rep = 0;
	char *strtok_state = nullptr;
	char *s=nullptr;

	*string=nullptr;
	if(sscanf(code,"%*20x %20x %*5000s %*5000s\n",&rep)==1)
	{
		char *backup=strdup(code);
		if(backup==nullptr) return(-1);

		strtok_r(backup," ",&strtok_state);
		strtok_r(nullptr," ",&strtok_state);
		char *button=strtok_r(nullptr," ",&strtok_state);
		char *remote=strtok_r(nullptr,"\n",&strtok_state);

		if(button==nullptr || remote==nullptr)
		{
			free(backup);
			return(0);
		}
		
		struct lirc_config_entry *scan=config->next;
		int quit_happened=0;
		while(scan!=nullptr)
		{
			int exec_level = lirc_iscode(scan,remote,button,rep);
			if(exec_level > 0 &&
			   (scan->mode==nullptr ||
			    (scan->mode!=nullptr &&
			     config->current_mode!=nullptr &&
			     strcasecmp(scan->mode,config->current_mode)==0)) &&
			   quit_happened==0
			   )
			{
				if(exec_level > 1)
				{
					s=lirc_execute(state,config,scan);
					if(s != nullptr && prog != nullptr)
					{
						*prog = scan->prog;
					}
				}
				else
				{
					s = nullptr;
				}
				if(scan->flags&quit)
				{
					quit_happened=1;
					config->next=nullptr;
					scan=scan->next;
					continue;
				}
				if(s!=nullptr)
				{
					config->next=scan->next;
					break;
				}
			}
			scan=scan->next;
		}
		free(backup);
		if(s!=nullptr)
		{
			*string=s;
			return(0);
		}
	}
	config->next=config->first;
	return(0);
}

static constexpr size_t PACKET_SIZE { 100 };

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
	if(ret==-1) return nullptr;
	return(code);
}
#endif

int lirc_nextcode(struct lirc_state *state, char **code)
{
	static size_t s_packetSize=PACKET_SIZE;
	static size_t s_endLen=0;
	char *end = nullptr;

	*code=nullptr;
	if(state->lirc_buffer==nullptr)
	{
		state->lirc_buffer=(char *) malloc(s_packetSize+1);
		if(state->lirc_buffer==nullptr)
		{
			lirc_printf(state, "%s: out of memory\n",state->lirc_prog);
			return(-1);
		}
		state->lirc_buffer[0]=0;
	}
	while((end=strchr(state->lirc_buffer,'\n'))==nullptr)
	{
		if(s_endLen>=s_packetSize)
		{
			s_packetSize+=PACKET_SIZE;
			char *new_buffer=(char *) realloc(state->lirc_buffer,s_packetSize+1);
			if(new_buffer==nullptr)
			{
				return(-1);
			}
			state->lirc_buffer=new_buffer;
		}
                ssize_t len=read(state->lirc_lircd,state->lirc_buffer+s_endLen,s_packetSize-s_endLen);
		if(len<=0)
		{
			if(len==-1 && errno==EAGAIN) return(0);
                        return(-1);
		}
		s_endLen+=len;
		state->lirc_buffer[s_endLen]=0;
		/* return if next code not yet available completely */
		if(strchr(state->lirc_buffer,'\n')==nullptr)
		{
			return(0);
		}
	}
	/* copy first line to buffer (code) and move remaining chars to
	   state->lirc_buffers start */

        // Cppcheck doesn't parse the previous loop properly.  The
        // only way for the loop to exit and execute the next line of
        // code is if end becomes non-null.
        //
	end++;
	s_endLen=strlen(end);
	char c=end[0];
	end[0]=0;
	*code=strdup(state->lirc_buffer);
	end[0]=c;
	memmove(state->lirc_buffer,end,s_endLen+1);
	if(*code==nullptr) return(-1);
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
		static std::array<char,LIRC_PACKET_SIZE> s_buf;
		size_t buf_len = s_buf.size();
		int success = LIRC_RET_ERROR;
		
		int ret = lirc_send_command(state, config->sockfd, "GETMODE\n",
                                            s_buf.data(), &buf_len, &success);
		if(success == LIRC_RET_SUCCESS)
		{
			if(ret > 0)
			{
				return s_buf.data();
			}
                        return nullptr;
		}
		return nullptr;
	}
	return config->current_mode;
}

const char *lirc_setmode(const struct lirc_state *state, struct lirc_config *config, const char *mode)
{
	if(config->sockfd!=-1)
	{
		static std::array<char,LIRC_PACKET_SIZE> s_buf {};
		std::array<char,LIRC_PACKET_SIZE> cmd {};
		size_t buf_len = s_buf.size();
		int success = LIRC_RET_ERROR;
		if(snprintf(cmd.data(), LIRC_PACKET_SIZE, "SETMODE%s%s\n",
			    mode ? " ":"",
			    mode ? mode:"")
		   >= static_cast<int>(LIRC_PACKET_SIZE))
		{
			return nullptr;
		}
		
		int ret = lirc_send_command(state, config->sockfd, cmd.data(),
					s_buf.data(), &buf_len, &success);
		if(success == LIRC_RET_SUCCESS)
		{
			if(ret > 0)
			{
				return s_buf.data();
			}
                        return nullptr;
		}
		return nullptr;
	}
	
	free(config->current_mode);
	config->current_mode = mode ? strdup(mode) : nullptr;
	return config->current_mode;
}

static const char *lirc_read_string(const struct lirc_state *state, int fd)
{
	static std::array<char,LIRC_PACKET_SIZE+1> s_buffer;
	char *end = nullptr;
	static size_t s_head=0;
	static size_t s_tail=0;
	int ret = 0;
	ssize_t n = 0;
	fd_set fds;
	struct timeval tv {};
	
        auto cleanup_fn = [&](int */*x*/) {
		s_head=s_tail=0;
		s_buffer[0]=0;
        };
        std::unique_ptr<int,decltype(cleanup_fn)> cleanup { &ret, cleanup_fn };

	if(s_head>0)
	{
		memmove(s_buffer.data(),s_buffer.data()+s_head,s_tail-s_head+1);
		s_tail-=s_head;
		s_head=0;
		end=strchr(s_buffer.data(),'\n');
	}
	else
	{
		end=nullptr;
	}
	if(strlen(s_buffer.data())!=s_tail)
	{
		lirc_printf(state, "%s: protocol error\n", state->lirc_prog);
		return nullptr;
	}
	
	while(end==nullptr)
	{
		if(LIRC_PACKET_SIZE<=s_tail)
		{
			lirc_printf(state, "%s: bad packet\n", state->lirc_prog);
			return nullptr;
		}
		
		FD_ZERO(&fds); // NOLINT(readability-isolate-declaration)
		FD_SET(fd,&fds);
		tv.tv_sec=LIRC_TIMEOUT;
		tv.tv_usec=0;
		ret=select(fd+1,&fds,nullptr,nullptr,&tv);
		while(ret==-1 && errno==EINTR)
			ret=select(fd+1,&fds,nullptr,nullptr,&tv);
		if(ret==-1)
		{
			lirc_printf(state, "%s: select() failed\n", state->lirc_prog);
			lirc_perror(state, state->lirc_prog);
			return nullptr;
		}
		if(ret==0)
		{
			lirc_printf(state, "%s: timeout\n", state->lirc_prog);
			return nullptr;
		}
		
		n=read(fd, s_buffer.data()+s_tail, LIRC_PACKET_SIZE-s_tail);
		if(n<=0)
		{
			lirc_printf(state, "%s: read() failed\n", state->lirc_prog);
			lirc_perror(state, state->lirc_prog);			
			return nullptr;
		}
		s_buffer[s_tail+n]=0;
		s_tail+=n;
		end=strchr(s_buffer.data(),'\n');
	}
	
	end[0]=0;
	s_head=strlen(s_buffer.data())+1;
	(void)cleanup.release();
	return(s_buffer.data());
}

int lirc_send_command(const struct lirc_state *lstate, int sockfd, const char *command, char *buf, size_t *buf_len, int *ret_status)
{
	char *endptr = nullptr;
	unsigned long n = 0;
	unsigned long data_n=0;
	size_t written=0;
	size_t max=0;
	size_t len = 0;

	if(buf_len!=nullptr)
	{
		max=*buf_len;
	}
	int todo=strlen(command);
	const char *data=command;
	lirc_printf(lstate, "%s: sending command: %s",
		    lstate->lirc_prog, command);
	while(todo>0)
	{
		int done=write(sockfd,(const void *) data,todo);
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
	int status=LIRC_RET_SUCCESS;
	enum packet_state state=P_BEGIN;
	bool good_packet = false;
	bool bad_packet = false;
	n=0;
	while(!good_packet && !bad_packet)
	{
		const char *string=lirc_read_string(lstate, sockfd);
		if(string==nullptr) return(-1);
		lirc_printf(lstate, "%s: read response: %s\n",
			    lstate->lirc_prog, string);
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
				good_packet = true;
				break;
			}
			else if(strcasecmp(string,"ERROR")==0)
			{
				lirc_printf(lstate, "%s: command failed: %s",
					    lstate->lirc_prog, command);
				status=LIRC_RET_ERROR;
			}
			else
			{
				bad_packet = true;
				break;
			}
			state=P_DATA;
			break;
		case P_DATA:
			if(strcasecmp(string,"END")==0)
			{
				good_packet = true;
				break;
			}
			else if(strcasecmp(string,"DATA")==0)
			{
				state=P_N;
				break;
			}
			bad_packet = true;
			break;
		case P_N:
			errno=0;
			data_n=strtoul(string,&endptr,0);
			if(!*string || *endptr)
			{
				bad_packet = true;
				break;
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
			if(buf!=nullptr && written+len+1<max)
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
				good_packet = true;
				break;
			}
			bad_packet = true;
			break;
		}
	}

	if (bad_packet)
	{
		lirc_printf(lstate, "%s: bad return packet\n", lstate->lirc_prog);
		return(-1);
	}

	if(ret_status!=nullptr)
	{
		*ret_status=status;
	}
	if(buf_len!=nullptr)
	{
		*buf_len=written;
	}
	return (int) data_n;
}

int lirc_identify(const struct lirc_state *state, int sockfd)
{
	char* command = static_cast<char*>(malloc((10+strlen(state->lirc_prog)+1+1) * sizeof(char)));
	if (command == nullptr)
		return LIRC_RET_ERROR;
	int success = LIRC_RET_ERROR;

	sprintf(command, "IDENT %s\n", state->lirc_prog);

	(void) lirc_send_command(state, sockfd, command, nullptr, nullptr, &success);
	free(command);
	return success;
}
// NOLINTEND(performance-no-int-to-ptr)
