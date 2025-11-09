// Std
#include <climits>
#include <cmath>
#include <cstdint>
#include <thread>

// Qt
#include <QtGlobal>
#include <QImage>
#include <QByteArray>
#include <QPair>
#include <QMutexLocker>

// MythTV
#include "libmythbase/compat.h"
#include "libmythbase/mythlogging.h"
#include "libmythui/mythimage.h"
#include "libmythui/mythpainter.h"

#include "audiooutputgraph.h"

using namespace std::chrono_literals;

#define LOC QString("AOG: ")

const int kBufferMilliSecs = 500;

class AudioOutputGraph::AOBuffer : public QByteArray
{
  public:
    AOBuffer() = default;

    void SetMaxSamples(uint16_t Samples)    { m_maxSamples = Samples; }
    void SetSampleRate(uint16_t SampleRate) { m_sampleRate = SampleRate; }
    static int BitsPerChannel() { return sizeof(short) * CHAR_BIT; }
    int Channels() const { return m_channels; }
    std::chrono::milliseconds Next() const  { return m_tcNext; }
    std::chrono::milliseconds First() const { return m_tcFirst; }

    using Range = std::pair<std::chrono::milliseconds, std::chrono::milliseconds>;

    // Determine the range of sound samples to use, from first to second, centered
    // around the requested timecode. The range is clipped against the first and
    // last timecode of all the sound samples that are available.
    // It is possible that requested Timecode is not in the resulting range.
    Range Avail(std::chrono::milliseconds Timecode) const
    {
        if (Timecode == 0ms || Timecode == -1ms)
            Timecode = m_tcNext;

        std::chrono::milliseconds first = Timecode - Samples2MS(m_maxSamples / 2);
        if (first < m_tcFirst)
            first = m_tcFirst;

        std::chrono::milliseconds second = first + Samples2MS(m_maxSamples);
        if (second > m_tcNext)
        {
            second = m_tcNext;
            if (second < first + Samples2MS(m_maxSamples))
            {
                first = second - Samples2MS(m_maxSamples);
                if (first < m_tcFirst)
                    first = m_tcFirst;
            }
        }
        return {first, second};
    }

    // All samples to be shown in the graph are present
    bool EnoughData(std::chrono::milliseconds Timecode)
    {
        // return m_tcNext >= Timecode + Samples2MS(m_maxSamples / 2);
        return (m_tcFirst <= Timecode) && (m_tcNext >= Timecode + Samples2MS(m_maxSamples / 2));
    }

    // Number of samples to be shown in the graph
    int Samples(Range Available) const
    {
        return MS2Samples(Available.second - Available.first);
    }

    // Reset all values and release sample buffer
    void Empty()
    {
        m_tcFirst  = 0ms;
        m_tcNext   = 0ms;
        m_bits     = 0;
        m_channels = 0;
        resize(0);
    }

    void Append(const void * Buffer, unsigned long Length, std::chrono::milliseconds Timecode,
                int Channels, int Bits)
    {
        if (m_bits != Bits || m_channels != Channels)
        {
            LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Length:%1 Timecode:%2 Channels:%3 Bits:%4")
                .arg(Length).arg(Timecode.count()).arg(Channels).arg(Bits));
            Resize(Channels, Bits);
            m_tcFirst = Timecode;
            m_tcNext  = Timecode;
        }

        auto samples = Bytes2Samples(static_cast<unsigned>(Length));
        std::chrono::milliseconds tcNext = Timecode + Samples2MS(samples);

        LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
            QString("m_tcFirst:%1 m_tcNext:%2 Timecode:%3 samples:%4 ms:%5")
                .arg(m_tcFirst.count()).arg(m_tcNext.count()).arg(Timecode.count())
                .arg(samples).arg(Samples2MS(samples).count()));

        if (qAbs((Timecode - m_tcNext).count()) <= 1)
        {
            Append(Buffer, Length, Bits);
            m_tcNext = tcNext;
        }
        else if (Timecode >= m_tcFirst && tcNext <= m_tcNext)
        {
            // Duplicate
            LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Duplicate"));
            return;
        }
        else
        {
            LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Discontinuity %1 -> %2")
                .arg(m_tcNext.count()).arg(Timecode.count()));

            Resize(Channels, Bits);
            Append(Buffer, Length, Bits);
            m_tcFirst = Timecode;
            m_tcNext = tcNext;
        }

        auto overflow = size() - m_sizeMax;

        LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Overflow:%1 size:%2 m_sizeMax:%3")
                .arg(overflow).arg(size()).arg(m_sizeMax));

        if (overflow > 0)
        {
            remove(0, overflow);

            auto old_m_tcFirst = m_tcFirst;
            m_tcFirst = m_tcNext - Samples2MS(Bytes2SamplesNormalized(static_cast<unsigned>(m_sizeMax)));
            LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
                QString("Overflow:%1 m_tcFirst old, new, delta:%2 %3 %4")
                    .arg(overflow).arg(old_m_tcFirst.count()).arg(m_tcFirst.count())
                    .arg((m_tcFirst - old_m_tcFirst).count()));
        }
    }

    // Pointer to first sample to be shown in the graph
    const int16_t* Data16(Range Available) const
    {
        auto start = MS2Samples(Available.first - m_tcFirst);
        return reinterpret_cast<const int16_t*>(constData() + (static_cast<ptrdiff_t>(start) * BytesPerSampleNormalized()));
    }

  protected:

    // Bytes per sample as received; one value for one channel can be 8, 16 or 32 bits.
    uint BytesPerSample() const
    {
        return static_cast<uint>(m_channels * ((m_bits + 7) / 8));
    }

    unsigned Bytes2Samples(unsigned Bytes) const
    {
        return  (m_channels && m_bits) ? Bytes / BytesPerSample() : 0;
    }

    // Bytes per sample as stored; one value for one channel is always 16 bits
    uint BytesPerSampleNormalized() const
    {
        return static_cast<uint>(m_channels * sizeof(int16_t));
    }

    unsigned Bytes2SamplesNormalized(unsigned Bytes) const
    {
        return  (m_channels && m_bits) ? Bytes / BytesPerSampleNormalized() : 0;
    }

    std::chrono::milliseconds Samples2MS(unsigned Samples) const
    {
        return m_sampleRate ? std::chrono::milliseconds((Samples * 1000UL + m_sampleRate - 1) / m_sampleRate) : 0ms; // round up
    }

    int MS2Samples(std::chrono::milliseconds Msecs) const
    {
        return Msecs > 0ms ? static_cast<int>((Msecs.count() * m_sampleRate) / 1000) : 0; // NB round down
    }

    // Store all received samples but convert the sample from 8, 16 or 32 bits to 16 bits.

    // All arithmetic that refers to samples stored inside (in the QByteArray that we inherit from)
    // must be done with the ..Normalized functions which use the fixed size of 16 bits.
    // Arithmetic on the incoming data must use the number of bits per sample (8, 16 or 32).
    void Append(const void * Buffer, unsigned long Length, int Bits)
    {
        switch (Bits)
        {
          case 8:
            // Convert 8bit unsigned to 16bit signed
            {
                auto count = Length;
                auto n = size();
                resize(n + static_cast<int>(sizeof(int16_t) * count));
                const auto * src = reinterpret_cast<const uchar*>(Buffer);
                auto * dst = reinterpret_cast<int16_t*>(data() + n);
                while (count--)
                    *dst++ = static_cast<int16_t>((static_cast<int16_t>(*src++) - CHAR_MAX) << (16 - CHAR_BIT));
            }
            break;
          case 16:
            // Copy 16bit samples without conversion
            append(reinterpret_cast<const char*>(Buffer), static_cast<int>(Length));
            break;
          case 32:
            // Convert 32bit float to 16bit signed
            {
                unsigned long count = Length / sizeof(float);
                auto n = size();
                resize(n + static_cast<int>(sizeof(int16_t) * count));
                const float f((1 << 15) - 1);
                const auto * src = reinterpret_cast<const float*>(Buffer);
                auto * dst = reinterpret_cast<int16_t*>(data() + n);
                while (count--)
                    *dst++ = static_cast<int16_t>(f * (*src++));
            }
            break;
          default:
            append(reinterpret_cast<const char*>(Buffer), static_cast<int>(Length));
            break;
        }
    }

  private:
    void Resize(int Channels, int Bits)
    {
        m_bits = Bits;
        m_channels = Channels;
        m_sizeMax = static_cast<int>(((m_sampleRate * kBufferMilliSecs) / 1000) * BytesPerSampleNormalized());
        resize(0);
    }

  private:
    std::chrono::milliseconds m_tcFirst { 0ms };
    std::chrono::milliseconds m_tcNext  { 0ms };
    uint16_t m_maxSamples { 0 };
    uint16_t m_sampleRate { 44100 };
    int      m_bits       { 0 };
    int      m_channels   { 0 };
    int      m_sizeMax    { 0 };
};

AudioOutputGraph::AudioOutputGraph() :
    m_buffer(new AudioOutputGraph::AOBuffer())
{
}

AudioOutputGraph::~AudioOutputGraph()
{
    delete m_buffer;
    if (m_image)
        m_image->DecrRef();
}

void AudioOutputGraph::SetPainter(MythPainter* Painter)
{
    QMutexLocker lock(&m_mutex);
    m_painter = Painter;
    m_image = new MythImage(m_painter);
}

void AudioOutputGraph::SetSampleRate(uint16_t SampleRate)
{
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Set sample rate %1").arg(SampleRate));
    QMutexLocker lock(&m_mutex);
    m_buffer->SetSampleRate(SampleRate);
}

void AudioOutputGraph::SetSampleCount(uint16_t SampleCount)
{
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Set sample count %1").arg(SampleCount));
    QMutexLocker lock(&m_mutex);
    m_buffer->SetMaxSamples(SampleCount);
}

void AudioOutputGraph::prepare()
{
}

void AudioOutputGraph::add(const void * Buffer, unsigned long Length,
                           std::chrono::milliseconds Timecode, int Channnels, int Bits)
{
    QMutexLocker lock(&m_mutex);
    m_buffer->Append(Buffer, Length, Timecode, Channnels, Bits);
}

void AudioOutputGraph::Reset()
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Reset");
    QMutexLocker lock(&m_mutex);
    m_buffer->Empty();
}

MythImage* AudioOutputGraph::GetImage(std::chrono::milliseconds Timecode) const
{
    // Needed to erase a previous image if there is no audio data available.
    auto EmptyImage = [=, this]() {
        QImage image(8, 8, QImage::Format_ARGB32);
        image.fill(0);
        if (m_image)
            m_image->Assign(image);
        return m_image;
    };

    QMutexLocker lock(&m_mutex);
    AOBuffer::Range avail = m_buffer->Avail(Timecode);

    // Wait until all audio data has arrived for our requested timecode.
    bool data_present = false;
    for (int i=0; i<10 && !data_present; i++)
    {
        if (m_buffer->EnoughData(Timecode))
        {
            LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
                QString("Audio data for timecode %1 arrived after %2 wait")
                    .arg(Timecode.count()).arg(i));
            data_present = true;
        }
        else
        {
            lock.unlock();
            std::this_thread::sleep_for(20ms);
            avail = m_buffer->Avail(Timecode);
            lock.relock();
        }
    }

    if (!data_present)
    {
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
            QString("No audio data for timecode %1")
                .arg(Timecode.count()));
        return EmptyImage();
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("GetImage for timecode %1 using [%2..%3] available [%4..%5]")
        .arg(Timecode.count()).arg(avail.first.count()).arg(avail.second.count())
        .arg(m_buffer->First().count()).arg(m_buffer->Next().count()) );

    auto width = m_buffer->Samples(avail);
    if (width <= 0)
    {
        LOG(VB_PLAYBACK, LOG_WARNING, LOC +
            QString("GetImage for timecode %1 SKIPPED because width <= 0").arg(Timecode.count()));
        return EmptyImage();
    }

    const unsigned range = 1U << AudioOutputGraph::AOBuffer::BitsPerChannel();
    const auto threshold = 20 * log10(1.0 / range); // 16bit=-96.3296dB => ~6dB/bit
    auto height = static_cast<int>(-ceil(threshold)); // 96
    if (height <= 0)
    {
        LOG(VB_PLAYBACK, LOG_WARNING, LOC +
            QString("GetImage for timecode %1 SKIPPED because height <= 0").arg(Timecode.count()));
        return EmptyImage();
    }

    const int channels = m_buffer->Channels();

    // Start position of audio data has to be before max (the end of the buffer)
    const auto * data   = m_buffer->Data16(avail);
    const auto * max = reinterpret_cast<const int16_t*>(m_buffer->constData() + m_buffer->size());
    if (data >= max)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC +
            QString("GetImage for timecode %1 SKIPPED because data starts beyond end of buffer")
                .arg(Timecode.count()));
        return EmptyImage();
    }

    // End position of audio data cannot extend beyond max (the end of the buffer)
    if ((data + (channels * static_cast<ptrdiff_t>(width))) > max)
    {
        LOG(VB_PLAYBACK, LOG_WARNING, LOC + "Buffer overflow. Clipping samples.");
        width = static_cast<int>(max - data) / channels;
    }

    QImage image(width, height, QImage::Format_ARGB32);
    image.fill(0);

    for (int x = 0; x < width; ++x)
    {
        auto left = data[0];
        auto right = channels > 1 ? data[1] : left;
        data += channels;

        auto avg = qAbs(left) + qAbs(right);
        double db = 20 * log10(static_cast<double>(avg ? avg : 1) / range);
        auto idb = static_cast<int>(ceil(db));
        auto rgb { qRgb(255,   0,   0) };
        if (idb <= m_dBsilence)
            rgb = qRgb(255, 255, 255);
        else if (idb <= m_dBquiet)
            rgb = qRgb(  0, 255, 255);
        else if (idb <= m_dBLoud)
            rgb = qRgb(  0, 255,   0);
        else if (idb <= m_dbMax)
            rgb = qRgb(255, 255,   0);

        int v = height - static_cast<int>(height * (db / threshold));
        if (v >= height)
            v = height - 1;
        for (int y = 0; y <= v; ++y)
            image.setPixel(x, height - 1 - y, rgb);
    }

    if (m_image)
        m_image->Assign(image);
    return m_image;
}
