#ifndef AUDIOOUTPUT_OMX_H
#define AUDIOOUTPUT_OMX_H

#include <OMX_Types.h>
#include <OMX_Core.h>

#include <QMutex>
#include <QSemaphore>
#include <QList>
#include <QAtomicInt>

#include "audiooutputbase.h"
#include "omxcontext.h"

class AudioDecoderOMX;

class AudioOutputOMX : public AudioOutputBase, private OMXComponentCtx
{
    friend class AudioDecoderOMX;

    // No copying
    AudioOutputOMX(const AudioOutputOMX&);
    AudioOutputOMX & operator =(const AudioOutputOMX&);

  public:
    explicit AudioOutputOMX(const AudioSettings &settings);
    virtual ~AudioOutputOMX();

    // VolumeBase implementation
    virtual int GetVolumeChannel(int channel) const; // Returns 0-100
    virtual void SetVolumeChannel(int channel, int volume); // range 0-100 for vol

    // AudioOutput overrides
    virtual int DecodeAudio(AVCodecContext*, uint8_t*, int&, const AVPacket*);

  protected:
    // AudioOutputBase implementation
    virtual bool OpenDevice(void);
    virtual void CloseDevice(void);
    virtual void WriteAudio(uchar *aubuf, int size);
    virtual int  GetBufferedOnSoundcard(void) const;

    // AudioOutputBase overrides
    virtual AudioOutputSettings* GetOutputSettings(bool passthrough);

  private:
    // OMXComponentCtx implementation
    virtual OMX_ERRORTYPE EmptyBufferDone(OMXComponent&, OMX_BUFFERHEADERTYPE*);
    virtual void ReleaseBuffers(OMXComponent&);

  private:
    // implementation
    bool OpenMixer();

    // OMXComponentCB actions
    typedef OMX_ERRORTYPE ComponentCB();
    ComponentCB FreeBuffersCB, AllocBuffersCB;

  private:
    OMXComponent m_audiorender;
    AudioDecoderOMX *m_audiodecoder;

    QSemaphore m_ibufs_sema;    // EmptyBufferDone signal
    QMutex mutable m_lock;      // Protects data following
    QList<OMX_BUFFERHEADERTYPE*> m_ibufs;
    QAtomicInt m_pending;
};

#endif // ndef AUDIOOUTPUT_OMX_H
