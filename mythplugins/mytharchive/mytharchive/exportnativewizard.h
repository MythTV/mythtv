#ifndef EXPORTNATIVEWIZARD_H_
#define EXPORTNATIVEWIZARD_H_

#include <mythtv/uitypes.h>
#include <mythtv/uilistbtntype.h>
#include <mythtv/dialogbox.h>

#include "archiveutil.h"

enum NativeItemType
{
    RT_RECORDING = 1,
    RT_VIDEO,
    RT_FILE
};

typedef struct
{
    int     id;
    QString type;
    QString title;
    QString subtitle;
    QString description;
    QString startDate;
    QString startTime;
    QString filename;
    long long size;
    bool hasCutlist;
    bool useCutlist;
    bool editedDetails;
} NativeItem;

class ExportNativeWizard : public MythThemedDialog
{

  Q_OBJECT

  public:
    ExportNativeWizard(MythMainWindow *parent, QString window_name,
                   QString theme_filename, const char *name = 0);

    ~ExportNativeWizard(void);

    void setSaveFilename(QString filename) {saveFilename = filename;}
    void createConfigFile(const QString &filename);

  public slots:

    void handleNextPage(void);
    void handlePrevPage(void);
    void handleCancel(void);

    void setCategory(int);
    void titleChanged(UIListBtnTypeItem *item);
    void toggleUseCutlist(bool state);
    void showMenu(void);
    void closePopupMenu(void);
    void removeItem(void);
    void toggleCreateISO(bool state) { bCreateISO = state; };
    void toggleDoBurn(bool state) { bDoBurn = state; };
    void toggleEraseDvdRw(bool state) { bEraseDvdRw = state; };
    void handleFind(void);
    void filenameEditLostFocus(void);
    void setDestination(int);

  private:
    ArchiveDestination archiveDestination;
    int destination_no;
    int freeSpace;
    int usedSpace;

    void keyPressEvent(QKeyEvent *e);
    void updateArchiveList(void);
    void toggleSelectedState(void);
    void getArchiveList(void);
    void wireUpTheme(void);
    void updateSizeBar(void);
    void loadConfiguration(void);
    void saveConfiguration(void);
    void updateSelectedArchiveList(void);
    vector<NativeItem *>  *getArchiveListFromDB(void);
    void runScript();

    vector<NativeItem *>  *archiveList;
    QPtrList<NativeItem> selectedList;

    UIListBtnType    *archive_list;
    UIListBtnType    *selected_list;

    bool             bCreateISO;
    bool             bDoBurn;
    bool             bEraseDvdRw;

    QString          saveFilename;

    UITextButtonType *next_button;
    UITextButtonType *prev_button;
    UITextButtonType *cancel_button;

    UISelectorType   *destination_selector;
    UITextType       *destination_text;

    UITextType       *freespace_text;

    UIRemoteEditType *filename_edit;
    UITextButtonType *find_button;

    UISelectorType   *category_selector;
    UITextType       *title_text;
    UITextType       *datetime_text;
    UITextType       *description_text;
    UITextType       *usecutlist_text;
    UICheckBoxType   *usecutlist_check;
    UITextType       *nocutlist_text;
    UITextType       *filesize_text;

    UIStatusBarType  *size_bar;

    UICheckBoxType   *createISO_check;
    UICheckBoxType   *doBurn_check;
    UICheckBoxType   *eraseDvdRw_check;
    UITextType       *createISO_text;
    UITextType       *doBurn_text;
    UITextType       *eraseDvdRw_text;

    MythPopupBox     *popupMenu;
};

#endif
