/* ============================================================
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published bythe Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ============================================================ */

// C++
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <unistd.h>

// qt
#include <QTimer>

// MythTV
#include <libmyth/mythcontext.h>
#include <libmythbase/mythdate.h>
#include <libmythbase/mythdbcon.h>
#include <libmythui/mythmainwindow.h>
#include <libmythui/mythuitext.h>

// zoneminder
#include "zmclient.h"
#include "zmconsole.h"

constexpr std::chrono::milliseconds STATUS_UPDATE_TIME { 10s }; // update the status every 10 seconds
constexpr std::chrono::milliseconds TIME_UPDATE_TIME   {  1s }; // update the time every 1 second

bool FunctionDialog::Create()
{
    if (!LoadWindowFromXML("zoneminder-ui.xml", "functionpopup", this))
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_captionText,  "caption_text", &err);
    UIUtilE::Assign(this, m_functionList, "function_list", &err);
    UIUtilE::Assign(this, m_enabledCheck, "enable_check", &err);
    UIUtilE::Assign(this, m_notificationCheck, "notification_check", &err);
    UIUtilE::Assign(this, m_okButton,     "ok_button", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'functionpopup'");
        return false;
    }

    new MythUIButtonListItem(m_functionList, "Monitor");
    new MythUIButtonListItem(m_functionList, "Modect");
    new MythUIButtonListItem(m_functionList, "Nodect");
    new MythUIButtonListItem(m_functionList, "Record");
    new MythUIButtonListItem(m_functionList, "Mocord");
    new MythUIButtonListItem(m_functionList, "None");

    m_functionList->MoveToNamedPosition(m_monitor->function);

    m_captionText->SetText(m_monitor->name);

    connect(m_okButton, &MythUIButton::Clicked, this, &FunctionDialog::setMonitorFunction);

    if (m_monitor->enabled)
        m_enabledCheck->SetCheckState(MythUIStateType::Full);
    else
        m_enabledCheck->SetCheckState(MythUIStateType::Off);


    if (m_monitor->showNotifications)
        m_notificationCheck->SetCheckState(MythUIStateType::Full);
    else
        m_notificationCheck->SetCheckState(MythUIStateType::Off);

    BuildFocusList();

    SetFocusWidget(m_functionList);

    return true;
}

void FunctionDialog::setMonitorFunction(void)
{
    QString function = m_functionList->GetValue();
    bool enabled = (m_enabledCheck->GetCheckState() == MythUIStateType::Full);
    bool showNotifications = (m_notificationCheck->GetCheckState() == MythUIStateType::Full);

    LOG(VB_GENERAL, LOG_INFO,
        "Monitor id : " + QString::number(m_monitor->id) +
        " function change " + m_monitor->function + " -> " + function +
        ", enable value " + QString::number(m_monitor->enabled) + " -> " + QString::number(enabled) +
        ", notification value " + QString::number(m_monitor->showNotifications) + " -> " + QString::number(showNotifications));

    m_monitor->showNotifications = showNotifications;
    ZMClient::get()->saveNotificationMonitors();

    if ((m_monitor->function == function) && (m_monitor->enabled == enabled))
    {
        LOG(VB_GENERAL, LOG_WARNING,
            "Monitor Function/Enable values not changed so not updating.");
        emit haveResult(false);
        Close();
        return;
    }

    ZMClient::get()->setMonitorFunction(m_monitor->id, function, enabled);

    emit haveResult(true);

    Close();
}

///////////////////////////////////////////////////////////////////////////////

ZMConsole::ZMConsole(MythScreenStack *parent)
          :MythScreenType(parent, "zmconsole"),
           m_popupStack(GetMythMainWindow()->GetStack("popup stack")),
           m_timeTimer(new QTimer(this)),
           m_timeFormat(gCoreContext->GetSetting("TimeFormat", "h:mm AP")),
           m_updateTimer(new QTimer(this))
{
    connect(m_timeTimer, &QTimer::timeout, this,
            &ZMConsole::updateTime);

    connect(m_updateTimer, &QTimer::timeout, this,
            &ZMConsole::updateStatus);
}

ZMConsole::~ZMConsole()
{
    delete m_timeTimer;
}

bool ZMConsole::Create(void)
{
    // Load the theme for this screen
    bool foundtheme = LoadWindowFromXML("zoneminder-ui.xml", "zmconsole", this);
    if (!foundtheme)
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_monitorList, "monitor_list", &err);
    UIUtilE::Assign(this, m_statusText,  "status_text", &err);
    UIUtilE::Assign(this, m_timeText,    "time_text", &err);
    UIUtilE::Assign(this, m_dateText,    "date_text", &err);
    UIUtilE::Assign(this, m_loadText,    "load_text", &err);
    UIUtilE::Assign(this, m_diskText,    "disk_text", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'zmconsole'");
        return false;
    }

    BuildFocusList();

    SetFocusWidget(m_monitorList);

    m_timeTimer->start(TIME_UPDATE_TIME);
    m_updateTimer->start(100ms);

    updateTime();

    return true;
}

void ZMConsole::updateTime(void)
{
    QString s = MythDate::current().toLocalTime().toString(m_timeFormat);

    if (s != m_timeText->GetText())
        m_timeText->SetText(s);

    s = MythDate::current().toLocalTime().toString("dddd\ndd MMM yyyy");

    if (s != m_dateText->GetText())
        m_dateText->SetText(s);
}

void ZMConsole::updateStatus()
{
    m_updateTimer->stop();
    getDaemonStatus();
    getMonitorStatus();
    m_updateTimer->start(STATUS_UPDATE_TIME);
}

void ZMConsole::getDaemonStatus(void)
{
    ZMClient::get()->getServerStatus(m_daemonStatus, m_cpuStat, m_diskStat);

    if (m_daemonStatus.left(7) == "running")
    {
        m_statusText->SetFontState("running");
        m_statusText->SetText(tr("Running"));
    }
    else
    {
        m_statusText->SetFontState("stopped");
        m_statusText->SetText(tr("Stopped"));
    }

    m_loadText->SetText("Load: " + m_cpuStat);
    m_diskText->SetText("Disk: " + m_diskStat);
}

void ZMConsole::getMonitorStatus(void)
{
    updateMonitorList();
}

bool ZMConsole::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == "MENU")
        {
            showEditFunctionPopup();
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

void ZMConsole::showEditFunctionPopup()
{
    auto *currentMonitor = m_monitorList->GetItemCurrent()->GetData().value<Monitor*>();
    if (!currentMonitor)
        return;

    m_functionDialog = new FunctionDialog(m_popupStack, currentMonitor);

    if (m_functionDialog->Create())
    {
        m_popupStack->AddScreen(m_functionDialog, false);
        connect(m_functionDialog, &FunctionDialog::haveResult,
                this, &ZMConsole::functionChanged);
    }
}

void ZMConsole::updateMonitorList()
{
    ZMClient::get()->updateMonitorStatus();

    int pos = m_monitorList->GetCurrentPos();
    m_monitorList->Reset();

    for (int x = 0; x < ZMClient::get()->getMonitorCount(); x++)
    {
        Monitor *monitor = ZMClient::get()->getMonitorAt(x);

        if (monitor)
        {
            auto *item = new MythUIButtonListItem(m_monitorList,
                "", nullptr, true, MythUIButtonListItem::NotChecked);
            item->SetData(QVariant::fromValue(monitor));
            item->SetText(monitor->name, "name");
            item->SetText(monitor->zmcStatus, "zmcstatus");
            item->SetText(monitor->zmaStatus, "zmastatus");
            item->SetText(QString("%1").arg(monitor->events), "eventcount");
        }
    }

    m_monitorList->SetItemCurrent(pos);
}

void ZMConsole::functionChanged(bool changed)
{
    if (changed)
        updateStatus();
}
