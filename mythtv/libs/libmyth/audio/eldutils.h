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
 *  this program; if not, write to the Free Software Foundation, Inc., 51
 *  Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef ELDUTILS_H
#define ELDUTILS_H

#include <cstdint>
#include <QString>
#include "libmyth/mythexp.h"

static constexpr uint8_t ELD_FIXED_BYTES    { 20 };
static constexpr size_t  ELD_MAX_SAD        { 16 };

static constexpr uint8_t PRINT_RATES_ADVISED_BUFSIZE              { 80 };
static constexpr uint8_t PRINT_BITS_ADVISED_BUFSIZE               { 16 };
static constexpr uint8_t PRINT_CHANNEL_ALLOCATION_ADVISED_BUFSIZE { 80 };

class MPUBLIC eld
{
  public:
    eld(const char *buf, int size);
    eld(const eld& rhs) = default;
    eld();
    ~eld()= default;
    eld& operator=(const eld& /*rhs*/);
    void show();
    QString eld_version_name() const;
    QString edid_version_name() const;
    QString info_desc() const;
    QString channel_allocation_desc() const;
    QString product_name() const;
    QString connection_name() const;
    bool isValid() const;
    int maxLPCMChannels();
    int maxChannels();
    QString codecs_desc() const;
    
    enum cea_audio_coding_types : std::uint8_t {
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
    static QString print_pcm_rates(int pcm);
    static QString print_pcm_bits(int pcm);

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
        bool     eld_valid       { false };
        int      eld_size        { 0 };
        int      baseline_len    { 0 };
        int      eld_ver         { 0 };
        int      cea_edid_ver    { 0 };
        QString  monitor_name;
        int      manufacture_id  { 0 };
        int      product_id      { 0 };
        uint64_t port_id         { 0 };
        uint64_t formats         { 0 };
        int      support_hdcp    { 0 };
        int      support_ai      { 0 };
        int      conn_type       { 0 };
        int      aud_synch_delay { 0 };
        int      spk_alloc       { 0 };
        int      sad_count       { 0 };
        // NOLINTNEXTLINE(modernize-avoid-c-arrays)
        struct cea_sad sad[ELD_MAX_SAD] {};
        eld_data() = default;
    };
    eld_data m_e;
};

#endif /* ELDUTILSL_H */
