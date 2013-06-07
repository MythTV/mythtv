#ifndef NETTREE_H
#define NETTREE_H

// libmythui
#include <mythuibuttonlist.h>
#include <mythuibuttontree.h>
#include <mythgenerictree.h>
#include <mythuiprogressbar.h>
#include <mythprogressdialog.h>
#include <mythuistatetype.h>
#include <mythscreentype.h>
#include <mythdialogbox.h>
#include <netgrabbermanager.h>
#include <mythrssmanager.h>
#include <mythdownloadmanager.h>
#include <metadata/metadataimagedownload.h>

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

    void initProgressDialog();

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

    MythDialogBox        *m_menuPopup;
    MythScreenStack      *m_popupStack;
    MythUIProgressDialog *m_progressDialog;

    MetadataImageDownload *m_imageDownload;
    GrabberDownloadThread *m_gdt;
    QString               m_downloadFile;

    GrabberScript::scriptList m_grabberList;
    RSSSite::rssList m_rssList;

    DialogType          m_type;

    uint                m_updateFreq;
    bool                m_rssAutoUpdate;
    bool                m_treeAutoUpdate;

  private slots:
    void streamWebVideo(void);
    void showWebVideo(void);
    void doDownloadAndPlay(void);
    void doPlayVideo(QString filename);
    void showMenu(void);
    MythMenu* createShowViewMenu(void);
    MythMenu* createShowManageMenu(void);
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
    void DownloadVideo(QString url, QString dest);

  protected:
    static const QString RSSNode;
    static const QString SearchNode;
    static const QString DownloadNode;
};

#endif

