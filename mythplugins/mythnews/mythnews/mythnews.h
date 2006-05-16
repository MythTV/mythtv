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


#include <mythtv/uitypes.h>
#include <mythtv/uilistbtntype.h>
#include <mythtv/xmlparse.h>
#include <mythtv/mythdialogs.h>

#include "newsengine.h"

class QTimer;
class UIListBtnType;

class MythNews : public MythDialog
{
    Q_OBJECT

public:

    MythNews(MythMainWindow *parent, const char *name = 0);
    ~MythNews();

private:

    void loadTheme();
    void loadWindow(QDomElement &element);
    void paintEvent(QPaintEvent *e);

    void updateSitesView();
    void updateArticlesView();
    void updateInfoView();
    
    void keyPressEvent(QKeyEvent *e);
    void cursorUp(bool page=false);
    void cursorDown(bool page=false);
    void cursorRight();
    void cursorLeft();

    void cancelRetrieve();
    void processAndShowNews(NewsSite *site);
    void loadSites();
    bool findInDB(const QString& name);
    bool insertInDB(const QString &name, const QString &url, 
                    const QString &icon, const QString &category);

    bool removeFromDB(const QString &name);

    XMLParse      *m_Theme;

    UIListBtnType *m_UISites;
    UIListBtnType *m_UIArticles;
    QRect          m_SitesRect;
    QRect          m_ArticlesRect;
    QRect          m_InfoRect;
    unsigned int   m_InColumn;

    NewsSite::List m_NewsSites;

    QTimer        *m_RetrieveTimer;
    int            m_TimerTimeout;
    unsigned int   m_UpdateFreq;

    QString        timeFormat;
    QString        dateFormat;
    QString        zoom;
    QString        browser;
    MythPopupBox   *menu;

private slots:
    void slotViewArticle();
    void slotRetrieveNews();
    void slotNewsRetrieved(NewsSite* site);

    void slotSiteSelected(UIListBtnTypeItem *item);
    void slotSiteSelected(NewsSite*);
    
    void slotArticleSelected(UIListBtnTypeItem *item);

    // menu stuff
    void showMenu();
    void addNewsSite();
    void editNewsSite();
    void deleteNewsSite();
    void cancelMenu();
    bool showEditDialog(bool edit);
};

#endif /* MYTHNEWS_H */
