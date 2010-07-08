/*
 * This file is part of libbluray
 * Copyright (C) 2009-2010  Obliter0n
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

#ifndef BLURAY_H_
#define BLURAY_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Title flags */
#define TITLES_ALL              0
#define TITLES_FILTER_DUP_TITLE 0x01
#define TITLES_FILTER_DUP_CLIP  0x02
#define TITLES_RELEVANT         (TITLES_FILTER_DUP_TITLE | TITLES_FILTER_DUP_CLIP)

typedef struct bluray BLURAY;

typedef struct bd_stream_info {
    uint8_t     coding_type;
    uint8_t     format;
    uint8_t     rate;
    uint8_t     char_code;
    uint8_t     lang[4];
    uint16_t    pid;
} BLURAY_STREAM_INFO;

typedef struct bd_clip {
    uint8_t            video_stream_count;
    uint8_t            audio_stream_count;
    uint8_t            pg_stream_count;
    uint8_t            ig_stream_count;
    uint8_t            sec_audio_stream_count;
    uint8_t            sec_video_stream_count;
    BLURAY_STREAM_INFO *video_streams;
    BLURAY_STREAM_INFO *audio_streams;
    BLURAY_STREAM_INFO *pg_streams;
    BLURAY_STREAM_INFO *ig_streams;
    BLURAY_STREAM_INFO *sec_audio_streams;
    BLURAY_STREAM_INFO *sec_video_streams;
} BLURAY_CLIP_INFO;

typedef struct bd_chapter {
    uint32_t    idx;
    uint64_t    start;
    uint64_t    duration;
    uint64_t    offset;
} BLURAY_TITLE_CHAPTER;

typedef struct bd_title_info {
    uint32_t             idx;
    uint64_t             duration;
    uint32_t             clip_count;
    uint8_t              angle_count;
    uint32_t             chapter_count;
    BLURAY_CLIP_INFO     *clips;
    BLURAY_TITLE_CHAPTER *chapters;
} BLURAY_TITLE_INFO;

uint32_t bd_get_titles(BLURAY *bd, uint8_t flags);
BLURAY_TITLE_INFO* bd_get_title_info(BLURAY *bd, uint32_t title_idx);
void bd_free_title_info(BLURAY_TITLE_INFO *title_info);

BLURAY *bd_open(const char* device_path, const char* keyfile_path); // Init libbluray objs
void bd_close(BLURAY *bd);                                          // Free libbluray objs

int64_t bd_seek(BLURAY *bd, uint64_t pos);              // Seek to pos in currently selected title file
int64_t bd_seek_time(BLURAY *bd, uint64_t tick); // Seek to a specific time in 90Khz ticks
int bd_read(BLURAY *bd, unsigned char *buf, int len);   // Read from currently selected title file, decrypt if possible
int64_t bd_seek_chapter(BLURAY *bd, unsigned chapter);  // Seek to a chapter. First chapter is 0
int64_t bd_chapter_pos(BLURAY *bd, unsigned chapter);   // Find the byte position of a chapter
uint32_t bd_get_current_chapter(BLURAY *bd);            // Get the current chapter
int64_t bd_seek_mark(BLURAY *bd, unsigned mark);        // Seek to a playmark. First mark is 0
int bd_select_playlist(BLURAY *bd, uint32_t playlist);
int bd_select_title(BLURAY *bd, uint32_t title);    // Select the title from the list created by bd_get_titles()
int bd_select_angle(BLURAY *bd, unsigned angle);           // Set the angle to play
void bd_seamless_angle_change(BLURAY *bd, unsigned angle); // Initiate seamless angle change
uint64_t bd_get_title_size(BLURAY *bd);             // Returns file size in bytes of currently selected title, 0 in no title selected
uint32_t bd_get_current_title(BLURAY *bd);          // Returns the current title index
unsigned bd_get_current_angle(BLURAY *bd);          // Returns the current angle

uint64_t bd_tell(BLURAY *bd);       // Return current pos
uint64_t bd_tell_time(BLURAY *bd);  // Return current time

/*
 * player settings
 */

#define BLURAY_PLAYER_SETTING_PARENTAL       13  /* Age for parental control (years) */
#define BLURAY_PLAYER_SETTING_AUDIO_CAP      15  /* Player capability for audio (bit mask) */
#define BLURAY_PLAYER_SETTING_AUDIO_LANG     16  /* Initial audio language: ISO 639-2 string, ex. "eng" */
#define BLURAY_PLAYER_SETTING_PG_LANG        17  /* Initial PG/SPU language: ISO 639-2 string, ex. "eng" */
#define BLURAY_PLAYER_SETTING_MENU_LANG      18  /* Initial menu language: ISO 639-2 string, ex. "eng" */
#define BLURAY_PLAYER_SETTING_COUNTRY_CODE   19  /* Player country code: ISO 3166-1 string, ex. "de" */
#define BLURAY_PLAYER_SETTING_REGION_CODE    20  /* Player region code: 1 - region A, 2 - B, 4 - C */
#define BLURAY_PLAYER_SETTING_VIDEO_CAP      29  /* Player capability for video (bit mask) */
#define BLURAY_PLAYER_SETTING_TEXT_CAP       30  /* Player capability for text subtitle (bit mask) */
#define BLURAY_PLAYER_SETTING_PLAYER_PROFILE 31  /* Profile1: 0, Profile1+: 1, Profile2: 3, Profile3: 8 */

int bd_set_player_setting(BLURAY *bd, uint32_t idx, uint32_t value);
int bd_set_player_setting_str(BLURAY *bd, uint32_t idx, const char *s);

/*
 * Java
 */
int bd_start_bdj(BLURAY *bd, const char* start_object); // start BD-J from the specified BD-J object (should be a 5 character string)
void bd_stop_bdj(BLURAY *bd); // shutdown BD-J and clean up resources

/*
 * navigaton mode
 */

typedef enum {
    BD_EVENT_NONE = 0,
    BD_EVENT_ERROR,

    BD_EVENT_ANGLE_ID,
    BD_EVENT_TITLE_ID,
    BD_EVENT_PLAYLIST,
    BD_EVENT_PLAYITEM,
    BD_EVENT_CHAPTER,
} bd_event_e;

typedef struct {
  bd_event_e event;
  uint32_t   param;
} BD_EVENT;

int  bd_play(BLURAY *bd); /* start playing disc in navigation mode */
int  bd_read_ext(BLURAY *bd, unsigned char *buf, int len, BD_EVENT *event);

int  bd_play_title(BLURAY *bd, unsigned title); /* play title (from disc index) */
int  bd_menu_call(BLURAY *bd);                  /* open disc root menu */

#ifdef __cplusplus
};
#endif

#endif /* BLURAY_H_ */
