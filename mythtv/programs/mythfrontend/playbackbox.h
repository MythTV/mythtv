// -*- Mode: c++ -*-
// vim:set sw=4 ts=4 expandtab:
#ifndef PLAYBACKBOX_H_
#define PLAYBACKBOX_H_

#include <qdatetime.h>
#include <qdom.h>
#include <qmutex.h>

#include "mythwidgets.h"
#include "mythdialogs.h"
#include "uitypes.h"
#include "xmlparse.h"
#include "programinfo.h"
#include "jobqueue.h"

#include <qvaluelist.h>
#include <pthread.h>

class QListViewItem;
class QLabel;
class QProgressBar;
class NuppelVideoPlayer;
class RingBuffer;
class QTimer;
class ProgramInfo;
class PreviewGenerator;

class LayerSet;

typedef QMap<QString,ProgramList>       ProgramMap;
typedef QMap<QString,QString>           Str2StrMap;
typedef QMap<QString,PreviewGenerator*> PreviewMap;
typedef QMap<QString,MythTimer>         LastCheckedMap;

class PlaybackBox : public MythDialog
{
    Q_OBJECT
  public:
    typedef enum
    {
        Play,
        Delete,
    } BoxType;

    // ViewType values cannot change; they are stored in the database.
    typedef enum {
        TitlesOnly = 0,
        TitlesCategories = 1,
        TitlesCategoriesRecGroups = 2,
        TitlesRecGroups = 3,
        Categories = 4,
        CategoriesRecGroups = 5,
        RecGroups = 6,
        ViewTypes,                  // placeholder value, not in database
    } ViewType;

    // Sort function when TitlesOnly. Values are stored in database.
    typedef enum {
        TitleSortAlphabetical = 0,
        TitleSortRecPriority = 1,
        TitleSortMethods,           // placeholder value, not in database
    } ViewTitleSort;

    typedef enum {
        VIEW_NONE       =  0x00,
        VIEW_TITLES     =  0x01,
        VIEW_CATEGORIES =  0x02,
        VIEW_RECGROUPS  =  0x04,
        VIEW_WATCHLIST  =  0x08,
        VIEW_ALL        = ~0x00,
    } ViewMask;

    typedef enum
    {
        EndOfRecording,
        DeleteRecording,
        StopRecording,
        ForceDeleteRecording,
    } deletePopupType;

    typedef enum 
    {
        kStarting,
        kPlaying,
        kKilling,
        kKilled,
        kStopping,
        kStopped,
        kChanging,
    } playerStateType;

    typedef enum
    {
        kNvpToPlay,
        kNvpToStop,
        kDone
    } killStateType;
     

    PlaybackBox(BoxType ltype, MythMainWindow *parent, const char *name = 0);
   ~PlaybackBox(void);
   
    void customEvent(QCustomEvent *e);
  
  protected slots:
    void timeout(void);

    void cursorLeft();
    void cursorRight();
    void cursorDown(bool page = false, bool newview = false);
    void cursorUp(bool page = false, bool newview = false);
    void pageDown() { cursorDown(true); }
    void pageUp() { cursorUp(true); }
    void customEdit();
    void upcoming();
    void details();
    void selected();
    void playSelected();
    void stopSelected();
    void deleteSelected();
    void showMenu();
    void showActionsSelected();
    void showRecGroupChanger();
    void showPlayGroupChanger();
    void showRecTitleChanger();
    void showRecGroupChooser();
    void showRecGroupPasswordChanger();
    void showPlayFromPopup();
    void showRecordingPopup();
    void showJobPopup();
    void showTranscodingProfiles();
    void changeProfileAndTranscode(QString profile);
    void changeProfileAndTranscodeAuto()
             { changeProfileAndTranscode("Autodetect"); }
    void changeProfileAndTranscodeHigh()
             { changeProfileAndTranscode("High Quality"); }
    void changeProfileAndTranscodeMedium()
             { changeProfileAndTranscode("Medium Quality"); }
    void changeProfileAndTranscodeLow()
             { changeProfileAndTranscode("Low Quality"); }
    void showStoragePopup();
    void showPlaylistPopup();
    void showPlaylistJobPopup();
    void showProgramDetails();

    void doPlay();
    void doPlayFromBeg();
    void doPlayListRandom();

    void askStop();
    void doStop();
    void noStop();

    void doEditScheduled();

    void askDelete();
    void doDelete();
    void doDeleteForgetHistory();
    void doForceDelete();
    void noDelete();

    void setWatched();
    void setUnwatched();

    void doAutoExpire();
    void noAutoExpire();
    void doPreserveEpisode();
    void noPreserveEpisode();

    void doCancel();
    void toggleTitleView();

    void exitWin();

    void listChanged(void);
    void setUpdateFreeSpace() { freeSpaceNeedsUpdate = true; }

    void initRecGroupPopup(QString title, QString name);
    void closeRecGroupPopup(bool refreshList = true);
    void setGroupFilter(void);
    void recGroupChangerListBoxChanged(void);
    void recGroupChooserListBoxChanged(void);
    void setRecGroup(void);
    void setPlayGroup(void);
    void setRecTitle(void);
    void setRecGroupPassword();
    void recGroupOldPasswordChanged(const QString &newText);

    void doJobQueueJob(int jobType, int jobFlags = 0);
    void doPlaylistJobQueueJob(int jobType, int jobFlags = 0);
    void stopPlaylistJobQueueJob(int jobType);
    void doBeginFlagging();
    void doBeginTranscoding()         {   doJobQueueJob(JOB_TRANSCODE,
                                                        JOB_USE_CUTLIST);      }
    void doBeginUserJob1()            {   doJobQueueJob(JOB_USERJOB1);         }
    void doBeginUserJob2()            {   doJobQueueJob(JOB_USERJOB2);         }
    void doBeginUserJob3()            {   doJobQueueJob(JOB_USERJOB3);         }
    void doBeginUserJob4()            {   doJobQueueJob(JOB_USERJOB4);         }
    void doPlaylistBeginTranscoding() {   doPlaylistJobQueueJob(JOB_TRANSCODE,
                                                             JOB_USE_CUTLIST); }
    void stopPlaylistTranscoding()    { stopPlaylistJobQueueJob(JOB_TRANSCODE);}
    void doPlaylistBeginFlagging()    {   doPlaylistJobQueueJob(JOB_COMMFLAG); }
    void stopPlaylistFlagging()       { stopPlaylistJobQueueJob(JOB_COMMFLAG); }
    void doPlaylistBeginUserJob1()    {   doPlaylistJobQueueJob(JOB_USERJOB1); }
    void stopPlaylistUserJob1()       { stopPlaylistJobQueueJob(JOB_USERJOB1); }
    void doPlaylistBeginUserJob2()    {   doPlaylistJobQueueJob(JOB_USERJOB2); }
    void stopPlaylistUserJob2()       { stopPlaylistJobQueueJob(JOB_USERJOB2); }
    void doPlaylistBeginUserJob3()    {   doPlaylistJobQueueJob(JOB_USERJOB3); }
    void stopPlaylistUserJob3()       { stopPlaylistJobQueueJob(JOB_USERJOB3); }
    void doPlaylistBeginUserJob4()    {   doPlaylistJobQueueJob(JOB_USERJOB4); }
    void stopPlaylistUserJob4()       { stopPlaylistJobQueueJob(JOB_USERJOB4); }
    void doClearPlaylist();
    void doPlaylistDelete();
    void doPlaylistChangeRecGroup();
    void doPlaylistChangePlayGroup();
    void togglePlayListTitle(void);
    void togglePlayListItem(void);
    void playSelectedPlaylist(bool random);
    void doPlayList(void);
    void showViewChanger(void);

    void previewThreadDone(const QString &fn, bool &success);
    void previewReady(const ProgramInfo *pginfo);

  protected:
    void paintEvent(QPaintEvent *);
    void keyPressEvent(QKeyEvent *e);

    bool SetPreviewGenerator(const QString &fn, PreviewGenerator *g);
    bool IsGeneratingPreview(const QString &fn) const;
    uint IncPreviewGeneratorAttempts(const QString &fn);

  private:
    bool FillList(bool useCachedData = false);
    void UpdateProgressBar(void);

    QString cutDown(QString, QFont *, int);
    QDateTime getPreviewLastModified(ProgramInfo *);
    QPixmap getPixmap(ProgramInfo *);
    QPainter backup;
    bool play(ProgramInfo *rec, bool inPlaylist = false);
    void stop(ProgramInfo *);
    void remove(ProgramInfo *);
    void showActions(ProgramInfo *);

    void togglePlayListItem(ProgramInfo *pginfo);
    void randomizePlayList(void);

    void processNetworkControlCommands(void);
    void processNetworkControlCommand(QString command);

    void LoadWindow(QDomElement &);

    void parsePopup(QDomElement &);
    void parseContainer(QDomElement &);

    ProgramInfo *findMatchingProg(ProgramInfo *);
    ProgramInfo *findMatchingProg(QString key);
    ProgramInfo *findMatchingProg(QString chanid, QString recstartts);

    bool killPlayer(void);
    void killPlayerSafe(void);
    void startPlayer(ProgramInfo *rec);

    bool doRemove(ProgramInfo *, bool forgetHistory, bool forceMetadataDelete);
    void promptEndOfRecording(ProgramInfo *);
    void showDeletePopup(ProgramInfo *, deletePopupType);
    void showActionPopup(ProgramInfo *program);
    void showFileNotFoundActionPopup(ProgramInfo *program);
    void initPopup(MythPopupBox *popup, ProgramInfo *program, 
                   QString message, QString message2);
    void cancelPopup();

    void showAvailablePopup(ProgramInfo *rec);

    bool fileExists(ProgramInfo *pginfo);

    void showIconHelp();

    QString getRecGroupPassword(QString recGroup);
    void fillRecGroupPasswordCache(void);

    void grayOut(QPainter *);
    void updateBackground(void);
    void updateVideo(QPainter *);
    void updateShowTitles(QPainter *);
    void updateInfo(QPainter *);
    void updateUsage(QPainter *);
    void updateProgramInfo(QPainter *p, QRect& pr, QPixmap& pix);
    void updateCurGroup(QPainter *p);
    void updateGroupInfo(QPainter *p, QRect& pr, QPixmap& pix,
                         QString cont_name = "group_info");
    void setDefaultView(ViewType defaultView);

    void clearProgramCache(void);

    // Settings ///////////////////////////////////////////////////////////////
    /// If "Play"  this is a recording playback selection UI,
    /// if "Delete this is a recording deletion selection UI.
    BoxType             type;
    // date/time formats from DB
    QString             formatShortDate;
    QString             formatLongDate;
    QString             formatTime;
    /// titleView controls showing titles in group list
    bool                titleView;
    /// useCategories controls showing categories in group list
    bool                useCategories;
    /// useRecGroups controls showing of recording groups in group list
    bool                useRecGroups;
    /// contains "DispRecGroupAsAllProg" setting
    bool                groupnameAsAllProg;
    /// include the Watch List feature
    bool                watchList;
    /// exclude recording not marked for auto-expire from the Watch List
    bool                watchListAutoExpire;
    /// add 1 to the Watch List scord up to this many days 
    int                 watchListMaxAge;
    /// adjust exclusion of a title from the Watch List after a delete
    int                 watchListBlackOut;
    /// if true we allow arrow keys to replace SELECT and ESCAPE as appropiate
    bool                arrowAccel;
    /// if true keypress events are ignored  
    bool                ignoreKeyPressEvents;
    /// listOrder controls the ordering of the recordings in the list
    int                 listOrder;
    /// Number of items in selector that can be shown on the screen at once
    int                 listsize;

    // Recording Group settings
    QString             groupDisplayName;
    QString             recGroup;
    QString             recGroupPassword;
    QString             curGroupPassword;
    QString             watchGroup;
    ViewMask            viewMask;

    // Theme parsing variables
    XMLParse           *theme;
    QDomElement         xmldata;

    // Non-volatile drawing variables /////////////////////////////////////////
    QPixmap            *drawTransPixmap;
    bool                drawPopupSolid;
    QColor              drawPopupFgColor;
    QColor              drawPopupBgColor;
    QColor              drawPopupSelColor;
    QRect               drawTotalBounds;
    QRect               drawListBounds;
    QRect               drawInfoBounds;
    QRect               drawGroupBounds;
    QRect               drawUsageBounds;
    QRect               drawVideoBounds;
    QRect               drawCurGroupBounds;

    // Popup support //////////////////////////////////////////////////////////
    // General popup support
    MythPopupBox       *popup;
    bool                expectingPopup;

    // Recording Group popup support
    int                 recGroupLastItem;
    QString             recGroupChooserPassword;
    Str2StrMap          recGroupType;
    Str2StrMap          recGroupPwCache;
    MythPopupBox       *recGroupPopup;
    MythListBox        *recGroupListBox;
    MythLineEdit       *recGroupLineEdit;
    MythLineEdit       *recGroupLineEdit1;
    MythLineEdit       *recGroupOldPassword;
    MythLineEdit       *recGroupNewPassword;
    MythPushButton     *recGroupOkButton;

    // State Variables ////////////////////////////////////////////////////////
    // Main Recording List support
    QTimer             *fillListTimer;
    bool                connected;  ///< true if last FillList() succeeded
    int                 titleIndex; ///< Index of selected item in whole list
    int                 progIndex;  ///< Index of selected item index on page
    QStringList         titleList;  ///< list of pages
    ProgramMap          progLists;  ///< lists of programs by page        
    int                 progsInDB;  ///< total number of recordings in DB

    // Play List support
    QStringList         playList;   ///< list of selected items "play list"

    // Other state
    /// Currently selected program
    ProgramInfo        *curitem;
    /// Program currently selected for deletion
    ProgramInfo        *delitem;
    /// Last Program played by play(ProgramInfo*,bool)
    ProgramInfo        *lastProgram;
    /// ProgramInfo cache for FillList()
    vector<ProgramInfo *> *progCache;
    /// playingSomething is set to true iff a full screen recording is playing
    bool                playingSomething;

    // Selection state variables
    bool                haveGroupInfoSet;
    bool                inTitle;
    /// If change is left or right, don't restart video
    bool                leftRight;

    // Free disk space tracking
    bool                freeSpaceNeedsUpdate;
    QTimer             *freeSpaceTimer;
    int                 freeSpaceTotal;
    int                 freeSpaceUsed;

    // Volatile drawing variables
    int                 paintSkipCount;
    bool                paintSkipUpdate;
    QPixmap             paintBackgroundPixmap;

    // Preview Video Variables ////////////////////////////////////////////////
    NuppelVideoPlayer  *previewVideoNVP;
    RingBuffer         *previewVideoRingBuf;
    QTimer             *previewVideoRefreshTimer;
    MythTimer           previewVideoStartTimer;
    MythTimer           previewVideoPlayingTimer;  
    int                 previewVideoBrokenRecId;
    playerStateType     previewVideoState;
    bool                previewVideoStartTimerOn;
    bool                previewVideoEnabled;
    bool                previewVideoPlaying;
    bool                previewVideoThreadRunning;
    pthread_t           previewVideoThread;

    mutable QMutex      previewVideoKillLock;
    mutable QMutex      previewVideoUnsafeKillLock;
    killStateType       previewVideoKillState;
    MythTimer           previewVideoKillTimeout;

    // Preview Pixmap Variables ///////////////////////////////////////////////
    bool                previewPixmapEnabled;
    bool                previewFromBookmark;
    QPixmap            *previewPixmap;
    QDateTime           previewLastModified;
    LastCheckedMap      previewLastModifyCheck;
    QDateTime           previewFilets;
    QDateTime           previewStartts;
    bool                previewSuspend;
    QString             previewChanid;
    mutable QMutex      previewGeneratorLock;
    PreviewMap          previewGenerator;
    QMap<QString,uint>  previewGeneratorAttempts;

    // Network Control Variables //////////////////////////////////////////////
    mutable QMutex      ncLock;
    QValueList<QString> networkControlCommands;
    bool                underNetworkControl;
};

#endif
/* vim: set expandtab tabstop=4 shiftwidth=4: */
