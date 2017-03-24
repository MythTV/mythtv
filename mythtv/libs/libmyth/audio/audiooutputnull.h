#ifndef AUDIOOUTPUTNULL
#define AUDIOOUTPUTNULL

#include "audiooutputbase.h"

#define NULLAUDIO_OUTPUT_BUFFER_SIZE 32768

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

    virtual ~AudioOutputNULL();

    virtual void Reset(void);


    // Volume control
    virtual int GetVolumeChannel(int /* channel */) const { return 100; }
    virtual void SetVolumeChannel(int /* channel */, int /* volume */){return;}

    virtual int readOutputData(unsigned char *read_buffer, int max_length);

  protected:
    // AudioOutputBase
    virtual bool OpenDevice(void);
    virtual void CloseDevice(void);
    virtual void WriteAudio(unsigned char *aubuf, int size);
    virtual int  GetBufferedOnSoundcard(void) const;
    virtual AudioOutputSettings* GetOutputSettings(bool digital);

  private:
    QMutex        pcm_output_buffer_mutex;
    unsigned char pcm_output_buffer[NULLAUDIO_OUTPUT_BUFFER_SIZE];
    int           current_buffer_size;
};

#endif

