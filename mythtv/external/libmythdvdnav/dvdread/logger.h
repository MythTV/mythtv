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

#ifndef LIBDVDREAD_LOGGER_H
#define LIBDVDREAD_LOGGER_H

#include "dvdread/attributes.h"

DVDREAD_API void DVDReadLog( void *priv, const dvd_logger_cb *logcb,
                 dvd_logger_level_t level, const char *fmt, ... );

#define LOG(ctx, level, ...) \
  DVDReadLog(ctx->priv, &ctx->logcb, level, __VA_ARGS__)
#define Log0(ctx, ...) LOG(ctx, DVD_LOGGER_LEVEL_ERROR, __VA_ARGS__)
#define Log1(ctx, ...) LOG(ctx, DVD_LOGGER_LEVEL_WARN,  __VA_ARGS__)
#define Log2(ctx, ...) LOG(ctx, DVD_LOGGER_LEVEL_INFO,  __VA_ARGS__)
#define Log3(ctx, ...) LOG(ctx, DVD_LOGGER_LEVEL_DEBUG, __VA_ARGS__)

#endif
