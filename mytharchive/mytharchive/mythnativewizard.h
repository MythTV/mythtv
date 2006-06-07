#ifndef MYTHNATIVEWIZARD_H_
#define MYTHBURNWIZARD_H_

#include <mythtv/uitypes.h>
#include <mythtv/uilistbtntype.h>
#include <mythtv/dialogbox.h>
#include <mythtv/libmythtv/programinfo.h>

#include "mytharchivewizard.h"

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

class MythNativeWizard : public MythThemedDialog
{

  Q_OBJECT

  public:
    MythNativeWizard(ArchiveDestination destination,
                   MythMainWindow *parent, QString window_name,
                   QString theme_filename, const char *name = 0);

    ~MythNativeWizard(void);

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
    void editDetails(void);
    void useSLDVD(void);
    void useDLDVD(void);
    void removeItem(void);
    void advancedPressed();
    void toggleCreateISO(bool state) { bCreateISO = state; };
    void toggleDoBurn(bool state) { bDoBurn = state; };
    void toggleEraseDvdRw(bool state) { bEraseDvdRw = state; };

  private:
    ArchiveDestination archiveDestination;
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
    void showEditMetadataDialog();
    bool extractDetailsFromFilename(const QString &filename, QString &chanID, QString &startTime);
    QString loadFile(const QString &filename);
    vector<NativeItem *>  *getArchiveListFromDB(void);

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

    MythPopupBox     *popupMenu;
};

#endif
