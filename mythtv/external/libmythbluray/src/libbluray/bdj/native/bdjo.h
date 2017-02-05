/*
 * This file is part of libbluray
 * Copyright (C) 2010  William Hahne
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

#ifndef BDJO_H_
#define BDJO_H_

#include "util/attributes.h"

#include <jni.h>

struct bdjo_data;

/*
 * Note:
 * This function should be called only from Java side.
 * If it is called from native C thread, lot of references are leaked !
 */

BD_PRIVATE jobject bdjo_make_jobj(JNIEnv* env, const struct bdjo_data *bdjo);

#endif /* BDJO_H_ */
