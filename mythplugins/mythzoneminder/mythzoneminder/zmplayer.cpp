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

// C
#include <cstdlib>

// C++
#include <iostream>
using namespace std;

// Qt
#include <QKeyEvent>
#include <QTimer>

// MythTV
#include <mythcontext.h>
#include <mythdbcon.h>
#include <mythuihelper.h>
#include <mythmainwindow.h>
#include <mythdate.h>

// zoneminder
#include "zmplayer.h"
#include "zmclient.h"

ZMPlayer::ZMPlayer(MythScreenStack *parent, const char *name,
                   vector<Event *> *eventList, int *currentEvent)
         :MythScreenType(parent, name),
          m_frameImage(NULL), m_noEventsText(NULL), m_eventText(NULL),
          m_cameraText(NULL), m_frameText(NULL), m_dateText(NULL),
          m_playButton(NULL), m_deleteButton(NULL), m_nextButton(NULL),
          m_prevButton(NULL), m_currentEvent(currentEvent),
          m_eventList(eventList), m_frameList(new vector<Frame*>),
          m_frameTimer(new QTimer(this)), m_curFrame(0),  m_lastFrame(0),
          m_paused(false), m_fullScreen(false), m_image(NULL)
{
    connect(m_frameTimer, SIGNAL(timeout()), this,
            SLOT(updateFrame()));
}

ZMPlayer::~ZMPlayer()
{
    stopPlayer();

    m_frameTimer->deleteLater();

    if (m_frameList)
        delete m_frameList;
}

void ZMPlayer::stopPlayer(void)
{
    m_frameTimer->stop();
}

bool ZMPlayer::Create(void)
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("zoneminder-ui.xml", "zmplayer", this);

    if (!foundtheme)
        return false;

    bool err = false;

    // hide the fullscreen image
    UIUtilE::Assign(this, m_frameImage,   "framefsimage", &err);
    if (m_frameImage)
        m_frameImage->SetVisible(false);

    UIUtilE::Assign(this, m_frameImage,   "frameimage", &err);
    UIUtilE::Assign(this, m_noEventsText, "noevents_text", &err);
    UIUtilE::Assign(this, m_eventText,    "event_text", &err);
    UIUtilE::Assign(this, m_cameraText,   "camera_text", &err);
    UIUtilE::Assign(this, m_frameText,    "frame_text", &err);
    UIUtilE::Assign(this, m_dateText,     "date_text", &err);

    UIUtilW::Assign(this, m_playButton,   "play_button");
    UIUtilW::Assign(this, m_deleteButton, "delete_button");
    UIUtilW::Assign(this, m_prevButton,   "prev_button");
    UIUtilW::Assign(this, m_nextButton,   "next_button");

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'zmplayer'");
        return false;
    }

    if (m_playButton)
    {
        m_playButton->SetText(tr("Pause"));
        connect(m_playButton, SIGNAL(Clicked()), this, SLOT(playPressed()));
    }

    if (m_deleteButton)
    {
        m_deleteButton->SetText(tr("Delete"));
        connect(m_deleteButton, SIGNAL(Clicked()), this, SLOT(deletePressed()));
    }

    if (m_prevButton)
    {
        m_prevButton->SetText(tr("Previous"));
        connect(m_prevButton, SIGNAL(Clicked()), this, SLOT(prevPressed()));
    }

    if (m_nextButton)
    {
        m_nextButton->SetText(tr("Next"));
        connect(m_nextButton, SIGNAL(Clicked()), this, SLOT(nextPressed()));
    }

    BuildFocusList();

    SetFocusWidget(m_playButton);

    getEventInfo();

    return true;
}

void ZMPlayer::getEventInfo()
{
    if (m_frameTimer)
        m_frameTimer->stop();

    if (*m_currentEvent == -1)
    {
        stopPlayer();

        if (m_noEventsText)
            m_noEventsText->SetVisible(true);

        m_frameImage->SetFilename(QString("mz_black.png"));
        m_frameImage->Load();

        m_eventText->Reset();
        m_cameraText->Reset();
        m_frameText->Reset();
        m_dateText->Reset();

        return;
    }
    else
    {
        if (m_noEventsText)
            m_noEventsText->SetVisible(false);
    }

    Event *event = m_eventList->at(*m_currentEvent);
    if (!event)
        return;

    m_curFrame = 0;
    m_lastFrame = 0;

    m_eventText->SetText(QString(event->eventName() + " (%1/%2)")
            .arg((*m_currentEvent) + 1)
            .arg(m_eventList->size()));
    m_cameraText->SetText(event->monitorName());
    m_dateText->SetText(
        MythDate::toString(
            event->startTime(),
            MythDate::kDateTimeFull | MythDate::kSimplify));

    // get frames data
    m_frameList->clear();
    if (class ZMClient *zm = ZMClient::get())
    {
        zm->getFrameList(event->eventID(), m_frameList);
        m_curFrame = 1;
        m_lastFrame = m_frameList->size();
        m_frameText->SetText(QString("%1/%2").arg(m_curFrame).arg(m_lastFrame));
        getFrame();
    }
}

bool ZMPlayer::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("TV Playback", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "PAUSE")
        {
            if (m_playButton)
                m_playButton->Push();
        }
        else if (action == "DELETE")
        {
            if (m_deleteButton)
                m_deleteButton->Push();
        }
        else if (action == "LEFT")
        {
            if (m_paused)
            {
                if (m_curFrame > 1)
                    m_curFrame--;
                getFrame();
            }
        }
        else if (action == "RIGHT")
        {
            if (m_paused)
            {
                if (m_curFrame < m_lastFrame)
                    m_curFrame++;
                getFrame();
            }
        }
        else if (action == "PAGEUP")
        {
            if (m_prevButton)
                m_prevButton->Push();
        }
        else if (action == "PAGEDOWN")
        {
            if (m_nextButton)
                m_nextButton->Push();
        }
        else if (action == "TOGGLEASPECT" || action == "TOGGLEFILL")
        {
            if (!m_eventList->empty())
            {
                stopPlayer();

                if (m_fullScreen)
                {
                    m_fullScreen = false;
                    m_frameImage->SetVisible(false);
                    m_frameImage = dynamic_cast<MythUIImage *> (GetChild("frameimage"));
                    m_frameImage->SetVisible(true);
                }
                else
                {
                    m_fullScreen = true;
                    m_frameImage->SetVisible(false);
                    m_frameImage = dynamic_cast<MythUIImage *> (GetChild("framefsimage"));
                    m_frameImage->SetVisible(true);
                }

                if (!m_paused)
                    m_frameTimer->start(1000 / 100);

            }
        }
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void ZMPlayer::playPressed()
{
    if (m_eventList->empty())
        return;

    if (m_paused)
    {
        m_frameTimer->start(1000/25);
        m_paused = false;
        if (m_playButton)
            m_playButton->SetText(tr("Pause"));
    }
    else
    {
        m_frameTimer->stop();
        m_paused = true;
        if (m_playButton)
            m_playButton->SetText(tr("Play"));
    }
}

void ZMPlayer::deletePressed()
{
    if (m_eventList->empty() || *m_currentEvent > (int) m_eventList->size() - 1)
        return;

    Event *event = m_eventList->at(*m_currentEvent);
    if (event)
    {
        m_frameTimer->stop();

        if (class ZMClient *zm = ZMClient::get())
            zm->deleteEvent(event->eventID());

        m_eventList->erase(m_eventList->begin() + *m_currentEvent);
        if (*m_currentEvent > (int)m_eventList->size() - 1)
            *m_currentEvent = m_eventList->size() - 1;

        getEventInfo();

        if (!m_eventList->empty())
        {
            m_frameTimer->start(1000 / 25);
            m_paused = false;
        }
    }
}

void ZMPlayer::nextPressed()
{
    if (m_eventList->empty())
        return;

    if (*m_currentEvent >= (int) m_eventList->size() - 1)
        return;

    (*m_currentEvent)++;

    getEventInfo();

    if (m_paused)
        playPressed();
}

void ZMPlayer::prevPressed()
{
    if (m_eventList->empty())
        return;

    if (*m_currentEvent <= 0)
        return;

    if (*m_currentEvent > (int) m_eventList->size())
        *m_currentEvent = m_eventList->size();

    (*m_currentEvent)--;

    getEventInfo();

    if (m_paused)
        playPressed();
}

void ZMPlayer::updateFrame(void)
{
    if (!m_lastFrame)
        return;

    m_frameTimer->stop();

    m_curFrame++;
    if (m_curFrame > m_lastFrame)
    {
        m_paused = true;
        m_curFrame = 0;
        if (m_playButton)
            m_playButton->SetText(tr("Play"));
        return;
    }

    getFrame();
}

void ZMPlayer::getFrame(void)
{
    if (m_eventList->empty())
        return;

    Event *event = m_eventList->at(*m_currentEvent);
    if (event)
    {
        if (class ZMClient *zm = ZMClient::get())
            zm->getEventFrame(event, m_curFrame, &m_image);

        if (m_image)
        {
            m_frameImage->SetImage(m_image);
            m_frameText->SetText(QString("%1/%2").arg(m_curFrame).arg(m_lastFrame));
        }

        if (!m_paused)
        {
            if (m_curFrame < (int) m_frameList->size())
            {
                double delta = m_frameList->at(m_curFrame)->delta -
                               m_frameList->at(m_curFrame - 1)->delta;

            // FIXME: this is a bit of a hack to try to not swamp the cpu
                if (delta < 0.1)
                    delta = 0.1;

                m_frameTimer->start((int) (1000 * delta));
            }
            else
                m_frameTimer->start(1000 / 100);
        }
    }
}
