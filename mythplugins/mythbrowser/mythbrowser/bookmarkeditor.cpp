
// MythTV
#include <libmyth/mythcontext.h>
#include <libmythbase/mythdate.h>
#include <libmythbase/mythdbcon.h>
#include <libmythui/mythmainwindow.h>

// mythbrowser
#include "bookmarkeditor.h"
#include "bookmarkmanager.h"
#include "browserdbutil.h"

/** \brief Creates a new BookmarkEditor Screen
 *  \param site   The bookmark we are adding/editing
 *  \param edit   If true we are editing an existing bookmark
 *  \param parent Pointer to the screen stack
 *  \param name   The name of the window
 */
BookmarkEditor::BookmarkEditor(Bookmark *site, bool edit,
                               MythScreenStack *parent, const char *name)
    : MythScreenType (parent, name),
      m_site(site),
      m_editing(edit)
{
    if (m_editing)
    {
        m_siteCategory = m_site->m_category;
        m_siteName = m_site->m_name;
    }
}

bool BookmarkEditor::Create()
{
    if (!LoadWindowFromXML("browser-ui.xml", "bookmarkeditor", this))
        return false;

    bool err = false;

    UIUtilW::Assign(this, m_titleText, "title",  &err);
    UIUtilE::Assign(this, m_categoryEdit, "category", &err);
    UIUtilE::Assign(this, m_nameEdit, "name", &err);
    UIUtilE::Assign(this, m_urlEdit, "url", &err);
    UIUtilE::Assign(this, m_isHomepage, "homepage", &err);
    UIUtilE::Assign(this, m_okButton, "ok", &err);
    UIUtilE::Assign(this, m_cancelButton, "cancel", &err);
    UIUtilE::Assign(this, m_findCategoryButton, "findcategory", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'bookmarkeditor'");
        return false;
    }

    if (m_titleText)
    {
      if (m_editing)
          m_titleText->SetText(tr("Edit Bookmark Details"));
      else
          m_titleText->SetText(tr("Enter Bookmark Details"));
    }

    connect(m_okButton, &MythUIButton::Clicked, this, &BookmarkEditor::Save);
    connect(m_cancelButton, &MythUIButton::Clicked, this, &BookmarkEditor::Exit);
    connect(m_findCategoryButton, &MythUIButton::Clicked, this, &BookmarkEditor::slotFindCategory);

    if (m_editing && m_site)
    {
        m_categoryEdit->SetText(m_site->m_category);
        m_nameEdit->SetText(m_site->m_name);
        m_urlEdit->SetText(m_site->m_url);

        if (m_site->m_isHomepage)
            m_isHomepage->SetCheckState(MythUIStateType::Full);
    }

    BuildFocusList();

    SetFocusWidget(m_categoryEdit);

    return true;
}

bool BookmarkEditor::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("News", event, actions);

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void BookmarkEditor::Exit()
{
    Close();
}

void BookmarkEditor::Save()
{
    if (m_editing && m_siteCategory != "" && m_siteName != "")
        RemoveFromDB(m_siteCategory, m_siteName);

    ResetHomepageFromDB();

    bool isHomepage = m_isHomepage->GetCheckState() == MythUIStateType::Full;
    InsertInDB(m_categoryEdit->GetText(), m_nameEdit->GetText(), m_urlEdit->GetText(), isHomepage );
    
    if (m_site)
    {
        m_site->m_category = m_categoryEdit->GetText();
        m_site->m_name = m_nameEdit->GetText();
        m_site->m_url = m_urlEdit->GetText();
        m_site->m_isHomepage = isHomepage;
    }

    Exit();
}

void BookmarkEditor::slotFindCategory(void)
{
    QStringList list;

    GetCategoryList(list);

    QString title = tr("Select a category");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    m_searchDialog = new MythUISearchDialog(popupStack, title, list,
                                            true, m_categoryEdit->GetText());

    if (!m_searchDialog->Create())
    {
        delete m_searchDialog;
        m_searchDialog = nullptr;
        return;
    }

    connect(m_searchDialog, &MythUISearchDialog::haveResult, this, &BookmarkEditor::slotCategoryFound);

    popupStack->AddScreen(m_searchDialog);
}

void BookmarkEditor::slotCategoryFound(const QString& category)
{
    m_categoryEdit->SetText(category);
}
