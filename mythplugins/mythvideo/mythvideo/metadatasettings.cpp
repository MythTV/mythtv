#include <iostream>

// qt
#include <QString>
#include <QProcess>

// myth
#include <mythcontext.h>
#include <mythdbcon.h>
#include <mythdirs.h>
#include <mythprogressdialog.h>

#include "metadatasettings.h"

using namespace std;

// ---------------------------------------------------

MetadataSettings::MetadataSettings(MythScreenStack *parent, const char *name)
    : MythScreenType(parent, name),
      m_trailerSpin(NULL),                 m_helpText(NULL),
      m_unknownFileCheck(NULL),            m_autoMetaUpdateCheck(NULL),
      m_treeLoadsMetaCheck(NULL),          m_randomTrailerCheck(NULL),
      m_okButton(NULL),                    m_cancelButton(NULL)
{
}

bool MetadataSettings::Create()
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("video-ui.xml", "metadatasettings", this);

    if (!foundtheme)
        return false;

    m_trailerSpin = dynamic_cast<MythUISpinBox *> (GetChild("trailernum"));

    m_helpText = dynamic_cast<MythUIText *> (GetChild("helptext"));
    m_unknownFileCheck = dynamic_cast<MythUICheckBox *> (GetChild("unknownfilecheck"));
    m_autoMetaUpdateCheck = dynamic_cast<MythUICheckBox *> (GetChild("autometaupdatecheck"));
    m_treeLoadsMetaCheck = dynamic_cast<MythUICheckBox *> (GetChild("treeloadsmetacheck"));
    m_randomTrailerCheck = dynamic_cast<MythUICheckBox *> (GetChild("randomtrailercheck"));

    m_okButton = dynamic_cast<MythUIButton *> (GetChild("ok"));
    m_cancelButton = dynamic_cast<MythUIButton *> (GetChild("cancel"));

    if (!m_trailerSpin || !m_autoMetaUpdateCheck ||
        !m_helpText || !m_unknownFileCheck || !m_treeLoadsMetaCheck ||
        !m_randomTrailerCheck ||!m_okButton || !m_cancelButton)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical theme elements.");
        return false;
    }

    int unknownSetting = gCoreContext->GetNumSetting("VideoListUnknownFiletypes", 0);
    if (unknownSetting == 1)
        m_unknownFileCheck->SetCheckState(MythUIStateType::Full);
    int autoMetaSetting = gCoreContext->GetNumSetting("mythvideo.AutoMetaDataScan", 0);
    if (autoMetaSetting == 1)
        m_autoMetaUpdateCheck->SetCheckState(MythUIStateType::Full);
    int loadMetaSetting = gCoreContext->GetNumSetting("VideoTreeLoadMetaData", 0);
    if (loadMetaSetting == 1)
        m_treeLoadsMetaCheck->SetCheckState(MythUIStateType::Full);
    int trailerSetting = gCoreContext->GetNumSetting("mythvideo.TrailersRandomEnabled", 0);
    if (trailerSetting == 1)
        m_randomTrailerCheck->SetCheckState(MythUIStateType::Full);

    m_trailerSpin->SetRange(0,100,1);
    m_trailerSpin->SetValue(gCoreContext->GetNumSetting(
                           "mythvideo.TrailersRandomCount"));

    if (m_randomTrailerCheck->GetCheckState() == MythUIStateType::Full)
        m_trailerSpin->SetVisible(true);
    else
        m_trailerSpin->SetVisible(false);

    connect(m_okButton, SIGNAL(Clicked()), this, SLOT(slotSave()));
    connect(m_cancelButton, SIGNAL(Clicked()), this, SLOT(Close()));

    connect(m_randomTrailerCheck, SIGNAL(valueChanged()), SLOT(toggleTrailers()));

    connect(m_trailerSpin,            SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));
    connect(m_unknownFileCheck,       SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));
    connect(m_autoMetaUpdateCheck,    SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));
    connect(m_treeLoadsMetaCheck,     SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));
    connect(m_randomTrailerCheck,     SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));
    connect(m_okButton,               SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));
    connect(m_cancelButton,           SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));

    BuildFocusList();

    return true;
}

MetadataSettings::~MetadataSettings()
{
}

void MetadataSettings::slotSave(void)
{
    gCoreContext->SaveSetting("mythvideo.TrailersRandomCount", m_trailerSpin->GetValue());

    int listUnknownState = 0;
    if (m_unknownFileCheck->GetCheckState() == MythUIStateType::Full)
        listUnknownState = 1;
    gCoreContext->SaveSetting("VideoListUnknownFiletypes", listUnknownState);

    int autoMetaState = 0;
    if (m_autoMetaUpdateCheck->GetCheckState() == MythUIStateType::Full)
        autoMetaState = 1;
    gCoreContext->SaveSetting("mythvideo.AutoMetaDataScan", autoMetaState);

    int loadMetaState = 0;
    if (m_treeLoadsMetaCheck->GetCheckState() == MythUIStateType::Full)
        loadMetaState = 1;
    gCoreContext->SaveSetting("VideoTreeLoadMetaData", loadMetaState);

    int trailerState = 0;
    if (m_randomTrailerCheck->GetCheckState() == MythUIStateType::Full)
        trailerState = 1;
    gCoreContext->SaveSetting("mythvideo.TrailersRandomEnabled", trailerState);

    Close();
}

bool MetadataSettings::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void MetadataSettings::slotFocusChanged(void)
{
    if (!m_helpText)
        return;

    QString msg = "";
    if (GetFocusWidget() == m_randomTrailerCheck)
        msg = tr("If set, this will enable a button "
                 "called \"Watch With Trailers\" which will "
                 "play a user-specified number of trailers "
                 "before the movie.");
    else if (GetFocusWidget() == m_trailerSpin)
        msg = tr("Number of trailers to play before a film.");
    else if (GetFocusWidget() == m_unknownFileCheck)
        msg = tr("If set, all files below the Myth Video "
                 "directory will be displayed unless their "
                 "extension is explicitly set to be ignored.");
    else if (GetFocusWidget() == m_autoMetaUpdateCheck)
        msg = tr("If set, every time a scan for new videos "
                 "is performed, a mass metadata update of the "
                 "collection will also occur.");
    else if (GetFocusWidget() == m_treeLoadsMetaCheck)
        msg = tr("If set along with Browse Files, this "
                 "will cause the Video List to load any known video meta"
                 "data from the database. Turning this off can greatly "
                 "speed up how long it takes to load the Video List tree.");
    else if (GetFocusWidget() == m_cancelButton)
        msg = tr("Exit without saving settings");
    else if (GetFocusWidget() == m_okButton)
        msg = tr("Save settings and Exit");

    m_helpText->SetText(msg);
}

void MetadataSettings::toggleTrailers()
{
    int checkstate = 0;
    if (m_randomTrailerCheck->GetCheckState() == MythUIStateType::Full)
        checkstate = 1;

    m_trailerSpin->SetVisible(checkstate);
}
