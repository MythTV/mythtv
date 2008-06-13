/* ============================================================
 * File  : mythflixbrowse.h
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

#ifndef MYTHFLIXQUEUE_H
#define MYTHFLIXQUEUE_H

// QT headers
#include <q3http.h>
#include <QKeyEvent>

// MythTV headers
#include <mythtv/libmythui/mythscreentype.h>
#include <mythtv/libmythui/mythuitext.h>
#include <mythtv/libmythui/mythlistbutton.h>
#include <mythtv/libmythui/mythuiimage.h>
#include <mythtv/libmythui/mythdialogbox.h>

#include "newsengine.h"

/** \class MythFlix
 *  \brief The netflix queue browser class.
 */
class MythFlixQueue : public MythScreenType
{
    Q_OBJECT

public:

    MythFlixQueue(MythScreenStack *, const char *);
    ~MythFlixQueue();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);
    void customEvent(QEvent*);

  private:
    void loadData();
    MythImage* LoadPosterImage(QString location);

    void UpdateNameText();

    void displayOptions();

    void cancelRetrieve();
    void processAndShowNews(NewsSite *site);

    QString executeExternal(const QStringList& args, const QString& purpose);

    MythListButton *m_articlesList;

    MythUIText *m_nameText;
    MythUIText *m_titleText;
    MythUIText *m_descText;

    MythUIImage *m_boxshotImage;

    MythDialogBox  *m_menuPopup;
    QString        zoom;
    QString        browser;
    NewsSite::List m_NewsSites;

    QString        m_queueName;

    Q3Http         *http;

private slots:
    void updateInfoView(MythListButtonItem*);
    void slotViewArticle();
    void slotRetrieveNews();
    void slotNewsRetrieved(NewsSite* site);

    void slotSiteSelected(NewsSite*);

    void slotMoveToTop();
    void slotRemoveFromQueue();
    void slotMoveToQueue();
    void slotShowNetFlixPage();
    void slotCancelPopup();

};

#endif /* MYTHFLIXQUEUE_H */

