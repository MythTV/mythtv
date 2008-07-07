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
        const QString    &audio_main_device,
        const QString    &audio_passthru_device,
        int               audio_bits,
        int               audio_channels,
        int               audio_samplerate,
        AudioOutputSource audio_source,
        bool              audio_set_initial_vol,
        bool              audio_use_passthru,
        void             *audio_codec = NULL);

    AudioSettings(int   audio_bits, 
                  int   audio_channels, 
                  int   audio_samplerate,
                  bool  audio_use_passthru,
                  void *audio_codec = NULL);

    void FixPassThrough(void);
    void TrimDeviceType(void);

    QString GetMainDevice(void) const;
    QString GetPassthruDevice(void) const;

  private:
    QString main_device;
    QString passthru_device;

  public:
    int     bits;
    int     channels;
    int     samplerate;
    bool    set_initial_vol;
    bool    use_passthru;
    void   *codec;
    AudioOutputSource source;
};

#endif // _AUDIO_SETTINGS_H_
