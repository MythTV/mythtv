#include <iostream>

// Qt
#include <QString>
#include <QStringList>

// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdbcon.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythsystemlegacy.h"
#include "libmythmetadata/metadatacommon.h"
#include "libmythui/mythprogressdialog.h"

// MythFrontend
#include "grabbersettings.h"

// ---------------------------------------------------

bool GrabberSettings::Create()
{
    // Load the theme for this screen
    bool foundtheme = LoadWindowFromXML("config-ui.xml", "grabbersettings", this);
    if (!foundtheme)
        return false;

    m_movieGrabberButtonList = dynamic_cast<MythUIButtonList *> (GetChild("moviegrabber"));
    m_tvGrabberButtonList = dynamic_cast<MythUIButtonList *> (GetChild("tvgrabber"));
    m_gameGrabberButtonList = dynamic_cast<MythUIButtonList *> (GetChild("gamegrabber"));

    m_dailyUpdatesCheck = dynamic_cast<MythUICheckBox *> (GetChild("dailyupdates"));

    m_okButton = dynamic_cast<MythUIButton *> (GetChild("ok"));
    m_cancelButton = dynamic_cast<MythUIButton *> (GetChild("cancel"));

    if (!m_movieGrabberButtonList || !m_tvGrabberButtonList ||
        !m_gameGrabberButtonList || !m_dailyUpdatesCheck || !m_okButton || !m_cancelButton)
    {
        LOG(VB_GENERAL, LOG_ERR, "Theme is missing critical theme elements.");
        return false;
    }

    m_movieGrabberButtonList->SetHelpText(tr("Select a source to use when searching for "
                                             "information and artwork about movies."));
    m_tvGrabberButtonList->SetHelpText(tr("Select a source to use when searching for "
                                          "information and artwork about television."));
    m_gameGrabberButtonList->SetHelpText(tr("Select a source to use when searching for "
                                            "information and artwork about video games."));
    m_okButton->SetHelpText(tr("Save your changes and close this window."));
    m_cancelButton->SetHelpText(tr("Discard your changes and close this window."));

    m_dailyUpdatesCheck->SetHelpText(tr("If set, the backend will attempt to "
                            "perform artwork updates for recordings daily. When "
                            "new seasons begin to record, this will attempt to "
                            "provide you with fresh, relevant artwork while "
                            "preserving the artwork assigned to old recordings."));

    connect(m_okButton, &MythUIButton::Clicked, this, &GrabberSettings::slotSave);
    connect(m_cancelButton, &MythUIButton::Clicked, this, &MythScreenType::Close);

    BuildFocusList();

    QString message = tr("Searching for data sources...");
    LoadInBackground(message);

    return true;
}

void GrabberSettings::Load(void)
{
    m_movieGrabberList = MetaGrabberScript::GetList(kGrabberMovie, true);
    m_tvGrabberList = MetaGrabberScript::GetList(kGrabberTelevision);
    m_gameGrabberList = MetaGrabberScript::GetList(kGrabberGame);
}

void GrabberSettings::Init(void)
{
    for (const auto & grabber : std::as_const(m_movieGrabberList))
    {
        InfoMap map;
        grabber.toMap(map);
        auto *item = new MythUIButtonListItem(m_movieGrabberButtonList,
                                              grabber.GetName());
        item->SetData(grabber.GetRelPath());
        item->SetTextFromMap(map);
    }

    m_movieGrabberList.clear();

    for (const auto & grabber: std::as_const(m_tvGrabberList))
    {
        InfoMap map;
        grabber.toMap(map);
        auto *item = new MythUIButtonListItem(m_tvGrabberButtonList,
                                              grabber.GetName());
        item->SetData(grabber.GetRelPath());
        item->SetTextFromMap(map);
    }

    m_tvGrabberList.clear();

    for (const auto & grabber : std::as_const(m_gameGrabberList))
    {
        InfoMap map;
        grabber.toMap(map);
        auto *item = new MythUIButtonListItem(m_gameGrabberButtonList,
                                              grabber.GetName());
        item->SetData(grabber.GetRelPath());
        item->SetTextFromMap(map);
    }

    m_gameGrabberList.clear();

    // TODO
    // pull these values from MetaGrabberScript so we're not defining them in multiple locations
    QString currentTVGrabber = gCoreContext->GetSetting("TelevisionGrabber",
                                         "metadata/Television/ttvdb4.py");
    QString currentMovieGrabber = gCoreContext->GetSetting("MovieGrabber",
                                         "metadata/Movie/tmdb3.py");
    QString currentGameGrabber = gCoreContext->GetSetting("mythgame.MetadataGrabber",
                                         "metadata/Game/giantbomb.py");

    m_movieGrabberButtonList->SetValueByData(QVariant::fromValue(currentMovieGrabber));
    m_tvGrabberButtonList->SetValueByData(QVariant::fromValue(currentTVGrabber));
    m_gameGrabberButtonList->SetValueByData(QVariant::fromValue(currentGameGrabber));

    int updates =
        gCoreContext->GetNumSetting("DailyArtworkUpdates", 0);
    if (updates == 1)
        m_dailyUpdatesCheck->SetCheckState(MythUIStateType::Full);
}

void GrabberSettings::slotSave(void)
{
    gCoreContext->SaveSettingOnHost("TelevisionGrabber", m_tvGrabberButtonList->GetDataValue().toString(), "");
    gCoreContext->SaveSettingOnHost("MovieGrabber", m_movieGrabberButtonList->GetDataValue().toString(), "");
    gCoreContext->SaveSetting("mythgame.MetadataGrabber", m_gameGrabberButtonList->GetDataValue().toString());

    int dailyupdatestate = 0;
    if (m_dailyUpdatesCheck->GetCheckState() == MythUIStateType::Full)
        dailyupdatestate = 1;
    gCoreContext->SaveSettingOnHost("DailyArtworkUpdates",
                                    QString::number(dailyupdatestate), "");

    Close();
}

bool GrabberSettings::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    return MythScreenType::keyPressEvent(event);
}
