/*
 * This file is part of libbluray
 * Copyright (C) 2014  VideoLAN
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

#if !defined(_BD_DISC_ENC_INFO_H_)
#define _BD_DISC_ENC_INFO_H_

#include <stdint.h>

typedef struct bd_enc_info {
    uint8_t  aacs_detected;
    uint8_t  libaacs_detected;
    uint8_t  aacs_handled;
    uint8_t  bdplus_detected;
    uint8_t  libbdplus_detected;
    uint8_t  bdplus_handled;
    int      aacs_error_code;
    int      aacs_mkbv;
    uint8_t  disc_id[20];
    uint8_t  bdplus_gen;
    uint32_t bdplus_date;

    uint8_t  no_menu_support;
} BD_ENC_INFO;

#endif /* _BD_DISC_ENC_INFO_H_ */
