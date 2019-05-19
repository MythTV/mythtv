#ifndef _AUDIOOUTPUTAUDIOTRACK_H_
#define _AUDIOOUTPUTAUDIOTRACK_H_

#include "audiooutputbase.h"

class QAndroidJniObject;
/*

    Audio output for android based on android.media.AudioTrack.

    This uses the java class org.mythtv.audio.AudioOutputAudioTrack
    to invoke android media playback methods.

*/

class AudioOutputAudioTrack : public AudioOutputBase
{
  public:
    explicit AudioOutputAudioTrack(const AudioSettings &settings);
    ~AudioOutputAudioTrack() override;

    bool AddData(void *buffer, int len, int64_t timecode, int frames) override; // AudioOutput

    // Volume control
    int GetVolumeChannel(int /* channel */) const override // VolumeBase
        { return 100; }
    void SetVolumeChannel(int /* channel */, int /* volume */) override // VolumeBase
        {}
    void Pause(bool paused) override; // AudioOutput

  protected:
    bool OpenDevice(void) override; // AudioOutputBase
    void CloseDevice(void) override; // AudioOutputBase
    void WriteAudio(unsigned char *aubuf, int size) override; // AudioOutputBase
    int  GetBufferedOnSoundcard(void) const override; // AudioOutputBase
    AudioOutputSettings* GetOutputSettings(bool digital) override; // AudioOutputBase
    void SetSourceBitrate(int rate) override; // AudioOutputBase
    bool StartOutputThread(void) override; // AudioOutputBase
    void StopOutputThread(void) override; // AudioOutputBase
    QAndroidJniObject *m_audioTrack {nullptr};
    int m_bitsPer10Frames {0};
};

#endif //_AUDIOOUTPUTAUDIOTRACK_H_
