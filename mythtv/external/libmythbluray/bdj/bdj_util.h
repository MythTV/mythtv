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
 * Lesser General Public License for more details.s
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef JNIUTIL_H_
#define JNIUTIL_H_

#include <util/attributes.h>

#include <jni.h>
#include <stdarg.h>

// makes an object from the specified class name and constructor signature
BD_PRIVATE jobject bdj_make_object(JNIEnv* env, const char* name, const char* sig, ...);

// makes an array for the specified class name, all elements are initialized to null
BD_PRIVATE jobjectArray bdj_make_array(JNIEnv* env, const char* name, int count);

// get java method
BD_PRIVATE int bdj_get_method(JNIEnv *env, jclass *cls, jmethodID *method_id,
                              const char *class_name, const char *method_name, const char *method_sig);

#endif
