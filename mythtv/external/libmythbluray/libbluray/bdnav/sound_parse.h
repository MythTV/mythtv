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

#if !defined(_SOUND_PARSE_H_)
#define _SOUND_PARSE_H_

#include "util/attributes.h"

#include <stdint.h>

typedef struct {
  uint32_t   sample_rate;
  uint8_t    num_channels;
  uint8_t    bits_per_sample;

  uint32_t   num_frames;
  uint16_t  *samples;       /* LPCM, interleaved */
} SOUND_OBJECT;

typedef struct {
    uint16_t     num_sounds;
    SOUND_OBJECT *sounds;
} SOUND_DATA;

struct bd_disc;

BD_PRIVATE SOUND_DATA* sound_get(struct bd_disc *disc);              /* parse sound.bdmv */
BD_PRIVATE void        sound_free(SOUND_DATA **sound);

#endif // _SOUND_PARSE_H_
