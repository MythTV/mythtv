/*
 * This file is part of libbluray
 * Copyright (C) 2010      William Hahne
 * Copyright (C) 2012-2014 Petri Hintukainen <phintuka@users.sourceforge.net>
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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "bdjo.h"
#include "util.h"

#include "libbluray/bdj/bdjo_parse.h"

#include "libbluray/bluray.h"
#include "libbluray/bluray_internal.h"
#include "libbluray/decoders/overlay.h"
#include "libbluray/disc/disc.h"

#include "file/file.h"
#include "util/logging.h"
#include "util/macro.h"

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

/* this automatically generated header is included to cross-check native function signatures */
#include "org_videolan_Libbluray.h"

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
 * build org.videolan.TitleInfo
 */

static jobject _make_title_info(JNIEnv* env, const BLURAY_TITLE *title, int title_number)
{
    jobject ti = NULL;
    if (title) {
        int title_type = title->bdj ? 2 : 1;
        int playback_type = (!!title->interactive) + ((!!title->bdj) << 1);
        ti = bdj_make_object(env, "org/videolan/TitleInfo",
                             "(IIII)V",
                             title_number, title_type, playback_type, title->id_ref);
    }
    return ti;
}

static jobjectArray _make_title_infos(JNIEnv * env, const BLURAY_DISC_INFO *disc_info)
{
    jobjectArray titleArr = bdj_make_array(env, "org/videolan/TitleInfo", disc_info->num_titles + 2);

    for (unsigned i = 0; i <= disc_info->num_titles; i++) {
        jobject titleInfo = _make_title_info(env, disc_info->titles[i], i);
        (*env)->SetObjectArrayElement(env, titleArr, i, titleInfo);
    }

    jobject titleInfo = _make_title_info(env, disc_info->first_play, 65535);
    (*env)->SetObjectArrayElement(env, titleArr, disc_info->num_titles + 1, titleInfo);

    return titleArr;
}

/*
 * build org.videolan.PlaylistInfo
 */

static jobjectArray _make_stream_array(JNIEnv* env, int count, BLURAY_STREAM_INFO* streams)
{
    jobjectArray streamArr = bdj_make_array(env,
                    "org/videolan/StreamInfo", count);
    for (int i = 0; i < count; i++) {
        BLURAY_STREAM_INFO s = streams[i];
        jstring lang = (*env)->NewStringUTF(env, (char*)s.lang);
        jobject streamObj = bdj_make_object(env, "org/videolan/StreamInfo",
                "(BBBCLjava/lang/String;BB)V", s.coding_type, s.format,
                s.rate, s.char_code, lang, s.aspect, s.subpath_id);
        (*env)->SetObjectArrayElement(env, streamArr, i, streamObj);
    }

    return streamArr;
}

static jobject _make_playlist_info(JNIEnv* env, BLURAY_TITLE_INFO* ti)
{
    jobjectArray marks = bdj_make_array(env, "org/videolan/TIMark",
            ti->mark_count);

    for (uint32_t i = 0; i < ti->mark_count; i++) {
        BLURAY_TITLE_MARK m = ti->marks[i];
        jobject mark = bdj_make_object(env, "org/videolan/TIMark",
                "(IIJJJI)V", m.idx, m.type, m.start, m.duration, m.offset, m.clip_ref);
        (*env)->SetObjectArrayElement(env, marks, i, mark);
    }

    jobjectArray clips = bdj_make_array(env, "org/videolan/TIClip",
            ti->clip_count);

    for (uint32_t i = 0; i < ti->clip_count; i++) {
        BLURAY_CLIP_INFO info = ti->clips[i];

        jobjectArray videoStreams = _make_stream_array(env, info.video_stream_count,
                info.video_streams);

        jobjectArray audioStreams = _make_stream_array(env, info.audio_stream_count,
                info.audio_streams);

        jobjectArray pgStreams = _make_stream_array(env, info.pg_stream_count,
                info.pg_streams);

        jobjectArray igStreams = _make_stream_array(env, info.ig_stream_count,
                info.ig_streams);

        jobjectArray secVideoStreams = _make_stream_array(env, info.sec_video_stream_count,
                info.sec_video_streams);

        jobjectArray secAudioStreams = _make_stream_array(env, info.sec_audio_stream_count,
                info.sec_audio_streams);

        jobject clip = bdj_make_object(env, "org/videolan/TIClip",
                "(I[Lorg/videolan/StreamInfo;[Lorg/videolan/StreamInfo;[Lorg/videolan/StreamInfo;[Lorg/videolan/StreamInfo;[Lorg/videolan/StreamInfo;[Lorg/videolan/StreamInfo;)V",
                i, videoStreams, audioStreams, pgStreams, igStreams, secVideoStreams, secAudioStreams);

        (*env)->SetObjectArrayElement(env, clips, i, clip);
    }

    return bdj_make_object(env, "org/videolan/PlaylistInfo",
            "(IJI[Lorg/videolan/TIMark;[Lorg/videolan/TIClip;)V",
            ti->playlist, ti->duration, ti->angle_count, marks, clips);
}

/*
 *
 */

JNIEXPORT jobjectArray JNICALL Java_org_videolan_Libbluray_getTitleInfosN
  (JNIEnv * env, jclass cls, jlong np)
 {
    BLURAY* bd = (BLURAY*)(intptr_t)np;
    const BLURAY_DISC_INFO *disc_info = bd_get_disc_info(bd);

    BD_DEBUG(DBG_JNI, "getTitleInfosN()\n");

    return  _make_title_infos(env, disc_info);
}

JNIEXPORT jobject JNICALL Java_org_videolan_Libbluray_getPlaylistInfoN
  (JNIEnv * env, jclass cls, jlong np, jint playlist)
{
    BLURAY *bd = (BLURAY*)(intptr_t)np;
    BLURAY_TITLE_INFO* ti;

    BD_DEBUG(DBG_JNI, "getPlaylistInfoN(%d)\n", (int)playlist);

    ti = bd_get_playlist_info(bd, playlist, 0);
    if (!ti)
        return NULL;

    jobject titleInfo = _make_playlist_info(env, ti);

    bd_free_title_info(ti);

    return titleInfo;
}

JNIEXPORT jbyteArray JNICALL Java_org_videolan_Libbluray_getAacsDataN
  (JNIEnv * env, jclass cls, jlong np, jint type)
{
    BLURAY* bd = (BLURAY*)(intptr_t)np;
    const uint8_t *data = bd_get_aacs_data(bd, type);
    size_t data_size = 16;

    BD_DEBUG(DBG_JNI, "getAacsDataN(%d) -> %p\n", (int)type, (const void *)data);

    if (!data) {
        return NULL;
    }
    if (type == 1/*BD_AACS_DISC_ID*/) {
        data_size = 20;
    }
    if (type == 7/*BD_AACS_CONTENT_CERT_ID*/) {
        data_size = 6;
    }
    if (type == 8/*BD_AACS_BDJ_ROOT_CERT_HASH*/) {
        data_size = 20;
    }

    jbyteArray array = (*env)->NewByteArray(env, data_size);
    (*env)->SetByteArrayRegion(env, array, 0, data_size, (const jbyte *)data);
    return array;
}

JNIEXPORT jlong JNICALL Java_org_videolan_Libbluray_getUOMaskN(JNIEnv * env,
        jclass cls, jlong np) {
    BLURAY* bd = (BLURAY*)(intptr_t)np;

    BD_DEBUG(DBG_JNI, "getUOMaskN()\n");
    return bd_get_uo_mask(bd);
}

JNIEXPORT void JNICALL Java_org_videolan_Libbluray_setUOMaskN(JNIEnv * env,
        jclass cls, jlong np, jboolean menuCallMask, jboolean titleSearchMask) {
    BLURAY* bd = (BLURAY*)(intptr_t)np;

    BD_DEBUG(DBG_JNI, "setUOMaskN(%d,%d)\n", (int)menuCallMask, (int)titleSearchMask);
    bd_set_bdj_uo_mask(bd, ((!!menuCallMask) * BDJ_MENU_CALL_MASK) | ((!!titleSearchMask) * BDJ_TITLE_SEARCH_MASK));
}

JNIEXPORT void JNICALL Java_org_videolan_Libbluray_setKeyInterestN(JNIEnv * env,
        jclass cls, jlong np, jint mask) {
    BLURAY* bd = (BLURAY*)(intptr_t)np;

    BD_DEBUG(DBG_JNI, "setKeyInterestN(0x%x)\n", (int)mask);
    bd_set_bdj_kit(bd, mask);
}

JNIEXPORT jint JNICALL Java_org_videolan_Libbluray_setVirtualPackageN(JNIEnv * env,
        jclass cls, jlong np, jstring vpPath, jboolean psr_init_backup) {
    BLURAY* bd = (BLURAY*)(intptr_t)np;
    const char *path = NULL;
    jint result;

    if (vpPath) {
        path = (*env)->GetStringUTFChars(env, vpPath, NULL);
    }

    BD_DEBUG(DBG_JNI|DBG_CRIT, "setVirtualPackageN(%s,%d)\n", path, (int)psr_init_backup);

    result = bd_set_virtual_package(bd, path, (int)psr_init_backup);

    if (vpPath) {
        (*env)->ReleaseStringUTFChars(env, vpPath, path);
    }

    return result;
}

JNIEXPORT jlong JNICALL Java_org_videolan_Libbluray_seekN(JNIEnv * env,
        jclass cls, jlong np, jint playitem, jint playmark, jlong tick) {
    BLURAY* bd = (BLURAY*)(intptr_t)np;

    BD_DEBUG(DBG_JNI, "seekN(tick=%"PRId64", mark=%d, playitem=%d)\n", (int64_t)tick, (int)playmark, (int)playitem);

    return bd_bdj_seek(bd, playitem, playmark, tick);
}

JNIEXPORT jint JNICALL Java_org_videolan_Libbluray_selectPlaylistN(
        JNIEnv * env, jclass cls, jlong np, jint playlist, jint playitem, jint playmark, jlong time) {
    BLURAY* bd = (BLURAY*)(intptr_t)np;

    if (!bd) {
        return 0;
    }

    BD_DEBUG(DBG_JNI, "selectPlaylistN(pl=%d, pi=%d, pm=%d, time=%ld)\n",
             (int)playlist, (int)playitem, (int)playmark, (long)time);

    return bd_play_playlist_at(bd, playlist, playitem, playmark, time);
}

JNIEXPORT jint JNICALL Java_org_videolan_Libbluray_selectTitleN(JNIEnv * env,
        jclass cls, jlong np, jint title) {
    BLURAY* bd = (BLURAY*)(intptr_t)np;

    BD_DEBUG(DBG_JNI, "selectTitleN(%d)\n", (int)title);

    return bd_play_title_internal(bd, title);
}

JNIEXPORT jint JNICALL Java_org_videolan_Libbluray_selectAngleN(JNIEnv * env,
        jclass cls, jlong np, jint angle) {
    BLURAY* bd = (BLURAY*)(intptr_t)np;
    return bd_select_angle(bd, angle - 1);
}

JNIEXPORT jint JNICALL Java_org_videolan_Libbluray_soundEffectN(JNIEnv * env,
        jclass cls, jlong np, jint id) {
    BLURAY* bd = (BLURAY*)(intptr_t)np;
    return bd_bdj_sound_effect(bd, id);
}

JNIEXPORT jlong JNICALL Java_org_videolan_Libbluray_tellTimeN(JNIEnv * env,
        jclass cls, jlong np) {
    BLURAY* bd = (BLURAY*)(intptr_t)np;
    return bd_tell_time(bd);
}

JNIEXPORT jint JNICALL Java_org_videolan_Libbluray_selectRateN(JNIEnv * env,
        jclass cls, jlong np, jfloat rate, jint reason) {
    BLURAY* bd = (BLURAY*)(intptr_t)np;

    BD_DEBUG(DBG_JNI, "selectRateN(%1.1f, %d)\n", (float)rate, (int)reason);

    bd_select_rate(bd, (float)rate, reason);
    return 1;
}

JNIEXPORT jint JNICALL Java_org_videolan_Libbluray_writeGPRN(JNIEnv * env,
        jclass cls, jlong np, jint num, jint value) {
    BLURAY* bd = (BLURAY*)(intptr_t)np;

    BD_DEBUG(DBG_JNI, "writeGPRN(%d,%d)\n", (int)num, (int)value);

    return bd_reg_write(bd, 0, num, value, ~0);
}

JNIEXPORT jint JNICALL Java_org_videolan_Libbluray_readGPRN(JNIEnv * env,
        jclass cls, jlong np, jint num) {
    BLURAY* bd = (BLURAY*)(intptr_t)np;
    int value = bd_reg_read(bd, 0, num);

    BD_DEBUG(DBG_JNI, "readGPRN(%d) -> %d\n", (int)num, (int)value);

    return value;
}

JNIEXPORT jint JNICALL Java_org_videolan_Libbluray_writePSRN(JNIEnv * env,
        jclass cls, jlong np, jint num, jint value, jint mask) {
    BLURAY* bd = (BLURAY*)(intptr_t)np;

    if ((uint32_t)mask == 0xffffffff) {
        BD_DEBUG(DBG_JNI, "writePSRN(%d,%d)\n", (int)num, (int)value);
    } else {
        BD_DEBUG(DBG_JNI, "writePSRN(%d,0x%x,0x%08x)\n", (int)num, (int)value, (int)mask);
    }

    return bd_reg_write(bd, 1, num, value, mask);
}

JNIEXPORT jint JNICALL Java_org_videolan_Libbluray_readPSRN(JNIEnv * env,
        jclass cls, jlong np, jint num) {
    BLURAY* bd = (BLURAY*)(intptr_t)np;
    int value = bd_reg_read(bd, 1, num);

    BD_DEBUG(DBG_JNI, "readPSRN(%d) -> %d\n", (int)num, (int)value);

    return value;
}

JNIEXPORT jint JNICALL Java_org_videolan_Libbluray_cacheBdRomFileN(JNIEnv * env,
                                                                   jclass cls, jlong np,
                                                                   jstring jrel_path, jstring jcache_path) {

    BLURAY *bd = (BLURAY*)(intptr_t)np;
    BD_DISC *disc = bd_get_disc(bd);
    int result = -1;

    const char *rel_path = (*env)->GetStringUTFChars(env, jrel_path, NULL);
    const char *cache_path = (*env)->GetStringUTFChars(env, jcache_path, NULL);
    if (!rel_path || !cache_path) {
        BD_DEBUG(DBG_JNI | DBG_CRIT, "cacheBdRomFile() failed: no path\n");
        goto out;
    }
    BD_DEBUG(DBG_JNI, "cacheBdRomFile(%s => %s)\n", rel_path, cache_path);

    result = disc_cache_bdrom_file(disc, rel_path, cache_path);

 out:
    if (rel_path) {
        (*env)->ReleaseStringUTFChars(env, jrel_path, rel_path);
    }
    if (cache_path) {
        (*env)->ReleaseStringUTFChars(env, jcache_path, cache_path);
    }

    return result;
}

JNIEXPORT jobjectArray JNICALL Java_org_videolan_Libbluray_listBdFilesN(JNIEnv * env,
                                                                        jclass cls, jlong np, jstring jpath,
                                                                        jboolean onlyBdRom) {

    BLURAY *bd = (BLURAY*)(intptr_t)np;
    BD_DISC *disc = bd_get_disc(bd);

    const char *path = (*env)->GetStringUTFChars(env, jpath, NULL);
    if (!path) {
        BD_DEBUG(DBG_JNI | DBG_CRIT, "listBdFilesN() failed: no path\n");
        return NULL;
    }
    BD_DEBUG(DBG_JNI, "listBdFilesN(%s)\n", path);

    /* open directory stream */
    BD_DIR_H *dp;
    if (onlyBdRom) {
        dp = disc_open_bdrom_dir(disc, path);
    } else {
        dp = disc_open_dir(disc, path);
    }
    if (!dp) {
        BD_DEBUG(DBG_JNI, "failed opening directory %s\n", path);
        (*env)->ReleaseStringUTFChars(env, jpath, path);
        return NULL;
    }
    (*env)->ReleaseStringUTFChars(env, jpath, path);

    /* count files and create java strings (java array size must be known when it is created) */
    jstring  *files = NULL;
    unsigned  count = 0;
    unsigned  allocated = 0;
    BD_DIRENT ent;
    while (!dir_read(dp, &ent)) {
        if (strcmp(ent.d_name, ".") && strcmp(ent.d_name, "..")) {
            if (allocated <= count) {
                allocated += 512;
                jstring *tmp = realloc(files, sizeof(*files) * allocated);
                if (!tmp) {
                    BD_DEBUG(DBG_JNI | DBG_CRIT, "failed allocating memory for %u directory entries\n", allocated);
                    break;
                }
                files = tmp;
            }
            files[count] = (*env)->NewStringUTF(env, ent.d_name);
            count++;
        }
    }
    dir_close(dp);

    /* allocate java array */
    jobjectArray arr = bdj_make_array(env, "java/lang/String", count);
    if (!arr) {
        BD_DEBUG(DBG_JNI | DBG_CRIT, "failed creating array [%d]\n", count);
    } else {
        /* populate files to array */
        unsigned ii;
        for (ii = 0; ii < count; ii++) {
            (*env)->SetObjectArrayElement(env, arr, ii, files[ii]);
        }
    }

    X_FREE(files);

    return arr;
}


JNIEXPORT jobject JNICALL Java_org_videolan_Libbluray_getBdjoN(JNIEnv * env,
                                                               jclass cls, jlong np, jstring jfile) {

    BLURAY           *bd = (BLURAY*)(intptr_t)np;
    struct bdjo_data *bdjo;
    jobject           jbdjo = NULL;

    const char *file = (*env)->GetStringUTFChars(env, jfile, NULL);
    if (!file) {
        BD_DEBUG(DBG_JNI | DBG_CRIT, "getBdjoN() failed: no path\n");
        return NULL;
    }
    BD_DEBUG(DBG_JNI, "getBdjoN(%s)\n", file);

    bdjo = bdjo_get(bd_get_disc(bd), file);
    if (bdjo) {
        jbdjo = bdjo_make_jobj(env, bdjo);
        bdjo_free(&bdjo);
    } else {
        BD_DEBUG(DBG_JNI | DBG_CRIT, "getBdjoN(%s) failed\n", file);
    }

    (*env)->ReleaseStringUTFChars(env, jfile, file);

    return jbdjo;
}

static void _updateGraphic(JNIEnv * env,
        BLURAY *bd, jint width, jint height, jintArray rgbArray,
        jint x0, jint y0, jint x1, jint y1,
        BD_ARGB_BUFFER *buf) {

    /* close ? */
    if (!rgbArray) {
        bd_bdj_osd_cb(bd, NULL, (int)width, (int)height, 0, 0, 0, 0);
        return;
    }

    if (buf) {

        /* copy to application-allocated buffer */

        jint y, *dst;
        jsize offset;

        /* set dirty area before lock() */
        buf->dirty[BD_OVERLAY_IG].x0 = (uint16_t)x0;
        buf->dirty[BD_OVERLAY_IG].x1 = (uint16_t)x1;
        buf->dirty[BD_OVERLAY_IG].y0 = (uint16_t)y0;
        buf->dirty[BD_OVERLAY_IG].y1 = (uint16_t)y1;

        /* get buffer */
        if (buf->lock) {
            buf->lock(buf);
        }
        if (!buf->buf[BD_OVERLAY_IG]) {
            BD_DEBUG(DBG_BDJ | DBG_CRIT, "ARGB frame buffer missing\n");
            if (buf->unlock) {
                buf->unlock(buf);
            }
            return;
        }

        /* check buffer size */

        if (buf->width < width || buf->height < height) {
            /* assume buffer is only for the dirty arrea */
            BD_DEBUG(DBG_BDJ, "ARGB frame buffer size is smaller than BD-J frame buffer size (app: %dx%d BD-J: %ldx%ld)\n",
                     buf->width, buf->height, (long)width, (long)height);

            if (buf->width < (x1 - x0 + 1) || buf->height < (y1 - y0 + 1)) {
                BD_DEBUG(DBG_BDJ | DBG_CRIT, "ARGB frame buffer size is smaller than dirty area\n");
                if (buf->unlock) {
                    buf->unlock(buf);
                }
                return;
            }

            dst = (jint*)buf->buf[BD_OVERLAY_IG];

        } else {

            dst = (jint*)buf->buf[BD_OVERLAY_IG] + y0 * buf->width + x0;

            /* clip */
            if (y1 >= buf->height) {
                BD_DEBUG(DBG_BDJ | DBG_CRIT, "Cropping %ld rows from bottom\n", (long)(y1 - buf->height));
                y1 = buf->height - 1;
            }
            if (x1 >= buf->width) {
                BD_DEBUG(DBG_BDJ | DBG_CRIT, "Cropping %ld pixels from right\n", (long)(x1 - buf->width));
                x1 = buf->width - 1;
            }
        }

        /* copy */

        offset = y0 * width + x0;

        for (y = y0; y <= y1; y++) {
            (*env)->GetIntArrayRegion(env, rgbArray, offset, x1 - x0 + 1, dst);
            offset += width;
            dst += buf->width;
        }

        /* check for errors */
        if ((*env)->ExceptionOccurred(env)) {
            BD_DEBUG(DBG_BDJ | DBG_CRIT, "Array access error at %ld (+%ld)\n", (long)offset, (long)(x1 - x0 + 1));
            (*env)->ExceptionDescribe(env);
            (*env)->ExceptionClear(env);
        }

        if (buf->unlock) {
            buf->unlock(buf);
        }

        bd_bdj_osd_cb(bd, buf->buf[BD_OVERLAY_IG], (int)width, (int)height,
                      x0, y0, x1, y1);

    } else {

        /* return java array */

        jint *image = (jint *)(*env)->GetPrimitiveArrayCritical(env, rgbArray, NULL);
        if (image) {
            bd_bdj_osd_cb(bd, (const unsigned *)image, (int)width, (int)height,
                          x0, y0, x1, y1);
            (*env)->ReleasePrimitiveArrayCritical(env, rgbArray, image, JNI_ABORT);
        } else {
            BD_DEBUG(DBG_BDJ | DBG_CRIT, "GetPrimitiveArrayCritical() failed\n");
        }
    }
}

JNIEXPORT void JNICALL Java_org_videolan_Libbluray_updateGraphicN(JNIEnv * env,
        jclass cls, jlong np, jint width, jint height, jintArray rgbArray,
        jint x0, jint y0, jint x1, jint y1) {

    BLURAY* bd = (BLURAY*)(intptr_t)np;

    BD_DEBUG(DBG_JNI, "updateGraphicN(%ld,%ld-%ld,%ld)\n", (long)x0, (long)y0, (long)x1, (long)y1);

    /* app callback not initialized ? */
    if (!bd) {
        return;
    }

    /* nothing to draw ? */
    if (rgbArray && (x1 < x0 || y1 < y0 || (x1 | y1) < 0)) {
        return;
    }

    BD_ARGB_BUFFER *buf = bd_lock_osd_buffer(bd);

    _updateGraphic(env, bd, width, height, rgbArray, x0, y0, x1, y1, buf);

    bd_unlock_osd_buffer(bd);
}

#define CC (char*)(uintptr_t)  /* cast a literal from (const char*) */
#define VC (void*)(uintptr_t)  /* cast function pointer to void* */

BD_PRIVATE CPP_EXTERN const JNINativeMethod
Java_org_videolan_Libbluray_methods[] =
{ /* AUTOMATICALLY GENERATED */
    {
        CC("getAacsDataN"),
        CC("(JI)[B"),
        VC(Java_org_videolan_Libbluray_getAacsDataN),
    },
    {
        CC("getUOMaskN"),
        CC("(J)J"),
        VC(Java_org_videolan_Libbluray_getUOMaskN),
    },
    {
        CC("setUOMaskN"),
        CC("(JZZ)V"),
        VC(Java_org_videolan_Libbluray_setUOMaskN),
    },
    {
        CC("setKeyInterestN"),
        CC("(JI)V"),
        VC(Java_org_videolan_Libbluray_setKeyInterestN),
    },
    {
        CC("getTitleInfosN"),
        CC("(J)[Lorg/videolan/TitleInfo;"),
        VC(Java_org_videolan_Libbluray_getTitleInfosN),
    },
    {
        CC("getPlaylistInfoN"),
        CC("(JI)Lorg/videolan/PlaylistInfo;"),
        VC(Java_org_videolan_Libbluray_getPlaylistInfoN),
    },
    {
        CC("seekN"),
        CC("(JIIJ)J"),
        VC(Java_org_videolan_Libbluray_seekN),
    },
    {
        CC("selectPlaylistN"),
        CC("(JIIIJ)I"),
        VC(Java_org_videolan_Libbluray_selectPlaylistN),
    },
    {
        CC("selectTitleN"),
        CC("(JI)I"),
        VC(Java_org_videolan_Libbluray_selectTitleN),
    },
    {
        CC("setVirtualPackageN"),
        CC("(JLjava/lang/String;Z)I"),
        VC(Java_org_videolan_Libbluray_setVirtualPackageN),
    },
    {
        CC("selectAngleN"),
        CC("(JI)I"),
        VC(Java_org_videolan_Libbluray_selectAngleN),
    },
    {
        CC("soundEffectN"),
        CC("(JI)I"),
        VC(Java_org_videolan_Libbluray_soundEffectN),
    },
    {
        CC("tellTimeN"),
        CC("(J)J"),
        VC(Java_org_videolan_Libbluray_tellTimeN),
    },
    {
        CC("selectRateN"),
        CC("(JFI)I"),
        VC(Java_org_videolan_Libbluray_selectRateN),
    },
    {
        CC("writeGPRN"),
        CC("(JII)I"),
        VC(Java_org_videolan_Libbluray_writeGPRN),
    },
    {
        CC("writePSRN"),
        CC("(JIII)I"),
        VC(Java_org_videolan_Libbluray_writePSRN),
    },
    {
        CC("readGPRN"),
        CC("(JI)I"),
        VC(Java_org_videolan_Libbluray_readGPRN),
    },
    {
        CC("readPSRN"),
        CC("(JI)I"),
        VC(Java_org_videolan_Libbluray_readPSRN),
    },
    {
        CC("cacheBdRomFileN"),
        CC("(JLjava/lang/String;Ljava/lang/String;)I"),
        VC(Java_org_videolan_Libbluray_cacheBdRomFileN),
    },
    {
        CC("listBdFilesN"),
        CC("(JLjava/lang/String;Z)[Ljava/lang/String;"),
        VC(Java_org_videolan_Libbluray_listBdFilesN),
    },
    {
        CC("getBdjoN"),
        CC("(JLjava/lang/String;)Lorg/videolan/bdjo/Bdjo;"),
        VC(Java_org_videolan_Libbluray_getBdjoN),
    },
    {
        CC("updateGraphicN"),
        CC("(JII[IIIII)V"),
        VC(Java_org_videolan_Libbluray_updateGraphicN),
    },
};

BD_PRIVATE CPP_EXTERN const int
Java_org_videolan_Libbluray_methods_count =
    sizeof(Java_org_videolan_Libbluray_methods)/sizeof(Java_org_videolan_Libbluray_methods[0]);

