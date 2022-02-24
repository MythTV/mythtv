#ifndef MYTHBURN_H_
#define MYTHBURN_H_

#include <utility>

// mythtv
#include <libmythui/mythscreentype.h>

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
                  QList<EncoderProfile *> profileList)
         : MythScreenType(parent, "functionpopup"),
           m_archiveItem(archiveItem),
           m_profileList(std::move(profileList)) {}
    bool Create() override; // MythScreenType

  signals:
    void haveResult(int profile);

  private slots:
    void save(void);
    void profileChanged(MythUIButtonListItem *item);

  private:
    ArchiveItem            *m_archiveItem     {nullptr};
    QList<EncoderProfile *> m_profileList;

    MythUIText             *m_captionText     {nullptr};
    MythUIText             *m_descriptionText {nullptr};
    MythUIText             *m_oldSizeText     {nullptr};
    MythUIText             *m_newSizeText     {nullptr};

    MythUIButtonList       *m_profileBtnList  {nullptr};
    MythUICheckBox         *m_enabledCheck    {nullptr};
    MythUIButton           *m_okButton        {nullptr};
};

class MythBurn : public MythScreenType
{

  Q_OBJECT

  public:
    MythBurn(MythScreenStack *parent, 
             MythScreenType *destinationScreen, MythScreenType *themeScreen,
             const ArchiveDestination &archiveDestination, const QString& name);

    ~MythBurn(void) override;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType

    void createConfigFile(const QString &filename);

  protected slots:
    void handleNextPage(void);
    void handlePrevPage(void);
    void handleCancel(void);
    void handleAddRecording(void);
    void handleAddVideo(void);
    void handleAddFile(void);

    void toggleUseCutlist(void);
    void ShowMenu(void) override; // MythScreenType
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
    static QString loadFile(const QString &filename);
    static bool isArchiveItemValid(const QString &type, const QString &filename);
    void loadEncoderProfiles(void);
    EncoderProfile *getDefaultProfile(ArchiveItem *item);
    void setProfile(EncoderProfile *profile, ArchiveItem *item);
    void runScript();

    MythScreenType         *m_destinationScreen {nullptr};
    MythScreenType         *m_themeScreen       {nullptr};
    ArchiveDestination      m_archiveDestination;

    QList<ArchiveItem *>    m_archiveList;
    QList<EncoderProfile *> m_profileList;

    bool              m_bCreateISO              {false};
    bool              m_bDoBurn                 {false};
    bool              m_bEraseDvdRw             {false};
    QString           m_saveFilename;
    QString           m_theme;

    bool              m_moveMode                {false};

    MythUIButton     *m_nextButton              {nullptr};
    MythUIButton     *m_prevButton              {nullptr};
    MythUIButton     *m_cancelButton            {nullptr};

    MythUIButtonList *m_archiveButtonList       {nullptr};
    MythUIText       *m_nofilesText             {nullptr};
    MythUIButton     *m_addrecordingButton      {nullptr};
    MythUIButton     *m_addvideoButton          {nullptr};
    MythUIButton     *m_addfileButton           {nullptr};

    // size bar
    MythUIProgressBar *m_sizeBar                {nullptr};
    MythUIText        *m_maxsizeText            {nullptr};
    MythUIText        *m_minsizeText            {nullptr};
    MythUIText        *m_currentsizeErrorText   {nullptr};
    MythUIText        *m_currentsizeText        {nullptr};
};

///////////////////////////////////////////////////////////////////////////////

class BurnMenu : public QObject
{
    Q_OBJECT

  public:
    BurnMenu(void);
    ~BurnMenu(void) override = default;

    void start(void);

  private:
    void customEvent(QEvent *event) override; // QObject
    static void doBurn(int mode);
};

#endif


