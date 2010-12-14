/* -*- Mode: c++ -*-
 *
 * Copyright (C) foobum@gmail.com 2010
 *
 * Licensed under the GPL v2 or a later version at your choosing.
 */

#ifndef _AUDIO_OUTPUT_SETTINGS_H_
#define _AUDIO_OUTPUT_SETTINGS_H_

#include <vector>

#include "mythexp.h"

using namespace std;

typedef enum {
    FORMAT_NONE = 0,
    FORMAT_U8,
    FORMAT_S16,
    FORMAT_S24LSB,
    FORMAT_S24,
    FORMAT_S32,
    FORMAT_FLT
} AudioFormat;

static const int srs[] = { 8000,  11025, 16000, 22050, 32000,  44100,
                           48000, 64000, 88200, 96000, 176400, 192000 };

static const AudioFormat fmts[] = { FORMAT_U8,  FORMAT_S16, FORMAT_S24LSB,
                                    FORMAT_S24, FORMAT_S32, FORMAT_FLT };

class MPUBLIC AudioOutputSettings
{
    public:
        AudioOutputSettings(bool invalid = false);
        ~AudioOutputSettings();
        AudioOutputSettings& operator=(const AudioOutputSettings&);
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
        void AddSupportedChannels(int channels);
        bool IsSupportedChannels(int channels);
        int  BestSupportedChannels();

        void setPassthrough(int val)    { m_passthrough = val; };
        int  canPassthrough()           { return m_passthrough; };
            /**
             * return true if device can or may support AC3
             */
        bool canAC3()                   { return m_AC3; };
            /**
             * return true if device can or may support DTS
             */
        bool canDTS()                   { return m_DTS; };
            /**
             * return true if device supports multichannels PCM
             */
        bool canLPCM()                  { return m_LPCM; };
            /**
             * return true if device supports E-AC3 or DTS-HD passthrough
             */
        bool canHD()                    { return m_HD; };
            /**
             * return true if device supports TrueHD or DTS-HD passthrough
             */
        bool canHDLL()                  { return m_HDLL; };
            /**
             * return true if class instance is marked invalid.
             * if true, you can not assume any of the other method returned
             * values are valid
             */
        bool IsInvalid()                { return m_invalid; };
            /**
             * return true if device supports TrueHD or DTS-HD passthrough
             */
        void setAC3(bool b)             { m_AC3 = b; };
        void setDTS(bool b)             { m_DTS = b; };
        void setLPCM(bool b)            { m_LPCM = b; };
        void setHD(bool b)              { m_HD = b; };
        void setHDLL(bool b)            { m_HDLL = b; };
            /**
             * Force set the greatest number of channels supported by the audio
             * device
             */
        void SetBestSupportedChannels(int channels);

    private:
        void SortSupportedChannels();

        /* passthrough status
         * -1 : no
         * 0: unknown
         * 1: yes
         */
        int  m_passthrough;
        bool m_AC3;
        bool m_DTS;
        bool m_LPCM;
        bool m_HD;
        bool m_HDLL;

        bool m_invalid;

        vector<int> m_sr, m_rates, m_channels;
        vector<AudioFormat> m_sf, m_formats;
        vector<int>::iterator m_sr_it;
        vector<AudioFormat>::iterator m_sf_it;
};

#endif // _AUDIO_OUTPUT_SETTINGS_H_
