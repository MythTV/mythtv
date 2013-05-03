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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "util/macro.h"
#include "util/logging.h"
#include "util/strutl.h"
#include "file/file.h"
#include "bluray.h"
#include "mpls_parse.h"
#include "navigation.h"

#include <stdlib.h>
#include <string.h>

static int _filter_dup(MPLS_PL *pl_list[], unsigned count, MPLS_PL *pl)
{
    unsigned ii, jj;

    for (ii = 0; ii < count; ii++) {
        if (pl->list_count != pl_list[ii]->list_count) {
            continue;
        }
        if (pl->mark_count != pl_list[ii]->mark_count) {
            continue;
        }
        for (jj = 0; jj < pl->list_count; jj++) {
            MPLS_PI *pi1, *pi2;

            pi1 = &pl->play_item[jj];
            pi2 = &pl_list[ii]->play_item[jj];

            if (memcmp(pi1->clip[0].clip_id, pi2->clip[0].clip_id, 5) != 0 ||
                pi1->in_time != pi2->in_time ||
                pi1->out_time != pi2->out_time) {
                break;
            }
        }
        if (jj != pl->list_count) {
            continue;
        }
        return 0;
    }
    return 1;
}

static unsigned int
_find_repeats(MPLS_PL *pl, const char *m2ts)
{
    unsigned ii, count = 0;

    for (ii = 0; ii < pl->list_count; ii++) {
        MPLS_PI *pi;

        pi = &pl->play_item[ii];
        // Ignore titles with repeated segments
        if (strcmp(pi->clip[0].clip_id, m2ts) == 0) {
          count++;
        }
    }
    return count;
}

static int
_filter_repeats(MPLS_PL *pl, unsigned repeats)
{
    unsigned ii;

    for (ii = 0; ii < pl->list_count; ii++) {
      MPLS_PI *pi;

      pi = &pl->play_item[ii];
      // Ignore titles with repeated segments
      if (_find_repeats(pl, pi->clip[0].clip_id) > repeats) {
        return 0;
      }
    }
    return 1;
}

static uint32_t
_pl_duration(MPLS_PL *pl)
{
    unsigned ii;
    uint32_t duration = 0;
    MPLS_PI *pi;

    for (ii = 0; ii < pl->list_count; ii++) {
        pi = &pl->play_item[ii];
        duration += pi->out_time - pi->in_time;
    }
    return duration;
}

NAV_TITLE_LIST* nav_get_title_list(const char *root, uint32_t flags, uint32_t min_title_length)
{
    BD_DIR_H *dir;
    BD_DIRENT ent;
    char *path = NULL;
    MPLS_PL **pl_list = NULL;
    MPLS_PL *pl = NULL;
    unsigned int ii, pl_list_size = 0;
    int res;
    NAV_TITLE_LIST *title_list;
    unsigned int title_info_alloc = 100;

    title_list = calloc(1, sizeof(NAV_TITLE_LIST));
    title_list->title_info = calloc(title_info_alloc, sizeof(NAV_TITLE_INFO));

    BD_DEBUG(DBG_NAV, "Root: %s:\n", root);
    path = str_printf("%s" DIR_SEP "BDMV" DIR_SEP "PLAYLIST", root);

    dir = dir_open(path);
    if (dir == NULL) {
        BD_DEBUG(DBG_NAV, "Failed to open dir: %s\n", path);
        X_FREE(path);
        X_FREE(title_list->title_info);
        X_FREE(title_list);
        return NULL;
    }
    X_FREE(path);

    ii = 0;
    for (res = dir_read(dir, &ent); !res; res = dir_read(dir, &ent)) {

        if (ent.d_name[0] == '.') {
            continue;
        }
        path = str_printf("%s" DIR_SEP "BDMV" DIR_SEP "PLAYLIST" DIR_SEP "%s",
                          root, ent.d_name);

        if (ii >= pl_list_size) {
            MPLS_PL **tmp = NULL;

            pl_list_size += 100;
            tmp = realloc(pl_list, pl_list_size * sizeof(MPLS_PL*));
            if (tmp == NULL) {
                X_FREE(path);
                break;
            }
            pl_list = tmp;
        }
        pl = mpls_parse(path, 0);
        X_FREE(path);
        if (pl != NULL) {
            if ((flags & TITLES_FILTER_DUP_TITLE) &&
                !_filter_dup(pl_list, ii, pl)) {
                mpls_free(pl);
                continue;
            }
            if ((flags & TITLES_FILTER_DUP_CLIP) && !_filter_repeats(pl, 2)) {
                mpls_free(pl);
                continue;
            }
            if (min_title_length > 0 &&
                _pl_duration(pl) < min_title_length*45000) {
                mpls_free(pl);
                continue;
            }
            if (ii >= title_info_alloc) {
                NAV_TITLE_INFO *tmp = NULL;
                title_info_alloc += 100;

                tmp = realloc(title_list->title_info,
                              title_info_alloc * sizeof(NAV_TITLE_INFO));
                if (tmp == NULL) {
                    break;
                }
                title_list->title_info = tmp;
            }
            pl_list[ii] = pl;
            strncpy(title_list->title_info[ii].name, ent.d_name, 11);
            title_list->title_info[ii].name[10] = '\0';
            title_list->title_info[ii].ref = ii;
            title_list->title_info[ii].mpls_id  = atoi(ent.d_name);
            title_list->title_info[ii].duration = _pl_duration(pl_list[ii]);
            ii++;
        }
    }
    dir_close(dir);

    title_list->count = ii;
    for (ii = 0; ii < title_list->count; ii++) {
        mpls_free(pl_list[ii]);
    }
    X_FREE(pl_list);
    return title_list;
}

void nav_free_title_list(NAV_TITLE_LIST *title_list)
{
    X_FREE(title_list->title_info);
    X_FREE(title_list);
}

char* nav_find_main_title(const char *root)
{
    BD_DIR_H *dir;
    BD_DIRENT ent;
    char *path = NULL;
    MPLS_PL **pl_list = NULL;
    MPLS_PL **tmp = NULL;
    MPLS_PL *pl = NULL;
    unsigned count, ii, jj, pl_list_size = 0;
    int res;
    char longest[11];

    BD_DEBUG(DBG_NAV, "Root: %s:\n", root);
    path = str_printf("%s" DIR_SEP "BDMV" DIR_SEP "PLAYLIST", root);

    dir = dir_open(path);
    if (dir == NULL) {
        fprintf(stderr, "Failed to open dir: %s\n", path);
        X_FREE(path);
        return NULL;
    }
    X_FREE(path);

    ii = jj = 0;
    for (res = dir_read(dir, &ent); !res; res = dir_read(dir, &ent)) {

        if (ent.d_name[0] == '.') {
            continue;
        }
        path = str_printf("%s" DIR_SEP "BDMV" DIR_SEP "PLAYLIST" DIR_SEP "%s",
                          root, ent.d_name);

        if (ii >= pl_list_size) {
            pl_list_size += 100;
            tmp = realloc(pl_list, pl_list_size * sizeof(MPLS_PL*));
            if (tmp == NULL) {
                X_FREE(path);
                break;
            }
            pl_list = tmp;
        }
        pl = mpls_parse(path, 0);
        X_FREE(path);
        if (pl != NULL) {
            if (_filter_dup(pl_list, ii, pl) &&
                _filter_repeats(pl, 2)) {
                pl_list[ii] = pl;
                if (_pl_duration(pl_list[ii]) >= _pl_duration(pl_list[jj])) {
                    strncpy(longest, ent.d_name, 11);
                    longest[10] = '\0';
                    jj = ii;
                }
                ii++;
            } else {
                mpls_free(pl);
            }
        }
    }
    dir_close(dir);

    count = ii;
    for (ii = 0; ii < count; ii++) {
        mpls_free(pl_list[ii]);
    }
    if (count > 0) {
        return str_dup(longest);
    } else {
        return NULL;
    }
}

uint8_t nav_lookup_aspect(NAV_CLIP *clip, int pid)
{
    CLPI_PROG *progs;
    int ii, jj;

    if (clip->cl == NULL) {
        return 0;
    }

    progs = clip->cl->program.progs;
    for (ii = 0; ii < clip->cl->program.num_prog; ii++) {
        CLPI_PROG_STREAM *ps = progs[ii].streams;
        for (jj = 0; jj < progs[ii].num_streams; jj++) {
            if (ps[jj].pid == pid)
            {
                return ps[jj].aspect;
            }
        }
    }
    return 0;
}

static void
_fill_mark(NAV_TITLE *title, NAV_MARK *mark, int entry)
{
    MPLS_PL *pl = title->pl;
    MPLS_PLM *plm;
    MPLS_PI *pi;
    NAV_CLIP *clip;

    plm = &pl->play_mark[entry];

    mark->plm = plm;
    mark->mark_type = plm->mark_type;
    mark->clip_ref = plm->play_item_ref;
    clip = &title->clip_list.clip[mark->clip_ref];
    if (clip->cl != NULL) {
        mark->clip_pkt = clpi_lookup_spn(clip->cl, plm->time, 1,
            title->pl->play_item[mark->clip_ref].clip[title->angle].stc_id);
    } else {
        mark->clip_pkt = clip->start_pkt;
    }
    mark->title_pkt = clip->title_pkt + mark->clip_pkt;
    mark->clip_time = plm->time;

    // Calculate start of mark relative to beginning of playlist
    if (plm->play_item_ref < title->clip_list.count) {
        clip = &title->clip_list.clip[plm->play_item_ref];
        pi = &pl->play_item[plm->play_item_ref];
        mark->title_time = clip->title_time + plm->time - pi->in_time;
    }
}

static void
_extrapolate_title(NAV_TITLE *title)
{
    uint32_t duration = 0;
    uint32_t pkt = 0;
    unsigned ii, jj;
    MPLS_PL *pl = title->pl;
    MPLS_PI *pi;
    MPLS_PLM *plm;
    NAV_MARK *mark, *prev = NULL;
    NAV_CLIP *clip;

    for (ii = 0; ii < title->clip_list.count; ii++) {
        clip = &title->clip_list.clip[ii];
        pi = &pl->play_item[ii];
        if (pi->angle_count > title->angle_count) {
            title->angle_count = pi->angle_count;
        }

        clip->title_time = duration;
        clip->duration = pi->out_time - pi->in_time;
        clip->title_pkt = pkt;
        duration += clip->duration;
        pkt += clip->end_pkt - clip->start_pkt;
    }
    title->duration = duration;
    title->packets = pkt;

    for (ii = 0, jj = 0; ii < pl->mark_count; ii++) {
        plm = &pl->play_mark[ii];
        if (plm->mark_type == BD_MARK_ENTRY) {

            mark = &title->chap_list.mark[jj];
            _fill_mark(title, mark, ii);
            mark->number = jj;

            // Calculate duration of "entry" marks (chapters)
            if (plm->duration != 0) {
                mark->duration = plm->duration;
            } else if (prev != NULL) {
                if (prev->duration == 0) {
                    prev->duration = mark->title_time - prev->title_time;
                }
            }
            prev = mark;
            jj++;
        }
        mark = &title->mark_list.mark[ii];
        _fill_mark(title, mark, ii);
        mark->number = ii;
    }
    title->chap_list.count = jj;
    if (prev != NULL && prev->duration == 0) {
        prev->duration = title->duration - prev->title_time;
    }
}

static void _fill_clip(NAV_TITLE *title,
                       MPLS_CLIP *mpls_clip,
                       uint8_t connection_condition, uint32_t in_time, uint32_t out_time,
                       unsigned pi_angle_count,
                       NAV_CLIP *clip,
                       unsigned ref, uint32_t *pos, uint32_t *time)

{
    char *path;

    clip->title = title;
    clip->ref   = ref;

    if (title->angle >= pi_angle_count) {
        clip->angle = 0;
    } else {
        clip->angle = title->angle;
    }

    strncpy(clip->name, mpls_clip[clip->angle].clip_id, 5);
    strncpy(&clip->name[5], ".m2ts", 6);
    clip->clip_id = atoi(mpls_clip[clip->angle].clip_id);

    path = str_printf("%s"DIR_SEP"BDMV"DIR_SEP"CLIPINF"DIR_SEP"%s.clpi",
                      title->root, mpls_clip[clip->angle].clip_id);
    clpi_free(clip->cl);
    clip->cl = clpi_parse(path, 0);
    X_FREE(path);
    if (clip->cl == NULL) {
        clip->start_pkt = 0;
        clip->end_pkt = 0;
        return;
    }
    switch (connection_condition) {
        case 5:
        case 6:
            clip->start_pkt = 0;
            clip->connection = CONNECT_SEAMLESS;
            break;
        default:
            if (ref) {
                clip->start_pkt = clpi_lookup_spn(clip->cl, in_time, 1,
                                              mpls_clip[clip->angle].stc_id);
            } else {
                clip->start_pkt = 0;
            }
            clip->connection = CONNECT_NON_SEAMLESS;
            break;
    }
    clip->end_pkt = clpi_lookup_spn(clip->cl, out_time, 0,
                                    mpls_clip[clip->angle].stc_id);
    clip->in_time = in_time;
    clip->out_time = out_time;
    clip->pos = *pos;
    *pos += clip->end_pkt - clip->start_pkt;
    clip->start_time = *time;
    *time += clip->out_time - clip->in_time;
}

NAV_TITLE* nav_title_open(const char *root, const char *playlist, unsigned angle)
{
    NAV_TITLE *title = NULL;
    char *path;
    unsigned ii, ss, chapters = 0;
    uint32_t pos = 0;
    uint32_t time = 0;

    title = calloc(1, sizeof(NAV_TITLE));
    if (title == NULL) {
        return NULL;
    }
    title->root = str_dup(root);
    strncpy(title->name, playlist, 11);
    title->name[10] = '\0';
    path = str_printf("%s" DIR_SEP "BDMV" DIR_SEP "PLAYLIST" DIR_SEP "%s",
                      root, playlist);
    title->angle_count = 0;
    title->angle = angle;
    title->pl = mpls_parse(path, 0);
    if (title->pl == NULL) {
        BD_DEBUG(DBG_NAV, "Fail: Playlist parse %s\n", path);
        X_FREE(title);
        X_FREE(path);
        return NULL;
    }
    X_FREE(path);
    // Find length in packets and end_pkt for each clip
    title->clip_list.count = title->pl->list_count;
    title->clip_list.clip = calloc(title->pl->list_count, sizeof(NAV_CLIP));
    title->packets = 0;
    for (ii = 0; ii < title->pl->list_count; ii++) {
        MPLS_PI *pi;
        NAV_CLIP *clip;

        pi = &title->pl->play_item[ii];

        clip = &title->clip_list.clip[ii];

        _fill_clip(title, pi->clip, pi->connection_condition, pi->in_time, pi->out_time, pi->angle_count,
                   clip, ii, &pos, &time);
    }

    // sub paths
    // Find length in packets and end_pkt for each clip
    if (title->pl->sub_count > 0) {
        title->sub_path_count = title->pl->sub_count;
        title->sub_path       = calloc(title->sub_path_count, sizeof(NAV_SUB_PATH));

        for (ss = 0; ss < title->sub_path_count; ss++) {
            NAV_SUB_PATH *sub_path = &title->sub_path[ss];

            sub_path->type            = title->pl->sub_path[ss].type;
            sub_path->clip_list.count = title->pl->sub_path[ss].sub_playitem_count;
            sub_path->clip_list.clip  = calloc(sub_path->clip_list.count, sizeof(NAV_CLIP));

            pos = time = 0;
            for (ii = 0; ii < sub_path->clip_list.count; ii++) {
                MPLS_SUB_PI *pi   = &title->pl->sub_path[ss].sub_play_item[ii];
                NAV_CLIP    *clip = &sub_path->clip_list.clip[ii];

                _fill_clip(title, pi->clip, pi->connection_condition, pi->in_time, pi->out_time, 0,
                           clip, ii, &pos, &time);
            }
        }
    }

    // Count the number of "entry" marks (skipping "link" marks)
    // This is the the number of chapters
    for (ii = 0; ii < title->pl->mark_count; ii++) {
        if (title->pl->play_mark[ii].mark_type == BD_MARK_ENTRY) {
            chapters++;
        }
    }
    title->chap_list.count = chapters;
    title->chap_list.mark = calloc(chapters, sizeof(NAV_MARK));
    title->mark_list.count = title->pl->mark_count;
    title->mark_list.mark = calloc(title->pl->mark_count, sizeof(NAV_MARK));

    _extrapolate_title(title);

    if (title->angle >= title->angle_count) {
        title->angle = 0;
    }

    return title;
}

void nav_title_close(NAV_TITLE *title)
{
    unsigned ii, ss;

    for (ss = 0; ss < title->sub_path_count; ss++) {
        for (ii = 0; ii < title->sub_path[ss].clip_list.count; ii++) {
            clpi_free(title->sub_path[ss].clip_list.clip[ii].cl);
        }
        X_FREE(title->sub_path[ss].clip_list.clip);
    }
    X_FREE(title->sub_path);

    for (ii = 0; ii < title->pl->list_count; ii++) {
        clpi_free(title->clip_list.clip[ii].cl);
    }
    mpls_free(title->pl);
    X_FREE(title->clip_list.clip);
    X_FREE(title->root);
    X_FREE(title->chap_list.mark);
    X_FREE(title->mark_list.mark);
    X_FREE(title);
}

// Search for random access point closest to the requested packet
// Packets are 192 byte TS packets
NAV_CLIP* nav_chapter_search(NAV_TITLE *title, unsigned chapter, uint32_t *clip_pkt, uint32_t *out_pkt)
{
    NAV_CLIP *clip;

    if (chapter > title->chap_list.count) {
        clip = &title->clip_list.clip[0];
        *clip_pkt = clip->start_pkt;
        *out_pkt = clip->pos + *clip_pkt - clip->start_pkt;
        return clip;
    }
    clip = &title->clip_list.clip[title->chap_list.mark[chapter].clip_ref];
    *clip_pkt = title->chap_list.mark[chapter].clip_pkt;
    *out_pkt = clip->pos + *clip_pkt - clip->start_pkt;
    return clip;
}

uint32_t nav_chapter_get_current(NAV_CLIP *clip, uint32_t pkt)
{
    NAV_MARK * mark;
    NAV_TITLE *title;
    uint32_t ii;

    // Clip can be null if we haven't started the first clip yet
    if (clip == NULL) {
        return 0;
    }
    title = clip->title;
    for (ii = 0; ii < title->chap_list.count; ii++) {
        mark = &title->chap_list.mark[ii];
        if (mark->clip_ref > clip->ref)
        {
            if (ii)
                return ii-1;
            else
                return 0;
        }
        if (mark->clip_ref == clip->ref && mark->clip_pkt <= pkt) {
            if ( ii == title->chap_list.count - 1 ) {
                return ii;
            }
            mark = &title->chap_list.mark[ii+1];
            if (mark->clip_ref != clip->ref || mark->clip_pkt > pkt) {
                return ii;
            }
        }
    }
    return 0;
}

// Search for random access point closest to the requested packet
// Packets are 192 byte TS packets
NAV_CLIP* nav_mark_search(NAV_TITLE *title, unsigned mark, uint32_t *clip_pkt, uint32_t *out_pkt)
{
    NAV_CLIP *clip;

    if (mark > title->mark_list.count) {
        clip = &title->clip_list.clip[0];
        *clip_pkt = clip->start_pkt;
        *out_pkt = clip->pos;
        return clip;
    }
    clip = &title->clip_list.clip[title->mark_list.mark[mark].clip_ref];
    *clip_pkt = title->mark_list.mark[mark].clip_pkt;
    *out_pkt = clip->pos + *clip_pkt - clip->start_pkt;
    return clip;
}

// Search for random access point closest to the requested packet
// Packets are 192 byte TS packets
// pkt is relative to the beginning of the title
// out_pkt and out_time is relative to the the clip which the packet falls in
NAV_CLIP* nav_packet_search(NAV_TITLE *title, uint32_t pkt, uint32_t *clip_pkt, uint32_t *out_pkt, uint32_t *out_time)
{
    uint32_t pos, len;
    NAV_CLIP *clip;
    unsigned ii;

    pos = 0;
    for (ii = 0; ii < title->pl->list_count; ii++) {
        clip = &title->clip_list.clip[ii];
        len = clip->end_pkt - clip->start_pkt;
        if (pkt < pos + len)
            break;
        pos += len;
    }
    if (ii == title->pl->list_count) {
        clip = &title->clip_list.clip[ii-1];
        *out_time = clip->duration + clip->in_time;
        *clip_pkt = clip->end_pkt;
    } else {
        clip = &title->clip_list.clip[ii];
        if (clip->cl != NULL) {
            *clip_pkt = clpi_access_point(clip->cl, pkt - pos + clip->start_pkt, 0, 0, out_time);
        } else {
            *clip_pkt = clip->start_pkt;
        }
    }
    if(*out_time < clip->in_time)
        *out_time = 0;
    else
        *out_time -= clip->in_time;
    *out_pkt = clip->pos + *clip_pkt - clip->start_pkt;
    return clip;
}

// Search for the nearest random access point after the given pkt
// which is an angle change point.
// Packets are 192 byte TS packets
// pkt is relative to the clip
// time is the clip relative time where the angle change point occurs
// returns a packet number
//
// To perform a seamless angle change, perform the following sequence:
// 1. Find the next angle change point with nav_angle_change_search.
// 2. Read and process packets until the angle change point is reached.
//    This may mean progressing to the next play item if the angle change
//    point is at the end of the current play item.
// 3. Change angles with nav_set_angle. Changing angles means changing
//    m2ts files. The new clip information is returned from nav_set_angle.
// 4. Close the current m2ts file and open the new one returned 
//    from nav_set_angle.
// 4. If the angle change point was within the time period of the current
//    play item (i.e. the angle change point is not at the end of the clip),
//    Search to the timestamp obtained from nav_angle_change_search using
//    nav_clip_time_search. Otherwise start at the start_pkt defined 
//    by the clip.
uint32_t nav_angle_change_search(NAV_CLIP *clip, uint32_t pkt, uint32_t *time)
{
    if (clip->cl == NULL) {
        return pkt;
    }
    return clpi_access_point(clip->cl, pkt, 1, 1, time);
}

// Search for random access point closest to the requested time
// Time is in 45khz ticks
NAV_CLIP* nav_time_search(NAV_TITLE *title, uint32_t tick, uint32_t *clip_pkt, uint32_t *out_pkt)
{
    uint32_t pos, len;
    MPLS_PI *pi;
    NAV_CLIP *clip;
    unsigned ii;

    pos = 0;
    for (ii = 0; ii < title->pl->list_count; ii++) {
        pi = &title->pl->play_item[ii];
        len = pi->out_time - pi->in_time;
        if (tick < pos + len)
            break;
        pos += len;
    }
    if (ii == title->pl->list_count) {
        clip = &title->clip_list.clip[ii-1];
        *clip_pkt = clip->end_pkt;
    } else {
        clip = &title->clip_list.clip[ii];
        if (clip->cl != NULL) {
            *clip_pkt = clpi_lookup_spn(clip->cl, tick - pos + pi->in_time, 1,
                      title->pl->play_item[clip->ref].clip[clip->angle].stc_id);
        } else {
            *clip_pkt = clip->start_pkt;
        }
    }
    *out_pkt = clip->pos + *clip_pkt - clip->start_pkt;
    return clip;
}

// Search for random access point closest to the requested time
// Time is in 45khz ticks relative to the beginning of a specific clip
void nav_clip_time_search(NAV_CLIP *clip, uint32_t tick, uint32_t *clip_pkt, uint32_t *out_pkt)
{
    if (tick >= clip->out_time) {
        *clip_pkt = clip->end_pkt;
    } else {
        if (clip->cl != NULL) {
            *clip_pkt = clpi_lookup_spn(clip->cl, tick, 1,
               clip->title->pl->play_item[clip->ref].clip[clip->angle].stc_id);
        } else {
            *clip_pkt = clip->start_pkt;
        }
    }
    *out_pkt = clip->pos + *clip_pkt - clip->start_pkt;
}

/*
 * Input Parameters:
 * title     - title struct obtained from nav_title_open
 *
 * Return value:
 * Pointer to NAV_CLIP struct
 * NULL - End of clip list
 */
NAV_CLIP* nav_next_clip(NAV_TITLE *title, NAV_CLIP *clip)
{
    if (clip == NULL) {
        return &title->clip_list.clip[0];
    }
    if (clip->ref >= title->clip_list.count - 1) {
        return NULL;
    }
    return &title->clip_list.clip[clip->ref + 1];
}

NAV_CLIP* nav_set_angle(NAV_TITLE *title, NAV_CLIP *clip, unsigned angle)
{
    int ii;
    uint32_t pos = 0;
    uint32_t time = 0;

    if (title == NULL) {
        return clip;
    }
    if (angle > 8) {
        // invalid angle
        return clip;
    }
    if (angle == title->angle) {
        // no change
        return clip;
    }

    title->angle = angle;
    // Find length in packets and end_pkt for each clip
    title->packets = 0;
    for (ii = 0; ii < title->pl->list_count; ii++) {
        MPLS_PI *pi;
        NAV_CLIP *cl;

        pi = &title->pl->play_item[ii];
        cl = &title->clip_list.clip[ii];

        _fill_clip(title, pi->clip, pi->connection_condition, pi->in_time, pi->out_time, pi->angle_count,
                   cl, ii, &pos, &time);
    }
    _extrapolate_title(title);
    return clip;
}

