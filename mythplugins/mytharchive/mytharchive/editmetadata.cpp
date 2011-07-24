
// myth
#include <mythcontext.h>
#include <mythuitext.h>
#include <mythuibutton.h>
#include <mythuitextedit.h>

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

    bool err = false;
    UIUtilE::Assign(this, m_titleEdit, "title_edit", &err);
    UIUtilE::Assign(this, m_subtitleEdit, "subtitle_edit", &err);
    UIUtilE::Assign(this, m_descriptionEdit, "description_edit", &err);
    UIUtilE::Assign(this, m_starttimeEdit, "starttime_edit", &err);
    UIUtilE::Assign(this, m_startdateEdit, "startdate_edit", &err);
    UIUtilE::Assign(this, m_okButton, "ok_button", &err);
    UIUtilE::Assign(this, m_cancelButton, "cancel_button", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'edit_metadata'");
        return false;
    }

    connect(m_okButton, SIGNAL(Clicked()), this, SLOT(okPressed()));
    connect(m_cancelButton, SIGNAL(Clicked()), this, SLOT(cancelPressed()));

    m_titleEdit->SetText(m_sourceMetadata->title);
    m_subtitleEdit->SetText(m_sourceMetadata->subtitle);
    m_descriptionEdit->SetText(m_sourceMetadata->description);
    m_startdateEdit->SetText(m_sourceMetadata->startDate);
    m_starttimeEdit->SetText(m_sourceMetadata->startTime);

    BuildFocusList();

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
