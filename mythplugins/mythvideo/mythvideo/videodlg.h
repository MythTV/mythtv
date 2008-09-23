#ifndef VIDEODLG_H_
#define VIDEODLG_H_

// QT headers
#include <Q3NetworkOperation>

// MythUI headers
#include <mythtv/libmythui/mythscreentype.h>
#include <mythtv/libmythui/mythuitext.h>
#include <mythtv/libmythui/mythuibuttonlist.h>
#include <mythtv/libmythui/mythuiimage.h>
#include <mythtv/libmythui/mythuistatetype.h>
#include <mythtv/libmythui/mythdialogbox.h>
#include <mythtv/libmythui/mythimage.h>
#include <mythtv/libmythui/mythprogressdialog.h>
#include <mythtv/libmythui/mythgenerictree.h>

// MythVideo headers
#include "parentalcontrols.h"
#include "videoutils.h"
#include "videoscan.h"

class Metadata;
class VideoList;
class ParentalLevel;

enum DialogType {DLG_DEFAULT = 0, DLG_BROWSER = 0x1, DLG_GALLERY = 0x2,
                 DLG_TREE = 0x4, DLG_MANAGER = 0x8 };

class VideoDialog : public MythScreenType
{
    Q_OBJECT

    typedef std::list<std::pair<QString, ParentalLevel::Level> >
            parental_level_map;

    struct rating_to_pl_less :
        public std::binary_function<parental_level_map::value_type,
                                    parental_level_map::value_type, bool>
    {
        bool operator()(const parental_level_map::value_type &lhs,
                    const parental_level_map::value_type &rhs) const
        {
            return lhs.first.length() < rhs.first.length();
        }
    };

  public:
    VideoDialog(MythScreenStack *parent, const QString &name,
                VideoList *video_list, DialogType type=DLG_GALLERY);
    ~VideoDialog();

    virtual bool Create(void);
    virtual bool keyPressEvent(QKeyEvent *);
    virtual void customEvent(QEvent*);

  protected:
    VideoList *m_videoList;

  private slots:
    void UpdateText(MythUIButtonListItem*);
    void handleSelect(MythUIButtonListItem *);
    void SetCurrentNode(MythGenericTree*);

    void playVideo(void);

    void SwitchTree(void);
    void SwitchGallery(void);
    void SwitchBrowse(void);
    void SwitchManager(void);

    void EditMetadata(void);
    void VideoSearch(void);
    void ManualVideoUID(void);
    void ManualVideoTitle(void);
    void ResetMetadata(void);
    void ToggleBrowseable(void);
    void RemoveVideo(void);
    void OnRemoveVideo(bool);

    void VideoMenu(void);
    void InfoMenu(void);
    void ManageMenu(void);
    void ViewMenu(void);

    void ChangeView(void);
    void ChangeFilter(void);

    void ToggleBrowseMode(void);
    void ToggleFlatView(void);

    void ViewPlot(void);
    void ShowCastDialog(void);

    // called during StartVideoPosterSet
    void OnPosterURL(const QString &uri, Metadata *metadata);
    void OnPosterCopyFinished(Q3NetworkOperation *op, Metadata *metadata);
    void OnPosterDownloadTimeout(const QString &url, Metadata *metadata);

    // called during StartVideoSearchByTitle
    void OnVideoSearchByTitleDone(bool normal_exit,
                                  const SearchListResults &results,
                                  Metadata *metadata);

// and now the end points

    // StartVideoPosterSet end
    void OnVideoPosterSetDone(Metadata *metadata);

    // StartVideoSearchByUID end
    void OnVideoSearchByUIDDone(bool normal_exit,
                                const QStringList &output,
                                Metadata *metadata, const QString &video_uid);

// End asynchronous functions.

    void OnParentalChange(int amount);

    // Called when the underlying data for an item changes
    void OnVideoMenuDone(void);
    void OnVideoSearchListSelection(const QString &video_uid);

    void OnManualVideoUID(const QString &video_uid);
    void OnManualVideoTitle(const QString &title);

    void doVideoScan(void);

  protected slots:
    void reloadData(void);
    void refreshData(void);
    void UpdateItem(MythUIButtonListItem *item = NULL);

  protected:
    virtual MythUIButtonListItem* GetItemCurrent();

    virtual void loadData(void);
    void fetchVideos(void);
    void SetWidgetText(QString, QString);
    MythImage* GetCoverImage(MythGenericTree *node);

    Metadata* GetMetadata(MythUIButtonListItem *item);

    void handleDirSelect(MythGenericTree *node);
    bool goBack(void);
    void setParentalLevel(const ParentalLevel::Level &level);
    void shiftParental(int amount);
    bool createPopup(void);
    void createBusyDialog(const QString &title);
    void createOkDialog(const QString &title);

    void SwitchLayout(DialogType type);

    // Edit Metadata
    void ResetItem(Metadata *metadata);

    MythGenericTree *m_rootNode;
    MythGenericTree *m_currentNode;

    bool m_treeLoaded;

    ParentalLevel *m_currentParentalLevel;
    bool m_isFileBrowser;
    bool m_isFlatList;
    int m_type;

    QString m_artDir;
    URLOperationProxy m_url_operator;
    TimeoutSignalProxy m_url_dl_timer;
    parental_level_map m_rating_to_pl;
    VideoScanner *m_scanner;

    MythDialogBox    *m_menuPopup;
    MythUIBusyDialog *m_busyPopup;
    MythScreenStack  *m_popupStack;

    MythUIButtonList *m_videoButtonList;
    MythUIText       *m_titleText;
    MythUIText       *m_novideoText;

    MythUIText       *m_positionText;
    MythUIText       *m_crumbText;

    MythUIImage      *m_coverImage;

    MythUIStateType  *m_parentalLevelState;
    MythUIStateType  *m_videoLevelState;

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
    void StartVideoSearchByUID(const QString &video_uid, Metadata *metadata);

    // StartVideoSearchByTitle()
    //   OnVideoSearchByTitleDone()
    void StartVideoSearchByTitle(const QString &video_uid,
                                const QString &title, Metadata *metadata);
};

#endif
