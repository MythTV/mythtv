#include "mythplayer.h"
#include "audiooutput.h"
#include "audioplayer.h"

#define LOC QString("AudioPlayer: ")

AudioPlayer::AudioPlayer(MythPlayer *parent, bool muted)
  : m_parent(parent),    m_audioOutput(NULL),   m_channels(-1),
    m_orig_channels(-1), m_codec(0),            m_format(FORMAT_NONE),
    m_samplerate(44100), m_stretchfactor(1.0f), m_passthru(false),
    m_lock(QMutex::Recursive), m_muted_on_creation(muted), 
    m_main_device(QString::null), m_passthru_device(QString::null),
    no_audio_in(false), no_audio_out(false)
{
}

AudioPlayer::~AudioPlayer()
{
    DeleteOutput();
}

void AudioPlayer::Reset(void)
{
    m_lock.lock();
    if (m_audioOutput)
        m_audioOutput->Reset();
    m_lock.unlock();
}

void AudioPlayer::DeleteOutput(void)
{
    QMutexLocker locker(&m_lock);
    if (m_audioOutput)
    {
        delete m_audioOutput;
        m_audioOutput = NULL;
    }
    no_audio_out = true;
}

QString AudioPlayer::ReinitAudio(void)
{
    bool want_audio = m_parent->IsAudioNeeded();
    QMutexLocker lock(&m_lock);
    QString errMsg = QString::null;

    bool firstinit = (m_format == FORMAT_NONE &&
                      m_channels < 0 &&
                      m_samplerate == 44100);

    if ((m_format == FORMAT_NONE) ||
        (m_channels <= 0) ||
        (m_samplerate <= 0))
    {
        if (!firstinit)
        {
            VERBOSE(VB_IMPORTANT, LOC +
                    QString("Disabling Audio, params(%1,%2,%3)")
                    .arg(m_format).arg(m_channels).arg(m_samplerate));
        }
        no_audio_in = no_audio_out = true;
    }
    else
        no_audio_in = false;

    if (no_audio_out && want_audio)
    {
        if (m_audioOutput)
            DeleteOutput();

        bool setVolume = gCoreContext->GetNumSetting("MythControlsVolume", 1);
        m_audioOutput = AudioOutput::OpenAudio(m_main_device,
                                               m_passthru_device,
                                               m_format, m_channels,
                                               m_codec, m_samplerate,
                                               AUDIOOUTPUT_VIDEO,
                                               setVolume, m_passthru);
        if (!m_audioOutput)
            errMsg = QObject::tr("Unable to create AudioOutput.");
        else
            errMsg = m_audioOutput->GetError();

        if (!errMsg.isEmpty())
        {
            if (!firstinit)
            {
                VERBOSE(VB_IMPORTANT, LOC + "Disabling Audio" +
                        QString(", reason is: %1").arg(errMsg));
            }
            no_audio_out = true;
        }
        else if (no_audio_out)
        {
            VERBOSE(VB_IMPORTANT, LOC + "Enabling Audio");
            no_audio_out = false;
        }
        if (m_muted_on_creation)
        {
            SetMuteState(kMuteAll);
            m_muted_on_creation = false;
        }
    }

    if (m_audioOutput && !no_audio_out)
    {
        const AudioSettings settings(m_format, m_channels, m_codec,
                                     m_samplerate, m_passthru);
        m_audioOutput->Reconfigure(settings);
        if (m_passthru)
            m_channels = 2;
        errMsg = m_audioOutput->GetError();
        SetStretchFactor(m_stretchfactor);
    }

    return errMsg;
}

void AudioPlayer::CheckFormat(void)
{
    if (m_format == FORMAT_NONE)
        no_audio_in = no_audio_out = true;
}

bool AudioPlayer::Pause(bool pause)
{
    bool result = false;
    m_lock.lock();
    if (m_audioOutput && !no_audio_out)
    {
        m_audioOutput->Pause(pause);
        result = true;
    }
    m_lock.unlock();
    return result;
}

bool AudioPlayer::IsPaused(void)
{
    bool paused = false;
    m_lock.lock();
    if (m_audioOutput && !no_audio_out)
        paused = m_audioOutput->IsPaused();
    m_lock.unlock();
    return paused;
}

void AudioPlayer::PauseAudioUntilBuffered()
{
    m_lock.lock();
    if (m_audioOutput && !no_audio_out)
        m_audioOutput->PauseUntilBuffered();
    m_lock.unlock();
}

void AudioPlayer::SetAudioOutput(AudioOutput *ao)
{
    m_lock.lock();
    m_audioOutput = ao;
    m_lock.unlock();
}

uint AudioPlayer::GetVolume(void)
{
    uint res = 0;
    m_lock.lock();
    if (m_audioOutput && !no_audio_out)
        res = m_audioOutput->GetCurrentVolume();
    m_lock.unlock();
    return res;
}

void AudioPlayer::SetAudioInfo(const QString &main_device,
                               const QString &passthru_device,
                               uint           samplerate)
{
    m_main_device = m_passthru_device = QString::null;
    if (!main_device.isEmpty())
    {
        m_main_device = main_device;
        m_main_device.detach();
    }
    if (!passthru_device.isEmpty())
    {
        m_passthru_device = passthru_device;
        m_passthru_device.detach();
    }
    m_samplerate = (int)samplerate;
}

void AudioPlayer::SetAudioParams(AudioFormat format, int orig_channels,
                                 int channels, int codec,
                                 int samplerate, bool passthru)
{
    m_format        = format;
    m_orig_channels = orig_channels;
    m_channels      = channels;
    m_codec         = codec;
    m_samplerate    = samplerate;
    m_passthru      = passthru;
}

void AudioPlayer::SetEffDsp(int dsprate)
{
    m_lock.lock();
    if (m_audioOutput && !no_audio_out)
        m_audioOutput->SetEffDsp(dsprate);
    m_lock.unlock();
}

bool AudioPlayer::SetMuted(bool mute)
{
    QMutexLocker lock(&m_lock);
    bool is_muted = IsMuted();

    if (m_audioOutput && !no_audio_out && !is_muted && mute &&
        (kMuteAll == SetMuteState(kMuteAll)))
    {
        VERBOSE(VB_AUDIO, "muting sound " <<IsMuted());
        return true;
    }
    else if (m_audioOutput && !no_audio_out && is_muted && !mute &&
             (kMuteOff == SetMuteState(kMuteOff)))
    {
        VERBOSE(VB_AUDIO, "unmuting sound "<<IsMuted());
        return true;
    }

    VERBOSE(VB_AUDIO, "not changing sound mute state "<<IsMuted());

    return false;
}

MuteState AudioPlayer::SetMuteState(MuteState mstate)
{
    QMutexLocker lock(&m_lock);
    if (m_audioOutput && !no_audio_out)
        return m_audioOutput->SetMuteState(mstate);
    return kMuteAll;
}

MuteState AudioPlayer::IncrMuteState(void)
{
    QMutexLocker lock(&m_lock);
    MuteState mstate = kMuteAll;
    if (m_audioOutput && !no_audio_out)
        mstate = SetMuteState(VolumeBase::NextMuteState(GetMuteState()));
    return mstate;
}

MuteState AudioPlayer::GetMuteState(void)
{
    QMutexLocker lock(&m_lock);
    if (m_audioOutput && !no_audio_out)
        return m_audioOutput->GetMuteState();
    return kMuteAll;
}

uint AudioPlayer::AdjustVolume(int change)
{
    QMutexLocker lock(&m_lock);
    if (m_audioOutput && !no_audio_out)
        m_audioOutput->AdjustCurrentVolume(change);
    return GetVolume();
}

int64_t AudioPlayer::GetAudioTime(void)
{
    int64_t time = 0;
    m_lock.lock();
    if (m_audioOutput && !no_audio_out)
        time = m_audioOutput->GetAudiotime();
    m_lock.unlock();
    return time;
}

bool AudioPlayer::ToggleUpmix(void)
{
    bool toggle = false;
    m_lock.lock();
    if (m_audioOutput)
        toggle = m_audioOutput->ToggleUpmix();
    m_lock.unlock();
    return toggle;
}

void AudioPlayer::SetStretchFactor(float factor)
{
    m_stretchfactor = factor;
    m_lock.lock();
    if (m_audioOutput && !no_audio_out)
        m_audioOutput->SetStretchFactor(m_stretchfactor);
    m_lock.unlock();
}

// The following methods are not locked as this hinders performance.
// They are however only called from the Decoder and only the decode
// thread will trigger a deletion/recreation of the AudioOutput device, hence
// they should be safe.

bool AudioPlayer::CanAC3(void)
{
    bool ret = false;
    if (m_audioOutput)
        ret = m_audioOutput->GetOutputSettingsUsers()->canAC3();
    return ret;
}

bool AudioPlayer::CanDTS(void)
{
    bool ret = false;
    if (m_audioOutput)
        ret = m_audioOutput->GetOutputSettingsUsers()->canDTS();
    return ret;
}

uint AudioPlayer::GetMaxChannels(void)
{
    uint ret = 2;
    if (m_audioOutput)
        ret = m_audioOutput->GetOutputSettingsUsers()->BestSupportedChannels();
    return ret;
}

bool AudioPlayer::CanPassthrough(int samplerate, int channels)
{
    bool ret = false;
    if (m_audioOutput)
        ret = m_audioOutput->CanPassthrough(samplerate, channels);
    return ret;
}

bool AudioPlayer::CanDownmix(void)
{
    if (!m_audioOutput)
        return false;
    return m_audioOutput->CanDownmix();
}

void AudioPlayer::AddAudioData(char *buffer, int len, int64_t timecode)
{
    if (!m_audioOutput)
        return;
    if (m_parent->PrepareAudioSample(timecode) && !no_audio_out)
        m_audioOutput->Drain();
    int samplesize = m_channels * AudioOutputSettings::SampleSize(m_format);
    if (samplesize <= 0)
        return;
    int frames = len / samplesize;
    if (!m_audioOutput->AddFrames(buffer, frames, timecode))
        VERBOSE(VB_PLAYBACK, LOC + "AddAudioData():p1: "
                "Audio buffer overflow, audio data lost!");
}

bool AudioPlayer::GetBufferStatus(uint &fill, uint &total)
{
    fill = total = 0;
    if (!m_audioOutput || no_audio_out)
        return false;
    m_audioOutput->GetBufferStatus(fill, total);
    return true;
}

bool AudioPlayer::IsBufferAlmostFull(void)
{
    uint ofill = 0, ototal = 0, othresh = 0;
    if (GetBufferStatus(ofill, ototal))
    {
        othresh =  ((ototal>>1) + (ototal>>2));
        return ofill > othresh;
    }
    return false;
}
