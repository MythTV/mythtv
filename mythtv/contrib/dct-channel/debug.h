/*
 * dct-channel
 * Copyright (c) 2003 Jim Paris <jim@jtan.com>
 *
 * This is free software; you can redistribute it and/or modify it and
 * it is provided under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation; see COPYING.
 */

#ifndef DEBUG_H
#define DEBUG_H

extern int verb_count;

#define debug(x...) ({ \
	if (verb_count >= 2) \
		fprintf(stderr,x); \
})

#define verb(x...) ({ \
	if (verb_count >= 1) \
		fprintf(stderr,x); \
})

#endif
