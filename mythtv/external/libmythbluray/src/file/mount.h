/*
 * This file is part of libbluray
 * Copyright (C) 2014  VideoLAN
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

#ifndef BD_MOUNT_H_
#define BD_MOUNT_H_

#include "util/attributes.h"

/*
 * Resolve device node (/dev/sr0) to mount point
 * Implemented on Linux and MacOSX
 */
BD_PRIVATE char *mount_get_mountpoint(const char *device_path);

#endif /* BD_MOUNT_H_ */
