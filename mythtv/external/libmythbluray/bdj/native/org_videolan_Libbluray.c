#include "org_videolan_Libbluray.h"

#include "bdj/bdj.h"
#include "bdj/bdj_private.h"
#include "bdj/bdj_util.h"
#include "bdj/bdjo_parser.h"
#include "register.h"
#include "bluray.h"

#include "util/strutl.h"
#include "util/macro.h"

/* Disable some warnings */
#if defined __GNUC__
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

static jobject _make_title_info(JNIEnv* env, int title, int objType, int playbackType, const char* bdjoName, int hdmvOID)
{
    jstring name = bdjoName ? (*env)->NewStringUTF(env, bdjoName) : NULL;
    jobject ti = bdj_make_object(env, "org/videolan/TitleInfo",
                                 "(IIILjava/lang/String;I)V",
                                 title, objType, playbackType, name, hdmvOID);
    if (name)
        (*env)->DeleteLocalRef(env, name);
    return ti;
}

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


JNIEXPORT jobject JNICALL Java_org_videolan_Libbluray_getTitleInfoN
  (JNIEnv * env, jclass cls, jlong np, jint title)
{
    BDJAVA* bdj = (BDJAVA*)(intptr_t)np;

    if (title == 65535) {
        if (bdj->index->first_play.object_type == indx_object_type_hdmv)
            return _make_title_info(env, 65535, indx_object_type_hdmv,
                                    bdj->index->first_play.hdmv.playback_type,
                                    NULL,
                                    bdj->index->first_play.hdmv.id_ref);
        else
            return _make_title_info(env, 65535, indx_object_type_bdj,
                                    bdj->index->first_play.bdj.playback_type,
                                    bdj->index->first_play.bdj.name,
                                    -1);
    } else if (title == 0) {
        if (bdj->index->top_menu.object_type == indx_object_type_hdmv)
            return _make_title_info(env, 0, indx_object_type_hdmv,
                                    bdj->index->top_menu.hdmv.playback_type,
                                    NULL,
                                    bdj->index->top_menu.hdmv.id_ref);
        else
            return _make_title_info(env, 0, indx_object_type_bdj,
                                    bdj->index->top_menu.bdj.playback_type,
                                    bdj->index->top_menu.bdj.name,
                                    -1);
    } else if ((title > 0) && (title <= bdj->index->num_titles)) {
        if (bdj->index->titles[title - 1].object_type == indx_object_type_hdmv)
            return _make_title_info(env, title, indx_object_type_hdmv,
                                    bdj->index->titles[title - 1].hdmv.playback_type,
                                    NULL,
                                    bdj->index->titles[title - 1].hdmv.id_ref);
        else
            return _make_title_info(env, title, indx_object_type_bdj,
                                    bdj->index->titles[title - 1].bdj.playback_type,
                                    bdj->index->titles[title - 1].bdj.name,
                                    -1);
    }
        return NULL;
}

JNIEXPORT jobject JNICALL Java_org_videolan_Libbluray_getPlaylistInfoN
  (JNIEnv * env, jclass cls, jlong np, jint playlist)
{
    BDJAVA* bdj = (BDJAVA*)(intptr_t)np;
    BLURAY_TITLE_INFO* ti = bd_get_playlist_info(bdj->bd, playlist, 0);
    if (!ti)
        return NULL;

    jobject titleInfo = _make_playlist_info(env, ti);

    bd_free_title_info(ti);

    return titleInfo;
}

JNIEXPORT jint JNICALL Java_org_videolan_Libbluray_getTitlesN(JNIEnv * env,
                                                              jclass cls, jlong np) {
    BDJAVA* bdj = (BDJAVA*)(intptr_t)np;
    return bdj->index->num_titles;
}

JNIEXPORT jlong JNICALL Java_org_videolan_Libbluray_seekN(JNIEnv * env,
        jclass cls, jlong np, jlong pos) {
    BDJAVA* bdj = (BDJAVA*)(intptr_t)np;
    return bd_seek(bdj->bd, pos);
}

JNIEXPORT jlong JNICALL Java_org_videolan_Libbluray_seekTimeN(JNIEnv * env,
        jclass cls, jlong np, jlong tick) {
    BDJAVA* bdj = (BDJAVA*)(intptr_t)np;
    return bd_seek_time(bdj->bd, tick);
}

JNIEXPORT jlong JNICALL Java_org_videolan_Libbluray_seekChapterN(JNIEnv * env,
        jclass cls, jlong np, jint chapter) {
    BDJAVA* bdj = (BDJAVA*)(intptr_t)np;
    return bd_seek_chapter(bdj->bd, chapter);
}

JNIEXPORT jlong JNICALL Java_org_videolan_Libbluray_chapterPosN(JNIEnv * env,
        jclass cls, jlong np, jint chapter) {
    BDJAVA* bdj = (BDJAVA*)(intptr_t)np;
    return bd_chapter_pos(bdj->bd, chapter);
}

JNIEXPORT jint JNICALL Java_org_videolan_Libbluray_getCurrentChapterN(
        JNIEnv * env, jclass cls, jlong np) {
    BDJAVA* bdj = (BDJAVA*)(intptr_t)np;
    return bd_get_current_chapter(bdj->bd);
}

JNIEXPORT jlong JNICALL Java_org_videolan_Libbluray_seekMarkN(JNIEnv * env,
        jclass cls, jlong np, jint mark) {
    BDJAVA* bdj = (BDJAVA*)(intptr_t)np;
    return bd_seek_mark(bdj->bd, mark);
}

JNIEXPORT jlong JNICALL Java_org_videolan_Libbluray_seekPlayItemN(JNIEnv * env,
        jclass cls, jlong np, jint clip) {
    BDJAVA* bdj = (BDJAVA*)(intptr_t)np;
    return bd_seek_playitem(bdj->bd, clip);
}

JNIEXPORT jint JNICALL Java_org_videolan_Libbluray_selectPlaylistN(
        JNIEnv * env, jclass cls, jlong np, jint playlist) {
    BDJAVA* bdj = (BDJAVA*)(intptr_t)np;
    return bd_select_playlist(bdj->bd, playlist);
}

JNIEXPORT jint JNICALL Java_org_videolan_Libbluray_selectTitleN(JNIEnv * env,
        jclass cls, jlong np, jint title) {
    BDJAVA* bdj = (BDJAVA*)(intptr_t)np;
    return bd_play_title(bdj->bd, title);
}

JNIEXPORT jint JNICALL Java_org_videolan_Libbluray_selectAngleN(JNIEnv * env,
        jclass cls, jlong np, jint angle) {
    BDJAVA* bdj = (BDJAVA*)(intptr_t)np;
    return bd_select_angle(bdj->bd, angle);
}

JNIEXPORT void JNICALL Java_org_videolan_Libbluray_seamlessAngleChangeN(
        JNIEnv * env, jclass cls, jlong np, jint angle) {
    BDJAVA* bdj = (BDJAVA*)(intptr_t)np;
    bd_seamless_angle_change(bdj->bd, angle);
}

JNIEXPORT jlong JNICALL Java_org_videolan_Libbluray_getTitleSizeN(JNIEnv * env,
        jclass cls, jlong np) {
    BDJAVA* bdj = (BDJAVA*)(intptr_t)np;
    return bd_get_title_size(bdj->bd);
}

JNIEXPORT jint JNICALL Java_org_videolan_Libbluray_getCurrentTitleN(
        JNIEnv * env, jclass cls, jlong np) {
    BDJAVA* bdj = (BDJAVA*)(intptr_t)np;
    return bd_psr_read(bdj->reg, PSR_TITLE_NUMBER);
}

JNIEXPORT jint JNICALL Java_org_videolan_Libbluray_getCurrentAngleN(
        JNIEnv * env, jclass cls, jlong np) {
    BDJAVA* bdj = (BDJAVA*)(intptr_t)np;
    return bd_get_current_angle(bdj->bd);
}

JNIEXPORT jlong JNICALL Java_org_videolan_Libbluray_tellN(JNIEnv * env,
        jclass cls, jlong np) {
    BDJAVA* bdj = (BDJAVA*)(intptr_t)np;
    return bd_tell(bdj->bd);
}

JNIEXPORT jlong JNICALL Java_org_videolan_Libbluray_tellTimeN(JNIEnv * env,
        jclass cls, jlong np) {
    BDJAVA* bdj = (BDJAVA*)(intptr_t)np;
    return bd_tell_time(bdj->bd);
}

JNIEXPORT jint JNICALL Java_org_videolan_Libbluray_selectRateN(JNIEnv * env,
        jclass cls, jlong np, jfloat rate) {
    return 1;
}

JNIEXPORT jint JNICALL Java_org_videolan_Libbluray_writeGPRN(JNIEnv * env,
        jclass cls, jlong np, jint num, jint value) {
    BDJAVA* bdj = (BDJAVA*)(intptr_t)np;
    return bd_gpr_write(bdj->reg, num, value);
}

JNIEXPORT jint JNICALL Java_org_videolan_Libbluray_readGPRN(JNIEnv * env,
        jclass cls, jlong np, jint num) {
    BDJAVA* bdj = (BDJAVA*)(intptr_t)np;
    return bd_gpr_read(bdj->reg, num);
}

JNIEXPORT jint JNICALL Java_org_videolan_Libbluray_writePSRN(JNIEnv * env,
        jclass cls, jlong np, jint num, jint value) {
    BDJAVA* bdj = (BDJAVA*)(intptr_t)np;
    return bd_psr_write(bdj->reg, num, value);
}

JNIEXPORT jint JNICALL Java_org_videolan_Libbluray_readPSRN(JNIEnv * env,
        jclass cls, jlong np, jint num) {
    BDJAVA* bdj = (BDJAVA*)(intptr_t)np;
    return bd_psr_read(bdj->reg, num);
}

JNIEXPORT jobject JNICALL Java_org_videolan_Libbluray_getBdjoN(JNIEnv * env,
                                                               jclass cls, jlong np, jstring name) {

  BDJAVA* bdj = (BDJAVA*)(intptr_t)np;
  const char *bdjo_name = (*env)->GetStringUTFChars(env, name, NULL);;
  char* bdjo_path = str_printf("%s%s/%s.bdjo", bdj->path, BDJ_BDJO_PATH, bdjo_name);
  (*env)->ReleaseStringUTFChars(env, name, bdjo_name);
  jobject bdjo = bdjo_read(env, bdjo_path);
  X_FREE(bdjo_path);

  return bdjo;
}

JNIEXPORT void JNICALL Java_org_videolan_Libbluray_updateGraphicN(JNIEnv * env,
        jclass cls, jlong np, jint width, jint height, jintArray rgbArray) {
}
