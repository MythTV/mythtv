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

// qt
#include <QDateTime>
#include <QTimer>
#include <QKeyEvent>

// myth
#include <mythcontext.h>
#include <mythuihelper.h>
#include <mythmainwindow.h>
#include <mythdialogbox.h>

// zoneminder
#include "zmliveplayer.h"
#include "zmclient.h"

// the maximum image size we are ever likely to get from ZM
#define MAX_IMAGE_SIZE  (2048*1536*3)

const int FRAME_UPDATE_TIME = 1000 / 10;  // try to update the frame 10 times a second

ZMLivePlayer::ZMLivePlayer(MythScreenStack *parent)
             :MythScreenType(parent, "zmliveview"),
              m_frameTimer(new QTimer(this)), m_paused(false), m_monitorLayout(1),
              m_monitorCount(0), m_players(NULL), m_monitors(NULL)
{
    GetMythUI()->DoDisableScreensaver();

    connect(m_frameTimer, SIGNAL(timeout()), this,
            SLOT(updateFrame()));

    getMonitorList();
}

bool ZMLivePlayer::Create(void)
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("zoneminder-ui.xml", "zmliveplayer", this);

    if (!foundtheme)
        return false;

    if (!hideAll())
        return false;

    if (!initMonitorLayout())
        return false;

    return true;
}

MythUIType* ZMLivePlayer::GetMythUIType(const QString &name, bool optional)
{
    MythUIType *type = GetChild(name);

    if (!optional && !type)
        throw name;

    return type;
}

bool ZMLivePlayer::hideAll(void)
{
    try
    {
        // one player layout
        GetMythUIType("name1-1")->SetVisible(false);
        GetMythUIType("status1-1")->SetVisible(false);
        GetMythUIType("frame1-1")->SetVisible(false);

        // two player layout
        for (int x = 1; x < 3; x++)
        {
            GetMythUIType(QString("name2-%1").arg(x))->SetVisible(false);
            GetMythUIType(QString("status2-%1").arg(x))->SetVisible(false);
            GetMythUIType(QString("frame2-%1").arg(x))->SetVisible(false);
        }

        // four player layout
        for (int x = 1; x < 5; x++)
        {
            GetMythUIType(QString("name3-%1").arg(x))->SetVisible(false);
            GetMythUIType(QString("status3-%1").arg(x))->SetVisible(false);
            GetMythUIType(QString("frame3-%1").arg(x))->SetVisible(false);
        }

        // six player layout
        for (int x = 1; x < 7; x++)
        {
            GetMythUIType(QString("name4-%1").arg(x))->SetVisible(false);
            GetMythUIType(QString("status4-%1").arg(x))->SetVisible(false);
            GetMythUIType(QString("frame4-%1").arg(x))->SetVisible(false);
        }

        // eight player layout
        for (int x = 1; x < 9; x++)
        {
            GetMythUIType(QString("name5-%1").arg(x))->SetVisible(false);
            GetMythUIType(QString("status5-%1").arg(x))->SetVisible(false);
            GetMythUIType(QString("frame5-%1").arg(x))->SetVisible(false);
        }
    }
    catch (const QString &name)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Theme is missing a critical theme element ('%1')")
                .arg(name));
        return false;
    }

    return true;
}

bool ZMLivePlayer::initMonitorLayout()
{
    // if we haven't got any monitors there's not much we can do so bail out!
    if (m_monitors->empty())
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot find any monitors. Bailing out!");
        ShowOkPopup(tr("Can't show live view.") + "\n" +
                    tr("You don't have any monitors defined!"));
        return false;
    }

    setMonitorLayout(gCoreContext->GetNumSetting("ZoneMinderLiveLayout", 1), true);
    m_frameTimer->start(FRAME_UPDATE_TIME);

    return true;
}

ZMLivePlayer::~ZMLivePlayer()
{
    gCoreContext->SaveSetting("ZoneMinderLiveLayout", m_monitorLayout);

    GetMythUI()->DoRestoreScreensaver();

    if (m_players)
    {
        QString s = "";
        Player *p;
        vector<Player*>::iterator i = m_players->begin();
        for (; i != m_players->end(); ++i)
        {
            p = *i;
            if (s != "")
                s += ",";
            s += QString("%1").arg(p->getMonitor()->id);
        }

        gCoreContext->SaveSetting("ZoneMinderLiveCameras", s);

        delete m_players;
    }
    else
        gCoreContext->SaveSetting("ZoneMinderLiveCameras", "");

    if (m_monitors)
        delete m_monitors;

    delete m_frameTimer;
}

bool ZMLivePlayer::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
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
            if (m_monitorLayout > 5)
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

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
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
    for (; i != m_monitors->end(); ++i)
    {
        mon = *i;
        if (oldMonID == mon->id)
        {
            break;
        }
    }

    // get the next monitor in the list
    if (i != m_monitors->end())
        ++i;

    // wrap around to the start if we've reached the end
    if (i == m_monitors->end())
        i = m_monitors->begin();

    mon = *i;

    m_players->at(playerNo - 1)->setMonitor(mon);
    m_players->at(playerNo - 1)->updateCamera();

    m_frameTimer->start(FRAME_UPDATE_TIME);
}

void ZMLivePlayer::updateFrame()
{
    class ZMClient *zm = ZMClient::get();
    if (!zm)
        return;

    static unsigned char buffer[MAX_IMAGE_SIZE];
    m_frameTimer->stop();

    // get a list of monitor id's that need updating
    QList<int> monList;
    Player *p;
    vector<Player*>::iterator i = m_players->begin();
    for (; i != m_players->end(); ++i)
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
            for (; i != m_players->end(); ++i)
            {
                p = *i;
                if (p->getMonitor()->id == monList[x])
                {
                    if (p->getMonitor()->status != status)
                    {
                        p->getMonitor()->status = status;
                        p->updateStatus();
                    }
                    p->updateFrame(buffer);
                }
            }
        }
    }

    m_frameTimer->start(FRAME_UPDATE_TIME);
}

void ZMLivePlayer::stopPlayers()
{
    m_frameTimer->stop();
}

void ZMLivePlayer::setMonitorLayout(int layout, bool restore)
{
    QStringList monList = gCoreContext->GetSetting("ZoneMinderLiveCameras", "").split(",");
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
        m_monitorCount = 6;
    else if (layout == 5)
        m_monitorCount = 8;

    hideAll();

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
                for (; i != m_monitors->end(); ++i)
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

        MythUIImage *frameImage = dynamic_cast<MythUIImage *> (GetChild(QString("frame%1-%2").arg(layout).arg(x)));
        MythUIText  *cameraText = dynamic_cast<MythUIText *> (GetChild(QString("name%1-%2").arg(layout).arg(x)));
        MythUIText  *statusText = dynamic_cast<MythUIText *> (GetChild(QString("status%1-%2").arg(layout).arg(x)));

        Player *p = new Player();
        p->setMonitor(monitor);
        p->setWidgets(frameImage, statusText, cameraText);
        p->updateCamera();
        m_players->push_back(p);

        monitorNo++;
        if (monitorNo > m_monitors->size())
            monitorNo = 1;
    }

    updateFrame();
}

void ZMLivePlayer::getMonitorList(void)
{
    if (!m_monitors)
        m_monitors = new vector<Monitor *>;

    m_monitors->clear();

    if (class ZMClient *zm = ZMClient::get())
        zm->getMonitorList(m_monitors);
}

////////////////////////////////////////////////////////////////////////////////////

Player::Player() :
    m_frameImage(NULL), m_statusText(NULL), m_cameraText(NULL),
    m_rgba(NULL)
{
}

Player::~Player()
{
    if (m_rgba)
        free(m_rgba);
}

void Player::setMonitor(Monitor *mon)
{
    m_monitor = *mon;

    if (m_rgba)
        free(m_rgba);

    m_rgba = (uchar *) malloc(m_monitor.width * m_monitor.height * 4);
}

void Player::setWidgets(MythUIImage *image, MythUIText *status, MythUIText  *camera)
{
    m_frameImage = image;
    m_statusText = status;
    m_cameraText = camera;

    if (m_frameImage)
        m_frameImage->SetVisible(true);

    if (m_statusText)
        m_statusText->SetVisible(true);

    if (m_cameraText)
        m_cameraText->SetVisible(true);
}

void Player::updateFrame(const unsigned char* buffer)
{
    unsigned int pos_data;
    unsigned int pos_rgba = 0;
    unsigned char a,r,g,b;

    if (m_monitor.bytes_per_pixel == 1)
    {
        // 8 bit grey scale
        for (pos_data = 0; pos_data < (unsigned int) (m_monitor.width * m_monitor.height); )
        {
            m_rgba[pos_rgba++] = buffer[pos_data];   //b
            m_rgba[pos_rgba++] = buffer[pos_data];   //g
            m_rgba[pos_rgba++] = buffer[pos_data++]; //r
            m_rgba[pos_rgba++] = 0xff;               //a
        }
    }
    else if (m_monitor.bytes_per_pixel == 3)
    {
        // 24 bits per pixel
        for (pos_data = 0; pos_data < (unsigned int) (m_monitor.width * m_monitor.height * 3); )
        {
            r = buffer[pos_data++];
            g = buffer[pos_data++];
            b = buffer[pos_data++];

            m_rgba[pos_rgba++] = b;
            m_rgba[pos_rgba++] = g;
            m_rgba[pos_rgba++] = r;
            m_rgba[pos_rgba++] = 0xff;
        }
    }
    else
    {
        // must be 32 bits per pixel
        for (pos_data = 0; pos_data < (unsigned int) (m_monitor.width * m_monitor.height * 4); )
        {
            r = buffer[pos_data++];
            g = buffer[pos_data++];
            b = buffer[pos_data++];
            a = buffer[pos_data++];

            m_rgba[pos_rgba++] = b;
            m_rgba[pos_rgba++] = g;
            m_rgba[pos_rgba++] = r;
            m_rgba[pos_rgba++] = a;
        }
    }

    QImage image(m_rgba, m_monitor.width, m_monitor.height, QImage::Format_ARGB32);

    MythImage *img = GetMythMainWindow()->GetCurrentPainter()->GetFormatImage();
    img->Assign(image);
    m_frameImage->SetImage(img);
    img->DecrRef();
}

void Player::updateStatus(void)
{
    if (m_statusText)
    {
        if (m_monitor.status == "Alarm" || m_monitor.status == "Error")
            m_statusText->SetFontState("alarm");
        else if (m_monitor.status == "Alert")
            m_statusText->SetFontState("alert");
        else
            m_statusText->SetFontState("idle");

        m_statusText->SetText(m_monitor.status);
    }
}

void Player::updateCamera()
{
    if (m_cameraText)
        m_cameraText->SetText(m_monitor.name);
}
