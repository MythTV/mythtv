/* -*- Mode: c++ -*-
 *
 * Copyright (C) Daniel Kristjansson 2008
 * Copyright (C) Jean-Yves Avenard 2010
 *
 * Licensed under the GPL v2 or a later version at your choosing.
 */

#ifndef AUDIO_SETTINGS_H
#define AUDIO_SETTINGS_H

#include <utility>

// Qt headers
#include <QString>

// MythTV headers
#include "libmyth/mythexp.h"
#include "libmyth/audio/audiooutputsettings.h"

enum AudioOutputSource : std::uint8_t {
    AUDIOOUTPUT_UNKNOWN,
    AUDIOOUTPUT_VIDEO,
    AUDIOOUTPUT_MUSIC,
    AUDIOOUTPUT_TELEPHONY,
};

class MPUBLIC AudioSettings
{
  public:
    AudioSettings() = default;
    AudioSettings &operator=(const AudioSettings &) = delete;
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
        const AudioOutputSettings *custom = nullptr);

    AudioSettings(AudioFormat   format,
                  int           channels,
                  AVCodecID     codec,
                  int           samplerate,
                  bool          use_passthru,
                  int           upmixer_startup = 0,
                  int           codec_profile = 0);

    explicit AudioSettings(QString main_device,
                  QString passthru_device = QString())
        : m_mainDevice(std::move(main_device)),
          m_passthruDevice(std::move(passthru_device)) {}

    ~AudioSettings();
    void FixPassThrough(void);
    void TrimDeviceType(void);

    QString GetMainDevice(void) const
        { return m_mainDevice; }
    QString GetPassthruDevice(void) const
        { return m_passthruDevice; }

  public:
    QString             m_mainDevice;
    QString             m_passthruDevice;
    AudioFormat         m_format          {FORMAT_NONE};
    int                 m_channels        {-1};
    AVCodecID           m_codec           {AV_CODEC_ID_NONE};
    int                 m_codecProfile    {-1};
    int                 m_sampleRate      {-1};
    bool                m_setInitialVol   {false};
    bool                m_usePassthru     {false};
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

#endif // AUDIO_SETTINGS_H
