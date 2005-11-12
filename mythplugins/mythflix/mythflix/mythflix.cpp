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

    setNoErase();
    loadTheme();

    // Load sites from database

    MSqlQuery query(MSqlQuery::InitCon());
    query.exec("SELECT name, url, updated FROM netflix WHERE is_queue=0 ORDER BY name");

    if (!query.isActive()) {
        VERBOSE(VB_IMPORTANT, QString("MythFlix: Error in loading sites from DB"));
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

    
    NewsSite* site = (NewsSite*) m_NewsSites.first();
    connect(site, SIGNAL(finished(NewsSite*)),
            this, SLOT(slotNewsRetrieved(NewsSite*)));
    
    slotRetrieveNews();

    netflixShopperId = gContext->GetSetting("NetflixShopperId");
    VERBOSE(VB_GENERAL, QString("MythFlix: Using NetflixShopperId %1").arg(netflixShopperId));
}

MythFlix::~MythFlix()
{
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
                VERBOSE(VB_IMPORTANT, QString("MythFlix: Unknown element: %1").arg(e.tagName()));
                exit(-1);
            }
        }
    }

    LayerSet *container = m_Theme->GetSet("sites");
    if (!container) {
        VERBOSE(VB_IMPORTANT, QString("MythFlix: Failed to get sites container."));
        exit(-1);
    }
        
    m_UISites = (UIListBtnType*)container->GetType("siteslist");
    if (!m_UISites) {
        VERBOSE(VB_IMPORTANT, QString("MythFlix: Failed to get list area."));
        exit(-1);
    }
        
    connect(m_UISites, SIGNAL(itemSelected(UIListBtnTypeItem*)),
            SLOT(slotSiteSelected(UIListBtnTypeItem*)));

    container = m_Theme->GetSet("articles");
    if (!container) {
        VERBOSE(VB_IMPORTANT, QString("MythFlix: Failed to get articles container."));
        exit(-1);
    }

    m_UIArticles = (UIListBtnType*)container->GetType("articleslist");
    if (!m_UIArticles) {
        VERBOSE(VB_IMPORTANT, QString("MythFlix: Failed to get articles list area."));
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
            
                VERBOSE(VB_FILE, QString("MythFlix: Boxshot File Prefix: %1").arg(fileprefix));

                QString sFilename(fileprefix + "/" + imageLoc);
            
                bool exists = QFile::exists(sFilename);
                if (!exists) 
                {
                    VERBOSE(VB_NETWORK, QString("MythFlix: Copying boxshot file from server (%1)").arg(imageLoc));
                    
                    QString sURL("http://cdn.nflximg.com/us/boxshots/large/" + imageLoc);
                
                    if (!HttpComms::getHttpFile(sFilename, sURL, 20000))
                        VERBOSE(VB_NETWORK, QString("MythFlix: Failed to download image from: %1").arg(sURL));
                
                    VERBOSE(VB_NETWORK, QString("MythFlix: Finished copying boxshot file from server (%1)").arg(imageLoc));
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
        else if(action == "SELECT")
            slotViewArticle();
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

    for (NewsSite* site = m_NewsSites.first(); site; site = m_NewsSites.next())
    {
        site->retrieve();
    }

}

void MythFlix::slotNewsRetrieved(NewsSite* site)
{
    processAndShowNews(site);
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

void MythFlix::slotSiteSelected(UIListBtnTypeItem *item)
{
    if (!item || !item->getData())
        return;
    
    processAndShowNews((NewsSite*) item->getData());
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

//            updateAddingView();

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

void MythFlix::slotMovieAdded(const QHttpResponseHeader &resp)
{
    QString location = resp.value(QString("Location"));

    if (location)
    {
        //How do you clean up http objects
        //do you listen for signal done
        //delete http;

        VERBOSE(VB_NETWORK, QString("MythFlix: Redirecting to (%1)").arg(location));

        http = new QHttp();
        connect(http, SIGNAL(responseHeaderReceived (const QHttpResponseHeader&)), this, SLOT(slotMovieAdded(const QHttpResponseHeader&)));

        QHttpRequestHeader header( "GET", location );
        header.setValue( "Cookie", "validReEntryCookie=Y; validReEntryConfirmed=Y; NetflixShopperId=" + netflixShopperId + ";" );
        header.setValue( "Host", "www.netflix.com" );
        http->setHost( "www.netflix.com" );
        http->request( header );
    }
}
