/*
	mytharchivewizard.h

    header for the mytharchive interface screen
*/

#ifndef MYTHARCHIVEWIZARD_H_
#define MYTHARCHIVEWIZARD_H_

#include <mythtv/uitypes.h>
#include <mythtv/uilistbtntype.h>
#include <mythtv/dialogbox.h>

#include "advancedoptions.h"

typedef struct ArchiveFormat
{
    QString name;
    QString description;
}_ArchiveFormat;

extern struct ArchiveFormat ArchiveFormats[];
extern int ArchiveFormatsCount;

typedef enum
{
    AD_DVD_SL = 0,
    AD_DVD_DL = 1,
    AD_DVD_RW = 2,
    AD_FILE   = 3
} ARCHIVEDESTINATION;

typedef struct ArchiveDestination
{
    ARCHIVEDESTINATION type;
    QString name;
    QString description;
    long long freeSpace;
}_ArchiveDestination;

extern struct ArchiveDestination ArchiveDestinations[];
extern int ArchiveDestinationsCount;

QString formatSize(long long sizeKB, int prec = 1);

class MythArchiveWizard : public MythThemedDialog
{

  Q_OBJECT

  public:
    MythArchiveWizard(MythMainWindow *parent, QString window_name,
                    QString theme_filename, const char *name = 0);

    ~MythArchiveWizard(void);

    void keyPressEvent(QKeyEvent *e);

  public slots:
    void handleNextPage(void);
    void handlePrevPage(void);
    void handleCancel(void);
    void handleFind(void);
    void filenameEditLostFocus(void);

    void setFormat(int);
    void setDestination(int);

    void advancedPressed();

  private:
    int format_no;
    int destination_no;
    void wireUpTheme(void);
    void loadConfiguration(void);
    void saveConfiguration(void);
    void runMythBurnWizard(void);
    void runMythNativeWizard(void);

    UISelectorType   *format_selector;
    UITextType       *format_text;
    UISelectorType   *destination_selector;
    UITextType       *destination_text;

    UICheckBoxType   *erase_check;
    UITextType       *erase_text;

    UITextType       *freespace_text;

    UIRemoteEditType *filename_edit;
    UITextButtonType *find_button;
    UITextButtonType *advanced_button;

    UITextButtonType *next_button;
    UITextButtonType *prev_button;
    UITextButtonType *cancel_button;
};

#endif


