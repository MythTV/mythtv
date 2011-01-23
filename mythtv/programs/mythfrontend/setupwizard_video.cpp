// qt
#include <QString>
#include <QVariant>

// myth
#include <mythcontext.h>
#include <mythdbcon.h>
#include <mythdirs.h>
#include <mythprogressdialog.h>

#include "setupwizard_general.h"
#include "setupwizard_audio.h"
#include "setupwizard_video.h"

VideoSetupWizard::VideoSetupWizard(MythScreenStack *parent, MythScreenType *general,
                                   MythScreenType *audio, const char *name)
    : MythScreenType(parent, name),
      m_generalScreen(general),          m_audioScreen(audio),
      m_playbackProfileButtonList(NULL), m_testSDButton(NULL),
      m_testHDButton(NULL),              m_nextButton(NULL),
      m_prevButton(NULL)
{
    m_vdp = new VideoDisplayProfile();
    m_audioScreen->Hide();
}

bool VideoSetupWizard::Create()
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("config-ui.xml", "videowizard", this);

    if (!foundtheme)
        return false;

    m_playbackProfileButtonList = dynamic_cast<MythUIButtonList *> (GetChild("playbackprofiles"));

    m_testSDButton = dynamic_cast<MythUIButton *> (GetChild("testsd"));
    m_testHDButton = dynamic_cast<MythUIButton *> (GetChild("testhd"));

    m_nextButton = dynamic_cast<MythUIButton *> (GetChild("next"));
    m_prevButton = dynamic_cast<MythUIButton *> (GetChild("previous"));

    if (!m_playbackProfileButtonList || !m_testSDButton ||
        !m_testHDButton ||!m_nextButton || !m_prevButton)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical theme elements.");
        return false;
    }

    m_playbackProfileButtonList->SetHelpText( tr("Select from one of the "
                                    "preconfigured playback profiles.  When "
                                    "satisfied, you can test Standard Definition "
                                    "and High Definition playback with your choice "
                                    "before moving on.") );
    m_testSDButton->SetHelpText( tr("Test your playback settings with Standard "
                                    "definition content. (480p)") );
    m_testHDButton->SetHelpText( tr("Test your playback settings with High "
                                    "definition content (1080p).") );
    m_nextButton->SetHelpText( tr("Save these changes and move on to the "
                                  "next configuration step.") );
    m_prevButton->SetHelpText( tr("Return to the previous configuration step.") );

    connect(m_testSDButton, SIGNAL(Clicked()), this, SLOT(testSDVideo()));
    connect(m_testHDButton, SIGNAL(Clicked()), this, SLOT(testHDVideo()));
    connect(m_nextButton, SIGNAL(Clicked()), this, SLOT(slotNext()));
    connect(m_prevButton, SIGNAL(Clicked()), this, SLOT(slotPrevious()));

    BuildFocusList();
    loadData();

    return true;
}

VideoSetupWizard::~VideoSetupWizard()
{
    if (m_vdp)
        delete m_vdp;

    if (m_audioScreen)
        m_audioScreen->Show();
}

void VideoSetupWizard::loadData(void)
{
    QStringList profiles = m_vdp->GetProfiles(gCoreContext->GetHostName());

    for (QStringList::const_iterator i = profiles.begin();
            i != profiles.end(); i++)
    {
        MythUIButtonListItem *item =
                    new MythUIButtonListItem(m_playbackProfileButtonList, *i);
        item->SetData(*i);
    }

    QString currentpbp = m_vdp->GetDefaultProfileName(gCoreContext->GetHostName());
    if (!currentpbp.isEmpty())
    {
        MythUIButtonListItem *set =
                m_playbackProfileButtonList->GetItemByData(currentpbp);
        m_playbackProfileButtonList->SetItemCurrent(set);
    }
}

void VideoSetupWizard::slotNext(void)
{
    save();
}

void VideoSetupWizard::save(void)
{
    QString desiredpbp =
        m_playbackProfileButtonList->GetItemCurrent()->GetText();
    m_vdp->SetDefaultProfileName(desiredpbp, gCoreContext->GetHostName());
}

void VideoSetupWizard::slotPrevious(void)
{
    if (m_audioScreen)
        m_audioScreen->Show();
    Close();
}

bool VideoSetupWizard::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void VideoSetupWizard::testSDVideo(void)
{
    QString sdtestfile =
           QString("%1themes/default/test_sd.mov")
           .arg(GetShareDir());

    QString desiredpbp =
        m_playbackProfileButtonList->GetItemCurrent()->GetText();
    QString currentpbp = m_vdp->GetDefaultProfileName(gCoreContext->GetHostName());

    QString desc = tr("A short test of your system's playback of "
                    "Standard Definition content with the %1 profile.")
                    .arg(desiredpbp);
    QString title = tr("Standard Definition Playback Test");

    m_vdp->SetDefaultProfileName(desiredpbp, gCoreContext->GetHostName());
    GetMythMainWindow()->HandleMedia("Internal", sdtestfile,
                                     desc, title);
    m_vdp->SetDefaultProfileName(currentpbp, gCoreContext->GetHostName());
}

void VideoSetupWizard::testHDVideo(void)
{
    QString hdtestfile =
           QString("%1themes/default/test_hd.mov")
           .arg(GetShareDir());

    QString desiredpbp =
        m_playbackProfileButtonList->GetItemCurrent()->GetText();
    QString currentpbp = m_vdp->GetDefaultProfileName(gCoreContext->GetHostName());

    QString desc = tr("A short test of your system's playback of "
                    "High Definition content with the %1 profile.")
                    .arg(desiredpbp);
    QString title = tr("High Definition Playback Test");

    m_vdp->SetDefaultProfileName(desiredpbp, gCoreContext->GetHostName());
    GetMythMainWindow()->HandleMedia("Internal", hdtestfile,
                                     desc, title);
    m_vdp->SetDefaultProfileName(currentpbp, gCoreContext->GetHostName());
}

