/*
 * This file is part of libbluray
 * Copyright (C) 2009-2010  John Stebbins
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

#if !defined(_NAVIGATION_H_)
#define _NAVIGATION_H_

#include <util/attributes.h>

#include "mpls_parse.h"
#include "clpi_parse.h"

#define CONNECT_NON_SEAMLESS 0
#define CONNECT_SEAMLESS 1

#define TITLES_ALL              0
#define TITLES_FILTER_DUP_TITLE 0x01
#define TITLES_FILTER_DUP_CLIP  0x02
#define TITLES_RELEVANT         (TITLES_FILTER_DUP_TITLE | TITLES_FILTER_DUP_CLIP)

typedef struct nav_title_s NAV_TITLE;

typedef struct nav_mark_s NAV_MARK;
struct nav_mark_s
{
    int      number;
    int      mark_type;
    unsigned clip_ref;
    uint32_t clip_pkt;
    uint32_t clip_time;

    // Title relative metrics
    uint32_t title_pkt;
    uint32_t title_time;
    uint32_t duration;

    MPLS_PLM *plm;
};


typedef struct nav_mark_list_s NAV_MARK_LIST;
struct nav_mark_list_s
{
    unsigned  count;
    NAV_MARK *mark;
};

typedef struct nav_clip_s NAV_CLIP;
struct nav_clip_s
{
    char     name[11];
    uint32_t clip_id;
    unsigned ref;
    uint32_t pos;
    uint32_t start_pkt;
    uint32_t end_pkt;
    uint8_t  connection;
    uint8_t  angle;

    uint32_t start_time;
    uint32_t duration;

    uint32_t in_time;
    uint32_t out_time;

    // Title relative metrics
    uint32_t title_pkt;
    uint32_t title_time;

    NAV_TITLE *title;

    CLPI_CL  *cl;
};

typedef struct nav_clip_list_s NAV_CLIP_LIST;
struct nav_clip_list_s
{
    unsigned count;
    NAV_CLIP *clip;
};

typedef struct nav_sub_path_s NAV_SUB_PATH;
struct nav_sub_path_s
{
    uint8_t       type;
    NAV_CLIP_LIST clip_list;
};

struct nav_title_s {
    char          *root;
    char          name[11];
    uint8_t       angle_count;
    uint8_t       angle;
    NAV_CLIP_LIST clip_list;
    NAV_MARK_LIST chap_list;
    NAV_MARK_LIST mark_list;

    unsigned      sub_path_count;
    NAV_SUB_PATH  *sub_path;

    uint32_t      packets;
    uint32_t      duration;

    MPLS_PL       *pl;
};

typedef struct nav_title_info_s NAV_TITLE_INFO;
struct nav_title_info_s
{
    char            name[11];
    uint32_t        mpls_id;
    uint32_t        duration;
    unsigned        ref;
};

typedef struct nav_title_list_s NAV_TITLE_LIST;
struct nav_title_list_s
{
    unsigned int     count;
    NAV_TITLE_INFO  *title_info;
};

BD_PRIVATE uint8_t nav_lookup_aspect(NAV_CLIP *clip, int pid);
BD_PRIVATE char* nav_find_main_title(const char *root);
BD_PRIVATE NAV_TITLE* nav_title_open(const char *root, const char *playlist, unsigned angle);
BD_PRIVATE void nav_title_close(NAV_TITLE *title);
BD_PRIVATE NAV_CLIP* nav_next_clip(NAV_TITLE *title, NAV_CLIP *clip);
BD_PRIVATE NAV_CLIP* nav_packet_search(NAV_TITLE *title, uint32_t pkt, uint32_t *clip_pkt,
                                       uint32_t *out_pkt, uint32_t *out_time);
BD_PRIVATE NAV_CLIP* nav_time_search(NAV_TITLE *title, uint32_t tick, uint32_t *clip_pkt, uint32_t *out_pkt);
BD_PRIVATE void nav_clip_time_search(NAV_CLIP *clip, uint32_t tick, uint32_t *clip_pkt, uint32_t *out_pkt);
BD_PRIVATE NAV_CLIP* nav_chapter_search(NAV_TITLE *title, unsigned chapter, uint32_t *clip_pkt, uint32_t *out_pkt);
BD_PRIVATE uint32_t nav_chapter_get_current(NAV_CLIP *clip, uint32_t pkt);
BD_PRIVATE NAV_CLIP* nav_mark_search(NAV_TITLE *title, unsigned mark, uint32_t *clip_pkt, uint32_t *out_pkt);
BD_PRIVATE uint32_t nav_angle_change_search(NAV_CLIP *clip, uint32_t pkt, uint32_t *time);
BD_PRIVATE NAV_CLIP* nav_set_angle(NAV_TITLE *title, NAV_CLIP *clip, unsigned angle);

BD_PRIVATE NAV_TITLE_LIST* nav_get_title_list(const char *root, uint32_t flags, uint32_t min_title_length);
BD_PRIVATE void nav_free_title_list(NAV_TITLE_LIST *title_list);

#endif // _NAVIGATION_H_
