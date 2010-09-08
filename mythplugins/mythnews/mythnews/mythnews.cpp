// POSIX headers
#include <unistd.h>

// C headers
#include <cmath>

// QT headers
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QTimer>
#include <QRegExp>
#include <QUrl>

// MythTV headers
#include <mythuitext.h>
#include <mythuiimage.h>
#include <mythdialogbox.h>
#include <mythuibuttonlist.h>
#include <mythprogressdialog.h>

#include <mythmainwindow.h>
#include <util.h>
#include <httpcomms.h>
#include <mythcontext.h>
#include <mythdb.h>
#include <mythdirs.h>

// MythNews headers
#include "mythnews.h"
#include "mythnewseditor.h"
#include "newsdbutil.h"

#define LOC      QString("MythNews: ")
#define LOC_WARN QString("MythNews, Warning: ")
#define LOC_ERR  QString("MythNews, Error: ")

/** \brief Creates a new MythNews Screen
 *  \param parent Pointer to the screen stack
 *  \param name The name of the window
 */
MythNews::MythNews(MythScreenStack *parent, QString name) :
    MythScreenType(parent, name),
    m_lock(QMutex::Recursive)
{
    // Setup cache directory

    QString fileprefix = GetConfDir();

    QDir dir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);
    fileprefix += "/MythNews";
    dir = QDir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    m_zoom = gCoreContext->GetSetting("WebBrowserZoomLevel", "1.4");
    m_browser = gCoreContext->GetSetting("WebBrowserCommand", "");

    // Initialize variables

    m_sitesList = m_articlesList = NULL;
    m_updatedText = m_titleText = m_descText = NULL;
    m_thumbnailImage = m_downloadImage = m_enclosureImage = NULL;
    m_menuPopup = NULL;
    m_progressPopup = NULL;

    m_TimerTimeout = 10*60*1000;
    m_httpGrabber = NULL;

    m_timeFormat = gCoreContext->GetSetting("TimeFormat", "h:mm AP");
    m_dateFormat = gCoreContext->GetSetting("DateFormat", "ddd MMMM d");

    // Now do the actual work
    m_RetrieveTimer = new QTimer(this);
    connect(m_RetrieveTimer, SIGNAL(timeout()),
            this, SLOT(slotRetrieveNews()));
    m_UpdateFreq = gCoreContext->GetNumSetting("NewsUpdateFrequency", 30);

    m_RetrieveTimer->stop();
    m_RetrieveTimer->setSingleShot(false);
    m_RetrieveTimer->start(m_TimerTimeout);
}

MythNews::~MythNews()
{
    QMutexLocker locker(&m_lock);
}

bool MythNews::Create(void)
{
    QMutexLocker locker(&m_lock);

    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("news-ui.xml", "news", this);

    if (!foundtheme)
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_sitesList, "siteslist", &err);
    UIUtilE::Assign(this, m_articlesList, "articleslist", &err);
    UIUtilE::Assign(this, m_titleText, "title", &err);
    UIUtilE::Assign(this, m_descText, "description", &err);

    // these are all optional
    UIUtilW::Assign(this, m_nositesText, "nosites", &err);
    UIUtilW::Assign(this, m_updatedText, "updated", &err);
    UIUtilW::Assign(this, m_thumbnailImage, "thumbnail", &err);
    UIUtilW::Assign(this, m_enclosureImage, "enclosures", &err);
    UIUtilW::Assign(this, m_downloadImage, "download", &err);
    UIUtilW::Assign(this, m_podcastImage, "ispodcast", &err);

    if (err)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'news'");
        return false;
    }

    if (m_nositesText)
    {
        m_nositesText->SetText(tr("You haven't configured MythNews to use any sites."));
        m_nositesText->Hide();
    }

    BuildFocusList();

    SetFocusWidget(m_sitesList);

    loadSites();
    updateInfoView(m_sitesList->GetItemFirst());

    connect(m_sitesList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            this, SLOT( slotSiteSelected(MythUIButtonListItem*)));
    connect(m_articlesList, SIGNAL(itemSelected( MythUIButtonListItem*)),
            this, SLOT( updateInfoView(MythUIButtonListItem*)));
    connect(m_articlesList, SIGNAL(itemClicked( MythUIButtonListItem*)),
            this, SLOT( slotViewArticle(MythUIButtonListItem*)));

    return true;
}

void MythNews::clearSites(void)
{
    m_NewsSites.clear();
    m_sitesList->Reset();
    m_articles.clear();
    m_articlesList->Reset();

    m_titleText->SetText("");
    m_descText->SetText("");

    if (m_updatedText)
        m_updatedText->SetText("");

    if (m_downloadImage)
        m_downloadImage->Hide();

    if (m_enclosureImage)
        m_enclosureImage->Hide();

    if (m_podcastImage)
        m_podcastImage->Hide();

    if (m_thumbnailImage)
        m_thumbnailImage->Hide();
}

void MythNews::loadSites(void)
{
    QMutexLocker locker(&m_lock);

    clearSites();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT name, url, ico, updated, podcast "
        "FROM newssites "
        "ORDER BY name");

    if (!query.exec())
    {
        MythDB::DBError(LOC_ERR + "Could not load sites from DB", query);
        return;
    }

    while (query.next())
    {
        QString name = query.value(0).toString();
        QString url  = query.value(1).toString();
        QString icon = query.value(2).toString();
        QDateTime time; time.setTime_t(query.value(3).toUInt());
        bool podcast = query.value(4).toInt();
        m_NewsSites.push_back(new NewsSite(name, url, time, podcast));
    }

    NewsSite::List::iterator it = m_NewsSites.begin();
    for (; it != m_NewsSites.end(); ++it)
    {
        MythUIButtonListItem *item =
            new MythUIButtonListItem(m_sitesList, (*it)->name());
        item->SetData(qVariantFromValue(*it));

        connect(*it, SIGNAL(finished(NewsSite*)),
                this, SLOT(slotNewsRetrieved(NewsSite*)));
    }

    slotRetrieveNews();

    if (m_nositesText)
    {
        if (m_NewsSites.size() == 0)
            m_nositesText->Show();
        else
            m_nositesText->Hide();
    }
}

void MythNews::updateInfoView(MythUIButtonListItem *selected)
{
    QMutexLocker locker(&m_lock);

    if (!selected)
        return;

    NewsSite *site = NULL;
    NewsArticle article;

    if (GetFocusWidget() == m_articlesList)
    {
        article = m_articles[selected];
        if (m_sitesList->GetItemCurrent())
            site = qVariantValue<NewsSite*>
                (m_sitesList->GetItemCurrent()->GetData());
    }
    else
    {
        site = qVariantValue<NewsSite*>(selected->GetData());
        if (m_articlesList->GetItemCurrent())
            article = m_articles[m_articlesList->GetItemCurrent()];
    }

    if (GetFocusWidget() == m_articlesList)
    {
        if (!article.title().isEmpty())
        {

            if (m_titleText)
                m_titleText->SetText(article.title());

            if (m_descText)
            {
                QString artText = article.description();
                // Replace paragraph and break HTML with newlines
                if( artText.indexOf(QRegExp("</(p|P)>")) )
                {
                    artText.replace( QRegExp("<(p|P)>"), "");
                    artText.replace( QRegExp("</(p|P)>"), "\n\n");
                }
                else
                {
                    artText.replace( QRegExp("<(p|P)>"), "\n\n");
                    artText.replace( QRegExp("</(p|P)>"), "");
                }
                artText.replace( QRegExp("<(br|BR|)/>"), "\n");
                artText.replace( QRegExp("<(br|BR|)>"), "\n");
                // These are done instead of simplifyWhitespace
                // because that function also strips out newlines
                // Replace tab characters with nothing
                artText.replace( QRegExp("\t"), "");
                // Replace double space with single
                artText.replace( QRegExp("  "), "");
                // Replace whitespace at beginning of lines with newline
                artText.replace( QRegExp("\n "), "\n");
                // Remove any remaining HTML tags
                QRegExp removeHTML(QRegExp("</?.+>"));
                removeHTML.setMinimal(true);
                artText.remove((const QRegExp&) removeHTML);
                artText = artText.trimmed();
                m_descText->SetText(artText);
            }

            if (!article.thumbnail().isEmpty())
            {
                QString fileprefix = GetConfDir();

                QDir dir(fileprefix);
                if (!dir.exists())
                        dir.mkdir(fileprefix);

                fileprefix += "/MythNews/tcache";

                dir = QDir(fileprefix);
                if (!dir.exists())
                    dir.mkdir(fileprefix);

                QString url = article.thumbnail();
                QString sFilename = QString("%1/%2")
                    .arg(fileprefix)
                    .arg(qChecksum(url.toLocal8Bit().constData(),
                                   url.toLocal8Bit().size()));

                bool exists = QFile::exists(sFilename);
                if (!exists)
                    HttpComms::getHttpFile(sFilename, url, 20000, 1, 2);

                if (m_thumbnailImage)
                {
                    m_thumbnailImage->SetFilename(sFilename);
                    m_thumbnailImage->Load();

                    if (!m_thumbnailImage->IsVisible())
                        m_thumbnailImage->Show();
                }
            }
            else
            {
                if (m_thumbnailImage)
                    m_thumbnailImage->Hide();

                if (!site->imageURL().isEmpty())
                {
                    QString fileprefix = GetConfDir();

                    QDir dir(fileprefix);
                    if (!dir.exists())
                        dir.mkdir(fileprefix);

                    fileprefix += "/MythNews/scache";

                    dir = QDir(fileprefix);
                    if (!dir.exists())
                        dir.mkdir(fileprefix);

                    QString url = site->imageURL();
                    QString extension = url.section('.', -1);

                    QString sitename = site->name();
                    QString sFilename = QString("%1/%2.%3").arg(fileprefix)
                                                           .arg(sitename)
                                                           .arg(extension);

                    bool exists = QFile::exists(sFilename);
                    if (!exists)
                        HttpComms::getHttpFile(sFilename, url, 20000, 1, 2);

                    if (m_thumbnailImage)
                    {
                        m_thumbnailImage->SetFilename(sFilename);
                        m_thumbnailImage->Load();

                        if (!m_thumbnailImage->IsVisible())
                            m_thumbnailImage->Show();
                    }
                }
            }

            if (m_downloadImage)
            {
                if (!article.enclosure().isEmpty())
                {
                    if (!m_downloadImage->IsVisible())
                        m_downloadImage->Show();
                }
                else
                    m_downloadImage->Hide();
            }

            if (m_enclosureImage)
            {
                if (!article.enclosure().isEmpty())
                {
                    if (!m_enclosureImage->IsVisible())
                        m_enclosureImage->Show();
                }
                else
                    m_enclosureImage->Hide();
            }

            if (m_podcastImage)
                m_podcastImage->Hide();
        }
    }
    else
    {
        if (m_downloadImage)
            m_downloadImage->Hide();

        if (m_enclosureImage)
            m_enclosureImage->Hide();

        if (m_podcastImage)
            m_podcastImage->Hide();

        if (site)
        {
            if (m_titleText)
                m_titleText->SetText(site->name());

            if (m_descText)
                m_descText->SetText(site->description());

            if (m_thumbnailImage && m_thumbnailImage->IsVisible())
                m_thumbnailImage->Hide();

            if (m_podcastImage && site->podcast() == 1)
                m_podcastImage->Show();

            if (!site->imageURL().isEmpty())
            {
                QString fileprefix = GetConfDir();

                QDir dir(fileprefix);
                if (!dir.exists())
                        dir.mkdir(fileprefix);

                fileprefix += "/MythNews/scache";

                dir = QDir(fileprefix);
                if (!dir.exists())
                    dir.mkdir(fileprefix);

                QString url = site->imageURL();
                QString extension = url.section('.', -1);

                QString sitename = site->name();
                QString sFilename = QString("%1/%2.%3").arg(fileprefix)
                                                        .arg(sitename)
                                                        .arg(extension);

                bool exists = QFile::exists(sFilename);
                if (!exists)
                    HttpComms::getHttpFile(sFilename, url, 20000, 1, 2);

                if (m_thumbnailImage)
                {
                    m_thumbnailImage->SetFilename(sFilename);
                    m_thumbnailImage->Load();

                    if (!m_thumbnailImage->IsVisible())
                        m_thumbnailImage->Show();
                }
            }
        }
    }

    if (m_updatedText) 
    {

        if (site)
        {
            QString text(tr("Updated") + " - ");
            QDateTime updated(site->lastUpdated());
            if (updated.toTime_t() != 0) {
                text += site->lastUpdated().toString(m_dateFormat) + " ";
                text += site->lastUpdated().toString(m_timeFormat);
            }
            else
                text += tr("Unknown");
            m_updatedText->SetText(text);
        }

        if (m_httpGrabber != NULL)
        {
            int progress = m_httpGrabber->getProgress();
            int total = m_httpGrabber->getTotal();
            if ((progress > 0) && (total > 0) && (progress < total))
            {
                float fProgress = (float)progress/total;
                QString text = tr("%1 of %2 (%3 percent)")
                        .arg(formatSize(progress, 2))
                        .arg(formatSize(total, 2))
                        .arg(floor(fProgress*100));
                m_updatedText->SetText(text);
            }
        }
    }
}

QString MythNews::formatSize(long long bytes, int prec)
{
    long long sizeKB = bytes / 1024;

    if (sizeKB>1024*1024*1024) // Terabytes
    {
        double sizeGB = sizeKB/(1024*1024*1024.0);
        return QString("%1 TB").arg(sizeGB, 0, 'f', (sizeGB>10)?0:prec);
    }
    else if (sizeKB>1024*1024) // Gigabytes
    {
        double sizeGB = sizeKB/(1024*1024.0);
        return QString("%1 GB").arg(sizeGB, 0, 'f', (sizeGB>10)?0:prec);
    }
    else if (sizeKB>1024) // Megabytes
    {
        double sizeMB = sizeKB/1024.0;
        return QString("%1 MB").arg(sizeMB, 0, 'f', (sizeMB>10)?0:prec);
    }
    // Kilobytes
    return QString("%1 KB").arg(sizeKB);
}

bool MythNews::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("News", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "RETRIEVENEWS")
            slotRetrieveNews();
        else if (action == "CANCEL")
            cancelRetrieve();
        else if (action == "MENU")
            ShowMenu();
        else if (action == "EDIT")
            ShowEditDialog(true);
        else if (action == "DELETE")
            deleteNewsSite();
        else if (action == "ESCAPE")
        {
            {
                QMutexLocker locker(&m_lock);

                if (m_progressPopup)
                {
                    m_progressPopup->Close();
                    m_progressPopup = NULL;
                }

                m_RetrieveTimer->stop();

                if (m_httpGrabber)
                    m_abortHttp = true;
            }

            Close();
        }
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void MythNews::slotRetrieveNews(void)
{
    QMutexLocker locker(&m_lock);

    if (m_NewsSites.empty())
        return;

    m_RetrieveTimer->stop();

    NewsSite::List::iterator it = m_NewsSites.begin();
    for (; it != m_NewsSites.end(); ++it)
    {
        if ((*it)->timeSinceLastUpdate() > m_UpdateFreq)
            (*it)->retrieve();
        else
            processAndShowNews(*it);
    }

    m_RetrieveTimer->stop();
    m_RetrieveTimer->setSingleShot(false);
    m_RetrieveTimer->start(m_TimerTimeout);
}

void MythNews::slotNewsRetrieved(NewsSite *site)
{
    unsigned int updated = site->lastUpdated().toTime_t();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE newssites SET updated = :UPDATED "
                  "WHERE name = :NAME ;");
    query.bindValue(":UPDATED", updated);
    query.bindValue(":NAME", site->name());
    if (!query.exec() || !query.isActive())
        MythDB::DBError("news update time", query);

    processAndShowNews(site);
}

void MythNews::cancelRetrieve(void)
{
    QMutexLocker locker(&m_lock);

    NewsSite::List::iterator it = m_NewsSites.begin();
    for (; it != m_NewsSites.end(); ++it)
    {
        (*it)->stop();
        processAndShowNews(*it);
    }
}

void MythNews::processAndShowNews(NewsSite *site)
{
    QMutexLocker locker(&m_lock);

    if (!site)
        return;

    site->process();

    MythUIButtonListItem *siteUIItem = m_sitesList->GetItemCurrent();
    if (!siteUIItem)
        return;

    if (site != qVariantValue<NewsSite*>(siteUIItem->GetData()))
        return;

    m_articlesList->Reset();
    m_articles.clear();

    NewsArticle::List articles = site->GetArticleList();
    NewsArticle::List::iterator it = articles.begin();
    for (; it != articles.end(); ++it)
    {
        MythUIButtonListItem *item =
            new MythUIButtonListItem(m_articlesList, (*it).title());
        m_articles[item] = *it;
    }
}

void MythNews::slotSiteSelected(MythUIButtonListItem *item)
{
    QMutexLocker locker(&m_lock);

    if (!item || item->GetData().isNull())
        return;

    NewsSite *site = qVariantValue<NewsSite*>(item->GetData());
    if (!site)
        return;

    m_articlesList->Reset();
    m_articles.clear();

    NewsArticle::List articles = site->GetArticleList();
    NewsArticle::List::iterator it = articles.begin();
    for (; it != articles.end(); ++it)
    {
        MythUIButtonListItem *item =
            new MythUIButtonListItem(m_articlesList, (*it).title());
        m_articles[item] = *it;
    }

    updateInfoView(item);
}

void MythNews::slotProgressCancelled(void)
{
    QMutexLocker locker(&m_lock);

    m_abortHttp = true;
}

void MythNews::createProgress(QString title)
{
    QMutexLocker locker(&m_lock);

    if (m_progressPopup)
        return;

    QString message = title;

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    m_progressPopup = new MythUIProgressDialog(message, popupStack,
                                               "mythnewsprogressdialog");

    if (m_progressPopup->Create())
        popupStack->AddScreen(m_progressPopup, false);
    else
    {
        delete m_progressPopup;
        m_progressPopup = NULL;
    }
}

bool MythNews::getHttpFile(QString sFilename, QString cmdURL)
{
    QMutexLocker locker(&m_lock);

    int redirectCount = 0;
    int timeoutCount = 0;
    QByteArray data(0);
    bool res = false;
    m_httpGrabber = NULL;
    QString hostname = "";

    createProgress(QObject::tr("Downloading media..."));
    while (1)
    {
        QUrl qurl(cmdURL);
        if (hostname.isEmpty())
            hostname = qurl.host();  // hold onto original host

        if (qurl.host().isEmpty()) // can occur on redirects to partial paths
            qurl.setHost(hostname);

        if (m_httpGrabber != NULL)
            delete m_httpGrabber;

        m_httpGrabber = new HttpComms;
        m_abortHttp = false;

        m_httpGrabber->request(qurl, -1, true);

        while ((!m_httpGrabber->isDone()) && (!m_abortHttp))
        {
            int total = m_httpGrabber->getTotal();
            m_progressPopup->SetTotal(total);
            int progress = m_httpGrabber->getProgress();
            m_progressPopup->SetProgress(progress);
            if ((progress > 0) && (total > 0) && (progress < total))
            {
                float fProgress = (float)progress/total;
                QString text = tr("%1 of %2 (%3 percent)")
                        .arg(formatSize(progress, 2))
                        .arg(formatSize(total, 2))
                        .arg(floor(fProgress*100));
                if (m_updatedText)
                    m_updatedText->SetText(text);
            }
            qApp->processEvents();
            usleep(100000);
        }

        if (m_abortHttp)
            break;

        // Check for redirection
        if (!m_httpGrabber->getRedirectedURL().isEmpty())
        {
            if (redirectCount++ < 3)
                cmdURL = m_httpGrabber->getRedirectedURL();

            // Try again
            timeoutCount = 0;
            continue;
        }

        data = m_httpGrabber->getRawData();

        if (data.size() > 0)
        {
            QFile file(sFilename);
            if (file.open(QIODevice::WriteOnly))
            {
                file.write(data);
                file.close();
                res = true;
            }
        }
        break;
    }

    if (m_progressPopup)
    {
        m_progressPopup->Close();
        m_progressPopup = NULL;
    }

    delete m_httpGrabber;
    m_httpGrabber = NULL;
    return res;

}

void MythNews::slotViewArticle(MythUIButtonListItem *articlesListItem)
{
    QMutexLocker locker(&m_lock);

    QMap<MythUIButtonListItem*,NewsArticle>::const_iterator it =
        m_articles.find(articlesListItem);

    if (it == m_articles.end())
        return;

    const NewsArticle article = *it;

    if (article.articleURL().isEmpty())
        return;

    if (article.enclosure().isEmpty())
    {
        QString cmdUrl(article.articleURL());

        if (m_browser.isEmpty())
        {
            ShowOkPopup(tr("No browser command set! MythNews needs MythBrowser to be installed."));
            return;
        }

        // display the web page
        if (m_browser.toLower() == "internal")
        {
            GetMythMainWindow()->HandleMedia("WebBrowser", cmdUrl);
            return;
        }
        else
        {
            QString cmd = m_browser;
            cmd.replace("%ZOOM%", m_zoom);
            cmd.replace("%URL%", cmdUrl);
            cmd.replace('\'', "%27");
            cmd.replace("&","\\&");
            cmd.replace(";","\\;");

            GetMythMainWindow()->AllowInput(false);
            myth_system(cmd, kMSDontDisableDrawing);
            GetMythMainWindow()->AllowInput(true);
            return;
        }
    }

    QString cmdURL(article.enclosure());

    // Handle special cases for media here
    // YouTube: Fetch the mediaURL page and parse out the video URL
    if (cmdURL.contains("youtube.com"))
    {
        cmdURL = QString(article.mediaURL());
        QString mediaPage = HttpComms::getHttp(cmdURL, 0, 1, 2);
        if (!mediaPage.isEmpty())
        {
            // If this breaks in the future, we are building the URL
            // to download a video.  At this time, this requires
            // the video_id and the t argument
            // from the source HTML of the page
            int playerPos = mediaPage.indexOf("swfArgs") + 7;

            int tArgStart = mediaPage.indexOf("\"t\": \"",
                                              playerPos) + 6;
            int tArgEnd = mediaPage.indexOf("\"", tArgStart);
            QString tArgString = mediaPage.mid(tArgStart,
                                               tArgEnd - tArgStart);

            int vidStart = mediaPage.indexOf("\"video_id\": \"",
                                             playerPos) + 13;
            int vidEnd = mediaPage.indexOf("\"", vidStart);
            QString vidString = mediaPage.mid(vidStart,
                                              vidEnd - vidStart);

            cmdURL = QString("http://youtube.com/get_video.php"
                             "?video_id=%2&t=%1")
                .arg(tArgString).arg(vidString);
            VERBOSE(VB_GENERAL, LOC + QString("VideoURL '%1'").arg(cmdURL));
        }
    }

    QString fileprefix = GetConfDir();

    QDir dir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    fileprefix += "/MythNews";

    dir = QDir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    QString sFilename(fileprefix + "/newstempfile");

    if (getHttpFile(sFilename, cmdURL))
        playVideo(sFilename);
}

void MythNews::ShowEditDialog(bool edit)
{
    QMutexLocker locker(&m_lock);

    NewsSite *site = NULL;

    if (edit)
    {
        MythUIButtonListItem *siteListItem = m_sitesList->GetItemCurrent();

        if (!siteListItem || siteListItem->GetData().isNull())
            return;

        site = qVariantValue<NewsSite*>(siteListItem->GetData());
    }

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    MythNewsEditor *mythnewseditor = new MythNewsEditor(site, edit, mainStack,
                                                        "mythnewseditor");

    if (mythnewseditor->Create())
    {
        connect(mythnewseditor, SIGNAL(Exiting()), SLOT(loadSites()));
        mainStack->AddScreen(mythnewseditor);
    }
    else
        delete mythnewseditor;
}

void MythNews::ShowMenu(void)
{
    QMutexLocker locker(&m_lock);

    QString label = tr("Options");

    MythScreenStack *popupStack =
                            GetMythMainWindow()->GetStack("popup stack");

    m_menuPopup = new MythDialogBox(label, popupStack, "mythnewsmenupopup");

    if (m_menuPopup->Create())
    {
        popupStack->AddScreen(m_menuPopup);

        m_menuPopup->SetReturnEvent(this, "options");

        if (m_NewsSites.size() > 0)
            m_menuPopup->AddButton(tr("Edit News Site"));
        m_menuPopup->AddButton(tr("Add News Site"));
        if (m_NewsSites.size() > 0)
            m_menuPopup->AddButton(tr("Delete News Site"));
        m_menuPopup->AddButton(tr("Cancel"));
    }
    else
    {
        delete m_menuPopup;
        m_menuPopup = NULL;
    }
}

void MythNews::deleteNewsSite(void)
{
    QMutexLocker locker(&m_lock);

    MythUIButtonListItem *siteUIItem = m_sitesList->GetItemCurrent();

    if (siteUIItem && !siteUIItem->GetData().isNull())
    {
        NewsSite *site = qVariantValue<NewsSite*>(siteUIItem->GetData());
        if (site)
        {
            removeFromDB(site->name());
            loadSites();
        }
    }
}

// does not need locking
void MythNews::playVideo(const QString &filename)
{
    QString command_string = gCoreContext->GetSetting("VideoDefaultPlayer");

    sendPlaybackStart();

    if ((command_string.indexOf("Internal", 0, Qt::CaseInsensitive) > -1) ||
        (command_string.length() < 1))
    {
        command_string = "Internal";
        GetMythMainWindow()->HandleMedia(command_string, filename);
    }
    else
    {
        if (command_string.contains("%s"))
            command_string = command_string.replace("%s", filename);

        myth_system(command_string);
    }

    sendPlaybackEnd();
}

// does not need locking
void MythNews::customEvent(QEvent *event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = (DialogCompletionEvent*)(event);

        QString resultid  = dce->GetId();
        int     buttonnum = dce->GetResult();

        if (resultid == "options")
        {
            if (m_NewsSites.size() > 0)
            {
                if (buttonnum == 0)
                    ShowEditDialog(true);
                else if (buttonnum == 1)
                    ShowEditDialog(false);
                else if (buttonnum == 2)
                    deleteNewsSite();
            }
            else
                if (buttonnum == 0)
                    ShowEditDialog(false);
        }

        m_menuPopup = NULL;
    }
}
