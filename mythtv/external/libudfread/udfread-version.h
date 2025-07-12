/*
 * This file is part of libudfread
 * Copyright (C) 2014 VLC authors and VideoLAN
 *
 * Authors: Petri Hintukainen <phintuka@users.sourceforge.net>
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

#ifndef UDFREAD_VERSION_H_
#define UDFREAD_VERSION_H_

#ifdef UDFREAD_API_EXPORT
#include "attributes.h"
#elif !defined(UDF_PUBLIC)
#define UDF_PUBLIC
#endif

#define UDFREAD_VERSION_CODE(major, minor, micro) \
    (((major) * 10000) +                         \
     ((minor) *   100) +                         \
     ((micro) *     1))

#define UDFREAD_VERSION_MAJOR 1
#define UDFREAD_VERSION_MINOR 2
#define UDFREAD_VERSION_MICRO 0

#define UDFREAD_VERSION_STRING "1.2.0"

#define UDFREAD_VERSION \
    UDFREAD_VERSION_CODE(UDFREAD_VERSION_MAJOR, UDFREAD_VERSION_MINOR, UDFREAD_VERSION_MICRO)

/**
 *  Get library version
 *
 */
UDF_PUBLIC void udfread_get_version(int *major, int *minor, int *micro);

#endif /* UDFREAD_VERSION_H_ */
