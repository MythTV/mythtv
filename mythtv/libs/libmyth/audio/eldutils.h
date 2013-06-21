/*
 * Universal Interface for Intel High Definition Audio Codec
 *
 * Local helper functions
 *
 * Copyright (c) 2004 Takashi Iwai <tiwai@suse.de>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 *
 *  You should have received a copy of the GNU General Public License along with
 *  this program; if not, write to the Free Software Foundation, Inc., 59
 *  Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef __ELDUTILS_H
#define __ELDUTILS_H

#include <stdint.h>
#include <QString>
#include "mythexp.h"

#define ELD_FIXED_BYTES    20
#define ELD_MAX_SIZE       256
#define ELD_MAX_MNL        16
#define ELD_MAX_SAD        16

#define PRINT_RATES_ADVISED_BUFSIZE              80
#define PRINT_BITS_ADVISED_BUFSIZE               16
#define PRINT_CHANNEL_ALLOCATION_ADVISED_BUFSIZE 80

class MPUBLIC ELD
{
  public:
    ELD(const char *buf, int size);
    ELD();
    ~ELD();
    ELD& operator=(const ELD&);
    void show();
    QString eld_version_name();
    QString edid_version_name();
    QString info_desc();
    QString channel_allocation_desc();
    QString product_name();
    QString connection_name();
    bool isValid();
    int maxLPCMChannels();
    int maxChannels();
    QString codecs_desc();
    
    enum cea_audio_coding_types {
        TYPE_REF_STREAM_HEADER =  0,
        TYPE_LPCM              =  1,
        TYPE_AC3               =  2,
        TYPE_MPEG1             =  3,
        TYPE_MP3               =  4,
        TYPE_MPEG2             =  5,
        TYPE_AACLC             =  6,
        TYPE_DTS               =  7,
        TYPE_ATRAC             =  8,
        TYPE_SACD              =  9,
        TYPE_EAC3              = 10,
        TYPE_DTS_HD            = 11,
        TYPE_MLP               = 12,
        TYPE_DST               = 13,
        TYPE_WMAPRO            = 14,
        TYPE_REF_CXT           = 15,
            /* also include valid xtypes below */
        TYPE_HE_AAC            = 15,
        TYPE_HE_AAC2           = 16,
        TYPE_MPEG_SURROUND     = 17,
    };

  private:

        // private methods
    int update_eld(const char *buf, int size);
    void update_sad(int index, const char *buf);
    QString sad_desc(int index);
    QString print_pcm_rates(int pcm);
    QString print_pcm_bits(int pcm);

        /*
         * CEA Short Audio Descriptor data
         */
    struct cea_sad {
        int    channels;
        int    format;         /* (format == 0) indicates invalid SAD */
        int    rates;
        int    sample_bits;    /* for LPCM */
        int    max_bitrate;    /* for AC3...ATRAC */
        int    profile;        /* for WMAPRO */
    };

        /*
         * ELD: EDID Like Data
         */
    struct eld_data {
        bool     eld_valid;
        int      eld_size;
        int      baseline_len;
        int      eld_ver;
        int      cea_edid_ver;
        char     monitor_name[ELD_MAX_MNL + 1];
        int      manufacture_id;
        int      product_id;
        uint64_t port_id;
        uint64_t formats;
        int      support_hdcp;
        int      support_ai;
        int      conn_type;
        int      aud_synch_delay;
        int      spk_alloc;
        int      sad_count;
        struct cea_sad sad[ELD_MAX_SAD];
        eld_data() { memset(this, 0, sizeof(*this)); }
    };
    eld_data m_e;
};

#endif /* __ELDUTILSL_H */
