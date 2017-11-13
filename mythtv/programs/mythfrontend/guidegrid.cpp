
#include "guidegrid.h"

// c/c++
#include <stdint.h>                     // for uint64_t
#include <deque>                        // for _Deque_iterator, operator!=, etc
#include <algorithm>
using namespace std;

//qt
#include <QCoreApplication>
#include <QKeyEvent>
#include <QDateTime>

// libmythbase
#include "mythdate.h"
#include "mythcorecontext.h"
#include "mythdbcon.h"
#include "mythlogging.h"
#include "autodeletedeque.h"            // for AutoDeleteDeque, etc
#include "mythevent.h"                  // for MythEvent, etc
#include "mythtypes.h"                  // for InfoMap

// libmyth
#include "programtypes.h"               // for RecStatus, etc

// libmythtv
#include "remoteutil.h"
#include "channelutil.h"
#include "cardutil.h"
#include "tvremoteutil.h"
#include "channelinfo.h"
#include "programinfo.h"
#include "recordingrule.h"
#include "tv_play.h"
#include "tv.h"                         // for ::kState_WatchingLiveTV
#include "tv_actions.h"                 // for ACTION_CHANNELSEARCH, etc
#include "recordingtypes.h"             // for toString, etc

// libmythui
#include "mythuibuttonlist.h"
#include "mythuiguidegrid.h"
#include "mythuistatetype.h"
#include "mythdialogbox.h"
#include "mythuiimage.h"
#include "mythuitext.h"
#include "mythmainwindow.h"             // for GetMythMainWindow, etc
#include "mythrect.h"                   // for MythRect
#include "mythscreenstack.h"            // for MythScreenStack
#include "mythscreentype.h"             // for MythScreenType
#include "mythuiactions.h"              // for ACTION_SELECT, ACTION_DOWN, etc
#include "mythuiutils.h"                // for UIUtilW, UIUtilE
#include "mythgesture.h"

// mythfrontend
#include "progfind.h"

QWaitCondition epgIsVisibleCond;

#define LOC      QString("GuideGrid: ")
#define LOC_ERR  QString("GuideGrid, Error: ")
#define LOC_WARN QString("GuideGrid, Warning: ")

const QString kUnknownTitle = "";
//const QString kUnknownCategory = QObject::tr("Unknown");
const unsigned long kUpdateMS = 60 * 1000UL; // Grid update interval (mS)
static bool SelectionIsTunable(const ChannelInfoList &selection);

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

// GuideStatus is used for transferring the relevant read-only data
// from GuideGrid to the GuideUpdateProgramRow constructor.
class GuideStatus
{
public:
    GuideStatus(unsigned int firstRow, unsigned int numRows,
                const QVector<int> &channums,
                const MythRect &gg_programRect,
                int gg_channelCount,
                const QDateTime &currentStartTime,
                const QDateTime &currentEndTime,
                uint currentStartChannel,
                int currentRow, int currentCol,
                int channelCount, int timeCount,
                bool verticalLayout,
                const QDateTime &firstTime, const QDateTime &lastTime)
        : m_firstRow(firstRow), m_numRows(numRows), m_channums(channums),
          m_gg_programRect(gg_programRect), m_gg_channelCount(gg_channelCount),
          m_currentStartTime(currentStartTime),
          m_currentEndTime(currentEndTime),
          m_currentStartChannel(currentStartChannel), m_currentRow(currentRow),
          m_currentCol(currentCol), m_channelCount(channelCount),
          m_timeCount(timeCount), m_verticalLayout(verticalLayout),
          m_firstTime(firstTime), m_lastTime(lastTime) {}
    const unsigned int m_firstRow, m_numRows;
    const QVector<int> m_channums;
    const MythRect m_gg_programRect;
    const int m_gg_channelCount;
    const QDateTime m_currentStartTime, m_currentEndTime;
    const uint m_currentStartChannel;
    const int m_currentRow, m_currentCol;
    const int m_channelCount, m_timeCount;
    const bool m_verticalLayout;
    const QDateTime m_firstTime, m_lastTime;
};

class GuideUpdaterBase
{
public:
    explicit GuideUpdaterBase(GuideGrid *guide) : m_guide(guide) {}
    virtual ~GuideUpdaterBase() {}

    // Execute the initial non-UI part (in a separate thread).  Return
    // true if ExecuteUI() should be run later, or false if the work
    // is no longer relevant (e.g., the UI elements have scrolled
    // offscreen by now).
    virtual bool ExecuteNonUI(void) = 0;
    // Execute the UI part in the UI thread.
    virtual void ExecuteUI(void) = 0;

protected:
    GuideGrid *m_guide;
};

class GuideUpdateProgramRow : public GuideUpdaterBase
{
public:
    GuideUpdateProgramRow(GuideGrid *guide, const GuideStatus &gs,
                          const QVector<ProgramList*> &proglists)
        : GuideUpdaterBase(guide),
          m_firstRow(gs.m_firstRow),
          m_numRows(gs.m_numRows),
          m_channums(gs.m_channums),
          m_gg_programRect(gs.m_gg_programRect),
          m_gg_channelCount(gs.m_gg_channelCount),
          m_currentStartTime(gs.m_currentStartTime),
          m_currentEndTime(gs.m_currentEndTime),
          m_currentStartChannel(gs.m_currentStartChannel),
          m_currentRow(gs.m_currentRow),
          m_currentCol(gs.m_currentCol),
          m_channelCount(gs.m_channelCount),
          m_timeCount(gs.m_timeCount),
          m_verticalLayout(gs.m_verticalLayout),
          m_firstTime(gs.m_firstTime),
          m_lastTime(gs.m_lastTime),
          m_proglists(proglists),
          m_progPast(0)
    {
        for (unsigned int i = m_firstRow;
             i < m_firstRow + m_numRows; ++i)
            for (int j = 0; j < MAX_DISPLAY_TIMES; ++j)
                m_programInfos[i][j] = NULL;
    }
    virtual ~GuideUpdateProgramRow() {}
    virtual bool ExecuteNonUI(void)
    {
        // Don't bother to do any work if the starting coordinates of
        // the guide have changed while the thread was waiting to
        // start.
        if (m_currentStartChannel != m_guide->GetCurrentStartChannel() ||
            m_currentStartTime != m_guide->GetCurrentStartTime())
        {
            return false;
        }

        for (unsigned int i = 0; i < m_numRows; ++i)
        {
            unsigned int row = i + m_firstRow;
            if (!m_proglists[i])
                m_proglists[i] =
                    m_guide->getProgramListFromProgram(m_channums[i]);
            fillProgramRowInfosWith(row,
                                    m_currentStartTime,
                                    m_proglists[i]);
        }
        return true;
    }
    virtual void ExecuteUI(void)
    {
        m_guide->updateProgramsUI(m_firstRow, m_numRows,
                                  m_progPast, m_proglists,
                                  m_programInfos, m_result);
    }

private:
    void fillProgramRowInfosWith(int row, QDateTime start,
                                 ProgramList *proglist);

    const unsigned int m_firstRow;
    const unsigned int m_numRows;
    const QVector<int> m_channums;
    const MythRect m_gg_programRect;
    const int m_gg_channelCount;
    const QDateTime m_currentStartTime;
    const QDateTime m_currentEndTime;
    const uint m_currentStartChannel;
    const int m_currentRow;
    const int m_currentCol;
    const int m_channelCount;
    const int m_timeCount;
    const bool m_verticalLayout;
    const QDateTime m_firstTime;
    const QDateTime m_lastTime;

    QVector<ProgramList*> m_proglists;
    ProgInfoGuideArray m_programInfos;
    int m_progPast;
    //QVector<GuideUIElement> m_result;
    QLinkedList<GuideUIElement> m_result;
};

class GuideUpdateChannels : public GuideUpdaterBase
{
public:
    GuideUpdateChannels(GuideGrid *guide, uint startChan)
        : GuideUpdaterBase(guide), m_currentStartChannel(startChan) {}
    virtual bool ExecuteNonUI(void)
    {
        if (m_currentStartChannel != m_guide->GetCurrentStartChannel())
            return false;
        m_guide->updateChannelsNonUI(m_chinfos, m_unavailables);
        return true;
    }
    virtual void ExecuteUI(void)
    {
        m_guide->updateChannelsUI(m_chinfos, m_unavailables);
    }
    uint m_currentStartChannel;
    QVector<ChannelInfo *> m_chinfos;
    QVector<bool> m_unavailables;
};

class UpdateGuideEvent : public QEvent
{
public:
    explicit UpdateGuideEvent(GuideUpdaterBase *updater) :
        QEvent(kEventType), m_updater(updater) {}
    GuideUpdaterBase *m_updater;
    static Type kEventType;
};
QEvent::Type UpdateGuideEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

class GuideHelper : public QRunnable
{
public:
    GuideHelper(GuideGrid *guide, GuideUpdaterBase *updater)
        : m_guide(guide), m_updater(updater)
    {
        QMutexLocker locker(&s_lock);
        ++s_loading[m_guide];
    }
    virtual void run(void)
    {
        QThread::currentThread()->setPriority(QThread::IdlePriority);
        if (m_updater)
        {
            if (m_updater->ExecuteNonUI())
                QCoreApplication::postEvent(m_guide,
                                            new UpdateGuideEvent(m_updater));
            else
            {
                delete m_updater;
                m_updater = NULL;
            }
        }

        QMutexLocker locker(&s_lock);
        --s_loading[m_guide];
        if (!s_loading[m_guide])
            s_wait.wakeAll();
    }
    static bool IsLoading(GuideGrid *guide)
    {
        QMutexLocker locker(&s_lock);
        return s_loading[guide];
    }
    static void Wait(GuideGrid *guide)
    {
        QMutexLocker locker(&s_lock);
        while (s_loading[guide])
        {
            if (!s_wait.wait(locker.mutex(), 15000UL))
                return;
        }
    }
private:
    GuideGrid *m_guide;
    GuideUpdaterBase *m_updater;

    static QMutex                s_lock;
    static QWaitCondition        s_wait;
    static QMap<GuideGrid*,uint> s_loading;
};
QMutex                GuideHelper::s_lock;
QWaitCondition        GuideHelper::s_wait;
QMap<GuideGrid*,uint> GuideHelper::s_loading;

void GuideGrid::RunProgramGuide(uint chanid, const QString &channum,
                                const QDateTime &startTime,
                                TV *player, bool embedVideo,
                                bool allowFinder, int changrpid)
{
    // which channel group should we default to
    if (changrpid == -2)
        changrpid = gCoreContext->GetNumSetting("ChannelGroupDefault", -1);

    // check there are some channels setup
    ChannelInfoList channels = ChannelUtil::GetChannels(
        0, true, "", (changrpid<0) ? 0 : changrpid);
    if (channels.empty())
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

    // If chanid/channum are unset, find the channel that would
    // naturally be selected when Live TV is started.  This depends on
    // the available tuners, their capturecard.livetvorder values, and
    // their capturecard.startchan values.
    QString actualChannum = channum;
    if (chanid == 0 && actualChannum.isEmpty())
    {
        uint defaultChanid = gCoreContext->GetNumSetting("DefaultChanid", 0);
        if (defaultChanid && TV::IsTunable(defaultChanid))
            chanid = defaultChanid;
    }
    if (chanid == 0 && actualChannum.isEmpty())
    {
        vector<uint> inputIDs = RemoteRequestFreeInputList(0);
        if (!inputIDs.empty())
            actualChannum = CardUtil::GetStartingChannel(inputIDs[0]);
    }

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    GuideGrid *gg = new GuideGrid(mainStack,
                                  chanid, actualChannum, startTime,
                                  player, embedVideo, allowFinder,
                                  changrpid);

    if (gg->Create())
        mainStack->AddScreen(gg, (player == NULL));
    else
        delete gg;
}

GuideGrid::GuideGrid(MythScreenStack *parent,
                     uint chanid, const QString &channum, const QDateTime &startTime,
                     TV *player, bool embedVideo,
                     bool allowFinder, int changrpid)
         : ScheduleCommon(parent, "guidegrid"),
           m_selectRecThreshold(gCoreContext->GetNumSetting("SelChangeRecThreshold", 16)),
           m_allowFinder(allowFinder),
           m_currentStartChannel(0),
           m_startChanID(chanid),
           m_startChanNum(channum),
           m_currentRow(0),
           m_currentCol(0),
           m_sortReverse(gCoreContext->GetNumSetting("EPGSortReverse", 0)),
           m_channelCount(5),
           m_timeCount(30),
           m_verticalLayout(false),
           m_player(player),
           m_usingNullVideo(false),
           m_embedVideo(embedVideo),
           m_previewVideoRefreshTimer(new QTimer(this)),
           m_channelOrdering(gCoreContext->GetSetting("ChannelOrdering", "channum")),
           m_updateTimer(new QTimer(this)),
           m_threadPool("GuideGridHelperPool"),
           m_changrpid(changrpid),
           m_changrplist(ChannelGroup::GetChannelGroups(false)),
           m_jumpToChannelLock(QMutex::Recursive),
           m_jumpToChannel(NULL),
           m_timeList(NULL),
           m_channelList(NULL),
           m_guideGrid(NULL),
           m_dateText(NULL),
           m_longdateText(NULL),
           m_jumpToText(NULL),
           m_changroupname(NULL),
           m_channelImage(NULL)
{
    connect(m_previewVideoRefreshTimer, SIGNAL(timeout()),
            this,                     SLOT(refreshVideo()));
    connect(m_updateTimer, SIGNAL(timeout()), SLOT(updateTimeout()) );

    for (uint i = 0; i < MAX_DISPLAY_CHANS; i++)
        m_programs.push_back(NULL);

    for (int x = 0; x < MAX_DISPLAY_TIMES; ++x)
    {
        for (int y = 0; y < MAX_DISPLAY_CHANS; ++y)
            m_programInfos[y][x] = NULL;
    }

    m_originalStartTime = MythDate::current();
    if (startTime.isValid() &&
        startTime > m_originalStartTime.addSecs(-8 * 3600))
        m_originalStartTime = startTime;

    int secsoffset = -((m_originalStartTime.time().minute() % 30) * 60 +
                        m_originalStartTime.time().second());
    m_currentStartTime = m_originalStartTime.addSecs(secsoffset);
    m_threadPool.setMaxThreadCount(1);
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

    m_updateTimer->start(kUpdateMS);

    updateDateText();

    QString changrpname = ChannelGroup::GetChannelGroupName(m_changrpid);

    if (m_changroupname)
        m_changroupname->SetText(changrpname);

    gCoreContext->addListener(this);
}

GuideGrid::~GuideGrid()
{
    m_updateTimer->disconnect(this);
    m_updateTimer = NULL;

    GuideHelper::Wait(this);

    gCoreContext->removeListener(this);

    while (!m_programs.empty())
    {
        if (m_programs.back())
            delete m_programs.back();
        m_programs.pop_back();
    }

    m_channelInfos.clear();

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
        else if (action == ACTION_CHANNELSEARCH)
            ShowChannelSearch();
        else if (action == "MENU")
            ShowMenu();
        else if (action == "ESCAPE" || action == ACTION_GUIDE)
            Close();
        else if (action == ACTION_SELECT)
        {
            ProgramInfo *pginfo =
                m_programInfos[m_currentRow][m_currentCol];
            int secsTillStart =
                (pginfo) ? MythDate::current().secsTo(
                    pginfo->GetScheduledStartTime()) : 0;
            if (m_player && (m_player->GetState(-1) == kState_WatchingLiveTV))
            {
                // See if this show is far enough into the future that it's
                // probable that the user wanted to schedule it to record
                // instead of changing the channel.
                if (pginfo && (pginfo->GetTitle() != kUnknownTitle) &&
                    ((secsTillStart / 60) >= m_selectRecThreshold))
                {
                    EditRecording();
                }
                else
                {
                    enter();
                }
            }
            else
                // Edit Recording should include "Watch this channel"
                // is we selected a show that is current.
                EditRecording(!m_player
                    && SelectionIsTunable(GetSelection())
                    && (secsTillStart / 60) < m_selectRecThreshold);
        }
        else if (action == "EDIT")
            EditScheduled();
        else if (action == "CUSTOMEDIT")
            EditCustom();
        else if (action == "DELETE")
            deleteRule();
        else if (action == "UPCOMING")
            ShowUpcoming();
        else if (action == "PREVRECORDED")
            ShowPrevious();
        else if (action == "DETAILS" || action == "INFO")
            ShowDetails();
        else if (action == ACTION_TOGGLERECORD)
            QuickRecord();
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

bool GuideGrid::gestureEvent(MythGestureEvent *event)
{
    bool handled = true;

    if (!event)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "Guide Gesture no event");
        return false;
    }

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Guide Gesture event %1")
        .arg((QString)event->gesture()));
    switch (event->gesture())
    {
        case MythGestureEvent::Click:
            {
                handled = false;

                // We want the relative position of the click
                QPoint position = event->GetPosition();
                if (m_Parent)
                    position -= m_Parent->GetArea().topLeft();

                MythUIType *type = GetChildAt(position, false, false);

                if (!type)
                    return false;

                MythUIStateType *object = dynamic_cast<MythUIStateType *>(type);

                if (object)
                {
                    QString name = object->objectName();
                    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Guide Gesture Click name %1").arg(name));

                    if (name.startsWith("channellist"))
                    {
                        MythUIButtonList* channelList = dynamic_cast<MythUIButtonList*>(object);

                        if (channelList)
                        {
                            handled = channelList->gestureEvent(event);
                            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Guide Gesture Click channel list %1").arg(handled));
                        }
                    }
                    else if (name.startsWith("guidegrid"))
                    {
                        MythUIGuideGrid* guidegrid = dynamic_cast<MythUIGuideGrid*>(object);

                        if (guidegrid)
                        {
                            handled = true;

                            QPoint rowCol = guidegrid->GetRowAndColumn(position - guidegrid->GetArea().topLeft());

                            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Guide Gesture Click gg %1,%2 (%3,%4)")
                                .arg(rowCol.y())
                                .arg(rowCol.x())
                                .arg(m_currentRow)
                                .arg(m_currentCol)
                                );
                            if ((rowCol.y() >= 0) &&  (rowCol.x() >= 0))
                            {
                                if ((rowCol.y() == m_currentRow) && (rowCol.x() == m_currentCol))
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
                                            //EditRecording();
                                            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Guide Gesture Click gg EditRec"));
                                        }
                                        else
                                        {
                                            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Guide Gesture Click gg enter"));
                                            enter();
                                        }
                                    }
                                    else
                                    {
                                        //EditRecording();
                                        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Guide Gesture Click gg not live"));
                                    }
                                }
                                else
                                {
                                    bool rowChanged = (rowCol.y() != m_currentRow);
                                    bool colChanged = (rowCol.x() != m_currentCol);
                                    if (rowChanged)
                                        setStartChannel(m_currentStartChannel + rowCol.y() - m_currentRow);

                                    m_currentRow = rowCol.y();
                                    m_currentCol = rowCol.x();

                                    fillProgramInfos();
                                    if (colChanged)
                                    {
                                        m_currentStartTime = m_programInfos[m_currentRow][m_currentCol]->GetScheduledStartTime();
                                        fillTimeInfos();
                                    }
                                    if (rowChanged)
                                        updateChannels();
                                    if (colChanged)
                                        updateDateText();
                                }
                            }
                        }
                    }

                }
            }
            break;

        case MythGestureEvent::Up:
            if (m_verticalLayout)
                cursorLeft();
            else
                cursorUp();
            break;

        case MythGestureEvent::Down:
            if (m_verticalLayout)
                cursorRight();
            else
                cursorDown();
            break;

        case MythGestureEvent::Left:
            if (m_verticalLayout)
                cursorUp();
            else
                cursorLeft();
            break;

        case MythGestureEvent::Right:
            if (m_verticalLayout)
                cursorDown();
            else
                cursorRight();
            break;

        case MythGestureEvent::UpThenLeft:
            if (m_verticalLayout)
                moveLeftRight(kPageLeft);
            else
                moveUpDown(kPageUp);
            break;

        case MythGestureEvent::DownThenRight:
            if (m_verticalLayout)
                moveLeftRight(kPageRight);
            else
                moveUpDown(kPageDown);
            break;

        case MythGestureEvent::LeftThenUp:
            if (m_verticalLayout)
                moveUpDown(kPageUp);
            else
                moveLeftRight(kPageLeft);
            break;

        case MythGestureEvent::RightThenDown:
            if (m_verticalLayout)
                moveUpDown(kPageDown);
            else
                moveLeftRight(kPageRight);
            break;

        case MythGestureEvent::LeftThenDown:
            moveLeftRight(kDayLeft);
            break;

        case MythGestureEvent::RightThenUp:
            moveLeftRight(kDayRight);
            break;

        case MythGestureEvent::RightThenLeft:
            enter();
            break;

        default:
            handled = false;
            break;
    }

    if (!handled && MythScreenType::gestureEvent(event))
        handled = true;

    return handled;
}

static bool SelectionIsTunable(const ChannelInfoList &selection)
{
    for (uint i = 0; i < selection.size(); ++i)
    {
        if (TV::IsTunable(selection[i].chanid))
            return true;
    }
    return false;
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
        else if (!m_player && SelectionIsTunable(GetSelection()))
            menuPopup->AddButton(tr("Watch This Channel"));

        menuPopup->AddButton(tr("Record This"));

        menuPopup->AddButton(tr("Recording Options"), NULL, true);

        menuPopup->AddButton(tr("Program Details"));

        menuPopup->AddButton(tr("Jump to Time"), NULL, true);

        menuPopup->AddButton(tr("Reverse Channel Order"));

        menuPopup->AddButton(tr("Channel Search"));

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
        menuPopup->AddButton(tr("Previously Recorded"));
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
        "WHERE program.chanid     = :CHANID       AND "
        "      program.endtime   >= :STARTTS      AND "
        "      program.starttime <= :ENDTS        AND "
        "      program.starttime >= :STARTLIMITTS AND "
        "      program.manualid   = 0 ";
    QDateTime starttime = m_currentStartTime.addSecs(0 - m_currentStartTime.time().second());
    bindings[":STARTTS"] = starttime;
    bindings[":STARTLIMITTS"] = starttime.addDays(-1);
    bindings[":ENDTS"] = m_currentEndTime.addSecs(0 - m_currentEndTime.time().second());
    bindings[":CHANID"]  = chanid;

    ProgramList dummy;
    LoadFromProgram(proglist, querystr, bindings, dummy);

    return proglist;
}

static ProgramList *CopyProglist(ProgramList *proglist)
{
    if (!proglist)
        return NULL;
    ProgramList *result = new ProgramList();
    for (ProgramList::iterator pi = proglist->begin();
         pi != proglist->end(); ++pi)
        result->push_back(new ProgramInfo(**pi));
    return result;
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

        if (!m_player->IsTunable(ctx, ciinfo->chanid))
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
            isAlt &= proglist[j]->IsSameTitleTimeslotAndChannel(*ch_proglist[j]);
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
            isAlt &= proglist[j]->IsSameTitleTimeslotAndChannel(*ch_proglist[j]);
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
    m_updateTimer->start(kUpdateMS);
}

void GuideGrid::fillChannelInfos(bool gotostartchannel)
{
    m_channelInfos.clear();
    m_channelInfoIdx.clear();
    m_currentStartChannel = 0;

    uint avail = 0;
    ChannelInfoList channels = ChannelUtil::LoadChannels(0, 0, avail, true,
                                         ChannelUtil::kChanOrderByChanNum,
                                         ChannelUtil::kChanGroupByChanid,
                                         0,
                                         (m_changrpid < 0) ? 0 : m_changrpid);

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
        int ch = FindChannel(m_startChanID, m_startChanNum, false);
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

    ChannelInfoList list;
    QVector<int> idxList;
    for (i = 0; i < GetChannelCount(); ++i)
    {
        for (uint j = 0; j < m_channelInfos[i].size(); ++j)
        {
            list.push_back(m_channelInfos[i][j]);
            idxList.push_back(i);
        }
    }
    int result = ChannelUtil::GetNearestChannel(list, channum);
    if (result >= 0)
        result = idxList[result];
    return result;
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
    fillProgramRowInfos(-1, useExistingData);
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
                           "  AND program.starttime >= :STARTLIMITTS "
                           "  AND program.manualid = 0 ";
        QDateTime starttime = m_currentStartTime.addSecs(0 - m_currentStartTime.time().second());
        bindings[":CHANID"]  = GetChannelInfo(chanNum)->chanid;
        bindings[":STARTTS"] = starttime;
        bindings[":STARTLIMITTS"] = starttime.addDays(-1);
        bindings[":ENDTS"] = m_currentEndTime.addSecs(0 - m_currentEndTime.time().second());

        LoadFromProgram(*proglist, querystr, bindings, m_recList);
    }

    return proglist;
}

void GuideGrid::fillProgramRowInfos(int firstRow, bool useExistingData)
{
    bool allRows = false;
    unsigned int numRows = 1;
    if (firstRow < 0)
    {
        firstRow = 0;
        allRows = true;
        numRows = min((unsigned int)m_channelInfos.size(),
                      (unsigned int)m_guideGrid->getChannelCount());
    }
    QVector<int> chanNums;
    QVector<ProgramList*> proglists;

    for (unsigned int i = 0; i < numRows; ++i)
    {
        unsigned int row = i + firstRow;
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

        ProgramList *proglist = NULL;
        if (useExistingData)
            proglist = CopyProglist(m_programs[row]);
        chanNums.push_back(chanNum);
        proglists.push_back(proglist);
    }
    if (allRows)
    {
        for (unsigned int i = numRows;
             i < (unsigned int) m_guideGrid->getChannelCount(); ++i)
        {
            delete m_programs[i];
            m_programs[i] = NULL;
            m_guideGrid->ResetRow(i);
        }
    }

    m_channelList->SetItemCurrent(m_currentRow);

    GuideStatus gs(firstRow, chanNums.size(), chanNums,
                   m_guideGrid->GetArea(), m_guideGrid->getChannelCount(),
                   m_currentStartTime, m_currentEndTime, m_currentStartChannel,
                   m_currentRow, m_currentCol, m_channelCount, m_timeCount,
                   m_verticalLayout, m_firstTime, m_lastTime);
    GuideUpdateProgramRow *updater =
        new GuideUpdateProgramRow(this, gs, proglists);
    m_threadPool.start(new GuideHelper(this, updater), "GuideHelper");
}

void GuideUpdateProgramRow::fillProgramRowInfosWith(int row,
                                                    QDateTime start,
                                                    ProgramList *proglist)
{
    if (row < 0 || row >= m_channelCount ||
        start != m_currentStartTime)
    {
        delete proglist;
        return;
    }

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

    m_progPast = progPast;

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
                if (proginfo)
                {
                    proginfo->spread++;
                    proginfo->SetScheduledEndTime(proginfo->GetScheduledEndTime().addSecs(5 * 60));
                }
            }
            else
            {
                proginfo = new ProgramInfo(kUnknownTitle,
                                           GuideGrid::tr("Unknown", "Unknown program title"),
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

    MythRect programRect = m_gg_programRect;

    /// use doubles to avoid large gaps at end..
    double ydifference = 0.0, xdifference = 0.0;

    if (m_verticalLayout)
    {
        ydifference = programRect.width() /
            (double) m_gg_channelCount;
        xdifference = programRect.height() /
            (double) m_timeCount;
    }
    else
    {
        ydifference = programRect.height() /
            (double) m_gg_channelCount;
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
            if (pginfo->GetRecordingStatus() == RecStatus::Conflict ||
                pginfo->GetRecordingStatus() == RecStatus::Offline)
                recStat = 2;
            else if (pginfo->GetRecordingStatus() <= RecStatus::WillRecord)
                recStat = 1;
            else
                recStat = 0;

            QString title = (pginfo->GetTitle() == kUnknownTitle) ?
                GuideGrid::tr("Unknown", "Unknown program title") :
                                pginfo->GetTitle();
            m_result.push_back(GuideUIElement(
                row, cnt, tempRect, title,
                pginfo->GetCategory(), arrow, recFlag,
                recStat, isCurrent));

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
            GuideHelper::Wait(this);
            LoadFromScheduler(m_recList);
            fillProgramInfos();
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
                dce->GetData().value<RecordingRule *>();
            if (record)
            {
                if ((buttonnum > 0) && !record->Delete())
                    LOG(VB_GENERAL, LOG_ERR, "Failed to delete recording rule");
                delete record;
            }
        }
        // Test for this here because it can come from 
        // different menus.
        else if (resulttext == tr("Watch This Channel"))
        {
            ChannelInfoList selection = GetSelection();
            if (SelectionIsTunable(selection))
                TV::StartTV(NULL, kStartTVNoFlags, selection);
        }
        else if (resultid == "guidemenu")
        {
            if (resulttext == tr("Record This"))
            {
                QuickRecord();
            }
            else if (resulttext == tr("Change to Channel"))
            {
                enter();
            }
            else if (resulttext == tr("Program Details"))
            {
                ShowDetails();
            }
            else if (resulttext == tr("Reverse Channel Order"))
            {
                m_sortReverse = !m_sortReverse;
                generateListings();
                updateChannels();
            }
            else if (resulttext == tr("Channel Search"))
            {
                ShowChannelSearch();
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
                EditRecording();
            }
            else if (resulttext == tr("Edit Schedule"))
            {
                EditScheduled();
            }
            else if (resulttext == tr("Show Upcoming"))
            {
                ShowUpcoming();
            }
            else if (resulttext == tr("Previously Recorded"))
            {
                ShowPrevious();
            }
            else if (resulttext == tr("Custom Edit"))
            {
                EditCustom();
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
    else if (event->type() == UpdateGuideEvent::kEventType)
    {
        UpdateGuideEvent *uge = static_cast<UpdateGuideEvent*>(event);
        if (uge->m_updater)
        {
            uge->m_updater->ExecuteUI();
            delete uge->m_updater;
            uge->m_updater = NULL;
        }
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

void GuideGrid::updateProgramsUI(unsigned int firstRow, unsigned int numRows,
                                 int progPast,
                                 const QVector<ProgramList*> &proglists,
                                 const ProgInfoGuideArray &programInfos,
                                 const QLinkedList<GuideUIElement> &elements)
{
    for (unsigned int i = 0; i < numRows; ++i)
    {
        unsigned int row = i + firstRow;
        m_guideGrid->ResetRow(row);
        if (m_programs[row] != proglists[i])
        {
            delete m_programs[row];
            m_programs[row] = proglists[i];
        }
    }
    m_guideGrid->SetProgPast(progPast);
    for (QLinkedList<GuideUIElement>::const_iterator it = elements.begin();
         it != elements.end(); ++it)
    {
        const GuideUIElement &r = *it;
        m_guideGrid->SetProgramInfo(r.m_row, r.m_col, r.m_area, r.m_title,
                                    r.m_category, r.m_arrow, r.m_recType,
                                    r.m_recStat, r.m_selected);
    }
    for (unsigned int i = firstRow; i < firstRow + numRows; ++i)
    {
        for (int j = 0; j < MAX_DISPLAY_TIMES; ++j)
            m_programInfos[i][j] = programInfos[i][j];
        if (i == (unsigned int)m_currentRow)
            updateInfo();
    }
    m_guideGrid->SetRedraw();
}

void GuideGrid::updateChannels(void)
{
    GuideUpdateChannels *updater =
        new GuideUpdateChannels(this, m_currentStartChannel);
    m_threadPool.start(new GuideHelper(this, updater), "GuideHelper");
}

void GuideGrid::updateChannelsNonUI(QVector<ChannelInfo *> &chinfos,
                                    QVector<bool> &unavailables)
{
    ChannelInfo *chinfo = GetChannelInfo(m_currentStartChannel);

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
                try_alt = !m_player->IsTunable(ctx, chinfo->chanid);
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
        chinfos.push_back(chinfo);
        unavailables.push_back(unavailable);
    }
}

void GuideGrid::updateChannelsUI(const QVector<ChannelInfo *> &chinfos,
                                 const QVector<bool> &unavailables)
{
    m_channelList->Reset();
    for (int i = 0; i < chinfos.size(); ++i)
    {
        ChannelInfo *chinfo = chinfos[i];
        bool unavailable = unavailables[i];
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
    m_channelList->SetItemCurrent(m_currentRow);
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
    // HACK - This should be done in ProgramInfo, but that needs more careful
    // review since it may have unintended consequences so we're doing it here
    // for now
    if (infoMap["title"] == kUnknownTitle)
    {
        infoMap["title"] = tr("Unknown", "Unknown program title");
        infoMap["titlesubtitle"] = tr("Unknown", "Unknown program title");
    }

    SetTextFromMap(infoMap);

    MythUIStateType *ratingState = dynamic_cast<MythUIStateType*>
                                                (GetChild("ratingstate"));
    if (ratingState)
    {
        QString rating = QString::number(pginfo->GetStars(10));
        ratingState->DisplayState(rating);
    }
    m_guideGrid->SetRedraw();
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
        fillProgramRowInfos(m_currentRow, true);
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
        fillProgramRowInfos(m_currentRow, true);
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
        fillProgramRowInfos(m_currentRow, true);
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
        fillProgramRowInfos(m_currentRow, true);
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
    updateChannels();
}

void GuideGrid::moveToTime(QDateTime datetime)
{
    if (!datetime.isValid())
        return;

    m_currentStartTime = datetime;

    fillTimeInfos();
    fillProgramInfos();
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

    m_updateTimer->stop();

    // don't fade the screen if we are returning to the player
    if (m_player)
        GetScreenStack()->PopScreen(this, false);
    else
        GetScreenStack()->PopScreen(this, true);

    epgIsVisibleCond.wakeAll();
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
