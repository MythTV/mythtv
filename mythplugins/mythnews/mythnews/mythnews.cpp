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

#include <qnetwork.h>
#include <qsqlquery.h>
#include <qdatetime.h>
#include <qpainter.h>
#include <qdir.h>
#include <qtimer.h>

#include "mythnews.h"

using namespace std;

MythNews::MythNews(QSqlDatabase *db, MythMainWindow *parent,
                   const char *name )
    : MythDialog(parent, name)
{
    m_db = db;

    qInitNetworkProtocols ();

    // Setup cache directory

    char *home = getenv("HOME");
    QString fileprefix = QString(home) + "/.mythtv";

    QDir dir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);
    fileprefix += "/MythNews";
    dir = QDir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    // Initialize variables

    m_RetrievingNews = false;
    m_InColumn = 0;
    m_CurSite    = 0;
    m_CurArticle = 0;
    m_ItemsPerListing = 0;

    if (m_ItemsPerListing < 1)
        m_ItemsPerListing = 7;
    if (m_ItemsPerListing % 2 == 0)
        m_ItemsPerListing++;

    m_TimerTimeout = 10*60*1000; // timeout every 10 minutes

    // Load sites from database

    QSqlQuery query("SELECT name, url, updated FROM newssites", db);
    if (!query.isActive()) {
        cerr << "MythNews: Error in loading Sites from DB" << endl;
    }
    else {
        QString name;
        QString url;
        QDateTime time;
        while ( query.next() ) {
            name = query.value(0).toString();
            url  = query.value(1).toString();
            time.setTime_t(query.value(2).toUInt());
            m_NewsSites.append(new NewsSite(name,url,time));
        }
    }

    // Load theme and ui file

    m_Theme = new XMLParse();
    m_Theme->SetWMult(wmult);
    m_Theme->SetHMult(hmult);
    QDomElement xmldata;
    m_Theme->LoadTheme(xmldata, "news", "news-");
    LoadWindow(xmldata);

    LayerSet *container = m_Theme->GetSet("selector");
    if (container)
    {
        UIListType *ltype =
            (UIListType *)container->GetType("sites");
        if (ltype)
        {
            m_ItemsPerListing = ltype->GetItems();
        }
    }
    else
    {
        cerr << "MythNews: Failed to get selector object.\n";
        exit(0);
    }

    setNoErase();
    updateBackground();

    // Now do the actual work

    m_UpdateFreq = gContext->GetNumSetting("NewsUpdateFrequency", 30);

    m_RetrieveTimer = new QTimer(this);
    connect(m_RetrieveTimer, SIGNAL(timeout()),
            this, SLOT(slotRetrieveNews()));

    slotRetrieveNews();

    m_RetrieveTimer->start(m_TimerTimeout, false);
}

MythNews::~MythNews()
{
    m_RetrieveTimer->stop();
}

void MythNews::LoadWindow(QDomElement &element)
{

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement e = child.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "font")
            {
                m_Theme->parseFont(e);
            }
            else if (e.tagName() == "container")
            {
                QRect area;
                QString name;
                int context;
                m_Theme->parseContainer(e, name, context, area);

                if (name.lower() == "news_info")
                    m_TopRect = area;
                if (name.lower() == "selector")
                    m_MidRect = area;
                if (name.lower() == "messages")
                    m_BotRect = area;
            }
            else
            {
                cerr << "Unknown element: " << e.tagName()
                     << endl;
                exit(0);
            }
        }
    }
}

void MythNews::updateBackground()
{
    QPixmap pix(size());
    pix.fill(this, 0, 0);

    QPainter p(&pix);
     LayerSet *container = m_Theme->GetSet("background");
    if (container)
    {
        container->Draw(&p, 0, 0);
    }
    p.end();

    setPaletteBackgroundPixmap(pix);
}

void MythNews::paintEvent(QPaintEvent *e)
{
    QRect r = e->rect();

    if (r.intersects(m_TopRect))
        updateTopView();
    if (r.intersects(m_MidRect))
        updateMidView();
    if (r.intersects(m_BotRect))
            updateBotView();
}


void MythNews::updateTopView()
{
    QPixmap pix(m_TopRect.size());
    pix.fill(this, m_TopRect.topLeft());
    QPainter p(&pix);

    LayerSet* container = m_Theme->GetSet("news_info");
    if (container)
    {

        NewsSite    *site     = m_NewsSites.at(m_CurSite);
        NewsArticle *article  = 0;
        if (site)
            article = site->articleList().at(m_CurArticle);

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
                    ttype->SetText(article->description());
             }
        }
        else {

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
             }
        }

        UITextType *ttype =
            (UITextType *)container->GetType("updated");
        if (ttype) {

            if (site)
            {
                QString text("Updated\n");
                QDateTime updated(site->lastUpdated());
                if (updated.toTime_t() != 0) {
                    text += site->lastUpdated().toString("ddd MMM d") + "\n";
                    text += site->lastUpdated().toString("hh:mm AP");
                }
                else
                    text += "Unknown";
                ttype->SetText(text);
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


    bitBlt(this, m_TopRect.left(), m_TopRect.top(),
           &pix, 0, 0, -1, -1, Qt::CopyROP);
}

void MythNews::updateMidView()
{
    QPixmap pix(m_MidRect.size());
    pix.fill(this, m_MidRect.topLeft());
    QPainter p(&pix);

    LayerSet* container = m_Theme->GetSet("selector");

    if (container)
    {
        UIListType *ltype =
            (UIListType *)container->GetType("sites");
        if (ltype)
        {
            if (m_InColumn == 0)
            {
                ltype->SetItemCurrent((int)(ltype->GetItems() / 2));
                ltype->SetActive(true);
            }
            else
            {
                ltype->SetItemCurrent(-1);
                ltype->SetActive(false);
            }
        }

        ltype = (UIListType *)container->GetType("info");
        if (ltype)
        {
            if (m_InColumn == 1)
            {
                ltype->SetItemCurrent((int)(ltype->GetItems() / 2));
                ltype->SetActive(true);
            }
            else
            {
                ltype->SetItemCurrent(-1);
                ltype->SetActive(false);
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


    bitBlt(this, m_MidRect.left(), m_MidRect.top(),
           &pix, 0, 0, -1, -1, Qt::CopyROP);
}

void MythNews::updateBotView()
{
    QPixmap pix(m_BotRect.size());
    pix.fill(this, m_BotRect.topLeft());
    QPainter p(&pix);

    LayerSet *container = m_Theme->GetSet("messages");
    if (container)
    {

        UITextType *ttype =
            (UITextType *)container->GetType("loading");
        if (ttype) {
            if (m_RetrievingNews)
                ttype->SetText(QString("Updating..."));
            else
                ttype->SetText(QString("Ready"));
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

    bitBlt(this, m_BotRect.left(), m_BotRect.top(),
           &pix, 0, 0, -1, -1, Qt::CopyROP);
}


void MythNews::showSitesList()
{
    int curLabel = 0;
    int t = 0;
    int count = 0;

    LayerSet *container = m_Theme->GetSet("selector");
    if (container) {

        UIListType *ltype =
            (UIListType *)container->GetType("sites");
        if (ltype)
        {
            ltype->ResetList();

            count = QMAX(m_NewsSites.count(), m_ItemsPerListing);

            for (int i = (m_CurSite - ((m_ItemsPerListing - 1) / 2));
                 i < int(m_CurSite + ((m_ItemsPerListing + 1) / 2)); i++)
            {
                t = i;

                 if (i < 0)
                     t = i + count;
                 if (i >= count)
                     t = i - count;
                 if (t < 0) {
                     cerr << "MythNews: -1 Error in showSitesList()\n";
                     exit(-1);
                 }

                NewsSite* item = m_NewsSites.at(t);
                if (item)
                    ltype->SetItemText(curLabel, " " + item->name() + " ");
                else
                    ltype->SetItemText(curLabel, "");
                curLabel++;
            }

        }

    }

}

void MythNews::showArticlesList()
{
    int curLabel = 0;
    int t = 0;
    int count = 0;

    LayerSet *container = m_Theme->GetSet("selector");
    if (container) {

        UIListType *ltype =
            (UIListType *)container->GetType("info");
        if (ltype)
        {
            ltype->ResetList();

            NewsSite *site = m_NewsSites.at(m_CurSite);
            if (!site) return;

            count = QMAX(site->articleList().count(), m_ItemsPerListing);

            for (int i = (m_CurArticle - ((m_ItemsPerListing - 1) / 2));
                 i < int(m_CurArticle + ((m_ItemsPerListing + 1) / 2)); i++)
            {
                t = i;

                if (i < 0)
                    t = i + count;
                if (i >= count)
                    t = i - count;
                if (t < 0) {
                    cerr << "MythNews: -1 Error in showArticlesList()\n";
                    exit(-1);
                }

                NewsArticle* item = site->articleList().at(t);
                if (item)
                    ltype->SetItemText(curLabel, " " + item->title() + " ");
                else
                    ltype->SetItemText(curLabel, "");
                curLabel++;
            }

        }
    }
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
        else if (action == "DOWN")
            cursorDown();
        else if (action == "LEFT")
            cursorLeft();
        else if (action == "RIGHT")
            cursorRight();
        else if (action == "RETRIEVENEWS")
            slotRetrieveNews();
        else if (action == "FORCERETRIEVE")
            forceRetrieveNews();
        else if (action == "CANCEL")
        {
            if (m_RetrievingNews)
                cancelRetrieve();
        }
        else
            handled = false;
    }

    if (!handled)
        MythDialog::keyPressEvent(e);
}

void MythNews::cursorUp()
{
    if (m_InColumn == 0) {

        m_CurSite--;
        if (m_CurSite == -1)
            m_CurSite = m_NewsSites.count() - 1;
        m_CurArticle = 0;

    }
    else {

        m_CurArticle--;
        if (m_CurArticle == -1) {
            NewsSite *item = m_NewsSites.at(m_CurSite);
            if (item)
                m_CurArticle = item->articleList().count() - 1;
        }
    }

    showSitesList();
    showArticlesList();

    update(m_MidRect);
    update(m_TopRect);
}

void MythNews::cursorDown()
{
    if (m_InColumn == 0) {

        m_CurSite++;
        if (m_CurSite >= (int) m_NewsSites.count())
            m_CurSite = 0;
        m_CurArticle = 0;
    }
    else {

        m_CurArticle++;
        NewsSite *item = m_NewsSites.at(m_CurSite);
        if (item && m_CurArticle >= (int) item->articleList().count())
            m_CurArticle = 0;
    }


    showSitesList();
    showArticlesList();

    update(m_MidRect);
    update(m_TopRect);
}

void MythNews::cursorRight()
{
    if (m_InColumn == 1) return;

    m_InColumn++;
    m_CurArticle = 0;

    showArticlesList();

    update(m_MidRect);
    update(m_TopRect);
}

void MythNews::cursorLeft()
{
    if (m_InColumn == 0) return;

    m_InColumn--;
    m_CurArticle = 0;

    showSitesList();
    showArticlesList();

    update(m_MidRect);
    update(m_TopRect);
}

void MythNews::forceRetrieveNews()
{
    if (m_NewsSites.count() == 0)
        return;

    cancelRetrieve();

    m_RetrieveTimer->stop();

    for (unsigned int i=0; i<m_NewsSites.count(); i++) {
        NewsSite* site = m_NewsSites.at(i);
        if (site) {
            site->stop();
            connect(site, SIGNAL(finished(const NewsSite*)),
                    this, SLOT(slotNewsRetrieved(const NewsSite*)));
        }
    }

    m_RetrievingNews = true;
    m_RetrievedSites = 0;

    update(m_BotRect);

    for (unsigned int i=0; i<m_NewsSites.count(); i++) {
        NewsSite* site = m_NewsSites.at(i);
        if (site)
            site->retrieve();
    }


    m_RetrieveTimer->start(m_TimerTimeout, false);
}

void MythNews::slotRetrieveNews()
{
    if (m_NewsSites.count() == 0)
        return;

    cancelRetrieve();

    m_RetrieveTimer->stop();

    for (unsigned int i=0; i<m_NewsSites.count(); i++) {
        NewsSite* site = m_NewsSites.at(i);
        if (site) {
            site->stop();
            connect(site, SIGNAL(finished(const NewsSite*)),
                    this, SLOT(slotNewsRetrieved(const NewsSite*)));
        }
    }

    m_RetrievingNews = true;
    m_RetrievedSites = 0;

    for (unsigned int i=0; i<m_NewsSites.count(); i++) {
        NewsSite* site = m_NewsSites.at(i);
        if (site && site->timeSinceLastUpdate() > m_UpdateFreq)
            site->retrieve();
        else
            m_RetrievedSites++;
    }

    if (m_RetrievedSites >= m_NewsSites.count()) {
        processAndShowNews();
    }
    else {
        update(m_BotRect);
    }

    m_RetrieveTimer->start(m_TimerTimeout, false);
}

void MythNews::slotNewsRetrieved(const NewsSite* site)
{
    m_RetrievedSites++;

    /*
      LayerSet *container = m_Theme->GetSet("messages");
      if (container)
      {
      UITextType *ttype =
      (UITextType *)container->GetType("retrieved");
      if (ttype) {
      if (item->successful())
      ttype->SetText(QString("[Retrieved News from ")
      + item->name() + QString("]"));
      else
      ttype->SetText(QString("[Failed to Retrieve News from ")
      + item->name() + QString("]"));
      }
      }
    */

    unsigned int updated = site->lastUpdated().toTime_t();


    QSqlQuery query("UPDATE newssites SET updated=" +
                    QString::number(updated) +
                    " WHERE name='" +
                    site->name() + "'",  m_db);
    if (!query.isActive()) {
        cerr << "MythNews: Error in updating time in DB" << endl;
    }


    if (m_RetrievedSites >= m_NewsSites.count()) {
        processAndShowNews();
    }
    else {
        update(m_BotRect);
    }
}

void MythNews::cancelRetrieve()
{
    for (NewsSite* item = m_NewsSites.first(); item;
         item = m_NewsSites.next()) {
        item->stop();
    }

    processAndShowNews();
}

void MythNews::processAndShowNews()
{
    m_RetrievingNews = false;

    // Reset current article number
    m_CurArticle = 0;

    for (NewsSite* item = m_NewsSites.first(); item;
         item = m_NewsSites.next()) {
        item->process();
    }
    showSitesList();
    showArticlesList();
    update();
}
