#ifndef NETTREE_H
#define NETTREE_H

#include <mythtv/uitypes.h>
#include <mythtv/xmlparse.h>
#include <mythtv/oldsettings.h>
#include <mythtv/mythwidgets.h>
#include <mythtv/mythdialogs.h>

// libmythui
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythuibuttontree.h>
#include <libmythui/mythgenerictree.h>
#include <libmythui/mythuiprogressbar.h>
#include <libmythui/mythuistatetype.h>
#include <libmythui/mythscreentype.h>
#include <libmythui/mythdialogbox.h>

#include "netgrabbermanager.h"
#include "mythrssmanager.h"
#include "downloadmanager.h"
#include "imagedownloadmanager.h"

enum DialogType { DLG_DEFAULT = 0, DLG_GALLERY = 0x1, DLG_TREE = 0x2,
                  DLG_BROWSER = 0x4, dtLast };

enum TreeNodeType {
    kSubFolder = -1,
    kUpFolder = -2,
    kRootNode = -3,
    kNoFilesFound = -4
};

// Tree node attribute index
enum TreeNodeAttributes {
    kNodeSort
};

enum NodeOrder {
    kOrderUp,
    kOrderSub,
    kOrderItem
};

class MythUIBusyDialog;

class NetTree : public MythScreenType
{
  Q_OBJECT

  public:
    NetTree(DialogType type, MythScreenStack *parent, const char *name = 0);
    ~NetTree();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

    void populateResultList(ResultItem::resultList list);
    QString getDownloadFilename(ResultItem *item);

  public slots:

  protected:
    void createBusyDialog(QString title);

  private:
    virtual void Load();
    virtual void Init();

    void fillTree(void);
    void SetCurrentNode(MythGenericTree *node);
    void handleDirSelect(MythGenericTree *node);
    bool goBack();
    void UpdateItem(MythUIButtonListItem *item);

    void buildGenericTree(MythGenericTree* dst,
                    QDomElement& domElem);
    void buildGenericTree(MythGenericTree *dst,
                          QStringList paths,
                          QString dirthumb,
                          QList<ResultItem*> videos);

    void cleanCacheDir(void);

    MythGenericTree *AddDirNode(
                    MythGenericTree *where_to_add,
                    QString name, QString thumbnail);
    int AddFileNode(MythGenericTree *where_to_add,
                    ResultItem *video);

    void switchView(void);

    // Only to keep track of what to delete
    QList<ResultItem*> m_videos;

    MythUIButtonTree   *m_siteMap;
    MythUIButtonList   *m_siteButtonList;
    MythGenericTree    *m_siteGeneric;
    MythGenericTree    *m_rssGeneric;
    MythGenericTree    *m_searchGeneric;
    MythGenericTree    *m_currentNode;

    MythUIText         *m_noSites;

    MythUIImage        *m_thumbImage;

    MythUIStateType    *m_downloadable;

    MythUIBusyDialog   *m_busyPopup;

    MythDialogBox      *m_menuPopup;
    MythScreenStack    *m_popupStack;

    DownloadManager    *m_download;
    ImageDownloadManager  *m_imageDownload;
    GrabberDownloadThread *m_gdt;

    QFile              *m_file;

    QProcess           *m_externaldownload;

    GrabberScript::scriptList m_grabberList;
    RSSSite::rssList m_rssList;

    DialogType          m_type;

    mutable QMutex      m_lock;

    uint                m_updateFreq;
    bool                m_rssAutoUpdate;
    bool                m_treeAutoUpdate;

  private slots:
    void showWebVideo(void);
    void doDownloadAndPlay(void);
    void doPlayVideo(void);
    void showMenu(void);
    void showViewMenu(void);
    void showManageMenu(void);
    void runTreeEditor(void);
    void runRSSEditor(void);
    void loadData(void);
    void handleSelect(MythUIButtonListItem *item);

    void switchTreeView(void);
    void switchGalleryView(void);
    void switchBrowseView(void);

    void updateRSS();
    void updateTrees();
    void toggleRSSUpdates();
    void toggleTreeUpdates();

    void slotDeleteVideo(void);
    void doDeleteVideo(bool remove);

    void slotItemChanged();

    void doTreeRefresh();
    void TreeRefresh();

    void customEvent(QEvent *levent);

  protected:
    static const QString RSSNode;
    static const QString SearchNode;
    static const QString DownloadNode;
};

#endif

