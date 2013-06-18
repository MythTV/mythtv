
#include "audioreencodebuffer.h"
#include "audioconvert.h"

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

/**
 * checkSize
 * Check that buffer can fit provided len, if not resize buffer
 * return true if resize was possible, false if not
 */
bool AudioBuffer::checkSize(int len)
{
    if (len > m_realsize)
    {
        // buffer is too small to fit all
        // can't use av_realloc as it doesn't guarantee reallocated memory
        // to be 16 bytes aligned
        m_realsize = ((m_size + len) / ABLOCK_SIZE + 1 ) * ABLOCK_SIZE;
        uint8_t *tmp = (uint8_t *)av_malloc(m_realsize);
        if (tmp == NULL)
        {
            return false;
        }
        memcpy(tmp, m_buffer, m_size);
        av_free(m_buffer);
        m_buffer = tmp;
    }
    return true;
}

void AudioBuffer::appendData(uint8_t *buffer, int len, int frames,
                             long long time)
{
    if (!checkSize(m_size+len))
    {
        throw std::bad_alloc();
    }
    memcpy(m_buffer + m_size, buffer, len);
    m_size   += len;
    m_frames += frames;
    m_time    = time;
}

//////////////////////////////////////////////////////////////////////////////

AudioReencodeBuffer::AudioReencodeBuffer(AudioFormat audio_format,
                                         int audio_channels, bool passthru)
  : m_last_audiotime(0),     m_passthru(false),        m_audioFrameSize(0),
    m_format(audio_format),  m_initpassthru(passthru), m_saveBuffer(NULL),
    m_convertBuffer(new AudioBuffer()),                m_audioConverter(NULL)
{
    Reset();
    const AudioSettings settings(audio_format, audio_channels, 0, 0, m_passthru);
    Reconfigure(settings);
}

AudioReencodeBuffer::~AudioReencodeBuffer()
{
    Reset();
    if (m_saveBuffer)
        delete m_saveBuffer;
    delete m_convertBuffer;
    m_convertBuffer = NULL;
    delete m_audioConverter;
    m_audioConverter = NULL;
}

// reconfigure sound out for new params
void AudioReencodeBuffer::Reconfigure(const AudioSettings &settings)
{
    ClearError();

    if (settings.format != m_format || m_passthru != settings.use_passthru)
    {
        delete m_audioConverter;
        if (!m_passthru)
        {
            // will convert all audio to S16 internally
            m_audioConverter = new AudioConvert(settings.format, FORMAT_S16);
        }
        else
        {
            m_audioConverter = NULL;
        }
    }
    m_passthru        = settings.use_passthru;
    m_channels        = settings.channels;
    m_bytes_per_frame = m_channels *
        AudioOutputSettings::SampleSize(settings.format);
    m_eff_audiorate   = settings.samplerate;
    m_format          = settings.format;
}

// dsprate is in 100 * frames/second
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

// timecode is in milliseconds.
bool AudioReencodeBuffer::AddFrames(void *buffer, int frames, int64_t timecode)
{
    return AddData(buffer, frames * m_bytes_per_frame, timecode, frames);
}

// timecode is in milliseconds.
bool AudioReencodeBuffer::AddData(void *buffer, int len, int64_t timecode,
                                  int frames)
{
    uint8_t *buf;

    if (!m_passthru)
    {
        if (!m_convertBuffer->checkSize(len))
        {
            // not enough space to process it
            return false;
        }
        if (!m_audioConverter)
        {
            // will never happen, but I'm guessing coverity will complain otherwise
            m_audioConverter = new AudioConvert(m_format, FORMAT_S16);
        }
        m_audioConverter->Process(m_convertBuffer->m_buffer, buffer, len);
        len = len * AudioOutputSettings::SampleSize(FORMAT_S16) / AudioOutputSettings::SampleSize(m_format);
        buf = m_convertBuffer->m_buffer;
    }
    else
    {
        buf = (uint8_t *)buffer;
    }

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
            int out_frames = part / m_channels / AudioOutputSettings::SampleSize(FORMAT_S16);
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

