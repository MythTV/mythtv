#ifndef VIDEODLG_H_
#define VIDEODLG_H_

#include <QPointer>

#include <mythtv/libmythui/mythscreentype.h>

#include "parentalcontrols.h"
#include "videoutils.h"
#include "quicksp.h"

class Q3NetworkOperation;
class MythUIText;
class MythUIButtonList;
class MythUIButtonTree;
class MythUIButtonListItem;
class MythUIBusyDialog;
class MythUIImage;
class MythUIStateType;
class MythDialogBox;
class MythGenericTree;

class Metadata;
class VideoList;
class VideoScanner;

class VideoDialog : public MythScreenType
{
    Q_OBJECT

  public:
    enum DialogType { DLG_DEFAULT = 0, DLG_BROWSER = 0x1, DLG_GALLERY = 0x2,
                      DLG_TREE = 0x4, DLG_MANAGER = 0x8, dtLast };

    typedef simple_ref_ptr<VideoList> VideoListPtr;

    typedef QPointer<class VideoListDeathDelay> VideoListDeathDelayPtr;

    static VideoListDeathDelayPtr &GetSavedVideoList();

  public:
    VideoDialog(MythScreenStack *lparent, QString lname,
            VideoListPtr video_list, DialogType type=DLG_GALLERY);
    ~VideoDialog();

    bool Create();
    bool keyPressEvent(QKeyEvent *levent);
    void customEvent(QEvent *levent);

  private slots:
    void UpdatePosition();
    void UpdateText(MythUIButtonListItem *);
    void handleSelect(MythUIButtonListItem *);
    void SetCurrentNode(MythGenericTree *);

    void playVideo();

    void SwitchTree();
    void SwitchGallery();
    void SwitchBrowse();
    void SwitchManager();

    void EditMetadata();
    void VideoSearch();
    void ManualVideoUID();
    void ManualVideoTitle();
    void ResetMetadata();
    void ToggleBrowseable();
    void RemoveVideo();
    void OnRemoveVideo(bool);

    void VideoMenu();
    void InfoMenu();
    void ManageMenu();
    void ViewMenu();

    void ChangeFilter();

    void ToggleBrowseMode();
    void ToggleFlatView();

    void ViewPlot();
    void ShowCastDialog();


    void OnParentalChange(int amount);

    // Called when the underlying data for an item changes
    void OnVideoSearchListSelection(QString video_uid);

    void OnManualVideoUID(QString video_uid);
    void OnManualVideoTitle(QString title);

    void doVideoScan();

  protected slots:
    void reloadData();
    void refreshData();
    void UpdateItem(MythUIButtonListItem *item = NULL);

  protected:
    virtual MythUIButtonListItem *GetItemCurrent();

    virtual void loadData();
    void fetchVideos();
    QString GetCoverImage(MythGenericTree *node);

    Metadata *GetMetadata(MythUIButtonListItem *item);

    void handleDirSelect(MythGenericTree *node);
    bool goBack();
    void setParentalLevel(const ParentalLevel::Level &level);
    void shiftParental(int amount);
    bool createPopup();
    void createBusyDialog(QString title);
    void createOkDialog(QString title);

    void SwitchLayout(DialogType type);

    // Edit Metadata
    void ResetItem(Metadata *metadata);

  private:
    MythGenericTree *m_rootNode;
    MythGenericTree *m_currentNode;

    bool m_treeLoaded;

    bool m_isFileBrowser;
    bool m_isFlatList;
    int m_type;

    QString m_artDir;
    VideoScanner *m_scanner;

    MythDialogBox    *m_menuPopup;
    MythUIBusyDialog *m_busyPopup;
    MythScreenStack  *m_popupStack;

    MythUIButtonList *m_videoButtonList;
    MythUIButtonTree *m_videoButtonTree;

    MythUIText       *m_titleText;
    MythUIText       *m_novideoText;

    MythUIText       *m_positionText;
    MythUIText       *m_crumbText;

    MythUIImage      *m_coverImage;

    MythUIStateType  *m_parentalLevelState;
    MythUIStateType  *m_videoLevelState;

    VideoListPtr m_videoList;

    bool m_rememberPosition;

    class VideoDialogPrivate *m_private;

  protected:
    void AutomaticParentalAdjustment(Metadata *metadata);

// Start asynchronous functions.

// These are the start points, separated for sanity.
    // StartVideoPosterSet() start wait background
    //   OnPosterURL()
    //     OnPosterCopyFinished()
    //       OnVideoPosterSetDone()
    //     OnPosterDownloadTimeout()
    //       OnPosterCopyFinished()
    // OnVideoPosterSetDone() stop wait background
    void StartVideoPosterSet(Metadata *metadata);

    // StartVideoSearchByUID() start wait background
    //   OnVideoSearchByUIDDone() stop wait background
    //     StartVideoPosterSet()
    void StartVideoSearchByUID(QString video_uid, Metadata *metadata);

    // StartVideoSearchByTitle()
    //   OnVideoSearchByTitleDone()
    void StartVideoSearchByTitle(QString video_uid, QString title,
            Metadata *metadata);

  private slots:
    // called during StartVideoPosterSet
    void OnPosterURL(QString uri, Metadata *metadata);
    void OnPosterCopyFinished(Q3NetworkOperation *op, Metadata *metadata);
    void OnPosterDownloadTimeout(QString url, Metadata *metadata);

    // called during StartVideoSearchByTitle
    void OnVideoSearchByTitleDone(bool normal_exit,
                                  const SearchListResults &results,
                                  Metadata *metadata);

// and now the end points

    // StartVideoPosterSet end
    void OnVideoPosterSetDone(Metadata *metadata);

    // StartVideoSearchByUID end
    void OnVideoSearchByUIDDone(bool normal_exit,
                                QStringList output,
                                Metadata *metadata, QString video_uid);

// End asynchronous functions.
};

class VideoListDeathDelay : public QObject
{
    Q_OBJECT

  public:
    VideoListDeathDelay(VideoDialog::VideoListPtr toSave);

    VideoDialog::VideoListPtr GetSaved();

  private slots:
    void OnTimeUp();

  private:
    VideoDialog::VideoListPtr m_savedList;
};

#endif
