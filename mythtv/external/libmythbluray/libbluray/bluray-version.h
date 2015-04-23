/*
 * This file is part of libbluray
 * Copyright (C) 2011 hpi1
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

#ifndef BLURAY_VERSION_H_
#define BLURAY_VERSION_H_

#define BLURAY_VERSION_CODE(major, minor, micro) \
    (((major) * 10000) +                         \
     ((minor) *   100) +                         \
     ((micro) *     1))

#define BLURAY_VERSION_MAJOR 2
#define BLURAY_VERSION_MINOR 3
#define BLURAY_VERSION_MICRO 109

#define BLURAY_VERSION_STRING "@BLURAY_VERSION_MAJOR@.@BLURAY_VERSION_MINOR@.@BLURAY_VERSION_MICRO@"

#define BLURAY_VERSION \
    BLURAY_VERSION_CODE(BLURAY_VERSION_MAJOR, BLURAY_VERSION_MINOR, BLURAY_VERSION_MICRO)

#endif /* BLURAY_VERSION_H_ */
