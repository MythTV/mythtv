/*
 * This file is part of libbluray
 * Copyright (C) 2015  Petri Hintukainen
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

#ifndef _BD_UDF_FS_H_
#define _BD_UDF_FS_H_

#include "util/attributes.h"

struct bd_file_s;
struct bd_dir_s;

BD_PRIVATE void *udf_image_open(const char *img_path,
                                void *read_block_handle,
                                 int (*read_blocks)(void *handle, void *buf, int lba, int num_blocks));
BD_PRIVATE void  udf_image_close(void *udf);

BD_PRIVATE const char       *udf_volume_id(void *udf);
BD_PRIVATE struct bd_file_s *udf_file_open(void *udf, const char *filename);
BD_PRIVATE struct bd_dir_s  *udf_dir_open(void *udf, const char* dirname);

#endif /* _BD_UDF_FS_H_ */
