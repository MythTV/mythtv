#ifndef VIDEODLG_H_
#define VIDEODLG_H_

#include <QPointer>
#include <QStringList>

#include "mythscreentype.h"
#include "metadatacommon.h"

#include "parentalcontrols.h"
#include "quicksp.h"

class MythUIText;
class MythUIButtonList;
class MythUIButtonTree;
class MythUIButtonListItem;
class MythUIBusyDialog;
class MythUIImage;
class MythUIStateType;
class MythDialogBox;
class MythGenericTree;
class MetadataFactory;
class VideoMetadata;
class VideoScanner;
class MythMenu;

class QUrl;

enum ImageDownloadErrorState { esOK, esError, esTimeout };

class VideoDialog : public MythScreenType
{
    Q_OBJECT

  public:
    enum DialogType { DLG_DEFAULT = 0, DLG_BROWSER = 0x1, DLG_GALLERY = 0x2,
                      DLG_TREE = 0x4, DLG_MANAGER = 0x8, dtLast };

    enum BrowseType { BRS_FOLDER = 0, BRS_GENRE = 0x1, BRS_CATEGORY = 0x2,
                      BRS_YEAR = 0x4, BRS_DIRECTOR = 0x8, BRS_CAST = 0x10,
                      BRS_USERRATING = 0x20, BRS_INSERTDATE = 0x40,
                      BRS_TVMOVIE = 0x80, BRS_STUDIO = 0x100, btLast };

    using VideoListPtr = simple_ref_ptr<class VideoList>;
    using VideoListDeathDelayPtr = QPointer<class VideoListDeathDelay>;

    static VideoListDeathDelayPtr &GetSavedVideoList();

  public:
    VideoDialog(MythScreenStack *lparent, const QString& lname,
            const VideoListPtr& video_list, DialogType type,
            BrowseType browse);
    ~VideoDialog() override;

    bool Create() override; // MythScreenType
    bool keyPressEvent(QKeyEvent *levent) override; // MythScreenType

  private:
    void searchStart();

  public slots:
    void searchComplete(const QString& string);

  protected slots:
    void Init() override; /// Called after the screen is created by MythScreenStack
    void Load() override; /// Called after the screen is created by MythScreenStack

  private slots:
    void UpdatePosition();
    void UpdateText(MythUIButtonListItem *item);
    void handleSelect(MythUIButtonListItem *item);
    void SetCurrentNode(MythGenericTree *node);

    void playVideo();
    void playVideoAlt();
    void playFolder();
    void playVideoWithTrailers();
    void playTrailer();

    void SwitchTree();
    void SwitchGallery();
    void SwitchBrowse();
    void SwitchManager();
    void SwitchVideoFolderGroup();
    void SwitchVideoGenreGroup();
    void SwitchVideoCategoryGroup();
    void SwitchVideoYearGroup();
    void SwitchVideoDirectorGroup();
    void SwitchVideoStudioGroup();
    void SwitchVideoCastGroup();
    void SwitchVideoUserRatingGroup();
    void SwitchVideoInsertDateGroup();
    void SwitchVideoTVMovieGroup();

    void EditMetadata();
    void VideoSearch(MythGenericTree *node = nullptr,
                     bool automode = false);
    void VideoAutoSearch(MythGenericTree *node = nullptr);
    void ResetMetadata();
    void ToggleWatched();
    void ToggleProcess();
    void RemoveVideo();
    void OnRemoveVideo(bool dodelete);

    void VideoMenu();
    MythMenu* CreateInfoMenu();
    MythMenu* CreateManageMenu();
    MythMenu* CreatePlayMenu();
    void DisplayMenu();
    MythMenu* CreateViewMenu();
    MythMenu* CreateSettingsMenu();
    MythMenu* CreateMetadataBrowseMenu();

    void popupClosed(const QString& which, int result);

    void PromptToScan();

    void ChangeFilter();

    void ToggleBrowseMode();
    void ToggleFlatView();

    void ViewPlot();
    void ShowCastDialog();
    void ShowHomepage();
    bool DoItemDetailShow();
    void ShowPlayerSettings();
    void ShowExtensionSettings();
    void ShowMetadataSettings();

    void OnParentalChange(int amount);

    // Called when the underlying data for an item changes
    void OnVideoSearchListSelection(RefCountHandler<MetadataLookup> lookup);

    void doVideoScan();

  protected slots:
    void scanFinished(bool dbChanged);
    void reloadData();
    void refreshData();
    void UpdateItem(MythUIButtonListItem *item);

  protected:
    void customEvent(QEvent *levent) override; // MythUIType

    virtual MythUIButtonListItem *GetItemCurrent();
    virtual MythUIButtonListItem *GetItemByMetadata(VideoMetadata *metadata);

    virtual void loadData();
    void fetchVideos();
    static QString RemoteImageCheck(const QString& host, const QString& filename);
    static QString GetCoverImage(MythGenericTree *node);
    QString GetFirstImage(MythGenericTree *node, const QString& type,
                          const QString& gpnode = QString(), int levels = 0);
    static QString GetImageFromFolder(VideoMetadata *metadata);
    static QString GetScreenshot(MythGenericTree *node);
    static QString GetBanner(MythGenericTree *node);
    static QString GetFanart(MythGenericTree *node);

    static VideoMetadata *GetMetadata(MythUIButtonListItem *item);

    void handleDirSelect(MythGenericTree *node);
    void handleDynamicDirSelect(MythGenericTree *node);
    bool goBack();
    void setParentalLevel(const ParentalLevel::Level &level);
    void shiftParental(int amount);
    bool createPopup();
    void createBusyDialog(const QString &title);
    void createFetchDialog(VideoMetadata *metadata);
    void dismissFetchDialog(VideoMetadata *metadata, bool ok);
    void createOkDialog(const QString& title);

    void SwitchLayout(DialogType type, BrowseType browse);

    void StartVideoImageSet(VideoMetadata *metadata);

    void SavePosition(void);

  private slots:

    void OnVideoImageSetDone(VideoMetadata *metadata);
    void OnVideoSearchDone(MetadataLookup *lookup);

  private:
    MythDialogBox    *m_menuPopup          {nullptr};
    MythUIBusyDialog *m_busyPopup          {nullptr};
    MythScreenStack  *m_popupStack         {nullptr};
    MythScreenStack  *m_mainStack          {nullptr};

    MythUIButtonList *m_videoButtonList    {nullptr};
    MythUIButtonTree *m_videoButtonTree    {nullptr};

    MythUIText       *m_titleText          {nullptr};
    MythUIText       *m_novideoText        {nullptr};

    MythUIText       *m_positionText       {nullptr};
    MythUIText       *m_crumbText          {nullptr};

    MythUIImage      *m_coverImage         {nullptr};
    MythUIImage      *m_screenshot         {nullptr};
    MythUIImage      *m_banner             {nullptr};
    MythUIImage      *m_fanart             {nullptr};

    MythUIStateType  *m_trailerState       {nullptr};
    MythUIStateType  *m_parentalLevelState {nullptr};
    MythUIStateType  *m_videoLevelState    {nullptr};
    MythUIStateType  *m_userRatingState    {nullptr};
    MythUIStateType  *m_watchedState       {nullptr};
    MythUIStateType  *m_studioState        {nullptr};

    MetadataFactory *m_metadataFactory     {nullptr};

    class VideoDialogPrivate *m_d {nullptr};
};

class VideoListDeathDelay : public QObject
{
    Q_OBJECT

  public:
    explicit VideoListDeathDelay(const VideoDialog::VideoListPtr& toSave);
    ~VideoListDeathDelay() override;

    VideoDialog::VideoListPtr GetSaved();
    // When exiting MythVideo, we delay destroying the data for kDelayTimeMS
    // milliseconds, and reuse the data (possibly avoiding a rescan) if the user
    // restarts MythVideo within that time period.
    static const int kDelayTimeMS = 3000;

  private slots:
    void OnTimeUp();

  private:
    class VideoListDeathDelayPrivate *m_d {nullptr};
};

#endif
