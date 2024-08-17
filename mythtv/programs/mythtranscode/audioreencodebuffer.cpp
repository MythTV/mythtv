
#include "audioreencodebuffer.h"

#include <algorithm> // for min/max

extern "C" {
#include "libavutil/mem.h"
}


AudioBuffer::AudioBuffer()
  : m_buffer((uint8_t *)av_malloc(ABLOCK_SIZE))
{
    if (m_buffer == nullptr)
    {
        throw std::bad_alloc();
    }
}

AudioBuffer::AudioBuffer(const AudioBuffer &old)
  : m_size(old.m_size),     m_realsize(old.m_realsize),
    m_frames(old.m_frames), m_time(old.m_time)
{
    m_buffer = (uint8_t *)av_malloc(m_realsize);
    if (m_buffer == nullptr)
    {
        throw std::bad_alloc();
    }
    memcpy(m_buffer, old.m_buffer, m_size);
}

AudioBuffer::~AudioBuffer()
{
    av_free(m_buffer);
}

void AudioBuffer::appendData(unsigned char *buffer, int len, int frames,
                             std::chrono::milliseconds time)
{
    if ((m_size + len) > m_realsize)
    {
        // buffer is too small to fit all
        // can't use av_realloc as it doesn't guarantee reallocated memory
        // to be 16 bytes aligned
        m_realsize = ((m_size + len) / ABLOCK_SIZE + 1 ) * ABLOCK_SIZE;
        auto *tmp = (uint8_t *)av_malloc(m_realsize);
        if (tmp == nullptr)
        {
            throw std::bad_alloc();
        }
        memcpy(tmp, m_buffer, m_size);
        av_free(m_buffer);
        m_buffer = tmp;
    }

    memcpy(m_buffer + m_size, buffer, len);
    m_size   += len;
    m_frames += frames;
    m_time    = time;
}

//////////////////////////////////////////////////////////////////////////////

AudioReencodeBuffer::AudioReencodeBuffer(AudioFormat audio_format,
                                         int audio_channels, bool passthru)
  : m_initpassthru(passthru)
{
    AudioReencodeBuffer::Reset();
    const AudioSettings settings(audio_format, audio_channels, AV_CODEC_ID_NONE, 0, false);
    AudioReencodeBuffer::Reconfigure(settings);
}

AudioReencodeBuffer::~AudioReencodeBuffer()
{
    AudioReencodeBuffer::Reset();
    delete m_saveBuffer;
}

/**
 * reconfigure sound out for new params
 */
void AudioReencodeBuffer::Reconfigure(const AudioSettings &settings)
{
    ClearError();

    m_passthru        = settings.m_usePassthru;
    m_channels        = settings.m_channels;
    m_bytes_per_frame = m_channels *
        AudioOutputSettings::SampleSize(settings.m_format);
    m_eff_audiorate   = settings.m_sampleRate;
}

/**
 * \param dsprate is in 100 * frames/second
 */
void AudioReencodeBuffer::SetEffDsp(int dsprate)
{
    m_eff_audiorate = (dsprate / 100);
}

void AudioReencodeBuffer::Reset(void)
{
    QMutexLocker locker(&m_bufferMutex);
    for (AudioBuffer *ab : std::as_const(m_bufferList))
    {
        delete ab;
    }
    m_bufferList.clear();
}

/**
 * Add frames to the audiobuffer for playback
 *
 * \param[in] buffer pointer to audio data
 * \param[in] frames number of frames added.
 * \param[in] timecode timecode of the first sample added (in msec)
 *
 * \return false if there wasn't enough space in audio buffer to
 *     process all the data
 */
bool AudioReencodeBuffer::AddFrames(void *buffer, int frames, std::chrono::milliseconds timecode)
{
    return AddData(buffer, frames * m_bytes_per_frame, timecode, frames);
}

/**
 * Add data to the audiobuffer for playback
 *
 * \param[in] buffer pointer to audio data
 * \param[in] len length of audio data added
 * \param[in] timecode timecode of the first sample added (in msec)
 * \param[in] frames number of frames added.
 *
 * \return false if there wasn't enough space in audio buffer to
 *     process all the data
 */
bool AudioReencodeBuffer::AddData(void *buffer, int len, std::chrono::milliseconds timecode,
                                  int frames)
{
    auto *buf = (unsigned char *)buffer;

    // Test if target is using a fixed buffer size.
    if (m_audioFrameSize)
    {
        int index = 0;

        // Target has a fixed buffer size, which may not match len.
        // Redistribute the bytes into appropriately sized buffers.
        while (index < len)
        {
            // See if we have some saved from last iteration in
            // m_saveBuffer. If not create a new empty buffer.
            if (!m_saveBuffer)
                m_saveBuffer = new AudioBuffer();

            // Use as many of the remaining frames as will fit in the space
            // left in the buffer.
            int bufsize = m_saveBuffer->size();
            int part = std::min(len - index, m_audioFrameSize - bufsize);
            int out_frames = part / m_bytes_per_frame;
            timecode += std::chrono::milliseconds(out_frames * 1000 / m_eff_audiorate);

            // Store frames in buffer, basing frame count on number of
            // bytes, which works only for uncompressed data.
            m_saveBuffer->appendData(&buf[index], part,
                                     out_frames, timecode);

            // If we have filled the buffer...
            if (m_saveBuffer->size() == m_audioFrameSize)
            {
                QMutexLocker locker(&m_bufferMutex);

                // store the buffer
                m_bufferList.append(m_saveBuffer);
                // mark m_saveBuffer as emtpy.
                m_saveBuffer = nullptr;
                // m_last_audiotime is updated iff we store a buffer.
                m_last_audiotime = timecode;
            }

            index += part;
        }
    }
    else
    {
        // Target has no fixed buffer size. We can use a simpler algorithm
        // and use 'frames' directly rather than 'len / m_bytes_per_frame',
        // thus also covering the passthrough case.
        m_saveBuffer = new AudioBuffer();
        timecode += std::chrono::milliseconds(frames * 1000 / m_eff_audiorate);
        m_saveBuffer->appendData(buf, len, frames, timecode);

        QMutexLocker locker(&m_bufferMutex);
        m_bufferList.append(m_saveBuffer);
        m_saveBuffer = nullptr;
        m_last_audiotime = timecode;
    }

    return true;
}

AudioBuffer *AudioReencodeBuffer::GetData(std::chrono::milliseconds time)
{
    QMutexLocker locker(&m_bufferMutex);

    if (m_bufferList.isEmpty())
        return nullptr;

    AudioBuffer *ab = m_bufferList.front();

    if (ab->m_time <= time)
    {
        m_bufferList.pop_front();
        return ab;
    }

    return nullptr;
}

long long AudioReencodeBuffer::GetSamples(std::chrono::milliseconds time)
{
    QMutexLocker locker(&m_bufferMutex);

    if (m_bufferList.isEmpty())
        return 0;

    long long samples = 0;
    for (auto *ab : std::as_const(m_bufferList))
    {
        if (ab->m_time <= time)
            samples += ab->m_frames;
        else
            break;
    }
    return samples;
}

void AudioReencodeBuffer::SetTimecode(std::chrono::milliseconds timecode)
{
    m_last_audiotime = timecode;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */

