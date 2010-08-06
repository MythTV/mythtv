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
    PrivateDecoderMPEG2 *mpeg2 = new PrivateDecoderMPEG2();
    if (mpeg2 && mpeg2->Init(decoder, no_hardware_decode, avctx))
        return mpeg2;
    delete mpeg2;

#if defined(Q_OS_MACX)
    PrivateDecoderVDA *vda = new PrivateDecoderVDA();
    if (vda && vda->Init(decoder, no_hardware_decode, avctx))
        return vda;
    delete vda;
#endif

    return NULL;
}
