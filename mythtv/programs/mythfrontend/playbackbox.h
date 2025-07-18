// -*- Mode: c++ -*-
// vim:set sw=4 ts=4 expandtab:
#ifndef PLAYBACKBOX_H_
#define PLAYBACKBOX_H_

// C++ headers
#include <cstdint> // for [u]int[32,64]_t
#include <deque>
#include <utility>
#include <vector>

// Qt
#include <QStringList>
#include <QDateTime>
#include <QMultiMap>
#include <QObject>
#include <QMutex>
#include <QMap>
#include <QSet>

// MythTV
#include "libmythmetadata/metadatafactory.h"
#include "libmythtv/jobqueue.h"
#include "libmythtv/tv_play.h"
#include "libmythui/mythscreentype.h"

// mythfrontend
#include "schedulecommon.h"
#include "programinfocache.h"
#include "playbackboxhelper.h"

class QKeyEvent;
class QEvent;
class QTimer;

class MythPlayer;
class MythMediaBuffer;
class ProgramInfo;

class MythUIButtonList;
class MythUIButtonListItem;
class MythUIImage;
class MythUIText;
class MythUITextEdit;
class MythUIButton;
class MythDialogBox;
class MythMenu;
class MythUIBusyDialog;
class PlaybackBox;

using ProgramMap = QMap<QString,ProgramList>;
using Str2StrMap = QMap<QString,QString>;
using PlaybackBoxCb = void (PlaybackBox::*)();

static constexpr size_t kMaxJobs {7};

static constexpr uint16_t kArtworkFanTimeout    { 300 };
static constexpr uint8_t  kArtworkBannerTimeout {  50 };
static constexpr uint8_t  kArtworkCoverTimeout  {  50 };

class PlaybackBox : public ScheduleCommon
{
    Q_OBJECT
    friend class PlaybackBoxListItem;
    friend class ChangeView;

  public:
    // ViewType values cannot change; they are stored in the database.
    enum ViewType : std::uint8_t {
        TitlesOnly = 0,
        TitlesCategories = 1,
        TitlesCategoriesRecGroups = 2,
        TitlesRecGroups = 3,
        Categories = 4,
        CategoriesRecGroups = 5,
        RecGroups = 6,
        ViewTypes = 7,              // placeholder value, not in database
    };

    // Sort function when TitlesOnly. Values are stored in database.
    enum ViewTitleSort : std::uint8_t {
        TitleSortAlphabetical = 0,
        TitleSortRecPriority = 1,
        TitleSortMethods = 2,       // placeholder value, not in database
    };

    enum ViewMask : std::uint16_t {
        VIEW_NONE       =  0x0000,
        VIEW_TITLES     =  0x0001,
        VIEW_CATEGORIES =  0x0002,
        VIEW_RECGROUPS  =  0x0004,
        VIEW_WATCHLIST  =  0x0008,
        VIEW_SEARCHES   =  0x0010,
        VIEW_LIVETVGRP  =  0x0020,
        // insert new entries above here
        VIEW_WATCHED    =  0x8000
    };

    enum DeletePopupType : std::uint8_t
    {
        kDeleteRecording,
        kStopRecording,
        kForceDeleteRecording,
    };

    enum DeleteFlags : std::uint8_t
    {
        kNoDelFlags    = 0x00,
        kForgetHistory = 0x01,
        kForce         = 0x02,
        kIgnore        = 0x04,
        kAllRemaining  = 0x08,
    };

    enum killStateType : std::uint8_t
    {
        kNvpToPlay,
        kNvpToStop,
        kDone
    };

    static std::array<PlaybackBoxCb,kMaxJobs*2> kMySlots;

    PlaybackBox(MythScreenStack *parent, const QString& name,
                TV *player = nullptr, bool showTV = false);
   ~PlaybackBox(void) override;

    bool Create(void) override; // MythScreenType
    void Load(void) override; // MythScreenType
    void Init(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType
    void customEvent(QEvent *event) override; // ScheduleCommon

    void setInitialRecGroup(const QString& initialGroup) { m_recGroup = initialGroup; }
    static void * RunPlaybackBox(void *player, bool showTV);

  public slots:
    void displayRecGroup(const QString &newRecGroup = "");
    void groupSelectorClosed(void);

  protected slots:
    void updateRecList(MythUIButtonListItem *sel_item);
    void selectUIGroupsAlphabet(MythUIButtonListItem *item);
    void ItemSelected(MythUIButtonListItem *item)
        { UpdateUIListItem(item, true); }
    void ItemVisible(MythUIButtonListItem *item);
    void ItemLoaded(MythUIButtonListItem *item);
    void selected(MythUIButtonListItem *item);
    void updateRecGroup(MythUIButtonListItem *sel_item);
    void PlayFromAnyMark(MythUIButtonListItem *item);
    void PlayFromAnyMark() { PlayFromAnyMark(nullptr); }
    void PlayFromBookmark(MythUIButtonListItem *item);
    void PlayFromBookmark() { PlayFromBookmark(nullptr); }
    void PlayFromBeginning(MythUIButtonListItem *item);
    void PlayFromBeginning() { PlayFromBeginning(nullptr); }
    void PlayFromLastPlayPos(MythUIButtonListItem *item);
    void PlayFromLastPlayPos() { PlayFromLastPlayPos(nullptr); }
    void deleteSelected(MythUIButtonListItem *item);
    void ClearBookmark();
    void ClearLastPlayPos();
    void SwitchList(void);

    void ShowGroupPopup(void);
    void StopSelected(void);
    void showMetadataEditor();
    void showGroupFilter();
    void showRecGroupPasswordChanger();
    MythMenu*  createPlayFromMenu();
    MythMenu*  createRecordingMenu();
    MythMenu*  createJobMenu();
    MythMenu*  createTranscodingProfilesMenu();
    void doCreateTranscodingProfilesMenu()
        {static_cast<void>(createTranscodingProfilesMenu());}
    MythMenu* createStorageMenu();
    MythMenu* createPlaylistMenu();
    MythMenu* createPlaylistStorageMenu();
    MythMenu* createPlaylistJobMenu();
    void changeProfileAndTranscode(int id);
    void showIconHelp();
    void ShowRecGroupChangerUsePlaylist(void)  { ShowRecGroupChanger(true);  }
    void ShowRecGroupChangerNoPlaylist(void)   { ShowRecGroupChanger(false);  }
    void ShowPlayGroupChangerUsePlaylist(void) { ShowPlayGroupChanger(true); }
    void ShowPlayGroupChangerNoPlaylist(void)  { ShowPlayGroupChanger(false); }
    void ShowRecGroupChanger(bool use_playlist = false);
    void ShowPlayGroupChanger(bool use_playlist = false);

    void popupClosed(const QString& which, int result);

    void doPlayListRandom();

    void askStop();

    void doAllowRerecord();

    void askDelete();
    void Undelete(void);
    void Delete(PlaybackBox::DeleteFlags flags);
    void Delete() { Delete(kNoDelFlags); }
    void DeleteForgetHistory(void)      { Delete(kForgetHistory); }
    void DeleteForce(void)              { Delete(kForce);         }
    void DeleteIgnore(void)             { Delete(kIgnore);        }
    void DeleteForceAllRemaining(void)
        { Delete((DeleteFlags)((int)kForce |(int)kAllRemaining)); }
    void DeleteIgnoreAllRemaining(void)
        { Delete((DeleteFlags)((int)kIgnore|(int)kAllRemaining)); }

    void ShowRecordedEpisodes();
    void ShowAllRecordings();

    void toggleWatched();
    void toggleAutoExpire();
    void togglePreserveEpisode();

    void toggleView(PlaybackBox::ViewMask itemMask, bool setOn);
    void toggleTitleView(bool setOn)     { toggleView(VIEW_TITLES, setOn); }
    void toggleCategoryView(bool setOn)  { toggleView(VIEW_CATEGORIES, setOn); }
    void toggleRecGroupView(bool setOn)  { toggleView(VIEW_RECGROUPS, setOn); }
    void toggleWatchListView(bool setOn) { toggleView(VIEW_WATCHLIST, setOn); }
    void toggleSearchView(bool setOn)    { toggleView(VIEW_SEARCHES, setOn); }
    void toggleLiveTVView(bool setOn)    { toggleView(VIEW_LIVETVGRP, setOn); }
    void toggleWatchedView(bool setOn)   { toggleView(VIEW_WATCHED, setOn); }

    void setGroupFilter(const QString &newRecGroup);
    void setRecGroup(QString newRecGroup);
    void setPlayGroup(QString newPlayGroup);

    void saveRecMetadata(const QString &newTitle, const QString &newSubtitle,
                         const QString &newDescription, const QString &newInetref,
                         uint season, uint episode);

    void SetRecGroupPassword(const QString &newPassword);

    void doJobQueueJob(int jobType, int jobFlags = 0);
    void doPlaylistJobQueueJob(int jobType, int jobFlags = 0);
    void stopPlaylistJobQueueJob(int jobType);
    void doBeginFlagging();
    void doBeginLookup();
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
    void doPlaylistBeginLookup()      {   doPlaylistJobQueueJob(JOB_METADATA); }
    void stopPlaylistLookup()         { stopPlaylistJobQueueJob(JOB_METADATA); }
    void doPlaylistBeginUserJob1()    {   doPlaylistJobQueueJob(JOB_USERJOB1); }
    void stopPlaylistUserJob1()       { stopPlaylistJobQueueJob(JOB_USERJOB1); }
    void doPlaylistBeginUserJob2()    {   doPlaylistJobQueueJob(JOB_USERJOB2); }
    void stopPlaylistUserJob2()       { stopPlaylistJobQueueJob(JOB_USERJOB2); }
    void doPlaylistBeginUserJob3()    {   doPlaylistJobQueueJob(JOB_USERJOB3); }
    void stopPlaylistUserJob3()       { stopPlaylistJobQueueJob(JOB_USERJOB3); }
    void doPlaylistBeginUserJob4()    {   doPlaylistJobQueueJob(JOB_USERJOB4); }
    void stopPlaylistUserJob4()       { stopPlaylistJobQueueJob(JOB_USERJOB4); }
    void doClearPlaylist();
    void PlaylistDeleteForgetHistory(void) { PlaylistDelete(true); }
    void PlaylistDeleteKeepHistory(void)   { PlaylistDelete(false); }
    void PlaylistDelete(bool forgetHistory = false);
    void doPlaylistExpireSetting(bool turnOn);
    void doPlaylistExpireSetOn()      { doPlaylistExpireSetting(true);  }
    void doPlaylistExpireSetOff()     { doPlaylistExpireSetting(false); }
    void doPlaylistWatchedSetting(bool turnOn);
    void doPlaylistWatchedSetOn()      { doPlaylistWatchedSetting(true);  }
    void doPlaylistWatchedSetOff()     { doPlaylistWatchedSetting(false); }
    void doPlaylistAllowRerecord();
    void togglePlayListTitle(void);
    void togglePlayListItem(void);
    void playSelectedPlaylist(bool Random);
    void doPlayList(void);
    void showViewChanger(void);
    void saveViewChanges(void);

    void checkPassword(const QString &password);
    void passwordClosed(void);

    void fanartLoad(void);
    void bannerLoad(void);
    void coverartLoad(void);

  private:
    bool UpdateUILists(void);
    void UpdateUIGroupList(const QStringList &groupPreferences);
    void UpdateUIRecGroupList(void);
    void SelectNextRecGroup(void);

    void UpdateProgressBar(void);

    void PlayX(const ProgramInfo &pginfo,
               bool ignoreBookmark,
               bool ignoreProgStart,
               bool ignoreLastPlayPos,
               bool underNetworkControl);

    bool Play(const ProgramInfo &rec,
              bool inPlaylist,
              bool ignoreBookmark,
              bool ignoreProgStart,
              bool ignoreLastPlayPos,
              bool underNetworkControl);

    ProgramInfo *GetCurrentProgram(void) const override; // ScheduleCommon

    void togglePlayListItem(ProgramInfo *pginfo);
    void randomizePlayList(void);

    void processNetworkControlCommands(void);
    void processNetworkControlCommand(const QString &command);

    ProgramInfo *FindProgramInUILists(const ProgramInfo &pginfo);
    ProgramInfo *FindProgramInUILists(uint recordingID,
                                      const QString& recgroup = "NotLiveTV");

    void RemoveProgram(uint recordingID,
                       bool forgetHistory, bool forceMetadataDelete);
    void ShowDeletePopup(DeletePopupType type);
    static void ShowAvailabilityPopup(const ProgramInfo &pginfo);
    void ShowActionPopup(const ProgramInfo &pginfo);

    QString getRecGroupPassword(const QString &recGroup);
    void fillRecGroupPasswordCache(void);

    bool IsUsageUIVisible(void) const;

    void updateIcons(const ProgramInfo *pginfo = nullptr);
    void UpdateUsageUI(void);
    void updateGroupInfo(const QString &groupname, const QString &grouplabel);

    void SetItemIcons(MythUIButtonListItem *item, ProgramInfo* pginfo);
    void UpdateUIListItem(
        ProgramInfo *pginfo,
        bool force_preview_reload);
    void UpdateUIListItem(MythUIButtonListItem *item, bool is_sel,
                          bool force_preview_reload = true);

    void HandlePreviewEvent(const QStringList &list);
    void HandleRecordingRemoveEvent(uint recordingID);
    void HandleRecordingAddEvent(const ProgramInfo &evinfo);
    void HandleUpdateItemEvent(uint recordingId, uint flags);

    void ScheduleUpdateUIList(void);
    void ShowMenu(void) override; // MythScreenType
    bool CreatePopupMenu(const QString &title);
    void DisplayPopupMenu(void);
    //bool CreatePopupMenu(const QString &title, const ProgramInfo &pginfo)
    //    { return CreatePopupMenu(title + CreateProgramInfoString(pginfo)); }
    bool CreatePopupMenuPlaylist(void);

    static QString CreateProgramInfoString(const ProgramInfo &pginfo) ;

    QString extract_job_state(const ProgramInfo &pginfo);
    QString extract_commflag_state(const ProgramInfo &pginfo);


    MythUIButtonList *m_recgroupList          {nullptr};
    MythUIButtonList *m_groupAlphaList       {nullptr};
    MythUIButtonList *m_groupList             {nullptr};
    MythUIButtonList *m_recordingList         {nullptr};

    MythUIText *m_noRecordingsText            {nullptr};

    MythUIImage *m_previewImage               {nullptr};

    MythUIProgressBar *m_recordedProgress     {nullptr};
    MythUIProgressBar *m_watchedProgress      {nullptr};

    QString      m_artHostOverride;
    constexpr static int kNumArtImages = 3;
    std::array<MythUIImage*,kNumArtImages> m_artImage {};
    std::array<QTimer*,kNumArtImages>      m_artTimer {};

    InfoMap m_currentMap;

    // Settings ///////////////////////////////////////////////////////////////
    /// titleView controls showing titles in group list
    bool                m_titleView           {false};
    /// useCategories controls showing categories in group list
    bool                m_useCategories       {false};
    /// useRecGroups controls showing of recording groups in group list
    bool                m_useRecGroups        {false};
    /// use the Watch List as the initial view
    bool                m_watchListStart      {false};
    /// exclude recording not marked for auto-expire from the Watch List
    bool                m_watchListAutoExpire {false};
    /// add 1 to the Watch List scord up to this many days
    int                 m_watchListMaxAge     {60};
    /// adjust exclusion of a title from the Watch List after a delete
    std::chrono::hours  m_watchListBlackOut   {std::chrono::days(2)};
    /// allOrder controls the ordering of the "All Programs" list
    int                 m_allOrder;
    /// listOrder controls the ordering of the recordings in the list
    int                 m_listOrder           {1};

    // Group alphabet support
    QString              m_currentLetter;
    QMap<QString, QString> m_groupAlphabet;

    // Recording Group settings
    QString             m_groupDisplayName;
    QString             m_recGroup;
    QString             m_curGroupPassword;
    QString             m_newRecGroup;
    QString             m_watchGroupName;
    QString             m_watchGroupLabel;
    ViewMask            m_viewMask            {VIEW_TITLES};

    // Popup support //////////////////////////////////////////////////////////
    // General popup support
    MythDialogBox      *m_menuDialog          {nullptr};
    MythMenu           *m_popupMenu           {nullptr};
    MythScreenStack    *m_popupStack          {nullptr};

    bool                m_doToggleMenu        {true};

    // Recording Group popup support
    using RecGroup = QPair<QString, QString>;
    QMap<QString,QString> m_recGroupType;
    QMap<QString,QString> m_recGroupPwCache;

    // State Variables ////////////////////////////////////////////////////////
    // Main Recording List support
    QStringList         m_titleList;  ///< list of pages
    ProgramMap          m_progLists;  ///< lists of programs by page
    int                 m_progsInDB           {0};  ///< total number of recordings in DB
    bool                m_isFilling           {false};

    QStringList         m_recGroups;
    mutable QMutex      m_recGroupsLock;
    int                 m_recGroupIdx         {-1};

    // Other state
    /// Recording[s] currently selected for deletion
    QStringList m_delList;
    /// Group currently selected
    QString m_currentGroup;

    // Play List support
    QList<uint>         m_playList;   ///< list of selected items "play list"
    bool                m_opOnPlaylist        {false};
    QList<uint>         m_playListPlay; ///< list of items being played.

    ProgramInfoCache    m_programInfoCache;

    /// playingSomething is set to true iff a full screen recording is playing
    bool                m_playingSomething    {false};

    /// Does the recording list need to be refilled
    bool                m_needUpdate          {false};

    // Selection state variables
    bool                m_haveGroupInfoSet    {false};

    // Network Control Variables //////////////////////////////////////////////
    mutable QMutex      m_ncLock;
    std::deque<QString> m_networkControlCommands;

    // Other
    TV                 *m_player              {nullptr};
    QStringList         m_playerSelectedNewShow;
    /// Main helper thread
    PlaybackBoxHelper   m_helper;
    /// Outstanding preview image requests
    QSet<QString>       m_previewTokens;

    bool                m_firstGroup          {true};
    bool                m_usingGroupSelector  {false};
    bool                m_groupSelected       {false};
    bool                m_passwordEntered     {false};

    bool                m_alwaysShowWatchedProgress {false};
    // This class caches the contents of the jobqueue table to avoid excessive
    // DB queries each time the PBB selection changes (currently 4 queries per
    // displayed item).  The cache remains valid for 15 seconds
    // (kInvalidateTimeMs).
    class PbbJobQueue
    {
    public:
        PbbJobQueue() { Update(); }
        bool IsJobQueued(int jobType, uint chanid,
                         const QDateTime &recstartts);
        bool IsJobRunning(int jobType, uint chanid,
                          const QDateTime &recstartts);
        bool IsJobQueuedOrRunning(int jobType, uint chanid,
                                  const QDateTime &recstartts);
    private:
        static constexpr std::chrono::milliseconds kInvalidateTimeMs { 15s };
        void Update();
        QDateTime m_lastUpdated;
        // Maps <chanid, recstartts> to a set of JobQueueEntry values.
        using MapType = QMultiMap<QPair<uint, QDateTime>, JobQueueEntry>;
        MapType m_jobs;
    } m_jobQueue;
};

class GroupSelector : public MythScreenType
{
    Q_OBJECT

  public:
    GroupSelector(MythScreenStack *lparent, QString label,
                  QStringList list, QStringList data,
                  QString selected)
        : MythScreenType(lparent, "groupselector"), m_label(std::move(label)),
          m_list(std::move(list)), m_data(std::move(data)),
          m_selected(std::move(selected)) {}

    bool Create(void) override; // MythScreenType

  signals:
    void result(QString);

  protected slots:
    void AcceptItem(MythUIButtonListItem *item);

  private:
    void loadGroups(void);

    QString m_label;
    QStringList m_list;
    QStringList m_data;
    QString m_selected;
};

class ChangeView : public MythScreenType
{
    Q_OBJECT

  public:
    ChangeView(MythScreenStack *lparent, PlaybackBox *parentScreen,
               int viewMask)
        : MythScreenType(lparent, "changeview"),
          m_parentScreen(parentScreen), m_viewMask(viewMask) {}

    bool Create(void) override; // MythScreenType

  signals:
    void save();

  protected slots:
    void SaveChanges(void);

  private:
    PlaybackBox *m_parentScreen;
    int m_viewMask;
};

class PasswordChange : public MythScreenType
{
    Q_OBJECT

  public:
    PasswordChange(MythScreenStack *lparent, QString  oldpassword)
        : MythScreenType(lparent, "passwordchanger"),
          m_oldPassword(std::move(oldpassword)){}

    bool Create(void) override; // MythScreenType

  signals:
    void result(const QString &);

  protected slots:
    void OldPasswordChanged(void);
    void SendResult(void);

  private:
    MythUITextEdit     *m_oldPasswordEdit {nullptr};
    MythUITextEdit     *m_newPasswordEdit {nullptr};
    MythUIButton       *m_okButton        {nullptr};

    QString m_oldPassword;
};

class RecMetadataEdit : public MythScreenType
{
    Q_OBJECT

  public:
    RecMetadataEdit(MythScreenStack *lparent, ProgramInfo *pginfo);

    bool Create(void) override; // MythScreenType

  signals:
    void result(const QString &, const QString &, const QString &,
                const QString &, uint, uint);

  protected slots:
    void SaveChanges(void);
    void ClearInetref();
    void PerformQuery(void);
    void OnSearchListSelection(const RefCountHandler<MetadataLookup>& lookup);

  private:
    void customEvent(QEvent *event) override; // MythUIType
    void QueryComplete(MetadataLookup *lookup);

    MythUITextEdit     *m_titleEdit       {nullptr};
    MythUITextEdit     *m_subtitleEdit    {nullptr};
    MythUITextEdit     *m_descriptionEdit {nullptr};
    MythUITextEdit     *m_inetrefEdit     {nullptr};
    MythUISpinBox      *m_seasonSpin      {nullptr};
    MythUISpinBox      *m_episodeSpin     {nullptr};
    MythUIBusyDialog   *m_busyPopup       {nullptr};
    MythUIButton       *m_queryButton     {nullptr};

    ProgramInfo        *m_progInfo        {nullptr};
    MythScreenStack    *m_popupStack      {nullptr};
    MetadataFactory    *m_metadataFactory {nullptr};
};

class HelpPopup : public MythScreenType
{
    Q_OBJECT

  public:
    explicit HelpPopup(MythScreenStack *lparent)
        : MythScreenType(lparent, "helppopup") {}

    bool Create(void) override; // MythScreenType

  private:
    void addItem(const QString &state, const QString &text);

    MythUIButtonList *m_iconList {nullptr};
};

#endif
/* vim: set expandtab tabstop=4 shiftwidth=4: */
