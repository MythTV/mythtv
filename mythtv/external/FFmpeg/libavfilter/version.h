/*
 * Version macros.
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef AVFILTER_VERSION_H
#define AVFILTER_VERSION_H

/**
 * @file
 * Libavfilter version macros
 */

#include "libavutil/avutil.h"

#define LIBAVFILTER_VERSION_MAJOR  2
#define LIBAVFILTER_VERSION_MINOR 77
#define LIBAVFILTER_VERSION_MICRO 100

#define LIBAVFILTER_VERSION_INT AV_VERSION_INT(LIBAVFILTER_VERSION_MAJOR, \
                                               LIBAVFILTER_VERSION_MINOR, \
                                               LIBAVFILTER_VERSION_MICRO)
#define LIBAVFILTER_VERSION     AV_VERSION(LIBAVFILTER_VERSION_MAJOR,   \
                                           LIBAVFILTER_VERSION_MINOR,   \
                                           LIBAVFILTER_VERSION_MICRO)
#define LIBAVFILTER_BUILD       LIBAVFILTER_VERSION_INT

/**
 * Those FF_API_* defines are not part of public API.
 * They may change, break or disappear at any time.
 */
#ifndef FF_API_GRAPH_AVCLASS
#define FF_API_GRAPH_AVCLASS            (LIBAVFILTER_VERSION_MAJOR > 2)
#endif
#ifndef FF_API_SAMPLERATE64
#define FF_API_SAMPLERATE64             (LIBAVFILTER_VERSION_MAJOR < 3)
#endif
#ifndef FF_API_VSRC_BUFFER_ADD_FRAME
#define FF_API_VSRC_BUFFER_ADD_FRAME        (LIBAVFILTER_VERSION_MAJOR < 3)
#endif
#ifndef FF_API_PACKING
#define FF_API_PACKING                  (LIBAVFILTER_VERSION_MAJOR < 3)
#endif
#ifndef FF_API_DEFAULT_CONFIG_OUTPUT_LINK
#define FF_API_DEFAULT_CONFIG_OUTPUT_LINK   (LIBAVFILTER_VERSION_MAJOR < 3)
#endif
#ifndef FF_API_FILTERS_PUBLIC
#define FF_API_FILTERS_PUBLIC               (LIBAVFILTER_VERSION_MAJOR < 3)
#endif

#endif // AVFILTER_VERSION_H
