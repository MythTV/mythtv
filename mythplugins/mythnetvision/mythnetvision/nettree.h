#ifndef NETTREE_H
#define NETTREE_H

#include "netbase.h"

// MythTV
#include <libmythbase/mythdownloadmanager.h>
#include <libmythbase/netgrabbermanager.h>
#include <libmythbase/rssmanager.h>
#include <libmythmetadata/metadataimagedownload.h>
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythgenerictree.h>
#include <libmythui/mythprogressdialog.h>
#include <libmythui/mythscreentype.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythuibuttontree.h>
#include <libmythui/mythuiprogressbar.h>
#include <libmythui/mythuistatetype.h>

enum DialogType : std::uint8_t
                { DLG_DEFAULT = 0, DLG_GALLERY = 0x1, DLG_TREE = 0x2,
                  DLG_BROWSER = 0x4, dtLast = 0x5 };

enum TreeNodeType : std::int8_t {
    kSubFolder = -1,
    kUpFolder = -2,
    kRootNode = -3,
    kNoFilesFound = -4
};

// Tree node attribute index
enum TreeNodeAttributes : std::uint8_t {
    kNodeSort
};

enum NodeOrder : std::uint8_t {
    kOrderUp,
    kOrderSub,
    kOrderItem
};

class NetTree : public NetBase
{
  Q_OBJECT

  public:
    NetTree(DialogType type, MythScreenStack *parent, const char *name = nullptr);
    ~NetTree() override;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType

  protected:
    ResultItem *GetStreamItem() override; // NetBase

  private:
    void Load() override; // MythScreenType

    void FillTree(void);
    void SetCurrentNode(MythGenericTree *node);
    void HandleDirSelect(MythGenericTree *node);
    bool GoBack();
    void UpdateItem(MythUIButtonListItem *item);

    void BuildGenericTree(MythGenericTree* dst,
                          QDomElement& domElem);
    void BuildGenericTree(MythGenericTree *dst,
                          QStringList paths,
                          const QString& dirthumb,
                          const QList<ResultItem*>& videos);

    void AddFileNode(MythGenericTree *where_to_add,
                     ResultItem *video);

    void SwitchView(void);

    static void SetSubfolderData(MythGenericTree *folder);
    void UpdateResultItem(ResultItem *item);
    void UpdateSiteItem(RSSSite *site);
    void UpdateCurrentItem(void);

    // Only to keep track of what to delete
    QList<ResultItem*> m_videos;

    MythUIButtonTree   *m_siteMap        {nullptr};
    MythUIButtonList   *m_siteButtonList {nullptr};
    MythGenericTree    *m_siteGeneric    {nullptr};
    MythGenericTree    *m_currentNode    {nullptr};

    MythUIText         *m_noSites        {nullptr};

    GrabberDownloadThread *m_gdt         {nullptr};
    RSSSite::rssList m_rssList;

    DialogType          m_type;

    uint                m_updateFreq     {6};
    bool                m_rssAutoUpdate  {false};
    bool                m_treeAutoUpdate {false};

  private slots:
    void ShowMenu(void) override; // MythScreenType
    MythMenu* CreateShowViewMenu(void);
    MythMenu* CreateShowManageMenu(void);
    void RunTreeEditor(void) const;
    void RunRSSEditor(void) const;
    void LoadData(void) override; // NetBase
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

    void customEvent(QEvent *levent) override; // NetBase

  protected:
    static const QString kRSSNode;
    static const QString kSearchNode;
    static const QString kDownloadNode;
};

#endif
