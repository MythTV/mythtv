
// -*- Mode: c++ -*-

// Standard UNIX C headers
#include <unistd.h>
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

extern "C" {
#include "libavformat/avformat.h"
}


class TriggeredItem : public TriggeredConfigurationGroup
{
  public:
    TriggeredItem(Setting *checkbox, ConfigurationGroup *group) :
        TriggeredConfigurationGroup(false, false, false, false)
    {
        setTrigger(checkbox);

        addTarget("1", group);
        addTarget("0", new VerticalConfigurationGroup(true));
    }
    TriggeredItem(Setting *checkbox, Setting *setting) :
        TriggeredConfigurationGroup(false, false, false, false)
    {
        setTrigger(checkbox);

        addTarget("1", setting);
        addTarget("0", new VerticalConfigurationGroup(false, false));
    }
};

AudioDeviceComboBox::AudioDeviceComboBox(AudioConfigSettings *parent) :
    HostComboBox("AudioOutputDevice", true), m_parent(parent)
{
    setLabel(tr("Audio output device"));
#ifdef USING_ALSA
    QString dflt = "ALSA:default";
#elif USING_PULSEOUTPUT
    QString dflt = "PulseAudio:default";
#elif CONFIG_DARWIN
    QString dflt = "CoreAudio:";
#elif USING_MINGW
    QString dflt = "Windows:";
#else
    QString dflt = "NULL";
#endif
    QString current = gCoreContext->GetSetting(QString("AudioOutputDevice"),
                                               dflt);
    addSelection(current, current, true);

    connect(this, SIGNAL(valueChanged(const QString&)),
            this, SLOT(AudioDescriptionHelp(const QString&)));
}

void AudioDeviceComboBox::AudioRescan()
{
    AudioOutput::ADCVect &vect = m_parent->AudioDeviceVect();
    AudioOutput::ADCVect::const_iterator it;

    if (vect.empty())
        return;

    QString value = getValue();
    clearSelections();
    resetMaxCount(vect.size());

    bool found = false;
    for (it = vect.begin(); it != vect.end(); ++it)
        addSelection(it->name, it->name,
                     value == it->name ? (found = true) : false);
    if (!found)
    {
        resetMaxCount(vect.size()+1);
        addSelection(value, value, true);
    }
        // For some reason, it adds an empty entry, remove it
    removeSelection(QString::null);
}

void AudioDeviceComboBox::AudioDescriptionHelp(const QString &device)
{
    QString desc = m_parent->AudioDeviceMap().value(device).desc;
    setHelpText(desc);
}

AudioConfigSettings::AudioConfigSettings(ConfigurationWizard *parent) :
    VerticalConfigurationGroup(false, true, false, false),
    m_OutputDevice(NULL),   m_MaxAudioChannels(NULL),
    m_AudioUpmix(NULL),     m_AudioUpmixType(NULL),
    m_AC3PassThrough(NULL), m_DTSPassThrough(NULL),
    m_EAC3PassThrough(NULL),m_TrueHDPassThrough(NULL), m_DTSHDPassThrough(NULL),
    m_parent(parent),       m_lastAudioDevice("")
{
    setLabel(tr("Audio System"));
    setUseLabel(false);

    ConfigurationGroup *devicegroup = new HorizontalConfigurationGroup(false,
                                                                       false);
    devicegroup->addChild((m_OutputDevice = new AudioDeviceComboBox(this)));
        // Rescan button
    TransButtonSetting *rescan = new TransButtonSetting("rescan");
    rescan->setLabel(tr("Rescan"));
    rescan->setHelpText(tr("Rescan for available audio devices. "
                           "Current entry will be checked and "
                           "capability entries populated."));
    devicegroup->addChild(rescan);
    connect(rescan, SIGNAL(pressed()), this, SLOT(AudioRescan()));
    addChild(devicegroup);

    QString name = m_OutputDevice->getValue();
    AudioOutput::AudioDeviceConfig *adc =
        AudioOutput::GetAudioDeviceConfig(name, name, true);
    if (adc->settings.IsInvalid())
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Audio device %1 isn't usable Check audio configuration")
                .arg(name));
    }
    audiodevs.insert(name, *adc);
    devices.append(*adc);

    delete adc;

    ConfigurationGroup *maingroup = new VerticalConfigurationGroup(false,
                                                                   false);
    addChild(maingroup);

    m_triggerDigital = new TransCheckBoxSetting();
    m_AC3PassThrough = AC3PassThrough();
    m_DTSPassThrough = DTSPassThrough();
    m_EAC3PassThrough = EAC3PassThrough();
    m_TrueHDPassThrough = TrueHDPassThrough();
    m_DTSHDPassThrough = DTSHDPassThrough();

    m_cgsettings = new HorizontalConfigurationGroup();
    m_cgsettings->setLabel(tr("Digital Audio Capabilities"));
    m_cgsettings->addChild(m_AC3PassThrough);
    m_cgsettings->addChild(m_DTSPassThrough);
    m_cgsettings->addChild(m_EAC3PassThrough);
    m_cgsettings->addChild(m_TrueHDPassThrough);
    m_cgsettings->addChild(m_DTSHDPassThrough);

    TriggeredItem *sub1 = new TriggeredItem(m_triggerDigital, m_cgsettings);

    maingroup->addChild(sub1);

    maingroup->addChild((m_MaxAudioChannels = MaxAudioChannels()));
    maingroup->addChild((m_AudioUpmix = AudioUpmix()));
    maingroup->addChild((m_AudioUpmixType = AudioUpmixType()));

    TransButtonSetting *test = new TransButtonSetting("test");
    test->setLabel(tr("Test"));
    test->setHelpText(tr("Will play a test pattern on all configured "
                         "speakers"));
    connect(test, SIGNAL(pressed()), this, SLOT(StartAudioTest()));
    addChild(test);

    TransButtonSetting *advanced = new TransButtonSetting("advanced");
    advanced->setLabel(tr("Advanced Audio Settings"));
    advanced->setHelpText(tr("Enable extra audio settings. Under most "
                             "usage all options should be left alone"));
    connect(advanced, SIGNAL(pressed()), this, SLOT(AudioAdvanced()));
    addChild(advanced);

        // Set slots
    connect(m_MaxAudioChannels, SIGNAL(valueChanged(const QString&)),
            this, SLOT(UpdateVisibility(const QString&)));
    connect(m_OutputDevice, SIGNAL(valueChanged(const QString&)),
            this, SLOT(UpdateCapabilities(const QString&)));
    connect(m_AC3PassThrough, SIGNAL(valueChanged(const QString&)),
            this, SLOT(UpdateCapabilitiesAC3()));

    m_maxspeakers = gCoreContext->GetNumSetting("MaxChannels", 2);
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
        if (adc->settings.IsInvalid())
        {
            QString msg = tr("%1 is invalid or not useable.").arg(name);

            MythPopupBox::showOkPopup(
                GetMythMainWindow(), 
                QCoreApplication::translate("(Common)", "Warning"), 
                msg);

            LOG(VB_GENERAL, LOG_ERR, QString("Audio device %1 isn't usable")
                    .arg(name));
        }
        audiodevs.insert(name, *adc);
        devices.append(*adc);
        delete adc;
    }
    m_OutputDevice->AudioRescan();
    if (!CheckPassthrough())
    {
        QString msg = tr("Passthrough device is invalid or not useable. Check "
                         "configuration in Advanced Settings:") +
            gCoreContext->GetSetting("PassThruOutputDevice");

        MythPopupBox::showOkPopup(
            GetMythMainWindow(), 
            QCoreApplication::translate("(Common)", "Warning"),
            msg);

        LOG(VB_GENERAL, LOG_ERR, QString("Audio device %1 isn't usable")
                .arg(name));
    }
    slotlock.unlock();
    UpdateCapabilities(QString::null);
}

void AudioConfigSettings::UpdateVisibility(const QString &device)
{
    if (!m_MaxAudioChannels || !m_AudioUpmix || !m_AudioUpmixType)
        return;

    int cur_speakers = m_MaxAudioChannels->getValue().toInt();
    m_AudioUpmix->setEnabled(cur_speakers > 2);
    m_AudioUpmixType->setEnabled(cur_speakers > 2);
}

AudioOutputSettings AudioConfigSettings::UpdateCapabilities(
    const QString &device, bool restore, bool AC3)
{
    int max_speakers = 8;
    int realmax_speakers = 8;

    bool invalid = false;

    if (device.length() > 0)
    {
        restore = device != m_lastAudioDevice;
        m_lastAudioDevice = device;
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

    QString out = m_OutputDevice->getValue();
    if (!audiodevs.contains(out))
    {
        LOG(VB_AUDIO, LOG_ERR, QString("Update not found (%1)").arg(out));
        invalid = true;
    }
    else
    {
        bool bForceDigital =
            gCoreContext->GetNumSetting("PassThruDeviceOverride", false);

        settings = audiodevs.value(out).settings;
        settingsdigital = bForceDigital ?
            audiodevs.value(gCoreContext->GetSetting("PassThruOutputDevice"))
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

    m_triggerDigital->setValue(invalid || settingsdigital.canFeature(
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
    m_MaxAudioChannels->resetMaxCount(3);
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
    return UpdateCapabilities(QString::null, false, true);
}

void AudioConfigSettings::AudioAdvanced()
{
    QString out  = m_OutputDevice->getValue();
    bool invalid = false;
    AudioOutputSettings settings;

    if (!audiodevs.contains(out))
    {
        invalid = true;
    }
    else
    {
        settings = audiodevs.value(out).settings;
    }

    bool LPCM1 = settings.canFeature(FEATURE_LPCM) &&
        gCoreContext->GetNumSetting("StereoPCM", false);

    AudioAdvancedSettingsGroup audiosettings(invalid ||
                                             (settings.canPassthrough() >= 0));

    if (audiosettings.exec() == kDialogCodeAccepted)
    {
        // Rescan audio list to check of override digital device
        AudioRescan();
        bool LPCM2 = settings.canFeature(FEATURE_LPCM) &&
            gCoreContext->GetNumSetting("StereoPCM", false);
            // restore speakers configure only of StereoPCM has changed and
            // we have LPCM capabilities
        UpdateCapabilities(QString::null, LPCM1 != LPCM2);
    }
}

HostComboBox *AudioConfigSettings::MaxAudioChannels()
{
    QString name = "MaxChannels";

    HostComboBox *gc = new HostComboBox(name, false);

    gc->setLabel(tr("Speaker configuration"));

    gc->addSelection(QCoreApplication::translate("(Common)", "Stereo"), 
                     "2", true); // default

    gc->setHelpText(tr("Select the maximum number of audio "
                       "channels supported by your receiver "
                       "and speakers."));
    return gc;
}

HostCheckBox *AudioConfigSettings::AudioUpmix()
{
    HostCheckBox *gc = new HostCheckBox("AudioDefaultUpmix");

    gc->setLabel(tr("Upconvert stereo to 5.1 surround"));

    gc->setValue(true);

    gc->setHelpText(tr("If enabled, MythTV will upconvert stereo "
                    "to 5.1 audio. You can enable or disable "
                    "the upconversion during playback at any time."));
    return gc;
}

HostComboBox *AudioConfigSettings::AudioUpmixType()
{
    HostComboBox *gc = new HostComboBox("AudioUpmixType",false);

    gc->setLabel(tr("Upmix Quality"));

    gc->addSelection(tr("Passive"), "0");
    gc->addSelection(tr("Hall", "Upmix Quality"), "3");
    gc->addSelection(tr("Good", "Upmix Quality"), "1");
    gc->addSelection(tr("Best", "Upmix Quality"), "2", true);  // default

    gc->setHelpText(tr("Set the audio surround-upconversion quality."));

    return gc;
}

HostCheckBox *AudioConfigSettings::AC3PassThrough()
{
    HostCheckBox *gc = new HostCheckBox("AC3PassThru");

    gc->setLabel(tr("Dolby Digital"));

    gc->setValue(false);

    gc->setHelpText(tr("Enable if your amplifier or sound decoder "
                       "supports AC-3/Dolby Digital. You must use a digital "
                       "connection. Uncheck if using an analog connection."));
    return gc;
}

HostCheckBox *AudioConfigSettings::DTSPassThrough()
{
    HostCheckBox *gc = new HostCheckBox("DTSPassThru");

    gc->setLabel(tr("DTS"));

    gc->setValue(false);

    gc->setHelpText(tr("Enable if your amplifier or sound decoder supports "
                       "DTS. You must use a digital connection. Uncheck "
                       "if using an analog connection"));
    return gc;
}

HostCheckBox *AudioConfigSettings::EAC3PassThrough()
{
    HostCheckBox *gc = new HostCheckBox("EAC3PassThru");

    gc->setLabel(tr("E-AC-3"));

    gc->setValue(false);

    gc->setHelpText(tr("Enable if your amplifier or sound decoder supports "
                       "E-AC-3 (DD+). You must use a HDMI connection."));
    return gc;
}

HostCheckBox *AudioConfigSettings::TrueHDPassThrough()
{
    HostCheckBox *gc = new HostCheckBox("TrueHDPassThru");

    gc->setLabel(tr("TrueHD"));

    gc->setValue(false);

    gc->setHelpText(tr("Enable if your amplifier or sound decoder supports "
                        "Dolby TrueHD. You must use a HDMI connection."));
    return gc;
}

HostCheckBox *AudioConfigSettings::DTSHDPassThrough()
{
    HostCheckBox *gc = new HostCheckBox("DTSHDPassThru");

    gc->setLabel(tr("DTS-HD"));

    gc->setValue(false);

    gc->setHelpText(tr("Enable if your amplifier or sound decoder supports "
                       "DTS-HD. You must use a HDMI connection."));
    return gc;
}

bool AudioConfigSettings::CheckPassthrough()
{
    bool ok = true;

    if (gCoreContext->GetNumSetting("PassThruDeviceOverride", false))
    {
        QString name = gCoreContext->GetSetting("PassThruOutputDevice");
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

void AudioConfigSettings::StartAudioTest()
{
    AudioOutputSettings settings = UpdateCapabilities(QString::null,
                                                      false, false);
    QString out = m_OutputDevice->getValue();
    QString passthrough =
        gCoreContext->GetNumSetting("PassThruDeviceOverride", false) ?
        gCoreContext->GetSetting("PassThruOutputDevice") : QString::null;
    int channels = m_MaxAudioChannels->getValue().toInt();

    AudioTestGroup audiotest(out, passthrough, channels, settings);

    audiotest.exec();
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
                    usleep(m_audioOutput->LengthLastData() * 1000);
                }
                m_audioOutput->Drain();
                m_audioOutput->Pause(true);
                usleep(500000); // .5s pause
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

AudioTest::AudioTest(QString main, QString passthrough,
                     int channels, AudioOutputSettings &settings)
    : VerticalConfigurationGroup(false, true, false, false),
      m_frontleft(NULL), m_frontright(NULL), m_center(NULL),
      m_surroundleft(NULL), m_surroundright(NULL),
      m_rearleft(NULL), m_rearright(NULL), m_lfe(NULL), m_button(NULL),
      m_hd(NULL), m_main(main), m_passthrough(passthrough),
      m_settings(settings), m_quality(false)
{
    m_channels = gCoreContext->GetNumSetting("TestingChannels", channels);
    setLabel(tr("Audio Configuration Testing"));

    m_at = new AudioTestThread(this, m_main, m_passthrough, m_channels,
                               m_settings, m_quality);
    if (!m_at->result().isEmpty())
    {
        QString msg = tr("%1 is invalid or not useable.").arg(main);

        MythPopupBox::showOkPopup(
            GetMythMainWindow(), tr("Warning"), msg);
        return;
    }

    m_button = new TransButtonSetting("start");
    m_button->setLabel(tr("Test All"));
    m_button->setHelpText(tr("Start all channels test"));
    connect(m_button, SIGNAL(pressed(QString)), this, SLOT(toggle(QString)));

    ConfigurationGroup *frontgroup =
        new HorizontalConfigurationGroup(false,
                                         false);
    ConfigurationGroup *middlegroup =
        new HorizontalConfigurationGroup(false,
                                         false);
    ConfigurationGroup *reargroup =
        new HorizontalConfigurationGroup(false,
                                         false);
    m_frontleft = new TransButtonSetting("0");
    m_frontleft->setLabel(tr("Front Left"));
    connect(m_frontleft,
            SIGNAL(pressed(QString)), this, SLOT(toggle(QString)));
    m_frontright = new TransButtonSetting(m_channels == 2 ? "1" : "2");
    m_frontright->setLabel(tr("Front Right"));
    connect(m_frontright,
            SIGNAL(pressed(QString)), this, SLOT(toggle(QString)));
    frontgroup->addChild(m_frontleft);

    switch(m_channels)
    {
        case 8:
            m_rearleft = new TransButtonSetting("5");
            m_rearleft->setLabel(tr("Rear Left"));
            connect(m_rearleft,
                    SIGNAL(pressed(QString)), this, SLOT(toggle(QString)));
            reargroup->addChild(m_rearleft);

        case 7:
            m_rearright = new TransButtonSetting("4");
            m_rearright->setLabel(m_channels == 8 ?
                                  tr("Rear Right") : tr("Rear Center"));
            connect(m_rearright,
                    SIGNAL(pressed(QString)), this, SLOT(toggle(QString)));

            reargroup->addChild(m_rearright);

        case 6:
            m_lfe = new TransButtonSetting(m_channels == 6 ? "5" :
                                           m_channels == 7 ? "6" : "7");
            m_lfe->setLabel(tr("LFE"));
            connect(m_lfe,
                    SIGNAL(pressed(QString)), this, SLOT(toggle(QString)));

        case 5:
            m_surroundleft = new TransButtonSetting(m_channels == 6 ? "4" :
                                                    m_channels == 7 ? "5" : "6");
            m_surroundleft->setLabel(tr("Surround Left"));
            connect(m_surroundleft,
                    SIGNAL(pressed(QString)), this, SLOT(toggle(QString)));
            m_surroundright = new TransButtonSetting("3");
            m_surroundright->setLabel(tr("Surround Right"));
            connect(m_surroundright,
                    SIGNAL(pressed(QString)), this, SLOT(toggle(QString)));
            m_center = new TransButtonSetting("1");
            m_center->setLabel(tr("Center"));
            connect(m_center,
                    SIGNAL(pressed(QString)), this, SLOT(toggle(QString)));
            frontgroup->addChild(m_center);
            middlegroup->addChild(m_surroundleft);
            if (m_lfe)
                middlegroup->addChild(m_lfe);
            middlegroup->addChild(m_surroundright);

        case 2:
            break;
    }
    frontgroup->addChild(m_frontright);
    addChild(frontgroup);
    addChild(middlegroup);
    addChild(reargroup);
    addChild(m_button);

    m_hd = new TransCheckBoxSetting();
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
    m_at->cancel();
    m_at->wait();
    delete m_at;
}

void AudioTest::toggle(QString str)
{
    if (str == "start")
    {
        if (m_at->isRunning())
        {
            m_at->cancel();
            m_button->setLabel(tr("Test All"));
            if (m_frontleft)
                m_frontleft->setEnabled(true);
            if (m_frontright)
                m_frontright->setEnabled(true);
            if (m_center)
                m_center->setEnabled(true);
            if (m_surroundleft)
                m_surroundleft->setEnabled(true);
            if (m_surroundright)
                m_surroundright->setEnabled(true);
            if (m_rearleft)
                m_rearleft->setEnabled(true);
            if (m_rearright)
                m_rearright->setEnabled(true);
            if (m_lfe)
                m_lfe->setEnabled(true);
        }
        else
        {
            m_at->setChannel(-1);
            m_at->start();
            m_button->setLabel(QCoreApplication::translate("(Common)", "Stop"));
        }
        return;
    }
    if (m_at->isRunning())
    {
        m_at->cancel();
        m_at->wait();
    }

    int channel = str.toInt();
    m_at->setChannel(channel);

    m_at->start();
}

void AudioTest::togglequality()
{
    if (m_at->isRunning())
    {
        toggle("start");
    }

    m_quality = m_hd->boolValue();
    delete m_at;
    m_at = new AudioTestThread(this, m_main, m_passthrough, m_channels,
                               m_settings, m_quality);
    if (!m_at->result().isEmpty())
    {
        QString msg = tr("Audio device is invalid or not useable.");

        MythPopupBox::showOkPopup(
            GetMythMainWindow(), QCoreApplication::translate("(Common)", 
                                                             "Warning"), msg);
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


AudioTestGroup::AudioTestGroup(QString main, QString passthrough,
                               int channels, AudioOutputSettings &settings)
{
    addChild(new AudioTest(main, passthrough, channels, settings));
}

HostCheckBox *AudioMixerSettings::MythControlsVolume()
{
    HostCheckBox *gc = new HostCheckBox("MythControlsVolume");

    gc->setLabel(tr("Use internal volume controls"));

    gc->setValue(true);

    gc->setHelpText(tr("If enabled, MythTV will control the PCM and "
                       "master mixer volume. Disable this option if you "
                       "prefer to control the volume externally (for "
                       "example, using your amplifier) or if you use an "
                       "external mixer program."));
    return gc;
}

HostComboBox *AudioMixerSettings::MixerDevice()
{
    HostComboBox *gc = new HostComboBox("MixerDevice", true);
    gc->setLabel(tr("Mixer device"));

#ifdef USING_OSS
    QDir dev("/dev", "mixer*", QDir::Name, QDir::System);
    gc->fillSelectionsFromDir(dev);

    dev.setPath("/dev/sound");
    if (dev.exists())
    {
        gc->fillSelectionsFromDir(dev);
    }
#endif
#ifdef USING_ALSA
    gc->addSelection("ALSA:default", "ALSA:default");
#endif
#ifdef USING_MINGW
    gc->addSelection("DirectX:", "DirectX:");
    gc->addSelection("Windows:", "Windows:");
#endif
#if !defined(USING_MINGW)
    gc->addSelection(tr("software"), "software");
#endif

    gc->setHelpText(tr("Setting the mixer device to \"%1\" lets MythTV control "
                       "the volume of all audio at the expense of a slight "
                       "quality loss.")
                       .arg(tr("software")));

    return gc;
}

const char* AudioMixerSettings::MixerControlControls[] = { QT_TR_NOOP("PCM"),
                                                           QT_TR_NOOP("Master")};

HostComboBox *AudioMixerSettings::MixerControl()
{
    HostComboBox *gc = new HostComboBox("MixerControl", true);

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

HostSlider *AudioMixerSettings::MixerVolume()
{
    HostSlider *gs = new HostSlider("MasterMixerVolume", 0, 100, 1);

    gs->setLabel(tr("Master mixer volume"));

    gs->setValue(70);

    gs->setHelpText(tr("Initial volume for the Master mixer. This affects "
                       "all sound created by the audio device. Note: Do not "
                       "set this too low."));
    return gs;
}

HostSlider *AudioMixerSettings::PCMVolume()
{
    HostSlider *gs = new HostSlider("PCMMixerVolume", 0, 100, 1);

    gs->setLabel(tr("PCM mixer volume"));

    gs->setValue(70);

    gs->setHelpText(tr("Initial volume for PCM output. Using the volume "
                       "keys in MythTV will adjust this parameter."));
    return gs;
}

AudioMixerSettings::AudioMixerSettings() :
    TriggeredConfigurationGroup(false, true, false, false)
{
    setLabel(tr("Audio Mixer"));

    setUseLabel(false);

    Setting *volumeControl = MythControlsVolume();
    addChild(volumeControl);

        // Mixer settings
    ConfigurationGroup *settings =
        new VerticalConfigurationGroup(false, true, false, false);
    settings->addChild(MixerDevice());
    settings->addChild(MixerControl());
    settings->addChild(MixerVolume());
    settings->addChild(PCMVolume());

    ConfigurationGroup *dummy =
        new VerticalConfigurationGroup(false, true, false, false);

        // Show Mixer config only if internal volume controls enabled
    setTrigger(volumeControl);
    addTarget("0", dummy);
    addTarget("1", settings);
}

AudioGeneralSettings::AudioGeneralSettings()
{
    addChild(new AudioConfigSettings(this));
    addChild(new AudioMixerSettings());
}

HostCheckBox *AudioAdvancedSettings::MPCM()
{
    HostCheckBox *gc = new HostCheckBox("StereoPCM");

    gc->setLabel(tr("Stereo PCM Only"));

    gc->setValue(false);

    gc->setHelpText(tr("Enable if your amplifier or sound decoder only "
                       "supports 2 channel PCM (typically an old HDMI 1.0 "
                       "device). Multichannel audio will be re-encoded to "
                       "AC-3 when required"));
    return gc;
}

HostCheckBox *AudioAdvancedSettings::SRCQualityOverride()
{
    HostCheckBox *gc = new HostCheckBox("SRCQualityOverride");

    gc->setLabel(tr("Override SRC quality"));

    gc->setValue(false);

    gc->setHelpText(tr("Enable to override audio sample rate "
                       "conversion quality."));
    return gc;
}

HostComboBox *AudioAdvancedSettings::SRCQuality()
{
    HostComboBox *gc = new HostComboBox("SRCQuality", false);

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

HostCheckBox *AudioAdvancedSettings::Audio48kOverride()
{
    HostCheckBox *gc = new HostCheckBox("Audio48kOverride");

    gc->setLabel(tr("Force audio device output to 48kHz"));
    gc->setValue(false);

    gc->setHelpText(tr("Force audio sample rate to 48kHz. Some audio devices "
                        "will report various rates, but they ultimately "
                        "crash."));
    return gc;
}

HostCheckBox *AudioAdvancedSettings::PassThroughOverride()
{
    HostCheckBox *gc = new HostCheckBox("PassThruDeviceOverride");

    gc->setLabel(tr("Separate digital output device"));

    gc->setValue(false);

    gc->setHelpText(tr("Use a distinct digital output device from default. "
                        "(default is not checked)"));
    return gc;
}

HostComboBox *AudioAdvancedSettings::PassThroughOutputDevice()
{
    HostComboBox *gc = new HostComboBox("PassThruOutputDevice", true);

    gc->setLabel(tr("Digital output device"));

    gc->addSelection(QCoreApplication::translate("(Common)", "Default"), 
                     "Default");
#ifdef USING_MINGW
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

HostCheckBox *AudioAdvancedSettings::SPDIFRateOverride()
{
    HostCheckBox *gc = new HostCheckBox("SPDIFRateOverride");

    gc->setLabel(tr("SPDIF 48kHz rate override"));

    gc->setValue(false);

    gc->setHelpText(tr("ALSA only. By default, let ALSA determine the "
                       "passthrough sampling rate. If checked set the sampling "
                       "rate to 48kHz for passthrough. (default is not "
                       "checked)"));
    return gc;
}

HostCheckBox *AudioAdvancedSettings::HBRPassthrough()
{
    HostCheckBox *gc = new HostCheckBox("HBRPassthru");

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

AudioAdvancedSettings::AudioAdvancedSettings(bool mpcm)
{
    ConfigurationGroup *settings3 =
        new HorizontalConfigurationGroup(false, false);

    m_PassThroughOverride = PassThroughOverride();
    TriggeredItem *sub3 =
        new TriggeredItem(m_PassThroughOverride, PassThroughOutputDevice());
    settings3->addChild(m_PassThroughOverride);
    settings3->addChild(sub3);

    ConfigurationGroup *settings4 =
        new HorizontalConfigurationGroup(false, false);
    Setting *srcqualityoverride = SRCQualityOverride();
    TriggeredItem *sub4 =
        new TriggeredItem(srcqualityoverride, SRCQuality());
    settings4->addChild(srcqualityoverride);
    settings4->addChild(sub4);

    ConfigurationGroup *settings5 =
        new HorizontalConfigurationGroup(false, false);
    settings5->addChild(Audio48kOverride());
#if USING_ALSA
    settings5->addChild(SPDIFRateOverride());
#endif

    ConfigurationGroup *settings6 =
        new HorizontalConfigurationGroup(false, false);
    settings6->addChild(HBRPassthrough());

    addChild(settings4);
    addChild(settings5);
    addChild(settings3);
    addChild(settings6);

    if (mpcm)
    {
        ConfigurationGroup *settings7;
        settings7 = new HorizontalConfigurationGroup(false, false);
        settings7->addChild(MPCM());
        addChild(settings7);
    }
}

AudioAdvancedSettingsGroup::AudioAdvancedSettingsGroup(bool mpcm)
{
    addChild(new AudioAdvancedSettings(mpcm));
}
// vim:set sw=4 ts=4 expandtab:
