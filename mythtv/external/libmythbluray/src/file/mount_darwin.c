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

#include <DiskArbitration/DADisk.h>

static char *bsdname_get_mountpoint(const char *device_path)
{
    char *result = NULL;

    DASessionRef session = DASessionCreate(kCFAllocatorDefault);
    if (session) {
        DADiskRef disk = DADiskCreateFromBSDName(kCFAllocatorDefault, session, device_path);
        if (disk) {
            CFDictionaryRef desc = DADiskCopyDescription(disk);
            if (desc) {
                // Get Volume path as CFURL
                CFURLRef url = CFDictionaryGetValue(desc, kDADiskDescriptionVolumePathKey);
                if (url) {
                    // Copy Volume path as C char array
                    char tmp_path[PATH_MAX];
                    if (CFURLGetFileSystemRepresentation(url, true, (UInt8*)tmp_path, sizeof(tmp_path))) {
                        result = str_dup(tmp_path);
                    }
                }
                CFRelease(desc);
            }
            CFRelease(disk);
        }
        CFRelease(session);
    }

    return result;
}


char *mount_get_mountpoint(const char *device_path)
{
    struct stat st;
    if (stat(device_path, &st) == 0) {
        // If it's a directory, all is good
        if (S_ISDIR(st.st_mode)) {
            return str_dup(device_path);
        }
    }

    char *mountpoint = bsdname_get_mountpoint(device_path);
    if (mountpoint) {
        return mountpoint;
    }

    return str_dup(device_path);
}
