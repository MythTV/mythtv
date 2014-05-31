#ifndef PRIVATEDECODER_CRYSTALHD_H
#define PRIVATEDECODER_CRYSTALHD_H

#include "mthread.h"

#if defined(WIN32)
typedef void                *HANDLE;
#else
#ifndef __LINUX_USER__
#define __LINUX_USER__
#endif
#endif

#include "mythframe.h"
#include <libcrystalhd/bc_dts_types.h>
#include <libcrystalhd/bc_dts_defs.h>
#include <libcrystalhd/libcrystalhd_if.h>
#include "privatedecoder.h"

class PrivateDecoderCrystalHD;
class FetcherThread : public MThread
{
  public:
    FetcherThread(PrivateDecoderCrystalHD *dec)
      : MThread("Fetcher"), m_dec(dec) { }

  protected:
    virtual void run(void);

  private:
    PrivateDecoderCrystalHD *m_dec;
};

typedef struct PacketBuffer_
{
    uint8_t *buf;
    int      size;
    int64_t  pts;
} PacketBuffer;

enum BC_DEVICE_TYPE
{
    BC_70012 = 0,
    BC_70015 = 1,
};

class PrivateDecoderCrystalHD : public PrivateDecoder
{
    friend class FetcherThread;

  public:
    static void GetDecoders(render_opts &opts);
    PrivateDecoderCrystalHD();
    virtual ~PrivateDecoderCrystalHD();
    virtual QString GetName(void) { return QString("crystalhd"); }
    virtual bool Init(const QString &decoder,
                      PlayerFlags flags,
                      AVCodecContext *avctx);
    virtual bool Reset(void);
    virtual int  GetFrame(AVStream *stream,
                          AVFrame *picture,
                          int *got_picture_ptr,
                          AVPacket *pkt);
    virtual bool HasBufferedFrames(void);
    virtual bool NeedsReorderedPTS(void) { return true; }

  private:
    void FetchFrames(void);
    bool StartFetcherThread(void);
    int  ProcessPacket(AVStream *stream, AVPacket *pkt);

    bool CreateFilter(AVCodecContext *avctx);
    void FillFrame(BC_DTS_PROC_OUT *out);
    void AddFrameToQueue(void);
    void CheckProcOutput(BC_DTS_PROC_OUT *out);
    void CheckPicInfo(BC_DTS_PROC_OUT *out);
    void CheckStatus(void);
    int GetTxFreeSize(bool hwsel);

    HANDLE             m_device;
    BC_DEVICE_TYPE     m_device_type;
    BC_OUTPUT_FORMAT   m_pix_fmt;
    QList<VideoFrame*> m_decoded_frames;
    QList<PacketBuffer*> m_packet_buffers;
    QMutex             m_decoded_frames_lock;
    FetcherThread     *m_fetcher_thread;
    bool               m_fetcher_pause;
    bool               m_fetcher_paused;
    bool               m_fetcher_stop;
    VideoFrame        *m_frame;
    AVBitStreamFilterContext *m_filter;
};

#endif // PRIVATEDECODER_CRYSTALHD_H
