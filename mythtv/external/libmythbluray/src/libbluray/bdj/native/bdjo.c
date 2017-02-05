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

/*
 * Note:
 * This module should be called only from Java side.
 * If it is called from native C thread, lot of references are leaked !
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "bdjo.h"

#include "util.h"

#include "libbluray/bdj/bdjo_data.h"

#include "util/logging.h"

#include <jni.h>

/* Documentation: HD Cookbook
 * https://hdcookbook.dev.java.net/
 */

#define JNICHK(a) \
  do {                                                              \
      if ((*env)->ExceptionOccurred(env)) {                         \
          BD_DEBUG(DBG_BDJ | DBG_CRIT, "Exception occured\n");      \
          (*env)->ExceptionDescribe(env);                           \
      }                                                             \
      if (!(a)) {                                                   \
          return NULL;                                              \
      } \
  } while (0)

/*
 *
 */

static jobject _make_terminal_info(JNIEnv* env, const BDJO_TERMINAL_INFO *p)
{
    jstring jdefault_font = (*env)->NewStringUTF(env, p->default_font);
    return bdj_make_object(env, "org/videolan/bdjo/TerminalInfo", "(Ljava/lang/String;IZZ)V",
                           jdefault_font, (jint)p->initial_havi_config_id,
                           (jint)p->menu_call_mask, (jint)p->title_search_mask);
}

static jobject _make_app_cache_info(JNIEnv* env, const BDJO_APP_CACHE_INFO *p)
{
    unsigned ii;

    jobjectArray app_cache_array = bdj_make_array(env, "org/videolan/bdjo/AppCache", p->num_item);
    JNICHK(app_cache_array);

    for (ii = 0; ii < p->num_item; ii++) {
        jstring jref_to_name = (*env)->NewStringUTF(env, p->item[ii].ref_to_name);
        jstring jlang_code = (*env)->NewStringUTF(env, p->item[ii].lang_code);
        JNICHK(jref_to_name);
        JNICHK(jlang_code);

        jobject entry = bdj_make_object(env, "org/videolan/bdjo/AppCache",
                                        "(ILjava/lang/String;Ljava/lang/String;)V",
                                        (jint)p->item[ii].type,
                                        jref_to_name, jlang_code);
        JNICHK(entry);

        (*env)->SetObjectArrayElement(env, app_cache_array, ii, entry);
    }

    return app_cache_array;
}

static jobject _make_accessible_playlists(JNIEnv* env, const BDJO_ACCESSIBLE_PLAYLISTS *p)
{
    unsigned ii;

    jobjectArray playlists = bdj_make_array(env, "java/lang/String", p->num_pl);
    JNICHK(playlists);

    for (ii = 0; ii < p->num_pl; ii++) {
        jstring jplaylist_name = (*env)->NewStringUTF(env, p->pl[ii].name);
        JNICHK(jplaylist_name);

        (*env)->SetObjectArrayElement(env, playlists, ii, jplaylist_name);
    }

    return bdj_make_object(env, "org/videolan/bdjo/PlayListTable", "(ZZ[Ljava/lang/String;)V",
                           (jboolean)p->access_to_all_flag, (jboolean)p->autostart_first_playlist_flag,
                           playlists);
}

static jobject _make_app(JNIEnv* env, const BDJO_APP *p)
{
    unsigned ii;

    jobjectArray profiles = bdj_make_array(env, "org/videolan/bdjo/AppProfile", p->num_profile);
    JNICHK(profiles);

    for (ii = 0; ii < p->num_profile; ii++) {
        jobject profile = bdj_make_object(env, "org/videolan/bdjo/AppProfile", "(SBBB)V",
                                          (jshort)p->profile[ii].profile_number,
                                          (jbyte)p->profile[ii].major_version,
                                          (jbyte)p->profile[ii].minor_version,
                                          (jbyte)p->profile[ii].micro_version);
        JNICHK(profile);

        (*env)->SetObjectArrayElement(env, profiles, ii, profile);
        JNICHK(1);
    }

    jobjectArray app_names = bdj_make_array(env, "[Ljava/lang/String;", p->num_name);
    JNICHK(app_names);

    for (ii = 0; ii < p->num_name; ii++) {
        jstring jlang = (*env)->NewStringUTF(env, p->name[ii].lang);
        JNICHK(jlang);

        jstring jname = (*env)->NewStringUTF(env, p->name[ii].name);
        JNICHK(jname);

        jobjectArray app_name = bdj_make_array(env, "java/lang/String", 2);
        JNICHK(app_name);

        (*env)->SetObjectArrayElement(env, app_name, 0, jlang);
        JNICHK(1);
        (*env)->SetObjectArrayElement(env, app_name, 1, jname);
        JNICHK(1);

        (*env)->SetObjectArrayElement(env, app_names, ii, app_name);
        JNICHK(1);
    }


    jstring icon_locator = (*env)->NewStringUTF(env, p->icon_locator);
    JNICHK(icon_locator);

    jstring base_dir = (*env)->NewStringUTF(env, p->base_dir);
    JNICHK(base_dir);

    jstring classpath_extension = (*env)->NewStringUTF(env, p->classpath_extension);
    JNICHK(classpath_extension);

    jstring initial_class = (*env)->NewStringUTF(env, p->initial_class);
    JNICHK(initial_class);

    jobjectArray params = bdj_make_array(env, "java/lang/String", p->num_param);
    JNICHK(params);

    for (ii = 0; ii < p->num_param; ii++) {
        jstring param = (*env)->NewStringUTF(env, p->param[ii].param);
        JNICHK(param);

        (*env)->SetObjectArrayElement(env, params, ii, param);
        JNICHK(1);
    }

    jobject entry = bdj_make_object(env, "org/videolan/bdjo/AppEntry",
                                    "(IIIS[Lorg/videolan/bdjo/AppProfile;SII[[Ljava/lang/String;Ljava/lang/String;"
                                    "SLjava/lang/String;Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;)V",
                                    (jint)p->control_code, (jint)p->type,
                                    (jint)p->org_id, (jshort)p->app_id,
                                    profiles,
                                    (jshort)p->priority, (jint)p->binding, (jint)p->visibility,
                                    app_names, icon_locator,
                                    (jshort)p->icon_flags,
                                    base_dir, classpath_extension, initial_class, params);

    return entry;
}

static jobjectArray _make_app_management_table(JNIEnv* env, const BDJO_APP_MANAGEMENT_TABLE *p)
{
    unsigned ii;

    jclass entries = bdj_make_array(env, "org/videolan/bdjo/AppEntry", p->num_app);
    JNICHK(entries);

    for (ii = 0; ii < p->num_app; ii++) {
        jobject entry = _make_app(env, &p->app[ii]);
        JNICHK(entry);

        (*env)->SetObjectArrayElement(env, entries, ii, entry);
    }

    return entries;
}

jobject bdjo_make_jobj(JNIEnv* env, const BDJO *p)
{
    jobject terminal_info = _make_terminal_info(env, &p->terminal_info);
    JNICHK(terminal_info);

    jobjectArray app_cache_info = _make_app_cache_info(env, &p->app_cache_info);
    JNICHK(app_cache_info);

    jobject accessible_playlists = _make_accessible_playlists(env, &p->accessible_playlists);
    JNICHK(accessible_playlists);

    jobjectArray app_table = _make_app_management_table(env, &p->app_table);
    JNICHK(app_table);

    jstring file_access_info = (*env)->NewStringUTF(env, p->file_access_info.path);
    JNICHK(file_access_info);

    jint key_interest_table =
      (p->key_interest_table.vk_play                   ) |
      (p->key_interest_table.vk_stop              << 1 ) |
      (p->key_interest_table.vk_ffw               << 2 ) |
      (p->key_interest_table.vk_rew               << 3 ) |
      (p->key_interest_table.vk_track_next        << 4 ) |
      (p->key_interest_table.vk_track_prev        << 5 ) |
      (p->key_interest_table.vk_pause             << 6 ) |
      (p->key_interest_table.vk_still_off         << 7 ) |
      (p->key_interest_table.vk_sec_audio_ena_dis << 8 ) |
      (p->key_interest_table.vk_sec_video_ena_dis << 9 ) |
      (p->key_interest_table.pg_textst_ena_dis    << 10);

    jobject result = bdj_make_object(env, "org/videolan/bdjo/Bdjo",
                                     "(Lorg/videolan/bdjo/TerminalInfo;[Lorg/videolan/bdjo/AppCache;"
                                     "Lorg/videolan/bdjo/PlayListTable;[Lorg/videolan/bdjo/AppEntry;ILjava/lang/String;)V",
                                     terminal_info, app_cache_info, accessible_playlists, app_table,
                                     key_interest_table, file_access_info);

    return result;
}
