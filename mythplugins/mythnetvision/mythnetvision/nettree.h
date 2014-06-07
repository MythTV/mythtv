#ifndef NETTREE_H
#define NETTREE_H

#include "netbase.h"

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

class NetTree : public NetBase
{
  Q_OBJECT

  public:
    NetTree(DialogType type, MythScreenStack *parent, const char *name = 0);
    ~NetTree();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

  protected:
    virtual ResultItem *GetStreamItem();

  private:
    virtual void Load();

    void FillTree(void);
    void SetCurrentNode(MythGenericTree *node);
    void HandleDirSelect(MythGenericTree *node);
    bool GoBack();
    void UpdateItem(MythUIButtonListItem *item);

    void BuildGenericTree(MythGenericTree* dst,
                          QDomElement& domElem);
    void BuildGenericTree(MythGenericTree *dst,
                          QStringList paths,
                          QString dirthumb,
                          QList<ResultItem*> videos);

    void AddFileNode(MythGenericTree *where_to_add,
                     ResultItem *video);

    void SwitchView(void);

    void SetSubfolderData(MythGenericTree *folder);
    void UpdateResultItem(ResultItem *item);
    void UpdateSiteItem(RSSSite *site);
    void UpdateCurrentItem(void);

    // Only to keep track of what to delete
    QList<ResultItem*> m_videos;

    MythUIButtonTree   *m_siteMap;
    MythUIButtonList   *m_siteButtonList;
    MythGenericTree    *m_siteGeneric;
    MythGenericTree    *m_currentNode;

    MythUIText         *m_noSites;

    GrabberDownloadThread *m_gdt;
    RSSSite::rssList m_rssList;

    DialogType          m_type;

    uint                m_updateFreq;
    bool                m_rssAutoUpdate;
    bool                m_treeAutoUpdate;

  private slots:
    void ShowMenu(void);
    MythMenu* CreateShowViewMenu(void);
    MythMenu* CreateShowManageMenu(void);
    void RunTreeEditor(void);
    void RunRSSEditor(void);
    void LoadData(void);
    void HandleSelect(MythUIButtonListItem *item);

    void SwitchTreeView(void);
    void SwitchGalleryView(void);
    void SwitchBrowseView(void);

    void UpdateRSS();
    void UpdateTrees();
    void ToggleRSSUpdates();
    void ToggleTreeUpdates();

    void SlotItemChanged();

    void DoTreeRefresh();
    void TreeRefresh();

    void customEvent(QEvent *levent);

  protected:
    static const QString RSSNode;
    static const QString SearchNode;
    static const QString DownloadNode;
};

#endif
