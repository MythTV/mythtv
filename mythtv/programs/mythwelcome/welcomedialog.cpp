// ANSI C
#include <cstdlib>

// POSIX
#include <unistd.h>

// qt
#include <QCoreApplication>
#include <QKeyEvent>
#include <QEvent>

// myth
#include "exitcodes.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "lcddevice.h"
#include "tv.h"
#include "compat.h"
#include "mythdirs.h"
#include "remoteutil.h"
#include "mythsystemlegacy.h"

#include "welcomedialog.h"
#include "welcomesettings.h"

#define UPDATE_STATUS_INTERVAL   30000
#define UPDATE_SCREEN_INTERVAL   15000


WelcomeDialog::WelcomeDialog(MythScreenStack *parent, const char *name)
              :MythScreenType(parent, name),
    m_status_text(NULL),        m_recording_text(NULL), m_scheduled_text(NULL),
    m_warning_text(NULL),       m_startfrontend_button(NULL),
    m_menuPopup(NULL),          m_updateStatusTimer(new QTimer(this)),
    m_updateScreenTimer(new QTimer(this)),              m_isRecording(false),
    m_hasConflicts(false),      m_bWillShutdown(false),
    m_secondsToShutdown(-1),    m_preRollSeconds(0),    m_idleWaitForRecordingTime(0),
    m_idleTimeoutSecs(0),       m_screenTunerNo(0),     m_screenScheduledNo(0),
    m_statusListNo(0),          m_frontendIsRunning(false),
    m_pendingRecListUpdate(false), m_pendingSchedUpdate(false)
{
    gCoreContext->addListener(this);

    m_appBinDir = GetAppBinDir();
    m_preRollSeconds = gCoreContext->GetNumSetting("RecordPreRoll");
    m_idleWaitForRecordingTime =
                       gCoreContext->GetNumSetting("idleWaitForRecordingTime", 15);

    m_timeFormat = gCoreContext->GetSetting("TimeFormat", "h:mm AP");
    m_dateFormat = gCoreContext->GetSetting("MythWelcomeDateFormat", "dddd\\ndd MMM yyyy");
    m_dateFormat.replace("\\n", "\n");

    // if idleTimeoutSecs is 0, the user disabled the auto-shutdown feature
    m_bWillShutdown = (gCoreContext->GetNumSetting("idleTimeoutSecs", 0) != 0);

    m_idleTimeoutSecs = gCoreContext->GetNumSetting("idleTimeoutSecs", 0);

    connect(m_updateStatusTimer, SIGNAL(timeout()),
            this, SLOT(updateStatus()));
    m_updateStatusTimer->start(UPDATE_STATUS_INTERVAL);

    connect(m_updateScreenTimer, SIGNAL(timeout()),
            this, SLOT(updateScreen()));
}

bool WelcomeDialog::Create(void)
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("welcome-ui.xml", "welcome_screen", this);

    if (!foundtheme)
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_status_text, "status_text", &err);
    UIUtilE::Assign(this, m_recording_text, "recording_text", &err);
    UIUtilE::Assign(this, m_scheduled_text, "scheduled_text", &err);
    UIUtilE::Assign(this, m_warning_text, "conflicts_text", &err);
    UIUtilE::Assign(this, m_startfrontend_button, "startfrontend_button", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'welcome_screen'");
        return false;
    }

    m_warning_text->SetVisible(false);

    m_startfrontend_button->SetText(tr("Start Frontend"));
    connect(m_startfrontend_button, SIGNAL(Clicked()),
            this, SLOT(startFrontendClick()));

    BuildFocusList();

    SetFocusWidget(m_startfrontend_button);

    checkConnectionToServer();
    checkAutoStart();

    return true;
}

void WelcomeDialog::startFrontend(void)
{
    QString startFECmd = gCoreContext->GetSetting("MythWelcomeStartFECmd",
                         m_appBinDir + "mythfrontend");

    myth_system(startFECmd, kMSDisableUDPListener);
    updateAll();
    m_frontendIsRunning = false;
}

void WelcomeDialog::startFrontendClick(void)
{
    if (m_frontendIsRunning)
        return;

    m_frontendIsRunning = true;

    // this makes sure the button appears to click properly
    QTimer::singleShot(500, this, SLOT(startFrontend()));
}

void WelcomeDialog::checkAutoStart(void)
{
    // mythshutdown --startup returns 0 for automatic startup
    //                                1 for manual startup
    QString command = m_appBinDir + "mythshutdown --startup";
    command += logPropagateArgs;
    uint state = myth_system(command);

    LOG(VB_GENERAL, LOG_NOTICE,
        QString("mythshutdown --startup returned: %1").arg(state));

    bool bAutoStartFrontend = gCoreContext->GetNumSetting("AutoStartFrontend", 1);

    if (state == 1 && bAutoStartFrontend)
        startFrontendClick();

    // update status now
    updateAll();
}

void WelcomeDialog::customEvent(QEvent *e)
{
    if ((MythEvent::Type)(e->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *) e;

        if (me->Message().startsWith("RECORDING_LIST_CHANGE") ||
            me->Message() == "UPDATE_PROG_INFO")
        {
            LOG(VB_GENERAL, LOG_NOTICE,
                "MythWelcome received a recording list change event");

            QMutexLocker lock(&m_RecListUpdateMuxtex);

            if (pendingRecListUpdate())
            {
                LOG(VB_GENERAL, LOG_NOTICE,
                    "            [deferred to pending handler]");
            }
            else
            {
                // we can't query the backend from inside a customEvent
                QTimer::singleShot(500, this, SLOT(updateRecordingList()));
                setPendingRecListUpdate(true);
            }
        }
        else if (me->Message().startsWith("SCHEDULE_CHANGE"))
        {
            LOG(VB_GENERAL, LOG_NOTICE,
                "MythWelcome received a SCHEDULE_CHANGE event");

            QMutexLocker lock(&m_SchedUpdateMuxtex);

            if (pendingSchedUpdate())
            {
                LOG(VB_GENERAL, LOG_NOTICE,
                    "            [deferred to pending handler]");
            }
            else
            {
                QTimer::singleShot(500, this, SLOT(updateScheduledList()));
                setPendingSchedUpdate(true);
            }
        }
        else if (me->Message().startsWith("SHUTDOWN_COUNTDOWN"))
        {
#if 0
            LOG(VB_GENERAL, LOG_NOTICE,
                "MythWelcome received a SHUTDOWN_COUNTDOWN event");
#endif
            QString secs = me->Message().mid(19);
            m_secondsToShutdown = secs.toInt();
            updateStatusMessage();
            updateScreen();
        }
        else if (me->Message().startsWith("SHUTDOWN_NOW"))
        {
            LOG(VB_GENERAL, LOG_NOTICE,
                "MythWelcome received a SHUTDOWN_NOW event");
            if (gCoreContext->IsFrontendOnly())
            {
                // does the user want to shutdown this frontend only machine
                // when the BE shuts down?
                if (gCoreContext->GetNumSetting("ShutdownWithMasterBE", 0) == 1)
                {
                     LOG(VB_GENERAL, LOG_NOTICE,
                         "MythWelcome is shutting this computer down now");
                     QString poweroff_cmd = gCoreContext->GetSetting("MythShutdownPowerOff", "");
                     if (!poweroff_cmd.isEmpty())
                         myth_system(poweroff_cmd);
                }
            }
        }
    }
}

bool WelcomeDialog::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Welcome", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "ESCAPE")
        {
            return true; // eat escape key
        }
        else if (action == "MENU")
        {
            showMenu();
        }
        else if (action == "NEXTVIEW")
        {
            Close();
        }
        else if (action == "INFO")
        {
            MythWelcomeSettings settings;
            if (kDialogCodeAccepted == settings.exec())
            {
                gCoreContext->SendMessage("CLEAR_SETTINGS_CACHE");
                updateStatus();
                updateScreen();

                m_dateFormat = gCoreContext->GetSetting("MythWelcomeDateFormat", "dddd\\ndd MMM yyyy");
                m_dateFormat.replace("\\n", "\n");
            }
        }
        else if (action == "SHOWSETTINGS")
        {
            MythShutdownSettings settings;
            if (kDialogCodeAccepted == settings.exec())
                gCoreContext->SendMessage("CLEAR_SETTINGS_CACHE");
        }
        else if (action == "0")
        {
            QString mythshutdown_status =
                m_appBinDir + "mythshutdown --status 0";
            QString mythshutdown_unlock =
                m_appBinDir + "mythshutdown --unlock";
            QString mythshutdown_lock =
                m_appBinDir + "mythshutdown --lock";

            uint statusCode;
            statusCode = myth_system(mythshutdown_status + logPropagateArgs);

            // is shutdown locked by a user
            if (!(statusCode & 0xFF00) && statusCode & 16)
            {
                myth_system(mythshutdown_unlock + logPropagateArgs);
            }
            else
            {
                myth_system(mythshutdown_lock + logPropagateArgs);
            }

            updateStatusMessage();
            updateScreen();
        }
        else if (action == "STARTXTERM")
        {
            QString cmd = gCoreContext->GetSetting("MythShutdownXTermCmd", "");
            if (!cmd.isEmpty())
                myth_system(cmd);
        }
        else if (action == "STARTSETUP")
        {
            QString mythtv_setup = m_appBinDir + "mythtv-setup";
            myth_system(mythtv_setup + logPropagateArgs);
        }
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void WelcomeDialog::closeDialog()
{
    Close();
}

WelcomeDialog::~WelcomeDialog()
{
    gCoreContext->removeListener(this);

    if (m_updateStatusTimer)
        m_updateStatusTimer->disconnect();

    if (m_updateScreenTimer)
        m_updateScreenTimer->disconnect();
}

void WelcomeDialog::updateStatus(void)
{
    checkConnectionToServer();

    updateStatusMessage();
}

void WelcomeDialog::updateScreen(void)
{
    QString status;

    if (!gCoreContext->IsConnectedToMaster())
    {
        m_recording_text->SetText(tr("Cannot connect to server!"));
        m_scheduled_text->SetText(tr("Cannot connect to server!"));
        m_warning_text->SetVisible(false);
    }
    else
    {
        // update recording
        if (m_isRecording && !m_tunerList.empty())
        {
            if (m_screenTunerNo >= m_tunerList.size())
                m_screenTunerNo = 0;

            TunerStatus tuner = m_tunerList[m_screenTunerNo];

            if (tuner.isRecording)
            {
                status = tr("Tuner %1 is recording:").arg(tuner.id);
                status += "\n";
                status += tuner.channame;
                status += "\n" + tuner.title;
                if (!tuner.subtitle.isEmpty())
                    status += "\n("+tuner.subtitle+")";
                status += "\n" +
                    tr("%1 to %2", "Time period, 'starttime to endtime'")
                        .arg(MythDate::toString(tuner.startTime, MythDate::kTime))
                        .arg(MythDate::toString(tuner.endTime, MythDate::kTime));
            }
            else
            {
                status = tr("Tuner %1 is not recording").arg(tuner.id);
            }

            if (m_screenTunerNo < m_tunerList.size() - 1)
                m_screenTunerNo++;
            else
                m_screenTunerNo = 0;
        }
        else
            status = tr("There are no recordings currently taking place");

        status.detach();

        m_recording_text->SetText(status);

        // update scheduled
        if (!m_scheduledList.empty())
        {
            if (m_screenScheduledNo >= m_scheduledList.size())
                m_screenScheduledNo = 0;

            ProgramInfo progInfo = m_scheduledList[m_screenScheduledNo];

            InfoMap infomap;
            progInfo.ToMap(infomap);

            //status = QString("%1 of %2\n").arg(m_screenScheduledNo + 1)
            //                               .arg(m_scheduledList.size());
            status = infomap["channame"] + "\n";
            status += infomap["title"];
            if (!infomap["subtitle"].isEmpty())
                status += "\n(" + infomap["subtitle"] + ")";

            status += "\n" + infomap["timedate"];

            if (m_screenScheduledNo < m_scheduledList.size() - 1)
                m_screenScheduledNo++;
            else
                m_screenScheduledNo = 0;
        }
        else
            status = tr("There are no scheduled recordings");

        m_scheduled_text->SetText(status);
    }

    // update status message
    if (m_statusList.empty())
        status = tr("Please Wait...");
    else
    {
        if ((int)m_statusListNo >= m_statusList.count())
            m_statusListNo = 0;

        status = m_statusList[m_statusListNo];
        if (m_statusList.count() > 1)
            status += "...";
        m_status_text->SetText(status);

        if ((int)m_statusListNo < m_statusList.count() - 1)
            m_statusListNo++;
        else
            m_statusListNo = 0;
    }

    m_updateScreenTimer->stop();
    m_updateScreenTimer->setSingleShot(true);
    m_updateScreenTimer->start(UPDATE_SCREEN_INTERVAL);
}

// taken from housekeeper.cpp
void WelcomeDialog::runMythFillDatabase()
{
    QString command;

    QString mfpath = gCoreContext->GetSetting("MythFillDatabasePath",
                                          "mythfilldatabase");
    QString mfarg = gCoreContext->GetSetting("MythFillDatabaseArgs", "");

    command = QString("%1 %2").arg(mfpath).arg(mfarg);
    command += logPropagateArgs;

    command += "&";

    LOG(VB_GENERAL, LOG_INFO, QString("Grabbing EPG data using command: %1\n")
            .arg(command));

    myth_system(command);
}

void WelcomeDialog::updateAll(void)
{
    updateRecordingList();
    updateScheduledList();
}

bool WelcomeDialog::updateRecordingList()
{
    {
        // clear pending flag early in case something happens while
        // we're updating
        QMutexLocker lock(&m_RecListUpdateMuxtex);
        setPendingRecListUpdate(false);
    }

    m_tunerList.clear();
    m_isRecording = false;
    m_screenTunerNo = 0;

    if (!gCoreContext->IsConnectedToMaster())
        return false;

    m_isRecording = RemoteGetRecordingStatus(&m_tunerList, true);

    return true;
}

bool WelcomeDialog::updateScheduledList()
{
    {
        // clear pending flag early in case something happens while
        // we're updating
        QMutexLocker lock(&m_SchedUpdateMuxtex);
        setPendingSchedUpdate(false);
    }

    m_scheduledList.clear();
    m_screenScheduledNo = 0;

    if (!gCoreContext->IsConnectedToMaster())
    {
        updateStatusMessage();
        return false;
    }

    GetNextRecordingList(m_nextRecordingStart, &m_hasConflicts,
                         &m_scheduledList);

    updateStatus();
    updateScreen();

    return true;
}

void WelcomeDialog::updateStatusMessage(void)
{
    m_statusList.clear();

    QDateTime curtime = MythDate::current();

    if (!m_isRecording && !m_nextRecordingStart.isNull() &&
        curtime.secsTo(m_nextRecordingStart) - m_preRollSeconds <
        (m_idleWaitForRecordingTime * 60) + m_idleTimeoutSecs)
    {
         m_statusList.append(tr("MythTV is about to start recording."));
    }

    if (m_isRecording)
    {
        m_statusList.append(tr("MythTV is busy recording."));
    }

    QString mythshutdown_status = m_appBinDir + "mythshutdown --status 0";
    uint statusCode = myth_system(mythshutdown_status + logPropagateArgs);

    if (!(statusCode & 0xFF00))
    {
        if (statusCode & 1)
            m_statusList.append(tr("MythTV is busy transcoding."));
        if (statusCode & 2)
            m_statusList.append(tr("MythTV is busy flagging commercials."));
        if (statusCode & 4)
            m_statusList.append(tr("MythTV is busy grabbing EPG data."));
        if (statusCode & 16)
            m_statusList.append(tr("MythTV is locked by a user."));
        if (statusCode & 32)
            m_statusList.append(tr("MythTV has running or pending jobs."));
        if (statusCode & 64)
            m_statusList.append(tr("MythTV is in a daily wakeup/shutdown period."));
        if (statusCode & 128)
            m_statusList.append(tr("MythTV is about to start a wakeup/shutdown period."));
    }

    if (m_statusList.empty())
    {
        if (m_bWillShutdown && m_secondsToShutdown != -1)
            m_statusList.append(tr("MythTV is idle and will shutdown in %n "
                                   "second(s).", "", m_secondsToShutdown));
        else
            m_statusList.append(tr("MythTV is idle."));
    }

    m_warning_text->SetVisible(m_hasConflicts);
}

bool WelcomeDialog::checkConnectionToServer(void)
{
    m_updateStatusTimer->stop();

    bool bRes = false;

    if (gCoreContext->IsConnectedToMaster())
        bRes = true;
    else
    {
        if (gCoreContext->SafeConnectToMasterServer(false))
        {
            bRes = true;
            updateAll();
        }
        else
           updateScreen();
    }

    if (bRes)
        m_updateStatusTimer->start(UPDATE_STATUS_INTERVAL);
    else
        m_updateStatusTimer->start(5000);

    return bRes;
}

void WelcomeDialog::showMenu(void)
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    m_menuPopup = new MythDialogBox("Menu", popupStack, "actionmenu");

    if (m_menuPopup->Create())
        popupStack->AddScreen(m_menuPopup);

    m_menuPopup->SetReturnEvent(this, "action");

    QString mythshutdown_status = m_appBinDir + "mythshutdown --status 0";
    uint statusCode = myth_system(mythshutdown_status + logPropagateArgs);

    if (!(statusCode & 0xFF00) && statusCode & 16)
        m_menuPopup->AddButton(tr("Unlock Shutdown"), SLOT(unlockShutdown()));
    else
        m_menuPopup->AddButton(tr("Lock Shutdown"), SLOT(lockShutdown()));

    m_menuPopup->AddButton(tr("Run mythfilldatabase"), SLOT(runEPGGrabber()));
    m_menuPopup->AddButton(tr("Shutdown Now"), SLOT(shutdownNow()));
    m_menuPopup->AddButton(tr("Exit"), SLOT(closeDialog()));
    m_menuPopup->AddButton(tr("Cancel"));
}

void WelcomeDialog::lockShutdown(void)
{
    QString command = m_appBinDir + "mythshutdown --lock";
    command += logPropagateArgs;
    myth_system(command);
    updateStatusMessage();
    updateScreen();
}

void WelcomeDialog::unlockShutdown(void)
{
    QString command = m_appBinDir + "mythshutdown --unlock";
    command += logPropagateArgs;
    myth_system(command);
    updateStatusMessage();
    updateScreen();
}

void WelcomeDialog::runEPGGrabber(void)
{
    runMythFillDatabase();
    sleep(1);
    updateStatusMessage();
    updateScreen();
}

void WelcomeDialog::shutdownNow(void)
{
    // if this is a frontend only machine just shut down now
    if (gCoreContext->IsFrontendOnly())
    {
        LOG(VB_GENERAL, LOG_INFO,
            "MythWelcome is shutting this computer down now");
        QString poweroff_cmd = gCoreContext->GetSetting("MythShutdownPowerOff", "");
        if (!poweroff_cmd.isEmpty())
            myth_system(poweroff_cmd);
        return;
    }

    // don't shutdown if we are recording
    if (m_isRecording)
    {
        ShowOkPopup(tr("Cannot shutdown because MythTV is currently recording"));
        return;
    }

    QDateTime curtime = MythDate::current();

    // don't shutdown if we are about to start recording
    if (!m_nextRecordingStart.isNull() &&
        curtime.secsTo(m_nextRecordingStart) - m_preRollSeconds <
        (m_idleWaitForRecordingTime * 60) + m_idleTimeoutSecs)
    {
        ShowOkPopup(tr("Cannot shutdown because MythTV is about to start recording"));
        return;
    }

    // don't shutdown if we are about to start a wakeup/shutdown period
    QString command = m_appBinDir + "mythshutdown --status 0";
    command += logPropagateArgs;

    uint statusCode = myth_system(command);
    if (!(statusCode & 0xFF00) && statusCode & 128)
    {
        ShowOkPopup(tr("Cannot shutdown because MythTV is about to start "
                       "a wakeup/shutdown period."));
        return;
    }

    // set the wakeup time for the next scheduled recording
    if (!m_nextRecordingStart.isNull())
    {
        QDateTime restarttime = m_nextRecordingStart.addSecs((-1) * m_preRollSeconds);

        int add = gCoreContext->GetNumSetting("StartupSecsBeforeRecording", 240);
        if (add)
            restarttime = restarttime.addSecs((-1) * add);

        QString wakeup_timeformat = gCoreContext->GetSetting("WakeupTimeFormat",
                                                            "yyyy-MM-ddThh:mm");
        QString setwakeup_cmd = gCoreContext->GetSetting("SetWakeuptimeCommand",
                                                        "echo \'Wakeuptime would "
                                                        "be $time if command "
                                                        "set.\'");

        if (wakeup_timeformat == "time_t")
        {
            QString time_ts;
            setwakeup_cmd.replace("$time",
                                  time_ts.setNum(restarttime.toTime_t()));
        }
        else
            setwakeup_cmd.replace(
                "$time", restarttime.toLocalTime().toString(wakeup_timeformat));

        if (!setwakeup_cmd.isEmpty())
        {
            myth_system(setwakeup_cmd);
        }
    }

    // run command to set wakeuptime in bios and shutdown the system
    command = QString();

#ifndef _WIN32
    command = "sudo ";
#endif

    command += m_appBinDir + "mythshutdown --shutdown" + logPropagateArgs;

    myth_system(command);
}

