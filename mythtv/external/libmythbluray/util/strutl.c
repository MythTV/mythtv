/*
 * This file is part of libbluray
 * Copyright (C) 2009-2010  John Stebbins
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "strutl.h"

#include "macro.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

char *str_dup(const char *str)
{
    char *dup = NULL;

    if (str) {
        size_t size = strlen(str) + 1;
        dup = malloc(size);
        if (dup) {
            memcpy(dup, str, size);
        }
    }
    return dup;
}

char *str_printf(const char *fmt, ...)
{
    /* Guess we need no more than 100 bytes. */
    va_list ap;
    int     len;
    int     size = 100;
    char   *tmp, *str = NULL;

    while (1) {

        tmp = realloc(str, size);
        if (tmp == NULL) {
            X_FREE(str);
            return NULL;
        }
        str = tmp;

        /* Try to print in the allocated space. */
        va_start(ap, fmt);
        len = vsnprintf(str, size, fmt, ap);
        va_end(ap);

        /* If that worked, return the string. */
        if (len > -1 && len < size) {
            return str;
        }

        /* Else try again with more space. */
        if (len > -1)    /* glibc 2.1 */
            size = len+1; /* precisely what is needed */
        else           /* glibc 2.0 */
            size *= 2;  /* twice the old size */
    }
}

uint32_t str_to_uint32(const char *s, int n)
{
    uint32_t val = 0;

    if (n > 4)
        n = 4;

    if (!s || !*s) {
        return (INT64_C(1) << (8*n)) - 1; /* default: all bits one */
    }

    while (n--) {
        val = (val << 8) | *s;
        if (*s) {
            s++;
        }
    }

    return val;
}

void str_tolower(char *s)
{
    while (*s) {
        *s = tolower(*s);
        s++;
    }
}

char *str_print_hex(char *out, const uint8_t *buf, int count)
{
    int zz;
    for (zz = 0; zz < count; zz++) {
        sprintf(out + (zz * 2), "%02x", buf[zz]);
    }

    return out;
}
