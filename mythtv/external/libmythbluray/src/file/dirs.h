/*
 * This file is part of libbluray
 * Copyright (C) 2011-2013  VideoLAN
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

#ifndef BLURAY_DIRS_H
#define BLURAY_DIRS_H

#include "util/attributes.h"

#ifdef _WIN32
BD_PRIVATE char       *win32_get_font_dir(const char *font_file);
#endif

/*
 * Config, cache and data dirs
 */

BD_PRIVATE const char *file_get_config_system(const char *dir);

BD_PRIVATE char *file_get_config_home(void) BD_ATTR_MALLOC;
BD_PRIVATE char *file_get_cache_home(void) BD_ATTR_MALLOC;
BD_PRIVATE char *file_get_data_home(void) BD_ATTR_MALLOC;

#endif
