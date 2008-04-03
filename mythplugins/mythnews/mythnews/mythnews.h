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

// MythTV headers
#include <mythtv/libmythui/mythscreentype.h>
#include <mythtv/libmythui/mythuitext.h>
#include <mythtv/libmythui/mythlistbutton.h>
#include <mythtv/libmythui/mythuiimage.h>
#include <mythtv/libmythui/mythdialogbox.h>
#include <mythtv/httpcomms.h>
#include "newsengine.h"

class QTimer;

// class MPUBLIC MythNewsBusyDialog : public MythBusyDialog
// {
//     Q_OBJECT
//   public:
//     MythNewsBusyDialog(const QString &title);
//     ~MythNewsBusyDialog();
//
//     void keyPressEvent(QKeyEvent *);
//
//   signals:
//     void cancelAction();
//
// };

/** \class MythNews
 *  \brief Plugin for browsing RSS news feeds.
 */
class MythNews : public MythScreenType
{
    Q_OBJECT

  public:

    MythNews(MythScreenStack *parent, const char *name);
    ~MythNews();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);
//    void customEvent(QEvent*);

private:
    void updateInfoView();

    void cancelRetrieve();
    void processAndShowNews(NewsSite *site);
    void loadSites();
    bool findInDB(const QString& name);
    bool insertInDB(const QString &name, const QString &url,
                    const QString &icon, const QString &category);

    bool removeFromDB(const QString &name);

    bool getHttpFile(QString sFilename, QString cmdURL);
    void createProgress(QString title);
    void createProgress(QString title, int total);
    QString formatSize(long long bytes, int prec);
    void playVideo(const QString &filename);

    NewsSite::List m_NewsSites;

    QTimer        *m_RetrieveTimer;
    int            m_TimerTimeout;
    unsigned int   m_UpdateFreq;

    QString        timeFormat;
    QString        dateFormat;
    QString        zoom;
    QString        browser;
    MythDialogBox *m_menuPopup;

    bool           abortHttp;

//     MythNewsBusyDialog *busy;
    HttpComms      *httpGrabber;

    MythListButton *m_sitesList;
    MythListButton *m_articlesList;

    MythUIText *m_updatedText;
    MythUIText *m_titleText;
    MythUIText *m_descText;

    MythUIImage *m_thumbnailImage;
    MythUIImage *m_downloadImage;
    MythUIImage *m_enclosureImage;

private slots:
    void updateInfoView(MythListButtonItem* selected);
    void slotViewArticle();
    void slotRetrieveNews();
    void slotNewsRetrieved(NewsSite* site);
    void slotSiteSelected(MythListButtonItem*);

    void slotProgressCancelled();

    // menu stuff
    void ShowMenu();
    void addNewsSite();
    void editNewsSite();
    void deleteNewsSite();
    void cancelMenu();
    bool ShowEditDialog(bool edit);
};

#endif /* MYTHNEWS_H */
