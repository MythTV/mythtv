#include "mythplayer.h"
#include "audiooutput.h"
#include "audioplayer.h"

#define LOC QString("AudioPlayer: ")

AudioPlayer::AudioPlayer(MythPlayer *parent, bool muted)
  : m_parent(parent),     m_audioOutput(NULL),   m_channels(-1),
    m_orig_channels(-1),  m_codec(0),            m_format(FORMAT_NONE),
    m_samplerate(44100),  m_codec_profile(0),
    m_stretchfactor(1.0f),m_passthru(false),
    m_lock(QMutex::Recursive), m_muted_on_creation(muted),
    m_main_device(QString::null), m_passthru_device(QString::null),
    m_no_audio_in(false), m_no_audio_out(true), m_controls_volume(true)
{
    m_controls_volume = gCoreContext->GetNumSetting("MythControlsVolume", 1);
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
    Visuals::iterator it = std::find(m_visuals.begin(), m_visuals.end(), vis);
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
    Visuals::iterator it = std::find(m_visuals.begin(), m_visuals.end(), vis);
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
    for (uint i = 0; i < m_visuals.size(); i++)
        m_audioOutput->addVisual(m_visuals[i]);
}

void AudioPlayer::RemoveVisuals(void)
{
    if (!m_audioOutput)
        return;

    QMutexLocker lock(&m_lock);
    for (uint i = 0; i < m_visuals.size(); i++)
        m_audioOutput->removeVisual(m_visuals[i]);
}

void AudioPlayer::ResetVisuals(void)
{
    if (!m_audioOutput)
        return;

    QMutexLocker lock(&m_lock);
    for (uint i = 0; i < m_visuals.size(); i++)
        m_visuals[i]->prepare();
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
        m_audioOutput = NULL;
    }
    m_no_audio_out = true;
}

QString AudioPlayer::ReinitAudio(void)
{
    bool want_audio = m_parent->IsAudioNeeded();
    QString errMsg = QString::null;
    QMutexLocker lock(&m_lock);

    bool firstinit = (m_format == FORMAT_NONE &&
                      m_channels < 0 &&
                      m_samplerate == 44100);

    if ((m_format == FORMAT_NONE) ||
        (m_channels <= 0) ||
        (m_samplerate <= 0))
    {
        m_no_audio_in = m_no_audio_out = true;
    }
    else
        m_no_audio_in = false;

    if (want_audio && !m_audioOutput)
    {
        // AudioOutput has never been created and we will want audio
        AudioSettings aos = AudioSettings(m_main_device,
                                          m_passthru_device,
                                          m_format, m_channels,
                                          m_codec, m_samplerate,
                                          AUDIOOUTPUT_VIDEO,
                                          m_controls_volume, m_passthru);
        if (m_no_audio_in)
            aos.init = false;

        m_audioOutput = AudioOutput::OpenAudio(aos);
        if (!m_audioOutput)
        {
            errMsg = QObject::tr("Unable to create AudioOutput.");
        }
        else
        {
            errMsg = m_audioOutput->GetError();
        }
        AddVisuals();
    }
    else if (!m_no_audio_in && m_audioOutput)
    {
        const AudioSettings settings(m_format, m_channels, m_codec,
                                     m_samplerate, m_passthru, 0,
                                     m_codec_profile);
        m_audioOutput->Reconfigure(settings);
        errMsg = m_audioOutput->GetError();
        SetStretchFactor(m_stretchfactor);
    }

    if (!errMsg.isEmpty())
    {
        if (!firstinit)
        {
            LOG(VB_GENERAL, LOG_NOTICE, LOC + "Disabling Audio" +
                    QString(", reason is: %1").arg(errMsg));
        }
        m_no_audio_out = true;
    }
    else if (m_no_audio_out && m_audioOutput)
    {
        LOG(VB_GENERAL, LOG_NOTICE, LOC + "Enabling Audio");
        m_no_audio_out = false;
    }

    if (m_muted_on_creation)
    {
        SetMuteState(kMuteAll);
        m_muted_on_creation = false;
    }

    ResetVisuals();

    return errMsg;
}

void AudioPlayer::CheckFormat(void)
{
    if (m_format == FORMAT_NONE)
        m_no_audio_in = m_no_audio_out = true;
}

bool AudioPlayer::Pause(bool pause)
{
    if (!m_audioOutput || m_no_audio_out)
        return false;

    QMutexLocker lock(&m_lock);
    m_audioOutput->Pause(pause);
    return true;
}

bool AudioPlayer::IsPaused(void)
{
    if (!m_audioOutput || m_no_audio_out)
        return false;
    QMutexLocker lock(&m_lock);
    return m_audioOutput->IsPaused();
}

void AudioPlayer::PauseAudioUntilBuffered()
{
    if (!m_audioOutput || m_no_audio_out)
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
    if (!m_audioOutput || m_no_audio_out)
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
    m_codec_profile    = codec_profile;
}

/**
 * Set audio output parameters.
 * codec_profile is currently only used for DTS
 */
void AudioPlayer::SetAudioParams(AudioFormat format, int orig_channels,
                                 int channels, int codec,
                                 int samplerate, bool passthru,
                                 int codec_profile)
{
    m_format        = CanProcess(format) ? format : FORMAT_S16;
    m_orig_channels = orig_channels;
    m_channels      = channels;
    m_codec         = codec;
    m_samplerate    = samplerate;
    m_passthru      = passthru;
    m_codec_profile = codec_profile;

    ResetVisuals();
}

void AudioPlayer::SetEffDsp(int dsprate)
{
    if (!m_audioOutput || !m_no_audio_out)
        return;
    QMutexLocker lock(&m_lock);
    m_audioOutput->SetEffDsp(dsprate);
}

bool AudioPlayer::SetMuted(bool mute)
{
    bool is_muted = IsMuted();
    QMutexLocker lock(&m_lock);

    if (m_audioOutput && !m_no_audio_out && !is_muted && mute &&
        (kMuteAll == SetMuteState(kMuteAll)))
    {
        LOG(VB_AUDIO, LOG_INFO, QString("muting sound %1").arg(IsMuted()));
        return true;
    }
    else if (m_audioOutput && !m_no_audio_out && is_muted && !mute &&
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
    if (!m_audioOutput || m_no_audio_out)
        return kMuteAll;
    QMutexLocker lock(&m_lock);
    return m_audioOutput->SetMuteState(mstate);
}

MuteState AudioPlayer::IncrMuteState(void)
{
    if (!m_audioOutput || m_no_audio_out)
        return kMuteAll;
    return SetMuteState(VolumeBase::NextMuteState(GetMuteState()));
}

MuteState AudioPlayer::GetMuteState(void)
{
    if (!m_audioOutput || m_no_audio_out)
        return kMuteAll;
    QMutexLocker lock(&m_lock);
    return m_audioOutput->GetMuteState();
}

uint AudioPlayer::AdjustVolume(int change)
{
    if (!m_audioOutput || m_no_audio_out)
        return GetVolume();
    QMutexLocker lock(&m_lock);
    m_audioOutput->AdjustCurrentVolume(change);
    return GetVolume();
}

uint AudioPlayer::SetVolume(int newvolume)
{
    if (!m_audioOutput || m_no_audio_out)
        return GetVolume();
    QMutexLocker lock(&m_lock);
    m_audioOutput->SetCurrentVolume(newvolume);
    return GetVolume();
}

int64_t AudioPlayer::GetAudioTime(void)
{
    if (!m_audioOutput || m_no_audio_out)
        return 0LL;
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
    m_stretchfactor = factor;
    if (!m_audioOutput)
        return;
    QMutexLocker lock(&m_lock);
    m_audioOutput->SetStretchFactor(m_stretchfactor);
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
                                 int codec, int profile)
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
                               int64_t timecode, int frames)
{
    if (!m_audioOutput || m_no_audio_out)
        return;

    if (m_parent->PrepareAudioSample(timecode) && !m_no_audio_out)
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
    else
        return m_audioOutput->NeedDecodingBeforePassthrough();
}

int64_t AudioPlayer::LengthLastData(void)
{
    if (!m_audioOutput)
        return 0;
    else
        return m_audioOutput->LengthLastData();
}

bool AudioPlayer::GetBufferStatus(uint &fill, uint &total)
{
    fill = total = 0;
    if (!m_audioOutput || m_no_audio_out)
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

bool AudioPlayer::CanProcess(AudioFormat fmt)
{
    if (!m_audioOutput)
        return false;
    else
        return m_audioOutput->CanProcess(fmt);
}

uint32_t AudioPlayer::CanProcess(void)
{
    if (!m_audioOutput)
        return 0;
    else
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
    else
        return m_audioOutput->DecodeAudio(ctx, buffer, data_size, pkt);
}
