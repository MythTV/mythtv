// -*- mode: C++ ; indent-tabs-mode: t; c-basic-offset: 8 -*-
/* NOTE: Extracted from LIRC release 0.8.4a -- dtk */

/****************************************************************************
 ** lirc_client.h ***********************************************************
 ****************************************************************************
 *
 * lirc_client - common routines for lircd clients
 *
 * Copyright (C) 1998 Trent Piepho <xyzzy@u.washington.edu>
 * Copyright (C) 1998 Christoph Bartelmus <lirc@bartelmus.de>
 *
 */ 
 
#ifndef LIRC_CLIENT_H
#define LIRC_CLIENT_H

#ifdef __cplusplus
#include <cstddef>
#else
#include <stddef.h>
#endif

#define LIRC_RET_SUCCESS  (0)
#define LIRC_RET_ERROR   (-1)

#define LIRC_ALL "ALL"

enum lirc_flags {none=0x00,
		 once=0x01,
		 quit=0x02,
		 modex=0x04,
		 ecno=0x08,
		 startup_mode=0x10,
		 toggle_reset=0x20,
};

struct lirc_state
{
	int lirc_lircd {};
	int lirc_verbose {};
	std::string lirc_prog;
	std::string lircrc_root_file;
	std::string lircrc_user_file;
};

struct lirc_list
{
	std::string string;
	struct lirc_list *next {nullptr};
};

struct lirc_code
{
	std::string remote;
	std::string button;
	struct lirc_code *next { nullptr };
};

struct lirc_config
{
	std::string current_mode;
	struct lirc_config_entry *next  { nullptr };
	struct lirc_config_entry *first { nullptr };
	
	int sockfd {};
};

struct lirc_config_entry
{
	std::string prog;
	struct lirc_code *code 	       { nullptr };
	unsigned int rep_delay         { 0 };
	unsigned int rep               { 0 };
	struct lirc_list *config       { nullptr };
	std::string change_mode;
	unsigned int flags             { none };
	
	std::string mode;
	struct lirc_list *next_config  { nullptr };
	struct lirc_code *next_code    { nullptr };

	struct lirc_config_entry *next { nullptr };
};

struct lirc_state *lirc_init(const char *lircrc_root_file,
                             const char *lircrc_user_file,
                             const char *prog, const char *lircd, int verbose);
int lirc_deinit(struct lirc_state *state);

int lirc_readconfig(const struct lirc_state *state,
                    const std::string& file,struct lirc_config **config,
                    int (check)(std::string& s));
void lirc_freeconfig(struct lirc_config *config);

int lirc_code2char(const struct lirc_state *state, struct lirc_config *config,
		   const std::string& code,std::string& string);

/* new interface for client daemon */
int lirc_readconfig_only(const struct lirc_state *state,
                         const std::string& file, struct lirc_config **config,
                         int (check)(std::string& s));
size_t lirc_getsocketname(const std::string& filename, char *buf, size_t size);
std::string lirc_getmode(const struct lirc_state *state, struct lirc_config *config);
std::string lirc_setmode(const struct lirc_state *state, struct lirc_config *config, const std::string& mode);

#endif
