#ifndef MYTHBURN_H_
#define MYTHBURN_H_

// mythtv
#include <mythscreentype.h>

// mytharchive
#include "archiveutil.h"

class MythUIText;
class MythUIButton;
class MythUICheckBox;
class MythUIButtonList;
class MythUIProgressBar;
class MythUIButtonListItem;

class ProfileDialog : public MythScreenType
{
    Q_OBJECT

  public:
    ProfileDialog(MythScreenStack *parent, ArchiveItem *archiveItem,
                  QList<EncoderProfile *> profileList);

    bool Create();

  signals:
    void haveResult(int profile);

  private slots:
    void save(void);
    void profileChanged(MythUIButtonListItem *item);

  private:
    ArchiveItem            *m_archiveItem;
    QList<EncoderProfile *> m_profileList;

    MythUIText       *m_captionText;
    MythUIText       *m_descriptionText;
    MythUIText       *m_oldSizeText;
    MythUIText       *m_newSizeText;

    MythUIButtonList *m_profile_list;
    MythUICheckBox   *m_enabledCheck;
    MythUIButton     *m_okButton;
};

class MythBurn : public MythScreenType
{

  Q_OBJECT

  public:
    MythBurn(MythScreenStack *parent, 
             MythScreenType *destinationScreen, MythScreenType *themeScreen,
             ArchiveDestination archiveDestination, QString name);

    ~MythBurn(void);

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

    void createConfigFile(const QString &filename);

  protected slots:
    void handleNextPage(void);
    void handlePrevPage(void);
    void handleCancel(void);
    void handleAddRecording(void);
    void handleAddVideo(void);
    void handleAddFile(void);

    void toggleUseCutlist(void);
    void showMenu(void);
    void editDetails(void);
    void editThumbnails(void);
    void changeProfile(void);
    void profileChanged(int profileNo);
    void removeItem(void);
    void selectorClosed(bool ok);
    void editorClosed(bool ok, ArchiveItem *item);
    void itemClicked(MythUIButtonListItem *item);

  private:
    void updateArchiveList(void);
    void updateSizeBar();
    void loadConfiguration(void);
    void saveConfiguration(void);
    EncoderProfile *getProfileFromName(const QString &profileName);
    QString loadFile(const QString &filename);
    bool isArchiveItemValid(const QString &type, const QString &filename);
    void loadEncoderProfiles(void);
    EncoderProfile *getDefaultProfile(ArchiveItem *item);
    void setProfile(EncoderProfile *profile, ArchiveItem *item);
    void runScript();

    MythScreenType         *m_destinationScreen;
    MythScreenType         *m_themeScreen;
    ArchiveDestination      m_archiveDestination;

    QList<ArchiveItem *>    m_archiveList;
    QList<EncoderProfile *> m_profileList;

    bool              m_bCreateISO;
    bool              m_bDoBurn;
    bool              m_bEraseDvdRw;
    QString           m_saveFilename;
    QString           m_theme;

    bool              m_moveMode;

    MythUIButton     *m_nextButton;
    MythUIButton     *m_prevButton;
    MythUIButton     *m_cancelButton;

    MythUIButtonList *m_archiveButtonList;
    MythUIText       *m_nofilesText;
    MythUIButton     *m_addrecordingButton;
    MythUIButton     *m_addvideoButton;
    MythUIButton     *m_addfileButton;

    // size bar
    MythUIProgressBar *m_sizeBar;
    MythUIText        *m_maxsizeText;
    MythUIText        *m_minsizeText;
    MythUIText        *m_currentsizeErrorText;
    MythUIText        *m_currentsizeText;
};

///////////////////////////////////////////////////////////////////////////////

class BurnMenu : public QObject
{
    Q_OBJECT

  public:
    BurnMenu(void);
    ~BurnMenu(void);

    void start(void);

  private:
    void customEvent(QEvent *event);
    void doBurn(int mode);
};

#endif


