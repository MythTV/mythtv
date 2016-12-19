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

#include "util/attributes.h"

#include <stdint.h>

/*
 * function pointer types
 */

#ifdef __cplusplus
typedef void    (*fptr_void)(...);
typedef int     (*fptr_int)(...);
typedef int32_t (*fptr_int32)(...);
typedef void*   (*fptr_p_void)(...);
#else
typedef void    (*fptr_void)();
typedef int     (*fptr_int)();
typedef int32_t (*fptr_int32)();
typedef void*   (*fptr_p_void)();
#endif

/*
 * Macro to call function without return value
 */

#define DL_CALL(lib,func,...)                       \
     do {                                           \
          fptr_p_void fptr;                         \
          *(void **)(&fptr) = dl_dlsym(lib, #func); \
          if (fptr) {                               \
              fptr(__VA_ARGS__);                    \
          }                                         \
      } while (0)


// We don't bother aliasing dlopen to dlopen_posix, since only one
// of the .C files will be compiled and linked, the right one for the
// platform.

// Note the dlopen takes just the name part. "aacs", internally we
// translate to "libaacs.so" "libaacs.dylib" or "aacs.dll".
BD_PRIVATE void   *dl_dlopen  ( const char* path, const char *version );
BD_PRIVATE void   *dl_dlsym   ( void* handle, const char* symbol );
BD_PRIVATE int     dl_dlclose ( void* handle );

/*
 * Installation path of currently running libbluray.so
 * returns NULL if unknown.
 *
 * This function is used to help finding libbluray.jar if location
 * is not given in LIBBLURAY_CP environment variable.
 */
BD_PRIVATE const char *dl_get_path(void);

#endif /* DL_H_ */
