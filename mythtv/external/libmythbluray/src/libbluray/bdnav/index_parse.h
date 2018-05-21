/*
 * This file is part of libbluray
 * Copyright (C) 2010  hpi1
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

#if !defined(_INDX_PARSE_H_)
#define _INDX_PARSE_H_

#include "util/attributes.h"

#include <stdint.h>

typedef enum {
    indx_video_format_ignored,
    indx_video_480i,
    indx_video_576i,
    indx_video_480p,
    indx_video_1080i,
    indx_video_720p,
    indx_video_1080p,
    indx_video_576p,
} indx_video_format;

typedef enum {
    indx_fps_reserved1,
    indx_fps_23_976,
    indx_fps_24,
    indx_fps_25,
    indx_fps_29_97,
    indx_fps_reserved2,
    indx_fps_50,
    indx_fps_59_94,
} indx_frame_rate;

typedef enum {
    indx_object_type_hdmv = 1,
    indx_object_type_bdj  = 2,
} indx_object_type;

typedef enum {
    indx_hdmv_playback_type_movie       = 0,
    indx_hdmv_playback_type_interactive = 1,
} indx_hdmv_playback_type;

typedef enum {
    indx_bdj_playback_type_movie       = 2,
    indx_bdj_playback_type_interactive = 3,
} indx_bdj_playback_type;

typedef enum {
    indx_access_permitted       = 0,  /* jump into this title is permitted.  title number may be shown on UI.  */
    indx_access_prohibited      = 1,  /* jump into this title is prohibited. title number may be shown on UI. */
    indx_access_hidden          = 3,  /* jump into this title is prohibited. title number shall not be shown on UI. */
} indx_access_type;

#define  INDX_ACCESS_PROHIBITED_MASK  0x01  /* if set, jump to this title is not allowed */
#define  INDX_ACCESS_HIDDEN_MASK      0x02  /* if set, title number shall not be displayed on UI */

typedef struct {
    unsigned int       initial_output_mode_preference : 1; /* 0 - 2D, 1 - 3D */
    unsigned int       content_exist_flag : 1;
    unsigned int       video_format : 4;
    unsigned int       frame_rate : 4;
    uint8_t            user_data[32];
} INDX_APP_INFO;

typedef struct {
    uint8_t            playback_type/* : 2*/;
    char               name[6];
} INDX_BDJ_OBJ;

typedef struct {
    uint8_t            playback_type/* : 2*/;
    uint16_t           id_ref;
} INDX_HDMV_OBJ;

typedef struct {
    uint8_t            object_type/* : 2*/;
    /*union {*/
        INDX_BDJ_OBJ   bdj;
        INDX_HDMV_OBJ  hdmv;
    /*};*/
} INDX_PLAY_ITEM;

typedef struct {
    uint8_t            object_type/* : 2*/;
    uint8_t            access_type/* : 2*/;
    /*union {*/
        INDX_BDJ_OBJ   bdj;
        INDX_HDMV_OBJ  hdmv;
    /*};*/
} INDX_TITLE;

typedef struct indx_root_s {
    INDX_APP_INFO  app_info;
    INDX_PLAY_ITEM first_play;
    INDX_PLAY_ITEM top_menu;

    uint16_t       num_titles;
    INDX_TITLE    *titles;
} INDX_ROOT;


struct bd_disc;

BD_PRIVATE INDX_ROOT* indx_get(struct bd_disc *disc);  /* parse index.bdmv */
BD_PRIVATE void       indx_free(INDX_ROOT **index);

#endif // _INDX_PARSE_H_

