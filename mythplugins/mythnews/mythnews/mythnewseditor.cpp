
// MythTV headers
#include <libmyth/mythcontext.h>
#include <libmythbase/mythdate.h>
#include <libmythbase/mythdbcon.h>
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythmainwindow.h>
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuicheckbox.h>
#include <libmythui/mythuitext.h>
#include <libmythui/mythuitextedit.h>

// MythNews headers
#include "mythnewseditor.h"
#include "newsdbutil.h"
#include "newssite.h"

#define LOC      QString("MythNewsEditor: ")
#define LOC_WARN QString("MythNewsEditor, Warning: ")
#define LOC_ERR  QString("MythNewsEditor, Error: ")

/** \brief Creates a new MythNewsEditor Screen
 *  \param site The web page for which an entry is being created.
 *  \param edit If true, then editing an existing entry instead of
 *              creating a new entry.
 *  \param parent Pointer to the screen stack
 *  \param name The name of the window
 */
MythNewsEditor::MythNewsEditor(NewsSite *site, bool edit,
                               MythScreenStack *parent,
                               const QString &name) :
    MythScreenType(parent, name),
    m_site(site),
    m_siteName((edit && site) ? site->name() : QString()),
    m_editing(edit)
{
}

MythNewsEditor::~MythNewsEditor()
{
    QMutexLocker locker(&m_lock);
}

bool MythNewsEditor::Create(void)
{
    QMutexLocker locker(&m_lock);

    // Load the theme for this screen
    bool foundtheme = LoadWindowFromXML("news-ui.xml", "editor", this);

    if (!foundtheme)
        return false;

    bool err = false;
    UIUtilW::Assign(this, m_titleText, "title", &err);
    UIUtilW::Assign(this, m_nameLabelText, "namelabel", &err);
    UIUtilW::Assign(this, m_urlLabelText, "urllabel", &err);
    UIUtilW::Assign(this, m_iconLabelText, "iconlabel", &err);
    UIUtilW::Assign(this, m_podcastLabelText, "podcastlabel", &err);
    UIUtilE::Assign(this, m_nameEdit, "name", &err);
    UIUtilE::Assign(this, m_urlEdit, "url", &err);
    UIUtilE::Assign(this, m_iconEdit, "icon", &err);
    UIUtilE::Assign(this, m_podcastCheck, "podcast_check", &err);
    UIUtilE::Assign(this, m_okButton, "ok", &err);
    UIUtilE::Assign(this, m_cancelButton, "cancel", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'editor'");
        return false;
    }

    if (m_titleText)
    {
        m_titleText->SetText(
            (m_editing) ? tr("Edit Site Details") : tr("Enter Site Details"));
    }

    if (m_nameLabelText)
        m_nameLabelText->SetText(tr("Name:"));
    if (m_urlLabelText)
        m_urlLabelText->SetText(tr("URL:"));
    if (m_iconLabelText)
        m_iconLabelText->SetText(tr("Icon:"));
    if (m_podcastLabelText)
        m_podcastLabelText->SetText(tr("Podcast:"));

    m_okButton->SetText(tr("OK"));
    m_cancelButton->SetText(tr("Cancel"));

    connect(m_okButton, &MythUIButton::Clicked, this, &MythNewsEditor::Save);
    connect(m_cancelButton, &MythUIButton::Clicked, this, &MythScreenType::Close);

    if (m_editing)
    {
        m_nameEdit->SetText(m_site->name());
        m_urlEdit->SetText(m_site->url());
        m_iconEdit->SetText(m_site->imageURL());
        if (m_site->podcast())
           m_podcastCheck->SetCheckState(MythUIStateType::Full);
    }

    BuildFocusList();

    SetFocusWidget(m_nameEdit);

    return true;
}

bool MythNewsEditor::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("News", event, actions);

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void MythNewsEditor::Save(void)
{
    {
        QMutexLocker locker(&m_lock);

        if (m_editing && !m_siteName.isEmpty())
            removeFromDB(m_siteName);

        insertInDB(m_nameEdit->GetText(), m_urlEdit->GetText(),
                   m_iconEdit->GetText(), "custom",
                   (m_podcastCheck->GetCheckState() == MythUIStateType::Full));
    }
    Close();
}
