/* -*- Mode: c++ -*-
 *
 * Copyright (C) Daniel Kristjansson 2008
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
        const QString    &main_device,
        const QString    &passthru_device,
        AudioFormat       format,
        int               channels,
        int               codec,
        int               samplerate,
        AudioOutputSource source,
        bool              set_initial_vol,
        bool              use_passthru,
        int               upmixer_startup = 0);

    AudioSettings(AudioFormat format,
                  int         channels,
                  int         codec,
                  int         samplerate,
                  bool        use_passthru,
                  int         upmixer_startup = 0);

    void FixPassThrough(void);
    void TrimDeviceType(void);

    QString GetMainDevice(void) const;
    QString GetPassthruDevice(void) const;

  private:
    QString           main_device;
    QString           passthru_device;

  public:
    AudioFormat       format;
    int               channels;
    int               codec;
    int               samplerate;
    bool              set_initial_vol;
    bool              use_passthru;
    AudioOutputSource source;
    int               upmixer;
};

#endif // _AUDIO_SETTINGS_H_
