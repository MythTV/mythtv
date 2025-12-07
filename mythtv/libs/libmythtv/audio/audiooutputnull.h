#ifndef AUDIOOUTPUTNULL
#define AUDIOOUTPUTNULL

#include "audiooutputbase.h"

static constexpr int32_t NULLAUDIO_OUTPUT_BUFFER_SIZE { 32768 };

/*

    In its default invocation, this AudioOutput object does almost nothing. 
    It takes all bytes written to its "device" and just ignores them. Since
    there is no device in the way consuming the audio bytes, nothing
    throttles the speed of decoding.
    
    If it is told to buffer the output data (bufferOutputData(true)), then
    it will maintain a small buffer and will not let anymore audio data be
    decoded until something pulls the data off (via readOutputData()). 

*/

class AudioOutputNULL : public AudioOutputBase
{
  public:
    explicit AudioOutputNULL(const AudioSettings &settings);
    ~AudioOutputNULL() override;

    void Reset(void) override; // AudioOutputBase


    // Volume control
    int GetVolumeChannel(int /* channel */) const override // VolumeBase
        { return 100; }
    void SetVolumeChannel(int /* channel */, int /* volume */) override // VolumeBase
        {}

    int readOutputData(unsigned char *read_buffer, size_t max_length) override; // AudioOutputBase

  protected:
    // AudioOutputBase
    bool OpenDevice(void) override; // AudioOutputBase
    void CloseDevice(void) override; // AudioOutputBase
    void WriteAudio(unsigned char *aubuf, int size) override; // AudioOutputBase
    int  GetBufferedOnSoundcard(void) const override; // AudioOutputBase
    AudioOutputSettings* GetOutputSettings(bool digital) override; // AudioOutputBase

  private:
    QMutex        m_pcmOutputBufferMutex;
    std::vector<unsigned char> m_pcmOutputBuffer {0};
};

#endif

