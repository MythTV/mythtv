// Std C headers
#include <cstdio>
#include <unistd.h>
#include <string.h>

#include "config.h"

// libav headers
extern "C" {
#include "libavcodec/avcodec.h"
#ifdef ENABLE_AC3_DECODER
#include "libavcodec/parser.h"
#else
#include <a52dec/a52.h>
#endif
}

// MythTV headers
#include "mythcontext.h"
#include "audiooutputdigitalencoder.h"
#include "compat.h"

#define LOC QString("DEnc: ")
#define LOC_ERR QString("DEnc, Error: ")

#define MAX_AC3_FRAME_SIZE 6144

AudioOutputDigitalEncoder::AudioOutputDigitalEncoder(void) :
    av_context(NULL),
    outbuf(NULL),
    outbuf_size(0),
    frame_buffer(NULL),
    one_frame_bytes(0)
{
}

AudioOutputDigitalEncoder::~AudioOutputDigitalEncoder()
{
    Dispose();
}

void AudioOutputDigitalEncoder::Dispose()
{
    if (av_context)
    {
        avcodec_close(av_context);
        av_free(av_context);
        av_context = NULL;
    }

    if (outbuf)
    {
        delete [] outbuf;
        outbuf = NULL;
        outbuf_size = 0;
    }

    if (frame_buffer)
    {
        delete [] frame_buffer;
        frame_buffer = NULL;
        one_frame_bytes = 0;
    }
}

//CODEC_ID_AC3
bool AudioOutputDigitalEncoder::Init(
    CodecID codec_id, int bitrate, int samplerate, int channels)
{
    AVCodec *codec;
    int ret;

    VERBOSE(VB_AUDIO, LOC + QString("Init codecid=%1, br=%2, sr=%3, ch=%4")
            .arg(codec_id_string(codec_id))
            .arg(bitrate)
            .arg(samplerate)
            .arg(channels));

    //codec = avcodec_find_encoder(codec_id);
    // always AC3 as there is no DTS encoder at the moment 2005/1/9
    codec = avcodec_find_encoder(CODEC_ID_AC3);
    if (!codec)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Could not find codec");
        return false;
    }

    av_context = avcodec_alloc_context();
    av_context->bit_rate = bitrate;
    av_context->sample_rate = samplerate;
    av_context->channels = channels;

    // open it */
    ret = avcodec_open(av_context, codec);
    if (ret < 0) 
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Could not open codec, invalid bitrate or samplerate");

        Dispose();
        return false;
    }

    size_t bytes_per_frame = av_context->channels * sizeof(short);
    audio_bytes_per_sample = bytes_per_frame;
    one_frame_bytes = bytes_per_frame * av_context->frame_size;

    outbuf_size = 16384;    // ok for AC3 but DTS?
    outbuf = new char [outbuf_size];
    VERBOSE(VB_AUDIO, QString("DigitalEncoder::Init fs=%1, bpf=%2 ofb=%3")
            .arg(av_context->frame_size)
            .arg(bytes_per_frame)
            .arg(one_frame_bytes));

    return true;
}

static int DTS_SAMPLEFREQS[16] =
{
    0,      8000,   16000,  32000,  64000,  128000, 11025,  22050,
    44100,  88200,  176400, 12000,  24000,  48000,  96000,  192000
};

static int DTS_BITRATES[30] =
{
    32000,    56000,    64000,    96000,    112000,   128000,
    192000,   224000,   256000,   320000,   384000,   448000,
    512000,   576000,   640000,   768000,   896000,   1024000,
    1152000,  1280000,  1344000,  1408000,  1411200,  1472000,
    1536000,  1920000,  2048000,  3072000,  3840000,  4096000
};

static int dts_decode_header(uint8_t *indata_ptr, int *rate,
                             int *nblks, int *sfreq)
{
    uint id = ((indata_ptr[0] << 24) | (indata_ptr[1] << 16) |
               (indata_ptr[2] << 8)  | (indata_ptr[3]));

    if (id != 0x7ffe8001)
        return -1;

    int ftype = indata_ptr[4] >> 7;

    int surp = (indata_ptr[4] >> 2) & 0x1f;
    surp = (surp + 1) % 32;

    *nblks = (indata_ptr[4] & 0x01) << 6 | (indata_ptr[5] >> 2);
    ++*nblks;

    int fsize = (indata_ptr[5] & 0x03) << 12 |
                (indata_ptr[6]         << 4) | (indata_ptr[7] >> 4);
    ++fsize;

    *sfreq = (indata_ptr[8] >> 2) & 0x0f;
    *rate = (indata_ptr[8] & 0x03) << 3 | ((indata_ptr[9] >> 5) & 0x07);

    if (ftype != 1)
    {
        VERBOSE(VB_IMPORTANT, LOC +
                QString("DTS: Termination frames not handled (ftype %1)")
                .arg(ftype));
        return -1;
    }

    if (*sfreq != 13)
    {
        VERBOSE(VB_IMPORTANT, LOC +
                QString("DTS: Only 48kHz supported (sfreq %1)").arg(*sfreq));
        return -1;
    }

    if ((fsize > 8192) || (fsize < 96))
    {
        VERBOSE(VB_IMPORTANT, LOC +
                QString("DTS: fsize: %1 invalid").arg(fsize));
        return -1;
    }

    if (*nblks != 8 && *nblks != 16 && *nblks != 32 &&
        *nblks != 64 && *nblks != 128 && ftype == 1)
    {
        VERBOSE(VB_IMPORTANT, LOC +
                QString("DTS: nblks %1 not valid for normal frame")
                .arg(*nblks));
        return -1;
    }

    return fsize;
}

static int dts_syncinfo(uint8_t *indata_ptr, int *flags,
                        int *sample_rate, int *bit_rate)
{
    (void) flags;

    int nblks;
    int rate;
    int sfreq;

    int fsize = dts_decode_header(indata_ptr, &rate, &nblks, &sfreq);
    if (fsize >= 0)
    {
        *bit_rate = (rate >= 0 && rate <= 29) ? DTS_BITRATES[rate] : 0;
        *sample_rate = (sfreq >= 1 && sfreq <= 15)? DTS_SAMPLEFREQS[sfreq] : 0;
    }

    return fsize;
}

// until there is an easy way to do this with ffmpeg
// get the code from libavcodec/parser.c made non static
extern "C" int ac3_sync(const uint8_t *buf, int *channels, int *sample_rate,
                        int *bit_rate, int *samples);

// from http://www.ebu.ch/CMSimages/en/tec_AES-EBU_eg_tcm6-11890.pdf
// http://en.wikipedia.org/wiki/S/PDIF
typedef struct {
    // byte 0
    unsigned professional_consumer:1;
    unsigned non_data:1;
    // 4 - no emphasis
    // 6 - 50/15us
    // 7 - CCITT J17
    unsigned audio_signal_emphasis:3;
    unsigned SSFL:1;
    // 0
    // 1 - 48k
    // 2 - 44.1k
    // 3 - 32k
    unsigned sample_frequency:2;    
    // byte 1
    // 0
    // 1 - 2 ch
    // 2 - mono
    // 3 - prim/sec
    // 4 - stereo
    unsigned channel_mode:4;
    // 0
    // 1 - 192 bit block
    // 2 - AES18
    // 3 - user def
    unsigned user_bit_management:4;
    // byte 2
    // 1 - audio data
    // 2 - co-ordn
    unsigned auxiliary_bits:3;
    // 4 - 16 bits
    // 5-7 - redither to 16 bits
    unsigned source_word_length:3;
    unsigned reserved:2;
    // byte 3
    unsigned multi_channel_function_description:8;
    // byte 4
    unsigned digital_audio_reference_signal:2;
    unsigned reserved2:6;

} AESHeader;

static int encode_frame(
        bool dts, 
        unsigned char *data,
        size_t &len)
{
    size_t enc_len;
    int flags, sample_rate, bit_rate;

    // we don't do any length/crc validation of the AC3 frame here; presumably
    // the receiver will have enough sense to do that.  if someone has a
    // receiver that doesn't, here would be a good place to put in a call
    // to a52_crc16_block(samples+2, data_size-2) - but what do we do if the
    // packet is bad?  we'd need to send something that the receiver would
    // ignore, and if so, may as well just assume that it will ignore
    // anything with a bad CRC...

    uint nr_samples = 0, block_len;
    if (dts)
    {
        enc_len = dts_syncinfo(data+8, &flags, &sample_rate, &bit_rate);
        int rate, sfreq, nblks;
        dts_decode_header(data+8, &rate, &nblks, &sfreq);
        nr_samples = nblks * 32;
        block_len = nr_samples * 2 * 2;
    }
    else
    {
#ifdef ENABLE_AC3_DECODER
        enc_len = ac3_sync(
            data + 8, &flags, &sample_rate, &bit_rate, (int*)&block_len);
        block_len *= 2 * 2;
#else
        enc_len = a52_syncinfo(data + 8, &flags, &sample_rate, &bit_rate);
        block_len = MAX_AC3_FRAME_SIZE;
#endif
    }

    if (enc_len == 0 || enc_len > len)
    {
        int l = len;
        len = 0;
        return l;
    }

    enc_len = min((uint)enc_len, block_len - 8);

    //uint32_t x = *(uint32_t*)(data+8);
    // in place swab
    swab((const char *)data + 8, (char *)data + 8, enc_len);
    //VERBOSE(VB_AUDIO|VB_TIMESTAMP, 
    //        QString("DigitalEncoder::Encode swab test %1 %2")
    //        .arg(x,0,16).arg(*(uint32_t*)(data+8),0,16));

    // the following values come from libmpcodecs/ad_hwac3.c in mplayer.
    // they form a valid IEC958 AC3 header.
    data[0] = 0x72;
    data[1] = 0xF8;
    data[2] = 0x1F;
    data[3] = 0x4E;
    data[4] = 0x01;
    if (dts)
    {
        switch(nr_samples)
        {
            case 256:
                data[4] = 0x0A;      /* DTS-? (256-sample bursts) */
                break;

            case 512:
                data[4] = 0x0B;      /* DTS-1 (512-sample bursts) */
                break;

            case 1024:
                data[4] = 0x0C;      /* DTS-2 (1024-sample bursts) */
                break;

            case 2048:
                data[4] = 0x0D;      /* DTS-3 (2048-sample bursts) */
                break;

            case 4096:
                data[4] = 0x0E;      /* DTS-? (4096-sample bursts) */
                break;

            default:
                VERBOSE(VB_IMPORTANT, LOC +
                        QString("DTS: %1-sample bursts not supported")
                        .arg(nr_samples));
                data[4] = 0x00;
                break;
        }
    }
    data[5] = 0x00;
    data[6] = (enc_len << 3) & 0xFF;
    data[7] = (enc_len >> 5) & 0xFF;
    memset(data + 8 + enc_len, 0, block_len - 8 - enc_len);
    len = block_len;

    return enc_len;
}

// must have exactly 1 frames worth of data
size_t AudioOutputDigitalEncoder::Encode(short *buff)
{
    int encsize = 0;
    size_t outsize = 0;
 
    // put data in the correct spot for encode frame
    outsize = avcodec_encode_audio(
        av_context, ((uchar*)outbuf) + 8, outbuf_size - 8, buff);

    size_t tmpsize = outsize;

    outsize = MAX_AC3_FRAME_SIZE;
    encsize = encode_frame(
        /*av_context->codec_id==CODEC_ID_DTS*/ false,
        (unsigned char*)outbuf, outsize);

    VERBOSE(VB_AUDIO|VB_TIMESTAMP, 
            QString("DigitalEncoder::Encode len1=%1 len2=%2 finallen=%3")
                .arg(tmpsize).arg(encsize).arg(outsize));

    return outsize;
}
