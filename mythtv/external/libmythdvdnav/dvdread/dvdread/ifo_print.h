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

#ifndef LIBDVDREAD_IFO_PRINT_H
#define LIBDVDREAD_IFO_PRINT_H

#include <inttypes.h>

#include <dvdread/attributes.h>
#include <dvdread/ifo_types.h>

DVDREAD_API void ifo_print(dvd_reader_t *dvd, int title);
DVDREAD_API void dvdread_print_time(dvd_time_t *dtime);

/* DVD-Audio ifo prints */
DVDREAD_API void ifoPrint_AMGI_MAT(amgi_mat_t *amgi_mat);
DVDREAD_API void ifoPrint_TIF(tracks_info_table_t *tracks_info_table);
DVDREAD_API void ifoPrint_ATSI_MAT(atsi_mat_t *atsi_mat);
DVDREAD_API void ifoPrint_TT(atsi_title_table_t *atsi_title_table);

#endif /* LIBDVDREAD_IFO_PRINT_H */
