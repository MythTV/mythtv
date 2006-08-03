/*
	recordingselector.h

    header for the recording selector interface screen
*/

#ifndef RECORDINGSELECTOR_H_
#define RECORDINGSELECTOR_H_

#include <mythtv/uitypes.h>
#include <mythtv/uilistbtntype.h>
#include <mythtv/dialogbox.h>

class ProgramInfo;

class RecordingSelector : public MythThemedDialog
{

  Q_OBJECT

  public:
    RecordingSelector(MythMainWindow *parent, QString window_name,
                    QString theme_filename, const char *name = 0);

    ~RecordingSelector(void);

    void keyPressEvent(QKeyEvent *e);

  public slots:
    void OKPressed(void);
    void cancelPressed(void);

    void showMenu(void);
    void closePopupMenu(void);
    void selectAll(void);
    void clearAll(void);

    void setCategory(int);
    void titleChanged(UIListBtnTypeItem *item);

  private:
    QString themeDir; 
    void updateRecordingList(void);
    void updateSelectedList(void);
    void toggleSelectedState(void);
    void getRecordingList(void);
    void wireUpTheme(void);

    vector<ProgramInfo *>  *recordingList;
    QPtrList<ProgramInfo> selectedList;

    UIListBtnType    *recording_list;

    UITextButtonType *ok_button;
    UITextButtonType *cancel_button;

    UISelectorType   *category_selector;
    UITextType       *title_text;
    UITextType       *datetime_text;
    UITextType       *filesize_text;
    UITextType       *description_text;
    UIImageType      *preview_image;
    UIImageType      *cutlist_image;

    MythPopupBox     *popupMenu;
};

#endif


