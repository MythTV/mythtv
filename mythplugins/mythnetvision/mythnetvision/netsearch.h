#ifndef NETSEARCH_H
#define NETSEARCH_H

#include "netbase.h"

// libmythui
#include <mythuibutton.h>
#include <mythuibuttonlist.h>
#include <mythuitextedit.h>
#include <mythuiprogressbar.h>
#include <mythprogressdialog.h>
#include <mythuistatetype.h>
#include <mythscreentype.h>
#include <mythdialogbox.h>
#include <netgrabbermanager.h>
#include <mythrssmanager.h>
#include <mythdownloadmanager.h>

class NetSearch : public NetBase
{
  Q_OBJECT

  public:

    NetSearch(MythScreenStack *parent, const char *name = 0);
    ~NetSearch();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

    void populateResultList(ResultItem::resultList list);

  protected:
    virtual ResultItem *GetStreamItem();

  private:
    virtual void Load();

    MythUIButtonList   *m_searchResultList;
    MythUIButtonList   *m_siteList;
    MythUITextEdit     *m_search;

    MythUIText         *m_pageText;
    MythUIText         *m_noSites;

    MythUIProgressBar  *m_progress;
    MythConfirmationDialog *m_okPopup;

    QNetworkAccessManager *m_netSearch;
    QNetworkReply         *m_reply;

    QString             m_currentSearch;
    int                 m_currentGrabber;
    QString             m_currentCmd;
    uint                m_pagenum;
    uint                m_maxpage;
    QString             m_mythXML;

    RSSSite::rssList    m_rssList;

  private slots:
    void showMenu(void);
    void getMoreResults();
    void getLastResults();
    void runSearchEditor();
    void doListRefresh();

    void doSearch(void);
    void searchFinished(void);
    void searchTimeout(Search *item);
    void loadData(void);
    void fillGrabberButtonList(void);
    void slotItemChanged(void);
    void slotDoProgress(qint64 bytesReceived,
                        qint64 bytesTotal);
    void slotDownloadFinished(void);
    void customEvent(QEvent *levent);
};

#endif
