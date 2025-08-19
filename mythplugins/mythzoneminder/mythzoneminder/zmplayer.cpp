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
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>

// Qt
#include <QKeyEvent>
#include <QTimer>

// MythTV
#include <libmythbase/mythdate.h>
#include <libmythbase/mythdbcon.h>
#include <libmythbase/mythlogging.h>
#include <libmythui/mythmainwindow.h>
#include <libmythui/mythuihelper.h>

// zoneminder
#include "zmclient.h"
#include "zmplayer.h"

ZMPlayer::ZMPlayer(MythScreenStack *parent, const char *name,
                   std::vector<Event *> *eventList, size_t *currentEvent)
         :MythScreenType(parent, name),
          m_currentEvent(currentEvent),
          m_eventList(eventList), m_frameList(new std::vector<Frame*>),
          m_frameTimer(new QTimer(this))
{
    connect(m_frameTimer, &QTimer::timeout, this,
            &ZMPlayer::updateFrame);
}

ZMPlayer::~ZMPlayer()
{
    stopPlayer();

    m_frameTimer->deleteLater();

    delete m_frameList;
}

void ZMPlayer::stopPlayer(void)
{
    m_frameTimer->stop();
}

bool ZMPlayer::Create(void)
{
    // Load the theme for this screen
    bool foundtheme = LoadWindowFromXML("zoneminder-ui.xml", "zmplayer", this);
    if (!foundtheme)
        return false;

    bool err = false;

    UIUtilE::Assign(this, m_frameImageFS, "framefsimage", &err);
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
        connect(m_playButton, &MythUIButton::Clicked, this, &ZMPlayer::playPressed);
    }

    if (m_deleteButton)
    {
        m_deleteButton->SetText(tr("Delete"));
        connect(m_deleteButton, &MythUIButton::Clicked, this, &ZMPlayer::deletePressed);
    }

    if (m_prevButton)
    {
        m_prevButton->SetText(tr("Previous"));
        connect(m_prevButton, &MythUIButton::Clicked, this, &ZMPlayer::prevPressed);
    }

    if (m_nextButton)
    {
        m_nextButton->SetText(tr("Next"));
        connect(m_nextButton, &MythUIButton::Clicked, this, &ZMPlayer::nextPressed);
    }

    // hide the fullscreen image
    m_frameImageFS->SetVisible(false);
    m_activeFrameImage = m_frameImage;

    BuildFocusList();

    SetFocusWidget(m_playButton);

    getEventInfo();

    return true;
}

void ZMPlayer::getEventInfo()
{
    m_frameTimer->stop();

    if (*m_currentEvent == static_cast<size_t>(-1))
    {
        stopPlayer();

        if (m_noEventsText)
            m_noEventsText->SetVisible(true);

        m_activeFrameImage->SetFilename(QString("mz_black.png"));
        m_activeFrameImage->Load();

        m_eventText->Reset();
        m_cameraText->Reset();
        m_frameText->Reset();
        m_dateText->Reset();

        return;
    }

    if (m_noEventsText)
        m_noEventsText->SetVisible(false);

    Event *event = m_eventList->at(*m_currentEvent);
    if (!event)
        return;

    m_curFrame = 1;

    m_eventText->SetText(event->eventName() + QString(" (%1/%2)")
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
        m_frameText->SetText(QString("%1/%2").arg(m_curFrame).arg(m_frameList->size()));
        getFrame();
    }
}

bool ZMPlayer::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("TV Playback", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
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
                if (m_curFrame < m_frameList->size())
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
                    m_frameImageFS->SetVisible(true);
                    m_activeFrameImage = m_frameImageFS;
                }
                else
                {
                    m_fullScreen = true;
                    m_frameImageFS->SetVisible(false);
                    m_frameImage->SetVisible(true);
                    m_activeFrameImage = m_frameImage;
                }

                if (!m_paused)
                    m_frameTimer->start(10ms);

            }
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

void ZMPlayer::playPressed()
{
    if (m_eventList->empty())
        return;

    if (m_paused)
    {
        m_frameTimer->start(40ms);
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
    if (m_eventList->empty() || (*m_currentEvent > m_eventList->size() - 1))
        return;

    Event *event = m_eventList->at(*m_currentEvent);
    if (event)
    {
        m_frameTimer->stop();

        if (class ZMClient *zm = ZMClient::get())
            zm->deleteEvent(event->eventID());

        m_eventList->erase(m_eventList->begin() + *m_currentEvent);
        *m_currentEvent = std::min(*m_currentEvent, m_eventList->size() - 1);

        getEventInfo();

        if (!m_eventList->empty())
        {
            m_frameTimer->start(40ms);
            m_paused = false;
        }
    }
}

void ZMPlayer::nextPressed()
{
    if (m_eventList->empty())
        return;

    if (*m_currentEvent >= (m_eventList->size() - 1))
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

    if (*m_currentEvent == 0)
        return;

    *m_currentEvent = std::min(*m_currentEvent, m_eventList->size());

    (*m_currentEvent)--;

    getEventInfo();

    if (m_paused)
        playPressed();
}

void ZMPlayer::updateFrame(void)
{
    if (m_frameList->empty())
        return;

    m_frameTimer->stop();

    m_curFrame++;
    if (m_curFrame > m_frameList->size())
    {
        m_paused = true;
        m_curFrame = 1;
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
            m_activeFrameImage->SetImage(m_image);
            m_frameText->SetText(QString("%1/%2").arg(m_curFrame).arg(m_frameList->size()));
        }

        if (!m_paused)
        {
            if (m_curFrame < m_frameList->size())
            {
                double delta = m_frameList->at(m_curFrame)->delta -
                               m_frameList->at(m_curFrame - 1)->delta;

            // FIXME: this is a bit of a hack to try to not swamp the cpu
                delta = std::max(delta, 0.1);

                m_frameTimer->start((int) (1000 * delta));
            }
            else
            {
                m_frameTimer->start(10ms);
            }
        }
    }
}

#include "moc_zmplayer.cpp"
