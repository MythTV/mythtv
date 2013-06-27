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

    int             m_id;
    QImage          m_image;
    QString         m_title;
    QString         m_artist;
    QString         m_album;
    QString         m_format;
    MythUIImage    *m_artworkImage;
    MythUIText     *m_titleText;
    MythUIText     *m_artistText;
    MythUIText     *m_albumText;
    MythUIText     *m_formatText;
    bool            m_added;
};

MythUINotificationScreen::MythUINotificationScreen(MythScreenStack *stack,
                                                   int id)
    : MythScreenType(stack, "mythuinotification"), m_id(id), m_added(false)
{
}

MythUINotificationScreen::~MythUINotificationScreen()
{
}

void MythUINotificationScreen::SetNotification(MythNotification &notification)
{
    if (dynamic_cast<MythImageNotification*>(&notification))
    {
        UpdateArtwork(static_cast<MythImageNotification*>(&notification)->GetImage());
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

    m_titleText     = dynamic_cast<MythUIText*>(GetChild("title"));
    m_artistText    = dynamic_cast<MythUIText*>(GetChild("artist"));
    m_albumText     = dynamic_cast<MythUIText*>(GetChild("album"));
    m_formatText    = dynamic_cast<MythUIText*>(GetChild("info"));
    return true;
}

/**
 * Update the various fields of a MythUINotificationScreen
 */
void MythUINotificationScreen::Init(void)
{
    if (m_artworkImage)
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

    if (m_titleText)
    {
        if (!m_title.isEmpty())
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

    if (m_artistText)
    {
        if (!m_artist.isEmpty())
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

    if (m_albumText)
    {
        if (!m_album.isEmpty())
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

    if (m_formatText)
    {
        if (!m_format.isEmpty())
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
 */
void MythUINotificationScreen::UpdateMetaData(const DMAP &data)
{
    m_title     = data["minm"];
    m_artist    = data["asar"];
    m_album     = data["asal"];
    m_format    = data["asfm"];
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

    g_singleton = new MythUINotificationCenter();

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
    delete m_screenStack;
    m_screenStack = NULL;
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

    // delete all screens waiting to be deleted
    while(!m_deletedScreens.isEmpty())
    {
        MythUINotificationScreen *screen = m_deletedScreens.last();
        // we remove the screen from the list before deleting the screen
        // so the MythScreenType::Exiting() signal won't process it a second time
        m_deletedScreens.removeLast();
        screen->GetScreenStack()->PopScreen(screen, true, true);
    }
    m_deletedScreens.clear();

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
        screen->Init();
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
