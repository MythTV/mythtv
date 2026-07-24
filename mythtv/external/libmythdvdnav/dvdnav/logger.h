/*
* This file is part of libdvdnav, a DVD navigation library.
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

#ifndef LIBDVDNAV_LOGGER_H
#define LIBDVDNAV_LOGGER_H

DVDNAV_API void dvdnav_log( void *priv, const dvdnav_logger_cb *logcb,
                 dvdnav_logger_level_t level, const char *fmt, ... );

#define LOG(ctx, level, ...) \
  dvdnav_log(ctx->priv, &ctx->logcb, level, __VA_ARGS__)
#define Log0(ctx, ...) LOG(ctx, DVDNAV_LOGGER_LEVEL_ERROR, __VA_ARGS__)
#define Log1(ctx, ...) LOG(ctx, DVDNAV_LOGGER_LEVEL_WARN,  __VA_ARGS__)
#define Log2(ctx, ...) LOG(ctx, DVDNAV_LOGGER_LEVEL_INFO,  __VA_ARGS__)
#define Log3(ctx, ...) LOG(ctx, DVDNAV_LOGGER_LEVEL_DEBUG, __VA_ARGS__)

#endif
