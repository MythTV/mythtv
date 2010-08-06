#include "privatedecoder.h"
#include "privatedecoder_mpeg2.h"

#if defined(Q_OS_MACX)
#include "privatedecoder_vda.h"
#endif

void PrivateDecoder::GetDecoders(render_opts &opts)
{
    PrivateDecoderMPEG2::GetDecoders(opts);

#if defined(Q_OS_MACX)
    PrivateDecoderVDA::GetDecoders(opts);
#endif
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

#if defined(Q_OS_MACX)
    test = new PrivateDecoderVDA();
    if (test && test->Init(decoder, no_hardware_decode, avctx))
        return test;
#endif
    delete test;
    return NULL;
}
