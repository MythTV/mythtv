#ifndef PRIVATEDECODER_MPEG2_H
#define PRIVATEDECODER_MPEG2_H

// C headers
#include <stdint.h>

// MythTV headers
#include "mythdeque.h"

extern "C" {
#if CONFIG_LIBMPEG2EXTERNAL
#include <mpeg2dec/mpeg2.h>
#else
#include "../libmythmpeg2/mpeg2.h"
#endif
}

#include "privatedecoder.h"

typedef MythDeque<AVFrame*> avframe_q;

class PrivateDecoderMPEG2 : public PrivateDecoder
{
  public:
    static void GetDecoders(render_opts &opts);
    PrivateDecoderMPEG2();
    virtual ~PrivateDecoderMPEG2();
    virtual QString GetName(void) { return "libmpeg2"; }
    virtual bool Init(const QString &decoder,
                      bool no_hardware_decode,
                      CodecID codec_id,
                      void* extradata,
                      int extradata_size);
    virtual bool Reset(void);
    virtual int  GetFrame(AVCodecContext *avctx,
                          AVFrame *picture,
                          int *got_picture_ptr,
                          AVPacket *pkt);
  private:
    void ClearFrames(void);
    mpeg2dec_t *mpeg2dec;
    avframe_q   partialFrames;
};

#endif // PRIVATEDECODER_MPEG2_H
