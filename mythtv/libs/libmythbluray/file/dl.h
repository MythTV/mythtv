/*
 * This file is part of libbluray
 * Copyright (C) 2009-2010  Obliter0n
 * Copyright (C) 2009-2010  John Stebbins
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

#ifndef DL_H_
#define DL_H_

#include <util/attributes.h>

// We don't bother aliasing dlopen to dlopen_posix, since only one
// of the .C files will be compiled and linked, the right one for the
// platform.

// Note the dlopen takes just the name part. "aacs", internally we
// translate to "libaacs.so" "libaacs.dylib" or "aacs.dll".
BD_PRIVATE void   *dl_dlopen  ( const char* path, const char *version );
BD_PRIVATE void   *dl_dlsym   ( void* handle, const char* symbol );
BD_PRIVATE int     dl_dlclose ( void* handle );

#endif /* DL_H_ */
