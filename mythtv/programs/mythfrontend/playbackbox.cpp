
#include "playbackbox.h"

// QT
#include <QCoreApplication>
#include <QWaitCondition>
#include <QDateTime>
#include <QLocale>
#include <QTimer>
#include <QMap>

// MythTV
#include "previewgeneratorqueue.h"
#include "mythuiprogressbar.h"
#include "mythuibuttonlist.h"
#include "mythcorecontext.h"
#include "mythsystemevent.h"
#include "mythuistatetype.h"
#include "mythuicheckbox.h"
#include "mythuitextedit.h"
#include "mythuispinbox.h"
#include "mythdialogbox.h"
#include "recordinginfo.h"
#include "recordingrule.h"
#include "mythuihelper.h"
#include "mythuinotificationcenter.h"
#include "storagegroup.h"
#include "mythuibutton.h"
#include "mythlogging.h"
#include "mythuiimage.h"
#include "programinfo.h"
#include "mythplayer.h"
#include "mythuitext.h"
#include "remoteutil.h"
#include "mythdbcon.h"
#include "playgroup.h"
#include "mythdirs.h"
#include "mythdb.h"
#include "mythdate.h"
#include "tv.h"

//  Mythfrontend
#include "playbackboxlistitem.h"
#include "customedit.h"
#include "proglist.h"

#define LOC      QString("PlaybackBox: ")
#define LOC_WARN QString("PlaybackBox Warning: ")
#define LOC_ERR  QString("PlaybackBox Error: ")

static const QString _Location = "Playback Box";

static int comp_programid(const ProgramInfo *a, const ProgramInfo *b)
{
    if (a->GetProgramID() == b->GetProgramID())
        return (a->GetRecordingStartTime() <
                b->GetRecordingStartTime() ? 1 : -1);
    else
        return (a->GetProgramID() < b->GetProgramID() ? 1 : -1);
}

static int comp_programid_rev(const ProgramInfo *a, const ProgramInfo *b)
{
    if (a->GetProgramID() == b->GetProgramID())
        return (a->GetRecordingStartTime() >
                b->GetRecordingStartTime() ? 1 : -1);
    else
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
    else
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
    else
        return (dt1 > dt2 ? 1 : -1);
}

static int comp_recpriority2(const ProgramInfo *a, const ProgramInfo *b)
{
    if (a->GetRecordingPriority2() == b->GetRecordingPriority2())
        return (a->GetRecordingStartTime() <
                b->GetRecordingStartTime() ? 1 : -1);
    else
        return (a->GetRecordingPriority2() <
                b->GetRecordingPriority2() ? 1 : -1);
}

static int comp_recordDate(const ProgramInfo *a, const ProgramInfo *b)
{
    if (a->GetScheduledStartTime().date() == b->GetScheduledStartTime().date())
        return (a->GetRecordingStartTime() <
                b->GetRecordingStartTime() ? 1 : -1);
    else
        return (a->GetScheduledStartTime().date() <
                b->GetScheduledStartTime().date() ? 1 : -1);
}

static int comp_recordDate_rev(const ProgramInfo *a, const ProgramInfo *b)
{
    if (a->GetScheduledStartTime().date() == b->GetScheduledStartTime().date())
        return (a->GetRecordingStartTime() >
                b->GetRecordingStartTime() ? 1 : -1);
    else
        return (a->GetScheduledStartTime().date() >
                b->GetScheduledStartTime().date() ? 1 : -1);
}

static int comp_season(const ProgramInfo *a, const ProgramInfo *b)
{
    if (a->GetSeason() == b->GetSeason())
        return (a->GetEpisode() <
                b->GetEpisode() ? 1 : -1);
    else
        return (a->GetSeason() <
                b->GetSeason() ? 1 : -1);
}

static int comp_season_rev(const ProgramInfo *a, const ProgramInfo *b)
{
    if (a->GetSeason() == b->GetSeason())
        return (a->GetEpisode() >
                b->GetEpisode() ? 1 : -1);
    else
        return (a->GetSeason() >
                b->GetSeason() ? 1 : -1);
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

static const uint s_artDelay[] =
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
    QString title, PlaybackBox::ViewMask viewmask,
    PlaybackBox::ViewTitleSort titleSort, int recpriority,
    const QRegExp &prefixes)
{
    if (title.isEmpty())
        return title;

    QString sTitle = title;

    sTitle.remove(prefixes);
    if (viewmask == PlaybackBox::VIEW_TITLES &&
            titleSort == PlaybackBox::TitleSortRecPriority)
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
            sortprefix.sprintf("+%03u", 1000 - recpriority);
        else
            sortprefix.sprintf("-%03u", -recpriority);

        sTitle = sortprefix + '-' + sTitle;
    }
    return sTitle;
}

static QString extract_main_state(const ProgramInfo &pginfo, const TV *player)
{
    QString state("normal");
    if (pginfo.GetRecordingStatus() == rsRecording)
        state = "running";

    if (((pginfo.GetRecordingStatus() != rsRecording) &&
         (pginfo.GetAvailableStatus() != asAvailable) &&
         (pginfo.GetAvailableStatus() != asNotYetAvailable)) ||
        (player && player->IsSameProgram(0, &pginfo)))
    {
        state = "disabled";
    }

    if (state == "normal" && (pginfo.GetVideoProperties() & VID_DAMAGED))
        state = "warning";

    return state;
}

static QString extract_job_state(const ProgramInfo &pginfo)
{
    QString job = "default";

    if (pginfo.GetRecordingStatus() == rsRecording)
        job = "recording";
    else if (JobQueue::IsJobQueuedOrRunning(
                 JOB_TRANSCODE, pginfo.GetChanID(),
                 pginfo.GetRecordingStartTime()))
        job = "transcoding";
    else if (JobQueue::IsJobQueuedOrRunning(
                 JOB_COMMFLAG,  pginfo.GetChanID(),
                 pginfo.GetRecordingStartTime()))
        job = "commflagging";

    return job;
}

static QString extract_commflag_state(const ProgramInfo &pginfo)
{
    QString job = "default";

    // commflagged can be yes, no or processing
    if (JobQueue::IsJobRunning(JOB_COMMFLAG, pginfo))
        return "running";
    if (JobQueue::IsJobQueued(JOB_COMMFLAG, pginfo.GetChanID(),
                              pginfo.GetRecordingStartTime()))
        return "queued";

    return (pginfo.GetProgramFlags() & FL_COMMFLAG ? "yes" : "no");
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
    list.push_back(QString::number(pginfo.GetChanID()));
    list.push_back(pginfo.GetRecordingStartTime(MythDate::ISODate));
    list.push_back(QString() /* force Delete */);
    list.push_back(QString()); /* forget history */
}

static bool extract_one_del(
    QStringList &list, uint &chanid, QDateTime &recstartts)
{
    if (list.size() < 4)
    {
        list.clear();
        return false;
    }

    chanid     = list[0].toUInt();
    recstartts = MythDate::fromString(list[1]);

    list.pop_front();
    list.pop_front();
    list.pop_front();
    list.pop_front();

    if (!chanid || !recstartts.isValid())
        LOG(VB_GENERAL, LOG_ERR, LOC + "extract_one_del() invalid entry");

    return chanid && recstartts.isValid();
}

void * PlaybackBox::RunPlaybackBox(void * player, bool showTV)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    PlaybackBox *pbb = new PlaybackBox(
        mainStack,"playbackbox", (TV *)player, showTV);

    if (pbb->Create())
        mainStack->AddScreen(pbb);
    else
        delete pbb;

    return NULL;
}

PlaybackBox::PlaybackBox(MythScreenStack *parent, QString name,
                            TV *player, bool showTV)
    : ScheduleCommon(parent, name),
      m_prefixes(QObject::tr("^(The |A |An )")),
      m_titleChaff(" \\(.*\\)$"),
      // UI variables
      m_recgroupList(NULL),
      m_groupList(NULL),
      m_recordingList(NULL),
      m_noRecordingsText(NULL),
      m_previewImage(NULL),
      // Artwork Variables
      m_artHostOverride(),
      // Settings
      m_titleView(false),
      m_useCategories(false),
      m_useRecGroups(false),
      m_watchListAutoExpire(false),
      m_watchListMaxAge(60),              m_watchListBlackOut(2),
      m_listOrder(1),
      // Recording Group settings
      m_groupDisplayName(ProgramInfo::i18n("All Programs")),
      m_recGroup("All Programs"),
      m_watchGroupName(tr("Watch List")),
      m_watchGroupLabel(m_watchGroupName.toLower()),
      m_viewMask(VIEW_TITLES),

      // General m_popupMenu support
      m_menuDialog(NULL),
      m_popupMenu(NULL),
      m_doToggleMenu(true),
      // Main Recording List support
      m_progsInDB(0),
      m_isFilling(false),
      // Other state
      m_op_on_playlist(false),
      m_programInfoCache(this),           m_playingSomething(false),
      // Selection state variables
      m_needUpdate(false),
      m_haveGroupInfoSet(false),
      // Other
      m_player(NULL),
      m_helper(this),

      m_firstGroup(true),
      m_usingGroupSelector(false),
      m_groupSelected(false),
      m_passwordEntered(false)
{
    for (uint i = 0; i < sizeof(m_artImage) / sizeof(MythUIImage*); i++)
    {
        m_artImage[i] = NULL;
        m_artTimer[i] = new QTimer(this);
        m_artTimer[i]->setSingleShot(true);
    }

    m_recGroup           = gCoreContext->GetSetting("DisplayRecGroup",
                                                "All Programs");
    int pbOrder        = gCoreContext->GetNumSetting("PlayBoxOrdering", 1);
    // Split out sort order modes, wacky order for backward compatibility
    m_listOrder = (pbOrder >> 1) ^ (m_allOrder = pbOrder & 1);
    m_watchListStart     = gCoreContext->GetNumSetting("PlaybackWLStart", 0);

    m_watchListAutoExpire= gCoreContext->GetNumSetting("PlaybackWLAutoExpire", 0);
    m_watchListMaxAge    = gCoreContext->GetNumSetting("PlaybackWLMaxAge", 60);
    m_watchListBlackOut  = gCoreContext->GetNumSetting("PlaybackWLBlackOut", 2);

    bool displayCat  = gCoreContext->GetNumSetting("DisplayRecGroupIsCategory", 0);

    m_viewMask = (ViewMask)gCoreContext->GetNumSetting(
                                    "DisplayGroupDefaultViewMask",
                                    VIEW_TITLES | VIEW_WATCHED);

    // Translate these external settings into mask values
    if (gCoreContext->GetNumSetting("PlaybackWatchList", 1) &&
        !(m_viewMask & VIEW_WATCHLIST))
    {
        m_viewMask = (ViewMask)(m_viewMask | VIEW_WATCHLIST);
        gCoreContext->SaveSetting("DisplayGroupDefaultViewMask", (int)m_viewMask);
    }
    else if (! gCoreContext->GetNumSetting("PlaybackWatchList", 1) &&
             m_viewMask & VIEW_WATCHLIST)
    {
        m_viewMask = (ViewMask)(m_viewMask & ~VIEW_WATCHLIST);
        gCoreContext->SaveSetting("DisplayGroupDefaultViewMask", (int)m_viewMask);
    }

    // This setting is deprecated in favour of viewmask, this just ensures the
    // that it is converted over when upgrading from earlier versions
    if (gCoreContext->GetNumSetting("LiveTVInAllPrograms",0) &&
        !(m_viewMask & VIEW_LIVETVGRP))
    {
        m_viewMask = (ViewMask)(m_viewMask | VIEW_LIVETVGRP);
        gCoreContext->SaveSetting("DisplayGroupDefaultViewMask", (int)m_viewMask);
    }

    if (gCoreContext->GetNumSetting("MasterBackendOverride", 0))
        m_artHostOverride = gCoreContext->GetMasterHostName();

    if (player)
    {
        m_player = player;
        QString tmp = m_player->GetRecordingGroup(0);
        if (!tmp.isEmpty())
            m_recGroup = tmp;
    }

    // recording group stuff
    m_recGroupIdx = -1;
    m_recGroupType.clear();
    m_recGroupType[m_recGroup] = (displayCat) ? "category" : "recgroup";
    m_groupDisplayName = ProgramInfo::i18n(m_recGroup);

    fillRecGroupPasswordCache();

    // misc setup
    gCoreContext->addListener(this);

    m_popupStack = GetMythMainWindow()->GetStack("popup stack");
}

PlaybackBox::~PlaybackBox(void)
{
    gCoreContext->removeListener(this);
    PreviewGeneratorQueue::RemoveListener(this);

    for (uint i = 0; i < sizeof(m_artImage) / sizeof(MythUIImage*); i++)
    {
        m_artTimer[i]->disconnect(this);
        m_artTimer[i] = NULL;
        m_artImage[i] = NULL;
    }

    if (m_player)
    {
        QString message = QString("PLAYBACKBOX_EXITING");
        qApp->postEvent(m_player, new MythEvent(
                            message, m_player_selected_new_show));
    }
}

bool PlaybackBox::Create()
{
    if (!LoadWindowFromXML("recordings-ui.xml", "watchrecordings", this))
        return false;

    m_recgroupList  = dynamic_cast<MythUIButtonList *> (GetChild("recgroups"));
    m_groupList     = dynamic_cast<MythUIButtonList *> (GetChild("groups"));
    m_recordingList = dynamic_cast<MythUIButtonList *> (GetChild("recordings"));

    m_noRecordingsText = dynamic_cast<MythUIText *> (GetChild("norecordings"));

    m_previewImage = dynamic_cast<MythUIImage *>(GetChild("preview"));
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
        m_recgroupList->SetCanTakeFocus(false);

    connect(m_groupList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            SLOT(updateRecList(MythUIButtonListItem*)));
    connect(m_groupList, SIGNAL(itemClicked(MythUIButtonListItem*)),
            SLOT(SwitchList()));
    connect(m_recordingList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            SLOT(ItemSelected(MythUIButtonListItem*)));
    connect(m_recordingList, SIGNAL(itemClicked(MythUIButtonListItem*)),
            SLOT(PlayFromBookmark(MythUIButtonListItem*)));
    connect(m_recordingList, SIGNAL(itemVisible(MythUIButtonListItem*)),
            SLOT(ItemVisible(MythUIButtonListItem*)));
    connect(m_recordingList, SIGNAL(itemLoaded(MythUIButtonListItem*)),
            SLOT(ItemLoaded(MythUIButtonListItem*)));

    // connect up timers...
    connect(m_artTimer[kArtworkFanart],   SIGNAL(timeout()), SLOT(fanartLoad()));
    connect(m_artTimer[kArtworkBanner],   SIGNAL(timeout()), SLOT(bannerLoad()));
    connect(m_artTimer[kArtworkCoverart], SIGNAL(timeout()), SLOT(coverartLoad()));

    BuildFocusList();
    m_programInfoCache.ScheduleLoad(false);
    LoadInBackground();

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

    if (!gCoreContext->GetNumSetting("PlaybackBoxStartInTitle", 0))
        SetFocusWidget(m_recordingList);
}

void PlaybackBox::SwitchList()
{
    if (GetFocusWidget() == m_groupList)
        SetFocusWidget(m_recordingList);
    else if (GetFocusWidget() == m_recordingList)
        SetFocusWidget(m_groupList);
}

void PlaybackBox::displayRecGroup(const QString &newRecGroup)
{
    m_groupSelected = true;

    QString password = m_recGroupPwCache[newRecGroup];

    m_newRecGroup = newRecGroup;
    if (m_curGroupPassword != password && !password.isEmpty())
    {
        MythScreenStack *popupStack =
                                GetMythMainWindow()->GetStack("popup stack");

        QString label = tr("Password for group '%1':").arg(newRecGroup);

        MythTextInputDialog *pwd = new MythTextInputDialog(popupStack,
                                            label, FilterNone, true);

        connect(pwd, SIGNAL(haveResult(QString)),
                SLOT(checkPassword(QString)));
        connect(pwd, SIGNAL(Exiting(void)),
                SLOT(passwordClosed(void)));

        m_passwordEntered = false;

        if (pwd->Create())
            popupStack->AddScreen(pwd, false);

        return;
    }

    setGroupFilter(newRecGroup);
}

void PlaybackBox::checkPassword(const QString &password)
{
    m_passwordEntered = true;

    QString grouppass = m_recGroupPwCache[m_newRecGroup];
    if (password == grouppass)
        setGroupFilter(m_newRecGroup);
    else
        qApp->postEvent(this, new MythEvent("DISPLAY_RECGROUP",
                                            m_newRecGroup));
}

void PlaybackBox::passwordClosed(void)
{
    if (m_passwordEntered)
        return;

    if (m_usingGroupSelector || m_firstGroup)
        showGroupFilter();
}

void PlaybackBox::updateGroupInfo(const QString &groupname,
                                  const QString &grouplabel)
{
    InfoMap infoMap;
    int countInGroup;

    infoMap["group"] = m_groupDisplayName;
    infoMap["title"] = grouplabel;
    infoMap["show"] =
        groupname.isEmpty() ? ProgramInfo::i18n("All Programs") : grouplabel;
    countInGroup = m_progLists[groupname].size();

    if (m_artImage[kArtworkFanart])
    {
        if (!groupname.isEmpty() && !m_progLists[groupname].empty())
        {
            ProgramInfo *pginfo = *m_progLists[groupname].begin();

            QString fn = m_helper.LocateArtwork(
                pginfo->GetInetRef(), pginfo->GetSeason(), kArtworkFanart, NULL, groupname);

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

    QString desc = tr("There is/are %n recording(s) in this display group",
                      "", countInGroup);

    if (countInGroup > 1)
    {
        ProgramList  group     = m_progLists[groupname];
        float        groupSize = 0.0;

        for (ProgramList::iterator it = group.begin(); it != group.end(); ++it)
        {
            ProgramInfo *info = *it;
            if (info)
            {
                uint64_t filesize = info->GetFilesize();
                if (filesize == 0 || info->GetRecordingStatus() == rsRecording)
                {
                    filesize = info->QueryFilesize();
                    info->SetFilesize(filesize);
                }
                groupSize += filesize;
            }
        }

        desc += tr(", which consume %1");
        desc += tr("GB", "GigaBytes");

        desc = desc.arg(groupSize / 1024.0 / 1024.0 / 1024.0, 0, 'f', 2);
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
        m_recordingList->GetItemByData(qVariantFromValue(pginfo));

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

static const char *disp_flags[] = { "playlist", "watched", "preserve",
                                    "cutlist", "autoexpire", "editing",
                                    "bookmark", "inuse", "transcoded" };

void PlaybackBox::SetItemIcons(MythUIButtonListItem *item, ProgramInfo* pginfo)
{
    bool disp_flag_stat[sizeof(disp_flags)/sizeof(char*)];

    disp_flag_stat[0] = !m_playList.filter(pginfo->MakeUniqueKey()).empty();
    disp_flag_stat[1] = pginfo->IsWatched();
    disp_flag_stat[2] = pginfo->IsPreserved();
    disp_flag_stat[3] = pginfo->HasCutlist();
    disp_flag_stat[4] = pginfo->IsAutoExpirable();
    disp_flag_stat[5] = pginfo->GetProgramFlags() & FL_EDITING;
    disp_flag_stat[6] = pginfo->IsBookmarkSet();
    disp_flag_stat[7] = pginfo->IsInUsePlaying();
    disp_flag_stat[8] = pginfo->GetProgramFlags() & FL_TRANSCODED;

    for (uint i = 0; i < sizeof(disp_flags) / sizeof(char*); ++i)
        item->DisplayState(disp_flag_stat[i]?"yes":"no", disp_flags[i]);
}

void PlaybackBox::UpdateUIListItem(MythUIButtonListItem *item,
                                   bool is_sel, bool force_preview_reload)
{
    if (!item)
        return;

    ProgramInfo *pginfo = qVariantValue<ProgramInfo *>(item->GetData());

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
            item->SetText(tempSubTitle, "titlesubtitle");
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
        m_preview_tokens.insert(m_helper.GetPreviewImage(*pginfo));

    if ((GetFocusWidget() == m_recordingList) && is_sel)
    {
        InfoMap infoMap;

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

        // Handle artwork
        QString arthost;
        for (uint i = 0; i < sizeof(m_artImage) / sizeof(MythUIImage*); i++)
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
    ProgramInfo *pginfo = qVariantValue<ProgramInfo*>(item->GetData());
    if (item->GetText("is_item_initialized").isNull())
    {
        QMap<AudioProps, QString> audioFlags;
        audioFlags[AUD_DOLBY] = "dolby";
        audioFlags[AUD_SURROUND] = "surround";
        audioFlags[AUD_STEREO] = "stereo";
        audioFlags[AUD_MONO] = "mono";

        QMap<VideoProps, QString> videoFlags;
        videoFlags[VID_1080] = "hd1080";
        videoFlags[VID_720] = "hd720";
        videoFlags[VID_HDTV] = "hdtv";
        videoFlags[VID_WIDESCREEN] = "widescreen";

        QMap<SubtitleTypes, QString> subtitleFlags;
        subtitleFlags[SUB_SIGNED] = "deafsigned";
        subtitleFlags[SUB_ONSCREEN] = "onscreensub";
        subtitleFlags[SUB_NORMAL] = "subtitles";
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
            item->SetText(tempSubTitle, "titlesubtitle");

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

        QMap<VideoProps, QString>::iterator vit;
        for (vit = videoFlags.begin(); vit != videoFlags.end(); ++vit)
        {
            if (pginfo->GetVideoProperties() & vit.key())
                item->DisplayState(vit.value(), "videoprops");
        }

        QMap<SubtitleTypes, QString>::iterator sit;
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
    ProgramInfo *pginfo = qVariantValue<ProgramInfo*>(item->GetData());

    ItemLoaded(item);
    // Job status (recording, transcoding, flagging)
    QString job = extract_job_state(*pginfo);
    item->DisplayState(job, "jobstate");

    // Flagging status (queued, running, no, yes)
    item->DisplayState(extract_commflag_state(*pginfo), "commflagged");

    MythUIButtonListItem *sel_item = item->parent()->GetItemCurrent();
    if ((item != sel_item) && item->GetImageFilename("preview").isEmpty() &&
        (asAvailable == pginfo->GetAvailableStatus()))
    {
        QString token = m_helper.GetPreviewImage(*pginfo, true);
        if (token.isEmpty())
            return;

        m_preview_tokens.insert(token);
        // now make sure selected item is still at the top of the queue
        ProgramInfo *sel_pginfo =
            qVariantValue<ProgramInfo*>(sel_item->GetData());
        if (sel_pginfo && sel_item->GetImageFilename("preview").isEmpty() &&
            (asAvailable == sel_pginfo->GetAvailableStatus()))
        {
            m_preview_tokens.insert(
                m_helper.GetPreviewImage(*sel_pginfo, false));
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

    const QString piKey       = list[0];
    const QString previewFile = list[1];
    const QString message     = list[2];

    bool found = false;
    for (uint i = 4; i < (uint) list.size(); i++)
    {
        QString token = list[i];
        QSet<QString>::iterator it = m_preview_tokens.find(token);
        if (it != m_preview_tokens.end())
        {
            found = true;
            m_preview_tokens.erase(it);
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

    ProgramInfo *info = m_programInfoCache.GetProgramInfo(piKey);
    MythUIButtonListItem *item = NULL;

    if (info)
        item = m_recordingList->GetItemByData(qVariantFromValue(info));

    if (!item)
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            "Ignoring PREVIEW_SUCCESS, item no longer on screen.");
    }

    if (item)
    {
        LOG(VB_GUI, LOG_INFO, LOC + QString("Loading preview %1,\n\t\t\tmsg %2")
                .arg(previewFile).arg(message));

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

    MythUIImage *iconImage = NULL;
    MythUIStateType *iconState = NULL;
    for (it = iconMap.begin(); it != iconMap.end(); ++it)
    {
        iconImage = dynamic_cast<MythUIImage *>(GetChild(it.key()));
        if (iconImage)
            iconImage->SetVisible(flags & (*it));

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
    iconMap["dolby"]  = AUD_DOLBY;
    iconMap["surround"]  = AUD_SURROUND;
    iconMap["stereo"] = AUD_STEREO;
    iconMap["mono"] = AUD_MONO;

    iconState = dynamic_cast<MythUIStateType *>(GetChild("audioprops"));
    bool haveIcon = false;
    if (pginfo && iconState)
    {
        for (it = iconMap.begin(); it != iconMap.end(); ++it)
        {
            if (pginfo->GetAudioProperties() & (*it))
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
    iconMap["avchd"] = VID_AVC;
    iconMap["hd1080"] = VID_1080;
    iconMap["hd720"] = VID_720;
    iconMap["hdtv"] = VID_HDTV;
    iconMap["widescreen"] = VID_WIDESCREEN;

    iconState = dynamic_cast<MythUIStateType *>(GetChild("videoprops"));
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

    double freeSpaceTotal = (double) m_helper.GetFreeSpaceTotalMB();
    double freeSpaceUsed  = (double) m_helper.GetFreeSpaceUsedMB();

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
                                                .arg(QString::number((int)perc))
                                                .arg(size);

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

    m_recgroupList->Reset();

    int idx = 0;
    QStringList::iterator it = m_recGroups.begin();
    for (; it != m_recGroups.end(); (++it), (++idx))
    {
        QString key = (*it);
        QString tmp = (key == "All Programs") ? "All" : key;
        QString name = ProgramInfo::i18n(tmp);

        if (m_recGroups.size() == 2 && key == "Default")
            continue;  // All and Default will be the same, so only show All

        MythUIButtonListItem *item = new MythUIButtonListItem(
            m_recgroupList, name, qVariantFromValue(key));

        if (idx == m_recGroupIdx)
            m_recgroupList->SetItemCurrent(item);
        item->SetText(name);
    }
}

void PlaybackBox::UpdateUIGroupList(const QStringList &groupPreferences)
{
    m_groupList->Reset();

    if (!m_titleList.isEmpty())
    {
        int best_pref = INT_MAX, sel_idx = 0;
        QStringList::iterator it;
        for (it = m_titleList.begin(); it != m_titleList.end(); ++it)
        {
            QString groupname = (*it);

            MythUIButtonListItem *item =
                new MythUIButtonListItem(
                    m_groupList, "", qVariantFromValue(groupname.toLower()));

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
    }
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

    ProgramList::iterator it = progList.begin();
    for (; it != progList.end(); ++it)
    {
        if ((*it)->GetAvailableStatus() == asPendingDelete ||
            (*it)->GetAvailableStatus() == asDeleted)
            continue;

        new PlaybackBoxListItem(this, m_recordingList, *it);
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
        ProgramInfo *pginfo = qVariantValue<ProgramInfo*>(item->GetData());
        itemSelPref.push_back(groupSelPref.front());
        itemSelPref.push_back(pginfo->MakeUniqueKey());
    }
    for (int i = curPos; (i >= 0) && (i < recordingList->GetCount()); i--)
    {
        MythUIButtonListItem *item = recordingList->GetItemAt(i);
        ProgramInfo *pginfo = qVariantValue<ProgramInfo*>(item->GetData());
        itemSelPref.push_back(groupSelPref.front());
        itemSelPref.push_back(pginfo->MakeUniqueKey());
    }

    int topPos = recordingList->GetTopItemPos();
    for (int i = topPos + 1; i >= topPos - 1; i--)
    {
        if (i >= 0 && i < recordingList->GetCount())
        {
            MythUIButtonListItem *item = recordingList->GetItemAt(i);
            ProgramInfo *pginfo = qVariantValue<ProgramInfo*>(item->GetData());
            if (i == topPos)
            {
                itemTopPref.push_front(pginfo->MakeUniqueKey());
                itemTopPref.push_front(groupSelPref.front());
            }
            else
            {
                itemTopPref.push_back(groupSelPref.front());
                itemTopPref.push_back(pginfo->MakeUniqueKey());
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

        const QString key = itemSelPref[i+1];
        for (uint j = 0; j < (uint)recordingList->GetCount(); j++)
        {
            MythUIButtonListItem *item = recordingList->GetItemAt(j);
            ProgramInfo *pginfo = qVariantValue<ProgramInfo*>(item->GetData());
            if (pginfo && (pginfo->MakeUniqueKey() == key))
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

        const QString key = itemTopPref[i+1];
        for (uint j = 0; j < (uint)recordingList->GetCount(); j++)
        {
            MythUIButtonListItem *item = recordingList->GetItemAt(j);
            ProgramInfo *pginfo = qVariantValue<ProgramInfo*>(item->GetData());
            if (pginfo && (pginfo->MakeUniqueKey() == key))
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
    QStringList groupSelPref, itemSelPref, itemTopPref;
    if (!save_position(m_groupList, m_recordingList,
                       groupSelPref, itemSelPref, itemTopPref))
    {
        // If user wants to start in watchlist and watchlist is displayed, then
        // make it the current group
        if (m_watchListStart && (m_viewMask & VIEW_WATCHLIST))
            groupSelPref.push_back(m_watchGroupLabel);
    }

    // Cache available status for later restoration
    QMap<QString, AvailableStatusType> asCache;
    QString asKey;

    if (!m_progLists.isEmpty())
    {
        ProgramList::iterator it = m_progLists[""].begin();
        ProgramList::iterator end = m_progLists[""].end();
        for (; it != end; ++it)
        {
            asKey = (*it)->MakeUniqueKey();
            asCache[asKey] = (*it)->GetAvailableStatus();
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

    QMap<QString, QString> sortedList;
    QMap<int, QString> searchRule;
    QMap<int, int> recidEpisodes;

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
                    tmpTitle.remove(m_titleChaff);
                    searchRule[query.value(0).toInt()] = tmpTitle;
                }
            }
        }

        vector<ProgramInfo*> list;
        bool newest_first = (0==m_allOrder);
        m_programInfoCache.GetOrdered(list, newest_first);
        vector<ProgramInfo*>::const_iterator it = list.begin();
        for ( ; it != list.end(); ++it)
        {
            if ((*it)->IsDeletePending())
                continue;

            m_progsInDB++;
            ProgramInfo *p = *it;

            if (p->GetTitle().isEmpty())
                p->SetTitle(tr("_NO_TITLE_"));

            if ((((p->GetRecordingGroup() == m_recGroup) ||
                  ((m_recGroup == "All Programs") &&
                   (p->GetRecordingGroup() != "Deleted") &&
                   (p->GetRecordingGroup() != "LiveTV")) ||
                  (p->GetRecordingGroup() == "LiveTV" &&
                   (m_viewMask & VIEW_LIVETVGRP))) &&
                 (m_recGroupPwCache[m_recGroup] == m_curGroupPassword)) ||
                ((m_recGroupType[m_recGroup] == "category") &&
                 ((p->GetCategory() == m_recGroup ) ||
                  ((p->GetCategory().isEmpty()) &&
                   (m_recGroup == tr("Unknown")))) &&
                 ( !m_recGroupPwCache.contains(p->GetRecordingGroup()))))
            {
                if ((!(m_viewMask & VIEW_WATCHED)) && p->IsWatched())
                    continue;

                if (m_viewMask != VIEW_NONE &&
                    (p->GetRecordingGroup() != "LiveTV" ||
                     m_recGroup == "LiveTV"))
                {
                    m_progLists[""].push_front(p);
                }

                asKey = p->MakeUniqueKey();
                if (asCache.contains(asKey))
                    p->SetAvailableStatus(asCache[asKey], "UpdateUILists");
                else
                    p->SetAvailableStatus(asAvailable,  "UpdateUILists");

                if (m_recGroup != "LiveTV" &&
                    (p->GetRecordingGroup() == "LiveTV") &&
                    (m_viewMask & VIEW_LIVETVGRP))
                {
                    QString tmpTitle = tr("Live TV");
                    sortedList[tmpTitle.toLower()] = tmpTitle;
                    m_progLists[tmpTitle.toLower()].push_front(p);
                    m_progLists[tmpTitle.toLower()].setAutoDelete(false);
                    continue;
                }

                if ((m_viewMask & VIEW_TITLES) && // Show titles
                    ((p->GetRecordingGroup() != "LiveTV") ||
                     (m_recGroup == "LiveTV")))
                {
                    sTitle = construct_sort_title(
                        p->GetTitle(), m_viewMask, titleSort,
                        p->GetRecordingPriority(), m_prefixes);
                    sTitle = sTitle.toLower();

                    if (!sortedList.contains(sTitle))
                        sortedList[sTitle] = p->GetTitle();
                    m_progLists[sortedList[sTitle].toLower()].push_front(p);
                    m_progLists[sortedList[sTitle].toLower()].setAutoDelete(false);
                }

                if ((m_viewMask & VIEW_RECGROUPS) &&
                    !p->GetRecordingGroup().isEmpty() &&
                    p->GetRecordingGroup() != "LiveTV") // Show recording groups
                {
                    sortedList[p->GetRecordingGroup().toLower()] =
                        p->GetRecordingGroup();
                    m_progLists[p->GetRecordingGroup().toLower()]
                        .push_front(p);
                    m_progLists[p->GetRecordingGroup().toLower()]
                        .setAutoDelete(false);
                }

                if ((m_viewMask & VIEW_CATEGORIES) &&
                    !p->GetCategory().isEmpty()) // Show categories
                {
                    QString catl = p->GetCategory().toLower();
                    sortedList[catl] = p->GetCategory();
                    m_progLists[catl].push_front(p);
                    m_progLists[catl].setAutoDelete(false);
                }

                if ((m_viewMask & VIEW_SEARCHES) &&
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
                    p->GetRecordingGroup() != "LiveTV" &&
                    p->GetRecordingGroup() != "Deleted")
                {
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
                        if (p->GetRecordingRuleID())
                            recidEpisodes[p->GetRecordingRuleID()] += 1;
                        if (recidEpisodes[p->GetRecordingRuleID()] == 1 ||
                            !p->GetRecordingRuleID())
                        {
                            m_progLists[m_watchGroupLabel].push_front(p);
                            m_progLists[m_watchGroupLabel].setAutoDelete(false);
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
        }
    }

    if (sortedList.empty())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "SortedList is Empty");
        m_progLists[""];
        m_titleList << "";
        m_playList.clear();

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
        QDateTime now = MythDate::current();
        int baseValue = m_watchListMaxAge * 2 / 3;

        QMap<int, int> recType;
        QMap<int, int> maxEpisodes;
        QMap<int, int> avgDelay;
        QMap<int, int> spanHours;
        QMap<int, int> delHours;
        QMap<int, int> nextHours;

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT recordid, type, maxepisodes, avg_delay, "
                      "next_record, last_record, last_delete FROM record;");

        if (query.exec())
        {
            while (query.next())
            {
                int recid = query.value(0).toInt();
                recType[recid] = query.value(1).toInt();
                maxEpisodes[recid] = query.value(2).toInt();
                avgDelay[recid] = query.value(3).toInt();

                QDateTime next_record =
                    MythDate::as_utc(query.value(4).toDateTime());
                QDateTime last_record =
                    MythDate::as_utc(query.value(5).toDateTime());
                QDateTime last_delete =
                    MythDate::as_utc(query.value(6).toDateTime());

                // Time between the last and next recordings
                spanHours[recid] = 1000;
                if (last_record.isValid() && next_record.isValid())
                    spanHours[recid] =
                        last_record.secsTo(next_record) / 3600 + 1;

                // Time since the last episode was deleted
                delHours[recid] = 1000;
                if (last_delete.isValid())
                    delHours[recid] = last_delete.secsTo(now) / 3600 + 1;

                // Time until the next recording if any
                if (next_record.isValid())
                    nextHours[recid] = now.secsTo(next_record) / 3600 + 1;
            }
        }

        ProgramList::iterator pit = m_progLists[m_watchGroupLabel].begin();
        while (pit != m_progLists[m_watchGroupLabel].end())
        {
            int recid = (*pit)->GetRecordingRuleID();
            int avgd =  avgDelay[recid];

            if (avgd == 0)
                avgd = 100;

            // Set the intervals beyond range if there is no record entry
            if (spanHours[recid] == 0)
            {
                spanHours[recid] = 1000;
                delHours[recid] = 1000;
            }

            // add point equal to baseValue for each additional episode
            if (!(*pit)->GetRecordingRuleID() || maxEpisodes[recid] > 0)
                (*pit)->SetRecordingPriority2(0);
            else
            {
                (*pit)->SetRecordingPriority2(
                    (recidEpisodes[(*pit)->GetRecordingRuleID()] - 1) *
                    baseValue);
            }

            // add points every 3hr leading up to the next recording
            if (nextHours[recid] > 0 && nextHours[recid] < baseValue * 3)
            {
                (*pit)->SetRecordingPriority2(
                    (*pit)->GetRecordingPriority2() +
                    (baseValue * 3 - nextHours[recid]) / 3);
            }

            int hrs = (*pit)->GetScheduledEndTime().secsTo(now) / 3600;
            if (hrs < 1)
                hrs = 1;

            // add points for a new recording that decrease each hour
            if (hrs < 42)
            {
                (*pit)->SetRecordingPriority2(
                    (*pit)->GetRecordingPriority2() + 42 - hrs);
            }

            // add points for how close the recorded time of day is to 'now'
            (*pit)->SetRecordingPriority2(
                (*pit)->GetRecordingPriority2() + abs((hrs % 24) - 12) * 2);

            // Daily
            if (spanHours[recid] < 50 ||
                recType[recid] == kDailyRecord)
            {
                if (delHours[recid] < m_watchListBlackOut * 4)
                {
                    (*pit)->SetRecordingPriority2(wlDeleted);
                    LOG(VB_FILE, LOG_INFO,
                        QString("Recently deleted daily:  %1")
                            .arg((*pit)->GetTitle()));
                    pit = m_progLists[m_watchGroupLabel].erase(pit);
                    continue;
                }
                else
                {
                    LOG(VB_FILE, LOG_INFO, QString("Daily interval:  %1")
                            .arg((*pit)->GetTitle()));

                    if (maxEpisodes[recid] > 0)
                    {
                        (*pit)->SetRecordingPriority2(
                            (*pit)->GetRecordingPriority2() +
                            (baseValue / 2) + (hrs / 24));
                    }
                    else
                    {
                        (*pit)->SetRecordingPriority2(
                            (*pit)->GetRecordingPriority2() +
                            (baseValue / 5) + hrs);
                    }
                }
            }
            // Weekly
            else if (nextHours[recid] ||
                     recType[recid] == kWeeklyRecord)

            {
                if (delHours[recid] < (m_watchListBlackOut * 24) - 4)
                {
                    (*pit)->SetRecordingPriority2(wlDeleted);
                    LOG(VB_FILE, LOG_INFO,
                        QString("Recently deleted weekly:  %1")
                            .arg((*pit)->GetTitle()));
                    pit = m_progLists[m_watchGroupLabel].erase(pit);
                    continue;
                }
                else
                {
                    LOG(VB_FILE, LOG_INFO, QString("Weekly interval: %1")
                            .arg((*pit)->GetTitle()));

                    if (maxEpisodes[recid] > 0)
                    {
                        (*pit)->SetRecordingPriority2(
                            (*pit)->GetRecordingPriority2() +
                            (baseValue / 2) + (hrs / 24));
                    }
                    else
                    {
                        (*pit)->SetRecordingPriority2(
                            (*pit)->GetRecordingPriority2() +
                            (baseValue / 3) + (baseValue * hrs / 24 / 4));
                    }
                }
            }
            // Not recurring
            else
            {
                if (delHours[recid] < (m_watchListBlackOut * 48) - 4)
                {
                    (*pit)->SetRecordingPriority2(wlDeleted);
                    pit = m_progLists[m_watchGroupLabel].erase(pit);
                    continue;
                }
                else
                {
                    // add points for a new Single or final episode
                    if (hrs < 36)
                    {
                        (*pit)->SetRecordingPriority2(
                            (*pit)->GetRecordingPriority2() +
                            baseValue * (36 - hrs) / 36);
                    }

                    if (avgd != 100)
                    {
                        if (maxEpisodes[recid] > 0)
                        {
                            (*pit)->SetRecordingPriority2(
                                (*pit)->GetRecordingPriority2() +
                                (baseValue / 2) + (hrs / 24));
                        }
                        else
                        {
                            (*pit)->SetRecordingPriority2(
                                (*pit)->GetRecordingPriority2() +
                                (baseValue / 3) + (baseValue * hrs / 24 / 4));
                        }
                    }
                    else if ((hrs / 24) < m_watchListMaxAge)
                    {
                        (*pit)->SetRecordingPriority2(
                            (*pit)->GetRecordingPriority2() +
                            hrs / 24);
                    }
                    else
                    {
                        (*pit)->SetRecordingPriority2(
                            (*pit)->GetRecordingPriority2() +
                            m_watchListMaxAge);
                    }
                }
            }

            // Factor based on the average time shift delay.
            // Scale the avgd range of 0 thru 200 hours to 133% thru 67%
            int delaypct = avgd / 3 + 67;

            if (avgd < 100)
            {
                (*pit)->SetRecordingPriority2(
                    (*pit)->GetRecordingPriority2() * (200 - delaypct) / 100);
            }
            else if (avgd > 100)
            {
                (*pit)->SetRecordingPriority2(
                    (*pit)->GetRecordingPriority2() * 100 / delaypct);
            }

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
    if (m_progLists[m_watchGroupLabel].size() > 0)
        m_titleList << m_watchGroupName;
    if ((m_progLists["livetv"].size() > 0) &&
        (!sortedList.values().contains(tr("Live TV"))))
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
                if (name != "Deleted" && name != "LiveTV")
                {
                    m_recGroups.append(name);
                    m_recGroupType[name] = "recgroup";
                }
            }

            m_recGroupIdx = m_recGroups.indexOf(m_recGroup);
            if (m_recGroupIdx < 0)
                m_recGroupIdx = 0;
        }
    }

    UpdateUIRecGroupList();
    UpdateUIGroupList(groupSelPref);
    UpdateUsageUI();

    QStringList::const_iterator it = m_playList.begin();
    for (; it != m_playList.end(); ++it)
    {
        ProgramInfo *pginfo = FindProgramInUILists(*it);
        if (!pginfo)
            continue;
        MythUIButtonListItem *item =
            m_recordingList->GetItemByData(qVariantFromValue(pginfo));
        if (item)
            item->DisplayState("yes", "playlist");
    }

    restore_position(m_groupList, m_recordingList,
                     groupSelPref, itemSelPref, itemTopPref);

    m_isFilling = false;

    return true;
}

void PlaybackBox::playSelectedPlaylist(bool _random)
{
    if (_random)
    {
        m_playListPlay.clear();
        QStringList tmp = m_playList;
        while (!tmp.empty())
        {
            uint i = random() % tmp.size();
            m_playListPlay.push_back(tmp[i]);
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

void PlaybackBox::PlayFromBookmark(MythUIButtonListItem *item)
{
    if (!item)
        item = m_recordingList->GetItemCurrent();

    if (!item)
        return;

    ProgramInfo *pginfo = qVariantValue<ProgramInfo *>(item->GetData());

    if (pginfo)
        PlayX(*pginfo, false, false);
}

void PlaybackBox::PlayFromBeginning(MythUIButtonListItem *item)
{
    if (!item)
        item = m_recordingList->GetItemCurrent();

    if (!item)
        return;

    ProgramInfo *pginfo = qVariantValue<ProgramInfo *>(item->GetData());

    if (pginfo)
        PlayX(*pginfo, true, false);
}

void PlaybackBox::PlayX(const ProgramInfo &pginfo,
                        bool ignoreBookmark,
                        bool underNetworkControl)
{
    if (!m_player)
    {
        Play(pginfo, false, ignoreBookmark, underNetworkControl);
        return;
    }

    if (!m_player->IsSameProgram(0, &pginfo))
    {
        pginfo.ToStringList(m_player_selected_new_show);
        m_player_selected_new_show.push_back(
            ignoreBookmark ? "1" : "0");
        m_player_selected_new_show.push_back(
            underNetworkControl ? "1" : "0");
    }
    Close();
}

void PlaybackBox::StopSelected(void)
{
    ProgramInfo *pginfo = CurrentItem();
    if (pginfo)
        m_helper.StopRecording(*pginfo);
}

void PlaybackBox::deleteSelected(MythUIButtonListItem *item)
{
    if (!item)
        return;

    ProgramInfo *pginfo = qVariantValue<ProgramInfo *>(item->GetData());

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

void PlaybackBox::previous()
{
    ProgramInfo *pginfo = CurrentItem();
    if (pginfo)
        ShowPrevious(pginfo);
}

void PlaybackBox::upcoming()
{
    ProgramInfo *pginfo = CurrentItem();
    if (pginfo)
        ShowUpcoming(pginfo);
}

void PlaybackBox::upcomingScheduled()
{
    ProgramInfo *pginfo = CurrentItem();
    if (pginfo)
        ShowUpcomingScheduled(pginfo);
}

ProgramInfo *PlaybackBox::CurrentItem(void)
{
    ProgramInfo *pginfo = NULL;

    MythUIButtonListItem *item = m_recordingList->GetItemCurrent();

    if (!item)
        return NULL;

    pginfo = qVariantValue<ProgramInfo *>(item->GetData());

    if (!pginfo)
        return NULL;

    return pginfo;
}

void PlaybackBox::customEdit()
{
    ProgramInfo *pginfo = CurrentItem();
    if (pginfo)
        EditCustom(pginfo);
}

void PlaybackBox::details()
{
    ProgramInfo *pginfo = CurrentItem();
    if (pginfo)
        ShowDetails(pginfo);
}

void PlaybackBox::selected(MythUIButtonListItem *item)
{
    if (!item)
        return;

    PlayFromBookmark(item);
}

void PlaybackBox::popupClosed(QString which, int result)
{
    m_menuDialog = NULL;

    if (result == -2)
    {
        if (!m_doToggleMenu)
        {
            m_doToggleMenu = true;
            return;
        }

        if (which == "groupmenu")
        {
            ProgramInfo *pginfo = CurrentItem();
            if (pginfo)
            {
                m_helper.CheckAvailability(*pginfo, kCheckForMenuAction);

                if (asPendingDelete == pginfo->GetAvailableStatus())
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
        m_doToggleMenu = true;
}

void PlaybackBox::ShowGroupPopup()
{
    QString label = tr("Group List Menu");

    ProgramInfo *pginfo = CurrentItem();

    m_popupMenu = new MythMenu(label, this, "groupmenu");

    m_popupMenu->AddItem(tr("Change Group Filter"),
                         SLOT(showGroupFilter()));

    m_popupMenu->AddItem(tr("Change Group View"),
                         SLOT(showViewChanger()));

    if (m_recGroupType[m_recGroup] == "recgroup")
        m_popupMenu->AddItem(tr("Change Group Password"),
                             SLOT(showRecGroupPasswordChanger()));

    if (m_playList.size())
    {
        m_popupMenu->AddItem(tr("Playlist options"), NULL, createPlaylistMenu());
    }
    else if (!m_player)
    {
        if (GetFocusWidget() == m_groupList)
        {
            m_popupMenu->AddItem(tr("Add this Group to Playlist"),
                                 SLOT(togglePlayListTitle()));
        }
        else if (pginfo)
        {
            m_popupMenu->AddItem(tr("Add this recording to Playlist"),
                                 SLOT(togglePlayListItem()));
        }
    }

    m_popupMenu->AddItem(tr("Help (Status Icons)"), SLOT(showIconHelp()));

    DisplayPopupMenu();
}

bool PlaybackBox::Play(
    const ProgramInfo &rec,
    bool inPlaylist, bool ignoreBookmark, bool underNetworkControl)
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

    for (uint i = 0; i < sizeof(m_artImage) / sizeof(MythUIImage*); i++)
    {
        if (!m_artImage[i])
            continue;

        m_artTimer[i]->stop();
        m_artImage[i]->Reset();
    }

    ProgramInfo tvrec(rec);

    m_playingSomething = true;
    int initIndex = m_recordingList->StopLoad();

    uint flags =
        (inPlaylist          ? kStartTVInPlayList       : kStartTVNoFlags) |
        (underNetworkControl ? kStartTVByNetworkCommand : kStartTVNoFlags) |
        (ignoreBookmark      ? kStartTVIgnoreBookmark   : kStartTVNoFlags);

    playCompleted = TV::StartTV(&tvrec, flags);

    m_playingSomething = false;
    m_recordingList->LoadInBackground(initIndex);

    if (inPlaylist && !m_playListPlay.empty())
    {
        QCoreApplication::postEvent(
            this, new MythEvent("PLAY_PLAYLIST"));
    }
    else
    {
        // User may have saved or deleted a bookmark,
        // requiring update of preview..
        ProgramInfo *pginfo = m_programInfoCache.GetProgramInfo(
            tvrec.GetChanID(), tvrec.GetRecordingStartTime());
        if (pginfo)
            UpdateUIListItem(pginfo, true);
    }

    if (m_needUpdate)
        ScheduleUpdateUIList();

    return playCompleted;
}

void PlaybackBox::RemoveProgram(
    uint chanid, const QDateTime &recstartts,
    bool forgetHistory, bool forceMetadataDelete)
{
    ProgramInfo *delItem = FindProgramInUILists(chanid, recstartts);

    if (!delItem)
        return;

    if (!forceMetadataDelete &&
        ((delItem->GetAvailableStatus() == asPendingDelete) ||
         !delItem->QueryIsDeleteCandidate()))
    {
        return;
    }

    if (m_playList.filter(delItem->MakeUniqueKey()).size())
        togglePlayListItem(delItem);

    if (!forceMetadataDelete)
        delItem->UpdateLastDelete(true);

    delItem->SetAvailableStatus(asPendingDelete, "RemoveProgram");
    m_helper.DeleteRecording(
        delItem->GetChanID(), delItem->GetRecordingStartTime(),
        forceMetadataDelete, forgetHistory);

    // if the item is in the current recording list UI then delete it.
    MythUIButtonListItem *uiItem =
        m_recordingList->GetItemByData(qVariantFromValue(delItem));
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

    ProgramInfo *delItem = NULL;
    if (m_delList.empty() && (delItem = CurrentItem()))
    {
        push_onto_del(m_delList, *delItem);
    }
    else if (m_delList.size() >= 4)
    {
        delItem = FindProgramInUILists(
            m_delList[0].toUInt(), MythDate::fromString(m_delList[1]));
    }

    if (!delItem)
        return;

    uint other_delete_cnt = (m_delList.size() / 4) - 1;

    label += CreateProgramInfoString(*delItem);

    m_popupMenu = new MythMenu(label, this, "deletemenu");

    QString tmpmessage;
    const char *tmpslot = NULL;

    if ((kDeleteRecording == type) &&
        delItem->GetRecordingGroup() != "Deleted" &&
        delItem->GetRecordingGroup() != "LiveTV")
    {
        tmpmessage = tr("Yes, and allow re-record");
        tmpslot = SLOT(DeleteForgetHistory());
        m_popupMenu->AddItem(tmpmessage, tmpslot);
    }

    switch (type)
    {
        case kDeleteRecording:
            tmpmessage = tr("Yes, delete it");
            tmpslot = SLOT(Delete());
            break;
        case kForceDeleteRecording:
            tmpmessage = tr("Yes, delete it");
            tmpslot = SLOT(DeleteForce());
            break;
        case kStopRecording:
            tmpmessage = tr("Yes, stop recording");
            tmpslot = SLOT(StopSelected());
            break;
    }

    bool defaultIsYes =
        ((kDeleteRecording      != type) &&
         (kForceDeleteRecording != type) &&
         (delItem->QueryAutoExpire() != kDisableAutoExpire));

    m_popupMenu->AddItem(tmpmessage, tmpslot, NULL, defaultIsYes);

    if ((kForceDeleteRecording == type) && other_delete_cnt)
    {
        tmpmessage = tr("Yes, delete it and the remaining %1 list items")
            .arg(other_delete_cnt);
        tmpslot = SLOT(DeleteForceAllRemaining());
        m_popupMenu->AddItem(tmpmessage, tmpslot);
    }

    switch (type)
    {
        case kDeleteRecording:
        case kForceDeleteRecording:
            tmpmessage = tr("No, keep it");
            tmpslot = SLOT(DeleteIgnore());
            break;
        case kStopRecording:
            tmpmessage = tr("No, continue recording");
            tmpslot = SLOT(DeleteIgnore());
            break;
    }
    m_popupMenu->AddItem(tmpmessage, tmpslot, NULL, !defaultIsYes);

    if ((type == kForceDeleteRecording) && other_delete_cnt)
    {
        tmpmessage = tr("No, and keep the remaining %1 list items")
            .arg(other_delete_cnt);
        tmpslot = SLOT(DeleteIgnoreAllRemaining());
        m_popupMenu->AddItem(tmpmessage, tmpslot);
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
                                      _Location, msg +
                                 tr("This recording is currently in "
                                    "use by:") + "\n" + byWho);
            }
            else
            {
                ShowNotification(tr("Recording Available\n"),
                                      _Location, msg +
                                 tr("This recording is currently "
                                    "Available"));
            }
            break;
        case asPendingDelete:
            ShowNotificationError(tr("Recording Unavailable\n"),
                                  _Location, msg +
                                  tr("This recording is currently being "
                                     "deleted and is unavailable"));
            break;
        case asDeleted:
            ShowNotificationError(tr("Recording Unavailable\n"),
                                  _Location, msg +
                                  tr("This recording has been "
                                     "deleted and is unavailable"));
            break;
        case asFileNotFound:
            ShowNotificationError(tr("Recording Unavailable\n"),
                                  _Location, msg +
                                  tr("The file for this recording can "
                                     "not be found"));
            break;
        case asZeroByte:
            ShowNotificationError(tr("Recording Unavailable\n"),
                                  _Location, msg +
                                  tr("The file for this recording is "
                                     "empty."));
            break;
        case asNotYetAvailable:
            ShowNotificationError(tr("Recording Unavailable\n"),
                                  _Location, msg +
                                  tr("This recording is not yet "
                                     "available."));
    }
}

MythMenu* PlaybackBox::createPlaylistMenu(void)
{
    QString label = tr("There is %n item(s) in the playlist. Actions affect "
           "all items in the playlist", "", m_playList.size());

    MythMenu *menu = new MythMenu(label, this, "slotmenu");

    menu->AddItem(tr("Play"), SLOT(doPlayList()));
    menu->AddItem(tr("Shuffle Play"), SLOT(doPlayListRandom()));
    menu->AddItem(tr("Clear Playlist"), SLOT(doClearPlaylist()));

    if (GetFocusWidget() == m_groupList)
    {
        if ((m_viewMask & VIEW_TITLES))
            menu->AddItem(tr("Toggle playlist for this Category/Title"),
                          SLOT(togglePlayListTitle()));
        else
            menu->AddItem(tr("Toggle playlist for this Group"),
                          SLOT(togglePlayListTitle()));
    }
    else
        menu->AddItem(tr("Toggle playlist for this recording"),
                      SLOT(togglePlayListItem()));

    menu->AddItem(tr("Storage Options"), NULL, createPlaylistStorageMenu());
    menu->AddItem(tr("Job Options"), NULL, createPlaylistJobMenu());
    menu->AddItem(tr("Delete"), SLOT(PlaylistDelete()));
    menu->AddItem(tr("Delete, and allow re-record"),
                  SLOT(PlaylistDeleteForgetHistory()));

    return menu;
}

MythMenu* PlaybackBox::createPlaylistStorageMenu()
{
    QString label = tr("There is %n item(s) in the playlist. Actions affect "
           "all items in the playlist", "", m_playList.size());

    MythMenu *menu = new MythMenu(label, this, "slotmenu");

    menu->AddItem(tr("Change Recording Group"), SLOT(ShowRecGroupChangerUsePlaylist()));
    menu->AddItem(tr("Change Playback Group"), SLOT(ShowPlayGroupChangerUsePlaylist()));
    menu->AddItem(tr("Disable Auto Expire"), SLOT(doPlaylistExpireSetOff()));
    menu->AddItem(tr("Enable Auto Expire"), SLOT(doPlaylistExpireSetOn()));
    menu->AddItem(tr("Mark As Watched"), SLOT(doPlaylistWatchedSetOn()));
    menu->AddItem(tr("Mark As Unwatched"), SLOT(doPlaylistWatchedSetOff()));

    return menu;
}

MythMenu* PlaybackBox::createPlaylistJobMenu(void)
{
    QString label = tr("There is %n item(s) in the playlist. Actions affect "
           "all items in the playlist", "", m_playList.size());

    MythMenu *menu = new MythMenu(label, this, "slotmenu");

    QString jobTitle;
    QString command;
    QStringList::Iterator it;
    ProgramInfo *tmpItem;
    bool isTranscoding = true;
    bool isFlagging = true;
    bool isMetadataLookup = true;
    bool isRunningUserJob1 = true;
    bool isRunningUserJob2 = true;
    bool isRunningUserJob3 = true;
    bool isRunningUserJob4 = true;

    for(it = m_playList.begin(); it != m_playList.end(); ++it)
    {
        tmpItem = FindProgramInUILists(*it);
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
        menu->AddItem(tr("Begin Transcoding"), SLOT(doPlaylistBeginTranscoding()));
    else
        menu->AddItem(tr("Stop Transcoding"), SLOT(stopPlaylistTranscoding()));

    if (!isFlagging)
        menu->AddItem(tr("Begin Commercial Detection"), SLOT(doPlaylistBeginFlagging()));
    else
        menu->AddItem(tr("Stop Commercial Detection"), SLOT(stopPlaylistFlagging()));

    if (!isMetadataLookup)
        menu->AddItem(tr("Begin Metadata Lookup"), SLOT(doPlaylistBeginLookup()));
    else
        menu->AddItem(tr("Stop Metadata Lookup"), SLOT(stopPlaylistLookup()));

    command = gCoreContext->GetSetting("UserJob1", "");
    if (!command.isEmpty())
    {
        jobTitle = gCoreContext->GetSetting("UserJobDesc1");

        if (!isRunningUserJob1)
            menu->AddItem(tr("Begin") + ' ' + jobTitle,
                          SLOT(doPlaylistBeginUserJob1()));
        else
            menu->AddItem(tr("Stop") + ' ' + jobTitle,
                          SLOT(stopPlaylistUserJob1()));
    }

    command = gCoreContext->GetSetting("UserJob2", "");
    if (!command.isEmpty())
    {
        jobTitle = gCoreContext->GetSetting("UserJobDesc2");

        if (!isRunningUserJob2)
            menu->AddItem(tr("Begin") + ' ' + jobTitle,
                          SLOT(doPlaylistBeginUserJob2()));
        else
            menu->AddItem(tr("Stop") + ' ' + jobTitle,
                          SLOT(stopPlaylistUserJob2()));
    }

    command = gCoreContext->GetSetting("UserJob3", "");
    if (!command.isEmpty())
    {
        jobTitle = gCoreContext->GetSetting("UserJobDesc3");

        if (!isRunningUserJob3)
            menu->AddItem(tr("Begin") + ' ' + jobTitle,
                          SLOT(doPlaylistBeginUserJob3()));
        else
            menu->AddItem(tr("Stop") + ' ' + jobTitle,
                          SLOT(stopPlaylistUserJob3()));
    }

    command = gCoreContext->GetSetting("UserJob4", "");
    if (!command.isEmpty())
    {
        jobTitle = gCoreContext->GetSetting("UserJobDesc4");

        if (!isRunningUserJob4)
            menu->AddItem(QString("%1 %2").arg(tr("Begin")).arg(jobTitle),
                          SLOT(doPlaylistBeginUserJob4()));
        else
            menu->AddItem(QString("%1 %2").arg(tr("Stop")).arg(jobTitle),
                          SLOT(stopPlaylistUserJob4()));
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
        connect(m_menuDialog, SIGNAL(Closed(QString,int)), SLOT(popupClosed(QString,int)));
    }
    else
        delete m_menuDialog;
}

void PlaybackBox::ShowMenu()
{
    if (m_menuDialog)
        return;

    if (GetFocusWidget() == m_groupList)
        ShowGroupPopup();
    else
    {
        ProgramInfo *pginfo = CurrentItem();
        if (pginfo)
        {
            m_helper.CheckAvailability(
                *pginfo, kCheckForMenuAction);

            if ((asPendingDelete == pginfo->GetAvailableStatus()) ||
                (asPendingDelete == pginfo->GetAvailableStatus()))
            {
                ShowAvailabilityPopup(*pginfo);
            }
            else
            {
                ShowActionPopup(*pginfo);
            }
        }
        else
            ShowGroupPopup();
    }
}

MythMenu* PlaybackBox::createPlayFromMenu()
{
    ProgramInfo *pginfo = CurrentItem();
    if (!pginfo)
        return NULL;

    QString title = tr("Play Options") + CreateProgramInfoString(*pginfo);

    MythMenu *menu = new MythMenu(title, this, "slotmenu");

    menu->AddItem(tr("Play from bookmark"),  SLOT(PlayFromBookmark()));
    menu->AddItem(tr("Play from beginning"), SLOT(PlayFromBeginning()));

    return menu;
}

MythMenu* PlaybackBox::createStorageMenu()
{
    ProgramInfo *pginfo = CurrentItem();
    if (!pginfo)
        return NULL;

    QString title = tr("Storage Options") + CreateProgramInfoString(*pginfo);
    QString autoExpireText = (pginfo->IsAutoExpirable()) ?
        tr("Disable Auto Expire") : tr("Enable Auto Expire");
    QString preserveText = (pginfo->IsPreserved()) ?
        tr("Do not preserve this episode") : tr("Preserve this episode");

    MythMenu *menu = new MythMenu(title, this, "slotmenu");
    menu->AddItem(tr("Change Recording Group"), SLOT(ShowRecGroupChanger()));
    menu->AddItem(tr("Change Playback Group"), SLOT(ShowPlayGroupChanger()));
    menu->AddItem(autoExpireText, SLOT(toggleAutoExpire()));
    menu->AddItem(preserveText, SLOT(togglePreserveEpisode()));

    return menu;
}

MythMenu* PlaybackBox::createRecordingMenu(void)
{
    ProgramInfo *pginfo = CurrentItem();
    if (!pginfo)
        return NULL;

    QString title = tr("Scheduling Options") + CreateProgramInfoString(*pginfo);

    MythMenu *menu = new MythMenu(title, this, "slotmenu");

    menu->AddItem(tr("Edit Recording Schedule"), SLOT(doEditScheduled()));

    menu->AddItem(tr("Allow this episode to re-record"), SLOT(doAllowRerecord()));

    menu->AddItem(tr("Show Recording Details"), SLOT(showProgramDetails()));

    menu->AddItem(tr("Change Recording Metadata"), SLOT(showMetadataEditor()));

    menu->AddItem(tr("Custom Edit"), SLOT(customEdit()));

    return menu;
}

MythMenu* PlaybackBox::createJobMenu()
{
    ProgramInfo *pginfo = CurrentItem();
    if (!pginfo)
        return NULL;

    QString title = tr("Job Options") + CreateProgramInfoString(*pginfo);

    MythMenu *menu = new MythMenu(title, this, "slotmenu");

    QString jobTitle;
    QString command;

    bool add[7] =
    {
        true,
        true,
        true,
        !gCoreContext->GetSetting("UserJob1", "").isEmpty(),
        !gCoreContext->GetSetting("UserJob2", "").isEmpty(),
        !gCoreContext->GetSetting("UserJob3", "").isEmpty(),
        !gCoreContext->GetSetting("UserJob4", "").isEmpty(),
    };
    int jobs[7] =
    {
        JOB_TRANSCODE,
        JOB_COMMFLAG,
        JOB_METADATA,
        JOB_USERJOB1,
        JOB_USERJOB2,
        JOB_USERJOB3,
        JOB_USERJOB4,
    };
    QString desc[14] =
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
    const char *myslots[14] =
    {   // stop                         start
        SLOT(doBeginTranscoding()),     SLOT(createTranscodingProfilesMenu()),
        SLOT(doBeginFlagging()),        SLOT(doBeginFlagging()),
        SLOT(doBeginLookup()),          SLOT(doBeginLookup()),
        SLOT(doBeginUserJob1()),        SLOT(doBeginUserJob1()),
        SLOT(doBeginUserJob2()),        SLOT(doBeginUserJob2()),
        SLOT(doBeginUserJob3()),        SLOT(doBeginUserJob3()),
        SLOT(doBeginUserJob4()),        SLOT(doBeginUserJob4()),
    };

    for (uint i = 0; i < sizeof(add) / sizeof(bool); i++)
    {
        if (!add[i])
            continue;

        QString stop_desc  = desc[i*2+0];
        QString start_desc = desc[i*2+1];

        if (start_desc.toUInt())
        {
            QString jobTitle = gCoreContext->GetSetting(
                "UserJobDesc"+start_desc, tr("User Job") + " #" + start_desc);
            stop_desc  = tr("Stop")  + ' ' + jobTitle;
            start_desc = tr("Begin") + ' ' + jobTitle;
        }

        bool running = JobQueue::IsJobQueuedOrRunning(
            jobs[i], pginfo->GetChanID(), pginfo->GetRecordingStartTime());

        const char *slot = myslots[i * 2 + (running ? 0 : 1)];
        MythMenu *submenu = (slot == myslots[1] ? createTranscodingProfilesMenu() : NULL);

        menu->AddItem((running) ? stop_desc : start_desc, slot, submenu);
    }

    return menu;
}

MythMenu* PlaybackBox::createTranscodingProfilesMenu()
{
    QString label = tr("Transcoding profiles");

    MythMenu *menu = new MythMenu(label, this, "transcode");

    menu->AddItem(tr("Default"), qVariantFromValue(-1));
    menu->AddItem(tr("Autodetect"), qVariantFromValue(0));

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
        return NULL;
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

        menu->AddItem(transcoder_name, qVariantFromValue(transcoder_id));
    }

    return menu;
}

void PlaybackBox::changeProfileAndTranscode(int id)
{
    ProgramInfo *pginfo = CurrentItem();

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
    QString label =
        (asFileNotFound == pginfo.GetAvailableStatus()) ?
        tr("Recording file cannot be found") :
        (asZeroByte     == pginfo.GetAvailableStatus()) ?
        tr("Recording file contains no data") :
        tr("Recording Options");

    m_popupMenu = new MythMenu(label + CreateProgramInfoString(pginfo), this, "actionmenu");

    if ((asFileNotFound  == pginfo.GetAvailableStatus()) ||
        (asZeroByte      == pginfo.GetAvailableStatus()))
    {
        m_popupMenu->AddItem(tr("Show Recording Details"), SLOT(showProgramDetails()));
        m_popupMenu->AddItem(tr("Delete"), SLOT(askDelete()));

        if (m_playList.filter(pginfo.MakeUniqueKey()).size())
        {
            m_popupMenu->AddItem(tr("Remove from Playlist"), SLOT(togglePlayListItem()));
        }
        else
        {
            m_popupMenu->AddItem(tr("Add to Playlist"), SLOT(togglePlayListItem()));
        }

        DisplayPopupMenu();

        return;
    }

    bool sameProgram = false;

    if (m_player)
        sameProgram = m_player->IsSameProgram(0, &pginfo);

    TVState tvstate = kState_None;

    if (!sameProgram)
    {
        if (pginfo.IsBookmarkSet())
            m_popupMenu->AddItem(tr("Play from..."), NULL, createPlayFromMenu());
        else
            m_popupMenu->AddItem(tr("Play"), SLOT(PlayFromBookmark()));
    }

    if (!m_player)
    {
        if (m_playList.filter(pginfo.MakeUniqueKey()).size())
            m_popupMenu->AddItem(tr("Remove from Playlist"),
                                 SLOT(togglePlayListItem()));
        else
            m_popupMenu->AddItem(tr("Add to Playlist"),
                                 SLOT(togglePlayListItem()));
        if (m_playList.size())
        {
            m_popupMenu->AddItem(tr("Playlist options"), NULL, createPlaylistMenu());
        }
    }

    if (pginfo.GetRecordingStatus() == rsRecording &&
        (!(sameProgram &&
           (tvstate == kState_WatchingLiveTV ||
            tvstate == kState_WatchingRecording))))
    {
        m_popupMenu->AddItem(tr("Stop Recording"), SLOT(askStop()));
    }

    if (pginfo.IsWatched())
        m_popupMenu->AddItem(tr("Mark as Unwatched"), SLOT(toggleWatched()));
    else
        m_popupMenu->AddItem(tr("Mark as Watched"), SLOT(toggleWatched()));

    m_popupMenu->AddItem(tr("Storage Options"), NULL, createStorageMenu());
    m_popupMenu->AddItem(tr("Recording Options"), NULL, createRecordingMenu());
    m_popupMenu->AddItem(tr("Job Options"), NULL, createJobMenu());

    if (!sameProgram)
    {
        if (pginfo.GetRecordingGroup() == "Deleted")
        {
            push_onto_del(m_delList, pginfo);
            m_popupMenu->AddItem(tr("Undelete"), SLOT(Undelete()));
            m_popupMenu->AddItem(tr("Delete Forever"), SLOT(Delete()));
        }
        else
        {
            m_popupMenu->AddItem(tr("Delete"), SLOT(askDelete()));
        }
    }

    DisplayPopupMenu();
}

QString PlaybackBox::CreateProgramInfoString(const ProgramInfo &pginfo) const
{
    QDateTime recstartts = pginfo.GetRecordingStartTime();
    QDateTime recendts   = pginfo.GetRecordingEndTime();

    QString timedate = QString("%1 - %2")
        .arg(MythDate::toString(
                 recstartts, MythDate::kDateTimeFull | MythDate::kSimplify))
        .arg(MythDate::toString(recendts, MythDate::kTime));

    QString title = pginfo.GetTitle();

    QString extra;

    if (!pginfo.GetSubtitle().isEmpty())
    {
        extra = QString('\n') + pginfo.GetSubtitle();
        int maxll = max(title.length(), 20);
        if (extra.length() > maxll)
            extra = extra.left(maxll - 3) + "...";
    }

    return QString("\n%1%2\n%3").arg(title).arg(extra).arg(timedate);
}

void PlaybackBox::doClearPlaylist(void)
{
    QStringList::Iterator it;
    for (it = m_playList.begin(); it != m_playList.end(); ++it)
    {
        ProgramInfo *tmpItem = FindProgramInUILists(*it);

        if (!tmpItem)
            continue;

        MythUIButtonListItem *item =
            m_recordingList->GetItemByData(qVariantFromValue(tmpItem));

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
    ProgramInfo *pginfo = CurrentItem();
    if (pginfo)
    {
        push_onto_del(m_delList, *pginfo);
        ShowDeletePopup(kStopRecording);
    }
}

void PlaybackBox::showProgramDetails()
{
    ProgramInfo *pginfo = CurrentItem();
    if (pginfo)
        ShowDetails(pginfo);
}

void PlaybackBox::doEditScheduled()
{
    ProgramInfo *pginfo = CurrentItem();
    if (pginfo)
        EditScheduled(pginfo);
}

/**
 *  \brief Callback function when Allow Re-record is pressed in Watch Recordings
 *
 * Hide the current program from the scheduler by calling ForgetHistory
 * This will allow it to re-record without deleting
 */
void PlaybackBox::doAllowRerecord()
{
   ProgramInfo *pginfo = CurrentItem();

    if (!pginfo)
        return;

    RecordingInfo ri(*pginfo);
    ri.ForgetHistory();
    *pginfo = ri;
}

void PlaybackBox::doJobQueueJob(int jobType, int jobFlags)
{
   ProgramInfo *pginfo = CurrentItem();

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
        if (gCoreContext->GetNumSetting("JobsRunOnRecordHost", 0))
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
    ProgramInfo *tmpItem;
    QStringList::Iterator it;

    for (it = m_playList.begin(); it != m_playList.end(); ++it)
    {
        tmpItem = FindProgramInUILists(*it);
        if (tmpItem &&
            (!JobQueue::IsJobQueuedOrRunning(
                jobType,
                tmpItem->GetChanID(), tmpItem->GetRecordingStartTime())))
    {
            QString jobHost;
            if (gCoreContext->GetNumSetting("JobsRunOnRecordHost", 0))
                jobHost = tmpItem->GetHostname();

            JobQueue::QueueJob(jobType, tmpItem->GetChanID(),
                               tmpItem->GetRecordingStartTime(),
                               "", "", jobHost, jobFlags);
        }
    }
}

void PlaybackBox::stopPlaylistJobQueueJob(int jobType)
{
    ProgramInfo *tmpItem;
    QStringList::Iterator it;

    for (it = m_playList.begin(); it != m_playList.end(); ++it)
    {
        tmpItem = FindProgramInUILists(*it);
        if (tmpItem &&
            (JobQueue::IsJobQueuedOrRunning(
                jobType,
                tmpItem->GetChanID(), tmpItem->GetRecordingStartTime())))
        {
            JobQueue::ChangeJobCmds(
                jobType, tmpItem->GetChanID(),
                tmpItem->GetRecordingStartTime(), JOB_STOP);

            if ((jobType & JOB_COMMFLAG) && (tmpItem))
            {
                tmpItem->SetEditing(false);
                tmpItem->SetFlagging(false);
            }
        }
    }
}

void PlaybackBox::askDelete()
{
    ProgramInfo *pginfo = CurrentItem();
    if (pginfo)
    {
        push_onto_del(m_delList, *pginfo);
        ShowDeletePopup(kDeleteRecording);
    }
}

void PlaybackBox::PlaylistDelete(bool forgetHistory)
{
    QString forceDeleteStr("0");

    QStringList::const_iterator it;
    QStringList list;
    for (it = m_playList.begin(); it != m_playList.end(); ++it)
    {
        ProgramInfo *tmpItem = FindProgramInUILists(*it);
        if (tmpItem && tmpItem->QueryIsDeleteCandidate())
        {
            tmpItem->SetAvailableStatus(asPendingDelete, "PlaylistDelete");
            list.push_back(QString::number(tmpItem->GetChanID()));
            list.push_back(tmpItem->GetRecordingStartTime(MythDate::ISODate));
            list.push_back(forceDeleteStr);
            list.push_back(forgetHistory ? "1" : "0");

            // if the item is in the current recording list UI then delete it.
            MythUIButtonListItem *uiItem =
                m_recordingList->GetItemByData(qVariantFromValue(tmpItem));
            if (uiItem)
                m_recordingList->RemoveItem(uiItem);
        }
    }
    m_playList.clear();

    if (!list.empty())
        m_helper.DeleteRecordings(list);

    doClearPlaylist();
}

void PlaybackBox::Undelete(void)
{
    uint chanid;
    QDateTime recstartts;
    if (extract_one_del(m_delList, chanid, recstartts))
        m_helper.UndeleteRecording(chanid, recstartts);
}

void PlaybackBox::Delete(DeleteFlags flags)
{
    uint chanid;
    QDateTime recstartts;
    while (extract_one_del(m_delList, chanid, recstartts))
    {
        if (flags & kIgnore)
            continue;

        RemoveProgram(chanid, recstartts,
                      flags & kForgetHistory, flags & kForce);

        if (!(flags & kAllRemaining))
            break;
    }

    if (!m_delList.empty())
    {
        MythEvent *e = new MythEvent("DELETE_FAILURES", m_delList);
        m_delList.clear();
        QCoreApplication::postEvent(this, e);
    }
}

ProgramInfo *PlaybackBox::FindProgramInUILists(const ProgramInfo &pginfo)
{
    return FindProgramInUILists(
        pginfo.GetChanID(), pginfo.GetRecordingStartTime(),
        pginfo.GetRecordingGroup());
}

/// Extracts chanid and recstartts from a string constructed by
/// ProgramInfo::MakeUniqueKey() returns the matching program info
/// from the UI program info lists.
ProgramInfo *PlaybackBox::FindProgramInUILists(const QString &key)
{
    uint chanid;
    QDateTime recstartts;
    if (ProgramInfo::ExtractKey(key, chanid, recstartts))
        return FindProgramInUILists(chanid, recstartts);

    LOG(VB_GENERAL, LOG_ERR, LOC +
        QString("FindProgramInUILists(%1) called with invalid key").arg(key));

    return NULL;
}

ProgramInfo *PlaybackBox::FindProgramInUILists(
    uint chanid, const QDateTime &recstartts,
    QString recgroup)
{
    // LiveTV ProgramInfo's are not in the aggregated list
    ProgramList::iterator _it[2] = {
        m_progLists[tr("Live TV").toLower()].begin(), m_progLists[""].begin() };
    ProgramList::iterator _end[2] = {
        m_progLists[tr("Live TV").toLower()].end(),   m_progLists[""].end()   };

    if (recgroup != "LiveTV")
    {
        swap( _it[0],  _it[1]);
        swap(_end[0], _end[1]);
    }

    for (uint i = 0; i < 2; i++)
    {
        ProgramList::iterator it = _it[i], end = _end[i];
        for (; it != end; ++it)
        {
            if ((*it)->GetRecordingStartTime() == recstartts &&
                (*it)->GetChanID() == chanid)
            {
                return *it;
            }
        }
    }

    return NULL;
}

void PlaybackBox::toggleWatched(void)
{
    MythUIButtonListItem *item = m_recordingList->GetItemCurrent();

    if (!item)
        return;

    ProgramInfo *pginfo = qVariantValue<ProgramInfo *>(item->GetData());

    if (!pginfo)
        return;

    if (pginfo)
    {
        bool on = !pginfo->IsWatched();
        pginfo->SaveWatched(on);
        item->DisplayState((on)?"yes":"on", "watched");
        updateIcons(pginfo);

        // A refill affects the responsiveness of the UI and we only
        // need to rebuild the list if the watch list is displayed
        if (m_viewMask & VIEW_WATCHLIST)
            UpdateUILists();
    }
}

void PlaybackBox::toggleAutoExpire()
{
    MythUIButtonListItem *item = m_recordingList->GetItemCurrent();

    if (!item)
        return;

    ProgramInfo *pginfo = qVariantValue<ProgramInfo *>(item->GetData());

    if (!pginfo)
        return;

    if (pginfo)
    {
        bool on = !pginfo->IsAutoExpirable();
        pginfo->SaveAutoExpire(
            (on) ? kNormalAutoExpire : kDisableAutoExpire, true);
        item->DisplayState((on)?"yes":"no", "autoexpire");
        updateIcons(pginfo);
    }
}

void PlaybackBox::togglePreserveEpisode()
{
    MythUIButtonListItem *item = m_recordingList->GetItemCurrent();

    if (!item)
        return;

    ProgramInfo *pginfo = qVariantValue<ProgramInfo *>(item->GetData());

    if (!pginfo)
        return;

    if (pginfo)
    {
        bool on = !pginfo->IsPreserved();
        pginfo->SavePreserve(on);
        item->DisplayState(on?"yes":"no", "preserve");
        updateIcons(pginfo);
    }
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

    ProgramList::iterator it = m_progLists[groupname].begin();
    ProgramList::iterator end = m_progLists[groupname].end();
    for (; it != end; ++it)
    {
        if (*it && ((*it)->GetAvailableStatus() == asAvailable))
            togglePlayListItem(*it);
    }
}

void PlaybackBox::togglePlayListItem(void)
{
    MythUIButtonListItem *item = m_recordingList->GetItemCurrent();

    if (!item)
        return;

    ProgramInfo *pginfo = qVariantValue<ProgramInfo *>(item->GetData());

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

    QString key = pginfo->MakeUniqueKey();

    MythUIButtonListItem *item =
                    m_recordingList->GetItemByData(qVariantFromValue(pginfo));

    if (m_playList.filter(key).size())
    {
        if (item)
            item->DisplayState("no", "playlist");

        QStringList tmpList;
        QStringList::Iterator it;

        tmpList = m_playList;
        m_playList.clear();

        for (it = tmpList.begin(); it != tmpList.end(); ++it )
        {
            if (*it != key)
                m_playList << *it;
        }
    }
    else
    {
        if (item)
          item->DisplayState("yes", "playlist");
        m_playList << key;
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
                    .arg(tokens[1]).arg(tokens[3]).arg(tokens[4]));

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

                bool ignoreBookmark = (tokens[1] == "PLAY");
                PlayX(pginfo, ignoreBookmark, true);
            }
            else
            {
                QString message = QString("NETWORK_CONTROL RESPONSE %1 "
                                          "ERROR: Could not find recording for "
                                          "chanid %2 @ %3").arg(clientID)
                                          .arg(tokens[3]).arg(tokens[4]);
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

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("TV Frontend",
                                                     event, actions);

    for (int i = 0; i < actions.size() && !handled; ++i)
    {
        QString action = actions[i];
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
        else if (action == "CHANGERECGROUP")
            showGroupFilter();
        else if (action == "CHANGEGROUPVIEW")
            showViewChanger();
        else if (action == "EDIT")
            doEditScheduled();
        else if (m_titleList.size() > 1)
        {
            if (action == "DELETE")
                deleteSelected(m_recordingList->GetItemCurrent());
            else if (action == ACTION_PLAYBACK)
                PlayFromBookmark();
            else if (action == "DETAILS" || action == "INFO")
                details();
            else if (action == "CUSTOMEDIT")
                customEdit();
            else if (action == "UPCOMING")
                upcoming();
            else if (action == ACTION_VIEWSCHEDULED)
                upcomingScheduled();
            else if (action == ACTION_PREVRECORDED)
                previous();
            else
                handled = false;
        }
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void PlaybackBox::customEvent(QEvent *event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = dynamic_cast<DialogCompletionEvent*>(event);

        if (!dce)
            return;

        QString resultid = dce->GetId();

        if (resultid == "transcode" && dce->GetResult() >= 0)
            changeProfileAndTranscode(dce->GetData().toInt());
    }
    else if ((MythEvent::Type)(event->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)event;
        QString message = me->Message();

        if (message.startsWith("RECORDING_LIST_CHANGE"))
        {
            QStringList tokens = message.simplified().split(" ");
            uint chanid = 0;
            QDateTime recstartts;
            if (tokens.size() >= 4)
            {
                chanid = tokens[2].toUInt();
                recstartts = MythDate::fromString(tokens[3]);
            }

            if ((tokens.size() >= 2) && tokens[1] == "UPDATE")
            {
                ProgramInfo evinfo(me->ExtraDataList());
                if (evinfo.HasPathname() || evinfo.GetChanID())
                    HandleUpdateProgramInfoEvent(evinfo);
            }
            else if (chanid && recstartts.isValid() && (tokens[1] == "ADD"))
            {
                ProgramInfo evinfo(chanid, recstartts);
                if (evinfo.GetChanID())
                {
                    evinfo.SetRecordingStatus(rsRecording);
                    HandleRecordingAddEvent(evinfo);
                }
            }
            else if (chanid && recstartts.isValid() && (tokens[1] == "DELETE"))
            {
                if (chanid && recstartts.isValid())
                    HandleRecordingRemoveEvent(chanid, recstartts);
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
                QKeyEvent *keyevent;
                Qt::KeyboardModifiers modifiers =
                    Qt::ShiftModifier |
                    Qt::ControlModifier |
                    Qt::AltModifier |
                    Qt::MetaModifier |
                    Qt::KeypadModifier;
                keyevent = new QKeyEvent(QEvent::KeyPress,
                                         Qt::Key_LaunchMedia, modifiers);
                QCoreApplication::postEvent((QObject*)(GetMythMainWindow()),
                                            keyevent);

                keyevent = new QKeyEvent(QEvent::KeyRelease,
                                         Qt::Key_LaunchMedia, modifiers);
                QCoreApplication::postEvent((QObject*)(GetMythMainWindow()),
                                            keyevent);
            }
        }
        else if (message.startsWith("UPDATE_FILE_SIZE"))
        {
            QStringList tokens = message.simplified().split(" ");
            bool ok = false;
            uint chanid = 0;
            QDateTime recstartts;
            uint64_t filesize = 0ULL;
            if (tokens.size() >= 4)
            {
                chanid     = tokens[1].toUInt();
                recstartts = MythDate::fromString(tokens[2]);
                filesize   = tokens[3].toLongLong(&ok);
            }
            if (chanid && recstartts.isValid() && ok)
            {
                HandleUpdateProgramInfoFileSizeEvent(
                    chanid, recstartts, filesize);
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
            for (uint i = 0; i+3 < (uint)me->ExtraDataList().size(); i+=4)
            {
                ProgramInfo *pginfo = m_programInfoCache.GetProgramInfo(
                        me->ExtraDataList()[i+0].toUInt(),
                        MythDate::fromString(me->ExtraDataList()[i+1]));

                if (!pginfo)
                    continue;

                QString forceDeleteStr = me->ExtraDataList()[i+2];
                QString forgetHistoryStr = me->ExtraDataList()[i+3];

                list.push_back(QString::number(pginfo->GetChanID()));
                list.push_back(
                    pginfo->GetRecordingStartTime(MythDate::ISODate));
                list.push_back(forceDeleteStr);
                list.push_back(forgetHistoryStr);
                pginfo->SetAvailableStatus(asPendingDelete,
                                           "LOCAL_PBB_DELETE_RECORDINGS");

                // if the item is in the current recording list UI
                // then delete it.
                MythUIButtonListItem *uiItem =
                    m_recordingList->GetItemByData(qVariantFromValue(pginfo));
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
            if (me->ExtraDataList().size() < 4)
                return;

            for (uint i = 0; i+3 < (uint)me->ExtraDataList().size(); i += 4)
            {
                ProgramInfo *pginfo = m_programInfoCache.GetProgramInfo(
                        me->ExtraDataList()[i+0].toUInt(),
                        MythDate::fromString(me->ExtraDataList()[i+1]));
                if (pginfo)
                {
                    pginfo->SetAvailableStatus(asAvailable, "DELETE_FAILURES");
                    m_helper.CheckAvailability(*pginfo, kCheckForCache);
                }
            }

            bool forceDelete = me->ExtraDataList()[2].toUInt();
            if (!forceDelete)
            {
                m_delList = me->ExtraDataList();
                if (!m_menuDialog)
                {
                    ShowDeletePopup(kForceDeleteRecording);
                    return;
                }
                else
                {
                    LOG(VB_GENERAL, LOG_WARNING, LOC +
                        "Delete failures not handled due to "
                        "pre-existing popup.");
                }
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
                QString token = me->ExtraData(i);
                QSet<QString>::iterator it = m_preview_tokens.find(token);
                if (it != m_preview_tokens.end())
                    m_preview_tokens.erase(it);
            }
        }
        else if (message == "AVAILABILITY" && me->ExtraDataCount() == 8)
        {
            const uint kMaxUIWaitTime = 10000; // ms
            QStringList list = me->ExtraDataList();
            QString key = list[0];
            CheckAvailabilityType cat =
                (CheckAvailabilityType) list[1].toInt();
            AvailableStatusType availableStatus =
                (AvailableStatusType) list[2].toInt();
            uint64_t fs = list[3].toULongLong();
            QTime tm;
            tm.setHMS(list[4].toUInt(), list[5].toUInt(),
                      list[6].toUInt(), list[7].toUInt());
            QTime now = QTime::currentTime();
            int time_elapsed = tm.msecsTo(now);
            if (time_elapsed < 0)
                time_elapsed += 24 * 60 * 60 * 1000;

            AvailableStatusType old_avail = availableStatus;
            ProgramInfo *pginfo = FindProgramInUILists(key);
            if (pginfo)
            {
                pginfo->SetFilesize(max(pginfo->GetFilesize(), fs));
                old_avail = pginfo->GetAvailableStatus();
                pginfo->SetAvailableStatus(availableStatus, "AVAILABILITY");
            }

            if ((uint)time_elapsed >= kMaxUIWaitTime)
                m_playListPlay.clear();

            bool playnext = ((kCheckForPlaylistAction == cat) &&
                             !m_playListPlay.empty());


            if (((kCheckForPlayAction     == cat) ||
                 (kCheckForPlaylistAction == cat)) &&
                ((uint)time_elapsed < kMaxUIWaitTime))
            {
                if (asAvailable != availableStatus)
                {
                    if (kCheckForPlayAction == cat && pginfo)
                        ShowAvailabilityPopup(*pginfo);
                }
                else if (pginfo)
                {
                    playnext = false;
                    Play(*pginfo, kCheckForPlaylistAction == cat, false, false);
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
            QString key = m_playListPlay.front();
            m_playListPlay.pop_front();

            if (!m_playListPlay.empty())
            {
                const ProgramInfo *pginfo =
                    FindProgramInUILists(m_playListPlay.front());
                if (pginfo)
                    m_helper.CheckAvailability(*pginfo, kCheckForCache);
            }

            ProgramInfo *pginfo = FindProgramInUILists(key);
            if (pginfo)
                Play(*pginfo, true, false, false);
        }
        else if ((message == "SET_PLAYBACK_URL") && (me->ExtraDataCount() == 2))
        {
            QString piKey = me->ExtraData(0);
            ProgramInfo *info = m_programInfoCache.GetProgramInfo(piKey);
            if (info)
                info->SetPathname(me->ExtraData(1));
        }
        else if ((message == "FOUND_ARTWORK") && (me->ExtraDataCount() >= 5))
        {
            VideoArtworkType type      = (VideoArtworkType) me->ExtraData(2).toInt();
            QString          pikey     = me->ExtraData(3);
            QString          group     = me->ExtraData(4);
            QString          fn        = me->ExtraData(5);

            if (!pikey.isEmpty())
            {
                ProgramInfo *pginfo = m_programInfoCache.GetProgramInfo(pikey);
                if (pginfo &&
                    m_recordingList->GetItemByData(qVariantFromValue(pginfo)) ==
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
        else if ((message == "DISPLAY_RECGROUP") &&
                 (me->ExtraDataCount() >= 1))
        {
            QString recGroup = me->ExtraData(0);
            displayRecGroup(recGroup);
        }
    }
    else
        ScheduleCommon::customEvent(event);
}

void PlaybackBox::HandleRecordingRemoveEvent(
    uint chanid, const QDateTime &recstartts)
{
    if (!m_programInfoCache.Remove(chanid, recstartts))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("Failed to remove %1:%2, reloading list")
                .arg(chanid).arg(recstartts.toString(Qt::ISODate)));
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
        ProgramList::iterator pit = (*git).begin();
        while (pit != (*git).end())
        {
            if ((*pit)->GetChanID()             == chanid &&
                (*pit)->GetRecordingStartTime() == recstartts)
            {
                if (!git.key().isEmpty() && git.key() == groupname)
                {
                    MythUIButtonListItem *item_by_data =
                        m_recordingList->GetItemByData(
                            qVariantFromValue(*pit));
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

void PlaybackBox::HandleUpdateProgramInfoEvent(const ProgramInfo &evinfo)
{
    QString old_recgroup = m_programInfoCache.GetRecGroup(
        evinfo.GetChanID(), evinfo.GetRecordingStartTime());

    if (!m_programInfoCache.Update(evinfo))
        return;

    // If the recording group has changed, reload lists from the recently
    // updated cache; if not, only update UI for the updated item
    if (evinfo.GetRecordingGroup() == old_recgroup)
    {
        ProgramInfo *dst = FindProgramInUILists(evinfo);
        if (dst)
            UpdateUIListItem(dst, true);
        return;
    }

    ScheduleUpdateUIList();
}

void PlaybackBox::HandleUpdateProgramInfoFileSizeEvent(
    uint chanid, const QDateTime &recstartts, uint64_t filesize)
{
    m_programInfoCache.UpdateFileSize(chanid, recstartts, filesize);

    ProgramInfo *dst = FindProgramInUILists(chanid, recstartts);
    if (dst)
        UpdateUIListItem(dst, false);
}

void PlaybackBox::ScheduleUpdateUIList(void)
{
    if (!m_programInfoCache.IsLoadInProgress())
        QCoreApplication::postEvent(this, new MythEvent("UPDATE_UI_LIST"));
}

void PlaybackBox::showIconHelp(void)
{
    HelpPopup *helpPopup = new HelpPopup(m_popupStack);

    if (helpPopup->Create())
        m_popupStack->AddScreen(helpPopup);
    else
        delete helpPopup;
}

void PlaybackBox::showViewChanger(void)
{
    ChangeView *viewPopup = new ChangeView(m_popupStack, this, m_viewMask);

    if (viewPopup->Create())
    {
        connect(viewPopup, SIGNAL(save()), SLOT(saveViewChanges()));
        m_popupStack->AddScreen(viewPopup);
    }
    else
        delete viewPopup;
}

void PlaybackBox::saveViewChanges()
{
    if (m_viewMask == VIEW_NONE)
        m_viewMask = VIEW_TITLES;
    gCoreContext->SaveSetting("DisplayGroupDefaultViewMask", (int)m_viewMask);
    gCoreContext->SaveSetting("PlaybackWatchList",
                                            (bool)(m_viewMask & VIEW_WATCHLIST));
}

void PlaybackBox::showGroupFilter(void)
{
    QString dispGroup = ProgramInfo::i18n(m_recGroup);

    QStringList groupNames;
    QStringList displayNames;
    QStringList groups;
    QStringList displayGroups;

    MSqlQuery query(MSqlQuery::InitCon());

    m_recGroupType.clear();

    uint items = 0;
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
            dispGroup = query.value(0).toString();
            items     = query.value(1).toInt();

            if ((dispGroup != "LiveTV" || (m_viewMask & VIEW_LIVETVGRP)) &&
                (dispGroup != "Deleted"))
                totalItems += items;

            groupNames.append(dispGroup);

            dispGroup = (dispGroup == "Default") ? tr("Default") : dispGroup;
            dispGroup = (dispGroup == "Deleted") ? tr("Deleted") : dispGroup;
            dispGroup = (dispGroup == "LiveTV")  ? tr("Live TV")  : dispGroup;

            displayNames.append(tr("%1 [%n item(s)]", 0, items).arg(dispGroup));

            m_recGroupType[query.value(0).toString()] = "recgroup";
        }
    }

    // Create and add the "All Programs" entry
    displayNames.push_front(tr("%1 [%n item(s)]", 0, totalItems)
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
            items     = query.value(1).toInt();
            dispGroup = query.value(0).toString();
            if (dispGroup.isEmpty())
            {
                unknownCount += items;
                dispGroup = tr("Unknown");
            }
            else if (dispGroup == tr("Unknown"))
                unknownCount += items;

            if ((!m_recGroupType.contains(dispGroup)) &&
                (dispGroup != tr("Unknown")))
            {
                displayGroups += tr("%1 [%n item(s)]", 0, items).arg(dispGroup);
                groups += dispGroup;

                m_recGroupType[dispGroup] = "category";
            }
        }

        if (unknownCount > 0)
        {
            dispGroup = tr("Unknown");
            items     = unknownCount;
            displayGroups += tr("%1 [%n item(s)]", 0, items).arg(dispGroup);
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

    GroupSelector *recGroupPopup = new GroupSelector(m_popupStack, label,
                                                     displayNames, groupNames,
                                                     m_recGroup);

    if (recGroupPopup->Create())
    {
        m_usingGroupSelector = true;
        m_groupSelected = false;
        connect(recGroupPopup, SIGNAL(result(QString)),
                SLOT(displayRecGroup(QString)));
        connect(recGroupPopup, SIGNAL(Exiting()),
                SLOT(groupSelectorClosed()));
        m_popupStack->AddScreen(recGroupPopup);
    }
    else
        delete recGroupPopup;
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

    m_curGroupPassword = m_recGroupPwCache[recGroup];

    m_recGroup = newRecGroup;

    m_groupDisplayName = ProgramInfo::i18n(m_recGroup);

    // Since the group filter is changing, the current position in the lists
    // is meaningless -- so reset the lists so the position won't be saved.
    m_recordingList->Reset();
    m_groupList->Reset();

    UpdateUILists();

    if (gCoreContext->GetNumSetting("RememberRecGroup",1))
        gCoreContext->SaveSetting("DisplayRecGroup", m_recGroup);

    if (m_recGroupType[m_recGroup] == "recgroup")
        gCoreContext->SaveSetting("DisplayRecGroupIsCategory", 0);
    else
        gCoreContext->SaveSetting("DisplayRecGroupIsCategory", 1);
}

QString PlaybackBox::getRecGroupPassword(const QString &group)
{
    return m_recGroupPwCache[group];
}

void PlaybackBox::fillRecGroupPasswordCache(void)
{
    m_recGroupPwCache.clear();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT recgroup, password FROM recgrouppassword "
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
            else if (m_recGroup == ProgramInfo::i18n("Deleted"))
                recgroup = "Deleted";

            m_recGroupPwCache[recgroup] = query.value(1).toString();
        }
    }
}

/// \brief Used to change the recording group of a program or playlist.
void PlaybackBox::ShowRecGroupChanger(bool use_playlist)
{
    m_op_on_playlist = use_playlist;

    ProgramInfo *pginfo = NULL;
    if (use_playlist)
    {
        if (!m_playList.empty())
            pginfo = FindProgramInUILists(m_playList[0]);
    }
    else
        pginfo = CurrentItem();

    if (!pginfo)
        return;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT recgroup, COUNT(title) FROM recorded "
        "WHERE deletepending = 0 GROUP BY recgroup ORDER BY recgroup");

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

    GroupSelector *rgChanger = new GroupSelector(
        m_popupStack, label, displayNames, groupNames,
        pginfo->GetRecordingGroup());

    if (rgChanger->Create())
    {
        connect(rgChanger, SIGNAL(result(QString)), SLOT(setRecGroup(QString)));
        m_popupStack->AddScreen(rgChanger);
    }
    else
        delete rgChanger;
}

/// \brief Used to change the play group of a program or playlist.
void PlaybackBox::ShowPlayGroupChanger(bool use_playlist)
{
    m_op_on_playlist = use_playlist;

    ProgramInfo *pginfo = NULL;
    if (use_playlist)
    {
        if (!m_playList.empty())
            pginfo = FindProgramInUILists(m_playList[0]);
    }
    else
        pginfo = CurrentItem();

    if (!pginfo)
        return;

    QStringList groupNames(tr("Default"));
    QStringList displayNames("Default");

    QStringList list = PlayGroup::GetNames();
    QStringList::const_iterator it = list.begin();
    for (; it != list.end(); ++it)
    {
        displayNames.push_back(*it);
        groupNames.push_back(*it);
    }

    QString label = tr("Select Playback Group") +
        CreateProgramInfoString(*pginfo);

    GroupSelector *pgChanger = new GroupSelector(
        m_popupStack, label,displayNames, groupNames,
        pginfo->GetPlaybackGroup());

    if (pgChanger->Create())
    {
        connect(pgChanger, SIGNAL(result(QString)),
                SLOT(setPlayGroup(QString)));
        m_popupStack->AddScreen(pgChanger);
    }
    else
        delete pgChanger;
}

void PlaybackBox::doPlaylistExpireSetting(bool turnOn)
{
    ProgramInfo *tmpItem;
    QStringList::Iterator it;

    for (it = m_playList.begin(); it != m_playList.end(); ++it)
    {
        if ((tmpItem = FindProgramInUILists(*it)))
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
    ProgramInfo *tmpItem;
    QStringList::Iterator it;

    for (it = m_playList.begin(); it != m_playList.end(); ++it)
    {
        if ((tmpItem = FindProgramInUILists(*it)))
        {
            tmpItem->SaveWatched(turnOn);
        }
    }

    doClearPlaylist();
    UpdateUILists();
}

void PlaybackBox::showMetadataEditor()
{
    ProgramInfo *pgInfo = CurrentItem();

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    RecMetadataEdit *editMetadata = new RecMetadataEdit(mainStack, pgInfo);

    if (editMetadata->Create())
    {
        connect(editMetadata, SIGNAL(result(const QString &, const QString &,
                const QString &, const QString &, uint, uint)), SLOT(
                saveRecMetadata(const QString &, const QString &,
                const QString &, const QString &, uint, uint)));
        mainStack->AddScreen(editMetadata);
    }
    else
        delete editMetadata;
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

    ProgramInfo *pginfo = qVariantValue<ProgramInfo *>(item->GetData());

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
                            .arg(tempSubTitle).arg(newSubtitle);

        QString seasone;
        QString seasonx;
        QString season;
        QString episode;
        if (newSeason > 0 || newEpisode > 0)
        {
            season = format_season_and_episode(newSeason, 1);
            episode = format_season_and_episode(newEpisode, 1);
            seasone = QString("s%1e%2")
                .arg(format_season_and_episode(newSeason, 2))
                .arg(format_season_and_episode(newEpisode, 2));
            seasonx = QString("%1x%2")
                .arg(format_season_and_episode(newSeason, 1))
                .arg(format_season_and_episode(newEpisode, 2));
        }

        item->SetText(tempSubTitle, "titlesubtitle");
        item->SetText(newTitle, "title");
        item->SetText(newSubtitle, "subtitle");
        item->SetText(newInetref, "inetref");
        item->SetText(seasonx, "00x00");
        item->SetText(seasone, "s00e00");
        item->SetText(season, "season");
        item->SetText(episode, "episode");
        if (newDescription != NULL)
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

        MythTextInputDialog *newgroup = new MythTextInputDialog(
            popupStack, tr("New Recording Group"));

        connect(newgroup, SIGNAL(haveResult(QString)),
                SLOT(setRecGroup(QString)));

        if (newgroup->Create())
            popupStack->AddScreen(newgroup, false);
        else
            delete newgroup;
        return;
    }

    RecordingRule record;
    record.LoadTemplate("Default");
    uint defaultAutoExpire = record.m_autoExpire;

    if (m_op_on_playlist)
    {
        QStringList::const_iterator it;
        for (it = m_playList.begin(); it != m_playList.end(); ++it )
        {
            ProgramInfo *p = FindProgramInUILists(*it);
            if (!p)
                continue;

            if ((p->GetRecordingGroup() == "LiveTV") &&
                (newRecGroup != "LiveTV"))
            {
                p->SaveAutoExpire((AutoExpireType)defaultAutoExpire);
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

    ProgramInfo *p = CurrentItem();
    if (!p)
        return;

    if ((p->GetRecordingGroup() == "LiveTV") && (newRecGroup != "LiveTV"))
        p->SaveAutoExpire((AutoExpireType)defaultAutoExpire);
    else if ((p->GetRecordingGroup() != "LiveTV") && (newRecGroup == "LiveTV"))
        p->SaveAutoExpire(kLiveTVAutoExpire);

    RecordingInfo ri(*p);
    ri.ApplyRecordRecGroupChange(newRecGroup);
    *p = ri;
    UpdateUILists();
}

void PlaybackBox::setPlayGroup(QString newPlayGroup)
{
    ProgramInfo *tmpItem = CurrentItem();

    if (newPlayGroup.isEmpty() || !tmpItem)
        return;

    if (newPlayGroup == tr("Default"))
        newPlayGroup = "Default";

    if (m_op_on_playlist)
    {
        QStringList::Iterator it;

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
    else if (tmpItem)
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

    QString recgroup = item->GetData().toString();
    QString currentPassword = getRecGroupPassword(m_recGroup);

    PasswordChange *pwChanger = new PasswordChange(m_popupStack,
                                                   currentPassword);

    if (pwChanger->Create())
    {
        connect(pwChanger, SIGNAL(result(const QString &)),
                SLOT(SetRecGroupPassword(const QString &)));
        m_popupStack->AddScreen(pwChanger);
    }
    else
        delete pwChanger;
}

void PlaybackBox::SetRecGroupPassword(const QString &newPassword)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("DELETE FROM recgrouppassword "
                        "WHERE recgroup = :RECGROUP ;");
    query.bindValue(":RECGROUP", m_recGroup);

    if (!query.exec())
        MythDB::DBError("PlaybackBox::SetRecGroupPassword -- delete",
                        query);

    if (!newPassword.isEmpty())
    {
        query.prepare("INSERT INTO recgrouppassword "
                        "(recgroup, password) VALUES "
                        "( :RECGROUP , :PASSWD )");
        query.bindValue(":RECGROUP", m_recGroup);
        query.bindValue(":PASSWD", newPassword);

        if (!query.exec())
            MythDB::DBError("PlaybackBox::SetRecGroupPassword -- insert",
                            query);
    }

    m_recGroupPwCache[m_recGroup] = newPassword;
}

///////////////////////////////////////////////////

GroupSelector::GroupSelector(MythScreenStack *lparent, const QString &label,
                             const QStringList &list, const QStringList &data,
                             const QString &selected)
            : MythScreenType(lparent, "groupselector"), m_label(label),
              m_List(list), m_Data(data), m_selected(selected)
{
}

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

    for (int i = 0; i < m_List.size(); ++i)
    {
        new MythUIButtonListItem(groupList, m_List.at(i),
                                 qVariantFromValue(m_Data.at(i)));
    }

    // Set the current position in the list
    groupList->SetValueByData(qVariantFromValue(m_selected));

    BuildFocusList();

    connect(groupList, SIGNAL(itemClicked(MythUIButtonListItem *)),
            SLOT(AcceptItem(MythUIButtonListItem *)));

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

ChangeView::ChangeView(MythScreenStack *lparent, MythScreenType *parentscreen,
                       int viewMask)
                : MythScreenType(lparent, "changeview"),
                   m_parentScreen(parentscreen), m_viewMask(viewMask)
{
}

bool ChangeView::Create()
{
    if (!LoadWindowFromXML("recordings-ui.xml", "changeview", this))
        return false;

    MythUICheckBox *checkBox;

    checkBox = dynamic_cast<MythUICheckBox*>(GetChild("titles"));
    if (checkBox)
    {
        if (m_viewMask & PlaybackBox::VIEW_TITLES)
            checkBox->SetCheckState(MythUIStateType::Full);
        connect(checkBox, SIGNAL(toggled(bool)),
                m_parentScreen, SLOT(toggleTitleView(bool)));
    }

    checkBox = dynamic_cast<MythUICheckBox*>(GetChild("categories"));
    if (checkBox)
    {
        if (m_viewMask & PlaybackBox::VIEW_CATEGORIES)
            checkBox->SetCheckState(MythUIStateType::Full);
        connect(checkBox, SIGNAL(toggled(bool)),
                m_parentScreen, SLOT(toggleCategoryView(bool)));
    }

    checkBox = dynamic_cast<MythUICheckBox*>(GetChild("recgroups"));
    if (checkBox)
    {
        if (m_viewMask & PlaybackBox::VIEW_RECGROUPS)
            checkBox->SetCheckState(MythUIStateType::Full);
        connect(checkBox, SIGNAL(toggled(bool)),
                m_parentScreen, SLOT(toggleRecGroupView(bool)));
    }

    // TODO Do we need two separate settings to determine whether the watchlist
    //      is shown? The filter setting be enough?
        checkBox = dynamic_cast<MythUICheckBox*>(GetChild("watchlist"));
        if (checkBox)
        {
            if (m_viewMask & PlaybackBox::VIEW_WATCHLIST)
                checkBox->SetCheckState(MythUIStateType::Full);
            connect(checkBox, SIGNAL(toggled(bool)),
                    m_parentScreen, SLOT(toggleWatchListView(bool)));
        }
    //

    checkBox = dynamic_cast<MythUICheckBox*>(GetChild("searches"));
    if (checkBox)
    {
        if (m_viewMask & PlaybackBox::VIEW_SEARCHES)
            checkBox->SetCheckState(MythUIStateType::Full);
        connect(checkBox, SIGNAL(toggled(bool)),
                m_parentScreen, SLOT(toggleSearchView(bool)));
    }

    // TODO Do we need two separate settings to determine whether livetv
    //      recordings are shown? Same issue as the watchlist above
        checkBox = dynamic_cast<MythUICheckBox*>(GetChild("livetv"));
        if (checkBox)
        {
            if (m_viewMask & PlaybackBox::VIEW_LIVETVGRP)
                checkBox->SetCheckState(MythUIStateType::Full);
            connect(checkBox, SIGNAL(toggled(bool)),
                    m_parentScreen, SLOT(toggleLiveTVView(bool)));
        }
    //

    checkBox = dynamic_cast<MythUICheckBox*>(GetChild("watched"));
    if (checkBox)
    {
        if (m_viewMask & PlaybackBox::VIEW_WATCHED)
            checkBox->SetCheckState(MythUIStateType::Full);
        connect(checkBox, SIGNAL(toggled(bool)),
                m_parentScreen, SLOT(toggleWatchedView(bool)));
    }

    MythUIButton *savebutton = dynamic_cast<MythUIButton*>(GetChild("save"));
    connect(savebutton, SIGNAL(Clicked()), SLOT(SaveChanges()));

    BuildFocusList();

    return true;
}

void ChangeView::SaveChanges()
{
    emit save();
    Close();
}

////////////////////////////////////////////////

PasswordChange::PasswordChange(MythScreenStack *lparent, QString oldpassword)
                : MythScreenType(lparent, "passwordchanger"),
                    m_oldPassword(oldpassword)
{
    m_oldPasswordEdit = m_newPasswordEdit = NULL;
    m_okButton = NULL;
}

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
    m_newPasswordEdit->SetPassword(true);
    m_newPasswordEdit->SetMaxLength(10);

    BuildFocusList();

    connect(m_oldPasswordEdit, SIGNAL(valueChanged()),
                               SLOT(OldPasswordChanged()));
    connect(m_okButton, SIGNAL(Clicked()), SLOT(SendResult()));

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
                    m_progInfo(pginfo)
{
    m_titleEdit = m_subtitleEdit = m_descriptionEdit = m_inetrefEdit = NULL;
    m_seasonSpin = m_episodeSpin = NULL;
}

bool RecMetadataEdit::Create()
{
    if (!LoadWindowFromXML("recordings-ui.xml", "editmetadata", this))
        return false;

    m_titleEdit = dynamic_cast<MythUITextEdit*>(GetChild("title"));
    m_subtitleEdit = dynamic_cast<MythUITextEdit*>(GetChild("subtitle"));
    m_descriptionEdit = dynamic_cast<MythUITextEdit*>(GetChild("description"));
    m_inetrefEdit = dynamic_cast<MythUITextEdit*>(GetChild("inetref"));
    m_seasonSpin = dynamic_cast<MythUISpinBox*>(GetChild("season"));
    m_episodeSpin = dynamic_cast<MythUISpinBox*>(GetChild("episode"));
    MythUIButton *okButton = dynamic_cast<MythUIButton*>(GetChild("ok"));

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

    connect(okButton, SIGNAL(Clicked()), SLOT(SaveChanges()));

    BuildFocusList();

    return true;
}

void RecMetadataEdit::SaveChanges()
{
    QString newRecTitle = m_titleEdit->GetText();
    QString newRecSubtitle = m_subtitleEdit->GetText();
    QString newRecDescription = NULL;
    QString newRecInetref = NULL;
    uint newRecSeason = 0, newRecEpisode = 0;
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

//////////////////////////////////////////

HelpPopup::HelpPopup(MythScreenStack *lparent)
                : MythScreenType(lparent, "helppopup"),
                  m_iconList(NULL)
{

}

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

    addItem("hd1080",      tr("Recording is in 1080i/p High Definition"));
    addItem("hd720",       tr("Recording is in 720p High Definition"));
    addItem("hdtv",        tr("Recording is in High Definition"));
    addItem("widescreen",  tr("Recording is Widescreen"));
    addItem("avchd",       tr("Recording is in HD using H.264 codec"));

    addItem("watched",     tr("Recording has been watched"));
//    addItem("preserved",   tr("Recording is preserved"));

    return true;
}

void HelpPopup::addItem(const QString &state, const QString &text)
{
    MythUIButtonListItem *item = new MythUIButtonListItem(m_iconList, text);
    item->DisplayState(state, "icons");
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
