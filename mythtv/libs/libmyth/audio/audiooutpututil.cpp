#include <cinttypes>
#include <cmath>
#include <sys/types.h>

#include "mythconfig.h"
#include "mythlogging.h"
#include "audiooutpututil.h"
#include "audioconvert.h"
#include "bswap.h"
#include "libmythtv/mythavutil.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "pink.h"
}

#define LOC QString("AOUtil: ")

#define ISALIGN(x) (((unsigned long)(x) & 0xf) == 0)

#if ARCH_X86
static int has_sse2 = -1;

// Check cpuid for SSE2 support on x86 / x86_64
static inline bool sse_check()
{
    if (has_sse2 != -1)
        return (bool)has_sse2;
    __asm__(
        // -fPIC - we may not clobber ebx/rbx
#if ARCH_X86_64
        "push       %%rbx               \n\t"
#else
        "push       %%ebx               \n\t"
#endif
        "mov        $1, %%eax           \n\t"
        "cpuid                          \n\t"
        "and        $0x4000000, %%edx   \n\t"
        "shr        $26, %%edx          \n\t"
#if ARCH_X86_64
        "pop        %%rbx               \n\t"
#else
        "pop        %%ebx               \n\t"
#endif
        :"=d"(has_sse2)
        ::"%eax","%ecx"
    );
    return (bool)has_sse2;
}
#endif //ARCH_x86

/**
 * Returns true if platform has an FPU.
 * for the time being, this test is limited to testing if SSE2 is supported
 */
bool AudioOutputUtil::has_hardware_fpu()
{
#if ARCH_X86
    return sse_check();
#else
    return false;
#endif
}

/**
 * Convert integer samples to floats
 *
 * Consumes 'bytes' bytes from in and returns the numer of bytes written to out
 */
int AudioOutputUtil::toFloat(AudioFormat format, void *out, const void *in,
                             int bytes)
{
    return AudioConvert::toFloat(format, out, in, bytes);
}

/**
 * Convert float samples to integers
 *
 * Consumes 'bytes' bytes from in and returns the numer of bytes written to out
 */
int AudioOutputUtil::fromFloat(AudioFormat format, void *out, const void *in,
                               int bytes)
{
    return AudioConvert::fromFloat(format, out, in, bytes);
}

/**
 * Convert a mono stream to stereo by copying and interleaving samples
 */
void AudioOutputUtil::MonoToStereo(void *dst, const void *src, int samples)
{
    AudioConvert::MonoToStereo(dst, src, samples);
}

/**
 * Adjust the volume of samples
 *
 * Makes a crude attempt to normalise the relative volumes of
 * PCM from mythmusic, PCM from video and upmixed AC-3
 */
void AudioOutputUtil::AdjustVolume(void *buf, int len, int volume,
                                   bool music, bool upmix)
{
    float g     = volume / 100.0F;
    auto *fptr  = (float *)buf;
    int samples = len >> 2;
    int i       = 0;

    // Should be exponential - this'll do
    g *= g;

    // Try to ~ match stereo volume when upmixing
    if (upmix)
        g *= 1.5F;

    // Music is relatively loud
    if (music)
        g *= 0.4F;

    if (g == 1.0F)
        return;

#if ARCH_X86
    if (sse_check() && samples >= 16)
    {
        int loops = samples >> 4;
        i = loops << 4;

        __asm__ volatile (
            "movss      %2, %%xmm0          \n\t"
            "punpckldq  %%xmm0, %%xmm0      \n\t"
            "punpckldq  %%xmm0, %%xmm0      \n\t"
            "1:                             \n\t"
            "movups     (%0), %%xmm1        \n\t"
            "movups     16(%0), %%xmm2      \n\t"
            "mulps      %%xmm0, %%xmm1      \n\t"
            "movups     32(%0), %%xmm3      \n\t"
            "mulps      %%xmm0, %%xmm2      \n\t"
            "movups     48(%0), %%xmm4      \n\t"
            "mulps      %%xmm0, %%xmm3      \n\t"
            "movups     %%xmm1, (%0)        \n\t"
            "mulps      %%xmm0, %%xmm4      \n\t"
            "movups     %%xmm2, 16(%0)      \n\t"
            "movups     %%xmm3, 32(%0)      \n\t"
            "movups     %%xmm4, 48(%0)      \n\t"
            "add        $64,    %0          \n\t"
            "sub        $1, %%ecx           \n\t"
            "jnz        1b                  \n\t"
            :"+r"(fptr)
            :"c"(loops),"m"(g)
            :"xmm0","xmm1","xmm2","xmm3","xmm4"
        );
    }
#endif //ARCH_X86
    for (; i < samples; i++)
        *fptr++ *= g;
}

template <class AudioDataType>
void _MuteChannel(AudioDataType *buffer, int channels, int ch, int frames)
{
    AudioDataType *s1 = buffer + ch;
    AudioDataType *s2 = buffer - ch + 1;

    for (int i = 0; i < frames; i++)
    {
        *s1 = *s2;
        s1 += channels;
        s2 += channels;
    }
}

/**
 * Mute individual channels through mono->stereo duplication
 *
 * Mute given channel (left or right) by copying right or left
 * channel over.
 */
void AudioOutputUtil::MuteChannel(int obits, int channels, int ch,
                                  void *buffer, int bytes)
{
    int frames = bytes / ((obits >> 3) * channels);

    if (obits == 8)
        _MuteChannel((uchar *)buffer, channels, ch, frames);
    else if (obits == 16)
        _MuteChannel((short *)buffer, channels, ch, frames);
    else
        _MuteChannel((int *)buffer, channels, ch, frames);
}

#if HAVE_BIGENDIAN
#define LE_SHORT(v)      bswap_16(v)
#define LE_INT(v)        bswap_32(v)
#else
#define LE_SHORT(v)      (v)
#define LE_INT(v)        (v)
#endif

char *AudioOutputUtil::GeneratePinkFrames(char *frames, int channels,
                                          int channel, int count, int bits)
{
    pink_noise_t pink;

    initialize_pink_noise(&pink, bits);

    auto *samp16 = (int16_t*) frames;
    auto *samp32 = (int32_t*) frames;

    while (count-- > 0)
    {
        for(int chn = 0 ; chn < channels; chn++)
        {
            if (chn==channel)
            {
                /* Don't use MAX volume */
                double res = generate_pink_noise_sample(&pink) *
                    static_cast<float>(0x03fffffff);
                int32_t ires = res;
                if (bits == 16)
                    *samp16++ = LE_SHORT(ires >> 16);
                else
                    *samp32++ = LE_INT(ires);
            }
            else
            {
                if (bits == 16)
                    *samp16++ = 0;
                else
                    *samp32++ = 0;
            }
        }
    }
    return frames;
}

/**
 * DecodeAudio
 * Decode an audio packet, and compact it if data is planar
 * Return negative error code if an error occurred during decoding
 * or the number of bytes consumed from the input AVPacket
 * data_size contains the size of decoded data copied into buffer
 */
int AudioOutputUtil::DecodeAudio(AVCodecContext *ctx,
                                 uint8_t *buffer, int &data_size,
                                 const AVPacket *pkt)
{
    MythAVFrame frame;
    bool got_frame = false;
    char error[AV_ERROR_MAX_STRING_SIZE];

    data_size = 0;
    if (!frame)
    {
        return AVERROR(ENOMEM);
    }

//  SUGGESTION
//  Now that avcodec_decode_audio4 is deprecated and replaced
//  by 2 calls (receive frame and send packet), this could be optimized
//  into separate routines or separate threads.
//  Also now that it always consumes a whole buffer some code
//  in the caller may be able to be optimized.
    int ret = avcodec_receive_frame(ctx,frame);
    if (ret == 0)
        got_frame = true;
    if (ret == AVERROR(EAGAIN))
        ret = 0;
    if (ret == 0)
        ret = avcodec_send_packet(ctx, pkt);
    if (ret == AVERROR(EAGAIN))
        ret = 0;
    else if (ret < 0)
    {
        LOG(VB_AUDIO, LOG_ERR, LOC +
            QString("audio decode error: %1 (%2)")
            .arg(av_make_error_string(error, sizeof(error), ret))
            .arg(got_frame));
        return ret;
    }
    else
        ret = pkt->size;

    if (!got_frame)
    {
        LOG(VB_AUDIO, LOG_DEBUG, LOC +
            QString("audio decode, no frame decoded (%1)").arg(ret));
        return ret;
    }

    auto format = (AVSampleFormat)frame->format;

    data_size = frame->nb_samples * frame->channels * av_get_bytes_per_sample(format);

    if (av_sample_fmt_is_planar(format))
    {
        InterleaveSamples(AudioOutputSettings::AVSampleFormatToFormat(format, ctx->bits_per_raw_sample),
                          frame->channels, buffer, (const uint8_t **)frame->extended_data,
                          data_size);
    }
    else
    {
        // data is already compacted... simply copy it
        memcpy(buffer, frame->extended_data[0], data_size);
    }

    return ret;
}

/**
 * Deinterleave input samples
 * Deinterleave audio samples and compact them
 */
void AudioOutputUtil::DeinterleaveSamples(AudioFormat format, int channels,
                                          uint8_t *output, const uint8_t *input,
                                          int data_size)
{
    AudioConvert::DeinterleaveSamples(format, channels, output, input, data_size);
}

/**
 * Interleave input samples
 * Planar audio is contained in array of pointers
 * Interleave audio samples (convert from planar format)
 */
void AudioOutputUtil::InterleaveSamples(AudioFormat format, int channels,
                                        uint8_t *output, const uint8_t * const *input,
                                        int data_size)
{
    AudioConvert::InterleaveSamples(format, channels, output, input, data_size);
}

/**
 * Interleave input samples
 * Interleave audio samples (convert from planar format)
 */
void AudioOutputUtil::InterleaveSamples(AudioFormat format, int channels,
                                        uint8_t *output, const uint8_t *input,
                                        int data_size)
{
    AudioConvert::InterleaveSamples(format, channels, output, input, data_size);
}
