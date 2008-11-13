
// MythTV headers
#include <mythuibutton.h>
#include <mythuitext.h>
#include <mythuitextedit.h>
#include <mythuicheckbox.h>
#include <mythmainwindow.h>
#include <mythdialogbox.h>
#include <util.h>
#include <mythcontext.h>
#include <mythdbcon.h>

// MythNews headers
#include "mythnewseditor.h"
#include "newsdbutil.h"
#include "newssite.h"

#define LOC      QString("MythNewsEditor: ")
#define LOC_WARN QString("MythNewsEditor, Warning: ")
#define LOC_ERR  QString("MythNewsEditor, Error: ")

/** \brief Creates a new MythNewsEditor Screen
 *  \param parent Pointer to the screen stack
 *  \param name The name of the window
 */
MythNewsEditor::MythNewsEditor(NewsSite *site, bool edit,
                               MythScreenStack *parent,
                               const QString name) :
    MythScreenType(parent, name),
    m_lock(QMutex::Recursive),
    m_site(site),          m_siteName(QString::null),
    m_editing(edit),
    m_titleText(NULL),     m_nameLabelText(NULL),
    m_urlLabelText(NULL),  m_iconLabelText(NULL),
    m_nameEdit(NULL),      m_urlEdit(NULL),
    m_iconEdit(NULL),
    m_okButton(NULL),      m_cancelButton(NULL),
    m_podcastCheck(NULL)
{
    if (m_editing)
        m_siteName = m_site->name();
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

    m_titleText = dynamic_cast<MythUIText *> (GetChild("title"));

    if (m_titleText)
    {
        m_titleText->SetText(
            (m_editing) ? tr("Edit Site Details") : tr("Enter Site Details"));
    }

    m_nameLabelText = dynamic_cast<MythUIText *> (GetChild("namelabel"));
    m_urlLabelText = dynamic_cast<MythUIText *> (GetChild("urllabel"));
    m_iconLabelText = dynamic_cast<MythUIText *> (GetChild("iconlabel"));
    m_podcastLabelText = dynamic_cast<MythUIText *> (GetChild("podcastlabel"));

    m_nameEdit = dynamic_cast<MythUITextEdit *> (GetChild("name"));
    m_urlEdit = dynamic_cast<MythUITextEdit *> (GetChild("url"));
    m_iconEdit = dynamic_cast<MythUITextEdit *> (GetChild("icon"));

    m_podcastCheck = dynamic_cast<MythUICheckBox *> (GetChild("podcast_check"));

    m_okButton = dynamic_cast<MythUIButton *> (GetChild("ok"));
    m_cancelButton = dynamic_cast<MythUIButton *> (GetChild("cancel"));

    if (!m_nameEdit || !m_urlEdit || !m_iconEdit || !m_okButton ||
        !m_cancelButton || !m_podcastCheck)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Theme is missing critical theme elements.");

        return false;
    }

    if (m_nameLabelText)
        m_nameLabelText->SetText(tr("Name:"));
    if (m_urlLabelText)
        m_urlLabelText->SetText(tr("URL:"));
    if (m_iconLabelText)
        m_iconLabelText->SetText(tr("Icon:"));
    if (m_podcastLabelText)
        m_podcastLabelText->SetText(tr("Podcast:"));

    m_okButton->SetText(tr("Ok"));
    m_cancelButton->SetText(tr("Cancel"));

    connect(m_okButton, SIGNAL(Clicked()), this, SLOT(Save()));
    connect(m_cancelButton, SIGNAL(Clicked()), this, SLOT(Close()));

    if (m_editing)
    {
        m_nameEdit->SetText(m_site->name());
        m_urlEdit->SetText(m_site->url());
        m_iconEdit->SetText(m_site->imageURL());
        if (m_site->podcast() == 1)
           m_podcastCheck->SetCheckState(MythUIStateType::Full);
    }

    if (!BuildFocusList())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Failed to build a focuslist. Something is wrong");
    }

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
                   m_podcastCheck->GetCheckState());
    }
    Close();
}
