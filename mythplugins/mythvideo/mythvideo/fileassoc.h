#ifndef FILEASSOC_H_
#define FILEASSOC_H_

/*
    fileassoc.h

    (c) 2003 Thor Sigvaldason, Isaac Richards, and ?? ??
    Part of the mythTV project

    Classes to manipulate the file associations stored
    in the videotypes table (in the mythconverg database)

*/

#include <qptrlist.h>

#include <mythtv/mythdialogs.h>

class FileAssociation;

class FileAssocDialog : public MythThemedDialog
{

  Q_OBJECT

    //
    //  Dialog to manipulate the data
    //

  public:

    FileAssocDialog(MythMainWindow *parent_,
                    QString window_name,
                    QString theme_filename,
                    const char *name_ = 0);
    ~FileAssocDialog();

    void keyPressEvent(QKeyEvent *e);
    void loadFileAssociations();
    void saveFileAssociations();
    void showCurrentFA();
    void wireUpTheme();

  public slots:

    void takeFocusAwayFromEditor(bool up_or_down);
    void switchToFA(int which_one);
    void saveAndExit();
    void toggleDefault(bool on_or_off);
    void toggleIgnore(bool on_or_off);
    void setPlayerCommand(QString);
    void deleteCurrent();
    void makeNewExtension();
    void createExtension();
    void removeExtensionPopup();

  private:

    QPtrList<FileAssociation>   file_associations;
    FileAssociation             *current_fa;

    //
    //  GUI stuff
    //

    MythRemoteLineEdit  *command_editor;
    UISelectorType      *extension_select;
    UIBlackHoleType     *command_hack;
    UICheckBoxType      *default_check;
    UICheckBoxType      *ignore_check;
    UITextButtonType    *done_button;
    UITextButtonType    *new_button;
    UITextButtonType    *delete_button;

    //
    //  Stuff for new extension
    //

    MythPopupBox        *new_extension_popup;
    MythRemoteLineEdit  *new_extension_editor;
};

#endif
