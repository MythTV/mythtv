/*****************************************************************************
 * md5.h: MD5 hash
 *****************************************************************************
 * Copyright © 2004-2011 VLC authors and VideoLAN
 *
 * Authors: Rémi Denis-Courmont
 *          Rafaël Carré
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef LIBDVDREAD_MD5_H
#define LIBDVDREAD_MD5_H

#include <stdint.h>

/**
 * \file
 * This file defines functions and structures to compute MD5 digests
 */

struct md5_s
{
    uint32_t A, B, C, D;          /* chaining variables */
    uint32_t nblocks;
    uint8_t buf[64];
    int count;
};

void InitMD5( struct md5_s * );
void AddMD5( struct md5_s *, const void *, size_t );
void EndMD5( struct md5_s * );

#endif
