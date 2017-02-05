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

#ifdef HAVE_MNTENT_H
#include <string.h>
#include <sys/stat.h>
#include <mntent.h>
#endif

char *mount_get_mountpoint(const char *device_path)
{
#ifndef __ANDROID__
#ifdef HAVE_MNTENT_H
    struct stat st;
    if (stat (device_path, &st) ) {
        return str_dup(device_path);
    }

    /* If it's a directory, all is good */
    if (S_ISDIR(st.st_mode)) {
        return str_dup(device_path);
    }

    FILE *f = setmntent ("/proc/self/mounts", "r");
    if (f) {
        struct mntent* m;
#ifdef HAVE_GETMNTENT_R
        struct mntent mbuf;
        char buf [8192];
        while ((m = getmntent_r (f, &mbuf, buf, sizeof(buf))) != NULL) {
#else
        while ((m = getmntent (f)) != NULL) {
#endif
            if (!strcmp (m->mnt_fsname, device_path)) {
                endmntent (f);
                return str_dup (m->mnt_dir);
            }
        }
        endmntent (f);
    }
#endif /* HAVE_MNTENT_H */
#endif /*  __ANDROID__ */

    return str_dup(device_path);
}
