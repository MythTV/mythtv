/*
 * This file is part of libbluray
 * Copyright (C) 2010  Joakim
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

#include "file.h"


BD_FILE_OPEN bd_register_file(BD_FILE_OPEN p)
{
  BD_FILE_OPEN old = file_open;
  file_open = p;
  return old;
}

BD_DIR_OPEN bd_register_dir(BD_DIR_OPEN p)
{
  BD_DIR_OPEN old = dir_open;
  dir_open = p;
  return old;
}
