/*
 * This file is part of libbluray
 * Copyright (C) 2014-2017  VideoLAN
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

#ifndef BD_PLAYER_SETTINGS_H_
#define BD_PLAYER_SETTINGS_H_

/*
 * BLURAY_PLAYER_SETTING_AUDIO_CAP (PSR15)
 *
 * Player capability for audio (bitmask)
 */

enum {

    /* LPCM */

    /* 48/96kHz (mandatory) */
    BLURAY_ACAP_LPCM_48_96_STEREO_ONLY = 0x0001,  /* LPCM 48kHz and 96kHz stereo */
    BLURAY_ACAP_LPCM_48_96_SURROUND    = 0x0002,  /* LPCM 48kHz and 96kHz surround */

    /* 192kHz (optional) */
    BLURAY_ACAP_LPCM_192_STEREO_ONLY   = 0x0004,  /* LPCM 192kHz stereo */
    BLURAY_ACAP_LPCM_192_SURROUND      = 0x0008,  /* LPCM 192kHz surround */

    /* Dolby Digital Plus */

    /* independent substream (mandatory) */
    BLURAY_ACAP_DDPLUS_STEREO_ONLY     = 0x0010,
    BLURAY_ACAP_DDPLUS_SURROUND        = 0x0020,

    /* dependent substream (optional) */
    BLURAY_ACAP_DDPLUS_DEP_STEREO_ONLY = 0x0040,
    BLURAY_ACAP_DDPLUS_DEP_SURROUND    = 0x0080,

    /* DTS-HD */

    /* Core substream (mandatory) */
    BLURAY_ACAP_DTSHD_CORE_STEREO_ONLY = 0x0100,
    BLURAY_ACAP_DTSHD_CORE_SURROUND    = 0x0200,

    /* Extension substream (optional) */
    BLURAY_ACAP_DTSHD_EXT_STEREO_ONLY  = 0x0400,
    BLURAY_ACAP_DTSHD_EXT_SURROUND     = 0x0800,

    /* Dolby lossless (TrueHD) */

    /* Dolby Digital (mandatory) */
    BLURAY_ACAP_DD_STEREO_ONLY         = 0x1000,
    BLURAY_ACAP_DD_SURROUND            = 0x2000,

    /* MLP (optional) */
    BLURAY_ACAP_MLP_STEREO_ONLY        = 0x4000,
    BLURAY_ACAP_MLP_SURROUND           = 0x8000,
};


/*
 * BLURAY_PLAYER_SETTING_REGION_CODE (PSR20)
 *
 * Player region code (integer)
 *
 * Region A: the Americas, East and Southeast Asia, U.S. territories, and Bermuda.
 * Region B: Africa, Europe, Oceania, the Middle East, the Kingdom of the Netherlands,
 *           British overseas territories, French territories, and Greenland.
 * Region C: Central and South Asia, Mongolia, Russia, and the People's Republic of China.
 *
 */

enum {
    BLURAY_REGION_A = 1,
    BLURAY_REGION_B = 2,
    BLURAY_REGION_C = 4,
};


/*
 * BLURAY_PLAYER_SETTING_OUTPUT_PREFER (PSR21)
 *
 * Output mode preference (integer)
 */

enum {
    BLURAY_OUTPUT_PREFER_2D = 0,
    BLURAY_OUTPUT_PREFER_3D = 1,
};


/*
 * BLURAY_PLAYER_SETTING_DISPLAY_CAP (PSR23)
 *
 * Display capability (bit mask) and display size
 */

#define BLURAY_DCAP_1080p_720p_3D           0x01  /* capable of 1920x1080 23.976Hz and 1280x720 59.94Hz 3D */
#define BLURAY_DCAP_720p_50Hz_3D            0x02  /* capable of 1280x720 50Hz 3D */
#define BLURAY_DCAP_NO_3D_CLASSES_REQUIRED  0x04  /* 3D glasses are not required */
#define BLURAY_DCAP_INTERLACED_3D           0x08  /* */

/* horizintal display size in centimeters */
#define BLURAY_DCAP_DISPLAY_SIZE_UNDEFINED  0
#define BLURAY_DCAP_DISPLAY_SIZE(cm)        (((cm) > 0xfff ? 0xfff : (cm)) << 8)


/*
 * BLURAY_PLAYER_SETTING_VIDEO_CAP (PSR29)
 *
 * Player capability for video (bit mask)
 */

enum {
    BLURAY_VCAP_SECONDARY_HD = 0x01,  /* player can play secondary stream in HD */
    BLURAY_VCAP_25Hz_50Hz    = 0x02,  /* player can play 25Hz and 50Hz video */
};

/*
 * BLURAY_PLAYER_SETTING_PLAYER_PROFILE (PSR31)
 *
 * Player profile and version
 *
 * Profile 1, version 1.0: no local storage, no VFS, no internet
 * Profile 1, version 1.1: PiP, VFS, sec. audio, 256MB local storage, no internet
 * Profile 2, version 2.0: BdLive (internet), 1GB local storage
 */

enum {
    BLURAY_PLAYER_PROFILE_1_v1_0 = ((0x00 << 16) | (0x0100)),   /* Profile 1, version 1.0 (Initial Standard Profile) */
    BLURAY_PLAYER_PROFILE_1_v1_1 = ((0x01 << 16) | (0x0110)),   /* Profile 1, version 1.1 (secondary stream support) */
    BLURAY_PLAYER_PROFILE_2_v2_0 = ((0x03 << 16) | (0x0200)),   /* Profile 2, version 2.0 (network access, BdLive) */
    BLURAY_PLAYER_PROFILE_3_v2_0 = ((0x08 << 16) | (0x0200)),   /* Profile 3, version 2.0 (audio only player) */
    BLURAY_PLAYER_PROFILE_5_v2_4 = ((0x13 << 16) | (0x0240)),   /* Profile 5, version 2.4 (3D) */
    BLURAY_PLAYER_PROFILE_6_v3_0 = ((0x00 << 16) | (0x0300)),   /* Profile 6, version 3.0 (UHD) */
    BLURAY_PLAYER_PROFILE_6_v3_1 = ((0x00 << 16) | (0x0310)),   /* Profile 6, version 3.1 (UHD) */
};

#define BLURAY_PLAYER_PROFILE_3D_FLAG       0x100000
#define BLURAY_PLAYER_PROFILE_VERSION_MASK  0xffff

/*
 * BLURAY_PLAYER_SETTING_DECODE_PG
 *
 * Enable Presentation Graphics and Text Subtitle decoder
 */

enum {
    BLURAY_PG_TEXTST_DECODER_DISABLE  = 0,  /* disable both decoders */
    BLURAY_PG_TEXTST_DECODER_ENABLE   = 1,  /* enable both decoders */
};


/*
 * BLURAY_PLAYER_SETTING_PERSISTENT_STORAGE
 *
 * Enable / disable BD-J persistent storage.
 *
 * If persistent storage is disabled, BD-J Xlets can't access any data
 * stored during earlier playback sessions. Persistent data stored during
 * current playback session will be removed and can't be accessed later.
 *
 * This setting can't be changed after bd_play() has been called.
 */

enum {
    BLURAY_PERSISTENT_STORAGE_DISABLE = 0,  /* disable persistent storage between playback sessions */
    BLURAY_PERSISTENT_STORAGE_ENABLE  = 1,  /* enable persistent storage */
};

#endif /* BD_PLAYER_SETTINGS_H_ */
