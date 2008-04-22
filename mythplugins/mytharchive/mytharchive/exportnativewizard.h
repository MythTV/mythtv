#ifndef EXPORTNATIVEWIZARD_H_
#define EXPORTNATIVEWIZARD_H_

// qt
#include <QKeyEvent>

// mythtv
#include <mythtv/uitypes.h>
#include <mythtv/uilistbtntype.h>
#include <mythtv/dialogbox.h>

// mytharchive
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
    void handleAddRecording(void);
    void handleAddVideo(void);

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
    void getArchiveList(void);
    void wireUpTheme(void);
    void updateSizeBar(void);
    void loadConfiguration(void);
    void saveConfiguration(void);
    void getArchiveListFromDB(void);
    void runScript();

    vector<NativeItem *>  *archiveList;
    UIListBtnType         *archive_list;

    bool             bCreateISO;
    bool             bDoBurn;
    bool             bEraseDvdRw;

    QString          saveFilename;

    UITextButtonType *next_button;
    UITextButtonType *prev_button;
    UITextButtonType *cancel_button;

    UITextButtonType *addrecording_button;
    UITextButtonType *addvideo_button;

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
    UITextType       *nofiles_text;

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
