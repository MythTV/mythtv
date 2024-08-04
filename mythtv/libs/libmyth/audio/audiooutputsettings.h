/* -*- Mode: c++ -*-
 *
 * Copyright (C) foobum@gmail.com 2010
 * Copyright (C) Jean-Yves Avenard 2010
 *
 * Licensed under the GPL v2 or a later version at your choosing.
 */

#ifndef AUDIO_OUTPUT_SETTINGS_H
#define AUDIO_OUTPUT_SETTINGS_H

#include <array>
#include <vector>

#include "libmyth/mythexp.h"

#include <QString>

extern "C" {
#include "libavcodec/avcodec.h"  // to get codec id
}
#include "eldutils.h"

enum AudioFormat : std::uint8_t {
    FORMAT_NONE = 0,
    FORMAT_U8,
    FORMAT_S16,
    FORMAT_S24LSB,
    FORMAT_S24,
    FORMAT_S32,
    FORMAT_FLT
};

enum DigitalFeature : std::uint8_t {
    FEATURE_NONE   = 0,
    FEATURE_AC3    = 1 << 0,
    FEATURE_DTS    = 1 << 1,
    FEATURE_LPCM   = 1 << 2,
    FEATURE_EAC3   = 1 << 3,
    FEATURE_TRUEHD = 1 << 4,
    FEATURE_DTSHD  = 1 << 5,
    FEATURE_AAC    = 1 << 6,
};

using rate_array = std::array<const int,12>;
using format_array = std::array<const AudioFormat,6>;

class MPUBLIC AudioOutputSettings
{
    public:
        explicit AudioOutputSettings(bool invalid = false);
        AudioOutputSettings(const AudioOutputSettings&) = default;
        ~AudioOutputSettings();
        AudioOutputSettings& operator=(const AudioOutputSettings& /*rhs*/);
        AudioOutputSettings *GetCleaned(bool newcopy = false);
        AudioOutputSettings *GetUsers(bool newcopy = false);

        int  GetNextRate();
        void AddSupportedRate(int rate);
        bool IsSupportedRate(int rate);
        int  NearestSupportedRate(int rate);
        int  BestSupportedRate();
        AudioFormat GetNextFormat();
        void AddSupportedFormat(AudioFormat format);
        bool IsSupportedFormat(AudioFormat format);
        AudioFormat BestSupportedFormat();
        static int FormatToBits(AudioFormat format);
        static const char* FormatToString(AudioFormat format);
        static int SampleSize(AudioFormat format);
        static AudioFormat AVSampleFormatToFormat(AVSampleFormat format, int bits = 0);
        static AVSampleFormat FormatToAVSampleFormat(AudioFormat format);
        void AddSupportedChannels(int channels);
        bool IsSupportedChannels(int channels);
        int  BestSupportedChannels();

        void setPassthrough(int val)    { m_passthrough = val; };
        int  canPassthrough() const     { return m_passthrough; };
            /**
             * return DigitalFeature mask.
             * possible values are:
             * - FEATURE_AC3
             * - FEATURE_DTS
             * - FEATURE_LPCM
             * - FEATURE_EAC3
             * - FEATURE_TRUEHD
             * - FEATURE_DTSHD
             */
        bool canFeature(DigitalFeature arg) const
        { return (m_features & arg) != 0U; };
        bool canFeature(unsigned int arg) const
        { return (m_features & arg) != 0U; };

            /**
             * return true if device can or may support AC3
             * (deprecated, see canFeature())
             */
        bool canAC3() const             { return canFeature(FEATURE_AC3); };
            /**
             * return true if device can or may support DTS
             * (deprecated, see canFeature())
             */
        bool canDTS() const             { return canFeature(FEATURE_DTS); };
            /**
             * return true if device supports multichannels PCM
             * (deprecated, see canFeature())
             */
        bool canLPCM() const            { return canFeature(FEATURE_LPCM); };
            /**
             * return true if class instance is marked invalid.
             * if true, you can not assume any of the other method returned
             * values are valid
             */
        bool IsInvalid() const          { return m_invalid; };

            /**
             * set the provided digital feature
             * possible values are:
             * - FEATURE_AC3
             * - FEATURE_DTS
             * - FEATURE_LPCM
             * - FEATURE_EAC3
             * - FEATURE_TRUEHD
             * - FEATURE_DTSHD
             */
        void setFeature(DigitalFeature arg)     { m_features |= arg; };
        void setFeature(unsigned int arg)       { m_features |= arg; };

            /**
             * clear or set digital feature internal mask
             */ 
        void setFeature(bool val, DigitalFeature arg);
        void setFeature(bool val, int arg);

            /**
             * Force set the greatest number of channels supported by the audio
             * device
             */
        void SetBestSupportedChannels(int channels);

            /**
             * return the highest iec958 rate supported.
             * return 0 if no HD rate are supported
             */
        int  GetMaxHDRate() const;

            /**
             * Display in human readable form the digital features
             * supported by the output device
             */
        static QString FeaturesToString(DigitalFeature arg);
        QString FeaturesToString(void) const
        { return FeaturesToString((DigitalFeature)m_features); };

            /**
             * Setup samplerate and number of channels for passthrough
             */
        static QString GetPassthroughParams(int codec, int codec_profile,
                                         int &samplerate, int &channels,
                                         bool canDTSHDMA);

            // ELD related methods

            /**
             * get the ELD flag
             */
        bool hasELD() const;
        bool hasValidELD();
            /**
             * set ELD data
             */
        void setELD(QByteArray *ba);
            /**
             * retrieve ELD data
             */
        eld &getELD(void)             { return m_eld; };
            /**
             * Reports best supported channel number, restricted to ELD range
             */
        int  BestSupportedChannelsELD();
            /**
             * Reports best supported PCM channel number, restricted to ELD
             */
        int  BestSupportedPCMChannelsELD();

  private:
        void SortSupportedChannels();

        /** passthrough status
         * -1 : no
         * 0: unknown
         * 1: yes
         */
        int          m_passthrough {-1};

        unsigned int m_features    {FEATURE_NONE};

        bool         m_invalid     {false};
            /**
             * will be set to true if we were able to retrieve the device ELD
             * (EDID like Data). ELD contains information about the audio
             * processing capabilities of the device connected to the audio card
             * ELD is usually retrieved from EDID CEA-861-E extension.
             */
        bool         m_hasEld      {false};
        eld          m_eld;

        std::vector<int>          m_rates;
        std::vector<int>          m_channels;
        std::vector<AudioFormat>  m_formats;
        rate_array::iterator      m_srIt { };
        format_array::iterator    m_sfIt { };

        static const rate_array   kStdRates;
        static const format_array kStdFormats;
};

#endif // AUDIO_OUTPUT_SETTINGS_H
