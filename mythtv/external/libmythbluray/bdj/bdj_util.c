/*
 * This file is part of libbluray
 * Copyright (C) 2010  William Hahne
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
 * Lesser General Public License for more details.s
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "util/logging.h"

#include "bdj_util.h"

jobject bdj_make_object(JNIEnv* env, const char* name, const char* sig, ...)
{
    jclass obj_class = (*env)->FindClass(env, name);
    jmethodID obj_constructor = (*env)->GetMethodID(env, obj_class, "<init>", sig);

    if (!obj_class) {
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "Class %s not found\n", name);
        return NULL;
    }

    va_list ap;
    va_start(ap, sig);
    jobject obj = (*env)->NewObjectV(env, obj_class, obj_constructor, ap);
    va_end(ap);

    return obj;
}

jobjectArray bdj_make_array(JNIEnv* env, const char* name, int count)
{
    jclass arr_class = (*env)->FindClass(env, name);
    return (*env)->NewObjectArray(env, count, arr_class, NULL);
}

int bdj_get_method(JNIEnv *env, jclass *cls, jmethodID *method_id,
                   const char *class_name, const char *method_name, const char *method_sig)
{
    *method_id = NULL;
    *cls = (*env)->FindClass(env, class_name);
    if (!*cls) {
        (*env)->ExceptionDescribe(env);
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "Failed to locate class %s\n", class_name);
        return 0;
    }

    *method_id = (*env)->GetStaticMethodID(env, *cls, method_name, method_sig);
    if (!*method_id) {
        (*env)->ExceptionDescribe(env);
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "Failed to locate class %s method %s %s\n",
                 class_name, method_name, method_sig);
        (*env)->DeleteLocalRef(env, *cls);
        *cls = NULL;
        return 0;
    }

    return 1;
}

