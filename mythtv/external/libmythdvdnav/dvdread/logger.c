/*
 * This file is part of libdvdread.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 */

#include "config.h"
#include <stdio.h>
#include <stdarg.h>

#include "dvdread/dvd_reader.h"
#include "logger.h"

#ifdef _WIN32
    #define TTY_DEVICE "CON"
#else
    #define TTY_DEVICE "/dev/tty"
#endif

void DVDReadLog( void *priv, const dvd_logger_cb *logcb,
                 dvd_logger_level_t level, const char *fmt, ... )
{
    va_list list;
    va_start(list, fmt);
    if(logcb && logcb->pf_log)
        logcb->pf_log(priv, level, fmt, list);
    else
    {
        static FILE *tty_stream = NULL;
        if (!tty_stream)
            tty_stream = fopen(TTY_DEVICE, "w");

        FILE *stream = (level == DVD_LOGGER_LEVEL_ERROR) ? stderr : tty_stream;

        /* no controlling terminal so fall back to stderr */
        if (!stream)
            stream = stderr;

        fprintf(stream, "libdvdread: ");
        vfprintf(stream, fmt, list);
        fprintf(stream, "\n");
    }
    va_end(list);
}
