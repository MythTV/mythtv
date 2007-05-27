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

#include <iostream>
#include <math.h>
#include <unistd.h>

#include <qapplication.h>
#include <qnetwork.h>
#include <qdatetime.h>
#include <qpainter.h>
#include <qdir.h>
#include <qtimer.h>
#include <qregexp.h>

#include "mythtv/mythcontext.h"
#include "mythtv/mythdbcon.h"
#include "mythtv/httpcomms.h"

#include "mythnews.h"

MythNewsBusyDialog::MythNewsBusyDialog(const QString &title)
    : MythBusyDialog(title)
{
}

MythNewsBusyDialog::~MythNewsBusyDialog()
{
}

void MythNewsBusyDialog::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    if (gContext->GetMainWindow()->TranslateKeyPress("qt", e, actions))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            if (action == "ESCAPE")
            {
                emit cancelAction();
                handled = true;
            }
        }
    }

    if (!handled)
        MythDialog::keyPressEvent(e);
}


MythNews::MythNews(MythMainWindow *parent, const char *name )
    : MythDialog(parent, name)
{
    qInitNetworkProtocols ();

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

    m_InColumn     = 0;
    m_UISites      = 0;
    m_UIArticles   = 0;
    m_TimerTimeout = 10*60*1000; 
    httpGrabber = NULL;

    timeFormat = gContext->GetSetting("TimeFormat", "h:mm AP");
    dateFormat = gContext->GetSetting("DateFormat", "ddd MMMM d");

    setNoErase();
    loadTheme();

    updateBackground();

    // Now do the actual work
    m_RetrieveTimer = new QTimer(this);
    connect(m_RetrieveTimer, SIGNAL(timeout()),
            this, SLOT(slotRetrieveNews()));
    m_UpdateFreq = gContext->GetNumSetting("NewsUpdateFrequency", 30);

    // Load sites from database
    loadSites();

    m_RetrieveTimer->start(m_TimerTimeout, false);
}

void MythNews::loadSites(void)
{
    m_NewsSites.clear();
    m_UISites->Reset();

    MSqlQuery query(MSqlQuery::InitCon());
    query.exec("SELECT name, url, ico, updated FROM newssites ORDER BY name");

    if (!query.isActive()) {
        cerr << "MythNews: Error in loading Sites from DB" << endl;
    }
    else {
        QString name;
        QString url;
        QString icon;
        QDateTime time;
        while ( query.next() ) {
            name = QString::fromUtf8(query.value(0).toString());
            url  = QString::fromUtf8(query.value(1).toString());
            icon = QString::fromUtf8(query.value(2).toString());
            time.setTime_t(query.value(3).toUInt());
            m_NewsSites.append(new NewsSite(name,url,time));
        }
    }

    for (NewsSite *site = m_NewsSites.first(); site; site = m_NewsSites.next())
    {
        UIListBtnTypeItem* item =
            new UIListBtnTypeItem(m_UISites, site->name());
        item->setData(site);
    }

    slotRetrieveNews();

    slotSiteSelected((NewsSite*) m_NewsSites.first());
}

MythNews::~MythNews()
{
    m_RetrieveTimer->stop();
    delete m_Theme;
}

void MythNews::loadTheme()
{
    m_Theme = new XMLParse();
    m_Theme->SetWMult(wmult);
    m_Theme->SetHMult(hmult);

    QDomElement xmldata;
    m_Theme->LoadTheme(xmldata, "news", "news-");

    for (QDomNode child = xmldata.firstChild(); !child.isNull();
         child = child.nextSibling()) {
        
        QDomElement e = child.toElement();
        if (!e.isNull()) {

            if (e.tagName() == "font") {
                m_Theme->parseFont(e);
            }
            else if (e.tagName() == "container") {
                QRect area;
                QString name;
                int context;
                m_Theme->parseContainer(e, name, context, area);

                if (name.lower() == "sites")
                    m_SitesRect = area;
                else if (name.lower() == "articles")
                    m_ArticlesRect = area;
                else if (name.lower() == "info")
                    m_InfoRect = area;
                else if (name.lower() == "icons")
                    m_StatusRect = area;
            }
            else {
                std::cerr << "Unknown element: " << e.tagName()
                          << std::endl;
                exit(-1);
            }
        }
    }

    LayerSet *container = m_Theme->GetSet("sites");
    if (!container) {
        std::cerr << "MythNews: Failed to get sites container." << std::endl;
        exit(-1);
    }
        
    m_UISites = (UIListBtnType*)container->GetType("siteslist");
    if (!m_UISites) {
        std::cerr << "MythNews: Failed to get sites list area." << std::endl;
        exit(-1);
    }
        
    connect(m_UISites, SIGNAL(itemSelected(UIListBtnTypeItem*)),
            SLOT(slotSiteSelected(UIListBtnTypeItem*)));

    container = m_Theme->GetSet("articles");
    if (!container) {
        std::cerr << "MythNews: Failed to get articles container."
                  << std::endl;
        exit(-1);
    }

    m_UIArticles = (UIListBtnType*)container->GetType("articleslist");
    if (!m_UIArticles) {
        std::cerr << "MythNews: Failed to get articles list area."
                  << std::endl;
        exit(-1);
    }
    
    connect(m_UIArticles, SIGNAL(itemSelected(UIListBtnTypeItem*)),
            SLOT(slotArticleSelected(UIListBtnTypeItem*)));
    
    m_UISites->SetActive(true);
    m_UIArticles->SetActive(false);
}


void MythNews::paintEvent(QPaintEvent *e)
{
    QRect r = e->rect();

    if (r.intersects(m_SitesRect))
        updateSitesView();
    if (r.intersects(m_ArticlesRect))
        updateArticlesView();
    if (r.intersects(m_InfoRect))
        updateInfoView();
    if (r.intersects(m_StatusRect))
        updateStatusView();
}

void MythNews::updateBackground(void)
{
    QPixmap bground(size());
    bground.fill(this, 0, 0);

    QPainter tmp(&bground);

    LayerSet *container = m_Theme->GetSet("background");
    if (container)
    {
        container->Draw(&tmp, 0, 0);
    }

    tmp.end();
    m_background = bground;

    setPaletteBackgroundPixmap(m_background);
}

void MythNews::updateSitesView()
{
    QPixmap pix(m_SitesRect.size());
    pix.fill(this, m_SitesRect.topLeft());
    QPainter p(&pix);

    LayerSet* container = m_Theme->GetSet("sites");
    if (container) {
        container->Draw(&p, 0, 0);
        container->Draw(&p, 1, 0);
        container->Draw(&p, 2, 0);
        container->Draw(&p, 3, 0);
        container->Draw(&p, 4, 0);
        container->Draw(&p, 5, 0);
        container->Draw(&p, 6, 0);
        container->Draw(&p, 7, 0);
        container->Draw(&p, 8, 0);
    }
    p.end();

    bitBlt(this, m_SitesRect.left(), m_SitesRect.top(),
           &pix, 0, 0, -1, -1, Qt::CopyROP);
}

void MythNews::updateArticlesView()
{
    QPixmap pix(m_ArticlesRect.size());
    pix.fill(this, m_ArticlesRect.topLeft());
    QPainter p(&pix);

    LayerSet* container = m_Theme->GetSet("articles");
    if (container) {
        container->Draw(&p, 0, 0);
        container->Draw(&p, 1, 0);
        container->Draw(&p, 2, 0);
        container->Draw(&p, 3, 0);
        container->Draw(&p, 4, 0);
        container->Draw(&p, 5, 0);
        container->Draw(&p, 6, 0);
        container->Draw(&p, 7, 0);
        container->Draw(&p, 8, 0);
    }
    p.end();

    bitBlt(this, m_ArticlesRect.left(), m_ArticlesRect.top(),
           &pix, 0, 0, -1, -1, Qt::CopyROP);
}

void MythNews::updateStatusView()
{
    QPixmap pix(m_StatusRect.size());
    pix.fill(this, m_StatusRect.topLeft());
    QPainter p(&pix);

    LayerSet* container = m_Theme->GetSet("icons");
    if (container)
    {
        NewsSite    *site     = 0;
        NewsArticle *article  = 0;

        UIListBtnTypeItem *siteUIItem = m_UISites->GetItemCurrent();
        if (siteUIItem && siteUIItem->getData()) 
            site = (NewsSite*) siteUIItem->getData();
        
        UIListBtnTypeItem *articleUIItem = m_UIArticles->GetItemCurrent();
        if (articleUIItem && articleUIItem->getData())
            article = (NewsArticle*) articleUIItem->getData();

        UIImageType *itype = (UIImageType *)container->GetType("enclosures");
        if (itype)
        {
            // if (!itype->isHidden())
            //     itype->hide();
            if ((article) && (m_InColumn == 1))
            {
                if (article->enclosure())
                {
                    if (itype->isHidden())
                        itype->show();
                } else {
                    itype->hide();
                }
            } else {
                itype->hide(); 
            }
        }

        UIImageType *dtype = (UIImageType *)container->GetType("download");
        if (dtype)
        {
            // if (!dtype->isHidden())
            //     dtype->hide();
            if ((article) && (m_InColumn == 1))
            {
                if (article->enclosure())
                {
                    if (dtype->isHidden())
                        dtype->show();
                } else {
                    dtype->hide(); 
                }
            } else {
                dtype->hide(); 
            }
        }

        container->Draw(&p, 0, 0);
        container->Draw(&p, 1, 0);
        container->Draw(&p, 2, 0);
        container->Draw(&p, 3, 0);
        container->Draw(&p, 4, 0);
        container->Draw(&p, 5, 0);
        container->Draw(&p, 6, 0);
        container->Draw(&p, 7, 0);
        container->Draw(&p, 8, 0);
    }

    p.end();

    bitBlt(this, m_StatusRect.left(), m_StatusRect.top(),
           &pix, 0, 0, -1, -1, Qt::CopyROP);
}

void MythNews::updateInfoView()
{
    QPixmap pix(m_InfoRect.size());
    pix.fill(this, m_InfoRect.topLeft());
    QPainter p(&pix);

    LayerSet* container = m_Theme->GetSet("info");
    if (container)
    {
        NewsSite    *site     = 0;
        NewsArticle *article  = 0;

        UIListBtnTypeItem *siteUIItem = m_UISites->GetItemCurrent();
        if (siteUIItem && siteUIItem->getData()) 
            site = (NewsSite*) siteUIItem->getData();
        
        UIListBtnTypeItem *articleUIItem = m_UIArticles->GetItemCurrent();
        if (articleUIItem && articleUIItem->getData())
            article = (NewsArticle*) articleUIItem->getData();
        
        if (m_InColumn == 1) {

            if (article)
            {
                UITextType *ttype =
                    (UITextType *)container->GetType("title");
                if (ttype)
                    ttype->SetText(article->title());

                ttype =
                    (UITextType *)container->GetType("description");
                if (ttype)
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
                    ttype->SetText(artText);
                }

                if (article->thumbnail())
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
                    // VERBOSE(VB_GENERAL, QString("MythNews: Thumbnail url %1").arg(url));
                    QString sFilename = QString("%1/%2").arg(fileprefix).arg(qChecksum(url,url.length()));
                    // VERBOSE(VB_GENERAL, QString("MythNews: Thumbnail filename %1").arg(sFilename));

                    bool exists = QFile::exists(sFilename);
                    if (!exists)
                        HttpComms::getHttpFile(sFilename, url, 20000);

                    UIImageType *itype = (UIImageType *)container->GetType("thumbnail");
                    if (itype)
                    {
                       itype->SetImage(sFilename);
                       itype->LoadImage();

                       if (itype->isHidden())
                           itype->show();
                    }
                } else {
                    UIImageType *itype = (UIImageType *)container->GetType("thumbnail");
                    if (itype)
                    {
                        itype->hide();
                    }
                }
            }
        }
        else {
            UIImageType *itype = (UIImageType *)container->GetType("thumbnail");
            if (itype)
            {
                itype->hide();
            }

            if (site)
            {
                UITextType *ttype =
                    (UITextType *)container->GetType("title");
                if (ttype)
                    ttype->SetText(site->name());

                ttype =
                    (UITextType *)container->GetType("description");
                if (ttype)
                    ttype->SetText(site->description());

                if (site->imageURL())
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

                    UIImageType *itype = (UIImageType *)container->GetType("thumbnail");
                    if (itype)
                    {
                       itype->SetImage(sFilename);
                       itype->LoadImage();

                       if (itype->isHidden())
                           itype->show();
                    }
                }

            }
        }

        UITextType *ttype =
            (UITextType *)container->GetType("updated");
        if (ttype) {

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
                ttype->SetText(text);
            }

            if (httpGrabber != NULL)
            {
                int progress = httpGrabber->getProgress();
                int total = httpGrabber->getTotal();
                if ((progress > 0) && (total > 0) && (progress < total))
                {
                    float fProgress = (float)progress/total;
                    QString text = QString("%1 of %2 (%3 percent)").arg(progress).arg(total).arg(floor(fProgress*100));
                    ttype->SetText(text);
                }
            }
        }

        container->Draw(&p, 0, 0);
        container->Draw(&p, 1, 0);
        container->Draw(&p, 2, 0);
        container->Draw(&p, 3, 0);
        container->Draw(&p, 4, 0);
        container->Draw(&p, 5, 0);
        container->Draw(&p, 6, 0);
        container->Draw(&p, 7, 0);
        container->Draw(&p, 8, 0);
    }

    p.end();


    bitBlt(this, m_InfoRect.left(), m_InfoRect.top(),
           &pix, 0, 0, -1, -1, Qt::CopyROP);
}

void MythNews::keyPressEvent(QKeyEvent *e)
{
    if (!e) return;

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("News", e, actions);
   
    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "UP")
            cursorUp();
        else if (action == "PAGEUP")
             cursorUp(true);
        else if (action == "DOWN")
            cursorDown();
        else if (action == "PAGEDOWN")
             cursorDown(true);
        else if (action == "LEFT")
            cursorLeft();
        else if (action == "RIGHT")
            cursorRight();
        else if (action == "RETRIEVENEWS")
            slotRetrieveNews();
        else if(action == "SELECT")
            slotViewArticle();
        else if (action == "CANCEL")
            cancelRetrieve();
        else if (action == "MENU")
            showMenu();
        else
            handled = false;
    }

    if (!handled)
        MythDialog::keyPressEvent(e);
}

void MythNews::cursorUp(bool page)
{
    UIListBtnType::MovementUnit unit = page ? UIListBtnType::MovePage : UIListBtnType::MoveItem;

    if (m_InColumn == 0) {
        m_UISites->MoveUp(unit);
    }
    else {
        m_UIArticles->MoveUp(unit);
    }
}

void MythNews::cursorDown(bool page)
{
    UIListBtnType::MovementUnit unit = page ? UIListBtnType::MovePage : UIListBtnType::MoveItem;

    if (m_InColumn == 0) {
        m_UISites->MoveDown(unit);
    }
    else {
        m_UIArticles->MoveDown(unit);
    }
}

void MythNews::cursorRight()
{
    if (m_InColumn == 1)
    {
        slotViewArticle();
        return;
    }

    m_InColumn++;

    m_UISites->SetActive(false);
    m_UIArticles->SetActive(true);

    update(m_SitesRect);
    update(m_ArticlesRect);
    update(m_InfoRect);
    update(m_StatusRect);
}

void MythNews::cursorLeft()
{
    if (m_InColumn == 0)
    {
        accept();
        return;
    }

    m_InColumn--;

    m_UISites->SetActive(true);
    m_UIArticles->SetActive(false);

    update(m_SitesRect);
    update(m_ArticlesRect);
    update(m_InfoRect);
    update(m_StatusRect);
}

void MythNews::slotRetrieveNews()
{
    if (m_NewsSites.count() == 0)
        return;

    cancelRetrieve();

    m_RetrieveTimer->stop();

    for (NewsSite* site = m_NewsSites.first(); site; site = m_NewsSites.next())
    {
        site->stop();
        connect(site, SIGNAL(finished(NewsSite*)),
                this, SLOT(slotNewsRetrieved(NewsSite*)));
    }

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
    query.bindValue(":NAME", site->name().utf8());
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

    UIListBtnTypeItem *siteUIItem = m_UISites->GetItemCurrent();
    if (!siteUIItem || !siteUIItem->getData())
        return;
    
    if (site == (NewsSite*) siteUIItem->getData()) {

        m_UIArticles->Reset();

        for (NewsArticle* article = site->articleList().first(); article;
             article = site->articleList().next()) {
            UIListBtnTypeItem* item =
                new UIListBtnTypeItem(m_UIArticles, article->title());
            item->setData(article);
        }

        update(m_ArticlesRect);
        update(m_InfoRect);
    } 
}
void MythNews::slotSiteSelected(NewsSite* site)
{
    if(!site)
        return;
        
    m_UIArticles->Reset();

    for (NewsArticle* article = site->articleList().first(); article;
         article = site->articleList().next()) {
        UIListBtnTypeItem* item =
            new UIListBtnTypeItem(m_UIArticles, article->title());
        item->setData(article);
    }

    update(m_SitesRect);
    update(m_ArticlesRect);
    update(m_InfoRect);
}

void MythNews::slotSiteSelected(UIListBtnTypeItem *item)
{
    if (!item || !item->getData())
        return;
    
    slotSiteSelected((NewsSite*) item->getData());
}

void MythNews::slotArticleSelected(UIListBtnTypeItem*)
{
    update(m_ArticlesRect);
    update(m_InfoRect);
    update(m_StatusRect);
}

void MythNews::slotProgressCancelled()
{
    abortHttp = true;
}

void MythNews::createProgress(QString title)
{
    busy = new MythNewsBusyDialog(title);
    busy->start();
    connect(busy, SIGNAL(cancelAction()),
            SLOT(slotProgressCancelled()));
}

bool MythNews::getHttpFile(QString sFilename, QString cmdURL)
{
    int redirectCount = 0;
    int timeoutCount = 0;
    QByteArray data(0);
    bool res = false;
    httpGrabber = NULL;
    QString hostname = "";

                busy = NULL;
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
                        update(m_InfoRect);
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
                        if (file.open( IO_WriteOnly ))
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
                delete busy;
                return res;

}

void MythNews::slotViewArticle()
{
    UIListBtnTypeItem *articleUIItem = m_UIArticles->GetItemCurrent();

    if (articleUIItem && articleUIItem->getData())
    {
        NewsArticle *article = (NewsArticle*) articleUIItem->getData();
        if(article)
        {
            if (article->enclosure())
            {
                QString cmdURL(article->enclosure());
                // Handle special cases for media here
                // YouTube: Fetch the mediaURL page and parse out the video URL
                if (cmdURL.contains("youtube.com"))
                {
                    cmdURL = QString(article->mediaURL());
                    QString mediaPage = HttpComms::getHttp(cmdURL);
                    if (mediaPage)
                    {
                        int playerPos = mediaPage.find("player2.swf?") + 12;
                        int playerEndPos = mediaPage.find("\"", playerPos);
                        QString videoURL = mediaPage.mid(playerPos, playerEndPos - playerPos);
                        // VERBOSE(VB_GENERAL, QString("MythNews: VideoURL %1").arg(videoURL));
                        cmdURL = QString("http://youtube.com/get_video.php?%1").arg(videoURL);
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
                    QString handler = "Internal";
                    gContext->GetMainWindow()->HandleMedia(handler, sFilename);
                    qApp->lock();
                    // myth_system(QString("mplayer -fs %1").arg(sFilename));
                }
            } else {
                QString cmdUrl(article->articleURL());
                cmdUrl.replace('\'', "%27");

                QString geometry = QString(" -x %1 -y %2 -w %3 -h %4 ")
                        .arg(gContext->GetMainWindow()->x())
                        .arg(gContext->GetMainWindow()->y())
                        .arg(gContext->GetMainWindow()->width())
                        .arg(gContext->GetMainWindow()->height());

                if (!gContext->GetMainWindow()->testWFlags(Qt::WStyle_NoBorder))
                    geometry += " -g ";

                QString cmd = QString("%1 %2 %3 '%4'")
                     .arg(browser)
                     .arg(geometry)
                     .arg(zoom)
                     .arg(cmdUrl);
                myth_system(cmd);
            }
        }
    }
}

bool MythNews::showEditDialog(bool edit)
{
    MythPopupBox *popup = new MythPopupBox(GetMythMainWindow(), "edit news site");

    QVBoxLayout *vbox = new QVBoxLayout(NULL, 0, (int)(10 * hmult));
    QHBoxLayout *hbox = new QHBoxLayout(vbox, (int)(10 * hmult));

    QString title;
    if (edit)
        title = tr("Edit Site Details");
    else
        title = tr("Enter Site Details");

    QLabel *label = new QLabel(title, popup);
    QFont font = label->font();
    font.setPointSize(int (font.pointSize() * 1.2));
    font.setBold(true);
    label->setFont(font);
    label->setPaletteForegroundColor(QColor("yellow"));
    label->setAlignment(Qt::AlignCenter);
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred)));
    label->setMinimumWidth((int)(500 * wmult));
    label->setMaximumWidth((int)(500 * wmult));
    hbox->addWidget(label);

    hbox = new QHBoxLayout(vbox, (int)(10 * hmult));
    label = new QLabel(tr("Name:"), popup, "nopopsize");
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    label->setMinimumWidth((int)(110 * wmult));
    label->setMaximumWidth((int)(110 * wmult));
    hbox->addWidget(label);

    MythRemoteLineEdit *titleEditor = new MythRemoteLineEdit(popup);
    titleEditor->setFocus(); 
    hbox->addWidget(titleEditor);

    hbox = new QHBoxLayout(vbox, (int)(10 * hmult));
    label = new QLabel(tr("URL:"), popup, "nopopsize");
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    label->setMinimumWidth((int)(110 * wmult));
    label->setMaximumWidth((int)(110 * wmult));
    hbox->addWidget(label);

    MythRemoteLineEdit *urlEditor = new MythRemoteLineEdit(popup);
    hbox->addWidget(urlEditor);

    hbox = new QHBoxLayout(vbox, (int)(10 * hmult));
    label = new QLabel(tr("Icon:"), popup, "nopopsize");
    label->setBackgroundOrigin(WindowOrigin);
    label->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    label->setMinimumWidth((int)(110 * wmult));
    label->setMaximumWidth((int)(110 * wmult));
    hbox->addWidget(label);

    MythRemoteLineEdit *iconEditor = new MythRemoteLineEdit(popup);
    hbox->addWidget(iconEditor);

    popup->addLayout(vbox, 0);

    popup->addButton(tr("OK"));
    popup->addButton(tr("Cancel"));

    QString siteName = "";
    if (edit)
    {
        UIListBtnTypeItem *siteUIItem = m_UISites->GetItemCurrent();

        if (siteUIItem && siteUIItem->getData())
        {
            NewsSite *site = (NewsSite*) siteUIItem->getData();
            if(site)
            {
                siteName = site->name();
                titleEditor->setText(site->name());
                urlEditor->setText(site->url());
            }
        }
    }

    int res = popup->ExecPopup();

    if (res == 0)
    {
        if (edit && siteName != "")
            removeFromDB(siteName);
        insertInDB(titleEditor->text(), urlEditor->text(), iconEditor->text(), "custom");
        loadSites();
    }

    delete popup;

    return (res == 0);
}

void MythNews::showMenu()
{
    menu = new MythPopupBox(GetMythMainWindow(),"popupMenu");

    QButton *temp = menu->addButton(tr("Edit News Site"), this, SLOT(editNewsSite()));
    menu->addButton(tr("Add News Site"), this, SLOT(addNewsSite()));
    menu->addButton(tr("Delete News Site"), this, SLOT(deleteNewsSite()));
    menu->addButton(tr("Cancel"), this, SLOT(cancelMenu()));
    temp->setFocus();

    menu->ShowPopup(this, SLOT(cancelMenu()));
}

void MythNews::cancelMenu()
{
    if (menu) 
    {
        menu->hide();
        delete menu;
        menu=NULL;
    }
}

void MythNews::editNewsSite()
{
    cancelMenu();
    showEditDialog(true);
}

void MythNews::addNewsSite()
{
    cancelMenu();
    showEditDialog(false);
}

void MythNews::deleteNewsSite()
{
    cancelMenu();

    UIListBtnTypeItem *siteUIItem = m_UISites->GetItemCurrent();

    QString siteName;
    if (siteUIItem && siteUIItem->getData())
    {
        NewsSite *site = (NewsSite*) siteUIItem->getData();
        if(site)
        {
            siteName = site->name();

            if (MythPopupBox::showOkCancelPopup(gContext->GetMainWindow(), QObject::tr("MythNews"),
                                      QObject::tr("Are you sure you want to delete this news site\n\n%1")
                                              .arg(siteName), true))
            {
                removeFromDB(siteName);
                loadSites();
            }
        }
    }
}

bool MythNews::findInDB(const QString& name)
{
    bool val = false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT name FROM newssites WHERE name = :NAME ;");
    query.bindValue(":NAME", name.utf8());
    if (!query.exec() || !query.isActive()) {
        MythContext::DBError("new find in db", query);
        return val;
    }

    val = query.numRowsAffected() > 0;

    return val;
}

bool MythNews::insertInDB(const QString &name, const QString &url, 
                          const QString &icon, const QString &category)
{
    if (findInDB(name))
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("INSERT INTO newssites (name,category,url,ico) "
            " VALUES( :NAME, :CATEGORY, :URL, :ICON );");
    query.bindValue(":NAME", name.utf8());
    query.bindValue(":CATEGORY", category.utf8());
    query.bindValue(":URL", url.utf8());
    query.bindValue(":ICON", icon.utf8());
    if (!query.exec() || !query.isActive()) {
        MythContext::DBError("news: inserting in DB", query);
        return false;
    }

    return (query.numRowsAffected() > 0);
}

bool MythNews::removeFromDB(const QString &name)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM newssites WHERE name = :NAME ;");
    query.bindValue(":NAME", name.utf8());
    if (!query.exec() || !query.isActive()) {
        MythContext::DBError("news: delete from db", query);
        return false;
    }

    return (query.numRowsAffected() > 0);
}
