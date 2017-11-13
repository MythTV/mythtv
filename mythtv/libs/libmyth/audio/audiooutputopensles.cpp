#include <iostream>

using namespace std;

#include <dlfcn.h>
#include <assert.h>

#include <QAndroidJniEnvironment>

#include "mythlogging.h"
#include "audiooutputopensles.h"


#define LOC QString("AOSLES: ")

#define OPENSLES_BUFFERS 10  /* maximum number of buffers */
#define OPENSLES_BUFLEN  10  /* ms */
//#define POSITIONUPDATEPERIOD 1000
//#define POSITIONUPDATEPERIOD 25000
#define POSITIONUPDATEPERIOD 40
//#define POSITIONUPDATEPERIOD 200000

#define CHECK_OPENSL_ERROR(msg)                \
    if (result != SL_RESULT_SUCCESS) \
    {                                          \
        VBERROR(QString("Open Error: (%1)").arg(result));    \
        Close();                            \
        return false;   \
    }

#define CHECK_OPENSL_START_ERROR(msg)                \
    if (result != SL_RESULT_SUCCESS) \
    {                                          \
        VBERROR(QString("Start Error: (%1)").arg(result));    \
        Stop();                            \
        return false;   \
    }


#define Destroy(a) (*a)->Destroy(a);
#define SetPlayState(a, b) (*a)->SetPlayState(a, b)
#define RegisterCallback(a, b, c) (*a)->RegisterCallback(a, b, c)
#define GetInterface(a, b, c) (*a)->GetInterface(a, b, c)
#define Realize(a, b) (*a)->Realize(a, b)
#define CreateOutputMix(a, b, c, d, e) (*a)->CreateOutputMix(a, b, c, d, e)
#define CreateAudioPlayer(a, b, c, d, e, f, g) \
    (*a)->CreateAudioPlayer(a, b, c, d, e, f, g)
#define Enqueue(a, b, c) (*a)->Enqueue(a, b, c)
#define Clear(a) (*a)->Clear(a)
#define GetState(a, b) (*a)->GetState(a, b)
#define SetPositionUpdatePeriod(a, b) (*a)->SetPositionUpdatePeriod(a, b)
#define SetVolumeLevel(a, b) (*a)->SetVolumeLevel(a, b)
#define GetVolumeLevel(a, b) (*a)->GetVolumeLevel(a, b)
#define SetMute(a, b) (*a)->SetMute(a, b)

int GetNativeOutputSampleRate(void);

int GetNativeOutputSampleRate(void)
{
#if 1    
    QAndroidJniEnvironment jniEnv;
    jclass cls = jniEnv->FindClass("android/media/AudioTrack");
    jmethodID method = jniEnv->GetStaticMethodID (cls, "getNativeOutputSampleRate", "(I)I");
    int sample_rate = jniEnv->CallStaticIntMethod (cls, method, 3); // AudioManager.STREAM_MUSIC
    return sample_rate;
#else    
    //AudioManager.getProperty(PROPERTY_OUTPUT_SAMPLE_RATE)
    return QAndroidJniObject::CallStaticMethodInt("android/media/AudioTrack", "getNativeOutputSampleRate", "(I)I", 3);
#endif
}

#if 0
int GetNativeOutputFramesPerBuffer(void)
{
    QAndroidJniObject activity = QtAndroid::androidActivity();
    //Context.getSystemService(Context.AUDIO_SERVICE)
    QAndroidJniObject audioManager = activity.CallObjectMethod("getSystemService", "", );
    jstring sValue = audioManager.CallMethod<jstring>("getProperty", PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
    // getNativeOutputFramesPerBuffer
    return QAndroidJniObject::CallStaticMethodInt("android/media/AudioManager", "getProperty", "(I)I", 3);
}
#endif

bool AudioOutputOpenSLES::CreateEngine()
{
    SLresult result;

    p_so_handle = dlopen("libOpenSLES.so", RTLD_NOW);
    if (p_so_handle == nullptr)
    {
        VBERROR("Error: Failed to load libOpenSLES");
        Close();
        return false;
    }

    slCreateEnginePtr = (slCreateEngine_t)dlsym(p_so_handle, "slCreateEngine");
    if (slCreateEnginePtr == 0)
    {
        VBERROR("Error: Failed to load symbol slCreateEngine");
        Close();
        return false;
    }

#define OPENSL_DLSYM(dest, name)                       \
    do {                                                       \
        const SLInterfaceID *sym = (const SLInterfaceID *)dlsym(p_so_handle, "SL_IID_" name);        \
        if (sym == 0)                             \
        {                                                      \
            LOG(VB_GENERAL, LOG_ERR, "AOOSLES Error: Failed to load symbol SL_IID_" name); \
            Close();  \
            return false;   \
        }                                                      \
        dest = *sym;                                           \
    } while(0)

    OPENSL_DLSYM(SL_IID_ANDROIDSIMPLEBUFFERQUEUE, "ANDROIDSIMPLEBUFFERQUEUE");
    OPENSL_DLSYM(SL_IID_ENGINE, "ENGINE");
    OPENSL_DLSYM(SL_IID_PLAY, "PLAY");
    OPENSL_DLSYM(SL_IID_VOLUME, "VOLUME");
#undef OPENSL_DLSYM

    // create engine
    result = slCreateEnginePtr(&engineObject, 0, NULL, 0, NULL, NULL);
    CHECK_OPENSL_ERROR("Failed to create engine");

    // realize the engine in synchronous mode
    result = Realize(engineObject, SL_BOOLEAN_FALSE);
    CHECK_OPENSL_ERROR("Failed to realize engine");

    // get the engine interface, needed to create other objects
    result = GetInterface(engineObject, SL_IID_ENGINE, &engineEngine);
    CHECK_OPENSL_ERROR("Failed to get the engine interface");

    // create output mix, with environmental reverb specified as a non-required interface
    const SLInterfaceID ids1[] = { SL_IID_VOLUME };
    const SLboolean req1[] = { SL_BOOLEAN_FALSE };
    result = CreateOutputMix(engineEngine, &outputMixObject, 1, ids1, req1);
    CHECK_OPENSL_ERROR("Failed to create output mix");

    // realize the output mix in synchronous mode
    result = Realize(outputMixObject, SL_BOOLEAN_FALSE);
    CHECK_OPENSL_ERROR("Failed to realize output mix");

    return true;
}

bool AudioOutputOpenSLES::StartPlayer()
{
    SLresult       result;

    // configure audio source - this defines the number of samples you can enqueue.
    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {
        SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
        OPENSLES_BUFFERS
    };

    SLDataFormat_PCM format_pcm;
    format_pcm.formatType       = SL_DATAFORMAT_PCM;
    format_pcm.numChannels      = 2;
    format_pcm.samplesPerSec    = ((SLuint32) samplerate * 1000) ;
    format_pcm.endianness       = SL_BYTEORDER_LITTLEENDIAN;
    switch (output_format)
    {
        case FORMAT_U8:     format_pcm.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_8;  break;
        case FORMAT_S16:    format_pcm.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16; break;
#if 0
        case FORMAT_S24LSB: format_pcm.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_24; break;
        case FORMAT_S24:
            format_pcm.endianness    = SL_BYTEORDER_BIGENDIAN;
            format_pcm.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_24;
            break;
        case FORMAT_S32:    format_pcm.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_32; break;
        // case FORMAT_FLT:    format_pcm.bitsPerSample    = SL_PCMSAMPLEFORMAT_FIXED_32; break;
#endif
        default:
            Error(QObject::tr("Unknown sample format: %1").arg(output_format));
            return false;
    }
    format_pcm.containerSize    = format_pcm.bitsPerSample;
    format_pcm.channelMask      = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;

    SLDataSource audioSrc = {&loc_bufq, &format_pcm};

    // configure audio sink
    SLDataLocator_OutputMix loc_outmix = {
        SL_DATALOCATOR_OUTPUTMIX,
        outputMixObject
    };
    SLDataSink audioSnk = {&loc_outmix, NULL};

    //create audio player
    const SLInterfaceID ids2[] = { SL_IID_ANDROIDSIMPLEBUFFERQUEUE, SL_IID_VOLUME };
    static const SLboolean req2[] = { SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE };

    if (GetNativeOutputSampleRate() >= samplerate) { // FIXME
        result = CreateAudioPlayer(engineEngine, &playerObject, &audioSrc,
                                    &audioSnk, sizeof(ids2) / sizeof(*ids2),
                                    ids2, req2);
    } else {
        // Don't try to play back a sample rate higher than the native one,
        // since OpenSL ES will try to use the fast path, which AudioFlinger
        // will reject (fast path can't do resampling), and will end up with
        // too small buffers for the resampling. See http://b.android.com/59453
        // for details. This bug is still present in 4.4. If it is fixed later
        // this workaround could be made conditional.
        result = SL_RESULT_UNKNOWN_ERROR;
    }
    if (result != SL_RESULT_SUCCESS) {
        /* Try again with a more sensible samplerate */
        //fmt->i_rate = 44100;
        format_pcm.samplesPerSec = ((SLuint32) 48000 * 1000) ;
        result = CreateAudioPlayer(engineEngine, &playerObject, &audioSrc,
                &audioSnk, sizeof(ids2) / sizeof(*ids2),
                ids2, req2);
    }
    CHECK_OPENSL_ERROR("Failed to create audio player");

    result = Realize(playerObject, SL_BOOLEAN_FALSE);
    CHECK_OPENSL_ERROR("Failed to realize player object.");

    result = GetInterface(playerObject, SL_IID_PLAY, &playerPlay);
    CHECK_OPENSL_ERROR("Failed to get player interface.");

    result = GetInterface(playerObject, SL_IID_VOLUME, &volumeItf);
    CHECK_OPENSL_ERROR("failed to get volume interface.");

    result = GetInterface(playerObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                                  &playerBufferQueue);
    CHECK_OPENSL_ERROR("Failed to get buff queue interface");

    result = RegisterCallback(playerBufferQueue,
        &AudioOutputOpenSLES::SPlayedCallback,
                                   (void*)this);
    CHECK_OPENSL_ERROR("Failed to register buff queue callback.");

    // set the player's state to playing
    result = SetPlayState(playerPlay, SL_PLAYSTATE_PLAYING);
    CHECK_OPENSL_ERROR("Failed to switch to playing state");

    /* XXX: rounding shouldn't affect us at normal sampling rate */
    uint32_t samplesPerBuf = OPENSLES_BUFLEN * samplerate / 1000;
    buf = (uint8_t*)malloc(OPENSLES_BUFFERS * samplesPerBuf * bytes_per_frame);
    if (!buf)
    {
        Stop();
        return false;
    }

    started = false;
    bufWriteIndex = 0;
    bufWriteBase = 0;


    // we want 16bit signed data native endian.
    //fmt->i_format              = VLC_CODEC_S16N;
    //fmt->i_physical_channels   = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;

/* Buffers which arrive after pts - AOUT_MIN_PREPARE_TIME will be trashed
    to avoid too heavy resampling */
    //SetPositionUpdatePeriod(playerPlay, 25000);
    //SetPositionUpdatePeriod(playerPlay, 40);
    SetPositionUpdatePeriod(playerPlay, POSITIONUPDATEPERIOD);

    return true;
}

bool AudioOutputOpenSLES::Stop()
{
    SLresult       result;
    if (playerObject)
    {
        // set the player's state to playing
        result = SetPlayState(playerPlay, SL_PLAYSTATE_STOPPED);
        CHECK_OPENSL_ERROR("Failed to switch to not playing state");
        Destroy(playerObject);
        playerObject = nullptr;
    }
    if (buf)
    {
        free(buf);
        buf = nullptr;
    }
    return true;
}

bool AudioOutputOpenSLES::Open()
{
    bool result = CreateEngine();
    if (result)
    {
        result = StartPlayer();
    }
    if (!result)
    {
        Close();
    }
    return result;
}

void AudioOutputOpenSLES::Close()
{
    Stop();
    if (outputMixObject)
    {
        Destroy(outputMixObject);
        outputMixObject = nullptr;
    }
    if (engineObject)
    {
        Destroy(engineObject);
        engineObject = nullptr;
    }
    if (p_so_handle)
    {
        dlclose(p_so_handle);
        p_so_handle = nullptr;
    }
}



AudioOutputOpenSLES::AudioOutputOpenSLES(const AudioSettings &settings) :
    AudioOutputBase(settings),
    nativeOutputSampleRate(GetNativeOutputSampleRate())
{
    InitSettings(settings);
    if (settings.init)
        Reconfigure(settings);
}

AudioOutputOpenSLES::~AudioOutputOpenSLES()
{
    KillAudio();

    CloseDevice();
}

AudioOutputSettings* AudioOutputOpenSLES::GetOutputSettings(bool /*digital*/)
{
    AudioOutputSettings *settings = new AudioOutputSettings();

    int32_t nativeRate = GetNativeOutputSampleRate(); // in Hz
    VBGENERAL(QString("Native Rate %1").arg(nativeRate));
    //settings->AddSupportedRate(48000);
    settings->AddSupportedRate(nativeRate);

    // Support all standard formats
    settings->AddSupportedFormat(FORMAT_U8);
    settings->AddSupportedFormat(FORMAT_S16);
    // settings->AddSupportedFormat(FORMAT_S24);
    // settings->AddSupportedFormat(FORMAT_S32);
#if 0 // 32-bit floating point (AC3) is not supported on all platforms.
    settings->AddSupportedFormat(FORMAT_FLT);
#endif

    // Guess that we can do up to 2
    settings->AddSupportedChannels(2);

    settings->setPassthrough(0); //No passthrough

    return settings;
}

bool AudioOutputOpenSLES::OpenDevice(void)
{
    CloseDevice();

    if (!Open())
    {
        CloseDevice();
        return false;
    }

    // fragments are 10ms worth of samples
    fragment_size = OPENSLES_BUFLEN * output_bytes_per_frame * samplerate / 1000;
    // OpenSLES buffer holds 10 fragments = 80ms worth of samples
    soundcard_buffer_size = OPENSLES_BUFFERS * fragment_size;

    VBAUDIO(QString("Buffering %1 fragments of %2 bytes each, total: %3 bytes")
            .arg(OPENSLES_BUFFERS).arg(fragment_size).arg(soundcard_buffer_size));

    return true;
}

void AudioOutputOpenSLES::CloseDevice(void)
{
    Stop();
    Close();
}

void AudioOutputOpenSLES::SPlayedCallback(SLAndroidSimpleBufferQueueItf caller, void *pContext)
{
    ((AudioOutputOpenSLES*)pContext)->PlayedCallback(caller);
}

void AudioOutputOpenSLES::PlayedCallback(SLAndroidSimpleBufferQueueItf caller)
{
    assert (caller == playerBufferQueue);

    VBAUDIO(QString("Freed"));

    lock.lock();
    started = true;
    lock.unlock();
}

int AudioOutputOpenSLES::GetNumberOfBuffersQueued() const
{
    SLAndroidSimpleBufferQueueState st;
    SLresult res = GetState(playerBufferQueue, &st);
    if (res != SL_RESULT_SUCCESS) {
        VBERROR(QString("Could not query buffer queue state in %1 (%2)").arg(__func__).arg(res));
        return -1;
    }
    VBAUDIO(QString("Num Queued %1").arg(st.count));
    return st.count;
}

void AudioOutputOpenSLES::WriteAudio(unsigned char * buffer, int size)
{
    VBAUDIO(QString("WriteAudio %1").arg(size));
    while (size > 0)
    {
        // check if there are any buffers available
        int32_t numBufferesQueued = GetNumberOfBuffersQueued();
        if (numBufferesQueued < 0)
            return;

        if (numBufferesQueued == OPENSLES_BUFFERS)
        {
            // wait for a buffer period, should be clear next time around
            usleep(OPENSLES_BUFLEN * 1000);
            continue;
        }

        if (size < (fragment_size + bufWriteIndex))
        {
            memcpy(&buf[bufWriteBase + bufWriteIndex], buffer, size);
            size = 0;
            // no more to do so exit now, dont have a full buffer
            break;
        }
        else
        {
            memcpy(&buf[bufWriteBase + bufWriteIndex], buffer, fragment_size - bufWriteIndex);
            size -= fragment_size - bufWriteIndex;
        }

        SLresult r = Enqueue(playerBufferQueue, &buf[bufWriteBase], fragment_size);
        VBAUDIO(QString("Enqueue %1").arg(bufWriteBase));

        if (r != SL_RESULT_SUCCESS)
        {
            // should never happen so show a log
            VBERROR(QString("error %1 when writing %2 bytes %3")
                    .arg(r)
                    .arg(fragment_size)
                    .arg((r == SL_RESULT_BUFFER_INSUFFICIENT) ? " (buffer insufficient)" : ""));
            // toss the remaining data, we got bigger problems
            return;
        }
        // update pointers for next time only if the data was queued correctly
        bufWriteBase += fragment_size;
        if (bufWriteBase >= (fragment_size * OPENSLES_BUFFERS))
            bufWriteBase = 0;
        bufWriteIndex = 0;
    }
}

int AudioOutputOpenSLES::GetBufferedOnSoundcard(void) const
{
    int numBufferesQueued = GetNumberOfBuffersQueued();
    if (numBufferesQueued < 0)
    {
        return 0;
    }
    return numBufferesQueued * fragment_size + bufWriteIndex;
}

int AudioOutputOpenSLES::GetVolumeChannel(int channel) const
{
    SLmillibel mb = 0;
    int volume = 0;
    SLresult r = GetVolumeLevel(volumeItf, &mb);
    if (r == SL_RESULT_SUCCESS)
    {
        if (mb <= SL_MILLIBEL_MIN)
        {
            volume = 0;
        }
        else
        {
            volume = lroundf(expf(mb / (3*2000.0f)) * 100);
        }
        VBAUDIO(QString("GetVolume(%1) %2 (%3)")
                        .arg(channel).arg(volume).arg(mb));
    }
    else
    {
        VBERROR(QString("GetVolume(%1) %2 (%3) : %4")
                        .arg(channel).arg(volume).arg(mb).arg(r));
    }

    return volume;
}

void AudioOutputOpenSLES::SetVolumeChannel(int channel, int volume)
{
    if (channel > 1)
        VBERROR("Android volume only supports stereo!");

    // Volume is 0-100
    // android expects 0-1.0 before conversion
    // Convert UI volume to linear factor (cube) in log
    float vol = volume / 100.f;

    int mb;
    if (volume == 0)
    {
        mb = SL_MILLIBEL_MIN;
    }
    else
    {
        // millibels from linear amplification
        mb = lroundf(3 * 2000.f * log10f(vol));
        if (mb < SL_MILLIBEL_MIN)
            mb = SL_MILLIBEL_MIN;
        else if (mb > 0)
            mb = 0; // maximum supported level could be higher: GetMaxVolumeLevel */
    }

    SLresult r = SetVolumeLevel(volumeItf, mb);
    if (r == SL_RESULT_SUCCESS)
    {
        VBAUDIO(QString("SetVolume(%1) %2(%3)")
                .arg(channel).arg(volume).arg(mb));
    }
    else
    {
        VBERROR(QString("SetVolume(%1) %2(%3) : %4")
                .arg(channel).arg(volume).arg(mb).arg(r));
    }

}
