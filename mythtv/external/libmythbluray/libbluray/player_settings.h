/*
 * This file is part of libbluray
 * Copyright (C) 2014  VideoLAN
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
 */

enum {
    BLURAY_PLAYER_PROFILE_2_v2_0 = ((0x03 << 16) | (0x0200)),   /* Profile 2, version 2.0 (network access, BdLive) */
};

#endif /* BD_PLAYER_SETTINGS_H_ */
