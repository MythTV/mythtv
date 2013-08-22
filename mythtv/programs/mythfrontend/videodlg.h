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

    typedef simple_ref_ptr<class VideoList> VideoListPtr;

    typedef QPointer<class VideoListDeathDelay> VideoListDeathDelayPtr;

    static VideoListDeathDelayPtr &GetSavedVideoList();

  public:
    VideoDialog(MythScreenStack *lparent, QString lname,
            VideoListPtr video_list, DialogType type,
            BrowseType browse);
    ~VideoDialog();

    bool Create();
    bool keyPressEvent(QKeyEvent *levent);

  private:
    void searchStart();

  public slots:
    void searchComplete(QString string);

  protected slots:
    void Init(); /// Called after the screen is created by MythScreenStack
    void Load(); /// Called after the screen is created by MythScreenStack

  private slots:
    void UpdatePosition();
    void UpdateText(MythUIButtonListItem *);
    void handleSelect(MythUIButtonListItem *);
    void SetCurrentNode(MythGenericTree *);

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
    void VideoSearch(MythGenericTree *node = NULL,
                     bool automode = false);
    void VideoAutoSearch(MythGenericTree *node = NULL);
    void ResetMetadata();
    void ToggleWatched();
    void ToggleProcess();
    void RemoveVideo();
    void OnRemoveVideo(bool);

    void VideoMenu();
    MythMenu* CreateInfoMenu();
    MythMenu* CreateManageMenu();
    MythMenu* CreatePlayMenu();
    void DisplayMenu();
    MythMenu* CreateViewMenu();
    MythMenu* CreateSettingsMenu();
    MythMenu* CreateMetadataBrowseMenu();

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
    void scanFinished(bool);
    void reloadData();
    void refreshData();
    void UpdateItem(MythUIButtonListItem *item);

  protected:
    void customEvent(QEvent *levent);

    virtual MythUIButtonListItem *GetItemCurrent();
    virtual MythUIButtonListItem *GetItemByMetadata(VideoMetadata *metadata);

    virtual void loadData();
    void fetchVideos();
    QString RemoteImageCheck(QString host, QString filename);
    QString GetCoverImage(MythGenericTree *node);
    QString GetFirstImage(MythGenericTree *node, QString type,
                          QString gpnode = QString(), int levels = 0);
    QString GetImageFromFolder(VideoMetadata *metadata);
    QString GetScreenshot(MythGenericTree *node);
    QString GetBanner(MythGenericTree *node);
    QString GetFanart(MythGenericTree *node);

    VideoMetadata *GetMetadata(MythUIButtonListItem *item);

    void handleDirSelect(MythGenericTree *node);
    void handleDynamicDirSelect(MythGenericTree *node);
    bool goBack();
    void setParentalLevel(const ParentalLevel::Level &level);
    void shiftParental(int amount);
    bool createPopup();
    void createBusyDialog(const QString &title);
    void createFetchDialog(VideoMetadata *metadata);
    void dismissFetchDialog(VideoMetadata *metadata, bool ok);
    void createOkDialog(QString title);

    void SwitchLayout(DialogType type, BrowseType browse);

    void StartVideoImageSet(VideoMetadata *metadata);

    void SavePosition(void);

  private slots:

    void OnVideoImageSetDone(VideoMetadata *metadata);
    void OnVideoSearchDone(MetadataLookup *lookup);

  private:
    MythDialogBox    *m_menuPopup;
    MythUIBusyDialog *m_busyPopup;
    MythScreenStack  *m_popupStack;
    MythScreenStack  *m_mainStack;

    MythUIButtonList *m_videoButtonList;
    MythUIButtonTree *m_videoButtonTree;

    MythUIText       *m_titleText;
    MythUIText       *m_novideoText;

    MythUIText       *m_positionText;
    MythUIText       *m_crumbText;

    MythUIImage      *m_coverImage;
    MythUIImage      *m_screenshot;
    MythUIImage      *m_banner;
    MythUIImage      *m_fanart;

    MythUIStateType  *m_trailerState;
    MythUIStateType  *m_parentalLevelState;
    MythUIStateType  *m_videoLevelState;
    MythUIStateType  *m_userRatingState;
    MythUIStateType  *m_watchedState;
    MythUIStateType  *m_studioState;

    MetadataFactory *m_metadataFactory;

    class VideoDialogPrivate *m_d;
};

class VideoListDeathDelay : public QObject
{
    Q_OBJECT

  public:
    VideoListDeathDelay(VideoDialog::VideoListPtr toSave);
    ~VideoListDeathDelay();

    VideoDialog::VideoListPtr GetSaved();

  private slots:
    void OnTimeUp();

  private:
    class VideoListDeathDelayPrivate *m_d;
};

#endif
