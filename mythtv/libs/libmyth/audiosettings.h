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
    AudioSettings();
    AudioSettings(const AudioSettings &other);
    AudioSettings(
        const QString          &main_device,
        const QString          &passthru_device,
        AudioFormat             format,
        int                     channels,
        int                     codec,
        int                     samplerate,
        AudioOutputSource       source,
        bool                    set_initial_vol,
        bool                    use_passthru,
        int                     upmixer_startup = 0,
        AudioOutputSettings     *custom = NULL);

    AudioSettings(AudioFormat   format,
                  int           channels,
                  int           codec,
                  int           samplerate,
                  bool          use_passthru,
                  int           upmixer_startup = 0,
                  int           codec_profile = 0);

    AudioSettings(const QString    &main_device,
                  const QString    &passthru_device = QString::null);

    ~AudioSettings();
    void FixPassThrough(void);
    void TrimDeviceType(void);

    QString GetMainDevice(void) const;
    QString GetPassthruDevice(void) const;

  public:
    QString             main_device;
    QString             passthru_device;
    AudioFormat         format;
    int                 channels;
    int                 codec;
    int                 codec_profile;
    int                 samplerate;
    bool                set_initial_vol;
    bool                use_passthru;
    AudioOutputSource   source;
    int                 upmixer;
    /**
     * If set to false, AudioOutput instance will not try to initially open
     * the audio device
     */
    bool                init;
    /**
     * custom contains a pointer to the audio device capabilities
     * if defined, AudioOutput will not try to automatically discover them.
     * This is used by the AudioTest setting screen where the user can
     * manually override and immediately use them.
     */
    AudioOutputSettings *custom;
};

#endif // _AUDIO_SETTINGS_H_
