#ifndef AUDIOOUTPUTOPENSLES
#define AUDIOOUTPUTOPENSLES

#include <QMutex>
// For native audio
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <jni.h>

// MythTV headers
#include "audiooutputbase.h"

using slCreateEngine_t = SLresult (*)(
        SLObjectItf*, SLuint32, const SLEngineOption*, SLuint32,
        const SLInterfaceID*, const SLboolean*);

class AudioOutputOpenSLES : public AudioOutputBase
{
  friend class AudioOutputOpenSLESPrivate;
  public:
    explicit AudioOutputOpenSLES(const AudioSettings &settings);
    virtual ~AudioOutputOpenSLES();

    // Volume control
    int  GetVolumeChannel(int channel) const override; // VolumeBase
    void SetVolumeChannel(int channel, int volume) override; // VolumeBase

  protected:
    bool OpenDevice(void) override; // AudioOutputBase
    void CloseDevice(void) override; // AudioOutputBase
    void WriteAudio(unsigned char *aubuf, int size) override; // AudioOutputBase
    int  GetBufferedOnSoundcard(void) const override; // AudioOutputBase
    AudioOutputSettings* GetOutputSettings(bool digital) override; // AudioOutputBase

  private:
    int GetNumberOfBuffersQueued() const;

    //static const uint      kPacketCnt;

    bool CreateEngine();
    static void SPlayedCallback(SLAndroidSimpleBufferQueueItf caller, void *pContext);
    void PlayedCallback(SLAndroidSimpleBufferQueueItf caller);
    bool StartPlayer();
    bool Stop();
    bool Open();
    void Close();

  private:
    /* OpenSL objects */
    SLObjectItf                     m_engineObject      {nullptr};
    SLObjectItf                     m_outputMixObject   {nullptr};
    SLAndroidSimpleBufferQueueItf   m_playerBufferQueue {nullptr};
    SLObjectItf                     m_playerObject      {nullptr};
    SLVolumeItf                     m_volumeItf         {nullptr};
    SLEngineItf                     m_engineEngine      {nullptr};
    SLPlayItf                       m_playerPlay        {nullptr};

    /* OpenSL symbols */
    void                           *m_so_handle         {nullptr};

    slCreateEngine_t                m_slCreateEnginePtr {nullptr};
    SLInterfaceID                   m_SL_IID_ENGINE     {nullptr};
    SLInterfaceID                   m_SL_IID_ANDROIDSIMPLEBUFFERQUEUE {nullptr};
    SLInterfaceID                   m_SL_IID_VOLUME     {nullptr};
    SLInterfaceID                   m_SL_IID_PLAY       {nullptr};

    /* audio buffered through opensles */
    uint8_t                        *m_buf               {nullptr};
    int                             m_bufWriteBase     {0}; // always fragment aligned
    int                             m_bufWriteIndex    {0}; // offset from base

    /* if we can measure latency already */
    bool                            m_started          {false};
    int                             m_nativeOutputSampleRate;

    QMutex                          m_lock;

};

#endif // AUDIOOUTPUTOPENSLES
