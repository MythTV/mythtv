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

    explicit NetSearch(MythScreenStack *parent, const char *name = nullptr);
    ~NetSearch() override;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType

    void PopulateResultList(const ResultItem::resultList& list);

  protected:
    ResultItem *GetStreamItem() override; // NetBase

  private:
    void Load() override; // MythScreenType

    MythUIButtonList   *m_searchResultList {nullptr};
    MythUIButtonList   *m_siteList         {nullptr};
    MythUITextEdit     *m_search           {nullptr};

    MythUIText         *m_pageText         {nullptr};
    MythUIText         *m_noSites          {nullptr};

    MythUIProgressBar  *m_progress         {nullptr};
    MythConfirmationDialog *m_okPopup      {nullptr};

    QNetworkAccessManager *m_netSearch     {nullptr};
    QNetworkReply         *m_reply         {nullptr};

    QString             m_currentSearch;
    int                 m_currentGrabber   {0};
    QString             m_currentCmd;
    uint                m_pagenum          {0};
    uint                m_maxpage          {0};
    QString             m_mythXML;

    RSSSite::rssList    m_rssList;

    QString m_nextPageToken;
    QString m_prevPageToken;

  private slots:
    void ShowMenu(void) override; // MythScreenType
    void GetMoreResults();
    void GetLastResults();
    void SkipPagesBack();
    void SkipPagesForward();
    void RunSearchEditor() const;
    void DoListRefresh();

    void DoSearch(void);
    void SearchFinished(void);
    void SearchTimeout(Search *item);
    void LoadData(void) override; // NetBase
    void FillGrabberButtonList(void);
    void SlotItemChanged(void);
    void SetTextAndThumbnail(MythUIButtonListItem *btn, ResultItem *item);
    void SetThumbnail(MythUIButtonListItem *btn);
    void customEvent(QEvent *levent) override; // NetBase
};

#endif
