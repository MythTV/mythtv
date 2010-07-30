#ifndef PRIVATEDECODER_H
#define PRIVATEDECODER_H

#include <QString>
extern "C" {
#include "avcodec.h"
}

#include "videodisplayprofile.h"
#include "mythcodecid.h"

class PrivateDecoder
{
  public:
    static void GetDecoders(render_opts &opts);
    static PrivateDecoder* Create(const QString &decoder,
                                  bool no_hardware_decode,
                                  CodecID codec_id,
                                  void* extradata,
                                  int extradata_size);
    PrivateDecoder() { }
    virtual ~PrivateDecoder() { }
    virtual QString GetName(void) = 0;
    virtual bool Init(const QString &decoder,
                      bool no_hardware_decode,
                      CodecID codec_id,
                      void* extradata,
                      int extradata_size) = 0;
    virtual bool Reset(void) = 0;
    virtual int  GetFrame(AVCodecContext *avctx,
                          AVFrame *picture,
                          int *got_picture_ptr,
                          AVPacket *pkt) = 0;
};

#endif // PRIVATEDECODER_H
