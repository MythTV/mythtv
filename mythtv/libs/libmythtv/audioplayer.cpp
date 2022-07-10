
#include "libmyth/audio/audiooutput.h"
#include "libmythui/mythnotificationcenter.h"

#include "audioplayer.h"
#include "mythplayer.h"
#include "visualisations/videovisual.h"

#define LOC QString("AudioPlayer: ")

//static const QString _Location = AudioPlayer::tr("Audio Player");

AudioPlayer::AudioPlayer(MythPlayer *parent, bool muted)
  : m_parent(parent),
    m_mutedOnCreation(muted)
{
    m_controlsVolume = gCoreContext->GetBoolSetting("MythControlsVolume", true);
}

AudioPlayer::~AudioPlayer()
{
    DeleteOutput();
    m_visuals.clear();
}

void AudioPlayer::addVisual(MythTV::Visual *vis)
{
    if (!m_audioOutput)
        return;

    QMutexLocker lock(&m_lock);
    auto it = std::find(m_visuals.begin(), m_visuals.end(), vis);
    if (it == m_visuals.end())
    {
        m_visuals.push_back(vis);
        m_audioOutput->addVisual(vis);
    }
}

void AudioPlayer::removeVisual(MythTV::Visual *vis)
{
    if (!m_audioOutput)
        return;

    QMutexLocker lock(&m_lock);
    auto it = std::find(m_visuals.begin(), m_visuals.end(), vis);
    if (it != m_visuals.end())
    {
        m_visuals.erase(it);
        m_audioOutput->removeVisual(vis);
    }
}

void AudioPlayer::AddVisuals(void)
{
    if (!m_audioOutput)
        return;

    QMutexLocker lock(&m_lock);
    for (auto & visual : m_visuals)
        m_audioOutput->addVisual(visual);
}

void AudioPlayer::RemoveVisuals(void)
{
    if (!m_audioOutput)
        return;

    QMutexLocker lock(&m_lock);
    for (auto & visual : m_visuals)
        m_audioOutput->removeVisual(visual);
}

void AudioPlayer::ResetVisuals(void)
{
    if (!m_audioOutput)
        return;

    QMutexLocker lock(&m_lock);
    for (auto & visual : m_visuals)
        visual->prepare();
}

void AudioPlayer::Reset(void)
{
    if (!m_audioOutput)
        return;

    QMutexLocker lock(&m_lock);
    m_audioOutput->Reset();
}

void AudioPlayer::DeleteOutput(void)
{
    RemoveVisuals();
    QMutexLocker locker(&m_lock);
    if (m_audioOutput)
    {
        delete m_audioOutput;
        m_audioOutput = nullptr;
    }
    m_noAudioOut = true;
}

QString AudioPlayer::ReinitAudio(void)
{
    bool want_audio = m_parent->IsAudioNeeded();
    QString errMsg;
    QMutexLocker lock(&m_lock);

    if ((m_state.m_format == FORMAT_NONE) || (m_state.m_channels <= 0) || (m_state.m_sampleRate <= 0))
        m_noAudioIn = m_noAudioOut = true;
    else
        m_noAudioIn = false;

    if (want_audio && !m_audioOutput)
    {
        // AudioOutput has never been created and we will want audio
        AudioSettings aos = AudioSettings(m_mainDevice, m_passthruDevice,
                                          m_state.m_format, m_state.m_channels,
                                          m_state.m_codec, m_state.m_sampleRate,
                                          AUDIOOUTPUT_VIDEO,
                                          m_controlsVolume, m_state.m_passthru);
        if (m_noAudioIn)
            aos.m_init = false;

        m_audioOutput = AudioOutput::OpenAudio(aos);
        if (!m_audioOutput)
        {
            errMsg = tr("Unable to create AudioOutput.");
        }
        else
        {
            errMsg = m_audioOutput->GetError();
        }
        AddVisuals();
    }
    else if (!m_noAudioIn && m_audioOutput)
    {
        const AudioSettings settings(m_state.m_format, m_state.m_channels, m_state.m_codec,
                                     m_state.m_sampleRate, m_state.m_passthru, 0,
                                     m_state.m_codecProfile);
        m_audioOutput->Reconfigure(settings);
        errMsg = m_audioOutput->GetError();
        SetStretchFactor(m_stretchFactor);
    }

    if (!errMsg.isEmpty())
    {
        LOG(VB_GENERAL, LOG_NOTICE, LOC + "Disabling Audio" +
                QString(", reason is: %1").arg(errMsg));
        ShowNotificationError(tr("Disabling Audio"),
                              AudioPlayer::tr( "Audio Player" ), errMsg);
        m_noAudioOut = true;
    }
    else if (m_noAudioOut && m_audioOutput)
    {
        LOG(VB_GENERAL, LOG_NOTICE, LOC + "Enabling Audio");
        m_noAudioOut = false;
    }

    if (m_mutedOnCreation)
    {
        SetMuteState(kMuteAll);
        m_mutedOnCreation = false;
    }

    ResetVisuals();

    return errMsg;
}

void AudioPlayer::CheckFormat(void)
{
    if (m_state.m_format == FORMAT_NONE)
        m_noAudioIn = m_noAudioOut = true;
}

bool AudioPlayer::Pause(bool pause)
{
    if (!m_audioOutput || m_noAudioOut)
        return false;

    QMutexLocker lock(&m_lock);
    m_audioOutput->Pause(pause);
    return true;
}

bool AudioPlayer::IsPaused(void)
{
    if (!m_audioOutput || m_noAudioOut)
        return false;
    QMutexLocker lock(&m_lock);
    return m_audioOutput->IsPaused();
}

void AudioPlayer::PauseAudioUntilBuffered()
{
    if (!m_audioOutput || m_noAudioOut)
        return;
    QMutexLocker lock(&m_lock);
    m_audioOutput->PauseUntilBuffered();
}

void AudioPlayer::SetAudioOutput(AudioOutput *ao)
{
    // delete current audio class if any
    DeleteOutput();
    m_lock.lock();
    m_audioOutput = ao;
    AddVisuals();
    m_lock.unlock();
}

uint AudioPlayer::GetVolume(void)
{
    if (!m_audioOutput || m_noAudioOut)
        return 0;
    QMutexLocker lock(&m_lock);
    return m_audioOutput->GetCurrentVolume();
}

/**
 * Set audio output device parameters.
 * codec_profile is currently only used for DTS
 */
void AudioPlayer::SetAudioInfo(const QString &main_device,
                               const QString &passthru_device,
                               uint           samplerate,
                               int            codec_profile)
{
    m_mainDevice.clear();
    m_passthruDevice.clear();
    if (!main_device.isEmpty())
        m_mainDevice = main_device;
    if (!passthru_device.isEmpty())
        m_passthruDevice = passthru_device;
    m_state.m_sampleRate = static_cast<int>(samplerate);
    m_state.m_codecProfile = codec_profile;
    emit AudioPlayerStateChanged(m_state);
}

/**
 * Set audio output parameters.
 * codec_profile is currently only used for DTS
 */
void AudioPlayer::SetAudioParams(AudioFormat format, int orig_channels,
                                 int channels, AVCodecID codec,
                                 int samplerate, bool passthru,
                                 int codec_profile)
{
    m_state.m_format       = CanProcess(format) ? format : FORMAT_S16;
    m_state.m_origChannels = orig_channels;
    m_state.m_channels     = channels;
    m_state.m_codec        = codec;
    m_state.m_sampleRate   = samplerate;
    m_state.m_passthru     = passthru;
    m_state.m_codecProfile = codec_profile;
    ResetVisuals();
    emit AudioPlayerStateChanged(m_state);
}

void AudioPlayer::SetEffDsp(int dsprate)
{
    if (!m_audioOutput || !m_noAudioOut)
        return;
    QMutexLocker lock(&m_lock);
    m_audioOutput->SetEffDsp(dsprate);
}

bool AudioPlayer::SetMuted(bool mute)
{
    bool is_muted = IsMuted();
    QMutexLocker lock(&m_lock);

    if (m_audioOutput && !m_noAudioOut && !is_muted && mute &&
        (kMuteAll == SetMuteState(kMuteAll)))
    {
        LOG(VB_AUDIO, LOG_INFO, QString("muting sound %1").arg(IsMuted()));
        return true;
    }
    if (m_audioOutput && !m_noAudioOut && is_muted && !mute &&
             (kMuteOff == SetMuteState(kMuteOff)))
    {
        LOG(VB_AUDIO, LOG_INFO, QString("unmuting sound %1").arg(IsMuted()));
        return true;
    }

    LOG(VB_AUDIO, LOG_ERR,
        QString("not changing sound mute state %1").arg(IsMuted()));

    return false;
}

MuteState AudioPlayer::SetMuteState(MuteState mstate)
{
    if (!m_audioOutput || m_noAudioOut)
        return kMuteAll;
    QMutexLocker lock(&m_lock);
    return m_audioOutput->SetMuteState(mstate);
}

MuteState AudioPlayer::IncrMuteState(void)
{
    if (!m_audioOutput || m_noAudioOut)
        return kMuteAll;
    return SetMuteState(VolumeBase::NextMuteState(GetMuteState()));
}

MuteState AudioPlayer::GetMuteState(void)
{
    if (!m_audioOutput || m_noAudioOut)
        return kMuteAll;
    QMutexLocker lock(&m_lock);
    return m_audioOutput->GetMuteState();
}

uint AudioPlayer::AdjustVolume(int change)
{
    if (!m_audioOutput || m_noAudioOut)
        return GetVolume();
    QMutexLocker lock(&m_lock);
    m_audioOutput->AdjustCurrentVolume(change);
    return GetVolume();
}

uint AudioPlayer::SetVolume(int newvolume)
{
    if (!m_audioOutput || m_noAudioOut)
        return GetVolume();
    QMutexLocker lock(&m_lock);
    m_audioOutput->SetCurrentVolume(newvolume);
    return GetVolume();
}

std::chrono::milliseconds AudioPlayer::GetAudioTime(void)
{
    if (!m_audioOutput || m_noAudioOut)
        return 0ms;
    QMutexLocker lock(&m_lock);
    return m_audioOutput->GetAudiotime();
}

bool AudioPlayer::IsUpmixing(void)
{
    if (!m_audioOutput)
        return false;
    QMutexLocker lock(&m_lock);
    return m_audioOutput->IsUpmixing();
}

bool AudioPlayer::EnableUpmix(bool enable, bool toggle)
{
    if (!m_audioOutput)
        return false;
    QMutexLocker lock(&m_lock);
    if (toggle || (enable != IsUpmixing()))
        return m_audioOutput->ToggleUpmix();
    return enable;
}

bool AudioPlayer::CanUpmix(void)
{
    if (!m_audioOutput)
        return false;
    QMutexLocker lock(&m_lock);
    return m_audioOutput->CanUpmix();
}

void AudioPlayer::SetStretchFactor(float factor)
{
    m_stretchFactor = factor;
    if (!m_audioOutput)
        return;
    QMutexLocker lock(&m_lock);
    m_audioOutput->SetStretchFactor(m_stretchFactor);
}

// The following methods are not locked as this hinders performance.
// They are however only called from the Decoder and only the decode
// thread will trigger a deletion/recreation of the AudioOutput device, hence
// they should be safe.

inline bool TestDigitalFeature(AudioOutput *ao, DigitalFeature feature)
{
    if (!ao)
        return false;

    return ao->GetOutputSettingsUsers(true)->canFeature(feature);
}

bool AudioPlayer::CanAC3(void)
{
    return TestDigitalFeature(m_audioOutput, FEATURE_AC3);
}

bool AudioPlayer::CanDTS(void)
{
    return TestDigitalFeature(m_audioOutput, FEATURE_DTS);
}

bool AudioPlayer::CanEAC3(void)
{
    return TestDigitalFeature(m_audioOutput, FEATURE_EAC3);
}

bool AudioPlayer::CanTrueHD(void)
{
    return TestDigitalFeature(m_audioOutput, FEATURE_TRUEHD);
}

bool AudioPlayer::CanDTSHD(void)
{
    return TestDigitalFeature(m_audioOutput, FEATURE_DTSHD);
}

uint AudioPlayer::GetMaxChannels(void)
{
    if (!m_audioOutput)
        return 2;
    return m_audioOutput->GetOutputSettingsUsers(false)->BestSupportedChannels();
}

int AudioPlayer::GetMaxHDRate()
{
    if (!m_audioOutput)
        return 0;
    return m_audioOutput->GetOutputSettingsUsers(true)->GetMaxHDRate();
}

bool AudioPlayer::CanPassthrough(int samplerate, int channels,
                                 AVCodecID codec, int profile)
{
    if (!m_audioOutput)
        return false;
    return m_audioOutput->CanPassthrough(samplerate, channels, codec, profile);
}

bool AudioPlayer::CanDownmix(void)
{
    if (!m_audioOutput)
        return false;
    return m_audioOutput->CanDownmix();
}

/*
 * if frames = -1 : let AudioOuput calculate value
 * if frames = 0 && len > 0: will calculate according to len
 */
void AudioPlayer::AddAudioData(char *buffer, int len,
                               std::chrono::milliseconds timecode, int frames)
{
    if (!m_audioOutput || m_noAudioOut)
        return;

    if (m_parent->PrepareAudioSample(timecode))
        m_audioOutput->Drain();
    int samplesize = m_audioOutput->GetBytesPerFrame();

    if (samplesize <= 0)
        return;

    if (frames == 0 && len > 0)
        frames = len / samplesize;

    if (!m_audioOutput->AddData(buffer, len, timecode, frames))
        LOG(VB_PLAYBACK, LOG_ERR, LOC + "AddAudioData(): "
                "Audio buffer overflow, audio data lost!");
}

bool AudioPlayer::NeedDecodingBeforePassthrough(void)
{
    if (!m_audioOutput)
        return true;
    return m_audioOutput->NeedDecodingBeforePassthrough();
}

std::chrono::milliseconds AudioPlayer::LengthLastData(void)
{
    if (!m_audioOutput)
        return 0ms;
    return m_audioOutput->LengthLastData();
}

bool AudioPlayer::GetBufferStatus(uint &fill, uint &total)
{
    fill = total = 0;
    if (!m_audioOutput || m_noAudioOut)
        return false;
    m_audioOutput->GetBufferStatus(fill, total);
    return true;
}

bool AudioPlayer::IsBufferAlmostFull(void)
{
    uint ofill = 0;
    uint ototal = 0;
    if (GetBufferStatus(ofill, ototal))
    {
        uint othresh =  ((ototal>>1) + (ototal>>2));
        if (ofill > othresh)
            return true;
        return GetAudioBufferedTime() > 8s;
    }
    return false;
}

std::chrono::milliseconds AudioPlayer::GetAudioBufferedTime(void)
{
    return m_audioOutput ? m_audioOutput->GetAudioBufferedTime() : 0ms;
}


bool AudioPlayer::CanProcess(AudioFormat fmt)
{
    if (!m_audioOutput)
        return false;
    return m_audioOutput->CanProcess(fmt);
}

uint32_t AudioPlayer::CanProcess(void)
{
    if (!m_audioOutput)
        return 0;
    return m_audioOutput->CanProcess();
}

/**
 * DecodeAudio
 * Utility routine.
 * Decode an audio packet, and compact it if data is planar
 * Return negative error code if an error occurred during decoding
 * or the number of bytes consumed from the input AVPacket
 * data_size contains the size of decoded data copied into buffer
 * data decoded will be S16 samples if class instance can't handle HD audio
 * or S16 and above otherwise. No U8 PCM format can be returned
 */
int AudioPlayer::DecodeAudio(AVCodecContext *ctx,
                             uint8_t *buffer, int &data_size,
                             const AVPacket *pkt)
{
    if (!m_audioOutput)
    {
        data_size = 0;
        return 0;
    }
    return m_audioOutput->DecodeAudio(ctx, buffer, data_size, pkt);
}
