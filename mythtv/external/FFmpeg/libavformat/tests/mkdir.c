/*
 * Copyright (c) 2026 Jun Zhao
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

#include "config.h"

#include <errno.h>
#include <stdio.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "libavutil/random_seed.h"

#include "libavformat/internal.h"
#include "libavformat/os_support.h"

int main(void)
{
    char parent[64];
    char child[80];

    snprintf(parent, sizeof(parent), "ff-mkdir-test-%08x", av_get_random_seed());
    snprintf(child, sizeof(child), "%s/child", parent);

    if (mkdir(parent, 0755) < 0) {
        perror("mkdir parent");
        return 1;
    }

    if (ff_mkdir_p(child) < 0) {
        perror("ff_mkdir_p");
        rmdir(parent);
        return 1;
    }

    if (rmdir(child) < 0) {
        perror("rmdir child");
        rmdir(parent);
        return 1;
    }

    if (rmdir(parent) < 0) {
        perror("rmdir parent");
        return 1;
    }

    return 0;
}
