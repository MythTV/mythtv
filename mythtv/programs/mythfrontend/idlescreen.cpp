
#include "idlescreen.h"

#include <QTimer>

#include <mythcontext.h>
#include <mythsystemlegacy.h>

#include <mythuibuttonlist.h>
#include <mythuistatetype.h>
#include <mythmainwindow.h>

#include <programinfo.h>

#include <tvremoteutil.h>

#define UPDATE_INTERVAL   15000

IdleScreen::IdleScreen(MythScreenStack *parent)
              :MythScreenType(parent, "standbymode"),
              m_updateScreenTimer(new QTimer(this)), m_statusState(NULL),
              m_currentRecordings(NULL),
              m_nextRecordings(NULL),
              m_conflictingRecordings(NULL),
              m_conflictWarning(NULL),
              m_secondsToShutdown(-1),
              m_pendingSchedUpdate(false),
              m_hasConflicts(false)
{
    gCoreContext->addListener(this);
    GetMythMainWindow()->EnterStandby();

    connect(m_updateScreenTimer, SIGNAL(timeout()),
            this, SLOT(UpdateScreen()));
    m_updateScreenTimer->start(UPDATE_INTERVAL);
}

IdleScreen::~IdleScreen()
{
    GetMythMainWindow()->ExitStandby();
    gCoreContext->removeListener(this);

    if (m_updateScreenTimer)
        m_updateScreenTimer->disconnect();
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

    /* currentrecording, nextrecording, conflicts and conflictwarning are optional */
    UIUtilW::Assign(this, m_currentRecordings, "currentrecording");
    UIUtilW::Assign(this, m_nextRecordings, "nextrecording");
    UIUtilW::Assign(this, m_conflictingRecordings, "conflicts");
    UIUtilW::Assign(this, m_conflictWarning, "conflictwarning");

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'standbymode'");
        return false;
    }

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
    m_updateScreenTimer->stop();

    bool bRes = false;

    if (gCoreContext->IsConnectedToMaster())
        bRes = true;
    else
    {
        if (gCoreContext->SafeConnectToMasterServer(false))
            bRes = true;
    }

    if (bRes)
        m_updateScreenTimer->start(UPDATE_INTERVAL);
    else
        m_updateScreenTimer->start(5000);

    return bRes;
}

void IdleScreen::UpdateStatus(void)
{
    QString state = "idle";

    if (CheckConnectionToServer())
    {
        if (m_secondsToShutdown >= 0)
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
            if (m_secondsToShutdown >= 0)
            {
                QString status = tr("Backend will shutdown in %n "
                                    "second(s).", "", m_secondsToShutdown);

                statusText->SetText(status);
            }
            else
                statusText->Reset();
        }
    }
}

void IdleScreen::UpdateScreen(void)
{
    if (m_currentRecordings)
    {
        m_currentRecordings->Reset();
        m_currentRecordings->SetCanTakeFocus(false);
    }

    if (m_nextRecordings)
    {
        m_nextRecordings->Reset();
        m_nextRecordings->SetCanTakeFocus(false);
    }

    if (m_conflictingRecordings)
    {
        m_conflictingRecordings->Reset();
        m_conflictingRecordings->SetCanTakeFocus(false);
    }

    if (m_conflictWarning)
        m_conflictWarning->SetVisible(m_hasConflicts);

    // update scheduled
    if (!m_scheduledList.empty())
    {
        ProgramList::iterator pit = m_scheduledList.begin();
        MythUIButtonListItem *item;

        while (pit != m_scheduledList.end())
        {
            ProgramInfo *progInfo = *pit;
            if (progInfo)
            {
                MythUIButtonList *list = NULL;
                const RecStatusType recstatus = progInfo->GetRecordingStatus();

                switch(recstatus)
                {
                    case rsRecording:
                    case rsTuning:
                        list = m_currentRecordings;
                        break;

                    case rsWillRecord:
                        list = m_nextRecordings;
                        break;

                    case rsConflict:
                        list = m_conflictingRecordings;
                        break;

                    default:
                        list = NULL;
                        break;
                }

                if (list != NULL)
                {
                    item = new MythUIButtonListItem(list,"",
                                                    qVariantFromValue(progInfo));

                    InfoMap infoMap;
                    progInfo->ToMap(infoMap);
                    item->SetTextFromMap(infoMap, "");
                }
            }
            ++pit;
        }
    }

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

    if (!gCoreContext->IsConnectedToMaster())
    {
        return false;
    }

    if (!LoadFromScheduler(m_scheduledList, m_hasConflicts))
        return false;

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

        if (me->Message().startsWith("RECONNECT_"))
        {
            m_secondsToShutdown = -1;
            UpdateStatus();
        }
        else if (me->Message().startsWith("SHUTDOWN_COUNTDOWN"))
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
        else if (me->Message().startsWith("SCHEDULE_CHANGE") ||
                 me->Message().startsWith("RECORDING_LIST_CHANGE") ||
                 me->Message() == "UPDATE_PROG_INFO")
        {
            QMutexLocker lock(&m_schedUpdateMutex);

            if (!PendingSchedUpdate())
            {
                QTimer::singleShot(50, this, SLOT(UpdateScheduledList()));
                SetPendingSchedUpdate(true);
            }
        }
    }

    MythUIType::customEvent(event);
}
