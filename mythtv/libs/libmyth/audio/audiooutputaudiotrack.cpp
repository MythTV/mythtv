#include <QtGlobal>
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
#include <QAndroidJniObject>
#include <QAndroidJniEnvironment>
#else
#include <QJniEnvironment>
#include <QJniObject>
#define QAndroidJniEnvironment QJniEnvironment
#define QAndroidJniObject QJniObject
#endif
#include <android/log.h>

#include "libmythbase/mythlogging.h"
#include "audiooutputaudiotrack.h"

#define CHANNELS_MIN 1
#define CHANNELS_MAX 8

#define ANDROID_EXCEPTION_CHECK \
  if (env->ExceptionCheck()) { \
    env->ExceptionDescribe(); \
    env->ExceptionClear(); \
    exception=true; \
  } else \
    exception=false;
// clear exception without checking
#define ANDROID_EXCEPTION_CLEAR \
  if (env->ExceptionCheck()) { \
    env->ExceptionDescribe(); \
    env->ExceptionClear(); \
  }

#define LOC QString("AudioTrack: ")

// Constants from Android Java API
// class android.media.AudioFormat
#define AF_CHANNEL_OUT_MONO 4
#define AF_ENCODING_AC3 5
#define AF_ENCODING_E_AC3 6
#define AF_ENCODING_DTS 7
#define AF_ENCODING_DOLBY_TRUEHD 14
#define AF_ENCODING_PCM_8BIT 3
#define AF_ENCODING_PCM_16BIT 2
#define AF_ENCODING_PCM_FLOAT 4

// for debugging
#include <android/log.h>

AudioOutputAudioTrack::AudioOutputAudioTrack(const AudioSettings &settings) :
    AudioOutputBase(settings)
{
    InitSettings(settings);
    if (settings.m_init)
        Reconfigure(settings);
}

AudioOutputAudioTrack::~AudioOutputAudioTrack()
{
    KillAudio();
}

bool AudioOutputAudioTrack::OpenDevice()
{
    bool exception=false;
    QAndroidJniEnvironment env;
    jint encoding = 0;
    jint sampleRate = m_sampleRate;

    // m_bitsPer10Frames = output bits per 10 frames
    m_bitsPer10Frames = m_outputBytesPerFrame * 80;

    if ((m_passthru || m_enc) && m_sourceBitRate > 0)
        m_bitsPer10Frames = m_sourceBitRate * 10 / m_sourceSampleRate;

    // 50 milliseconds
    m_fragmentSize = m_bitsPer10Frames * m_sourceSampleRate * 5 / 8000;

    if (m_fragmentSize < 1536)
        m_fragmentSize = 1536;


    if (m_passthru || m_enc)
    {
        switch (m_codec)
        {
            case AV_CODEC_ID_AC3:
                encoding = AF_ENCODING_AC3;
                break;
            case AV_CODEC_ID_DTS:
                encoding = AF_ENCODING_DTS;
                break;
            case AV_CODEC_ID_EAC3:
                encoding = AF_ENCODING_E_AC3;
                break;
            case AV_CODEC_ID_TRUEHD:
                encoding = AF_ENCODING_DOLBY_TRUEHD;
                break;

            default:
                LOG(VB_GENERAL, LOG_ERR, LOC + __func__ + QString(" No support for audio passthru encoding %1").arg(m_codec));
                return false;
        }
    }
    else
    {
        switch (m_outputFormat)
        {
            case FORMAT_U8:
                // This could be used to get the value from java instead         // of haning these constants in pour header file.
                // encoding = QAndroidJniObject::getStaticField<jint>
                //   ("android.media.AudioFormat","ENCODING_PCM_8BIT");
                encoding = AF_ENCODING_PCM_8BIT;
                break;
            case FORMAT_S16:
                encoding = AF_ENCODING_PCM_16BIT;
                break;
            case FORMAT_FLT:
                encoding = AF_ENCODING_PCM_FLOAT;
                break;
            default:
                LOG(VB_GENERAL, LOG_ERR, LOC + __func__ + QString(" No support for audio format %1").arg(m_outputFormat));
                return false;
        }
    }

    jint minBufferSize = m_fragmentSize * 4;
    m_soundcardBufferSize = minBufferSize;
    jint channels = m_channels;

    m_audioTrack = new QAndroidJniObject("org/mythtv/audio/AudioOutputAudioTrack",
        "(IIII)V", encoding, sampleRate, minBufferSize, channels);
    ANDROID_EXCEPTION_CHECK

    if (exception)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + __func__ + QString(" Java Exception when creating AudioTrack"));
        m_audioTrack = nullptr;
        return false;
    }
    if (!m_passthru && !m_enc)
    {
        jint bitsPer10Frames = m_bitsPer10Frames;
        m_audioTrack->callMethod<void>("setBitsPer10Frames","(I)V",bitsPer10Frames);
    }
    return true;
}

void AudioOutputAudioTrack::CloseDevice()
{
    QAndroidJniEnvironment env;
    if (m_audioTrack)
    {
        m_audioTrack->callMethod<void>("release");
        ANDROID_EXCEPTION_CLEAR
        delete m_audioTrack;
        m_audioTrack = nullptr;
    }
}

AudioOutputSettings* AudioOutputAudioTrack::GetOutputSettings(bool /* digital */)
{
    bool exception=false;
    QAndroidJniEnvironment env;
    jint bufsize = 0;

    AudioOutputSettings *settings = new AudioOutputSettings();

    int supportedrate = 0;
    while (int rate = settings->GetNextRate())
    {
        // Checking for valid rates using getMinBufferSize.
        // See https://stackoverflow.com/questions/8043387/android-audiorecord-supported-sampling-rates/22317382
        bufsize = QAndroidJniObject::callStaticMethod<jint>
            ("android/media/AudioTrack", "getMinBufferSize", "(III)I",
             rate, AF_CHANNEL_OUT_MONO, AF_ENCODING_PCM_16BIT);
        ANDROID_EXCEPTION_CHECK
        if (bufsize > 0 && !exception)
        {
            settings->AddSupportedRate(rate);
            // save any supported rate for later
            supportedrate = rate;
        }
    }

    // Checking for valid format using getMinBufferSize.
    bufsize = QAndroidJniObject::callStaticMethod<jint>
        ("android/media/AudioTrack", "getMinBufferSize", "(III)I",
            supportedrate, AF_CHANNEL_OUT_MONO, AF_ENCODING_PCM_8BIT);
    ANDROID_EXCEPTION_CHECK
    if (bufsize > 0 && !exception)
        settings->AddSupportedFormat(FORMAT_U8);
    // 16bit always supported
    settings->AddSupportedFormat(FORMAT_S16);

    bufsize = QAndroidJniObject::callStaticMethod<jint>
        ("android/media/AudioTrack", "getMinBufferSize", "(III)I",
            supportedrate, AF_CHANNEL_OUT_MONO, AF_ENCODING_PCM_FLOAT);
    ANDROID_EXCEPTION_CHECK
    if (bufsize > 0 && !exception)
    settings->AddSupportedFormat(FORMAT_FLT);

    for (uint channels = CHANNELS_MIN; channels <= CHANNELS_MAX; channels++)
    {
        settings->AddSupportedChannels(channels);
    }
    settings->setPassthrough(0);

    return settings;
}

void AudioOutputAudioTrack::WriteAudio(unsigned char* aubuf, int size)
{
    bool exception=false;
    QAndroidJniEnvironment env;
    if (m_actuallyPaused)
    {
        if (m_audioTrack)
        {
            jboolean param = true;
            m_audioTrack->callMethod<void>("pause","(Z)V",param);
            ANDROID_EXCEPTION_CLEAR
        }
        return;
    }
    // create a java byte array
    jbyteArray arr = env->NewByteArray(size);
    env->SetByteArrayRegion(arr, 0, size, reinterpret_cast<jbyte*>(aubuf));
    jint ret = -99;
    if (m_audioTrack)
    {
        ret = m_audioTrack->callMethod<jint>("write","([BI)I", arr, size);
        ANDROID_EXCEPTION_CHECK
    }
    env->DeleteLocalRef(arr);
    if (ret != size || exception)
        LOG(VB_GENERAL, LOG_ERR, LOC + __func__
          + QString(" Audio Write failed, size %1 return %2 exception %3")
          .arg(size).arg(ret).arg(exception));

    LOG(VB_AUDIO | VB_TIMESTAMP, LOG_INFO, LOC + __func__
        + QString(" WriteAudio size=%1 written=%2")
        .arg(size).arg(ret));
}


int AudioOutputAudioTrack::GetBufferedOnSoundcard(void) const
{
    QAndroidJniEnvironment env;
    int buffered (0);
    if (m_audioTrack)
    {
        // This may return a negative value, because there
        // is data already played that is still in the "Audio circular buffer"
        buffered
            = m_audioTrack->callMethod<jint>("getBufferedBytes");
        bool exception=false;
        ANDROID_EXCEPTION_CHECK
        if (exception)
            buffered = 0;
        int latency
            = m_audioTrack->callMethod<jint>("getLatencyViaHeadPosition");
        ANDROID_EXCEPTION_CHECK
        if (exception)
            latency = 0;
        buffered += latency * m_sampleRate / 1000 * m_bitsPer10Frames / 80 ;
    }

    return buffered;
}

bool AudioOutputAudioTrack::AddData(void *in_buffer, int in_len,
                              std::chrono::milliseconds timecode, int in_frames)
{
    bool ret = AudioOutputBase::AddData
        (in_buffer, in_len, timecode,in_frames);

    return ret;
}

void AudioOutputAudioTrack::Pause(bool paused)
{
    AudioOutputBase::Pause(paused);
    if (m_audioTrack)
    {
        jboolean param = paused;
        m_audioTrack->callMethod<void>("pause","(Z)V",param);
    }
}

void AudioOutputAudioTrack::SetSourceBitrate(int rate)
{
    AudioOutputBase::SetSourceBitrate(rate);
    if (m_sourceBitRate > 0
        && (m_passthru || m_enc)
        &&  m_audioTrack)
    {
        m_bitsPer10Frames = m_sourceBitRate * 10 / m_sourceSampleRate;
        jint bitsPer10Frames = m_bitsPer10Frames;
        m_audioTrack->callMethod<void>("setBitsPer10Frames","(I)V",bitsPer10Frames);
    }
}

bool AudioOutputAudioTrack::StartOutputThread(void)
{
    QAndroidJniEnvironment env;
    if (m_audioTrack)
    {
        m_audioTrack->callMethod<void>("setOutputThread","(Z)V",true);
        ANDROID_EXCEPTION_CLEAR
    }

    return AudioOutputBase::StartOutputThread();
}

void AudioOutputAudioTrack::StopOutputThread(void)
{
    QAndroidJniEnvironment env;
    if (m_audioTrack)
    {
        m_audioTrack->callMethod<void>("setOutputThread","(Z)V",false);
        ANDROID_EXCEPTION_CLEAR
    }

    AudioOutputBase::StopOutputThread();
}
