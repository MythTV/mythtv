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

#ifndef BDJ_PRIVATE_H_
#define BDJ_PRIVATE_H_

#include "register.h"
#include "bluray.h"
#include "bdnav/index_parse.h"
#include <jni.h>

struct bdjava_s {
    BLURAY       *bd;
    BD_REGISTERS *reg;
    INDX_ROOT    *index;

    // JVM library
    void *h_libjvm;

    // JNI
    JavaVM* jvm;

    const char *path;
};

#endif
