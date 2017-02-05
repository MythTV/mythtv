/*
 * This file is part of libbluray
 * Copyright (C) 2009-2010  Obliter0n
 * Copyright (C) 2009-2010  John Stebbins
 * Copyright (C) 2010-2014  Petri Hintukainen
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

/**
 * @file libbluray/bluray.h
 * external API header
 */

#include <stdint.h>

#define TITLES_ALL              0    /**< all titles. */
#define TITLES_FILTER_DUP_TITLE 0x01 /**< remove duplicate titles. */
#define TITLES_FILTER_DUP_CLIP  0x02 /**< remove titles that have duplicate
                                          clips. */
#define TITLES_RELEVANT \
  (TITLES_FILTER_DUP_TITLE | TITLES_FILTER_DUP_CLIP) /**< remove duplicate
                                                          titles and clips */

typedef struct bluray BLURAY;

/*
 * Disc info
 */

/* AACS error codes */
#define BD_AACS_CORRUPTED_DISC  -1
#define BD_AACS_NO_CONFIG       -2
#define BD_AACS_NO_PK           -3
#define BD_AACS_NO_CERT         -4
#define BD_AACS_CERT_REVOKED    -5
#define BD_AACS_MMC_FAILED      -6

/* HDMV / BD-J title */
typedef struct {
    const char *name;         /* optional title name in preferred language */
    uint8_t     interactive;  /* 1 if title is interactive (title length and playback position should not be shown in UI) */
    uint8_t     accessible;   /* 1 if it is allowed to jump into this title */
    uint8_t     hidden;       /* 1 if title number should not be shown during playback */

    uint8_t     bdj;          /* 0 - HDMV title. 1 - BD-J title */
    uint32_t    id_ref;       /* Movie Object number / bdjo file number */
} BLURAY_TITLE;

typedef struct {
    uint8_t  bluray_detected;

    uint8_t  first_play_supported;
    uint8_t  top_menu_supported;

    uint32_t num_hdmv_titles;
    uint32_t num_bdj_titles;
    uint32_t num_unsupported_titles;

    uint8_t  aacs_detected;
    uint8_t  libaacs_detected;
    uint8_t  aacs_handled;

    uint8_t  bdplus_detected;
    uint8_t  libbdplus_detected;
    uint8_t  bdplus_handled;

    /* aacs error code */
    int      aacs_error_code;
    /* aacs MKB version */
    int      aacs_mkbv;

    /* Disc ID */
    uint8_t  disc_id[20];

    /* BD-J */
    uint8_t  bdj_detected;     /* 1 if disc uses BD-J */
    uint8_t  bdj_supported;    /* 1 if BD-J support was compiled in */
    uint8_t  libjvm_detected;  /* 1 if usable Java VM was found */
    uint8_t  bdj_handled;      /* 1 if usable Java VM + libbluray.jar was found */

    /* BD+ content code generation */
    uint8_t  bdplus_gen;
    /* BD+ content code relese date */
    uint32_t bdplus_date;      /* (year << 16) | (month << 8) | day */

    /* disc application info */
    uint8_t video_format;             /* bd_video_format_e */
    uint8_t frame_rate;               /* bd_frame_rate_e */
    uint8_t content_exist_3D;
    uint8_t initial_output_mode_preference;   /* 0 - 2D, 1 - 3D */
    uint8_t provider_data[32];

    /* HDMV / BD-J titles */
    uint32_t             num_titles;
    const BLURAY_TITLE  *const *titles;  /* index is title number 1 ... N */
    const BLURAY_TITLE  *first_play;     /* titles[N+1].   NULL if not present on the disc. */
    const BLURAY_TITLE  *top_menu;       /* titles[0]. NULL if not present on the disc. */

    char bdj_org_id[9];      /* (BD-J) disc organization ID */
    char bdj_disc_id[33];    /* (BD-J) disc ID */

    const char *udf_volume_id; /* optional UDF volume identifier */

    uint8_t no_menu_support;   /* 1 if this disc can't be played using on-disc menus */

} BLURAY_DISC_INFO;

/*
 * Playlist info
 */

typedef enum {
    BLURAY_STREAM_TYPE_VIDEO_MPEG1              = 0x01,
    BLURAY_STREAM_TYPE_VIDEO_MPEG2              = 0x02,
    BLURAY_STREAM_TYPE_AUDIO_MPEG1              = 0x03,
    BLURAY_STREAM_TYPE_AUDIO_MPEG2              = 0x04,
    BLURAY_STREAM_TYPE_AUDIO_LPCM               = 0x80,
    BLURAY_STREAM_TYPE_AUDIO_AC3                = 0x81,
    BLURAY_STREAM_TYPE_AUDIO_DTS                = 0x82,
    BLURAY_STREAM_TYPE_AUDIO_TRUHD              = 0x83,
    BLURAY_STREAM_TYPE_AUDIO_AC3PLUS            = 0x84,
    BLURAY_STREAM_TYPE_AUDIO_DTSHD              = 0x85,
    BLURAY_STREAM_TYPE_AUDIO_DTSHD_MASTER       = 0x86,
    BLURAY_STREAM_TYPE_VIDEO_VC1                = 0xea,
    BLURAY_STREAM_TYPE_VIDEO_H264               = 0x1b,
    BLURAY_STREAM_TYPE_SUB_PG                   = 0x90,
    BLURAY_STREAM_TYPE_SUB_IG                   = 0x91,
    BLURAY_STREAM_TYPE_SUB_TEXT                 = 0x92,
    BLURAY_STREAM_TYPE_AUDIO_AC3PLUS_SECONDARY  = 0xa1,
    BLURAY_STREAM_TYPE_AUDIO_DTSHD_SECONDARY    = 0xa2
} bd_stream_type_e;

typedef enum {
    BLURAY_VIDEO_FORMAT_480I              = 1,  // ITU-R BT.601-5
    BLURAY_VIDEO_FORMAT_576I              = 2,  // ITU-R BT.601-4
    BLURAY_VIDEO_FORMAT_480P              = 3,  // SMPTE 293M
    BLURAY_VIDEO_FORMAT_1080I             = 4,  // SMPTE 274M
    BLURAY_VIDEO_FORMAT_720P              = 5,  // SMPTE 296M
    BLURAY_VIDEO_FORMAT_1080P             = 6,  // SMPTE 274M
    BLURAY_VIDEO_FORMAT_576P              = 7   // ITU-R BT.1358
} bd_video_format_e;

typedef enum {
    BLURAY_VIDEO_RATE_24000_1001          = 1,  // 23.976
    BLURAY_VIDEO_RATE_24                  = 2,
    BLURAY_VIDEO_RATE_25                  = 3,
    BLURAY_VIDEO_RATE_30000_1001          = 4,  // 29.97
    BLURAY_VIDEO_RATE_50                  = 6,
    BLURAY_VIDEO_RATE_60000_1001          = 7   // 59.94
} bd_video_rate_e;

typedef enum {
    BLURAY_ASPECT_RATIO_4_3               = 2,
    BLURAY_ASPECT_RATIO_16_9              = 3
} bd_video_aspect_e;

typedef enum {
    BLURAY_AUDIO_FORMAT_MONO              = 1,
    BLURAY_AUDIO_FORMAT_STEREO            = 3,
    BLURAY_AUDIO_FORMAT_MULTI_CHAN        = 6,
    BLURAY_AUDIO_FORMAT_COMBO             = 12  // Stereo ac3/dts, 
} bd_audio_format_e;
                                                // multi mlp/dts-hd

typedef enum {
    BLURAY_AUDIO_RATE_48                  = 1,
    BLURAY_AUDIO_RATE_96                  = 4,
    BLURAY_AUDIO_RATE_192                 = 5,
    BLURAY_AUDIO_RATE_192_COMBO           = 12, // 48 or 96 ac3/dts
                                                // 192 mpl/dts-hd
    BLURAY_AUDIO_RATE_96_COMBO            = 14  // 48 ac3/dts
                                                // 96 mpl/dts-hd
} bd_audio_rate_e;

typedef enum {
    BLURAY_TEXT_CHAR_CODE_UTF8            = 0x01,
    BLURAY_TEXT_CHAR_CODE_UTF16BE         = 0x02,
    BLURAY_TEXT_CHAR_CODE_SHIFT_JIS       = 0x03,
    BLURAY_TEXT_CHAR_CODE_EUC_KR          = 0x04,
    BLURAY_TEXT_CHAR_CODE_GB18030_20001   = 0x05,
    BLURAY_TEXT_CHAR_CODE_CN_GB           = 0x06,
    BLURAY_TEXT_CHAR_CODE_BIG5            = 0x07
} bd_char_code_e;

typedef enum {
    BLURAY_STILL_NONE     = 0x00,
    BLURAY_STILL_TIME     = 0x01,
    BLURAY_STILL_INFINITE = 0x02,
} bd_still_mode_e;

typedef enum {
    BLURAY_MARK_ENTRY     = 0x01,  /* entry mark for chapter search */
    BLURAY_MARK_LINK      = 0x02,  /* link point */
} bd_mark_type_e;

typedef struct bd_stream_info {
    uint8_t     coding_type;
    uint8_t     format;
    uint8_t     rate;
    uint8_t     char_code;
    uint8_t     lang[4];
    uint16_t    pid;
    uint8_t     aspect;
    uint8_t     subpath_id;
} BLURAY_STREAM_INFO;

typedef struct bd_clip {
    uint32_t           pkt_count;
    uint8_t            still_mode;
    uint16_t           still_time;  /* seconds */
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

    uint64_t           start_time;  /* start media time, 90kHz, ("playlist time") */
    uint64_t           in_time;     /* start timestamp, 90kHz */
    uint64_t           out_time;    /* end timestamp, 90kHz */
} BLURAY_CLIP_INFO;

typedef struct bd_chapter {
    uint32_t    idx;
    uint64_t    start;     /* start media time, 90kHz, ("playlist time") */
    uint64_t    duration;  /* duration */
    uint64_t    offset;    /* distance from title start, bytes */
    unsigned    clip_ref;
} BLURAY_TITLE_CHAPTER;

typedef struct bd_mark {
    uint32_t    idx;
    int         type;      /* bd_mark_type_e */
    uint64_t    start;     /* mark media time, 90kHz, ("playlist time") */
    uint64_t    duration;  /* time to next mark */
    uint64_t    offset;    /* mark distance from title start, bytes */
    unsigned    clip_ref;
} BLURAY_TITLE_MARK;

typedef struct bd_title_info {
    uint32_t             idx;
    uint32_t             playlist;
    uint64_t             duration;
    uint32_t             clip_count;
    uint8_t              angle_count;
    uint32_t             chapter_count;
    BLURAY_CLIP_INFO     *clips;
    BLURAY_TITLE_CHAPTER *chapters;

    uint32_t             mark_count;
    BLURAY_TITLE_MARK    *marks;
} BLURAY_TITLE_INFO;

/*
 * Sound effect data
 */

typedef struct bd_sound_effect {
    uint8_t         num_channels; /* 1 - mono, 2 - stereo */
    uint32_t        num_frames;
    const int16_t  *samples;      /* 48000 Hz, 16 bit LPCM. interleaved if stereo */
} BLURAY_SOUND_EFFECT;


/**
 *  Get library version
 *
 */
void bd_get_version(int *major, int *minor, int *micro);

/*
 * Disc functions
 */

/**
 *  Open BluRay disc
 *
 *  Shortcut for bd_open_disc(bd_init(), device_path, keyfile_path)
 *
 * @param device_path   path to mounted Blu-ray disc, device or image file
 * @param keyfile_path  path to KEYDB.cfg (may be NULL)
 * @return allocated BLURAY object, NULL if error
 */
BLURAY *bd_open(const char *device_path, const char *keyfile_path);

/**
 *  Initialize BLURAY object
 *
 *  Resulting object can be passed to following bd_open_??? functions.
 *
 * @return allocated BLURAY object, NULL if error
 */
BLURAY *bd_init(void);

/**
 *  Open BluRay disc
 *
 * @param bd  BLURAY object
 * @param device_path   path to mounted Blu-ray disc, device or image file
 * @param keyfile_path  path to KEYDB.cfg (may be NULL)
 * @return 1 on success, 0 if error
 */
int bd_open_disc(BLURAY *bd, const char *device_path, const char *keyfile_path);

/**
 *  Open BluRay disc
 *
 * @param bd  BLURAY object
 * @param handle  opaque handle for read_blocks
 * @param read_blocks  function used to read disc blocks
 * @return 1 on success, 0 if error
 */
int bd_open_stream(BLURAY *bd,
                   void *read_blocks_handle,
                   int (*read_blocks)(void *handle, void *buf, int lba, int num_blocks));

/**
 *  Open BluRay disc
 *
 * @param bd  BLURAY object
 * @param handle  opaque handle for open_dir and open_file
 * @param open_dir  function used to open a directory
 * @param open_file  function used to open a file
 * @return 1 on success, 0 if error
 */
struct bd_dir_s;
struct bd_file_s;
int bd_open_files(BLURAY *bd,
                  void *handle,
                  struct bd_dir_s *(*open_dir)(void *handle, const char *rel_path),
                  struct bd_file_s *(*open_file)(void *handle, const char *rel_path));

/**
 *  Close BluRay disc
 *
 * @param bd  BLURAY object
 */
void bd_close(BLURAY *bd);

/**
 *
 *  Get information about current BluRay disc
 *
 * @param bd  BLURAY object
 * @return pointer to BLURAY_DISC_INFO object, NULL on error
 */
const BLURAY_DISC_INFO *bd_get_disc_info(BLURAY *bd);

/**
 *
 *  Get meta information about current BluRay disc.
 *
 *  Meta information is optional in BluRay discs.
 *  If information is provided in multiple languages, currently
 *  selected language (BLURAY_PLAYER_SETTING_MENU_LANG) is used.
 *
 *  Referenced thumbnail images should be read with bd_get_meta_file().
 *
 * @param bd  BLURAY object
 * @return META_DL (disclib) object, NULL on error
 */
struct meta_dl;
const struct meta_dl *bd_get_meta(BLURAY *bd);

/**
 *
 *  Read metadata file from BluRay disc.
 *
 *  Allocate large enough memory block and read file contents.
 *  Caller must free the memory block with free().
 *
 * @param bd  BLURAY object
 * @param file_name  name of metadata file
 * @param data  where to store pointer to file data
 * @param size  where to store file size
 * @return 1 on success, 0 on error
 */
int bd_get_meta_file(BLURAY *bd, const char *file_name, void **data, int64_t *size);


/*
 * Title selection without on-disc menus
 */

/**
 *
 *  Get number of titles (playlists)
 *
 *  This must be called after bd_open() and before bd_select_title().
 *  Populates the title list in BLURAY.
 *  Filtering of the returned list is controled through title flags
 *
 * @param bd  BLURAY object
 * @param flags  title flags
 * @param min_title_length  filter out titles shorter than min_title_length seconds
 * @return number of titles found
 */
uint32_t bd_get_titles(BLURAY *bd, uint8_t flags, uint32_t min_title_length);

/**
 *
 *  Get main title
 *  Returned number is an index to the list created by bd_get_titles()
 *
 * @param bd  BLURAY object
 * @return title index of main title, -1 on error
 */
int bd_get_main_title(BLURAY *bd);

/**
 *
 *  Get information about a title
 *
 * @param bd  BLURAY object
 * @param title_idx title index number
 * @param angle angle number (chapter offsets and clip size depend on selected angle)
 * @return allocated BLURAY_TITLE_INFO object, NULL on error
 */
BLURAY_TITLE_INFO* bd_get_title_info(BLURAY *bd, uint32_t title_idx, unsigned angle);

/**
 *
 *  Free BLURAY_TITLE_INFO object
 *
 * @param title_info  BLURAY_TITLE_INFO object
 */
void bd_free_title_info(BLURAY_TITLE_INFO *title_info);

/**
 *
 *  Select the title from the list created by bd_get_titles()
 *
 * @param bd  BLURAY object
 * @param title title to select
 * @return 1 on success, 0 if error
 */
int bd_select_title(BLURAY *bd, uint32_t title);

/**
 *
 *  Select a playlist
 *
 * @param bd  BLURAY object
 * @param playlist playlist to select
 * @return 1 on success, 0 if error
 */
int bd_select_playlist(BLURAY *bd, uint32_t playlist);

/**
 *
 *  Returns the current title index
 *
 * @param bd  BLURAY object
 * @return current title index
 */
uint32_t bd_get_current_title(BLURAY *bd);

/**
 *
 *  Read from currently selected title file, decrypt if possible
 *
 * @param bd  BLURAY object
 * @param buf buffer to read data into
 * @param len size of data to be read
 * @return size of data read, -1 if error, 0 if EOF
 */
int bd_read(BLURAY *bd, unsigned char *buf, int len);


/*
 * Playback control functions
 */

/**
 *  Seek to pos in currently selected title
 *
 * @param bd  BLURAY object
 * @param pos position to seek to
 * @return current seek position
 */
int64_t bd_seek(BLURAY *bd, uint64_t pos);

/**
 *
 * Seek to specific time in 90Khz ticks
 *
 * @param bd    BLURAY ojbect
 * @param tick  tick count
 * @return current seek position
 */
int64_t bd_seek_time(BLURAY *bd, uint64_t tick);

/**
 *
 *  Seek to a chapter. First chapter is 0
 *
 * @param bd  BLURAY object
 * @param chapter chapter to seek to
 * @return current seek position
 */
int64_t bd_seek_chapter(BLURAY *bd, unsigned chapter);

/**
 *
 * Seek to a playmark. First mark is 0
 *
 * @param bd  BLURAY object
 * @param mark playmark to seek to
 * @return current seek position
 */
int64_t bd_seek_mark(BLURAY *bd, unsigned mark);

/**
 *
 *  Seek to a playitem.
 *
 * @param bd  BLURAY object
 * @param playitem to seek to
 * @return current seek position
 */
int64_t bd_seek_playitem(BLURAY *bd, unsigned clip_ref);

/**
 *
 *  Set the angle to play
 *
 * @param bd  BLURAY object
 * @param angle angle to play
 * @return 1 on success, 0 if error
 */
int bd_select_angle(BLURAY *bd, unsigned angle);

/**
 *
 *  Initiate seamless angle change
 *
 * @param bd  BLURAY object
 * @param angle angle to change to
 */
void bd_seamless_angle_change(BLURAY *bd, unsigned angle);

/**
 *
 *  Select stream (PG / TextST track)
 *
 *  This function can be used to override automatic stream selection.
 *  Selecting the stream is useful only when using libbluray internal decoders
 *  or stream is stored in a sub-path.
 *
 * @param bd  BLURAY object
 * @param stream_type  BLURAY_*_STREAM
 * @param stream_id  stream number (1..N)
 * @param enable_flag  set to 0 to disable streams of this type
 */
#define BLURAY_AUDIO_STREAM      0
#define BLURAY_PG_TEXTST_STREAM  1

void bd_select_stream(BLURAY *bd, uint32_t stream_type, uint32_t stream_id, uint32_t enable_flag);


/*
 * Playback status functions
 */

/**
 *
 *  Find the byte position of a chapter
 *
 * @param bd  BLURAY object
 * @param chapter chapter to find position of
 * @return seek position of chapter start
 */
int64_t bd_chapter_pos(BLURAY *bd, unsigned chapter);

/**
 *
 *  Get the current chapter
 *
 * @param bd  BLURAY object
 * @return current chapter
 */
uint32_t bd_get_current_chapter(BLURAY *bd);

/**
 *
 *  Returns file size in bytes of currently selected title, 0 in no title
 *  selected
 *
 * @param bd  BLURAY object
 * @return file size in bytes of currently selected title, 0 if no title
 * selected
 */
uint64_t bd_get_title_size(BLURAY *bd);

/**
 *
 *  Return the current angle
 *
 * @param bd  BLURAY object
 * @return current angle
 */
unsigned bd_get_current_angle(BLURAY *bd);

/**
 *
 *  Return current pos
 *
 * @param bd  BLURAY object
 * @return current seek position
 */
uint64_t bd_tell(BLURAY *bd);

/**
 *
 *  Return current time
 *
 * @param bd  BLURAY object
 * @return current time
 */
uint64_t bd_tell_time(BLURAY *bd);


/*
 * player settings
 */

typedef enum {
    BLURAY_PLAYER_SETTING_AUDIO_LANG     = 16,    /* Initial audio language.      String (ISO 639-2/T). */
    BLURAY_PLAYER_SETTING_PG_LANG        = 17,    /* Initial PG/SPU language.     String (ISO 639-2/T). */
    BLURAY_PLAYER_SETTING_MENU_LANG      = 18,    /* Initial menu language.       String (ISO 639-2/T). */
    BLURAY_PLAYER_SETTING_COUNTRY_CODE   = 19,    /* Player country code.         String (ISO 3166-1/alpha-2). */
    BLURAY_PLAYER_SETTING_REGION_CODE    = 20,    /* Player region code.          Integer. */
    BLURAY_PLAYER_SETTING_OUTPUT_PREFER  = 21,    /* Output mode preference.      Integer. */
    BLURAY_PLAYER_SETTING_PARENTAL       = 13,    /* Age for parental control.    Integer. */
    BLURAY_PLAYER_SETTING_AUDIO_CAP      = 15,    /* Audio capability.            Bit mask. */
    BLURAY_PLAYER_SETTING_VIDEO_CAP      = 29,    /* Video capability.            Bit mask. */
    BLURAY_PLAYER_SETTING_DISPLAY_CAP    = 23,    /* Display capability.          Bit mask. */
    BLURAY_PLAYER_SETTING_3D_CAP         = 24,    /* 3D capability.               Bit mask. */
    BLURAY_PLAYER_SETTING_TEXT_CAP       = 30,    /* Text Subtitle capability.    Bit mask. */
    BLURAY_PLAYER_SETTING_PLAYER_PROFILE = 31,    /* Player profile and version. */

    BLURAY_PLAYER_SETTING_DECODE_PG          = 0x100, /* Enable/disable PG (subtitle) decoder. Integer. Default: disabled. */
    BLURAY_PLAYER_SETTING_PERSISTENT_STORAGE = 0x101, /* Enable/disable BD-J persistent storage. Integer. Default: enabled. */

    BLURAY_PLAYER_PERSISTENT_ROOT        = 400,   /* Root path to the BD_J persistent storage location. String. */
    BLURAY_PLAYER_CACHE_ROOT             = 401,   /* Root path to the BD_J cache storage location. String. */
} bd_player_setting;

/**
 *
 *  Update player setting
 *
 *  Bit masks and enumeration values are defined in player_settings.h.
 *
 * @param bd  BLURAY object
 * @param idx Player setting to update
 * @param value New value for player setting
 * @return 1 on success, 0 on error (invalid setting)
 */

int bd_set_player_setting(BLURAY *bd, uint32_t idx, uint32_t value);
int bd_set_player_setting_str(BLURAY *bd, uint32_t idx, const char *value);


/*
 * events
 */

typedef enum {

    BD_EVENT_NONE       = 0,  /* no pending events */

    /*
     * errors
     */

    BD_EVENT_ERROR        = 1,  /* Fatal error. Playback can't be continued. */
    BD_EVENT_READ_ERROR   = 2,  /* Reading of .m2ts aligned unit failed. Next call to read will try next block. */
    BD_EVENT_ENCRYPTED    = 3,  /* .m2ts file is encrypted and can't be played */

    /*
     * current playback position
     */

    BD_EVENT_ANGLE        = 4,  /* current angle, 1...N */
    BD_EVENT_TITLE        = 5,  /* current title, 1...N (0 = top menu) */
    BD_EVENT_PLAYLIST     = 6,  /* current playlist (xxxxx.mpls) */
    BD_EVENT_PLAYITEM     = 7,  /* current play item, 0...N-1  */
    BD_EVENT_CHAPTER      = 8,  /* current chapter, 1...N */
    BD_EVENT_PLAYMARK     = 30, /* playmark reached */
    BD_EVENT_END_OF_TITLE = 9,

    /*
     * stream selection
     */

    BD_EVENT_AUDIO_STREAM           = 10,  /* 1..32,  0xff  = none */
    BD_EVENT_IG_STREAM              = 11,  /* 1..32                */
    BD_EVENT_PG_TEXTST_STREAM       = 12,  /* 1..255, 0xfff = none */
    BD_EVENT_PIP_PG_TEXTST_STREAM   = 13,  /* 1..255, 0xfff = none */
    BD_EVENT_SECONDARY_AUDIO_STREAM = 14,  /* 1..32,  0xff  = none */
    BD_EVENT_SECONDARY_VIDEO_STREAM = 15,  /* 1..32,  0xff  = none */

    BD_EVENT_PG_TEXTST              = 16,  /* 0 - disable, 1 - enable */
    BD_EVENT_PIP_PG_TEXTST          = 17,  /* 0 - disable, 1 - enable */
    BD_EVENT_SECONDARY_AUDIO        = 18,  /* 0 - disable, 1 - enable */
    BD_EVENT_SECONDARY_VIDEO        = 19,  /* 0 - disable, 1 - enable */
    BD_EVENT_SECONDARY_VIDEO_SIZE   = 20,  /* 0 - PIP, 0xf - fullscreen */

    /*
     * playback control
     */

    /* HDMV VM or JVM stopped playlist playback. Flush all buffers. */
    BD_EVENT_PLAYLIST_STOP          = 31,

    /* discontinuity in the stream (non-seamless connection). Reset demuxer PES buffers. */
    BD_EVENT_DISCONTINUITY          = 28,  /* new timestamp (45 kHz) */

    /* HDMV VM or JVM seeked the stream. Next read() will return data from new position. Flush all buffers. */
    BD_EVENT_SEEK                   = 21,  /* new media time (45 kHz) */

    /* still playback (pause) */
    BD_EVENT_STILL                  = 22,  /* 0 - off, 1 - on */

    /* Still playback for n seconds (reached end of still mode play item).
     * Playback continues by calling bd_read_skip_still(). */
    BD_EVENT_STILL_TIME             = 23,  /* 0 = infinite ; 1...300 = seconds */

    /* Play sound effect */
    BD_EVENT_SOUND_EFFECT           = 24,  /* effect ID */

    /*
     * status
     */

    /* Nothing to do. Playlist is not playing, but title applet is running.
     * Application should not call bd_read*() immediately again to avoid busy loop. */
    BD_EVENT_IDLE                   = 29,

    /* Pop-Up menu available */
    BD_EVENT_POPUP                  = 25,  /* 0 - no, 1 - yes */

    /* Interactive menu visible */
    BD_EVENT_MENU                   = 26,  /* 0 - no, 1 - yes */

    /* 3D */
    BD_EVENT_STEREOSCOPIC_STATUS    = 27,  /* 0 - 2D, 1 - 3D */

    /* BD-J key interest table changed */
    BD_EVENT_KEY_INTEREST_TABLE     = 32,  /* bitmask, BLURAY_KIT_* */

    /* UO mask changed */
    BD_EVENT_UO_MASK_CHANGED        = 33,  /* bitmask, BLURAY_UO_* */

    /*BD_EVENT_LAST = 33, */

} bd_event_e;

typedef struct {
    uint32_t   event;  /* bd_event_e */
    uint32_t   param;
} BD_EVENT;

/* BD_EVENT_ERROR param values */
#define BD_ERROR_HDMV    1
#define BD_ERROR_BDJ     2

/* BD_EVENT_ENCRYPTED param vlues */
#define BD_ERROR_AACS    3
#define BD_ERROR_BDPLUS  4

/* BD_EVENT_TITLE special titles */
#define BLURAY_TITLE_FIRST_PLAY  0xffff
#define BLURAY_TITLE_TOP_MENU    0

/* BD_EVENT_KEY_INTEREST flags */
#define BLURAY_KIT_PLAY          0x1
#define BLURAY_KIT_STOP          0x2
#define BLURAY_KIT_FFW           0x4
#define BLURAY_KIT_REW           0x8
#define BLURAY_KIT_TRACK_NEXT    0x10
#define BLURAY_KIT_TRACK_PREV    0x20
#define BLURAY_KIT_PAUSE         0x40
#define BLURAY_KIT_STILL_OFF     0x80
#define BLURAY_KIT_SEC_AUDIO     0x100
#define BLURAY_KIT_SEC_VIDEO     0x200
#define BLURAY_KIT_PG_TEXTST     0x400

/* BD_EVENT_UO_MASK flags */
#define BLURAY_UO_MENU_CALL      0x1
#define BLURAY_UO_TITLE_SEARCH   0x2

/**
 *
 *  Get event from libbluray event queue.
 *
 * @param bd  BLURAY object
 * @param event next BD_EVENT from event queue, NULL to initialize event queue
 * @return 1 on success, 0 if no events
 */
int  bd_get_event(BLURAY *bd, BD_EVENT *event);


/*
 * On-screen display
 */

struct bd_overlay_s;      /* defined in overlay.h */
struct bd_argb_overlay_s; /* defined in overlay.h */
struct bd_argb_buffer_s;  /* defined in overlay.h */
typedef void (*bd_overlay_proc_f)(void *, const struct bd_overlay_s * const);
typedef void (*bd_argb_overlay_proc_f)(void *, const struct bd_argb_overlay_s * const);

/**
 *
 *  Register handler for compressed YUV overlays
 *
 *  Compressed YUV overlays are used with presentation graphics (subtitles)
 *  and HDMV mode menus.
 *  This function can be used when player does not support full-screen ARGB overlays
 *  or player can optimize drawing of compressed overlays, color space conversion etc.
 *
 *  Callback function is called from application thread context while bd_*() functions
 *  are called.
 *
 *  Note that BD-J mode outputs only ARGB graphics.
 *
 * @param bd  BLURAY object
 * @param handle application-specific handle that will be passed to handler function
 * @param func handler function pointer
 * @return 1 on success, 0 if error
 */
void bd_register_overlay_proc(BLURAY *bd, void *handle, bd_overlay_proc_f func);

/**
 *
 *  Register handler for ARGB overlays
 *
 *  ARGB overlays are used with BD-J (Java) menus.
 *
 *  Callback function can be called at any time by a thread created by Java VM.
 *  No more than single call for each overlay plane are executed in paraller.
 *
 * @param bd  BLURAY object
 * @param handle  application-specific handle that will be passed to handler function
 * @param func  handler function pointer
 * @param buf  optional application-allocated frame buffer
 * @return 1 on success, 0 if error
 */
void bd_register_argb_overlay_proc(BLURAY *bd, void *handle, bd_argb_overlay_proc_f func, struct bd_argb_buffer_s *buf);


/*
 * Playback with on-disc menus
 */

/**
 *
 *  Start playing disc with on-disc menus
 *
 *  Playback is started from "First Play" title.
 *
 * @param bd  BLURAY object
 * @return 1 on success, 0 if error
 */
int  bd_play(BLURAY *bd);

/**
 *
 *  Play a title (from disc index).
 *
 *  Title 0      = Top Menu
 *  Title 0xffff = First Play title
 *  Number of titles can be found from BLURAY_DISC_INFO.
 *
 * @param bd  BLURAY object
 * @param title title number from disc index
 * @return 1 on success, 0 if error
 */
int  bd_play_title(BLURAY *bd, unsigned title);

/**
 *
 *  Open BluRay disc Top Menu.
 *
 *  Current pts is needed for resuming playback when menu is closed.
 *
 * @param bd  BLURAY object
 * @param pts current playback position (1/90000s) or -1
 * @return 1 on success, 0 if error
 */
int  bd_menu_call(BLURAY *bd, int64_t pts);

/**
 *
 *  Read from currently playing title.
 *
 *  When playing disc in navigation mode this function must be used instead of bd_read().
 *
 * @param bd  BLURAY object
 * @param buf buffer to read data into
 * @param len size of data to be read
 * @param event next BD_EVENT from event queue (BD_EVENT_NONE if no events)
 * @return size of data read, -1 if error, 0 if event needs to be handled first, 0 if end of title was reached
 */
int  bd_read_ext(BLURAY *bd, unsigned char *buf, int len, BD_EVENT *event);

/**
 *
 *  Continue reading after still mode clip
 *
 * @param bd  BLURAY object
 * @return 0 on error
 */
int bd_read_skip_still(BLURAY *bd);

/**
 *
 *  Get information about a playlist
 *
 * @param bd  BLURAY object
 * @param playlist playlist number
 * @param angle angle number (chapter offsets and clip size depend on selected angle)
 * @return allocated BLURAY_TITLE_INFO object, NULL on error
 */
BLURAY_TITLE_INFO* bd_get_playlist_info(BLURAY *bd, uint32_t playlist, unsigned angle);

/**
 *
 *  Get sound effect
 *
 * @param bd  BLURAY object
 * @param effect_id  sound effect id (0...N)
 * @param effect     sound effect data
 * @return <0 when no effects, 0 when id out of range, 1 on success
 */
int bd_get_sound_effect(BLURAY *bd, unsigned sound_id, struct bd_sound_effect *effect);


/*
 * User interaction
 */

/**
 *
 *  Update current pts.
 *
 * @param bd  BLURAY object
 * @param pts current playback position (1/90000s) or -1
 */
void bd_set_scr(BLURAY *bd, int64_t pts);

/**
 *
 *  Set current playback rate.
 *
 *  Notify BD-J media player when user changes playback rate
 *  (ex. pauses playback).
 *  Changing rate may fail if corresponding UO is masked or
 *  playlist is not playing.
 *
 * @param bd  BLURAY object
 * @param rate current playback rate * 90000 (0 = paused, 90000 = normal)
 * @return <0 on error, 0 on success
 */
#define BLURAY_RATE_PAUSED  0
#define BLURAY_RATE_NORMAL  90000
int bd_set_rate(BLURAY *bd, uint32_t rate);

/**
 *
 *  Pass user input to graphics controller.
 *  Keys are defined in libbluray/keys.h.
 *  Current pts can be updated by using BD_VK_NONE key. This is required for animated menus.
 *
 * @param bd  BLURAY object
 * @param pts current playback position (1/90000s) or -1
 * @param key input key
 * @return <0 on error, 0 on success, >0 if selection/activation changed
 */
int bd_user_input(BLURAY *bd, int64_t pts, uint32_t key);

/**
 *
 *  Select menu button at location (x,y).
 *
 * @param bd  BLURAY object
 * @param pts current playback position (1/90000s) or -1
 * @param x mouse pointer x-position
 * @param y mouse pointer y-position
 * @return <0 on error, 0 when mouse is outside of buttons, 1 when mouse is inside button
 */
int bd_mouse_select(BLURAY *bd, int64_t pts, uint16_t x, uint16_t y);


/*
 * Testing and debugging
 */

/* access to internal information */

struct clpi_cl;
/**
 *
 *  Get copy of clip information for requested playitem.
 *
 * @param bd  BLURAY objects
 * @param clip_ref  requested playitem number
 * @return pointer to allocated CLPI_CL object on success, NULL on error
 */
struct clpi_cl *bd_get_clpi(BLURAY *bd, unsigned clip_ref);
struct clpi_cl *bd_read_clpi(const char *clpi_file);

/**
 *
 *  Free CLPI_CL object
 *
 * @param cl  CLPI_CL objects
 */
void bd_free_clpi(struct clpi_cl *cl);


struct mpls_pl;
struct mpls_pl *bd_read_mpls(const char *mpls_file);
void bd_free_mpls(struct mpls_pl *);

struct mobj_objects;
struct mobj_objects *bd_read_mobj(const char *mobj_file);
void bd_free_mobj(struct mobj_objects *);

struct bdjo_data;
struct bdjo_data *bd_read_bdjo(const char *bdjo_file);
void bd_free_bdjo(struct bdjo_data *);

/* BD-J testing */

int  bd_start_bdj(BLURAY *bd, const char* start_object); // start BD-J from the specified BD-J object (should be a 5 character string)
void bd_stop_bdj(BLURAY *bd); // shutdown BD-J and clean up resources

/**
 *
 *  Read a file from BluRay Virtual File System.
 *
 *  Allocate large enough memory block and read file contents.
 *  Caller must free the memory block with free().
 *
 * @param bd  BLURAY object
 * @param file_name  path to the file (relative to disc root)
 * @param data  where to store pointer to allocated data
 * @param size  where to store file size
 * @return 1 on success, 0 on error
 */
int bd_read_file(BLURAY *, const char *path, void **data, int64_t *size);


#ifdef __cplusplus
}
#endif

#endif /* BLURAY_H_ */
