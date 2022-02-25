#ifndef BOOKMARKEDITOR_H
#define BOOKMARKEDITOR_H

// MythTV
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythscreentype.h>
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuicheckbox.h>
#include <libmythui/mythuitext.h>
#include <libmythui/mythuitextedit.h>


class Bookmark;

/** \class BookmarkEditor
 *  \brief Site category, name and URL edit screen.
 */
class BookmarkEditor : public MythScreenType
{
    Q_OBJECT

  public:

    BookmarkEditor(Bookmark *site, bool edit, MythScreenStack *parent,
                   const char *name);
    ~BookmarkEditor() override = default;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType

  private:
    Bookmark   *m_site                 {nullptr};
    QString     m_siteName;
    QString     m_siteCategory;
    bool        m_editing;

    MythUIText     *m_titleText        {nullptr};

    MythUITextEdit *m_categoryEdit     {nullptr};
    MythUITextEdit *m_nameEdit         {nullptr};
    MythUITextEdit *m_urlEdit          {nullptr};
    MythUICheckBox *m_isHomepage       {nullptr};

    MythUIButton *m_okButton           {nullptr};
    MythUIButton *m_cancelButton       {nullptr};
    MythUIButton *m_findCategoryButton {nullptr};

    MythUISearchDialog *m_searchDialog {nullptr};

  private slots:
    void slotFindCategory(void);
    void slotCategoryFound(const QString& category);

    void Save(void);
    void Exit(void);
};

#endif
