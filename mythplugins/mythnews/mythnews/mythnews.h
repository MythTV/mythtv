/* ============================================================
 * File  : mythnews.h
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

#ifndef MYTHNEWS_H
#define MYTHNEWS_H

#include <qsqldatabase.h>
#include <qwidget.h>
#include <qdatetime.h>

#include <mythtv/uitypes.h>
#include <mythtv/xmlparse.h>
#include <mythtv/oldsettings.h>
#include <mythtv/mythwidgets.h>
#include <mythtv/mythdialogs.h>

#include "newsengine.h"

class QTimer;

class MythNews : public MythDialog
{
    Q_OBJECT

public:

    MythNews(QSqlDatabase *db, MythMainWindow *parent,
             const char *name = 0);
    ~MythNews();

private:

    void LoadWindow(QDomElement &element);
    void updateBackground();
    void paintEvent(QPaintEvent *e);
    void updateTopView();
    void updateMidView();
    void updateBotView();

    void keyPressEvent(QKeyEvent *e);
    void cursorUp();
    void cursorDown();
    void cursorRight();
    void cursorLeft();

    void showSitesList();
    void showArticlesList();

    void forceRetrieveNews();
    void cancelRetrieve();
    void processAndShowNews();

    QSqlDatabase *m_db;
    XMLParse     *m_Theme;

    QRect        m_TopRect;
    QRect        m_MidRect;
    QRect        m_BotRect;

    int          m_CurSite;
    int          m_CurArticle;
    unsigned int m_ItemsPerListing;
    unsigned int m_InColumn;


    bool           m_RetrievingNews;
    unsigned int   m_RetrievedSites;
    NewsSite::List m_NewsSites;
    unsigned int   m_UpdateFreq;
    QTimer        *m_RetrieveTimer;
    int            m_TimerTimeout;

private slots:

    void slotRetrieveNews();
    void slotNewsRetrieved(const NewsSite* site);
};

#endif /* MYTHNEWS_H */
