#include <iostream>
#include <set>
#include <map>

// qt
#include <QString>
#include <QProcess>
#include <QMetaType>
#include <QDateTime>
#include <QFileInfo>
#include <QImageReader>

// myth
#include <mythdb.h>
#include <mythcontext.h>
#include <mythdbcon.h>
#include <mythdirs.h>
#include <mythprogressdialog.h>
#include <storagegroup.h>
#include <remotefile.h>
#include <netgrabbermanager.h>
#include <netutils.h>
#include <rssparse.h>

#include "netsearch.h"
#include "netcommon.h"
#include "rsseditor.h"
#include "searcheditor.h"

using namespace std;

// ---------------------------------------------------

NetSearch::NetSearch(MythScreenStack *parent, const char *name)
    : MythScreenType(parent, name),
      m_searchResultList(NULL),      m_siteList(NULL),
      m_search(NULL),                m_thumbImage(NULL),
      m_downloadable(NULL),          m_progress(NULL),
      m_busyPopup(NULL),             m_okPopup(NULL),
      m_popupStack(),                m_netSearch(NULL),
      m_reply(NULL),                 m_currentSearch(QString()),
      m_currentGrabber(0),           m_currentCmd(QString()),
      m_currentDownload(QString()),  m_pagenum(0),
      m_lock(QMutex::Recursive)
{
    m_mythXML = GetMythXMLURL();
    m_playing = false;
    m_download = new MythDownloadManager();
    m_imageDownload = new MetadataImageDownload(this);
    m_popupStack = GetMythMainWindow()->GetStack("popup stack");
    m_menuPopup = NULL;
}

bool NetSearch::Create()
{
    QMutexLocker locker(&m_lock);

    bool foundtheme = false;

    m_type = static_cast<DialogType>(gCoreContext->GetNumSetting(
                       "mythnetvision.ViewMode", DLG_SEARCH));

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("netvision-ui.xml", "netsearch", this);

    if (!foundtheme)
        return false;

    m_siteList = dynamic_cast<MythUIButtonList *> (GetChild("sites"));
    m_searchResultList = dynamic_cast<MythUIButtonList *> (GetChild("results"));

    m_pageText = dynamic_cast<MythUIText *> (GetChild("page"));
    m_noSites = dynamic_cast<MythUIText *> (GetChild("nosites"));

    m_thumbImage = dynamic_cast<MythUIImage *> (GetChild("preview"));

    m_downloadable = dynamic_cast<MythUIStateType *> (GetChild("downloadable"));

    m_progress = dynamic_cast<MythUIProgressBar *> (GetChild("progress"));

    if (m_progress)
        m_progress->SetVisible(false);

    if (m_noSites)
        m_noSites->SetVisible(false);

    m_search = dynamic_cast<MythUITextEdit *> (GetChild("search"));
    m_search->SetMaxLength(255);

    if (!m_siteList || !m_searchResultList || !m_search)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical theme elements.");
        return false;
    }

    // UI Hookups
    connect(m_siteList, SIGNAL(itemSelected(MythUIButtonListItem *)),
                       SLOT(slotItemChanged()));
    connect(m_siteList, SIGNAL(itemClicked(MythUIButtonListItem *)),
                       SLOT(doSearch(void)));
//    connect(m_netSearch, SIGNAL(searchTimedOut(Search *)),
//                       SLOT(searchTimeout(Search *)));
    connect(m_searchResultList, SIGNAL(itemClicked(MythUIButtonListItem *)),
                       SLOT(showWebVideo(void)));
    connect(m_searchResultList, SIGNAL(itemSelected(MythUIButtonListItem *)),
                       SLOT(slotItemChanged()));

    BuildFocusList();

    LoadInBackground();

    return true;
}

void NetSearch::Load()
{
    m_grabberList = findAllDBSearchGrabbers(VIDEO);
}

void NetSearch::Init()
{
    loadData();
}

NetSearch::~NetSearch()
{
    QMutexLocker locker(&m_lock);

    cleanCacheDir();

    qDeleteAll(m_grabberList);
    m_grabberList.clear();

    if (m_netSearch)
    {
        m_netSearch->disconnect();
        m_netSearch->deleteLater();
        m_netSearch = NULL;
    }

    if (m_download)
    {
        delete m_download;
        m_download = NULL;
    }

    cleanThumbnailCacheDir();

    if (m_imageDownload)
    {
        delete m_imageDownload;
        m_imageDownload = NULL;
    }
}

void NetSearch::loadData(void)
{
    QMutexLocker locker(&m_lock);

    fillGrabberButtonList();

    if (m_grabberList.count() == 0 && m_noSites)
        m_noSites->SetVisible(true);
    else if (m_noSites)
        m_noSites->SetVisible(false);

    if (m_grabberList.isEmpty())
        runSearchEditor();
}

bool NetSearch::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Internet Video", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "MENU")
        {
            showMenu();
        }
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void NetSearch::createBusyDialog(QString title)
{
    if (m_busyPopup)
        return;

    QString message = title;

    m_busyPopup = new MythUIBusyDialog(message, m_popupStack,
            "mythvideobusydialog");

    if (m_busyPopup->Create())
        m_popupStack->AddScreen(m_busyPopup);
    else
    {
        delete m_busyPopup;
        m_busyPopup = NULL;
    }
}

void NetSearch::showMenu(void)
{
    QMutexLocker locker(&m_lock);

    QString label = tr("Search Options");

    MythDialogBox *menuPopup = new MythDialogBox(label, m_popupStack,
                                                    "mythnetvisionmenupopup");

    if (menuPopup->Create())
    {
        m_popupStack->AddScreen(menuPopup);

        menuPopup->SetReturnEvent(this, "options");

        if (m_searchResultList->GetCount() > 0)
        {
            ResultItem *item =
                  qVariantValue<ResultItem *>(m_searchResultList->GetDataValue());

            QString filename;
            bool exists = false;

            if (item)
            {
                menuPopup->AddButton(tr("Open Web Link"), SLOT(showWebVideo()));

                filename = getDownloadFilename(item);

                if (filename.startsWith("myth://"))
                    exists = RemoteFile::Exists(filename);
                else
                    exists = QFile::exists(filename);
            }

            if (item && item->GetDownloadable() &&
                GetFocusWidget() == m_searchResultList)
            {
                if (exists)
                    menuPopup->AddButton(tr("Play"), SLOT(doPlayVideo()));
                else
                    menuPopup->AddButton(tr("Save This Video"), SLOT(doDownloadAndPlay()));
            }

            if (item && item->GetDownloadable() &&
                GetFocusWidget() == m_searchResultList &&
                exists)
            {
                menuPopup->AddButton(tr("Delete"), SLOT(slotDeleteVideo()));
            }


            if (m_pagenum > 1)
                menuPopup->AddButton(tr("Previous Page"), SLOT(getLastResults()));
            if (m_searchResultList->GetCount() > 0 && m_pagenum < m_maxpage)
                menuPopup->AddButton(tr("Next Page"), SLOT(getMoreResults()));
        }
        menuPopup->AddButton(tr("Manage Search Scripts"), SLOT(runSearchEditor()));

    }
    else
    {
        delete menuPopup;
    }
}

void NetSearch::fillGrabberButtonList()
{
    QMutexLocker locker(&m_lock);

    m_siteList->Reset();

    for (GrabberScript::scriptList::iterator i = m_grabberList.begin();
            i != m_grabberList.end(); ++i)
    {
        MythUIButtonListItem *item =
                    new MythUIButtonListItem(m_siteList, (*i)->GetTitle());
        if (item)
        {
        item->SetText((*i)->GetTitle(), "title");
        item->SetData((*i)->GetCommandline());
        QString thumb = QString("%1mythnetvision/icons/%2").arg(GetShareDir())
                            .arg((*i)->GetImage());
        item->SetImage(thumb);
        }
        else
            delete item;
    }
}

void NetSearch::cleanCacheDir()
{
    QMutexLocker locker(&m_lock);

    QString cache = QString("%1/MythNetvision/thumbcache")
                       .arg(GetConfDir());
    QDir cacheDir(cache);
    QStringList thumbs = cacheDir.entryList(QDir::Files);

    for (QStringList::const_iterator i = thumbs.end() - 1;
            i != thumbs.begin() - 1; --i)
    {
        QString filename = QString("%1/%2").arg(cache).arg(*i);
        VERBOSE(VB_GENERAL|VB_EXTRA, QString("Deleting file %1").arg(filename));
        QFileInfo fi(filename);
        QDateTime lastmod = fi.lastModified();
        if (lastmod.addDays(7) < QDateTime::currentDateTime())
            QFile::remove(filename);
    }
}

void NetSearch::doSearch()
{
    QMutexLocker locker(&m_lock);

    m_searchResultList->Reset();

    int numScripts = m_siteList->GetCount();
    for (int i = 0; i < numScripts; i++)
        m_siteList->GetItemAt(i)->SetText("", "count");

    if (m_pageText)
        m_pageText->SetText("");

    m_pagenum = 1;
    m_maxpage = 1;

    QString cmd = m_siteList->GetDataValue().toString();
    QString grabber = m_siteList->GetItemCurrent()->GetText();
    QString query = m_search->GetText();

    if (query.isEmpty())
        return;

    QFileInfo fi(cmd);
    m_currentCmd = fi.fileName();
    m_currentGrabber = m_siteList->GetCurrentPos();
    m_currentSearch = query;

    QString title = tr("Searching %1 for \"%2\"...")
                    .arg(grabber).arg(query);
    createBusyDialog(title);

    m_netSearch = new QNetworkAccessManager(this);
    connect(m_netSearch, SIGNAL(finished(QNetworkReply*)),
                       SLOT(searchFinished(void)));

    QUrl init = GetMythXMLSearch(m_mythXML, m_currentSearch,
                                m_currentCmd, m_pagenum);
    QUrl req(init.toEncoded(), QUrl::TolerantMode);
    VERBOSE(VB_GENERAL, QString("Using Search URL %1").arg(req.toString()));
    m_reply = m_netSearch->get(QNetworkRequest(req));
}

void NetSearch::getLastResults()
{
    QMutexLocker locker(&m_lock);

    m_searchResultList->Reset();

    m_pagenum--;

    QString title = tr("Changing to page %1 of search \"%2\"...")
                    .arg(QString::number(m_pagenum))
                    .arg(m_currentSearch);
    createBusyDialog(title);

    QUrl req = GetMythXMLSearch(m_mythXML, m_currentSearch,
                                m_currentCmd, m_pagenum);
    m_reply = m_netSearch->get(QNetworkRequest(req));
}

void NetSearch::getMoreResults()
{
    QMutexLocker locker(&m_lock);

    m_searchResultList->Reset();

    m_pagenum++;

    QString title = tr("Changing to page %1 of search \"%2\"...")
                    .arg(QString::number(m_pagenum))
                    .arg(m_currentSearch);
    createBusyDialog(title);

    QUrl req = GetMythXMLSearch(m_mythXML, m_currentSearch,
                                m_currentCmd, m_pagenum);
    m_reply = m_netSearch->get(QNetworkRequest(req));
}

void NetSearch::searchFinished(void)
{
    QMutexLocker locker(&m_lock);

    if (m_busyPopup)
    {
        m_busyPopup->Close();
        m_busyPopup = NULL;
    }

    Search *item = new Search();
    QByteArray data = m_reply->readAll();
    item->SetData(data);

    item->process();

    uint searchresults = item->numResults();
    uint returned = item->numReturned();
    uint firstitem = item->numIndex();

    if (returned > 0)
        m_siteList->GetItemAt(m_currentGrabber)->
                  SetText(QString::number(
                  searchresults), "count");
    else
        return;

    if (firstitem + returned == searchresults)
        m_maxpage = m_pagenum;
    else
    {
        if (((float)searchresults/returned + 0.999) >=
            ((int)searchresults/returned + 1))
            m_maxpage = (searchresults/returned + 1);
        else
            m_maxpage = (searchresults/returned);
    }
    if (m_pageText && m_maxpage > 0 && m_pagenum > 0 &&
        returned > 0)
        m_pageText->SetText(QString("%1 / %2")
                        .arg(QString::number(m_pagenum))
                        .arg(QString::number(m_maxpage)));

    ResultItem::resultList list = item->GetVideoList();
    populateResultList(list);
}

void NetSearch::searchTimeout(Search *)
{
    QMutexLocker locker(&m_lock);

    if (m_busyPopup)
    {
        m_busyPopup->Close();
        m_busyPopup = NULL;
    }

    QString message = tr("Timed out waiting for query to "
                         "finish.  API might be down.");

//    MythConfirmationDialog *okPopup =
    if (!m_okPopup)
    {
        m_okPopup = new MythConfirmationDialog(m_popupStack,
                                         message, false);

        if (m_okPopup->Create())
            m_popupStack->AddScreen(m_okPopup);
        else
        {
            delete m_okPopup;
            m_okPopup = NULL;
        }
    }
}

void NetSearch::populateResultList(ResultItem::resultList list)
{
    QMutexLocker locker(&m_lock);

    for (ResultItem::resultList::iterator i = list.begin();
            i != list.end(); ++i)
    {
        QString title = (*i)->GetTitle();
        MythUIButtonListItem *item =
                    new MythUIButtonListItem(
                    m_searchResultList, title);
        if (item)
        {
            MetadataMap metadataMap;
            (*i)->toMap(metadataMap);
            item->SetTextFromMap(metadataMap);

            item->SetData(qVariantFromValue(*i));

            if (!(*i)->GetThumbnail().isEmpty())
            {
                QString dlfile = (*i)->GetThumbnail();

                if (dlfile.contains("%SHAREDIR%"))
                {
                    dlfile.replace("%SHAREDIR%", GetShareDir());
                    item->SetImage(dlfile);
                }
                else
                {
                    uint pos = m_searchResultList->GetItemPos(item);

                    m_imageDownload->addThumb((*i)->GetTitle(),
                                              (*i)->GetThumbnail(),
                                              qVariantFromValue<uint>(pos));
                }
            }
        }
        else
            delete item;
    }
}

void NetSearch::showWebVideo()
{
    QMutexLocker locker(&m_lock);

    ResultItem *item =
        qVariantValue<ResultItem *>(m_searchResultList->GetDataValue());

    if (!item)
        return;

    if (!item->GetPlayer().isEmpty())
    {
        QString cmd = item->GetPlayer();
        QStringList args = item->GetPlayerArguments();
        if (!args.size())
        {
            args += item->GetMediaURL();
            if (!args.size())
                args += item->GetURL();
        }
        else
        {
            args.replaceInStrings("%DIR%", QString(GetConfDir() + "/MythNetvision"));
            args.replaceInStrings("%MEDIAURL%", item->GetMediaURL());
            args.replaceInStrings("%URL%", item->GetURL());
            args.replaceInStrings("%TITLE%", item->GetTitle());
        }
        QString playerCommand = cmd + " " + args.join(" ");

        myth_system(playerCommand);
    }
    else
    {
        QString url = item->GetURL();

        VERBOSE(VB_GENERAL|VB_EXTRA, QString("Web URL = %1").arg(url));

        if (url.isEmpty())
            return;

        QString browser = gCoreContext->GetSetting("WebBrowserCommand", "");
        QString zoom = gCoreContext->GetSetting("WebBrowserZoomLevel", "1.0");

        if (browser.isEmpty())
        {
            ShowOkPopup(tr("No browser command set! MythNetVision needs MythBrowser "
                           "installed to display the video."));
            return;
        }

        if (browser.toLower() == "internal")
        {
            GetMythMainWindow()->HandleMedia("WebBrowser", url);
            return;
        }
        else
        {
            QString cmd = browser;
            cmd.replace("%ZOOM%", zoom);
            cmd.replace("%URL%", url);
            cmd.replace('\'', "%27");
            cmd.replace("&","\\&");
            cmd.replace(";","\\;");

            GetMythMainWindow()->AllowInput(false);
            myth_system(cmd, kMSDontDisableDrawing);
            GetMythMainWindow()->AllowInput(true);
            return;
        }
    }
}

void NetSearch::doPlayVideo()
{
    QMutexLocker locker(&m_lock);

    ResultItem *item =
          qVariantValue<ResultItem *>(m_searchResultList->GetDataValue());

    if (!item)
        return;

    GetMythMainWindow()->HandleMedia("Internal", getDownloadFilename(item));
}

void NetSearch::slotDeleteVideo()
{
    QMutexLocker locker(&m_lock);

    QString message = tr("Are you sure you want to delete this file?");

    MythConfirmationDialog *confirmdialog =
            new MythConfirmationDialog(m_popupStack,message);

    if (confirmdialog->Create())
    {
        m_popupStack->AddScreen(confirmdialog);
        connect(confirmdialog, SIGNAL(haveResult(bool)),
                SLOT(doDeleteVideo(bool)));
    }
    else
        delete confirmdialog;
}

void NetSearch::doDeleteVideo(bool remove)
{
    QMutexLocker locker(&m_lock);

    if (!remove)
        return;

    ResultItem *item =
          qVariantValue<ResultItem *>(m_searchResultList->GetDataValue());

    if (!item)
        return;

    QString filename = getDownloadFilename(item);

    if (filename.startsWith("myth://"))
        RemoteFile::DeleteFile(filename);
    else
    {
        QFile file(filename);
        file.remove();
    }
}

void NetSearch::runSearchEditor()
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    SearchEditor *searchedit = new SearchEditor(mainStack, "mythnetsearchedit");

    if (searchedit->Create())
    {
        connect(searchedit, SIGNAL(itemsChanged()), this,
                       SLOT(doListRefresh()));

        mainStack->AddScreen(searchedit);
    }
    else
    {
        delete searchedit;
    }
}

void NetSearch::doListRefresh()
{
    m_grabberList = findAllDBSearchGrabbers(VIDEO);
    if (m_grabberList.isEmpty())
        runSearchEditor();

    loadData();
}

void NetSearch::doDownloadAndPlay()
{
    QMutexLocker locker(&m_lock);

    ResultItem *item =
          qVariantValue<ResultItem *>(m_searchResultList->GetDataValue());

    if (!item)
        return;

    VERBOSE(VB_GENERAL, QString("Downloading and Inserting %1 "
                                "into Recordings").arg(item->GetTitle()));

    QString filename = getDownloadFilename(item);

    // Does the file already exist?
    bool exists;
    if (filename.startsWith("myth://"))
        exists = RemoteFile::Exists(filename);
    else
        exists = QFile::exists(filename);

    if (exists)
    {
        QString message = tr("This file already downloaded to:\n%1").arg(filename);

        MythConfirmationDialog *confirmdialog =
            new MythConfirmationDialog(m_popupStack,message, false);

        if (confirmdialog->Create())
            m_popupStack->AddScreen(confirmdialog);
        else
            delete confirmdialog;

        return;
    }

    if (m_progress)
        m_progress->SetVisible(true);

    // Initialize the download
    m_redirects = 0;
    m_currentDownload = filename;

}

void NetSearch::slotItemChanged()
{
    QMutexLocker locker(&m_lock);

    ResultItem *item =
              qVariantValue<ResultItem *>(m_searchResultList->GetDataValue());

    if (item && GetFocusWidget() == m_searchResultList)
    {
        MetadataMap metadataMap;
        item->toMap(metadataMap);
        SetTextFromMap(metadataMap);

        if (!item->GetThumbnail().isEmpty() && m_thumbImage)
        {
            MythUIButtonListItem *btn = m_searchResultList->GetItemCurrent();
            QString filename = btn->GetImage();
            if (filename.contains("%SHAREDIR%"))
                filename.replace("%SHAREDIR%", GetShareDir());
            m_thumbImage->Reset();

            if (!filename.isEmpty())
            {
                m_thumbImage->SetFilename(filename);
                m_thumbImage->Load();
            }
        }

        if (m_downloadable)
        {
            if (item->GetDownloadable())
                m_downloadable->DisplayState("yes");
            else
                m_downloadable->DisplayState("no");
        }
    }
    else if (GetFocusWidget() == m_siteList)
    {
        MythUIButtonListItem *item = m_siteList->GetItemCurrent();

        ResultItem *res = new ResultItem(item->GetText(), QString(), QString(),
              QString(), QString(), QString(), QString(), QDateTime(),
              0, 0, -1, QString(), QStringList(), QString(), QStringList(), 0, 0, QString(),
              0, QStringList(), 0, 0, 0);

        MetadataMap metadataMap;
        res->toMap(metadataMap);
        SetTextFromMap(metadataMap);

        if (m_thumbImage)
        {
            QString filename = item->GetImage();
            m_thumbImage->Reset();
            if (filename.contains("%SHAREDIR%"))
                filename.replace("%SHAREDIR%", GetShareDir());

            if (!filename.isEmpty())
            {
                m_thumbImage->SetFilename(filename);
                m_thumbImage->Load();
            }
        }
    }
}

void NetSearch::slotDoProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    QMutexLocker locker(&m_lock);

    if (m_progress)
    {
        int total = bytesTotal/100;
        int received = bytesReceived/100;
        m_progress->SetTotal(total);
        m_progress->SetUsed(received);
        VERBOSE(VB_GENERAL|VB_EXTRA, QString("Progress event received: %1/%2")
                               .arg(received).arg(total));
    }
}

void NetSearch::slotDownloadFinished()
{
    QMutexLocker locker(&m_lock);

    if (m_progress)
       m_progress->SetVisible(false);
}

QString NetSearch::getDownloadFilename(ResultItem *item)
{
    QByteArray urlarr(item->GetMediaURL().toLatin1());
    quint16 urlChecksum = qChecksum(urlarr.data(), urlarr.length());
    QByteArray titlearr(item->GetTitle().toLatin1());
    quint16 titleChecksum = qChecksum(titlearr.data(), titlearr.length());
    QUrl qurl(item->GetMediaURL());
    QString ext = QFileInfo(qurl.path()).suffix();
    QString basefilename = QString("download_%1_%2.%3")
                           .arg(QString::number(urlChecksum))
                           .arg(QString::number(titleChecksum)).arg(ext);

    QString finalFilename = GetConfDir() + "/" + basefilename;

    return finalFilename;
}

void NetSearch::customEvent(QEvent *event)
{
    if (event->type() == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)event;

        if (me->Message().left(17) == "DOWNLOAD_COMPLETE")
        {
            QStringList tokens = me->Message()
                .split(" ", QString::SkipEmptyParts);

            if (tokens.size() != 2)
            {
                VERBOSE(VB_IMPORTANT, "Bad DOWNLOAD_COMPLETE message");
                return;
            }

            GetMythMainWindow()->HandleMedia("Internal", tokens.takeAt(1));
        }
    }
    else if (event->type() == ThumbnailDLEvent::kEventType)
    {
        ThumbnailDLEvent *tde = (ThumbnailDLEvent *)event;

        if (!tde)
            return;

        ThumbnailData *data = tde->thumb;

        if (!data)
            return;

        QString title = data->title;
        QString file = data->url;
        uint pos = qVariantValue<uint>(data->data);

        if (file.isEmpty() || !((uint)m_searchResultList->GetCount() >= pos))
        {
            delete data;
            return;
        }

        MythUIButtonListItem *item =
                  m_searchResultList->GetItemAt(pos);

        if (item && item->GetText() == title)
        {
            item->SetImage(file);
        }

        delete data;
    }
}

