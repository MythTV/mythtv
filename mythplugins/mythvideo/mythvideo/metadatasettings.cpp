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
      m_movieGrabberButtonList(NULL),      m_tvGrabberButtonList(NULL),
      m_trailerSpin(NULL),                 m_helpText(NULL),
      m_unknownFileCheck(NULL),            m_treeLoadsMetaCheck(NULL),
      m_randomTrailerCheck(NULL),          m_okButton(NULL),
      m_cancelButton(NULL)
{
}

bool MetadataSettings::Create()
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("video-ui.xml", "metadatasettings", this);

    if (!foundtheme)
        return false;

    m_movieGrabberButtonList = dynamic_cast<MythUIButtonList *> (GetChild("moviegrabber"));
    m_tvGrabberButtonList = dynamic_cast<MythUIButtonList *> (GetChild("tvgrabber"));
    m_trailerSpin = dynamic_cast<MythUISpinBox *> (GetChild("trailernum"));

    m_helpText = dynamic_cast<MythUIText *> (GetChild("helptext"));
    m_unknownFileCheck = dynamic_cast<MythUICheckBox *> (GetChild("unknownfilecheck"));
    m_treeLoadsMetaCheck = dynamic_cast<MythUICheckBox *> (GetChild("treeloadsmetacheck"));
    m_randomTrailerCheck = dynamic_cast<MythUICheckBox *> (GetChild("randomtrailercheck"));

    m_okButton = dynamic_cast<MythUIButton *> (GetChild("ok"));
    m_cancelButton = dynamic_cast<MythUIButton *> (GetChild("cancel"));

    if (!m_movieGrabberButtonList || !m_tvGrabberButtonList || !m_trailerSpin ||
        !m_helpText || !m_unknownFileCheck || !m_treeLoadsMetaCheck ||
        !m_randomTrailerCheck ||!m_okButton || !m_cancelButton)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical theme elements.");
        return false;
    }

    int unknownSetting = gCoreContext->GetNumSetting("VideoListUnknownFiletypes", 0);
    if (unknownSetting == 1)
        m_unknownFileCheck->SetCheckState(MythUIStateType::Full);
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

    connect(m_movieGrabberButtonList, SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));
    connect(m_tvGrabberButtonList,    SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));
    connect(m_trailerSpin,            SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));
    connect(m_unknownFileCheck,       SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));
    connect(m_treeLoadsMetaCheck,     SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));
    connect(m_randomTrailerCheck,     SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));
    connect(m_okButton,               SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));
    connect(m_cancelButton,           SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));

    BuildFocusList();

    SetFocusWidget(m_movieGrabberButtonList);

    loadData();

    return true;
}

MetadataSettings::~MetadataSettings()
{
}

void MetadataSettings::loadData(void)
{
    QString busymessage = tr("Searching for Grabbers...");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythUIBusyDialog *busyPopup = new MythUIBusyDialog(busymessage, popupStack,
                                                       "metadatabusydialog");

    if (busyPopup->Create())
    {
        popupStack->AddScreen(busyPopup, false);
    }
    else
    {
        delete busyPopup;
        busyPopup = NULL;
    }

    QDir TVScriptPath = QString("%1/mythvideo/scripts/Television/").arg(GetShareDir());
    QStringList TVScripts = TVScriptPath.entryList(QDir::Files);
    QDir MovieScriptPath = QString("%1/mythvideo/scripts/Movie/").arg(GetShareDir());
    QStringList MovieScripts = MovieScriptPath.entryList(QDir::Files);
    QString currentTVGrabber = gCoreContext->GetSetting("mythvideo.TVGrabber",
                                         QString("%1mythvideo/scripts/Television/%2")
                                         .arg(GetShareDir()).arg("ttvdb.py"));
    QString currentMovieGrabber = gCoreContext->GetSetting("mythvideo.MovieGrabber",
                                         QString("%1mythvideo/scripts/Movie/%2")
                                         .arg(GetShareDir()).arg("tmdb.py"));

    for (QStringList::const_iterator i = MovieScripts.end() - 1;
            i != MovieScripts.begin() - 1; --i)
    {
        QProcess versionCheck;

        QString commandline = QString("%1mythvideo/scripts/Movie/%2")
                                      .arg(GetShareDir()).arg(*i);
        versionCheck.setReadChannelMode(QProcess::MergedChannels);
        versionCheck.start(commandline, QStringList() << "-v");
        versionCheck.waitForFinished();
        QByteArray result = versionCheck.readAll();
        QString resultString(result);

        if (resultString.isEmpty())
            resultString = *i;

        MythUIButtonListItem *item =
                    new MythUIButtonListItem(m_movieGrabberButtonList, resultString);
        item->SetData(commandline);
    }

    for (QStringList::const_iterator i = TVScripts.end() - 1;
            i != TVScripts.begin() - 1; --i)
    {
        QProcess versionCheck;

        QString commandline = QString("%1mythvideo/scripts/Television/%2")
                                      .arg(GetShareDir()).arg(*i);
        versionCheck.setReadChannelMode(QProcess::MergedChannels);
        versionCheck.start(commandline, QStringList() << "-v");
        versionCheck.waitForFinished();
        QByteArray result = versionCheck.readAll();
        QString resultString(result);

        if (resultString.isEmpty())
            resultString = *i;

        MythUIButtonListItem *item =
                    new MythUIButtonListItem(m_tvGrabberButtonList, resultString);
        item->SetData(commandline);
    }

    m_movieGrabberButtonList->SetValueByData(qVariantFromValue(currentMovieGrabber));
    m_tvGrabberButtonList->SetValueByData(qVariantFromValue(currentTVGrabber));

    if (busyPopup)
    {
        busyPopup->Close();
        busyPopup = NULL;
    }
}

void MetadataSettings::slotSave(void)
{
    gCoreContext->SaveSetting("mythvideo.TrailersRandomCount", m_trailerSpin->GetValue());
    gCoreContext->SaveSetting("mythvideo.TVGrabber", m_tvGrabberButtonList->GetDataValue().toString());
    gCoreContext->SaveSetting("mythvideo.MovieGrabber", m_movieGrabberButtonList->GetDataValue().toString());

    int listUnknownState = 0;
    if (m_unknownFileCheck->GetCheckState() == MythUIStateType::Full)
        listUnknownState = 1;
    gCoreContext->SaveSetting("VideoListUnknownFiletypes", listUnknownState);

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
    if (GetFocusWidget() == m_movieGrabberButtonList)
        msg = tr("This is the script used to search "
                 "for and download Movie Metadata.");
    else if (GetFocusWidget() == m_tvGrabberButtonList)
        msg = tr("This is the script used to search "
                 "for and download Television Metadata.");
    else if (GetFocusWidget() == m_randomTrailerCheck)
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
