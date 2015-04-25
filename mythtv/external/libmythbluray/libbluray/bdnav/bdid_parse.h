/*
 * This file is part of libbluray
 * Copyright (C) 2012  Petri Hintukainen <phintuka@users.sourceforge.net>
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

#if !defined(_BDID_PARSE_H_)
#define _BDID_PARSE_H_

#include "util/attributes.h"

#include <stdint.h>

typedef struct bdid_s {
    char org_id[9];
    char disc_id[33];
} BDID_DATA;


struct bd_disc;

BD_PRIVATE BDID_DATA* bdid_get(struct bd_disc *disc); /* parse id.bdmv */
BD_PRIVATE void       bdid_free(BDID_DATA **p);

#endif // _BDID_PARSE_H_
