#include "audiooutputgraph.h"

#include <climits>
#include <cmath>
#include <cstdint>

#include <QtGlobal>
#include <QImage>
#include <QByteArray>
#include <QPair>
#include <QMutexLocker>

#include "mythlogging.h"
#include "mythpainter.h"
#include "mythimage.h"
#include "compat.h"

#define LOC QString("AOG::%1").arg(__func__)

const int kBufferMilliSecs = 500;

/*
 * Audio data buffer
 */
class AudioOutputGraph::Buffer : public QByteArray
{
public:
    Buffer() = default;

    // Properties
    void SetMaxSamples(unsigned samples) { m_maxSamples = samples; }
    void SetSampleRate(unsigned sample_rate) { m_sampleRate = sample_rate; }

    static inline int BitsPerChannel() { return sizeof(short) * CHAR_BIT; }
    inline int Channels() const { return m_channels; }

    inline int64_t Next() const { return m_tcNext; }
    inline int64_t First() const { return m_tcFirst; }

    using range_t = QPair<int64_t, int64_t>;
    range_t Avail(int64_t timecode) const
    {
        if (timecode == 0 || timecode == -1)
            timecode = m_tcNext;

        int64_t tc1 = timecode - Samples2MS(m_maxSamples / 2);
        if (tc1 < (int64_t)m_tcFirst)
            tc1 = m_tcFirst;

        int64_t tc2 = tc1 + Samples2MS(m_maxSamples);
        if (tc2 > (int64_t)m_tcNext)
        {
            tc2 = m_tcNext;
            if (tc2 < tc1 + (int64_t)Samples2MS(m_maxSamples))
            {
                tc1 = tc2 - Samples2MS(m_maxSamples);
                if (tc1 < (int64_t)m_tcFirst)
                    tc1 = m_tcFirst;
            }
        }
        return {tc1, tc2};
    }

    int Samples(const range_t &avail) const
    {
        return MS2Samples(avail.second - avail.first);
    }

    // Operations
    void Empty()
    {
        m_tcFirst = m_tcNext = 0;
        m_bits = m_channels = 0;
        resize(0);
    }

    void Append(const void *b, unsigned long len, unsigned long timecode, int channels, int bits)
    {
        if (m_bits != bits || m_channels != channels)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("(%1, %2 channels, %3 bits)")
                .arg(timecode).arg(channels).arg(bits));

            Resize(channels, bits);
            m_tcNext = m_tcFirst = timecode;
        }

        unsigned samples = Bytes2Samples(len);
        uint64_t tcNext = timecode + Samples2MS(samples);

        if (qAbs(timecode - m_tcNext) <= 1)
        {
            Append(b, len, bits);
            m_tcNext = tcNext;
        }
        else if (timecode >= m_tcFirst && tcNext <= m_tcNext)
        {
            // Duplicate
            return;
        }
        else
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString(" discontinuity %1 -> %2")
                .arg(m_tcNext).arg(timecode));

            Resize(channels, bits);
            Append(b, len, bits);
            m_tcFirst = timecode;
            m_tcNext = tcNext;
        }

        int overflow = size() - m_sizeMax;
        if (overflow > 0)
        {
            remove(0, overflow);
            m_tcFirst = m_tcNext - Samples2MS(Bytes2Samples(m_sizeMax));
        }
    }

    const int16_t* Data16(const range_t &avail) const
    {
        unsigned start = MS2Samples(avail.first - m_tcFirst);
        return reinterpret_cast< const int16_t* >(constData() + start * BytesPerSample());
    }

protected:
    inline unsigned BytesPerSample() const
    {
        return m_channels * ((m_bits + 7) / 8);
    }

    inline unsigned Bytes2Samples(unsigned bytes) const
    {
        return  (m_channels && m_bits) ? bytes / BytesPerSample() : 0;
    }

    inline unsigned long Samples2MS(unsigned samples) const
    {
        return m_sampleRate ? (samples * 1000UL + m_sampleRate - 1) / m_sampleRate : 0; // round up
    }

    inline unsigned MS2Samples(int64_t ms) const
    {
        return ms > 0 ? (ms * m_sampleRate) / 1000 : 0; // NB round down
    }

    void Append(const void *b, unsigned long len, int bits)
    {
        switch (bits)
        {
          case 8:
            // 8bit unsigned to 16bit signed
            {
                unsigned long cnt = len;
                int n = size();
                resize(n + sizeof(int16_t) * cnt);
                const auto *s = reinterpret_cast< const uchar* >(b);
                auto *p = reinterpret_cast< int16_t* >(data() + n);
                while (cnt--)
                    *p++ = (int16_t(*s++) - CHAR_MAX) << (16 - CHAR_BIT);
            }
            break;

          case 16:
            append( reinterpret_cast< const char* >(b), len);
            break;

          case 32:
            // 32bit float to 16bit signed
            {
                unsigned long cnt = len / sizeof(float);
                int n = size();
                resize(n + sizeof(int16_t) * cnt);
                const float f((1 << 15) - 1);
                const auto *s = reinterpret_cast< const float* >(b);
                auto *p = reinterpret_cast< int16_t* >(data() + n);
                while (cnt--)
                    *p++ = int16_t(f * *s++);
            }
            break;

          default:
            append( reinterpret_cast< const char* >(b), len);
            break;
        }
    }

private:
    void Resize(int channels, int bits)
    {
        m_bits = bits;
        m_channels = channels;
        m_sizeMax = ((m_sampleRate * kBufferMilliSecs) / 1000) * BytesPerSample();
        resize(0);
    }

private:
    unsigned m_maxSamples {0};
    unsigned m_sampleRate {44100};
    unsigned long m_tcFirst {0}, m_tcNext {0};
    int m_bits {0};
    int m_channels {0};
    int m_sizeMax {0};
};


/*
 * Audio graphic
 */
AudioOutputGraph::AudioOutputGraph() :
    m_buffer(new AudioOutputGraph::Buffer())
{ }

AudioOutputGraph::~AudioOutputGraph()
{
    delete m_buffer;
}

void AudioOutputGraph::SetPainter(MythPainter* painter)
{
    QMutexLocker lock(&m_mutex);
    m_painter = painter;
}

void AudioOutputGraph::SetSampleRate(unsigned sample_rate)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("(%1)")
        .arg(sample_rate));

    QMutexLocker lock(&m_mutex);
    m_buffer->SetSampleRate(sample_rate);
}

void AudioOutputGraph::SetSampleCount(unsigned sample_count)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("(%1)")
        .arg(sample_count));

    QMutexLocker lock(&m_mutex);
    m_buffer->SetMaxSamples(sample_count);
}

void AudioOutputGraph::prepare()
{
}

void AudioOutputGraph::add(const void *buf, unsigned long len, unsigned long timecode, int channels, int bits)
{
    QMutexLocker lock(&m_mutex);
    m_buffer->Append(buf, len, timecode, channels, bits);
}

void AudioOutputGraph::Reset()
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC);

    QMutexLocker lock(&m_mutex);
    m_buffer->Empty();
}

MythImage *AudioOutputGraph::GetImage(int64_t timecode) const
{
    QMutexLocker lock(&m_mutex);
    Buffer::range_t avail = m_buffer->Avail(timecode);

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("(%1) using [%2..%3] avail [%4..%5]")
        .arg(timecode).arg(avail.first).arg(avail.second)
        .arg(m_buffer->First()).arg(m_buffer->Next()) );

    int width = m_buffer->Samples(avail);
    if (width <= 0)
        return nullptr;

    const unsigned range = 1U << AudioOutputGraph::Buffer::BitsPerChannel();
    const double threshold = 20 * log10(1.0 / range); // 16bit=-96.3296dB => ~6dB/bit
    const int height = (int)-ceil(threshold); // 96
    if (height <= 0)
        return nullptr;

    const int channels = m_buffer->Channels();

    // Assume signed 16 bit/sample
    const auto * p   = m_buffer->Data16(avail);
    const auto * max = reinterpret_cast<const int16_t*>(m_buffer->constData() + m_buffer->size());
    if (p >= max)
        return nullptr;

    if ((p + (channels * width)) >= max)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + " Buffer overflow. Clipping samples.");
        width = static_cast<int>(max - p) / channels;
    }

    QImage image(width, height, QImage::Format_ARGB32);
    image.fill(0);

    for (int x = 0; x < width; ++x)
    {
        int left = p[0];
        int right = channels > 1 ? p[1] : left;
        p += channels;

        unsigned avg = qAbs(left) + qAbs(right);
        double db = 20 * log10( (double)(avg ? avg : 1) / range);

        int idb = (int)ceil(db);
        QRgb rgb = idb <= m_dBsilence ? qRgb(255, 255, 255)
                 : idb <= m_dBquiet   ? qRgb(  0, 255, 255)
                 : idb <= m_dBLoud    ? qRgb(  0, 255,   0)
                 : idb <= m_dbMax     ? qRgb(255, 255,   0)
                 :                      qRgb(255,   0,   0);

        int v = height - (int)(height * (db / threshold));
        if (v >= height)
            v = height - 1;
        else if (v < 0)
            v = 0;

        for (int y = 0; y <= v; ++y)
            image.setPixel(x, height - 1 - y, rgb);
    }

    auto *mi = new MythImage(m_painter);
    mi->Assign(image);
    return mi;
}
