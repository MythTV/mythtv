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

#include <iostream>
#include <unistd.h>
#include <cstdlib>

// qt
#include <QTimer>

// myth
#include <mythmainwindow.h>
#include <mythcontext.h>
#include <mythdbcon.h>
#include <mythdate.h>

// zoneminder
#include "zmconsole.h"
#include "zmclient.h"

const int STATUS_UPDATE_TIME = 1000 * 10; // update the status every 10 seconds
const int TIME_UPDATE_TIME = 1000 * 1;    // update the time every 1 second

FunctionDialog::FunctionDialog(MythScreenStack *parent, Monitor *monitor) :
    MythScreenType(parent, "functionpopup"), m_monitor(monitor),
    m_captionText(NULL), m_functionList(NULL),
    m_enabledCheck(NULL), m_okButton(NULL)
{
}

bool FunctionDialog::Create()
{
    if (!LoadWindowFromXML("zoneminder-ui.xml", "functionpopup", this))
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_captionText,  "caption_text", &err);
    UIUtilE::Assign(this, m_functionList, "function_list", &err);
    UIUtilE::Assign(this, m_enabledCheck, "enable_check", &err);
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

    m_okButton->SetText(tr("OK"));

    connect(m_okButton, SIGNAL(Clicked()), this, SLOT(setMonitorFunction()));

    if (m_monitor->enabled)
        m_enabledCheck->SetCheckState(MythUIStateType::Full);
    else
        m_enabledCheck->SetCheckState(MythUIStateType::Off);

    BuildFocusList();

    SetFocusWidget(m_functionList);

    return true;
}

void FunctionDialog::setMonitorFunction(void)
{
    QString function = m_functionList->GetValue();
    bool enabled = (m_enabledCheck->GetCheckState() == MythUIStateType::Full);

    LOG(VB_GENERAL, LOG_INFO,
        "Monitor id : " + QString::number(m_monitor->id) +
        " function change " + m_monitor->function + " -> " + function +
        ", enable value " + QString::number(m_monitor->enabled) + " -> " +
        QString::number(enabled));

    if (m_monitor->function == function && m_monitor->enabled == enabled)
    {
        LOG(VB_GENERAL, LOG_WARNING,
            "Monitor Function/Enable values not changed so not updating.");
        emit haveResult(false);
        Close();
        return;
    }

    if (class ZMClient *zm = ZMClient::get())
        zm->setMonitorFunction(m_monitor->id, function, enabled);

    emit haveResult(true);

    Close();
}

///////////////////////////////////////////////////////////////////////////////

ZMConsole::ZMConsole(MythScreenStack *parent)
          :MythScreenType(parent, "zmconsole"),
           m_monitorList(NULL),
           m_monitor_list(NULL), m_running_text(NULL), m_status_text(NULL),
           m_time_text(NULL), m_date_text(NULL), m_load_text(NULL),
           m_disk_text(NULL), m_functionDialog(NULL),
           m_popupStack(GetMythMainWindow()->GetStack("popup stack")),
           m_timeTimer(new QTimer(this)), m_updateTimer(new QTimer(this))
{
    m_timeFormat = gCoreContext->GetSetting("TimeFormat", "h:mm AP");

    connect(m_timeTimer, SIGNAL(timeout()), this,
            SLOT(updateTime()));

    connect(m_updateTimer, SIGNAL(timeout()), this,
            SLOT(updateStatus()));
}

ZMConsole::~ZMConsole()
{
    delete m_timeTimer;

    if (m_monitorList)
        delete m_monitorList;
}

bool ZMConsole::Create(void)
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("zoneminder-ui.xml", "zmconsole", this);

    if (!foundtheme)
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_monitor_list, "monitor_list", &err);
    UIUtilE::Assign(this, m_status_text,  "status_text", &err);
    UIUtilE::Assign(this, m_time_text,    "time_text", &err);
    UIUtilE::Assign(this, m_date_text,    "date_text", &err);
    UIUtilE::Assign(this, m_load_text,    "load_text", &err);
    UIUtilE::Assign(this, m_disk_text,    "disk_text", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'zmconsole'");
        return false;
    }

    BuildFocusList();

    SetFocusWidget(m_monitor_list);

    m_timeTimer->start(TIME_UPDATE_TIME);
    m_updateTimer->start(100);

    updateTime();

    return true;
}

void ZMConsole::updateTime(void)
{
    QString s = MythDate::current().toLocalTime().toString(m_timeFormat);

    if (s != m_time_text->GetText())
        m_time_text->SetText(s);

    s = MythDate::current().toLocalTime().toString("dddd\ndd MMM yyyy");

    if (s != m_date_text->GetText())
        m_date_text->SetText(s);
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
    if (class ZMClient *zm = ZMClient::get())
    {
        zm->getServerStatus(m_daemonStatus, m_cpuStat, m_diskStat);

        if (m_daemonStatus.left(7) == "running")
        {
            m_status_text->SetFontState("running");
            m_status_text->SetText(tr("Running"));
        }
        else
        {
            m_status_text->SetFontState("stopped");
            m_status_text->SetText(tr("Stopped"));
        }

        m_load_text->SetText("Load: " + m_cpuStat);
        m_disk_text->SetText("Disk: " + m_diskStat);
    }
}

void ZMConsole::getMonitorStatus(void)
{
    if (!m_monitorList)
        m_monitorList = new vector<Monitor*>;

    if (class ZMClient *zm = ZMClient::get())
    {
        zm->getMonitorStatus(m_monitorList);
        updateMonitorList();
    }
}

bool ZMConsole::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "MENU")
        {
            showEditFunctionPopup();
        }
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void ZMConsole::showEditFunctionPopup()
{
    Monitor *currentMonitor = NULL;

    int pos = m_monitor_list->GetCurrentPos();

    if (pos >= 0 && pos < (int) m_monitorList->size())
        currentMonitor = m_monitorList->at(pos);

    if (!currentMonitor)
        return;

    m_functionDialog = new FunctionDialog(m_popupStack, currentMonitor);

    if (m_functionDialog->Create())
    {
        m_popupStack->AddScreen(m_functionDialog, false);
        connect(m_functionDialog, SIGNAL(haveResult(bool)),
                this, SLOT(functionChanged(bool)));
    }
}

void ZMConsole::updateMonitorList()
{
    int pos = m_monitor_list->GetCurrentPos();
    m_monitor_list->Reset();

    for (uint x = 0; x < m_monitorList->size(); x++)
    {
        Monitor *monitor = m_monitorList->at(x);

        MythUIButtonListItem *item = new MythUIButtonListItem(m_monitor_list,
                "", NULL, true, MythUIButtonListItem::NotChecked);
        item->SetText(monitor->name, "name");
        item->SetText(monitor->zmcStatus, "zmcstatus");
        item->SetText(monitor->zmaStatus, "zmastatus");
        item->SetText(QString("%1").arg(monitor->events), "eventcount");
    }

    m_monitor_list->SetItemCurrent(pos);
}

void ZMConsole::functionChanged(bool changed)
{
    if (changed)
        updateStatus();
}
