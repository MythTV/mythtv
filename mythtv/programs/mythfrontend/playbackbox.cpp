#include "playbackbox.h"

// C++
#include <algorithm>
#include <array>

// QT
#include <QCoreApplication>
#include <QDateTime>
#include <QLocale>
#include <QTimer>
#include <QMap>

// MythTV
#ifdef _MSC_VER
#  include "libmythbase/compat.h"               // for random
#endif
#include "libmythbase/mconcurrent.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythdbcon.h"
#include "libmythbase/mythevent.h"              // for MythEvent, etc
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythrandom.h"
#include "libmythbase/programinfo.h"
#include "libmythbase/programtypes.h"           // for AudioProps, SubtitleTypes, etc
#include "libmythbase/recordingtypes.h"
#include "libmythbase/stringutil.h"
#include "libmythmetadata/mythuimetadataresults.h"
#include "libmythtv/playgroup.h"
#include "libmythtv/previewgeneratorqueue.h"
#include "libmythtv/recordinginfo.h"
#include "libmythtv/recordingrule.h"
#include "libmythtv/tv.h"
#include "libmythtv/tv_actions.h"               // for ACTION_LISTRECORDEDEPISODES, etc
#include "libmythui/mythdialogbox.h"
#include "libmythui/mythmainwindow.h"           // for GetMythMainWindow, etc
#include "libmythui/mythnotificationcenter.h"   // for ShowNotificationError, etc
#include "libmythui/mythprogressdialog.h"
#include "libmythui/mythscreenstack.h"          // for MythScreenStack
#include "libmythui/mythuiactions.h"            // for ACTION_1
#include "libmythui/mythuibutton.h"
#include "libmythui/mythuibuttonlist.h"
#include "libmythui/mythuicheckbox.h"
#include "libmythui/mythuiimage.h"
#include "libmythui/mythuiprogressbar.h"
#include "libmythui/mythuispinbox.h"
#include "libmythui/mythuistatetype.h"
#include "libmythui/mythuitext.h"
#include "libmythui/mythuitextedit.h"

//  MythFrontend
#include "playbackboxlistitem.h"

#define LOC      QString("PlaybackBox: ")
#define LOC_WARN QString("PlaybackBox Warning: ")
#define LOC_ERR  QString("PlaybackBox Error: ")

static const QString sLocation = "Playback Box";

static int comp_programid(const ProgramInfo *a, const ProgramInfo *b)
{
    if (a->GetProgramID() == b->GetProgramID())
        return (a->GetRecordingStartTime() <
                b->GetRecordingStartTime() ? 1 : -1);
    return (a->GetProgramID() < b->GetProgramID() ? 1 : -1);
}

static int comp_programid_rev(const ProgramInfo *a, const ProgramInfo *b)
{
    if (a->GetProgramID() == b->GetProgramID())
        return (a->GetRecordingStartTime() >
                b->GetRecordingStartTime() ? 1 : -1);
    return (a->GetProgramID() > b->GetProgramID() ? 1 : -1);
}

static int comp_originalAirDate(const ProgramInfo *a, const ProgramInfo *b)
{
    QDate dt1 = (a->GetOriginalAirDate().isValid()) ?
        a->GetOriginalAirDate() : a->GetScheduledStartTime().date();
    QDate dt2 = (b->GetOriginalAirDate().isValid()) ?
        b->GetOriginalAirDate() : b->GetScheduledStartTime().date();

    if (dt1 == dt2)
        return (a->GetRecordingStartTime() <
                b->GetRecordingStartTime() ? 1 : -1);
    return (dt1 < dt2 ? 1 : -1);
}

static int comp_originalAirDate_rev(const ProgramInfo *a, const ProgramInfo *b)
{
    QDate dt1 = (a->GetOriginalAirDate().isValid()) ?
        a->GetOriginalAirDate() : a->GetScheduledStartTime().date();
    QDate dt2 = (b->GetOriginalAirDate().isValid()) ?
        b->GetOriginalAirDate() : b->GetScheduledStartTime().date();

    if (dt1 == dt2)
        return (a->GetRecordingStartTime() >
                b->GetRecordingStartTime() ? 1 : -1);
    return (dt1 > dt2 ? 1 : -1);
}

static int comp_recpriority2(const ProgramInfo *a, const ProgramInfo *b)
{
    if (a->GetRecordingPriority2() == b->GetRecordingPriority2())
        return (a->GetRecordingStartTime() <
                b->GetRecordingStartTime() ? 1 : -1);
    return (a->GetRecordingPriority2() <
            b->GetRecordingPriority2() ? 1 : -1);
}

static int comp_recordDate(const ProgramInfo *a, const ProgramInfo *b)
{
    if (a->GetScheduledStartTime().date() == b->GetScheduledStartTime().date())
        return (a->GetRecordingStartTime() <
                b->GetRecordingStartTime() ? 1 : -1);
    return (a->GetScheduledStartTime().date() <
            b->GetScheduledStartTime().date() ? 1 : -1);
}

static int comp_recordDate_rev(const ProgramInfo *a, const ProgramInfo *b)
{
    if (a->GetScheduledStartTime().date() == b->GetScheduledStartTime().date())
        return (a->GetRecordingStartTime() >
                b->GetRecordingStartTime() ? 1 : -1);
    return (a->GetScheduledStartTime().date() >
            b->GetScheduledStartTime().date() ? 1 : -1);
}

static int comp_season(const ProgramInfo *a, const ProgramInfo *b)
{
    if (a->GetSeason() == 0 || b->GetSeason() == 0)
        return comp_originalAirDate(a, b);
    if (a->GetSeason() != b->GetSeason())
        return (a->GetSeason() < b->GetSeason() ? 1 : -1);
    if (a->GetEpisode() == 0 && b->GetEpisode() == 0)
        return comp_originalAirDate(a, b);
    return (a->GetEpisode() < b->GetEpisode() ? 1 : -1);
}

static int comp_season_rev(const ProgramInfo *a, const ProgramInfo *b)
{
    if (a->GetSeason() == 0 || b->GetSeason() == 0)
        return comp_originalAirDate_rev(a, b);
    if (a->GetSeason() != b->GetSeason())
        return (a->GetSeason() > b->GetSeason() ? 1 : -1);
    if (a->GetEpisode() == 0 && b->GetEpisode() == 0)
        return comp_originalAirDate_rev(a, b);
    return (a->GetEpisode() > b->GetEpisode() ? 1 : -1);
}

static bool comp_programid_less_than(
    const ProgramInfo *a, const ProgramInfo *b)
{
    return comp_programid(a, b) < 0;
}

static bool comp_programid_rev_less_than(
    const ProgramInfo *a, const ProgramInfo *b)
{
    return comp_programid_rev(a, b) < 0;
}

static bool comp_originalAirDate_less_than(
    const ProgramInfo *a, const ProgramInfo *b)
{
    return comp_originalAirDate(a, b) < 0;
}

static bool comp_originalAirDate_rev_less_than(
    const ProgramInfo *a, const ProgramInfo *b)
{
    return comp_originalAirDate_rev(a, b) < 0;
}

static bool comp_recpriority2_less_than(
    const ProgramInfo *a, const ProgramInfo *b)
{
    return comp_recpriority2(a, b) < 0;
}

static bool comp_recordDate_less_than(
    const ProgramInfo *a, const ProgramInfo *b)
{
    return comp_recordDate(a, b) < 0;
}

static bool comp_recordDate_rev_less_than(
    const ProgramInfo *a, const ProgramInfo *b)
{
    return comp_recordDate_rev(a, b) < 0;
}

static bool comp_season_less_than(
    const ProgramInfo *a, const ProgramInfo *b)
{
    return comp_season(a, b) < 0;
}

static bool comp_season_rev_less_than(
    const ProgramInfo *a, const ProgramInfo *b)
{
    return comp_season_rev(a, b) < 0;
}

static const std::array<const uint,3> s_artDelay
    { kArtworkFanTimeout, kArtworkBannerTimeout, kArtworkCoverTimeout,};

static PlaybackBox::ViewMask m_viewMaskToggle(PlaybackBox::ViewMask mask,
        PlaybackBox::ViewMask toggle)
{
    // can only toggle a single bit at a time
    if ((mask & toggle))
        return (PlaybackBox::ViewMask)(mask & ~toggle);
    return (PlaybackBox::ViewMask)(mask | toggle);
}

static QString construct_sort_title(
    QString title, const QString& sortTitle, PlaybackBox::ViewMask viewmask,
    PlaybackBox::ViewTitleSort sortType, int recpriority)
{
    if (title.isEmpty())
        return title;

    QString sTitle = sortTitle.isEmpty()
        ? title : sortTitle + " - " + title;

    if (viewmask == PlaybackBox::VIEW_TITLES &&
            sortType == PlaybackBox::TitleSortRecPriority)
    {
        // Also incorporate recpriority (reverse numeric sort). In
        // case different episodes of a recording schedule somehow
        // have different recpriority values (e.g., manual fiddling
        // with database), the title will appear once for each
        // distinct recpriority value among its episodes.
        //
        // Deal with QMap sorting. Positive recpriority values have a
        // '+' prefix (QMap alphabetically sorts before '-'). Positive
        // recpriority values are "inverted" by subtracting them from
        // 1000, so that high recpriorities are sorted first (QMap
        // alphabetically). For example:
        //
        //      recpriority =>  sort key
        //          95          +905
        //          90          +910
        //          89          +911
        //           1          +999
        //           0          -000
        //          -5          -005
        //         -10          -010
        //         -99          -099

        QString sortprefix;
        if (recpriority > 0)
            sortprefix = QString("+%1").arg(1000 - recpriority, 3, 10, QChar('0'));
        else
            sortprefix = QString("-%1").arg(-recpriority, 3, 10, QChar('0'));

        sTitle = sortprefix + '-' + sTitle;
    }
    return sTitle;
}

static QString extract_main_state(const ProgramInfo &pginfo, const TV *player)
{
    QString state("normal");
    if (pginfo.GetFilesize() == 0)
        state = "error";
    else if (pginfo.GetRecordingStatus() == RecStatus::Recording ||
             pginfo.GetRecordingStatus() == RecStatus::Tuning ||
             pginfo.GetRecordingStatus() == RecStatus::Failing)
        state = "running";

    if (((pginfo.GetRecordingStatus() != RecStatus::Recording) &&
         (pginfo.GetAvailableStatus() != asAvailable) &&
         (pginfo.GetAvailableStatus() != asNotYetAvailable)) ||
        (player && player->IsSameProgram(&pginfo)))
    {
        state = "disabled";
    }

    if ((state == "normal" || state == "running") &&
        pginfo.GetVideoProperties() & VID_DAMAGED)
    {
        state = "warning";
    }

    return state;
}

QString PlaybackBox::extract_job_state(const ProgramInfo &pginfo)
{
    QString job = "default";

    if (pginfo.GetRecordingStatus() == RecStatus::Recording ||
        pginfo.GetRecordingStatus() == RecStatus::Tuning ||
        pginfo.GetRecordingStatus() == RecStatus::Failing)
        job = "recording";
    else if (m_jobQueue.IsJobQueuedOrRunning(
                 JOB_TRANSCODE, pginfo.GetChanID(),
                 pginfo.GetRecordingStartTime()))
        job = "transcoding";
    else if (m_jobQueue.IsJobQueuedOrRunning(
                 JOB_COMMFLAG,  pginfo.GetChanID(),
                 pginfo.GetRecordingStartTime()))
        job = "commflagging";

    return job;
}

QString PlaybackBox::extract_commflag_state(const ProgramInfo &pginfo)
{
    // commflagged can be yes, no or processing
    if (m_jobQueue.IsJobRunning(JOB_COMMFLAG, pginfo.GetChanID(),
                                pginfo.GetRecordingStartTime()))
        return "running";
    if (m_jobQueue.IsJobQueued(JOB_COMMFLAG, pginfo.GetChanID(),
                               pginfo.GetRecordingStartTime()))
        return "queued";

    return ((pginfo.GetProgramFlags() & FL_COMMFLAG) ? "yes" : "no");
}


static QString extract_subtitle(
    const ProgramInfo &pginfo, const QString &groupname)
{
    QString subtitle;
    if (groupname != pginfo.GetTitle().toLower())
    {
        subtitle = pginfo.toString(ProgramInfo::kTitleSubtitle, " - ");
    }
    else
    {
        subtitle = pginfo.GetSubtitle();
        if (subtitle.trimmed().isEmpty())
            subtitle = pginfo.GetTitle();
    }
    return subtitle;
}

static void push_onto_del(QStringList &list, const ProgramInfo &pginfo)
{
    list.clear();
    list.push_back(QString::number(pginfo.GetRecordingID()));
    list.push_back(QString() /* force Delete */);
    list.push_back(QString()); /* forget history */
}

static bool extract_one_del(QStringList &list, uint &recordingID)
{
    if (list.size() < 3)
    {
        list.clear();
        return false;
    }

    recordingID = list[0].toUInt();

    list.pop_front();
    list.pop_front();
    list.pop_front();

    if (recordingID == 0U) {
        LOG(VB_GENERAL, LOG_ERR, LOC + "extract_one_del() invalid entry");
        return false;
    }
    return true;
}

void * PlaybackBox::RunPlaybackBox(void * player, bool showTV)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *pbb = new PlaybackBox(mainStack,"playbackbox", (TV *)player, showTV);

    if (pbb->Create())
        mainStack->AddScreen(pbb);
    else
        delete pbb;

    return nullptr;
}

PlaybackBox::PlaybackBox(MythScreenStack *parent, const QString& name,
                         TV *player, bool /*showTV*/)
    : ScheduleCommon(parent, name),
      // Recording Group settings
      m_groupDisplayName(ProgramInfo::i18n("All Programs")),
      m_recGroup("All Programs"),
      m_watchGroupName(tr("Watch List")),
      m_watchGroupLabel(m_watchGroupName.toLower()),

      // Other state
      m_programInfoCache(this),
      // Other
      m_helper(this)
{
    for (size_t i = 0; i < kNumArtImages; i++)
    {
        m_artImage[i] = nullptr;
        m_artTimer[i] = new QTimer(this);
        m_artTimer[i]->setSingleShot(true);
    }

    m_recGroup           = gCoreContext->GetSetting("DisplayRecGroup",
                                                "All Programs");
    int pbOrder        = gCoreContext->GetNumSetting("PlayBoxOrdering", 3);
    // Split out sort order modes, wacky order for backward compatibility
    m_listOrder = (pbOrder >> 1) ^ (m_allOrder = pbOrder & 1);
    m_watchListStart     = gCoreContext->GetBoolSetting("PlaybackWLStart", false);

    m_watchListAutoExpire= gCoreContext->GetBoolSetting("PlaybackWLAutoExpire", false);
    m_watchListMaxAge    = gCoreContext->GetNumSetting("PlaybackWLMaxAge", 60);
    m_watchListBlackOut  = gCoreContext->GetDurSetting<std::chrono::days>("PlaybackWLBlackOut",
                                                                          std::chrono::days(2));

    bool displayCat  = gCoreContext->GetBoolSetting("DisplayRecGroupIsCategory", false);

    m_viewMask = (ViewMask)gCoreContext->GetNumSetting(
                                    "DisplayGroupDefaultViewMask",
                                    VIEW_TITLES | VIEW_WATCHED);

    // Translate these external settings into mask values
    if (gCoreContext->GetBoolSetting("PlaybackWatchList", true) &&
        ((m_viewMask & VIEW_WATCHLIST) == 0))
    {
        m_viewMask = (ViewMask)(m_viewMask | VIEW_WATCHLIST);
        gCoreContext->SaveSetting("DisplayGroupDefaultViewMask", (int)m_viewMask);
    }
    else if (! gCoreContext->GetBoolSetting("PlaybackWatchList", true) &&
             ((m_viewMask & VIEW_WATCHLIST) != 0))
    {
        m_viewMask = (ViewMask)(m_viewMask & ~VIEW_WATCHLIST);
        gCoreContext->SaveSetting("DisplayGroupDefaultViewMask", (int)m_viewMask);
    }

    // This setting is deprecated in favour of viewmask, this just ensures the
    // that it is converted over when upgrading from earlier versions
    if (gCoreContext->GetBoolSetting("LiveTVInAllPrograms",false) &&
        ((m_viewMask & VIEW_LIVETVGRP) == 0))
    {
        m_viewMask = (ViewMask)(m_viewMask | VIEW_LIVETVGRP);
        gCoreContext->SaveSetting("DisplayGroupDefaultViewMask", (int)m_viewMask);
    }

    if (gCoreContext->GetBoolSetting("MasterBackendOverride", false))
        m_artHostOverride = gCoreContext->GetMasterHostName();

    if (player)
    {
        m_player = player;
        m_player->IncrRef();
        QString tmp = m_player->GetRecordingGroup();
        if (!tmp.isEmpty())
            m_recGroup = tmp;
    }

    // recording group stuff
    m_recGroupIdx = -1;
    m_recGroupType.clear();
    m_recGroupType[m_recGroup] =
            (displayCat && m_recGroup != "All Programs") ? "category" : "recgroup";
    m_groupDisplayName = ProgramInfo::i18n(m_recGroup);

    fillRecGroupPasswordCache();

    m_alwaysShowWatchedProgress = gCoreContext->GetBoolSetting("AlwaysShowWatchedProgress", false);

    // misc setup
    gCoreContext->addListener(this);

    m_popupStack = GetMythMainWindow()->GetStack("popup stack");
}

PlaybackBox::~PlaybackBox(void)
{
    gCoreContext->removeListener(this);
    PreviewGeneratorQueue::RemoveListener(this);

    for (size_t i = 0; i < kNumArtImages; i++)
    {
        m_artTimer[i]->disconnect(this);
        m_artTimer[i] = nullptr;
        m_artImage[i] = nullptr;
    }

    if (m_player)
    {
        emit m_player->RequestEmbedding(false, {}, m_playerSelectedNewShow);
        m_player->DecrRef();
    }
}

bool PlaybackBox::Create()
{
    if (!LoadWindowFromXML("recordings-ui.xml", "watchrecordings", this))
        return false;

    m_recgroupList  = dynamic_cast<MythUIButtonList *> (GetChild("recgroups"));
    m_groupAlphaList = dynamic_cast<MythUIButtonList *> (GetChild("groupsAlphabet"));
    m_groupList     = dynamic_cast<MythUIButtonList *> (GetChild("groups"));
    m_recordingList = dynamic_cast<MythUIButtonList *> (GetChild("recordings"));

    m_noRecordingsText = dynamic_cast<MythUIText *> (GetChild("norecordings"));

    m_previewImage = dynamic_cast<MythUIImage *>(GetChild("preview"));
    m_recordedProgress = dynamic_cast<MythUIProgressBar *>(GetChild("recordedprogressbar"));
    m_watchedProgress = dynamic_cast<MythUIProgressBar *>(GetChild("watchedprogressbar"));
    m_artImage[kArtworkFanart] = dynamic_cast<MythUIImage*>(GetChild("fanart"));
    m_artImage[kArtworkBanner] = dynamic_cast<MythUIImage*>(GetChild("banner"));
    m_artImage[kArtworkCoverart]= dynamic_cast<MythUIImage*>(GetChild("coverart"));

    if (!m_recordingList || !m_groupList)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Theme is missing critical theme elements.");
        return false;
    }

    if (m_recgroupList)
    {
        if (gCoreContext->GetBoolSetting("RecGroupsFocusable", false))
        {
            connect(m_recgroupList, &MythUIButtonList::itemSelected,
                    this, &PlaybackBox::updateRecGroup);
        }
        else
        {
            m_recgroupList->SetCanTakeFocus(false);
        }
    }

    if (m_groupAlphaList)
    {
        connect(m_groupAlphaList, &MythUIButtonList::itemSelected,
                this, &PlaybackBox::selectUIGroupsAlphabet);
    }

    connect(m_groupList, &MythUIButtonList::itemSelected,
            this, &PlaybackBox::updateRecList);
    connect(m_groupList, &MythUIButtonList::itemClicked,
            this, &PlaybackBox::SwitchList);
    connect(m_recordingList, &MythUIButtonList::itemSelected,
            this, &PlaybackBox::ItemSelected);
    connect(m_recordingList, &MythUIButtonList::itemClicked,
            this, qOverload<>(&PlaybackBox::PlayFromAnyMark));
    connect(m_recordingList, &MythUIButtonList::itemVisible,
            this, &PlaybackBox::ItemVisible);
    connect(m_recordingList, &MythUIButtonList::itemLoaded,
            this, &PlaybackBox::ItemLoaded);

    // connect up timers...
    connect(m_artTimer[kArtworkFanart],   &QTimer::timeout, this, &PlaybackBox::fanartLoad);
    connect(m_artTimer[kArtworkBanner],   &QTimer::timeout, this, &PlaybackBox::bannerLoad);
    connect(m_artTimer[kArtworkCoverart], &QTimer::timeout, this, &PlaybackBox::coverartLoad);

    BuildFocusList();
    SetFocusWidget(m_groupList);
    m_programInfoCache.ScheduleLoad(false);
    LoadInBackground();

    if (m_player)
        emit m_player->RequestEmbedding(true);
    return true;
}

void PlaybackBox::Load(void)
{
    m_programInfoCache.WaitForLoadToComplete();
    PreviewGeneratorQueue::AddListener(this);
}

void PlaybackBox::Init()
{
    m_groupList->SetLCDTitles(tr("Groups"));
    m_recordingList->SetLCDTitles(tr("Recordings"),
                                  "titlesubtitle|shortdate|starttime");

    m_recordingList->SetSearchFields("titlesubtitle");

    if (gCoreContext->GetNumSetting("QueryInitialFilter", 0) == 1)
        showGroupFilter();
    else if (!m_player)
        displayRecGroup(m_recGroup);
    else
    {
        UpdateUILists();

        if ((m_titleList.size() <= 1) && (m_progsInDB > 0))
        {
            m_recGroup.clear();
            showGroupFilter();
        }
    }

    if (!gCoreContext->GetBoolSetting("PlaybackBoxStartInTitle", false))
        SetFocusWidget(m_recordingList);
}

void PlaybackBox::SwitchList()
{
    if (GetFocusWidget() == m_groupList)
        SetFocusWidget(m_recordingList);
    else if (GetFocusWidget() == m_recordingList ||
             GetFocusWidget() == m_groupAlphaList)
        SetFocusWidget(m_groupList);
}

void PlaybackBox::displayRecGroup(const QString &newRecGroup)
{
    m_groupSelected = true;

    QString password = getRecGroupPassword(newRecGroup);

    m_newRecGroup = newRecGroup;
    if (m_curGroupPassword != password && !password.isEmpty())
    {
        MythScreenStack *popupStack =
                                GetMythMainWindow()->GetStack("popup stack");

        QString label = tr("Password for group '%1':").arg(newRecGroup);

        auto *pwd = new MythTextInputDialog(popupStack, label, FilterNone, true);

        connect(pwd, &MythTextInputDialog::haveResult,
                this, &PlaybackBox::checkPassword);
        connect(pwd, &MythScreenType::Exiting,
                this, &PlaybackBox::passwordClosed);

        m_passwordEntered = false;

        if (pwd->Create())
            popupStack->AddScreen(pwd, false);

        return;
    }

    setGroupFilter(newRecGroup);
}

void PlaybackBox::checkPassword(const QString &password)
{
    if (password == getRecGroupPassword(m_newRecGroup))
    {
        m_curGroupPassword = password;
        m_passwordEntered = true;
        setGroupFilter(m_newRecGroup);
    }
}

void PlaybackBox::passwordClosed(void)
{
    if (!m_passwordEntered &&
        (m_usingGroupSelector || m_firstGroup))
        showGroupFilter();
}

void PlaybackBox::updateGroupInfo(const QString &groupname,
                                  const QString &grouplabel)
{
    InfoMap infoMap;
    QString desc;

    infoMap["group"] = m_groupDisplayName;
    infoMap["title"] = grouplabel;
    infoMap["show"] =
        groupname.isEmpty() ? ProgramInfo::i18n("All Programs") : grouplabel;
    int countInGroup = m_progLists[groupname].size();

    if (m_artImage[kArtworkFanart])
    {
        if (!groupname.isEmpty() && !m_progLists[groupname].empty())
        {
            ProgramInfo *pginfo = *m_progLists[groupname].begin();

            QString fn = m_helper.LocateArtwork(
                pginfo->GetInetRef(), pginfo->GetSeason(), kArtworkFanart, nullptr, groupname);

            if (fn.isEmpty())
            {
                m_artTimer[kArtworkFanart]->stop();
                m_artImage[kArtworkFanart]->Reset();
            }
            else if (m_artImage[kArtworkFanart]->GetFilename() != fn)
            {
                m_artImage[kArtworkFanart]->SetFilename(fn);
                m_artTimer[kArtworkFanart]->start(kArtworkFanTimeout);
            }
        }
        else
        {
            m_artImage[kArtworkFanart]->Reset();
        }
    }


    if (countInGroup >= 1)
    {
        ProgramList  group     = m_progLists[groupname];
        float        groupSize = 0.0;

        for (auto *info : group)
        {
            if (info)
            {
                uint64_t filesize = info->GetFilesize();
// This query should be unnecessary if the ProgramInfo Updater is working
//                 if (filesize == 0 || info->GetRecordingStatus() == RecStatus::Recording)
//                 {
//                     filesize = info->QueryFilesize();
//                     info->SetFilesize(filesize);
//                 }
                groupSize += filesize;
            }
        }

        desc = tr("There is/are %n recording(s) in this display "
                  "group, which consume(s) %1 GiB.", "", countInGroup)
               .arg(groupSize / 1024.0F / 1024.0F / 1024.0F, 0, 'f', 2);
    }
    else
    {
        desc = tr("There is no recording in this display group.");
    }

    infoMap["description"] = desc;
    infoMap["rec_count"] = QString("%1").arg(countInGroup);

    ResetMap(m_currentMap);
    SetTextFromMap(infoMap);
    m_currentMap = infoMap;

    MythUIStateType *ratingState = dynamic_cast<MythUIStateType*>
                                                (GetChild("ratingstate"));
    if (ratingState)
        ratingState->Reset();

    MythUIStateType *jobState = dynamic_cast<MythUIStateType*>
                                                (GetChild("jobstate"));
    if (jobState)
        jobState->Reset();

    if (m_previewImage)
        m_previewImage->Reset();

    if (m_artImage[kArtworkBanner])
        m_artImage[kArtworkBanner]->Reset();

    if (m_artImage[kArtworkCoverart])
        m_artImage[kArtworkCoverart]->Reset();

    updateIcons();
}

void PlaybackBox::UpdateUIListItem(ProgramInfo *pginfo,
                                   bool force_preview_reload)
{
    if (!pginfo)
        return;

    MythUIButtonListItem *item =
        m_recordingList->GetItemByData(QVariant::fromValue(pginfo));

    if (item)
    {
        MythUIButtonListItem *sel_item =
            m_recordingList->GetItemCurrent();
        UpdateUIListItem(item, item == sel_item, force_preview_reload);
    }
    else
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("UpdateUIListItem called with a title unknown "
                    "to us in m_recordingList\n\t\t\t%1")
                .arg(pginfo->toString(ProgramInfo::kTitleSubtitle)));
    }
}

static const std::array<const std::string,9> disp_flags
{
    "playlist", "watched", "preserve",
    "cutlist", "autoexpire", "editing",
    "bookmark", "inuse", "transcoded"
};

void PlaybackBox::SetItemIcons(MythUIButtonListItem *item, ProgramInfo* pginfo)
{
    std::array<bool,disp_flags.size()> disp_flag_stat {};

    disp_flag_stat[0] = m_playList.contains(pginfo->GetRecordingID());
    disp_flag_stat[1] = pginfo->IsWatched();
    disp_flag_stat[2] = pginfo->IsPreserved();
    disp_flag_stat[3] = pginfo->HasCutlist();
    disp_flag_stat[4] = pginfo->IsAutoExpirable();
    disp_flag_stat[5] = ((pginfo->GetProgramFlags() & FL_EDITING) != 0U);
    disp_flag_stat[6] = pginfo->IsBookmarkSet();
    disp_flag_stat[7] = pginfo->IsInUsePlaying();
    disp_flag_stat[8] = ((pginfo->GetProgramFlags() & FL_TRANSCODED) != 0U);

    for (size_t i = 0; i < disp_flags.size(); ++i)
        item->DisplayState(disp_flag_stat[i] ? "yes" : "no",
                           QString::fromStdString(disp_flags[i]));
}

void PlaybackBox::UpdateUIListItem(MythUIButtonListItem *item,
                                   bool is_sel, bool force_preview_reload)
{
    if (!item)
        return;

    auto *pginfo = item->GetData().value<ProgramInfo *>();

    if (!pginfo)
        return;

    QString state = extract_main_state(*pginfo, m_player);

    // Update the text, e.g. Title or subtitle may have been changed on another
    // frontend
    if (m_groupList && m_groupList->GetItemCurrent())
    {
        InfoMap infoMap;
        pginfo->ToMap(infoMap);
        item->SetTextFromMap(infoMap);

        QString groupname =
            m_groupList->GetItemCurrent()->GetData().toString();

        QString tempSubTitle  = extract_subtitle(*pginfo, groupname);

        if (groupname == pginfo->GetTitle().toLower())
        {
            item->SetText(tempSubTitle, "titlesubtitle");
            // titlesubtitle will just have the subtitle, so put the full
            // string in titlesubtitlefull, when a theme can then "depend" on.
            item->SetText(pginfo->toString(ProgramInfo::kTitleSubtitle, " - "),
                          "titlesubtitlefull");
        }
    }

    // Recording and availability status
    item->SetFontState(state);
    item->DisplayState(state, "status");

    // Job status (recording, transcoding, flagging)
    QString job = extract_job_state(*pginfo);
    item->DisplayState(job, "jobstate");

    // Flagging status (queued, running, no, yes)
    item->DisplayState(extract_commflag_state(*pginfo), "commflagged");

    SetItemIcons(item, pginfo);

    QString rating = QString::number(pginfo->GetStars(10));

    item->DisplayState(rating, "ratingstate");

    QString oldimgfile = item->GetImageFilename("preview");
    if (oldimgfile.isEmpty() || force_preview_reload)
        m_previewTokens.insert(m_helper.GetPreviewImage(*pginfo));

    if ((GetFocusWidget() == m_recordingList) && is_sel)
    {
        InfoMap infoMap;

        pginfo->CalculateProgress(pginfo->QueryLastPlayPos());

        pginfo->ToMap(infoMap);
        infoMap["group"] = m_groupDisplayName;
        ResetMap(m_currentMap);
        SetTextFromMap(infoMap);
        m_currentMap = infoMap;

        MythUIStateType *ratingState = dynamic_cast<MythUIStateType*>
                                                    (GetChild("ratingstate"));
        if (ratingState)
            ratingState->DisplayState(rating);

        MythUIStateType *jobState = dynamic_cast<MythUIStateType*>
                                                    (GetChild("jobstate"));
        if (jobState)
            jobState->DisplayState(job);

        if (m_previewImage)
        {
            m_previewImage->SetFilename(oldimgfile);
            m_previewImage->Load(true, true);
        }

        if (m_recordedProgress)
            m_recordedProgress->Set(0, 100, pginfo->GetRecordedPercent());
        if (m_watchedProgress)
            m_watchedProgress->Set(0, 100, pginfo->GetWatchedPercent());

        // Handle artwork
        QString arthost;
        for (size_t i = 0; i < kNumArtImages; i++)
        {
            if (!m_artImage[i])
                continue;

            if (arthost.isEmpty())
            {
                arthost = (!m_artHostOverride.isEmpty()) ?
                    m_artHostOverride : pginfo->GetHostname();
            }

            QString fn = m_helper.LocateArtwork(
                pginfo->GetInetRef(), pginfo->GetSeason(),
                (VideoArtworkType)i, pginfo);

            if (fn.isEmpty())
            {
                m_artTimer[i]->stop();
                m_artImage[i]->Reset();
            }
            else if (m_artImage[i]->GetFilename() != fn)
            {
                m_artImage[i]->SetFilename(fn);
                m_artTimer[i]->start(s_artDelay[i]);
            }
        }

        updateIcons(pginfo);
    }
}

void PlaybackBox::ItemLoaded(MythUIButtonListItem *item)
{
    auto *pginfo = item->GetData().value<ProgramInfo*>();
    if (item->GetText("is_item_initialized").isNull())
    {
        QMap<AudioProps, QString> audioFlags;
        audioFlags[AUD_DOLBY]    = "dolby";
        audioFlags[AUD_SURROUND] = "surround";
        audioFlags[AUD_STEREO]   = "stereo";
        audioFlags[AUD_MONO]     = "mono";

        QMap<VideoProps, QString> codecFlags;
        codecFlags[VID_MPEG2] = "mpeg2";
        codecFlags[VID_AVC]   = "avc";
        codecFlags[VID_HEVC]  = "hevc";

        QMap<SubtitleProps, QString> subtitleFlags;
        subtitleFlags[SUB_SIGNED]   = "deafsigned";
        subtitleFlags[SUB_ONSCREEN] = "onscreensub";
        subtitleFlags[SUB_NORMAL]   = "subtitles";
        subtitleFlags[SUB_HARDHEAR] = "cc";

        QString groupname =
            m_groupList->GetItemCurrent()->GetData().toString();

        QString state = extract_main_state(*pginfo, m_player);

        item->SetFontState(state);

        InfoMap infoMap;
        pginfo->ToMap(infoMap);
        item->SetTextFromMap(infoMap);

        QString tempSubTitle  = extract_subtitle(*pginfo, groupname);

        if (groupname == pginfo->GetTitle().toLower())
        {
            item->SetText(tempSubTitle, "titlesubtitle");
            // titlesubtitle will just have the subtitle, so put the full
            // string in titlesubtitlefull, when a theme can then "depend" on.
            item->SetText(pginfo->toString(ProgramInfo::kTitleSubtitle, " - "),
                          "titlesubtitlefull");
        }

        item->DisplayState(state, "status");

        item->DisplayState(QString::number(pginfo->GetStars(10)),
                           "ratingstate");

        SetItemIcons(item, pginfo);

        QMap<AudioProps, QString>::iterator ait;
        for (ait = audioFlags.begin(); ait != audioFlags.end(); ++ait)
        {
            if (pginfo->GetAudioProperties() & ait.key())
                item->DisplayState(ait.value(), "audioprops");
        }

        uint props = pginfo->GetVideoProperties();

        QMap<VideoProps, QString>::iterator cit;
        for (cit = codecFlags.begin(); cit != codecFlags.end(); ++cit)
        {
            if (props & cit.key())
            {
                item->DisplayState(cit.value(), "videoprops");
                item->DisplayState(cit.value(), "codecprops");
            }
        }

        if (props & VID_PROGRESSIVE)
        {
            item->DisplayState("progressive", "videoprops");
            if (props & VID_4K)
                item->DisplayState("uhd4Kp", "videoprops");
            if (props & VID_1080)
                item->DisplayState("hd1080p", "videoprops");
        }
        else
        {
            if (props & VID_4K)
                item->DisplayState("uhd4Ki", "videoprops");
            if (props & VID_1080)
                item->DisplayState("hd1080i", "videoprops");
        }
        if (props & VID_720)
            item->DisplayState("hd720", "videoprops");
        if (!(props & (VID_4K | VID_1080 | VID_720)))
        {
            if (props & VID_HDTV)
                item->DisplayState("hdtv", "videoprops");
            else if (props & VID_WIDESCREEN)
                item->DisplayState("widescreen", "videoprops");
            else
                item->DisplayState("sd", "videoprops");
        }

        QMap<SubtitleProps, QString>::iterator sit;
        for (sit = subtitleFlags.begin(); sit != subtitleFlags.end(); ++sit)
        {
            if (pginfo->GetSubtitleType() & sit.key())
                item->DisplayState(sit.value(), "subtitletypes");
        }

        item->DisplayState(pginfo->GetCategoryTypeString(), "categorytype");

        // Mark this button list item as initialized.
        item->SetText("yes", "is_item_initialized");
    }

}

void PlaybackBox::ItemVisible(MythUIButtonListItem *item)
{
    auto *pginfo = item->GetData().value<ProgramInfo*>();

    ItemLoaded(item);
    // Job status (recording, transcoding, flagging)
    QString job = extract_job_state(*pginfo);
    item->DisplayState(job, "jobstate");

    // Flagging status (queued, running, no, yes)
    item->DisplayState(extract_commflag_state(*pginfo), "commflagged");

    const auto watchedPercent = pginfo->GetWatchedPercent();
    const bool showProgress = watchedPercent && (m_alwaysShowWatchedProgress || !pginfo->IsWatched());
    item->SetProgress1(0, showProgress ? 100 : 0, watchedPercent);
    item->SetProgress2(0, 100, pginfo->GetRecordedPercent());

    MythUIButtonListItem *sel_item = item->parent()->GetItemCurrent();
    if ((item != sel_item) && item->GetImageFilename("preview").isEmpty() &&
        (asAvailable == pginfo->GetAvailableStatus()))
    {
        QString token = m_helper.GetPreviewImage(*pginfo, true);
        if (token.isEmpty())
            return;

        m_previewTokens.insert(token);
        // now make sure selected item is still at the top of the queue
        auto *sel_pginfo = sel_item->GetData().value<ProgramInfo*>();
        if (sel_pginfo && sel_item->GetImageFilename("preview").isEmpty() &&
            (asAvailable == sel_pginfo->GetAvailableStatus()))
        {
            m_previewTokens.insert(m_helper.GetPreviewImage(*sel_pginfo, false));
        }
    }
}


/** \brief Updates the UI properties for a new preview file.
 *  This first update the image property of the MythUIButtonListItem
 *  with the new preview file, then if it is selected and there is
 *  a preview image UI item in the theme that it's filename property
 *  gets updated as well.
 */
void PlaybackBox::HandlePreviewEvent(const QStringList &list)
{
    if (list.size() < 5)
    {
        LOG(VB_GENERAL, LOG_ERR, "HandlePreviewEvent() -- too few args");
        for (uint i = 0; i < (uint) list.size(); i++)
        {
            LOG(VB_GENERAL, LOG_INFO, QString("%1: %2")
                    .arg(i).arg(list[i]));
        }
        return;
    }

    uint           recordingID  = list[0].toUInt();
    const QString& previewFile  = list[1];
    const QString& message      = list[2];

    bool found = false;
    for (uint i = 4; i < (uint) list.size(); i++)
    {
        const QString& token = list[i];
        QSet<QString>::iterator it = m_previewTokens.find(token);
        if (it != m_previewTokens.end())
        {
            found = true;
            m_previewTokens.erase(it);
        }
    }

    if (!found)
    {
        QString tokens("\n\t\t\ttokens: ");
        for (uint i = 4; i < (uint) list.size(); i++)
            tokens += list[i] + ", ";
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            "Ignoring PREVIEW_SUCCESS, no matcing token" + tokens);
        return;
    }

    if (previewFile.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Ignoring PREVIEW_SUCCESS, no preview file.");
        return;
    }

    ProgramInfo *info = m_programInfoCache.GetRecordingInfo(recordingID);
    MythUIButtonListItem *item = nullptr;

    if (info)
        item = m_recordingList->GetItemByData(QVariant::fromValue(info));

    if (!item)
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            "Ignoring PREVIEW_SUCCESS, item no longer on screen.");
    }

    if (item)
    {
        LOG(VB_GUI, LOG_INFO, LOC + QString("Loading preview %1,\n\t\t\tmsg %2")
                .arg(previewFile, message));

        item->SetImage(previewFile, "preview", true);

        if ((GetFocusWidget() == m_recordingList) &&
            (m_recordingList->GetItemCurrent() == item) &&
            m_previewImage)
        {
            m_previewImage->SetFilename(previewFile);
            m_previewImage->Load(true, true);
        }
    }
}

void PlaybackBox::updateIcons(const ProgramInfo *pginfo)
{
    uint32_t flags = FL_NONE;

    if (pginfo)
        flags = pginfo->GetProgramFlags();

    QMap <QString, int>::iterator it;
    QMap <QString, int> iconMap;

    iconMap["commflagged"] = FL_COMMFLAG;
    iconMap["cutlist"]     = FL_CUTLIST;
    iconMap["autoexpire"]  = FL_AUTOEXP;
    iconMap["processing"]  = FL_COMMPROCESSING;
    iconMap["editing"]     = FL_EDITING;
    iconMap["bookmark"]    = FL_BOOKMARK;
    iconMap["inuse"]       = (FL_INUSERECORDING |
                              FL_INUSEPLAYING |
                              FL_INUSEOTHER);
    iconMap["transcoded"]  = FL_TRANSCODED;
    iconMap["watched"]     = FL_WATCHED;
    iconMap["preserved"]   = FL_PRESERVED;

    MythUIImage *iconImage = nullptr;
    MythUIStateType *iconState = nullptr;
    for (it = iconMap.begin(); it != iconMap.end(); ++it)
    {
        iconImage = dynamic_cast<MythUIImage *>(GetChild(it.key()));
        if (iconImage)
            iconImage->SetVisible((flags & (*it)) != 0U);

        iconState = dynamic_cast<MythUIStateType *>(GetChild(it.key()));
        if (iconState)
        {
            if (flags & (*it))
                iconState->DisplayState("yes");
            else
                iconState->DisplayState("no");
        }
    }

    iconMap.clear();
    // Add prefix to ensure iteration order in case 2 or more properties set
    iconMap["1dolby"]    = AUD_DOLBY;
    iconMap["2surround"] = AUD_SURROUND;
    iconMap["3stereo"]   = AUD_STEREO;
    iconMap["4mono"]     = AUD_MONO;

    iconState = dynamic_cast<MythUIStateType *>(GetChild("audioprops"));
    bool haveIcon = false;
    if (pginfo && iconState)
    {
        for (it = iconMap.begin(); it != iconMap.end(); ++it)
        {
            if (pginfo->GetAudioProperties() & (*it))
            {
                if (iconState->DisplayState(it.key().mid(1)))
                {
                    haveIcon = true;
                    break;
                }
            }
        }
    }

    if (iconState && !haveIcon)
        iconState->Reset();

    iconState = dynamic_cast<MythUIStateType *>(GetChild("videoprops"));
    haveIcon = false;
    if (pginfo && iconState)
    {
        uint props = pginfo->GetVideoProperties();

        iconMap.clear();
        if (props & VID_PROGRESSIVE)
        {
            iconMap["uhd4Kp"]  = VID_4K;
            iconMap["hd1080p"] = VID_1080;
        }
        else
        {
            iconMap["uhd4Ki"]  = VID_4K;
            iconMap["hd1080i"] = VID_1080;
        }
        iconMap["hd1080"]     = VID_1080;
        iconMap["hd720"]      = VID_720;
        iconMap["hdtv"]       = VID_HDTV;
        iconMap["widescreen"] = VID_WIDESCREEN;

        for (it = iconMap.begin(); it != iconMap.end(); ++it)
        {
            if (props & (*it))
            {
                if (iconState->DisplayState(it.key()))
                {
                    haveIcon = true;
                    break;
                }
            }
        }
    }

    if (iconState && !haveIcon)
        iconState->Reset();
    iconMap.clear();
    iconMap["damaged"] = VID_DAMAGED;

    iconState = dynamic_cast<MythUIStateType *>(GetChild("videoquality"));
    haveIcon = false;
    if (pginfo && iconState)
    {
        for (it = iconMap.begin(); it != iconMap.end(); ++it)
        {
            if (pginfo->GetVideoProperties() & (*it))
            {
                if (iconState->DisplayState(it.key()))
                {
                    haveIcon = true;
                    break;
                }
            }
        }
    }

    if (iconState && !haveIcon)
        iconState->Reset();
    iconMap.clear();
    iconMap["deafsigned"] = SUB_SIGNED;
    iconMap["onscreensub"] = SUB_ONSCREEN;
    iconMap["subtitles"] = SUB_NORMAL;
    iconMap["cc"] = SUB_HARDHEAR;

    iconState = dynamic_cast<MythUIStateType *>(GetChild("subtitletypes"));
    haveIcon = false;
    if (pginfo && iconState)
    {
        for (it = iconMap.begin(); it != iconMap.end(); ++it)
        {
            if (pginfo->GetSubtitleType() & (*it))
            {
                if (iconState->DisplayState(it.key()))
                {
                    haveIcon = true;
                    break;
                }
            }
        }
    }

    if (iconState && !haveIcon)
        iconState->Reset();

    iconState = dynamic_cast<MythUIStateType *>(GetChild("categorytype"));
    if (iconState)
    {
        if (!(pginfo && iconState->DisplayState(pginfo->GetCategoryTypeString())))
            iconState->Reset();
    }
}

bool PlaybackBox::IsUsageUIVisible(void) const
{
    return GetChild("freereport") || GetChild("usedbar");
}

void PlaybackBox::UpdateUsageUI(void)
{
    MythUIText        *freereportText =
        dynamic_cast<MythUIText*>(GetChild("freereport"));
    MythUIProgressBar *usedProgress   =
        dynamic_cast<MythUIProgressBar *>(GetChild("usedbar"));

    // If the theme doesn't have these widgets,
    // don't waste time querying the backend...
    if (!freereportText && !usedProgress && !GetChild("diskspacetotal") &&
        !GetChild("diskspaceused") && !GetChild("diskspacefree") &&
        !GetChild("diskspacepercentused") && !GetChild("diskspacepercentfree"))
        return;

    auto freeSpaceTotal = (double) m_helper.GetFreeSpaceTotalMB();
    auto freeSpaceUsed  = (double) m_helper.GetFreeSpaceUsedMB();

    QLocale locale = gCoreContext->GetQLocale();
    InfoMap usageMap;
    usageMap["diskspacetotal"] = locale.toString((freeSpaceTotal / 1024.0),
                                               'f', 2);
    usageMap["diskspaceused"] = locale.toString((freeSpaceUsed / 1024.0),
                                               'f', 2);
    usageMap["diskspacefree"] = locale.toString(
                                    ((freeSpaceTotal - freeSpaceUsed) / 1024.0),
                                    'f', 2);

    double perc = 0.0;
    if (freeSpaceTotal > 0.0)
        perc = (100.0 * freeSpaceUsed) / freeSpaceTotal;

    usageMap["diskspacepercentused"] = QString::number((int)perc);
    usageMap["diskspacepercentfree"] = QString::number(100 - (int)perc);

    QString size = locale.toString(((freeSpaceTotal - freeSpaceUsed) / 1024.0),
                                   'f', 2);

    QString usestr = tr("%1% used, %2 GB free", "Diskspace")
                                                .arg(QString::number((int)perc),
                                                     size);

    if (freereportText)
        freereportText->SetText(usestr);

    if (usedProgress)
    {
        usedProgress->SetTotal((int)freeSpaceTotal);
        usedProgress->SetUsed((int)freeSpaceUsed);
    }

    SetTextFromMap(usageMap);
}

/*
 * \fn PlaybackBox::updateUIRecGroupList(void)
 * \brief called when the list of recording groups may have changed
 */
void PlaybackBox::UpdateUIRecGroupList(void)
{
    if (m_recGroupIdx < 0 || !m_recgroupList || m_recGroups.size() < 2)
        return;

    QSignalBlocker blocker(m_recgroupList);

    m_recgroupList->Reset();

    int idx = 0;
    QStringList::iterator it = m_recGroups.begin();
    for (; it != m_recGroups.end(); (++it), (++idx))
    {
        const QString& key = (*it);
        QString tmp = (key == "All Programs") ? "All" : key;
        QString name = ProgramInfo::i18n(tmp);

        if (m_recGroups.size() == 2 && key == "Default")
            continue;  // All and Default will be the same, so only show All

        auto *item = new MythUIButtonListItem(m_recgroupList, name,
                                              QVariant::fromValue(key));

        if (idx == m_recGroupIdx)
            m_recgroupList->SetItemCurrent(item);
        item->SetText(name);
    }
}

void PlaybackBox::UpdateUIGroupList(const QStringList &groupPreferences)
{
    m_groupList->Reset();
    if (m_groupAlphaList)
        m_groupAlphaList->Reset();

    if (!m_titleList.isEmpty())
    {
        int best_pref = INT_MAX;
        int sel_idx = 0;

        QStringList::iterator it;
        for (it = m_titleList.begin(); it != m_titleList.end(); ++it)
        {
            const QString& groupname = (*it);

            auto *item = new MythUIButtonListItem(m_groupList, "",
                                     QVariant::fromValue(groupname.toLower()));

            int pref = groupPreferences.indexOf(groupname.toLower());
            if ((pref >= 0) && (pref < best_pref))
            {
                best_pref = pref;
                sel_idx = m_groupList->GetItemPos(item);
                m_currentGroup = groupname.toLower();
            }

            QString displayName = groupname;
            if (displayName.isEmpty())
            {
                if (m_recGroup == "All Programs")
                    displayName = ProgramInfo::i18n("All Programs");
                else
                    displayName = ProgramInfo::i18n("All Programs - %1")
                        .arg(m_groupDisplayName);
            }

            item->SetText(groupname, "groupname");
            item->SetText(displayName, "name");
            item->SetText(displayName);

            int count = m_progLists[groupname.toLower()].size();
            item->SetText(QString::number(count), "reccount");
        }

        m_needUpdate = true;
        m_groupList->SetItemCurrent(sel_idx);
        // We need to explicitly call updateRecList in this case,
        // since 0 is selected by default, and we need updateRecList
        // to be called with m_needUpdate set.
        if (!sel_idx)
            updateRecList(m_groupList->GetItemCurrent());

        if (m_groupAlphaList)
        {
            for (auto Iqs = m_groupAlphabet.keyValueBegin();
                 Iqs != m_groupAlphabet.keyValueEnd(); ++Iqs)
            {
                auto *item = new MythUIButtonListItem(m_groupAlphaList, "",
                                               QVariant::fromValue(Iqs->first));
                item->SetText(Iqs->first);
            }
        }
    }
}

void PlaybackBox::updateRecGroup(MythUIButtonListItem *sel_item)
{
    QString newRecGroup = sel_item->GetData().toString();
    displayRecGroup(newRecGroup);
}

void PlaybackBox::SelectNextRecGroup(void)
{
    QString nextGroup;
    m_recGroupsLock.lock();
    if (m_recGroupIdx >= 0 && !m_recGroups.empty())
    {
        if (++m_recGroupIdx >= m_recGroups.size())
            m_recGroupIdx = 0;
        nextGroup = m_recGroups[m_recGroupIdx];
    }
    m_recGroupsLock.unlock();

    if (!nextGroup.isEmpty())
        displayRecGroup(nextGroup);
}

void PlaybackBox::updateRecList(MythUIButtonListItem *sel_item)
{
    if (!sel_item)
        return;

    QString groupname = sel_item->GetData().toString();
    QString grouplabel = sel_item->GetText();

    updateGroupInfo(groupname, grouplabel);
    if (((m_currentGroup == groupname) && !m_needUpdate) ||
        m_playingSomething)
        return;

    m_needUpdate = false;

    if (!m_isFilling)
        m_currentGroup = groupname;

    m_recordingList->Reset();

    ProgramMap::iterator pmit = m_progLists.find(groupname);
    if (pmit == m_progLists.end())
        return;

    ProgramList &progList = *pmit;

    for (auto & prog : progList)
    {
        if (prog->GetAvailableStatus() == asPendingDelete ||
            prog->GetAvailableStatus() == asDeleted)
            continue;

        new PlaybackBoxListItem(this, m_recordingList, prog);
    }
    m_recordingList->LoadInBackground();

    if (m_noRecordingsText)
    {
        if (!progList.empty())
            m_noRecordingsText->SetVisible(false);
        else
        {
            QString txt = m_programInfoCache.empty() ?
                tr("There are no recordings available") :
                tr("There are no recordings in your current view");
            m_noRecordingsText->SetText(txt);
            m_noRecordingsText->SetVisible(true);
        }
    }

    if (m_groupAlphaList)
    {
        if (grouplabel.startsWith("Watch List") ||
            grouplabel.startsWith("All Programs"))
        {
            m_currentLetter = "All";
        }
        else
        {
            ProgramInfo *pginfo = GetCurrentProgram();
            m_currentLetter = pginfo->GetSortTitle().at(0).toUpper();
            m_groupAlphaList->MoveToNamedPosition(m_currentLetter);
        }
    }
}

void PlaybackBox::selectUIGroupsAlphabet(MythUIButtonListItem *item)
{
    if (!item  || (m_currentLetter == item->GetText()) )
        return;

    if (!item->GetText().isEmpty())
    {
        m_currentLetter = item->GetText();
        m_groupList->MoveToNamedPosition(m_groupAlphabet[m_currentLetter]);
    }
}

static bool save_position(
    const MythUIButtonList *groupList, const MythUIButtonList *recordingList,
    QStringList &groupSelPref, QStringList &itemSelPref,
    QStringList &itemTopPref)
{
    MythUIButtonListItem *prefSelGroup = groupList->GetItemCurrent();
    if (!prefSelGroup)
        return false;

    groupSelPref.push_back(prefSelGroup->GetData().toString());
    for (int i = groupList->GetCurrentPos();
         i < groupList->GetCount(); i++)
    {
        prefSelGroup = groupList->GetItemAt(i);
        if (prefSelGroup)
            groupSelPref.push_back(prefSelGroup->GetData().toString());
    }

    int curPos = recordingList->GetCurrentPos();
    for (int i = curPos; (i >= 0) && (i < recordingList->GetCount()); i++)
    {
        MythUIButtonListItem *item = recordingList->GetItemAt(i);
        auto *pginfo = item->GetData().value<ProgramInfo*>();
        itemSelPref.push_back(groupSelPref.front());
        itemSelPref.push_back(QString::number(pginfo->GetRecordingID()));
    }
    for (int i = curPos; (i >= 0) && (i < recordingList->GetCount()); i--)
    {
        MythUIButtonListItem *item = recordingList->GetItemAt(i);
        auto *pginfo = item->GetData().value<ProgramInfo*>();
        itemSelPref.push_back(groupSelPref.front());
        itemSelPref.push_back(QString::number(pginfo->GetRecordingID()));
    }

    int topPos = recordingList->GetTopItemPos();
    for (int i = topPos + 1; i >= topPos - 1; i--)
    {
        if (i >= 0 && i < recordingList->GetCount())
        {
            MythUIButtonListItem *item = recordingList->GetItemAt(i);
            auto *pginfo = item->GetData().value<ProgramInfo*>();
            if (i == topPos)
            {
                itemTopPref.push_front(QString::number(pginfo->GetRecordingID()));
                itemTopPref.push_front(groupSelPref.front());
            }
            else
            {
                itemTopPref.push_back(groupSelPref.front());
                itemTopPref.push_back(QString::number(pginfo->GetRecordingID()));
            }
        }
    }

    return true;
}

static void restore_position(
    MythUIButtonList *groupList, MythUIButtonList *recordingList,
    const QStringList &groupSelPref, const QStringList &itemSelPref,
    const QStringList &itemTopPref)
{
    // If possible reselect the item selected before,
    // otherwise select the nearest available item.
    MythUIButtonListItem *prefSelGroup = groupList->GetItemCurrent();
    if (!prefSelGroup ||
        !groupSelPref.contains(prefSelGroup->GetData().toString()) ||
        !itemSelPref.contains(prefSelGroup->GetData().toString()))
    {
        return;
    }

    // the group is selected in UpdateUIGroupList()
    QString groupname = prefSelGroup->GetData().toString();

    // find best selection
    int sel = -1;
    for (uint i = 0; i+1 < (uint)itemSelPref.size(); i+=2)
    {
        if (itemSelPref[i] != groupname)
            continue;

        uint recordingID = itemSelPref[i+1].toUInt();
        for (uint j = 0; j < (uint)recordingList->GetCount(); j++)
        {
            MythUIButtonListItem *item = recordingList->GetItemAt(j);
            auto *pginfo = item->GetData().value<ProgramInfo*>();
            if (pginfo && (pginfo->GetRecordingID() == recordingID))
            {
                sel = j;
                i = itemSelPref.size();
                break;
            }
        }
    }

    // find best top item
    int top = -1;
    for (uint i = 0; i+1 < (uint)itemTopPref.size(); i+=2)
    {
        if (itemTopPref[i] != groupname)
            continue;

        uint recordingID = itemTopPref[i+1].toUInt();
        for (uint j = 0; j < (uint)recordingList->GetCount(); j++)
        {
            MythUIButtonListItem *item = recordingList->GetItemAt(j);
            auto *pginfo = item->GetData().value<ProgramInfo*>();
            if (pginfo && (pginfo->GetRecordingID() == recordingID))
            {
                top = j;
                i = itemTopPref.size();
                break;
            }
        }
    }

    if (sel >= 0)
    {
#if 0
        LOG(VB_GENERAL, LOG_DEBUG, QString("Reselect success (%1,%2)")
                .arg(sel).arg(top));
#endif
        recordingList->SetItemCurrent(sel, top);
    }
    else
    {
#if 0
        LOG(VB_GENERAL, LOG_DEBUG, QString("Reselect failure (%1,%2)")
                .arg(sel).arg(top));
#endif
    }
}

bool PlaybackBox::UpdateUILists(void)
{
    m_isFilling = true;

    // Save selection, including next few items & groups
    QStringList groupSelPref;
    QStringList itemSelPref;
    QStringList itemTopPref;
    if (!save_position(m_groupList, m_recordingList,
                       groupSelPref, itemSelPref, itemTopPref))
    {
        // If user wants to start in watchlist and watchlist is displayed, then
        // make it the current group
        if (m_watchListStart && (m_viewMask & VIEW_WATCHLIST))
            groupSelPref.push_back(m_watchGroupLabel);
    }

    // Cache available status for later restoration
    QMap<uint, AvailableStatusType> asCache;

    if (!m_progLists.isEmpty())
    {
        for (auto & prog : m_progLists[""])
        {
            uint asRecordingID = prog->GetRecordingID();
            asCache[asRecordingID] = prog->GetAvailableStatus();
        }
    }

    m_progsInDB = 0;
    m_titleList.clear();
    m_progLists.clear();
    m_recordingList->Reset();
    m_groupList->Reset();
    if (m_recgroupList)
        m_recgroupList->Reset();
    // Clear autoDelete for the "all" list since it will share the
    // objects with the title lists.
    m_progLists[""] = ProgramList(false);
    m_progLists[""].setAutoDelete(false);

    ViewTitleSort titleSort = (ViewTitleSort)gCoreContext->GetNumSetting(
                                "DisplayGroupTitleSort", TitleSortAlphabetical);

    bool isAllProgsGroup   = (m_recGroup == "All Programs");
    QMap<QString, QString> sortedList;
    QMap<int, QString> searchRule;
    QMap<int, QDateTime> recidLastEventTime;
    QMap<int, ProgramInfo*> recidWatchListProgram;

    m_programInfoCache.Refresh();

    if (!m_programInfoCache.empty())
    {
        QString sTitle;

        if ((m_viewMask & VIEW_SEARCHES))
        {
            MSqlQuery query(MSqlQuery::InitCon());
            query.prepare("SELECT recordid,title FROM record "
                          "WHERE search > 0 AND search != :MANUAL;");
            query.bindValue(":MANUAL", kManualSearch);

            if (query.exec())
            {
                while (query.next())
                {
                    QString tmpTitle = query.value(1).toString();
                    tmpTitle.remove(RecordingInfo::kReSearchTypeName);
                    searchRule[query.value(0).toInt()] = tmpTitle;
                }
            }
        }

        bool isCategoryFilter  = (m_recGroupType[m_recGroup] == "category");
        bool isUnknownCategory = (m_recGroup == tr("Unknown"));
        bool isDeletedGroup    = (m_recGroup == "Deleted");
        bool isLiveTvGroup     = (m_recGroup == "LiveTV");

        std::vector<ProgramInfo*> list;
        bool newest_first = (0==m_allOrder);
        m_programInfoCache.GetOrdered(list, newest_first);
        for (auto *p : list)
        {
            if (p->IsDeletePending())
                continue;

            m_progsInDB++;

            const QString& pRecgroup(p->GetRecordingGroup());
            const bool     isLiveTVProg(pRecgroup == "LiveTV");

            // Never show anything from unauthorised passworded groups
            QString password = getRecGroupPassword(pRecgroup);
            if (m_curGroupPassword != password && !password.isEmpty())
                continue;

            if (pRecgroup == "Deleted")
            {
                // Filter nothing from Deleted group
                // Never show Deleted recs anywhere else
                if (!isDeletedGroup)
                    continue;
            }
            // Optionally ignore LiveTV programs if not viewing LiveTV group
            else if (!(m_viewMask & VIEW_LIVETVGRP) &&
                     !isLiveTvGroup && isLiveTVProg)
            {   // NOLINT(bugprone-branch-clone)
                continue;
            }
            // Optionally ignore watched
            else if (!(m_viewMask & VIEW_WATCHED) && p->IsWatched())
            {
                continue;
            }
            else if (isCategoryFilter)
            {
                // Filter by category
                if (isUnknownCategory ? !p->GetCategory().isEmpty()
                                      : p->GetCategory() != m_recGroup)
                    continue;
            }
            // Filter by recgroup
            else if (!isAllProgsGroup && pRecgroup != m_recGroup)
            {
                continue;
            }

            if (p->GetTitle().isEmpty())
                p->SetTitle(tr("_NO_TITLE_"));

            if (m_viewMask != VIEW_NONE && (!isLiveTVProg || isLiveTvGroup))
            {
                m_progLists[""].push_front(p);
            }

            uint asRecordingID = p->GetRecordingID();
            if (asCache.contains(asRecordingID))
                p->SetAvailableStatus(asCache[asRecordingID], "UpdateUILists");
            else
                p->SetAvailableStatus(asAvailable,  "UpdateUILists");

            if (!isLiveTvGroup && isLiveTVProg && (m_viewMask & VIEW_LIVETVGRP))
            {
                QString tmpTitle = tr("Live TV");
                sortedList[tmpTitle.toLower()] = tmpTitle;
                m_progLists[tmpTitle.toLower()].push_front(p);
                m_progLists[tmpTitle.toLower()].setAutoDelete(false);
                continue;
            }

            // Show titles
            if ((m_viewMask & VIEW_TITLES) && (!isLiveTVProg || isLiveTvGroup))
            {
                sTitle = construct_sort_title(
                            p->GetTitle(), p->GetSortTitle(), m_viewMask, titleSort,
                            p->GetRecordingPriority());
                sTitle = sTitle.toLower();

                if (!sortedList.contains(sTitle))
                    sortedList[sTitle] = p->GetTitle();
                m_progLists[sortedList[sTitle].toLower()].push_front(p);
                m_progLists[sortedList[sTitle].toLower()].setAutoDelete(false);
            }

            // Show recording groups
            if ((m_viewMask & VIEW_RECGROUPS) &&
                !pRecgroup.isEmpty() && !isLiveTVProg)
            {
                sortedList[pRecgroup.toLower()] = pRecgroup;
                m_progLists[pRecgroup.toLower()].push_front(p);
                m_progLists[pRecgroup.toLower()].setAutoDelete(false);
            }

            // Show categories
            if (((m_viewMask & VIEW_CATEGORIES) != 0) && !p->GetCategory().isEmpty())
            {
                QString catl = p->GetCategory().toLower();
                sortedList[catl] = p->GetCategory();
                m_progLists[catl].push_front(p);
                m_progLists[catl].setAutoDelete(false);
            }

            if (((m_viewMask & VIEW_SEARCHES) != 0) &&
                    !searchRule[p->GetRecordingRuleID()].isEmpty() &&
                    p->GetTitle() != searchRule[p->GetRecordingRuleID()])
            {   // Show search rules
                QString tmpTitle = QString("(%1)")
                        .arg(searchRule[p->GetRecordingRuleID()]);
                sortedList[tmpTitle.toLower()] = tmpTitle;
                m_progLists[tmpTitle.toLower()].push_front(p);
                m_progLists[tmpTitle.toLower()].setAutoDelete(false);
            }

            if ((m_viewMask & VIEW_WATCHLIST) &&
                !isLiveTVProg && pRecgroup != "Deleted")
            {
                int rid = p->GetRecordingRuleID();
                auto letIt = recidLastEventTime.find(rid);
                if (letIt == recidLastEventTime.end() || *letIt < p->GetLastModifiedTime())
                {
                    recidLastEventTime[rid] = p->GetLastModifiedTime();
                }

                if (m_watchListAutoExpire && !p->IsAutoExpirable())
                {
                    p->SetRecordingPriority2(wlExpireOff);
                    LOG(VB_FILE, LOG_INFO, QString("Auto-expire off:  %1")
                        .arg(p->GetTitle()));
                }
                else if (p->IsWatched())
                {
                    p->SetRecordingPriority2(wlWatched);
                    LOG(VB_FILE, LOG_INFO,
                        QString("Marked as 'watched':  %1")
                        .arg(p->GetTitle()));
                }
                else
                {
                    auto wlpIt = recidWatchListProgram.find(rid);
                    if (wlpIt == recidWatchListProgram.end())
                    {
                        recidWatchListProgram[rid] = p;
                    }
                    else if(comp_season(p, *wlpIt) > 0)
                    {
                        (*wlpIt)->SetRecordingPriority2(wlEarlier);
                        LOG(VB_FILE, LOG_INFO,
                            QString("Not the earliest:  %1")
                            .arg((*wlpIt)->GetTitle()));

                        recidWatchListProgram[rid] = p;
                    }
                    else
                    {
                        p->SetRecordingPriority2(wlEarlier);
                        LOG(VB_FILE, LOG_INFO,
                            QString("Not the earliest:  %1")
                            .arg(p->GetTitle()));
                    }
                }
            }
        }

        if ((m_viewMask & VIEW_WATCHLIST) && !recidWatchListProgram.empty())
        {
            for (auto *p : std::as_const(recidWatchListProgram))
            {
                m_progLists[m_watchGroupLabel].push_back(p);
            }

            m_progLists[m_watchGroupLabel].setAutoDelete(false);
        }
    }

    if (sortedList.empty())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "SortedList is Empty");
        m_progLists[""];
        m_titleList << "";
        m_playList.clear();
        if (!isAllProgsGroup)
            SelectNextRecGroup();

        UpdateUIRecGroupList();
        UpdateUIGroupList(groupSelPref);

        m_isFilling = false;
        return false;
    }

    QString episodeSort = gCoreContext->GetSetting("PlayBoxEpisodeSort", "Date");

    if (episodeSort == "OrigAirDate")
    {
        QMap<QString, ProgramList>::Iterator Iprog;
        for (Iprog = m_progLists.begin(); Iprog != m_progLists.end(); ++Iprog)
        {
            if (!Iprog.key().isEmpty())
            {
                std::stable_sort((*Iprog).begin(), (*Iprog).end(),
                                 (m_listOrder == 0) ?
                                 comp_originalAirDate_rev_less_than :
                                 comp_originalAirDate_less_than);
            }
        }
    }
    else if (episodeSort == "Id")
    {
        QMap<QString, ProgramList>::Iterator Iprog;
        for (Iprog = m_progLists.begin(); Iprog != m_progLists.end(); ++Iprog)
        {
            if (!Iprog.key().isEmpty())
            {
                std::stable_sort((*Iprog).begin(), (*Iprog).end(),
                                 (m_listOrder == 0) ?
                                 comp_programid_rev_less_than :
                                 comp_programid_less_than);
            }
        }
    }
    else if (episodeSort == "Date")
    {
        QMap<QString, ProgramList>::iterator it;
        for (it = m_progLists.begin(); it != m_progLists.end(); ++it)
        {
            if (!it.key().isEmpty())
            {
                std::stable_sort((*it).begin(), (*it).end(),
                                 (!m_listOrder) ?
                                 comp_recordDate_rev_less_than :
                                 comp_recordDate_less_than);
            }
        }
    }
    else if (episodeSort == "Season")
    {
        QMap<QString, ProgramList>::iterator it;
        for (it = m_progLists.begin(); it != m_progLists.end(); ++it)
        {
            if (!it.key().isEmpty())
            {
                std::stable_sort((*it).begin(), (*it).end(),
                                 (!m_listOrder) ?
                                 comp_season_rev_less_than :
                                 comp_season_less_than);
            }
        }
    }

    if (!m_progLists[m_watchGroupLabel].empty())
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT recordid, last_delete FROM record;");

        if (query.exec())
        {
            while (query.next())
            {
                int recid = query.value(0).toInt();

                QDateTime last_delete =
                    MythDate::as_utc(query.value(1).toDateTime());

                if (last_delete.isValid())
                {
                    auto it = recidLastEventTime.find(recid);
                    if (it != recidLastEventTime.end() && last_delete > *it)
                    {
                        recidLastEventTime[recid] = last_delete;
                    }
                }
            }
        }

        auto pit = m_progLists[m_watchGroupLabel].begin();
        while (pit != m_progLists[m_watchGroupLabel].end())
        {
            int recid = (*pit)->GetRecordingRuleID();

            (*pit)->SetRecordingPriority2(recidLastEventTime[recid].toSecsSinceEpoch()/60);

            LOG(VB_FILE, LOG_INFO, QString(" %1  %2  %3")
                    .arg(MythDate::toString((*pit)->GetScheduledStartTime(),
                                            MythDate::kDateShort))
                    .arg((*pit)->GetRecordingPriority2())
                    .arg((*pit)->GetTitle()));

            ++pit;
        }

        std::stable_sort(m_progLists[m_watchGroupLabel].begin(),
                         m_progLists[m_watchGroupLabel].end(),
                         comp_recpriority2_less_than);
    }

    m_titleList = QStringList("");
    if (!m_progLists[m_watchGroupLabel].empty())
        m_titleList << m_watchGroupName;
    if ((!m_progLists["livetv"].empty()) &&
        (std::find(sortedList.cbegin(), sortedList.cend(), tr("Live TV"))
         == sortedList.cend()))
        m_titleList << tr("Live TV");
    m_titleList << sortedList.values();

    // Populate list of recording groups
    if (!m_programInfoCache.empty())
    {
        QMutexLocker locker(&m_recGroupsLock);

        m_recGroups.clear();
        m_recGroupIdx = -1;

        m_recGroups.append("All Programs");

        MSqlQuery query(MSqlQuery::InitCon());

        query.prepare("SELECT distinct recgroup from recorded WHERE "
                      "deletepending = 0 ORDER BY recgroup");
        if (query.exec())
        {
            QString name;
            while (query.next())
            {
                name = query.value(0).toString();
                if (name != "Deleted" && name != "LiveTV" && !name.startsWith('.'))
                {
                    m_recGroups.append(name);
                    m_recGroupType[name] = "recgroup";
                }
            }

            m_recGroupIdx = m_recGroups.indexOf(m_recGroup);
            m_recGroupIdx = std::max(m_recGroupIdx, 0);
        }
    }

    QChar first;
    m_groupAlphabet.clear();
    for (auto it = sortedList.keyValueBegin();
         it != sortedList.keyValueEnd(); ++it)
    {
        first = (*it).first.at(0).toUpper();
        if (!m_groupAlphabet.contains(first))
            m_groupAlphabet[first] = (*it).second;
    }

    UpdateUIRecGroupList();
    UpdateUIGroupList(groupSelPref);
    UpdateUsageUI();

    for (uint id : std::as_const(m_playList))
    {
        ProgramInfo *pginfo = FindProgramInUILists(id);
        if (!pginfo)
            continue;
        MythUIButtonListItem *item =
            m_recordingList->GetItemByData(QVariant::fromValue(pginfo));
        if (item)
            item->DisplayState("yes", "playlist");
    }

    restore_position(m_groupList, m_recordingList,
                     groupSelPref, itemSelPref, itemTopPref);

    m_isFilling = false;

    return true;
}

void PlaybackBox::playSelectedPlaylist(bool Random)
{
    if (Random)
    {
        m_playListPlay.clear();
        QList<uint> tmp = m_playList;
        while (!tmp.isEmpty())
        {
            unsigned int i = MythRandom(0, tmp.size() - 1);
            m_playListPlay.append(tmp[i]);
            tmp.removeAll(tmp[i]);
        }
    }
    else
    {
        m_playListPlay = m_playList;
    }

    QCoreApplication::postEvent(
        this, new MythEvent("PLAY_PLAYLIST"));
}

void PlaybackBox::PlayFromAnyMark(MythUIButtonListItem *item)
{
    if (!item)
        item = m_recordingList->GetItemCurrent();

    if (!item)
        return;

    auto *pginfo = item->GetData().value<ProgramInfo *>();

    const bool ignoreBookmark = false;
    const bool ignoreProgStart = false;
    const bool ignoreLastPlayPos = false;
    const bool underNetworkControl = false;
    if (pginfo)
        PlayX(*pginfo, ignoreBookmark, ignoreProgStart, ignoreLastPlayPos,
              underNetworkControl);
}

void PlaybackBox::PlayFromBookmark(MythUIButtonListItem *item)
{
    if (!item)
        item = m_recordingList->GetItemCurrent();

    if (!item)
        return;

    auto *pginfo = item->GetData().value<ProgramInfo *>();

    const bool ignoreBookmark = false;
    const bool ignoreProgStart = true;
    const bool ignoreLastPlayPos = true;
    const bool underNetworkControl = false;
    if (pginfo)
        PlayX(*pginfo, ignoreBookmark, ignoreProgStart, ignoreLastPlayPos,
              underNetworkControl);
}

void PlaybackBox::PlayFromBeginning(MythUIButtonListItem *item)
{
    if (!item)
        item = m_recordingList->GetItemCurrent();

    if (!item)
        return;

    auto *pginfo = item->GetData().value<ProgramInfo *>();

    const bool ignoreBookmark = true;
    const bool ignoreProgStart = true;
    const bool ignoreLastPlayPos = true;
    const bool underNetworkControl = false;
    if (pginfo)
        PlayX(*pginfo, ignoreBookmark, ignoreProgStart, ignoreLastPlayPos,
              underNetworkControl);
}

void PlaybackBox::PlayFromLastPlayPos(MythUIButtonListItem *item)
{
    if (!item)
        item = m_recordingList->GetItemCurrent();

    if (!item)
        return;

    auto *pginfo = item->GetData().value<ProgramInfo *>();

    const bool ignoreBookmark = true;
    const bool ignoreProgStart = true;
    const bool ignoreLastPlayPos = false;
    const bool underNetworkControl = false;
    if (pginfo)
        PlayX(*pginfo, ignoreBookmark, ignoreProgStart, ignoreLastPlayPos,
              underNetworkControl);
}

void PlaybackBox::PlayX(const ProgramInfo &pginfo,
                        bool ignoreBookmark,
                        bool ignoreProgStart,
                        bool ignoreLastPlayPos,
                        bool underNetworkControl)
{
    if (!m_player)
    {
        Play(pginfo, false, ignoreBookmark, ignoreProgStart, ignoreLastPlayPos, underNetworkControl);
        return;
    }

    if (!m_player->IsSameProgram(&pginfo))
    {
        pginfo.ToStringList(m_playerSelectedNewShow);
        m_playerSelectedNewShow.push_back(ignoreBookmark ? "1" : "0");
        m_playerSelectedNewShow.push_back(underNetworkControl ? "1" : "0");
        // XXX add anything for ignoreProgStart and ignoreLastPlayPos?
    }
    Close();
}

void PlaybackBox::ClearBookmark()
{
    ProgramInfo *pginfo = GetCurrentProgram();
    if (pginfo)
        pginfo->SaveBookmark(0);
}

void PlaybackBox::ClearLastPlayPos()
{
    ProgramInfo *pginfo = GetCurrentProgram();
    if (pginfo)
        pginfo->SaveLastPlayPos(0);
}

void PlaybackBox::StopSelected(void)
{
    ProgramInfo *pginfo = GetCurrentProgram();
    if (pginfo)
        m_helper.StopRecording(*pginfo);
}

void PlaybackBox::deleteSelected(MythUIButtonListItem *item)
{
    if (!item)
        return;

    auto *pginfo = item->GetData().value<ProgramInfo *>();

    if (!pginfo)
        return;

    if (pginfo->GetAvailableStatus() == asPendingDelete)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("deleteSelected(%1) -- failed ")
                .arg(pginfo->toString(ProgramInfo::kTitleSubtitle)) +
            QString("availability status: %1 ")
                .arg(pginfo->GetAvailableStatus()));

        ShowOkPopup(tr("Cannot delete\n") +
                    tr("This recording is already being deleted"));
    }
    else if (!pginfo->QueryIsDeleteCandidate())
    {
        QString byWho;
        pginfo->QueryIsInUse(byWho);

        LOG(VB_GENERAL, LOG_ERR, QString("deleteSelected(%1) -- failed ")
                .arg(pginfo->toString(ProgramInfo::kTitleSubtitle)) +
            QString("delete candidate: %1 in use by %2")
                .arg(pginfo->QueryIsDeleteCandidate()).arg(byWho));

        if (byWho.isEmpty())
        {
            ShowOkPopup(tr("Cannot delete\n") +
                        tr("This recording is already being deleted"));
        }
        else
        {
            ShowOkPopup(tr("Cannot delete\n") +
                        tr("This recording is currently in use by:") + "\n" +
                        byWho);
        }
    }
    else
    {
        push_onto_del(m_delList, *pginfo);
        ShowDeletePopup(kDeleteRecording);
    }
}

ProgramInfo *PlaybackBox::GetCurrentProgram(void) const
{
    ProgramInfo *pginfo = nullptr;

    MythUIButtonListItem *item = m_recordingList->GetItemCurrent();

    if (!item)
        return nullptr;

    pginfo = item->GetData().value<ProgramInfo *>();

    if (!pginfo)
        return nullptr;

    return pginfo;
}

void PlaybackBox::selected(MythUIButtonListItem *item)
{
    if (!item)
        return;

    PlayFromAnyMark(item);
}

void PlaybackBox::popupClosed(const QString& which, int result)
{
    m_menuDialog = nullptr;

    if (result == -2)
    {
        if (!m_doToggleMenu)
        {
            m_doToggleMenu = true;
            return;
        }

        if (which == "groupmenu")
        {
            ProgramInfo *pginfo = GetCurrentProgram();
            if (pginfo)
            {
                m_helper.CheckAvailability(*pginfo, kCheckForMenuAction);

                if ((asPendingDelete == pginfo->GetAvailableStatus()) ||
                    (asDeleted == pginfo->GetAvailableStatus()) ||
                    (asNotYetAvailable == pginfo->GetAvailableStatus()))
                {
                    ShowAvailabilityPopup(*pginfo);
                }
                else
                {
                    ShowActionPopup(*pginfo);
                    m_doToggleMenu = false;
                }
            }
        }
        else if (which == "actionmenu")
        {
            ShowGroupPopup();
            m_doToggleMenu = false;
        }
    }
    else
    {
        m_doToggleMenu = true;
    }
}

void PlaybackBox::ShowGroupPopup()
{
    QString label = tr("Group List Menu");

    ProgramInfo *pginfo = GetCurrentProgram();

    m_popupMenu = new MythMenu(label, this, "groupmenu");

    m_popupMenu->AddItem(tr("Change Group Filter"),
                         &PlaybackBox::showGroupFilter);

    m_popupMenu->AddItem(tr("Change Group View"),
                         &PlaybackBox::showViewChanger);

    if (m_recGroupType[m_recGroup] == "recgroup")
        m_popupMenu->AddItem(tr("Change Group Password"),
                             &PlaybackBox::showRecGroupPasswordChanger);

    if (!m_playList.isEmpty())
    {
        m_popupMenu->AddItem(tr("Playlist Options"), nullptr, createPlaylistMenu());
    }
    else if (!m_player)
    {
        if (GetFocusWidget() == m_groupList)
        {
            m_popupMenu->AddItem(tr("Add this Group to Playlist"),
                                 &PlaybackBox::togglePlayListTitle);
        }
        else if (pginfo)
        {
            m_popupMenu->AddItem(tr("Add this recording to Playlist"),
                                 qOverload<>(&PlaybackBox::togglePlayListItem));
        }
    }

    m_popupMenu->AddItem(tr("Help (Status Icons)"), &PlaybackBox::showIconHelp);

    DisplayPopupMenu();
}

bool PlaybackBox::Play(
    const ProgramInfo &rec,
    bool inPlaylist, bool ignoreBookmark, bool ignoreProgStart,
    bool ignoreLastPlayPos, bool underNetworkControl)
{
    bool playCompleted = false;

    if (m_player)
        return true;

    if ((asAvailable != rec.GetAvailableStatus()) || !rec.GetFilesize() ||
        !rec.IsPathSet())
    {
        m_helper.CheckAvailability(
            rec, (inPlaylist) ? kCheckForPlaylistAction : kCheckForPlayAction);
        return false;
    }

    for (size_t i = 0; i < kNumArtImages; i++)
    {
        if (!m_artImage[i])
            continue;

        m_artTimer[i]->stop();
        m_artImage[i]->Reset();
    }

    ProgramInfo tvrec(rec);

    m_playingSomething = true;
    int initIndex = m_recordingList->StopLoad();

    if (!gCoreContext->GetBoolSetting("UseProgStartMark", false))
        ignoreProgStart = true;

    uint flags =
        (inPlaylist          ? kStartTVInPlayList        : kStartTVNoFlags) |
        (underNetworkControl ? kStartTVByNetworkCommand  : kStartTVNoFlags) |
        (ignoreLastPlayPos   ? kStartTVIgnoreLastPlayPos : kStartTVNoFlags) |
        (ignoreProgStart     ? kStartTVIgnoreProgStart   : kStartTVNoFlags) |
        (ignoreBookmark      ? kStartTVIgnoreBookmark    : kStartTVNoFlags);

    playCompleted = TV::StartTV(&tvrec, flags);

    m_playingSomething = false;
    m_recordingList->LoadInBackground(initIndex);

    if (inPlaylist && !m_playListPlay.empty())
    {
        QCoreApplication::postEvent(
            this, new MythEvent("PLAY_PLAYLIST"));
    }

    if (m_needUpdate)
        ScheduleUpdateUIList();

    return playCompleted;
}

void PlaybackBox::RemoveProgram( uint recordingID, bool forgetHistory,
                                 bool forceMetadataDelete)
{
    ProgramInfo *delItem = FindProgramInUILists(recordingID);

    if (!delItem)
        return;

    if (!forceMetadataDelete &&
        ((delItem->GetAvailableStatus() == asPendingDelete) ||
         !delItem->QueryIsDeleteCandidate()))
    {
        return;
    }

    if (m_playList.contains(delItem->GetRecordingID()))
        togglePlayListItem(delItem);

    if (!forceMetadataDelete)
        delItem->UpdateLastDelete(true);

    delItem->SetAvailableStatus(asPendingDelete, "RemoveProgram");
    m_helper.DeleteRecording( delItem->GetRecordingID(),
                              forceMetadataDelete, forgetHistory);

    // if the item is in the current recording list UI then delete it.
    MythUIButtonListItem *uiItem =
        m_recordingList->GetItemByData(QVariant::fromValue(delItem));
    if (uiItem)
        m_recordingList->RemoveItem(uiItem);
}

void PlaybackBox::fanartLoad(void)
{
    m_artImage[kArtworkFanart]->Load();
}

void PlaybackBox::bannerLoad(void)
{
    m_artImage[kArtworkBanner]->Load();
}

void PlaybackBox::coverartLoad(void)
{
    m_artImage[kArtworkCoverart]->Load();
}

void PlaybackBox::ShowDeletePopup(DeletePopupType type)
{
    QString label;
    switch (type)
    {
        case kDeleteRecording:
            label = tr("Are you sure you want to delete:"); break;
        case kForceDeleteRecording:
            label = tr("Recording file does not exist.\n"
                       "Are you sure you want to delete:");
            break;
        case kStopRecording:
            label = tr("Are you sure you want to stop:"); break;
    }

    ProgramInfo *delItem = nullptr;
    if (m_delList.empty())
    {
        delItem = GetCurrentProgram();
        if (delItem != nullptr)
            push_onto_del(m_delList, *delItem);
    }
    else if (m_delList.size() >= 3)
    {
        delItem = FindProgramInUILists(m_delList[0].toUInt());
    }

    if (!delItem)
        return;

    uint other_delete_cnt = (m_delList.size() / 3) - 1;

    label += CreateProgramInfoString(*delItem);

    m_popupMenu = new MythMenu(label, this, "deletemenu");

    if ((kDeleteRecording == type) &&
        delItem->GetRecordingGroup() != "Deleted" &&
        delItem->GetRecordingGroup() != "LiveTV")
    {
        m_popupMenu->AddItem(tr("Yes, and allow re-record"),
                             &PlaybackBox::DeleteForgetHistory);
    }

    bool defaultIsYes =
        ((kDeleteRecording      != type) &&
         (kForceDeleteRecording != type) &&
         (delItem->QueryAutoExpire() != kDisableAutoExpire));

    switch (type)
    {
        case kDeleteRecording:
            m_popupMenu->AddItem(tr("Yes, delete it"),
                                 qOverload<>(&PlaybackBox::Delete), nullptr, defaultIsYes);
            break;
        case kForceDeleteRecording:
            m_popupMenu->AddItem(tr("Yes, delete it"),
                                 &PlaybackBox::DeleteForce, nullptr, defaultIsYes);
            break;
        case kStopRecording:
            m_popupMenu->AddItem(tr("Yes, stop recording"),
                                 &PlaybackBox::StopSelected, nullptr, defaultIsYes);
            break;
    }


    if ((kForceDeleteRecording == type) && other_delete_cnt)
    {
        m_popupMenu->AddItem(
            tr("Yes, delete it and the remaining %1 list items")
            .arg(other_delete_cnt), &PlaybackBox::DeleteForceAllRemaining);
    }

    switch (type)
    {
        case kDeleteRecording:
        case kForceDeleteRecording:
            m_popupMenu->AddItem(tr("No, keep it"), &PlaybackBox::DeleteIgnore,
                                 nullptr, !defaultIsYes);
            break;
        case kStopRecording:
            m_popupMenu->AddItem(tr("No, continue recording"), &PlaybackBox::DeleteIgnore,
                                 nullptr, !defaultIsYes);
            break;
    }

    if ((type == kForceDeleteRecording) && other_delete_cnt)
    {
        m_popupMenu->AddItem(
            tr("No, and keep the remaining %1 list items")
            .arg(other_delete_cnt),
            &PlaybackBox::DeleteIgnoreAllRemaining);
    }

    DisplayPopupMenu();
}

void PlaybackBox::ShowAvailabilityPopup(const ProgramInfo &pginfo)
{
    QString msg = pginfo.toString(ProgramInfo::kTitleSubtitle, " ");
    msg += "\n";

    QString byWho;
    switch (pginfo.GetAvailableStatus())
    {
        case asAvailable:
            if (pginfo.QueryIsInUse(byWho))
            {
                ShowNotification(tr("Recording Available\n"),
                                      sLocation, msg +
                                 tr("This recording is currently in "
                                    "use by:") + "\n" + byWho);
            }
            else
            {
                ShowNotification(tr("Recording Available\n"),
                                      sLocation, msg +
                                 tr("This recording is currently "
                                    "Available"));
            }
            break;
        case asPendingDelete:
            ShowNotificationError(tr("Recording Unavailable\n"),
                                  sLocation, msg +
                                  tr("This recording is currently being "
                                     "deleted and is unavailable"));
            break;
        case asDeleted:
            ShowNotificationError(tr("Recording Unavailable\n"),
                                  sLocation, msg +
                                  tr("This recording has been "
                                     "deleted and is unavailable"));
            break;
        case asFileNotFound:
            ShowNotificationError(tr("Recording Unavailable\n"),
                                  sLocation, msg +
                                  tr("The file for this recording can "
                                     "not be found"));
            break;
        case asZeroByte:
            ShowNotificationError(tr("Recording Unavailable\n"),
                                  sLocation, msg +
                                  tr("The file for this recording is "
                                     "empty."));
            break;
        case asNotYetAvailable:
            ShowNotificationError(tr("Recording Unavailable\n"),
                                  sLocation, msg +
                                  tr("This recording is not yet "
                                     "available."));
    }
}

MythMenu* PlaybackBox::createPlaylistMenu(void)
{
    QString label = tr("There is %n item(s) in the playlist. Actions affect "
           "all items in the playlist", "", m_playList.size());

    auto *menu = new MythMenu(label, this, "slotmenu");

    menu->AddItem(tr("Play"), &PlaybackBox::doPlayList);
    menu->AddItem(tr("Shuffle Play"), &PlaybackBox::doPlayListRandom);
    menu->AddItem(tr("Clear Playlist"), &PlaybackBox::doClearPlaylist);

    if (GetFocusWidget() == m_groupList)
    {
        if ((m_viewMask & VIEW_TITLES))
        {
            menu->AddItem(tr("Toggle playlist for this Category/Title"),
                          &PlaybackBox::togglePlayListTitle);
        }
        else
        {
            menu->AddItem(tr("Toggle playlist for this Group"),
                          &PlaybackBox::togglePlayListTitle);
        }
    }
    else
    {
        menu->AddItem(tr("Toggle playlist for this recording"),
                      qOverload<>(&PlaybackBox::togglePlayListItem));
    }

    menu->AddItem(tr("Storage Options"), nullptr, createPlaylistStorageMenu());
    menu->AddItem(tr("Job Options"), nullptr, createPlaylistJobMenu());
    menu->AddItem(tr("Delete"), &PlaybackBox::PlaylistDeleteKeepHistory);
    menu->AddItem(tr("Delete, and allow re-record"),
                  &PlaybackBox::PlaylistDeleteForgetHistory);

    return menu;
}

MythMenu* PlaybackBox::createPlaylistStorageMenu()
{
    QString label = tr("There is %n item(s) in the playlist. Actions affect "
           "all items in the playlist", "", m_playList.size());

    auto *menu = new MythMenu(label, this, "slotmenu");

    menu->AddItem(tr("Change Recording Group"), &PlaybackBox::ShowRecGroupChangerUsePlaylist);
    menu->AddItem(tr("Change Playback Group"), &PlaybackBox::ShowPlayGroupChangerUsePlaylist);
    menu->AddItem(tr("Disable Auto Expire"), &PlaybackBox::doPlaylistExpireSetOff);
    menu->AddItem(tr("Enable Auto Expire"), &PlaybackBox::doPlaylistExpireSetOn);
    menu->AddItem(tr("Mark as Watched"), &PlaybackBox::doPlaylistWatchedSetOn);
    menu->AddItem(tr("Mark as Unwatched"), &PlaybackBox::doPlaylistWatchedSetOff);
    menu->AddItem(tr("Allow Re-record"), &PlaybackBox::doPlaylistAllowRerecord);

    return menu;
}

MythMenu* PlaybackBox::createPlaylistJobMenu(void)
{
    QString label = tr("There is %n item(s) in the playlist. Actions affect "
           "all items in the playlist", "", m_playList.size());

    auto *menu = new MythMenu(label, this, "slotmenu");

    QString jobTitle;
    QString command;
    QList<uint>::Iterator it;
    bool isTranscoding = true;
    bool isFlagging = true;
    bool isMetadataLookup = true;
    bool isRunningUserJob1 = true;
    bool isRunningUserJob2 = true;
    bool isRunningUserJob3 = true;
    bool isRunningUserJob4 = true;

    for(it = m_playList.begin(); it != m_playList.end(); ++it)
    {
        ProgramInfo *tmpItem = FindProgramInUILists(*it);
        if (tmpItem)
        {
            if (!JobQueue::IsJobQueuedOrRunning(
                    JOB_TRANSCODE,
                    tmpItem->GetChanID(), tmpItem->GetRecordingStartTime()))
                isTranscoding = false;
            if (!JobQueue::IsJobQueuedOrRunning(
                    JOB_COMMFLAG,
                    tmpItem->GetChanID(), tmpItem->GetRecordingStartTime()))
                isFlagging = false;
            if (!JobQueue::IsJobQueuedOrRunning(
                    JOB_METADATA,
                    tmpItem->GetChanID(), tmpItem->GetRecordingStartTime()))
                isMetadataLookup = false;
            if (!JobQueue::IsJobQueuedOrRunning(
                    JOB_USERJOB1,
                    tmpItem->GetChanID(), tmpItem->GetRecordingStartTime()))
                isRunningUserJob1 = false;
            if (!JobQueue::IsJobQueuedOrRunning(
                    JOB_USERJOB2,
                    tmpItem->GetChanID(), tmpItem->GetRecordingStartTime()))
                isRunningUserJob2 = false;
            if (!JobQueue::IsJobQueuedOrRunning(
                    JOB_USERJOB3,
                    tmpItem->GetChanID(), tmpItem->GetRecordingStartTime()))
                isRunningUserJob3 = false;
            if (!JobQueue::IsJobQueuedOrRunning(
                    JOB_USERJOB4,
                    tmpItem->GetChanID(), tmpItem->GetRecordingStartTime()))
                isRunningUserJob4 = false;
            if (!isTranscoding && !isFlagging && !isRunningUserJob1 &&
                !isRunningUserJob2 && !isRunningUserJob3 && !isRunningUserJob4)
                break;
        }
    }

    if (!isTranscoding)
        menu->AddItem(tr("Begin Transcoding"), &PlaybackBox::doPlaylistBeginTranscoding);
    else
        menu->AddItem(tr("Stop Transcoding"), &PlaybackBox::stopPlaylistTranscoding);

    if (!isFlagging)
        menu->AddItem(tr("Begin Commercial Detection"), &PlaybackBox::doPlaylistBeginFlagging);
    else
        menu->AddItem(tr("Stop Commercial Detection"), &PlaybackBox::stopPlaylistFlagging);

    if (!isMetadataLookup)
        menu->AddItem(tr("Begin Metadata Lookup"), &PlaybackBox::doPlaylistBeginLookup);
    else
        menu->AddItem(tr("Stop Metadata Lookup"), &PlaybackBox::stopPlaylistLookup);

    command = gCoreContext->GetSetting("UserJob1", "");
    if (!command.isEmpty())
    {
        jobTitle = gCoreContext->GetSetting("UserJobDesc1");

        if (!isRunningUserJob1)
        {
            menu->AddItem(tr("Begin") + ' ' + jobTitle,
                          &PlaybackBox::doPlaylistBeginUserJob1);
        }
        else
        {
            menu->AddItem(tr("Stop") + ' ' + jobTitle,
                          &PlaybackBox::stopPlaylistUserJob1);
        }
    }

    command = gCoreContext->GetSetting("UserJob2", "");
    if (!command.isEmpty())
    {
        jobTitle = gCoreContext->GetSetting("UserJobDesc2");

        if (!isRunningUserJob2)
        {
            menu->AddItem(tr("Begin") + ' ' + jobTitle,
                          &PlaybackBox::doPlaylistBeginUserJob2);
        }
        else
        {
            menu->AddItem(tr("Stop") + ' ' + jobTitle,
                          &PlaybackBox::stopPlaylistUserJob2);
        }
    }

    command = gCoreContext->GetSetting("UserJob3", "");
    if (!command.isEmpty())
    {
        jobTitle = gCoreContext->GetSetting("UserJobDesc3");

        if (!isRunningUserJob3)
        {
            menu->AddItem(tr("Begin") + ' ' + jobTitle,
                          &PlaybackBox::doPlaylistBeginUserJob3);
        }
        else
        {
            menu->AddItem(tr("Stop") + ' ' + jobTitle,
                          &PlaybackBox::stopPlaylistUserJob3);
        }
    }

    command = gCoreContext->GetSetting("UserJob4", "");
    if (!command.isEmpty())
    {
        jobTitle = gCoreContext->GetSetting("UserJobDesc4");

        if (!isRunningUserJob4)
        {
            menu->AddItem(QString("%1 %2").arg(tr("Begin"), jobTitle),
                          &PlaybackBox::doPlaylistBeginUserJob4);
        }
        else
        {
            menu->AddItem(QString("%1 %2").arg(tr("Stop"), jobTitle),
                          &PlaybackBox::stopPlaylistUserJob4);
        }
    }

    return menu;
}

void PlaybackBox::DisplayPopupMenu(void)
{
    if (m_menuDialog || !m_popupMenu)
        return;

    m_menuDialog = new MythDialogBox(m_popupMenu, m_popupStack, "pbbmainmenupopup");

    if (m_menuDialog->Create())
    {
        m_popupStack->AddScreen(m_menuDialog);
        connect(m_menuDialog, &MythDialogBox::Closed, this, &PlaybackBox::popupClosed);
    }
    else
    {
        delete m_menuDialog;
    }
}

void PlaybackBox::ShowMenu()
{
    if (m_menuDialog)
        return;

    if (GetFocusWidget() == m_groupList)
        ShowGroupPopup();
    else
    {
        ProgramInfo *pginfo = GetCurrentProgram();
        if (pginfo)
        {
            m_helper.CheckAvailability(
                *pginfo, kCheckForMenuAction);

            if ((asPendingDelete == pginfo->GetAvailableStatus()) ||
                (asDeleted == pginfo->GetAvailableStatus()) ||
                (asNotYetAvailable == pginfo->GetAvailableStatus()))
            {
                ShowAvailabilityPopup(*pginfo);
            }
            else
            {
                ShowActionPopup(*pginfo);
            }
        }
        else
        {
            ShowGroupPopup();
        }
    }
}

MythMenu* PlaybackBox::createPlayFromMenu()
{
    ProgramInfo *pginfo = GetCurrentProgram();
    if (!pginfo)
        return nullptr;

    QString title = tr("Play Options") + CreateProgramInfoString(*pginfo);

    auto *menu = new MythMenu(title, this, "slotmenu");
    bool hasLastPlay = pginfo->IsLastPlaySet();
    bool hasBookMark = pginfo->IsBookmarkSet();
    if (hasLastPlay)
        menu->AddItem(tr("Play from last played position"),
                      qOverload<>(&PlaybackBox::PlayFromLastPlayPos));
    if (hasBookMark)
        menu->AddItem(tr("Play from bookmark"),
                      qOverload<>(&PlaybackBox::PlayFromBookmark));
    menu->AddItem(tr("Play from beginning"),
                  qOverload<>(&PlaybackBox::PlayFromBeginning));
    if (hasLastPlay)
        menu->AddItem(tr("Clear last played position"),
                      &PlaybackBox::ClearLastPlayPos);
    if (hasBookMark)
        menu->AddItem(tr("Clear bookmark"), &PlaybackBox::ClearBookmark);

    return menu;
}

MythMenu* PlaybackBox::createStorageMenu()
{
    ProgramInfo *pginfo = GetCurrentProgram();
    if (!pginfo)
        return nullptr;

    QString title = tr("Storage Options") + CreateProgramInfoString(*pginfo);
    QString autoExpireText = (pginfo->IsAutoExpirable()) ?
        tr("Disable Auto Expire") : tr("Enable Auto Expire");
    QString preserveText = (pginfo->IsPreserved()) ?
        tr("Do not preserve this episode") : tr("Preserve this episode");

    auto *menu = new MythMenu(title, this, "slotmenu");
    menu->AddItem(tr("Change Recording Group"), &PlaybackBox::ShowRecGroupChangerNoPlaylist);
    menu->AddItem(tr("Change Playback Group"), &PlaybackBox::ShowPlayGroupChangerNoPlaylist);
    menu->AddItem(autoExpireText, &PlaybackBox::toggleAutoExpire);
    menu->AddItem(preserveText, &PlaybackBox::togglePreserveEpisode);

    return menu;
}

MythMenu* PlaybackBox::createRecordingMenu(void)
{
    ProgramInfo *pginfo = GetCurrentProgram();
    if (!pginfo)
        return nullptr;

    QString title = tr("Scheduling Options") + CreateProgramInfoString(*pginfo);

    auto *menu = new MythMenu(title, this, "slotmenu");

    menu->AddItem(tr("Edit Recording Schedule"),
                  qOverload<>(&PlaybackBox::EditScheduled));

    menu->AddItem(tr("Allow this episode to re-record"), &PlaybackBox::doAllowRerecord);

    menu->AddItem(tr("Show Recording Details"), &PlaybackBox::ShowDetails);

    menu->AddItem(tr("Change Recording Metadata"), &PlaybackBox::showMetadataEditor);

    menu->AddItem(tr("Custom Edit"), &PlaybackBox::EditCustom);

    return menu;
}

static const std::array<const int,kMaxJobs> kJobs
{
    JOB_TRANSCODE,
    JOB_COMMFLAG,
    JOB_METADATA,
    JOB_USERJOB1,
    JOB_USERJOB2,
    JOB_USERJOB3,
    JOB_USERJOB4,
};
std::array<PlaybackBoxCb,kMaxJobs*2> PlaybackBox::kMySlots
{   // stop                           start
    &PlaybackBox::doBeginTranscoding, &PlaybackBox::doCreateTranscodingProfilesMenu,
    &PlaybackBox::doBeginFlagging,    &PlaybackBox::doBeginFlagging,
    &PlaybackBox::doBeginLookup,      &PlaybackBox::doBeginLookup,
    &PlaybackBox::doBeginUserJob1,    &PlaybackBox::doBeginUserJob1,
    &PlaybackBox::doBeginUserJob2,    &PlaybackBox::doBeginUserJob2,
    &PlaybackBox::doBeginUserJob3,    &PlaybackBox::doBeginUserJob3,
    &PlaybackBox::doBeginUserJob4,    &PlaybackBox::doBeginUserJob4,
};

MythMenu* PlaybackBox::createJobMenu()
{
    ProgramInfo *pginfo = GetCurrentProgram();
    if (!pginfo)
        return nullptr;

    QString title = tr("Job Options") + CreateProgramInfoString(*pginfo);

    auto *menu = new MythMenu(title, this, "slotmenu");

    const std::array<const bool,kMaxJobs> add
    {
        true,
        true,
        true,
        !gCoreContext->GetSetting("UserJob1", "").isEmpty(),
        !gCoreContext->GetSetting("UserJob2", "").isEmpty(),
        !gCoreContext->GetSetting("UserJob3", "").isEmpty(),
        !gCoreContext->GetSetting("UserJob4", "").isEmpty(),
    };
    const std::array<const QString,kMaxJobs*2> desc
    {
        // stop                         start
        tr("Stop Transcoding"),         tr("Begin Transcoding"),
        tr("Stop Commercial Detection"), tr("Begin Commercial Detection"),
        tr("Stop Metadata Lookup"),      tr("Begin Metadata Lookup"),
        "1",                            "1",
        "2",                            "2",
        "3",                            "3",
        "4",                            "4",
    };

    for (size_t i = 0; i < kMaxJobs; i++)
    {
        if (!add[i])
            continue;

        QString stop_desc  = desc[(i*2)+0];
        QString start_desc = desc[(i*2)+1];

        if (start_desc.toUInt())
        {
            QString jobTitle = gCoreContext->GetSetting(
                "UserJobDesc"+start_desc, tr("User Job") + " #" + start_desc);
            stop_desc  = tr("Stop")  + ' ' + jobTitle;
            start_desc = tr("Begin") + ' ' + jobTitle;
        }

        bool running = JobQueue::IsJobQueuedOrRunning(
            kJobs[i], pginfo->GetChanID(), pginfo->GetRecordingStartTime());

        MythMenu *submenu = ((kJobs[i] == JOB_TRANSCODE) && !running)
            ? createTranscodingProfilesMenu() : nullptr;
        menu->AddItem((running) ? stop_desc : start_desc,
                      kMySlots[(i * 2) + (running ? 0 : 1)], submenu);
    }

    return menu;
}

MythMenu* PlaybackBox::createTranscodingProfilesMenu()
{
    QString label = tr("Transcoding profiles");

    auto *menu = new MythMenu(label, this, "transcode");

    menu->AddItemV(tr("Default"), QVariant::fromValue(-1));
    menu->AddItemV(tr("Autodetect"), QVariant::fromValue(0));

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT r.name, r.id "
                  "FROM recordingprofiles r, profilegroups p "
                  "WHERE p.name = 'Transcoders' "
                  "AND r.profilegroup = p.id "
                  "AND r.name != 'RTjpeg/MPEG4' "
                  "AND r.name != 'MPEG2' ");

    if (!query.exec())
    {
        MythDB::DBError(LOC + "unable to query transcoders", query);
        return nullptr;
    }

    while (query.next())
    {
        QString transcoder_name = query.value(0).toString();
        int transcoder_id = query.value(1).toInt();

        // Translatable strings for known profiles
        if (transcoder_name == "High Quality")
            transcoder_name = tr("High Quality");
        else if (transcoder_name == "Medium Quality")
            transcoder_name = tr("Medium Quality");
        else if (transcoder_name == "Low Quality")
            transcoder_name = tr("Low Quality");

        menu->AddItemV(transcoder_name, QVariant::fromValue(transcoder_id));
    }

    return menu;
}

void PlaybackBox::changeProfileAndTranscode(int id)
{
    ProgramInfo *pginfo = GetCurrentProgram();

    if (!pginfo)
        return;

    if (id >= 0)
    {
        RecordingInfo ri(*pginfo);
        ri.ApplyTranscoderProfileChangeById(id);
    }
    doBeginTranscoding();
}

void PlaybackBox::ShowActionPopup(const ProgramInfo &pginfo)
{
    QString label;
    if (asFileNotFound == pginfo.GetAvailableStatus())
        label = tr("Recording file cannot be found");
    else if (asZeroByte == pginfo.GetAvailableStatus())
        label = tr("Recording file contains no data");
    else
        tr("Recording Options");

    m_popupMenu = new MythMenu(label + CreateProgramInfoString(pginfo), this, "actionmenu");

    if ((asFileNotFound  == pginfo.GetAvailableStatus()) ||
        (asZeroByte      == pginfo.GetAvailableStatus()))
    {
        if (m_playList.contains(pginfo.GetRecordingID()))
        {
            m_popupMenu->AddItem(tr("Remove from Playlist"),
                                 qOverload<>(&PlaybackBox::togglePlayListItem));
        }
        else
        {
            m_popupMenu->AddItem(tr("Add to Playlist"),
                                 qOverload<>(&PlaybackBox::togglePlayListItem));
        }

        if (!m_playList.isEmpty())
            m_popupMenu->AddItem(tr("Playlist Options"), nullptr, createPlaylistMenu());

        m_popupMenu->AddItem(tr("Recording Options"), nullptr, createRecordingMenu());

        if (m_groupList->GetItemPos(m_groupList->GetItemCurrent()) == 0 ||
            m_groupList->GetItemCurrent()->GetData().toString() == m_watchGroupLabel)
        {
            m_popupMenu->AddItem(tr("List Recorded Episodes"),
                                 &PlaybackBox::ShowRecordedEpisodes);
        }
        else
        {
            m_popupMenu->AddItem(tr("List All Recordings"),
                                 &PlaybackBox::ShowAllRecordings);
        }

        m_popupMenu->AddItem(tr("Delete"), &PlaybackBox::askDelete);

        DisplayPopupMenu();

        return;
    }

    bool sameProgram = false;

    if (m_player)
        sameProgram = m_player->IsSameProgram(&pginfo);

    TVState tvstate = kState_None;

    if (!sameProgram)
    {
        if (pginfo.IsBookmarkSet() || pginfo.IsLastPlaySet())
            m_popupMenu->AddItem(tr("Play from..."), nullptr, createPlayFromMenu());
        else
            m_popupMenu->AddItem(tr("Play"),
                                 qOverload<>(&PlaybackBox::PlayFromAnyMark));
    }

    if (!m_player)
    {
        if (m_playList.contains(pginfo.GetRecordingID()))
        {
            m_popupMenu->AddItem(tr("Remove from Playlist"),
                                 qOverload<>(&PlaybackBox::togglePlayListItem));
        }
        else
        {
            m_popupMenu->AddItem(tr("Add to Playlist"),
                                 qOverload<>(&PlaybackBox::togglePlayListItem));
        }
        if (!m_playList.isEmpty())
        {
            m_popupMenu->AddItem(tr("Playlist Options"), nullptr, createPlaylistMenu());
        }
    }

    if ((pginfo.GetRecordingStatus() == RecStatus::Recording ||
         pginfo.GetRecordingStatus() == RecStatus::Tuning ||
         pginfo.GetRecordingStatus() == RecStatus::Failing) &&
        (!sameProgram ||
           (tvstate != kState_WatchingLiveTV &&
            tvstate != kState_WatchingRecording)))
    {
        m_popupMenu->AddItem(tr("Stop Recording"), &PlaybackBox::askStop);
    }

    if (pginfo.IsWatched())
        m_popupMenu->AddItem(tr("Mark as Unwatched"), &PlaybackBox::toggleWatched);
    else
        m_popupMenu->AddItem(tr("Mark as Watched"), &PlaybackBox::toggleWatched);

    m_popupMenu->AddItem(tr("Storage Options"), nullptr, createStorageMenu());
    m_popupMenu->AddItem(tr("Recording Options"), nullptr, createRecordingMenu());
    m_popupMenu->AddItem(tr("Job Options"), nullptr, createJobMenu());

    if (m_groupList->GetItemPos(m_groupList->GetItemCurrent()) == 0 ||
        m_groupList->GetItemCurrent()->GetData().toString() == m_watchGroupLabel)
    {
        m_popupMenu->AddItem(tr("List Recorded Episodes"),
                             &PlaybackBox::ShowRecordedEpisodes);
    }
    else
    {
        m_popupMenu->AddItem(tr("List All Recordings"),
                             &PlaybackBox::ShowAllRecordings);
    }

    if (!sameProgram)
    {
        if (pginfo.GetRecordingGroup() == "Deleted")
        {
            push_onto_del(m_delList, pginfo);
            m_popupMenu->AddItem(tr("Undelete"), &PlaybackBox::Undelete);
            m_popupMenu->AddItem(tr("Delete Forever"), qOverload<>(&PlaybackBox::Delete));
        }
        else
        {
            m_popupMenu->AddItem(tr("Delete"), &PlaybackBox::askDelete);
        }
    }

    DisplayPopupMenu();
}

QString PlaybackBox::CreateProgramInfoString(const ProgramInfo &pginfo)
{
    QDateTime recstartts = pginfo.GetRecordingStartTime();
    QDateTime recendts   = pginfo.GetRecordingEndTime();

    QString timedate = QString("%1 - %2")
        .arg(MythDate::toString(
                 recstartts, MythDate::kDateTimeFull | MythDate::kSimplify),
             MythDate::toString(recendts, MythDate::kTime));

    QString title = pginfo.GetTitle();

    QString extra;

    if (!pginfo.GetSubtitle().isEmpty())
    {
        extra = QString('\n') + pginfo.GetSubtitle();
    }

    return QString("\n%1%2\n%3").arg(title, extra, timedate);
}

void PlaybackBox::doClearPlaylist(void)
{
    QList<uint>::Iterator it;
    for (it = m_playList.begin(); it != m_playList.end(); ++it)
    {
        ProgramInfo *tmpItem = FindProgramInUILists(*it);

        if (!tmpItem)
            continue;

        MythUIButtonListItem *item =
            m_recordingList->GetItemByData(QVariant::fromValue(tmpItem));

        if (item)
            item->DisplayState("no", "playlist");
    }
    m_playList.clear();
}

void PlaybackBox::doPlayList(void)
{
    playSelectedPlaylist(false);
}


void PlaybackBox::doPlayListRandom(void)
{
    playSelectedPlaylist(true);
}

void PlaybackBox::askStop(void)
{
    ProgramInfo *pginfo = GetCurrentProgram();
    if (pginfo)
    {
        push_onto_del(m_delList, *pginfo);
        ShowDeletePopup(kStopRecording);
    }
}

/**
 *  \brief Callback function when Allow Re-record is pressed in Watch Recordings
 *
 * Hide the current program from the scheduler by calling ForgetHistory
 * This will allow it to re-record without deleting
 */
void PlaybackBox::doAllowRerecord()
{
   ProgramInfo *pginfo = GetCurrentProgram();

    if (!pginfo)
        return;

    RecordingInfo ri(*pginfo);
    ri.ForgetHistory();
    *pginfo = ri;
}

void PlaybackBox::doPlaylistAllowRerecord()
{
    QList<uint>::Iterator it;

    for (it = m_playList.begin(); it != m_playList.end(); ++it)
    {
        ProgramInfo *pginfo = FindProgramInUILists(*it);
        if (pginfo != nullptr)
        {
            RecordingInfo ri(*pginfo);
            ri.ForgetHistory();
            *pginfo = ri;
        }
    }

    doClearPlaylist();
    UpdateUILists();
}

void PlaybackBox::doJobQueueJob(int jobType, int jobFlags)
{
   ProgramInfo *pginfo = GetCurrentProgram();

    if (!pginfo)
        return;

    ProgramInfo *tmpItem = FindProgramInUILists(*pginfo);

    if (JobQueue::IsJobQueuedOrRunning(
            jobType, pginfo->GetChanID(), pginfo->GetRecordingStartTime()))
    {
        JobQueue::ChangeJobCmds(
            jobType, pginfo->GetChanID(), pginfo->GetRecordingStartTime(),
            JOB_STOP);
        if ((jobType & JOB_COMMFLAG) && (tmpItem))
        {
            tmpItem->SetEditing(false);
            tmpItem->SetFlagging(false);
        }
    }
    else
    {
        QString jobHost;
        if (gCoreContext->GetBoolSetting("JobsRunOnRecordHost", false))
            jobHost = pginfo->GetHostname();

        JobQueue::QueueJob(jobType, pginfo->GetChanID(),
                           pginfo->GetRecordingStartTime(), "", "", jobHost,
                           jobFlags);
    }
}

void PlaybackBox::doBeginFlagging()
{
    doJobQueueJob(JOB_COMMFLAG);
}

void PlaybackBox::doBeginLookup()
{
    doJobQueueJob(JOB_METADATA);
}

void PlaybackBox::doPlaylistJobQueueJob(int jobType, int jobFlags)
{
    for (const uint pbs : std::as_const(m_playList))
    {
        ProgramInfo *tmpItem = FindProgramInUILists(pbs);
        if (tmpItem &&
            (!JobQueue::IsJobQueuedOrRunning(
                jobType,
                tmpItem->GetChanID(), tmpItem->GetRecordingStartTime())))
        {
            QString jobHost;
            if (gCoreContext->GetBoolSetting("JobsRunOnRecordHost", false))
                jobHost = tmpItem->GetHostname();

            JobQueue::QueueJob(jobType, tmpItem->GetChanID(),
                               tmpItem->GetRecordingStartTime(),
                               "", "", jobHost, jobFlags);
        }
    }
}

void PlaybackBox::stopPlaylistJobQueueJob(int jobType)
{
    QList<uint>::Iterator it;

    for (it = m_playList.begin(); it != m_playList.end(); ++it)
    {
        ProgramInfo *tmpItem = FindProgramInUILists(*it);
        if (tmpItem &&
            (JobQueue::IsJobQueuedOrRunning(
                jobType,
                tmpItem->GetChanID(), tmpItem->GetRecordingStartTime())))
        {
            JobQueue::ChangeJobCmds(
                jobType, tmpItem->GetChanID(),
                tmpItem->GetRecordingStartTime(), JOB_STOP);

            if (jobType & JOB_COMMFLAG)
            {
                tmpItem->SetEditing(false);
                tmpItem->SetFlagging(false);
            }
        }
    }
}

void PlaybackBox::askDelete()
{
    ProgramInfo *pginfo = GetCurrentProgram();
    if (pginfo)
    {
        push_onto_del(m_delList, *pginfo);
        ShowDeletePopup(kDeleteRecording);
    }
}

void PlaybackBox::PlaylistDelete(bool forgetHistory)
{
    QString forceDeleteStr("0");

    QStringList list;
    for (int id : std::as_const(m_playList))
    {
        ProgramInfo *tmpItem = FindProgramInUILists(id);
        if (tmpItem && tmpItem->QueryIsDeleteCandidate())
        {
            tmpItem->SetAvailableStatus(asPendingDelete, "PlaylistDelete");
            list.push_back(QString::number(tmpItem->GetRecordingID()));
            list.push_back(forceDeleteStr);
            list.push_back(forgetHistory ? "1" : "0");

            // if the item is in the current recording list UI then delete it.
            MythUIButtonListItem *uiItem =
                m_recordingList->GetItemByData(QVariant::fromValue(tmpItem));
            if (uiItem)
                m_recordingList->RemoveItem(uiItem);
        }
    }
    m_playList.clear();

    if (!list.empty())
        m_helper.DeleteRecordings(list);

    doClearPlaylist();
}

// FIXME: Huh? This doesn't specify which recording to undelete, it just
// undeletes the first one on the list
void PlaybackBox::Undelete(void)
{
    uint recordingID = 0;
    if (extract_one_del(m_delList, recordingID))
        m_helper.UndeleteRecording(recordingID);
}

void PlaybackBox::Delete(DeleteFlags flags)
{
    uint recordingID = 0;
    while (extract_one_del(m_delList, recordingID))
    {
        if (flags & kIgnore)
            continue;

        RemoveProgram(recordingID, (flags & kForgetHistory) != 0, (flags & kForce) != 0);

        if (!(flags & kAllRemaining))
            break;
    }

    if (!m_delList.empty())
    {
        auto *e = new MythEvent("DELETE_FAILURES", m_delList);
        m_delList.clear();
        QCoreApplication::postEvent(this, e);
    }
}

void PlaybackBox::ShowRecordedEpisodes()
{
    ProgramInfo *pginfo = GetCurrentProgram();
    if (pginfo) {
        QString title = pginfo->GetTitle().toLower();
        MythUIButtonListItem* group = m_groupList->GetItemByData(QVariant::fromValue(title));
        if (group)
        {
            m_groupList->SetItemCurrent(group);
            // set focus back to previous item
            MythUIButtonListItem *previousItem = m_recordingList->GetItemByData(QVariant::fromValue(pginfo));
            m_recordingList->SetItemCurrent(previousItem);
        }
    }
}

void PlaybackBox::ShowAllRecordings(void)
{
    ProgramInfo *pginfo = GetCurrentProgram();
    m_groupList->SetItemCurrent(0);
    if (pginfo)
    {
        // set focus back to previous item
        MythUIButtonListItem *previousitem =
            m_recordingList->GetItemByData(QVariant::fromValue(pginfo));
        m_recordingList->SetItemCurrent(previousitem);
    }
}

ProgramInfo *PlaybackBox::FindProgramInUILists(const ProgramInfo &pginfo)
{
    return FindProgramInUILists( pginfo.GetRecordingID(),
                                 pginfo.GetRecordingGroup());
}

ProgramInfo *PlaybackBox::FindProgramInUILists(uint recordingID,
                                               const QString& recgroup)
{
    // LiveTV ProgramInfo's are not in the aggregated list
    std::array<ProgramList::iterator,2> _it {
        m_progLists[tr("Live TV").toLower()].begin(), m_progLists[""].begin() };
    std::array<ProgramList::iterator,2> _end {
        m_progLists[tr("Live TV").toLower()].end(),   m_progLists[""].end()   };

    if (recgroup != "LiveTV")
    {
        swap( _it[0],  _it[1]);
        swap(_end[0], _end[1]);
    }

    for (uint i = 0; i < 2; i++)
    {
        auto it = _it[i];
        const auto& end = _end[i];
        for (; it != end; ++it)
        {
            if ((*it)->GetRecordingID() == recordingID)
            {
                return *it;
            }
        }
    }

    return nullptr;
}

void PlaybackBox::toggleWatched(void)
{
    MythUIButtonListItem *item = m_recordingList->GetItemCurrent();

    if (!item)
        return;

    auto *pginfo = item->GetData().value<ProgramInfo *>();

    if (!pginfo)
        return;

    bool on = !pginfo->IsWatched();
    pginfo->SaveWatched(on);
    item->DisplayState((on)?"yes":"on", "watched");
    updateIcons(pginfo);

    // A refill affects the responsiveness of the UI and we only
    // need to rebuild the list if the watch list is displayed
    if (m_viewMask & VIEW_WATCHLIST)
        UpdateUILists();
}

void PlaybackBox::toggleAutoExpire()
{
    MythUIButtonListItem *item = m_recordingList->GetItemCurrent();

    if (!item)
        return;

    auto *pginfo = item->GetData().value<ProgramInfo *>();

    if (!pginfo)
        return;

    bool on = !pginfo->IsAutoExpirable();
    pginfo->SaveAutoExpire((on) ? kNormalAutoExpire : kDisableAutoExpire, true);
    item->DisplayState((on)?"yes":"no", "autoexpire");
    updateIcons(pginfo);
}

void PlaybackBox::togglePreserveEpisode()
{
    MythUIButtonListItem *item = m_recordingList->GetItemCurrent();

    if (!item)
        return;

    auto *pginfo = item->GetData().value<ProgramInfo *>();

    if (!pginfo)
        return;

    bool on = !pginfo->IsPreserved();
    pginfo->SavePreserve(on);
    item->DisplayState(on?"yes":"no", "preserve");
    updateIcons(pginfo);
}

void PlaybackBox::toggleView(ViewMask itemMask, bool setOn)
{
    if (setOn)
        m_viewMask = (ViewMask)(m_viewMask | itemMask);
    else
        m_viewMask = (ViewMask)(m_viewMask & ~itemMask);

    UpdateUILists();
}

void PlaybackBox::togglePlayListTitle(void)
{
    QString groupname = m_groupList->GetItemCurrent()->GetData().toString();

    for (auto *pl : std::as_const(m_progLists[groupname]))
    {
        if (pl && (pl->GetAvailableStatus() == asAvailable))
            togglePlayListItem(pl);
    }
}

void PlaybackBox::togglePlayListItem(void)
{
    MythUIButtonListItem *item = m_recordingList->GetItemCurrent();

    if (!item)
        return;

    auto *pginfo = item->GetData().value<ProgramInfo *>();

    if (!pginfo)
        return;

    togglePlayListItem(pginfo);

    if (GetFocusWidget() == m_recordingList)
        m_recordingList->MoveDown(MythUIButtonList::MoveItem);
}

void PlaybackBox::togglePlayListItem(ProgramInfo *pginfo)
{
    if (!pginfo)
        return;

    uint recordingID = pginfo->GetRecordingID();

    MythUIButtonListItem *item =
                    m_recordingList->GetItemByData(QVariant::fromValue(pginfo));

    if (m_playList.contains(recordingID))
    {
        if (item)
            item->DisplayState("no", "playlist");

        m_playList.removeAll(recordingID);
    }
    else
    {
        if (item)
          item->DisplayState("yes", "playlist");
        m_playList.append(recordingID);
    }
}

void PlaybackBox::processNetworkControlCommands(void)
{
    int commands = 0;
    QString command;

    m_ncLock.lock();
    commands = m_networkControlCommands.size();
    m_ncLock.unlock();

    while (commands)
    {
        m_ncLock.lock();
        command = m_networkControlCommands.front();
        m_networkControlCommands.pop_front();
        m_ncLock.unlock();

        processNetworkControlCommand(command);

        m_ncLock.lock();
        commands = m_networkControlCommands.size();
        m_ncLock.unlock();
    }
}

void PlaybackBox::processNetworkControlCommand(const QString &command)
{
    QStringList tokens = command.simplified().split(" ");

    if (tokens.size() >= 4 && (tokens[1] == "PLAY" || tokens[1] == "RESUME"))
    {
        if (tokens.size() == 6 && tokens[2] == "PROGRAM")
        {
            int clientID = tokens[5].toInt();

            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("NetworkControl: Trying to %1 program '%2' @ '%3'")
                    .arg(tokens[1], tokens[3], tokens[4]));

            if (m_playingSomething)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "NetworkControl: Already playing");

                QString msg = QString(
                    "NETWORK_CONTROL RESPONSE %1 ERROR: Unable to play, "
                    "player is already playing another recording.")
                    .arg(clientID);

                MythEvent me(msg);
                gCoreContext->dispatch(me);
                return;
            }

            uint chanid = tokens[3].toUInt();
            QDateTime recstartts = MythDate::fromString(tokens[4]);
            ProgramInfo pginfo(chanid, recstartts);

            if (pginfo.GetChanID())
            {
                QString msg = QString("NETWORK_CONTROL RESPONSE %1 OK")
                                      .arg(clientID);
                MythEvent me(msg);
                gCoreContext->dispatch(me);

                pginfo.SetPathname(pginfo.GetPlaybackURL());

                const bool ignoreBookmark = (tokens[1] == "PLAY");
                const bool ignoreProgStart = true;
                const bool ignoreLastPlayPos = true;
                const bool underNetworkControl = true;
                PlayX(pginfo, ignoreBookmark, ignoreProgStart,
                      ignoreLastPlayPos, underNetworkControl);
            }
            else
            {
                QString message = QString("NETWORK_CONTROL RESPONSE %1 "
                                          "ERROR: Could not find recording for "
                                          "chanid %2 @ %3")
                    .arg(tokens[5], tokens[3], tokens[4]);
                MythEvent me(message);
                gCoreContext->dispatch(me);
            }
        }
    }
}

bool PlaybackBox::keyPressEvent(QKeyEvent *event)
{
    // This should be an impossible keypress we've simulated
    if ((event->key() == Qt::Key_LaunchMedia) &&
        (event->modifiers() ==
         (Qt::ShiftModifier |
          Qt::ControlModifier |
          Qt::AltModifier |
          Qt::MetaModifier |
          Qt::KeypadModifier)))
    {
        event->accept();
        m_ncLock.lock();
        int commands = m_networkControlCommands.size();
        m_ncLock.unlock();
        if (commands)
            processNetworkControlCommands();
        return true;
    }

    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("TV Frontend",
                                                     event, actions);

    for (int i = 0; i < actions.size() && !handled; ++i)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == ACTION_1 || action == "HELP")
            showIconHelp();
        else if (action == "MENU")
        {
            ShowMenu();
        }
        else if (action == "NEXTFAV")
        {
            if (GetFocusWidget() == m_groupList)
                togglePlayListTitle();
            else
                togglePlayListItem();
        }
        else if (action == "TOGGLEFAV")
        {
            m_playList.clear();
            UpdateUILists();
        }
        else if (action == ACTION_TOGGLERECORD)
        {
            m_viewMask = m_viewMaskToggle(m_viewMask, VIEW_TITLES);
            UpdateUILists();
        }
        else if (action == ACTION_PAGERIGHT)
        {
            SelectNextRecGroup();
        }
        else if (action == ACTION_PAGELEFT)
        {
            QString nextGroup;
            m_recGroupsLock.lock();
            if (m_recGroupIdx >= 0 && !m_recGroups.empty())
            {
                if (--m_recGroupIdx < 0)
                    m_recGroupIdx = m_recGroups.size() - 1;
                nextGroup = m_recGroups[m_recGroupIdx];
            }
            m_recGroupsLock.unlock();

            if (!nextGroup.isEmpty())
                displayRecGroup(nextGroup);
        }
        else if (action == "NEXTVIEW")
        {
            int curpos = m_groupList->GetItemPos(m_groupList->GetItemCurrent());
            if (++curpos >= m_groupList->GetCount())
                curpos = 0;
            m_groupList->SetItemCurrent(curpos);
        }
        else if (action == "PREVVIEW")
        {
            int curpos = m_groupList->GetItemPos(m_groupList->GetItemCurrent());
            if (--curpos < 0)
                curpos = m_groupList->GetCount() - 1;
            m_groupList->SetItemCurrent(curpos);
        }
        else if (action == ACTION_LISTRECORDEDEPISODES)
        {
            if (m_groupList->GetItemPos(m_groupList->GetItemCurrent()) == 0 ||
                m_groupList->GetItemCurrent()->GetData().toString() == m_watchGroupLabel)
                ShowRecordedEpisodes();
            else
                ShowAllRecordings();
        }
        else if (action == "CHANGERECGROUP")
        {
            showGroupFilter();
        }
        else if (action == "CHANGEGROUPVIEW")
        {
            showViewChanger();
        }
        else if (action == "EDIT")
        {
            EditScheduled();
        }
        else if (m_titleList.size() > 1)
        {
            if (action == "DELETE")
                deleteSelected(m_recordingList->GetItemCurrent());
            else if (action == ACTION_PLAYBACK)
                PlayFromAnyMark();
            else if (action == "DETAILS" || action == "INFO")
                ShowDetails();
            else if (action == "CUSTOMEDIT")
                EditCustom();
            else if (action == "GUIDE")
                ShowGuide();
            else if (action == "UPCOMING")
                ShowUpcoming();
            else if (action == ACTION_VIEWSCHEDULED)
                ShowUpcomingScheduled();
            else if (action == ACTION_PREVRECORDED)
                ShowPrevious();
            else
                handled = false;
        }
        else
        {
            handled = false;
        }
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void PlaybackBox::customEvent(QEvent *event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        auto *dce = dynamic_cast<DialogCompletionEvent*>(event);
        if (!dce)
            return;

        QString resultid = dce->GetId();

        if (resultid == "transcode" && dce->GetResult() >= 0)
            changeProfileAndTranscode(dce->GetData().toInt());
    }
    else if (event->type() == MythEvent::kMythEventMessage)
    {
        auto *me = dynamic_cast<MythEvent *>(event);
        if (me == nullptr)
            return;

        const QString& message = me->Message();

        if (message.startsWith("RECORDING_LIST_CHANGE"))
        {
            QStringList tokens = message.simplified().split(" ");
            uint recordingID = 0;
            if (tokens.size() >= 3)
                recordingID = tokens[2].toUInt();

            if ((tokens.size() >= 2) && tokens[1] == "UPDATE")
            {
                ProgramInfo evinfo(me->ExtraDataList());
                if (evinfo.HasPathname() || evinfo.GetChanID())
                 {
                     uint32_t flags = m_programInfoCache.Update(evinfo);
                     if (flags != ProgramInfoCache::PIC_NO_ACTION)
                         HandleUpdateItemEvent(evinfo.GetRecordingID(), flags);
                 }
            }
            else if (recordingID && (tokens[1] == "ADD"))
            {
                ProgramInfo evinfo(recordingID);
                if (evinfo.GetChanID())
                {
                    evinfo.SetRecordingStatus(RecStatus::Recording);
                    HandleRecordingAddEvent(evinfo);
                }
            }
            else if (recordingID && (tokens[1] == "DELETE"))
            {
                HandleRecordingRemoveEvent(recordingID);
            }
            else
            {
                m_programInfoCache.ScheduleLoad();
            }
        }
        else if (message.startsWith("NETWORK_CONTROL"))
        {
            QStringList tokens = message.simplified().split(" ");
            if ((tokens[1] != "ANSWER") && (tokens[1] != "RESPONSE"))
            {
                m_ncLock.lock();
                m_networkControlCommands.push_back(message);
                m_ncLock.unlock();

                // This should be an impossible keypress we're simulating
                Qt::KeyboardModifiers modifiers =
                    Qt::ShiftModifier |
                    Qt::ControlModifier |
                    Qt::AltModifier |
                    Qt::MetaModifier |
                    Qt::KeypadModifier;
                auto *keyevent = new QKeyEvent(QEvent::KeyPress,
                                               Qt::Key_LaunchMedia, modifiers);
                QCoreApplication::postEvent(GetMythMainWindow(), keyevent);

                keyevent = new QKeyEvent(QEvent::KeyRelease,
                                         Qt::Key_LaunchMedia, modifiers);
                QCoreApplication::postEvent(GetMythMainWindow(), keyevent);
            }
        }
        else if (message.startsWith("UPDATE_FILE_SIZE"))
        {
            QStringList tokens = message.simplified().split(" ");
            if (tokens.size() >= 3)
            {
                bool ok = false;
                uint recordingID  = tokens[1].toUInt();
                uint64_t filesize = tokens[2].toLongLong(&ok);
                if (ok)
                {
                    // Delegate to background thread
                    MConcurrent::run("UpdateFileSize", &m_programInfoCache,
                                     &ProgramInfoCache::UpdateFileSize,
                                     recordingID, filesize,
                                     ProgramInfoCache::PIC_NONE);
                }
            }
        }
        else if (message == "UPDATE_UI_LIST")
        {
            if (m_playingSomething)
                m_needUpdate = true;
            else
            {
                UpdateUILists();
                m_helper.ForceFreeSpaceUpdate();
            }
        }
        else if (message.startsWith("UPDATE_UI_ITEM"))
        {
            QStringList tokens = message.simplified().split(" ");
            if (tokens.size() < 3)
                return;

            uint recordingID  = tokens[1].toUInt();
            auto flags = static_cast<ProgramInfoCache::UpdateState>(tokens[2].toUInt());

            if (flags != ProgramInfoCache::PIC_NO_ACTION)
                HandleUpdateItemEvent(recordingID, flags);
        }
        else if (message == "UPDATE_USAGE_UI")
        {
            UpdateUsageUI();
        }
        else if (message == "RECONNECT_SUCCESS")
        {
            m_programInfoCache.ScheduleLoad();
        }
        else if (message == "LOCAL_PBB_DELETE_RECORDINGS")
        {
            QStringList list;
            for (uint i = 0; i+2 < (uint)me->ExtraDataList().size(); i+=3)
            {
                uint recordingID = me->ExtraDataList()[i+0].toUInt();
                ProgramInfo *pginfo =
                                m_programInfoCache.GetRecordingInfo(recordingID);

                if (!pginfo)
                {
                    LOG(VB_GENERAL, LOG_WARNING, LOC +
                                    QString("LOCAL_PBB_DELETE_RECORDINGS - "
                                            "No matching recording %1")
                                                    .arg(recordingID));
                    continue;
                }

                QString forceDeleteStr = me->ExtraDataList()[i+1];
                QString forgetHistoryStr = me->ExtraDataList()[i+2];

                list.push_back(QString::number(pginfo->GetRecordingID()));
                list.push_back(forceDeleteStr);
                list.push_back(forgetHistoryStr);
                pginfo->SetAvailableStatus(asPendingDelete,
                                           "LOCAL_PBB_DELETE_RECORDINGS");

                // if the item is in the current recording list UI
                // then delete it.
                MythUIButtonListItem *uiItem =
                    m_recordingList->GetItemByData(QVariant::fromValue(pginfo));
                if (uiItem)
                    m_recordingList->RemoveItem(uiItem);
            }
            if (!list.empty())
                m_helper.DeleteRecordings(list);
        }
        else if (message == "DELETE_SUCCESSES")
        {
            m_helper.ForceFreeSpaceUpdate();
        }
        else if (message == "DELETE_FAILURES")
        {
            if (me->ExtraDataList().size() < 3)
                return;

            for (uint i = 0; i+2 < (uint)me->ExtraDataList().size(); i += 3)
            {
                ProgramInfo *pginfo = m_programInfoCache.GetRecordingInfo(
                        me->ExtraDataList()[i+0].toUInt());
                if (pginfo)
                {
                    pginfo->SetAvailableStatus(asAvailable, "DELETE_FAILURES");
                    m_helper.CheckAvailability(*pginfo, kCheckForCache);
                }
            }

            bool forceDelete = me->ExtraDataList()[1].toUInt() != 0U;
            if (!forceDelete)
            {
                m_delList = me->ExtraDataList();
                if (!m_menuDialog)
                {
                    ShowDeletePopup(kForceDeleteRecording);
                    return;
                }
                LOG(VB_GENERAL, LOG_WARNING, LOC +
                    "Delete failures not handled due to "
                    "pre-existing popup.");
            }

            // Since we deleted items from the UI after we set
            // asPendingDelete, we need to put them back now..
            ScheduleUpdateUIList();
        }
        else if (message == "PREVIEW_SUCCESS")
        {
            HandlePreviewEvent(me->ExtraDataList());
        }
        else if (message == "PREVIEW_FAILED" && me->ExtraDataCount() >= 5)
        {
            for (uint i = 4; i < (uint) me->ExtraDataCount(); i++)
            {
                const QString& token = me->ExtraData(i);
                QSet<QString>::iterator it = m_previewTokens.find(token);
                if (it != m_previewTokens.end())
                    m_previewTokens.erase(it);
            }
        }
        else if (message == "AVAILABILITY" && me->ExtraDataCount() == 8)
        {
            static constexpr std::chrono::milliseconds kMaxUIWaitTime = 10s;
            QStringList list = me->ExtraDataList();
            uint recordingID = list[0].toUInt();
            auto cat = (CheckAvailabilityType) list[1].toInt();
            auto availableStatus = (AvailableStatusType) list[2].toInt();
            uint64_t fs = list[3].toULongLong();
            QTime tm;
            tm.setHMS(list[4].toUInt(), list[5].toUInt(),
                      list[6].toUInt(), list[7].toUInt());
            QTime now = QTime::currentTime();
            auto time_elapsed = std::chrono::milliseconds(tm.msecsTo(now));
            if (time_elapsed < 0ms)
                time_elapsed += 24h;

            AvailableStatusType old_avail = availableStatus;
            ProgramInfo *pginfo = FindProgramInUILists(recordingID);
            if (pginfo)
            {
                pginfo->SetFilesize(std::max(pginfo->GetFilesize(), fs));
                old_avail = pginfo->GetAvailableStatus();
                pginfo->SetAvailableStatus(availableStatus, "AVAILABILITY");
            }

            if (time_elapsed >= kMaxUIWaitTime)
                m_playListPlay.clear();

            bool playnext = ((kCheckForPlaylistAction == cat) &&
                             !m_playListPlay.empty());


            if (((kCheckForPlayAction     == cat) ||
                 (kCheckForPlaylistAction == cat)) &&
                (time_elapsed < kMaxUIWaitTime))
            {
                if (asAvailable != availableStatus)
                {
                    if (kCheckForPlayAction == cat && pginfo)
                        ShowAvailabilityPopup(*pginfo);
                }
                else if (pginfo)
                {
                    playnext = false;
                    const bool ignoreBookmark = false;
                    const bool ignoreProgStart = false;
                    const bool ignoreLastPlayPos = true;
                    const bool underNetworkControl = false;
                    Play(*pginfo, kCheckForPlaylistAction == cat,
                         ignoreBookmark, ignoreProgStart, ignoreLastPlayPos,
                         underNetworkControl);
                }
            }

            if (playnext)
            {
                // failed to play this item, instead
                // play the next item on the list..
                QCoreApplication::postEvent(
                    this, new MythEvent("PLAY_PLAYLIST"));
            }

            if (old_avail != availableStatus)
                UpdateUIListItem(pginfo, true);
        }
        else if ((message == "PLAY_PLAYLIST") && !m_playListPlay.empty())
        {
            uint recordingID = m_playListPlay.front();
            m_playListPlay.pop_front();

            if (!m_playListPlay.empty())
            {
                const ProgramInfo *pginfo =
                    FindProgramInUILists(m_playListPlay.front());
                if (pginfo)
                    m_helper.CheckAvailability(*pginfo, kCheckForCache);
            }

            ProgramInfo *pginfo = FindProgramInUILists(recordingID);
            const bool ignoreBookmark = false;
            const bool ignoreProgStart = true;
            const bool ignoreLastPlayPos = true;
            const bool underNetworkControl = false;
            if (pginfo)
                Play(*pginfo, true, ignoreBookmark, ignoreProgStart,
                     ignoreLastPlayPos, underNetworkControl);
        }
        else if ((message == "SET_PLAYBACK_URL") && (me->ExtraDataCount() == 2))
        {
            uint recordingID = me->ExtraData(0).toUInt();
            ProgramInfo *info = m_programInfoCache.GetRecordingInfo(recordingID);
            if (info)
                info->SetPathname(me->ExtraData(1));
        }
        else if ((message == "FOUND_ARTWORK") && (me->ExtraDataCount() >= 5))
        {
            auto type = (VideoArtworkType) me->ExtraData(2).toInt();
            uint             recordingID = me->ExtraData(3).toUInt();
            const QString&   group       = me->ExtraData(4);
            const QString&   fn          = me->ExtraData(5);

            if (recordingID)
            {
                ProgramInfo *pginfo = m_programInfoCache.GetRecordingInfo(recordingID);
                if (pginfo &&
                    m_recordingList->GetItemByData(QVariant::fromValue(pginfo)) ==
                    m_recordingList->GetItemCurrent() &&
                    m_artImage[(uint)type]->GetFilename() != fn)
                {
                    m_artImage[(uint)type]->SetFilename(fn);
                    m_artTimer[(uint)type]->start(s_artDelay[(uint)type]);
                }
            }
            else if (!group.isEmpty() &&
                     (m_currentGroup == group) &&
                     m_artImage[type] &&
                     m_groupList->GetItemCurrent() &&
                     m_artImage[(uint)type]->GetFilename() != fn)
            {
                m_artImage[(uint)type]->SetFilename(fn);
                m_artTimer[(uint)type]->start(s_artDelay[(uint)type]);
            }
        }
        else if (message == "EXIT_TO_MENU" ||
                 message == "CANCEL_PLAYLIST")
        {
            m_playListPlay.clear();
        }
    }
    else
    {
        ScheduleCommon::customEvent(event);
    }
}

void PlaybackBox::HandleRecordingRemoveEvent(uint recordingID)
{
    if (!m_programInfoCache.Remove(recordingID))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("Failed to remove %1, reloading list")
                .arg(recordingID));
        m_programInfoCache.ScheduleLoad();
        return;
    }

    MythUIButtonListItem *sel_item = m_groupList->GetItemCurrent();
    QString groupname;
    if (sel_item)
        groupname = sel_item->GetData().toString();

    ProgramMap::iterator git = m_progLists.begin();
    while (git != m_progLists.end())
    {
        auto pit = (*git).begin();
        while (pit != (*git).end())
        {
            if ((*pit)->GetRecordingID() == recordingID)
            {
                if (!git.key().isEmpty() && git.key() == groupname)
                {
                    MythUIButtonListItem *item_by_data =
                        m_recordingList->GetItemByData(
                            QVariant::fromValue(*pit));
                    MythUIButtonListItem *item_cur =
                        m_recordingList->GetItemCurrent();

                    if (item_cur && (item_by_data == item_cur))
                    {
                        MythUIButtonListItem *item_next =
                            m_recordingList->GetItemNext(item_cur);
                        if (item_next)
                            m_recordingList->SetItemCurrent(item_next);
                    }

                    m_recordingList->RemoveItem(item_by_data);
                }
                pit = (*git).erase(pit);
            }
            else
            {
                ++pit;
            }
        }

        if ((*git).empty())
        {
            if (!groupname.isEmpty() && (git.key() == groupname))
            {
                MythUIButtonListItem *next_item =
                    m_groupList->GetItemNext(sel_item);
                if (next_item)
                    m_groupList->SetItemCurrent(next_item);

                m_groupList->RemoveItem(sel_item);

                sel_item = next_item;
                groupname = "";
                if (sel_item)
                    groupname = sel_item->GetData().toString();
            }
            git = m_progLists.erase(git);
        }
        else
        {
            ++git;
        }
    }

    m_helper.ForceFreeSpaceUpdate();
}

void PlaybackBox::HandleRecordingAddEvent(const ProgramInfo &evinfo)
{
    m_programInfoCache.Add(evinfo);
    ScheduleUpdateUIList();
}

void PlaybackBox::HandleUpdateItemEvent(uint recordingID, uint flags)
{
    // Changing recording group full reload
    if (flags & ProgramInfoCache::PIC_RECGROUP_CHANGED)
    {
        ScheduleUpdateUIList();
    }
    else
    {
        ProgramInfo *pginfo = FindProgramInUILists(recordingID);
        if (pginfo == nullptr)
            return;
        bool genPreview = (flags & ProgramInfoCache::PIC_MARK_CHANGED);
        UpdateUIListItem(pginfo, genPreview);
    }
}

void PlaybackBox::ScheduleUpdateUIList(void)
{
    if (!m_programInfoCache.IsLoadInProgress())
        QCoreApplication::postEvent(this, new MythEvent("UPDATE_UI_LIST"));
}

void PlaybackBox::showIconHelp(void)
{
    auto *helpPopup = new HelpPopup(m_popupStack);

    if (helpPopup->Create())
        m_popupStack->AddScreen(helpPopup);
    else
        delete helpPopup;
}

void PlaybackBox::showViewChanger(void)
{
    auto *viewPopup = new ChangeView(m_popupStack, this, m_viewMask);

    if (viewPopup->Create())
    {
        connect(viewPopup, &ChangeView::save, this, &PlaybackBox::saveViewChanges);
        m_popupStack->AddScreen(viewPopup);
    }
    else
    {
        delete viewPopup;
    }
}

void PlaybackBox::saveViewChanges()
{
    if (m_viewMask == VIEW_NONE)
        m_viewMask = VIEW_TITLES;
    gCoreContext->SaveSetting("DisplayGroupDefaultViewMask", (int)m_viewMask);
    gCoreContext->SaveBoolSetting("PlaybackWatchList",
                                            (m_viewMask & VIEW_WATCHLIST) != 0);
}

void PlaybackBox::showGroupFilter(void)
{
    QStringList groupNames;
    QStringList displayNames;
    QStringList groups;
    QStringList displayGroups;

    MSqlQuery query(MSqlQuery::InitCon());

    m_recGroupType.clear();

    uint totalItems = 0;

    // Add the group entries
    displayNames.append(QString("------- %1 -------").arg(tr("Groups")));
    groupNames.append("");

    // Find each recording group, and the number of recordings in each
    query.prepare("SELECT recgroup, COUNT(title) FROM recorded "
                  "WHERE deletepending = 0 AND watched <= :WATCHED "
                  "GROUP BY recgroup");
    query.bindValue(":WATCHED", (m_viewMask & VIEW_WATCHED));
    if (query.exec())
    {
        while (query.next())
        {
            QString dispGroup = query.value(0).toString();
            uint    items     = query.value(1).toInt();

            if ((dispGroup != "LiveTV" || (m_viewMask & VIEW_LIVETVGRP)) &&
                (dispGroup != "Deleted"))
                totalItems += items;

            groupNames.append(dispGroup);

            dispGroup = (dispGroup == "Default") ? tr("Default") : dispGroup;
            dispGroup = (dispGroup == "Deleted") ? tr("Deleted") : dispGroup;
            dispGroup = (dispGroup == "LiveTV")  ? tr("Live TV")  : dispGroup;

            displayNames.append(tr("%1 [%n item(s)]", nullptr, items).arg(dispGroup));

            m_recGroupType[query.value(0).toString()] = "recgroup";
        }
    }

    // Create and add the "All Programs" entry
    displayNames.push_front(tr("%1 [%n item(s)]", nullptr, totalItems)
                            .arg(ProgramInfo::i18n("All Programs")));
    groupNames.push_front("All Programs");
    m_recGroupType["All Programs"] = "recgroup";

    // Find each category, and the number of recordings in each
    query.prepare("SELECT DISTINCT category, COUNT(title) FROM recorded "
                  "WHERE deletepending = 0 AND watched <= :WATCHED "
                  "GROUP BY category");
    query.bindValue(":WATCHED", (m_viewMask & VIEW_WATCHED));
    if (query.exec())
    {
        int unknownCount = 0;
        while (query.next())
        {
            uint    items     = query.value(1).toInt();
            QString dispGroup = query.value(0).toString();
            if (dispGroup.isEmpty())
            {
                unknownCount += items;
                dispGroup = tr("Unknown");
            }
            else if (dispGroup == tr("Unknown"))
            {
                unknownCount += items;
            }

            if ((!m_recGroupType.contains(dispGroup)) &&
                (dispGroup != tr("Unknown")))
            {
                displayGroups += tr("%1 [%n item(s)]", nullptr, items).arg(dispGroup);
                groups += dispGroup;

                m_recGroupType[dispGroup] = "category";
            }
        }

        if (unknownCount > 0)
        {
            QString dispGroup = tr("Unknown");
            uint    items     = unknownCount;
            displayGroups += tr("%1 [%n item(s)]", nullptr, items).arg(dispGroup);
            groups += dispGroup;

            m_recGroupType[dispGroup] = "category";
        }
    }

    // Add the category entries
    displayNames.append(QString("------- %1 -------").arg(tr("Categories")));
    groupNames.append("");
    groups.sort();
    displayGroups.sort();
    QStringList::iterator it;
    for (it = displayGroups.begin(); it != displayGroups.end(); ++it)
        displayNames.append(*it);
    for (it = groups.begin(); it != groups.end(); ++it)
        groupNames.append(*it);

    QString label = tr("Change Filter");

    auto *recGroupPopup = new GroupSelector(m_popupStack, label, displayNames,
                                            groupNames, m_recGroup);

    if (recGroupPopup->Create())
    {
        m_usingGroupSelector = true;
        m_groupSelected = false;
        connect(recGroupPopup, &GroupSelector::result,
                this, &PlaybackBox::displayRecGroup);
        connect(recGroupPopup, &MythScreenType::Exiting,
                this, &PlaybackBox::groupSelectorClosed);
        m_popupStack->AddScreen(recGroupPopup);
    }
    else
    {
        delete recGroupPopup;
    }
}

void PlaybackBox::groupSelectorClosed(void)
{
    if (m_groupSelected)
        return;

    if (m_firstGroup)
        Close();

    m_usingGroupSelector = false;
}

void PlaybackBox::setGroupFilter(const QString &recGroup)
{
    QString newRecGroup = recGroup;

    if (newRecGroup.isEmpty())
        return;

    m_firstGroup = false;
    m_usingGroupSelector = false;

    if (newRecGroup == ProgramInfo::i18n("Default"))
        newRecGroup = "Default";
    else if (newRecGroup == ProgramInfo::i18n("All Programs"))
        newRecGroup = "All Programs";
    else if (newRecGroup == ProgramInfo::i18n("LiveTV"))
        newRecGroup = "LiveTV";
    else if (newRecGroup == ProgramInfo::i18n("Deleted"))
        newRecGroup = "Deleted";

    m_recGroup = newRecGroup;

    m_groupDisplayName = ProgramInfo::i18n(m_recGroup);

    // Since the group filter is changing, the current position in the lists
    // is meaningless -- so reset the lists so the position won't be saved.
    m_recordingList->Reset();
    m_groupList->Reset();

    UpdateUILists();

    if (gCoreContext->GetBoolSetting("RememberRecGroup",true))
        gCoreContext->SaveSetting("DisplayRecGroup", m_recGroup);

    if (m_recGroupType[m_recGroup] == "recgroup")
        gCoreContext->SaveSetting("DisplayRecGroupIsCategory", 0);
    else
        gCoreContext->SaveSetting("DisplayRecGroupIsCategory", 1);
}

QString PlaybackBox::getRecGroupPassword(const QString &group)
{
    return m_recGroupPwCache.value(group);
}

void PlaybackBox::fillRecGroupPasswordCache(void)
{
    m_recGroupPwCache.clear();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT recgroup, password FROM recgroups "
                  "WHERE password IS NOT NULL AND password <> '';");

    if (query.exec())
    {
        while (query.next())
        {
            QString recgroup = query.value(0).toString();

            if (recgroup == ProgramInfo::i18n("Default"))
                recgroup = "Default";
            else if (recgroup == ProgramInfo::i18n("All Programs"))
                recgroup = "All Programs";
            else if (recgroup == ProgramInfo::i18n("LiveTV"))
                recgroup = "LiveTV";
            else if (recgroup == ProgramInfo::i18n("Deleted"))
                recgroup = "Deleted";

            m_recGroupPwCache.insert(recgroup, query.value(1).toString());
        }
    }
}

/// \brief Used to change the recording group of a program or playlist.
void PlaybackBox::ShowRecGroupChanger(bool use_playlist)
{
    m_opOnPlaylist = use_playlist;

    ProgramInfo *pginfo = nullptr;
    if (use_playlist)
    {
        if (!m_playList.empty())
            pginfo = FindProgramInUILists(m_playList[0]);
    }
    else
    {
        pginfo = GetCurrentProgram();
    }

    if (!pginfo)
        return;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT g.recgroup, COUNT(r.title) FROM recgroups g "
        "LEFT JOIN recorded r ON g.recgroupid=r.recgroupid AND r.deletepending = 0 "
        "WHERE g.recgroupid != 2 AND g.recgroupid != 3 "
        "GROUP BY g.recgroupid ORDER BY g.recgroup");

    QStringList displayNames(tr("Add New"));
    QStringList groupNames("addnewgroup");

    if (!query.exec())
        return;

    while (query.next())
    {
        QString dispGroup = query.value(0).toString();
        groupNames.push_back(dispGroup);

        if (dispGroup == "Default")
            dispGroup = tr("Default");
        else if (dispGroup == "LiveTV")
            dispGroup = tr("Live TV");
        else if (dispGroup == "Deleted")
            dispGroup = tr("Deleted");

        displayNames.push_back(tr("%1 [%n item(s)]", "", query.value(1).toInt())
                               .arg(dispGroup));
    }

    QString label = tr("Select Recording Group") +
        CreateProgramInfoString(*pginfo);

    auto *rgChanger = new GroupSelector(m_popupStack, label, displayNames,
                                        groupNames, pginfo->GetRecordingGroup());

    if (rgChanger->Create())
    {
        connect(rgChanger, &GroupSelector::result, this, &PlaybackBox::setRecGroup);
        m_popupStack->AddScreen(rgChanger);
    }
    else
    {
        delete rgChanger;
    }
}

/// \brief Used to change the play group of a program or playlist.
void PlaybackBox::ShowPlayGroupChanger(bool use_playlist)
{
    m_opOnPlaylist = use_playlist;

    ProgramInfo *pginfo = nullptr;
    if (use_playlist)
    {
        if (!m_playList.empty())
            pginfo = FindProgramInUILists(m_playList[0]);
    }
    else
    {
        pginfo = GetCurrentProgram();
    }

    if (!pginfo)
        return;

    QStringList groupNames(tr("Default"));
    QStringList displayNames("Default");

    QStringList list = PlayGroup::GetNames();
    for (const auto& name : std::as_const(list))
    {
        displayNames.push_back(name);
        groupNames.push_back(name);
    }

    QString label = tr("Select Playback Group") +
        CreateProgramInfoString(*pginfo);

    auto *pgChanger = new GroupSelector(m_popupStack, label,displayNames,
                                        groupNames, pginfo->GetPlaybackGroup());

    if (pgChanger->Create())
    {
        connect(pgChanger, &GroupSelector::result,
                this, &PlaybackBox::setPlayGroup);
        m_popupStack->AddScreen(pgChanger);
    }
    else
    {
        delete pgChanger;
    }
}

void PlaybackBox::doPlaylistExpireSetting(bool turnOn)
{
    QList<uint>::Iterator it;

    for (it = m_playList.begin(); it != m_playList.end(); ++it)
    {
        ProgramInfo *tmpItem = FindProgramInUILists(*it);
        if (tmpItem != nullptr)
        {
            if (!tmpItem->IsAutoExpirable() && turnOn)
                tmpItem->SaveAutoExpire(kNormalAutoExpire, true);
            else if (tmpItem->IsAutoExpirable() && !turnOn)
                tmpItem->SaveAutoExpire(kDisableAutoExpire, true);
        }
    }
}

void PlaybackBox::doPlaylistWatchedSetting(bool turnOn)
{
    QList<uint>::Iterator it;

    for (it = m_playList.begin(); it != m_playList.end(); ++it)
    {
        ProgramInfo *tmpItem = FindProgramInUILists(*it);
        if (tmpItem != nullptr)
        {
            tmpItem->SaveWatched(turnOn);
        }
    }

    doClearPlaylist();
    UpdateUILists();
}

void PlaybackBox::showMetadataEditor()
{
    ProgramInfo *pgInfo = GetCurrentProgram();

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *editMetadata = new RecMetadataEdit(mainStack, pgInfo);

    if (editMetadata->Create())
    {
        connect(editMetadata, &RecMetadataEdit::result,
                this, &PlaybackBox::saveRecMetadata);
        mainStack->AddScreen(editMetadata);
    }
    else
    {
        delete editMetadata;
    }
}

void PlaybackBox::saveRecMetadata(const QString &newTitle,
                                  const QString &newSubtitle,
                                  const QString &newDescription,
                                  const QString &newInetref,
                                  uint newSeason,
                                  uint newEpisode)
{
    MythUIButtonListItem *item = m_recordingList->GetItemCurrent();

    if (!item)
        return;

    auto *pginfo = item->GetData().value<ProgramInfo *>();

    if (!pginfo)
        return;

    QString groupname = m_groupList->GetItemCurrent()->GetData().toString();

    if (groupname == pginfo->GetTitle().toLower() &&
        newTitle != pginfo->GetTitle())
    {
        m_recordingList->RemoveItem(item);
    }
    else
    {
        QString tempSubTitle = newTitle;
        if (!newSubtitle.trimmed().isEmpty())
            tempSubTitle = QString("%1 - \"%2\"")
                            .arg(tempSubTitle, newSubtitle);

        QString seasone;
        QString seasonx;
        QString season;
        QString episode;
        if (newSeason > 0 || newEpisode > 0)
        {
            season  = StringUtil::intToPaddedString(newSeason,  1);
            episode = StringUtil::intToPaddedString(newEpisode, 1);
            seasone = QString("s%1e%2")
                            .arg(StringUtil::intToPaddedString(newSeason,  2),
                                 StringUtil::intToPaddedString(newEpisode, 2));
            seasonx = QString("%1x%2")
                            .arg(StringUtil::intToPaddedString(newSeason,  1),
                                 StringUtil::intToPaddedString(newEpisode, 2));
        }

        item->SetText(tempSubTitle, "titlesubtitle");
        item->SetText(newTitle, "title");
        item->SetText(newSubtitle, "subtitle");
        item->SetText(newInetref, "inetref");
        item->SetText(seasonx, "00x00");
        item->SetText(seasone, "s00e00");
        item->SetText(season, "season");
        item->SetText(episode, "episode");
        if (newDescription != nullptr)
            item->SetText(newDescription, "description");
    }

    pginfo->SaveInetRef(newInetref);
    pginfo->SaveSeasonEpisode(newSeason, newEpisode);

    RecordingInfo ri(*pginfo);
    ri.ApplyRecordRecTitleChange(newTitle, newSubtitle, newDescription);
    *pginfo = ri;
}

void PlaybackBox::setRecGroup(QString newRecGroup)
{
    newRecGroup = newRecGroup.simplified();

    if (newRecGroup.isEmpty())
        return;

    if (newRecGroup == "addnewgroup")
    {
        MythScreenStack *popupStack =
            GetMythMainWindow()->GetStack("popup stack");

        auto *newgroup = new MythTextInputDialog(popupStack,
                                                 tr("New Recording Group"));

        connect(newgroup, &MythTextInputDialog::haveResult,
                this, &PlaybackBox::setRecGroup);

        if (newgroup->Create())
            popupStack->AddScreen(newgroup, false);
        else
            delete newgroup;
        return;
    }

    RecordingRule record;
    record.LoadTemplate("Default");
    AutoExpireType defaultAutoExpire =
        record.m_autoExpire ? kNormalAutoExpire : kDisableAutoExpire;

    if (m_opOnPlaylist)
    {
        for (int id : std::as_const(m_playList))
        {
            ProgramInfo *p = FindProgramInUILists(id);
            if (!p)
                continue;

            if ((p->GetRecordingGroup() == "LiveTV") &&
                (newRecGroup != "LiveTV"))
            {
                p->SaveAutoExpire(defaultAutoExpire);
            }
            else if ((p->GetRecordingGroup() != "LiveTV") &&
                     (newRecGroup == "LiveTV"))
            {
                p->SaveAutoExpire(kLiveTVAutoExpire);
            }

            RecordingInfo ri(*p);
            ri.ApplyRecordRecGroupChange(newRecGroup);
            *p = ri;
        }
        doClearPlaylist();
        UpdateUILists();
        return;
    }

    ProgramInfo *p = GetCurrentProgram();
    if (!p)
        return;

    if ((p->GetRecordingGroup() == "LiveTV") && (newRecGroup != "LiveTV"))
        p->SaveAutoExpire(defaultAutoExpire);
    else if ((p->GetRecordingGroup() != "LiveTV") && (newRecGroup == "LiveTV"))
        p->SaveAutoExpire(kLiveTVAutoExpire);

    RecordingInfo ri(*p);
    ri.ApplyRecordRecGroupChange(newRecGroup);
    *p = ri;
    UpdateUILists();
}

void PlaybackBox::setPlayGroup(QString newPlayGroup)
{
    ProgramInfo *tmpItem = GetCurrentProgram();

    if (newPlayGroup.isEmpty() || !tmpItem)
        return;

    if (newPlayGroup == tr("Default"))
        newPlayGroup = "Default";

    if (m_opOnPlaylist)
    {
        QList<uint>::Iterator it;

        for (it = m_playList.begin(); it != m_playList.end(); ++it )
        {
            tmpItem = FindProgramInUILists(*it);
            if (tmpItem)
            {
                RecordingInfo ri(*tmpItem);
                ri.ApplyRecordPlayGroupChange(newPlayGroup);
                *tmpItem = ri;
            }
        }
        doClearPlaylist();
    }
    else
    {
        RecordingInfo ri(*tmpItem);
        ri.ApplyRecordPlayGroupChange(newPlayGroup);
        *tmpItem = ri;
    }
}

void PlaybackBox::showRecGroupPasswordChanger(void)
{
    MythUIButtonListItem *item = m_groupList->GetItemCurrent();

    if (!item)
        return;

    QString currentPassword = getRecGroupPassword(m_recGroup);

    auto *pwChanger = new PasswordChange(m_popupStack, currentPassword);

    if (pwChanger->Create())
    {
        connect(pwChanger, &PasswordChange::result,
                this, &PlaybackBox::SetRecGroupPassword);
        m_popupStack->AddScreen(pwChanger);
    }
    else
    {
        delete pwChanger;
    }
}

void PlaybackBox::SetRecGroupPassword(const QString &newPassword)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("UPDATE recgroups SET password = :PASSWD WHERE "
                  "recgroup = :RECGROUP");
    query.bindValue(":RECGROUP", m_recGroup);
    query.bindValue(":PASSWD", newPassword);

    if (!query.exec())
        MythDB::DBError("PlaybackBox::SetRecGroupPassword",
                        query);

    if (newPassword.isEmpty())
        m_recGroupPwCache.remove(m_recGroup);
    else
        m_recGroupPwCache.insert(m_recGroup, newPassword);
}

///////////////////////////////////////////////////

bool GroupSelector::Create()
{
    if (!LoadWindowFromXML("recordings-ui.xml", "groupselector", this))
        return false;

    MythUIText *labelText = dynamic_cast<MythUIText*> (GetChild("label"));
    MythUIButtonList *groupList = dynamic_cast<MythUIButtonList*>
                                        (GetChild("groups"));

    if (!groupList)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Theme is missing 'groups' button list.");
        return false;
    }

    if (labelText)
        labelText->SetText(m_label);

    for (int i = 0; i < m_list.size(); ++i)
    {
        new MythUIButtonListItem(groupList, m_list.at(i),
                                 QVariant::fromValue(m_data.at(i)));
    }

    // Set the current position in the list
    groupList->SetValueByData(QVariant::fromValue(m_selected));

    BuildFocusList();

    connect(groupList, &MythUIButtonList::itemClicked,
            this, &GroupSelector::AcceptItem);

    return true;
}

void GroupSelector::AcceptItem(MythUIButtonListItem *item)
{
    if (!item)
        return;

    // ignore the dividers
    if (item->GetData().toString().isEmpty())
        return;

    QString group = item->GetData().toString();
    emit result(group);
    Close();
}

////////////////////////////////////////////////

bool ChangeView::Create()
{
    if (!LoadWindowFromXML("recordings-ui.xml", "changeview", this))
        return false;

    MythUICheckBox *checkBox = dynamic_cast<MythUICheckBox*>(GetChild("titles"));
    if (checkBox)
    {
        if (m_viewMask & PlaybackBox::VIEW_TITLES)
            checkBox->SetCheckState(MythUIStateType::Full);
        connect(checkBox, &MythUICheckBox::toggled,
                m_parentScreen, &PlaybackBox::toggleTitleView);
    }

    checkBox = dynamic_cast<MythUICheckBox*>(GetChild("categories"));
    if (checkBox)
    {
        if (m_viewMask & PlaybackBox::VIEW_CATEGORIES)
            checkBox->SetCheckState(MythUIStateType::Full);
        connect(checkBox, &MythUICheckBox::toggled,
                m_parentScreen, &PlaybackBox::toggleCategoryView);
    }

    checkBox = dynamic_cast<MythUICheckBox*>(GetChild("recgroups"));
    if (checkBox)
    {
        if (m_viewMask & PlaybackBox::VIEW_RECGROUPS)
            checkBox->SetCheckState(MythUIStateType::Full);
        connect(checkBox, &MythUICheckBox::toggled,
                m_parentScreen, &PlaybackBox::toggleRecGroupView);
    }

    // TODO Do we need two separate settings to determine whether the watchlist
    //      is shown? The filter setting be enough?
        checkBox = dynamic_cast<MythUICheckBox*>(GetChild("watchlist"));
        if (checkBox)
        {
            if (m_viewMask & PlaybackBox::VIEW_WATCHLIST)
                checkBox->SetCheckState(MythUIStateType::Full);
            connect(checkBox, &MythUICheckBox::toggled,
                    m_parentScreen, &PlaybackBox::toggleWatchListView);
        }
    //

    checkBox = dynamic_cast<MythUICheckBox*>(GetChild("searches"));
    if (checkBox)
    {
        if (m_viewMask & PlaybackBox::VIEW_SEARCHES)
            checkBox->SetCheckState(MythUIStateType::Full);
        connect(checkBox, &MythUICheckBox::toggled,
                m_parentScreen, &PlaybackBox::toggleSearchView);
    }

    // TODO Do we need two separate settings to determine whether livetv
    //      recordings are shown? Same issue as the watchlist above
        checkBox = dynamic_cast<MythUICheckBox*>(GetChild("livetv"));
        if (checkBox)
        {
            if (m_viewMask & PlaybackBox::VIEW_LIVETVGRP)
                checkBox->SetCheckState(MythUIStateType::Full);
            connect(checkBox, &MythUICheckBox::toggled,
                    m_parentScreen, &PlaybackBox::toggleLiveTVView);
        }
    //

    checkBox = dynamic_cast<MythUICheckBox*>(GetChild("watched"));
    if (checkBox)
    {
        if (m_viewMask & PlaybackBox::VIEW_WATCHED)
            checkBox->SetCheckState(MythUIStateType::Full);
        connect(checkBox, &MythUICheckBox::toggled,
                m_parentScreen, &PlaybackBox::toggleWatchedView);
    }

    MythUIButton *savebutton = dynamic_cast<MythUIButton*>(GetChild("save"));
    connect(savebutton, &MythUIButton::Clicked, this, &ChangeView::SaveChanges);

    BuildFocusList();

    return true;
}

void ChangeView::SaveChanges()
{
    emit save();
    Close();
}

////////////////////////////////////////////////

bool PasswordChange::Create()
{
    if (!LoadWindowFromXML("recordings-ui.xml", "passwordchanger", this))
        return false;

    m_oldPasswordEdit = dynamic_cast<MythUITextEdit *>(GetChild("oldpassword"));
    m_newPasswordEdit = dynamic_cast<MythUITextEdit *>(GetChild("newpassword"));
    m_okButton = dynamic_cast<MythUIButton *>(GetChild("ok"));

    if (!m_oldPasswordEdit || !m_newPasswordEdit || !m_okButton)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Window 'passwordchanger' is missing required elements.");
        return false;
    }

    m_oldPasswordEdit->SetPassword(true);
    m_oldPasswordEdit->SetMaxLength(10);
//     if (m_oldPassword.isEmpty())
//         m_oldPasswordEdit->SetDisabled(true);
    m_newPasswordEdit->SetPassword(true);
    m_newPasswordEdit->SetMaxLength(10);

    BuildFocusList();

    connect(m_oldPasswordEdit, &MythUITextEdit::valueChanged,
                               this, &PasswordChange::OldPasswordChanged);
    connect(m_okButton, &MythUIButton::Clicked, this, &PasswordChange::SendResult);

    return true;
}

void PasswordChange::OldPasswordChanged()
{
    QString newText = m_oldPasswordEdit->GetText();
    bool ok = (newText == m_oldPassword);
    m_okButton->SetEnabled(ok);
}


void PasswordChange::SendResult()
{
    emit result(m_newPasswordEdit->GetText());
    Close();
}

////////////////////////////////////////////////////////

RecMetadataEdit::RecMetadataEdit(MythScreenStack *lparent, ProgramInfo *pginfo)
  : MythScreenType(lparent, "recmetadataedit"),
    m_progInfo(pginfo),
    m_metadataFactory(new MetadataFactory(this))
{
    m_popupStack = GetMythMainWindow()->GetStack("popup stack");
}

bool RecMetadataEdit::Create()
{
    if (!LoadWindowFromXML("recordings-ui.xml", "editmetadata", this))
        return false;

    m_titleEdit = dynamic_cast<MythUITextEdit*>(GetChild("title"));
    m_subtitleEdit = dynamic_cast<MythUITextEdit*>(GetChild("subtitle"));
    m_descriptionEdit = dynamic_cast<MythUITextEdit*>(GetChild("description"));
    m_inetrefEdit = dynamic_cast<MythUITextEdit*>(GetChild("inetref"));
    MythUIButton *inetrefClear = dynamic_cast<MythUIButton*>
                                 (GetChild("inetref_clear"));
    m_seasonSpin = dynamic_cast<MythUISpinBox*>(GetChild("season"));
    m_episodeSpin = dynamic_cast<MythUISpinBox*>(GetChild("episode"));
    MythUIButton *okButton = dynamic_cast<MythUIButton*>(GetChild("ok"));
    m_queryButton = dynamic_cast<MythUIButton*>(GetChild("query_button"));

    if (!m_titleEdit || !m_subtitleEdit || !m_inetrefEdit || !m_seasonSpin ||
        !m_episodeSpin || !okButton)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Window 'editmetadata' is missing required elements.");
        return false;
    }

    m_titleEdit->SetText(m_progInfo->GetTitle());
    m_titleEdit->SetMaxLength(128);
    m_subtitleEdit->SetText(m_progInfo->GetSubtitle());
    m_subtitleEdit->SetMaxLength(128);
    if (m_descriptionEdit)
    {
        m_descriptionEdit->SetText(m_progInfo->GetDescription());
        m_descriptionEdit->SetMaxLength(255);
    }
    m_inetrefEdit->SetText(m_progInfo->GetInetRef());
    m_inetrefEdit->SetMaxLength(255);
    m_seasonSpin->SetRange(0,9999,1,5);
    m_seasonSpin->SetValue(m_progInfo->GetSeason());
    m_episodeSpin->SetRange(0,9999,1,10);
    m_episodeSpin->SetValue(m_progInfo->GetEpisode());

    connect(inetrefClear, &MythUIButton::Clicked, this, &RecMetadataEdit::ClearInetref);
    connect(okButton, &MythUIButton::Clicked, this, &RecMetadataEdit::SaveChanges);
    if (m_queryButton)
    {
        connect(m_queryButton, &MythUIButton::Clicked, this, &RecMetadataEdit::PerformQuery);
    }

    BuildFocusList();

    return true;
}

void RecMetadataEdit::ClearInetref()
{
    m_inetrefEdit->SetText("");
}

void RecMetadataEdit::SaveChanges()
{
    QString newRecTitle = m_titleEdit->GetText();
    QString newRecSubtitle = m_subtitleEdit->GetText();
    QString newRecDescription = nullptr;
    QString newRecInetref = nullptr;
    uint newRecSeason = 0;
    uint newRecEpisode = 0;
    if (m_descriptionEdit)
        newRecDescription = m_descriptionEdit->GetText();
    newRecInetref = m_inetrefEdit->GetText();
    newRecSeason = m_seasonSpin->GetIntValue();
    newRecEpisode = m_episodeSpin->GetIntValue();

    if (newRecTitle.isEmpty())
        return;

    emit result(newRecTitle, newRecSubtitle, newRecDescription,
                newRecInetref, newRecSeason, newRecEpisode);
    Close();
}

void RecMetadataEdit::PerformQuery()
{
    if (m_busyPopup)
        return;

    m_busyPopup = new MythUIBusyDialog(tr("Trying to manually find this "
                                          "recording online..."),
                                       m_popupStack,
                                       "metaoptsdialog");

    if (m_busyPopup->Create())
        m_popupStack->AddScreen(m_busyPopup);

    auto *lookup = new MetadataLookup();
    lookup->SetStep(kLookupSearch);
    lookup->SetType(kMetadataRecording);
    LookupType type = GuessLookupType(m_inetrefEdit->GetText());

    if (type == kUnknownVideo)
    {
        if (m_seasonSpin->GetIntValue() == 0 &&
            m_episodeSpin->GetIntValue() == 0 &&
            m_subtitleEdit->GetText().isEmpty())
        {
            lookup->SetSubtype(kProbableMovie);
        }
        else
        {
            lookup->SetSubtype(kProbableTelevision);
        }
    }
    else
    {
        // we could determine the type from the inetref
        lookup->SetSubtype(type);
    }
    lookup->SetAllowGeneric(true);
    lookup->SetHandleImages(false);
    lookup->SetHost(gCoreContext->GetMasterHostName());
    lookup->SetTitle(m_titleEdit->GetText());
    lookup->SetSubtitle(m_subtitleEdit->GetText());
    lookup->SetInetref(m_inetrefEdit->GetText());
    lookup->SetCollectionref(m_inetrefEdit->GetText());
    lookup->SetSeason(m_seasonSpin->GetIntValue());
    lookup->SetEpisode(m_episodeSpin->GetIntValue());
    lookup->SetAutomatic(false);

    m_metadataFactory->Lookup(lookup);
}

void RecMetadataEdit::QueryComplete(MetadataLookup *lookup)
{
    if (!lookup)
        return;

    m_inetrefEdit->SetText(lookup->GetInetref());
    m_seasonSpin->SetValue(lookup->GetSeason());
    m_episodeSpin->SetValue(lookup->GetEpisode());
    if (!lookup->GetSubtitle().isEmpty())
    {
        m_subtitleEdit->SetText(lookup->GetSubtitle());
    }
    if (!lookup->GetDescription().isEmpty())
    {
        m_descriptionEdit->SetText(lookup->GetDescription());
    }
}

void RecMetadataEdit::OnSearchListSelection(const RefCountHandler<MetadataLookup>& lookup)
{
    QueryComplete(lookup);
}

void RecMetadataEdit::customEvent(QEvent *levent)
{
    if (levent->type() == MetadataFactoryMultiResult::kEventType)
    {
        if (m_busyPopup)
        {
            m_busyPopup->Close();
            m_busyPopup = nullptr;
        }

        auto *mfmr = dynamic_cast<MetadataFactoryMultiResult*>(levent);

        if (!mfmr)
            return;

        MetadataLookupList list = mfmr->m_results;

        auto *resultsdialog = new MetadataResultsDialog(m_popupStack, list);

        connect(resultsdialog, &MetadataResultsDialog::haveResult,
                this, &RecMetadataEdit::OnSearchListSelection,
                Qt::QueuedConnection);

        if (resultsdialog->Create())
            m_popupStack->AddScreen(resultsdialog);
    }
    else if (levent->type() == MetadataFactorySingleResult::kEventType)
    {
        if (m_busyPopup)
        {
            m_busyPopup->Close();
            m_busyPopup = nullptr;
        }

        auto *mfsr = dynamic_cast<MetadataFactorySingleResult*>(levent);

        if (!mfsr || !mfsr->m_result)
            return;

        QueryComplete(mfsr->m_result);
    }
    else if (levent->type() == MetadataFactoryNoResult::kEventType)
    {
        if (m_busyPopup)
        {
            m_busyPopup->Close();
            m_busyPopup = nullptr;
        }

        auto *mfnr = dynamic_cast<MetadataFactoryNoResult*>(levent);

        if (!mfnr)
            return;

        QString title = tr("No match found for this recording. You can "
                           "try entering a TVDB/TMDB number, season, and "
                           "episode manually.");

        auto *okPopup = new MythConfirmationDialog(m_popupStack, title, false);

        if (okPopup->Create())
            m_popupStack->AddScreen(okPopup);
    }
}

//////////////////////////////////////////

bool HelpPopup::Create()
{
    if (!LoadWindowFromXML("recordings-ui.xml", "iconhelp", this))
        return false;

    m_iconList = dynamic_cast<MythUIButtonList*>(GetChild("iconlist"));

    if (!m_iconList)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Window 'iconhelp' is missing required elements.");
        return false;
    }

    BuildFocusList();

    addItem("watched",     tr("Recording has been watched"));
    addItem("commflagged", tr("Commercials are flagged"));
    addItem("cutlist",     tr("An editing cutlist is present"));
    addItem("autoexpire",  tr("The program is able to auto-expire"));
    addItem("processing",  tr("Commercials are being flagged"));
    addItem("bookmark",    tr("A bookmark is set"));
#if 0
    addItem("inuse",       tr("Recording is in use"));
    addItem("transcoded",  tr("Recording has been transcoded"));
#endif

    addItem("mono",        tr("Recording is in Mono"));
    addItem("stereo",      tr("Recording is in Stereo"));
    addItem("surround",    tr("Recording is in Surround Sound"));
    addItem("dolby",       tr("Recording is in Dolby Surround Sound"));

    addItem("cc",          tr("Recording is Closed Captioned"));
    addItem("subtitles",   tr("Recording has Subtitles Available"));
    addItem("onscreensub", tr("Recording is Subtitled"));

    addItem("SD",          tr("Recording is in Standard Definition"));
    addItem("widescreen",  tr("Recording is Widescreen"));
    addItem("hdtv",        tr("Recording is in High Definition"));
    addItem("hd720",       tr("Recording is in 720p High Definition"));
    addItem("hd1080i",     tr("Recording is in 1080i High Definition"));
    addItem("hd1080p",     tr("Recording is in 1080p High Definition"));
    addItem("uhd4Ki",      tr("Recording is in 4k(interlaced) UHD resolution"));
    addItem("uhd4Kp",      tr("Recording is in 4k UHD resolution"));
    addItem("mpeg2",       tr("Recording is using MPEG-2 codec"));
    addItem("avchd",       tr("Recording is using AVC/H.264 codec"));
    addItem("hevc",        tr("Recording is using HEVC/H.265 codec"));
//    addItem("preserved",   tr("Recording is preserved"));

    return true;
}

void HelpPopup::addItem(const QString &state, const QString &text)
{
    auto *item = new MythUIButtonListItem(m_iconList, text);
    item->DisplayState(state, "icons");
}

void PlaybackBox::PbbJobQueue::Update()
{
    QDateTime now = QDateTime::currentDateTime();
    if (!m_lastUpdated.isValid() ||
        m_lastUpdated.msecsTo(now) >= kInvalidateTimeMs.count())
    {
        QMap<int, JobQueueEntry> jobs;
        JobQueue::GetJobsInQueue(jobs, JOB_LIST_ALL);
        m_jobs.clear();
        for (const auto& job : std::as_const(jobs))
        {
            m_jobs.insert(qMakePair(job.chanid, job.recstartts), job);
        }
        m_lastUpdated = now;
    }
}

bool PlaybackBox::PbbJobQueue::IsJobQueued(int jobType, uint chanid,
                                           const QDateTime &recstartts)
{
    Update();
    QList<JobQueueEntry> values = m_jobs.values(qMakePair(chanid, recstartts));
    auto end = values.cend();
    for (auto iter = values.cbegin(); iter != end; ++iter)
    {
        if (iter->type == jobType)
            return JobQueue::IsJobStatusQueued(iter->status);
    }
    return false;
}

bool PlaybackBox::PbbJobQueue::IsJobRunning(int jobType, uint chanid,
                                            const QDateTime &recstartts)
{
    Update();
    QList<JobQueueEntry> values = m_jobs.values(qMakePair(chanid, recstartts));
    auto end = values.cend();
    for (auto iter = values.cbegin(); iter != end; ++iter)
    {
        if (iter->type == jobType)
            return JobQueue::IsJobStatusRunning(iter->status);
    }
    return false;
}

bool PlaybackBox::PbbJobQueue::IsJobQueuedOrRunning(int jobType, uint chanid,
                                                    const QDateTime &recstartts)
{
    return IsJobQueued(jobType, chanid, recstartts) ||
        IsJobRunning(jobType, chanid, recstartts);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
