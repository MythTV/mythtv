
// myth
#include <mythmainwindow.h>
#include <mythdate.h>
#include <mythcontext.h>
#include <mythdbcon.h>

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
      m_site(site),               m_siteName(""),
      m_siteCategory(),           m_editing(edit),
      m_titleText(NULL),          m_categoryEdit(NULL),
      m_nameEdit(NULL),           m_urlEdit(NULL),
      m_isHomepage(NULL),
      m_okButton(NULL),           m_cancelButton(NULL),
      m_findCategoryButton(NULL), m_searchDialog(NULL)
{
    if (m_editing)
    {
        m_siteCategory = m_site->category;
        m_siteName = m_site->name;
    }
}

BookmarkEditor::~BookmarkEditor()
{
}

bool BookmarkEditor::Create()
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("browser-ui.xml", "bookmarkeditor", this);

    if (!foundtheme)
        return false;

    m_titleText = dynamic_cast<MythUIText *> (GetChild("title"));

    if (m_titleText)
    {
      if (m_editing)
          m_titleText->SetText(tr("Edit Bookmark Details"));
      else
          m_titleText->SetText(tr("Enter Bookmark Details"));
    }

    m_categoryEdit = dynamic_cast<MythUITextEdit *> (GetChild("category"));
    m_nameEdit = dynamic_cast<MythUITextEdit *> (GetChild("name"));
    m_urlEdit = dynamic_cast<MythUITextEdit *> (GetChild("url"));
    m_isHomepage = dynamic_cast<MythUICheckBox *> (GetChild("homepage"));

    m_okButton = dynamic_cast<MythUIButton *> (GetChild("ok"));
    m_cancelButton = dynamic_cast<MythUIButton *> (GetChild("cancel"));

    m_findCategoryButton = dynamic_cast<MythUIButton *> (GetChild("findcategory"));

    if (!m_categoryEdit || !m_nameEdit || !m_urlEdit || !m_isHomepage ||  !m_okButton
        || !m_cancelButton || !m_findCategoryButton)
    {
        LOG(VB_GENERAL, LOG_ERR, "Theme is missing critical theme elements.");
        return false;
    }

    connect(m_okButton, SIGNAL(Clicked()), this, SLOT(Save()));
    connect(m_cancelButton, SIGNAL(Clicked()), this, SLOT(Exit()));
    connect(m_findCategoryButton, SIGNAL(Clicked()), this, SLOT(slotFindCategory()));

    if (m_editing && m_site)
    {
        m_categoryEdit->SetText(m_site->category);
        m_nameEdit->SetText(m_site->name);
        m_urlEdit->SetText(m_site->url);

        if (m_site->isHomepage)
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

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("News", event, actions);

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

    bool isHomepage = (m_isHomepage->GetCheckState() == MythUIStateType::Full) ? true : false;
    InsertInDB(m_categoryEdit->GetText(), m_nameEdit->GetText(), m_urlEdit->GetText(), isHomepage );
    
    if (m_site)
    {
        m_site->category = m_categoryEdit->GetText();
        m_site->name = m_nameEdit->GetText();
        m_site->url = m_urlEdit->GetText();
        m_site->isHomepage = isHomepage;
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
        m_searchDialog = NULL;
        return;
    }

    connect(m_searchDialog, SIGNAL(haveResult(QString)), SLOT(slotCategoryFound(QString)));

    popupStack->AddScreen(m_searchDialog);
}

void BookmarkEditor::slotCategoryFound(QString category)
{
    m_categoryEdit->SetText(category);
}
