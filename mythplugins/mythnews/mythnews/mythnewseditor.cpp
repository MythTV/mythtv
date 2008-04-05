// QT headers

// MythTV headers
#include <mythtv/libmythui/mythmainwindow.h>
#include <mythtv/libmythui/mythdialogbox.h>
#include <mythtv/util.h>
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>

// MythNews headers
#include "mythnewseditor.h"

/** \brief Creates a new MythNewsEditor Screen
 *  \param parent Pointer to the screen stack
 *  \param name The name of the window
 */
MythNewsEditor::MythNewsEditor(NewsSite *site, bool edit,
                               MythScreenStack *parent, const char *name)
    : MythScreenType (parent, name)
{

    m_titleText = m_nameLabelText = m_urlLabelText = m_iconLabelText = NULL;
    m_nameEdit = m_urlEdit = m_iconEdit = NULL;

    m_site = site;
    m_siteName = m_site->name();
    m_editing = edit;
}

MythNewsEditor::~MythNewsEditor()
{
}

bool MythNewsEditor::Create()
{

    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("news-ui.xml", "editor", this);

    if (!foundtheme)
        return false;

    m_titleText = dynamic_cast<MythUIText *>
                (GetChild("title"));

    if (m_titleText)
    {
      if (m_editing)
          m_titleText->SetText(tr("Edit Site Details"));
      else
          m_titleText->SetText(tr("Enter Site Details"));
    }

    m_nameLabelText = dynamic_cast<MythUIText *>
                (GetChild("namelabel"));

    if (m_nameLabelText)
        m_nameLabelText->SetText(tr("Name:"));

    m_nameEdit = dynamic_cast<MythUITextEdit *>
                (GetChild("name"));

    m_urlLabelText = dynamic_cast<MythUIText *>
                (GetChild("urllabel"));

    if (m_urlLabelText)
        m_urlLabelText->SetText(tr("URL:"));

    m_urlEdit = dynamic_cast<MythUITextEdit *>
                (GetChild("url"));

    m_iconLabelText = dynamic_cast<MythUIText *>
                (GetChild("iconlabel"));

    if (m_iconLabelText)
        m_iconLabelText->SetText(tr("Icon:"));

    m_iconEdit = dynamic_cast<MythUITextEdit *>
                (GetChild("icon"));

    if (!m_nameEdit || !m_urlEdit || !m_iconEdit)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical theme elements.");
        return false;
    }

    if (m_editing)
    {
        m_nameEdit->SetText(m_site->name());
        m_urlEdit->SetText(m_site->url());
        m_iconEdit->SetText(m_site->imageURL());
    }

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist. Something is wrong");

    SetFocusWidget(m_nameEdit);

    return true;
}

bool MythNewsEditor::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("News", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "LEFT")
            NextPrevWidgetFocus(false);
        else if (action == "RIGHT")
            NextPrevWidgetFocus(true);
        else if (action == "ESCAPE")
            GetMythMainWindow()->GetMainStack()->PopScreen();
        else
            handled = false;
    }

    return handled;
}

void MythNewsEditor::Save()
{
    QDateTime time;

    if (m_editing && m_siteName != "")
        removeFromDB(m_siteName);

    insertInDB(m_nameEdit->GetText(), m_urlEdit->GetText(),
               m_iconEdit->GetText(), "custom");
}
