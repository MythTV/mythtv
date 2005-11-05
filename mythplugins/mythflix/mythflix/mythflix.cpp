/* ============================================================
 * File  : mythflix.cpp
 * Author: John Petrocik <john@petrocik.net>
 * Date  : 2005-10-28
 * Description :
 *
 * Copyright 2005 by John Petrocik

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

#include <qnetwork.h>
#include <qdatetime.h>
#include <qpainter.h>
#include <qdir.h>
#include <qtimer.h>
#include <qregexp.h>

#include <qurl.h>
#include "mythtv/mythcontext.h"
#include "mythtv/mythdbcon.h"
#include "mythtv/httpcomms.h"

#include "mythflix.h"

MythFlix::MythFlix(MythMainWindow *parent, const char *name )
    : MythDialog(parent, name)
{
    qInitNetworkProtocols ();

    // Setup cache directory

    QString fileprefix = MythContext::GetConfDir();

    QDir dir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    fileprefix += "/MythFlix";
    dir = QDir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);
    
    // Initialize variables

    m_InColumn     = 0;
    m_UISites      = 0;
    m_UIArticles   = 0;
    m_TimerTimeout = 10*60*1000; 

    setNoErase();
    loadTheme();

    // Load sites from database

    MSqlQuery query(MSqlQuery::InitCon());
    query.exec("SELECT name, url, updated FROM netflix WHERE is_queue=0 ORDER BY name");

    if (!query.isActive()) {
        cerr << "MythFlix: Error in loading Sites from DB" << endl;
    }
    else {
        QString name;
        QString url;
        QDateTime time;
        while ( query.next() ) {
            name = QString::fromUtf8(query.value(0).toString());
            url  = QString::fromUtf8(query.value(1).toString());
            time.setTime_t(query.value(2).toUInt());
            m_NewsSites.append(new NewsSite(name,url,time));
        }
    }

    for (NewsSite *site = m_NewsSites.first(); site; site = m_NewsSites.next())
    {
        UIListBtnTypeItem* item =
            new UIListBtnTypeItem(m_UISites, site->name());
        item->setData(site);
    }
    
    // Now do the actual work
    m_RetrieveTimer = new QTimer(this);
    connect(m_RetrieveTimer, SIGNAL(timeout()),
            this, SLOT(slotRetrieveNews()));
    m_UpdateFreq = gContext->GetNumSetting("NewsUpdateFrequency", 30);
    m_RetrieveTimer->start(m_TimerTimeout, false);

    slotRetrieveNews();

    slotSiteSelected((NewsSite*) m_NewsSites.first());

    netflixShopperId = gContext->GetSetting("NetflixShopperId");
    std::cerr << "MythFlix: Using NetflixShopperId " << netflixShopperId << std::endl;


}

MythFlix::~MythFlix()
{
    m_RetrieveTimer->stop();
    delete m_Theme;
}

void MythFlix::loadTheme()
{
    m_Theme = new XMLParse();
    m_Theme->SetWMult(wmult);
    m_Theme->SetHMult(hmult);

    QDomElement xmldata;
    m_Theme->LoadTheme(xmldata, "browse", "netflix-");

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
        std::cerr << "MythFlix: Failed to get sites container." << std::endl;
        exit(-1);
    }
        
    m_UISites = (UIListBtnType*)container->GetType("siteslist");
    if (!m_UISites) {
        std::cerr << "MythFlix: Failed to get sites list area." << std::endl;
        exit(-1);
    }
        
    connect(m_UISites, SIGNAL(itemSelected(UIListBtnTypeItem*)),
            SLOT(slotSiteSelected(UIListBtnTypeItem*)));

    container = m_Theme->GetSet("articles");
    if (!container) {
        std::cerr << "MythFlix: Failed to get articles container."
                  << std::endl;
        exit(-1);
    }

    m_UIArticles = (UIListBtnType*)container->GetType("articleslist");
    if (!m_UIArticles) {
        std::cerr << "MythFlix: Failed to get articles list area."
                  << std::endl;
        exit(-1);
    }
    
    connect(m_UIArticles, SIGNAL(itemSelected(UIListBtnTypeItem*)),
            SLOT(slotArticleSelected(UIListBtnTypeItem*)));
    
    m_UISites->SetActive(true);
    m_UIArticles->SetActive(false);
}


void MythFlix::paintEvent(QPaintEvent *e)
{
    QRect r = e->rect();

    if (r.intersects(m_SitesRect))
        updateSitesView();
    if (r.intersects(m_ArticlesRect))
        updateArticlesView();
    if (r.intersects(m_InfoRect))
        updateInfoView();
}


void MythFlix::updateSitesView()
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

void MythFlix::updateArticlesView()
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

void MythFlix::updateInfoView()
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
                    (UITextType *)container->GetType("status");
//                if (ttype)
//                    ttype->SetText("");

                ttype =
                    (UITextType *)container->GetType("title");
                if (ttype)
                    ttype->SetText(article->title());

                ttype =
                    (UITextType *)container->GetType("description");
                if (ttype)
                    ttype->SetText(article->description());

                QString imageLoc = article->articleURL();
                int index = imageLoc.find("movieid=");
                imageLoc = imageLoc.mid(index+8,8) + ".jpg";

                QString fileprefix = MythContext::GetConfDir();
                
                QDir dir(fileprefix);
                if (!dir.exists())
                    dir.mkdir(fileprefix);
            
                fileprefix += "/MythFlix";
            
                dir = QDir(fileprefix);
                if (!dir.exists())
                    dir.mkdir(fileprefix);
            
                if (debug)
                    cerr << "MythFlix: Boxshot File Prefix: " << fileprefix << endl;

                QString sFilename(fileprefix + "/" + imageLoc);
            
                
                bool exists = QFile::exists(sFilename);
                if (!exists) 
                {
                    if (debug)
                        cerr << "MythFlix: Copying BoxShot File from NetFlix (" << imageLoc << ")..." << endl;
    
                    VERBOSE(VB_NETWORK, QString("MythFlix: Copying boxshot file from server (%1)").arg(imageLoc));
                    
                    QString sURL("http://cdn.nflximg.com/us/boxshots/large/" + imageLoc);
                
                    if (!HttpComms::getHttpFile(sFilename, sURL, 20000))
                        cerr << "Failed to download image from:" << sURL << endl;
                
                    if (debug)
                        cerr << "Done.\n";
                }

                UIImageType *itype = (UIImageType *)container->GetType("boxshot");
                if (itype)
                {
                   itype->SetImage(sFilename);
                   itype->LoadImage();
        
                   if (itype->isHidden())
                       itype->show();   
                }

            }
        }
        else {

            if (site)
            {
                UITextType *ttype =
                    (UITextType *)container->GetType("status");
//                if (ttype)
//                    ttype->SetText("");

                ttype =
                    (UITextType *)container->GetType("title");
                if (ttype)
                    ttype->SetText(site->name());

                ttype =
                    (UITextType *)container->GetType("description");
                if (ttype)
                    ttype->SetText(site->description());

                UIImageType *itype = (UIImageType *)container->GetType("boxshot");
                if (itype)
                {
                    itype->hide();   
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

void MythFlix::keyPressEvent(QKeyEvent *e)
{
    if (!e) return;

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("NetFlix", e, actions);
   
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
        {
            cancelRetrieve();
        }
        else
            handled = false;
    }

    if (!handled)
        MythDialog::keyPressEvent(e);
}

void MythFlix::cursorUp(bool page)
{
    UIListBtnType::MovementUnit unit = page ? UIListBtnType::MovePage : UIListBtnType::MoveItem;

    if (m_InColumn == 0) {
        m_UISites->MoveUp(unit);
    }
    else {
        m_UIArticles->MoveUp(unit);
    }
}

void MythFlix::cursorDown(bool page)
{
    UIListBtnType::MovementUnit unit = page ? UIListBtnType::MovePage : UIListBtnType::MoveItem;

    if (m_InColumn == 0) {
        m_UISites->MoveDown(unit);
    }
    else {
        m_UIArticles->MoveDown(unit);
    }
}

void MythFlix::cursorRight()
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
}

void MythFlix::cursorLeft()
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
}

void MythFlix::slotRetrieveNews()
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

void MythFlix::slotNewsRetrieved(NewsSite* site)
{
    unsigned int updated = site->lastUpdated().toTime_t();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE netflix SET updated = :UPDATED "
                  "WHERE name = :NAME ;");
    query.bindValue(":UPDATED", updated);
    query.bindValue(":NAME", site->name().utf8());
    if (!query.exec() || !query.isActive())
        MythContext::DBError("news update time", query);

    processAndShowNews(site);
}

void MythFlix::cancelRetrieve()
{
    for (NewsSite* site = m_NewsSites.first(); site;
         site = m_NewsSites.next()) {
        site->stop();
        processAndShowNews(site);
    }
}

void MythFlix::processAndShowNews(NewsSite* site)
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
void MythFlix::slotSiteSelected(NewsSite* site)
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

void MythFlix::slotSiteSelected(UIListBtnTypeItem *item)
{
    if (!item || !item->getData())
        return;
    
    slotSiteSelected((NewsSite*) item->getData());
}

void MythFlix::slotArticleSelected(UIListBtnTypeItem*)
{
    update(m_ArticlesRect);
    update(m_InfoRect);
}

void MythFlix::slotViewArticle()
{
    UIListBtnTypeItem *articleUIItem = m_UIArticles->GetItemCurrent();

    if (articleUIItem && articleUIItem->getData())
    {
        NewsArticle *article = (NewsArticle*) articleUIItem->getData();
        if(article)
        {
            QString cmdUrl(article->articleURL());
            cmdUrl.replace('\'', "%27");

            QUrl url(cmdUrl);

            QString query = url.query();

            QString get("/AddToQueue?");
            get.append(query);

            http = new QHttp();
            connect(http, SIGNAL(responseHeaderReceived (const QHttpResponseHeader&)), this, SLOT(slotMovieAdded(const QHttpResponseHeader&)));
            //connect(http, SIGNAL(done (const QHttpResponseHeader&)), this, SLOT(slotMovieAdded(const QHttpResponseHeader&)));
            
            QHttpRequestHeader header( "GET", get );
            header.setValue( "Cookie", "validReEntryCookie=Y; validReEntryConfirmed=Y; NetflixShopperId=" + netflixShopperId + ";" );
            header.setValue( "Host", "www.netflix.com" );

            http->setHost( "www.netflix.com" );
            http->request( header );
        }
    } 
}

void MythFlix::slotMovieAdded(const QHttpResponseHeader &resp){
    QString location = resp.value(QString("Location"));

    if (location)
    {

      //How do you clean up http objects
      //do you listen for signal done
      //delete http;

     if (debug)
        cerr << "MythFlix: Redirecting to (" << location << ")..." << endl;

      http = new QHttp();
      connect(http, SIGNAL(responseHeaderReceived (const QHttpResponseHeader&)), this, SLOT(slotMovieAdded(const QHttpResponseHeader&)));

      QHttpRequestHeader header( "GET", location );
      header.setValue( "Cookie", "validReEntryCookie=Y; validReEntryConfirmed=Y; NetflixShopperId=" + netflixShopperId + ";" );
      header.setValue( "Host", "www.netflix.com" );
      http->setHost( "www.netflix.com" );
      http->request( header );
    }
    else 
    {

      //How do you clean up http objects
      //do you listen for signal done
      //delete http;

/*
     if (debug)
        cerr << "MythFlix: Movie Added" << endl;

      LayerSet* container = m_Theme->GetSet("info");
      UITextType* ttype =
        (UITextType *)container->GetType("status");
        
      if (ttype)
        ttype->SetText("Added");

*/        

    }
}
