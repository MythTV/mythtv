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
#include <qdatetime.h>
#include <qpainter.h>
#include <qdir.h>
#include <qtimer.h>
#include <qapplication.h>
#include <QKeyEvent>

// myth
#include "mythtv/mythcontext.h"
#include "mythtv/mythdbcon.h"

// zoneminder
#include "zmconsole.h"
#include "zmclient.h"

const int STATUS_UPDATE_TIME = 1000 * 10; // update the status every 10 seconds
const int TIME_UPDATE_TIME = 1000 * 1;    // update the time every 1 second

ZMConsole::ZMConsole(MythMainWindow *parent, 
                const QString &window_name, const QString &theme_filename, 
                const char *name)
    :MythThemedDialog(parent, window_name, theme_filename, name)
{
    m_monitorListSize = 0;
    m_currentMonitor = 0;
    wireUpTheme();

    m_monitorList = NULL;

    m_timeFormat = gContext->GetSetting("TimeFormat", "h:mm AP");

    m_timeTimer = new QTimer(this);
    connect(m_timeTimer, SIGNAL(timeout()), this,
            SLOT(updateTime()));
    m_timeTimer->start(TIME_UPDATE_TIME);

    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, SIGNAL(timeout()), this,
            SLOT(updateStatus()));
    m_updateTimer->start(100);

    updateTime();
}

ZMConsole::~ZMConsole()
{
    delete m_timeTimer;

    if (m_monitorList)
        delete m_monitorList;
}

void ZMConsole::updateTime(void)
{
    QString s = QTime::currentTime().toString(m_timeFormat);

    if (s != m_time_text->GetText())
        m_time_text->SetText(s);

    s = QDateTime::currentDateTime().toString("dddd\ndd MMM yyyy");

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
            m_status_text->SetText(tr("Running"));
            m_status_text->SetFont(m_runningFont);
        }
        else
        {
            m_status_text->SetText(tr("Stopped"));
            m_status_text->SetFont(m_stoppedFont);
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

void ZMConsole::keyPressEvent(QKeyEvent *e)
{
    if (!e) return;

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Global", e, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "UP")
        {
            if (getCurrentFocusWidget() == m_monitor_list)
            {
                monitorListUp(false);
            }
            else
                nextPrevWidgetFocus(true);
        }
        else if (action == "DOWN")
        {
            if (getCurrentFocusWidget() == m_monitor_list)
            {
                monitorListDown(false);
            }
            else
                nextPrevWidgetFocus(true);
        }
        else if (action == "PAGEUP")
        {
            if (getCurrentFocusWidget() == m_monitor_list)
            {
                monitorListUp(true);
            }
            else
                nextPrevWidgetFocus(true);
        }
        else if (action == "PAGEDOWN")
        {
            if (getCurrentFocusWidget() == m_monitor_list)
            {
                monitorListDown(true);
            }
            else
                nextPrevWidgetFocus(true);
        }
        else if (action == "ESCAPE")
        {
            handled = false;
        }
        else
            handled = false;
    }

    if (!handled)
        MythThemedDialog::keyPressEvent(e);
}

UITextType* ZMConsole::getTextType(QString name)
{
    UITextType* type = getUITextType(name);

    if (!type)
    {
        VERBOSE(VB_IMPORTANT, "ERROR: Failed to find '" + name + "' UI element in theme file\n" +
                "              Bailing out!");
        exit(0);
    }

    return type;
}

void ZMConsole::wireUpTheme()
{
    m_status_text = getTextType("status_text");
    m_time_text = getTextType("time_text");
    m_date_text = getTextType("date_text");
    m_load_text = getTextType("load_text");
    m_disk_text = getTextType("disk_text");

    m_runningFont = getFont("running");
    m_stoppedFont = getFont("stopped");

    // monitor list
    m_monitor_list = (UIListType*) getUIObject("monitor_list");
    if (m_monitor_list)
    {
        m_monitorListSize = m_monitor_list->GetItems();
        m_monitor_list->SetItemCurrent(0);
    }
}

void ZMConsole::updateMonitorList()
{
    QString tmptitle;
    if (m_monitor_list)
    {
        m_monitor_list->ResetList();
        if (m_monitor_list->isFocused())
            m_monitor_list->SetActive(true);

        int skip;
        if ((int)m_monitorList->size() <= m_monitorListSize || 
                m_currentMonitor <= m_monitorListSize / 2)
            skip = 0;
        else if (m_currentMonitor >= (int)m_monitorList->size() - 
                 m_monitorListSize + m_monitorListSize / 2)
            skip = m_monitorList->size() - m_monitorListSize;
        else
            skip = m_currentMonitor - m_monitorListSize / 2;
        m_monitor_list->SetUpArrow(skip > 0);
        m_monitor_list->SetDownArrow(skip + m_monitorListSize < (int)m_monitorList->size());

        int i;
        for (i = 0; i < m_monitorListSize; i++)
        {
            if (i + skip >= (int)m_monitorList->size())
                break;

            Monitor *monitor = m_monitorList->at(i + skip);

            m_monitor_list->SetItemText(i, 1, monitor->name);
            m_monitor_list->SetItemText(i, 2, monitor->zmcStatus);
            m_monitor_list->SetItemText(i, 3, monitor->zmaStatus);
            m_monitor_list->SetItemText(i, 4, QString::number(monitor->events));

            if (i + skip == m_currentMonitor)
                m_monitor_list->SetItemCurrent(i);
        }

        m_monitor_list->refresh();
    }
}

void ZMConsole::monitorListDown(bool page)
{
    if (m_currentMonitor < (int)m_monitorList->size() - 1)
    {
        m_currentMonitor += (page ? m_monitorListSize : 1);
        if (m_currentMonitor > (int)m_monitorList->size() - 1)
            m_currentMonitor = m_monitorList->size() - 1;

        updateMonitorList();
    }
}

void ZMConsole::monitorListUp(bool page)
{
    if (m_currentMonitor > 0)
    {
        m_currentMonitor -= (page ? m_monitorListSize : 1);
        if (m_currentMonitor < 0)
            m_currentMonitor = 0;

        updateMonitorList();
    }
}
