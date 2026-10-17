/*
 * Copyright (C) 2000, 2001 Björn Englund <d4bjorn@dtek.chalmers.se>,
 *                          Håkan Hjort <d95hjort@dtek.chalmers.se>
 *
 * This file is part of libdvdread.
 *
 * libdvdread is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * libdvdread is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with libdvdread; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef LIBDVDREAD_IFO_TYPES_H
#define LIBDVDREAD_IFO_TYPES_H

#include <inttypes.h>

#include <dvdread/dvd_reader.h>

#undef ATTRIBUTE_PACKED

#if defined(__GNUC__) || defined(__clang__)
# define ATTRIBUTE_PACKED __attribute__ ((packed))
# define PRAGMA_PACK 0
#endif

#if !defined(ATTRIBUTE_PACKED)
#define ATTRIBUTE_PACKED
#define PRAGMA_PACK 1
#endif

#if PRAGMA_PACK
#pragma pack(push, 1)
#endif


/**
 * Common
 *
 * The following structures are used in both the VMGI and VTSI.
 */


/**
 * DVD Time Information.
 */
typedef struct {
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint8_t frame_u; /* The two high bits are the frame rate. */
} ATTRIBUTE_PACKED dvd_time_t;
#define DVD_TIME_SIZE 4U

/**
 * Type to store per-command data.
 */
typedef struct {
  uint8_t bytes[8];
} ATTRIBUTE_PACKED vm_cmd_t;
#define COMMAND_DATA_SIZE 8U


/**
 * Video Attributes.
 */
typedef struct {
  unsigned char mpeg_version         : 2;
  unsigned char video_format         : 2;
  unsigned char display_aspect_ratio : 2;
  unsigned char permitted_df         : 2;

  unsigned char line21_cc_1          : 1;
  unsigned char line21_cc_2          : 1;
  unsigned char unknown1             : 1;
  unsigned char bit_rate             : 1;

  unsigned char picture_size         : 2;
  unsigned char letterboxed          : 1;
  unsigned char film_mode            : 1;
} ATTRIBUTE_PACKED video_attr_t;
#define VIDEO_ATTR_SIZE  2U

/**
 * Audio code extension, see SPRM 17.
 */
typedef enum {
  DVD_AUDIO_CODE_EXT_UNSPECIFIED       = 0,
  DVD_AUDIO_CODE_EXT_NORMAL            = 1,
  DVD_AUDIO_CODE_EXT_VISUALLY_IMPAIRED = 2,
  DVD_AUDIO_CODE_EXT_DIRECTOR          = 3, /**< director's comments */
  DVD_AUDIO_CODE_EXT_ALT_DIRECTOR      = 4, /**< alternate director's comments */
} dvd_audio_code_ext_t;

/**
 * Audio Attributes.
 */
typedef struct {
  unsigned char audio_format           : 3;
  unsigned char multichannel_extension : 1;
  unsigned char lang_type              : 2;
  unsigned char application_mode       : 2;

  unsigned char quantization           : 2;
  unsigned char sample_frequency       : 2;
  unsigned char unknown1               : 1;
  unsigned char channels               : 3;
  uint16_t lang_code;
  uint8_t  lang_extension;
  uint8_t  code_extension; /**< dvd_audio_code_ext_t */
  uint8_t unknown3;
  union {
    struct ATTRIBUTE_PACKED {
      unsigned char unknown4           : 1;
      unsigned char channel_assignment : 3;
      unsigned char version            : 2;
      unsigned char mc_intro           : 1; /* probably 0: true, 1:false */
      unsigned char mode               : 1; /* Karaoke mode 0: solo 1: duet */
    } karaoke;
    struct ATTRIBUTE_PACKED {
      unsigned char unknown5           : 4;
      unsigned char dolby_encoded      : 1; /* suitable for surround decoding */
      unsigned char unknown6           : 3;
    } surround;
  } ATTRIBUTE_PACKED app_info;
} ATTRIBUTE_PACKED audio_attr_t;
#define AUDIO_ATTR_SIZE  8U


/**
 * MultiChannel Extension
 */
typedef struct {
  unsigned char zero1      : 7;
  unsigned char ach0_gme   : 1;

  unsigned char zero2      : 7;
  unsigned char ach1_gme   : 1;

  unsigned char zero3      : 4;
  unsigned char ach2_gv1e  : 1;
  unsigned char ach2_gv2e  : 1;
  unsigned char ach2_gm1e  : 1;
  unsigned char ach2_gm2e  : 1;

  unsigned char zero4      : 4;
  unsigned char ach3_gv1e  : 1;
  unsigned char ach3_gv2e  : 1;
  unsigned char ach3_gmAe  : 1;
  unsigned char ach3_se2e  : 1;

  unsigned char zero5      : 4;
  unsigned char ach4_gv1e  : 1;
  unsigned char ach4_gv2e  : 1;
  unsigned char ach4_gmBe  : 1;
  unsigned char ach4_seBe  : 1;
  uint8_t zero6[19];
} ATTRIBUTE_PACKED multichannel_ext_t;
#define MULTICHANNEL_EXT_SIZE  24U


/**
 * Subpicture code extension, see SPRM 19.
 */
typedef enum {
  DVD_SUBP_CODE_EXT_UNSPECIFIED       = 0,
  DVD_SUBP_CODE_EXT_NORMAL            = 1,
  DVD_SUBP_CODE_EXT_LARGE             = 2,
  DVD_SUBP_CODE_EXT_CHILDREN          = 3,
  DVD_SUBP_CODE_EXT_NORMAL_CAPTIONS   = 5,
  DVD_SUBP_CODE_EXT_LARGE_CAPTIONS    = 6,
  DVD_SUBP_CODE_EXT_CHILDREN_CAPTIONS = 7,
  DVD_SUBP_CODE_EXT_FORCED            = 9,
  DVD_SUBP_CODE_EXT_DIRECTOR          = 13, /**< director's comments */
  DVD_SUBP_CODE_EXT_LARGE_DIRECTOR    = 14, /**< large director's comments */
  DVD_SUBP_CODE_EXT_CHILDREN_DIRECTOR = 15, /**< director's comments for children */
} dvd_subp_code_ext_t;

/**
 * Subpicture Attributes.
 */
typedef struct {
  /*
   * type: 0 not specified
   *       1 language
   *       2 other
   * coding mode: 0 run length
   *              1 extended
   *              2 other
   * language: indicates language if type == 1
   * lang extension: if type == 1 contains the lang extension
   */
  unsigned char code_mode : 3;
  unsigned char zero1     : 3;
  unsigned char type      : 2;
  uint8_t  zero2;
  uint16_t lang_code;
  uint8_t  lang_extension;
  uint8_t  code_extension; /**< dvd_subp_code_ext_t */
} ATTRIBUTE_PACKED subp_attr_t;
#define SUBP_ATTR_SIZE  6U



/**
 * PGC Command Table.
 */
typedef struct {
  uint16_t nr_of_pre;
  uint16_t nr_of_post;
  uint16_t nr_of_cell;
  uint16_t last_byte;
  vm_cmd_t *pre_cmds;
  vm_cmd_t *post_cmds;
  vm_cmd_t *cell_cmds;
} ATTRIBUTE_PACKED pgc_command_tbl_t;
#define PGC_COMMAND_TBL_SIZE 8U

/**
 * PGC Program Map
 */
typedef uint8_t pgc_program_map_t;

/**
 * Cell Playback Information.
 */
typedef struct {
  unsigned char block_mode       : 2;
  unsigned char block_type       : 2;
  unsigned char seamless_play    : 1;
  unsigned char interleaved      : 1;
  unsigned char stc_discontinuity: 1;
  unsigned char seamless_angle   : 1;
  unsigned char zero_1           : 1;
  unsigned char playback_mode    : 1;  /**< When set, enter StillMode after each VOBU */
  unsigned char restricted       : 1;  /**< ?? drop out of fastforward? */
  unsigned char cell_type        : 5;  /** for karaoke, reserved otherwise */
  uint8_t still_time;
  uint8_t cell_cmd_nr;
  dvd_time_t playback_time;
  uint32_t first_sector;
  uint32_t first_ilvu_end_sector;
  uint32_t last_vobu_start_sector;
  uint32_t last_sector;
} ATTRIBUTE_PACKED cell_playback_t;
#define CELL_PLAYBACK_SIZE  24U

#define BLOCK_TYPE_NONE         0x0
#define BLOCK_TYPE_ANGLE_BLOCK  0x1

#define BLOCK_MODE_NOT_IN_BLOCK 0x0
#define BLOCK_MODE_FIRST_CELL   0x1
#define BLOCK_MODE_IN_BLOCK     0x2
#define BLOCK_MODE_LAST_CELL    0x3

/**
 * Cell Position Information.
 */
typedef struct {
  uint16_t vob_id_nr;
  uint8_t  zero_1;
  uint8_t  cell_nr;
} ATTRIBUTE_PACKED cell_position_t;
#define CELL_POSITION_SIZE 4U

/**
 * User Operations.
 */
typedef struct {
  unsigned char zero                           : 7; /* 25-31 */
  unsigned char video_pres_mode_change         : 1; /* 24 */

  unsigned char karaoke_audio_pres_mode_change : 1; /* 23 */
  unsigned char angle_change                   : 1;
  unsigned char subpic_stream_change           : 1;
  unsigned char audio_stream_change            : 1;
  unsigned char pause_on                       : 1;
  unsigned char still_off                      : 1;
  unsigned char button_select_or_activate      : 1;
  unsigned char resume                         : 1; /* 16 */

  unsigned char chapter_menu_call              : 1; /* 15 */
  unsigned char angle_menu_call                : 1;
  unsigned char audio_menu_call                : 1;
  unsigned char subpic_menu_call               : 1;
  unsigned char root_menu_call                 : 1;
  unsigned char title_menu_call                : 1;
  unsigned char backward_scan                  : 1;
  unsigned char forward_scan                   : 1; /* 8 */

  unsigned char next_pg_search                 : 1; /* 7 */
  unsigned char prev_or_top_pg_search          : 1;
  unsigned char time_or_chapter_search         : 1;
  unsigned char go_up                          : 1;
  unsigned char stop                           : 1;
  unsigned char title_play                     : 1;
  unsigned char chapter_search_or_play         : 1;
  unsigned char title_or_time_play             : 1; /* 0 */
} ATTRIBUTE_PACKED user_ops_t;
#define USER_OPS_SIZE  4U

/**
 * Program Chain Information.
 */
typedef struct {
  uint16_t zero_1;
  uint8_t  nr_of_programs;
  uint8_t  nr_of_cells;
  dvd_time_t playback_time;
  user_ops_t prohibited_ops;
  uint16_t audio_control[8]; /* New type? */
  uint32_t subp_control[32]; /* New type? */
  uint16_t next_pgc_nr;
  uint16_t prev_pgc_nr;
  uint16_t goup_pgc_nr;
  uint8_t  pg_playback_mode;
  uint8_t  still_time;
  uint32_t palette[16]; /* New type struct {zero_1, Y, Cr, Cb} ? */
  uint16_t command_tbl_offset;
  uint16_t program_map_offset;
  uint16_t cell_playback_offset;
  uint16_t cell_position_offset;
  pgc_command_tbl_t *command_tbl;
  pgc_program_map_t  *program_map;
  cell_playback_t *cell_playback;
  cell_position_t *cell_position;
  int      ref_count;
} ATTRIBUTE_PACKED pgc_t;
#define PGC_SIZE 236U

/**
 * Program Chain Information Search Pointer.
 */
typedef struct {
  uint8_t  entry_id;
  unsigned char block_mode : 2;
  unsigned char block_type : 2;
  unsigned char zero_1   : 4;
  uint16_t ptl_id_mask;
  uint32_t pgc_start_byte;
  pgc_t *pgc;
} ATTRIBUTE_PACKED pgci_srp_t;
#define PGCI_SRP_SIZE 8U

/**
 * Program Chain Information Table.
 */
typedef struct {
  uint16_t nr_of_pgci_srp;
  uint16_t zero_1;
  uint32_t last_byte;
  pgci_srp_t *pgci_srp;
  int      ref_count;
} ATTRIBUTE_PACKED pgcit_t;
#define PGCIT_SIZE 8U

/**
 * Menu PGCI Language Unit.
 */
typedef struct {
  uint16_t lang_code;
  uint8_t  lang_extension;
  uint8_t  exists;
  uint32_t lang_start_byte;
  pgcit_t *pgcit;
} ATTRIBUTE_PACKED pgci_lu_t;
#define PGCI_LU_SIZE 8U

/**
 * Menu PGCI Unit Table.
 */
typedef struct {
  uint16_t nr_of_lus;
  uint16_t zero_1;
  uint32_t last_byte;
  pgci_lu_t *lu;
} ATTRIBUTE_PACKED pgci_ut_t;
#define PGCI_UT_SIZE 8U

/**
 * Cell Address Information.
 */
typedef struct {
  uint16_t vob_id;
  uint8_t  cell_id;
  uint8_t  zero_1;
  uint32_t start_sector;
  uint32_t last_sector;
} ATTRIBUTE_PACKED cell_adr_t;
#define CELL_ADDR_SIZE  12U

/**
 * Cell Address Table.
 */
typedef struct {
  uint16_t nr_of_vobs; /* VOBs */
  uint16_t zero_1;
  uint32_t last_byte;
  cell_adr_t *cell_adr_table;  /* No explicit size given. */
} ATTRIBUTE_PACKED c_adt_t;
#define C_ADT_SIZE 8U

/**
 * VOBU Address Map.
 */
typedef struct {
  uint32_t last_byte;
  uint32_t *vobu_start_sectors;
} ATTRIBUTE_PACKED vobu_admap_t;
#define VOBU_ADMAP_SIZE 4U




/**
 * VMGI
 *
 * The following structures relate to the Video Manager.
 */

/**
 * Video Manager Information Management Table.
 */
typedef struct {
  char     vmg_identifier[12];
  uint32_t vmg_last_sector;
  uint8_t  zero_1[12];
  uint32_t vmgi_last_sector;
  uint8_t  zero_2;
  uint8_t  specification_version;
  uint32_t vmg_category;
  uint16_t vmg_nr_of_volumes;
  uint16_t vmg_this_volume_nr;
  uint8_t  disc_side;
  uint8_t  zero_3[19];
  uint16_t vmg_nr_of_title_sets;  /* Number of VTSs. */
  char     provider_identifier[32];
  uint64_t vmg_pos_code;
  uint8_t  zero_4[24];
  uint32_t vmgi_last_byte;
  uint32_t first_play_pgc;
  uint8_t  zero_5[56];
  uint32_t vmgm_vobs;             /* sector */
  uint32_t tt_srpt;               /* sector */
  uint32_t vmgm_pgci_ut;          /* sector */
  uint32_t ptl_mait;              /* sector */
  uint32_t vts_atrt;              /* sector */
  uint32_t txtdt_mgi;             /* sector */
  uint32_t vmgm_c_adt;            /* sector */
  uint32_t vmgm_vobu_admap;       /* sector */
  uint8_t  zero_6[32];

  video_attr_t vmgm_video_attr;
  uint8_t  zero_7;
  uint8_t  nr_of_vmgm_audio_streams; /* should be 0 or 1 */
  audio_attr_t vmgm_audio_attr;
  audio_attr_t zero_8[7];
  uint8_t  zero_9[17];
  uint8_t  nr_of_vmgm_subp_streams; /* should be 0 or 1 */
  subp_attr_t  vmgm_subp_attr;
  subp_attr_t  zero_10[27];  /* XXX: how much 'padding' here? */
} ATTRIBUTE_PACKED vmgi_mat_t;
#define VMGI_MAT_SIZE 510U

/**
 * One down mix coefficient table (ATS_DM_COEFT). Sixteen tables follow the
 * audio attributes in the ATSI_MAT; each audio program selects the table to
 * mix its multichannel audio down to two channels through the dm_coeftn of
 * its ATS_PGI:
 *
 *   Left_out  = sum over the channels of coef ch.left  * ch
 *   Right_out = sum over the channels of coef ch.right * ch
 *
 * The pairs follow the channels of the stream, given by the channel group
 * assignment of the audio attribute, except that the LFE is ordered last:
 * a 5.1 track uses Lf, Rf, C, Ls, Rs, LFE. Pairs past the channel count
 * are zero. A coefficient is a gain code: 0.2 dB per step from 0 dB at
 * 0x00 to -40 dB at 0xc8, then 0.4 dB per step down to -61.8 dB at 0xfe,
 * and 0xff mutes the channel. The tables are all zero when the ATS has no
 * AOBs of its own, and on discs whose MLP streams carry their own down
 * mix instead.
 */
typedef struct {
  uint16_t zero_1;
  struct ATTRIBUTE_PACKED {
    uint8_t left;
    uint8_t right;
  } dm_coef[8]; /* one pair per audio channel of the stream */
} ATTRIBUTE_PACKED downmix_coeff_t;
#define DOWNMIX_COEFF_SIZE 18U


/**
 * SAMG
 *
 * The following structures relate to the Simple Audio Manager, exclusive to DVD-Audio discs
 */

/**
 * This is one of two of the DVD-A content managers, for simple audio dvd only players
 */

typedef struct {
  uint16_t zero_1;
  uint8_t  group_num;
  uint8_t  chapter_num; /* chapter number within group*/
  uint32_t timestamp_pts; /* this is MPEG time, Not DVD time */
  uint32_t chapter_len;
  uint32_t zero_2;
  uint8_t  record_code; /* the top bit is set on the first chapter of a group */
  /* the next three bytes use the audio pack private header encoding: a
   * nibble per channel group, 0xf when there is no second group */
  uint8_t  bit_depth;          /* quantization: 0 16-bit, 1 20-bit, 2 24-bit */
  uint8_t  sampling_rate;      /* 0 48 kHz, 8 44.1 kHz, +1 and +2 double it */
  uint8_t  channel_assignment; /* channel layout code, 0 to 20 */
  /* some DVD's made with authoring software keep downmix coefficients here:
   * bytes 4 to 15 then hold six left/right gain code pairs as in the
   * ATSI_MAT tables */
  uint8_t  zero_3[20];
  uint32_t start_sector_1; /* first sector of the chapter, an absolute sector of the volume */
  uint32_t start_sector_2; /* the same sector repeated again */
  uint32_t end_sector;     /* last sector of the chapter, absolute */
} ATTRIBUTE_PACKED samg_chapter_t;
#define SAMG_CHAPTER_SIZE 52U

typedef struct {
  char           samg_identifier[12];
  uint16_t       nr_chapters;
  uint8_t        unknown_1;             /* 0, 1 or 2 seen, not the volume number */
  uint8_t        specification_version; /* 0x12, a few discs write zero */
  samg_chapter_t *samg_chapters;
} ATTRIBUTE_PACKED samg_mat_t;
#define SAMG_MAT_SIZE 16U

/**
 * AMGI
 *
 * The following structures relate to the Audio Manager, exclusive to DVD-Audio discs
 */

/**
 * Audio Manager Information Management Table.
 */
typedef struct {
  char     amg_identifier[12];
  uint32_t amg_start_sector;
  uint8_t  zero_1[12];
  uint32_t amgi_last_sector;
  uint16_t specification_version;
  uint8_t  zero_2[4];
  uint16_t amg_nr_of_volumes;
  uint16_t amg_this_volume_nr;
  uint8_t  disc_side;
  uint8_t  zero_3[4];
  uint8_t  autoplay;
  uint32_t audio_sv_ifo_relative_p;
  uint16_t unknown_1; /* some discs have a value here, undocumented in DVD-Audio specs */
  uint8_t  zero_4[8];
  uint8_t  vmg_nr_of_title_sets;  /* Number of video titlesets in audio zone. */
  uint8_t  amg_nr_of_title_sets;  /* Number of audio titlesets in audio zone. */
  char     provider_identifier[32];
  uint8_t  unknown_3[8]; /* may be set to zeros */
  uint8_t  zero_5[24];  
  uint32_t amg_end_byte_address;
  uint8_t  unknown_4[4]; /* may be set to zeros */
  uint8_t  zero_6[56];
  uint32_t amgm_vobs_sa;    /* start sector of the menu AMGM_VOBS; zero without a menu */
  uint32_t att_srpt_sa;     /* start sector of the audio title SRPT */
  uint32_t aott_srpt_sa;    /* start sector of the audio only title SRPT */
  uint32_t amgm_pgci_ut_sa; /* start sector of the menu PGCI unit table */
  uint8_t  zero_9[4];
  uint32_t txtdt_mgi_sa;    /* start sector of the text data manager */
  uint8_t  zero_10[40];
  uint8_t  last_sector_audio_sys_space;
  uint8_t  zero_11[79];
  uint8_t  menu_prescence_3; /* will be set to 0x01*/
    /* XXX lots of padding after this to complete the sector*/
} ATTRIBUTE_PACKED amgi_mat_t;
#define AMGI_MAT_SIZE 337U

/* The ATT_SRPT (at att_srpt_sa) may have video tracks, the AOTT_SRPT (at
 * aott_srpt_sa) will not. If there are no video tracks the tables will be
 * the same */

/*this struct repeats for every audio or video track*/
typedef struct {
  uint8_t  type_and_rank; /*first nibble is type, second rank, from 1-9*/
  uint8_t  nr_chapters_in_title;
  uint8_t  nr_visible_chapters_in_vts_title; /* this will be zeros in an audio record */
  uint8_t  zero_1;
  uint32_t len_audio_zone_pts; /* this is MPEG time, Not DVD time */

  uint8_t  group_property; /* in a video track, this is video titleset number, in audio its rank of group */
  uint8_t  title_property; /* in video track this is title number , in audio track this is rank of title*/
  uint32_t ts_pointer_relative_sector; /* for ats or vts*/
} ATTRIBUTE_PACKED track_info_t;
#define TRACK_INFO_SIZE 14U

typedef struct {
  uint16_t     nr_of_titles;
  uint16_t     last_byte_in_table;
  track_info_t *tracks_info;
} ATTRIBUTE_PACKED tracks_info_table_t;
#define TRACKS_INFO_TABLE_SIZE 4U


typedef struct {
  unsigned char zero_1                    : 1;
  unsigned char multi_or_random_pgc_title : 1; /* 0: one sequential pgc title */
  unsigned char jlc_exists_in_cell_cmd    : 1;
  unsigned char jlc_exists_in_prepost_cmd : 1;
  unsigned char jlc_exists_in_button_cmd  : 1;
  unsigned char jlc_exists_in_tt_dom      : 1;
  unsigned char chapter_search_or_play    : 1; /* UOP 1 */
  unsigned char title_or_time_play        : 1; /* UOP 0 */
} ATTRIBUTE_PACKED playback_type_t;
#define PLAYBACK_TYPE_SIZE 1U

/**
 * Title Information.
 */
typedef struct {
  playback_type_t pb_ty;
  uint8_t  nr_of_angles;
  uint16_t nr_of_ptts;
  uint16_t parental_id;
  uint8_t  title_set_nr;
  uint8_t  vts_ttn;
  uint32_t title_set_sector;
} ATTRIBUTE_PACKED title_info_t;
#define TITLE_INFO_SIZE 12U

/**
 * PartOfTitle Search Pointer Table.
 */
typedef struct {
  uint16_t nr_of_srpts;
  uint16_t zero_1;
  uint32_t last_byte;
  title_info_t *title;
} ATTRIBUTE_PACKED tt_srpt_t;
#define TT_SRPT_SIZE 8U


/**
 * Parental Management Information Unit Table.
 * Level 1 (US: G), ..., 7 (US: NC-17), 8
 */
#define PTL_MAIT_NUM_LEVEL 8
typedef uint16_t pf_level_t[PTL_MAIT_NUM_LEVEL];

/**
 * Parental Management Information Unit Table.
 */
typedef struct {
  uint16_t country_code;
  uint16_t zero_1;
  uint16_t pf_ptl_mai_start_byte;
  uint16_t zero_2;
  pf_level_t *pf_ptl_mai; /* table of (nr_of_vtss + 1), video_ts is first */
} ATTRIBUTE_PACKED ptl_mait_country_t;
#define PTL_MAIT_COUNTRY_SIZE 8U

/**
 * Parental Management Information Table.
 */
typedef struct {
  uint16_t nr_of_countries;
  uint16_t nr_of_vtss;
  uint32_t last_byte;
  ptl_mait_country_t *countries;
} ATTRIBUTE_PACKED ptl_mait_t;
#define PTL_MAIT_SIZE 8U

/**
 * Video Title Set Attributes.
 */
typedef struct {
  uint32_t last_byte;
  uint32_t vts_cat;

  video_attr_t vtsm_vobs_attr;
  uint8_t  zero_1;
  uint8_t  nr_of_vtsm_audio_streams; /* should be 0 or 1 */
  audio_attr_t vtsm_audio_attr;
  audio_attr_t zero_2[7];
  uint8_t  zero_3[16];
  uint8_t  zero_4;
  uint8_t  nr_of_vtsm_subp_streams; /* should be 0 or 1 */
  subp_attr_t vtsm_subp_attr;
  subp_attr_t zero_5[27];

  uint8_t  zero_6[2];

  video_attr_t vtstt_vobs_video_attr;
  uint8_t  zero_7;
  uint8_t  nr_of_vtstt_audio_streams;
  audio_attr_t vtstt_audio_attr[8];
  uint8_t  zero_8[16];
  uint8_t  zero_9;
  uint8_t  nr_of_vtstt_subp_streams;
  subp_attr_t vtstt_subp_attr[32];
} ATTRIBUTE_PACKED vts_attributes_t;
#define VTS_ATTRIBUTES_SIZE 542U
#define VTS_ATTRIBUTES_MIN_SIZE 356U

/**
 * Video Title Set Attribute Table.
 */
typedef struct {
  uint16_t nr_of_vtss;
  uint16_t zero_1;
  uint32_t last_byte;
  vts_attributes_t *vts;
  uint32_t *vts_atrt_offsets; /* offsets table for each vts_attributes */
} ATTRIBUTE_PACKED vts_atrt_t;
#define VTS_ATRT_SIZE 8U

/**
 * Text Data. (Incomplete)
 */
typedef struct {
  uint32_t last_byte;    /* offsets are relative here */
  uint16_t offsets[100]; /* == nr_of_srpts + 1 (first is disc title) */
#if 0
  uint16_t unknown; /* 0x48 ?? 0x48 words (16bit) info following */
  uint16_t zero_1;

  uint8_t type_of_info; /* ?? 01 == disc, 02 == Title, 04 == Title part */
  uint8_t unknown1;
  uint8_t unknown2;
  uint8_t unknown3;
  uint8_t unknown4; /* ?? always 0x30 language?, text format? */
  uint8_t unknown5;
  uint16_t offset; /* from first */

  char text[12]; /* ended by 0x09 */
#endif
} ATTRIBUTE_PACKED txtdt_t;
#define TXTDT_SIZE 204U

/**
 * Text Data Language Unit. (Incomplete)
 */
typedef struct {
  uint16_t lang_code;
  uint8_t  zero_1;
  uint8_t char_set;      /* 0x00 reserved Unicode, 0x01 ISO 646, 0x10 JIS Roman & JIS Kanji, 0x11 ISO 8859-1, 0x12 Shift JIS Kanji */
  uint32_t txtdt_start_byte;  /* prt, rel start of vmg_txtdt_mgi  */
  txtdt_t  *txtdt;
} ATTRIBUTE_PACKED txtdt_lu_t;
#define TXTDT_LU_SIZE 8U

/**
 * Text Data Manager Information. (Incomplete)
 */
typedef struct {
  char disc_name[12];
  uint16_t unknown1;
  uint16_t nr_of_language_units;
  uint32_t last_byte;
  txtdt_lu_t *lu;
} ATTRIBUTE_PACKED txtdt_mgi_t;
#define TXTDT_MGI_SIZE 20U


/**
 * VTS
 *
 * Structures relating to the Video Title Set (VTS).
 */

/**
 * Video Title Set Information Management Table.
 */
typedef struct {
  char vts_identifier[12];
  uint32_t vts_last_sector;
  uint8_t  zero_1[12];
  uint32_t vtsi_last_sector;
  uint8_t  zero_2;
  uint8_t  specification_version;
  uint32_t vts_category;
  uint16_t zero_3;
  uint16_t zero_4;
  uint8_t  zero_5;
  uint8_t  zero_6[19];
  uint16_t zero_7;
  uint8_t  zero_8[32];
  uint64_t zero_9;
  uint8_t  zero_10[24];
  uint32_t vtsi_last_byte;
  uint32_t zero_11;
  uint8_t  zero_12[56];
  uint32_t vtsm_vobs;       /* sector */
  uint32_t vtstt_vobs;      /* sector */
  uint32_t vts_ptt_srpt;    /* sector */
  uint32_t vts_pgcit;       /* sector */
  uint32_t vtsm_pgci_ut;    /* sector */
  uint32_t vts_tmapt;       /* sector */
  uint32_t vtsm_c_adt;      /* sector */
  uint32_t vtsm_vobu_admap; /* sector */
  uint32_t vts_c_adt;       /* sector */
  uint32_t vts_vobu_admap;  /* sector */
  uint8_t  zero_13[24];

  video_attr_t vtsm_video_attr;
  uint8_t  zero_14;
  uint8_t  nr_of_vtsm_audio_streams; /* should be 0 or 1 */
  audio_attr_t vtsm_audio_attr;
  audio_attr_t zero_15[7];
  uint8_t  zero_16[17];
  uint8_t  nr_of_vtsm_subp_streams; /* should be 0 or 1 */
  subp_attr_t vtsm_subp_attr;
  subp_attr_t zero_17[27];
  uint8_t  zero_18[2];

  video_attr_t vts_video_attr;
  uint8_t  zero_19;
  uint8_t  nr_of_vts_audio_streams;
  audio_attr_t vts_audio_attr[8];
  uint8_t  zero_20[17];
  uint8_t  nr_of_vts_subp_streams;
  subp_attr_t vts_subp_attr[32];
  uint16_t zero_21;
  multichannel_ext_t vts_mu_audio_attr[8];
  /* XXX: how much 'padding' here, if any? */
} ATTRIBUTE_PACKED vtsi_mat_t;
#define VTSI_MAT_SIZE 984U


typedef struct {
  /* 0x0000 for lpcm or 0x0100 for mlp, DTS seems to be 0x0500 and has different fields*/
  uint8_t encoding;
  uint8_t unknown1;
  /* for stereo 0f 16 bit, 1f 20 bit, 2f 24 bit*/
  /* f is a placeholder for the nibbles that is used for channel groups*/
  /* otherwise if its 5.1 channels each nibble represents a channel group (G1, G2),*/
  uint8_t  bitrate; 
  /* 8f 44khz, 0f 48khz, 1f 96khz, 2f 192khz*/
  /* otherwise if its 5.1 channels each nibble represents a channel group (G1, G2)*/
  uint8_t  sampling_frequency; 
  /* 00 1 channel, 01 2 channels, 11 5.1 channels*/
  uint8_t  nr_channels;
  uint8_t  unknown2;
  uint8_t zero[10];
} ATTRIBUTE_PACKED atsi_record_t;
#define ATSI_RECORD_SIZE 16U

/**
 * ATS
 *
 * Structures relating to the Audio Title Set (ATS).
 */

/**
 * Audio Title Set Information Management Table. Exclusive to DVD-Audio discs
 */
#define ATSI_RECORD_MAX_SIZE 8
#define DOWNMIX_COEFF_MAX_SIZE 16

typedef struct {
  char          ats_identifier[12];
  uint32_t      ats_last_sector; /* last sector of ATS_XX_0.BUP*/
  uint8_t       zero_1[12];
  uint32_t      atsi_last_sector; /* last sector of ATS_XX_0.IFO*/
  uint16_t      specification_version; /* always 0x0012 */
  uint32_t      unknown_1;
  uint8_t       zero_2[90];
  uint32_t      atsi_last_byte; /* last byte of sector 0, not the end of the file */
  uint8_t       zero_3[60];
  uint32_t      vts_sa;          /* start sector of the video title set whose
                                    title VOBs this ATS plays in place of AOBs
                                    of its own (VTS_SA), counted from the first
                                    sector of the AMG.  Only set on hybrid
                                    discs, for an ATS without AOB files;
                                    zero otherwise */
  uint32_t      atst_aobs;       /* start sector of the audio objects: within
                                    this ATS (AOTT_AOBS_SA), or of the title
                                    VOBs within the linked video title set
                                    (VTSTT_VOBS_SA) when vts_sa is set */
  uint32_t      vts_ptt_srpt;    /* may be zeros */
  uint32_t      ats_pgci_ut;    /* sector */
  uint32_t      vtsm_pgci_ut;    /* may be zeros */
  uint32_t      vts_tmapt;       /* sector */
  uint32_t      vtsm_c_adt;      /* sector */
  uint32_t      vtsm_vobu_admap; /* sector */
  uint32_t      vts_c_adt;       /* sector */
  uint32_t      vts_vobu_admap;  /* sector */
  uint8_t       zero_4[24];
  /* the majority, or even all of these entries may be zero*/
  atsi_record_t atsi_record[ATSI_RECORD_MAX_SIZE];
  downmix_coeff_t downmix_coefficients[DOWNMIX_COEFF_MAX_SIZE];
} ATTRIBUTE_PACKED atsi_mat_t;
#define ATSI_MAT_SIZE 672U

typedef struct {
  uint8_t  srp_index;         /* 1-based, top bit set marks the primary entry and clear the secondary */
  uint8_t  srp_flags;         /* varies per entry, purpose not established */
  uint8_t  audio_encode_mode; /* either 0x00, 0x01 or 0x05, meaning not confirmed */
  uint8_t  reserved;          /* always 0x00 */
  uint32_t offset_record_table; /* byte offset of this title's record from the start of the title table */
} ATTRIBUTE_PACKED atsi_title_index_t;
#define ATSI_TITLE_INDEX_SIZE 8U

/* one per audio program in the title (the ATS_PGI) */
typedef struct {
  uint8_t  prog_alloc_flags;      /* b7 the relation to the previous program, set
                                     on the first; b6 an STC discontinuity;
                                     b5-b3 which audio attribute of the ATSI_MAT
                                     applies (ATRN); b2-b0 the bit shift of
                                     channel group 2 */
  uint8_t  prog_downmix_flags;    /* b5 down mix prohibited; b4 down mix
                                     coefficients not valid; b3-b0 which down
                                     mix coefficient table applies (DM_COEFTN) */
  uint16_t rti_flags;             /* real time information flags, zero without RTI */
  uint8_t  track_number_in_title; /* the cell this program starts on, 1-based */
  uint8_t  unknown_3;             /* always 0x00 */
  uint32_t first_pts_of_track;    /* start PTS of the program's first cell, not always a title-relative timeline */
  uint32_t length_pts_of_track;   /* length of the program in PTS, the programs sum to length_pts */
  uint32_t pause_pts_of_track;    /* pause after the program, MPEG PTS; only 0 seen */
  uint8_t  cmi;                   /* copyright management information; only 0 seen */
  uint8_t  reserved_2;
} ATTRIBUTE_PACKED atsi_track_timestamp_t;
#define ATSI_TRACK_TIMESTAMP_SIZE 20U

/* this will come after all of the track timstamps, a set of 12byte sector pointer records. One for each track*/
typedef struct {
  uint8_t  cell_index;   /* 1-based, the first cell of a program is 0x01 */
  uint8_t  cell_type;    /* 0x00 for an audio cell, other types not seen */
  uint16_t reserved;     /* always 0x0000 */
  uint32_t start_sector; /* first sector, relative to the first AOB file */
  uint32_t end_sector;   /* last sector of the cell, relative to the first AOB file */
} ATTRIBUTE_PACKED atsi_track_pointer_t;
#define ATSI_TRACK_POINTER_SIZE 12U

/**
 * One entry per audio program. start_value and end_value give the first and
 * last byte of this program's records in the dlist table that follows. The
 * first entry starts right after this table, so its start_value equals the
 * table's byte length, 6 per program.
 */
typedef struct {
  uint8_t  asvu_n;      /* 1-based ASVU number holding this program's stills, several programs may share one */
  struct ATTRIBUTE_PACKED {
    uint8_t order   : 2; /* 0 sequential, 1 random, 3 shuffle; only 0 seen */
    uint8_t timing  : 2; /* 0 timed slideshow, 1 user browsable */
    uint8_t zero    : 4;
  } dmod;
  uint16_t start_value; /* first byte of this entry */
  uint16_t end_value;   /* last byte of this entry */
} ATTRIBUTE_PACKED ats_pg_asv_pbi_srp_t;
#define ATS_PG_ASV_PBI_SRP_SIZE 6U

/**
 * One still picture. These records follow the entry table and run up to the
 * largest end_value in that table. asv_number starts again at 1 for each still
 * video unit. display_timing holds the picture's MPEG time in slideshow mode
 * and is 0 in browsable mode. In random and shuffle order asv_number is
 * reserved, and in browsable mode track_nr is.
 */
typedef struct {
  uint8_t  asv_number;     /* the still's number within its unit, restarting at 1 each unit */
  uint8_t  reserved;       /* always 0x00 */
  uint8_t  fosl_btnn;      /* number of the button selected first on a menu still */
  uint8_t  track_nr;       /* on a shared slideshow, which track the still belongs to, unused when each track has its own stills */
  uint32_t display_timing; /* when the still is shown, an MPEG PTS, 0 in browsable mode */
  struct ATTRIBUTE_PACKED {
    uint8_t period : 4;    /* transition length */
    uint8_t mode   : 4;    /* transition effect */
  } start_effect, end_effect;
} ATTRIBUTE_PACKED asv_dlist_t;
#define ASV_DLIST_SIZE 10U

typedef struct {
  /* the address field positions below hold for the common layout, some discs
   * arrange them differently */
  uint16_t header_flags;                /* only 0x0000 seen */
  uint8_t  nr_tracks;                   /* number of audio programs in this title */
  uint8_t  nr_pointer_records;          /* number of cell records */
  uint32_t length_pts;                  /* total play time of the title, MPEG PTS */
  uint16_t reserved_1;                  /* always 0x0000 */
  uint16_t ats_pgit_sa;                 /* always 0x0010, the low 12 bits look like a byte offset but this is untested */
  uint16_t start_sector_pointers_table; /* byte offset of the cell info table, from title record start */
  uint16_t ats_asv_pbit_sa;             /* byte offset of the still video table, 0x0000 when the title has no stills */
  atsi_track_timestamp_t *atsi_track_timestamp_rows; /*length determined by nr_tracks*/
  atsi_track_pointer_t   *atsi_track_pointer_rows;

  /* these entries only exist when theres an ASVS */
  ats_pg_asv_pbi_srp_t   *ats_pg_asv_pbi_srp; /* one per audio program */
  asv_dlist_t                *dlist;               /* runs up to the largest end_value in the entries above */
} ATTRIBUTE_PACKED atsi_title_record_t;
#define ATSI_TITLE_ROW_TABLE_SIZE 16U

typedef struct {
  uint16_t               nr_titles;
  uint16_t               zero_1;
  uint32_t               last_byte_address;
  atsi_title_index_t     *atsi_index_rows;   /* length determined by nr_titles*/
  atsi_title_record_t    *atsi_title_row_tables;   /* length determined by nr_titles*/
} ATTRIBUTE_PACKED atsi_title_table_t;
#define ATSI_TITLE_TABLE_SIZE 8U

/**
 * ASVS
 *
 * Structures relating to the Audio Still Video Set (ASVS).
 *
 * An ASVU groups the still pictures (ASVs) that are loaded before the
 * matching audio program plays. Each ASV is one still I-picture with
 * optional sub-picture streams.
 */

/* general information for one Audio Still Video Unit */
typedef struct {
  uint8_t  asv_ns;         /* number of still pictures in this unit */
  uint8_t  asvu_atrn;      /* which asvu_atr slot describes this unit's stills */
  uint16_t first_abs_asvn; /* number of this unit's first still picture, 1-based */
  uint32_t asvu_sa;        /* start sector of the unit, counted from the first
                              sector of the ASVOBS (AUDIO_SV.VOB) */
} ATTRIBUTE_PACKED asvu_gi_t;
#define ASVU_GI_SIZE 8U
#define ASVU_GI_MAX_SIZE 99U

/**
 * Search pointer to one still picture. The still starts at sector
 * asvu_sa + offset of its unit. The top two bits are not part of the
 * address.
 */
typedef struct {
  uint16_t offset : 14;
  uint16_t cci    : 2;
} ATTRIBUTE_PACKED asv_srpt_t;
#define ASV_SRPT_SIZE 2U

/* the ASV_SRPT must fit in the two sector ASVS information */
#define ASV_MAX_NR ((2 * 2048 - ASVS_MAT_SIZE) / ASV_SRPT_SIZE)

/**
 * Audio Still Video Set Management Table. Exclusive to DVD-Audio discs
 */
typedef struct {
  char          asvs_identifier[12];   /* DVDAUDIOASVS */
  uint16_t      asvs_nr_of_asvus;      /* number of still video units in asvu_gi */
  uint16_t      specification_version; /* 0x0012, a few discs write zero */
  uint32_t      asvobs_sa;             /* start sector of the still picture area,
                                          counted from the ASVS start; always 2,
                                          right after this two sector table */
  uint32_t      asv_ea;                /* last sector of the last still picture,
                                          counted from the first ASVOBS sector */
  uint16_t      asvu_atr[4];           /* still picture attributes, selected per
                                          unit by asvu_atrn: b15-b14 compression
                                          (1 MPEG-2), b13-b12 TV system (0
                                          525/60, 1 625/50), b11-b10 aspect (0
                                          4:3, 3 16:9), b9-b8 display mode,
                                          b5-b3 source resolution; the low bits
                                          hold the sub-picture setup: 0x00 no
                                          streams, 0x03 one, 0x05 two */
  uint32_t      sp_plt[16];            /* sub-picture palette, 16 colours,
                                          0x00YYCrCb, default 0x00108080 */
  asvu_gi_t     asvu_gi[ASVU_GI_MAX_SIZE]; /* size determined by asvs_nr_of_asvus */
  asv_srpt_t   *asv_srpt;              /* one per still picture, the sum of
                                          asv_ns over asvu_gi */
} ATTRIBUTE_PACKED asvs_mat_t;
#define ASVS_MAT_SIZE 888U

/**
 * PartOfTitle Unit Information.
 */
typedef struct {
  uint16_t pgcn;
  uint16_t pgn;
} ATTRIBUTE_PACKED ptt_info_t;
#define PTT_INFO_SIZE  4U

/**
 * PartOfTitle Information.
 */
typedef struct {
  uint16_t nr_of_ptts;
  ptt_info_t *ptt;
} ATTRIBUTE_PACKED ttu_t;
#define TTU_SIZE 2U

/**
 * PartOfTitle Search Pointer Table.
 */
typedef struct {
  uint16_t nr_of_srpts;
  uint16_t zero_1;
  uint32_t last_byte;
  ttu_t  *title;
  uint32_t *ttu_offset; /* offset table for each ttu */
} ATTRIBUTE_PACKED vts_ptt_srpt_t;
#define VTS_PTT_SRPT_SIZE 8U


/**
 * Time Map Entry.
 */
/* Should this be bit field at all or just the uint32_t? */
typedef uint32_t map_ent_t;

/**
 * Time Map.
 */
typedef struct {
  uint8_t  tmu;   /* Time unit, in seconds */
  uint8_t  zero_1;
  uint16_t nr_of_entries;
  map_ent_t *map_ent;
} ATTRIBUTE_PACKED vts_tmap_t;
#define VTS_TMAP_SIZE 4U

/**
 * Time Map Table.
 */
typedef struct {
  uint16_t nr_of_tmaps;
  uint16_t zero_1;
  uint32_t last_byte;
  vts_tmap_t *tmap;
  uint32_t *tmap_offset; /* offset table for each tmap */
} ATTRIBUTE_PACKED vts_tmapt_t;
#define VTS_TMAPT_SIZE 8U

/**
 * DVD-VR Structures
 *
 * Structures in the DVD-VR format (DVD-R, DVD-RW, DVD+RW, DVD-RAM)
 * For DVDs produced with a DVD-VR recorder, which can be a cam-corder or something that resembles a dvd-player
 */

typedef struct {
    uint8_t supported;   /* Encrypted Title Key Status */
    uint8_t title_key[8];/* This needs to be decrypted using media key */
} ATTRIBUTE_PACKED cprm_info_t;
#define CPRM_INFO_SIZE 9U

typedef struct {
  /* Suffix numbers are decimal offsets */
  /* 0 */
  char     id[12];
  uint32_t vmg_ea;         /* end address */
  uint8_t  zero_16[12];
  uint32_t vmgi_ea;        /* includes playlist info after this structure */
  uint16_t version;        /* specification version */
  /* 34 */                 /* Different from DVD-Video from here */
  uint8_t  zero_34[30];
  uint8_t  tm_zone;        /* time zone */
  uint16_t still_tm;       /* still time for still pictures */
  uint8_t  txt_encoding;   /* as per VideoTextDataUsage.pdf */
  uint8_t  data_68[30];
  /* 98 */
  char     disc_info1[64]; /* format name, or copy of disc_info2. */
  char     disc_info2[64]; /* format name, time or user label.. */
  uint8_t  zero_226[30];
  /* 256 */
  uint32_t org_pgci_sa;    /* original program chain start address */
  uint32_t org_pgci_ea;    /* end address for the org_pgci */
  uint8_t  unknown_264[7];
  cprm_info_t cprm_info;
  uint8_t  zero_280[24];
  /* 304 */
  uint32_t ud_pgcit_sa;     /* user defined program chain information table */
  uint32_t info_308_sa;    /* ? start address */
  uint32_t info_312_sa;
  uint32_t info_316_sa;    /* ? start address */
  uint8_t  zero_320[32];
  uint32_t txtdt_mg_sa;    /* extra attributes for programs (chan id etc.) */
  uint32_t mnfit_sa;       /* manufacturer information table */
  uint8_t  zero_360[152];
} ATTRIBUTE_PACKED rtav_vmgi_t;
#define RTAV_VMGI_SIZE 512U

typedef struct {
  uint8_t audio_attr[3];
} ATTRIBUTE_PACKED audio_attr_vr_t;
#define AUDIO_ATTR_VR_SIZE 3U

typedef struct {
  uint8_t pgtm[5];
} ATTRIBUTE_PACKED pgtm_t;
#define PGTM_SIZE 5U

/* used throughout the program for timestamps */
typedef struct {
  uint32_t ptm;
  uint16_t ptm_extra; /* extra to DSI pkts */
} ATTRIBUTE_PACKED ptm_t;
#define PTM_SIZE 6U

typedef struct {
  uint8_t  data[12];
} ATTRIBUTE_PACKED adj_vob_t;
#define ADJ_VOB_SIZE 12U

typedef struct {
  uint16_t  vobu_entn; /* entry vobu */
  uint8_t   tm_diff;   /* time difference */
  uint32_t  vobu_adr;  /* time difference */
} ATTRIBUTE_PACKED time_info_t;
#define TIME_INFO_SIZE 7U

typedef struct {
  uint8_t  data1;
  /* only 14 bits are used for size,
   * but use full 16 for easy access.  */
  uint16_t vobu_size;
} ATTRIBUTE_PACKED vobu_info_t;
#define VOBU_INFO_SIZE 3U

typedef struct {
  uint16_t vob_attr;
  pgtm_t   vob_timestamp;
  uint8_t  data1;
  uint8_t  vob_format_id;
  ptm_t    vob_v_s_ptm;
  ptm_t    vob_v_e_ptm;
} ATTRIBUTE_PACKED vvob_t; /* Virtual VOB */
#define VVOB_SIZE 21U

typedef struct {
  uint16_t nr_of_time_info;
  uint16_t nr_of_vobu_info;
  uint16_t time_offset;
  uint32_t vob_offset;

  time_info_t* time_infos;
  vobu_info_t* vobu_infos;
} ATTRIBUTE_PACKED vobu_map_t;
#define VOBU_MAP_SIZE 10U

/* This struct can not be packed, since adj_info may be skipped */
typedef struct {
  vvob_t      header;       /* The 21-byte header */

  /* Always allocated. Contains valid data ONLY if (header.vob_attr & 0x80) */
  /* optional */
  adj_vob_t   adj_info;     /* The Optional ADJ block */
  /* optional */

  vobu_map_t  map;          /* The Time Map */
} pgi_t;

typedef struct {
  uint16_t video_attr;
  uint8_t  nr_of_audio_streams;
  uint8_t  data1;
  audio_attr_vr_t  audio_attr0;
  audio_attr_vr_t  audio_attr1;
  uint8_t  data2[50];
} ATTRIBUTE_PACKED vob_format_t;
#define VOB_FORMAT_SIZE 60U

/* codec settings, done automatically by demuxer */
typedef struct {
  uint16_t zero1;
  uint8_t  nr_of_pgi;
  uint8_t  nr_of_vob_formats;
  uint32_t len_org_pgci; /* length of PGCI + PGC_GI */

  vob_format_t* vob_formats;
} ATTRIBUTE_PACKED pgci_t; /* Program Info Table */
#define PGCI_SIZE 8U

/* this defines individual video recordings (clips, programs) */
typedef struct {
  uint16_t  nr_of_programs;
  uint32_t* program_offsets; /* vobu start addresses */
  pgi_t*    pgi;
} ATTRIBUTE_PACKED pgc_gi_t; /* program locations table */
#define PGC_GI_SIZE 2U

typedef struct  {
  uint8_t  data1;
  uint8_t  data2;
  uint16_t nr_of_programs;  /* Num programs in program set */
  char     label[64];       /* ASCII. Might not be NUL terminated */
  char     title[64];       /* Could be same as label, NUL, or another charset */
  uint16_t prog_set_id;     /* On LG V1.1 discs this is program set ID */
  uint16_t first_prog_id;   /* ID of first program in this program set */
  char     data3[6];
} ATTRIBUTE_PACKED ud_pgci_t;
#define UD_PGCI_SIZE 142U

typedef struct  {
  uint8_t    ep_ty; /* type */
  ptm_t      ep_ptm; /* entry point time pts */
} ATTRIBUTE_PACKED m_c_epi_t;
#define M_C_EPI_SIZE 7U

typedef struct  {
  uint8_t    type;
  uint8_t    reserved;
  union {
    struct {
      uint8_t group_id;
      uint8_t start_image;
    } still_id;
    uint16_t m_vobi_srpn;
  };
  uint8_t    end_image; /* will be zero if type is movie cell */
  uint8_t    c_epi_n; /* used to allocate m_c_epi */
  ptm_t      c_v_s_ptm;
  ptm_t      c_v_e_ptm;
  m_c_epi_t* m_c_epi; /*  movie cell entry points. seekable chapters */
} ATTRIBUTE_PACKED m_c_gi_t;
#define M_C_GI_SIZE 18U

#define MOVIE_CELL_TYPE 0
#define STILL_CELL_TYPE 2

/* this is like a playlist menu, it defines titles (groups of clips) */
typedef struct  {
  uint8_t    data1;
  uint8_t    nr_of_pgci;
  uint16_t   total_nr_of_programs;  /* Num programs on disc */
  ud_pgci_t* ud_pgci_items;
  uint32_t*  ci_offsets;
  m_c_gi_t*  m_c_gi;
} ATTRIBUTE_PACKED ud_pgcit_t; /* global info for Program Set*/
#define UD_PGCIT_SIZE 4U

#define CI_OFFSET_SIZE 4U

#if PRAGMA_PACK
#pragma pack(pop)
#endif


/**
 * The following structure defines an IFO file.  The structure is divided into
 * two parts, the VMGI, or Video Manager Information, which is read from the
 * VIDEO_TS.[IFO,BUP] file, and the VTSI, or Video Title Set Information, which
 * is read in from the VTS_XX_0.[IFO,BUP] files.
 */

typedef enum{
    IFO_UNKNOWN,
    IFO_VIDEO,
    IFO_AUDIO,
    IFO_VIDEO_RECORDING
} ifo_format_t;

/* format type will be used in order to reduce ammount of code refactoring, hopefully add some 
 * detection mechanism later as well.
 * */
typedef struct {
  
  
  union{
    struct{
      /* VMGI */
      vmgi_mat_t     *vmgi_mat;
      tt_srpt_t      *tt_srpt;
      pgc_t          *first_play_pgc;
      ptl_mait_t     *ptl_mait;
      vts_atrt_t     *vts_atrt;
      txtdt_mgi_t    *txtdt_mgi;

      /* Common */
      pgci_ut_t      *pgci_ut;
      c_adt_t        *menu_c_adt;
      vobu_admap_t   *menu_vobu_admap;

      /* VTSI */
      vtsi_mat_t     *vtsi_mat;
      vts_ptt_srpt_t *vts_ptt_srpt;
      pgcit_t        *vts_pgcit;
      vts_tmapt_t    *vts_tmapt;
      c_adt_t        *vts_c_adt;
      vobu_admap_t   *vts_vobu_admap;
    };

    struct{
      /* SAMG */
      samg_mat_t             *samg_mat;

      /* AMGI */
      amgi_mat_t             *amgi_mat;
      tracks_info_table_t    *info_table_first_sector;
      tracks_info_table_t    *info_table_second_sector;

      /* ATSI */
      atsi_mat_t             *atsi_mat;
      atsi_title_table_t     *atsi_title_table;

      /* ASVS */
      asvs_mat_t             *asvs_mat;

      pgci_ut_t              *amgm_pgci_ut;
      txtdt_mgi_t            *amg_txtdt_mgi;
    };

    struct{
      /* VMGI_MAT for DVD-VR */
      rtav_vmgi_t            *rtav_vmgi;

      /* these two together make up the org_pgci, describe the VRO file's contents */
      pgci_t                 *pgci;      /* Program cell information table */
      pgc_gi_t               *pgc_gi;    /* Program cell general information table */

      /* the user defined pgci */
      ud_pgcit_t             *ud_pgcit;  /* titles, labels for recordings */

    };

  };

  ifo_format_t ifo_format;

} ifo_handle_t;

#endif /* LIBDVDREAD_IFO_TYPES_H */
