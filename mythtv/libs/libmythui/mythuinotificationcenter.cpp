//
//  mythuinotificationcenter.cpp
//  MythTV
//
//  Created by Jean-Yves Avenard on 25/06/13.
//  Copyright (c) 2013 Bubblestuff Pty Ltd. All rights reserved.
//

#include <QCoreApplication>
#include <QEvent>
#include <QObject>
#include <QThread>
#include "mythcorecontext.h"

#include "mythuinotificationcenter.h"

#include "mythscreenstack.h"
#include "mythscreentype.h"
#include "mythuiimage.h"
#include "mythuitext.h"
#include "mythuiprogressbar.h"

#define LOC QString("NotificationCenter: ")

//// MythUINotificationCenterEvent

QEvent::Type MythUINotificationCenterEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

//// class MythUINotificationScreen

class MythUINotificationScreen : public MythScreenType
{
public:
    MythUINotificationScreen(MythScreenStack *stack,
                             int id = -1);

    virtual ~MythUINotificationScreen();

    // These two methods are declared by MythScreenType and their signatures
    // should not be changed
    virtual bool Create(void);
    virtual void Init(void);

    void SetNotification(MythNotification &notification);

    void UpdateArtwork(const QImage &image);
    void UpdateMetaData(const DMAP &data);
    void UpdatePlayback(int duration, int position);

    QString stringFromSeconds(int time);

    enum Update {
        kForce      = 0,
        kImage      = 1 << 0,
        kDuration   = 1 << 1,
        kMetaData   = 1 << 2,
    };

    int                 m_id;
    QImage              m_image;
    QString             m_title;
    QString             m_artist;
    QString             m_album;
    QString             m_format;
    int                 m_duration;
    int                 m_position;
    bool                m_added;
    uint32_t            m_update;
    MythUIImage        *m_artworkImage;
    MythUIText         *m_titleText;
    MythUIText         *m_artistText;
    MythUIText         *m_albumText;
    MythUIText         *m_formatText;
    MythUIText         *m_timeText;
    MythUIProgressBar  *m_progressBar;
};

MythUINotificationScreen::MythUINotificationScreen(MythScreenStack *stack,
                                                   int id)
    : MythScreenType(stack, "mythuinotification"),  m_id(id),
      m_duration(-1),       m_position(-1),         m_added(false),
      m_update(0),
      m_artworkImage(NULL), m_titleText(NULL),      m_artistText(NULL),
      m_albumText(NULL),    m_formatText(NULL),     m_timeText(NULL),
      m_progressBar(NULL)
{
}

MythUINotificationScreen::~MythUINotificationScreen()
{
}

/**
 * stringFromSeconds:
 *
 * Usage: stringFromSeconds(seconds)
 * Description: create a string in the format HH:mm:ss from a duration in seconds
 * HH: will not be displayed if there's less than one hour
 */
QString MythUINotificationScreen::stringFromSeconds(int time)
{
    int   hour    = time / 3600;
    int   minute  = (time - hour * 3600) / 60;
    int seconds   = time - hour * 3600 - minute * 60;
    QString str;

    if (hour)
    {
        str += QString("%1:").arg(hour);
    }
    if (minute < 10)
    {
        str += "0";
    }
    str += QString("%1:").arg(minute);
    if (seconds < 10)
    {
        str += "0";
    }
    str += QString::number(seconds);
    return str;
}

void MythUINotificationScreen::SetNotification(MythNotification &notification)
{
    bool update = notification.type() == MythNotification::Update;
    m_update = kForce;

    MythImageNotification *img =
        dynamic_cast<MythImageNotification*>(&notification);
    if (img)
    {
        UpdateArtwork(img->GetImage());
        m_update |= kImage;
        if (m_artworkImage)
        {
            m_artworkImage->SetVisible(true);
        }
    }
    else if (!update)
    {
        if (m_artworkImage)
        {
            m_artworkImage->SetVisible(false);
        }
    }
    MythPlaybackNotification *play =
        dynamic_cast<MythPlaybackNotification*>(&notification);
    if (play)
    {
        UpdatePlayback(play->GetDuration(), play->GetPosition());
        m_update |= kDuration;
        if (m_progressBar)
        {
            m_progressBar->SetVisible(true);
        }
        if (m_timeText)
        {
            m_timeText->SetVisible(true);
        }
    }
    else if (!update)
    {
        if (m_progressBar)
        {
            m_progressBar->SetVisible(false);
        }
        if (m_timeText)
        {
            m_timeText->SetVisible(false);
        }
    }
    if (update)
    {
        m_update |= kMetaData;
    }
    UpdateMetaData(notification.GetMetaData());
}

bool MythUINotificationScreen::Create(void)
{
    bool foundtheme = false;

    // Load the theme for this screen
    // The xml file containing the screen definition is airplay-ui.xml in this
    // example, the name of the screen in the xml is airplaypicture. This
    // should make sense when you look at the xml below
    foundtheme = LoadWindowFromXML("music-ui.xml", "miniplayer", this);

    if (!foundtheme) // If we cannot load the theme for any reason ...
        return false;

    // The xml should contain an <imagetype> named 'coverart', if it doesn't
    // then we cannot display the image and may as well abort
    m_artworkImage = dynamic_cast<MythUIImage*>(GetChild("coverart"));
    if (!m_artworkImage)
        return false;
    m_artworkImage->SetVisible(false);
    m_titleText     = dynamic_cast<MythUIText*>(GetChild("title"));
    m_artistText    = dynamic_cast<MythUIText*>(GetChild("artist"));
    m_albumText     = dynamic_cast<MythUIText*>(GetChild("album"));
    m_formatText    = dynamic_cast<MythUIText*>(GetChild("info"));
    m_timeText      = dynamic_cast<MythUIText*>(GetChild("time"));
    m_timeText->SetVisible(false);
    m_progressBar   = dynamic_cast<MythUIProgressBar*>(GetChild("progress"));
    m_progressBar->SetVisible(false);
    return true;
}

/**
 * Update the various fields of a MythUINotificationScreen
 * If metadata update flag is set; a Null string means to leave the text field
 * unchanged
 */
void MythUINotificationScreen::Init(void)
{
    if (m_artworkImage && (m_update & kImage))
    {
        if (!m_image.isNull())
        {
            MythImage *img = new MythImage(m_artworkImage->GetPainter());
            img->Assign(m_image);
            m_artworkImage->SetImage(img);
            img->DecrRef();
        }
        else
        {
            // Will default to displaying whatever placeholder image is defined
            // in the xml by the themer, means we can show _something_ rather than
            // a big empty hole. Generally you always want to call Reset() in
            // these circumstances
            m_artworkImage->Reset();
        }
    }

    if (m_titleText && !(m_title.isNull() && (m_update & kMetaData)))
    {
        if (!m_title.isNull())
        {
            m_titleText->SetText(m_title);
        }
        else
        {
            // Same as above, calling Reset() allows for a sane, themer defined
            //default to be displayed
            m_titleText->Reset();
        }
    }

    if (m_artistText && !(m_artist.isNull() && (m_update & kMetaData)))
    {
        if (!m_artist.isNull())
        {
            m_artistText->SetText(m_artist);
        }
        else
        {
            // Same as above, calling Reset() allows for a sane, themer defined
            //default to be displayed
            m_artistText->Reset();
        }
    }

    if (m_albumText && !(m_album.isNull() && (m_update & kMetaData)))
    {
        if (!m_album.isNull())
        {
            m_albumText->SetText(m_album);
        }
        else
        {
            // Same as above, calling Reset() allows for a sane, themer defined
            //default to be displayed
            m_albumText->Reset();
        }
    }

    if (m_formatText && !(m_title.isNull() && (m_update & kMetaData)))
    {
        if (!m_format.isNull())
        {
            m_formatText->SetText(m_format);
        }
        else
        {
            // Same as above, calling Reset() allows for a sane, themer defined
            //default to be displayed
            m_formatText->Reset();
        }
    }

    if (m_timeText && (m_update & kDuration))
    {
        if (m_duration >= 0)
        {
            m_timeText->SetText(stringFromSeconds(m_duration));
        }
        else
        {
            // Same as above, calling Reset() allows for a sane, themer defined
            //default to be displayed
            m_timeText->Reset();
        }
    }

    if (m_progressBar && (m_update & kDuration))
    {
        if (m_duration >= 0 && m_position >= 0)
        {
            m_progressBar->SetStart(0);
            m_progressBar->SetTotal(m_duration);
            m_progressBar->SetUsed(m_position);
        }
        else
        {
            // Same as above, calling Reset() allows for a sane, themer defined
            //default to be displayed
            m_progressBar->Reset();
        }
    }

    // All field will be refreshed the next time unless specified otherwise
    m_update = kForce;

    if (!m_added)
    {
        GetScreenStack()->AddScreen(this);
        m_added = true;
    }
}

/**
 * Update artwork image
 * must call Init() for screen to be updated
 */
void MythUINotificationScreen::UpdateArtwork(const QImage &image)
{
    m_image = image;
}

/**
 * Read some DMAP tag to extract title, artist, album and file format
 * must call Init() for screen to be updated
 * If metadata update flag is set; a Null string means to leave the text field
 * unchanged
 */
void MythUINotificationScreen::UpdateMetaData(const DMAP &data)
{
    QString tmp;

    tmp = data["minm"];
    if (!(tmp.isNull() && (m_update & kMetaData)))
    {
        m_title = tmp;
    }
    tmp = data["asar"];
    if (!(tmp.isNull() && (m_update & kMetaData)))
    {
        m_artist = tmp;
    }
    tmp = data["asal"];
    if (!(tmp.isNull() && (m_update & kMetaData)))
    {
        m_album = tmp;
    }
    tmp = data["asfm"];
    if (!(tmp.isNull() && (m_update & kMetaData)))
    {
        m_format = tmp;
    }
}

/**
 * Update playback position information
 * must call Init() for screen to be updated
 */
void MythUINotificationScreen::UpdatePlayback(int duration, int position)
{
    m_duration  = duration;
    m_position  = position;
}

/////////////////////// MythUINotificationCenter

MythUINotificationCenter *MythUINotificationCenter::g_singleton = NULL;

int MythUINotificationCenter::m_currentId = 0;
QMutex MythUINotificationCenter::m_lock(QMutex::Recursive);

MythUINotificationCenter *MythUINotificationCenter::GetInstance(void)
{
    QMutexLocker lock(&m_lock);

    if (g_singleton)
        return g_singleton;

    const bool isGuiThread =
        QThread::currentThread() == QCoreApplication::instance()->thread();

    if (!isGuiThread)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Constructor not called from GUI thread");
    }
    else
    {
        g_singleton = new MythUINotificationCenter();
    }
    return g_singleton;
}

MythUINotificationCenter::MythUINotificationCenter() : m_screenStack(NULL)
{
    const bool isGuiThread =
        QThread::currentThread() == QCoreApplication::instance()->thread();

    if (!isGuiThread)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Constructor not called from GUI thread");
    }
}

MythUINotificationCenter::~MythUINotificationCenter()
{
    const bool isGuiThread =
        QThread::currentThread() == QCoreApplication::instance()->thread();

    if (!isGuiThread)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Destructor not called from GUI thread");
    }

    QMutexLocker lock(&m_lock);

    DeleteAllScreens();

    // Delete all outstanding queued notifications
    foreach(MythNotification *n, m_notifications)
    {
        delete n;
    }
    m_notifications.clear();

    delete m_screenStack;
    m_screenStack = NULL;

    // if we're deleting global instancce; set g_singleton to 0
    // in practice this is always the case
    if (this == g_singleton)
    {
        g_singleton = NULL;
    }
}

/**
 * Remove screen from screens list
 * Qt slot called upong MythScreenType::Exiting()
 */
void MythUINotificationCenter::ScreenDeleted(void)
{
    MythUINotificationScreen *screen =
        static_cast<MythUINotificationScreen*>(sender());

    QMutexLocker lock(&m_lock);

    // search if an application had registered for it
    if (m_registrations.contains(screen->m_id))
    {
        // don't remove the id from the list, as the application is still registered
        m_registrations[screen->m_id] = NULL;
    }

    // Check that screen wasn't about to be deleted
    if (m_deletedScreens.contains(screen))
    {
        m_deletedScreens.removeAll(screen);
    }
}

bool MythUINotificationCenter::Queue(MythNotification &notification)
{
    QMutexLocker lock(&m_lock);

    // Just in case we got deleted half-way while GUI thread was quitting
    if (!g_singleton)
        return false;

    int id = notification.GetId();
    void *parent = notification.GetParent();

    MythNotification *tmp = static_cast<MythNotification*>(notification.clone());
    if (id > 0)
    {
        // quick sanity check to ensure the right caller is attempting
        // to register a notification
        if (!m_registrations.contains(id) || m_clients[id] != parent)
        {
            LOG(VB_GENERAL, LOG_DEBUG, LOC +
                QString("Queue: 0x%1, not registered for id (%2)")
                .arg((size_t)parent, QT_POINTER_SIZE, 16)
                .arg(id));
            id = -1;
        }
    }
    m_notifications.append(tmp);

    // Tell the GUI thread we have new notifications to process
    QCoreApplication::postEvent(
        GetMythMainWindow(), new MythUINotificationCenterEvent());

    return true;
}

void MythUINotificationCenter::ProcessQueue(void)
{
    QMutexLocker lock(&m_lock);

    const bool isGuiThread =
        QThread::currentThread() == QCoreApplication::instance()->thread();

    if (!isGuiThread)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "ProcessQueue not called from GUI thread");
        return;
    }

    DeleteAllScreens();

    foreach (MythNotification *n, m_notifications)
    {
        int id = n->GetId();
        MythUINotificationScreen *screen = NULL;

        if (id > 0)
        {
            screen = m_registrations[id];
        }
        if (!screen)
        {
            // We have a registration, but no screen. Create one and display it
            screen = CreateScreen(id);
            if (!screen) // Reads screen definition from xml, and constructs screen
                continue; // something is wrong ; ignore
            if (id > 0)
            {
                m_registrations[id] = screen;
            }
        }
        screen->SetNotification(*n);
        screen->doInit();
        delete n;
    }
    m_notifications.clear();
}

/**
 * CreateScreen will create a MythUINotificationScreen instance
 * This screen will not be displayed until it is used
 */
MythUINotificationScreen *MythUINotificationCenter::CreateScreen(int id)
{
    MythUINotificationScreen *screen =
        new MythUINotificationScreen(GetScreenStack(), id);

    if (!screen->Create()) // Reads screen definition from xml, and constructs screen
    {
        // If we can't create the screen then we can't display it, so delete
        // and abort
        delete screen;
        return NULL;
    }
    connect(screen, SIGNAL(Exiting()), this, SLOT(ScreenDeleted()));
    return screen;
}

int MythUINotificationCenter::Register(void *from)
{
    QMutexLocker lock(&m_lock);

    // Just in case we got deleted half-way while GUI thread was quitting
    if (!g_singleton)
        return false;

    if (!from)
        return -1;

    m_currentId++;
    m_registrations.insert(m_currentId, NULL);
    m_clients.insert(m_currentId, from);

    return m_currentId;
}

void MythUINotificationCenter::UnRegister(void *from, int id)
{
    QMutexLocker lock(&m_lock);

    // Just in case we got deleted half-way while GUI thread was quitting
    if (!g_singleton)
        return;

    if (!m_registrations.contains(id))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("UnRegister: 0x%1, no such registration (%2)")
            .arg((size_t)from, QT_POINTER_SIZE, 16)
            .arg(id));
        return;
    }

    if (m_clients[id] != from)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("UnRegister: 0x%1, not registered for id (%2")
            .arg((size_t)from, QT_POINTER_SIZE, 16)
            .arg(id));
    }

    MythUINotificationScreen *screen = m_registrations[id];
    if (screen != NULL)
    {
        // mark the screen for deletion
        m_deletedScreens.append(screen);
    }
    m_registrations.remove(id);
    m_clients.remove(id);

    // Tell the GUI thread we have something to process
    QCoreApplication::postEvent(
        GetMythMainWindow(), new MythUINotificationCenterEvent());
}

MythScreenStack *MythUINotificationCenter::GetScreenStack(void)
{
    if (!m_screenStack)
    {
        m_screenStack = new MythScreenStack(GetMythMainWindow(),
                                            "mythuinotification");
    }
    return m_screenStack;
}

void MythUINotificationCenter::DeleteAllScreens(void)
{
    // delete all screens waiting to be deleted
    while(!m_deletedScreens.isEmpty())
    {
        MythUINotificationScreen *screen = m_deletedScreens.last();
        // we remove the screen from the list before deleting the screen
        // so the MythScreenType::Exiting() signal won't process it a second time
        m_deletedScreens.removeLast();
        screen->GetScreenStack()->PopScreen(screen, true, true);
    }
}
