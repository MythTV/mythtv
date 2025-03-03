// C++
#include <algorithm>

// Qt
#include <QString>
#include <QVariant>

// MythTV
#include "libmyth/audio/audiooutpututil.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdbcon.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythlogging.h"
#include "libmythui/mythprogressdialog.h"

// MythFrontend
#include "audiogeneralsettings.h"
#include "setupwizard_audio.h"
#include "setupwizard_general.h"
#include "setupwizard_video.h"

bool AudioSetupWizard::Create()
{
    // Load the theme for this screen
    bool foundtheme = LoadWindowFromXML("config-ui.xml", "audiowizard", this);
    if (!foundtheme)
        return false;

    m_audioDeviceButtonList =
        dynamic_cast<MythUIButtonList *> (GetChild("audiodevices"));
    m_speakerNumberButtonList =
        dynamic_cast<MythUIButtonList *> (GetChild("speakers"));

    m_dtsCheck = dynamic_cast<MythUICheckBox *> (GetChild("dtscheck"));
    m_ac3Check = dynamic_cast<MythUICheckBox *> (GetChild("ac3check"));
    m_eac3Check = dynamic_cast<MythUICheckBox *> (GetChild("eac3check"));
    m_truehdCheck = dynamic_cast<MythUICheckBox *> (GetChild("truehdcheck"));
    m_dtshdCheck = dynamic_cast<MythUICheckBox *> (GetChild("dtshdcheck"));

    m_testSpeakerButton =
        dynamic_cast<MythUIButton *> (GetChild("testspeakers"));

    m_nextButton = dynamic_cast<MythUIButton *> (GetChild("next"));
    m_prevButton = dynamic_cast<MythUIButton *> (GetChild("previous"));

    if (!m_audioDeviceButtonList || !m_speakerNumberButtonList ||
        !m_dtsCheck || !m_ac3Check || !m_eac3Check || !m_truehdCheck ||
        !m_dtshdCheck || !m_testSpeakerButton || !m_nextButton || !m_prevButton)
    {
        LOG(VB_GENERAL, LOG_ERR, "Theme is missing critical theme elements.");
        return false;
    }

    // Pre-set the widgets to their database values
    // Buttonlists are set in load()

    int dtsSetting = gCoreContext->GetNumSetting("DTSPassThru", 0);
    if (dtsSetting == 1)
        m_dtsCheck->SetCheckState(MythUIStateType::Full);

    int ac3Setting = gCoreContext->GetNumSetting("AC3PassThru", 0);
    if (ac3Setting == 1)
        m_ac3Check->SetCheckState(MythUIStateType::Full);

    int eac3Setting = gCoreContext->GetNumSetting("EAC3PassThru", 0);
    if (eac3Setting == 1)
        m_eac3Check->SetCheckState(MythUIStateType::Full);

    int truehdSetting = gCoreContext->GetNumSetting("TrueHDPassThru", 0);
    if (truehdSetting == 1)
        m_truehdCheck->SetCheckState(MythUIStateType::Full);

    int dtshdSetting = gCoreContext->GetNumSetting("DTSHDPassThru", 0);
    if (dtshdSetting == 1)
        m_dtshdCheck->SetCheckState(MythUIStateType::Full);

    // Help Text

    // Buttonlists
    m_audioDeviceButtonList->SetHelpText( tr("Select from one of the "
                                    "audio devices detected on your system.  When "
                                    "satisfied, you can test audio before moving "
                                    "on.  If you fail to configure audio, video "
                                    "playback may fail as well.") );
    m_speakerNumberButtonList->SetHelpText( tr("Select the number of speakers you "
                                    "have.") );

    // Checkboxes
    m_dtsCheck->SetHelpText( tr("Select this checkbox if your receiver "
                                   "is capable of playing DTS.") );
    m_ac3Check->SetHelpText( tr("Select this checkbox if your receiver "
                                   "is capable of playing AC-3 (Dolby Digital).") );
    m_eac3Check->SetHelpText( tr("Select this checkbox if your receiver "
                                   "is capable of playing E-AC-3 (Dolby Digital Plus).") );
    m_truehdCheck->SetHelpText( tr("Select this checkbox if your receiver "
                                   "is capable of playing TrueHD.") );
    m_dtshdCheck->SetHelpText( tr("Select this checkbox if your receiver "
                                   "is capable of playing DTS-HD.") );

    // Buttons
    m_testSpeakerButton->SetHelpText( tr("Test your audio settings by playing "
                                    "noise through each speaker.") );
    m_nextButton->SetHelpText( tr("Save these changes and move on to the next "
                                  "configuration step.") );
    m_prevButton->SetHelpText( tr("Return to the previous configuration step.") );

    // I hate to SetText but it's the only way to make it reliably bi-modal
    m_testSpeakerButton->SetText(tr("Test Speakers"));

    connect(m_testSpeakerButton, &MythUIButton::Clicked, this, &AudioSetupWizard::toggleSpeakers);
    connect(m_nextButton, &MythUIButton::Clicked, this, &AudioSetupWizard::slotNext);
    connect(m_prevButton, &MythUIButton::Clicked, this, &AudioSetupWizard::slotPrevious);

    QString message = tr("Discovering audio devices...");
    LoadInBackground(message);

    BuildFocusList();

    return true;
}

AudioSetupWizard::~AudioSetupWizard()
{
    if (m_testThread)
    {
        m_testThread->cancel();
        m_testThread->wait();
        delete m_testThread;
    }
    delete m_outputlist;
}

void AudioSetupWizard::Load(void)
{
    m_outputlist = AudioOutput::GetOutputList();
}

void AudioSetupWizard::Init(void)
{
    QString current = gCoreContext->GetSetting(QString("AudioOutputDevice"));
    bool found = false;

    if (!current.isEmpty())
    {
        auto samename = [current](const auto & ao){ return ao.m_name == current; };
        found = std::any_of(m_outputlist->cbegin(), m_outputlist->cend(), samename);
        if (!found)
        {
            AudioOutput::AudioDeviceConfig *adc =
                AudioOutput::GetAudioDeviceConfig(current, current, true);
            if (adc->m_settings.IsInvalid())
            {
                LOG(VB_GENERAL, LOG_ERR, QString("Audio device %1 isn't usable")
                    .arg(current));
            }
            else
            {
                    // only insert the device if it is valid
                m_outputlist->insert(0, *adc);
            }
            delete adc;
        }
    }
    for (const auto & ao : std::as_const(*m_outputlist))
    {
        QString name = ao.m_name;
        auto *output = new MythUIButtonListItem(m_audioDeviceButtonList, name);
        output->SetData(name);
    }
    if (found)
    {
        m_audioDeviceButtonList->SetValueByData(QVariant::fromValue(current));
    }

    m_maxspeakers = gCoreContext->GetNumSetting("MaxChannels", 2);
    m_lastAudioDevice = m_audioDeviceButtonList->GetItemCurrent()->GetText();

    // Update list for default audio device
    UpdateCapabilities();

    connect(m_ac3Check, &MythUICheckBox::valueChanged,
            this, &AudioSetupWizard::UpdateCapabilitiesAC3);
    connect(m_audioDeviceButtonList, &MythUIButtonList::itemSelected,
            this, qOverload<MythUIButtonListItem*>(&AudioSetupWizard::UpdateCapabilities));
}

AudioOutputSettings AudioSetupWizard::UpdateCapabilities(bool restore, bool AC3)
{
    QString out = m_audioDeviceButtonList->GetItemCurrent()->GetText();
    int max_speakers = 8;
    int realmax_speakers = 8;

    AudioOutputSettings settings;

    auto samename = [out](const auto & ao){ return ao.m_name == out; };
    // NOLINTNEXTLINE(readability-qualified-auto) // qt6
    const auto ao = std::find_if(m_outputlist->cbegin(), m_outputlist->cend(), samename);
    if (ao != m_outputlist->cend())
        settings = ao->m_settings;

    realmax_speakers = max_speakers = settings.BestSupportedChannels();

    bool bAC3  = settings.canFeature(FEATURE_AC3);
    bool bDTS  = settings.canFeature(FEATURE_DTS);
    bool bLPCM = settings.canFeature(FEATURE_LPCM);
    bool bEAC3 = settings.canFeature(FEATURE_EAC3);
    bool bTRUEHD = settings.canFeature(FEATURE_TRUEHD);
    bool bDTSHD = settings.canFeature(FEATURE_DTSHD);

    bAC3    ? m_ac3Check->Show() : m_ac3Check->Hide();
    bDTS    ? m_dtsCheck->Show() : m_dtsCheck->Hide();
    bEAC3   ? m_eac3Check->Show() : m_eac3Check->Hide();
    bTRUEHD ? m_truehdCheck->Show() : m_truehdCheck->Hide();
    bDTSHD  ? m_dtshdCheck->Show() : m_dtshdCheck->Hide();

    bAC3 &= (m_ac3Check->GetCheckState() == MythUIStateType::Full);
    bDTS &= (m_dtsCheck->GetCheckState() == MythUIStateType::Full);

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

    int cur_speakers = m_maxspeakers;

    if (m_speakerNumberButtonList->GetItemCurrent() != nullptr)
    {
        cur_speakers = m_speakerNumberButtonList->GetItemCurrent()->GetData()
                                      .value<int>();
    }
    m_maxspeakers = std::max(cur_speakers, m_maxspeakers);
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
    m_speakerNumberButtonList->Reset();
    for (int i = 1; i <= max_speakers; i++)
    {
        if (settings.IsSupportedChannels(i))
        {
            switch (i)
            {
                case 2:
                {
                    auto *stereo =
                        new MythUIButtonListItem(m_speakerNumberButtonList,
                                                 QObject::tr("Stereo"));
                    stereo->SetData(2);
                    break;
                }
                case 6:
                {
                    auto *sixchan =
                        new MythUIButtonListItem(m_speakerNumberButtonList,
                                                 QObject::tr("5.1 Channel Audio"));
                    sixchan->SetData(6);
                    break;
                }
                case 8:
                {
                    auto *eightchan =
                        new MythUIButtonListItem(m_speakerNumberButtonList,
                                                 QObject::tr("7.1 Channel Audio"));
                    eightchan->SetData(8);
                    break;
                }
                default:
                    continue;
            }
        }
    }
    m_speakerNumberButtonList->SetValueByData(QVariant::fromValue(cur_speakers));

        // Return values is used by audio test
        // where we mainly are interested by the number of channels
        // if we support AC3 and/or LPCM
    settings.SetBestSupportedChannels(cur_speakers);
    settings.setFeature(bAC3, FEATURE_AC3);
    settings.setFeature(bDTS, FEATURE_DTS);
    settings.setFeature(bLPCM && realmax_speakers > 2, FEATURE_LPCM);

    return settings;
}

AudioOutputSettings AudioSetupWizard::UpdateCapabilities(
    MythUIButtonListItem* item)
{
    bool restore = false;
    if (item)
    {
        restore = item->GetText() != m_lastAudioDevice;
        m_lastAudioDevice = item->GetText();
    }
    return UpdateCapabilities(restore);
}

AudioOutputSettings AudioSetupWizard::UpdateCapabilitiesAC3(void)
{
    return UpdateCapabilities(false, true);
}

void AudioSetupWizard::slotNext(void)
{
    if (m_testThread)
    {
        toggleSpeakers();
    }

    save();

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *sw = new VideoSetupWizard(mainStack, m_generalScreen,
                                    this, "videosetupwizard");

    if (sw->Create())
    {
        mainStack->AddScreen(sw);
    }
    else
    {
        delete sw;
    }
}

void AudioSetupWizard::save(void)
{
    // reset advanced audio config to default values
    gCoreContext->SaveBoolSetting("StereoPCM", false);
    gCoreContext->SaveBoolSetting("Audio48kOverride", false);
    gCoreContext->SaveBoolSetting("HBRPassthru", true);
    gCoreContext->SaveBoolSetting("PassThruDeviceOverride", false);
    gCoreContext->SaveSetting("PassThruOutputDevice", QString());

    int channels = m_speakerNumberButtonList->GetItemCurrent()->GetData()
                               .value<int>();
    gCoreContext->SaveSetting("MaxChannels", channels);

    QString device =
        m_audioDeviceButtonList->GetItemCurrent()->GetText();
    gCoreContext->SaveSetting("AudioOutputDevice", device);

    bool ac3State = (m_ac3Check->GetCheckState() == MythUIStateType::Full);
    gCoreContext->SaveBoolSetting("AC3PassThru", ac3State);

    bool dtsState = (m_dtsCheck->GetCheckState() == MythUIStateType::Full);
    gCoreContext->SaveBoolSetting("DTSPassThru", dtsState);

    bool eac3State = (m_eac3Check->GetCheckState() == MythUIStateType::Full);
    gCoreContext->SaveBoolSetting("EAC3PassThru", eac3State);

    bool truehdState = (m_truehdCheck->GetCheckState() == MythUIStateType::Full);
    gCoreContext->SaveBoolSetting("TrueHDPassThru", truehdState);

    bool dtshdState = (m_dtshdCheck->GetCheckState() == MythUIStateType::Full);
    gCoreContext->SaveBoolSetting("DTSHDPassThru", dtshdState);
}

void AudioSetupWizard::slotPrevious(void)
{
    Close();
}

bool AudioSetupWizard::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;

    if (MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void AudioSetupWizard::toggleSpeakers(void)
{
    if (m_testThread)
    {
        m_testThread->cancel();
        m_testThread->wait();
        delete m_testThread;
        m_testThread = nullptr;
        m_testSpeakerButton->SetText(tr("Test Speakers"));
        return;
    }

    AudioOutputSettings settings = UpdateCapabilities(false);
    QString out = m_audioDeviceButtonList->GetItemCurrent()->GetText();
    int channels = m_speakerNumberButtonList->GetItemCurrent()->GetData()
                        .value<int> ();

    m_testThread =
        new AudioTestThread(this, out, out, channels, settings, false);
    if (!m_testThread->result().isEmpty())
    {
        QString msg = QObject::tr("Audio device is invalid or not useable.");
        ShowOkPopup(msg);
        delete m_testThread;
        m_testThread = nullptr;
    }
    else
    {
        m_testThread->start();
        m_testSpeakerButton->SetText(tr("Stop Speaker Test"));
    }
}
