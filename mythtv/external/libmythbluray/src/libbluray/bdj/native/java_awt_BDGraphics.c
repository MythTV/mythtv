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

#ifdef HAVE_FT2
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

#include "java_awt_BDGraphics.h"

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
Java_java_awt_BDGraphics_drawStringN(JNIEnv * env, jobject obj, jlong ftFace, jstring string, jint x, jint y, jint rgb)
{
#ifdef HAVE_FT2
    jsize length;
    const jchar *chars;
    jclass cls;
    jmethodID mid;
    jint a, c;
    jint i;
    unsigned j, k;
    FT_Face face = (FT_Face)(intptr_t)ftFace;

    if (!face)
        return;

    length = (*env)->GetStringLength(env, string);
    if (length <= 0)
      return;
    chars = (*env)->GetStringCritical(env, string, NULL);
    if (chars == NULL)
      return;

    cls = (*env)->GetObjectClass(env, obj);
    mid = (*env)->GetMethodID(env, cls, "drawPoint", "(III)V");

    a = ((unsigned)rgb >> 24) & 0xff;
    c = rgb & 0xffffff;

    for (i = 0; i < length; i++) {
        if (FT_Load_Char(face, chars[i], FT_LOAD_RENDER) == 0) {
            for (j = 0; j < face->glyph->bitmap.rows; j++) {
                for (k = 0; k < face->glyph->bitmap.width; k++) {
                    jint pixel;
                    pixel = face->glyph->bitmap.buffer[j*face->glyph->bitmap.pitch + k];
                    pixel = ((unsigned)(a * pixel / 255) << 24) | c;
                    (*env)->CallVoidMethod(env, obj, mid,
                                           x + face->glyph->bitmap_left + k,
                                           y - face->glyph->bitmap_top + j,
                                           pixel);
                }
            }
            x += face->glyph->metrics.horiAdvance >> 6;
        }
    }

    (*env)->ReleaseStringCritical(env, string, chars);
#endif /* HAVE_FT2 */
}

#define CC (char*)(uintptr_t)  /* cast a literal from (const char*) */
#define VC (void*)(uintptr_t)  /* cast function pointer to void* */

BD_PRIVATE CPP_EXTERN const JNINativeMethod
Java_java_awt_BDGraphics_methods[] =
{ /* AUTOMATICALLY GENERATED */
    {
        CC("drawStringN"),
        CC("(JLjava/lang/String;III)V"),
        VC(Java_java_awt_BDGraphics_drawStringN),
    },
};

BD_PRIVATE CPP_EXTERN const int
Java_java_awt_BDGraphics_methods_count =
     sizeof(Java_java_awt_BDGraphics_methods)/sizeof(Java_java_awt_BDGraphics_methods[0]);
