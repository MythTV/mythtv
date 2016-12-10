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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "mount.h"

#include "util/strutl.h"

#include <string.h>

#define _DARWIN_C_SOURCE
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/ucred.h>
#include <sys/mount.h>

char *mount_get_mountpoint(const char *device_path)
{
    struct stat st;
    if (stat (device_path, &st) ) {
        return str_dup(device_path);
    }

    /* If it's a directory, all is good */
    if (S_ISDIR(st.st_mode)) {
        return str_dup(device_path);
    }

    struct statfs mbuf[128];
    int fs_count;

    if ( (fs_count = getfsstat (NULL, 0, MNT_NOWAIT)) != -1 ) {

        getfsstat (mbuf, fs_count * sizeof(mbuf[0]), MNT_NOWAIT);

        for ( int i = 0; i < fs_count; ++i) {
            if (!strcmp (mbuf[i].f_mntfromname, device_path)) {
                return str_dup (mbuf[i].f_mntonname);
            }
        }
    }

    return str_dup (device_path);
}
