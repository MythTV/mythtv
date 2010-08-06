#include "privatedecoder.h"
#include "privatedecoder_mpeg2.h"

void PrivateDecoder::GetDecoders(render_opts &opts)
{
    PrivateDecoderMPEG2::GetDecoders(opts);
}

PrivateDecoder* PrivateDecoder::Create(const QString &decoder,
                                       bool no_hardware_decode,
                                       AVCodecContext *avctx)
{
    PrivateDecoder *test = NULL;
    test = new PrivateDecoderMPEG2();
    if (test && test->Init(decoder, no_hardware_decode, avctx))
        return test;
    delete test;
    return NULL;
}
