#include "privatedecoder.h"

void PrivateDecoder::GetDecoders(RenderOptions &opts)
{
    Q_UNUSED(opts);
}

PrivateDecoder* PrivateDecoder::Create(const QString &decoder,
                                       PlayerFlags flags,
                                       AVCodecContext *avctx)
{
    Q_UNUSED(decoder);
    Q_UNUSED(flags);
    Q_UNUSED(avctx);
    return nullptr;
}
