#ifndef BOOKMARKMANAGER_H
#define BOOKMARKMANAGER_H

// libmythbase
#include <mythmiscutil.h>

// libmythui
#include <mythscreentype.h>

class MythBrowser;
class MythDialogBox;

class Bookmark
{
  public:
    Bookmark(void) = default;

    QString m_category;
    QString m_name;
    QString m_sortName;
    QString m_url;
    bool    m_isHomepage {false};
    bool    m_selected   {false};

    inline bool operator == (const Bookmark &b) const
    {
        return m_category == b.m_category && m_name == b.m_name && m_url == b.m_url;
    }
    static inline bool sortByName(Bookmark *a, Bookmark *b)
        { return naturalCompare(a->m_sortName, b->m_sortName) < 0; }
};

class BrowserConfig : public MythScreenType
{
  Q_OBJECT

  public:

    BrowserConfig(MythScreenStack *parent, const char *name = nullptr)
        : MythScreenType(parent, name) {}
    ~BrowserConfig() = default;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *) override; // MythScreenType

  private:
    MythUITextEdit   *m_commandEdit        {nullptr};
    MythUITextEdit   *m_zoomEdit           {nullptr};

    MythUIText       *m_descriptionText    {nullptr};
    MythUIText       *m_titleText          {nullptr};
    MythUICheckBox   *m_enablePluginsCheck {nullptr};

    MythUIButton     *m_okButton           {nullptr};
    MythUIButton     *m_cancelButton       {nullptr};

  private slots:
    void slotSave(void);
    void slotFocusChanged(void);
};

class BookmarkManager : public MythScreenType
{
  Q_OBJECT

  public:
    BookmarkManager(MythScreenStack *parent, const char *name)
        : MythScreenType(parent, name) {}
    ~BookmarkManager();

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *) override; // MythScreenType

  private slots:
    void slotGroupSelected(MythUIButtonListItem *item);
    void slotBookmarkClicked(MythUIButtonListItem *item);
    void slotEditDialogExited(void);
    void slotDoDeleteCurrent(bool doDelete);
    void slotDoDeleteMarked(bool doDelete);
    void slotBrowserClosed(void);

    static void slotSettings(void);
    void slotSetHomepage(void);
    void slotAddBookmark(void);
    void slotEditBookmark(void);
    void slotDeleteCurrent(void);
    void slotDeleteMarked(void);
    void slotShowCurrent(void);
    void slotShowMarked(void);
    void slotClearMarked(void);

  private:
    uint GetMarkedCount(void);
    void UpdateGroupList(void);
    void UpdateURLList(void);
    void ShowEditDialog(bool edit);
    void ReloadBookmarks(void);

    QList<Bookmark*>  m_siteList;

    Bookmark          m_savedBookmark;

    MythUIButtonList *m_bookmarkList {nullptr};
    MythUIButtonList *m_groupList    {nullptr};
    MythUIText       *m_messageText  {nullptr};

    MythDialogBox    *m_menuPopup    {nullptr};
};

Q_DECLARE_METATYPE(Bookmark *)

#endif
