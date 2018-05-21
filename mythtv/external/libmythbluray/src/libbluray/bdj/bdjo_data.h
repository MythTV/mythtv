/*
 * This file is part of libbluray
 * Copyright (C) 2014  Petri Hintukainen <phintuka@users.sourceforge.net>
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

#if !defined(_BDJO_DATA_H_)
#define _BDJO_DATA_H_

#include <stdint.h>


typedef struct {
    char    default_font[6];         // default AWT font. AUXDATA/?????.otf. "*****" = none.
    uint8_t initial_havi_config_id;  // valid only if no autostart playlist.
    uint8_t menu_call_mask;
    uint8_t title_search_mask;
} BDJO_TERMINAL_INFO;

typedef struct {
    uint8_t type;             // 1 - JAR, 2 - directory
    char    ref_to_name[6];   // JAR/xxxxx.jar or JAR/xxxxx/
    char    lang_code[4];     // ref. to PSR18. "*.*" = cache always. "%%%" = fallback when no language matches PSR18.
} BDJO_APP_CACHE_ITEM;

typedef struct {
    uint8_t              num_item;
    BDJO_APP_CACHE_ITEM *item;
} BDJO_APP_CACHE_INFO;

typedef struct {
    char     name[6];
} BDJO_PLAYLIST;

typedef struct {
    uint8_t        access_to_all_flag;
    uint8_t        autostart_first_playlist_flag;

    uint16_t       num_pl;
    BDJO_PLAYLIST *pl;
} BDJO_ACCESSIBLE_PLAYLISTS;

typedef struct {
    uint16_t profile_number; /* 1: BD-ROM profile 1. 2: BD-ROM profile 2 */
    uint8_t  major_version;
    uint8_t  minor_version;
    uint8_t  micro_version;
} BDJO_APP_PROFILE;

typedef struct {
    char  lang[4];
    char *name;    /* UTF-8, informative name */
} BDJO_APP_NAME;

typedef struct {
    char *param;
} BDJO_APP_PARAM;

typedef struct {
    uint8_t           control_code; /* 1 - autostart, 2 - present */
    uint8_t           type;         /* 1 - BD-J App */
    uint32_t          org_id;
    uint16_t          app_id;

    /* descriptor */

    uint8_t           num_profile;
    BDJO_APP_PROFILE *profile;

    uint8_t           priority;
    uint8_t           binding;
    uint8_t           visibility;

    uint16_t          num_name;
    BDJO_APP_NAME    *name;

    uint8_t           icon_flags;
    char             *icon_locator; // relative to base_dir
    char             *base_dir;     // "00000" -> 00000.jar!/ , "00000/base" -> 00000.jar!/base/
    char             *classpath_extension; // separator ; . relative paths relative to base_dir. absolute paths refer to .jar files.
    char             *initial_class;

    uint8_t           num_param;
    BDJO_APP_PARAM   *param;
} BDJO_APP;

typedef struct {
    uint8_t    num_app;
    BDJO_APP  *app;
} BDJO_APP_MANAGEMENT_TABLE;

typedef struct {
    unsigned int vk_play : 1;
    unsigned int vk_stop : 1;
    unsigned int vk_ffw : 1;
    unsigned int vk_rew : 1;
    unsigned int vk_track_next : 1;
    unsigned int vk_track_prev : 1;
    unsigned int vk_pause : 1;
    unsigned int vk_still_off : 1;
    unsigned int vk_sec_audio_ena_dis : 1;
    unsigned int vk_sec_video_ena_dis : 1;
    unsigned int pg_textst_ena_dis : 1;
} BDJO_KEY_INTEREST_TABLE;

typedef struct {
    // relative to VFS root. Separator ";". Should not start with "/".
    // common "." == access to all files in VFS.
    char *path;
} BDJO_FILE_ACCESS_INFO;

typedef struct bdjo_data {
    BDJO_TERMINAL_INFO        terminal_info;
    BDJO_APP_CACHE_INFO       app_cache_info;
    BDJO_ACCESSIBLE_PLAYLISTS accessible_playlists;
    BDJO_APP_MANAGEMENT_TABLE app_table;

    BDJO_KEY_INTEREST_TABLE   key_interest_table;
    BDJO_FILE_ACCESS_INFO     file_access_info;
} BDJO;


#endif // _BDJO_DATA_H_
