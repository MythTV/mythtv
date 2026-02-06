// -*- mode: C++ ; indent-tabs-mode: t; c-basic-offset: 8 -*-
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
#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
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
	FILE               *m_file   { nullptr };
	std::string         m_name;
	int                 m_line   { 0 };
	struct filestack_t *m_parent { nullptr };
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
static void lirc_perror(const struct lirc_state* /*state*/);
static int lirc_readline(const struct lirc_state *state, std::string& line,FILE *f);
static std::string lirc_trim(const std::string& s);
static void lirc_parse_escape(const struct lirc_state *state, std::string& s,
			      size_t bs, const std::string& name, int line);
static void lirc_parse_string(const struct lirc_state *state, std::string& s,const std::string& name,int line);
static void lirc_parse_include(std::string& s,const std::string& name,int line);
static int lirc_mode(
             const struct lirc_state *state,
             const std::string& token,const std::string& token2,std::string& mode,
		     struct lirc_config_entry **new_config,
		     struct lirc_config_entry **first_config,
		     struct lirc_config_entry **last_config,
		     int (check)(std::string& s),
		     const std::string& name,int line);
/*
  lircrc_config relies on this function, hence don't make it static
  but it's not part of the official interface, so there's no guarantee
  that it will stay available in the future
*/
static unsigned int lirc_flags(const struct lirc_state *state, const std::string& string);
static std::string lirc_getfilename(const struct lirc_state *state,
				    const std::string& file,
				    const std::string& current_file);
static FILE *lirc_open(const struct lirc_state *state,
		       const std::string& file, const std::string& current_file,
		       std::string& full_name);
static struct filestack_t *stack_push(const struct lirc_state *state, struct filestack_t *parent);
static struct filestack_t *stack_pop(struct filestack_t *entry);
static void stack_free(struct filestack_t *entry);
static int lirc_readconfig_only_internal(const struct lirc_state *state,
                                         const std::string& file,
                                         struct lirc_config **config,
                                         int (check)(std::string& s),
                                         std::string& full_name,
                                         std::string& sha_bang);
static std::string lirc_startupmode(const struct lirc_state *state,
							  struct lirc_config_entry *first);
static void lirc_freeconfigentries(struct lirc_config_entry *first);
static void lirc_clearmode(struct lirc_config *config);
static std::string lirc_execute(const struct lirc_state *state,
			  struct lirc_config *config,
			  struct lirc_config_entry *scan);
static int sstrcasecmp(std::string s1, std::string s2);
static int lirc_iscode(struct lirc_config_entry *scan, std::string& remote,
		       std::string& button,unsigned int rep);
static int lirc_code2char_internal(const struct lirc_state *state,
				   struct lirc_config *config,const std::string& code,
				   std::string& string, std::string& prog);
static const char *lirc_read_string(const struct lirc_state *state, int fd);
static int lirc_identify(const struct lirc_state *state, int sockfd);

static int lirc_send_command(const struct lirc_state *state, int sockfd, const std::string& command, char *buf, size_t *buf_len, int *ret_status);

static void lirc_printf(const struct lirc_state *state, const char *format_str, ...)
{
	va_list ap;  
	
	if(state)
	{
		if (!state->lirc_verbose)
			return;
		std::string lformat = state->lirc_prog + ": " + format_str;
		va_start(ap,format_str);
		vfprintf(stderr,lformat.data(),ap);
		va_end(ap);
		return;
	}

	va_start(ap,format_str);
	vfprintf(stderr,format_str,ap);
	va_end(ap);
}

static void lirc_perror(const struct lirc_state *state)
{
	if(!state->lirc_verbose) return;

	perror(state->lirc_prog.data());
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

	auto *state = new lirc_state;
	if(state==nullptr)
	{
		lirc_printf(nullptr, "%s: out of memory\n",prog);
		return nullptr;
	}
	state->lirc_lircd=-1;
	state->lirc_verbose=verbose;

	state->lircrc_root_file=lircrc_root_file;
	state->lircrc_user_file=lircrc_user_file;
	state->lirc_prog=prog;

	if (lircd)
	{
		addr.sun_family=AF_UNIX;
		strncpy(addr.sun_path,lircd,sizeof(addr.sun_path)-1);
		state->lirc_lircd=socket(AF_UNIX,SOCK_STREAM,0);
		if(state->lirc_lircd==-1)
		{
			lirc_printf(state, "could not open socket\n");
			lirc_perror(state);
			lirc_deinit(state);
			return nullptr;
		}
		if(connect(state->lirc_lircd,(struct sockaddr *)&addr,sizeof(addr))==-1)
		{
			close(state->lirc_lircd);
			lirc_printf(state, "could not connect to socket\n");
			lirc_perror(state);
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
	state->lircrc_root_file.clear();
	state->lircrc_user_file.clear();
	state->lirc_prog.clear();
	if (state->lirc_lircd!=-1)
		ret = close(state->lirc_lircd);
	delete state;
	return ret;
}

static int lirc_readline(const struct lirc_state */*state*/, std::string& line,FILE *f)
{
	std::array<char,LIRC_READ+1> newline {};
	line.clear();
	while(true)
	{
		char *ret=fgets(newline.data(),LIRC_READ+1,f);
		if(ret==nullptr)
		{
			return(line.empty() ? -1 : 0);
		}
		line += newline.data();
		if (line.back() == '\n')
		{
			line.pop_back();
			return(0);
		}
	}
}

static std::string lirc_trim(const std::string& s)
{
	const size_t start = s.find_first_not_of(" \t");
	if (start == std::string::npos)
		return {};
	const size_t end = s.find_last_not_of(" \t");
	if (end == std::string::npos)
		return s.substr(start);
	return s.substr(start, end-start+1);
}

/* parse standard C escape sequences + \@,\A-\Z is ^@,^A-^Z */

static void lirc_parse_escape(const struct lirc_state *state, std::string& s,
			      size_t bs, const std::string& name, int line)
{
	unsigned int i = 0;
	size_t count { 1 };

	const char c=s[bs+1];
	switch(c)
	{
	case 'a': i = '\a'; break;
	case 'b': i = '\b'; break;
	case 'e': i = '\e'; break;
	case 'f': i = '\f'; break;
	case 'n': i = '\n'; break;
	case 'r': i = '\r'; break;
	case 't': i = '\t'; break;
	case 'v': i = '\v'; break;
	case '\n':
	case 0:
	case '@':
		s.resize(bs);
		return;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
		try {
			i = std::stoi(s.substr(bs+1), &count, 8);
		}
		catch (std::out_of_range& e) {
			i = 1<<CHAR_BIT;
		}
		if(i>(1<<CHAR_BIT)-1)
		{
			i&=(1<<CHAR_BIT)-1;
			lirc_printf(state, "octal escape sequence "
				    "out of range in %s:%d\n",name.c_str(),line);
		}
		break;
	case 'x':
		try {
			i = std::stoi(s.substr(bs+2), &count, 16);
			count++; // for the 'x'
		}
		catch (std::invalid_argument& e) {
			lirc_printf(state, "\\x used with no "
				    "following hex digits in %s:%d\n",
				    name.c_str(),line);
		}
		catch (std::out_of_range& e) {
			i = 1<<CHAR_BIT;
		}
		if (i == 0)
		{
			s.resize(bs);
			return;
		}
		if(i>(1<<CHAR_BIT)-1)
		{
			i&=(1<<CHAR_BIT)-1;
			lirc_printf(state, "hex escape sequence out "
				    "of range in %s:%d\n",name.c_str(),line);
		}
		break;
	default:
		// '@' used to be here, but you can have a NUL
		// character in a std::string.  Move it earlier to
		// terminate the string to match the old behavior.
		if(c>'@' && c<='Z')
			i = c-'@';
		else
			i = static_cast<uint8_t>(c);
		break;;
	}
	s[bs] = i;
	s.erase(bs+1, count);
}

static void lirc_parse_string(const struct lirc_state *state, std::string& s,const std::string& name,int line)
{
	size_t bs = s.find_first_of('\\');
	while(bs != std::string::npos)
	{
		lirc_parse_escape(state, s, bs, name, line);
		bs = s.find_first_of('\\',bs);
	}
}

static void lirc_parse_include(std::string& s,
                               [[maybe_unused]] const std::string& name,
                               [[maybe_unused]] int line)
{
	if(s.size()<2)
	{
		return;
	}
	if(s.front() != '"' && s.front() != '<')
	{
		return;
	}
	if (((s.front() == '"') && (s.back() != '"')) ||
	    ((s.front() == '<') && (s.back() != '>')))
	{
		return;
	}
	s.pop_back();
	s.erase(0,1);
}

int lirc_mode(const struct lirc_state *state,
	      const std::string& token,const std::string& token2,std::string& mode,
	      struct lirc_config_entry **new_config,
	      struct lirc_config_entry **first_config,
	      struct lirc_config_entry **last_config,
	      int (check)(std::string& s),
	      const std::string& name,int line)
{
	struct lirc_config_entry *new_entry=*new_config;
	if(token == "begin")
	{
		if(token2.empty())
		{
			if(new_entry==nullptr)
			{
				new_entry = new lirc_config_entry;
				if(new_entry==nullptr)
				{
					lirc_printf(state, "out of memory\n");
					return(-1);
				}
                                *new_config=new_entry;
			}
			else
			{
				lirc_printf(state, "bad file format, "
					    "%s:%d\n",name.c_str(),line);
				return(-1);
			}
		}
		else
		{
			if(new_entry==nullptr)
			{
				mode=token2;
			}
			else
			{
				lirc_printf(state, "bad file format, "
					    "%s:%d\n",name.c_str(),line);
				return(-1);
			}
		}
	}
	else if(token == "end")
	{
		if(token2.empty())
		{
			if(new_entry!=nullptr)
			{
#if 0
				if(new_entry->prog.empty())
				{
					lirc_printf(state, "prog missing in "
						    "config before line %d\n",
						    line);
					lirc_freeconfigentries(new_entry);
					*new_config=nullptr;
					return(-1);
				}
				if(sstrcasecmp(new_entry->prog,state->lirc_prog)!=0)
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

				if(!mode.empty())
				{
					new_entry->mode=mode;
				}

				if(check!=nullptr &&
				   sstrcasecmp(new_entry->prog,state->lirc_prog)==0)
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
				lirc_printf(state, "%s:%d: 'end' without "
					    "'begin'\n",name.c_str(),line);
				return(-1);
			}
		}
		else
		{
			if(!mode.empty())
			{
				if(new_entry!=nullptr)
				{
					lirc_printf(state, "%s:%d: missing "
						    "'end' token\n",name.c_str(),line);
					return(-1);
				}
				if(mode == token2)
				{
					mode.clear();
				}
				else
				{
					lirc_printf(state, "\"%s\" doesn't "
						    "match mode \"%s\"\n",
						    token2.c_str(),mode.c_str());
					return(-1);
				}
			}
			else
			{
				lirc_printf(state, "%s:%d: 'end %s' without "
					    "'begin'\n",name.c_str(),line,token2.c_str());
				return(-1);
			}
		}
	}
	else
	{
		lirc_printf(state, "unknown token \"%s\" in %s:%d ignored\n",
			    token.c_str(),name.c_str(),line);
	}
	return(0);
}

unsigned int lirc_flags(const struct lirc_state *state, const std::string& string)
{
	unsigned int flags=none;
	size_t start = string.find_first_not_of(" \t|");
	while(start != std::string::npos)
	{
		size_t end = string.find_first_of(" \t|", start);
		std::string s = string.substr(start,end-start);
		if(s == "once")
		{
			flags|=once;
		}
		else if(s == "quit")
		{
			flags|=quit;
		}
		else if(s == "mode")
		{
			flags|=modex;
		}
		else if(s == "startup_mode")
		{
			flags|=startup_mode;
		}
		else if(s == "toggle_reset")
		{
			flags|=toggle_reset;
		}
		else
		{
			lirc_printf(state, "unknown flag \"%s\"\n",s.c_str());
		}
		start = string.find_first_not_of(" \t|", end);
	}
	return(flags);
}

static std::string lirc_getfilename(const struct lirc_state *state,
				    const std::string& file,
				    const std::string& current_file)
{
	std::string filename;

	if(file.empty())
	{
		const char *home=getenv("HOME");
		if(home==nullptr)
		{
			home="/";
		}
		filename = home;
		if(strlen(home)>0 && filename.back()!='/')
		{
			filename += "/";
		}
		filename += state->lircrc_user_file;
	}
	else if(file.compare(0,2,"~/")==0)
	{
		const char *home=getenv("HOME");
		if(home==nullptr)
		{
			home="/";
		}
		filename = home;
		filename += file.substr(1);
	}
	else if(file[0]=='/' || current_file.empty())
	{
		/* absulute path or root */
		filename = file;
	}
	else
	{
		/* get path from parent filename */
		filename = current_file;
		filename.resize(filename.find_last_of('/'));
		if (file[0] != '/')
			filename += '/';
		filename += file;
	}
	return filename;
}

static FILE *lirc_open(const struct lirc_state *state,
                       const std::string& file, const std::string& current_file,
                       std::string& full_name)
{
	std::string filename=lirc_getfilename(state, file, current_file);
	if(filename.empty())
	{
		return nullptr;
	}

	FILE *fin=fopen(filename.data(),"r");
	if(fin==nullptr && (!file.empty() || errno!=ENOENT))
	{
		lirc_printf(state, "could not open config file %s\n",
			    filename.data());
		lirc_perror(state);
	}
	else if(fin==nullptr)
	{
		fin=fopen(state->lircrc_root_file.data(),"r");
		if(fin==nullptr && errno!=ENOENT)
		{
			lirc_printf(state, "could not open config file %s\n",
				    state->lircrc_root_file.data());
			lirc_perror(state);
		}
		else if(fin==nullptr)
		{
			lirc_printf(state, "could not open config files %s and %s\n",
				    filename.data(),state->lircrc_root_file.data());
			lirc_perror(state);
		}
		else
		{
			filename = state->lircrc_root_file;
			if(filename.empty())
			{
				fclose(fin);
				lirc_printf(state, "out of memory\n");
				return nullptr;
			}
		}
	}
	if(fin!=nullptr)
	{
		full_name = filename;
	}
	return fin;
}

static struct filestack_t *stack_push(const struct lirc_state *state, struct filestack_t *parent)
{
	try {
		auto *entry = new filestack_t;
		entry->m_parent = parent;
		return entry;
	} catch (const std::bad_alloc& e) {
		lirc_printf(state, "out of memory\n");
		return nullptr;
	}
}

static struct filestack_t *stack_pop(struct filestack_t *entry)
{
	struct filestack_t *parent = nullptr;
	if (!entry)
		return nullptr;
	parent = entry->m_parent;
	delete entry;
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
                    const std::string& file,
                    struct lirc_config **config,
                    int (check)(std::string& s))
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
	if(lirc_getsocketname(filename, addr.sun_path, sizeof(addr.sun_path))>sizeof(addr.sun_path))
	{
		lirc_printf(state, "WARNING: file name too long\n");
		return 0;
	}
	sockfd=socket(AF_UNIX,SOCK_STREAM,0);
	if(sockfd==-1)
	{
		lirc_printf(state, "WARNING: could not open socket\n");
		lirc_perror(state);
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
		*config = nullptr;
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
		lirc_printf(state, "WARNING: could not open socket\n");
		lirc_perror(state);
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
	*config = nullptr;
	return -1;
}

int lirc_readconfig_only(const struct lirc_state *state,
                         const std::string& file,
                         struct lirc_config **config,
                         int (check)(std::string& s))
{
	std::string filename;
	std::string sha_bang;
	return lirc_readconfig_only_internal(state, file, config, check, filename, sha_bang);
}

static std::string parse_token (const std::string& string, size_t& next)
{
	if (next == std::string::npos)
		return {};
	size_t start = string.find_first_not_of(" \t", next);
	if (start == std::string::npos) {
		next = std::string::npos;
		return {};
	}
	next = string.find_first_of(" \t", start);
	if (next == std::string::npos)
		return string.substr(start);
	return string.substr(start, next-start);
}

static int lirc_readconfig_only_internal(const struct lirc_state *state,
                                         const std::string& file,
                                         struct lirc_config **config,
                                         int (check)(std::string& s),
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
	std::string dummy;
	filestack->m_file = lirc_open(state, file, dummy, filestack->m_name);
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
	std::string mode;
	std::string remote=LIRC_ALL;
	while (filestack)
	{
		std::string string;
		ret=lirc_readline(state,string,filestack->m_file);
		if(ret==-1)
		{
			fclose(filestack->m_file);
			if(open_files == 1)
			{
				full_name = filestack->m_name;
			}
			filestack = stack_pop(filestack);
			open_files--;
			ret = 0; // this is not an error
			continue;
		}
		/* check for sha-bang */
		if(firstline)
		{
			firstline = 0;
			if(string.compare(0,2,"#!")==0)
			{
				sha_bang=string.substr(2);
			}
		}
		filestack->m_line++;
		const size_t eq = string.find_first_of('=');
		if(eq == std::string::npos)
		{
			size_t next = 0;
			std::string token = parse_token(string, next);
			std::ranges::transform(token, token.begin(),
				       [](char c){return std::tolower(c);  });
			if (token[0]=='#')
			{
				/* ignore empty line or comment */
			}
			else if(token == "include")
			{
				if (open_files >= MAX_INCLUDES)
				{
					lirc_printf(state, "too many files "
						    "included at %s:%d\n",
						    filestack->m_name.c_str(),
						    filestack->m_line);
					ret=-1;
				}
				else
				{
					std::string token2 {};
					if (next != std::string::npos)
						token2 = string.substr(next);
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
						stack_tmp->m_file = lirc_open(state, token2, filestack->m_name, stack_tmp->m_name);
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
				std::string token2 = parse_token(string, next);
				std::ranges::transform(token2, token2.begin(),
					       [](char c){return std::tolower(c);  });
				if (string.find_first_not_of(" \t", next) != std::string::npos)
				{
					lirc_printf(state, "unexpected token in line %s:%d\n",
						    filestack->m_name.c_str(),filestack->m_line);
				}
				else
				{
					ret=lirc_mode(state, token,token2,mode,
						      &new_entry,&first,&last,
						      check,
						      filestack->m_name,
						      filestack->m_line);
					if(ret==0)
					{
						remote=LIRC_ALL;
					}
					else
					{
						mode.clear();
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
			std::string token=lirc_trim(string.substr(0,eq));
			std::string token2=lirc_trim(string.substr(eq+1));
			std::ranges::transform(token, token.begin(),
				       [](char c){return std::tolower(c);  });
			if(token[0]=='#')
			{
				/* ignore comment */
			}
			else if(new_entry==nullptr)
			{
				lirc_printf(state, "bad file format, %s:%d\n",
					    filestack->m_name.c_str(),filestack->m_line);
				ret=-1;
			}
			else
			{
				if(token == "prog")
				{
					new_entry->prog=token2;
				}
				else if(token == "remote")
				{
					if("*" == token2)
					{
						remote=LIRC_ALL;
					}
					else
					{
						remote=token2;
					}
				}
				else if(token == "button")
				{
					auto *code = new lirc_code;
					if(code==nullptr)
					{
						lirc_printf(state, "out of memory\n");
						ret=-1;
					}
					else
					{
						code->remote=remote;
						if("*"== token2)
						{
							code->button=LIRC_ALL;
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
					}
				}
				else if(token == "delay")
				{
					size_t end {0};
					bool fail { false };
					try {
						new_entry->rep_delay=std::stoi(token2,&end,0);
					}
					catch (std::invalid_argument& e) { fail = true; }
					catch (std::out_of_range& e) { fail = true; }
					if(fail || (end == 0) || (token2[end] != 0))
					{
						lirc_printf(state, "\"%s\" not"
							    " a  valid number for "
							    "delay\n",token2.c_str());
					}
				}
				else if(token == "repeat")
				{
					size_t end {0};
					bool fail { false };
					try {
						new_entry->rep=std::stoi(token2,&end,0);
					}
					catch (std::invalid_argument& e) { fail = true; }
					catch (std::out_of_range& e) { fail = true; }
					if(fail || (end == 0) || (token2[end] != 0))
					{
						lirc_printf(state, "\"%s\" not"
							    " a  valid number for "
							    "repeat\n",token2.c_str());
					}
				}
				else if(token == "config")
				{
					auto *new_list = new lirc_list;
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
				else if(token == "mode")
				{
					new_entry->change_mode=token2;
				}
				else if(token == "flags")
				{
					new_entry->flags=lirc_flags(state, token2);
				}
				else
				{
					lirc_printf(state, "unknown token \"%s\" in %s:%d ignored\n",
						    token.c_str(),filestack->m_name.c_str(),filestack->m_line);
				}
			}
		}
		if(ret==-1) break;
	}
	if(new_entry!=nullptr)
	{
		if(ret==0)
		{
			// The mode("end") call uses new_entry so it isn't leaked.
			// NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
			ret=lirc_mode(state, "end", "",mode,&new_entry,
				      &first,&last,check,"",0);
			lirc_printf(state, "warning: end token missing at end "
				    "of file\n");
		}
		else
		{
			lirc_freeconfigentries(new_entry);
			new_entry=nullptr;
		}
	}
	if(!mode.empty())
	{
		if(ret==0)
		{
			lirc_printf(state, "warning: no end token found for mode "
				    "\"%s\"\n",mode.c_str());
		}
		mode.clear();
	}
	if(ret==0)
	{
		*config = new lirc_config;
		if(*config==nullptr)
		{
			lirc_printf(state, "out of memory\n");
			lirc_freeconfigentries(first);
			return(-1);
		}
		(*config)->first=first;
		(*config)->next=first;
		std::string startupmode = lirc_startupmode(state, (*config)->first);
		(*config)->current_mode= startupmode;
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

static std::string lirc_startupmode(const struct lirc_state *state, struct lirc_config_entry *first)
{
	std::string startupmode;
	struct lirc_config_entry *scan=first;

	/* Set a startup mode based on flags=startup_mode */
	while(scan!=nullptr)
	{
		if(scan->flags&startup_mode) {
			if(!scan->change_mode.empty()) {
				startupmode=scan->change_mode;
				/* Remove the startup mode or it confuses lirc mode system */
				scan->change_mode.clear();
				break;
			}
			lirc_printf(state, "startup_mode flags requires 'mode ='\n");
		}
		scan=scan->next;
	}

	/* Set a default mode if we find a mode = client app name */
	if(startupmode.empty()) {
		scan=first;
		while(scan!=nullptr)
		{
			if(sstrcasecmp(state->lirc_prog,scan->mode)==0)
			{
				startupmode=state->lirc_prog;
				break;
			}
			scan=scan->next;
		}
	}

	if(startupmode.empty()) return {};
	scan=first;
	while(scan!=nullptr)
	{
		if(!scan->change_mode.empty() &&
		   ((scan->flags & once) != 0U) &&
		   sstrcasecmp(startupmode,scan->change_mode)==0)
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
		config->current_mode.clear();
		delete config;
	}
}

static void lirc_freeconfigentries(struct lirc_config_entry *first)
{
	struct lirc_config_entry *c=first;
	while(c!=nullptr)
	{
		c->prog.clear();
		c->change_mode.clear();
		c->mode.clear();

		struct lirc_code *code=c->code;
		while(code!=nullptr)
		{
			code->remote.clear();
			code->button.clear();
			struct lirc_code *code_temp=code->next;
			delete code;
			code=code_temp;
		}

                struct lirc_list *list=c->config;
		while(list!=nullptr)
		{
			list->string.clear();
			struct lirc_list *list_temp=list->next;
			delete list;
			list=list_temp;
		}
		struct lirc_config_entry *config_temp=c->next;
		delete c;
		c=config_temp;
	}
}

static void lirc_clearmode(struct lirc_config *config)
{
	if(config->current_mode.empty())
	{
		return;
	}
	struct lirc_config_entry *scan=config->first;
	while(scan!=nullptr)
	{
		if(!scan->change_mode.empty())
		{
			if(sstrcasecmp(scan->change_mode,config->current_mode)==0)
			{
				scan->flags&=~ecno;
			}
		}
		scan=scan->next;
	}
	config->current_mode.clear();
}

static std::string lirc_execute(const struct lirc_state *state,
			  struct lirc_config *config,
			  struct lirc_config_entry *scan)
{
	int do_once=1;
	
	if(scan->flags&modex)
	{
		lirc_clearmode(config);
	}
	if(!scan->change_mode.empty())
	{
		config->current_mode=scan->change_mode;
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
	   sstrcasecmp(scan->prog,state->lirc_prog)==0 &&
	   do_once==1)
	{
		std::string s=scan->next_config->string;
		scan->next_config=scan->next_config->next;
		if(scan->next_config==nullptr)
			scan->next_config=scan->config;
		return(s);
	}
	return {};
}

static int sstrcasecmp(std::string s1, std::string s2)
{
	std::ranges::transform(s1, s1.begin(),
		       [](char c){ return std::tolower(c); });
	std::ranges::transform(s2, s2.begin(),
		       [](char c){ return std::tolower(c); });
	return(strcasecmp(s1.data(), s2.data()));
}

static int lirc_iscode(struct lirc_config_entry *scan, std::string& remote,
		       std::string& button,unsigned int rep)
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
	   sstrcasecmp(scan->next_code->remote,remote)==0)
	{
		if(scan->next_code->button==LIRC_ALL || 
		   sstrcasecmp(scan->next_code->button,button)==0)
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
                           sstrcasecmp(prev->remote,next->remote)==0)
                        {
                                if(prev->button==LIRC_ALL ||
                                   sstrcasecmp(prev->button,next->button)==0)
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
                           sstrcasecmp(prev->remote,remote)==0)
                        {
                                if(prev->button==LIRC_ALL ||
                                   sstrcasecmp(prev->button,button)==0)
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

int lirc_code2char(const struct lirc_state *state, struct lirc_config *config,
		   const std::string& code,std::string& string)
{
	if(config->sockfd!=-1)
	{
		std::string command;
		static std::array<char,LIRC_PACKET_SIZE> s_buf;
		size_t buf_len = s_buf.size();
		int success = LIRC_RET_ERROR;
		
		command = "CODE ";
		command += code;
		command += "\n";
		
		int ret = lirc_send_command(state, config->sockfd, command,
					s_buf.data(), &buf_len, &success);
		if(success == LIRC_RET_SUCCESS)
		{
			if(ret > 0)
			{
				string = s_buf.data();
			}
			else
			{
				string.clear();
			}
			return LIRC_RET_SUCCESS;
		}
		return LIRC_RET_ERROR;
	}
	std::string dummy;
	return lirc_code2char_internal(state, config, code, string, dummy);
}

static int lirc_code2char_internal(const struct lirc_state *state,
                                   struct lirc_config *config, const std::string& code,
                                   std::string& string, std::string& prog)
{
	unsigned int rep = 0;

	string.clear();
	if(sscanf(code.c_str(),"%*20x %20x %*5000s %*5000s\n",&rep)==1)
	{
		// start at first word
		size_t end = code.find_first_of(" \t\n", 0);
		size_t start = code.find_first_not_of(" \t\n",end);
		// at second word
	        end = code.find_first_of(" \t\n",start);
		start = code.find_first_not_of(" \t\n",end);
		// at third word
		end = code.find_first_of(" \t\n",start);
		std::string button = code.substr(start,end-start);
		start = code.find_first_not_of(" \t\n",end);
		// at fourth word
		end = code.find_first_of(" \t\n",start);
		std::string remote = code.substr(start,end-start);

		if(button.empty() || remote.empty())
		{
			return(0);
		}
		
		struct lirc_config_entry *scan=config->next;
		int quit_happened=0;
		std::string s;
		while(scan!=nullptr)
		{
			int exec_level = lirc_iscode(scan,remote,button,rep);
			if(exec_level > 0 &&
			   (scan->mode.empty() ||
			    (!scan->mode.empty() &&
			     sstrcasecmp(scan->mode,config->current_mode)==0)) &&
			   quit_happened==0
			   )
			{
				if(exec_level > 1)
				{
					s=lirc_execute(state,config,scan);
					if(!s.empty())
					{
						prog = scan->prog;
					}
				}
				else
				{
					s.clear();
				}
				if(scan->flags&quit)
				{
					quit_happened=1;
					config->next=nullptr;
					scan=scan->next;
					continue;
				}
				if(!s.empty())
				{
					config->next=scan->next;
					break;
				}
			}
			scan=scan->next;
		}
		if(!s.empty())
		{
			string=s;
			return(0);
		}
	}
	config->next=config->first;
	return(0);
}

size_t lirc_getsocketname(const std::string& filename, char *buf, size_t size)
{
	if(filename.size()+2<=size)
	{
		strcpy(buf, filename.data());
		strcat(buf, "d");
	}
	return filename.size()+2;
}

std::string lirc_getmode(const struct lirc_state *state, struct lirc_config *config)
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
                        return {};
		}
		return {};
	}
	return config->current_mode;
}

std::string lirc_setmode(const struct lirc_state *state, struct lirc_config *config, const std::string& mode)
{
	if(config->sockfd!=-1)
	{
		static std::array<char,LIRC_PACKET_SIZE> s_buf {};
		std::string cmd;
		size_t buf_len = s_buf.size();
		int success = LIRC_RET_ERROR;
		cmd = "SETMODE";
		if (!mode.empty())
			cmd += " " + mode;
		cmd += "\n";

		int ret = lirc_send_command(state, config->sockfd, cmd,
					s_buf.data(), &buf_len, &success);
		if(success == LIRC_RET_SUCCESS)
		{
			if(ret > 0)
			{
				return s_buf.data();
			}
                        return {};
		}
		return {};
	}
	
	config->current_mode = mode;
	return config->current_mode;
}

// This returns a pointer into a static variable.
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
		lirc_printf(state, "protocol error\n");
		return nullptr;
	}
	
	while(end==nullptr)
	{
		if(LIRC_PACKET_SIZE<=s_tail)
		{
			lirc_printf(state, "bad packet\n");
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
			lirc_printf(state, "select() failed\n");
			lirc_perror(state);
			return nullptr;
		}
		if(ret==0)
		{
			lirc_printf(state, "timeout\n");
			return nullptr;
		}
		
		n=read(fd, s_buffer.data()+s_tail, LIRC_PACKET_SIZE-s_tail);
		if(n<=0)
		{
			lirc_printf(state, "read() failed\n");
			lirc_perror(state);
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

int lirc_send_command(const struct lirc_state *lstate, int sockfd, const std::string& command, char *buf, size_t *buf_len, int *ret_status)
{
	size_t end = 0;
	unsigned long n = 0;
	unsigned long data_n=0;
	size_t written=0;
	size_t max=0;
	size_t len = 0;

	if(buf_len!=nullptr)
	{
		max=*buf_len;
	}
	size_t todo=command.size();
	const char *data=command.data();
	lirc_printf(lstate, "sending command: %s", command.data());
	while(todo>0)
	{
		ssize_t done=write(sockfd,(const void *) data,todo);
		if(done<0)
		{
			lirc_printf(lstate, "could not send packet\n");
			lirc_perror(lstate);
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
	bool fail = false;
	n=0;
	while(!good_packet && !bad_packet)
	{
		// This points into a static variable. Do not free.
		const char *string=lirc_read_string(lstate, sockfd);
		if(string==nullptr) return(-1);
		lirc_printf(lstate, "read response: %s\n", string);
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
			if(strncasecmp(string,command.data(),strlen(string))!=0 ||
			   strlen(string)+1!=command.size())
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
				lirc_printf(lstate, "command failed: %s",
					    command.data());
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
			end=0;
			try {
				data_n=std::stoul(string,&end,0);
			}
			catch (std::invalid_argument& /*e*/) { fail = true; }
			catch (std::out_of_range& /*e*/) { fail = true; }
			if(fail || (end == 0) || (string[end] != 0))
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
		lirc_printf(lstate, "bad return packet\n");
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
	std::string command;
	int success = LIRC_RET_ERROR;

	command = "IDENT ";
	command += state->lirc_prog;
	command += "\n";

	(void) lirc_send_command(state, sockfd, command, nullptr, nullptr, &success);
	return success;
}
// NOLINTEND(performance-no-int-to-ptr)
