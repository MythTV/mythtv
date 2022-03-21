// C++
#include <iostream>

// Qt
#include <QString>

// MythTV
#include "libmyth/mythcontext.h"
#include "libmythbase/mythdbcon.h"
#include "libmythbase/mythdirs.h"
#include "libmythui/mythprogressdialog.h"

// MythFrontend
#include "videometadatasettings.h"

// ---------------------------------------------------

bool MetadataSettings::Create()
{
    // Load the theme for this screen
    bool foundtheme = LoadWindowFromXML("video-ui.xml", "metadatasettings", this);
    if (!foundtheme)
        return false;

    m_trailerSpin =
        dynamic_cast<MythUISpinBox *> (GetChild("trailernum"));

    m_unknownFileCheck =
        dynamic_cast<MythUICheckBox *> (GetChild("unknownfilecheck"));
    m_autoMetaUpdateCheck =
        dynamic_cast<MythUICheckBox *> (GetChild("autometaupdatecheck"));
    m_treeLoadsMetaCheck =
        dynamic_cast<MythUICheckBox *> (GetChild("treeloadsmetacheck"));
    m_randomTrailerCheck =
        dynamic_cast<MythUICheckBox *> (GetChild("randomtrailercheck"));

    m_okButton = dynamic_cast<MythUIButton *> (GetChild("ok"));
    m_cancelButton = dynamic_cast<MythUIButton *> (GetChild("cancel"));

    if (!m_trailerSpin || !m_autoMetaUpdateCheck ||
        !m_unknownFileCheck || !m_treeLoadsMetaCheck ||
        !m_randomTrailerCheck ||!m_okButton || !m_cancelButton)
    {
        LOG(VB_GENERAL, LOG_ERR, "Theme is missing critical theme elements.");
        return false;
    }

    int unknownSetting =
        gCoreContext->GetNumSetting("VideoListUnknownFiletypes", 0);
    if (unknownSetting == 1)
        m_unknownFileCheck->SetCheckState(MythUIStateType::Full);

    int autoMetaSetting =
        gCoreContext->GetNumSetting("mythvideo.AutoMetaDataScan", 1);
    if (autoMetaSetting == 1)
        m_autoMetaUpdateCheck->SetCheckState(MythUIStateType::Full);

    int loadMetaSetting =
        gCoreContext->GetNumSetting("VideoTreeLoadMetaData", 1);
    if (loadMetaSetting == 1)
        m_treeLoadsMetaCheck->SetCheckState(MythUIStateType::Full);

    int trailerSetting =
        gCoreContext->GetNumSetting("mythvideo.TrailersRandomEnabled", 0);
    if (trailerSetting == 1)
        m_randomTrailerCheck->SetCheckState(MythUIStateType::Full);

    m_trailerSpin->SetRange(0,100,1);
    m_trailerSpin->SetValue(gCoreContext->GetNumSetting(
                           "mythvideo.TrailersRandomCount"));

    if (m_randomTrailerCheck->GetCheckState() == MythUIStateType::Full)
        m_trailerSpin->SetVisible(true);
    else
        m_trailerSpin->SetVisible(false);

    connect(m_okButton, &MythUIButton::Clicked, this, &MetadataSettings::slotSave);
    connect(m_cancelButton, &MythUIButton::Clicked, this, &MythScreenType::Close);

    connect(m_randomTrailerCheck, &MythUICheckBox::valueChanged,
            this, &MetadataSettings::toggleTrailers);

    m_randomTrailerCheck->SetHelpText(
                                tr("If set, this will enable a button "
                                   "called \"Watch With Trailers\" which will "
                                   "play a user-specified number of trailers "
                                   "before the movie."));
    m_trailerSpin->SetHelpText(tr("Number of trailers to play before a film."));
    m_unknownFileCheck->SetHelpText(
                                tr("If set, all files below the MythVideo "
                                "directory will be displayed unless their "
                                "extension is explicitly set to be ignored."));
    m_autoMetaUpdateCheck->SetHelpText(
                                tr("If set, every time a scan for new videos "
                                "is performed, a mass metadata update of the "
                                "collection will also occur."));
    m_treeLoadsMetaCheck->SetHelpText(
                    tr("If set along with Browse Files, this "
                    "will cause the Video List to load any known video meta"
                    "data from the database. Turning this off can greatly "
                    "speed up how long it takes to load the Video List tree."));
    m_cancelButton->SetHelpText(tr("Exit without saving settings"));
    m_okButton->SetHelpText(tr("Save settings and Exit"));

    BuildFocusList();

    return true;
}

void MetadataSettings::slotSave(void)
{
    gCoreContext->SaveSetting("mythvideo.TrailersRandomCount",
                              m_trailerSpin->GetValue());

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

    return MythScreenType::keyPressEvent(event);
}

void MetadataSettings::toggleTrailers()
{
    int checkstate = 0;
    if (m_randomTrailerCheck->GetCheckState() == MythUIStateType::Full)
        checkstate = 1;

    m_trailerSpin->SetVisible(checkstate != 0);
}
