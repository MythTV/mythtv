#include <iostream>
using namespace std;

// Qt
#include <QString>

// MythTV
#include <mythcontext.h>
#include <mythdbcon.h>
#include <mythdirs.h>

#include "playersettings.h"

// ---------------------------------------------------

PlayerSettings::PlayerSettings(MythScreenStack *parent, const char *name)
    : MythScreenType(parent, name),
      m_defaultPlayerEdit(NULL),     m_dvdPlayerEdit(NULL),
      m_dvdDriveEdit(NULL),          m_altPlayerEdit(NULL),
      m_altCheck(NULL),
      m_okButton(NULL),              m_cancelButton(NULL)
{
}

bool PlayerSettings::Create()
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("video-ui.xml", "playersettings", this);

    if (!foundtheme)
        return false;

    m_defaultPlayerEdit = dynamic_cast<MythUITextEdit *> (GetChild("defaultplayer"));
    m_dvdPlayerEdit = dynamic_cast<MythUITextEdit *> (GetChild("dvdplayer"));
    m_dvdDriveEdit = dynamic_cast<MythUITextEdit *> (GetChild("dvddrive"));
    m_blurayMountEdit = dynamic_cast<MythUITextEdit *> (GetChild("bluraymount"));
    m_altPlayerEdit = dynamic_cast<MythUITextEdit *> (GetChild("altplayer"));

    m_altCheck = dynamic_cast<MythUICheckBox *> (GetChild("altcheck"));

    m_okButton = dynamic_cast<MythUIButton *> (GetChild("ok"));
    m_cancelButton = dynamic_cast<MythUIButton *> (GetChild("cancel"));

    if (!m_defaultPlayerEdit || !m_dvdPlayerEdit ||
        !m_altCheck || !m_altPlayerEdit || !m_dvdDriveEdit || !m_blurayMountEdit ||
        !m_okButton || !m_cancelButton)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical theme elements.");
        return false;
    }

    int setting = gCoreContext->GetNumSetting("mythvideo.EnableAlternatePlayer", 0);
    if (setting == 1)
        m_altCheck->SetCheckState(MythUIStateType::Full);

    m_defaultPlayerEdit->SetText(gCoreContext->GetSetting("VideoDefaultPlayer",
                           "Internal"));
    m_dvdPlayerEdit->SetText(gCoreContext->GetSetting("mythdvd.DVDPlayerCommand",
                           "Internal"));
    m_dvdDriveEdit->SetText(gCoreContext->GetSetting("DVDDeviceLocation",
                           "default"));
    m_blurayMountEdit->SetText(gCoreContext->GetSetting("BluRayMountpoint",
                           "/media/cdrom"));
    m_altPlayerEdit->SetText(gCoreContext->GetSetting(
                           "mythvideo.VideoAlternatePlayer", "Internal"));

    if (m_altCheck->GetCheckState() == MythUIStateType::Full)
        m_altPlayerEdit->SetVisible(true);
    else
        m_altPlayerEdit->SetVisible(false);

    connect(m_okButton, SIGNAL(Clicked()), this, SLOT(slotSave()));
    connect(m_cancelButton, SIGNAL(Clicked()), this, SLOT(Close()));

    connect(m_altCheck, SIGNAL(valueChanged()), SLOT(toggleAlt()));

    m_defaultPlayerEdit->SetHelpText(
                 tr("This is the command used for any file "
                 "whose extension is not specifically defined. "
                 "You may also enter the name of one of the playback "
                 "plugins such as 'Internal'."));
    m_dvdPlayerEdit->SetHelpText(
                 tr("This can be any command to launch a DVD "
                 " player. Internal is the default.  For other players, %d "
                 "will be substituted for the DVD device (e.g. /dev/dvd)."));
    m_dvdDriveEdit->SetHelpText(
                    tr("This device must exist, and the user "
                    "playing the DVD needs to have read permission "
                    "on the device.  'default' will let the "
                    "MediaMonitor choose a device."));
    m_blurayMountEdit->SetHelpText(
                    tr("This path is the location your "
                       "operating system mounts Blu-ray discs."));
    m_altPlayerEdit->SetHelpText(
                 tr("If for some reason the default player "
                 "doesn't play a video, you can play it in an alternate "
                 "player by selecting 'Play in Alternate Player.'"));
    m_cancelButton->SetHelpText(tr("Exit without saving settings"));
    m_okButton->SetHelpText(tr("Save settings and Exit"));

    BuildFocusList();

    SetFocusWidget(m_defaultPlayerEdit);

    return true;
}

PlayerSettings::~PlayerSettings()
{
}

void PlayerSettings::slotSave(void)
{
    gCoreContext->SaveSetting("VideoDefaultPlayer", m_defaultPlayerEdit->GetText());
    gCoreContext->SaveSetting("mythdvd.DVDPlayerCommand", m_dvdPlayerEdit->GetText());
    gCoreContext->SaveSetting("DVDDeviceLocation", m_dvdDriveEdit->GetText());
    gCoreContext->SaveSetting("BluRayMountpoint", m_blurayMountEdit->GetText());
    gCoreContext->SaveSetting("mythvideo.VideoAlternatePlayer", m_altPlayerEdit->GetText());

    int checkstate = 0;
    if (m_altCheck->GetCheckState() == MythUIStateType::Full)
        checkstate = 1;
    gCoreContext->SaveSetting("mythvideo.EnableAlternatePlayer", checkstate);

    Close();
}

bool PlayerSettings::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void PlayerSettings::toggleAlt()
{
    int checkstate = 0;
    if (m_altCheck->GetCheckState() == MythUIStateType::Full)
        checkstate = 1;

    m_altPlayerEdit->SetVisible(checkstate);
}
