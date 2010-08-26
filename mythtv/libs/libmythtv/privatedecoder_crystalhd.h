#ifndef PRIVATEDECODER_CRYSTALHD_H
#define PRIVATEDECODER_CRYSTALHD_H

#include <QThread>

#if defined(WIN32)
typedef void                *HANDLE;
#else
#ifndef __LINUX_USER__
#define __LINUX_USER__
#endif
#endif

#include "frame.h"
#include <libcrystalhd/bc_dts_types.h>
#include <libcrystalhd/bc_dts_defs.h>
#include <libcrystalhd/libcrystalhd_if.h>
#include "privatedecoder.h"

enum BC_DEVICE_TYPE
{
    BC_70012 = 0,
    BC_70015 = 1,
};

class PrivateDecoderCrystalHD : public PrivateDecoder
{
  public:
    static void GetDecoders(render_opts &opts);
    PrivateDecoderCrystalHD();
    virtual ~PrivateDecoderCrystalHD();
    virtual QString GetName(void) { return QString("crystalhd"); }
    virtual bool Init(const QString &decoder,
                      bool no_hardware_decode,
                      AVCodecContext *avctx);
    virtual bool Reset(void);
    virtual int  GetFrame(AVCodecContext *avtx,
                          AVFrame *picture,
                          int *got_picture_ptr,
                          AVPacket *pkt);
    void         RetrieveFrame(void);

  private:

    bool CreateFilter(AVCodecContext *avctx);
    void FillFrame(BC_DTS_PROC_OUT *out);
    void AddFrameToQueue(void);
    void CheckProcOutput(BC_DTS_PROC_OUT *out);
    void CheckPicInfo(BC_DTS_PROC_OUT *out);
    void CheckStatus(void);

    HANDLE             m_device;
    BC_DEVICE_TYPE     m_device_type;
    BC_OUTPUT_FORMAT   m_pix_fmt;
    QList<VideoFrame*> m_decoded_frames;
    VideoFrame        *m_frame;
    AVBitStreamFilterContext *m_filter;
};

#endif // PRIVATEDECODER_CRYSTALHD_H
