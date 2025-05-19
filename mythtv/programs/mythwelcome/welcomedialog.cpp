// POSIX
#include <unistd.h>

// C++
#include <chrono>
#include <cstdlib>

// qt
#include <QEvent>
#include <QGuiApplication>
#include <QKeyEvent>

// myth
#include "libmythbase/compat.h"
#include "libmythbase/exitcodes.h"
#include "libmythbase/lcddevice.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdbcon.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythsystemlegacy.h"
#include "libmythbase/remoteutil.h"
#include "libmythtv/tv.h"

// mythwelcome
#include "welcomedialog.h"
#include "welcomesettings.h"

static constexpr std::chrono::milliseconds UPDATE_STATUS_INTERVAL { 30s };
static constexpr std::chrono::milliseconds UPDATE_SCREEN_INTERVAL { 15s };


WelcomeDialog::WelcomeDialog(MythScreenStack *parent, const char *name)
              :MythScreenType(parent, name),
               m_updateStatusTimer(new QTimer(this)),
               m_updateScreenTimer(new QTimer(this))
{
    gCoreContext->addListener(this);

    m_appBinDir = GetAppBinDir();
    m_preRollSeconds = gCoreContext->GetDurSetting<std::chrono::seconds>("RecordPreRoll");
    m_idleWaitForRecordingTime =
        gCoreContext->GetDurSetting<std::chrono::minutes>("idleWaitForRecordingTime", 15min);
    m_idleTimeoutSecs = gCoreContext->GetDurSetting<std::chrono::seconds>("idleTimeoutSecs", 0s);

    // if idleTimeoutSecs is 0, the user disabled the auto-shutdown feature
    m_willShutdown = (m_idleTimeoutSecs != 0s);

    connect(m_updateStatusTimer, &QTimer::timeout,
            this, &WelcomeDialog::updateStatus);
    m_updateStatusTimer->start(UPDATE_STATUS_INTERVAL);

    connect(m_updateScreenTimer, &QTimer::timeout,
            this, &WelcomeDialog::updateScreen);
}

bool WelcomeDialog::Create(void)
{
    // Load the theme for this screen
    bool foundtheme = LoadWindowFromXML("welcome-ui.xml", "welcome_screen", this);
    if (!foundtheme)
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_statusText, "status_text", &err);
    UIUtilE::Assign(this, m_recordingText, "recording_text", &err);
    UIUtilE::Assign(this, m_scheduledText, "scheduled_text", &err);
    UIUtilE::Assign(this, m_warningText, "conflicts_text", &err);
    UIUtilE::Assign(this, m_startFrontendButton, "startfrontend_button", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'welcome_screen'");
        return false;
    }

    m_warningText->SetVisible(false);

    m_startFrontendButton->SetText(tr("Start Frontend"));
    connect(m_startFrontendButton, &MythUIButton::Clicked,
            this, &WelcomeDialog::startFrontendClick);

    BuildFocusList();

    SetFocusWidget(m_startFrontendButton);

    checkConnectionToServer();
    checkAutoStart();

    return true;
}

void WelcomeDialog::startFrontend(void)
{
    QString startFECmd = gCoreContext->GetSetting("MythWelcomeStartFECmd",
                         m_appBinDir + "mythfrontend");

    // Ensure we use the same platform for mythfrontend
    QStringList args;
    if (!startFECmd.contains("platform"))
        args << QString("--platform %1").arg(QGuiApplication::platformName());
    myth_system(startFECmd, args, kMSDisableUDPListener | kMSProcessEvents);
    updateAll();
    m_frontendIsRunning = false;
}

void WelcomeDialog::startFrontendClick(void)
{
    if (m_frontendIsRunning)
        return;

    m_frontendIsRunning = true;

    // this makes sure the button appears to click properly
    QTimer::singleShot(500ms, this, &WelcomeDialog::startFrontend);
}

void WelcomeDialog::checkAutoStart(void)
{
    // mythshutdown --startup returns 0 for automatic startup
    //                                1 for manual startup
    QString command = m_appBinDir + "mythshutdown --startup";
    command += logPropagateArgs;
    uint state = myth_system(command, kMSDontBlockInputDevs);

    LOG(VB_GENERAL, LOG_NOTICE,
        QString("mythshutdown --startup returned: %1").arg(state));

    bool bAutoStartFrontend = gCoreContext->GetBoolSetting("AutoStartFrontend", true);

    if (state == 1 && bAutoStartFrontend)
        startFrontendClick();

    // update status now
    updateAll();
}

void WelcomeDialog::customEvent(QEvent *e)
{
    if (e->type() == MythEvent::kMythEventMessage)
    {
        auto *me = dynamic_cast<MythEvent *>(e);
        if (me == nullptr)
            return;

        if (me->Message().startsWith("RECORDING_LIST_CHANGE") ||
            me->Message() == "UPDATE_PROG_INFO")
        {
            LOG(VB_GENERAL, LOG_NOTICE,
                "MythWelcome received a recording list change event");

            QMutexLocker lock(&m_recListUpdateMuxtex);

            if (pendingRecListUpdate())
            {
                LOG(VB_GENERAL, LOG_NOTICE,
                    "            [deferred to pending handler]");
            }
            else
            {
                // we can't query the backend from inside a customEvent
                QTimer::singleShot(500ms, this, &WelcomeDialog::updateRecordingList);
                setPendingRecListUpdate(true);
            }
        }
        else if (me->Message().startsWith("SCHEDULE_CHANGE"))
        {
            LOG(VB_GENERAL, LOG_NOTICE,
                "MythWelcome received a SCHEDULE_CHANGE event");

            QMutexLocker lock(&m_schedUpdateMuxtex);

            if (pendingSchedUpdate())
            {
                LOG(VB_GENERAL, LOG_NOTICE,
                    "            [deferred to pending handler]");
            }
            else
            {
                QTimer::singleShot(500ms, this, &WelcomeDialog::updateScheduledList);
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
                         myth_system(poweroff_cmd, kMSDontBlockInputDevs);
                }
            }
        }
    }
}

void WelcomeDialog::ShowSettings(GroupSetting *screen)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *ssd = new StandardSettingDialog(mainStack, "settings", screen);
    if (ssd->Create())
        mainStack->AddScreen(ssd);
    else
        delete ssd;
}

bool WelcomeDialog::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Welcome", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == "ESCAPE")
        {
            return true; // eat escape key
        }
        if (action == "MENU")
        {
            ShowMenu();
        }
        else if (action == "NEXTVIEW")
        {
            Close();
        }
        else if (action == "INFO")
        {
            ShowSettings(new MythWelcomeSettings());
        }
        else if (action == "SHOWSETTINGS")
        {
            ShowSettings(new MythShutdownSettings());
        }
        else if (action == "0")
        {
            QString mythshutdown_status =
                m_appBinDir + "mythshutdown --status 0";
            QString mythshutdown_unlock =
                m_appBinDir + "mythshutdown --unlock";
            QString mythshutdown_lock =
                m_appBinDir + "mythshutdown --lock";

            uint statusCode =
                myth_system(mythshutdown_status + logPropagateArgs, kMSDontBlockInputDevs);

            // is shutdown locked by a user
            if (!(statusCode & 0xFF00) && statusCode & 16)
            {
                myth_system(mythshutdown_unlock + logPropagateArgs, kMSDontBlockInputDevs);
            }
            else
            {
                myth_system(mythshutdown_lock + logPropagateArgs, kMSDontBlockInputDevs);
            }

            updateStatusMessage();
            updateScreen();
        }
        else if (action == "STARTXTERM")
        {
            QString cmd = gCoreContext->GetSetting("MythShutdownXTermCmd", "");
            if (!cmd.isEmpty())
                myth_system(cmd, kMSDontBlockInputDevs);
        }
        else if (action == "STARTSETUP")
        {
            QString mythtv_setup = m_appBinDir + "mythtv-setup";
            myth_system(mythtv_setup + logPropagateArgs);
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
        m_recordingText->SetText(tr("Cannot connect to server!"));
        m_scheduledText->SetText(tr("Cannot connect to server!"));
        m_warningText->SetVisible(false);
    }
    else
    {
        // update recording
        if (m_isRecording && !m_tunerList.empty())
        {
            TunerStatus tuner = m_tunerList[m_screenTunerNo];

            while (!tuner.isRecording)
            {
                if (m_screenTunerNo < m_tunerList.size() - 1)
                    m_screenTunerNo++;
                else
                    m_screenTunerNo = 0;
              tuner = m_tunerList[m_screenTunerNo];
            }

            status = tr("Tuner %1 is recording:").arg(tuner.id);
            status += "\n";
            status += tuner.channame;
            status += "\n" + tuner.title;
            if (!tuner.subtitle.isEmpty())
                status += "\n("+tuner.subtitle+")";

            status += "\n" +
              tr("%1 to %2", "Time period, 'starttime to endtime'")
                  .arg(MythDate::toString(tuner.startTime, MythDate::kTime),
                       MythDate::toString(tuner.endTime, MythDate::kTime));
        }
        else
        {
            status = tr("There are no recordings currently taking place");
        }

        m_recordingText->SetText(status);

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
        {
            status = tr("There are no scheduled recordings");
        }

        m_scheduledText->SetText(status);
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
        m_statusText->SetText(status);

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

    command = QString("%1 %2").arg(mfpath, mfarg);
    command += logPropagateArgs;

    command += "&";

    LOG(VB_GENERAL, LOG_INFO, QString("Grabbing EPG data using command: %1\n")
            .arg(command));

    myth_system(command, kMSDontBlockInputDevs);
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
        QMutexLocker lock(&m_recListUpdateMuxtex);
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
        QMutexLocker lock(&m_schedUpdateMuxtex);
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
        std::chrono::seconds(curtime.secsTo(m_nextRecordingStart)) - m_preRollSeconds <
        m_idleWaitForRecordingTime + m_idleTimeoutSecs)
    {
         m_statusList.append(tr("MythTV is about to start recording."));
    }

    if (m_isRecording)
    {
        m_statusList.append(tr("MythTV is busy recording."));
    }

    QString mythshutdown_status = m_appBinDir + "mythshutdown --status 0";
    uint statusCode = myth_system(mythshutdown_status + logPropagateArgs, kMSDontBlockInputDevs);

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
        if (m_willShutdown && m_secondsToShutdown != -1)
        {
            m_statusList.append(tr("MythTV is idle and will shutdown in %n "
                                   "second(s).", "", m_secondsToShutdown));
        }
        else
        {
            m_statusList.append(tr("MythTV is idle."));
        }
    }

    m_warningText->SetVisible(m_hasConflicts);
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
        {
           updateScreen();
        }
    }

    if (bRes)
        m_updateStatusTimer->start(UPDATE_STATUS_INTERVAL);
    else
        m_updateStatusTimer->start(5s);

    return bRes;
}

void WelcomeDialog::ShowMenu(void)
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    m_menuPopup = new MythDialogBox("Menu", popupStack, "actionmenu");

    if (m_menuPopup->Create())
        popupStack->AddScreen(m_menuPopup);

    m_menuPopup->SetReturnEvent(this, "action");

    QString mythshutdown_status = m_appBinDir + "mythshutdown --status 0";
    uint statusCode = myth_system(mythshutdown_status + logPropagateArgs, kMSDontBlockInputDevs);

    if (!(statusCode & 0xFF00) && statusCode & 16)
        m_menuPopup->AddButton(tr("Unlock Shutdown"), &WelcomeDialog::unlockShutdown);
    else
        m_menuPopup->AddButton(tr("Lock Shutdown"), &WelcomeDialog::lockShutdown);

    m_menuPopup->AddButton(tr("Run mythfilldatabase"), &WelcomeDialog::runEPGGrabber);
    m_menuPopup->AddButton(tr("Shutdown Now"), &WelcomeDialog::shutdownNow);
    m_menuPopup->AddButton(tr("Exit"), &WelcomeDialog::closeDialog);
    m_menuPopup->AddButton(tr("Cancel"));
}

void WelcomeDialog::lockShutdown(void)
{
    QString command = m_appBinDir + "mythshutdown --lock";
    command += logPropagateArgs;
    myth_system(command, kMSDontBlockInputDevs);
    updateStatusMessage();
    updateScreen();
}

void WelcomeDialog::unlockShutdown(void)
{
    QString command = m_appBinDir + "mythshutdown --unlock";
    command += logPropagateArgs;
    myth_system(command, kMSDontBlockInputDevs);
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
            myth_system(poweroff_cmd, kMSDontBlockInputDevs);
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
        std::chrono::seconds(curtime.secsTo(m_nextRecordingStart)) - m_preRollSeconds <
        m_idleWaitForRecordingTime + m_idleTimeoutSecs)
    {
        ShowOkPopup(tr("Cannot shutdown because MythTV is about to start recording"));
        return;
    }

    // don't shutdown if we are about to start a wakeup/shutdown period
    QString command = m_appBinDir + "mythshutdown --status 0";
    command += logPropagateArgs;

    uint statusCode = myth_system(command, kMSDontBlockInputDevs);
    if (!(statusCode & 0xFF00) && statusCode & 128)
    {
        ShowOkPopup(tr("Cannot shutdown because MythTV is about to start "
                       "a wakeup/shutdown period."));
        return;
    }

    // set the wakeup time for the next scheduled recording
    if (!m_nextRecordingStart.isNull())
    {
        QDateTime restarttime = m_nextRecordingStart.addSecs((-1) * m_preRollSeconds.count());

        int add = gCoreContext->GetNumSetting("StartupSecsBeforeRecording", 240);
        if (add)
            restarttime = restarttime.addSecs((-1LL) * add);

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
                                  time_ts.setNum(restarttime.toSecsSinceEpoch())
                );
        }
        else
        {
            setwakeup_cmd.replace(
                "$time", restarttime.toLocalTime().toString(wakeup_timeformat));
        }

        if (!setwakeup_cmd.isEmpty())
        {
            myth_system(setwakeup_cmd, kMSDontBlockInputDevs);
        }
    }

    // run command to set wakeuptime in bios and shutdown the system
    command = QString();

#ifndef _WIN32
    command = "sudo ";
#endif

    command += m_appBinDir + "mythshutdown --shutdown" + logPropagateArgs;

    myth_system(command, kMSDontBlockInputDevs);
}

