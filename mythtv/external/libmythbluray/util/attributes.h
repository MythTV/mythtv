/*
 * This file is part of libbluray
 * Copyright (C) 2010  hpi1
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

#ifndef LIBBLURAY_ATTRIBUTES_H_
#define LIBBLURAY_ATTRIBUTES_H_

#if defined(__GNUC__) && (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 3 ))
#    define BD_ATTR_FORMAT_PRINTF(format,var) __attribute__((__format__(__printf__,format,var)))
#    define BD_ATTR_MALLOC                    __attribute__((__malloc__))
#    define BD_ATTR_PACKED                    __attribute__((packed))
#else
#    define BD_ATTR_FORMAT_PRINTF(format,var)
#    define BD_ATTR_MALLOC
#    define BD_ATTR_PACKED
#endif

#if defined(_WIN32)
#    if defined(__GNUC__)
#        define BD_PUBLIC  __attribute__((dllexport))
#        define BD_PRIVATE
#    else
#        define BD_PUBLIC  __declspec(dllexport)
#        define BD_PRIVATE
#    endif
#elif defined(__GNUC__) && __GNUC__ >= 4
#    define BD_PUBLIC  __attribute__((visibility("default")))
#    define BD_PRIVATE __attribute__((visibility("hidden")))
#else
#    define BD_PUBLIC
#    define BD_PRIVATE
#endif

#endif /* LIBBLURAY_ATTRIBUTES_H_ */
