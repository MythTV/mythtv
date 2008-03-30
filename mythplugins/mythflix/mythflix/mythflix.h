/* ============================================================
 * File  : mythflix.h
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

#ifndef MYTHFLIX_H
#define MYTHFLIX_H

// MythTV headers
#include <mythtv/libmythui/mythscreentype.h>
#include <mythtv/libmythui/mythuitext.h>
#include <mythtv/libmythui/mythlistbutton.h>
#include <mythtv/libmythui/mythuiimage.h>
#include <mythtv/libmythui/mythdialogbox.h>

#include "newsengine.h"
#include <QKeyEvent>

/** \class MythFlix
 *  \brief The netflix browser class.
 */
class MythFlix : public MythScreenType
{
    Q_OBJECT

  public:

    MythFlix(MythScreenStack *parent, const char *name);
    ~MythFlix();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);
    void customEvent(QEvent*);

  private:
    void loadData();

    void displayOptions();

    void cancelRetrieve();
    void processAndShowNews(NewsSite *site);
    void InsertMovieIntoQueue(QString queueName = "", bool atTop = false);

    QString executeExternal(const QStringList& args, const QString& purpose);

    MythListButton *m_sitesList;
    MythListButton *m_articlesList;

    MythUIText *m_statusText;
    MythUIText *m_titleText;
    MythUIText *m_descText;

    MythUIImage *m_boxshotImage;

    MythDialogBox  *m_menuPopup;
    QString        zoom;
    QString        browser;
    NewsSite::List m_NewsSites;

private slots:
    void updateInfoView(MythListButtonItem*);
    void slotRetrieveNews();
    void slotNewsRetrieved(NewsSite* site);

    void slotSiteSelected(MythListButtonItem *item);
    void slotShowNetFlixPage();
    void slotCancelPopup();
};

#endif /* MYTHFLIX_H */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
