/*
 * This file is part of libbluray
 * Copyright (C) 2013  Petri Hintukainen <phintuka@users.sourceforge.net>
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

#if !defined(_TEXTST_DECODE_H_)
#define _TEXTST_DECODE_H_

#include "textst.h"

#include "util/attributes.h"
#include "util/bits.h"

#include <stdint.h>

/*
 * segments
 */

BD_PRIVATE int  textst_decode_dialog_style(BITBUFFER *bb, BD_TEXTST_DIALOG_STYLE *p);
BD_PRIVATE int  textst_decode_dialog_presentation(BITBUFFER *bb, BD_TEXTST_DIALOG_PRESENTATION *p);

BD_PRIVATE void textst_free_dialog_style(BD_TEXTST_DIALOG_STYLE **p);
BD_PRIVATE void textst_clean_dialog_presentation(BD_TEXTST_DIALOG_PRESENTATION *p);


#endif // _TEXTST_DECODE_H_
