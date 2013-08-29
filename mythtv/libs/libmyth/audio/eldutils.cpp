/*
 * eldutils.cpp (c) Jean-Yves Avenard <jyavenard@mythtv.org>
 * a utility class to decode EDID Like Data (ELD) byte stream
 * 
 * Based on ALSA hda_eld.c
 * Copyright(c) 2008 Intel Corporation.
 *
 * Authors:
 *         Wu Fengguang <wfg@linux.intel.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "mythconfig.h"
#include "eldutils.h"
#include <sys/types.h>
#include <inttypes.h>
#include "bswap.h"
#include "audiooutputbase.h"
#include <QString>

#define LOC QString("ELDUTILS: ")

#if HAVE_BIGENDIAN
#define LE_SHORT(v)      bswap_16(*((uint16_t *)v))
#define LE_INT(v)        bswap_32(*((uint32_t *)v))
#define LE_INT64(v)      bswap_64(*((uint64_t *)v))
#else
#define LE_SHORT(v)      (*((uint16_t *)v))
#define LE_INT(v)        (*((uint32_t *)v))
#define LE_INT64(v)      (*((uint64_t *)v))
#endif

#define SIZE_ARRAY(x) (sizeof(x) / sizeof(x[0]))

enum eld_versions
{
    ELD_VER_CEA_861D    = 2,
    ELD_VER_PARTIAL     = 31,
};

enum cea_edid_versions
{
    CEA_EDID_VER_NONE      = 0,
    CEA_EDID_VER_CEA861    = 1,
    CEA_EDID_VER_CEA861A   = 2,
    CEA_EDID_VER_CEA861BCD = 3,
    CEA_EDID_VER_RESERVED  = 4,
};

static const char *cea_speaker_allocation_names[] = {
    /*  0 */ "FL/FR",
    /*  1 */ "LFE",
    /*  2 */ "FC",
    /*  3 */ "RL/RR",
    /*  4 */ "RC",
    /*  5 */ "FLC/FRC",
    /*  6 */ "RLC/RRC",
    /*  7 */ "FLW/FRW",
    /*  8 */ "FLH/FRH",
    /*  9 */ "TC",
    /* 10 */ "FCH",
};

static const char *eld_connection_type_names[4] = {
    "HDMI",
    "DisplayPort",
    "2-reserved",
    "3-reserved"
};

enum cea_audio_coding_xtypes
{
    XTYPE_HE_REF_CT      = 0,
    XTYPE_HE_AAC         = 1,
    XTYPE_HE_AAC2        = 2,
    XTYPE_MPEG_SURROUND  = 3,
    XTYPE_FIRST_RESERVED = 4,
};

static const char *audiotype_names[] = {
    /*  0 */ "undefined",
    /*  1 */ "LPCM",
    /*  2 */ "AC3",
    /*  3 */ "MPEG1",
    /*  4 */ "MP3",
    /*  5 */ "MPEG2",
    /*  6 */ "AAC-LC",
    /*  7 */ "DTS",
    /*  8 */ "ATRAC",
    /*  9 */ "DSD (One Bit Audio)",
    /* 10 */ "E-AC3",
    /* 11 */ "DTS-HD",
    /* 12 */ "TrueHD",
    /* 13 */ "DST",
    /* 14 */ "WMAPro",
    /* 15 */ "HE-AAC",
    /* 16 */ "HE-AACv2",
    /* 17 */ "MPEG Surround",
};

/*
 * The following two lists are shared between
 *     - HDMI audio InfoFrame (source to sink)
 *     - CEA E-EDID Extension (sink to source)
 */

/*
 * SF2:SF1:SF0 index => sampling frequency
 */
#define SNDRV_PCM_RATE_5512             (1<<0)          /* 5512Hz */
#define SNDRV_PCM_RATE_8000             (1<<1)          /* 8000Hz */
#define SNDRV_PCM_RATE_11025            (1<<2)          /* 11025Hz */
#define SNDRV_PCM_RATE_16000            (1<<3)          /* 16000Hz */
#define SNDRV_PCM_RATE_22050            (1<<4)          /* 22050Hz */
#define SNDRV_PCM_RATE_32000            (1<<5)          /* 32000Hz */
#define SNDRV_PCM_RATE_44100            (1<<6)          /* 44100Hz */
#define SNDRV_PCM_RATE_48000            (1<<7)          /* 48000Hz */
#define SNDRV_PCM_RATE_64000            (1<<8)          /* 64000Hz */
#define SNDRV_PCM_RATE_88200            (1<<9)          /* 88200Hz */
#define SNDRV_PCM_RATE_96000            (1<<10)         /* 96000Hz */
#define SNDRV_PCM_RATE_176400           (1<<11)         /* 176400Hz */
#define SNDRV_PCM_RATE_192000           (1<<12)         /* 192000Hz */

static int cea_sampling_frequencies[8] = {
    0,                       /* 0: Refer to Stream Header */
    SNDRV_PCM_RATE_32000,    /* 1:  32000Hz */
    SNDRV_PCM_RATE_44100,    /* 2:  44100Hz */
    SNDRV_PCM_RATE_48000,    /* 3:  48000Hz */
    SNDRV_PCM_RATE_88200,    /* 4:  88200Hz */
    SNDRV_PCM_RATE_96000,    /* 5:  96000Hz */
    SNDRV_PCM_RATE_176400,   /* 6: 176400Hz */
    SNDRV_PCM_RATE_192000,   /* 7: 192000Hz */
};

#define GRAB_BITS(buf, byte, lowbit, bits)            \
(                                                    \
    (buf[byte] >> (lowbit)) & ((1 << (bits)) - 1)    \
)

ELD::ELD(const char *buf, int size)
{
    m_e.formats = 0LL;
    update_eld(buf, size);
}

ELD::ELD()
{
    m_e.formats = 0LL;
    m_e.eld_valid = false;
}

ELD::~ELD()
{
}

ELD& ELD::operator=(const ELD &rhs)
{
    if (this == &rhs)
        return *this;
    m_e = rhs.m_e;
    return *this;
}

void ELD::update_sad(int index,
                     const char *buf)
{
    int val;

    cea_sad *a = m_e.sad + index;

    val = GRAB_BITS(buf, 1, 0, 7);
    a->rates = 0;
    for (int i = 0; i < 7; i++)
        if ((val & (1 << i)) != 0)
            a->rates |= cea_sampling_frequencies[i + 1];

    a->channels = GRAB_BITS(buf, 0, 0, 3);
    a->channels++;

    a->sample_bits = 0;
    a->max_bitrate = 0;

    a->format = GRAB_BITS(buf, 0, 3, 4);
    m_e.formats |= 1 << a->format;
    switch (a->format)
    {
        case TYPE_REF_STREAM_HEADER:
            VBAUDIO("audio coding type 0 not expected");
            break;

        case TYPE_LPCM:
            a->sample_bits = GRAB_BITS(buf, 2, 0, 3);
            break;

        case TYPE_AC3:
        case TYPE_MPEG1:
        case TYPE_MP3:
        case TYPE_MPEG2:
        case TYPE_AACLC:
        case TYPE_DTS:
        case TYPE_ATRAC:
            a->max_bitrate = GRAB_BITS(buf, 2, 0, 8);
            a->max_bitrate *= 8000;
            break;

        case TYPE_SACD:
            break;

        case TYPE_EAC3:
            break;

        case TYPE_DTS_HD:
            break;

        case TYPE_MLP:
            break;

        case TYPE_DST:
            break;

        case TYPE_WMAPRO:
            a->profile = GRAB_BITS(buf, 2, 0, 3);
            break;

        case TYPE_REF_CXT:
            a->format = GRAB_BITS(buf, 2, 3, 5);
            if (a->format == XTYPE_HE_REF_CT ||
                a->format >= XTYPE_FIRST_RESERVED)
            {
                VBAUDIO(QString("audio coding xtype %1 not expected")
                        .arg(a->format));
                a->format = 0;
            }
            else
            {
                a->format += TYPE_HE_AAC - XTYPE_HE_AAC;
            }
            break;
    }
}

int ELD::update_eld(const char *buf, int size)
{
    int mnl;
    int i;

    m_e.eld_ver = GRAB_BITS(buf, 0, 3, 5);
    if (m_e.eld_ver != ELD_VER_CEA_861D &&
        m_e.eld_ver != ELD_VER_PARTIAL)
    {
        VBAUDIO(QString("Unknown ELD version %1").arg(m_e.eld_ver));
        goto out_fail;
    }

    m_e.eld_size = size;
    m_e.baseline_len    = GRAB_BITS(buf, 2, 0, 8);
    mnl                = GRAB_BITS(buf, 4, 0, 5);
    m_e.cea_edid_ver    = GRAB_BITS(buf, 4, 5, 3);

    m_e.support_hdcp    = GRAB_BITS(buf, 5, 0, 1);
    m_e.support_ai      = GRAB_BITS(buf, 5, 1, 1);
    m_e.conn_type       = GRAB_BITS(buf, 5, 2, 2);
    m_e.sad_count       = GRAB_BITS(buf, 5, 4, 4);

    m_e.aud_synch_delay = GRAB_BITS(buf, 6, 0, 8) * 2;
    m_e.spk_alloc       = GRAB_BITS(buf, 7, 0, 7);

    m_e.port_id         = LE_INT64(buf + 8);

    /* not specified, but the spec's tendency is little endian */
    m_e.manufacture_id  = LE_SHORT(buf + 16);
    m_e.product_id      = LE_SHORT(buf + 18);

    if (mnl > ELD_MAX_MNL)
    {
        VBAUDIO(QString("MNL is reserved value %1").arg(mnl));
        goto out_fail;
    }
    else if (ELD_FIXED_BYTES + mnl > size)
    {
        VBAUDIO(QString("out of range MNL %1").arg(mnl));
        goto out_fail;
    }
    else
    {
        strncpy(m_e.monitor_name, (char *)buf + ELD_FIXED_BYTES, mnl + 1);
        m_e.monitor_name[mnl] = '\0';
    }

    for (i = 0; i < m_e.sad_count; i++)
    {
        if (ELD_FIXED_BYTES + mnl + 3 * (i + 1) > size)
        {
            VBAUDIO(QString("out of range SAD %1").arg(i));
            goto out_fail;
        }
        update_sad(i, buf + ELD_FIXED_BYTES + mnl + 3 * i);
    }

    /*
     * Assume the highest speakers configuration
     */
    if (!m_e.spk_alloc)
        m_e.spk_alloc = 0xffff;

    m_e.eld_valid = true;
    return 0;

  out_fail:
    m_e.eld_valid = false;
    return -1;
}

/**
 * SNDRV_PCM_RATE_* and AC_PAR_PCM values don't match, print correct rates with
 * hdmi-specific routine.
 */
QString ELD::print_pcm_rates(int pcm)
{
    unsigned int rates[] = {
        5512, 8000, 11025, 16000, 22050, 32000, 44100, 48000, 88200,
        96000, 176400, 192000 };
    QString result = QString();

    for (unsigned int i = 0; i < SIZE_ARRAY(rates); i++)
    {
        if ((pcm & (1 << i)) != 0)
        {
            result += QString(" %1").arg(rates[i]);
        }
    }
    return result;
}

/**
 * print_pcm_bits - Print the supported PCM fmt bits to the string buffer
 * @pcm: PCM caps bits
 */
QString ELD::print_pcm_bits(int pcm)
{
    unsigned int bits[] = { 16, 20, 24 };
    unsigned int i;
    QString result = QString();

    for (i = 0; i < SIZE_ARRAY(bits); i++)
    {
        if ((pcm & (1 << i)) != 0)
        {
            result += QString(" %1").arg(bits[i]);
        }
    }
    return result;
}

QString ELD::sad_desc(int index)
{
    cea_sad *a = m_e.sad + index;
    if (!a->format)
        return "";

    QString buf  = print_pcm_rates(a->rates);
    QString buf2 = ", bits =";

    if (a->format == TYPE_LPCM)
        buf2 += print_pcm_bits(a->sample_bits);
    else if (a->max_bitrate)
        buf2 = QString(", max bitrate = %1").arg(a->max_bitrate);
    else
        buf2 = "";

    return QString("supports coding type %1:"
                   " channels = %2, rates =%3%4")
        .arg(audiotype_names[a->format])
        .arg(a->channels)
        .arg(buf)
        .arg(buf2);
}

QString ELD::channel_allocation_desc()
{
    QString result = QString();
    unsigned int i;

    for (i = 0; i < sizeof(cea_speaker_allocation_names) / sizeof(char *); i++)
    {
        if ((m_e.spk_alloc & (1 << i)) != 0)
        {
            result += QString(" %1").arg(cea_speaker_allocation_names[i]);
        }
    }
    return result;
}

QString ELD::eld_version_name()
{
    switch (m_e.eld_ver)
    {
        case 2:  return "CEA-861D or below";
        case 31: return "partial";
        default: return "reserved";
    }
}

QString ELD::edid_version_name()
{
    switch (m_e.cea_edid_ver)
    {
        case 0:  return "no CEA EDID Timing Extension block present";
        case 1:  return "CEA-861";
        case 2:  return "CEA-861-A";
        case 3:  return "CEA-861-B, C or D";
        default: return "reserved";
    }
}

QString ELD::info_desc()
{
    QString result = QString("manufacture_id\t\t0x%1\n")
        .arg(m_e.manufacture_id, 0, 16);
    result += QString("product_id\t\t0x%1\n").arg(m_e.product_id, 0, 16);
    result += QString("port_id\t\t\t0x%1\n").arg((long long)m_e.port_id);
    result += QString("support_hdcp\t\t%1\n").arg(m_e.support_hdcp);
    result += QString("support_ai\t\t%1\n").arg(m_e.support_ai);
    result += QString("audio_sync_delay\t%1\n").arg(m_e.aud_synch_delay);
    result += QString("sad_count\t\t%1\n").arg(m_e.sad_count);
    return result;
}

bool ELD::isValid()
{
    return m_e.eld_valid;
}

void ELD::show()
{
    int i;

    if (!isValid())
    {
        VBAUDIO("Invalid ELD");
        return;
    }
    VBAUDIO(QString("Detected monitor %1 at connection type %2")
            .arg(product_name().simplified())
            .arg(connection_name()));

    if (m_e.spk_alloc)
    {
        VBAUDIO(QString("available speakers:%1")
                .arg(channel_allocation_desc()));
    }
    VBAUDIO(QString("max LPCM channels = %1").arg(maxLPCMChannels()));
    VBAUDIO(QString("max channels = %1").arg(maxChannels()));
    VBAUDIO(QString("supported codecs = %1").arg(codecs_desc()));
    for (i = 0; i < m_e.sad_count; i++)
    {
        VBAUDIO(sad_desc(i));
    }
}

QString ELD::product_name()
{
    return QString(m_e.monitor_name);
}

QString ELD::connection_name()
{
    return eld_connection_type_names[m_e.conn_type];
}

int ELD::maxLPCMChannels()
{
    int channels = 2; // assume stereo at the minimum
    for (int i = 0; i < m_e.sad_count; i++)
    {
        struct cea_sad *a = m_e.sad + i;
        if (a->format == TYPE_LPCM)
        {
            if (a->channels > channels)
                channels = a->channels;
        }
    }
    return channels;
}

int ELD::maxChannels()
{
    int channels = 2; // assume stereo at the minimum
    for (int i = 0; i < m_e.sad_count; i++)
    {
        struct cea_sad *a = m_e.sad + i;
        if (a->channels > channels)
            channels = a->channels;
    }
    return channels;
}

QString ELD::codecs_desc()
{
    QString result = QString();
    bool found_one = false;
    for (unsigned int i = 0; i < SIZE_ARRAY(audiotype_names); i++)
    {
        if ((m_e.formats & (1 << i)) != 0)
        {
            if (found_one)
                result += ", ";
            result += audiotype_names[i];
            found_one = true;
        }
    }
    return result;
}
