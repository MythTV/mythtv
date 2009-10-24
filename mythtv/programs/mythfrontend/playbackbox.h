
// -*- Mode: c++ -*-
// vim:set sw=4 ts=4 expandtab:
#ifndef PLAYBACKBOX_H_
#define PLAYBACKBOX_H_

// C++ headers
#include <vector>
using namespace std;

#include <QDateTime>
#include <QMutex>

#include "programinfo.h"
#include "jobqueue.h"
#include "tv_play.h"

#include <pthread.h>

#include "mythscreentype.h"

// mythfrontend
#include "schedulecommon.h"

class NuppelVideoPlayer;
class RingBuffer;
class QTimer;
class ProgramInfo;
class PreviewGenerator;

class MythUIButtonList;
class MythUIButtonListItem;
class MythUIImage;
class MythUIText;
class MythUITextEdit;
class MythUIButton;
class MythDialogBox;

class PreviewGenState
{
  public:
    PreviewGenState() :
        gen(NULL), genStarted(false), ready(false),
        attempts(0), lastBlockTime(0) {}
    PreviewGenerator *gen;
    bool              genStarted;
    bool              ready;
    uint              attempts;
    uint              lastBlockTime;
    QDateTime         blockRetryUntil;

    static const uint maxAttempts;
    static const uint minBlockSeconds;
};

typedef QMap<QString,ProgramList>       ProgramMap;
typedef QMap<QString,QString>           Str2StrMap;
typedef QMap<QString,PreviewGenState>   PreviewMap;

class PlaybackBox : public ScheduleCommon
{
    Q_OBJECT
    friend class PlaybackBoxListItem;

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
        VIEW_NONE       =  0x0000,
        VIEW_TITLES     =  0x0001,
        VIEW_CATEGORIES =  0x0002,
        VIEW_RECGROUPS  =  0x0004,
        VIEW_WATCHLIST  =  0x0008,
        VIEW_SEARCHES   =  0x0010,
        VIEW_LIVETVGRP  =  0x0020,
        // insert new entries above here
        VIEW_WATCHED    =  0x8000
    } ViewMask;

    typedef enum
    {
        DeleteRecording,
        StopRecording,
        ForceDeleteRecording,
    } deletePopupType;

    typedef enum
    {
        kNvpToPlay,
        kNvpToStop,
        kDone
    } killStateType;

    PlaybackBox(MythScreenStack *parent, QString name, BoxType ltype,
                TV *player = NULL, bool showTV = false);
   ~PlaybackBox(void);

    bool Create(void);
    virtual void Init(void);
    bool keyPressEvent(QKeyEvent *);
    void customEvent(QEvent *event);

    void setInitialRecGroup(QString initialGroup) { m_recGroup = initialGroup; }
    static ProgramInfo *RunPlaybackBox(void *player, bool);

  public slots:
    void displayRecGroup(const QString &newRecGroup = "");

  protected slots:
    void updateRecList(MythUIButtonListItem *);
    void ItemSelected(MythUIButtonListItem *item)
        { UpdateProgramInfo(item, true); }
    void selected(MythUIButtonListItem *item);
    void playSelected(MythUIButtonListItem *item = NULL);
    void deleteSelected(MythUIButtonListItem *item);

    void SwitchList(void);

    void customEdit();
    void upcoming();
    void details();
    void stopSelected();
    void showMenu();
    void showActionsSelected();
    void showRecGroupChanger();
    void showPlayGroupChanger();
    void showMetadataEditor();
    void showGroupFilter();
    void showRecGroupPasswordChanger();
    MythDialogBox *createPopupMenu(const QString &title);
    MythDialogBox *createProgramPopupMenu(const QString &title,
                                          ProgramInfo *pginfo = NULL);
    MythDialogBox *createPlaylistPopupMenu();
    void showPlayFromPopup();
    void showRecordingPopup();
    void showJobPopup();
    void showTranscodingProfiles();
    void changeProfileAndTranscode(const QString &profile);
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
    void showPlaylistStoragePopup();
    void showPlaylistJobPopup();
    void showProgramDetails();
    void showIconHelp();

    void popupClosed();

    void doPlayFromBeg();
    void doPlayListRandom();
    void doPIPPlay(void);
    void doPIPPlay(PIPState state);
    void doPBPPlay(void);

    void askStop();
    void doStop();

    void doEditScheduled();
    void doAllowRerecord();

    void askDelete();
    void doUndelete();
    void doDelete();
    void doDeleteForgetHistory();
    void doForceDelete();

    void toggleWatched();
    void toggleAutoExpire();
    void togglePreserveEpisode();

    void toggleView(ViewMask itemMask, bool setOn);
    void toggleTitleView(bool setOn)     { toggleView(VIEW_TITLES, setOn); }
    void toggleCategoryView(bool setOn)  { toggleView(VIEW_CATEGORIES, setOn); }
    void toggleRecGroupView(bool setOn)  { toggleView(VIEW_RECGROUPS, setOn); }
    void toggleWatchListView(bool setOn) { toggleView(VIEW_WATCHLIST, setOn); }
    void toggleSearchView(bool setOn)    { toggleView(VIEW_SEARCHES, setOn); }
    void toggleLiveTVView(bool setOn)    { toggleView(VIEW_LIVETVGRP, setOn); }
    void toggleWatchedView(bool setOn)   { toggleView(VIEW_WATCHED, setOn); }

    void listChanged(void);
    void setUpdateFreeSpace() { m_freeSpaceNeedsUpdate = true; }

    void setGroupFilter(const QString &newRecGroup);
    void setRecGroup(QString newRecGroup);
    void setPlayGroup(QString newPlayGroup);

    void saveRecMetadata(const QString &newTitle, const QString &newSubtitle);

    void SetRecGroupPassword(const QString &newPasswd);

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
    void doPlaylistDeleteForgetHistory();
    void doPlaylistChangeRecGroup();
    void doPlaylistChangePlayGroup();
    void doPlaylistExpireSetting(bool turnOn);
    void doPlaylistExpireSetOn()      { doPlaylistExpireSetting(true);  }
    void doPlaylistExpireSetOff()     { doPlaylistExpireSetting(false); }
    void togglePlayListTitle(void);
    void togglePlayListItem(void);
    void playSelectedPlaylist(bool random);
    void doPlayList(void);
    void showViewChanger(void);
    void saveViewChanges(void);

    void previewThreadDone(const QString &fn, bool &success);
    void previewReady(const ProgramInfo *pginfo);

    void checkPassword(const QString &password);

    void fanartLoad(void);
    void bannerLoad(void);
    void coverartLoad(void);

  protected:
    bool SetPreviewGenerator(const QString &fn, PreviewGenerator *g);
    void IncPreviewGeneratorPriority(const QString &fn);
    void UpdatePreviewGeneratorThreads(void);
    bool IsGeneratingPreview(const QString &fn, bool really = false) const;
    uint IncPreviewGeneratorAttempts(const QString &fn);

  private:
    bool FillList(bool useCachedData = false);
    void UpdateProgressBar(void);

    QString cutDown(const QString &, QFont *, int);
    QString GetPreviewImage(ProgramInfo *);

    bool play(ProgramInfo *rec, bool inPlaylist = false);
    void stop(ProgramInfo *);
    void remove(ProgramInfo *);
    void showActions(ProgramInfo *);
    ProgramInfo *CurrentItem(void);

    void playlistDelete(bool forgetHistory = false);

    void togglePlayListItem(ProgramInfo *pginfo);
    void randomizePlayList(void);

    void processNetworkControlCommands(void);
    void processNetworkControlCommand(const QString &command);

    ProgramInfo *findMatchingProg(const ProgramInfo *);
    ProgramInfo *findMatchingProg(const QString &key);
    ProgramInfo *findMatchingProg(const QString &chanid,
                                  const QString &recstartts);

    bool doRemove(ProgramInfo *, bool forgetHistory, bool forceMetadataDelete);
    void showDeletePopup(deletePopupType);
    void showActionPopup(ProgramInfo *program);
    void showFileNotFoundActionPopup(ProgramInfo *program);
    void popupString(ProgramInfo *program, QString &message);

    void showAvailablePopup(ProgramInfo *rec);

    bool fileExists(ProgramInfo *pginfo);

    QString getRecGroupPassword(const QString &recGroup);
    void fillRecGroupPasswordCache(void);

    bool loadArtwork(QString artworkFile, MythUIImage *image, QTimer *timer,
                     int delay = 500, bool resetImage = false);
    QString findArtworkFile(QString &seriesID, QString &titleIn,
                            QString imagetype, QString host);

    void updateGroupList();
    void updateIcons(const ProgramInfo *pginfo = NULL);
    void updateUsage();
    void updateGroupInfo(const QString &groupname, const QString &grouplabel);
    void UpdateProgramInfo(MythUIButtonListItem *item, bool is_sel,
                           bool force_preview_reload = false);

    void clearProgramCache(void);

    void HandlePreviewEvent(const ProgramInfo &evinfo);

    MythUIButtonList *m_groupList;
    MythUIButtonList *m_recordingList;

    MythUIText *m_noRecordingsText;

    MythUIImage *m_previewImage;
    MythUIImage *m_fanart;
    MythUIImage *m_banner;
    MythUIImage *m_coverart;
    QTimer      *m_fanartTimer;
    QTimer      *m_bannerTimer;
    QTimer      *m_coverartTimer;

    InfoMap m_currentMap;

    // Settings ///////////////////////////////////////////////////////////////
    /// If "Play"  this is a recording playback selection UI,
    /// if "Delete this is a recording deletion selection UI.
    BoxType             m_type;
    // date/time formats from DB
    QString             m_formatShortDate;
    QString             m_formatLongDate;
    QString             m_formatTime;
    /// titleView controls showing titles in group list
    bool                m_titleView;
    /// useCategories controls showing categories in group list
    bool                m_useCategories;
    /// useRecGroups controls showing of recording groups in group list
    bool                m_useRecGroups;
    /// use the Watch List as the initial view
    bool                m_watchListStart;
    /// exclude recording not marked for auto-expire from the Watch List
    bool                m_watchListAutoExpire;
    /// add 1 to the Watch List scord up to this many days
    int                 m_watchListMaxAge;
    /// adjust exclusion of a title from the Watch List after a delete
    int                 m_watchListBlackOut;
    /// contains "DispRecGroupAsAllProg" setting
    bool                m_groupnameAsAllProg;
    /// allOrder controls the ordering of the "All Programs" list
    int                 m_allOrder;
    /// listOrder controls the ordering of the recordings in the list
    int                 m_listOrder;

    // Recording Group settings
    QString             m_groupDisplayName;
    QString             m_recGroup;
    QString             m_recGroupPassword;
    QString             m_curGroupPassword;
    QString             m_newRecGroup;
    QString             m_watchGroupName;
    QString             m_watchGroupLabel;
    ViewMask            m_viewMask;

    // Popup support //////////////////////////////////////////////////////////
    // General popup support
    MythDialogBox      *m_popupMenu;
    MythScreenStack    *m_popupStack;

    bool m_expectingPopup;

    // Recording Group popup support
    QMap<QString,QString> m_recGroupType;
    QMap<QString,QString> m_recGroupPwCache;

    // State Variables ////////////////////////////////////////////////////////
    // Main Recording List support
    QTimer             *m_fillListTimer; // audited ref #5318
    bool                m_fillListFromCache;
    bool                m_connected;  ///< true if last FillList() succeeded
    QStringList         m_titleList;  ///< list of pages
    ProgramMap          m_progLists;  ///< lists of programs by page
    int                 m_progsInDB;  ///< total number of recordings in DB
    bool                m_isFilling;

    QStringList         m_recGroups;
    mutable QMutex      m_recGroupsLock;
    int                 m_recGroupIdx;

    // Other state
    /// Program currently selected for deletion
    ProgramInfo *m_delItem;
    /// Program currently selected during an update
    ProgramInfo *m_currentItem;
    /// Group currently selected
    QString m_currentGroup;

    // Play List support
    QStringList         m_playList;   ///< list of selected items "play list"

    /// ProgramInfo cache for FillList()
    mutable QMutex      m_progCacheLock;
    vector<ProgramInfo *> *m_progCache;

    /// playingSomething is set to true iff a full screen recording is playing
    bool                m_playingSomething;

    /// Does the recording list need to be refilled
    bool m_needUpdate;

    // Selection state variables
    bool                m_haveGroupInfoSet;

    // Free disk space tracking
    bool                m_freeSpaceNeedsUpdate;
    QTimer             *m_freeSpaceTimer; // audited ref #5318
    int                 m_freeSpaceTotal;
    int                 m_freeSpaceUsed;

    // Preview Pixmap Variables ///////////////////////////////////////////////
    bool                m_previewFromBookmark;
    uint                m_previewGeneratorMode;
    QMap<QString,QDateTime> m_previewFileTS;
    bool                m_previewSuspend;
    mutable QMutex      m_previewGeneratorLock;
    PreviewMap          m_previewGenerator;
    vector<QString>     m_previewGeneratorQueue;
    uint                m_previewGeneratorRunning;
    static const uint   PREVIEW_GEN_MAX_RUN;

    // Network Control Variables //////////////////////////////////////////////
    mutable QMutex      m_ncLock;
    deque<QString>      m_networkControlCommands;
    bool                m_underNetworkControl;

    TV                  *m_player;
};

class GroupSelector : public MythScreenType
{
    Q_OBJECT

  public:
    GroupSelector(MythScreenStack *lparent, const QString &label,
                  const QStringList &list, const QStringList &data,
                  const QString &selected);

    bool Create(void);

  signals:
    void result(QString);

  protected slots:
    void AcceptItem(MythUIButtonListItem *item);

  private:
    void loadGroups(void);

    QString m_label;
    QStringList m_List;
    QStringList m_Data;
    QString m_selected;
};

class ChangeView : public MythScreenType
{
    Q_OBJECT

  public:
    ChangeView(MythScreenStack *lparent, MythScreenType *parentScreen,
               int viewMask);

    bool Create(void);

  signals:
    void save();

  protected slots:
    void SaveChanges(void);

  private:
    MythScreenType *m_parentScreen;
    int m_viewMask;
};

class PasswordChange : public MythScreenType
{
    Q_OBJECT

  public:
    PasswordChange(MythScreenStack *lparent, QString oldpassword);

    bool Create(void);

  signals:
    void result(const QString &);

  protected slots:
    void OldPasswordChanged(void);
    void SendResult(void);

  private:
    MythUITextEdit     *m_oldPasswordEdit;
    MythUITextEdit     *m_newPasswordEdit;
    MythUIButton       *m_okButton;

    QString m_oldPassword;
};

class RecMetadataEdit : public MythScreenType
{
    Q_OBJECT

  public:
    RecMetadataEdit(MythScreenStack *lparent, ProgramInfo *pginfo);

    bool Create(void);

  signals:
    void result(const QString &, const QString &);

  protected slots:
    void SaveChanges(void);

  private:
    MythUITextEdit     *m_titleEdit;
    MythUITextEdit     *m_subtitleEdit;

    ProgramInfo *m_progInfo;
};

class HelpPopup : public MythScreenType
{
    Q_OBJECT

  public:
    HelpPopup(MythScreenStack *lparent);

    bool Create(void);

  private:
    void addItem(const QString &state, const QString &text);

    MythUIButtonList *m_iconList;
};

#endif
/* vim: set expandtab tabstop=4 shiftwidth=4: */
