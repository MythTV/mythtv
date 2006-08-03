#ifndef EDITMETADATA_H_
#define EDITMETADATA_H_

#include <iostream>

// myth
#include <mythtv/mythdialogs.h>

// mytharchive
#include "mythburnwizard.h"

class EditMetadataDialog : public MythThemedDialog
{

  Q_OBJECT

  public:

    EditMetadataDialog(ArchiveItem *source_metadata,
                       MythMainWindow *parent,
                       QString window_name,
                       QString theme_filename,
                       const char* name = 0);
    ~EditMetadataDialog();

    void keyPressEvent(QKeyEvent *e);
    void wireUpTheme();
    void fillWidgets();

  public slots:

    void closeDialog();
    void showSaveMenu();
    void savePressed();
    void cancelPressed();
    void editLostFocus();

  private:
    ArchiveItem *sourceMetadata;
    ArchiveItem workMetadata;

    //
    //  GUI stuff
    //

    UIRemoteEditType    *title_edit;
    UIRemoteEditType    *subtitle_edit;
    UIRemoteEditType    *description_edit;
    UIRemoteEditType    *startdate_edit;
    UIRemoteEditType    *starttime_edit;

    UITextButtonType    *cancel_button;
    UITextButtonType    *ok_button;
};

#endif
