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

// qt
#include <qdatetime.h>
#include <qpainter.h>
#include <qdir.h>
#include <qtimer.h>
#include <qprocess.h>

// myth
#include "mythtv/mythcontext.h"
#include "mythtv/mythdbcon.h"

// zoneminder
#include "zmevents.h"
#include "zmplayer.h"
#include "zmutils.h"

const int EVENTS_UPDATE_TIME = 1000 * 5; // update the events list every 5 seconds

ZMEvents::ZMEvents(MythMainWindow *parent, 
                const QString &window_name, const QString &theme_filename, 
                const char *name)
    :MythThemedDialog(parent, window_name, theme_filename, name)
{
    m_eventListSize = 0;
    m_currentEvent = 0;
    m_eventList = NULL;

    wireUpTheme();
    getEventList();
    getCameraList();

    updateUIList();

    m_updateTimer = new QTimer(this);

    connect(m_updateTimer, SIGNAL(timeout()), this,
            SLOT(updateTimeout()));
    m_updateTimer->start(EVENTS_UPDATE_TIME);
}

ZMEvents::~ZMEvents()
{
    if (m_updateTimer)
        delete m_updateTimer;

    if (!m_eventList)
        delete m_eventList;
}

UITextType* ZMEvents::getTextType(QString name)
{
    UITextType* type = getUITextType(name);

    if (!type)
    {
        cout << "ERROR: Failed to find '" << name <<  "' UI element in theme file\n"
                << "Bailing out!" << endl;
        exit(0);
    }

    return type;
}

void ZMEvents::keyPressEvent(QKeyEvent *e)
{
    if (!e) return;

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Global", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "UP")
        {
            if (getCurrentFocusWidget() == m_event_list)
            {
                eventListUp(false);
            }
            else
                nextPrevWidgetFocus(true);
        }
        else if (action == "DOWN")
        {
            if (getCurrentFocusWidget() == m_event_list)
            {
                eventListDown(false);
            }
            else
                nextPrevWidgetFocus(true);
        }
        else if (action == "LEFT")
        {
            if (getCurrentFocusWidget() == m_cameraSelector)
                m_cameraSelector->push(false);
            else
                nextPrevWidgetFocus(false);
        }
        else if (action == "RIGHT")
        {
            if (getCurrentFocusWidget() == m_cameraSelector)
                m_cameraSelector->push(true);
            else
                nextPrevWidgetFocus(true);
        }
        else if (action == "PAGEUP")
        {
            if (getCurrentFocusWidget() == m_event_list)
            {
                eventListUp(true);
            }
            else
                nextPrevWidgetFocus(true);
        }
        else if (action == "PAGEDOWN")
        {
            if (getCurrentFocusWidget() == m_event_list)
            {
                eventListDown(true);
            }
            else
                nextPrevWidgetFocus(true);
        }
        else if (action == "SELECT")
        {
            if (getCurrentFocusWidget() == m_event_list)
            {
                if (m_playButton)
                    m_playButton->push();
            }
            else
                activateCurrent();
        }
        else if (action == "DELETE")
        {
            if (m_deleteButton)
                m_deleteButton->push();
        }
        else
            handled = false;
    }

    if (!handled)
        MythThemedDialog::keyPressEvent(e);
}

void ZMEvents::wireUpTheme()
{
    // event list
    m_event_list = (UIListType*) getUIObject("event_list");
    if (m_event_list)
    {
        m_eventListSize = m_event_list->GetItems();
        m_event_list->SetItemCurrent(0);
    }

    m_eventNoText = getUITextType("eventno_text");

    // play button
    m_playButton = getUITextButtonType("play_button");
    if (m_playButton)
    {
        m_playButton->setText(tr("Play"));
        connect(m_playButton, SIGNAL(pushed()), this, SLOT(playPressed()));
    }

    // delete button
    m_deleteButton = getUITextButtonType("delete_button");
    if (m_deleteButton)
    {
        m_deleteButton->setText(tr("Delete"));
        connect(m_deleteButton, SIGNAL(pushed()), this, SLOT(deletePressed()));
    }

    // cameras selector
    m_cameraSelector = getUISelectorType("camera_selector");
    if (m_cameraSelector)
    {
        connect(m_cameraSelector, SIGNAL(pushed(int)),
                this, SLOT(setCamera(int)));
    }

    buildFocusList();
    assignFirstFocus();
}

void ZMEvents::getEventList()
{
    if (!m_eventList)
        m_eventList = new vector<Event*>;

    m_eventList->clear();

    QSqlQuery query(g_ZMDatabase);
    QString sql = "SELECT E.Id, E.Name, E.StartTime, M.Id AS MonitorID, M.Name AS MonitorName, "
                  "M.Width, M.Height, M.DefaultRate, M.DefaultScale, E.Length "
                  "from Events as E inner join Monitors as M on E.MonitorId = M.Id ";

    if (m_cameraSelector && m_cameraSelector->getCurrentString() != tr("All Cameras") && 
        m_cameraSelector->getCurrentString() != "")
    {
        sql += "WHERE M.Name = :NAME ";
        sql += "ORDER BY E.StartTime";
        query.prepare(sql);
        query.bindValue(":NAME", m_cameraSelector->getCurrentString());
    }
    else
    {
        sql += "ORDER BY E.StartTime";
        query.prepare(sql);
    }

    query.exec();
    if (query.isActive())
    {
        while (query.next())
        {
            Event *item = new Event;
            item->eventID = query.value(0).toInt();
            item->eventName = query.value(1).toString();
            item->monitorName = QString("%2").arg(query.value(4).toString());
            item->startTime = query.value(2).toDateTime().toString("ddd - dd/MM hh:mm:ss");
            item->monitorID = query.value(3).toInt();
            item->length = query.value(9).toString();
            m_eventList->push_back(item);
        }
    }
    else
        VERBOSE(VB_IMPORTANT, "ERROR: Failed to run get monitors query");
}

void ZMEvents::updateUIList()
{
    if (!m_eventList)
        return;

    QString tmptitle;
    if (m_event_list)
    {
        m_event_list->ResetList();
        if (m_event_list->isFocused())
            m_event_list->SetActive(true);

        int skip;
        if ((int)m_eventList->size() <= m_eventListSize || m_currentEvent <= m_eventListSize / 2)
            skip = 0;
        else if (m_currentEvent >= (int)m_eventList->size() - m_eventListSize + m_eventListSize / 2)
            skip = m_eventList->size() - m_eventListSize;
        else
            skip = m_currentEvent - m_eventListSize / 2;
        m_event_list->SetUpArrow(skip > 0);
        m_event_list->SetDownArrow(skip + m_eventListSize < (int)m_eventList->size());

        int i;
        for (i = 0; i < m_eventListSize; i++)
        {
            if (i + skip >= (int)m_eventList->size())
                break;

            Event *event = m_eventList->at(i + skip);

            m_event_list->SetItemText(i, 1, event->eventName);
            m_event_list->SetItemText(i, 2, event->monitorName);
            m_event_list->SetItemText(i, 3, event->startTime);
            m_event_list->SetItemText(i, 4, event->length);
            if (i + skip == m_currentEvent)
                m_event_list->SetItemCurrent(i);
        }

        m_event_list->refresh();
    }

    if (m_eventNoText)
        m_eventNoText->SetText(QString("%1/%2").arg(m_currentEvent + 1).arg(m_eventList->size()));
}

void ZMEvents::eventListDown(bool page)
{
    if (m_currentEvent < (int)m_eventList->size() - 1)
    {
        m_currentEvent += (page ? m_eventListSize : 1);
        if (m_currentEvent > (int)m_eventList->size() - 1)
            m_currentEvent = m_eventList->size() - 1;

        updateUIList();
    }
}

void ZMEvents::eventListUp(bool page)
{
    if (m_currentEvent > 0)
    {
        m_currentEvent -= (page ? m_eventListSize : 1);
        if (m_currentEvent < 0)
            m_currentEvent = 0;

        updateUIList();
    }
}

void ZMEvents::playPressed(void)
{
    Event *event = m_eventList->at(m_currentEvent);
    if (event)
    {
        ZMPlayer player(m_eventList, m_currentEvent, gContext->GetMainWindow(),
                        "zmplayer", "zoneminder-", "zmplayer");
        player.exec();

        // update the event list incase some events where deleted in the player
        if (m_currentEvent > (int)m_eventList->size() - 1)
            m_currentEvent = m_eventList->size() - 1;

        updateUIList();
    }
}

void ZMEvents::deletePressed(void)
{
    Event *event = m_eventList->at(m_currentEvent);
    if (event)
    {
        deleteEvent(event->eventID);
        getEventList();

        if (m_currentEvent > (int)m_eventList->size() - 1)
            m_currentEvent = m_eventList->size() - 1;

        updateUIList();
    }
}

void ZMEvents::getCameraList(void)
{
    QStringList cameras;
    Event *e;

    if (m_eventList && m_eventList->size() > 0)
    {
        vector<Event *>::iterator i = m_eventList->begin();
        for ( ; i != m_eventList->end(); i++)
        {
            e = *i;
            if (cameras.find(e->monitorName) == cameras.end())
                cameras.append(e->monitorName);
        }
    }

    // sort and add cameras to selector
    cameras.sort();

    if (m_cameraSelector)
    {
        m_cameraSelector->addItem(0, tr("All Cameras"));
        m_cameraSelector->setToItem(0);
    }

    for (uint x = 1; x <= cameras.count(); x++)
    {
        if (m_cameraSelector)
            m_cameraSelector->addItem(x, cameras[x-1]); 
    }
}

void ZMEvents::setCamera(int item)
{
    (void) item;
    cout << "setCamera(): " << item << endl;
    getEventList();
    updateUIList();
}

void ZMEvents::updateTimeout(void)
{
    m_updateTimer->stop();

    getEventList();
    updateUIList();

    m_updateTimer->start(EVENTS_UPDATE_TIME);
}