
// myth
#include <mythtv/mythcontext.h>
#include <libmythui/mythuitext.h>
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuitextedit.h>

// mytharchive
#include "editmetadata.h"

EditMetadataDialog::EditMetadataDialog(MythScreenStack *parent, 
                                       ArchiveItem *source_metadata)
                   :MythScreenType(parent, "EditMetadataDialog")
{
    m_sourceMetadata = source_metadata;
}

bool EditMetadataDialog::Create(void)
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("mythburn-ui.xml", "edit_metadata", this);

    if (!foundtheme)
        return false;

    try
    {
        m_titleEdit = GetMythUITextEdit("title_edit");
        m_subtitleEdit = GetMythUITextEdit("subtitle_edit");
        m_descriptionEdit = GetMythUITextEdit("description_edit");
        m_starttimeEdit = GetMythUITextEdit("starttime_edit");
        m_startdateEdit = GetMythUITextEdit("startdate_edit");
        m_okButton = GetMythUIButton("ok_button");
        m_cancelButton = GetMythUIButton("cancel_button");
    }
    catch (QString &error)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'edit_metadata'\n\t\t\t"
                              "Error was: " + error);
        return false;
    }

    m_okButton->SetText(tr("OK"));
    connect(m_okButton, SIGNAL(Clicked()), this, SLOT(okPressed()));

    m_cancelButton->SetText(tr("Cancel"));
    connect(m_cancelButton, SIGNAL(Clicked()), this, SLOT(cancelPressed()));

    m_titleEdit->SetText(m_sourceMetadata->title);
    m_subtitleEdit->SetText(m_sourceMetadata->subtitle);
    m_descriptionEdit->SetText(m_sourceMetadata->description);
    m_startdateEdit->SetText(m_sourceMetadata->startDate);
    m_starttimeEdit->SetText(m_sourceMetadata->startTime);

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist. Something is wrong");

    SetFocusWidget(m_titleEdit);

    return true;
}

bool EditMetadataDialog::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    if (MythScreenType::keyPressEvent(event))
        return true;

    return false;
}

void EditMetadataDialog::okPressed(void)
{
    m_sourceMetadata->title = m_titleEdit->GetText();
    m_sourceMetadata->subtitle = m_subtitleEdit->GetText();
    m_sourceMetadata->startDate = m_startdateEdit->GetText();
    m_sourceMetadata->startTime = m_starttimeEdit->GetText();
    m_sourceMetadata->description = m_descriptionEdit->GetText();
    m_sourceMetadata->editedDetails = true;

    emit haveResult(true, m_sourceMetadata);
    Close();
}

void EditMetadataDialog::cancelPressed(void)
{
    emit haveResult(false, m_sourceMetadata);
    Close();
}

EditMetadataDialog::~EditMetadataDialog()
{
}
