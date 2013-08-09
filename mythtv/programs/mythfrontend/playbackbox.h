// -*- Mode: c++ -*-
// vim:set sw=4 ts=4 expandtab:
#ifndef PLAYBACKBOX_H_
#define PLAYBACKBOX_H_

// ANSI C headers
#include <stdint.h> // for [u]int[32,64]_t

// C++ headers
#include <vector>
#include <deque>
using namespace std;

#include <QStringList>
#include <QDateTime>
#include <QObject>
#include <QMutex>
#include <QMap>
#include <QSet>

#include "jobqueue.h"
#include "tv_play.h"

#include "mythscreentype.h"

// mythfrontend
#include "schedulecommon.h"
#include "programinfocache.h"
#include "playbackboxhelper.h"

class QKeyEvent;
class QEvent;
class QTimer;

class MythPlayer;
class RingBuffer;
class ProgramInfo;

class MythUIButtonList;
class MythUIButtonListItem;
class MythUIImage;
class MythUIText;
class MythUITextEdit;
class MythUIButton;
class MythDialogBox;
class MythMenu;

typedef QMap<QString,ProgramList>       ProgramMap;
typedef QMap<QString,QString>           Str2StrMap;

enum {
    kArtworkFanTimeout    = 300,
    kArtworkBannerTimeout = 50,
    kArtworkCoverTimeout  = 50,
};

class PlaybackBox : public ScheduleCommon
{
    Q_OBJECT
    friend class PlaybackBoxListItem;

  public:
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
        kDeleteRecording,
        kStopRecording,
        kForceDeleteRecording,
    } DeletePopupType;

    typedef enum
    {
        kNoFlags       = 0x00,
        kForgetHistory = 0x01,
        kForce         = 0x02,
        kIgnore        = 0x04,
        kAllRemaining  = 0x08,
    } DeleteFlags;

    typedef enum
    {
        kNvpToPlay,
        kNvpToStop,
        kDone
    } killStateType;

    PlaybackBox(MythScreenStack *parent, QString name,
                TV *player = NULL, bool showTV = false);
   ~PlaybackBox(void);

    bool Create(void);
    virtual void Load(void);
    virtual void Init(void);
    bool keyPressEvent(QKeyEvent *);
    void customEvent(QEvent *event);

    void setInitialRecGroup(QString initialGroup) { m_recGroup = initialGroup; }
    static void * RunPlaybackBox(void *player, bool);

  public slots:
    void displayRecGroup(const QString &newRecGroup = "");
    void groupSelectorClosed(void);

  protected slots:
    void updateRecList(MythUIButtonListItem *);
    void ItemSelected(MythUIButtonListItem *item)
        { UpdateUIListItem(item, true); }
    void ItemVisible(MythUIButtonListItem *item);
    void ItemLoaded(MythUIButtonListItem *item);
    void selected(MythUIButtonListItem *item);
    void PlayFromBookmark(MythUIButtonListItem *item = NULL);
    void PlayFromBeginning(MythUIButtonListItem *item = NULL);
    void deleteSelected(MythUIButtonListItem *item);

    void SwitchList(void);

    void ShowGroupPopup(void);
    void customEdit();
    void previous();
    void upcoming();
    void upcomingScheduled();
    void details();
    void StopSelected(void);
    void showMetadataEditor();
    void showGroupFilter();
    void showRecGroupPasswordChanger();
    MythMenu*  createPlayFromMenu();
    MythMenu*  createRecordingMenu();
    MythMenu*  createJobMenu();
    MythMenu*  createTranscodingProfilesMenu();
    MythMenu* createStorageMenu();
    MythMenu* createPlaylistMenu();
    MythMenu* createPlaylistStorageMenu();
    MythMenu* createPlaylistJobMenu();
    void changeProfileAndTranscode(int id);
    void showProgramDetails();
    void showIconHelp();
    void ShowRecGroupChangerUsePlaylist(void)  { ShowRecGroupChanger(true);  }
    void ShowPlayGroupChangerUsePlaylist(void) { ShowPlayGroupChanger(true); }
    void ShowRecGroupChanger(bool use_playlist = false);
    void ShowPlayGroupChanger(bool use_playlist = false);

    void popupClosed(QString which, int reason);

    void doPlayListRandom();

    void askStop();

    void doEditScheduled();
    void doAllowRerecord();

    void askDelete();
    void Undelete(void);
    void Delete(DeleteFlags = kNoFlags);
    void DeleteForgetHistory(void)      { Delete(kForgetHistory); }
    void DeleteForce(void)              { Delete(kForce);         }
    void DeleteIgnore(void)             { Delete(kIgnore);        }
    void DeleteForceAllRemaining(void)
        { Delete((DeleteFlags)((int)kForce |(int)kAllRemaining)); }
    void DeleteIgnoreAllRemaining(void)
        { Delete((DeleteFlags)((int)kIgnore|(int)kAllRemaining)); }

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

    void setGroupFilter(const QString &newRecGroup);
    void setRecGroup(QString newRecGroup);
    void setPlayGroup(QString newPlayGroup);

    void saveRecMetadata(const QString &newTitle, const QString &newSubtitle,
                         const QString &newDescription, const QString &newInetref,
                         uint season, uint episode);

    void SetRecGroupPassword(const QString &newPasswd);

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
    void PlaylistDelete(bool forgetHistory = false);
    void doPlaylistExpireSetting(bool turnOn);
    void doPlaylistExpireSetOn()      { doPlaylistExpireSetting(true);  }
    void doPlaylistExpireSetOff()     { doPlaylistExpireSetting(false); }
    void doPlaylistWatchedSetting(bool turnOn);
    void doPlaylistWatchedSetOn()      { doPlaylistWatchedSetting(true);  }
    void doPlaylistWatchedSetOff()     { doPlaylistWatchedSetting(false); }
    void togglePlayListTitle(void);
    void togglePlayListItem(void);
    void playSelectedPlaylist(bool random);
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

    void UpdateProgressBar(void);

    void PlayX(const ProgramInfo &rec,
               bool ignoreBookmark,
               bool underNetworkControl);

    bool Play(const ProgramInfo &rec,
              bool inPlaylist,
              bool ignoreBookmark,
              bool underNetworkControl);

    ProgramInfo *CurrentItem(void);

    void togglePlayListItem(ProgramInfo *pginfo);
    void randomizePlayList(void);

    void processNetworkControlCommands(void);
    void processNetworkControlCommand(const QString &command);

    ProgramInfo *FindProgramInUILists(const ProgramInfo&);
    ProgramInfo *FindProgramInUILists(const QString &key);
    ProgramInfo *FindProgramInUILists(
        uint chanid, const QDateTime &recstartts,
        QString recgroup = "NotLiveTV");

    void RemoveProgram(uint chanid, const QDateTime &recstartts,
                       bool forgetHistory, bool forceMetadataDelete);
    void ShowDeletePopup(DeletePopupType);
    void ShowAvailabilityPopup(const ProgramInfo&);
    void ShowActionPopup(const ProgramInfo&);

    QString getRecGroupPassword(const QString &recGroup);
    void fillRecGroupPasswordCache(void);

    bool IsUsageUIVisible(void) const;

    void updateIcons(const ProgramInfo *pginfo = NULL);
    void UpdateUsageUI(void);
    void updateGroupInfo(const QString &groupname, const QString &grouplabel);

    void SetItemIcons(MythUIButtonListItem *item, ProgramInfo* pginfo);
    void UpdateUIListItem(
        ProgramInfo *ProgramInfo_pointer_from_FindProgramInUILists,
        bool force_preview_reload);
    void UpdateUIListItem(MythUIButtonListItem *item, bool is_sel,
                          bool force_preview_reload = true);

    void HandlePreviewEvent(const QStringList &list);
    void HandleRecordingRemoveEvent(uint chanid, const QDateTime &recstartts);
    void HandleRecordingAddEvent(const ProgramInfo &evinfo);
    void HandleUpdateProgramInfoEvent(const ProgramInfo &evinfo);
    void HandleUpdateProgramInfoFileSizeEvent(
        uint chanid, const QDateTime &recstartts, uint64_t filesize);

    void ScheduleUpdateUIList(void);
    void ShowMenu(void);
    bool CreatePopupMenu(const QString &title);
    void DisplayPopupMenu(void);
    //bool CreatePopupMenu(const QString &title, const ProgramInfo &pginfo)
    //    { return CreatePopupMenu(title + CreateProgramInfoString(pginfo)); }
    bool CreatePopupMenuPlaylist(void);

    QString CreateProgramInfoString(const ProgramInfo &program) const;


    QRegExp m_prefixes;   ///< prefixes to be ignored when sorting
    QRegExp m_titleChaff; ///< stuff to remove for search rules

    MythUIButtonList *m_recgroupList;
    MythUIButtonList *m_groupList;
    MythUIButtonList *m_recordingList;

    MythUIText *m_noRecordingsText;

    MythUIImage *m_previewImage;

    QString      m_artHostOverride;
    MythUIImage *m_artImage[3];
    QTimer      *m_artTimer[3];

    InfoMap m_currentMap;

    // Settings ///////////////////////////////////////////////////////////////
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
    /// allOrder controls the ordering of the "All Programs" list
    int                 m_allOrder;
    /// listOrder controls the ordering of the recordings in the list
    int                 m_listOrder;

    // Recording Group settings
    QString             m_groupDisplayName;
    QString             m_recGroup;
    QString             m_curGroupPassword;
    QString             m_newRecGroup;
    QString             m_watchGroupName;
    QString             m_watchGroupLabel;
    ViewMask            m_viewMask;

    // Popup support //////////////////////////////////////////////////////////
    // General popup support
    MythDialogBox      *m_menuDialog;
    MythMenu           *m_popupMenu;
    MythScreenStack    *m_popupStack;

    bool m_doToggleMenu;

    // Recording Group popup support
    QMap<QString,QString> m_recGroupType;
    QMap<QString,QString> m_recGroupPwCache;

    // State Variables ////////////////////////////////////////////////////////
    // Main Recording List support
    QStringList         m_titleList;  ///< list of pages
    ProgramMap          m_progLists;  ///< lists of programs by page
    int                 m_progsInDB;  ///< total number of recordings in DB
    bool                m_isFilling;

    QStringList         m_recGroups;
    mutable QMutex      m_recGroupsLock;
    int                 m_recGroupIdx;

    // Other state
    /// Program[s] currently selected for deletion
    QStringList m_delList;
    /// Group currently selected
    QString m_currentGroup;

    // Play List support
    QStringList         m_playList;   ///< list of selected items "play list"
    bool                m_op_on_playlist;
    QStringList         m_playListPlay; ///< list of items being played.

    ProgramInfoCache    m_programInfoCache;

    /// playingSomething is set to true iff a full screen recording is playing
    bool                m_playingSomething;

    /// Does the recording list need to be refilled
    bool m_needUpdate;

    // Selection state variables
    bool                m_haveGroupInfoSet;

    // Network Control Variables //////////////////////////////////////////////
    mutable QMutex      m_ncLock;
    deque<QString>      m_networkControlCommands;

    // Other
    TV                 *m_player;
    QStringList         m_player_selected_new_show;
    /// Main helper thread
    PlaybackBoxHelper   m_helper;
    /// Outstanding preview image requests
    QSet<QString>       m_preview_tokens;

    bool                m_firstGroup;
    bool                m_usingGroupSelector;
    bool                m_groupSelected;
    bool                m_passwordEntered;
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
    void result(const QString &, const QString &, const QString &,
                const QString &, uint, uint);

  protected slots:
    void SaveChanges(void);

  private:
    MythUITextEdit     *m_titleEdit;
    MythUITextEdit     *m_subtitleEdit;
    MythUITextEdit     *m_descriptionEdit;
    MythUITextEdit     *m_inetrefEdit;
    MythUISpinBox      *m_seasonSpin;
    MythUISpinBox      *m_episodeSpin;

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
