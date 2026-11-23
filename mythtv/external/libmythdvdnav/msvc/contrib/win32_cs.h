/*
 * This file is part of libdvdread.
 *
 * libdvdread is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * libdvdread is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with libdvdread; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdlib.h>
#include <windows.h>

static inline wchar_t *_utf8_to_wchar(const char *utf8)
{
  wchar_t *wstr;
  int      wlen;

  wlen = MultiByteToWideChar (CP_UTF8, 0, utf8, -1, NULL, 0);
  if (wlen < 1) {
    return NULL;
  }
  wstr = (wchar_t*)malloc(sizeof(wchar_t) * wlen);
  if (!wstr ) {
    return NULL;
  }
  if (!MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wstr, wlen)) {
    free(wstr);
    return NULL;
  }
  return wstr;
}
