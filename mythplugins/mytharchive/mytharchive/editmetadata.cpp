
// myth
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>

// mytharchive
#include "editmetadata.h"

EditMetadataDialog::EditMetadataDialog(ArchiveItem *source_metadata,
                                 MythMainWindow *parent,
                                 QString window_name,
                                 QString theme_filename,
                                 const char* name)
                :MythThemedDialog(parent, window_name, theme_filename, name)
{
    // make a copy so we can abandon changes
    workMetadata = *source_metadata;
    sourceMetadata = source_metadata;
    wireUpTheme();
    fillWidgets();
    assignFirstFocus();
}

void EditMetadataDialog::fillWidgets()
{
    if (title_edit)
    {
        title_edit->setText(workMetadata.title);
    }

    if (subtitle_edit)
    {
        subtitle_edit->setText(workMetadata.subtitle);
    }

    if (description_edit)
    {
        description_edit->setText(workMetadata.description);
    }

    if (startdate_edit)
    {
        startdate_edit->setText(workMetadata.startDate);
    }

    if (starttime_edit)
    {
        starttime_edit->setText(workMetadata.startTime);
    }
}

void EditMetadataDialog::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;

    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Global", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "UP")
            nextPrevWidgetFocus(false);
        else if (action == "DOWN")
            nextPrevWidgetFocus(true);
        else if (action == "LEFT")
        {

        }
        else if (action == "RIGHT")
        {

        }
        else if (action == "SELECT")
        {
            activateCurrent();
        }
        else if (action == "0")
        {
            if (ok_button)
                ok_button->push();
        }
        else if (action == "1")
        {
            if (cancel_button)
                cancel_button->push();
        }
        else
            handled = false;
    }

    if (!handled)
        MythThemedDialog::keyPressEvent(e);
}

void EditMetadataDialog::wireUpTheme()
{
    title_edit = getUIRemoteEditType("title_edit");
    if (title_edit)
    {
        title_edit->createEdit(this);
        connect(title_edit, SIGNAL(loosingFocus()), this, SLOT(editLostFocus()));
    }

    subtitle_edit = getUIRemoteEditType("subtitle_edit");
    if (subtitle_edit)
    {
        subtitle_edit->createEdit(this);
        connect(subtitle_edit, SIGNAL(loosingFocus()), this, SLOT(editLostFocus()));
    }

    description_edit = getUIRemoteEditType("description_edit");
    if (description_edit)
    {
        description_edit->createEdit(this);
        MythRemoteLineEdit *edit = (MythRemoteLineEdit *) description_edit->getEdit();
        if (edit)
            edit->setWordWrap(QTextEdit::WidgetWidth); 
        connect(description_edit, SIGNAL(loosingFocus()), this, SLOT(editLostFocus()));
    }

    startdate_edit = getUIRemoteEditType("startdate_edit");
    if (startdate_edit)
    {
        startdate_edit->createEdit(this);
        connect(startdate_edit, SIGNAL(loosingFocus()), this, SLOT(editLostFocus()));
    }

    starttime_edit = getUIRemoteEditType("starttime_edit");
    if (starttime_edit)
    {
        starttime_edit->createEdit(this);
        connect(starttime_edit, SIGNAL(loosingFocus()), this, SLOT(editLostFocus()));
    }

    ok_button = getUITextButtonType("ok_button");
    if (ok_button)
    {
        ok_button->setText(tr("Save"));
        connect(ok_button, SIGNAL(pushed()), this, SLOT(savePressed()));
    }

    cancel_button = getUITextButtonType("cancel_button");
    if (cancel_button)
    {
        cancel_button->setText(tr("Cancel"));
        connect(cancel_button, SIGNAL(pushed()), this, SLOT(cancelPressed()));
    }

    buildFocusList();
}

void EditMetadataDialog::editLostFocus()
{
    UIRemoteEditType *whichEditor = (UIRemoteEditType *) getCurrentFocusWidget();

    if (whichEditor == title_edit)
    {
        workMetadata.title = title_edit->getText();
    }
    else if (whichEditor == subtitle_edit)
    {
        workMetadata.subtitle = subtitle_edit->getText();
    }
    else if (whichEditor == startdate_edit)
    {
        workMetadata.startDate = startdate_edit->getText();
    }
    else if (whichEditor == starttime_edit)
    {
        workMetadata.startTime = starttime_edit->getText();
    }
    else if (whichEditor == description_edit)
    {
        workMetadata.description = description_edit->getText();
    }
}

void EditMetadataDialog::savePressed()
{
    *sourceMetadata = workMetadata;
    sourceMetadata->editedDetails = true;
    done(1);
}

void EditMetadataDialog::cancelPressed()
{
    done(Rejected);
}

EditMetadataDialog::~EditMetadataDialog()
{
}
