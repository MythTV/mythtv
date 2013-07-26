#include <iostream>

// qt
#include <QString>
#include <QString>
#include <QStringList>

// myth
#include "mythcorecontext.h"
#include "mythsystemlegacy.h"
#include "mythdbcon.h"
#include "mythdirs.h"

#include "mythprogressdialog.h"
#include "metadatacommon.h"
#include "grabbersettings.h"

using namespace std;

// ---------------------------------------------------

GrabberSettings::GrabberSettings(MythScreenStack *parent, const char *name)
    : MythScreenType(parent, name),
      m_movieGrabberButtonList(NULL),      m_tvGrabberButtonList(NULL),
      m_gameGrabberButtonList(NULL),       m_dailyUpdatesCheck(NULL),
      m_okButton(NULL),                    m_cancelButton(NULL)
{
}

bool GrabberSettings::Create()
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("config-ui.xml", "grabbersettings", this);

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

    connect(m_okButton, SIGNAL(Clicked()), this, SLOT(slotSave()));
    connect(m_cancelButton, SIGNAL(Clicked()), this, SLOT(Close()));

    BuildFocusList();

    QString message = tr("Searching for data sources...");
    LoadInBackground(message);

    return true;
}

GrabberSettings::~GrabberSettings()
{
}

void GrabberSettings::Load(void)
{
    QDir TVScriptPath = QString("%1metadata/Television/").arg(GetShareDir());
    QStringList TVScripts = TVScriptPath.entryList(QDir::Files);
    QDir MovieScriptPath = QString("%1metadata/Movie/").arg(GetShareDir());
    QStringList MovieScripts = MovieScriptPath.entryList(QDir::Files);
    QDir GameScriptPath = QString("%1metadata/Game/").arg(GetShareDir());
    QStringList GameScripts = GameScriptPath.entryList(QDir::Files);

    if (MovieScripts.count())
    {
        for (QStringList::const_iterator i = MovieScripts.begin();
             i != MovieScripts.end(); ++i)
        {
            QString commandline = QString("%1/%2")
                                      .arg(MovieScriptPath.path()).arg(*i);
            MythSystemLegacy grabber(commandline, QStringList() << "-v", kMSRunShell | kMSStdOut);
            grabber.Run();
            grabber.Wait();
            QByteArray result = grabber.ReadAll();

            if (!result.isEmpty())
            {
                QDomDocument doc;
                doc.setContent(result, true);
                QDomElement root = doc.documentElement();
                if (!root.isNull())
                {
                    MetaGrabberScript *script = ParseGrabberVersion(root);
                    if (!script->GetName().isEmpty())
                        m_movieGrabberList.append(script);
                }
            }
        }
    }

    if (TVScripts.count())
    {
        for (QStringList::const_iterator i = TVScripts.end() - 1;
                i != TVScripts.begin() - 1; --i)
        {
            QString commandline = QString("%1/%2")
                                      .arg(TVScriptPath.path()).arg(*i);
            MythSystemLegacy grabber(commandline, QStringList() << "-v", kMSRunShell | kMSStdOut);
            grabber.Run();
            grabber.Wait();
            QByteArray result = grabber.ReadAll();

            if (!result.isEmpty())
            {
                QDomDocument doc;
                doc.setContent(result, true);
                QDomElement root = doc.documentElement();
                if (!root.isNull())
                {
                    MetaGrabberScript *script = ParseGrabberVersion(root);
                    if (!script->GetName().isEmpty())
                        m_tvGrabberList.append(script);

                }
            }
        }
    }

    if (GameScripts.count())
    {
        for (QStringList::const_iterator i = GameScripts.end() - 1;
                i != GameScripts.begin() - 1; --i)
        {
            QString commandline = QString("%1/%2")
                                      .arg(GameScriptPath.path()).arg(*i);
            MythSystemLegacy grabber(commandline, QStringList() << "-v", kMSRunShell | kMSStdOut);
            grabber.Run();
            grabber.Wait();
            QByteArray result = grabber.ReadAll();

            if (!result.isEmpty())
            {
                QDomDocument doc;
                doc.setContent(result, true);
                QDomElement root = doc.documentElement();
                if (!root.isNull())
                {
                    MetaGrabberScript *script = ParseGrabberVersion(root);
                    if (!script->GetName().isEmpty())
                        m_gameGrabberList.append(script);
                }
            }
        }
    }
}

void GrabberSettings::Init(void)
{
    for (QList<MetaGrabberScript*>::const_iterator it =
             m_movieGrabberList.begin();
         it != m_movieGrabberList.end(); ++it)
    {
        QString commandline = QString("metadata/Movie/%1")
                    .arg((*it)->GetCommand());
        InfoMap map;
        (*it)->toMap(map);
        MythUIButtonListItem *item =
                    new MythUIButtonListItem(m_movieGrabberButtonList, (*it)->GetName());
        item->SetData(commandline);
        item->SetTextFromMap(map);
    }

    m_movieGrabberList.clear();

    for (QList<MetaGrabberScript*>::const_iterator it = m_tvGrabberList.begin();
         it != m_tvGrabberList.end(); ++it)
    {
        QString commandline = QString("metadata/Television/%1")
                    .arg((*it)->GetCommand());
        InfoMap map;
        (*it)->toMap(map);
        MythUIButtonListItem *item =
                    new MythUIButtonListItem(m_tvGrabberButtonList, (*it)->GetName());
        item->SetData(commandline);
        item->SetTextFromMap(map);
    }

    m_tvGrabberList.clear();

    for (QList<MetaGrabberScript*>::const_iterator it =
             m_gameGrabberList.begin();
         it != m_gameGrabberList.end(); ++it)
    {
        QString commandline = QString("metadata/Game/%1")
                    .arg((*it)->GetCommand());
        InfoMap map;
        (*it)->toMap(map);
        MythUIButtonListItem *item =
                    new MythUIButtonListItem(m_gameGrabberButtonList, (*it)->GetName());
        item->SetData(commandline);
        item->SetTextFromMap(map);
    }

    m_gameGrabberList.clear();

    QString currentTVGrabber = gCoreContext->GetSetting("TelevisionGrabber",
                                         "metadata/Television/ttvdb.py");
    QString currentMovieGrabber = gCoreContext->GetSetting("MovieGrabber",
                                         "metadata/Movie/tmdb3.py");
    QString currentGameGrabber = gCoreContext->GetSetting("mythgame.MetadataGrabber",
                                         "metadata/Game/giantbomb.py");

    m_movieGrabberButtonList->SetValueByData(qVariantFromValue(currentMovieGrabber));
    m_tvGrabberButtonList->SetValueByData(qVariantFromValue(currentTVGrabber));
    m_gameGrabberButtonList->SetValueByData(qVariantFromValue(currentGameGrabber));

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
    gCoreContext->SaveSetting("DailyArtworkUpdates", dailyupdatestate);

    Close();
}

bool GrabberSettings::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}
