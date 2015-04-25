/*
 * This file is part of libbluray
 * Copyright (C) 2012  libbluray
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

#include "util/logging.h"

#include <jni.h>
#include <stdint.h>

#include "org_videolan_Logger.h"

/* Disable some warnings */
#if defined __GNUC__
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#ifdef __cplusplus
#define CPP_EXTERN extern
#else
#define CPP_EXTERN
#endif

JNIEXPORT void JNICALL
Java_org_videolan_Logger_logN(JNIEnv *env, jclass cls, jboolean error, jstring jfile, jint line, jstring string)
{
    const char *msg, *file;
    jsize length;
    uint32_t mask = DBG_BDJ;

    length = (*env)->GetStringLength(env, string);
    if (length <= 0) {
        return;
    }

    msg = (*env)->GetStringUTFChars(env, string, NULL);
    if (!msg) {
        return;
    }

    file = (*env)->GetStringUTFChars(env, jfile, NULL);

    if (error) {
        mask |= DBG_CRIT;
    }

    bd_debug(file ? file : "JVM", line, mask, "%s\n", msg);

    if (file) {
        (*env)->ReleaseStringUTFChars(env, jfile, file);
    }
    (*env)->ReleaseStringUTFChars(env, string, msg);
}

#define CC (char*)(uintptr_t)  /* cast a literal from (const char*) */
#define VC (void*)(uintptr_t)  /* cast function pointer to void* */

BD_PRIVATE CPP_EXTERN const JNINativeMethod
Java_org_videolan_Logger_methods[] =
{ /* AUTOMATICALLY GENERATED */
    {
        CC("logN"),
        CC("(ZLjava/lang/String;ILjava/lang/String;)V"),
        VC(Java_org_videolan_Logger_logN),
    },
};

BD_PRIVATE CPP_EXTERN const int
Java_org_videolan_Logger_methods_count =
     sizeof(Java_org_videolan_Logger_methods)/sizeof(Java_org_videolan_Logger_methods[0]);
