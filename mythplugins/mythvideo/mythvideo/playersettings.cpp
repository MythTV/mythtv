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
      m_dvdDriveEdit(NULL),          m_vcdPlayerEdit(NULL),
      m_vcdDriveEdit(NULL),          m_altPlayerEdit(NULL),
      m_helpText(NULL),              m_altCheck(NULL),
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
    m_vcdPlayerEdit = dynamic_cast<MythUITextEdit *> (GetChild("vcdplayer"));
    m_vcdDriveEdit = dynamic_cast<MythUITextEdit *> (GetChild("vcddrive"));
    m_altPlayerEdit = dynamic_cast<MythUITextEdit *> (GetChild("altplayer"));

    m_helpText = dynamic_cast<MythUIText *> (GetChild("helptext"));
    m_altCheck = dynamic_cast<MythUICheckBox *> (GetChild("altcheck"));

    m_okButton = dynamic_cast<MythUIButton *> (GetChild("ok"));
    m_cancelButton = dynamic_cast<MythUIButton *> (GetChild("cancel"));

    if (!m_defaultPlayerEdit || !m_dvdPlayerEdit || !m_vcdPlayerEdit ||
        !m_altCheck || !m_altPlayerEdit || !m_dvdDriveEdit || !m_vcdDriveEdit ||
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
    m_vcdPlayerEdit->SetText(gCoreContext->GetSetting("VCDPlayerCommand",
                           "mplayer vcd:// -cdrom-device %d -fs -zoom -vo xv"));
    m_vcdDriveEdit->SetText(gCoreContext->GetSetting("VCDDeviceLocation",
                           "default"));
    m_altPlayerEdit->SetText(gCoreContext->GetSetting(
                           "mythvideo.VideoAlternatePlayer", "Internal"));

    if (m_altCheck->GetCheckState() == MythUIStateType::Full)
        m_altPlayerEdit->SetVisible(true);
    else
        m_altPlayerEdit->SetVisible(false);

    connect(m_okButton, SIGNAL(Clicked()), this, SLOT(slotSave()));
    connect(m_cancelButton, SIGNAL(Clicked()), this, SLOT(Close()));

    connect(m_altCheck, SIGNAL(valueChanged()), SLOT(toggleAlt()));

    connect(m_defaultPlayerEdit,  SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));
    connect(m_dvdPlayerEdit,  SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));
    connect(m_dvdDriveEdit,  SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));
    connect(m_vcdPlayerEdit,  SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));
    connect(m_vcdDriveEdit,  SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));
    connect(m_altPlayerEdit,  SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));
    connect(m_okButton,     SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));
    connect(m_cancelButton, SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));

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
    gCoreContext->SaveSetting("VCDPlayerCommand", m_vcdPlayerEdit->GetText());
    gCoreContext->SaveSetting("VCDDeviceLocation", m_vcdDriveEdit->GetText());
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

void PlayerSettings::slotFocusChanged(void)
{
    if (!m_helpText)
        return;

    QString msg = "";
    if (GetFocusWidget() == m_defaultPlayerEdit)
        msg = tr("This is the command used for any file "
                 "whose extension is not specifically defined. "
                 "You may also enter the name of one of the playback "
                 "plugins such as 'Internal'.");
    else if (GetFocusWidget() == m_dvdPlayerEdit)
        msg = tr("This can be any command to launch a DVD "
                 " player. Internal is the default.  For other players, %d "
                 "will be substituted for the DVD device (e.g. /dev/dvd).");
    else if (GetFocusWidget() == m_dvdDriveEdit)
        msg = tr("This device must exist, and the user "
                    "playing the DVD needs to have read permission "
                    "on the device.  'default' will let the "
                    "MediaMonitor choose a device.");
    else if (GetFocusWidget() == m_vcdPlayerEdit)
        msg = tr("This can be any command to launch a VCD "
                 "player. The Internal player will not play VCDs. "
                 "%d will be substituted for the VCD device (e.g. /dev/cdrom).");
    else if (GetFocusWidget() == m_vcdDriveEdit)
        msg = tr("This device must exist, and the user "   
                    "playing the VCD needs to have read permission "
                    "on the device.  'default' will let the "
                    "MediaMonitor choose a device.");
    else if (GetFocusWidget() == m_altPlayerEdit)
        msg = tr("If for some reason the default player "
                 "doesn't play a video, you can play it in an alternate "
                 "player by selecting 'Play in Alternate Player.'");
    else if (GetFocusWidget() == m_cancelButton)
        msg = tr("Exit without saving settings");
    else if (GetFocusWidget() == m_okButton)
        msg = tr("Save settings and Exit");

    m_helpText->SetText(msg);
}

void PlayerSettings::toggleAlt()
{
    int checkstate = 0;
    if (m_altCheck->GetCheckState() == MythUIStateType::Full)
        checkstate = 1;

    m_altPlayerEdit->SetVisible(checkstate);
}
