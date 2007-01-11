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
#include <sys/shm.h>

// qt
#include <qdatetime.h>
#include <qtimer.h>
#include <qsqlquery.h>
#include <qsqldatabase.h>

// myth
#include "mythtv/mythcontext.h"

// zoneminder
#include "zmliveplayer.h"
#include "zmclient.h"

// the maximum image size we are ever likely to get from ZM
#define MAX_IMAGE_SIZE  (2048*1536*3)

const int FRAME_UPDATE_TIME = 1000 / 10;  // try to update the frame 25 times a second
const int STATUS_UPDATE_TIME = 1000 / 2;  // update the monitor status 2 times a second

ZMLivePlayer::ZMLivePlayer(int monitorID, int eventID, MythMainWindow *parent, 
                const QString &window_name, const QString &theme_filename, 
                const char *name)
    :MythThemedDialog(parent, window_name, theme_filename, name)
{
    m_monitorID = monitorID;
    m_eventID = eventID;
    wireUpTheme();

    m_paused = false;

    m_players = NULL;
    m_monitors = NULL;
    m_monitorLayout = 1;

    m_frameTimer = new QTimer(this);
    connect(m_frameTimer, SIGNAL(timeout()), this,
            SLOT(updateFrame()));

    m_statusTimer = new QTimer(this);
    connect(m_statusTimer, SIGNAL(timeout()), this,
            SLOT(updateMonitorStatus()));

    getMonitorList();

    // need to call this after everything is contructed so 
    // the player windows are created properly
    QTimer::singleShot(100, this, SLOT(initMonitorLayout()));
}

void ZMLivePlayer::initMonitorLayout()
{
    setMonitorLayout(1);
    m_frameTimer->start(FRAME_UPDATE_TIME);
    m_statusTimer->start(STATUS_UPDATE_TIME);
}

ZMLivePlayer::~ZMLivePlayer()
{
    if (m_players)
    {
        stopPlayers();
        delete m_players;
    }

    if (m_monitors)
        delete m_monitors;

    delete m_frameTimer;
    delete m_statusTimer;
}

UITextType* ZMLivePlayer::getTextType(QString name)
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

void ZMLivePlayer::keyPressEvent(QKeyEvent *e)
{
    if (!e) return;

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("TV Playback", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "PAUSE")
        {
            if (m_paused)
            {
                m_frameTimer->start(FRAME_UPDATE_TIME);
                m_paused = false;
            }
            else
            {
                m_frameTimer->stop();
                m_paused = true;
            }
        }
        else if (action == "INFO")
        {
            m_monitorLayout++;
            if (m_monitorLayout > 3)
                m_monitorLayout = 1;
            setMonitorLayout(m_monitorLayout);
        }
        else if (action == "1" || action == "2" || action == "3" ||
                 action == "4" || action == "5" || action == "6" ||
                 action == "7" || action == "8" || action == "9")
            changePlayerMonitor(action.toInt());
        else
            handled = false;
    }

    if (!handled)
        MythThemedDialog::keyPressEvent(e);
}

void ZMLivePlayer::changePlayerMonitor(int playerNo)
{
    m_frameTimer->stop();

    if (playerNo > (int)m_players->size())
        return;

    Monitor *mon = m_players->at(playerNo - 1)->getMonitor();
    int oldMonID = mon->id;

    // find the old monID in the list of available monitors
    vector<Monitor*>::iterator i = m_monitors->begin();
    for (; i != m_monitors->end(); i++)
    {
        mon = *i;
        if (oldMonID == mon->id)
        {
            break;
        }
    }

    // get the next monitor in the list
    if (i != m_monitors->end())
        i++;

    // wrap around to the start if we've reached the end
    if (i == m_monitors->end())
        i = m_monitors->begin();

    mon = *i;
    int newMonID = mon->id;
    m_players->at(playerNo - 1)->setMonitor(newMonID, winId());

    UITextType *text = getUITextType(QString("name%1-%2").arg(m_monitorLayout).arg(playerNo));
    if (text)
        text->SetText(mon->name);

    m_frameTimer->start(FRAME_UPDATE_TIME);
}

void ZMLivePlayer::wireUpTheme()
{
    // just get the fontProp's for later use
    m_idleFont = getFont("idle");
    m_alarmFont = getFont("alarm");
    m_alertFont = getFont("alert");
}

void ZMLivePlayer::updateFrame()
{
    static unsigned char buffer[MAX_IMAGE_SIZE];
    m_frameTimer->stop();

    if (m_players)
    {
        Player *p;
        vector<Player*>::iterator i = m_players->begin();
        for (; i != m_players->end(); i++)
        {
            QString status;
            p = *i;
            if (class ZMClient *zm = ZMClient::get())
            {
                int frameSize = zm->getLiveFrame(p->getMonitor()->id, status, buffer, sizeof(buffer));

                if (frameSize > 0 && !status.startsWith("ERROR"))
                {
                    p->getMonitor()->status = status;
                    p->updateScreen(buffer);
                }
            }
        }
    }

    m_frameTimer->start(FRAME_UPDATE_TIME);
}

void ZMLivePlayer::stopPlayers()
{
    m_frameTimer->stop();
    m_statusTimer->stop();

    Player *p;
    vector<Player*>::iterator i = m_players->begin();
    for (; i != m_players->end(); i++)
    {
        p = *i;
        p->stopPlaying();
    }
}

void ZMLivePlayer::setMonitorLayout(int layout)
{
    if (m_players)
    {
        stopPlayers();
        delete m_players;
    }

    m_players = new vector<Player *>;
    m_monitorCount = 1;

    if (layout == 1)
        m_monitorCount = 1;
    else if (layout == 2)
        m_monitorCount = 2;
    else if (layout == 3)
        m_monitorCount = 4;
    else if (layout == 4)
        m_monitorCount = 9;

    uint monitorNo = 1;

    for (int x = 1; x <= m_monitorCount; x++)
    {
        Monitor *monitor = m_monitors->at(monitorNo-1);

        UIImageType *frameImage = getUIImageType(QString("frame%1-%2").arg(layout).arg(x));
        if (frameImage)
        {
            QPoint pos = frameImage->DisplayPos();
            QSize size = frameImage->GetSize(true);
            monitor->displayRect.setRect(pos.x(), pos.y(), size.width(), size.height());
            Player *p = new Player();
            p->startPlayer(monitor, winId());
            m_players->push_back(p);
        }

        UITextType *text = getUITextType(QString("name%1-%2").arg(layout).arg(x));
        if (text)
            text->SetText(monitor->name);

        monitorNo++;
        if (monitorNo > m_monitors->size())
            monitorNo = 1;
    }

    setContext(layout);
    updateForeground();

    m_frameTimer->start(FRAME_UPDATE_TIME);
    m_statusTimer->start(STATUS_UPDATE_TIME);
}

void ZMLivePlayer::getMonitorList(void)
{
    if (!m_monitors)
        m_monitors = new vector<Monitor *>;

    m_monitors->clear();

    if (class ZMClient *zm = ZMClient::get())
        zm->getMonitorList(m_monitors);
}

void ZMLivePlayer::updateMonitorStatus()
{
    m_statusTimer->stop();

    for (int x = 1; x <= (int) m_players->size(); x++)
    {
        Monitor *monitor = m_players->at(x-1)->getMonitor();

        UITextType *text = getUITextType(QString("status%1-%2").arg(m_monitorLayout).arg(x));
        if (text)
        {
            if (monitor->status == "Alarm" || monitor->status == "Error")
                text->SetFont(m_alarmFont);
            else if (monitor->status == "Alert")
                text->SetFont(m_alertFont);
            else
                text->SetFont(m_idleFont);

            text->SetText(monitor->status);
        }
    }

    m_statusTimer->start(STATUS_UPDATE_TIME);
}


////////////////////////////////////////////////////////////////////////////////////

#define TEXTURE_WIDTH 1024
#define TEXTURE_HEIGHT 1024

Player::Player()
{
    m_initalized = false;
}

Player::~Player()
{
}

void Player::setMonitor(int monID, Window winID)
{
    stopPlaying();
    m_monitor.id = monID;
    startPlayer(&m_monitor, winID);
}

bool Player::startPlayer(Monitor *mon, Window winID)
{
    m_initalized = false;

    m_monitor = *mon;

    int screen_number;
    Window parent = winID;

    m_dis = XOpenDisplay(NULL);
    if (m_dis == NULL)
    {
        VERBOSE(VB_IMPORTANT, "MythZoneMinder: Unable to open display\n");
        m_monitor.status = "Error";
        return false;
    }

    screen_number = DefaultScreen(m_dis);

    if (!glXQueryExtension(m_dis, NULL, NULL))
    {
        VERBOSE(VB_IMPORTANT, "MythZoneMinder: X server has no OpenGL GLX extension");
        m_monitor.status = "Error";
        return false;
    }

    int configuration[] = {GLX_DOUBLEBUFFER,GLX_RGBA,GLX_DEPTH_SIZE, 24, None};
    XVisualInfo *vi;

    vi = glXChooseVisual(m_dis,screen_number,configuration);
    if (vi==NULL)
    {
        VERBOSE(VB_IMPORTANT, "MythZoneMinder: No appropriate RGB visual with depth buffer");
        m_monitor.status = "Error";
        return false;
    }

    m_cx = glXCreateContext(m_dis, vi, NULL, GL_TRUE);
    if (m_cx == NULL)
    {
        VERBOSE(VB_IMPORTANT, "MythZoneMinder: Couldn't create rendering context");
        m_monitor.status = "Error";
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
                               m_monitor.displayRect.x(), m_monitor.displayRect.y(),
                               m_monitor.displayRect.width(), m_monitor.displayRect.height(),
                               2, 0, 0);

    if (m_win == None)
    {
        VERBOSE(VB_IMPORTANT, "MythZoneMinder: Unable to create window\n");
        m_monitor.status = "Error";
        return false;
    }

    XMapWindow (m_dis, m_win);
    XMoveWindow(m_dis, m_win, m_monitor.displayRect.x(), m_monitor.displayRect.y());

    glXMakeCurrent(m_dis, m_win, m_cx);

    glTexImage2D(GL_TEXTURE_2D, 0, 3, TEXTURE_WIDTH, TEXTURE_HEIGHT, 0, GL_RGB, 
                 GL_UNSIGNED_BYTE, NULL);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glEnable(GL_TEXTURE_2D);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glShadeModel(GL_FLAT); 

    m_initalized = true;
    return true;
}

void Player::stopPlaying(void)
{
    if (!m_initalized)
        return;

    glXDestroyContext(m_dis, m_cx);
    XDestroyWindow(m_dis, m_win);
    XCloseDisplay(m_dis);
}

void Player::updateScreen(const unsigned char* buffer)
{
    if (!m_initalized)
        return;

    if (m_monitor.palette != MP_RGB24 && m_monitor.palette != MP_GREY)
        return;

    glXMakeCurrent(m_dis, m_win, m_cx);

    if (m_monitor.palette == MP_RGB24)
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_monitor.width, m_monitor.height,
                    GL_RGB, GL_UNSIGNED_BYTE, buffer);
    else
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_monitor.width, m_monitor.height,
                    GL_LUMINANCE, GL_UNSIGNED_BYTE, buffer);

    glViewport(0, 0, m_monitor.displayRect.width(), m_monitor.displayRect.height());

    glLoadIdentity();
    glTranslatef(-1.0,1.0,0.0);
    glScalef((GLfloat)TEXTURE_WIDTH / (GLfloat) m_monitor.width, 
                -(GLfloat)TEXTURE_HEIGHT / (GLfloat) m_monitor.height, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0,0.0); glVertex2f(0,0);
    glTexCoord2f(0.0,1.0); glVertex2f(0,2);
    glTexCoord2f(1.0,1.0); glVertex2f(2,2);
    glTexCoord2f(1.0,0.0); glVertex2f(2,0);
    glEnd();
    glXSwapBuffers(m_dis,m_win);
}
