
#include "idlescreen.h"

#include <QTimer>

#include <mythcontext.h>
#include <mythsystem.h>

#include <mythuibuttonlist.h>
#include <mythuistatetype.h>
#include <mythmainwindow.h>

#include <programinfo.h>

#include <tvremoteutil.h>

#define UPDATE_STATUS_INTERVAL   30000

IdleScreen::IdleScreen(MythScreenStack *parent)
              :MythScreenType(parent, "standbymode"),
              m_updateStatusTimer(new QTimer(this)), m_statusState(NULL),
              m_secondsToShutdown(0), m_backendRecording(false)
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

    m_statusState = dynamic_cast<MythUIStateType*>
                                            (GetChild("backendstatus"));

    if (!m_statusState)
        return false;

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
}

void IdleScreen::UpdateScreen(void)
{

    MythUIText *statusText = dynamic_cast<MythUIText *>(GetChild("status"));

    if (statusText)
    {
        QString status;

        if (m_secondsToShutdown)
            status = tr("MythTV is idle and will shutdown in %n "
                        "second(s).", "", m_secondsToShutdown);

        if (!status.isEmpty())
            statusText->SetText(status);
        else
            statusText->Reset();
    }

    UpdateStatus();
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

        if (me->Message().left(18) == "SHUTDOWN_COUNTDOWN")
        {
            QString secs = me->Message().mid(19);
            m_secondsToShutdown = secs.toInt();
            UpdateStatus();
        }
        else if (me->Message().left(12) == "SHUTDOWN_NOW")
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
    }

    MythUIType::customEvent(event);
}

