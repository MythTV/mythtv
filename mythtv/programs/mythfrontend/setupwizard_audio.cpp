// qt
#include <QString>
#include <QVariant>

// myth
#include <mythcontext.h>
#include <mythdbcon.h>
#include <mythdirs.h>
#include <mythprogressdialog.h>

#include <audiooutpututil.h>

#include "audiogeneralsettings.h"
#include "setupwizard_general.h"
#include "setupwizard_audio.h"
#include "setupwizard_video.h"

AudioSetupWizard::AudioSetupWizard(MythScreenStack *parent, MythScreenType *previous,
                                   const char *name)
    : MythScreenType(parent, name),
      m_outputlist(NULL),                m_testThread(NULL),
      m_generalScreen(previous),         m_audioDeviceButtonList(NULL),
      m_speakerNumberButtonList(NULL),   m_dtsCheck(NULL),
      m_ac3Check(NULL),                  m_hdCheck(NULL),
      m_hdplusCheck(NULL),               m_testSpeakerButton(NULL),
      m_nextButton(NULL),                m_prevButton(NULL)
{
    m_generalScreen->Hide();
}

bool AudioSetupWizard::Create()
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("config-ui.xml", "audiowizard", this);

    if (!foundtheme)
        return false;

    m_audioDeviceButtonList = dynamic_cast<MythUIButtonList *> (GetChild("audiodevices"));
    m_speakerNumberButtonList = dynamic_cast<MythUIButtonList *> (GetChild("speakers"));

    m_dtsCheck = dynamic_cast<MythUICheckBox *> (GetChild("dtscheck"));
    m_ac3Check = dynamic_cast<MythUICheckBox *> (GetChild("ac3check"));
    m_hdCheck = dynamic_cast<MythUICheckBox *> (GetChild("hdcheck"));
    m_hdplusCheck = dynamic_cast<MythUICheckBox *> (GetChild("hdpluscheck"));

    m_testSpeakerButton = dynamic_cast<MythUIButton *> (GetChild("testspeakers"));

    m_nextButton = dynamic_cast<MythUIButton *> (GetChild("next"));
    m_prevButton = dynamic_cast<MythUIButton *> (GetChild("previous"));

    if (!m_audioDeviceButtonList || !m_speakerNumberButtonList ||
        !m_dtsCheck || !m_ac3Check ||
        !m_hdCheck || !m_hdplusCheck ||
        !m_testSpeakerButton ||!m_nextButton || !m_prevButton)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical theme elements.");
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

    int hdSetting = gCoreContext->GetNumSetting("EAC3PassThru", 0);
    if (hdSetting == 1)
        m_hdCheck->SetCheckState(MythUIStateType::Full);

    int hdplusSetting = gCoreContext->GetNumSetting("TrueHDPassThru", 0);
    if (hdplusSetting == 1)
        m_hdplusCheck->SetCheckState(MythUIStateType::Full);

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
    m_hdCheck->SetHelpText( tr("Select this checkbox if your receiver "
                                   "is capable of playing E-AC-3 (Dolby Digital Plus) "
                                   "or DTS-HD.") );
    m_hdplusCheck->SetHelpText( tr("Select this checkbox if your receiver "
                                   "is capable of playing TrueHD or DTS-HD MA.") );

    // Buttons
    m_testSpeakerButton->SetHelpText( tr("Test your audio settings by playing "
                                    "noise through each speaker.") );
    m_nextButton->SetHelpText( tr("Save these changes and move on to the next "
                                  "configuration step.") );
    m_prevButton->SetHelpText( tr("Return to the previous configuration step.") );

    // I hate to SetText but it's the only way to make it reliably bi-modal
    m_testSpeakerButton->SetText(tr("Test Speakers"));

    connect(m_testSpeakerButton, SIGNAL(Clicked()), this, SLOT(toggleSpeakers()));
    connect(m_nextButton, SIGNAL(Clicked()), this, SLOT(slotNext()));
    connect(m_prevButton, SIGNAL(Clicked()), this, SLOT(slotPrevious()));

    QString message = tr("Discovering audio devices...");
    LoadInBackground(message);

    BuildFocusList();

    return true;
}

AudioSetupWizard::~AudioSetupWizard()
{
    if (m_generalScreen)
        m_generalScreen->Show();

    if (m_testThread)
    {
        m_testThread->cancel();
        m_testThread->wait();
        delete m_testThread;
        m_testThread = NULL;
    }
}

void AudioSetupWizard::Load(void)
{
    m_outputlist = AudioOutput::GetOutputList();
}

void AudioSetupWizard::Init(void)
{
    for (QVector<AudioOutput::AudioDeviceConfig>::const_iterator it = m_outputlist->begin();
        it != m_outputlist->end(); it++)
    {
        QString name = it->name;
        MythUIButtonListItem *output =
                new MythUIButtonListItem(m_audioDeviceButtonList, name);
        output->SetData(name);
    }

    MythUIButtonListItem *stereo =
                new MythUIButtonListItem(m_speakerNumberButtonList, "Stereo");
    stereo->SetData(2);

    MythUIButtonListItem *sixchan =
                new MythUIButtonListItem(m_speakerNumberButtonList, "5.1 Channel Audio");
    sixchan->SetData(6);

    MythUIButtonListItem *eightchan =
                new MythUIButtonListItem(m_speakerNumberButtonList, "7.1 Channel Audio");
    eightchan->SetData(8);

    // If there is a DB value for device and channels, set the buttons accordingly.

    QString currentDevice = gCoreContext->GetSetting("AudioOutputDevice");
    int channels = gCoreContext->GetNumSetting("MaxChannels", 2);

    if (!currentDevice.isEmpty())
        m_audioDeviceButtonList->SetValueByData(qVariantFromValue(currentDevice));
    m_speakerNumberButtonList->SetValueByData(qVariantFromValue(channels));
}

void AudioSetupWizard::slotNext(void)
{
    if (m_testThread)
    {
        m_testThread->cancel();
        m_testThread->wait();
    }

    save();

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    VideoSetupWizard *sw = new VideoSetupWizard(mainStack, m_generalScreen,
                                                this, "videosetupwizard");

    if (sw->Create())
    {
        mainStack->AddScreen(sw);
    }
    else
        delete sw;
}

void AudioSetupWizard::save(void)
{
    int channels = qVariantValue<int>
        (m_speakerNumberButtonList->GetItemCurrent()->GetData());
    gCoreContext->SaveSetting("MaxChannels", channels);

    QString device =
        m_audioDeviceButtonList->GetItemCurrent()->GetText();
    gCoreContext->SaveSetting("AudioOutputDevice", device);

    int ac3State = 0;
    if (m_ac3Check->GetCheckState() == MythUIStateType::Full)
        ac3State = 1;
    gCoreContext->SaveSetting("AC3PassThru", ac3State);

    int dtsState = 0;
    if (m_dtsCheck->GetCheckState() == MythUIStateType::Full)
        dtsState = 1;
    gCoreContext->SaveSetting("DTSPassThru", dtsState);

    int hdState = 0;
    if (m_hdCheck->GetCheckState() == MythUIStateType::Full)
        hdState = 1;
    gCoreContext->SaveSetting("EAC3PassThru", hdState);

    int hdplusState = 0;
    if (m_hdplusCheck->GetCheckState() == MythUIStateType::Full)
        hdplusState = 1;
    gCoreContext->SaveSetting("TrueHDPassThru", hdplusState);
}

void AudioSetupWizard::slotPrevious(void)
{
    m_generalScreen->Show();
    Close();
}

bool AudioSetupWizard::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;

    if (!handled && MythScreenType::keyPressEvent(event))
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
        m_testThread = NULL;
        m_testSpeakerButton->SetText(tr("Test Speakers"));
    }
    else
    {
        QString device = m_audioDeviceButtonList->GetItemCurrent()->GetText();
        int channels = qVariantValue<int> (m_speakerNumberButtonList->GetItemCurrent()->GetData());
        AudioOutputSettings settings;
        m_testThread = new AudioTestThread(this, device, device, channels, settings, false);
        m_testThread->start();
        m_testSpeakerButton->SetText(tr("Stop Speaker Test"));
    }
}

