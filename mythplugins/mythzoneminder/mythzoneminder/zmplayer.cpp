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
#include "zmplayer.h"
#include "zmclient.h"

ZMPlayer::ZMPlayer(vector<Event *> *eventList, int currentEvent, MythMainWindow *parent, 
                const QString &window_name, const QString &theme_filename, 
                const char *name)
    :MythThemedDialog(parent, window_name, theme_filename, name)
{
    m_eventList = eventList;
    m_currentEvent = currentEvent;

    wireUpTheme();

    m_frameList = new vector<Frame*>;
    m_initalized = false;
    m_paused = false;

    m_frameTimer = new QTimer(this);
    connect(m_frameTimer, SIGNAL(timeout()), this,
            SLOT(updateFrame()));

    getEventInfo();

    m_bFullScreen = false;
    setContext(1);
}

ZMPlayer::~ZMPlayer()
{
    stopPlayer();

    m_frameTimer->deleteLater();

    if (!m_frameList)
        delete m_frameList;
}

void ZMPlayer::getEventInfo()
{
    if (m_frameTimer)
        m_frameTimer->stop();

    if (m_currentEvent == -1)
    {
        if (m_noEventsText)
            m_noEventsText->show();

        m_frameImage->SetImage(QString("mz_black.png"));
        m_frameImage->LoadImage();

        m_eventText->SetText("");
        m_cameraText->SetText("");
        m_frameText->SetText("");
        m_dateText->SetText("");

        return;
    }
    else
    {
        if (m_noEventsText)
            m_noEventsText->hide();
    }

    Event *event = m_eventList->at(m_currentEvent);
    if (!event)
        return;

    m_curFrame = 0;
    m_lastFrame = 0;

    m_eventText->SetText(QString(event->eventName + " (%1/%2)")
            .arg(m_currentEvent + 1)
            .arg(m_eventList->size()));
    m_cameraText->SetText(event->monitorName);
    m_dateText->SetText(event->startTime);

    // get frames data
    m_frameList->clear();
    if (class ZMClient *zm = ZMClient::get())
    {
        zm->getFrameList(event->eventID, m_frameList);
        m_curFrame = 1;
        m_lastFrame = m_frameList->size();
        m_frameText->SetText(QString("%1/%2").arg(m_curFrame).arg(m_lastFrame));
        getFrame();
    }
}

UITextType* ZMPlayer::getTextType(QString name)
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

void ZMPlayer::keyPressEvent(QKeyEvent *e)
{
    if (!e) return;

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("TV Playback", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "UP")
        {
            nextPrevWidgetFocus(false);
        }
        else if (action == "DOWN")
        {
            nextPrevWidgetFocus(true);
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
                m_prevButton->push();
        }
        else if (action == "PAGEDOWN")
        {
            if (m_nextButton)
                m_nextButton->push();
        }
        else if (action == "PAUSE")
        {
            if (m_playButton)
                m_playButton->push();
        }
        else if (action == "DELETE")
        {
            if (m_deleteButton)
                m_deleteButton->push();
        }
        else if (action == "TOGGLEASPECT")
        {
            if (m_bFullScreen)
            {
                m_bFullScreen = false;
                setContext(1);

                QPoint pos = m_frameImage->DisplayPos();
                QSize size = m_frameImage->GetSize(true);
                m_displayRect.setRect(pos.x(), pos.y(), size.width(), size.height());
                stopPlayer();
                initPlayer();
                displayFrame();
            }
            else
            {
                m_bFullScreen = true;
                setContext(2);

                QPoint pos = m_frameFSImage->DisplayPos();
                QSize size = m_frameFSImage->GetSize(true);
                m_displayRect.setRect(pos.x(), pos.y(), size.width(), size.height());
                stopPlayer();
                initPlayer();
                displayFrame();
            }

            updateForeground();
        }
        else if (action == "SELECT")
        {
            activateCurrent();
        }
        else
            handled = false;
    }

    if (!handled)
        MythThemedDialog::keyPressEvent(e);
}

void ZMPlayer::wireUpTheme()
{
    m_frameImage = getUIImageType("frameimage");

    if (m_frameImage)
    {
        QPoint pos = m_frameImage->DisplayPos();
        QSize size = m_frameImage->GetSize(true);
        m_displayRect.setRect(pos.x(), pos.y(), size.width(), size.height());
    }

    m_frameFSImage = getUIImageType("framefsimage");

    m_noEventsText = getTextType("noevents_text");
    m_eventText = getTextType("event_text");
    m_cameraText = getTextType("camera_text");
    m_frameText = getTextType("frame_text");
    m_dateText = getTextType("date_text");

    // play button
    m_playButton = getUITextButtonType("play_button");
    if (m_playButton)
    {
        m_playButton->setText(tr("Pause"));
        connect(m_playButton, SIGNAL(pushed()), this, SLOT(playPressed()));
    }

    // delete button
    m_deleteButton = getUITextButtonType("delete_button");
    if (m_deleteButton)
    {
        m_deleteButton->setText(tr("Delete"));
        connect(m_deleteButton, SIGNAL(pushed()), this, SLOT(deletePressed()));
    }

    // prev button
    m_prevButton = getUITextButtonType("prev_button");
    if (m_prevButton)
    {
        m_prevButton->setText(tr("Previous"));
        connect(m_prevButton, SIGNAL(pushed()), this, SLOT(prevPressed()));
    }

    // next button
    m_nextButton = getUITextButtonType("next_button");
    if (m_nextButton)
    {
        m_nextButton->setText(tr("Next"));
        connect(m_nextButton, SIGNAL(pushed()), this, SLOT(nextPressed()));
    }

    buildFocusList();
    assignFirstFocus();
}

void ZMPlayer::playPressed()
{
    if (m_eventList->size() == 0)
        return;

    if (m_paused)
    {
        m_frameTimer->start(1000/25);
        m_paused = false;
        if (m_playButton)
            m_playButton->setText(tr("Pause"));
    }
    else
    {
        m_frameTimer->stop();
        m_paused = true;
        if (m_playButton)
            m_playButton->setText(tr("Play"));
    }
}

void ZMPlayer::deletePressed()
{
    if (m_eventList->size() == 0 || m_currentEvent > (int) m_eventList->size() - 1)
        return;

    Event *event = m_eventList->at(m_currentEvent);
    if (event)
    {
        m_frameTimer->stop();

        if (class ZMClient *zm = ZMClient::get())
            zm->deleteEvent(event->eventID);

        m_eventList->erase(m_eventList->begin() + m_currentEvent);
        if (m_currentEvent > (int)m_eventList->size() - 1)
            m_currentEvent = m_eventList->size() - 1;

        getEventInfo();
        m_frameTimer->start(1000 / 25);
        m_paused = false;
    }
}

void ZMPlayer::nextPressed()
{
    if (m_eventList->size() == 0)
        return;

    if (m_currentEvent >= (int) m_eventList->size() - 1)
        return;

    m_currentEvent++;

    getEventInfo();

    if (m_paused)
        playPressed();
}

void ZMPlayer::prevPressed()
{
    if (m_eventList->size() == 0)
        return;

    if (m_currentEvent <= 0)
        return;

    if (m_currentEvent > (int) m_eventList->size())
        m_currentEvent = m_eventList->size();

    m_currentEvent--;

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
        m_curFrame = 1;
        if (m_playButton)
            m_playButton->setText(tr("Play"));
        return;
    }

    getFrame();
}

void ZMPlayer::getFrame(void)
{
    Event *event = m_eventList->at(m_currentEvent);
    if (event)
    {
        if (class ZMClient *zm = ZMClient::get())
            zm->getEventFrame(event->monitorID, event->eventID, m_curFrame, m_image);

        displayFrame();

        if (!m_paused)
        {
            if (m_curFrame < (int) m_frameList->size())
            {
                double delta = m_frameList->at(m_curFrame)->delta - m_frameList->at(m_curFrame - 1)->delta;

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

#define TEXTURE_WIDTH 1024
#define TEXTURE_HEIGHT 1024

bool ZMPlayer::initPlayer(void)
{
    m_initalized = false;

    int screen_number;

    Window parent = winId();

    m_dis = XOpenDisplay(NULL);
    if (m_dis == NULL)
    {
        fprintf(stderr,"ERROR: Unable to open display\n");
        return false;
    }

    screen_number = DefaultScreen(m_dis);

    if (!glXQueryExtension(m_dis, NULL, NULL))
    {
        fprintf(stderr,"ERROR: X server has no OpenGL GLX extension");
        return false;
    }

    int configuration[] = {GLX_DOUBLEBUFFER,GLX_RGBA,GLX_DEPTH_SIZE, 24, None};
    XVisualInfo *vi;

    vi = glXChooseVisual(m_dis, screen_number, configuration);
    if (vi == NULL)
    {
        fprintf(stderr,"ERROR: no appropriate RGB visual with depth buffer");
        return false;
    }

    m_cx = glXCreateContext(m_dis, vi, NULL, GL_TRUE);
    if (m_cx == NULL)
    {
        fprintf(stderr,"ERROR: couldn't create rendering context");
        return false;
    }

    glDisable(GL_ALPHA_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_DITHER);
    glDisable(GL_FOG);
    glDisable(GL_LIGHTING);
    glDisable(GL_LOGIC_OP);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_TEXTURE_1D);
    glPixelTransferi(GL_MAP_COLOR, GL_FALSE);
    glPixelTransferi(GL_RED_SCALE, 1);
    glPixelTransferi(GL_RED_BIAS, 0);
    glPixelTransferi(GL_GREEN_SCALE, 1);
    glPixelTransferi(GL_GREEN_BIAS, 0);
    glPixelTransferi(GL_BLUE_SCALE, 1);
    glPixelTransferi(GL_BLUE_BIAS, 0);
    glPixelTransferi(GL_ALPHA_SCALE, 1);
    glPixelTransferi(GL_ALPHA_BIAS, 0);

    m_win = XCreateSimpleWindow (m_dis, parent, 
                                 m_displayRect.x(), m_displayRect.y(),
                                 m_displayRect.width(), m_displayRect.height(),
                                 2, 0, 0);

    if (m_win == None)
    {
        fprintf(stderr,"ERROR: Unable to create window\n");
        return false;
    }

    XMapWindow (m_dis, m_win);
    XMoveWindow(m_dis, m_win, m_displayRect.x(), m_displayRect.y());

    glXMakeCurrent(m_dis, m_win, m_cx);

    glTexImage2D(GL_TEXTURE_2D, 0, 3, TEXTURE_WIDTH, TEXTURE_HEIGHT, 0, GL_RGB, 
                 GL_UNSIGNED_BYTE, NULL);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glEnable(GL_TEXTURE_2D);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glShadeModel(GL_FLAT); 

    // don't know why we have to do this - without it the window sometimes isn't shown?
    XWindowAttributes win_attrib;
    XGetWindowAttributes(m_dis, m_win, &win_attrib);

    m_initalized = true;
    return true;
}

void ZMPlayer::stopPlayer(void)
{
    m_frameTimer->stop();

    if (!m_initalized)
        return;

    m_initalized = false;

    glXDestroyContext(m_dis, m_cx);
    XDestroyWindow(m_dis, m_win);
    XCloseDisplay(m_dis);
}

void ZMPlayer::displayFrame(void)
{
    if (m_eventList->size() == 0)
        return;

    if (!m_initalized)
        if (!initPlayer())
            return;

    if (m_image.isNull())
        return;

    glXMakeCurrent(m_dis, m_win, m_cx);

    m_image = m_image.swapRGB();
    unsigned char *data = m_image.bits();

    m_frameText->SetText(QString("%1/%2").arg(m_curFrame).arg(m_lastFrame));

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_image.width(), m_image.height(),
                    GL_RGBA, GL_UNSIGNED_BYTE, data);

    glViewport(0, 0, m_displayRect.width(), m_displayRect.height());

    glLoadIdentity();
    glTranslatef(-1.0,1.0,0.0);
    glScalef((GLfloat)TEXTURE_WIDTH / (GLfloat) m_image.width(), 
              -(GLfloat)TEXTURE_HEIGHT / (GLfloat) m_image.height(), 1);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0,0.0); glVertex2f(0,0);
    glTexCoord2f(0.0,1.0); glVertex2f(0,2);
    glTexCoord2f(1.0,1.0); glVertex2f(2,2);
    glTexCoord2f(1.0,0.0); glVertex2f(2,0);
    glEnd();
    glXSwapBuffers(m_dis, m_win);
}
