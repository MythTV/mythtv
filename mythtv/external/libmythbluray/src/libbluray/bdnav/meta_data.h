/*
 * This file is part of libbluray
 * Copyright (C) 2010 fraxinas
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

#if !defined(_META_DATA_H_)
#define _META_DATA_H_

#include <stdint.h>

typedef struct meta_thumbnail {
    char *               path;
    uint32_t             xres;
    uint32_t             yres;
} META_THUMBNAIL;

typedef struct meta_title {
    uint32_t             title_number;
    char *               title_name;
} META_TITLE;

typedef struct meta_dl {
    char                 language_code[4];
    char *               filename;
    char *               di_name;
    char *               di_alternative;
    uint8_t              di_num_sets;
    uint8_t              di_set_number;
    uint32_t             toc_count;
    META_TITLE *         toc_entries;
    uint8_t              thumb_count;
    META_THUMBNAIL *     thumbnails;
} META_DL;

typedef struct meta_root {
    uint8_t              dl_count;
    META_DL *            dl_entries;
} META_ROOT;

#endif // _META_DATA_H_

