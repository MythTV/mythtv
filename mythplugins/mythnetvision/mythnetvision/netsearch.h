#ifndef NETSEARCH_H
#define NETSEARCH_H

#include <mythtv/uitypes.h>
#include <mythtv/xmlparse.h>
#include <mythtv/oldsettings.h>
#include <mythtv/mythwidgets.h>
#include <mythtv/mythdialogs.h>

// libmythui
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythuitextedit.h>
#include <libmythui/mythuiprogressbar.h>
#include <libmythui/mythuistatetype.h>
#include <libmythui/mythscreentype.h>
#include <libmythui/mythdialogbox.h>

#include "search.h"
#include "grabbermanager.h"
#include "rssmanager.h"
#include "downloadmanager.h"
#include "imagedownloadmanager.h"

class MythUIBusyDialog;

class NetSearch : public MythScreenType
{
  Q_OBJECT

  public:

    enum DialogType { DLG_DEFAULT = 0, DLG_SEARCH = 0x1, DLG_RSS = 0x2,
                      dtLast };

    NetSearch(MythScreenStack *parent, const char *name = 0);
    ~NetSearch();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

    void populateResultList(ResultVideo::resultList list);
    QString getDownloadFilename(ResultVideo *item);

  public slots:

  protected:
    void createBusyDialog(QString title);

  private:
    virtual void Load();
    virtual void Init();

    void cleanCacheDir(void);

    MythUIButtonList   *m_searchResultList;
    MythUIButtonList   *m_siteList;
    MythUITextEdit     *m_search;

    MythUIText         *m_title;
    MythUIText         *m_description;
    MythUIText         *m_url;
    MythUIText         *m_thumbnail;
    MythUIText         *m_mediaurl;
    MythUIText         *m_author;
    MythUIText         *m_date;
    MythUIText         *m_time;
    MythUIText         *m_filesize;
    MythUIText         *m_filesize_str;
    MythUIText         *m_rating;
    MythUIText         *m_pageText;
    MythUIText         *m_noSites;
    MythUIText         *m_width;
    MythUIText         *m_height;
    MythUIText         *m_resolution;
    MythUIText         *m_countries;
    MythUIText         *m_season;
    MythUIText         *m_episode;
    MythUIText         *m_s00e00;
    MythUIText         *m_00x00;

    MythUIImage        *m_thumbImage;
    MythUIStateType    *m_downloadable;
    MythUIProgressBar  *m_progress;
    MythUIBusyDialog   *m_busyPopup;
    MythConfirmationDialog *m_okPopup;

    MythDialogBox      *m_menuPopup;
    MythScreenStack    *m_popupStack;

    Search             *m_netSearch;
    DownloadManager    *m_download;
    ImageDownloadManager  *m_imageDownload;
    QFile              *m_file;
    QProcess           *m_externaldownload;

    QString             m_currentSearch;
    int                 m_currentGrabber;
    QString             m_currentCmd;
    QString             m_currentDownload;
    uint                m_pagenum;
    uint                m_maxpage;
    bool                m_playing;
    uint                m_redirects;

    GrabberScript::scriptList m_grabberList;
    RSSSite::rssList    m_rssList;
    QMap<MythUIButtonListItem*,ResultVideo> m_rssitems;
    DialogType          m_dialogType;

    mutable QMutex      m_lock;

    NetSearch::DialogType m_type;

  private slots:
    void showWebVideo(void);
    void doDownloadAndPlay(void);
    void doPlayVideo(void);
    void showMenu(void);
    void getMoreResults();
    void getLastResults();

    void doSearch(void);
    void searchFinished(Search *item);
    void searchTimeout(Search *item);
    void loadData(void);
    GrabberScript::scriptList fillGrabberList(void);
    void fillGrabberButtonList(void);
    void slotItemChanged(void);
    void slotDoProgress(qint64 bytesReceived,
                        qint64 bytesTotal);
    void slotDownloadFinished(void);
    void slotDeleteVideo(void);
    void doDeleteVideo(bool remove);

    void customEvent(QEvent *levent);
};

#endif

