/*
 * dct-channel
 * Copyright (c) 2003 Jim Paris <jim@jtan.com>
 *
 * This is free software; you can redistribute it and/or modify it and
 * it is provided under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation; see COPYING.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opt.h"

void opt_init(int *optind) {
	*optind=0;
}

char opt_parse(int argc, char **argv, int *optind, char **optarg,
	       struct options *opt) {
	char c;
	int i;
	(*optind)++;
	if (*optind>=argc)
		return 0;
	
	if (argv[*optind][0]=='-' && 
	   argv[*optind][1]!='-' &&
	   argv[*optind][1]!=0) {
		/* Short option (or a bunch of 'em) */
		/* Save this and shift others over */
		c=argv[*optind][1];
		for(i=2;argv[*optind][i]!=0;i++)
			argv[*optind][i-1]=argv[*optind][i];
		argv[*optind][i-1]=0;
		if (argv[*optind][1]!=0)
			(*optind)--;
		/* Now find it */
		for(i=0;opt[i].shortopt!=0;i++)
			if (opt[i].shortopt==c)
				break;
		if (opt[i].shortopt==0) {
			fprintf(stderr,"Error: unknown option '-%c'\n",c);
			return '?';
		}
		if (opt[i].arg==NULL)
			return c;
		(*optind)++;
		if (*optind>=argc || (argv[*optind][0]=='-' &&
			argv[*optind][1]!=0)) {
			fprintf(stderr,"Error: option '-%c' requires an "
				"argument\n",c);
			return '?';
		} 
		(*optarg)=argv[*optind];
		return c;
	} else if (argv[*optind][0]=='-' &&
		  argv[*optind][1]=='-' &&
		  argv[*optind][2]!=0) {
		/* Long option */
		for(i=0;(c=opt[i].shortopt)!=0;i++)
			if (strcmp(opt[i].longopt,argv[*optind]+2)==0)
				break;
		if (opt[i].shortopt==0) {
			fprintf(stderr,"Error: unknown option '%s'\n",
				argv[*optind]);
			return '?';
		}
		if (opt[i].arg==NULL)
			return c;
		(*optind)++;
		if (*optind>=argc || (argv[*optind][0]=='-' &&
			argv[*optind][1]!=0)) {
			fprintf(stderr,"Error: option '%s' requires an "
				"argument\n",argv[*optind-1]);
			return '?';
		} 
		(*optarg)=argv[*optind];
		return c;
	} else {
		/* End of options */
		return 0;
	}
}

void opt_help(struct options *opt, FILE *out) {
	int i;
	int printed;

	for(i=0;opt[i].shortopt!=0;i++) {
		fprintf(out,"  -%c, --%s%n",opt[i].shortopt,
			opt[i].longopt,&printed);
		fprintf(out," %-*s%s\n",30-printed,
			opt[i].arg?opt[i].arg:"",opt[i].help);
	}
}
