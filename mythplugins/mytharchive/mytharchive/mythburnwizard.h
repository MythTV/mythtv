/*
	mythburnwizard.h

    header for the mythburn interface screen
*/

#ifndef MYTHBURNWIZARD_H_
#define MYTHBURNWIZARD_H_

#include <mythtv/uitypes.h>
#include <mythtv/uilistbtntype.h>
#include <mythtv/dialogbox.h>

#include "archiveutil.h"
//Added by qt3to4:
#include <QPixmap>
#include <QKeyEvent>

class MythburnWizard : public MythThemedDialog
{

  Q_OBJECT

  public:
    MythburnWizard(MythMainWindow *parent, QString window_name,
                   QString theme_filename, const char *name = 0);

    ~MythburnWizard(void);

    void keyPressEvent(QKeyEvent *e);
    void createConfigFile(const QString &filename);

    void setSaveFilename(QString filename) {saveFilename = filename;}

  public slots:
    void handleNextPage(void);
    void handlePrevPage(void);
    void handleCancel(void);
    void handleAddRecording(void);
    void handleAddVideo(void);
    void handleAddFile(void);

    void setTheme(int);
    void setProfile(int);
    void titleChanged(UIListBtnTypeItem *item);
    void selectedChanged(UIListBtnTypeItem *item);
    void toggleUseCutlist(bool state);
    void showMenu(void);
    void closePopupMenu(void);
    void editDetails(void);
    void removeItem(void);
    void handleFind(void);
    void filenameEditLostFocus(void);
    void setDestination(int);

    void toggleCreateISO(bool state) { bCreateISO = state; };
    void toggleDoBurn(bool state) { bDoBurn = state; };
    void toggleEraseDvdRw(bool state) { bEraseDvdRw = state; };

  private:
    void getThemeList(void);
    void updateArchiveList(void);
    void getArchiveList(void);
    void wireUpTheme(void);
    void updateSizeBar();
    void loadConfiguration(void);
    void saveConfiguration(void);
    void updateSelectedArchiveList(void);
    void toggleReorderState(void);
    void reloadSelectedList(void);
    void showEditMetadataDialog();
    QString loadFile(const QString &filename);
    void getArchiveListFromDB(void);
    bool doRemoveArchiveItem(const QString &filename);
    bool isArchiveItemValid(const QString &type, const QString &filename);
    bool hasCutList(QString &type, QString &filename);
    void loadEncoderProfiles(void);
    EncoderProfile *getDefaultProfile(ArchiveItem *item);
    long long recalcSize(EncoderProfile *profile, ArchiveItem *a);
    void recalcItemSize(ArchiveItem *item);
    void setProfile(EncoderProfile *profile, ArchiveItem *item);
    void runScript();

    ArchiveDestination archiveDestination;
    int destination_no;
    QString themeDir; 
    int freeSpace;
    int usedSpace;

    vector<ArchiveItem *>    *archiveList;
    vector<EncoderProfile *> *profileList;

    UISelectorType   *destination_selector;
    UITextType       *destination_text;

    UITextType       *freespace_text;

    UIRemoteEditType *filename_edit;
    UITextButtonType *find_button;

    UISelectorType *theme_selector;
    UIImageType    *theme_image;
    int             theme_no;
    QStringList     theme_list;

    UIImageType    *intro_image;
    UIImageType    *mainmenu_image;
    UIImageType    *chapter_image;
    UIImageType    *details_image;
    UITextType     *themedesc_text;

    UIListBtnType  *archive_list;
    UIListBtnType  *selected_list;

    // selected list reordering state
    bool            bReordering;
    QPixmap        *movePixmap;

    bool            bCreateISO;
    bool            bDoBurn;
    bool            bEraseDvdRw;

    QString         saveFilename;

    UITextButtonType *next_button;
    UITextButtonType *prev_button;
    UITextButtonType *cancel_button;

    UITextType       *title_text;
    UITextType       *datetime_text;
    UITextType       *description_text;
    UITextType       *usecutlist_text;
    UICheckBoxType   *usecutlist_check;
    UITextType       *nocutlist_text;
    UITextType       *filesize_text;
    UITextType       *nofiles_text;
    UITextButtonType *addrecording_button;
    UITextButtonType *addvideo_button;
    UITextButtonType *addfile_button;

    // size bar
    UIStatusBarType  *size_bar;
    UITextType       *maxsize_text;
    UITextType       *minsize_text;
    UITextType       *currentsize_error_text;
    UITextType       *currentsize_text;

    // profile
    UISelectorType   *profile_selector;
    UITextType       *profile_text;
    UITextType       *oldsize_text;
    UITextType       *newsize_text;

    UICheckBoxType   *createISO_check;
    UICheckBoxType   *doBurn_check;
    UICheckBoxType   *eraseDvdRw_check;
    UITextType       *createISO_text;
    UITextType       *doBurn_text;
    UITextType       *eraseDvdRw_text;

    MythPopupBox     *popupMenu;
};

#endif


