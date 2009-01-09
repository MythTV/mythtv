
using namespace std;

#include <QDateTime>
#include <QDir>
#include <QApplication>
#include <QTimer>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QWaitCondition>

#include "playbackbox.h"
#include "playbackboxlistitem.h"
#include "proglist.h"
#include "tv.h"
#include "oldsettings.h"
#include "NuppelVideoPlayer.h"

#include "mythdirs.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "mythverbose.h"
#include "programinfo.h"
#include "scheduledrecording.h"
#include "remoteutil.h"
#include "lcddevice.h"
#include "previewgenerator.h"
#include "playgroup.h"
#include "customedit.h"
#include "util.h"

#include "mythuihelper.h"
#include "mythuitext.h"
#include "mythuistatetype.h"
#include "mythdialogbox.h"
#include "mythuitextedit.h"
#include "mythuiimage.h"
#include "mythuicheckbox.h"
#include "mythuiprogressbar.h"

#define LOC QString("PlaybackBox: ")
#define LOC_ERR QString("PlaybackBox Error: ")

#define REC_CAN_BE_DELETED(rec) \
    ((((rec)->programflags & FL_INUSEPLAYING) == 0) && \
     ((((rec)->programflags & FL_INUSERECORDING) == 0) || \
      ((rec)->recgroup != "LiveTV")))

#define USE_PREV_GEN_THREAD

const uint PreviewGenState::maxAttempts     = 5;
const uint PreviewGenState::minBlockSeconds = 60;

const uint PlaybackBox::PREVIEW_GEN_MAX_RUN = 2;

static int comp_programid(const ProgramInfo *a, const ProgramInfo *b)
{
    if (a->programid == b->programid)
        return (a->recstartts < b->recstartts ? 1 : -1);
    else
        return (a->programid < b->programid ? 1 : -1);
}

static int comp_programid_rev(const ProgramInfo *a, const ProgramInfo *b)
{
    if (a->programid == b->programid)
        return (a->recstartts > b->recstartts ? 1 : -1);
    else
        return (a->programid > b->programid ? 1 : -1);
}

static int comp_originalAirDate(const ProgramInfo *a, const ProgramInfo *b)
{
    QDate dt1, dt2;

    if (a->hasAirDate)
        dt1 = a->originalAirDate;
    else
        dt1 = a->startts.date();

    if (b->hasAirDate)
        dt2 = b->originalAirDate;
    else
        dt2 = b->startts.date();

    if (dt1 == dt2)
        return (a->recstartts < b->recstartts ? 1 : -1);
    else
        return (dt1 < dt2 ? 1 : -1);
}

static int comp_originalAirDate_rev(const ProgramInfo *a, const ProgramInfo *b)
{
    QDate dt1, dt2;

    if (a->hasAirDate)
        dt1 = a->originalAirDate;
    else
        dt1 = a->startts.date();

    if (b->hasAirDate)
        dt2 = b->originalAirDate;
    else
        dt2 = b->startts.date();

    if (dt1 == dt2)
        return (a->recstartts > b->recstartts ? 1 : -1);
    else
        return (dt1 > dt2 ? 1 : -1);
}

static int comp_recpriority2(const ProgramInfo *a, const ProgramInfo *b)
{
    if (a->recpriority2 == b->recpriority2)
        return (a->recstartts < b->recstartts ? 1 : -1);
    else
        return (a->recpriority2 < b->recpriority2 ? 1 : -1);
}

static int comp_recordDate(const ProgramInfo *a, const ProgramInfo *b)
{
    if (a->startts.date() == b->startts.date())
        return (a->recstartts < b->recstartts ? 1 : -1);
    else
        return (a->startts.date() < b->startts.date() ? 1 : -1);
}

static int comp_recordDate_rev(const ProgramInfo *a, const ProgramInfo *b)
{
    if (a->startts.date() == b->startts.date())
        return (a->recstartts > b->recstartts ? 1 : -1);
    else
        return (a->startts.date() > b->startts.date() ? 1 : -1);
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

static PlaybackBox::ViewMask m_viewMaskToggle(PlaybackBox::ViewMask mask,
        PlaybackBox::ViewMask toggle)
{
    // can only toggle a single bit at a time
    if ((mask & toggle))
        return (PlaybackBox::ViewMask)(mask & ~toggle);
    return (PlaybackBox::ViewMask)(mask | toggle);
}

static QString sortTitle(QString title, PlaybackBox::ViewMask viewmask,
        PlaybackBox::ViewTitleSort titleSort, int recpriority)
{
    if (title.isEmpty())
        return title;

    QRegExp prefixes( QObject::tr("^(The |A |An )") );
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
        // recpriority values are "inverted" by substracting them from
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

        sTitle = sortprefix + "-" + sTitle;
    }
    return sTitle;
}

ProgramInfo *PlaybackBox::RunPlaybackBox(void * player, bool showTV)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    PlaybackBox *pbb = new PlaybackBox(mainStack,"playbackbox",
                                        PlaybackBox::Play, (TV *)player,
                                        showTV);

    if (pbb->Create())
        mainStack->AddScreen(pbb);
    else
        delete pbb;

    ProgramInfo *nextProgram = NULL;
    if (pbb->CurrentItem())
        nextProgram = new ProgramInfo(*pbb->CurrentItem());

    return nextProgram;
}

PlaybackBox::PlaybackBox(MythScreenStack *parent, QString name, BoxType ltype,
                            TV *player, bool showTV)
    : MythScreenType(parent, name),
      // Settings
      m_type(ltype),
      m_formatShortDate("M/d"),           m_formatLongDate("ddd MMMM d"),
      m_formatTime("h:mm AP"),
      m_watchListAutoExpire(false),
      m_watchListMaxAge(60),              m_watchListBlackOut(2),
      m_groupnameAsAllProg(false),
      m_listOrder(1),
      // Recording Group settings
      m_groupDisplayName(ProgramInfo::i18n("All Programs")),
      m_recGroup("All Programs"),
      m_watchGroupName(tr("Watch List")),
      m_watchGroupLabel(m_watchGroupName.toLower()),
      m_viewMask(VIEW_TITLES),

      // General m_popupMenu support
      m_popupMenu(NULL),
      // Main Recording List support
      m_fillListTimer(new QTimer(this)),  m_fillListFromCache(false),
      m_connected(false),                 m_progsInDB(0),
      // Other state
      m_delItem(NULL),                    m_currentItem(NULL),
      m_progCache(NULL),                  m_playingSomething(false),
      // Selection state variables
      m_needUpdate(false),
      // Free disk space tracking
      m_freeSpaceNeedsUpdate(true),       m_freeSpaceTimer(new QTimer(this)),
      m_freeSpaceTotal(0),                m_freeSpaceUsed(0),
      // Preview Image Variables
      m_previewGeneratorRunning(0),
      // Network Control Variables
      m_underNetworkControl(false),
      m_player(NULL)
{
    m_formatShortDate    = gContext->GetSetting("ShortDateFormat", "M/d");
    m_formatLongDate     = gContext->GetSetting("DateFormat", "ddd MMMM d");
    m_formatTime         = gContext->GetSetting("TimeFormat", "h:mm AP");
    m_recGroup           = gContext->GetSetting("DisplayRecGroup",
                                                "All Programs");
    int pbOrder        = gContext->GetNumSetting("PlayBoxOrdering", 1);
    // Split out sort order modes, wacky order for backward compatibility
    m_listOrder = (pbOrder >> 1) ^ (m_allOrder = pbOrder & 1);
    m_watchListStart     = gContext->GetNumSetting("PlaybackWLStart", 0);
    m_watchListAutoExpire= gContext->GetNumSetting("PlaybackWLAutoExpire", 0);
    m_watchListMaxAge    = gContext->GetNumSetting("PlaybackWLMaxAge", 60);
    m_watchListBlackOut  = gContext->GetNumSetting("PlaybackWLBlackOut", 2);
    m_groupnameAsAllProg = gContext->GetNumSetting("DispRecGroupAsAllProg", 0);
    m_previewFromBookmark = gContext->GetNumSetting("PreviewFromBookmark");
    m_previewGeneratorMode =
            gContext->GetNumSetting("GeneratePreviewRemotely", 0) ?
            PreviewGenerator::kRemote : PreviewGenerator::kLocalAndRemote;

    bool displayCat  = gContext->GetNumSetting("DisplayRecGroupIsCategory", 0);

    m_viewMask = (ViewMask)gContext->GetNumSetting(
                                    "DisplayGroupDefaultViewMask", VIEW_TITLES);

    if (gContext->GetNumSetting("PlaybackWatchList", 1))
        m_viewMask = (ViewMask)(m_viewMask | VIEW_WATCHLIST);

    if (player)
    {
        m_player = player;
        QString tmp = m_player->GetRecordingGroup(0);
        if (!tmp.isEmpty())
            m_recGroup = tmp;
    }

    // recording group stuff
    m_recGroupType.clear();
    m_recGroupType[m_recGroup] = (displayCat) ? "category" : "recgroup";
    if (m_groupnameAsAllProg)
        m_groupDisplayName = ProgramInfo::i18n(m_recGroup);

    fillRecGroupPasswordCache();

    if (!m_player)
        m_recGroupPassword = getRecGroupPassword(m_recGroup);

    // misc setup
    gContext->addListener(this);

    m_popupStack = GetMythMainWindow()->GetStack("popup stack");
}

PlaybackBox::~PlaybackBox(void)
{
    gContext->removeListener(this);

    if (m_fillListTimer)
    {
        m_fillListTimer->disconnect(this);
        m_fillListTimer->deleteLater();
        m_fillListTimer = NULL;
    }

    if (m_freeSpaceTimer)
    {
        m_freeSpaceTimer->disconnect(this);
        m_freeSpaceTimer->deleteLater();
        m_freeSpaceTimer = NULL;
    }

    clearProgramCache();

    if (m_currentItem)
        delete m_currentItem;

    // disconnect preview generators
    QMutexLocker locker(&m_previewGeneratorLock);
    PreviewMap::iterator it = m_previewGenerator.begin();
    for (;it != m_previewGenerator.end(); ++it)
    {
        if ((*it).gen)
            (*it).gen->disconnectSafe();
    }
}

bool PlaybackBox::Create()
{
    if (!LoadWindowFromXML("recordings-ui.xml", "watchrecordings", this))
        return false;

    m_groupList     = dynamic_cast<MythUIButtonList *> (GetChild("groups"));
    m_recordingList = dynamic_cast<MythUIButtonList *> (GetChild("recordings"));

    m_noRecordingsText = dynamic_cast<MythUIText *> (GetChild("norecordings"));

    m_previewImage = dynamic_cast<MythUIImage *>(GetChild("preview"));

    if (!m_recordingList || !m_groupList)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical theme elements.");
        return false;
    }

    connect(m_groupList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            SLOT(updateRecList(MythUIButtonListItem*)));
    connect(m_groupList, SIGNAL(itemClicked(MythUIButtonListItem*)),
            SLOT(SwitchList()));
    connect(m_recordingList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            SLOT(ItemSelected(MythUIButtonListItem*)));
    connect(m_recordingList, SIGNAL(itemClicked(MythUIButtonListItem*)),
            SLOT(playSelected(MythUIButtonListItem*)));

    if (!m_player && !m_recGroupPassword.isEmpty())
        displayRecGroup(m_recGroup);
    else if (gContext->GetNumSetting("QueryInitialFilter", 0) == 1)
        showGroupFilter();
    else
    {
        FillList(true);

        if ((m_titleList.size() <= 1) && (m_progsInDB > 0))
        {
            m_recGroup = "";
            showGroupFilter();
        }
    }

    // connect up timers...
    connect(m_freeSpaceTimer, SIGNAL(timeout()), SLOT(setUpdateFreeSpace()));
    connect(m_fillListTimer, SIGNAL(timeout()), SLOT(listChanged()));

    BuildFocusList();

    if (gContext->GetNumSetting("PlaybackBoxStartInTitle", 0))
        SetFocusWidget(m_recordingList);

    return true;
}

void PlaybackBox::SetTextFromMap(MythUIType *parent,
                                   QMap<QString, QString> &infoMap)
{
    ResetMap(this, m_currentMap);

    if (!parent)
        return;

    QList<MythUIType *> *children = parent->GetAllChildren();

    MythUIText *textType;
    QMutableListIterator<MythUIType *> i(*children);
    while (i.hasNext())
    {
        MythUIType *type = i.next();
        if (!type->IsVisible())
            continue;

        textType = dynamic_cast<MythUIText *> (type);
        if (textType && infoMap.contains(textType->objectName()))
        {
            QString newText = textType->GetDefaultText();
            QRegExp regexp("%(\\|(.))?([^\\|]+)(\\|(.))?%");
            regexp.setMinimal(true);
            if (newText.contains(regexp))
            {
                int pos = 0;
                QString tempString = newText;
                while ((pos = regexp.indexIn(newText, pos)) != -1)
                {
                    QString key = regexp.cap(3).toLower().trimmed();
                    QString replacement;
                    if (!infoMap.value(key).isEmpty())
                    {
                        replacement = QString("%1%2%3")
                                                .arg(regexp.cap(2))
                                                .arg(infoMap.value(key))
                                                .arg(regexp.cap(5));
                    }
                    tempString.replace(regexp.cap(0), replacement);
                    pos += regexp.matchedLength();
                }
                newText = tempString;
            }
            else
                newText = infoMap.value(textType->objectName());

            textType->SetText(newText);
        }
    }
}

void PlaybackBox::ResetMap(MythUIType *parent, QMap<QString, QString> &infoMap)
{
    if (!parent)
        return;

    if (infoMap.isEmpty())
        return;

    QList<MythUIType *> *children = parent->GetAllChildren();

    MythUIText *textType;
    QMutableListIterator<MythUIType *> i(*children);
    while (i.hasNext())
    {
        MythUIType *type = i.next();
        if (!type->IsVisible())
            continue;

        textType = dynamic_cast<MythUIText *> (type);
        if (textType && infoMap.contains(textType->objectName()))
            textType->Reset();
    }
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

        if (pwd->Create())
            popupStack->AddScreen(pwd, false);
        return;
    }

    setGroupFilter(newRecGroup);
}

void PlaybackBox::checkPassword(const QString &password)
{
    QString grouppass = m_recGroupPwCache[m_newRecGroup];
    if (password == grouppass)
        setGroupFilter(m_newRecGroup);
}

void PlaybackBox::updateGroupInfo(const QString &groupname)
{
    QMap<QString, QString> infoMap;
    int countInGroup;

    if (groupname.isEmpty())
    {
        countInGroup = m_progLists[""].size();
        infoMap["title"] = m_groupDisplayName;
        infoMap["group"] = m_groupDisplayName;
        infoMap["show"] = ProgramInfo::i18n("All Programs");
    }
    else
    {
        countInGroup = m_progLists[groupname].size();
        infoMap["group"] = m_groupDisplayName;
        infoMap["show"] = groupname;
        infoMap["title"] = QString("%1 - %2").arg(m_groupDisplayName)
                                                .arg(groupname);
    }

    if (countInGroup > 1)
        infoMap["description"] = QString(tr("There are %1 recordings in "
                                            "this display group"))
                                            .arg(countInGroup);
    else if (countInGroup == 1)
        infoMap["description"] = QString(tr("There is one recording in "
                                                "this display group"));
    else
        infoMap["description"] = QString(tr("There are no recordings in "
                                            "this display group"));

    infoMap["rec_count"] = QString("%1").arg(countInGroup);

    SetTextFromMap(this, infoMap);
    m_currentMap = infoMap;

    if (m_previewImage)
        m_previewImage->SetVisible(false);

    updateIcons();
}

void PlaybackBox::UpdateProgramInfo(
    MythUIButtonListItem *item, bool is_sel, bool force_preview_reload)
{
    if (!item)
        return;

    ProgramInfo *pginfo = qVariantValue<ProgramInfo *>(item->GetData());

    if (!pginfo)
        return;

    static const char *disp_flags[] = { "transcoding", "commflagging", };
    const bool disp_flag_stat[] =
    {
        !JobQueue::IsJobQueuedOrRunning(
            JOB_TRANSCODE, pginfo->chanid, pginfo->recstartts),
        !JobQueue::IsJobQueuedOrRunning(
            JOB_COMMFLAG,  pginfo->chanid, pginfo->recstartts),
    };

    for (uint i = 0; i < sizeof(disp_flags) / sizeof(char*); i++)
        item->DisplayState(disp_flag_stat[i]?"yes":"no", disp_flags[i]);

    QString oldimgfile = item->GetImage("preview");
    QString imagefile = QString::null;
    if (oldimgfile.isEmpty() || force_preview_reload ||
        ((is_sel && GetFocusWidget() == m_recordingList)))
    {
        imagefile = getPreviewImage(pginfo);
    }

    if (!imagefile.isEmpty())
        item->SetImage(imagefile, "preview", force_preview_reload);

    if ((GetFocusWidget() == m_recordingList) && is_sel)
    {
        QMap<QString, QString> infoMap;

        pginfo->ToMap(infoMap);
        SetTextFromMap(this, infoMap);
        m_currentMap = infoMap;

        if (m_previewImage)
        {
            QString imagefile = getPreviewImage(pginfo);
            m_previewImage->SetVisible(!imagefile.isEmpty());
            m_previewImage->SetFilename(imagefile);
            m_previewImage->Load();
        }

        updateIcons(pginfo);
    }
}

void PlaybackBox::updateIcons(const ProgramInfo *pginfo)
{
    int flags = 0;

    if (pginfo)
        flags = pginfo->programflags;

    QMap <QString, int>::iterator it;
    QMap <QString, int> iconMap;

    iconMap["commflagged"] = FL_COMMFLAG;
    iconMap["cutlist"]     = FL_CUTLIST;
    iconMap["autoexpire"]  = FL_AUTOEXP;
    iconMap["processing"]  = FL_EDITING;
    iconMap["bookmark"]    = FL_BOOKMARK;
    iconMap["inuse"]       = (FL_INUSERECORDING | FL_INUSEPLAYING);
    iconMap["transcoded"]  = FL_TRANSCODED;
    iconMap["watched"]     = FL_WATCHED;
    iconMap["preserved"]   = FL_PRESERVED;

    MythUIImage *iconImage;
    for (it = iconMap.begin(); it != iconMap.end(); ++it)
    {
        iconImage = dynamic_cast<MythUIImage *>(GetChild(it.key()));
        if (iconImage)
            iconImage->SetVisible(flags & (*it));
    }

    iconMap.clear();
    iconMap["dolby"]  = AUD_DOLBY;
    iconMap["surround"]  = AUD_SURROUND;
    iconMap["stereo"] = AUD_STEREO;
    iconMap["mono"] = AUD_MONO;

    MythUIStateType *iconState;
    iconState = dynamic_cast<MythUIStateType *>(GetChild("audioprops"));
    if (pginfo && iconState)
    {
        for (it = iconMap.begin(); it != iconMap.end(); ++it)
        {
            if (pginfo && pginfo->audioproperties & (*it))
            {
                iconState->DisplayState(it.key());
                return;
            }
        }
    }
    else if (iconState)
        iconState->DisplayState("default");

    iconMap.clear();
    iconMap["hdtv"] = VID_HDTV;
    iconMap["widescreen"] = VID_WIDESCREEN;

    iconState = dynamic_cast<MythUIStateType *>(GetChild("videoprops"));
    if (pginfo && iconState)
    {
        for (it = iconMap.begin(); it != iconMap.end(); ++it)
        {
            if (pginfo && pginfo->videoproperties & (*it))
            {
                iconState->DisplayState(it.key());
                return;
            }
        }
    }
    else if (iconState)
        iconState->DisplayState("default");

    iconMap.clear();
    iconMap["onscreensub"] = SUB_ONSCREEN;
    iconMap["subtitles"] = SUB_NORMAL;
    iconMap["cc"] = SUB_HARDHEAR;
    iconMap["deafsigned"] = SUB_SIGNED;

    iconState = dynamic_cast<MythUIStateType *>(GetChild("subtitletypes"));
    if (pginfo && iconState)
    {
        for (it = iconMap.begin(); it != iconMap.end(); ++it)
        {
            if (pginfo->subtitleType & (*it))
            {
                iconState->DisplayState(it.key());
                return;
            }
        }
    }
    else if (iconState)
        iconState->DisplayState("default");
}

void PlaybackBox::updateUsage()
{
    vector<FileSystemInfo> fsInfos;
    if (m_freeSpaceNeedsUpdate && m_connected)
    {
        m_freeSpaceNeedsUpdate = false;
        m_freeSpaceTotal = 0;
        m_freeSpaceUsed = 0;

        fsInfos = RemoteGetFreeSpace();
        for (unsigned int i = 0; i < fsInfos.size(); i++)
        {
            if (fsInfos[i].directory == "TotalDiskSpace")
            {
                m_freeSpaceTotal = (int) (fsInfos[i].totalSpaceKB >> 10);
                m_freeSpaceUsed = (int) (fsInfos[i].usedSpaceKB >> 10);
            }
        }
        m_freeSpaceTimer->start(15000);
    }

    QString usestr;

    double perc = 0.0;
    if (m_freeSpaceTotal > 0)
        perc = (100.0 * m_freeSpaceUsed) / (double) m_freeSpaceTotal;

    usestr.sprintf("%d", (int)perc);
    usestr = usestr + tr("% used");

    QString size;
    size.sprintf("%0.2f", (m_freeSpaceTotal - m_freeSpaceUsed) / 1024.0);
    QString rep = tr(", %1 GB free").arg(size);
    usestr = usestr + rep;

    MythUIText *m_freeSpaceText = dynamic_cast<MythUIText *>
                                                    (GetChild("freereport"));
    if (m_freeSpaceText)
        m_freeSpaceText->SetText(usestr);

    MythUIProgressBar *m_freeSpaceProgress = dynamic_cast<MythUIProgressBar *>
                                                        (GetChild("usedbar"));
    if (m_freeSpaceProgress)
    {
        m_freeSpaceProgress->SetUsed(m_freeSpaceUsed);
        m_freeSpaceProgress->SetTotal(m_freeSpaceTotal);
    }
}

void PlaybackBox::updateGroupList()
{
    m_groupList->Reset();

    LCD *lcddev = LCD::Get();
    QString lcdTitle;
    QList<LCDMenuItem> lcdItems;

    if (lcddev)
    {
        if (GetFocusWidget() == m_groupList)
            lcdTitle = "Recordings";;
    }

    if (!m_titleList.isEmpty())
    {
        MythUIButtonListItem *item;

        QString groupname;

        bool foundGroup = false;
        QStringList::iterator it;
        for (it = m_titleList.begin(); it != m_titleList.end(); it++)
        {
            groupname = (*it);
            groupname = groupname.simplified();

            // The first item added to the list will trigger an update of the
            // associated Recording List, m_needsUpdate should be false unless
            // this is the currently selected group
            if (groupname.toLower() == m_currentGroup)
            {
                m_needUpdate = true;
                foundGroup = true;
            }

            item = new MythUIButtonListItem(m_groupList, "",
                                        qVariantFromValue(groupname.toLower()));

            if (m_needUpdate)
                m_groupList->SetItemCurrent(item);

            if (groupname.isEmpty())
                groupname = m_groupDisplayName;

            item->SetText(groupname, "name");

            int count = m_progLists[groupname.toLower()].size();
            item->SetText(QString::number(count), "reccount");

            if (lcddev && (GetFocusWidget() == m_groupList))
                lcdItems.append(LCDMenuItem(0, NOTCHECKABLE, groupname));
        }

        if (!foundGroup)
        {
            m_needUpdate = true;
            m_groupList->SetItemCurrent(0);
        }
    }
}

void PlaybackBox::updateRecList(MythUIButtonListItem *sel_item)
{
    if (!sel_item)
        return;

    QString groupname = sel_item->GetData().toString();

    updateGroupInfo(groupname);

    if ((m_currentGroup == groupname) && !m_needUpdate)
        return;

    m_needUpdate = false;

    if (!m_isFilling)
        m_currentGroup = groupname;

    m_recordingList->Reset();

    if (groupname == "default")
        groupname = "";

    ProgramMap::iterator pmit = m_progLists.find(groupname);
    if (pmit == m_progLists.end())
        return;

    ProgramList &progList = *pmit;

    LCD    *lcddev   = LCD::Get();
    QString lcdTitle = "Recordings";
    if (lcddev && GetFocusWidget() != m_groupList)
        lcdTitle = " <<" + groupname;

    QList<LCDMenuItem> lcdItems;

    static const char *disp_flags[] = { "playlist", "watched", };
    bool disp_flag_stat[sizeof(disp_flags)/sizeof(char*)];

    ProgramList::iterator it = progList.begin();
    for (; it != progList.end(); it++)
    {
        MythUIButtonListItem *item =
            new PlaybackBoxListItem(this, m_recordingList, *it);

        QString tempSubTitle;
        if (groupname != (*it)->title)
        {
            tempSubTitle = (*it)->title;
            if (!(*it)->subtitle.trimmed().isEmpty())
            {
                tempSubTitle = QString("%1 - \"%2\"")
                    .arg(tempSubTitle).arg((*it)->subtitle);
            }
        }
        else
        {
            tempSubTitle = (*it)->subtitle;
            if (tempSubTitle.trimmed().isEmpty())
                tempSubTitle = (*it)->title;
        }

        QString tempShortDate = ((*it)->recstartts).toString(m_formatShortDate);
        QString tempLongDate  = ((*it)->recstartts).toString(m_formatLongDate);
        QString tempTime      = ((*it)->recstartts).toString(m_formatTime);
        QString tempSize      = tr("%1 GB")
            .arg((*it)->filesize * (1.0/(1024.0*1024.0*1024.0)), 0, 'f', 2);

        QString state = ((*it)->recstatus == rsRecording) ?
            QString("running") : QString::null;

        if ((((*it)->recstatus != rsRecording) &&
             ((*it)->availableStatus != asAvailable) &&
             ((*it)->availableStatus != asNotYetAvailable)) ||
            (m_player && m_player->IsSameProgram(0, *it)))
        {
            state = "disabled";
        }

        if (lcddev && !(GetFocusWidget() == m_groupList))
        {
            QString lcdSubTitle = tempSubTitle;
            QString lcdStr = QString("%1 %2")
                .arg(lcdSubTitle.replace('"', "'")).arg(tempShortDate);
            LCDMenuItem lcdItem(m_currentItem == *it, NOTCHECKABLE, lcdStr);
            lcdItems.push_back(lcdItem);
        }

        item->SetText(tempSubTitle,       "titlesubtitle", state);
        item->SetText((*it)->title,       "title",         state);
        item->SetText((*it)->subtitle,    "subtitle",      state);
        item->SetText((*it)->description, "description",   state);
        item->SetText(tempLongDate,       "longdate",      state);
        item->SetText(tempShortDate,      "shortdate",     state);
        item->SetText(tempTime,           "time",          state);
        item->SetText(tempSize,           "size",          state);

        item->DisplayState(state, "status");

        disp_flag_stat[0] = !m_playList.filter((*it)->MakeUniqueKey()).empty();
        disp_flag_stat[1] = (*it)->programflags & FL_WATCHED;

        for (uint i = 0; i < sizeof(disp_flags) / sizeof(char*); i++)
            item->DisplayState(disp_flag_stat[i]?"yes":"no", disp_flags[i]);

        if (m_currentItem &&
            (m_currentItem->chanid     == (*it)->chanid) &&
            (m_currentItem->recstartts == (*it)->recstartts))
        {
            m_recordingList->SetItemCurrent(item);
        }
    }

    if (lcddev && !lcdItems.isEmpty())
        lcddev->switchToMenu(lcdItems, lcdTitle);

    if (m_noRecordingsText)
    {
        if (!progList.isEmpty())
            m_noRecordingsText->SetVisible(false);
        else
        {
            QMutexLocker locker(&m_progCacheLock);
            QString txt = (m_progCache && !m_progCache->empty()) ?
                tr("There are no recordings in your current view") :
                tr("There are no recordings available");
            m_noRecordingsText->SetText(txt);
            m_noRecordingsText->SetVisible(true);
        }
    }
}

void PlaybackBox::listChanged(void)
{
    if (m_playingSomething)
        return;

    // If we are in the middle of filling the list, then wait
    if (m_isFilling)
    {
        m_fillListTimer->setSingleShot(true);
        m_fillListTimer->start(1000);
        return;
    }

    m_connected = FillList(m_fillListFromCache);
}

bool PlaybackBox::FillList(bool useCachedData)
{
    m_isFilling = true;

    ProgramInfo *p;

    if (m_currentItem)
    {
        delete m_currentItem;
        m_currentItem = NULL;
    }

    ProgramInfo *tmp = CurrentItem();
    if (tmp)
        m_currentItem = new ProgramInfo(*tmp);

    QMap<QString, AvailableStatusType> asCache;
    QString asKey;

    if (!m_progLists.isEmpty())
    {
        ProgramList::iterator it = m_progLists[""].begin();
        ProgramList::iterator end = m_progLists[""].end();
        for (; it != end; ++it)
        {
            asKey = (*it)->MakeUniqueKey();
            asCache[asKey] = (*it)->availableStatus;
        }
    }

    m_progsInDB = 0;
    m_titleList.clear();
    m_progLists.clear();
    m_recordingList->Reset();
    m_groupList->Reset();
    // Clear autoDelete for the "all" list since it will share the
    // objects with the title lists.
    m_progLists[""] = ProgramList(false);
    m_progLists[""].setAutoDelete(false);

    ViewTitleSort titleSort = (ViewTitleSort)gContext->GetNumSetting(
                                "DisplayGroupTitleSort", TitleSortAlphabetical);

    QMap<QString, QString> sortedList;
    QMap<int, QString> searchRule;
    QMap<int, int> recidEpisodes;
    QString sTitle;

    bool LiveTVInAllPrograms = gContext->GetNumSetting("LiveTVInAllPrograms",0);

    m_progCacheLock.lock();
    if (!useCachedData || !m_progCache || m_progCache->empty())
    {
        clearProgramCache();

        m_progCache = RemoteGetRecordedList(m_allOrder == 0 || m_type == Delete);
    }
    else
    {
        // Validate the cache
        vector<ProgramInfo *>::iterator i = m_progCache->begin();
        for ( ; i != m_progCache->end(); )
        {
            if ((*i)->availableStatus == asDeleted)
            {
                delete *i;
                i = m_progCache->erase(i);
            }
            else
                i++;
        }
    }

    if (m_progCache)
    {
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
                    tmpTitle.remove(QRegExp(" \\(.*\\)$"));
                    searchRule[query.value(0).toInt()] = tmpTitle;
                }
            }
        }

        if (!(m_viewMask & VIEW_WATCHLIST))
            m_watchListStart = 0;

        sortedList[""] = "";
        vector<ProgramInfo *>::iterator i = m_progCache->begin();
        for ( ; i != m_progCache->end(); i++)
        {
            m_progsInDB++;
            p = *i;

            if (p->title.isEmpty())
                p->title = tr("_NO_TITLE_");

            if ((((p->recgroup == m_recGroup) ||
                  ((m_recGroup == "All Programs") &&
                   (p->recgroup != "Deleted") &&
                   (p->recgroup != "LiveTV" || LiveTVInAllPrograms))) &&
                 (m_recGroupPassword == m_curGroupPassword)) ||
                ((m_recGroupType[m_recGroup] == "category") &&
                 ((p->category == m_recGroup ) ||
                  ((p->category.isEmpty()) && (m_recGroup == tr("Unknown")))) &&
                 ( !m_recGroupPwCache.contains(p->recgroup))))
            {
                if (m_viewMask != VIEW_NONE)
                    m_progLists[""].prepend(p);

                asKey = p->MakeUniqueKey();
                if (asCache.contains(asKey))
                    p->availableStatus = asCache[asKey];
                else
                    p->availableStatus = asAvailable;

                if ((m_viewMask & VIEW_TITLES) && // Show titles
                    ((p->recgroup != "LiveTV") ||
                     (m_recGroup == "LiveTV") ||
                     ((m_recGroup == "All Programs") &&
                      ((m_viewMask & VIEW_LIVETVGRP) == 0))))
                {
                    sTitle = sortTitle(p->title, m_viewMask, titleSort,
                            p->recpriority);
                    sTitle = sTitle.toLower();

                    if (!sortedList.contains(sTitle))
                        sortedList[sTitle] = p->title;
                    m_progLists[sortedList[sTitle].toLower()].prepend(p);
                    m_progLists[sortedList[sTitle].toLower()].setAutoDelete(false);
                }

                if ((m_viewMask & VIEW_RECGROUPS) &&
                    !p->recgroup.isEmpty()) // Show recording groups
                {
                    sortedList[p->recgroup.toLower()] = p->recgroup;
                    m_progLists[p->recgroup.toLower()].prepend(p);
                    m_progLists[p->recgroup.toLower()].setAutoDelete(false);
                }

                if ((m_viewMask & VIEW_CATEGORIES) &&
                    !p->category.isEmpty()) // Show categories
                {
                    sortedList[p->category.toLower()] = p->category;
                    m_progLists[p->category.toLower()].prepend(p);
                    m_progLists[p->category.toLower()].setAutoDelete(false);
                }

                if ((m_viewMask & VIEW_SEARCHES) &&
                    !searchRule[p->recordid].isEmpty() &&
                    p->title != searchRule[p->recordid]) // Show search rules
                {
                    QString tmpTitle = QString("(%1)")
                                               .arg(searchRule[p->recordid]);
                    sortedList[tmpTitle.toLower()] = tmpTitle;
                    m_progLists[tmpTitle.toLower()].prepend(p);
                    m_progLists[tmpTitle.toLower()].setAutoDelete(false);
                }

                if ((LiveTVInAllPrograms) &&
                    (m_recGroup == "All Programs") &&
                    (m_viewMask & VIEW_LIVETVGRP) &&
                    (p->recgroup == "LiveTV"))
                {
                    QString tmpTitle = QString(" %1").arg(tr("LiveTV"));
                    sortedList[tmpTitle.toLower()] = tmpTitle;
                    m_progLists[tmpTitle.toLower()].prepend(p);
                    m_progLists[tmpTitle.toLower()].setAutoDelete(false);
                }

                if ((m_viewMask & VIEW_WATCHLIST) && (p->recgroup != "LiveTV"))
                {
                    if (m_watchListAutoExpire && !(p->programflags & FL_AUTOEXP))
                    {
                        p->recpriority2 = wlExpireOff;
                        VERBOSE(VB_FILE, QString("Auto-expire off:  %1")
                                                 .arg(p->title));
                    }
                    else if (p->programflags & FL_WATCHED)
                    {
                        p->recpriority2 = wlWatched;
                        VERBOSE(VB_FILE, QString("Marked as 'watched':  %1")
                                                 .arg(p->title));
                    }
                    else
                    {
                        if (p->recordid > 0)
                            recidEpisodes[p->recordid] += 1;
                        if (recidEpisodes[p->recordid] == 1 ||
                            p->recordid == 0 )
                        {
                            sortedList[m_watchGroupLabel] = m_watchGroupName;
                            m_progLists[m_watchGroupLabel].prepend(p);
                            m_progLists[m_watchGroupLabel].setAutoDelete(false);
                        }
                        else
                        {
                            p->recpriority2 = wlEarlier;
                            VERBOSE(VB_FILE, QString("Not the earliest:  %1")
                                                     .arg(p->title));
                        }
                    }
                }
            }
        }
    }
    m_progCacheLock.unlock();

    if (sortedList.size() == 0)
    {
        VERBOSE(VB_IMPORTANT, "SortedList is Empty");
        m_progLists[""];
        m_titleList << "";
        m_playList.clear();

        m_isFilling = false;
        return false;
    }

    QString episodeSort = gContext->GetSetting("PlayBoxEpisodeSort", "Date");

    if (episodeSort == "OrigAirDate")
    {
        QMap<QString, ProgramList>::Iterator Iprog;
        for (Iprog = m_progLists.begin(); Iprog != m_progLists.end(); ++Iprog)
        {
            if (!Iprog.key().isEmpty())
            {
                if (m_listOrder == 0 || m_type == Delete)
                    (*Iprog).sort(comp_originalAirDate_rev_less_than);
                else
                    (*Iprog).sort(comp_originalAirDate_less_than);
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
                if (m_listOrder == 0 || m_type == Delete)
                    (*Iprog).sort(comp_programid_rev_less_than);
                else
                    (*Iprog).sort(comp_programid_less_than);
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
                if (!m_listOrder || m_type == Delete)
                    (*it).sort(comp_recordDate_rev_less_than);
                else
                    (*it).sort(comp_recordDate_less_than);
            }
        }
    }

    if (m_progLists[m_watchGroupLabel].size() > 1)
    {
        QDateTime now = QDateTime::currentDateTime();
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

                QDateTime next_record = query.value(4).toDateTime();
                QDateTime last_record = query.value(5).toDateTime();
                QDateTime last_delete = query.value(6).toDateTime();

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
            int recid = (*pit)->recordid;
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
            if ((*pit)->recordid == 0 || maxEpisodes[recid] > 0)
                (*pit)->recpriority2 = 0;
            else
            {
                (*pit)->recpriority2 =
                    (recidEpisodes[(*pit)->recordid] - 1) * baseValue;
            }

            // add points every 3hr leading up to the next recording
            if (nextHours[recid] > 0 && nextHours[recid] < baseValue * 3)
                (*pit)->recpriority2 += (baseValue * 3 - nextHours[recid]) / 3;

            int hrs = (*pit)->endts.secsTo(now) / 3600;
            if (hrs < 1)
                hrs = 1;

            // add points for a new recording that decrease each hour
            if (hrs < 42)
                (*pit)->recpriority2 += 42 - hrs;

            // add points for how close the recorded time of day is to 'now'
            (*pit)->recpriority2 += abs((hrs % 24) - 12) * 2;

            // Daily
            if (spanHours[recid] < 50 ||
                recType[recid] == kTimeslotRecord ||
                recType[recid] == kFindDailyRecord)
            {
                if (delHours[recid] < m_watchListBlackOut * 4)
                {
                    (*pit)->recpriority2 = wlDeleted;
                    VERBOSE(VB_FILE, QString("Recently deleted daily:  %1")
                                             .arg((*pit)->title));
                    pit = m_progLists[m_watchGroupLabel].erase(pit);
                    continue;
                }
                else
                {
                    VERBOSE(VB_FILE, QString("Daily interval:  %1")
                                             .arg((*pit)->title));

                    if (maxEpisodes[recid] > 0)
                        (*pit)->recpriority2 += (baseValue / 2) + (hrs / 24);
                    else
                        (*pit)->recpriority2 += (baseValue / 5) + hrs;
                }
            }
            // Weekly
            else if (nextHours[recid] ||
                     recType[recid] == kWeekslotRecord ||
                     recType[recid] == kFindWeeklyRecord)

            {
                if (delHours[recid] < (m_watchListBlackOut * 24) - 4)
                {
                    (*pit)->recpriority2 = wlDeleted;
                    VERBOSE(VB_FILE, QString("Recently deleted weekly:  %1")
                                             .arg((*pit)->title));
                    pit = m_progLists[m_watchGroupLabel].erase(pit);
                    continue;
                }
                else
                {
                    VERBOSE(VB_FILE, QString("Weekly interval: %1")
                                             .arg((*pit)->title));

                    if (maxEpisodes[recid] > 0)
                        (*pit)->recpriority2 += (baseValue / 2) + (hrs / 24);
                    else
                        (*pit)->recpriority2 +=
                            (baseValue / 3) + (baseValue * hrs / 24 / 4);
                }
            }
            // Not recurring
            else
            {
                if (delHours[recid] < (m_watchListBlackOut * 48) - 4)
                {
                    (*pit)->recpriority2 = wlDeleted;
                    pit = m_progLists[m_watchGroupLabel].erase(pit);
                    continue;
                }
                else
                {
                    // add points for a new Single or final episode
                    if (hrs < 36)
                        (*pit)->recpriority2 += baseValue * (36 - hrs) / 36;

                    if (avgd != 100)
                    {
                        if (maxEpisodes[recid] > 0)
                            (*pit)->recpriority2 += (baseValue / 2) + (hrs / 24);
                        else
                            (*pit)->recpriority2 +=
                                (baseValue / 3) + (baseValue * hrs / 24 / 4);
                    }
                    else if ((hrs / 24) < m_watchListMaxAge)
                        (*pit)->recpriority2 += hrs / 24;
                    else
                        (*pit)->recpriority2 += m_watchListMaxAge;
                }
            }

            // Factor based on the average time shift delay.
            // Scale the avgd range of 0 thru 200 hours to 133% thru 67%
            int delaypct = avgd / 3 + 67;

            if (avgd < 100)
            {
                (*pit)->recpriority2 =
                    (*pit)->recpriority2 * (200 - delaypct) / 100;
            }
            else if (avgd > 100)
                (*pit)->recpriority2 = (*pit)->recpriority2 * 100 / delaypct;

            VERBOSE(VB_FILE, QString(" %1  %2  %3")
                    .arg((*pit)->startts.toString(m_formatShortDate))
                    .arg((*pit)->recpriority2).arg((*pit)->title));

            ++pit;
        }
        m_progLists[m_watchGroupLabel].sort(comp_recpriority2_less_than);
    }

    m_titleList = sortedList.values();

    updateGroupList();
    updateUsage();

    m_isFilling = false;

    return (m_progCache != NULL);
}

void PlaybackBox::playSelectedPlaylist(bool random)
{
    ProgramInfo *tmpItem;
    QStringList::Iterator it = m_playList.begin();
    QStringList randomList = m_playList;
    bool playNext = true;
    int i = 0;

    while (randomList.size() && playNext)
    {
        if (random)
            i = (int)(1.0 * randomList.size() * rand() / (RAND_MAX + 1.0));

        it = randomList.begin() + i;

        tmpItem = findMatchingProg(*it);

        if ((tmpItem->availableStatus == asAvailable) ||
            (tmpItem->availableStatus == asNotYetAvailable))
            playNext = play(tmpItem, true);

        randomList.erase(it);
    }
}

void PlaybackBox::playSelected(MythUIButtonListItem *item)
{
    if (!item)
        return;

    item = m_recordingList->GetItemCurrent();

    if (!item)
        return;

    ProgramInfo *pginfo = qVariantValue<ProgramInfo *>(item->GetData());

    if (!pginfo)
        return;

    if (m_player && m_player->IsSameProgram(0, pginfo))
    {
        Close();
        return;
    }

    if ((pginfo->availableStatus == asAvailable) ||
        (pginfo->availableStatus == asNotYetAvailable))
        play(pginfo);
    else
        showAvailablePopup(pginfo);

    if (m_player)
    {
    }
}

void PlaybackBox::stopSelected()
{
    ProgramInfo *pginfo = CurrentItem();

    if (!pginfo)
        return;

    stop(pginfo);
}

void PlaybackBox::deleteSelected(MythUIButtonListItem *item)
{
    if (!item)
        return;

    ProgramInfo *pginfo = qVariantValue<ProgramInfo *>(item->GetData());

    if (!pginfo)
        return;

    bool undelete_possible =
            gContext->GetNumSetting("AutoExpireInsteadOfDelete", 0);

    if (pginfo->recgroup == "Deleted" && undelete_possible)
    {
        m_delItem = pginfo;
        doRemove(pginfo, false, false);
    }
    else if ((pginfo->availableStatus != asPendingDelete) &&
        (REC_CAN_BE_DELETED(pginfo)))
        askDelete(pginfo);
    else
        showAvailablePopup(pginfo);
}

void PlaybackBox::upcoming()
{
   ProgramInfo *pginfo = CurrentItem();

    if (!pginfo)
        return;

    if (pginfo->availableStatus != asAvailable)
    {
        showAvailablePopup(pginfo);
        return;
    }

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ProgLister *pl = new ProgLister(mainStack, plTitle, pginfo->title, "");
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
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

    if (!pginfo)
        return;

    if (pginfo->availableStatus != asAvailable)
    {
        showAvailablePopup(pginfo);
        return;
    }
    ProgramInfo *pi = pginfo;

    if (!pi)
        return;

    CustomEdit *ce = new CustomEdit(gContext->GetMainWindow(),
                                    "customedit", pi);
    ce->exec();
    delete ce;
}

void PlaybackBox::details()
{
    ProgramInfo *pginfo = CurrentItem();

    if (!pginfo)
        return;

    if (pginfo->availableStatus != asAvailable)
        showAvailablePopup(pginfo);
    else
        pginfo->showDetails();
}

void PlaybackBox::selected(MythUIButtonListItem *item)
{
    if (!item)
        return;

    switch (m_type)
    {
        case Play: playSelected(item); break;
        case Delete: deleteSelected(item); break;
    }
}

void PlaybackBox::showMenu()
{
    QString label = tr("Recording List Menu");

    ProgramInfo *pginfo = CurrentItem();

    m_popupMenu = new MythDialogBox(label, m_popupStack, "pbbmainmenupopup");

    if (m_popupMenu->Create())
        m_popupStack->AddScreen(m_popupMenu);

    m_popupMenu->SetReturnEvent(this, "slotmenu");

    m_popupMenu->AddButton(tr("Change Group Filter"),
                                 SLOT(showGroupFilter()));

    m_popupMenu->AddButton(tr("Change Group View"),
                     SLOT(showViewChanger()));

    if (m_recGroupType[m_recGroup] == "recgroup")
        m_popupMenu->AddButton(tr("Change Group Password"),
                         SLOT(showRecGroupPasswordChanger()));

    if (m_playList.size())
    {
        m_popupMenu->AddButton(tr("Playlist options"),
                         SLOT(showPlaylistPopup()), true);
    }
    else if (!m_player)
    {
        if (GetFocusWidget() == m_groupList)
        {
            m_popupMenu->AddButton(tr("Add this Group to Playlist"),
                             SLOT(togglePlayListTitle()));
        }
        else if (pginfo && pginfo->availableStatus == asAvailable)
        {
            m_popupMenu->AddButton(tr("Add this recording to Playlist"),
                                        SLOT(togglePlayListItem()));
        }
    }

    m_popupMenu->AddButton(tr("Help (Status Icons)"), SLOT(showIconHelp()));
}

void PlaybackBox::showActionsSelected()
{
    if (GetFocusWidget() == m_groupList)
        return;

    ProgramInfo *pginfo = CurrentItem();

    if (!pginfo)
        return;

    if ((pginfo->availableStatus != asAvailable) &&
        (pginfo->availableStatus != asFileNotFound))
        showAvailablePopup(pginfo);
    else
        showActions(pginfo);
}

bool PlaybackBox::play(ProgramInfo *rec, bool inPlaylist)
{
    bool playCompleted = false;

    if (!rec)
        return false;

    if (m_player)
        return true;

    rec->pathname = rec->GetPlaybackURL(true);

    if (rec->availableStatus == asNotYetAvailable)
        rec->availableStatus = asAvailable;

    if (fileExists(rec) == false)
    {
        VERBOSE(VB_IMPORTANT, QString("PlaybackBox::play(): Error, %1 file "
                                      "not found").arg(rec->pathname));

        if (rec->recstatus == rsRecording)
            rec->availableStatus = asNotYetAvailable;
        else
            rec->availableStatus = asFileNotFound;

        showAvailablePopup(rec);

        return false;
    }

    if (rec->filesize == 0)
    {
        VERBOSE(VB_IMPORTANT,
            QString("PlaybackBox::play(): Error, %1 is zero-bytes in size")
            .arg(rec->pathname));

        if (rec->recstatus == rsRecording)
            rec->availableStatus = asNotYetAvailable;
        else
            rec->availableStatus = asZeroByte;

        showAvailablePopup(rec);

        return false;
    }

    ProgramInfo *tvrec = new ProgramInfo(*rec);

    m_playingSomething = true;

    playCompleted = TV::StartTV(tvrec, false, inPlaylist, m_underNetworkControl);

    m_playingSomething = false;

    delete tvrec;

    // TODO 1) Potentially duplicate refill, watching a recording will normally
    //      trigger a recording list change event which also triggers a refill
    //
    //      2) We really don't need to do a refill here, but just update that
    //      one programinfo object
    m_connected = FillList();

    return playCompleted;
}

void PlaybackBox::stop(ProgramInfo *rec)
{
    RemoteStopRecording(rec);
}

bool PlaybackBox::doRemove(ProgramInfo *rec, bool forgetHistory,
                           bool forceMetadataDelete)
{
    m_delItem = NULL;

    if (!rec)
        return false;

    if (!forceMetadataDelete &&
        ((rec->availableStatus == asPendingDelete) ||
        (!REC_CAN_BE_DELETED(rec))))
    {
        showAvailablePopup(rec);
        return false;
    }

    if (m_playList.filter(rec->MakeUniqueKey()).size())
        togglePlayListItem(rec);

    if (!forceMetadataDelete)
        rec->UpdateLastDelete(true);

    bool result = RemoteDeleteRecording(rec, forgetHistory, forceMetadataDelete);

    if (result)
    {
        rec->availableStatus = asPendingDelete;
        m_recordingList->RemoveItem(m_recordingList->GetItemCurrent());
    }
    else if (!forceMetadataDelete)
        showDeletePopup(rec, ForceDeleteRecording);

    return result;
}

void PlaybackBox::showActions(ProgramInfo *pginfo)
{
    if (!pginfo)
        return;

    if (fileExists(pginfo) == false)
    {
        VERBOSE(VB_IMPORTANT, QString("PlaybackBox::showActions(): Error, %1 "
                                      "file not found").arg(pginfo->pathname));

        pginfo->availableStatus = asFileNotFound;
        showFileNotFoundActionPopup(pginfo);
    }
    else if (pginfo->availableStatus != asAvailable)
        showAvailablePopup(pginfo);
    else
        showActionPopup(pginfo);
}

void PlaybackBox::showDeletePopup(ProgramInfo *program, deletePopupType types)
{
    QString label;
    switch (types)
    {
        case DeleteRecording:
             label = tr("Are you sure you want to delete:"); break;
        case ForceDeleteRecording:
             label = tr("ERROR: Recorded file does not exist.\n"
                        "Are you sure you want to delete:");
             break;
        case StopRecording:
             label = tr("Are you sure you want to stop:"); break;
    }

    m_delItem = CurrentItem();

    popupString(m_delItem, label);

    m_popupMenu = new MythDialogBox(label, m_popupStack, "pbbmainmenupopup");

    if (m_popupMenu->Create())
        m_popupStack->AddScreen(m_popupMenu);

    m_popupMenu->SetReturnEvent(this, "slotmenu");

    m_freeSpaceNeedsUpdate = true;

    QString tmpmessage;
    const char *tmpslot = NULL;

    if ((types == DeleteRecording) &&
        (program->IsSameProgram(*program)) &&
        (program->recgroup != "LiveTV"))
    {
        tmpmessage = tr("Yes, and allow re-record");
        tmpslot = SLOT(doDeleteForgetHistory());
        m_popupMenu->AddButton(tmpmessage, tmpslot);
    }

    switch (types)
    {
        case DeleteRecording:
             tmpmessage = tr("Yes, delete it");
             tmpslot = SLOT(doDelete());
             break;
        case ForceDeleteRecording:
             tmpmessage = tr("Yes, delete it");
             tmpslot = SLOT(doForceDelete());
             break;
        case StopRecording:
             tmpmessage = tr("Yes, stop recording it");
             tmpslot = SLOT(doStop());
             break;
    }

    m_popupMenu->AddButton(tmpmessage, tmpslot);

    switch (types)
    {
        case DeleteRecording:
        case ForceDeleteRecording:
             tmpmessage = tr("No, keep it, I changed my mind");
             tmpslot = SLOT(noDelete());
             break;
        case StopRecording:
             tmpmessage = tr("No, continue recording it");
             tmpslot = SLOT(noStop());
             break;
    }
    m_popupMenu->AddButton(tmpmessage, tmpslot);
}

void PlaybackBox::showAvailablePopup(ProgramInfo *rec)
{
    if (!rec)
        return;

    QString msg = rec->title;
    if (!rec->subtitle.isEmpty())
        msg += " \"" + rec->subtitle + "\"";
    msg += "\n";

    switch (rec->availableStatus)
    {
        case asAvailable:
                 if (rec->programflags & (FL_INUSERECORDING | FL_INUSEPLAYING))
                 {
                     QString byWho;
                     rec->IsInUse(byWho);

                     ShowOkPopup(tr("Recording Available\n") + msg +
                                 tr("This recording is currently in "
                                               "use by:") + "\n" + byWho);
                 }
                 else
                 {
                     ShowOkPopup(tr("Recording Available\n") + msg +
                                 tr("This recording is currently "
                                               "Available"));
                 }
                 break;
        case asPendingDelete:
                 ShowOkPopup(tr("Recording Unavailable\n") + msg +
                             tr("This recording is currently being "
                                           "deleted and is unavailable"));
                 break;
        case asFileNotFound:
                 ShowOkPopup(tr("Recording Unavailable\n") + msg +
                             tr("The file for this recording can "
                                           "not be found"));
                 break;
        case asZeroByte:
                 ShowOkPopup(tr("Recording Unavailable\n") + msg +
                             tr("The file for this recording is "
                                           "empty."));
                 break;
        case asNotYetAvailable:
                 ShowOkPopup(tr("Recording Unavailable\n") + msg +
                             tr("This recording is not yet "
                                           "available."));
    }
}

void PlaybackBox::showPlaylistPopup()
{
    QString label;
    if (m_playList.size() > 1)
        label = tr("There are %1 items in the playlist.").arg(m_playList.size());
    else
        label = tr("There is %1 item in the playlist.").arg(m_playList.size());

    label.append(tr("Actions affect all items in the playlist"));

    m_popupMenu = new MythDialogBox(label, m_popupStack, "pbbmainmenupopup");

    if (m_popupMenu->Create())
        m_popupStack->AddScreen(m_popupMenu);

    m_popupMenu->SetReturnEvent(this, "slotmenu");

    m_popupMenu->AddButton(tr("Play"), SLOT(doPlayList()));
    m_popupMenu->AddButton(tr("Shuffle Play"), SLOT(doPlayListRandom()));
    m_popupMenu->AddButton(tr("Clear Playlist"), SLOT(doClearPlaylist()));

    if (GetFocusWidget() == m_groupList)
    {
        if ((m_viewMask & VIEW_TITLES))
            m_popupMenu->AddButton(tr("Toggle playlist for this Category/Title"),
                                SLOT(togglePlayListTitle()));
        else
            m_popupMenu->AddButton(tr("Toggle playlist for this Recording Group"),
                                SLOT(togglePlayListTitle()));
    }
    else
        m_popupMenu->AddButton(tr("Toggle playlist for this recording"),
                         SLOT(togglePlayListItem()));

    m_popupMenu->AddButton(tr("Change Recording Group"),
                     SLOT(doPlaylistChangeRecGroup()));
    m_popupMenu->AddButton(tr("Change Playback Group"),
                     SLOT(doPlaylistChangePlayGroup()));
    m_popupMenu->AddButton(tr("Job Options"),
                     SLOT(showPlaylistJobPopup()), true);
    m_popupMenu->AddButton(tr("Delete"), SLOT(doPlaylistDelete()));
    m_popupMenu->AddButton(tr("Delete, and allow re-record"),
                     SLOT(doPlaylistDeleteForgetHistory()));
}

void PlaybackBox::showPlaylistJobPopup()
{
    QString label;
    if (m_playList.size() > 1)
        label = tr("There are %1 items in the playlist.").arg(m_playList.size());
    else
        label = tr("There is %1 item in the playlist.").arg(m_playList.size());

    m_popupMenu = new MythDialogBox(label, m_popupStack, "pbbmainmenupopup");

    if (m_popupMenu->Create())
        m_popupStack->AddScreen(m_popupMenu);

    m_popupMenu->SetReturnEvent(this, "slotmenu");

    QString jobTitle;
    QString command;
    QStringList::Iterator it;
    ProgramInfo *tmpItem;
    bool isTranscoding = true;
    bool isFlagging = true;
    bool isRunningUserJob1 = true;
    bool isRunningUserJob2 = true;
    bool isRunningUserJob3 = true;
    bool isRunningUserJob4 = true;

    for(it = m_playList.begin(); it != m_playList.end(); ++it)
    {
        tmpItem = findMatchingProg(*it);
        if (tmpItem)
        {
            if (!JobQueue::IsJobQueuedOrRunning(JOB_TRANSCODE,
                                       tmpItem->chanid, tmpItem->recstartts))
                isTranscoding = false;
            if (!JobQueue::IsJobQueuedOrRunning(JOB_COMMFLAG,
                                       tmpItem->chanid, tmpItem->recstartts))
                isFlagging = false;
            if (!JobQueue::IsJobQueuedOrRunning(JOB_USERJOB1,
                                       tmpItem->chanid, tmpItem->recstartts))
                isRunningUserJob1 = false;
            if (!JobQueue::IsJobQueuedOrRunning(JOB_USERJOB2,
                                       tmpItem->chanid, tmpItem->recstartts))
                isRunningUserJob2 = false;
            if (!JobQueue::IsJobQueuedOrRunning(JOB_USERJOB3,
                                       tmpItem->chanid, tmpItem->recstartts))
                isRunningUserJob3 = false;
            if (!JobQueue::IsJobQueuedOrRunning(JOB_USERJOB4,
                                       tmpItem->chanid, tmpItem->recstartts))
                isRunningUserJob4 = false;
            if (!isTranscoding && !isFlagging && !isRunningUserJob1 &&
                !isRunningUserJob2 && !isRunningUserJob3 && !isRunningUserJob4)
                break;
        }
    }
    if (!isTranscoding)
        m_popupMenu->AddButton(tr("Begin Transcoding"),
                         SLOT(doPlaylistBeginTranscoding()));
    else
        m_popupMenu->AddButton(tr("Stop Transcoding"),
                         SLOT(stopPlaylistTranscoding()));
    if (!isFlagging)
        m_popupMenu->AddButton(tr("Begin Commercial Flagging"),
                         SLOT(doPlaylistBeginFlagging()));
    else
        m_popupMenu->AddButton(tr("Stop Commercial Flagging"),
                         SLOT(stopPlaylistFlagging()));

    command = gContext->GetSetting("UserJob1", "");
    if (!command.isEmpty())
    {
        jobTitle = gContext->GetSetting("UserJobDesc1");

        if (!isRunningUserJob1)
            m_popupMenu->AddButton(tr("Begin") + " " + jobTitle,
                             SLOT(doPlaylistBeginUserJob1()));
        else
            m_popupMenu->AddButton(tr("Stop") + " " + jobTitle,
                             SLOT(stopPlaylistUserJob1()));
    }

    command = gContext->GetSetting("UserJob2", "");
    if (!command.isEmpty())
    {
        jobTitle = gContext->GetSetting("UserJobDesc2");

        if (!isRunningUserJob2)
            m_popupMenu->AddButton(tr("Begin") + " " + jobTitle,
                             SLOT(doPlaylistBeginUserJob2()));
        else
            m_popupMenu->AddButton(tr("Stop") + " " + jobTitle,
                             SLOT(stopPlaylistUserJob2()));
    }

    command = gContext->GetSetting("UserJob3", "");
    if (!command.isEmpty())
    {
        jobTitle = gContext->GetSetting("UserJobDesc3");

        if (!isRunningUserJob3)
            m_popupMenu->AddButton(tr("Begin") + " " + jobTitle,
                             SLOT(doPlaylistBeginUserJob3()));
        else
            m_popupMenu->AddButton(tr("Stop") + " " + jobTitle,
                             SLOT(stopPlaylistUserJob3()));
    }

    command = gContext->GetSetting("UserJob4", "");
    if (!command.isEmpty())
    {
        jobTitle = gContext->GetSetting("UserJobDesc4");

        if (!isRunningUserJob4)
            m_popupMenu->AddButton(QString("%1 %2")
                                    .arg(tr("Begin")).arg(jobTitle),
                                     SLOT(doPlaylistBeginUserJob4()));
        else
            m_popupMenu->AddButton(QString("%1 %2")
                                    .arg(tr("Stop")).arg(jobTitle),
                                    SLOT(stopPlaylistUserJob4()));
    }
}

void PlaybackBox::showPlayFromPopup()
{
    QString label = tr("Play options");

    ProgramInfo *pginfo = CurrentItem();
    popupString(pginfo, label);

    m_popupMenu = new MythDialogBox(label, m_popupStack, "pbbmainmenupopup");

    if (m_popupMenu->Create())
        m_popupStack->AddScreen(m_popupMenu);

    m_popupMenu->SetReturnEvent(this, "slotmenu");

    m_popupMenu->AddButton(tr("Play from bookmark"), SLOT(playSelected()));
    m_popupMenu->AddButton(tr("Play from beginning"), SLOT(doPlayFromBeg()));
}

void PlaybackBox::showStoragePopup()
{
    QString label = tr("Storage options");

    ProgramInfo *pginfo = CurrentItem();
    popupString(pginfo, label);

    m_popupMenu = new MythDialogBox(label, m_popupStack, "pbbmainmenupopup");

    if (m_popupMenu->Create())
        m_popupStack->AddScreen(m_popupMenu);

    m_popupMenu->SetReturnEvent(this, "slotmenu");

    m_popupMenu->AddButton(tr("Change Recording Group"),
                            SLOT(showRecGroupChanger()));

    m_popupMenu->AddButton(tr("Change Playback Group"),
                            SLOT(showPlayGroupChanger()));

    if (pginfo)
    {
        if (pginfo->programflags & FL_AUTOEXP)
            m_popupMenu->AddButton(tr("Disable Auto Expire"),
                                            SLOT(toggleAutoExpire()));
        else
            m_popupMenu->AddButton(tr("Enable Auto Expire"),
                                            SLOT(toggleAutoExpire()));

        if (pginfo->programflags & FL_PRESERVED)
            m_popupMenu->AddButton(tr("Do not preserve this episode"),
                                            SLOT(togglePreserveEpisode()));
        else
            m_popupMenu->AddButton(tr("Preserve this episode"),
                                            SLOT(togglePreserveEpisode()));
    }
}

void PlaybackBox::showRecordingPopup()
{
    QString label = tr("Scheduling options");

    ProgramInfo *pginfo = CurrentItem();
    popupString(pginfo, label);

    m_popupMenu = new MythDialogBox(label, m_popupStack, "pbbmainmenupopup");

    if (m_popupMenu->Create())
        m_popupStack->AddScreen(m_popupMenu);

    m_popupMenu->SetReturnEvent(this, "slotmenu");

    m_popupMenu->AddButton(tr("Edit Recording Schedule"),
                            SLOT(doEditScheduled()));

    m_popupMenu->AddButton(tr("Allow this program to re-record"),
                            SLOT(doAllowRerecord()));

    m_popupMenu->AddButton(tr("Show Program Details"),
                            SLOT(showProgramDetails()));

    m_popupMenu->AddButton(tr("Change Recording Title"),
                            SLOT(showMetadataEditor()));
}

void PlaybackBox::showJobPopup()
{
    QString label = tr("Job options");

    ProgramInfo *pginfo = CurrentItem();
    popupString(pginfo, label);

    if (!pginfo)
        return;

    m_popupMenu = new MythDialogBox(label, m_popupStack, "pbbmainmenupopup");

    if (m_popupMenu->Create())
        m_popupStack->AddScreen(m_popupMenu);

    m_popupMenu->SetReturnEvent(this, "slotmenu");

    QString jobTitle;
    QString command;

    if (JobQueue::IsJobQueuedOrRunning(JOB_TRANSCODE, pginfo->chanid,
                                                  pginfo->recstartts))
        m_popupMenu->AddButton(tr("Stop Transcoding"),
                                    SLOT(doBeginTranscoding()));
    else
        m_popupMenu->AddButton(tr("Begin Transcoding"),
                                    SLOT(showTranscodingProfiles()));

    if (JobQueue::IsJobQueuedOrRunning(JOB_COMMFLAG, pginfo->chanid,
                                                  pginfo->recstartts))
        m_popupMenu->AddButton(tr("Stop Commercial Flagging"),
                                    SLOT(doBeginFlagging()));
    else
        m_popupMenu->AddButton(tr("Begin Commercial Flagging"),
                                    SLOT(doBeginFlagging()));

    command = gContext->GetSetting("UserJob1", "");
    if (!command.isEmpty())
    {
        jobTitle = gContext->GetSetting("UserJobDesc1", tr("User Job") + " #1");

        if (JobQueue::IsJobQueuedOrRunning(JOB_USERJOB1, pginfo->chanid,
                                   pginfo->recstartts))
            m_popupMenu->AddButton(tr("Stop") + " " + jobTitle,
                                    SLOT(doBeginUserJob1()));
        else
            m_popupMenu->AddButton(tr("Begin") + " " + jobTitle,
                                    SLOT(doBeginUserJob1()));
    }

    command = gContext->GetSetting("UserJob2", "");
    if (!command.isEmpty())
    {
        jobTitle = gContext->GetSetting("UserJobDesc2", tr("User Job") + " #2");

        if (JobQueue::IsJobQueuedOrRunning(JOB_USERJOB2, pginfo->chanid,
                                   pginfo->recstartts))
            m_popupMenu->AddButton(tr("Stop") + " " + jobTitle,
                                    SLOT(doBeginUserJob2()));
        else
            m_popupMenu->AddButton(tr("Begin") + " " + jobTitle,
                                    SLOT(doBeginUserJob2()));
    }

    command = gContext->GetSetting("UserJob3", "");
    if (!command.isEmpty())
    {
        jobTitle = gContext->GetSetting("UserJobDesc3", tr("User Job") + " #3");

        if (JobQueue::IsJobQueuedOrRunning(JOB_USERJOB3, pginfo->chanid,
                                   pginfo->recstartts))
            m_popupMenu->AddButton(tr("Stop") + " " + jobTitle,
                                    SLOT(doBeginUserJob3()));
        else
            m_popupMenu->AddButton(tr("Begin") + " " + jobTitle,
                                    SLOT(doBeginUserJob3()));
    }

    command = gContext->GetSetting("UserJob4", "");
    if (!command.isEmpty())
    {
        jobTitle = gContext->GetSetting("UserJobDesc4", tr("User Job") + " #4");

        if (JobQueue::IsJobQueuedOrRunning(JOB_USERJOB4, pginfo->chanid,
                                   pginfo->recstartts))
            m_popupMenu->AddButton(tr("Stop") + " " + jobTitle,
                                    SLOT(doBeginUserJob4()));
        else
            m_popupMenu->AddButton(tr("Begin") + " "  + jobTitle,
                                    SLOT(doBeginUserJob4()));
    }
}

void PlaybackBox::showTranscodingProfiles()
{
    QString label = tr("Transcoding profiles");

    m_popupMenu = new MythDialogBox(label, m_popupStack, "pbbmainmenupopup");

    if (m_popupMenu->Create())
        m_popupStack->AddScreen(m_popupMenu);

    m_popupMenu->SetReturnEvent(this, "slotmenu");

    m_popupMenu->AddButton(tr("Default"), SLOT(doBeginTranscoding()));
    m_popupMenu->AddButton(tr("Autodetect"),
                                    SLOT(changeProfileAndTranscodeAuto()));
    m_popupMenu->AddButton(tr("High Quality"),
                                    SLOT(changeProfileAndTranscodeHigh()));
    m_popupMenu->AddButton(tr("Medium Quality"),
                                    SLOT(changeProfileAndTranscodeMedium()));
    m_popupMenu->AddButton(tr("Low Quality"),
                                    SLOT(changeProfileAndTranscodeLow()));
}

void PlaybackBox::changeProfileAndTranscode(const QString &profile)
{
   ProgramInfo *pginfo = CurrentItem();

    if (!pginfo)
        return;

    pginfo->ApplyTranscoderProfileChange(profile);
    doBeginTranscoding();
}

void PlaybackBox::showActionPopup(ProgramInfo *pginfo)
{
    if (!pginfo)
        return;

    QString label = tr("Recording Options");

    popupString(pginfo, label);

    m_popupMenu = new MythDialogBox(label, m_popupStack, "pbbmainmenupopup");

    if (m_popupMenu->Create())
        m_popupStack->AddScreen(m_popupMenu);

    m_popupMenu->SetReturnEvent(this, "slotmenu");

    if (!(m_player && m_player->IsSameProgram(0, pginfo)))
    {
        if (pginfo->programflags & FL_BOOKMARK)
            m_popupMenu->AddButton(tr("Play from..."),
                                        SLOT(showPlayFromPopup()), true);
        else
            m_popupMenu->AddButton(tr("Play"), SLOT(playSelected()));
    }

    if (!m_player)
    {
        if (m_playList.filter(pginfo->MakeUniqueKey()).size())
            m_popupMenu->AddButton(tr("Remove from Playlist"),
                                        SLOT(togglePlayListItem()));
        else
            m_popupMenu->AddButton(tr("Add to Playlist"),
                                        SLOT(togglePlayListItem()));
    }

    TVState m_tvstate = kState_None;
    if (m_player)
        m_tvstate = m_player->GetState(0);

    if (pginfo->recstatus == rsRecording &&
        (!(m_player &&
            (m_tvstate == kState_WatchingLiveTV ||
            m_tvstate == kState_WatchingRecording) &&
            m_player->IsSameProgram(0, pginfo))))
    {
        m_popupMenu->AddButton(tr("Stop Recording"), SLOT(askStop()));
    }

    if (pginfo->programflags & FL_WATCHED)
        m_popupMenu->AddButton(tr("Mark as Unwatched"), SLOT(toggleWatched()));
    else
        m_popupMenu->AddButton(tr("Mark as Watched"), SLOT(toggleWatched()));

    m_popupMenu->AddButton(tr("Storage Options"), SLOT(showStoragePopup()),
                           true);
    m_popupMenu->AddButton(tr("Recording Options"), SLOT(showRecordingPopup()),
                           true);
    m_popupMenu->AddButton(tr("Job Options"), SLOT(showJobPopup()), true);

    if (!(m_player && m_player->IsSameProgram(0, pginfo)))
    {
        if (pginfo->recgroup == "Deleted")
        {
            m_delItem = pginfo;
            m_popupMenu->AddButton(tr("Undelete"), SLOT(doUndelete()));
            m_popupMenu->AddButton(tr("Delete Forever"), SLOT(doDelete()));
        }
        else
        {
            m_popupMenu->AddButton(tr("Delete"), SLOT(askDelete()));
        }
    }
}

void PlaybackBox::showFileNotFoundActionPopup(ProgramInfo *pginfo)
{
    if (!pginfo)
        return;

    QString label = tr("Recording file can not be found");

    popupString(pginfo, label);

    m_popupMenu = new MythDialogBox(label, m_popupStack, "pbbmainmenupopup");

    if (m_popupMenu->Create())
        m_popupStack->AddScreen(m_popupMenu);

    m_popupMenu->SetReturnEvent(this, "slotmenu");

    m_popupMenu->AddButton(tr("Show Program Details"),
                                SLOT(showProgramDetails()));
    m_popupMenu->AddButton(tr("Delete"), SLOT(askDelete()));
}

void PlaybackBox::popupString(ProgramInfo *program, QString &message)
{
    if (!program)
        return;

    QDateTime recstartts = program->recstartts;
    QDateTime recendts = program->recendts;

    QString timedate = QString("%1, %2 - %3")
                        .arg(recstartts.date().toString(m_formatLongDate))
                        .arg(recstartts.time().toString(m_formatTime))
                        .arg(recendts.time().toString(m_formatTime));

    QString title = program->title;

    message = QString("%1\n%2\n%3").arg(message).arg(title).arg(timedate);
}

void PlaybackBox::doClearPlaylist(void)
{
    m_playList.clear();
}

void PlaybackBox::doPlayFromBeg(void)
{
    ProgramInfo *pginfo = CurrentItem();
    pginfo->setIgnoreBookmark(true);
    playSelected(m_recordingList->GetItemCurrent());
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
    showDeletePopup(pginfo, StopRecording);
}

void PlaybackBox::doStop(void)
{
    ProgramInfo *pginfo = CurrentItem();
    stop(pginfo);
}

void PlaybackBox::showProgramDetails()
{
   ProgramInfo *pginfo = CurrentItem();

    if (!pginfo)
        return;

    pginfo->showDetails();
}

void PlaybackBox::doEditScheduled()
{
   ProgramInfo *pginfo = CurrentItem();

    if (!pginfo)
        return;

    if (pginfo->availableStatus != asAvailable)
    {
        showAvailablePopup(pginfo);
    }
    else
    {
        ScheduledRecording *record = new ScheduledRecording();
        ProgramInfo *t_pginfo = new ProgramInfo(*pginfo);
        record->loadByProgram(t_pginfo);
        record->exec();
        record->deleteLater();

        // TODO Can this be changed to a cached refill?
        m_connected = FillList();
        delete t_pginfo;
    }

}

/** \fn doAllowRerecord
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

    pginfo->ForgetHistory();
}

void PlaybackBox::doJobQueueJob(int jobType, int jobFlags)
{
   ProgramInfo *pginfo = CurrentItem();

    if (!pginfo)
        return;

    ProgramInfo *tmpItem = findMatchingProg(pginfo);

    if (JobQueue::IsJobQueuedOrRunning(jobType, pginfo->chanid,
                               pginfo->recstartts))
    {
        JobQueue::ChangeJobCmds(jobType, pginfo->chanid,
                               pginfo->recstartts, JOB_STOP);
        if ((jobType & JOB_COMMFLAG) && (tmpItem))
        {
            tmpItem->programflags &= ~FL_EDITING;
            tmpItem->programflags &= ~FL_COMMFLAG;
        }
    }
    else
    {
        QString jobHost;
        if (gContext->GetNumSetting("JobsRunOnRecordHost", 0))
            jobHost = pginfo->hostname;

        JobQueue::QueueJob(jobType, pginfo->chanid,
                            pginfo->recstartts, "", "", jobHost,
                            jobFlags);
    }
}

void PlaybackBox::doBeginFlagging()
{
    doJobQueueJob(JOB_COMMFLAG);
}

void PlaybackBox::doPlaylistJobQueueJob(int jobType, int jobFlags)
{
    ProgramInfo *tmpItem;
    QStringList::Iterator it;
    int jobID;

    for (it = m_playList.begin(); it != m_playList.end(); ++it)
    {
        jobID = 0;
        tmpItem = findMatchingProg(*it);
        if (tmpItem &&
            (!JobQueue::IsJobQueuedOrRunning(jobType, tmpItem->chanid,
                                    tmpItem->recstartts)))
    {
            QString jobHost;
            if (gContext->GetNumSetting("JobsRunOnRecordHost", 0))
                jobHost = tmpItem->hostname;

            JobQueue::QueueJob(jobType, tmpItem->chanid,
                               tmpItem->recstartts, "", "", jobHost, jobFlags);
        }
    }
}

void PlaybackBox::stopPlaylistJobQueueJob(int jobType)
{
    ProgramInfo *tmpItem;
    QStringList::Iterator it;
    int jobID;

    for (it = m_playList.begin(); it != m_playList.end(); ++it)
    {
        jobID = 0;
        tmpItem = findMatchingProg(*it);
        if (tmpItem &&
            (JobQueue::IsJobQueuedOrRunning(jobType, tmpItem->chanid,
                                tmpItem->recstartts)))
        {
            JobQueue::ChangeJobCmds(jobType, tmpItem->chanid,
                                     tmpItem->recstartts, JOB_STOP);
            if ((jobType & JOB_COMMFLAG) && (tmpItem))
            {
                tmpItem->programflags &= ~FL_EDITING;
                tmpItem->programflags &= ~FL_COMMFLAG;
            }
        }
    }
}

void PlaybackBox::askDelete(ProgramInfo *pginfo)
{
    if (!pginfo)
        pginfo = CurrentItem();
    showDeletePopup(pginfo, DeleteRecording);
}

void PlaybackBox::doPlaylistDelete(void)
{
    playlistDelete(false);
}

void PlaybackBox::doPlaylistDeleteForgetHistory(void)
{
    playlistDelete(true);
}

void PlaybackBox::playlistDelete(bool forgetHistory)
{
    ProgramInfo *tmpItem;
    QStringList::Iterator it;

    for (it = m_playList.begin(); it != m_playList.end(); ++it )
    {
        tmpItem = findMatchingProg(*it);
        if (tmpItem && (REC_CAN_BE_DELETED(tmpItem)))
            RemoteDeleteRecording(tmpItem, forgetHistory, false);
    }

    m_connected = FillList(true);
    m_playList.clear();
}

void PlaybackBox::doUndelete(void)
{
   ProgramInfo *pginfo = CurrentItem();

    if (!pginfo)
        return;

    RemoteUndeleteRecording(pginfo);
}

void PlaybackBox::doDelete()
{
    doRemove(m_delItem, false, false);
}

void PlaybackBox::doForceDelete()
{
    doRemove(m_delItem, true, true);
}

void PlaybackBox::doDeleteForgetHistory()
{
    doRemove(m_delItem, true, false);
}

ProgramInfo *PlaybackBox::findMatchingProg(const ProgramInfo *pginfo)
{
    ProgramList::iterator it = m_progLists[""].begin();
    ProgramList::iterator end = m_progLists[""].end();
    for (; it != end; ++it)
    {
        if ((*it)->recstartts == pginfo->recstartts &&
            (*it)->chanid == pginfo->chanid)
        {
            return *it;
        }
    }

    return NULL;
}

ProgramInfo *PlaybackBox::findMatchingProg(const QString &key)
{
    QStringList keyParts;

    if ((key.isEmpty()) || (key.indexOf('_') < 0))
        return NULL;

    keyParts = key.split("_");

    // ProgramInfo::MakeUniqueKey() has 2 parts separated by '_' characters
    if (keyParts.size() == 2)
        return findMatchingProg(keyParts[0], keyParts[1]);
    else
        return NULL;
}

ProgramInfo *PlaybackBox::findMatchingProg(const QString &chanid,
                                           const QString &recstartts)
{
    ProgramList::iterator it = m_progLists[""].begin();
    ProgramList::iterator end = m_progLists[""].end();
    for (; it != end; ++it)
    {
        if ((*it)->recstartts.toString(Qt::ISODate) == recstartts &&
            (*it)->chanid == chanid)
        {
            return *it;
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
        bool on = !(pginfo->programflags & FL_WATCHED);

        pginfo->SetWatchedFlag(on);

        if (on)
        {
            item->DisplayState("yes", "watched");
            pginfo->programflags |= FL_WATCHED;
        }
        else
        {
            item->DisplayState("no", "watched");
            pginfo->programflags &= ~FL_WATCHED;
        }

        updateIcons(pginfo);

        // A refill affects the responsiveness of the UI and we only
        // need to rebuild the list if the watch list is displayed
        if (m_viewMask & VIEW_WATCHLIST)
            m_connected = FillList(true);
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
        bool on = !(pginfo->programflags & FL_AUTOEXP);
        pginfo->SetAutoExpire(on, true);

        if (on)
            pginfo->programflags |= FL_AUTOEXP;
        else
            pginfo->programflags &= ~FL_AUTOEXP;

        item->DisplayState("off", "expiry");
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
        bool on = !(pginfo->programflags & FL_PRESERVED);
        pginfo->SetPreserveEpisode(on);

        if (on)
            pginfo->programflags |= FL_PRESERVED;
        else
            pginfo->programflags &= ~FL_PRESERVED;

        item->DisplayState("on", "preserve");
        updateIcons(pginfo);
    }
}

void PlaybackBox::toggleView(ViewMask itemMask, bool setOn)
{
    if (setOn)
        m_viewMask = (ViewMask)(m_viewMask | itemMask);
    else
        m_viewMask = (ViewMask)(m_viewMask & ~itemMask);

    m_connected = FillList(true);
}

void PlaybackBox::togglePlayListTitle(void)
{
    QString groupname = m_groupList->GetItemCurrent()->GetData().toString();

    ProgramList::iterator it = m_progLists[groupname].begin();
    ProgramList::iterator end = m_progLists[groupname].end();
    for (; it != end; ++it)
    {
        if (*it && ((*it)->availableStatus == asAvailable))
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

    if (pginfo->availableStatus != asAvailable)
    {
        showAvailablePopup(pginfo);
        return;
    }

    togglePlayListItem(pginfo);

    if (GetFocusWidget() == m_recordingList)
        m_recordingList->MoveDown(MythUIButtonList::MoveItem);
}

void PlaybackBox::togglePlayListItem(ProgramInfo *pginfo)
{
    if (!pginfo)
        return;

    if (pginfo->availableStatus != asAvailable)
    {
        showAvailablePopup(pginfo);
        return;
    }

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
        if (tokens.size() == 5 && tokens[2] == "PROGRAM")
        {
            VERBOSE(VB_IMPORTANT,
                    QString("NetworkControl: Trying to %1 program '%2' @ '%3'")
                            .arg(tokens[1]).arg(tokens[3]).arg(tokens[4]));

            if (m_playingSomething)
            {
                VERBOSE(VB_IMPORTANT, "NetworkControl: ERROR: Already playing");

                MythEvent me("NETWORK_CONTROL RESPONSE ERROR: Unable to play, "
                             "player is already playing another recording.");
                gContext->dispatch(me);
                return;
            }

            ProgramInfo *tmpItem =
                ProgramInfo::GetProgramFromRecorded(tokens[3], tokens[4]);

            if (tmpItem)
            {
                m_recordingList->SetValueByData(tmpItem);

                MythEvent me("NETWORK_CONTROL RESPONSE OK");
                gContext->dispatch(me);

                if (tokens[1] == "PLAY")
                    tmpItem->setIgnoreBookmark(true);

                m_underNetworkControl = true;
                playSelected(m_recordingList->GetItemCurrent());
                m_underNetworkControl = false;
            }
            else
            {
                QString message = QString("NETWORK_CONTROL RESPONSE "
                                          "ERROR: Could not find recording for "
                                          "chanid %1 @ %2")
                                          .arg(tokens[3]).arg(tokens[4]);
                MythEvent me(message);
                gContext->dispatch(me);
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
    gContext->GetMainWindow()->TranslateKeyPress("TV Frontend", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "1" || action == "HELP")
            showIconHelp();
        else if (action == "MENU")
            showMenu();
        else if (action == "ESCAPE")
        {
            if (GetFocusWidget() == m_recordingList)
            {
                SwitchList();
            }
            else
                handled = false;
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
            m_connected = FillList(true);
        }
        else if (action == "TOGGLERECORD")
        {
            m_viewMask = m_viewMaskToggle(m_viewMask, VIEW_TITLES);
            m_connected = FillList(true);
        }
        else if (action == "CHANGERECGROUP")
            showGroupFilter();
        else if (action == "CHANGEGROUPVIEW")
            showViewChanger();
        else if (m_titleList.size() > 1)
        {
            if (action == "DELETE")
                deleteSelected(m_recordingList->GetItemCurrent());
            else if (action == "PLAYBACK")
                playSelected(m_recordingList->GetItemCurrent());
            else if (action == "INFO")
                showActionsSelected();
            else if (action == "DETAILS")
                details();
            else if (action == "CUSTOMEDIT")
                customEdit();
            else if (action == "UPCOMING")
                upcoming();
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
    if ((MythEvent::Type)(event->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)event;
        QString message = me->Message();

        if (message.left(21) == "RECORDING_LIST_CHANGE")
        {
            QStringList tokens = message.simplified().split(" ");
            if (tokens.size() == 1)
            {
                m_fillListFromCache = false;
                if (!m_fillListTimer->isActive())
                {
                    m_fillListTimer->stop();
                    m_fillListTimer->setSingleShot(true);
                    m_fillListTimer->start(1000);
                }
            }
            else if ((tokens[1] == "DELETE") && (tokens.size() == 4))
            {
                m_freeSpaceNeedsUpdate = true;
                m_progCacheLock.lock();
                if (!m_progCache)
                {
                    m_progCacheLock.unlock();
                    m_freeSpaceNeedsUpdate = true;
                    m_fillListFromCache = false;
                    m_fillListTimer->stop();
                    m_fillListTimer->setSingleShot(true);
                    m_fillListTimer->start(1000);
                    return;
                }
                vector<ProgramInfo *>::iterator i = m_progCache->begin();
                for ( ; i != m_progCache->end(); i++)
                {
                   if (((*i)->chanid == tokens[2]) &&
                       ((*i)->recstartts.toString(Qt::ISODate) == tokens[3]))
                   {
                        // We've set this recording to be deleted locally
                        // it's no longer in the recording list, so don't
                        // trigger a list reload which would cause a UI lag
                        if ((*i)->availableStatus == asPendingDelete
                             && m_currentGroup != m_watchGroupLabel)
                        {
                            (*i)->availableStatus = asDeleted;
                            m_progCacheLock.unlock();
                            return;
                        }
                        else
                            (*i)->availableStatus = asDeleted;
                        break;
                   }
                }

                m_progCacheLock.unlock();
                if (!m_fillListTimer->isActive())
                {
                    m_fillListFromCache = true;
                    m_fillListTimer->stop();
                    m_fillListTimer->setSingleShot(true);
                    m_fillListTimer->start(1000);
                }
            }
        }
        else if (message.left(15) == "NETWORK_CONTROL")
        {
            QStringList tokens = message.simplified().split(" ");
            if ((tokens[1] != "ANSWER") && (tokens[1] != "RESPONSE"))
            {
                m_ncLock.lock();
                m_networkControlCommands.push_back(message);
                m_ncLock.unlock();

                // This should be an impossible keypress we're simulating
                QKeyEvent *event;
                Qt::KeyboardModifiers modifiers =
                    Qt::ShiftModifier |
                    Qt::ControlModifier |
                    Qt::AltModifier |
                    Qt::MetaModifier |
                    Qt::KeypadModifier;
                event = new QKeyEvent(QEvent::KeyPress, Qt::Key_LaunchMedia,
                                      modifiers);
                QApplication::postEvent((QObject*)(gContext->GetMainWindow()),
                                        event);

                event = new QKeyEvent(QEvent::KeyRelease, Qt::Key_LaunchMedia,
                                      modifiers);
                QApplication::postEvent((QObject*)(gContext->GetMainWindow()),
                                        event);
            }
        }
        else if (message == "PREVIEW_READY")
        {
            ProgramInfo evinfo;
            if (evinfo.FromStringList(me->ExtraDataList(), 0))
                HandlePreviewEvent(evinfo);
        }
    }
}

bool PlaybackBox::fileExists(ProgramInfo *pginfo)
{
    if (pginfo)
       return pginfo->PathnameExists();
    return false;
}

QDateTime PlaybackBox::getPreviewLastModified(ProgramInfo *pginfo)
{
    QDateTime datetime;
    QString filename = pginfo->pathname + ".png";

    if (filename.left(7) != "myth://")
    {
        QFileInfo retfinfo(filename);
        if (retfinfo.exists())
            datetime = retfinfo.lastModified();
    }
    else
    {
        datetime = RemoteGetPreviewLastModified(pginfo);
    }

    return datetime;
}

void PlaybackBox::IncPreviewGeneratorPriority(const QString &xfn)
{
    QString fn = xfn.mid(qMax(xfn.lastIndexOf('/') + 1,0));

    QMutexLocker locker(&m_previewGeneratorLock);
    vector<QString> &q = m_previewGeneratorQueue;
    vector<QString>::iterator it = std::find(q.begin(), q.end(), fn);
    if (it != q.end())
        q.erase(it);

    PreviewMap::iterator pit = m_previewGenerator.find(fn);
    if (pit != m_previewGenerator.end() && (*pit).gen && !(*pit).genStarted)
        q.push_back(fn);
}

void PlaybackBox::UpdatePreviewGeneratorThreads(void)
{
    QMutexLocker locker(&m_previewGeneratorLock);
    vector<QString> &q = m_previewGeneratorQueue;
    if ((m_previewGeneratorRunning < PREVIEW_GEN_MAX_RUN) && q.size())
    {
        QString fn = q.back();
        q.pop_back();
        PreviewMap::iterator it = m_previewGenerator.find(fn);
        if (it != m_previewGenerator.end() && (*it).gen && !(*it).genStarted)
        {
            m_previewGeneratorRunning++;
            (*it).gen->Start();
            (*it).genStarted = true;
        }
    }
}

/** \fn PlaybackBox::SetPreviewGenerator(const QString&, PreviewGenerator*)
 *  \brief Sets the PreviewGenerator for a specific file.
 *  \return true iff call succeeded.
 */
bool PlaybackBox::SetPreviewGenerator(const QString &xfn, PreviewGenerator *g)
{
    QString fn = xfn.mid(qMax(xfn.lastIndexOf('/') + 1,0));
    QMutexLocker locker(&m_previewGeneratorLock);

    if (!g)
    {
        m_previewGeneratorRunning = qMax(0, (int)m_previewGeneratorRunning - 1);
        PreviewMap::iterator it = m_previewGenerator.find(fn);
        if (it == m_previewGenerator.end())
            return false;

        (*it).gen        = NULL;
        (*it).genStarted = false;
        (*it).ready      = false;
        (*it).lastBlockTime =
            qMax(PreviewGenState::minBlockSeconds, (*it).lastBlockTime * 2);
        (*it).blockRetryUntil =
            QDateTime::currentDateTime().addSecs((*it).lastBlockTime);

        return true;
    }

    g->AttachSignals(this);
    m_previewGenerator[fn].gen = g;
    m_previewGenerator[fn].genStarted = false;
    m_previewGenerator[fn].ready = false;

    m_previewGeneratorLock.unlock();
    IncPreviewGeneratorPriority(xfn);
    m_previewGeneratorLock.lock();

    return true;
}

/** \fn PlaybackBox::IsGeneratingPreview(const QString&, bool) const
 *  \brief Returns true if we have already started a
 *         PreviewGenerator to create this file.
 */
bool PlaybackBox::IsGeneratingPreview(const QString &xfn, bool really) const
{
    PreviewMap::const_iterator it;
    QMutexLocker locker(&m_previewGeneratorLock);

    QString fn = xfn.mid(qMax(xfn.lastIndexOf('/') + 1,0));
    if ((it = m_previewGenerator.find(fn)) == m_previewGenerator.end())
        return false;

    if (really)
        return ((*it).gen && !(*it).ready);

    if ((*it).blockRetryUntil.isValid())
        return QDateTime::currentDateTime() < (*it).blockRetryUntil;

    return (*it).gen;
}

/** \fn PlaybackBox::IncPreviewGeneratorAttempts(const QString&)
 *  \brief Increments and returns number of times we have
 *         started a PreviewGenerator to create this file.
 */
uint PlaybackBox::IncPreviewGeneratorAttempts(const QString &xfn)
{
    QMutexLocker locker(&m_previewGeneratorLock);
    QString fn = xfn.mid(qMax(xfn.lastIndexOf('/') + 1,0));
    return m_previewGenerator[fn].attempts++;
}

void PlaybackBox::previewThreadDone(const QString &fn, bool &success)
{
    success = SetPreviewGenerator(fn, NULL);
    UpdatePreviewGeneratorThreads();
}

/** \fn PlaybackBox::previewReady(const ProgramInfo*)
 *  \brief Callback used by PreviewGenerator to tell us a m_preview
 *         we requested has been returned from the backend.
 *  \param pginfo ProgramInfo describing the previewed recording.
 */
void PlaybackBox::previewReady(const ProgramInfo *pginfo)
{
    QString xfn = pginfo->pathname + ".png";
    QString fn = xfn.mid(qMax(xfn.lastIndexOf('/') + 1,0));

    m_previewGeneratorLock.lock();
    PreviewMap::iterator it = m_previewGenerator.find(fn);
    if (it != m_previewGenerator.end())
    {
        (*it).ready         = true;
        (*it).attempts      = 0;
        (*it).lastBlockTime = 0;
    }
    m_previewGeneratorLock.unlock();

    if (pginfo)
    {
        QStringList extra;
        pginfo->ToStringList(extra);
        extra.detach();
        MythEvent me("PREVIEW_READY", extra);
        gContext->dispatch(me);
    }
}

void PlaybackBox::HandlePreviewEvent(const ProgramInfo &evinfo)
{
    ProgramInfo *info = findMatchingProg(&evinfo);

    if (!info)
        return;

    MythUIButtonListItem *item =
        m_recordingList->GetItemByData(qVariantFromValue(info));

    if (!item)
        return;

    MythUIButtonListItem *sel_item = m_recordingList->GetItemCurrent();
    UpdateProgramInfo(item, item == sel_item, true);
}

bool check_lastmod(LastCheckedMap &elapsedtime, const QString &filename)
{
    LastCheckedMap::iterator it = elapsedtime.find(filename);

    if (it != elapsedtime.end() && ((*it).elapsed() < 300))
        return false;

    elapsedtime[filename].restart();
    return true;
}

QString PlaybackBox::getPreviewImage(ProgramInfo *pginfo)
{
    QString filename;

    if (!pginfo || pginfo->availableStatus == asPendingDelete)
        return filename;

    filename = pginfo->GetPlaybackURL() + ".png";

    IncPreviewGeneratorPriority(filename);

    bool check_date = check_lastmod(m_previewLastModifyCheck, filename);

    QDateTime previewLastModified;

    if (check_date)
        previewLastModified = getPreviewLastModified(pginfo);

    if (m_previewFromBookmark && check_date &&
        (!previewLastModified.isValid() ||
         (previewLastModified <  pginfo->lastmodified &&
          previewLastModified >= pginfo->recendts)) &&
        !pginfo->IsEditing() &&
        !JobQueue::IsJobRunning(JOB_COMMFLAG, pginfo) &&
        !IsGeneratingPreview(filename))
    {
        uint attempts = IncPreviewGeneratorAttempts(filename);
        if (attempts < PreviewGenState::maxAttempts)
        {
            PreviewGenerator::Mode mode =
                (PreviewGenerator::Mode) m_previewGeneratorMode;
            SetPreviewGenerator(filename, new PreviewGenerator(pginfo, mode));
        }
        else if (attempts == PreviewGenState::maxAttempts)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("Attempted to generate preview for '%1' "
                            "%2 times, giving up.")
                    .arg(filename).arg(PreviewGenState::maxAttempts));
        }

        if (attempts >= PreviewGenState::maxAttempts)
            return filename;
    }

    UpdatePreviewGeneratorThreads();

    // Image is local
    if (QFileInfo(filename).exists())
        return filename;

    // If this is a remote frontend then check the remote file cache
    QString cachefile = QString("%1/remotecache/%3").arg(GetConfDir())
                                                .arg(filename.section('/', -1));
    QFileInfo cachefileinfo(cachefile);
    if (!cachefileinfo.exists() ||
        previewLastModified.addSecs(-60) > cachefileinfo.lastModified())
    {
        if (!IsGeneratingPreview(filename))
        {
            uint attempts = IncPreviewGeneratorAttempts(filename);
            if (attempts < PreviewGenState::maxAttempts)
            {
                SetPreviewGenerator(filename, new PreviewGenerator(pginfo,
                                (PreviewGenerator::Mode)m_previewGeneratorMode));
            }
            else if (attempts == PreviewGenState::maxAttempts)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        QString("Attempted to generate m_preview for '%1' "
                                "%2 times, giving up.")
                        .arg(filename).arg(PreviewGenState::maxAttempts));
            }
        }
    }

    if (cachefileinfo.exists())
        return cachefile;

    return "";
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
        connect(viewPopup, SIGNAL(result(int)), SLOT(saveViewChanges(int)));
        m_popupStack->AddScreen(viewPopup);
    }
    else
        delete viewPopup;
}

void PlaybackBox::saveViewChanges(int viewMask)
{
    if (viewMask == VIEW_NONE)
        viewMask = VIEW_TITLES;
    gContext->SaveSetting("DisplayGroupDefaultViewMask", (int)viewMask);
    gContext->SaveSetting("PlaybackWatchList",
                                            (bool)(viewMask & VIEW_WATCHLIST));
    m_viewMask = (ViewMask)viewMask;
}

void PlaybackBox::showGroupFilter(void)
{
    QString dispGroup = ProgramInfo::i18n(m_recGroup);

    QStringList groupNames;
    QStringList displayNames;
    QStringList groups;

    MSqlQuery query(MSqlQuery::InitCon());

    bool liveTVInAll = gContext->GetNumSetting("LiveTVInAllPrograms",0);

    QMap<QString,QString> recGroupType;

    uint items = 0;
    uint totalItems = 0;
    QString itemStr;

    // Add the group entries
    displayNames.append(QString("------- %1 -------").arg(tr("Groups")));
    groupNames.append("");

    // Find each recording group, and the number of recordings in each
    query.prepare("SELECT recgroup, COUNT(title) FROM recorded "
                  "WHERE deletepending = 0 GROUP BY recgroup");
    if (query.exec())
    {
        while (query.next())
        {
            dispGroup = query.value(0).toString();
            items     = query.value(1).toInt();
            itemStr   = (items == 1) ? tr("item") : tr("items");

            if ((dispGroup != "LiveTV" || liveTVInAll) &&
                (dispGroup != "Deleted"))
                totalItems += items;

            groupNames.append(dispGroup);

            dispGroup = (dispGroup == "Default") ? tr("Default") : dispGroup;
            dispGroup = (dispGroup == "Deleted") ? tr("Deleted") : dispGroup;
            dispGroup = (dispGroup == "LiveTV")  ? tr("LiveTV")  : dispGroup;

            displayNames.append(QString("%1 [%2 %3]").arg(dispGroup).arg(items)
                                .arg(itemStr));

            recGroupType[query.value(0).toString()] = "recgroup";
        }
    }

    // Create and add the "All Programs" entry
    itemStr = (totalItems == 1) ? tr("item") : tr("items");
    displayNames.prepend(QString("%1 [%2 %3]")
                        .arg(ProgramInfo::i18n("All Programs"))
                        .arg(totalItems).arg(itemStr));
    groupNames.prepend("All Programs");
    recGroupType["All Programs"] = "recgroup";

    // Find each category, and the number of recordings in each
    query.prepare("SELECT DISTINCT category, COUNT(title) FROM recorded "
                  "WHERE deletepending = 0 GROUP BY category");
    if (query.exec())
    {
        int unknownCount = 0;
        while (query.next())
        {
            items     = query.value(1).toInt();
            itemStr   = (items == 1) ? tr("item") : tr("items");

            dispGroup = query.value(0).toString();
            if (dispGroup.isEmpty())
            {
                unknownCount += items;
                dispGroup = tr("Unknown");
            }
            else if (dispGroup == tr("Unknown"))
                unknownCount += items;

            if ((!recGroupType.contains(dispGroup)) &&
                (dispGroup != tr("Unknown")))
            {
                groups += QString("%1 [%2 %3]").arg(dispGroup)
                                  .arg(items).arg(itemStr);

                recGroupType[dispGroup] = "category";
            }
        }

        if (unknownCount > 0)
        {
            dispGroup = tr("Unknown");
            items     = unknownCount;
            itemStr   = (items == 1) ? tr("item") : tr("items");
            groups += QString("%1 [%2 %3]").arg(dispGroup)
                              .arg(items).arg(itemStr);

            recGroupType[dispGroup] = "category";
        }
    }

    // Add the category entries
    displayNames.append(QString("------- %1 -------").arg(tr("Categories")));
    groupNames.append("");
    groups.sort();
    QStringList::iterator it;
    for (it = groups.begin(); it != groups.end(); it++)
    {
        displayNames.append(*it);
        groupNames.append(*it);
    }

    QString label = tr("Change Filter");

    GroupSelector *recGroupPopup = new GroupSelector(m_popupStack, label,
                                                     displayNames, groupNames,
                                                     m_recGroup);

    if (recGroupPopup->Create())
    {
        connect(recGroupPopup, SIGNAL(result(QString)),
                SLOT(displayRecGroup(QString)));
        m_popupStack->AddScreen(recGroupPopup);
    }
    else
        delete recGroupPopup;
}

void PlaybackBox::setGroupFilter(const QString &recGroup)
{
    QString newRecGroup = recGroup;

    if (newRecGroup.isEmpty())
        return;

    if (newRecGroup == ProgramInfo::i18n("Default"))
        newRecGroup = "Default";
    else if (newRecGroup == ProgramInfo::i18n("All Programs"))
        newRecGroup = "All Programs";
    else if (newRecGroup == ProgramInfo::i18n("LiveTV"))
        newRecGroup = "LiveTV";
    else if (m_recGroup == ProgramInfo::i18n("Deleted"))
        newRecGroup = "Deleted";

    m_curGroupPassword = m_recGroupPassword;

    m_recGroup = newRecGroup;

    if (m_groupnameAsAllProg)
        m_groupDisplayName = ProgramInfo::i18n(m_recGroup);

    m_connected = FillList(true);

    // Don't save anything if we forced a new group
    if (!newRecGroup.isEmpty())
        return;

    if (gContext->GetNumSetting("RememberRecGroup",1))
        gContext->SaveSetting("DisplayRecGroup", m_recGroup);

    if (m_recGroupType[m_recGroup] == "recgroup")
        gContext->SaveSetting("DisplayRecGroupIsCategory", 0);
    else
        gContext->SaveSetting("DisplayRecGroupIsCategory", 1);
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

    m_recGroupPwCache["All Programs"] =
                                gContext->GetSetting("AllRecGroupPassword", "");
}

void PlaybackBox::doPlaylistChangeRecGroup(void)
{
    // If m_delItem is not NULL, then the Recording Group changer will operate
    // on just that recording, otherwise it operates on the items in theplaylist
    if (m_delItem)
    {
        delete m_delItem;
        m_delItem = NULL;
    }

    showRecGroupChanger();
}

void PlaybackBox::showRecGroupChanger(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT recgroup, COUNT(title) FROM recorded "
        "WHERE deletepending = 0 GROUP BY recgroup ORDER BY recgroup");

    QStringList groupNames;
    QStringList displayNames;
    QString selected = "";

    QString itemStr;
    QString dispGroup;

    if (query.exec())
    {
        while (query.next())
        {
            dispGroup = query.value(0).toString();

            groupNames.append(dispGroup);

            if (dispGroup == "Default")
                dispGroup = tr("Default");
            else if (dispGroup == "LiveTV")
                dispGroup = tr("LiveTV");
            else if (dispGroup == "Deleted")
                dispGroup = tr("Deleted");

            if (query.value(1).toInt() == 1)
                itemStr = tr("item");
            else
                itemStr = tr("items");

            displayNames.append(QString("%1 [%2 %3]").arg(dispGroup)
                              .arg(query.value(1).toInt()).arg(itemStr));
        }
    }

    ProgramInfo *pginfo = CurrentItem();
    selected = pginfo->recgroup;

    QString label = tr("Select Recording Group");

    popupString(pginfo, label);

    GroupSelector *rgChanger = new GroupSelector(m_popupStack, label,
                                                 displayNames, groupNames,
                                                 selected);

    if (rgChanger->Create())
    {
        connect(rgChanger, SIGNAL(result(QString)), SLOT(setRecGroup(QString)));
        m_popupStack->AddScreen(rgChanger);
    }
    else
        delete rgChanger;
}

void PlaybackBox::doPlaylistChangePlayGroup(void)
{
    // If m_delItem is not NULL, then the Playback Group changer will operate
    // on just that recording, otherwise it operates on the items in theplaylist
    if (m_delItem)
    {
        delete m_delItem;
        m_delItem = NULL;
    }

    showPlayGroupChanger();
}

void PlaybackBox::showPlayGroupChanger(void)
{
    QStringList groupNames;
    QStringList displayNames;
    QString selected = "";

    displayNames.append(tr("Default"));
    groupNames.append("Default");

    QStringListIterator it(PlayGroup::GetNames());
    while (it.hasNext())
    {
        QString group = it.next();
        displayNames.append(group);
        groupNames.append(group);
    }

    ProgramInfo *pginfo = CurrentItem();
    selected = pginfo->playgroup;

    QString label = tr("Select Playback Group");

    popupString(pginfo, label);

    GroupSelector *pgChanger = new GroupSelector(m_popupStack, label,
                                                 displayNames, groupNames,
                                                 selected);

    if (pgChanger->Create())
    {
        connect(pgChanger, SIGNAL(result(QString)),
                SLOT(setPlayGroup(QString)));
        m_popupStack->AddScreen(pgChanger);
    }
    else
        delete pgChanger;
}

void PlaybackBox::showMetadataEditor()
{
    ProgramInfo *pgInfo = CurrentItem();

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    RecMetadataEdit *editMetadata = new RecMetadataEdit(mainStack, pgInfo);

    if (editMetadata->Create())
    {
        connect(editMetadata, SIGNAL(result(const QString &, const QString &)),
                SLOT(saveRecMetadata(const QString &, const QString &)));
        mainStack->AddScreen(editMetadata);
    }
    else
        delete editMetadata;
}

void PlaybackBox::saveRecMetadata(const QString &newTitle,
                                  const QString &newSubtitle)
{
    MythUIButtonListItem *item = m_recordingList->GetItemCurrent();

    if (!item)
        return;

    ProgramInfo *pginfo = qVariantValue<ProgramInfo *>(item->GetData());

    if (!pginfo)
        return;

    QString groupname = m_groupList->GetItemCurrent()->GetData().toString();

    if (groupname != pginfo->title)
    {
        QString tempSubTitle = newTitle;
        if (!newSubtitle.trimmed().isEmpty())
            tempSubTitle = QString("%1 - \"%2\"")
                            .arg(tempSubTitle).arg(newSubtitle);

        pginfo->ApplyRecordRecTitleChange(newTitle, newSubtitle);
        item->SetText(tempSubTitle, "titlesubtitle");
        item->SetText(newTitle, "title");
        item->SetText(newSubtitle, "subtitle");

        UpdateProgramInfo(item, true);
    }
    else
        m_recordingList->RemoveItem(item);
}

void PlaybackBox::setRecGroup(QString newRecGroup)
{
    ProgramInfo *tmpItem = CurrentItem();

    if (newRecGroup.isEmpty() || !tmpItem)
        return;

    if (m_playList.size() > 0)
    {
        QStringList::Iterator it;

        for (it = m_playList.begin(); it != m_playList.end(); ++it )
        {
            tmpItem = findMatchingProg(*it);
            if (tmpItem)
            {
                if ((tmpItem->recgroup == "LiveTV") && (newRecGroup != "LiveTV"))
                    tmpItem->SetAutoExpire(
                        gContext->GetNumSetting("AutoExpireDefault", 0));
                else if ((tmpItem->recgroup != "LiveTV") && (newRecGroup == "LiveTV"))
                    tmpItem->SetAutoExpire(kLiveTVAutoExpire);

                tmpItem->ApplyRecordRecGroupChange(newRecGroup);
            }
        }
        m_playList.clear();
    }
    else
    {
        if ((tmpItem->recgroup == "LiveTV") && (newRecGroup != "LiveTV"))
            tmpItem->SetAutoExpire(gContext->GetNumSetting("AutoExpireDefault", 0));
        else if ((tmpItem->recgroup != "LiveTV") && (newRecGroup == "LiveTV"))
            tmpItem->SetAutoExpire(kLiveTVAutoExpire);

        tmpItem->ApplyRecordRecGroupChange(newRecGroup);
    }

    // TODO May not rebuild the list here, check current filter and removeitem
    //      if it shouldn't be visible any more
    FillList(true);
}

void PlaybackBox::setPlayGroup(QString newPlayGroup)
{
    ProgramInfo *tmpItem = CurrentItem();

    if (newPlayGroup.isEmpty() || !tmpItem)
        return;

    if (newPlayGroup == tr("Default"))
        newPlayGroup = "Default";

    if (m_playList.size() > 0)
    {
        QStringList::Iterator it;

        for (it = m_playList.begin(); it != m_playList.end(); ++it )
        {
            tmpItem = findMatchingProg(*it);
            if (tmpItem)
                tmpItem->ApplyRecordPlayGroupChange(newPlayGroup);
        }
        m_playList.clear();
    }
    else if (tmpItem)
        tmpItem->ApplyRecordPlayGroupChange(newPlayGroup);
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
    if (m_recGroup == "All Programs")
    {
        gContext->SaveSetting("AllRecGroupPassword", newPassword);
    }
    else
    {
        MSqlQuery query(MSqlQuery::InitCon());

        query.prepare("DELETE FROM recgrouppassword "
                           "WHERE recgroup = :RECGROUP ;");
        query.bindValue(":RECGROUP", m_recGroup);

        query.exec();

        if (!newPassword.isEmpty())
        {
            query.prepare("INSERT INTO recgrouppassword "
                          "(recgroup, password) VALUES "
                          "( :RECGROUP , :PASSWD )");
            query.bindValue(":RECGROUP", m_recGroup);
            query.bindValue(":PASSWD", newPassword);

            query.exec();
        }
    }

    m_recGroupPassword = newPassword;
}

void PlaybackBox::clearProgramCache(void)
{
    if (!m_progCache)
        return;

    vector<ProgramInfo *>::iterator i = m_progCache->begin();
    for ( ; i != m_progCache->end(); i++)
        delete *i;
    delete m_progCache;
    m_progCache = NULL;
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
        VERBOSE(VB_IMPORTANT, "Theme is missing 'groups' button list.");
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

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist.");

    connect(groupList, SIGNAL(itemClicked(MythUIButtonListItem *)),
            SLOT(AcceptItem(MythUIButtonListItem *)));

    return true;
}

void GroupSelector::AcceptItem(MythUIButtonListItem *item)
{
    if (!item)
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
    if (m_viewMask & PlaybackBox::VIEW_TITLES)
        checkBox->SetCheckState(MythUIStateType::Full);
    connect(checkBox, SIGNAL(toggled(bool)),
            m_parentScreen, SLOT(toggleTitleView(bool)));

    checkBox = dynamic_cast<MythUICheckBox*>(GetChild("categories"));
    if (m_viewMask & PlaybackBox::VIEW_CATEGORIES)
        checkBox->SetCheckState(MythUIStateType::Full);
    connect(checkBox, SIGNAL(toggled(bool)),
            m_parentScreen, SLOT(toggleCategoryView(bool)));

    checkBox = dynamic_cast<MythUICheckBox*>(GetChild("recgroups"));
    if (m_viewMask & PlaybackBox::VIEW_RECGROUPS)
        checkBox->SetCheckState(MythUIStateType::Full);
    connect(checkBox, SIGNAL(toggled(bool)),
            m_parentScreen, SLOT(toggleRecGroupView(bool)));

    // TODO Do we need two separate settings to determine whether the watchlist
    //      is shown? The filter setting be enough?
    checkBox = dynamic_cast<MythUICheckBox*>(GetChild("watchlist"));
    if (m_viewMask & PlaybackBox::VIEW_WATCHLIST)
        checkBox->SetCheckState(MythUIStateType::Full);
    connect(checkBox, SIGNAL(toggled(bool)),
            m_parentScreen, SLOT(toggleWatchListView(bool)));

    checkBox = dynamic_cast<MythUICheckBox*>(GetChild("searches"));
    if (m_viewMask & PlaybackBox::VIEW_SEARCHES)
        checkBox->SetCheckState(MythUIStateType::Full);
    connect(checkBox, SIGNAL(toggled(bool)),
            m_parentScreen, SLOT(toggleSearchView(bool)));

    // TODO Do we need two separate settings to determine whether livetv
    //      recordings are shown? Same issue as the watchlist above

    //if ((m_recGroup == "All Programs") &&
    //    (gContext->GetNumSetting("LiveTVInAllPrograms",0)))
    //{
        checkBox = dynamic_cast<MythUICheckBox*>(GetChild("livetv"));
        if (checkBox)
        {
            if (m_viewMask & PlaybackBox::VIEW_LIVETVGRP)
                checkBox->SetCheckState(MythUIStateType::Full);
            connect(checkBox, SIGNAL(toggled(bool)),
                    m_parentScreen, SLOT(toggleLiveTVView(bool)));
        }
    //}

    MythUIButton *savebutton = dynamic_cast<MythUIButton*>(GetChild("save"));
    connect(savebutton, SIGNAL(Clicked()), SLOT(SaveChanges()));

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist.");

    return true;
}

void ChangeView::SaveChanges()
{
    emit result(m_viewMask);
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
        VERBOSE(VB_IMPORTANT, "Window 'passwordchanger' is missing required "
                              "elements.");
        return false;
    }

    m_oldPasswordEdit->SetPassword(true);
    m_newPasswordEdit->SetPassword(true);

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist.");

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
    m_titleEdit = m_subtitleEdit = NULL;
}

bool RecMetadataEdit::Create()
{
    if (!LoadWindowFromXML("recordings-ui.xml", "editmetadata", this))
        return false;

    m_titleEdit = dynamic_cast<MythUITextEdit*>(GetChild("title"));
    m_subtitleEdit = dynamic_cast<MythUITextEdit*>(GetChild("subtitle"));
    MythUIButton *okButton = dynamic_cast<MythUIButton*>(GetChild("ok"));

    if (!m_titleEdit || !m_subtitleEdit || !okButton)
    {
        VERBOSE(VB_IMPORTANT, "Window 'editmetadata' is missing required "
                              "elements.");
        return false;
    }

    m_titleEdit->SetText(m_progInfo->title);
    m_subtitleEdit->SetText(m_progInfo->subtitle);

    connect(okButton, SIGNAL(Clicked()), SLOT(SaveChanges()));

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist.");

    return true;
}

void RecMetadataEdit::SaveChanges()
{
    QString newRecTitle = m_titleEdit->GetText();
    QString newRecSubtitle = m_subtitleEdit->GetText();

    if (newRecTitle.isEmpty())
        return;

    emit result(newRecTitle, newRecSubtitle);
    Close();
}

//////////////////////////////////////////

HelpPopup::HelpPopup(MythScreenStack *lparent)
                : MythScreenType(lparent, "helppopup")
{

}

bool HelpPopup::Create()
{
    if (!LoadWindowFromXML("recordings-ui.xml", "iconhelp", this))
        return false;

    MythUIButtonList *iconList = dynamic_cast<MythUIButtonList*>
                                                        (GetChild("icons"));

    if (!iconList)
    {
        VERBOSE(VB_IMPORTANT, "Window 'iconhelp' is missing required "
                              "elements.");
        return false;
    }

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist.");

    QMap <QString, QString>::iterator it;
    QMap <QString, QString> iconMap;
    iconMap["commflagged"] = tr("Commercials are flagged");
    iconMap["cutlist"]     = tr("An editing cutlist is present");
    iconMap["autoexpire"]  = tr("The program is able to auto-expire");
    iconMap["processing"]  = tr("Commercials are being flagged");
    iconMap["bookmark"]    = tr("A bookmark is set");
    iconMap["inuse"]       = tr("Recording is in use");
    iconMap["transcoded"]  = tr("Recording has been transcoded");

    iconMap["mono"]        = tr("Recording is in Mono");
    iconMap["stereo"]      = tr("Recording is in Stereo");
    iconMap["surround"]    = tr("Recording is in Surround Sound");
    iconMap["dolby"]       = tr("Recording is in Dolby Surround Sound");

    iconMap["cc"]          = tr("Recording is Closed Captioned");
    iconMap["subtitles"]    = tr("Recording has Subtitles Available");
    iconMap["onscreensub"] = tr("Recording is Subtitled");

    iconMap["hdtv"]        = tr("Recording is in High Definition");
    iconMap["widescreen"]  = tr("Recording is in WideScreen");

    iconMap["watched"]     = tr("Recording has been watched");
    iconMap["preserved"]   = tr("Recording is preserved");

    for (it = iconMap.begin(); it != iconMap.end(); ++it)
    {
        MythUIImage *iconImage = dynamic_cast<MythUIImage*>(GetChild(it.key()));
        if (iconImage)
        {
            //MythUIButtonListItem *item = new MythUIButtonListItem(iconList,
            //                                                      it.value());
            //item->SetImage();
        }
    }

    return true;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
