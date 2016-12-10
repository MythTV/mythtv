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

#include <jni.h>

#include "util.h"

#include "util/logging.h"
#include "util/macro.h"

#ifdef HAVE_FT2
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

#ifdef HAVE_FONTCONFIG
#include "util/strutl.h"
#include <fontconfig/fontconfig.h>
#endif

#if defined(_WIN32) && defined (HAVE_FT2)
#define NEED_WIN32_FONTS
#endif

#ifdef NEED_WIN32_FONTS
#include "file/dirs.h"  // win32_get_font_dir
#include <windows.h>
#endif

#include "java_awt_BDFontMetrics.h"

/* Disable some warnings */
#if defined __GNUC__
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#ifdef __cplusplus
#define CPP_EXTERN extern
#else
#define CPP_EXTERN
#endif

/*
 * Windows fonts
 */

#ifdef NEED_WIN32_FONTS

typedef struct {
    int   bold;
    int   italic;
    char *filename;
} SEARCH_DATA;

static int CALLBACK EnumFontCallbackW(const ENUMLOGFONTEXW *lpelfe, const NEWTEXTMETRICEX *metric,
                                      DWORD type, LPARAM lParam)
{
    const LOGFONTW *lplf = &lpelfe->elfLogFont;
    const wchar_t *font_name = lpelfe->elfFullName;
    SEARCH_DATA   *data = (SEARCH_DATA *)lParam;
    int            index = 0;
    HKEY           hKey;
    wchar_t        wvalue[MAX_PATH];
    wchar_t        wdata[256];

    if (type & RASTER_FONTTYPE) {
        return 1;
    }

    /* match attributes */
    if (data->italic >= 0 && (!!lplf->lfItalic != !!data->italic)) {
        return 1;
    }
    if (data->bold >= 0 &&
        ((data->bold && lplf->lfWeight <= FW_MEDIUM) ||
         (!data->bold && lplf->lfWeight >= FW_SEMIBOLD))) {
        return 1;
    }

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts",
                     0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return 0;
    }

    while (!data->filename) {
        DWORD wvalue_len = MAX_PATH - 1;
        DWORD wdata_len  = 255;

        LONG result = RegEnumValueW(hKey, index++, wvalue, &wvalue_len,
                                   NULL, NULL, (LPBYTE)wdata, &wdata_len);
        if (result != ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return result;
        }

        if (!_wcsicmp(wvalue, font_name)) {
            size_t len = WideCharToMultiByte(CP_UTF8, 0, wdata, -1, NULL, 0, NULL, NULL);
            if (len != 0) {
                data->filename = (char *)malloc(len);
                if (data->filename) {
                    if (!WideCharToMultiByte(CP_UTF8, 0, wdata, -1, data->filename, len, NULL, NULL)) {
                        data->filename[0] = 0;
                    }
                    break;
                }
            }
        }
    }

    RegCloseKey(hKey);
    return 0;
}

static char *_win32_resolve_font(const char *family, int style)
{
    LOGFONTW lf;
    HDC     hDC;
    SEARCH_DATA data = {style & 2, style & 1, NULL};

    memset(&lf, 0, sizeof(lf));
    lf.lfCharSet = DEFAULT_CHARSET;
    int length = MultiByteToWideChar(CP_UTF8, 0, family, -1, lf.lfFaceName, LF_FACESIZE);
    if (!length) {
        return NULL;
    }

    hDC = GetDC(NULL);
    EnumFontFamiliesExW(hDC, &lf, (FONTENUMPROCW)&EnumFontCallbackW, (LPARAM)&data, 0);
    ReleaseDC(NULL, hDC);

    if (!data.filename) {
        return win32_get_font_dir("arial.ttf");
    }

    if (!strchr(data.filename, '\\')) {
        char *tmp = win32_get_font_dir(data.filename);
        X_FREE(data.filename);
        return tmp;
    }
    return data.filename;
}

#endif /* NEED_WIN32_FONTS */

/*
 * fontconfig
 */

#ifdef HAVE_FONTCONFIG
static FcConfig *_get_fc_lib(JNIEnv * env, jclass cls)
{
    jfieldID fid   = (*env)->GetStaticFieldID(env, cls, "fcLib", "J");
    jlong    fcLib = (*env)->GetStaticLongField (env, cls, fid);
    FcConfig *lib  = (FcConfig *)(intptr_t)fcLib;

    if (lib) {
        return lib;
    }

    lib = FcInitLoadConfigAndFonts();
    (*env)->SetStaticLongField (env, cls, fid, (jlong)(intptr_t)lib);

    if (!lib) {
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "Loading fontconfig failed\n");
    }

    return lib;
}
#endif

#ifdef HAVE_FONTCONFIG
static void _unload_fc_lib(JNIEnv * env, jclass cls)
{
    jfieldID fid   = (*env)->GetStaticFieldID(env, cls, "fcLib", "J");
    jlong    fcLib = (*env)->GetStaticLongField (env, cls, fid);

    if (fcLib) {
        FcConfig *lib  = (FcConfig *)(intptr_t)fcLib;
        (*env)->SetStaticLongField (env, cls, fid, 0);
        FcConfigDestroy(lib);
    }
}
#endif

#ifdef HAVE_FONTCONFIG
static void _fill_fc_pattern(FcPattern *pat, const char *font_family, jint fontStyle)
{
    int weight = (fontStyle & 1) ? FC_WEIGHT_EXTRABOLD : FC_WEIGHT_NORMAL;
    int slant  = (fontStyle & 2) ? FC_SLANT_ITALIC     : FC_SLANT_ROMAN;

    if (strncmp(font_family, "mono", 4)) { /* mono, monospace, monospaced */
        FcPatternAddString(pat, FC_FAMILY, (const FcChar8*)font_family);
    } else {
        FcPatternAddString(pat, FC_FAMILY, (const FcChar8*)"monospace");
    }
    FcPatternAddBool   (pat, FC_OUTLINE, FcTrue);
    FcPatternAddInteger(pat, FC_SLANT,   slant);
    FcPatternAddInteger(pat, FC_WEIGHT,  weight);
}
#endif

#ifdef HAVE_FONTCONFIG
static char *_fontconfig_resolve_font(FcConfig *lib, const char *font_family, jint font_style)
{
    FcResult   result = FcResultMatch;
    FcPattern *pat, *font;
    FcChar8   *fc_filename = NULL;
    char      *filename = NULL;

    pat = FcPatternCreate();
    if (!pat) {
        return NULL;
    }

    _fill_fc_pattern(pat, font_family, font_style);

    FcDefaultSubstitute(pat);
    if (!FcConfigSubstitute(lib, pat, FcMatchPattern)) {
        FcPatternDestroy(pat);
        return NULL;
    }

    font = FcFontMatch(lib, pat, &result);
    FcPatternDestroy(pat);
    if (!font || result == FcResultNoMatch) {
        return NULL;
    }

    if (FcResultMatch == FcPatternGetString(font, FC_FILE, 0, &fc_filename)) {
        filename = str_dup((const char*)fc_filename);
    }
    FcPatternDestroy(font);

    return filename;
}
#endif

/*
 * Font resolver
 */

JNIEXPORT void JNICALL
Java_java_awt_BDFontMetrics_unloadFontConfigN(JNIEnv * env, jclass cls)
{
#ifdef HAVE_FONTCONFIG
    _unload_fc_lib(env, cls);
#endif
}

JNIEXPORT jstring JNICALL
Java_java_awt_BDFontMetrics_resolveFontN(JNIEnv * env, jclass cls, jstring jfont_family, jint font_style)
{
    const char *font_family = (*env)->GetStringUTFChars(env, jfont_family, NULL);
    char       *filename = NULL;
    jstring     jfilename = NULL;

#ifdef HAVE_FONTCONFIG
    FcConfig *lib = _get_fc_lib(env, cls);
    if (lib) {
        filename = _fontconfig_resolve_font(lib, font_family, font_style);
    }
#elif defined(NEED_WIN32_FONTS)
    filename = _win32_resolve_font(font_family, font_style);
#else
    BD_DEBUG(DBG_BDJ | DBG_CRIT, "BD-J font config support not compiled in\n");
#endif

    if (filename) {
        jfilename = (*env)->NewStringUTF(env, (const char*)filename);
        X_FREE(filename);
    }

    (*env)->ReleaseStringUTFChars(env, jfont_family, font_family);
    return jfilename;
}

/*
 * Font metrics (freetype)
 */

JNIEXPORT jlong JNICALL
Java_java_awt_BDFontMetrics_initN(JNIEnv * env, jclass cls)
{
#ifdef HAVE_FT2
    FT_Library ftLib;
    if (!FT_Init_FreeType(&ftLib)) {
        return (jlong)(intptr_t)ftLib;
    }
    BD_DEBUG(DBG_BDJ | DBG_CRIT, "Loading FreeType2 failed\n");
#else
    BD_DEBUG(DBG_BDJ | DBG_CRIT, "BD-J font support not compiled in\n");
#endif
    return 0;
}

JNIEXPORT void JNICALL
Java_java_awt_BDFontMetrics_destroyN(JNIEnv * env, jclass cls, jlong ftLib)
{
#ifdef HAVE_FT2
    FT_Library lib = (FT_Library)(intptr_t)ftLib;

    if (!lib) {
        return;
    }

    FT_Done_FreeType(lib);

    Java_java_awt_BDFontMetrics_unloadFontConfigN(env, cls);
#endif
}

JNIEXPORT jobjectArray JNICALL
Java_java_awt_BDFontMetrics_getFontFamilyAndStyleN(JNIEnv * env, jclass cls, jlong ftLib, jstring fontName)
{
    jobjectArray array = bdj_make_array(env, "java/lang/String", 2);

#ifdef HAVE_FT2
    const char *name;
    FT_Face ftFace;
    FT_Error result;
    FT_Library lib = (FT_Library)(intptr_t)ftLib;
    jstring jfamily, jstyle;

    if (!lib) {
        return NULL;
    }

    name = (*env)->GetStringUTFChars(env, fontName, NULL);
    result = FT_New_Face(lib, name, 0, &ftFace);
    (*env)->ReleaseStringUTFChars(env, fontName, name);
    if (result) {
        return NULL;
    }

    jfamily = (*env)->NewStringUTF(env, ftFace->family_name);
    jstyle  = (*env)->NewStringUTF(env, ftFace->style_name);

    FT_Done_Face(ftFace);

    (*env)->SetObjectArrayElement(env, array, 0, jfamily);
    (*env)->SetObjectArrayElement(env, array, 1, jstyle);
#endif

    return array;
}

JNIEXPORT jlong JNICALL
Java_java_awt_BDFontMetrics_loadFontN(JNIEnv * env, jobject obj, jlong ftLib, jstring fontName, jint size)
{
#ifdef HAVE_FT2
    const char *name;
    FT_Face ftFace;
    FT_Error result;
    jclass cls;
    jfieldID fid;
    FT_Library lib = (FT_Library)(intptr_t)ftLib;

    if (!lib) {
        return 0;
    }

    name = (*env)->GetStringUTFChars(env, fontName, NULL);
    result = FT_New_Face(lib, name, 0, &ftFace);
    (*env)->ReleaseStringUTFChars(env, fontName, name);
    if (result)
        return 0;

    FT_Set_Char_Size(ftFace, 0, size << 6, 0, 0);

    cls = (*env)->GetObjectClass(env, obj);
    fid = (*env)->GetFieldID(env, cls, "ascent", "I");
    (*env)->SetIntField (env, obj, fid, ftFace->size->metrics.ascender >> 6);
    fid = (*env)->GetFieldID(env, cls, "descent", "I");
    (*env)->SetIntField (env, obj, fid, -ftFace->size->metrics.descender >> 6);
    fid = (*env)->GetFieldID(env, cls, "leading", "I");
    (*env)->SetIntField (env, obj, fid, (ftFace->size->metrics.height - ftFace->size->metrics.ascender + ftFace->size->metrics.descender) >> 6);
    fid = (*env)->GetFieldID(env, cls, "maxAdvance", "I");
    (*env)->SetIntField (env, obj, fid, ftFace->size->metrics.max_advance >> 6);

    return (jlong)(intptr_t)ftFace;
#else  /* HAVE_FT2 */
    return 0;
#endif /* HAVE_FT2 */
}

JNIEXPORT void JNICALL
Java_java_awt_BDFontMetrics_destroyFontN(JNIEnv *env, jobject obj, jlong ftFace)
{
#ifdef HAVE_FT2
    FT_Face face = (FT_Face)(intptr_t)ftFace;

    if (!face) {
        return;
    }

    FT_Done_Face(face);
#endif
}

JNIEXPORT jint JNICALL
Java_java_awt_BDFontMetrics_charWidthN(JNIEnv * env, jobject obj, jlong ftFace, jchar c)
{
#ifdef HAVE_FT2
    FT_Face face = (FT_Face)(intptr_t)ftFace;

    if (!face) {
        return 0;
    }

    if (FT_Load_Char(face, c, FT_LOAD_DEFAULT))
        return 0;
    return face->glyph->metrics.horiAdvance >> 6;
#else
    return 0;
#endif
}

JNIEXPORT jint JNICALL
Java_java_awt_BDFontMetrics_stringWidthN(JNIEnv * env, jobject obj, jlong ftFace, jstring string)
{
#ifdef HAVE_FT2
    jsize length;
    const jchar *chars;
    jint i, width;
    FT_Face face = (FT_Face)(intptr_t)ftFace;

    if (!face) {
        return 0;
    }

    length = (*env)->GetStringLength(env, string);
    if (length <= 0)
        return 0;
    chars = (*env)->GetStringCritical(env, string, NULL);
    if (chars == NULL)
        return 0;

    for (i = 0, width = 0; i < length; i++) {
        if (FT_Load_Char(face, chars[i], FT_LOAD_DEFAULT) == 0) {
            width += face->glyph->metrics.horiAdvance >> 6;
        }
    }

    (*env)->ReleaseStringCritical(env, string, chars);

    return width;

#else  /* HAVE_FT2 */
    return 0;
#endif /* HAVE_FT2 */
}

JNIEXPORT jint JNICALL
Java_java_awt_BDFontMetrics_charsWidthN(JNIEnv * env, jobject obj, jlong ftFace, jcharArray charArray,
                                                jint offset, jint length)
{
#ifdef HAVE_FT2
    jchar *chars;
    jint i, width;
    FT_Face face = (FT_Face)(intptr_t)ftFace;

    if (!face) {
        return 0;
    }

    chars = (jchar *)malloc(sizeof(jchar) * length);
    if (chars == NULL)
        return 0;
    (*env)->GetCharArrayRegion(env, charArray, offset, length, chars);
    if ((*env)->ExceptionCheck(env)) {
        X_FREE(chars);
        return 0;
    }

    for (i = 0, width = 0; i < length; i++) {
        if (FT_Load_Char(face, chars[i], FT_LOAD_DEFAULT) == 0) {
            width += face->glyph->metrics.horiAdvance >> 6;
        }
    }

    X_FREE(chars);

    return width;

#else  /* HAVE_FT2 */
    return 0;
#endif /* HAVE_FT2 */
}

#define CC (char*)(uintptr_t)  /* cast a literal from (const char*) */
#define VC (void*)(uintptr_t)  /* cast function pointer to void* */

BD_PRIVATE CPP_EXTERN const JNINativeMethod
Java_java_awt_BDFontMetrics_methods[] =
{ /* AUTOMATICALLY GENERATED */
    {
        CC("initN"),
        CC("()J"),
        VC(Java_java_awt_BDFontMetrics_initN),
    },
    {
        CC("destroyN"),
        CC("(J)V"),
        VC(Java_java_awt_BDFontMetrics_destroyN),
    },
    {
        CC("resolveFontN"),
        CC("(Ljava/lang/String;I)Ljava/lang/String;"),
        VC(Java_java_awt_BDFontMetrics_resolveFontN),
    },
    {
        CC("unloadFontConfigN"),
        CC("()V"),
        VC(Java_java_awt_BDFontMetrics_unloadFontConfigN),
    },
    {
        CC("getFontFamilyAndStyleN"),
        CC("(JLjava/lang/String;)[Ljava/lang/String;"),
        VC(Java_java_awt_BDFontMetrics_getFontFamilyAndStyleN),
    },
    {
        CC("loadFontN"),
        CC("(JLjava/lang/String;I)J"),
        VC(Java_java_awt_BDFontMetrics_loadFontN),
    },
    {
        CC("destroyFontN"),
        CC("(J)V"),
        VC(Java_java_awt_BDFontMetrics_destroyFontN),
    },
    {
        CC("charWidthN"),
        CC("(JC)I"),
        VC(Java_java_awt_BDFontMetrics_charWidthN),
    },
    {
        CC("stringWidthN"),
        CC("(JLjava/lang/String;)I"),
        VC(Java_java_awt_BDFontMetrics_stringWidthN),
    },
    {
        CC("charsWidthN"),
        CC("(J[CII)I"),
        VC(Java_java_awt_BDFontMetrics_charsWidthN),
    },
};

BD_PRIVATE CPP_EXTERN const int
Java_java_awt_BDFontMetrics_methods_count =
    sizeof(Java_java_awt_BDFontMetrics_methods)/sizeof(Java_java_awt_BDFontMetrics_methods[0]);
