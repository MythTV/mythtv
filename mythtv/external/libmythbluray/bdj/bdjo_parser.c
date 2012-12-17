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

#include "bdjo_parser.h"
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file/file.h"
#include "util/bits.h"
#include "util/macro.h"
#include "bdj_util.h"

/* Documentation: HD Cookbook
 * https://hdcookbook.dev.java.net/
 */

#define JNICHK(a) if((*env)->ExceptionOccurred(env)) { \
    BD_DEBUG(DBG_BDJ, "Exception occured\n"); \
    (*env)->ExceptionDescribe(env); \
    } if (!a) return NULL;

static int _get_version(const uint8_t* str)
{
    if (strcmp((const char*) str, "0100") != 0)
        return 100;
    else if (strcmp((const char*) str, "0200") != 0)
        return 200;
    else
        return BDJ_ERROR;
}

// use when string is already allocated, out should be length + 1
static void _get_string(BITBUFFER* buf, char* out, uint32_t length)
{
    bb_read_bytes(buf, (uint8_t*)out, length);
    out[length] = 0; // add null termination
}

static void _make_string(BITBUFFER* buf, char** out, uint32_t length)
{
    *out = malloc(length + 1);
    bb_read_bytes(buf, (uint8_t*)*out, length);
    (*out)[length] = 0; // add null termination
}

static jstring _read_jstring(JNIEnv* env, BITBUFFER* buf, uint32_t length)
{
    char* str;
    _make_string(buf, &str, length);
    jstring jstr = (*env)->NewStringUTF(env, str);
    free(str);
    return jstr;
}

static jobject _parse_terminal_info(JNIEnv* env, BITBUFFER* buf)
{
    // skip length specifier
    bb_seek(buf, 32, SEEK_CUR);

    char default_font[6];
    _get_string(buf, default_font, 5);
    jstring jdefault_font = (*env)->NewStringUTF(env, default_font);

    jint havi_config = bb_read(buf, 4);
    jboolean menu_call_mask = bb_read(buf, 1);
    jboolean title_search_mask = bb_read(buf, 1);

    bb_seek(buf, 34, SEEK_CUR);

    return bdj_make_object(env, "org/videolan/bdjo/TerminalInfo", "(Ljava/lang/String;IZZ)V",
            jdefault_font, havi_config, menu_call_mask, title_search_mask);
}

static jobject _parse_app_cache_info(JNIEnv* env, BITBUFFER* buf)
{
    // skip length specifier
    bb_seek(buf, 32, SEEK_CUR);

    int count = bb_read(buf, 8);

    jobjectArray app_cache_array = bdj_make_array(env, "org/videolan/bdjo/AppCache", count);
    JNICHK(app_cache_array);

    // skip padding
    bb_seek(buf, 8, SEEK_CUR);


    for (int i = 0; i < count; i++) {
        jint type = bb_read(buf, 8);

        char ref_to_name[6];
        _get_string(buf, ref_to_name, 5);
        jstring jref_to_name = (*env)->NewStringUTF(env, ref_to_name);
        JNICHK(jref_to_name);

        char language_code[4];
        _get_string(buf, language_code, 3);
        jstring jlanguage_code = (*env)->NewStringUTF(env, language_code);
        JNICHK(jlanguage_code);

        jobject entry = bdj_make_object(env, "org/videolan/bdjo/AppCache",
                "(ILjava/lang/String;Ljava/lang/String;)V", type, jref_to_name, jlanguage_code);
        JNICHK(entry);

        (*env)->SetObjectArrayElement(env, app_cache_array, i, entry);

        // skip padding
        bb_seek(buf, 24, SEEK_CUR);
    }

    return app_cache_array;
}

static jobject _parse_accessible_playlists(JNIEnv* env, BITBUFFER* buf)
{
    // skip length specifier
    bb_seek(buf, 32, SEEK_CUR);

    int count = bb_read(buf, 11);
    jobjectArray playlists = bdj_make_array(env, "java/lang/String", count);

    jboolean access_to_all_flag = bb_read(buf, 1);
    jboolean autostart_first_playlist = bb_read(buf, 1);

    // skip padding
    bb_seek(buf, 19, SEEK_CUR);

    for (int i = 0; i < count; i++) {
        char playlist_name[6];
        _get_string(buf, playlist_name, 5);
        jstring jplaylist_name = (*env)->NewStringUTF(env, playlist_name);
        JNICHK(jplaylist_name);

        (*env)->SetObjectArrayElement(env, playlists, i, jplaylist_name);

        // skip padding
        bb_seek(buf, 8, SEEK_CUR);
    }

    return bdj_make_object(env, "org/videolan/bdjo/PlayListTable", "(ZZ[Ljava/lang/String;)V",
            access_to_all_flag, autostart_first_playlist, playlists);
}

static jobjectArray _parse_app_management_table(JNIEnv* env, BITBUFFER* buf)
{
    // skip length specifier
    bb_seek(buf, 32, SEEK_CUR);

    int count = bb_read(buf, 8);

    jclass entries = bdj_make_array(env, "org/videolan/bdjo/AppEntry", count);

    // skip padding
    bb_seek(buf, 8, SEEK_CUR);

    for (int i = 0; i < count; i++) {
        jint control_code = bb_read(buf, 8);
        jint type = bb_read(buf, 4);

        // skip padding
        bb_seek(buf, 4, SEEK_CUR);

        jint organization_id = bb_read(buf, 32);
        jshort application_id = bb_read(buf, 16);

        // skip padding
        bb_seek(buf, 80, SEEK_CUR);

        int profiles_count = bb_read(buf, 4);
        jobjectArray profiles = bdj_make_array(env, "org/videolan/bdjo/AppProfile", profiles_count);
        JNICHK(profiles);

        // skip padding
        bb_seek(buf, 12, SEEK_CUR);

        for (int j = 0; j < profiles_count; j++) {
            jshort profile_num = bb_read(buf, 16);
            jbyte major_version = bb_read(buf, 8);
            jbyte minor_version = bb_read(buf, 8);
            jbyte micro_version = bb_read(buf, 8);

            jobject profile = bdj_make_object(env, "org/videolan/bdjo/AppProfile", "(SBBB)V",
                    profile_num, major_version, minor_version, micro_version);
            JNICHK(profile);

            (*env)->SetObjectArrayElement(env, profiles, j, profile);
            JNICHK(1);

            // skip padding
            bb_seek(buf, 8, SEEK_CUR);
        }

        jint priority = bb_read(buf, 8);
        jint binding = bb_read(buf, 2);
        jint visibility = bb_read(buf, 2);

        // skip padding
        bb_seek(buf, 4, SEEK_CUR);

        uint16_t name_data_length = bb_read(buf, 16);
        jobjectArray app_names = NULL;

        if (name_data_length > 0) {
            // first scan for the number of app names
            int app_name_count = 0;
            uint16_t name_bytes_read = 0;
            while (name_bytes_read < name_data_length) {
                bb_seek(buf, 24, SEEK_CUR);

                uint8_t name_length = bb_read(buf, 8);
                bb_seek(buf, 8*name_length, SEEK_CUR);

                app_name_count++;
                name_bytes_read += 4 + name_length;
            }

            // seek back to beginning of names
            bb_seek(buf, -name_data_length*8, SEEK_CUR);

            app_names = bdj_make_array(env, "[Ljava/lang/String;", app_name_count);
            JNICHK(app_names);

            for (int j = 0; j < app_name_count; j++) {
                char language[4];
                _get_string(buf, language, 3);
                jstring jlanguage = (*env)->NewStringUTF(env, language);
                JNICHK(jlanguage);

                uint8_t name_length = bb_read(buf, 8);
                jstring jname = _read_jstring(env, buf, name_length);
                JNICHK(jname);

                jobjectArray app_name = bdj_make_array(env, "java/lang/String", 2);
                JNICHK(app_name);

                (*env)->SetObjectArrayElement(env, app_name, 0, jlanguage);
                JNICHK(1);
                (*env)->SetObjectArrayElement(env, app_name, 1, jname);
                JNICHK(1);

                (*env)->SetObjectArrayElement(env, app_names, j, app_name);
                JNICHK(1);
            }
        }

        // skip padding to word boundary
        if ((name_data_length & 0x1) != 0) {
            bb_seek(buf, 8, SEEK_CUR);
        }

        uint8_t icon_locator_length = bb_read(buf, 8);
        jstring icon_locator = _read_jstring(env, buf, icon_locator_length);
        JNICHK(icon_locator);

        // skip padding to word boundary
        if ((icon_locator_length & 0x1) == 0) {
            bb_seek(buf, 8, SEEK_CUR);
        }

        jshort icon_flags = bb_read(buf, 16);

        uint8_t base_dir_length = bb_read(buf, 8);
        jstring base_dir = _read_jstring(env, buf, base_dir_length);
        JNICHK(base_dir);

        // skip padding to word boundary
        if ((base_dir_length & 0x1) == 0) {
            bb_seek(buf, 8, SEEK_CUR);
        }

        uint8_t classpath_length = bb_read(buf, 8);
        jstring classpath_extension = _read_jstring(env, buf, classpath_length);
        JNICHK(classpath_extension);

        // skip padding to word boundary
        if ((classpath_length & 0x1) == 0) {
            bb_seek(buf, 8, SEEK_CUR);
        }

        uint8_t initial_class_length = bb_read(buf, 8);
        jstring initial_class = _read_jstring(env, buf, initial_class_length);

        // skip padding to word boundary
        if ((initial_class_length & 0x1) == 0) {
            bb_seek(buf, 8, SEEK_CUR);
        }

        uint8_t param_data_length = bb_read(buf, 8);

        jobjectArray params = NULL;
        if (param_data_length > 0) {
            // first scan for the number of params
            int param_count = 0;
            uint16_t param_bytes_read = 0;
            while (param_bytes_read < param_data_length) {
                uint8_t param_length = bb_read(buf, 8);
                bb_seek(buf, 8*param_length, SEEK_CUR);

                param_count++;
                param_bytes_read += 1 + param_length;
            }

            // seek back to beginning of params
            bb_seek(buf, -param_data_length*8, SEEK_CUR);

            params = bdj_make_array(env, "java/lang/String", param_count);

            for (int j = 0; j < param_count; j++) {
                uint8_t param_length = bb_read(buf, 8);
                jstring param = _read_jstring(env, buf, param_length);
                JNICHK(param);

                (*env)->SetObjectArrayElement(env, params, j, param);
                JNICHK(1);
            }
        }

        // skip padding to word boundary
        if ((param_data_length & 0x1) == 0) {
            bb_seek(buf, 8, SEEK_CUR);
        }

        jobject entry = bdj_make_object(env, "org/videolan/bdjo/AppEntry",
                "(IIIS[Lorg/videolan/bdjo/AppProfile;SII[[Ljava/lang/String;Ljava/lang/String;SLjava/lang/String;Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;)V",
                control_code, type, organization_id, application_id, profiles,
                priority, binding, visibility, app_names, icon_locator,
                icon_flags, base_dir, classpath_extension, initial_class, params);
        JNICHK(entry);

        (*env)->SetObjectArrayElement(env, entries, i, entry);
    }

    return entries;
}

static jobject _parse_bdjo(JNIEnv* env, BITBUFFER* buf)
{
    // first check magic number
    uint8_t magic[4];
    bb_read_bytes(buf, magic, 4);

    if (strncmp((const char*) magic, "BDJO", 4) != 0) {
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "Invalid magic number in BDJO.\n");
        return NULL;
    }

    // get version string
    uint8_t version[4];
    bb_read_bytes(buf, version, 4);
    if (_get_version(version) == BDJ_ERROR) {
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "Invalid version of BDJO.\n");
        return NULL;
    }
    BD_DEBUG(DBG_BDJ, "[bdj] BDJO > Version: %.4s\n", version);

    // skip some unnecessary data
    bb_seek(buf, 8*0x28, SEEK_CUR);

    jobject terminal_info = _parse_terminal_info(env, buf);
    JNICHK(terminal_info);

    jobjectArray app_cache_info = _parse_app_cache_info(env, buf);
    JNICHK(app_cache_info);

    jobject accessible_playlists = _parse_accessible_playlists(env, buf);
    JNICHK(accessible_playlists);

    jobjectArray app_table = _parse_app_management_table(env, buf);
    JNICHK(app_table);

    jint key_interest_table = bb_read(buf, 32);

    uint16_t file_access_length = bb_read(buf, 16);
    jstring file_access_info = _read_jstring(env, buf, file_access_length);
    JNICHK(file_access_info);

    return bdj_make_object(env, "org/videolan/bdjo/Bdjo",
            "(Lorg/videolan/bdjo/TerminalInfo;[Lorg/videolan/bdjo/AppCache;Lorg/videolan/bdjo/PlayListTable;[Lorg/videolan/bdjo/AppEntry;ILjava/lang/String;)V",
            terminal_info, app_cache_info, accessible_playlists, app_table, key_interest_table, file_access_info);
}

jobject bdjo_read(JNIEnv* env, const char* file)
{
    jobject    result = NULL;
    BD_FILE_H *handle = file_open(file, "rb");

    if (handle == NULL) {
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "Failed to open bdjo file (%s)\n", file);
        return NULL;
    }

    file_seek(handle, 0, SEEK_END);
    int64_t length = file_tell(handle);

    if (length <= 0) {
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "Error reading %s\n", file);

    } else {
        file_seek(handle, 0, SEEK_SET);

        uint8_t *data = malloc(length);
        int64_t size_read = file_read(handle, data, length);

        if (size_read < length) {
            BD_DEBUG(DBG_BDJ | DBG_CRIT, "Error reading %s\n", file);

        } else {
            BITBUFFER *buf = malloc(sizeof(BITBUFFER));
            bb_init(buf, data, length);

            result = _parse_bdjo(env, buf);

            free(buf);
        }

        free(data);
    }

    file_close(handle);

    return result;
}
