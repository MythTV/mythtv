
#include "audioreencodebuffer.h"

extern "C" {
#include "libavutil/mem.h"
}


AudioBuffer::AudioBuffer()
  : m_frames(0), m_time(-1)
{
    m_size      = 0;
    m_buffer    = (uint8_t *)av_malloc(ABLOCK_SIZE);
    if (m_buffer == NULL)
    {
        throw std::bad_alloc();
    }
    m_realsize  = ABLOCK_SIZE;
}

AudioBuffer::AudioBuffer(const AudioBuffer &old)
  : m_size(old.m_size),     m_realsize(old.m_realsize),
    m_frames(old.m_frames), m_time(old.m_time)
{
    m_buffer = (uint8_t *)av_malloc(m_realsize);
    if (m_buffer == NULL)
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
                             long long time)
{
    if ((m_size + len) > m_realsize)
    {
        // buffer is too small to fit all
        // can't use av_realloc as it doesn't guarantee reallocated memory
        // to be 16 bytes aligned
        m_realsize = ((m_size + len) / ABLOCK_SIZE + 1 ) * ABLOCK_SIZE;
        uint8_t *tmp = (uint8_t *)av_malloc(m_realsize);
        if (tmp == NULL)
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
  : m_last_audiotime(0),        m_audioFrameSize(0),
    m_initpassthru(passthru),   m_saveBuffer(NULL)
{
    Reset();
    const AudioSettings settings(audio_format, audio_channels, 0, 0, false);
    Reconfigure(settings);
}

AudioReencodeBuffer::~AudioReencodeBuffer()
{
    Reset();
    if (m_saveBuffer)
        delete m_saveBuffer;
}

/**
 * reconfigure sound out for new params
 */
void AudioReencodeBuffer::Reconfigure(const AudioSettings &settings)
{
    ClearError();

    m_passthru        = settings.use_passthru;
    m_channels        = settings.channels;
    m_bytes_per_frame = m_channels *
        AudioOutputSettings::SampleSize(settings.format);
    m_eff_audiorate   = settings.samplerate;
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
    foreach (AudioBuffer *ab, m_bufferList)
    {
        delete ab;
    }
    m_bufferList.clear();
}

/**
 * \param timecode is in milliseconds.
 */
bool AudioReencodeBuffer::AddFrames(void *buffer, int frames, int64_t timecode)
{
    return AddData(buffer, frames * m_bytes_per_frame, timecode, frames);
}

/**
  * \param timecode is in milliseconds.
  */
bool AudioReencodeBuffer::AddData(void *buffer, int len, int64_t timecode,
                                  int frames)
{
    unsigned char *buf = (unsigned char *)buffer;

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
            int part = min(len - index, m_audioFrameSize - bufsize);
            int out_frames = part / m_bytes_per_frame;
            timecode += out_frames * 1000 / m_eff_audiorate;

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
                m_saveBuffer = NULL;
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
        timecode += frames * 1000 / m_eff_audiorate;
        m_saveBuffer->appendData(buf, len, frames, timecode);

        QMutexLocker locker(&m_bufferMutex);
        m_bufferList.append(m_saveBuffer);
        m_saveBuffer = NULL;
        m_last_audiotime = timecode;
    }

    return true;
}

AudioBuffer *AudioReencodeBuffer::GetData(long long time)
{
    QMutexLocker locker(&m_bufferMutex);

    if (m_bufferList.isEmpty())
        return NULL;

    AudioBuffer *ab = m_bufferList.front();

    if (ab->m_time <= time)
    {
        m_bufferList.pop_front();
        return ab;
    }

    return NULL;
}

long long AudioReencodeBuffer::GetSamples(long long time)
{
    QMutexLocker locker(&m_bufferMutex);

    if (m_bufferList.isEmpty())
        return 0;

    long long samples = 0;
    for (QList<AudioBuffer *>::iterator it = m_bufferList.begin();
         it != m_bufferList.end(); ++it)
    {
        AudioBuffer *ab = *it;

        if (ab->m_time <= time)
            samples += ab->m_frames;
        else
            break;
    }
    return samples;
}

void AudioReencodeBuffer::SetTimecode(int64_t timecode)
{
    m_last_audiotime = timecode;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */

