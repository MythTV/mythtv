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
#include <cstdlib>

// qt
#include <qdatetime.h>
#include <qtimer.h>
#include <qsqlquery.h>
#include <qsqldatabase.h>
#include <QKeyEvent>
#include <Q3ValueList>

// myth
#include "mythtv/mythcontext.h"
#include <mythtv/libmythui/mythuihelper.h>

// zoneminder
#include "zmliveplayer.h"
#include "zmclient.h"

// the maximum image size we are ever likely to get from ZM
#define MAX_IMAGE_SIZE  (2048*1536*3)

const int FRAME_UPDATE_TIME = 1000 / 10;  // try to update the frame 10 times a second
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

    GetMythUI()->DoDisableScreensaver();

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
    // if we haven't got any monitors there's not much we can do so bail out!
    if (m_monitors->size() == 0)
    {
        MythPopupBox::showOkPopup(gContext->GetMainWindow(), "No monitors",
                                  tr("Cannot find any monitors. Bailing out!"));
        reject();
        return;
    }

    setMonitorLayout(gContext->GetNumSetting("ZoneMinderLiveLayout", 1), true);
    m_frameTimer->start(FRAME_UPDATE_TIME);
    m_statusTimer->start(STATUS_UPDATE_TIME);
}

ZMLivePlayer::~ZMLivePlayer()
{
    gContext->SaveSetting("ZoneMinderLiveLayout", m_monitorLayout);

    GetMythUI()->DoRestoreScreensaver();

    if (m_players)
    {
        QString s = "";
        Player *p;
        vector<Player*>::iterator i = m_players->begin();
        for (; i != m_players->end(); i++)
        {
            p = *i;
            if (s != "")
                s += ",";
            s += QString("%1").arg(p->getMonitor()->id);
        }

        gContext->SaveSetting("ZoneMinderLiveCameras", s);
    }
    else
        gContext->SaveSetting("ZoneMinderLiveCameras", "");

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
        VERBOSE(VB_IMPORTANT, "ERROR: Failed to find '" + name + "' UI element in theme file\n" +
                "              Bailing out!");
    }

    return type;
}

void ZMLivePlayer::keyPressEvent(QKeyEvent *e)
{
    if (!e) return;

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("TV Playback", e, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
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
    if (playerNo > (int)m_players->size())
        return;

    m_frameTimer->stop();

    int oldMonID = m_players->at(playerNo - 1)->getMonitor()->id;
    Monitor *mon;

    // find the old monitor ID in the list of available monitors
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

    m_players->at(playerNo - 1)->setMonitor(mon, winId());

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
    class ZMClient *zm = ZMClient::get();
    if (!zm)
        return;

    static unsigned char buffer[MAX_IMAGE_SIZE];
    m_frameTimer->stop();

    // get a list of monitor id's that need updating
    Q3ValueList<int> monList;
    Player *p;
    vector<Player*>::iterator i = m_players->begin();
    for (; i != m_players->end(); i++)
    {
        p = *i;
        if (!monList.contains(p->getMonitor()->id))
            monList.append(p->getMonitor()->id);
    }

    for (int x = 0; x < monList.count(); x++)
    {
        QString status;
        int frameSize = zm->getLiveFrame(monList[x], status, buffer, sizeof(buffer));

        if (frameSize > 0 && !status.startsWith("ERROR"))
        {
            // update each player that is displaying this monitor
            Player *p;
            vector<Player*>::iterator i = m_players->begin();
            for (; i != m_players->end(); i++)
            {
                p = *i;
                if (p->getMonitor()->id == monList[x])
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

void ZMLivePlayer::setMonitorLayout(int layout, bool restore)
{
    QStringList monList = QStringList::split(",",
                          gContext->GetSetting("ZoneMinderLiveCameras", ""));
    m_monitorLayout = layout;

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
        Monitor *monitor = NULL;

        if (restore)
        {
            if (x <= (int) monList.size())
            {
                QString s = monList.at(x - 1);
                int monID = s.toInt(); 

                // try to find a monitor that matches the monID
                vector<Monitor*>::iterator i = m_monitors->begin();
                for (; i != m_monitors->end(); i++)
                {
                    if ((*i)->id == monID)
                    {
                        monitor = *i;
                        break;
                    }
                }
            }
        }

        if (!monitor)
            monitor = m_monitors->at(monitorNo - 1);

        UIImageType *frameImage = getUIImageType(QString("frame%1-%2").arg(layout).arg(x));
        if (frameImage)
        {
            QPoint pos = frameImage->DisplayPos();
            QSize size = frameImage->GetSize(true);
            Player *p = new Player();
            p->setDisplayRect(QRect(pos.x(), pos.y(), size.width(), size.height()));
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

    updateFrame();
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
    m_useGL = (gContext->GetNumSetting("ZoneMinderUseOpenGL", 1) == 1);
    m_XvImage = NULL;
    m_XImage = NULL;

    if (m_useGL)
        VERBOSE(VB_GENERAL, "MythZoneMinder: Using openGL for display");
    else
        VERBOSE(VB_GENERAL, "MythZoneMinder: Using Xv for display");
}

Player::~Player()
{
}

void Player::setMonitor(Monitor *mon, Window winID)
{
    stopPlaying();
    m_monitor = *mon;
    startPlayer(&m_monitor, winID);
}

bool Player::startPlayer(Monitor *mon, Window winID)
{
    bool res;

    if (m_useGL)
        res = startPlayerGL(mon, winID);
    else
        res = startPlayerXv(mon, winID);

    return res;
}

bool Player::startPlayerGL(Monitor *mon, Window winID)
{
    m_initalized = false;

    m_monitor = *mon;

    Window parent = winID;

    m_dis = XOpenDisplay(GetMythUI()->GetX11Display());
    if (m_dis == NULL)
    {
        VERBOSE(VB_IMPORTANT, "MythZoneMinder: Unable to open display\n");
        m_monitor.status = "Error";
        return false;
    }

    m_screenNum = DefaultScreen(m_dis);

    if (!glXQueryExtension(m_dis, NULL, NULL))
    {
        VERBOSE(VB_IMPORTANT, "MythZoneMinder: X server has no OpenGL GLX extension");
        m_monitor.status = "Error";
        return false;
    }

    int configuration[] = {GLX_DOUBLEBUFFER,GLX_RGBA,GLX_DEPTH_SIZE, 24, None};
    XVisualInfo *vi;

    vi = glXChooseVisual(m_dis, m_screenNum, configuration);
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
                               m_displayRect.x(), m_displayRect.y(),
                               m_displayRect.width(), m_displayRect.height(),
                               2, 0, 0);

    if (m_win == None)
    {
        VERBOSE(VB_IMPORTANT, "MythZoneMinder: Unable to create window\n");
        m_monitor.status = "Error";
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

    m_initalized = true;
    return true;
}

bool Player::startPlayerXv(Monitor *mon, Window winID)
{
    bool useXV = true;   // set to false to force a fall back to using ximage
    m_initalized = false;

    m_monitor = *mon;

    Window parent = winID;

    m_dis = XOpenDisplay(GetMythUI()->GetX11Display());
    if (m_dis == NULL)
    {
        VERBOSE(VB_IMPORTANT, "MythZoneMinder: Unable to open display\n");
        m_monitor.status = "Error";
        return false;
    }

    m_screenNum = DefaultScreen(m_dis);

    m_win = XCreateSimpleWindow (m_dis, parent, 
                                 m_displayRect.x(), m_displayRect.y(),
                                 m_displayRect.width(), m_displayRect.height(),
                                 2, 0, 0);

    if (m_win == None)
    {
        VERBOSE(VB_IMPORTANT, "MythZoneMinder: Unable to create window\n");
        m_monitor.status = "Error";
        return false;
    }

    XMapWindow (m_dis, m_win);
    XMoveWindow(m_dis, m_win, m_displayRect.x(), m_displayRect.y());

    m_XVport = -1;

    m_gc = XCreateGC(m_dis, m_win, 0, NULL);
    if (m_gc ==NULL)
    {
        VERBOSE(VB_IMPORTANT, "MythZoneMinder: Unable to create gc");
        m_monitor.status = "Error";
        return false; 
    }

    m_rgba = (char *) malloc(m_displayRect.width() * m_displayRect.height() * 4);

    m_haveXV = useXV;

    if (useXV)
    {
        m_XVport = getXvPortId(m_dis);
        if (m_XVport == -1)
        {
            VERBOSE(VB_GENERAL, "WARNING: Couldn't find free Xv adaptor with RGB XvImage support");
            VERBOSE(VB_GENERAL, "Falling back to XImage - slow and ugly rescaling");
            m_haveXV = false;
        }
        else
            VERBOSE(VB_GENERAL, "MythZoneMinder: Using Xv for scaling");
    }
    else
        VERBOSE(VB_GENERAL, "MythZoneMinder: Forcing use of software scaling");

    m_initalized = true;
    return true;
}

int Player::getXvPortId(Display *dpy)
{
    int portNum, numImages;
    unsigned int i, j, k, numAdapt;
    XvImageFormatValues *formats;
    XvAdaptorInfo *info;

    portNum = -1;

    if (Success != XvQueryAdaptors(dpy, DefaultRootWindow(dpy), &numAdapt, &info))
    {
        VERBOSE(VB_IMPORTANT, "No Xv adaptors found!");
        return -1;
    }

    VERBOSE(VB_GENERAL, QString("Found %1 Xv adaptors").arg(numAdapt));

    for (i = 0; i < numAdapt; i++)
    {
        if (info[i].type & XvImageMask)
        {
            // Adaptor has XvImage support
            formats = XvListImageFormats(dpy, info[i].base_id, &numImages);

            for (j = 0; j < (unsigned int) numImages; j++)
            {
                if (formats[j].id == RGB24)
                {
                    // It supports our format
                    for (k = 0; k < info[i].num_ports; k++)
                    {
                        /* try to grab a port */
                        if (Success == XvGrabPort(dpy, info[i].base_id + k, CurrentTime))
                        {
                            portNum = info[i].base_id + k;
                            break;
                        }
                    }
                }
                if (portNum != -1) 
                    break;
            }
            XFree(formats);
        }
        if (portNum != -1) 
            break;
    }

    XvFreeAdaptorInfo(info);
    return portNum;
}

void Player::stopPlaying(void)
{
    if (!m_initalized)
        return;

    if (m_useGL)
        glXDestroyContext(m_dis, m_cx);
    else
    {
        if (m_XVport != -1)
            XvUngrabPort(m_dis, m_XVport, CurrentTime);
        XFreeGC(m_dis, m_gc);

        if (m_XImage)
        {
            XDestroyImage(m_XImage);
            m_XImage = NULL;
        }

        if (m_XvImage)
        {
            XFree(m_XvImage);
            m_XvImage = NULL;
        }

        //free(m_rgba);
        //m_rgba = NULL;
    }

    XDestroyWindow(m_dis, m_win);
    XCloseDisplay(m_dis);
}

void Player::updateScreen(const unsigned char* buffer)
{
    if (m_useGL)
        updateScreenGL(buffer);
    else
        updateScreenXv(buffer);
}

void Player::updateScreenGL(const unsigned char* buffer)
{
    if (!m_initalized)
        return;

    glXMakeCurrent(m_dis, m_win, m_cx);

    if (m_monitor.palette == MP_GREY)
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_monitor.width, m_monitor.height,
                        GL_LUMINANCE, GL_UNSIGNED_BYTE, buffer);
    else
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_monitor.width, m_monitor.height,
                    GL_RGB, GL_UNSIGNED_BYTE, buffer);

    glViewport(0, 0, m_displayRect.width(), m_displayRect.height());

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

void Player::updateScreenXv(const unsigned char* buffer)
{
    if (!m_initalized)
        return;

    if (m_haveXV && !m_XvImage)
    {
        m_XvImage = XvCreateImage(m_dis, m_XVport, RGB24, m_rgba,
                                  m_monitor.width,  m_monitor.height);
        if (m_XvImage == NULL)
        {
            VERBOSE(VB_GENERAL, "WARNING: Unable to create XVImage");
            VERBOSE(VB_GENERAL, "Falling back to XImage - slow and ugly rescaling");
            m_haveXV = false;
        }
    }

    if (!m_haveXV && !m_XImage)
    {
        m_XImage = XCreateImage(m_dis, XDefaultVisual(m_dis, m_screenNum), 24, ZPixmap, 0,
                                m_rgba, m_displayRect.width(), m_displayRect.height(),
                                32, 4 * m_displayRect.width());
        if (m_XImage == NULL)
        {
            VERBOSE(VB_IMPORTANT, "ERROR: Unable to create XImage");
            return;
        }
    }

    unsigned int pos_data;
    unsigned int pos_rgba = 0;
    unsigned int r,g,b;

    if (m_haveXV)
    {
        if (m_monitor.palette == MP_GREY)
        {
            // grey palette
            for (pos_data = 0; pos_data < (unsigned int) (m_monitor.width * m_monitor.height); )
            {
                m_rgba[pos_rgba++] = buffer[pos_data];   //b
                m_rgba[pos_rgba++] = buffer[pos_data];   //g
                m_rgba[pos_rgba++] = buffer[pos_data++]; //r
                pos_rgba++;                              //a
            }
        }
        else
        {
            // all other color palettes
            for (pos_data = 0; pos_data < (unsigned int) (m_monitor.width * m_monitor.height * 3); )
            {
                r = buffer[pos_data++];
                g = buffer[pos_data++];
                b = buffer[pos_data++];
                m_rgba[pos_rgba++] = b;
                m_rgba[pos_rgba++] = g;
                m_rgba[pos_rgba++] = r;
                pos_rgba++;
            }
        }

        XvPutImage(m_dis, m_XVport, m_win, m_gc, m_XvImage, 0, 0, 
                   m_monitor.width, m_monitor.height,
                   0, 0, m_displayRect.width(), m_displayRect.height());
    }
    else
    {
        //software scaling
        for (int y = 0; y < m_displayRect.height(); y++)
        {
            for (int x = 0; x < m_displayRect.width(); x++)
            {

                pos_data = y * m_monitor.height / m_displayRect.height();
                pos_data = pos_data * m_monitor.width;
                pos_data = pos_data + x * m_monitor.width / m_displayRect.width();

                if (m_monitor.palette == MP_GREY)
                {
                    m_rgba[pos_rgba++] = buffer[pos_data];
                    m_rgba[pos_rgba++] = buffer[pos_data];
                    m_rgba[pos_rgba++] = buffer[pos_data++];
                    m_rgba[pos_rgba++] = 0;
                }

                if (m_monitor.palette != MP_GREY)
                {

                    pos_data = pos_data * 3;
                    r = buffer[pos_data++];
                    g = buffer[pos_data++];
                    b = buffer[pos_data++];
                    m_rgba[pos_rgba++] = b;
                    m_rgba[pos_rgba++] = g;
                    m_rgba[pos_rgba++] = r;
                    m_rgba[pos_rgba++] = 0;
                }
            }
        }

        XPutImage(m_dis, m_win, m_gc, m_XImage, 0, 0, 0, 0, 
                  m_displayRect.width(), m_displayRect.height());
    }
}
