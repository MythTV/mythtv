/* -*- Mode: c++ -*-
 *
 * Copyright (C) Daniel Kristjansson 2008
 * Copyright (C) Jean-Yves Avenard 2010
 *
 * Licensed under the GPL v2 or a later version at your choosing.
 */

#ifndef _AUDIO_SETTINGS_H_
#define _AUDIO_SETTINGS_H_

#include <QString>

#include "mythexp.h"
#include "audiooutputsettings.h"

typedef enum {
    AUDIOOUTPUT_UNKNOWN,
    AUDIOOUTPUT_VIDEO,
    AUDIOOUTPUT_MUSIC,
    AUDIOOUTPUT_TELEPHONY,
} AudioOutputSource;

class MPUBLIC AudioSettings
{
  public:
    AudioSettings() = default;
    AudioSettings(const AudioSettings &other);
    AudioSettings(
        QString                 main_device,
        QString                 passthru_device,
        AudioFormat             format,
        int                     channels,
        AVCodecID               codec,
        int                     samplerate,
        AudioOutputSource       source,
        bool                    set_initial_vol,
        bool                    use_passthru,
        int                     upmixer_startup = 0,
        AudioOutputSettings     *custom = nullptr);

    AudioSettings(AudioFormat   format,
                  int           channels,
                  AVCodecID     codec,
                  int           samplerate,
                  bool          use_passthru,
                  int           upmixer_startup = 0,
                  int           codec_profile = 0);

    AudioSettings(const QString    &main_device,
                  const QString    &passthru_device = QString())
        : m_main_device(main_device),
          m_passthru_device(passthru_device) {}

    ~AudioSettings();
    void FixPassThrough(void);
    void TrimDeviceType(void);

    QString GetMainDevice(void) const
        { return m_main_device; }
    QString GetPassthruDevice(void) const
        { return m_passthru_device; }

  public:
    QString             m_main_device;
    QString             m_passthru_device;
    AudioFormat         m_format          {FORMAT_NONE};
    int                 m_channels        {-1};
    AVCodecID           m_codec           {AV_CODEC_ID_NONE};
    int                 m_codec_profile   {-1};
    int                 m_samplerate      {-1};
    bool                m_set_initial_vol {false};
    bool                m_use_passthru    {false};
    AudioOutputSource   m_source          {AUDIOOUTPUT_UNKNOWN};
    int                 m_upmixer         {0};
    /**
     * If set to false, AudioOutput instance will not try to initially open
     * the audio device
     */
    bool                m_init            {false};
    /**
     * custom contains a pointer to the audio device capabilities
     * if defined, AudioOutput will not try to automatically discover them.
     * This is used by the AudioTest setting screen where the user can
     * manually override and immediately use them.
     */
    AudioOutputSettings *m_custom         {nullptr};
};

#endif // _AUDIO_SETTINGS_H_
