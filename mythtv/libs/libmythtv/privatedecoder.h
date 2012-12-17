#ifndef PRIVATEDECODER_H
#define PRIVATEDECODER_H

#include <QString>

#include "mythconfig.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}

#include "videodisplayprofile.h"
#include "mythcodecid.h"
#include "mythplayer.h"

class PrivateDecoder
{
  public:
    static void GetDecoders(render_opts &opts);
    static PrivateDecoder* Create(const QString &decoder,
                                  PlayerFlags flags,
                                  AVCodecContext *avctx);
    PrivateDecoder() { }
    virtual ~PrivateDecoder() { }
    virtual QString GetName(void) = 0;
    virtual bool Init(const QString &decoder,
                      PlayerFlags flags,
                      AVCodecContext *avctx) = 0;
    virtual bool Reset(void) = 0;
    virtual int  GetFrame(AVStream *stream,
                          AVFrame *picture,
                          int *got_picture_ptr,
                          AVPacket *pkt) = 0;
    virtual bool HasBufferedFrames(void) { return false; }
    virtual bool NeedsReorderedPTS(void) { return false; }
};

#endif // PRIVATEDECODER_H
