
// -*- Mode: c++ -*-

// Standard UNIX C headers
#include <chrono> // for milliseconds
#include <thread> // for sleep_for
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

// Qt headers
#include <QCoreApplication>
#include <QEvent>
#include <QDir>

// MythTV headers
#include "mythconfig.h"
#include "mythcorecontext.h"
#include "mythdbcon.h"
#include "audiooutpututil.h"
#include "audiogeneralsettings.h"
#include "mythdialogbox.h"
#include "mythlogging.h"
#include "mythmainwindow.h"

extern "C" {
#include "libavformat/avformat.h"
}

AudioDeviceComboBox::AudioDeviceComboBox(AudioConfigSettings *parent) :
    HostComboBoxSetting("AudioOutputDevice", true), m_parent(parent)
{
    setLabel(tr("Audio output device"));
#ifdef USING_ALSA
    QString dflt = "ALSA:default";
#elif USING_PULSEOUTPUT
    QString dflt = "PulseAudio:default";
#elif CONFIG_DARWIN
    QString dflt = "CoreAudio:";
#elif _WIN32
    QString dflt = "Windows:";
#elif ANDROID
    QString dflt = "OpenSLES:";
#else
    QString dflt = "NULL";
#endif
    QString current = gCoreContext->GetSetting(QString("AudioOutputDevice"),
                                               dflt);
    addSelection(current, current, true);

    connect(this, SIGNAL(valueChanged(StandardSetting *)),
            this, SLOT(AudioDescriptionHelp(StandardSetting *)));
}

void AudioDeviceComboBox::edit(MythScreenType * screen)
{
    AudioRescan();
    HostComboBoxSetting::edit(screen);
}

void AudioDeviceComboBox::AudioRescan()
{
    AudioOutput::ADCVect &vect = m_parent->AudioDeviceVect();
    AudioOutput::ADCVect::const_iterator it;

    if (vect.empty())
        return;

    QString value = getValue();
    clearSelections();

    // Adding the current value first avoids marking the setting as changed
    addSelection(value, value, true);
    for (it = vect.begin(); it != vect.end(); ++it)
    {
        if (value != it->name)
            addSelection(it->name, it->name);
    }
}

void AudioDeviceComboBox::AudioDescriptionHelp(StandardSetting * setting)
{
    QString desc = m_parent->AudioDeviceMap().value(setting->getValue()).desc;
    setHelpText(desc);
}

AudioConfigScreen::AudioConfigScreen(MythScreenStack *parent, const char *name,
                                     GroupSetting *groupSetting)
    : StandardSettingDialog(parent, name, groupSetting)
{
}

void AudioConfigScreen::Load(void)
{
    SetBusyPopupMessage(tr("Scanning for available audio devices"));
    StandardSettingDialog::Load();
}

void AudioConfigScreen::Init(void)
{
    StandardSettingDialog::Init();

    AudioConfigSettings *settings =
        static_cast<AudioConfigSettings*>(GetGroupSettings());
    settings->CheckConfiguration();
}

AudioConfigSettings::AudioConfigSettings() :
    GroupSetting()
{
    setLabel(tr("Audio System"));

    addChild((m_OutputDevice = new AudioDeviceComboBox(this)));
        // Rescan button

    ButtonStandardSetting *rescan = new ButtonStandardSetting("rescan");
    rescan->setLabel(tr("Rescan"));
    rescan->setHelpText(tr("Rescan for available audio devices. "
                           "Current entry will be checked and "
                           "capability entries populated."));
    addChild(rescan);
    connect(rescan, SIGNAL(clicked()), SLOT(AudioRescan()));

    // digital settings
    m_triggerDigital = new GroupSetting();
    m_triggerDigital->setLabel(tr("Digital Audio Capabilities"));
    m_triggerDigital->addChild(m_AC3PassThrough = AC3PassThrough());
    m_triggerDigital->addChild(m_DTSPassThrough = DTSPassThrough());
    m_triggerDigital->addChild(m_EAC3PassThrough = EAC3PassThrough());
    m_triggerDigital->addChild(m_TrueHDPassThrough = TrueHDPassThrough());
    m_triggerDigital->addChild(m_DTSHDPassThrough = DTSHDPassThrough());
    addChild(m_triggerDigital);

    addChild((m_MaxAudioChannels = MaxAudioChannels()));
    addChild((m_AudioUpmix = AudioUpmix()));
    addChild((m_AudioUpmixType = AudioUpmixType()));
    addChild(MythControlsVolume());

    //Advanced Settings
    GroupSetting * advancedSettings = new GroupSetting();
    advancedSettings->setLabel(tr("Advanced Audio Settings"));
    advancedSettings->setHelpText(tr("Enable extra audio settings. Under most "
                                     "usage all options should be left alone"));
    addChild(advancedSettings);
    m_PassThroughOverride = PassThroughOverride();
    advancedSettings->addChild(m_PassThroughOverride);
    m_PassThroughDeviceOverride = PassThroughOutputDevice();
    advancedSettings->addChild(m_PassThroughDeviceOverride);
    m_PassThroughDeviceOverride->setEnabled(m_PassThroughOverride->boolValue());
    connect(m_PassThroughOverride, SIGNAL(valueChanged(bool)),
            m_PassThroughDeviceOverride, SLOT(setEnabled(bool)));

    StandardSetting *srcqualityoverride = SRCQualityOverride();
    srcqualityoverride->addTargetedChild("1", SRCQuality());
    addChild(srcqualityoverride);

    advancedSettings->addChild(Audio48kOverride());
#if USING_ALSA
    advancedSettings->addChild(SPDIFRateOverride());
#endif

    advancedSettings->addChild(HBRPassthrough());

    advancedSettings->addChild(m_MPCM = MPCM());

    addChild(m_audioTest = new AudioTest());

        // Set slots
    connect(m_MaxAudioChannels, SIGNAL(valueChanged(StandardSetting *)),
            this, SLOT(UpdateVisibility(StandardSetting *)));
    connect(m_OutputDevice, SIGNAL(valueChanged(StandardSetting *)),
            this, SLOT(UpdateCapabilities()));
    connect(m_AC3PassThrough, SIGNAL(valueChanged(StandardSetting *)),
            this, SLOT(UpdateCapabilitiesAC3()));

    connect(m_DTSPassThrough, SIGNAL(valueChanged(StandardSetting *)),
            this, SLOT(UpdateCapabilities()));
    connect(m_EAC3PassThrough, SIGNAL(valueChanged(StandardSetting *)),
            this, SLOT(UpdateCapabilities()));
    connect(m_TrueHDPassThrough, SIGNAL(valueChanged(StandardSetting *)),
            this, SLOT(UpdateCapabilities()));
    connect(m_DTSHDPassThrough, SIGNAL(valueChanged(StandardSetting *)),
            this, SLOT(UpdateCapabilities()));
    //Slot for audio test
    connect(m_OutputDevice, SIGNAL(valueChanged(StandardSetting *)),
            this, SLOT(UpdateAudioTest()));
    connect(m_MaxAudioChannels, SIGNAL(valueChanged(StandardSetting *)),
            this, SLOT(UpdateAudioTest()));
}

void AudioConfigSettings::CheckConfiguration(void)
{
    QString name = m_OutputDevice->getValue();
    AudioOutput::AudioDeviceConfig *adc =
        AudioOutput::GetAudioDeviceConfig(name, name, true);
    if (adc)
    {
        if (adc->settings.IsInvalid())
        {
            QString msg = tr("%1 is invalid or not useable.").arg(name);

            ShowOkPopup(msg);

            LOG(VB_GENERAL, LOG_ERR, QString("Audio device %1 isn't usable")
                .arg(name));
        }
        delete adc;
    }

    if (!CheckPassthrough())
    {
        QString name = m_PassThroughDeviceOverride->getValue();
        QString msg = tr("Passthrough device is invalid or not useable. Check "
                         "configuration in Advanced Settings:") +
            name;

        ShowOkPopup(msg);

        LOG(VB_GENERAL, LOG_ERR, QString("Audio device %1 isn't usable")
            .arg(name));
    }
}

void AudioConfigSettings::Load()
{
    StandardSetting::Load();
    AudioRescan();
}

void AudioConfigSettings::AudioRescan()
{
    if (!slotlock.tryLock())
        return;

    AudioOutput::ADCVect* list = AudioOutput::GetOutputList();
    AudioOutput::ADCVect::const_iterator it;

    audiodevs.clear();
    for (it = list->begin(); it != list->end(); ++it)
        audiodevs.insert(it->name, *it);

    devices = *list;
    delete list;

    QString name = m_OutputDevice->getValue();
    if (!audiodevs.contains(name))
    {
        // Scan for possible custom entry that isn't in the list
        AudioOutput::AudioDeviceConfig *adc =
            AudioOutput::GetAudioDeviceConfig(name, name, true);
        audiodevs.insert(name, *adc);
        devices.append(*adc);
        delete adc;
    }
    m_OutputDevice->AudioRescan();
    slotlock.unlock();
    UpdateCapabilities();
}

void AudioConfigSettings::UpdateVisibility(StandardSetting *)
{
    if (!m_MaxAudioChannels || !m_AudioUpmix || !m_AudioUpmixType)
        return;

    int cur_speakers = m_MaxAudioChannels->getValue().toInt();
    m_AudioUpmix->setEnabled(cur_speakers > 2);
    m_AudioUpmixType->setEnabled(cur_speakers > 2);
}

AudioOutputSettings AudioConfigSettings::UpdateCapabilities(
    bool restore, bool AC3)
{
    int max_speakers = 8;
    int realmax_speakers = 8;

    bool invalid = false;
    QString out;

    if (m_OutputDevice)
        out = m_OutputDevice->getValue();

    if (!out.isEmpty())
    {
        restore = out != m_lastAudioDevice;
        m_lastAudioDevice = out;
    }

    AudioOutputSettings settings, settingsdigital;

        // Test if everything is set yet
    if (!m_OutputDevice    || !m_MaxAudioChannels   ||
        !m_AC3PassThrough  || !m_DTSPassThrough     ||
        !m_EAC3PassThrough || !m_TrueHDPassThrough  || !m_DTSHDPassThrough)
        return settings;

    if (!slotlock.tryLock()) // Doing a rescan of channels
        return settings;

    bool bAC3 = true;
    //bool bDTS = true;
    bool bLPCM = true;
    bool bEAC3 = true;
    bool bTRUEHD = true;
    bool bDTSHD = true;

    if (!audiodevs.contains(out))
    {
        LOG(VB_AUDIO, LOG_ERR, QString("Update not found (%1)").arg(out));
        invalid = true;
    }
    else
    {
        bool bForceDigital = m_PassThroughOverride->boolValue();

        settings = audiodevs.value(out).settings;
        settingsdigital = bForceDigital ?
            audiodevs.value(m_PassThroughDeviceOverride->getValue())
            .settings : settings;

        realmax_speakers = max_speakers = settings.BestSupportedChannels();

        bAC3  = settingsdigital.canFeature(FEATURE_AC3) &&
            m_AC3PassThrough->boolValue();
        //bDTS  = settingsdigital.canFeature(FEATURE_DTS)  &&
        //    m_DTSPassThrough->boolValue();
        bLPCM = settings.canFeature(FEATURE_LPCM) &&
            !gCoreContext->GetNumSetting("StereoPCM", false);
        bEAC3 = settingsdigital.canFeature(FEATURE_EAC3) &&
            !gCoreContext->GetNumSetting("Audio48kOverride", false);
        bTRUEHD = settingsdigital.canFeature(FEATURE_TRUEHD) &&
            !gCoreContext->GetNumSetting("Audio48kOverride", false) &&
            gCoreContext->GetNumSetting("HBRPassthru", true);
        bDTSHD = settingsdigital.canFeature(FEATURE_DTSHD) &&
            !gCoreContext->GetNumSetting("Audio48kOverride", false);

        if (max_speakers > 2 && !bLPCM)
            max_speakers = 2;
        if (max_speakers == 2 && bAC3)
        {
            max_speakers = 6;
            if (AC3)
            {
                restore = true;
            }
        }
    }

    m_triggerDigital->setEnabled(invalid || settingsdigital.canFeature(
                                   FEATURE_AC3 | FEATURE_DTS | FEATURE_EAC3 |
                                   FEATURE_TRUEHD | FEATURE_DTSHD));
    m_EAC3PassThrough->setEnabled(bEAC3);
    m_TrueHDPassThrough->setEnabled(bTRUEHD);
    m_DTSHDPassThrough->setEnabled(bDTSHD);

    int cur_speakers = m_MaxAudioChannels->getValue().toInt();
    if (cur_speakers > m_maxspeakers)
    {
        m_maxspeakers = m_MaxAudioChannels->getValue().toInt();
    }
    if (restore)
    {
        cur_speakers = m_maxspeakers;
    }

    if (cur_speakers > max_speakers)
    {
        LOG(VB_AUDIO, LOG_INFO, QString("Reset device %1").arg(out));
        cur_speakers = max_speakers;
    }

    // Remove everything and re-add available channels
    m_MaxAudioChannels->clearSelections();
    for (int i = 1; i <= max_speakers; i++)
    {
        if (invalid || settings.IsSupportedChannels(i) ||
            settingsdigital.IsSupportedChannels(i))
        {
            QString txt;

            switch (i)
            {
                case 2:
                    txt = QCoreApplication::translate("(Common)", "Stereo");
                    break;
                case 6:
                    txt = QCoreApplication::translate("(Common)", "5.1");
                    break;
                case 8:
                    txt = QCoreApplication::translate("(Common)", "7.1");
                    break;
                default:
                    continue;
            }
            m_MaxAudioChannels->addSelection(txt, QString::number(i),
                                             i == cur_speakers);
        }
    }

    setMPCMEnabled(settings.canPassthrough() >= 0);

        // Return values is used by audio test
        // where we mainly are interested by the number of channels
        // if we support AC3 and/or LPCM
    settings.SetBestSupportedChannels(cur_speakers);
    settings.setFeature(bAC3, FEATURE_AC3);
    settings.setFeature(bLPCM && realmax_speakers > 2, FEATURE_LPCM);

    slotlock.unlock();
    return settings;
}

AudioOutputSettings AudioConfigSettings::UpdateCapabilitiesAC3(void)
{
    return UpdateCapabilities(false, true);
}


HostComboBoxSetting *AudioConfigSettings::MaxAudioChannels()
{
    QString name = "MaxChannels";

    HostComboBoxSetting *gc = new HostComboBoxSetting(name, false);

    gc->setLabel(tr("Speaker configuration"));

    gc->addSelection(QCoreApplication::translate("(Common)", "Stereo"),
                     "2", true); // default

    gc->setHelpText(tr("Select the maximum number of audio "
                       "channels supported by your receiver "
                       "and speakers."));
    return gc;
}

HostCheckBoxSetting *AudioConfigSettings::AudioUpmix()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("AudioDefaultUpmix");

    gc->setLabel(tr("Upconvert stereo to 5.1 surround"));

    gc->setValue(true);

    gc->setHelpText(tr("If enabled, MythTV will upconvert stereo "
                    "to 5.1 audio. You can enable or disable "
                    "the upconversion during playback at any time."));
    return gc;
}

HostComboBoxSetting *AudioConfigSettings::AudioUpmixType()
{
    HostComboBoxSetting *gc = new HostComboBoxSetting("AudioUpmixType", false);

    gc->setLabel(tr("Upmix Quality"));

    gc->addSelection(tr("Passive"), "0");
    gc->addSelection(tr("Hall", "Upmix Quality"), "3");
    gc->addSelection(tr("Good", "Upmix Quality"), "1");
    gc->addSelection(tr("Best", "Upmix Quality"), "2", true);  // default

    gc->setHelpText(tr("Set the audio surround-upconversion quality."));

    return gc;
}

HostCheckBoxSetting *AudioConfigSettings::AC3PassThrough()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("AC3PassThru");

    gc->setLabel(tr("Dolby Digital"));

    gc->setValue(false);

    gc->setHelpText(tr("Enable if your amplifier or sound decoder "
                       "supports AC-3/Dolby Digital. You must use a digital "
                       "connection. Uncheck if using an analog connection."));
    return gc;
}

HostCheckBoxSetting *AudioConfigSettings::DTSPassThrough()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("DTSPassThru");

    gc->setLabel(tr("DTS"));

    gc->setValue(false);

    gc->setHelpText(tr("Enable if your amplifier or sound decoder supports "
                       "DTS. You must use a digital connection. Uncheck "
                       "if using an analog connection"));
    return gc;
}

HostCheckBoxSetting *AudioConfigSettings::EAC3PassThrough()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("EAC3PassThru");

    gc->setLabel(tr("E-AC-3"));

    gc->setValue(false);

    gc->setHelpText(tr("Enable if your amplifier or sound decoder supports "
                       "E-AC-3 (DD+). You must use a HDMI connection."));
    return gc;
}

HostCheckBoxSetting *AudioConfigSettings::TrueHDPassThrough()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("TrueHDPassThru");

    gc->setLabel(tr("TrueHD"));

    gc->setValue(false);

    gc->setHelpText(tr("Enable if your amplifier or sound decoder supports "
                        "Dolby TrueHD. You must use a HDMI connection."));
    return gc;
}

HostCheckBoxSetting *AudioConfigSettings::DTSHDPassThrough()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("DTSHDPassThru");

    gc->setLabel(tr("DTS-HD"));

    gc->setValue(false);

    gc->setHelpText(tr("Enable if your amplifier or sound decoder supports "
                       "DTS-HD. You must use a HDMI connection."));
    return gc;
}

bool AudioConfigSettings::CheckPassthrough()
{
    bool ok = true;

    if (m_PassThroughOverride->boolValue())
    {
        QString name = m_PassThroughDeviceOverride->getValue();
        AudioOutput::AudioDeviceConfig *adc =
            AudioOutput::GetAudioDeviceConfig(name, name, true);
        if (adc->settings.IsInvalid())
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Passthru device %1 isn't usable "
                        "Check audio configuration").arg(name));
            ok = false;
        }
        // add it to list of known devices
        audiodevs.insert(name, *adc);
        devices.append(*adc);
        delete adc;
    }
    return ok;
}

static void fillSelectionsFromDir(HostComboBoxSetting *comboBox,
                                  const QDir& dir, bool absPath = true)

{
    QFileInfoList il = dir.entryInfoList();

    for (QFileInfoList::Iterator it = il.begin();
                                 it != il.end();
                                ++it )
    {
        QFileInfo &fi = *it;

        if (absPath)
            comboBox->addSelection( fi.absoluteFilePath() );
        else
            comboBox->addSelection( fi.fileName() );
    }
}

void AudioConfigSettings::UpdateAudioTest()
{
    AudioOutputSettings settings = UpdateCapabilities(false);
    QString out = m_OutputDevice->getValue();
    QString passthrough =
        m_PassThroughOverride->boolValue() ?
        m_PassThroughDeviceOverride->getValue(): QString::null;
    int channels = m_MaxAudioChannels->getValue().toInt();

    m_audioTest->UpdateCapabilities(out, passthrough, channels, settings);
}

AudioTestThread::AudioTestThread(QObject *parent,
                                 QString main, QString passthrough,
                                 int channels,
                                 AudioOutputSettings &settings,
                                 bool hd) :
    MThread("AudioTest"),
    m_parent(parent), m_channels(channels), m_device(main),
    m_passthrough(passthrough), m_interrupted(false), m_channel(-1), m_hd(hd)
{
    m_format = hd ? settings.BestSupportedFormat() : FORMAT_S16;
    m_samplerate = hd ? settings.BestSupportedRate() : 48000;

    m_audioOutput = AudioOutput::OpenAudio(m_device, m_passthrough,
                                           m_format, m_channels,
                                           0, m_samplerate,
                                           AUDIOOUTPUT_VIDEO,
                                           true, false, 0, &settings);
    if (result().isEmpty())
    {
        m_audioOutput->Pause(true);
    }
}

QEvent::Type ChannelChangedEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

AudioTestThread::~AudioTestThread()
{
    cancel();
    wait();
    if (m_audioOutput)
        delete m_audioOutput;
}

void AudioTestThread::cancel()
{
    m_interrupted = true;
}

QString AudioTestThread::result()
{
    QString errMsg;
    if (!m_audioOutput)
        errMsg = tr("Unable to create AudioOutput.");
    else
        errMsg = m_audioOutput->GetError();
    return errMsg;
}

void AudioTestThread::setChannel(int channel)
{
    m_channel = channel;
}

void AudioTestThread::run()
{
    RunProlog();
    m_interrupted = false;
    int smptelayout[7][8] = {
        { 0, 1, 1 },                    //stereo
        { },                            //not used
        { },                            //not used
        { 0, 2, 1, 4, 3 },              //5.0
        { 0, 2, 1, 5, 4, 3 },           //5.1
        { 0, 2, 1, 6, 4, 5, 3 },        //6.1
        { 0, 2, 1, 7, 5, 4, 6, 3 },     //7.1
    };

    if (m_audioOutput)
    {
        char *frames_in = new char[m_channels * 1024 * sizeof(int32_t) + 15];
        char *frames = (char *)(((long)frames_in + 15) & ~0xf);

        m_audioOutput->Pause(false);

        int begin = 0;
        int end = m_channels;
        if (m_channel >= 0)
        {
            begin = m_channel;
            end = m_channel + 1;
        }
        while (!m_interrupted)
        {
            for (int i = begin; i < end && !m_interrupted; i++)
            {
                int current = smptelayout[m_channels - 2][i];

                if (m_parent)
                {
                    QString channel;

                    switch(current)
                    {
                        case 0:
                            channel = "frontleft";
                            break;
                        case 1:
                            channel = "frontright";
                            break;
                        case 2:
                            channel = "center";
                            break;
                        case 3:
                            channel = "lfe";
                            break;
                        case 4:
                            if (m_channels == 6)
                                channel = "surroundleft";
                            else if (m_channels == 7)
                                channel = "rearright";
                            else
                                channel = "rearleft";
                            break;
                        case 5:
                            if (m_channels == 6)
                                channel = "surroundright";
                            else if (m_channels == 7)
                                channel = "surroundleft";
                            else
                                channel = "rearright";
                            break;
                        case 6:
                            if (m_channels == 7)
                                channel = "surroundright";
                            else
                                channel = "surroundleft";
                            break;
                        case 7:
                            channel = "surroundright";
                            break;
                    }
                    QCoreApplication::postEvent(
                        m_parent, new ChannelChangedEvent(channel,
                                                          m_channel < 0));
                    LOG(VB_AUDIO, LOG_INFO, QString("AudioTest: %1 (%2->%3)")
                            .arg(channel).arg(i).arg(current));
                }

                    // play sample sound for about 3s
                int top = m_samplerate / 1000 * 3;
                for (int j = 0; j < top && !m_interrupted; j++)
                {
                    AudioOutputUtil::GeneratePinkFrames(frames, m_channels,
                                                        current, 1000,
                                                        m_hd ? 32 : 16);
                    if (!m_audioOutput->AddFrames(frames, 1000 , -1))
                    {
                        LOG(VB_AUDIO, LOG_ERR, "AddData() Audio buffer "
                                               "overflow, audio data lost!");
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(m_audioOutput->LengthLastData()));
                }
                m_audioOutput->Drain();
                m_audioOutput->Pause(true);
                std::this_thread::sleep_for(std::chrono::milliseconds(500)); // .5s pause
                m_audioOutput->Pause(false);
            }
            if (m_channel >= 0)
                break;
        }
        m_audioOutput->Pause(true);

        delete[] frames_in;
    }
    RunEpilog();
}

AudioTest::AudioTest()
    : GroupSetting(),
      m_at(NULL),
      m_quality(false)
{
    int channels = 2;

    m_channels = gCoreContext->GetNumSetting("TestingChannels", channels);
    setLabel(tr("Audio Configuration Testing"));
    setHelpText(tr("Will play a test pattern on all configured "
                   "speakers"));

    m_frontleft = new ButtonStandardSetting("0");
    m_frontleft->setLabel(tr("Front Left"));
    m_frontleft->setHelpText(tr("Start front left channel test"));
    addChild(m_frontleft);
    connect(m_frontleft,
            SIGNAL(clicked()), this, SLOT(toggle()));

    m_frontright = new ButtonStandardSetting(m_channels == 2 ? "1" : "2");
    m_frontright->setLabel(tr("Front Right"));
    m_frontright->setHelpText(tr("Start front right channel test"));
    addChild(m_frontright);
    connect(m_frontright,
            SIGNAL(clicked()), this, SLOT(toggle()));

    m_rearleft = new ButtonStandardSetting("5");
    m_rearleft->setLabel(tr("Rear Left"));
    m_rearleft->setHelpText(tr("Start rear left channel test"));
    addChild(m_rearleft);
    connect(m_rearleft,
            SIGNAL(clicked()), this, SLOT(toggle()));

    m_rearright = new ButtonStandardSetting("4");
    m_rearright->setLabel(tr("Rear Right"));
    m_rearright->setHelpText(tr("Start rear right channel test"));
    addChild(m_rearright);
    connect(m_rearright,
            SIGNAL(clicked()), this, SLOT(toggle()));

    m_lfe = new ButtonStandardSetting(m_channels == 6 ? "5" :
                                      m_channels == 7 ? "6" : "7");
    m_lfe->setLabel(tr("LFE"));
    m_lfe->setHelpText(tr("Start LFE channel test"));
    addChild(m_lfe);
    connect(m_lfe,
            SIGNAL(clicked()), this, SLOT(toggle()));

    m_surroundleft = new ButtonStandardSetting(m_channels == 6 ? "4" :
                                               m_channels == 7 ? "5" : "6");
    m_surroundleft->setLabel(tr("Surround Left"));
    m_surroundleft->setHelpText(tr("Start surround left channel test"));
    addChild(m_surroundleft);
    connect(m_surroundleft,
            SIGNAL(clicked()), this, SLOT(toggle()));

    m_surroundright = new ButtonStandardSetting("3");
    m_surroundright->setLabel(tr("Surround Right"));
    m_surroundright->setHelpText(tr("Start surround right channel test"));
    addChild(m_surroundright);
    connect(m_surroundright,
            SIGNAL(clicked()), this, SLOT(toggle()));

    m_center = new ButtonStandardSetting("1");
    m_center->setLabel(tr("Center"));
    m_center->setHelpText(tr("Start center channel test"));
    addChild(m_center);
    connect(m_center,
            SIGNAL(clicked()), this, SLOT(toggle()));

    m_startButton = new ButtonStandardSetting("start");
    m_startButton->setLabel(tr("Test All"));
    m_startButton->setHelpText(tr("Start all channels test"));
    addChild(m_startButton);
    connect(m_startButton, SIGNAL(clicked()), this, SLOT(toggle()));

    m_hd = new TransMythUICheckBoxSetting();
    m_hd->setLabel(tr("Use Highest Quality Mode"));
    m_hd->setHelpText(tr("Use the highest audio quality settings "
                         "supported by your audio card. This will be "
                         "a good place to start troubleshooting "
                         "potential errors"));
    addChild(m_hd);
    connect(m_hd, SIGNAL(valueChanged(QString)), this, SLOT(togglequality()));
}

AudioTest::~AudioTest()
{
    if (m_at)
    {
        m_at->cancel();
        m_at->wait();
        delete m_at;
    }
}


void AudioTest::UpdateCapabilities(const QString &main,
                                   const QString &passthrough,
                                   int channels,
                                   const AudioOutputSettings &settings)
{
    m_main = main;
    m_passthrough = passthrough;
    m_channels = channels;
    m_settings = settings;
    m_rearleft->setEnabled(m_channels >= 8);
    m_rearright->setEnabled(m_channels >= 7);
    m_lfe->setEnabled(m_channels >= 6);
    m_surroundleft->setEnabled(m_channels >= 5);
    m_surroundright->setEnabled(m_channels >= 5);
    m_center->setEnabled(m_channels >= 5);
}

void AudioTest::toggle()
{
    if (this->sender() == m_startButton)
    {
        if (m_at && m_at->isRunning())
            cancelTest();
        else
        {
            prepareTest();
            m_at->setChannel(-1);
            m_at->start();
            m_startButton->setLabel(QCoreApplication::translate("(Common)",
                                                                "Stop"));
        }
        return;
    }
    if (m_at && m_at->isRunning())
    {
        m_at->cancel();
        m_at->wait();
    }

    prepareTest();

    int channel = 1;

    if (this->sender() == m_frontleft)
        channel = 0;
    else if (this->sender() == m_frontright)
        channel = (m_channels == 2) ? 1 : 2;
    else if (this->sender() == m_rearleft)
        channel = 5;
    else if (this->sender() == m_rearright)
        channel = 4;
    else if (this->sender() == m_lfe)
        channel = ((m_channels == 6) ? 5 :((m_channels == 7) ? 6 : 7));
    else if (this->sender() == m_surroundleft)
        channel = ((m_channels == 6) ? 4 : ((m_channels == 7) ? 5 : 6));
    else if (this->sender() == m_surroundright)
        channel = 3;
    else if (this->sender() == m_center)
        channel = 1;

    m_at->setChannel(channel);

    m_at->start();
}

void AudioTest::togglequality()
{
    cancelTest();
    m_quality = m_hd->boolValue();
}

void AudioTest::cancelTest()
{
    if (m_at && m_at->isRunning())
    {
        m_at->cancel();
        m_startButton->setLabel(tr("Test All"));
    }
}

void AudioTest::prepareTest()
{
    cancelTest();
    if (m_at)
    {
        m_at->cancel();
        m_at->wait();
        delete m_at;
    }

    m_at = new AudioTestThread(this, m_main, m_passthrough, m_channels,
                               m_settings, m_quality);
    if (!m_at->result().isEmpty())
    {
        QString msg = tr("Audio device is invalid or not useable.");
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

        MythConfirmationDialog *mcd = new MythConfirmationDialog(mainStack,
                                                                 msg, false);

        if (mcd->Create())
            mainStack->AddScreen(mcd);
        else
            delete mcd;
    }
}

bool AudioTest::event(QEvent *event)
{
    if (event->type() != ChannelChangedEvent::kEventType)
        return QObject::event(event); //not handled

    ChannelChangedEvent *cce = (ChannelChangedEvent*)(event);
    QString channel          = cce->channel;

    if (!cce->fulltest)
        return false;

    bool fl, fr, c, lfe, sl, sr, rl, rr;
    fl = fr = c = lfe = sl = sr = rl = rr = false;

    if (channel == "frontleft")
    {
        fl = true;
    }
    else if (channel == "frontright")
    {
        fr = true;
    }
    else if (channel == "center")
    {
        c = true;
    }
    else if (channel == "lfe")
    {
        lfe = true;
    }
    else if (channel == "surroundleft")
    {
        sl = true;
    }
    else if (channel == "surroundright")
    {
        sr = true;
    }
    else if (channel == "rearleft")
    {
        rl = true;
    }
    else if (channel == "rearright")
    {
        rr = true;
    }
    if (m_frontleft)
        m_frontleft->setEnabled(fl);
    if (m_frontright)
        m_frontright->setEnabled(fr);
    if (m_center)
        m_center->setEnabled(c);
    if (m_surroundleft)
        m_surroundleft->setEnabled(sl);
    if (m_surroundright)
        m_surroundright->setEnabled(sr);
    if (m_rearleft)
        m_rearleft->setEnabled(rl);
    if (m_rearright)
        m_rearright->setEnabled(rr);
    if (m_lfe)
        m_lfe->setEnabled(lfe);
    return false;
}


HostCheckBoxSetting *AudioConfigSettings::MythControlsVolume()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("MythControlsVolume");

    gc->setLabel(tr("Use internal volume controls"));

    gc->setValue(true);

    gc->setHelpText(tr("If enabled, MythTV will control the PCM and "
                       "master mixer volume. Disable this option if you "
                       "prefer to control the volume externally (for "
                       "example, using your amplifier) or if you use an "
                       "external mixer program."));

    gc->addTargetedChild("1", MixerDevice());
    gc->addTargetedChild("1", MixerControl());
    gc->addTargetedChild("1", MixerVolume());
    gc->addTargetedChild("1", PCMVolume());
    return gc;
}

HostComboBoxSetting *AudioConfigSettings::MixerDevice()
{
    HostComboBoxSetting *gc = new HostComboBoxSetting("MixerDevice", true);
    gc->setLabel(tr("Mixer device"));

#ifdef USING_OSS
    QDir dev("/dev", "mixer*", QDir::Name, QDir::System);
    fillSelectionsFromDir(gc, dev);

    dev.setPath("/dev/sound");
    if (dev.exists())
    {
        fillSelectionsFromDir(gc, dev);
    }
#endif
#ifdef USING_ALSA
    gc->addSelection("ALSA:default", "ALSA:default");
#endif
#ifdef _WIN32
    gc->addSelection("DirectX:", "DirectX:");
    gc->addSelection("Windows:", "Windows:");
#endif
#ifdef ANDROID
    gc->addSelection("OpenSLES:", "OpenSLES:");
#endif
#ifdef USING_OPENMAX
    gc->addSelection("OpenMAX:",   "OpenMAX:");
#endif
#if !defined(_WIN32)
    gc->addSelection(tr("software"), "software");
#endif

    gc->setHelpText(tr("Setting the mixer device to \"%1\" lets MythTV control "
                       "the volume of all audio at the expense of a slight "
                       "quality loss.")
                       .arg(tr("software")));

    return gc;
}

const char* AudioConfigSettings::MixerControlControls[] =
    {QT_TR_NOOP("PCM"),
     QT_TR_NOOP("Master")};

HostComboBoxSetting *AudioConfigSettings::MixerControl()
{
    HostComboBoxSetting *gc = new HostComboBoxSetting("MixerControl", true);

    gc->setLabel(tr("Mixer controls"));

    for (unsigned int i = 0; i < sizeof(MixerControlControls) / sizeof(char*);
         ++i)
    {
        gc->addSelection(tr(MixerControlControls[i]),
                         MixerControlControls[i]);
    }

    gc->setHelpText(tr("Changing the volume adjusts the selected mixer."));

    return gc;
}

HostSpinBoxSetting *AudioConfigSettings::MixerVolume()
{
    HostSpinBoxSetting *gs = new HostSpinBoxSetting("MasterMixerVolume", 0, 100,
                                                    1);

    gs->setLabel(tr("Master mixer volume"));

    gs->setValue(70);

    gs->setHelpText(tr("Initial volume for the Master mixer. This affects "
                       "all sound created by the audio device. Note: Do not "
                       "set this too low."));
    return gs;
}

HostSpinBoxSetting *AudioConfigSettings::PCMVolume()
{
    HostSpinBoxSetting *gs = new HostSpinBoxSetting("PCMMixerVolume", 0, 100,
                                                    1);

    gs->setLabel(tr("PCM mixer volume"));

    gs->setValue(70);

    gs->setHelpText(tr("Initial volume for PCM output. Using the volume "
                       "keys in MythTV will adjust this parameter."));
    return gs;
}

HostCheckBoxSetting *AudioConfigSettings::MPCM()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("StereoPCM");

    gc->setLabel(tr("Stereo PCM Only"));

    gc->setValue(false);

    gc->setHelpText(tr("Enable if your amplifier or sound decoder only "
                       "supports 2 channel PCM (typically an old HDMI 1.0 "
                       "device). Multichannel audio will be re-encoded to "
                       "AC-3 when required"));
    return gc;
}

HostCheckBoxSetting *AudioConfigSettings::SRCQualityOverride()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("SRCQualityOverride");

    gc->setLabel(tr("Override SRC quality"));

    gc->setValue(false);

    gc->setHelpText(tr("Enable to override audio sample rate "
                       "conversion quality."));
    return gc;
}

HostComboBoxSetting *AudioConfigSettings::SRCQuality()
{
    HostComboBoxSetting *gc = new HostComboBoxSetting("SRCQuality", false);

    gc->setLabel(tr("Sample rate conversion"));

    gc->addSelection(tr("Disabled", "Sample rate conversion"), "-1");
    gc->addSelection(tr("Fastest", "Sample rate conversion"), "0");
    gc->addSelection(tr("Good", "Sample rate conversion"), "1", true); // default
    gc->addSelection(tr("Best", "Sample rate conversion"), "2");

    gc->setHelpText(tr("Set the quality of audio sample-rate "
                       "conversion. \"%1\" (default) provides the best "
                       "compromise between CPU usage and quality. \"%2\" "
                       "lets the audio device handle sample-rate conversion.")
                       .arg(tr("Good", "Sample rate conversion"))
                       .arg(tr("Disabled", "Sample rate conversion")));

    return gc;
}

HostCheckBoxSetting *AudioConfigSettings::Audio48kOverride()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("Audio48kOverride");

    gc->setLabel(tr("Force audio device output to 48kHz"));
    gc->setValue(false);

    gc->setHelpText(tr("Force audio sample rate to 48kHz. Some audio devices "
                        "will report various rates, but they ultimately "
                        "crash."));
    return gc;
}

HostCheckBoxSetting *AudioConfigSettings::PassThroughOverride()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("PassThruDeviceOverride");

    gc->setLabel(tr("Separate digital output device"));

    gc->setValue(false);

    gc->setHelpText(tr("Use a distinct digital output device from default. "
                        "(default is not checked)"));
    return gc;
}

HostComboBoxSetting *AudioConfigSettings::PassThroughOutputDevice()
{
    HostComboBoxSetting *gc = new HostComboBoxSetting("PassThruOutputDevice",
                                                      true);

    gc->setLabel(tr("Digital output device"));
    //TODO Is default not equivalent to PassThroughOverride off? if so
    //PassThruDeviceOverridedsetting could be removed
    gc->addSelection(QCoreApplication::translate("(Common)", "Default"),
                     "Default");
#ifdef _WIN32
    gc->addSelection("DirectX:Primary Sound Driver");
#else
    gc->addSelection("ALSA:iec958:{ AES0 0x02 }",
                     "ALSA:iec958:{ AES0 0x02 }");
    gc->addSelection("ALSA:hdmi", "ALSA:hdmi");
    gc->addSelection("ALSA:plughw:0,3", "ALSA:plughw:0,3");
#endif

    gc->setHelpText(tr("Audio output device to use for digital audio. This "
                       "value is currently only used with ALSA and DirectX "
                       "sound output."));
    return gc;
}

HostCheckBoxSetting *AudioConfigSettings::SPDIFRateOverride()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("SPDIFRateOverride");

    gc->setLabel(tr("SPDIF 48kHz rate override"));

    gc->setValue(false);

    gc->setHelpText(tr("ALSA only. By default, let ALSA determine the "
                       "passthrough sampling rate. If checked set the sampling "
                       "rate to 48kHz for passthrough. (default is not "
                       "checked)"));
    return gc;
}

HostCheckBoxSetting *AudioConfigSettings::HBRPassthrough()
{
    HostCheckBoxSetting *gc = new HostCheckBoxSetting("HBRPassthru");

    gc->setLabel(tr("HBR passthrough support"));

    gc->setValue(true);

    gc->setHelpText(tr("HBR support is required for TrueHD and DTS-HD "
                       "passthrough. If unchecked, Myth will limit the "
                       "passthrough bitrate to 6.144Mbit/s. This will "
                       "disable True-HD passthrough, however will allow "
                       "DTS-HD content to be sent as DTS-HD Hi-Res. (default "
                       "is checked)"));
    return gc;
}

void AudioConfigSettings::setMPCMEnabled(bool flag)
{
    m_MPCM->setEnabled(flag);
}
// vim:set sw=4 ts=4 expandtab:
