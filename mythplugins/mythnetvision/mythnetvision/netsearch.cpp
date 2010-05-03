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

#include "search.h"
#include "netsearch.h"
#include "parse.h"
#include "rssdbutil.h"
#include "rsseditor.h"

using namespace std;

// ---------------------------------------------------

NetSearch::NetSearch(MythScreenStack *parent, const char *name)
    : MythScreenType(parent, name),
      m_searchResultList(NULL),      m_siteList(NULL),
      m_search(NULL),
      m_title(NULL),                 m_description(NULL),
      m_url(NULL),                   m_thumbnail(NULL),
      m_mediaurl(NULL),              m_author(NULL),
      m_date(NULL),                  m_time(NULL),
      m_filesize(NULL),              m_filesize_str(NULL),
      m_rating(NULL),                m_pageText(NULL),
      m_noSites(NULL),               m_width(NULL),
      m_height(NULL),                m_resolution(NULL),
      m_countries(NULL),             m_season(NULL),
      m_episode(NULL),               m_thumbImage(NULL),
      m_downloadable(NULL),          m_progress(NULL),
      m_busyPopup(NULL),             m_okPopup(NULL),
      m_popupStack(),                m_netSearch(),
      m_currentSearch(NULL),         m_currentGrabber(0),
      m_currentCmd(NULL),            m_currentDownload(NULL),
      m_pagenum(0),                  m_lock(QMutex::Recursive)
{
    m_playing = false;
    m_netSearch = new Search();
    m_download = new DownloadManager(this);
    m_imageDownload = new ImageDownloadManager(this);
    m_popupStack = GetMythMainWindow()->GetStack("popup stack");
    m_menuPopup = NULL;
}

bool NetSearch::Create()
{
    QMutexLocker locker(&m_lock);

    bool foundtheme = false;

    m_type = static_cast<DialogType>(gContext->GetNumSetting(
                       "mythnetvision.ViewMode", DLG_SEARCH));

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("netvision-ui.xml", "netsearch", this);

    if (!foundtheme)
        return false;

    m_siteList = dynamic_cast<MythUIButtonList *> (GetChild("sites"));
    m_searchResultList = dynamic_cast<MythUIButtonList *> (GetChild("results"));

    m_title = dynamic_cast<MythUIText *> (GetChild("title"));
    m_description = dynamic_cast<MythUIText *> (GetChild("description"));
    m_url = dynamic_cast<MythUIText *> (GetChild("url"));
    m_thumbnail = dynamic_cast<MythUIText *> (GetChild("thumbnail"));
    m_mediaurl = dynamic_cast<MythUIText *> (GetChild("mediaurl"));
    m_author = dynamic_cast<MythUIText *> (GetChild("author"));
    m_date = dynamic_cast<MythUIText *> (GetChild("date"));
    m_time = dynamic_cast<MythUIText *> (GetChild("time"));
    m_filesize = dynamic_cast<MythUIText *> (GetChild("filesize"));
    m_filesize_str = dynamic_cast<MythUIText *> (GetChild("filesize_str"));
    m_rating = dynamic_cast<MythUIText *> (GetChild("rating"));
    m_pageText = dynamic_cast<MythUIText *> (GetChild("page"));
    m_noSites = dynamic_cast<MythUIText *> (GetChild("nosites"));
    m_width = dynamic_cast<MythUIText *> (GetChild("width"));
    m_height = dynamic_cast<MythUIText *> (GetChild("height"));
    m_resolution = dynamic_cast<MythUIText *> (GetChild("resolution"));
    m_countries = dynamic_cast<MythUIText *> (GetChild("countries"));
    m_season = dynamic_cast<MythUIText *> (GetChild("season"));
    m_episode = dynamic_cast<MythUIText *> (GetChild("episode"));

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
    connect(m_netSearch, SIGNAL(finishedSearch(Search *)),
                       SLOT(searchFinished(Search *)));
    connect(m_netSearch, SIGNAL(searchTimedOut(Search *)),
                       SLOT(searchTimeout(Search *)));
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
    m_grabberList = fillGrabberList();
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
        delete m_netSearch;
        m_netSearch = NULL;
    }

    if (m_download)
    {
        delete m_download;
        m_download = NULL;
    }

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

    if (m_searchResultList->GetCount() > 0 && menuPopup->Create())
    {
        m_popupStack->AddScreen(menuPopup);

        menuPopup->SetReturnEvent(this, "options");

        ResultVideo *item =
              qVariantValue<ResultVideo *>(m_searchResultList->GetDataValue());

        QString filename;
        bool exists;

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
    else
    {
        delete menuPopup;
    }
}

GrabberScript::scriptList NetSearch::fillGrabberList()
{
    QMutexLocker locker(&m_lock);

    GrabberScript::scriptList tmp;
    QDir ScriptPath = QString("%1/mythnetvision/scripts/").arg(GetShareDir());
    QStringList Scripts = ScriptPath.entryList(QDir::Files | QDir::Executable);

    for (QStringList::const_iterator i = Scripts.begin();
            i != Scripts.end(); ++i)
    {
        QProcess scriptCheck;
        QString title, image;
        bool search = false;
        bool tree = false;

        QString commandline = QString("%1mythnetvision/scripts/%2")
                                      .arg(GetShareDir()).arg(*i);
        scriptCheck.setReadChannel(QProcess::StandardOutput);
        scriptCheck.start(commandline, QStringList() << "-v");
        scriptCheck.waitForFinished();
        QByteArray result = scriptCheck.readAll();
        QString resultString(result);

        if (resultString.isEmpty())
            resultString = *i;

        QString capabilities = QString();
        QStringList specs = resultString.split("|");
        if (specs.size() > 1)
            capabilities = specs.takeAt(1);

        VERBOSE(VB_GENERAL|VB_EXTRA, QString("Capability of script %1: %2")
                              .arg(*i).arg(capabilities.trimmed()));

        title = specs.takeAt(0);

        if (capabilities.trimmed().contains("S"))
            search = true;

        if (capabilities.trimmed().contains("T"))
            tree = true;

        QFileInfo fi(*i);
        QString basename = fi.completeBaseName();
        QList<QByteArray> image_types = QImageReader::supportedImageFormats();

        typedef std::set<QString> image_type_list;
        image_type_list image_exts;

        for (QList<QByteArray>::const_iterator it = image_types.begin();
                it != image_types.end(); ++it)
        {
            image_exts.insert(QString(*it).toLower());
        }

        QStringList sfn;

        QString icondir = QString("%1/mythnetvision/icons/").arg(GetShareDir());
        QString dbIconDir = gContext->GetSetting("mythnetvision.iconDir", icondir);

        for (image_type_list::const_iterator ext = image_exts.begin();
                ext != image_exts.end(); ++ext)
        {
            sfn += QString(dbIconDir + basename + "." + *ext);
        }

        for (QStringList::const_iterator im = sfn.begin();
                        im != sfn.end(); ++im)
        {
            if (QFile::exists(*im))
            {
                image = *im;
                break;
            }
        }
        if (search)
            tmp.append(new GrabberScript(title, image, search, tree, commandline));
    }
    return tmp;
}

void NetSearch::fillGrabberButtonList()
{
    QMutexLocker locker(&m_lock);

    for (GrabberScript::scriptList::iterator i = m_grabberList.begin();
            i != m_grabberList.end(); ++i)
    {
        MythUIButtonListItem *item =
                    new MythUIButtonListItem(m_siteList, (*i)->GetTitle());
        if (item)
        {
        item->SetText((*i)->GetTitle(), "title");
        item->SetData((*i)->GetCommandline());
        item->SetImage((*i)->GetImage());
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

    m_currentCmd = cmd;
    m_currentGrabber = m_siteList->GetCurrentPos();
    m_currentSearch = query;

    QString title = tr("Searching %1 for \"%2\"...")
                    .arg(grabber).arg(query);
    createBusyDialog(title);

    m_netSearch->executeSearch(cmd, query);
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

    QString cmd = m_currentCmd;
    QString query = m_currentSearch;

    m_netSearch->executeSearch(cmd, query, m_pagenum);
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

    QString cmd = m_currentCmd;
    QString query = m_currentSearch;

    m_netSearch->executeSearch(cmd, query, m_pagenum);
}

void NetSearch::searchFinished(Search *item)
{
    QMutexLocker locker(&m_lock);

    if (m_busyPopup)
    {
        m_busyPopup->Close();
        m_busyPopup = NULL;
    }

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

    ResultVideo::resultList list = item->GetVideoList();
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

void NetSearch::populateResultList(ResultVideo::resultList list)
{
    QMutexLocker locker(&m_lock);

    for (ResultVideo::resultList::iterator i = list.begin();
            i != list.end(); ++i)
    {
        QString title = (*i)->GetTitle();
        MythUIButtonListItem *item =
                    new MythUIButtonListItem(
                    m_searchResultList, title);
        if (item)
        {
            item->SetText(title, "title");
            item->SetText((*i)->GetDescription(), "description");
            item->SetText((*i)->GetURL(), "url");
            item->SetText((*i)->GetThumbnail(), "thumbnail");
            item->SetText((*i)->GetMediaURL(), "mediaurl");
            item->SetText((*i)->GetAuthor(), "author");
            item->SetText((*i)->GetDate().toString(
               gContext->GetSetting("DateFormat",
               "yyyy-MM-dd hh:mm")), "date");
            item->SetText((*i)->GetTime(), "time");
            item->SetText((*i)->GetRating(), "rating");
            item->SetText(QString::number((*i)->GetWidth()), "width");
            item->SetText(QString::number((*i)->GetHeight()), "height");
            item->SetText(QString("%1x%2").arg((*i)->GetWidth())
                      .arg((*i)->GetHeight()), "resolution");
            item->SetText((*i)->GetCountries().join(","), "countries");
            item->SetText(QString::number((*i)->GetSeason()), "season");
            item->SetText(QString::number((*i)->GetEpisode()), "episode");

            off_t bytes = (*i)->GetFilesize();
            if (bytes > 0)
            {
                item->SetText(QString::number(bytes), "filesize");

                QString tmpSize;

                tmpSize.sprintf("%0.2f ", bytes / 1024.0 / 1024.0);
                tmpSize += QObject::tr("MB", "Megabytes");
                item->SetText(tmpSize, "filesize_str");
            }
            else if (!(*i)->GetDownloadable())
            {
                item->SetText(tr("Web Only"), "filesize");
                item->SetText(tr("Web Only"), "filesize_str");
            }
            else
            {
                item->SetText(tr("Downloadable"), "filesize");
                item->SetText(tr("Downloadable"), "filesize_str");
            }

            item->SetData(qVariantFromValue(*i));

            if (!(*i)->GetThumbnail().isEmpty())
            {
                QString fileprefix = GetConfDir();

                QDir dir(fileprefix);
                if (!dir.exists())
                        dir.mkdir(fileprefix);

                fileprefix += "/MythNetvision";

                dir = QDir(fileprefix);
                if (!dir.exists())
                    dir.mkdir(fileprefix);

                fileprefix += "/thumbcache";

                dir = QDir(fileprefix);
                if (!dir.exists())
                    dir.mkdir(fileprefix);

                QString title = (*i)->GetTitle();
                QString url = (*i)->GetThumbnail();
                QUrl qurl(url);
                QString ext = QFileInfo(qurl.path()).suffix();
                QString sFilename = QString("%1/%2_%3.%4")
                    .arg(fileprefix)
                    .arg(qChecksum(url.toLocal8Bit().constData(),
                               url.toLocal8Bit().size()))
                    .arg(qChecksum(title.toLocal8Bit().constData(),
                               title.toLocal8Bit().size()))
                    .arg(ext);
                uint pos = m_searchResultList->GetItemPos(item);
                m_imageDownload->addURL((*i)->GetTitle(),
                                        (*i)->GetThumbnail(),
                                        pos);
            }
        }
        else
            delete item;
    }
    m_imageDownload->start();
}

void NetSearch::showWebVideo()
{
    QMutexLocker locker(&m_lock);

    ResultVideo *item =
        qVariantValue<ResultVideo *>(m_searchResultList->GetDataValue());

    if (!item)
        return;

    QString url = item->GetURL();

    VERBOSE(VB_GENERAL|VB_EXTRA, QString("Web URL = %1").arg(url));

    if (url.isEmpty())
        return;

    QString browser = gContext->GetSetting("WebBrowserCommand", "");
    QString zoom = gContext->GetSetting("WebBrowserZoomLevel", "1.0");

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
        myth_system(cmd, MYTH_SYSTEM_DONT_BLOCK_PARENT);
        gContext->GetMainWindow()->AllowInput(true);
        return;
    }
}

void NetSearch::doPlayVideo()
{
    QMutexLocker locker(&m_lock);

    ResultVideo *item =
          qVariantValue<ResultVideo *>(m_searchResultList->GetDataValue());

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

    ResultVideo *item =
          qVariantValue<ResultVideo *>(m_searchResultList->GetDataValue());

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

void NetSearch::doDownloadAndPlay()
{
    QMutexLocker locker(&m_lock);

    ResultVideo *item =
          qVariantValue<ResultVideo *>(m_searchResultList->GetDataValue());

    if (!item)
        return;

    if (!item->GetPlayer().isEmpty())
    {
        m_externaldownload = new QProcess();
        QString cmd = item->GetPlayer();
        QStringList args = item->GetPlayerArguments();
        args.replaceInStrings("%DIR%", QString(GetConfDir() + "/MythNetvision"));
        args.replaceInStrings("%MEDIAURL%", item->GetMediaURL());
        args.replaceInStrings("%URL%", item->GetURL());
        args.replaceInStrings("%TITLE%", item->GetTitle());

        m_externaldownload->setReadChannel(QProcess::StandardOutput);

        m_externaldownload->start(cmd, args);
    }
    else
    {
        if (m_download->isRunning())
        {
            QString message = tr("Download already running.  Try again "
                                 "when the download is finished.");

            MythConfirmationDialog *okPopup =
                new MythConfirmationDialog(m_popupStack, message, false);

            if (okPopup->Create())
                m_popupStack->AddScreen(okPopup);
            else
                delete okPopup;

            return;
        }

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

        m_download->addDL(item);
        m_download->start();
    }
}

void NetSearch::slotItemChanged()
{
    QMutexLocker locker(&m_lock);

    ResultVideo *item =
              qVariantValue<ResultVideo *>(m_searchResultList->GetDataValue());

    if (item && GetFocusWidget() == m_searchResultList)
    {
        if (m_title)
            m_title->SetText(item->GetTitle());
        if (m_description)
            m_description->SetText(item->GetDescription());
        if (m_url)
            m_url->SetText(item->GetURL());
        if (m_thumbnail)
            m_thumbnail->SetText(item->GetThumbnail());
        if (m_mediaurl)
            m_mediaurl->SetText(item->GetMediaURL());
        if (m_author)
            m_author->SetText(item->GetAuthor());
        if (m_date)
            m_date->SetText(item->GetDate().toString(
                    gContext->GetSetting("DateFormat",
                    "yyyy-MM-dd hh:mm")));
        if (m_time)
            m_time->SetText(item->GetTime());
        if (m_rating)
            m_rating->SetText(item->GetRating());
        if (m_width)
            m_width->SetText(QString::number(item->GetWidth()));
        if (m_height)
            m_height->SetText(QString::number(item->GetHeight()));
        if (m_resolution)
            m_resolution->SetText(QString("%1x%2")
                          .arg(item->GetWidth()).arg(item->GetHeight()));
        if (m_countries)
            m_countries->SetText(item->GetCountries().join(","));
        if (m_season)
            m_season->SetText(QString::number(item->GetSeason()));
        if (m_episode)
            m_episode->SetText(QString::number(item->GetEpisode()));

        if (m_filesize || m_filesize_str)
        {
            off_t bytes = item->GetFilesize();
            if (m_filesize)
            {
                if (bytes == 0 && !item->GetDownloadable())
                    m_filesize_str->SetText(tr("Web Only"));
                else if (bytes == 0 && item->GetDownloadable())
                    m_filesize_str->SetText(tr("Downloadable"));
                else
                    m_filesize->SetText(QString::number(bytes));
            }
            if (m_filesize_str)
            {
                QString tmpSize;

                tmpSize.sprintf("%0.2f ", bytes / 1024.0 / 1024.0);
                tmpSize += QObject::tr("MB", "Megabytes");
                if (bytes == 0 && !item->GetDownloadable())
                    m_filesize_str->SetText(tr("Web Only"));
                else if (bytes == 0 && item->GetDownloadable())
                    m_filesize_str->SetText(tr("Downloadable"));
                else
                    m_filesize_str->SetText(tmpSize);
            }
        }

        if (!item->GetThumbnail().isEmpty() && m_thumbImage)
        {
            MythUIButtonListItem *btn = m_searchResultList->GetItemCurrent();
            QString filename = btn->GetImage();
            if (!filename.isEmpty())
            {
                m_thumbImage->SetFilename(filename);
                m_thumbImage->Load();
                m_thumbImage->SetVisible(true);
            }
            else
                m_thumbImage->SetVisible(false);
        }
        else if (m_thumbImage)
            m_thumbImage->SetVisible(false);

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

        if (m_title)
            m_title->SetText(item->GetText());
        if (m_description)
            m_description->SetText("");
        if (m_url)
            m_url->SetText("");
        if (m_thumbnail)
            m_thumbnail->SetText("");
        if (m_author)
            m_author->SetText("");
        if (m_mediaurl)
            m_mediaurl->SetText("");
        if (m_date)
            m_date->SetText("");
        if (m_time)
            m_time->SetText("");
        if (m_rating)
            m_rating->SetText("");
        if (m_filesize)
            m_filesize->SetText("");
        if (m_filesize_str)
            m_filesize_str->SetText("");

        if (m_thumbImage)
        {
            QString filename = item->GetImage();
            if (!filename.isEmpty())
            {
                m_thumbImage->SetFilename(filename);
                m_thumbImage->Load();
                m_thumbImage->SetVisible(true);
            }
            else
                m_thumbImage->SetVisible(false);
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

QString NetSearch::getDownloadFilename(ResultVideo *item)
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
    if (event->type() == ImageDLEvent::kEventType)
    {
        ImageDLEvent *ide = (ImageDLEvent *)event;

        ImageData *id = ide->imageData;
        if (!id)
            return;

        if ((uint)m_searchResultList->GetCount() <= id->pos)
        {
            delete id;
            return;
        }

        MythUIButtonListItem *item =
                  m_searchResultList->GetItemAt(id->pos);

        if (item && item->GetText() == id->title)
        {
            item->SetImage(id->filename);
        }

        delete id;
    }
    else if (event->type() == VideoDLEvent::kEventType)
    {
        VideoDLEvent *vde = (VideoDLEvent *)event;

        VideoDL *dl = vde->videoDL;
        if (!dl)
            return;

        GetMythMainWindow()->HandleMedia("Internal", dl->filename);
        delete dl;
    }
}

