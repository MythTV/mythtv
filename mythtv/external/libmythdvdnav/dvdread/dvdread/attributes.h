/*
 * This file is part of libdvdread
 * Copyright (C) 2010-2025 hpi1 and VideoLAN
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

#ifndef LIBDVDREAD_ATTRIBUTES_H
#define LIBDVDREAD_ATTRIBUTES_H

#ifdef DVDREAD_API_EXPORT
#  if defined(_WIN32) || defined(__OS2__)
#    define DVDREAD_API  __declspec(dllexport)
#  elif defined(__GNUC__) || defined(__clang__)
#    define DVDREAD_API  __attribute__((visibility("default")))
#  endif
#else
#  define DVDREAD_API
#endif

#endif /* LIBDVDREAD_ATTRIBUTES_H */
