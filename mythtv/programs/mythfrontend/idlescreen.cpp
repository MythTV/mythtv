
#include "idlescreen.h"

#include <QTimer>

#include <mythcontext.h>
#include <mythsystemlegacy.h>

#include <mythuibuttonlist.h>
#include <mythuistatetype.h>
#include <mythmainwindow.h>

#include <programinfo.h>

#include <tvremoteutil.h>

#define UPDATE_STATUS_INTERVAL   30000

IdleScreen::IdleScreen(MythScreenStack *parent)
              :MythScreenType(parent, "standbymode"),
              m_updateStatusTimer(new QTimer(this)), m_statusState(NULL),
              m_scheduledText(NULL), m_warningText(NULL),
              m_secondsToShutdown(0), m_backendRecording(false),
              m_pendingSchedUpdate(false), m_hasConflicts(false)

{
    gCoreContext->addListener(this);
    GetMythMainWindow()->EnterStandby();

    connect(m_updateStatusTimer, SIGNAL(timeout()),
            this, SLOT(UpdateStatus()));
    m_updateStatusTimer->start(UPDATE_STATUS_INTERVAL);
}

IdleScreen::~IdleScreen()
{
    GetMythMainWindow()->ExitStandby();
    gCoreContext->removeListener(this);

    if (m_updateStatusTimer)
        m_updateStatusTimer->disconnect();
}

bool IdleScreen::Create(void)
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("status-ui.xml", "standbymode", this);

    if (!foundtheme)
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_statusState, "backendstatus", &err);

    /* nextrecording and conflicts are optional */
    UIUtilE::Assign(this, m_scheduledText, "nextrecording");
    UIUtilE::Assign(this, m_warningText, "conflicts");

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'standbymode'");
        return false;
    }

    if (m_warningText)
        m_warningText->SetVisible(false);

    UpdateScheduledList();

    return true;
}

void IdleScreen::Load(void)
{
    MythScreenType::Load();
}

void IdleScreen::Init(void)
{
    UpdateScreen();
}

bool IdleScreen::CheckConnectionToServer(void)
{
    m_updateStatusTimer->stop();

    bool bRes = false;

    if (gCoreContext->IsConnectedToMaster())
        bRes = true;
    else
    {
        if (gCoreContext->SafeConnectToMasterServer(false))
            bRes = true;
    }

    if (bRes)
        m_updateStatusTimer->start(UPDATE_STATUS_INTERVAL);
    else
        m_updateStatusTimer->start(5000);

    return bRes;
}

void IdleScreen::UpdateStatus(void)
{
    QString state = "idle";

    if (CheckConnectionToServer())
    {
        if (m_secondsToShutdown)
            state = "shuttingdown";
        else if (RemoteGetRecordingStatus())
            state = "recording";
    }
    else
    {
        state = "offline";
    }

    m_statusState->DisplayState(state);

    MythUIType* shuttingdown = m_statusState->GetState("shuttingdown");

    if (shuttingdown)
    {
        MythUIText *statusText = dynamic_cast<MythUIText *>(shuttingdown->GetChild("status"));

        if (statusText)
        {
            if (m_secondsToShutdown)
            {
                QString status = tr("Backend will shutdown in %n "
                                    "second(s).", "", m_secondsToShutdown);

                statusText->SetText(status);
            }
            else
                statusText->Reset();
        }
    }

    if (m_warningText)
        m_warningText->SetVisible(m_hasConflicts);
}

void IdleScreen::UpdateScreen(void)
{
    QString scheduled;

    // update scheduled
    if (!m_scheduledList.empty())
    {
        ProgramInfo progInfo = m_scheduledList[0];

        InfoMap infomap;
        progInfo.ToMap(infomap);

        scheduled = infomap["channame"] + "\n";
        scheduled += infomap["title"];
        if (!infomap["subtitle"].isEmpty())
            scheduled += "\n(" + infomap["subtitle"] + ")";

        scheduled += "\n" + infomap["timedate"];
    }
    else
        scheduled = tr("There are no scheduled recordings");

    if (m_scheduledText)
        m_scheduledText->SetText(scheduled);

    UpdateStatus();
}

bool IdleScreen::UpdateScheduledList()
{
    {
        // clear pending flag early in case something happens while
        // we're updating
        QMutexLocker lock(&m_schedUpdateMutex);
        SetPendingSchedUpdate(false);
    }

    m_scheduledList.clear();
    m_hasConflicts = false;

    if (!gCoreContext->IsConnectedToMaster())
    {
        return false;
    }

    GetNextRecordingList(m_nextRecordingStart, &m_hasConflicts,
                         &m_scheduledList);

    UpdateScreen();

    return true;
}

bool IdleScreen::keyPressEvent(QKeyEvent* event)
{
    return MythScreenType::keyPressEvent(event);
}

void IdleScreen::customEvent(QEvent* event)
{
    if ((MythEvent::Type)(event->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = static_cast<MythEvent *>(event);

        if (me->Message().startsWith("SHUTDOWN_COUNTDOWN"))
        {
            QString secs = me->Message().mid(19);
            m_secondsToShutdown = secs.toInt();
            UpdateStatus();
        }
        else if (me->Message().startsWith("SHUTDOWN_NOW"))
        {
            if (gCoreContext->IsFrontendOnly())
            {
                // does the user want to shutdown this frontend only machine
                // when the BE shuts down?
                if (gCoreContext->GetNumSetting("ShutdownWithMasterBE", 0) == 1)
                {
                     LOG(VB_GENERAL, LOG_NOTICE,
                         "Backend has gone offline, Shutting down frontend");
                     QString poweroff_cmd =
                        gCoreContext->GetSetting("MythShutdownPowerOff", "");
                     if (!poweroff_cmd.isEmpty())
                         myth_system(poweroff_cmd);
                }
            }
        }
        else if (me->Message().startsWith("SCHEDULE_CHANGE"))
        {
            QMutexLocker lock(&m_schedUpdateMutex);

            if (!PendingSchedUpdate())
            {
                QTimer::singleShot(500, this, SLOT(UpdateScheduledList()));
                SetPendingSchedUpdate(true);
            }
        }
    }

    MythUIType::customEvent(event);
}

