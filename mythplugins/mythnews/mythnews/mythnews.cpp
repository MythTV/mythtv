/* ============================================================
 * File  : mythnews.cpp
 * Author: Renchi Raju <renchi@pooh.tam.uiuc.edu>
 * Date  : 2003-08-30
 * Description :
 *
 * Copyright 2003 by Renchi Raju

 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published bythe Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ============================================================ */

// C Headers
#include <unistd.h>
#include <math.h>

// QT headers
#include <qapplication.h>
#include <q3network.h>
#include <qdatetime.h>
#include <qdir.h>
#include <qtimer.h>
#include <qregexp.h>
//Added by qt3to4:
#include <QUrl>

// MythTV headers
#include <mythtv/libmythui/mythmainwindow.h>
#include <mythtv/util.h>
#include <mythtv/httpcomms.h>
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>

// MythNews headers
#include "mythnews.h"
#include "mythnewseditor.h"
#include "newsdbutil.h"

// MythNewsBusyDialog::MythNewsBusyDialog(const QString &title)
//     : MythBusyDialog(title)
// {
// }
// 
// MythNewsBusyDialog::~MythNewsBusyDialog()
// {
// }
// 
// void MythNewsBusyDialog::keyPressEvent(QKeyEvent *e)
// {
//     bool handled = false;
//     QStringList actions;
//     if (gContext->GetMainWindow()->TranslateKeyPress("qt", e, actions))
//     {
//         for (unsigned int i = 0; i < actions.size() && !handled; i++)
//         {
//             QString action = actions[i];
//             if (action == "ESCAPE")
//             {
//                 emit cancelAction();
//                 handled = true;
//             }
//         }
//     }
// 
//     if (!handled)
//         MythDialog::keyPressEvent(e);
// }


/** \brief Creates a new MythNews Screen
 *  \param parent Pointer to the screen stack
 *  \param name The name of the window
 */
MythNews::MythNews(MythScreenStack *parent, const char *name)
    : MythScreenType (parent, name)
{
    // Setup cache directory

    QString fileprefix = MythContext::GetConfDir();

    QDir dir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);
    fileprefix += "/MythNews";
    dir = QDir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    zoom = QString("-z %1").arg(gContext->GetNumSetting("WebBrowserZoomLevel",200));
    browser = gContext->GetSetting("WebBrowserCommand",
                                   gContext->GetInstallPrefix() +
                                      "/bin/mythbrowser");

    // Initialize variables

    m_sitesList = m_articlesList = NULL;
    m_updatedText = m_titleText = m_descText = NULL;
    m_thumbnailImage = m_downloadImage = m_enclosureImage = NULL;
    m_menuPopup = NULL;

    m_TimerTimeout = 10*60*1000;
    httpGrabber = NULL;

    timeFormat = gContext->GetSetting("TimeFormat", "h:mm AP");
    dateFormat = gContext->GetSetting("DateFormat", "ddd MMMM d");

    // Now do the actual work
    m_RetrieveTimer = new QTimer(this);
    connect(m_RetrieveTimer, SIGNAL(timeout()),
            this, SLOT(slotRetrieveNews()));
    m_UpdateFreq = gContext->GetNumSetting("NewsUpdateFrequency", 30);

    m_NewsSites.setAutoDelete(true);

    m_RetrieveTimer->start(m_TimerTimeout, false);
}

MythNews::~MythNews()
{
    m_RetrieveTimer->stop();
}

bool MythNews::Create()
{

    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("news-ui.xml", "news", this);

    if (!foundtheme)
        return false;

    m_sitesList = dynamic_cast<MythListButton *>
                (GetChild("siteslist"));

    m_articlesList = dynamic_cast<MythListButton *>
                (GetChild("articleslist"));

    m_updatedText = dynamic_cast<MythUIText *>
                (GetChild("updated"));

    m_titleText = dynamic_cast<MythUIText *>
                (GetChild("title"));

    m_descText = dynamic_cast<MythUIText *>
                (GetChild("description"));

    m_thumbnailImage = dynamic_cast<MythUIImage *>
                (GetChild("thumbnail"));

    m_enclosureImage = dynamic_cast<MythUIImage *>
                (GetChild("enclosures"));

    m_downloadImage = dynamic_cast<MythUIImage *>
                (GetChild("download"));

    if (!m_sitesList || !m_articlesList || !m_enclosureImage ||
        !m_downloadImage || !m_titleText || !m_descText)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical theme elements.");
        return false;
    }

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist. Something is wrong");

    SetFocusWidget(m_sitesList);
    m_sitesList->SetActive(true);
    m_articlesList->SetActive(false);

    loadSites();

    connect(m_sitesList, SIGNAL(itemSelected(MythListButtonItem*)),
            this, SLOT( slotSiteSelected(MythListButtonItem*)));
    connect(m_articlesList, SIGNAL(itemSelected( MythListButtonItem*)),
            this, SLOT( updateInfoView(MythListButtonItem*)));
    connect(m_articlesList, SIGNAL(itemClicked( MythListButtonItem*)),
            this, SLOT( slotViewArticle(MythListButtonItem*)));

    return true;
}

void MythNews::loadSites(void)
{
    m_NewsSites.clear();
    m_sitesList->Reset();

    MSqlQuery query(MSqlQuery::InitCon());
    query.exec("SELECT name, url, ico, updated FROM newssites ORDER BY name");

    if (!query.isActive()) {
        VERBOSE(VB_IMPORTANT, "MythNews: Error in loading Sites from DB");
    }
    else {
        QString name;
        QString url;
        QString icon;
        QDateTime time;
        while ( query.next() ) {
            name = query.value(0).toString();
            url  = query.value(1).toString();
            icon = query.value(2).toString();
            time.setTime_t(query.value(3).toUInt());
            m_NewsSites.append(new NewsSite(name,url,time));
        }
    }

    for (NewsSite *site = m_NewsSites.first(); site; site = m_NewsSites.next())
    {
        MythListButtonItem* item =
            new MythListButtonItem(m_sitesList, site->name());
        item->setData(site);

        connect(site, SIGNAL(finished(NewsSite*)),
                this, SLOT(slotNewsRetrieved(NewsSite*)));
    }

    slotRetrieveNews();
}

void MythNews::updateInfoView(MythListButtonItem* selected)
{
    if (!selected)
        return;

    NewsSite *site = NULL;
    NewsArticle *article = NULL;

    if (GetFocusWidget() == m_articlesList)
    {
        article = (NewsArticle*) selected->getData();
        if (m_sitesList->GetItemCurrent())
            site = (NewsSite*) m_sitesList->GetItemCurrent()->getData();
    }
    else
    {
        site = (NewsSite*) selected->getData();
        if (m_articlesList->GetItemCurrent())
            article = (NewsArticle*) m_articlesList->GetItemCurrent()->getData();
    }

    if (GetFocusWidget() == m_articlesList)
    {

        if (article)
        {

            if (m_titleText)
                m_titleText->SetText(article->title());

            if (m_descText)
            {
                QString artText = article->description();
                // Replace paragraph and break HTML with newlines
                if( artText.find(QRegExp("</(p|P)>")) )
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
                artText = artText.stripWhiteSpace();
                m_descText->SetText(artText);
            }

            if (!article->thumbnail().isEmpty())
            {
                QString fileprefix = MythContext::GetConfDir();

                QDir dir(fileprefix);
                if (!dir.exists())
                        dir.mkdir(fileprefix);

                fileprefix += "/MythNews/tcache";

                dir = QDir(fileprefix);
                if (!dir.exists())
                    dir.mkdir(fileprefix);

                QString url = article->thumbnail();
                QString sFilename = QString("%1/%2")
                        .arg(fileprefix).arg(qChecksum(url,url.length()));

                bool exists = QFile::exists(sFilename);
                if (!exists)
                    HttpComms::getHttpFile(sFilename, url, 20000);

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
                if (!site->imageURL().isEmpty())
                {
                    QString fileprefix = MythContext::GetConfDir();

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
                        HttpComms::getHttpFile(sFilename, url, 20000);

                    if (m_thumbnailImage)
                    {
                        m_thumbnailImage->SetFilename(sFilename);
                        m_thumbnailImage->Load();

                        if (!m_thumbnailImage->IsVisible())
                            m_thumbnailImage->Show();
                    }
                }
            }

            if (!article->enclosure().isEmpty())
            {
                if (!m_downloadImage->IsVisible())
                    m_downloadImage->Show();
            }
            else
                m_downloadImage->Hide();

            if (!article->enclosure().isEmpty())
            {
                if (m_enclosureImage->IsVisible())
                    m_enclosureImage->Show();
            }
            else
                m_enclosureImage->Hide();
        }
    }
    else
    {
        m_downloadImage->Hide();
        m_enclosureImage->Hide();

        if (site)
        {
            if (m_titleText)
                m_titleText->SetText(site->name());

            if (m_descText)
                m_descText->SetText(site->description());

            if (m_thumbnailImage && m_thumbnailImage->IsVisible())
                m_thumbnailImage->Hide();

            if (!site->imageURL().isEmpty())
            {
                QString fileprefix = MythContext::GetConfDir();

                QDir dir(fileprefix);
                if (!dir.exists())
                        dir.mkdir(fileprefix);

                fileprefix += "/MythNews/scache";

                dir = QDir(fileprefix);
                if (!dir.exists())
                    dir.mkdir(fileprefix);

                QString sitename = site->name();
                QString sFilename(fileprefix + "/" + sitename + ".jpg");
                QString url = site->imageURL();

                bool exists = QFile::exists(sFilename);
                if (!exists)
                    HttpComms::getHttpFile(sFilename, url, 20000);

                if (m_thumbnailImage)
                {
                    m_thumbnailImage->SetFilename(sFilename);
                    m_thumbnailImage->Load();

                    if (m_thumbnailImage->IsVisible())
                        m_thumbnailImage->Show();
                }
            }
        }
    }

    if (m_updatedText) {

        if (site)
        {
            QString text(tr("Updated") + " - ");
            QDateTime updated(site->lastUpdated());
            if (updated.toTime_t() != 0) {
                text += site->lastUpdated().toString(dateFormat) + " ";
                text += site->lastUpdated().toString(timeFormat);
            }
            else
                text += tr("Unknown");
            m_updatedText->SetText(text);
        }

        if (httpGrabber != NULL)
        {
            int progress = httpGrabber->getProgress();
            int total = httpGrabber->getTotal();
            if ((progress > 0) && (total > 0) && (progress < total))
            {
                float fProgress = (float)progress/total;
                QString text = QString("%1 of %2 (%3 percent)")
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
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("News", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "LEFT")
            NextPrevWidgetFocus(false);
        else if (action == "RIGHT")
            NextPrevWidgetFocus(true);
        else if (action == "RETRIEVENEWS")
            slotRetrieveNews();
        else if (action == "CANCEL")
            cancelRetrieve();
        else if (action == "MENU")
            ShowMenu();
        else if (action == "ESCAPE")
            GetMythMainWindow()->GetMainStack()->PopScreen();
        else
            handled = false;
    }

    return handled;
}

void MythNews::slotRetrieveNews()
{
    if (m_NewsSites.count() == 0)
        return;

    m_RetrieveTimer->stop();

    for (NewsSite* site = m_NewsSites.first(); site; site = m_NewsSites.next())
    {
        if (site->timeSinceLastUpdate() > m_UpdateFreq)
            site->retrieve();
        else
            processAndShowNews(site);
    }

    m_RetrieveTimer->start(m_TimerTimeout, false);
}

void MythNews::slotNewsRetrieved(NewsSite* site)
{
    unsigned int updated = site->lastUpdated().toTime_t();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE newssites SET updated = :UPDATED "
                  "WHERE name = :NAME ;");
    query.bindValue(":UPDATED", updated);
    query.bindValue(":NAME", site->name());
    if (!query.exec() || !query.isActive())
        MythContext::DBError("news update time", query);

    processAndShowNews(site);
}

void MythNews::cancelRetrieve()
{
    for (NewsSite* site = m_NewsSites.first(); site;
         site = m_NewsSites.next()) {
        site->stop();
        processAndShowNews(site);
    }
}

void MythNews::processAndShowNews(NewsSite* site)
{
    if (!site)
        return;

    site->process();

    MythListButtonItem *siteUIItem = m_sitesList->GetItemCurrent();
    if (!siteUIItem || !siteUIItem->getData())
        return;

    if (site == (NewsSite*) siteUIItem->getData()) {

        m_articlesList->Reset();

        for (NewsArticle* article = site->articleList().first(); article;
             article = site->articleList().next()) {
            MythListButtonItem* item =
                new MythListButtonItem(m_articlesList, article->title());
            item->setData(article);
        }
    }
}

void MythNews::slotSiteSelected(MythListButtonItem *item)
{
    if (!item || !item->getData())
        return;

    NewsSite *site = (NewsSite*)item->getData();

    m_articlesList->Reset();

    for (NewsArticle* article = site->articleList().first(); article;
         article = site->articleList().next()) {
        MythListButtonItem* articleitem =
            new MythListButtonItem(m_articlesList, article->title());
        articleitem->setData(article);
    }

    updateInfoView(item);
}

void MythNews::slotProgressCancelled()
{
    abortHttp = true;
}

void MythNews::createProgress(QString title)
{
//     busy = new MythNewsBusyDialog(title);
//     busy->start();
//     connect(busy, SIGNAL(cancelAction()),
//             SLOT(slotProgressCancelled()));
}

bool MythNews::getHttpFile(QString sFilename, QString cmdURL)
{
    int redirectCount = 0;
    int timeoutCount = 0;
    QByteArray data(0);
    bool res = false;
    httpGrabber = NULL;
    QString hostname = "";

//     busy = NULL;
    createProgress(QObject::tr("Downloading media..."));
    while (1)
    {
        QUrl qurl(cmdURL);
        if (hostname == "")
            hostname = qurl.host();  // hold onto original host

        if (!qurl.hasHost())        // can occur on redirects to partial paths
            qurl.setHost(hostname);

        if (httpGrabber != NULL)
            delete httpGrabber;

        httpGrabber = new HttpComms;
        abortHttp = false;

        httpGrabber->request(qurl, -1, true);

        while ((!httpGrabber->isDone()) && (!abortHttp))
        {
            qApp->processEvents();
            usleep(100000);
        }

        if (abortHttp)
            break;

        // Check for redirection
        if (!httpGrabber->getRedirectedURL().isEmpty())
        {
            if (redirectCount++ < 3)
                cmdURL = httpGrabber->getRedirectedURL();

            // Try again
            timeoutCount = 0;
            continue;
        }

        data = httpGrabber->getRawData();

        if (data.size() > 0)
        {
            QFile file(sFilename);
            if (file.open( QIODevice::WriteOnly ))
            {
                QDataStream stream(& file);
                stream.writeRawBytes( (const char*) (data), data.size() );
                file.close();
                res = true;
            }
        }
        break;
    }

    delete httpGrabber;
    httpGrabber = NULL;
    return res;

}

void MythNews::slotViewArticle(MythListButtonItem *articlesListItem)
{
    if (articlesListItem && articlesListItem->getData())
    {
        NewsArticle *article = (NewsArticle*) articlesListItem->getData();
        if(article)
        {
            if (!article->enclosure().isEmpty())
            {
                QString cmdURL(article->enclosure());
                // Handle special cases for media here
                // YouTube: Fetch the mediaURL page and parse out the video URL
                if (cmdURL.contains("youtube.com"))
                {
                    cmdURL = QString(article->mediaURL());
                    QString mediaPage = HttpComms::getHttp(cmdURL);
                    if (!mediaPage.isEmpty())
                    {
                        // If this breaks in the future, we are building the URL to download
                        // a video.  At this time, this requires the video_id and the t argument
                        // from the source HTML of the page
                        int playerPos = mediaPage.find("swfArgs") + 7;

                        int tArgStart = mediaPage.find("\"t\": \"", playerPos) + 6;
                        int tArgEnd = mediaPage.find("\"", tArgStart);
                        QString tArgString = mediaPage.mid(tArgStart, tArgEnd - tArgStart);

                        int vidStart = mediaPage.find("\"video_id\": \"", playerPos) + 13;
                        int vidEnd = mediaPage.find("\"", vidStart);
                        QString vidString = mediaPage.mid(vidStart, vidEnd - vidStart);

                        cmdURL = QString("http://youtube.com/get_video.php?video_id=%2&t=%1").arg(tArgString).arg(vidString);
                        VERBOSE(VB_GENERAL, QString("MythNews: VideoURL %1").arg(cmdURL));
                    }
                }

                QString fileprefix = MythContext::GetConfDir();

                QDir dir(fileprefix);
                if (!dir.exists())
                     dir.mkdir(fileprefix);

                fileprefix += "/MythNews";

                dir = QDir(fileprefix);
                if (!dir.exists())
                    dir.mkdir(fileprefix);

                QString sFilename(fileprefix + "/newstempfile");

                if (getHttpFile(sFilename, cmdURL))
                {
                    qApp->unlock();
                    playVideo(sFilename);
                    qApp->lock();
                }
            } else {
                QString cmdUrl(article->articleURL());
                cmdUrl.replace('\'', "%27");

                QString geometry = QString(" -x %1 -y %2 -w %3 -h %4 ")
                        .arg(gContext->GetMainWindow()->x())
                        .arg(gContext->GetMainWindow()->y())
                        .arg(gContext->GetMainWindow()->width())
                        .arg(gContext->GetMainWindow()->height());

                if (gContext->GetMainWindow()->windowFlags() & Qt::WStyle_NoBorder)
                    geometry += " -g ";

                QString cmd = QString("%1 %2 %3 '%4'")
                     .arg(browser)
                     .arg(geometry)
                     .arg(zoom)
                     .arg(cmdUrl);
                gContext->GetMainWindow()->AllowInput(false);
                myth_system(cmd, MYTH_SYSTEM_DONT_BLOCK_PARENT);
                gContext->GetMainWindow()->AllowInput(true);
            }
        }
    }
}

bool MythNews::ShowEditDialog(bool edit)
{
    NewsSite *site = NULL;

    if (edit)
    {
        MythListButtonItem *siteListItem = m_sitesList->GetItemCurrent();

        if (siteListItem && siteListItem->getData())
            site = (NewsSite*) siteListItem->getData();
    }


    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    MythNewsEditor *mythnewseditor = new MythNewsEditor(site, edit, mainStack,
                                                        "mythnewseditor");

    connect(mythnewseditor, SIGNAL(exiting()), this, SLOT(loadsites()));

    if (mythnewseditor->Create())
        mainStack->AddScreen(mythnewseditor);
}

void MythNews::ShowMenu()
{
    QString label = tr("Options");

    MythScreenStack *popupStack =
                            GetMythMainWindow()->GetStack("popup stack");

    m_menuPopup = new MythDialogBox(label, popupStack, "mythnewsmenupopup");

    if (m_menuPopup->Create())
        popupStack->AddScreen(m_menuPopup);

    m_menuPopup->SetReturnEvent(this, "options");

    m_menuPopup->AddButton(tr("Edit News Site"));
    m_menuPopup->AddButton(tr("Add News Site"));
    m_menuPopup->AddButton(tr("Delete News Site"));
    m_menuPopup->AddButton(tr("Cancel"));
}

void MythNews::deleteNewsSite()
{
    MythListButtonItem *siteUIItem = m_sitesList->GetItemCurrent();

    QString siteName;
    if (siteUIItem && siteUIItem->getData())
    {
        NewsSite *site = (NewsSite*) siteUIItem->getData();
        if(site)
        {
            siteName = site->name();

            removeFromDB(siteName);
            loadSites();
        }
    }
}

void MythNews::playVideo(const QString &filename)
{
    QString command_string = gContext->GetSetting("VideoDefaultPlayer");

    gContext->sendPlaybackStart();

    if ((command_string.find("Internal", 0, false) > -1) ||
        (command_string.length() < 1))
    {
        command_string = "Internal";
        gContext->GetMainWindow()->HandleMedia(command_string, filename);
    }
    else
    {
        if (command_string.contains("%s"))
            command_string = command_string.replace(QRegExp("%s"), filename);

        myth_system(command_string);
    }

    gContext->sendPlaybackEnd();
}

void MythNews::customEvent(QEvent *event)
{

    if (event->type() == kMythDialogBoxCompletionEventType)
    {
        DialogCompletionEvent *dce =
                                dynamic_cast<DialogCompletionEvent*>(event);

        QString resultid= dce->GetId();
        int buttonnum  = dce->GetResult();

        if (resultid == "options")
        {
            if (buttonnum == 0)
            {
                ShowEditDialog(true);
            }
            else if (buttonnum == 1)
            {
                ShowEditDialog(false);
            }
            else if (buttonnum == 2)
            {
                deleteNewsSite();
            }
        }

        m_menuPopup = NULL;
    }

}
