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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "register_native.h"

#include "util/logging.h"

#include <jni.h>

static int _register_methods(JNIEnv *env, const char *class_name,
                               const JNINativeMethod *methods, int methods_count)
{
    jclass cls;
    int error;

    (*env)->ExceptionClear(env);

    cls = (*env)->FindClass(env, class_name);

    if (!cls) {
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "Failed to locate class %s\n", class_name);
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
        return 0;
    }

    error =  (*env)->RegisterNatives(env, cls, methods, methods_count);

    if ((*env)->ExceptionOccurred(env)) {
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "Failed to register native methods for class %s\n", class_name);
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
        return 0;
    }

    if (error) {
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "Failed to register native methods for class %s\n", class_name);
    }

    return !error;
}

static int _unregister_methods(JNIEnv *env, const char *class_name)
{
    jclass cls;
    int error;

    (*env)->ExceptionClear(env);

    cls = (*env)->FindClass(env, class_name);

    if (!cls) {
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "Failed to locate class %s\n", class_name);
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
        return 0;
    }

    error =  (*env)->UnregisterNatives(env, cls);

    if ((*env)->ExceptionOccurred(env)) {
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "Failed to unregister native methods for class %s\n", class_name);
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
        return 0;
    }

    if (error) {
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "Failed to unegister native methods for class %s\n", class_name);
    }

    return !error;
}

int bdj_register_native_methods(JNIEnv *env)
{
    extern const JNINativeMethod Java_org_videolan_Logger_methods[];
    extern const JNINativeMethod Java_org_videolan_Libbluray_methods[];
    extern const JNINativeMethod Java_java_awt_BDGraphics_methods[];
    extern const JNINativeMethod Java_java_awt_BDFontMetrics_methods[];
    extern const int Java_org_videolan_Logger_methods_count;
    extern const int Java_org_videolan_Libbluray_methods_count;
    extern const int Java_java_awt_BDGraphics_methods_count;
    extern const int Java_java_awt_BDFontMetrics_methods_count;

    int result = _register_methods(env, "org/videolan/Logger",
        Java_org_videolan_Logger_methods,
        Java_org_videolan_Logger_methods_count);

    result *= _register_methods(env, "org/videolan/Libbluray",
        Java_org_videolan_Libbluray_methods,
        Java_org_videolan_Libbluray_methods_count);

      /* BDFontMetrics must be registered before BDGraphics */
    result *= _register_methods(env, "java/awt/BDFontMetrics",
        Java_java_awt_BDFontMetrics_methods,
        Java_java_awt_BDFontMetrics_methods_count);

    result *= _register_methods(env, "java/awt/BDGraphicsBase",
        Java_java_awt_BDGraphics_methods,
        Java_java_awt_BDGraphics_methods_count);

    return result;
}

void bdj_unregister_native_methods(JNIEnv *env)
{
    _unregister_methods(env, "java/awt/BDGraphicsBase");
    _unregister_methods(env, "java/awt/BDFontMetrics");
    _unregister_methods(env, "org/videolan/Libbluray");
    _unregister_methods(env, "org/videolan/Logger");
}
