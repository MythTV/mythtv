/*
 * dct-channel
 * Copyright (c) 2003 Jim Paris <jim@jtan.com>
 *
 * This is free software; you can redistribute it and/or modify it and
 * it is provided under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation; see COPYING.
 */

#ifndef OPT_H
#define OPT_H

#include <stdlib.h>

struct options { 
	char shortopt;
	char *longopt;
	char *arg;
	char *help;
};

void opt_init(int *optind);

char opt_parse(int argc, char **argv, int *optind, char **optarg,
	       struct options *opt);

void opt_help(struct options *opt, FILE *out);

#endif
