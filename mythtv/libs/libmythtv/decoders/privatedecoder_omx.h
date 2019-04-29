#ifndef PRIVATEDECODER_OMX_H
#define PRIVATEDECODER_OMX_H

// Qt
#include <QMutex>
#include <QSemaphore>
#include <QVector>
#include <QList>

// OpenMax
#include <OMX_Types.h>
#include <OMX_Core.h>
#include <OMX_Component.h>
#ifdef USING_BROADCOM
#include <OMX_Broadcom.h>
#endif

// MythTV
#include "omxcontext.h"
#include "privatedecoder.h"

class PrivateDecoderOMX : public PrivateDecoder, private OMXComponentCtx
{
  public:
    static  void GetDecoders(render_opts &Options);
    static  const QString DecoderName;

    PrivateDecoderOMX();
    virtual ~PrivateDecoderOMX() override;

    // PrivateDecoder
    QString GetName(void) override;
    bool    Init(const QString &Decoder, PlayerFlags Flags, AVCodecContext *AVCtx) override;
    bool    Reset(void) override;
    int     GetFrame(AVStream *Stream, AVFrame *Frame, int *GotPicturePtr, AVPacket *Packet) override;
    bool    HasBufferedFrames(void) override;
    bool    NeedsReorderedPTS(void) override { return true; }

  private:
    // OMXComponentCtx
    OMX_ERRORTYPE Event(OMXComponent& Component, OMX_EVENTTYPE EventType, OMX_U32 Data1, OMX_U32 Data2, OMX_PTR EventData) override;
    OMX_ERRORTYPE EmptyBufferDone(OMXComponent&, OMX_BUFFERHEADERTYPE* Header) override;
    OMX_ERRORTYPE FillBufferDone(OMXComponent&, OMX_BUFFERHEADERTYPE* Header) override;
    void          ReleaseBuffers(OMXComponent&) override;

  private:
    // Callbacks
    OMX_ERRORTYPE AllocBuffersCB(void);
    OMX_ERRORTYPE FreeOutputBuffersCB(void);
    OMX_ERRORTYPE AllocOutputBuffersCB(void);
    OMX_ERRORTYPE UseBuffersCB(void);

    OMX_ERRORTYPE FillOutputBuffers(void);
    OMX_ERRORTYPE SettingsChanged(AVCodecContext *AVCtx);
    OMX_ERRORTYPE SetNalType(AVCodecContext *AVCtx);
    int           ProcessPacket(AVStream *Stream, AVPacket *Packet);
    int           GetBufferedFrame(AVStream *Stream, AVFrame *Frame);
    bool          CreateFilter(AVCodecContext *AVCtx);
#ifdef USING_BROADCOM
    OMX_ERRORTYPE GetAspect(OMX_CONFIG_POINTTYPE &Point, int Index) const;
#endif

  private:
    OMXComponent    m_videoDecoder;
    AVBSFContext   *m_bitstreamFilter;
    bool            m_startTime;
    AVCodecContext *m_avctx;

    QMutex          m_settingsChangedLock;
    bool            m_settingsChanged;
    bool            m_settingsHaveChanged;

    QSemaphore m_inputBuffersSem;
    QSemaphore m_outputBuffersSem;
    QList<OMX_BUFFERHEADERTYPE*> m_inputBuffers;
    QList<OMX_BUFFERHEADERTYPE*> m_outputBuffers;
};

#endif // ndef PRIVATEDECODER_OMX_H
/* EOF */
