
#include "guidegrid.h"

// c/c++
#include <math.h>
#include <unistd.h>
#include <iostream>
#include <algorithm>
using namespace std;

//qt
#include <QCoreApplication>
#include <QKeyEvent>
#include <QDateTime>

// myth
#include "mythcorecontext.h"
#include "mythdbcon.h"
#include "mythlogging.h"
#include "channelinfo.h"
#include "programinfo.h"
#include "recordingrule.h"
#include "tv_play.h"
#include "tv_rec.h"
#include "customedit.h"
#include "mythdate.h"
#include "remoteutil.h"
#include "channelutil.h"
#include "cardutil.h"
#include "mythuibuttonlist.h"
#include "mythuiguidegrid.h"
#include "mythdialogbox.h"
#include "progfind.h"

QWaitCondition epgIsVisibleCond;

#define LOC      QString("GuideGrid: ")
#define LOC_ERR  QString("GuideGrid, Error: ")
#define LOC_WARN QString("GuideGrid, Warning: ")

const QString kUnknownTitle = QObject::tr("Unknown");
const QString kUnknownCategory = QObject::tr("Unknown");

JumpToChannel::JumpToChannel(
    JumpToChannelListener *parent, const QString &start_entry,
    int start_chan_idx, int cur_chan_idx, uint rows_disp) :
    m_listener(parent),
    m_entry(start_entry),
    m_previous_start_channel_index(start_chan_idx),
    m_previous_current_channel_index(cur_chan_idx),
    m_rows_displayed(rows_disp),
    m_timer(new QTimer(this))
{
    if (parent && m_timer)
    {
        connect(m_timer, SIGNAL(timeout()), SLOT(deleteLater()));
        m_timer->setSingleShot(true);
    }
    Update();
}


void JumpToChannel::deleteLater(void)
{
    if (m_listener)
    {
        m_listener->SetJumpToChannel(NULL);
        m_listener = NULL;
    }

    if (m_timer)
    {
        m_timer->stop();
        m_timer = NULL;
    }

    QObject::deleteLater();
}


static bool has_action(QString action, const QStringList &actions)
{
    QStringList::const_iterator it;
    for (it = actions.begin(); it != actions.end(); ++it)
    {
        if (action == *it)
            return true;
    }
    return false;
}

bool JumpToChannel::ProcessEntry(const QStringList &actions, const QKeyEvent *e)
{
    if (!m_listener)
        return false;

    if (has_action("ESCAPE", actions))
    {
        m_listener->GoTo(m_previous_start_channel_index,
                         m_previous_current_channel_index);
        deleteLater();
        return true;
    }

    if (has_action("DELETE", actions))
    {
        if (!m_entry.isEmpty())
            m_entry = m_entry.left(m_entry.length()-1);
        Update();
        return true;
    }

    if (has_action(ACTION_SELECT, actions))
    {
        if (Update())
            deleteLater();
        return true;
    }

    QString txt = e->text();
    bool isUInt;
    txt.toUInt(&isUInt);
    if (isUInt)
    {
        m_entry += txt;
        Update();
        return true;
    }

    if (!m_entry.isEmpty() && (txt=="_" || txt=="-" || txt=="#" || txt=="."))
    {
        m_entry += txt;
        Update();
        return true;
    }

    return false;
}

bool JumpToChannel::Update(void)
{
    if (!m_timer || !m_listener)
        return false;

    m_timer->stop();

    // find the closest channel ...
    int i = m_listener->FindChannel(0, m_entry, false);
    if (i >= 0)
    {
        // setup the timeout timer for jump mode
        m_timer->start(kJumpToChannelTimeout);

        // rows_displayed to center
        int start = i - m_rows_displayed/2;
        int cur   = m_rows_displayed/2;
        m_listener->GoTo(start, cur);
        return true;
    }
    else
    { // prefix must be invalid.. reset entry..
        deleteLater();
        return false;
    }
}

void GuideGrid::RunProgramGuide(uint chanid, const QString &channum,
                    TV *player, bool embedVideo, bool allowFinder, int changrpid)
{
    // which channel group should we default to
    if (changrpid == -2)
        changrpid = gCoreContext->GetNumSetting("ChannelGroupDefault", -1);

    // check there are some channels setup
    ChannelInfoList channels = ChannelUtil::GetChannels(
        0, true, "", (changrpid<0) ? 0 : changrpid);
    if (!channels.size())
    {
        QString message;
        if (changrpid == -1)
        {
            message = tr("You don't have any channels defined in the database."
                         "\n\t\t\tThe program guide will have nothing to show you.");
        }
        else
        {
            message = tr("Channel group '%1' doesn't have any channels defined."
                         "\n\t\t\tThe program guide will have nothing to show you.")
                         .arg(ChannelGroup::GetChannelGroupName(changrpid));
        }

        LOG(VB_GENERAL, LOG_WARNING, LOC + message);

        if (!player)
            ShowOkPopup(message);
        else
        {
            if (player && allowFinder)
            {
                message = QString("EPG_EXITING");
                qApp->postEvent(player, new MythEvent(message));
            }
        }

        return;
    }

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    GuideGrid *gg = new GuideGrid(mainStack,
                                  chanid, channum,
                                  player, embedVideo, allowFinder,
                                  changrpid);

    if (gg->Create())
        mainStack->AddScreen(gg, (player == NULL));
    else
        delete gg;
}

GuideGrid::GuideGrid(MythScreenStack *parent,
                     uint chanid, QString channum,
                     TV *player, bool embedVideo,
                     bool allowFinder, int changrpid)
         : ScheduleCommon(parent, "guidegrid"),
    m_allowFinder(allowFinder),
    m_player(player),
    m_usingNullVideo(false), m_embedVideo(embedVideo),
    m_previewVideoRefreshTimer(new QTimer(this)),
    m_updateTimer(NULL),
    m_jumpToChannelLock(QMutex::Recursive),
    m_jumpToChannel(NULL)
{
    connect(m_previewVideoRefreshTimer, SIGNAL(timeout()),
            this,                     SLOT(refreshVideo()));

    m_channelCount = 5;
    m_timeCount = 30;
    m_currentStartChannel = 0;
    m_changrpid = changrpid;
    m_changrplist = ChannelGroup::GetChannelGroups(false);

    m_sortReverse = gCoreContext->GetNumSetting("EPGSortReverse", 0);
    m_selectRecThreshold = gCoreContext->GetNumSetting("SelChangeRecThreshold", 16);

    m_channelOrdering = gCoreContext->GetSetting("ChannelOrdering", "channum");

    for (uint i = 0; i < MAX_DISPLAY_CHANS; i++)
        m_programs.push_back(NULL);

    for (int x = 0; x < MAX_DISPLAY_TIMES; ++x)
    {
        for (int y = 0; y < MAX_DISPLAY_CHANS; ++y)
            m_programInfos[y][x] = NULL;
    }

    m_originalStartTime = MythDate::current();

    int secsoffset = -((m_originalStartTime.time().minute() % 30) * 60 +
                        m_originalStartTime.time().second());
    m_currentStartTime = m_originalStartTime.addSecs(secsoffset);
    m_startChanID  = chanid;
    m_startChanNum = channum;
}

bool GuideGrid::Create()
{
    QString windowName = "programguide";

    if (m_embedVideo)
        windowName = "programguide-video";

    if (!LoadWindowFromXML("schedule-ui.xml", windowName, this))
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_timeList, "timelist", &err);
    UIUtilE::Assign(this, m_channelList, "channellist", &err);
    UIUtilE::Assign(this, m_guideGrid, "guidegrid", &err);
    UIUtilW::Assign(this, m_dateText, "datetext");
    UIUtilW::Assign(this, m_longdateText, "longdatetext");
    UIUtilW::Assign(this, m_changroupname, "channelgroup");
    UIUtilW::Assign(this, m_channelImage, "channelicon");
    UIUtilW::Assign(this, m_jumpToText, "jumptotext");

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Cannot load screen '%1'").arg(windowName));
        return false;
    }

    BuildFocusList();

    MythUIImage *videoImage = dynamic_cast<MythUIImage *>(GetChild("video"));
    if (videoImage && m_embedVideo)
        m_videoRect = videoImage->GetArea();
    else
        m_videoRect = QRect(0,0,0,0);

    m_channelCount = m_guideGrid->getChannelCount();
    m_timeCount = m_guideGrid->getTimeCount() * 6;
    m_verticalLayout = m_guideGrid->isVerticalLayout();

    m_currentEndTime = m_currentStartTime.addSecs(m_timeCount * 60 * 5);

    LoadInBackground();
    return true;
}

void GuideGrid::Load(void)
{
    LoadFromScheduler(m_recList);
    fillChannelInfos();

    int maxchannel = max((int)GetChannelCount() - 1, 0);
    setStartChannel((int)(m_currentStartChannel) - (int)(m_channelCount / 2));
    m_channelCount = min(m_channelCount, maxchannel + 1);

    for (int y = 0; y < m_channelCount; ++y)
    {
        int chanNum = y + m_currentStartChannel;
        if (chanNum >= (int) m_channelInfos.size())
            chanNum -= (int) m_channelInfos.size();
        if (chanNum >= (int) m_channelInfos.size())
            continue;

        if (chanNum < 0)
            chanNum = 0;

        delete m_programs[y];
        m_programs[y] = getProgramListFromProgram(chanNum);
    }
}

void GuideGrid::Init(void)
{
    m_currentRow = (int)(m_channelCount / 2);
    m_currentCol = 0;

    fillTimeInfos();

    updateChannels();

    fillProgramInfos(true);
    updateInfo();

    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, SIGNAL(timeout()), SLOT(updateTimeout()) );
    m_updateTimer->start(60 * 1000);

    updateDateText();

    QString changrpname = ChannelGroup::GetChannelGroupName(m_changrpid);

    if (m_changroupname)
        m_changroupname->SetText(changrpname);

    gCoreContext->addListener(this);
}

GuideGrid::~GuideGrid()
{
    gCoreContext->removeListener(this);

    while (!m_programs.empty())
    {
        if (m_programs.back())
            delete m_programs.back();
        m_programs.pop_back();
    }

    m_channelInfos.clear();

    if (m_updateTimer)
    {
        m_updateTimer->disconnect(this);
        m_updateTimer = NULL;
    }

    if (m_previewVideoRefreshTimer)
    {
        m_previewVideoRefreshTimer->disconnect(this);
        m_previewVideoRefreshTimer = NULL;
    }

    gCoreContext->SaveSetting("EPGSortReverse", m_sortReverse ? "1" : "0");

    // if we have a player and we are returning to it we need
    // to tell it to stop embedding and return to fullscreen
    if (m_player && m_allowFinder)
    {
        QString message = QString("EPG_EXITING");
        qApp->postEvent(m_player, new MythEvent(message));
    }

    // maybe the user selected a different channel group,
    // tell the player to update its channel list just in case
    if (m_player)
        m_player->UpdateChannelList(m_changrpid);

    if (gCoreContext->GetNumSetting("ChannelGroupRememberLast", 0))
        gCoreContext->SaveSetting("ChannelGroupDefault", m_changrpid);
}

bool GuideGrid::keyPressEvent(QKeyEvent *event)
{
    QStringList actions;
    bool handled = false;
    handled = GetMythMainWindow()->TranslateKeyPress("TV Frontend", event, actions);

    if (handled)
        return true;

    if (actions.size())
    {
        QMutexLocker locker(&m_jumpToChannelLock);

        if (!m_jumpToChannel)
        {
            QString chanNum = actions[0];
            bool isNum;
            chanNum.toInt(&isNum);
            if (isNum)
            {
                // see if we can find a matching channel before creating the JumpToChannel otherwise
                // JumpToChannel will delete itself in the ctor leading to a segfault
                int i = FindChannel(0, chanNum, false);
                if (i >= 0)
                {
                    m_jumpToChannel = new JumpToChannel(this, chanNum,
                                                        m_currentStartChannel,
                                                        m_currentRow, m_channelCount);
                    updateJumpToChannel();
                }

                handled = true;
            }
        }

        if (m_jumpToChannel && !handled)
            handled = m_jumpToChannel->ProcessEntry(actions, event);
    }

    for (int i = 0; i < actions.size() && !handled; ++i)
    {
        QString action = actions[i];
        handled = true;
        if (action == ACTION_UP)
        {
            if (m_verticalLayout)
                cursorLeft();
            else
                cursorUp();
        }
        else if (action == ACTION_DOWN)
        {
            if (m_verticalLayout)
                cursorRight();
            else
                cursorDown();
        }
        else if (action == ACTION_LEFT)
        {
            if (m_verticalLayout)
                cursorUp();
            else
                cursorLeft();
        }
        else if (action == ACTION_RIGHT)
        {
            if (m_verticalLayout)
                cursorDown();
            else
                cursorRight();
        }
        else if (action == "PAGEUP")
        {
            if (m_verticalLayout)
                moveLeftRight(kPageLeft);
            else
                moveUpDown(kPageUp);
        }
        else if (action == "PAGEDOWN")
        {
            if (m_verticalLayout)
                moveLeftRight(kPageRight);
            else
                moveUpDown(kPageDown);
        }
        else if (action == ACTION_PAGELEFT)
        {
            if (m_verticalLayout)
                moveUpDown(kPageUp);
            else
                moveLeftRight(kPageLeft);
        }
        else if (action == ACTION_PAGERIGHT)
        {
            if (m_verticalLayout)
                moveUpDown(kPageDown);
            else
                moveLeftRight(kPageRight);
        }
        else if (action == ACTION_DAYLEFT)
            moveLeftRight(kDayLeft);
        else if (action == ACTION_DAYRIGHT)
            moveLeftRight(kDayRight);
        else if (action == "NEXTFAV")
            toggleGuideListing();
        else if (action == ACTION_FINDER)
            showProgFinder();
        else if (action == "MENU")
            ShowMenu();
        else if (action == "ESCAPE" || action == ACTION_GUIDE)
            Close();
        else if (action == ACTION_SELECT)
        {
            if (m_player && (m_player->GetState(-1) == kState_WatchingLiveTV))
            {
                // See if this show is far enough into the future that it's
                // probable that the user wanted to schedule it to record
                // instead of changing the channel.
                ProgramInfo *pginfo =
                    m_programInfos[m_currentRow][m_currentCol];
                int secsTillStart =
                    (pginfo) ? MythDate::current().secsTo(
                        pginfo->GetScheduledStartTime()) : 0;
                if (pginfo && (pginfo->GetTitle() != kUnknownTitle) &&
                    ((secsTillStart / 60) >= m_selectRecThreshold))
                {
                    editRecSchedule();
                }
                else
                {
                    enter();
                }
            }
            else
                editRecSchedule();
        }
        else if (action == "EDIT")
            editSchedule();
        else if (action == "CUSTOMEDIT")
            customEdit();
        else if (action == "DELETE")
            deleteRule();
        else if (action == "UPCOMING")
            upcoming();
        else if (action == "DETAILS" || action == "INFO")
            details();
        else if (action == ACTION_TOGGLERECORD)
            quickRecord();
        else if (action == ACTION_TOGGLEFAV)
        {
            if (m_changrpid == -1)
                ChannelGroupMenu(0);
            else
                toggleChannelFavorite();
        }
        else if (action == "CHANUPDATE")
            channelUpdate();
        else if (action == ACTION_VOLUMEUP)
            volumeUpdate(true);
        else if (action == ACTION_VOLUMEDOWN)
            volumeUpdate(false);
        else if (action == "CYCLEAUDIOCHAN")
            toggleMute(true);
        else if (action == ACTION_MUTEAUDIO)
            toggleMute();
        else if (action == ACTION_TOGGLEPGORDER)
        {
            m_sortReverse = !m_sortReverse;
            generateListings();
            updateChannels();
        }
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void GuideGrid::ShowMenu(void)
{
    QString label = tr("Guide Options");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythDialogBox *menuPopup = new MythDialogBox(label, popupStack,
                                                 "guideMenuPopup");

    if (menuPopup->Create())
    {
        menuPopup->SetReturnEvent(this, "guidemenu");

        if (m_player && (m_player->GetState(-1) == kState_WatchingLiveTV))
            menuPopup->AddButton(tr("Change to Channel"));

        menuPopup->AddButton(tr("Record This"));

        menuPopup->AddButton(tr("Recording Options"), NULL, true);

        menuPopup->AddButton(tr("Program Details"));

        menuPopup->AddButton(tr("Jump to Time"), NULL, true);

        menuPopup->AddButton(tr("Reverse Channel Order"));

        if (!m_changrplist.empty())
        {
            menuPopup->AddButton(tr("Choose Channel Group"));

            if (m_changrpid == -1)
                menuPopup->AddButton(tr("Add To Channel Group"), NULL, true);
            else
                menuPopup->AddButton(tr("Remove from Channel Group"), NULL, true);
        }

        popupStack->AddScreen(menuPopup);
    }
    else
    {
        delete menuPopup;
    }
}

void GuideGrid::ShowRecordingMenu(void)
{
    QString label = tr("Recording Options");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythDialogBox *menuPopup = new MythDialogBox(label, popupStack,
                                                 "recMenuPopup");

    if (menuPopup->Create())
    {
        menuPopup->SetReturnEvent(this, "recmenu");

        ProgramInfo *pginfo = m_programInfos[m_currentRow][m_currentCol];

        if (pginfo && pginfo->GetRecordingRuleID())
            menuPopup->AddButton(tr("Edit Recording Status"));
        menuPopup->AddButton(tr("Edit Schedule"));
        menuPopup->AddButton(tr("Show Upcoming"));
        menuPopup->AddButton(tr("Custom Edit"));

        if (pginfo && pginfo->GetRecordingRuleID())
            menuPopup->AddButton(tr("Delete Rule"));

        popupStack->AddScreen(menuPopup);
    }
    else
    {
        delete menuPopup;
    }
}

ChannelInfo *GuideGrid::GetChannelInfo(uint chan_idx, int sel)
{
    sel = (sel >= 0) ? sel : m_channelInfoIdx[chan_idx];

    if (chan_idx >= GetChannelCount())
        return NULL;

    if (sel >= (int) m_channelInfos[chan_idx].size())
        return NULL;

    return &(m_channelInfos[chan_idx][sel]);
}

const ChannelInfo *GuideGrid::GetChannelInfo(uint chan_idx, int sel) const
{
    return ((GuideGrid*)this)->GetChannelInfo(chan_idx, sel);
}

uint GuideGrid::GetChannelCount(void) const
{
    return m_channelInfos.size();
}

int GuideGrid::GetStartChannelOffset(int row) const
{
    uint cnt = GetChannelCount();
    if (!cnt)
        return -1;

    row = (row < 0) ? m_currentRow : row;
    return (row + m_currentStartChannel) % cnt;
}

ProgramList GuideGrid::GetProgramList(uint chanid) const
{
    ProgramList proglist;
    MSqlBindings bindings;
    QString querystr =
        "WHERE program.chanid     = :CHANID  AND "
        "      program.endtime   >= :STARTTS AND "
        "      program.starttime <= :ENDTS   AND "
        "      program.manualid   = 0 ";
    bindings[":STARTTS"] =
        m_currentStartTime.addSecs(0 - m_currentStartTime.time().second());
    bindings[":ENDTS"] =
        m_currentEndTime.addSecs(0 - m_currentEndTime.time().second());
    bindings[":CHANID"]  = chanid;

    ProgramList dummy;
    LoadFromProgram(proglist, querystr, bindings, dummy);

    return proglist;
}

uint GuideGrid::GetAlternateChannelIndex(
    uint chan_idx, bool with_same_channum) const
{
    uint si = m_channelInfoIdx[chan_idx];
    const ChannelInfo *chinfo = GetChannelInfo(chan_idx, si);

    PlayerContext *ctx = m_player->GetPlayerReadLock(-1, __FILE__, __LINE__);

    const uint cnt = (ctx && chinfo) ? m_channelInfos[chan_idx].size() : 0;
    for (uint i = 0; i < cnt; ++i)
    {
        if (i == si)
            continue;

        const ChannelInfo *ciinfo = GetChannelInfo(chan_idx, i);
        if (!ciinfo)
            continue;

        bool same_channum = ciinfo->channum == chinfo->channum;

        if (with_same_channum != same_channum)
            continue;

        if (!m_player->IsTunable(ctx, ciinfo->chanid, true))
            continue;

        if (with_same_channum)
        {
            si = i;
            break;
        }

        ProgramList proglist    = GetProgramList(chinfo->chanid);
        ProgramList ch_proglist = GetProgramList(ciinfo->chanid);

        if (proglist.empty() ||
            proglist.size()  != ch_proglist.size())
            continue;

        bool isAlt = true;
        for (uint j = 0; j < proglist.size(); ++j)
        {
            isAlt &= proglist[j]->IsSameProgramTimeslot(*ch_proglist[j]);
        }

        if (isAlt)
        {
            si = i;
            break;
        }
    }

    m_player->ReturnPlayerLock(ctx);

    return si;
}


#define MKKEY(IDX,SEL) ((((uint64_t)IDX) << 32) | SEL)
ChannelInfoList GuideGrid::GetSelection(void) const
{
    ChannelInfoList selected;

    int idx = GetStartChannelOffset();
    if (idx < 0)
        return selected;

    uint si  = m_channelInfoIdx[idx];

    vector<uint64_t> sel;
    sel.push_back( MKKEY(idx, si) );

    const ChannelInfo *ch = GetChannelInfo(sel[0]>>32, sel[0]&0xffff);
    if (!ch)
        return selected;

    selected.push_back(*ch);
    if (m_channelInfos[idx].size() <= 1)
        return selected;

    ProgramList proglist = GetProgramList(selected[0].chanid);

    if (proglist.empty())
        return selected;

    for (uint i = 0; i < m_channelInfos[idx].size(); ++i)
    {
        const ChannelInfo *ci = GetChannelInfo(idx, i);
        if (ci && (i != si) &&
            (ci->callsign == ch->callsign) && (ci->channum  == ch->channum))
        {
            sel.push_back( MKKEY(idx, i) );
        }
    }

    for (uint i = 0; i < m_channelInfos[idx].size(); ++i)
    {
        const ChannelInfo *ci = GetChannelInfo(idx, i);
        if (ci && (i != si) &&
            (ci->callsign == ch->callsign) && (ci->channum  != ch->channum))
        {
            sel.push_back( MKKEY(idx, i) );
        }
    }

    for (uint i = 0; i < m_channelInfos[idx].size(); ++i)
    {
        const ChannelInfo *ci = GetChannelInfo(idx, i);
        if ((i != si) && (ci->callsign != ch->callsign))
        {
            sel.push_back( MKKEY(idx, i) );
        }
    }

    for (uint i = 1; i < sel.size(); ++i)
    {
        const ChannelInfo *ci = GetChannelInfo(sel[i]>>32, sel[i]&0xffff);
        const ProgramList ch_proglist = GetProgramList(ch->chanid);

        if (!ci || proglist.size() != ch_proglist.size())
            continue;

        bool isAlt = true;
        for (uint j = 0; j < proglist.size(); ++j)
        {
            isAlt &= proglist[j]->IsSameProgramTimeslot(*ch_proglist[j]);
        }

        if (isAlt)
            selected.push_back(*ci);
    }

    return selected;
}
#undef MKKEY

void GuideGrid::updateTimeout(void)
{
    m_updateTimer->stop();
    fillProgramInfos();
    m_updateTimer->start((int)(60 * 1000));
}

void GuideGrid::fillChannelInfos(bool gotostartchannel)
{
    m_channelInfos.clear();
    m_channelInfoIdx.clear();
    m_currentStartChannel = 0;

    ChannelInfoList channels = ChannelUtil::GetChannels(
        0, true, "", (m_changrpid < 0) ? 0 : m_changrpid);
    ChannelUtil::SortChannels(channels, m_channelOrdering, false);

    typedef vector<uint> uint_list_t;
    QMap<QString,uint_list_t> channum_to_index_map;
    QMap<QString,uint_list_t> callsign_to_index_map;

    for (uint i = 0; i < channels.size(); ++i)
    {
        uint chan = i;
        if (m_sortReverse)
        {
            chan = channels.size() - i - 1;
        }

        bool ndup = channum_to_index_map[channels[chan].channum].size();
        bool cdup = callsign_to_index_map[channels[chan].callsign].size();

        if (ndup && cdup)
            continue;

        ChannelInfo val(channels[chan]);

        channum_to_index_map[val.channum].push_back(GetChannelCount());
        callsign_to_index_map[val.callsign].push_back(GetChannelCount());

        // add the new channel to the list
        db_chan_list_t tmp;
        tmp.push_back(val);
        m_channelInfos.push_back(tmp);
    }

    // handle duplicates
    for (uint i = 0; i < channels.size(); ++i)
    {
        const uint_list_t &ndups = channum_to_index_map[channels[i].channum];
        for (uint j = 0; j < ndups.size(); ++j)
        {
            if (channels[i].chanid   != m_channelInfos[ndups[j]][0].chanid &&
                channels[i].callsign == m_channelInfos[ndups[j]][0].callsign)
                m_channelInfos[ndups[j]].push_back(channels[i]);
        }

        const uint_list_t &cdups = callsign_to_index_map[channels[i].callsign];
        for (uint j = 0; j < cdups.size(); ++j)
        {
            if (channels[i].chanid != m_channelInfos[cdups[j]][0].chanid)
                m_channelInfos[cdups[j]].push_back(channels[i]);
        }
    }

    if (gotostartchannel)
    {
        int ch = FindChannel(m_startChanID, m_startChanNum);
        m_currentStartChannel = (uint) max(0, ch);
    }

    if (m_channelInfos.empty())
    {
        LOG(VB_GENERAL, LOG_ERR, "GuideGrid: "
                "\n\t\t\tYou don't have any channels defined in the database."
                "\n\t\t\tGuide grid will have nothing to show you.");
    }
}

int GuideGrid::FindChannel(uint chanid, const QString &channum,
                           bool exact) const
{
    static QMutex chanSepRegExpLock;
    static QRegExp chanSepRegExp(ChannelUtil::kATSCSeparators);

    // first check chanid
    uint i = (chanid) ? 0 : GetChannelCount();
    for (; i < GetChannelCount(); ++i)
    {
        if (m_channelInfos[i][0].chanid == chanid)
                return i;
    }

    // then check for chanid in duplicates
    i = (chanid) ? 0 : GetChannelCount();
    for (; i < GetChannelCount(); ++i)
    {
        for (uint j = 1; j < m_channelInfos[i].size(); ++j)
        {
            if (m_channelInfos[i][j].chanid == chanid)
                return i;
        }
    }

    // then check channum, first only
    i = (channum.isEmpty()) ? GetChannelCount() : 0;
    for (; i < GetChannelCount(); ++i)
    {
         if (m_channelInfos[i][0].channum == channum)
            return i;
    }

    // then check channum duplicates
    i = (channum.isEmpty()) ? GetChannelCount() : 0;
    for (; i < GetChannelCount(); ++i)
    {
        for (uint j = 1; j < m_channelInfos[i].size(); ++j)
        {
            if (m_channelInfos[i][j].channum == channum)
                return i;
        }
    }

    if (exact || channum.isEmpty())
        return -1;

    // then check partial channum, first only
    for (i = 0; i < GetChannelCount(); ++i)
    {
        if (m_channelInfos[i][0].channum.left(channum.length()) == channum)
            return i;
    }

    // then check all partial channum
    for (i = 0; i < GetChannelCount(); ++i)
    {
        for (uint j = 0; j < m_channelInfos[i].size(); ++j)
        {
            if (m_channelInfos[i][j].channum.left(channum.length()) == channum)
                return i;
        }
    }

    // then check all channum with "_" for subchannels
    QMutexLocker locker(&chanSepRegExpLock);
    QString tmpchannum = channum;
    if (tmpchannum.contains(chanSepRegExp))
    {
        tmpchannum.replace(chanSepRegExp, "_");
    }
    else if (channum.length() >= 2)
    {
        tmpchannum = channum.left(channum.length() - 1) + '_' +
            channum.right(1);
    }
    else
    {
        return -1;
    }

    for (i = 0; i < GetChannelCount(); ++i)
    {
        for (uint j = 0; j < m_channelInfos[i].size(); ++j)
        {
            QString tmp = m_channelInfos[i][j].channum;
            tmp.replace(chanSepRegExp, "_");
            if (tmp == tmpchannum)
                return i;
        }
    }

    return -1;
}

void GuideGrid::fillTimeInfos()
{
    m_timeList->Reset();

    QDateTime starttime = m_currentStartTime;

    m_firstTime = m_currentStartTime;
    m_lastTime = m_firstTime.addSecs(m_timeCount * 60 * 4);

    for (int x = 0; x < m_timeCount; ++x)
    {
        int mins = starttime.time().minute();
        mins = 5 * (mins / 5);
        if (mins % 30 == 0)
        {
            QString timeStr = MythDate::toString(starttime, MythDate::kTime);
            
            InfoMap infomap;
            infomap["starttime"] = timeStr;
            
            QDateTime endtime = starttime.addSecs(60 * 30);
            
            infomap["endtime"] = MythDate::toString(endtime, MythDate::kTime);

            MythUIButtonListItem *item =
                new MythUIButtonListItem(m_timeList, timeStr);

            item->SetTextFromMap(infomap);
        }

        starttime = starttime.addSecs(5 * 60);
    }
    m_currentEndTime = starttime;
}

void GuideGrid::fillProgramInfos(bool useExistingData)
{
    m_guideGrid->ResetData();

    for (int y = 0; y < m_channelCount; ++y)
    {
        fillProgramRowInfos(y, useExistingData);
    }
}

ProgramList *GuideGrid::getProgramListFromProgram(int chanNum)
{
    ProgramList *proglist = new ProgramList();

    if (proglist)
    {
        MSqlBindings bindings;
        QString querystr = "WHERE program.chanid = :CHANID "
                           "  AND program.endtime >= :STARTTS "
                           "  AND program.starttime <= :ENDTS "
                           "  AND program.manualid = 0 ";
        bindings[":CHANID"]  = GetChannelInfo(chanNum)->chanid;
        bindings[":STARTTS"] =
            m_currentStartTime.addSecs(0 - m_currentStartTime.time().second());
        bindings[":ENDTS"] =
            m_currentEndTime.addSecs(0 - m_currentEndTime.time().second());

        LoadFromProgram(*proglist, querystr, bindings, m_recList);
    }

    return proglist;
}

void GuideGrid::fillProgramRowInfos(unsigned int row, bool useExistingData)
{
    m_guideGrid->ResetRow(row);

    // never divide by zero..
    if (!m_guideGrid->getChannelCount() || !m_timeCount)
        return;

    for (int x = 0; x < m_timeCount; ++x)
    {
        m_programInfos[row][x] = NULL;
    }

    if (m_channelInfos.empty())
        return;

    int chanNum = row + m_currentStartChannel;
    if (chanNum >= (int) m_channelInfos.size())
        chanNum -= (int) m_channelInfos.size();
    if (chanNum >= (int) m_channelInfos.size())
        return;

    if (chanNum < 0)
        chanNum = 0;

    if (!useExistingData)
    {
        delete m_programs[row];
        m_programs[row] = getProgramListFromProgram(chanNum);
    }

    ProgramList *proglist = m_programs[row];
    if (!proglist)
        return;

    QDateTime ts = m_currentStartTime;

    QDateTime tnow = MythDate::current();
    int progPast = 0;
    if (tnow > m_currentEndTime)
        progPast = 100;
    else if (tnow < m_currentStartTime)
        progPast = 0;
    else
    {
        int played = m_currentStartTime.secsTo(tnow);
        int length = m_currentStartTime.secsTo(m_currentEndTime);
        if (length)
            progPast = played * 100 / length;
    }

    m_guideGrid->SetProgPast(progPast);

    ProgramList::iterator program = proglist->begin();
    vector<ProgramInfo*> unknownlist;
    bool unknown = false;
    ProgramInfo *proginfo = NULL;
    for (int x = 0; x < m_timeCount; ++x)
    {
        if (program != proglist->end() &&
            (ts >= (*program)->GetScheduledEndTime()))
        {
            ++program;
        }

        if ((program == proglist->end()) ||
            (ts < (*program)->GetScheduledStartTime()))
        {
            if (unknown)
            {
                proginfo->spread++;
                proginfo->SetScheduledEndTime(
                    proginfo->GetScheduledEndTime().addSecs(5 * 60));
            }
            else
            {
                proginfo = new ProgramInfo(kUnknownTitle, kUnknownCategory,
                                           ts, ts.addSecs(5*60));
                unknownlist.push_back(proginfo);
                proginfo->startCol = x;
                proginfo->spread = 1;
                unknown = true;
            }
        }
        else
        {
            if (proginfo && proginfo == *program)
            {
                proginfo->spread++;
            }
            else
            {
                proginfo = *program;
                if (proginfo)
                {
                    proginfo->startCol = x;
                    proginfo->spread = 1;
                    unknown = false;
                }
            }
        }
        m_programInfos[row][x] = proginfo;
        ts = ts.addSecs(5 * 60);
    }

    vector<ProgramInfo*>::iterator it = unknownlist.begin();
    for (; it != unknownlist.end(); ++it)
        proglist->push_back(*it);

    MythRect programRect = m_guideGrid->GetArea();

    /// use doubles to avoid large gaps at end..
    double ydifference = 0.0, xdifference = 0.0;

    if (m_verticalLayout)
    {
        ydifference = programRect.width() /
            (double) m_guideGrid->getChannelCount();
        xdifference = programRect.height() /
            (double) m_timeCount;
    }
    else
    {
        ydifference = programRect.height() /
            (double) m_guideGrid->getChannelCount();
        xdifference = programRect.width() /
            (double) m_timeCount;
    }

    int arrow = 0;
    int cnt = 0;
    int spread = 1;
    QDateTime lastprog;
    QRect tempRect;
    bool isCurrent = false;

    for (int x = 0; x < m_timeCount; ++x)
    {
        ProgramInfo *pginfo = m_programInfos[row][x];
        if (!pginfo)
            continue;

        spread = 1;
        if (pginfo->GetScheduledStartTime() != lastprog)
        {
            arrow = 0;
            if (pginfo->GetScheduledStartTime() < m_firstTime.addSecs(-300))
                arrow = arrow + 1;
            if (pginfo->GetScheduledEndTime() > m_lastTime.addSecs(2100))
                arrow = arrow + 2;

            if (pginfo->spread != -1)
            {
                spread = pginfo->spread;
            }
            else
            {
                for (int z = x + 1; z < m_timeCount; ++z)
                {
                    ProgramInfo *test = m_programInfos[row][z];
                    if (test && (test->GetScheduledStartTime() ==
                                 pginfo->GetScheduledStartTime()))
                        spread++;
                }
                pginfo->spread = spread;
                pginfo->startCol = x;

                for (int z = x + 1; z < x + spread; ++z)
                {
                    ProgramInfo *test = m_programInfos[row][z];
                    if (test)
                    {
                        test->spread = spread;
                        test->startCol = x;
                    }
                }
            }

            if (m_verticalLayout)
            {
                tempRect = QRect((int)(row * ydifference),
                                 (int)(x * xdifference),
                                 (int)(ydifference),
                                 (int)(xdifference * pginfo->spread));
            }
            else
            {
                tempRect = QRect((int)(x * xdifference),
                                 (int)(row * ydifference),
                                 (int)(xdifference * pginfo->spread),
                                 (int)ydifference);
            }

            // snap to right edge for last entry.
            if (tempRect.right() + 2 >=  programRect.width())
                tempRect.setRight(programRect.width());
            if (tempRect.bottom() + 2 >=  programRect.bottom())
                tempRect.setBottom(programRect.bottom());

            if (m_currentRow == (int)row && (m_currentCol >= x) &&
                (m_currentCol < (x + spread)))
                isCurrent = true;
            else
                isCurrent = false;

            int recFlag;
            switch (pginfo->GetRecordingRuleType())
            {
            case kSingleRecord:
                recFlag = 1;
                break;
            case kDailyRecord:
                recFlag = 2;
                break;
            case kAllRecord:
                recFlag = 4;
                break;
            case kWeeklyRecord:
                recFlag = 5;
                break;
            case kOneRecord:
                recFlag = 6;
                break;
            case kOverrideRecord:
            case kDontRecord:
                recFlag = 7;
                break;
            case kNotRecording:
            default:
                recFlag = 0;
                break;
            }

            int recStat;
            if (pginfo->GetRecordingStatus() == rsConflict ||
                pginfo->GetRecordingStatus() == rsOffLine)
                recStat = 2;
            else if (pginfo->GetRecordingStatus() <= rsWillRecord)
                recStat = 1;
            else
                recStat = 0;

            m_guideGrid->SetProgramInfo(
                row, cnt, tempRect, pginfo->GetTitle(),
                pginfo->GetCategory(), arrow, recFlag,
                recStat, isCurrent);

            cnt++;
        }

        lastprog = pginfo->GetScheduledStartTime();
    }
}

void GuideGrid::customEvent(QEvent *event)
{
    if ((MythEvent::Type)(event->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)event;
        QString message = me->Message();

        if (message == "SCHEDULE_CHANGE")
        {
            LoadFromScheduler(m_recList);
            fillProgramInfos();
            updateInfo();
        }
        else if (message == "STOP_VIDEO_REFRESH_TIMER")
        {
            m_previewVideoRefreshTimer->stop();
        }
        else if (message == "START_VIDEO_REFRESH_TIMER")
        {
            m_previewVideoRefreshTimer->start(66);
        }
    }
    else if (event->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = (DialogCompletionEvent*)(event);

        QString resultid   = dce->GetId();
        QString resulttext = dce->GetResultText();
        int     buttonnum  = dce->GetResult();

        if (resultid == "deleterule")
        {
            RecordingRule *record =
                qVariantValue<RecordingRule *>(dce->GetData());
            if (record)
            {
                if ((buttonnum > 0) && !record->Delete())
                    LOG(VB_GENERAL, LOG_ERR, "Failed to delete recording rule");
                delete record;
            }
        }
        else if (resultid == "guidemenu")
        {
            if (resulttext == tr("Record This"))
            {
                quickRecord();
            }
            else if (resulttext == tr("Change to Channel"))
            {
                enter();
            }
            else if (resulttext == tr("Program Details"))
            {
                details();
            }
            else if (resulttext == tr("Reverse Channel Order"))
            {
                m_sortReverse = !m_sortReverse;
                generateListings();
                updateChannels();
            }
            else if (resulttext == tr("Add To Channel Group"))
            {
                if (m_changrpid == -1)
                    ChannelGroupMenu(0);
            }
            else if (resulttext == tr("Remove from Channel Group"))
            {
                toggleChannelFavorite();
            }
            else if (resulttext == tr("Choose Channel Group"))
            {
                ChannelGroupMenu(1);
            }
            else if (resulttext == tr("Recording Options"))
            {
                ShowRecordingMenu();
            }
            else if (resulttext == tr("Jump to Time"))
            {
                ShowJumpToTime();
            }
        }
        else if (resultid == "recmenu")
        {
            if (resulttext == tr("Edit Recording Status"))
            {
                editRecSchedule();
            }
            else if (resulttext == tr("Edit Schedule"))
            {
                editSchedule();
            }
            else if (resulttext == tr("Show Upcoming"))
            {
                upcoming();
            }
            else if (resulttext == tr("Custom Edit"))
            {
                customEdit();
            }
            else if (resulttext == tr("Delete Rule"))
            {
                deleteRule();
            }

        }
        else if (resultid == "channelgrouptogglemenu")
        {
            int changroupid;
            changroupid = ChannelGroup::GetChannelGroupId(resulttext);

            if (changroupid > 0)
                toggleChannelFavorite(changroupid);
        }
        else if (resultid == "channelgroupmenu")
        {
            if (buttonnum >= 0)
            {
                int changroupid;

                if (resulttext == QObject::tr("All Channels"))
                    changroupid = -1;
                else
                    changroupid = ChannelGroup::GetChannelGroupId(resulttext);

                m_changrpid = changroupid;
                generateListings();
                updateChannels();
                updateInfo();

                QString changrpname;
                changrpname = ChannelGroup::GetChannelGroupName(m_changrpid);

                if (m_changroupname)
                    m_changroupname->SetText(changrpname);
            }
        }
        else if (resultid == "jumptotime")
        {
            QDateTime datetime = dce->GetData().toDateTime();
            moveToTime(datetime);
        }
        else
            ScheduleCommon::customEvent(event);
    }
}

void GuideGrid::updateDateText(void)
{
    if (m_dateText)
        m_dateText->SetText(MythDate::toString(m_currentStartTime, MythDate::kDateShort));
    if (m_longdateText)
        m_longdateText->SetText(MythDate::toString(m_currentStartTime,
                                                 (MythDate::kDateFull | MythDate::kSimplify)));
}

void GuideGrid::updateChannels(void)
{
    m_channelList->Reset();

    ChannelInfo *chinfo = GetChannelInfo(m_currentStartChannel);

    if (m_player)
        m_player->ClearTunableCache();

    for (unsigned int y = 0; (y < (unsigned int)m_channelCount) && chinfo; ++y)
    {
        unsigned int chanNumber = y + m_currentStartChannel;
        if (chanNumber >= m_channelInfos.size())
            chanNumber -= m_channelInfos.size();
        if (chanNumber >= m_channelInfos.size())
            break;

        chinfo = GetChannelInfo(chanNumber);

        bool unavailable = false, try_alt = false;

        if (m_player)
        {
            const PlayerContext *ctx = m_player->GetPlayerReadLock(
                -1, __FILE__, __LINE__);
            if (ctx && chinfo)
                try_alt = !m_player->IsTunable(ctx, chinfo->chanid, true);
            m_player->ReturnPlayerLock(ctx);
        }

        if (try_alt)
        {
            unavailable = true;

            // Try alternates with same channum if applicable
            uint alt = GetAlternateChannelIndex(chanNumber, true);
            if (alt != m_channelInfoIdx[chanNumber])
            {
                unavailable = false;
                m_channelInfoIdx[chanNumber] = alt;
                chinfo = GetChannelInfo(chanNumber);
            }

            // Try alternates with different channum if applicable
            if (unavailable && chinfo &&
                !GetProgramList(chinfo->chanid).empty())
            {
                alt = GetAlternateChannelIndex(chanNumber, false);
                unavailable = (alt == m_channelInfoIdx[chanNumber]);
            }
        }

        MythUIButtonListItem *item =
            new MythUIButtonListItem(m_channelList,
                                     chinfo ? chinfo->GetFormatted(ChannelInfo::kChannelShort) : QString());

        QString state = "available";
        if (unavailable)
            state = (m_changrpid == -1) ? "unavailable" : "favunavailable";
        else
            state = (m_changrpid == -1) ? "available" : "favourite";

        item->SetFontState(state);
        item->DisplayState(state, "chanstatus");

        if (chinfo)
        {
            InfoMap infomap;
            chinfo->ToMap(infomap);
            item->SetTextFromMap(infomap);

            if (!chinfo->icon.isEmpty())
            {
                QString iconurl =
                                gCoreContext->GetMasterHostPrefix("ChannelIcons",
                                                                  chinfo->icon);
                item->SetImage(iconurl, "channelicon");
            }
        }
    }
}

void GuideGrid::updateInfo(void)
{
    if (m_currentRow < 0 || m_currentCol < 0)
        return;

    ProgramInfo *pginfo = m_programInfos[m_currentRow][m_currentCol];
    if (!pginfo)
        return;

    InfoMap infoMap;

    int chanNum = m_currentRow + m_currentStartChannel;
    if (chanNum >= (int)m_channelInfos.size())
        chanNum -= (int)m_channelInfos.size();
    if (chanNum >= (int)m_channelInfos.size())
        return;
    if (chanNum < 0)
        chanNum = 0;

    ChannelInfo *chinfo = GetChannelInfo(chanNum);

    if (m_channelImage)
    {
        m_channelImage->Reset();
        if (!chinfo->icon.isEmpty())
        {
            QString iconurl = gCoreContext->GetMasterHostPrefix("ChannelIcons",
                                                                chinfo->icon);

            m_channelImage->SetFilename(iconurl);
            m_channelImage->Load();
        }
    }

    chinfo->ToMap(infoMap);
    pginfo->ToMap(infoMap);
    SetTextFromMap(infoMap);

    MythUIStateType *ratingState = dynamic_cast<MythUIStateType*>
                                                (GetChild("ratingstate"));
    if (ratingState)
    {
        QString rating = QString::number(pginfo->GetStars(10));
        ratingState->DisplayState(rating);
    }
}

void GuideGrid::toggleGuideListing()
{
    int oldchangrpid = m_changrpid;

    m_changrpid = ChannelGroup::GetNextChannelGroup(m_changrplist, oldchangrpid);

    if (oldchangrpid != m_changrpid)
      generateListings();

    updateChannels();
    updateInfo();

    QString changrpname = ChannelGroup::GetChannelGroupName(m_changrpid);

    if (m_changroupname)
        m_changroupname->SetText(changrpname);
}

void GuideGrid::generateListings()
{
    m_currentStartChannel = 0;
    m_currentRow = 0;

    int maxchannel = 0;
    fillChannelInfos();
    maxchannel = max((int)GetChannelCount() - 1, 0);
    m_channelCount = min(m_guideGrid->getChannelCount(), maxchannel + 1);

    LoadFromScheduler(m_recList);
    fillProgramInfos();
}

void GuideGrid::ChannelGroupMenu(int mode)
{
    ChannelGroupList channels = ChannelGroup::GetChannelGroups(mode == 0);
    
    if (channels.empty())
    {
      QString message = tr("You don't have any channel groups defined");

      MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

      MythConfirmationDialog *okPopup = new MythConfirmationDialog(popupStack,
                                                                   message, false);
      if (okPopup->Create())
          popupStack->AddScreen(okPopup);
      else
          delete okPopup;

      return;
    }

    QString label = tr("Select Channel Group");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythDialogBox *menuPopup = new MythDialogBox(label, popupStack, "menuPopup");

    if (menuPopup->Create())
    {
        if (mode == 0)
        {
            // add channel to group menu
            menuPopup->SetReturnEvent(this, "channelgrouptogglemenu");
        }
        else
        {
            // switch to channel group menu
            menuPopup->SetReturnEvent(this, "channelgroupmenu");
            menuPopup->AddButton(QObject::tr("All Channels"));
        }

        for (uint i = 0; i < channels.size(); ++i)
        {
            menuPopup->AddButton(channels[i].name);
        }
        
        popupStack->AddScreen(menuPopup);
    }
    else
    {
        delete menuPopup;
    }
}

void GuideGrid::toggleChannelFavorite(int grpid)
{
    MSqlQuery query(MSqlQuery::InitCon());

    if (grpid == -1)
    {
      if (m_changrpid == -1)
          return;
      else
          grpid = m_changrpid;
    }

    // Get current channel id, and make sure it exists...
    int chanNum = m_currentRow + m_currentStartChannel;
    if (chanNum >= (int)m_channelInfos.size())
        chanNum -= (int)m_channelInfos.size();
    if (chanNum >= (int)m_channelInfos.size())
        return;
    if (chanNum < 0)
        chanNum = 0;

    ChannelInfo *ch = GetChannelInfo(chanNum);
    uint chanid = ch->chanid;

    if (m_changrpid == -1)
        // If currently viewing all channels, allow to add only not delete
        ChannelGroup::ToggleChannel(chanid, grpid, false);
    else
        // Only allow delete if viewing the favorite group in question
        ChannelGroup::ToggleChannel(chanid, grpid, true);

    //regenerate the list of non empty group in case it did change
    m_changrplist = ChannelGroup::GetChannelGroups(false);

    // If viewing favorites, refresh because a channel was removed
    if (m_changrpid != -1)
    {
        generateListings();
        updateChannels();
        updateInfo();
    }
}

void GuideGrid::cursorLeft()
{
    ProgramInfo *test = m_programInfos[m_currentRow][m_currentCol];

    if (!test)
    {
        moveLeftRight(kScrollLeft);
        return;
    }

    int startCol = test->startCol;
    m_currentCol = startCol - 1;

    if (m_currentCol < 0)
    {
        m_currentCol = 0;
        moveLeftRight(kScrollLeft);
    }
    else
    {
        fillProgramRowInfos(m_currentRow);
        m_guideGrid->SetRedraw();
        updateInfo();
    }
}

void GuideGrid::cursorRight()
{
    ProgramInfo *test = m_programInfos[m_currentRow][m_currentCol];

    if (!test)
    {
        moveLeftRight(kScrollRight);
        return;
    }

    int spread = test->spread;
    int startCol = test->startCol;

    m_currentCol = startCol + spread;

    if (m_currentCol > m_timeCount - 1)
    {
        m_currentCol = m_timeCount - 1;
        moveLeftRight(kScrollRight);
    }
    else
    {
        fillProgramRowInfos(m_currentRow);
        m_guideGrid->SetRedraw();
        updateInfo();
    }
}

void GuideGrid::cursorDown()
{
    m_currentRow++;

    if (m_currentRow > m_channelCount - 1)
    {
        m_currentRow = m_channelCount - 1;
        moveUpDown(kScrollDown);
    }
    else
    {
        fillProgramRowInfos(m_currentRow);
        m_guideGrid->SetRedraw();
        updateInfo();
        updateChannels();
    }
}

void GuideGrid::cursorUp()
{
    m_currentRow--;

    if (m_currentRow < 0)
    {
        m_currentRow = 0;
        moveUpDown(kScrollUp);
    }
    else
    {
        fillProgramRowInfos(m_currentRow);
        m_guideGrid->SetRedraw();
        updateInfo();
        updateChannels();
    }
}

void GuideGrid::moveLeftRight(MoveVector movement)
{
    switch (movement)
    {
        case kScrollLeft :
            m_currentStartTime = m_currentStartTime.addSecs(-30 * 60);
            break;
        case kScrollRight :
            m_currentStartTime = m_currentStartTime.addSecs(30 * 60);
            break;
        case kPageLeft :
            m_currentStartTime = m_currentStartTime.addSecs(-5 * 60 * m_timeCount);
            break;
        case kPageRight :
            m_currentStartTime = m_currentStartTime.addSecs(5 * 60 * m_timeCount);
            break;
        case kDayLeft :
            m_currentStartTime = m_currentStartTime.addSecs(-24 * 60 * 60);
            break;
        case kDayRight :
            m_currentStartTime = m_currentStartTime.addSecs(24 * 60 * 60);
            break;
        default :
            break;
    }

    fillTimeInfos();
    fillProgramInfos();
    m_guideGrid->SetRedraw();
    updateInfo();
    updateDateText();
}

void GuideGrid::moveUpDown(MoveVector movement)
{
    switch (movement)
    {
        case kScrollDown :
            setStartChannel(m_currentStartChannel + 1);
            break;
        case kScrollUp :
            setStartChannel((int)(m_currentStartChannel) - 1);
            break;
        case kPageDown :
            setStartChannel(m_currentStartChannel + m_channelCount);
            break;
        case kPageUp :
            setStartChannel((int)(m_currentStartChannel) - m_channelCount);
            break;
        default :
            break;
    }

    fillProgramInfos();
    m_guideGrid->SetRedraw();
    updateInfo();
    updateChannels();
}

void GuideGrid::moveToTime(QDateTime datetime)
{
    if (!datetime.isValid())
        return;

    m_currentStartTime = datetime;

    fillTimeInfos();
    fillProgramInfos();
    m_guideGrid->SetRedraw();
    updateInfo();
    updateDateText();
}

void GuideGrid::setStartChannel(int newStartChannel)
{
    if (newStartChannel < 0)
        m_currentStartChannel = newStartChannel + GetChannelCount();
    else if (newStartChannel >= (int) GetChannelCount())
        m_currentStartChannel = newStartChannel - GetChannelCount();
    else
        m_currentStartChannel = newStartChannel;
}

void GuideGrid::showProgFinder()
{
    if (m_allowFinder)
        RunProgramFinder(m_player, m_embedVideo, false);
}

void GuideGrid::enter()
{
    if (!m_player)
        return;

    if (m_updateTimer)
        m_updateTimer->stop();

    channelUpdate();

    // Don't perform transition effects when guide is being used during playback
    GetScreenStack()->PopScreen(this, false);

    epgIsVisibleCond.wakeAll();
}

void GuideGrid::Close()
{
    // HACK: Do not allow exit if we have a popup menu open, not convinced
    // that this is the right solution
    if (GetMythMainWindow()->GetStack("popup stack")->TotalScreens() > 0)
        return;

    if (m_updateTimer)
        m_updateTimer->stop();

    // don't fade the screen if we are returning to the player
    if (m_player)
        GetScreenStack()->PopScreen(this, false);
    else
        GetScreenStack()->PopScreen(this, true);

    epgIsVisibleCond.wakeAll();
}

void GuideGrid::quickRecord()
{
    ProgramInfo *pginfo = m_programInfos[m_currentRow][m_currentCol];

    if (!pginfo)
        return;

    if (pginfo->GetTitle() == kUnknownTitle)
        return;

    QuickRecord(pginfo);
    LoadFromScheduler(m_recList);
    fillProgramInfos();
    updateInfo();
}

void GuideGrid::editRecSchedule()
{
    ProgramInfo *pginfo = m_programInfos[m_currentRow][m_currentCol];

    if (!pginfo)
        return;

    if (pginfo->GetTitle() == kUnknownTitle)
        return;

    EditRecording(pginfo);
}

void GuideGrid::editSchedule()
{
    ProgramInfo *pginfo = m_programInfos[m_currentRow][m_currentCol];

    if (!pginfo)
        return;

    if (pginfo->GetTitle() == kUnknownTitle)
        return;

    EditScheduled(pginfo);
}

void GuideGrid::customEdit()
{
    ProgramInfo *pginfo = m_programInfos[m_currentRow][m_currentCol];

    EditCustom(pginfo);
}

void GuideGrid::deleteRule()
{
    ProgramInfo *pginfo = m_programInfos[m_currentRow][m_currentCol];

    if (!pginfo || !pginfo->GetRecordingRuleID())
        return;

    RecordingRule *record = new RecordingRule();
    if (!record->LoadByProgram(pginfo))
    {
        delete record;
        return;
    }

    QString message = tr("Delete '%1' %2 rule?").arg(record->m_title)
        .arg(toString(pginfo->GetRecordingRuleType()));

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythConfirmationDialog *okPopup = new MythConfirmationDialog(popupStack,
                                                                 message, true);

    okPopup->SetReturnEvent(this, "deleterule");
    okPopup->SetData(qVariantFromValue(record));

    if (okPopup->Create())
        popupStack->AddScreen(okPopup);
    else
        delete okPopup;
}

void GuideGrid::upcoming()
{
    ProgramInfo *pginfo = m_programInfos[m_currentRow][m_currentCol];

    if (!pginfo)
        return;

    if (pginfo->GetTitle() == kUnknownTitle)
        return;

    ShowUpcoming(pginfo);
}

void GuideGrid::details()
{
    ProgramInfo *pginfo = m_programInfos[m_currentRow][m_currentCol];

    if (!pginfo)
        return;

    if (pginfo->GetTitle() == kUnknownTitle)
        return;

    ShowDetails(pginfo);
}

void GuideGrid::channelUpdate(void)
{
    if (!m_player)
        return;

    ChannelInfoList sel = GetSelection();

    if (sel.size())
    {
        PlayerContext *ctx = m_player->GetPlayerReadLock(-1, __FILE__, __LINE__);
        m_player->ChangeChannel(ctx, sel);
        m_player->ReturnPlayerLock(ctx);
    }
}

void GuideGrid::volumeUpdate(bool up)
{
    if (m_player)
    {
        PlayerContext *ctx = m_player->GetPlayerReadLock(-1, __FILE__, __LINE__);
        m_player->ChangeVolume(ctx, up);
        m_player->ReturnPlayerLock(ctx);
    }
}

void GuideGrid::toggleMute(const bool muteIndividualChannels)
{
    if (m_player)
    {
        PlayerContext *ctx = m_player->GetPlayerReadLock(-1, __FILE__, __LINE__);
        m_player->ToggleMute(ctx, muteIndividualChannels);
        m_player->ReturnPlayerLock(ctx);
    }
}

void GuideGrid::GoTo(int start, int cur_row)
{
    setStartChannel(start);
    m_currentRow = cur_row % m_channelCount;
    updateChannels();
    fillProgramInfos();
    updateInfo();
    updateJumpToChannel();
}

void GuideGrid::updateJumpToChannel(void)
{
    QString txt;
    {
        QMutexLocker locker(&m_jumpToChannelLock);
        if (m_jumpToChannel)
            txt = m_jumpToChannel->GetEntry();
    }

    if (txt.isEmpty())
        return;

    if (m_jumpToText)
        m_jumpToText->SetText(txt);
}

void GuideGrid::SetJumpToChannel(JumpToChannel *ptr)
{
    QMutexLocker locker(&m_jumpToChannelLock);
    m_jumpToChannel = ptr;

    if (!m_jumpToChannel)
    {
        if (m_jumpToText)
            m_jumpToText->Reset();

        updateDateText();
    }
}

void GuideGrid::HideTVWindow(void)
{
    GetMythMainWindow()->GetPaintWindow()->clearMask();
}

void GuideGrid::EmbedTVWindow(void)
{
    MythEvent *me = new MythEvent("STOP_VIDEO_REFRESH_TIMER");
    qApp->postEvent(this, me);

    m_usingNullVideo = !m_player->StartEmbedding(m_videoRect);
    if (!m_usingNullVideo)
    {
        QRegion r1 = QRegion(m_Area);
        QRegion r2 = QRegion(m_videoRect);
        GetMythMainWindow()->GetPaintWindow()->setMask(r1.xored(r2));
        m_player->DrawUnusedRects();
    }
    else
    {
        me = new MythEvent("START_VIDEO_REFRESH_TIMER");
        qApp->postEvent(this, me);
    }
}

void GuideGrid::refreshVideo(void)
{
    if (m_player && m_usingNullVideo)
    {
        GetMythMainWindow()->GetPaintWindow()->update(m_videoRect);
    }
}

void GuideGrid::aboutToHide(void)
{
    if (m_player)
        HideTVWindow();

    MythScreenType::aboutToHide();
}

void GuideGrid::aboutToShow(void)
{
    if (m_player)
        EmbedTVWindow();

    MythScreenType::aboutToShow();
}

void GuideGrid::ShowJumpToTime(void)
{
    QString message =  tr("Jump to a specific date and time in the guide");
    int flags = (MythTimeInputDialog::kAllDates | MythTimeInputDialog::kDay |
                 MythTimeInputDialog::kHours);

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythTimeInputDialog *timedlg = new MythTimeInputDialog(popupStack, message,
                                                           flags);

    if (timedlg->Create())
    {
        timedlg->SetReturnEvent(this, "jumptotime");
        popupStack->AddScreen(timedlg);
    }
    else
        delete timedlg;
}

