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
#include <QTimer>

#include "mythcorecontext.h"

#include "mythuinotificationcenter.h"
#include "mythuinotificationcenter_private.h"

#include "mythpainter.h"
#include "mythscreenstack.h"
#include "mythscreentype.h"
#include "mythuiimage.h"
#include "mythuitext.h"
#include "mythuiprogressbar.h"
#include "mythdate.h"

#define LOC QString("NotificationCenter: ")

#define HGAP 5
#define DEFAULT_DURATION 5000  // in ms

//// MythUINotificationCenterEvent

QEvent::Type MythUINotificationCenterEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

//// class MythUINotificationScreen

MythUINotificationScreen::MythUINotificationScreen(MythScreenStack *stack,
                                                   int id)
    : MythScreenType(stack, "mythuinotification"),  m_id(id),
      m_duration(-1),           m_progress(-1.0),   m_fullscreen(false),
      m_added(false),
      m_created(false),         m_content(kNone),   m_update(kAll),
      m_artworkImage(NULL),     m_titleText(NULL),  m_originText(NULL),
      m_descriptionText(NULL),  m_extraText(NULL),  m_progresstextText(NULL),
      m_progressBar(NULL),      m_index(0),         m_timer(new QTimer(this))
{
    // Set timer if need be
    SetSingleShotTimer(m_duration);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(ProcessTimer()));
}

MythUINotificationScreen::MythUINotificationScreen(MythScreenStack *stack,
                                                   MythNotification &notification)
    : MythScreenType(stack, "mythuinotification"),  m_id(notification.GetId()),
      m_duration(notification.GetDuration()),       m_progress(-1.0),
      m_fullscreen(false),
      m_added(false),           m_created(false),   m_content(kNone),
      m_update(kAll),
      m_artworkImage(NULL),     m_titleText(NULL),  m_originText(NULL),
      m_descriptionText(NULL),  m_extraText(NULL),  m_progresstextText(NULL),
      m_progressBar(NULL),      m_index(0),         m_timer(new QTimer(this))
{
    SetNotification(notification);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(ProcessTimer()));
}

MythUINotificationScreen::MythUINotificationScreen(MythScreenStack *stack,
                                                   const MythUINotificationScreen &s)
    : MythScreenType(stack, "mythuinotification"),
      m_duration(-1),           m_progress(-1.0),   m_fullscreen(false),
      m_added(false),
      m_created(false),         m_content(kNone),   m_update(kAll),
      m_artworkImage(NULL),     m_titleText(NULL),  m_originText(NULL),
      m_descriptionText(NULL),  m_extraText(NULL),  m_progresstextText(NULL),
      m_progressBar(NULL),      m_timer(new QTimer(this))
{
    *this = s;
    connect(m_timer, SIGNAL(timeout()), this, SLOT(ProcessTimer()));
}

MythUINotificationScreen::~MythUINotificationScreen()
{
    m_timer->stop();
    LOG(VB_GUI, LOG_DEBUG, LOC + "MythUINotificationScreen dtor");
    // We can't rely on Exiting() default MythScreenType signal as
    // by the time it is emitted, the destructor would have already been called
    // making the members unusable
    emit ScreenDeleted();
}

void MythUINotificationScreen::SetNotification(MythNotification &notification)
{
    bool update = notification.type() == MythNotification::Update;
    m_update = kNone;

    MythImageNotification *img =
        dynamic_cast<MythImageNotification*>(&notification);
    if (img)
    {
        QString path = img->GetImagePath();

        m_update |= kImage;

        if (path.isNull())
        {
            UpdateArtwork(img->GetImage());
        }
        else
        {
            UpdateArtwork(path);
        }
    }

    MythPlaybackNotification *play =
        dynamic_cast<MythPlaybackNotification*>(&notification);
    if (play)
    {
        UpdatePlayback(play->GetProgress(), play->GetProgressText());

        m_update |= kDuration;
    }

    if (!notification.GetMetaData().isEmpty())
    {
        UpdateMetaData(notification.GetMetaData());
        m_update  |= kMetaData;
    }
    else if (!update)
    {
        // A new notification, will always update the metadata field
        m_update |= kMetaData;
    }

    if (!notification.GetStyle().isEmpty())
    {
        m_style = notification.GetStyle();
        m_update |= kStyle;
    }

    if (!update)
    {
        m_content = m_update;
        m_fullscreen = notification.GetFullScreen();
    }

    m_duration      = notification.GetDuration();
    m_visibility    = notification.GetVisibility();
    m_priority      = notification.GetPriority();

    // Set timer if need be
    SetSingleShotTimer(m_duration);
}

bool MythUINotificationScreen::Create(void)
{
    bool foundtheme = false;

    // Load the theme for this screen
    // The xml file containing the screen definition is airplay-ui.xml in this
    // example, the name of the screen in the xml is airplaypicture. This
    // should make sense when you look at the xml below

    QString theme;
    if (m_fullscreen)
    {
        theme = "notification-full";
    }
    else if (m_content & kImage)
    {
        theme = "notification-image";
    }
    else
    {
        theme = "notification";
    }

    QString theme_attempt = theme + (m_style.isEmpty() ? "" : "-" + m_style);

    // See if we have an alternative theme available as defined in the notification
    foundtheme = LoadWindowFromXML("notification-ui.xml", theme_attempt, this);
    if (!foundtheme && theme_attempt != theme)
    {
        // if not, default to the main one
        foundtheme = LoadWindowFromXML("notification-ui.xml", theme, this);
    }

    if (!foundtheme) // If we cannot load the theme for any reason ...
        return false;

    // The xml should contain an <imagetype> named 'coverart', if it doesn't
    // then we cannot display the image and may as well abort
    m_artworkImage      = dynamic_cast<MythUIImage*>(GetChild("image"));
    m_titleText         = dynamic_cast<MythUIText*>(GetChild("title"));
    m_originText        = dynamic_cast<MythUIText*>(GetChild("origin"));
    m_descriptionText   = dynamic_cast<MythUIText*>(GetChild("description"));
    m_extraText         = dynamic_cast<MythUIText*>(GetChild("extra"));
    m_progresstextText  = dynamic_cast<MythUIText*>(GetChild("progress_text"));
    m_progressBar       = dynamic_cast<MythUIProgressBar*>(GetChild("progress"));

    // store original position
    m_position      = GetPosition();
    m_created = true;

    if ((m_visibility & ~MythNotification::kPlayback) == 0)
    {
        // Visibility will be set automatically during video playback
        // so can be ignored here
        SetVisible(false);
    }
    return true;
}

/**
 * Update the various fields of a MythUINotificationScreen.
 */
void MythUINotificationScreen::Init(void)
{
    AdjustYPosition();

    if (m_artworkImage && (m_update & kImage))
    {
        if (!m_imagePath.isNull())
        {
            // We have a path to the image, use it
            m_artworkImage->SetFilename(m_imagePath);
            m_artworkImage->Load();
        }
        else if (!m_image.isNull())
        {
            // We don't have a path to the image, but the image itself
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

    if (m_update != kNone)
    {
        InfoMap tmap;

        tmap["title"]               = m_title;
        if (m_update & kImage)
        {
            tmap["image"]           = m_imagePath;
        }
        tmap["origin"]              = m_origin;
        tmap["description"]         = m_description;
        tmap["extra"]               = m_extra;
        if (m_update & kDuration)
        {
            tmap["progress_text"]   = m_progresstext;
            tmap["progress"]        = QString("%1").arg((int)(m_progress * 100));
        }
        SetTextFromMap(tmap);
    }

    if (m_update & kMetaData)
    {
        if (m_titleText && m_title.isNull())
        {
            m_titleText->Reset();
        }
        if (m_originText && m_origin.isNull())
        {
            m_originText->Reset();
        }
        if (m_descriptionText && m_description.isNull())
        {
            m_descriptionText->Reset();
        }
        if (m_extraText && m_extra.isNull())
        {
            m_extraText->Reset();
        }
    }

    if (m_update & kDuration)
    {
        if (m_progresstextText && m_progresstext.isEmpty())
        {
            m_progresstextText->Reset();
        }
        if (m_progressBar)
        {
            if (m_progress >= 0)
            {
                m_progressBar->SetStart(0);
                m_progressBar->SetTotal(100);
                m_progressBar->SetUsed(100 * m_progress);
            }
            else
            {
                // Same as above, calling Reset() allows for a sane, themer defined
                //default to be displayed
                m_progressBar->Reset();
            }
        }
    }
    if (m_progressBar)
    {
        m_progressBar->SetVisible((m_content & kDuration) != 0);

    }
    if (m_progresstextText)
    {
        m_progresstextText->SetVisible((m_content & kDuration) != 0);
    }

    // No field will be refreshed the next time unless specified otherwise
    m_update = kNone;

    if (GetScreenStack() && !m_added)
    {
        GetScreenStack()->AddScreen(this);
        m_added = true;
    }
}

/**
 * Update artwork image.
 * must call Init() for screen to be updated.
 */
void MythUINotificationScreen::UpdateArtwork(const QImage &image)
{
    m_image = image;
}

/**
 * Update artwork image via URL or file path.
 * must call Init() for screen to be updated.
 */
void MythUINotificationScreen::UpdateArtwork(const QString &image)
{
    m_imagePath = image;
}

/**
 * Read some DMAP tag to extract title, artist, album and file format.
 * must call Init() for screen to be updated.
 * If metadata update flag is set; a Null string means to leave the text field
 * unchanged.
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
        m_origin = tmp;
    }
    tmp = data["asal"];
    if (!(tmp.isNull() && (m_update & kMetaData)))
    {
        m_description = tmp;
    }
    tmp = data["asfm"];
    if (!(tmp.isNull() && (m_update & kMetaData)))
    {
        m_extra = tmp;
    }
}

/**
 * Update playback position information.
 * must call Init() for screen to be updated.
 */
void MythUINotificationScreen::UpdatePlayback(float progress, const QString &text)
{
    m_progress      = progress;
    m_progresstext  = text;
}

/**
 * Update Y position of the screen
 * All children elements will be relocated.
 */
void MythUINotificationScreen::AdjustYPosition(void)
{
    MythPoint point = m_position;
    point.setY(m_position.getY().toInt() + (GetHeight() + HGAP) * m_index);

    if (point == GetPosition())
        return;

    SetPosition(point);
}

void MythUINotificationScreen::AdjustIndex(int by, bool set)
{
    if (set)
    {
        m_index = by;
    }
    else
    {
        m_index += by;
    }
    AdjustYPosition();
}

int MythUINotificationScreen::GetHeight(void)
{
    return GetArea().getHeight().toInt();
}

void MythUINotificationScreen::ProcessTimer(void)
{
    LOG(VB_GUI, LOG_DEBUG, LOC + "ProcessTimer()");
    // delete screen
    GetScreenStack()->PopScreen(this, true, true);
}

MythUINotificationScreen &MythUINotificationScreen::operator=(const MythUINotificationScreen &s)
{
    m_id            = s.m_id;
    m_image         = s.m_image;
    m_imagePath     = s.m_imagePath;
    m_title         = s.m_title;
    m_origin        = s.m_origin;
    m_description         = s.m_description;
    m_extra        = s.m_extra;
    m_duration      = s.m_duration;
    m_progress      = s.m_progress;
    m_progresstext  = s.m_progresstext;
    m_content       = s.m_content;
    m_fullscreen    = s.m_fullscreen;
    m_expiry        = s.m_expiry;
    m_index         = s.m_index;
    m_style         = s.m_style;
    m_visibility    = s.m_visibility;
    m_priority      = s.m_priority;

    m_update = kAll; // so all fields are initialised regardless of notification type

    return *this;
}

void MythUINotificationScreen::SetSingleShotTimer(int s)
{
    // only registered application can display non-expiring notification
    if (m_id > 0 && s < 0)
        return;

    int ms = s * 1000;
    ms = ms <= DEFAULT_DURATION ? DEFAULT_DURATION : ms;

    m_expiry = MythDate::current().addMSecs(ms);

    m_timer->stop();
    m_timer->setSingleShot(true);
    m_timer->start(ms);
}


/////////////////////// NCPrivate

NCPrivate::NCPrivate()
    : m_currentId(0)
{
    m_screenStack = new MythNotificationScreenStack(GetMythMainWindow(),
                                                    "mythuinotification",
                                                    this);
}

NCPrivate::~NCPrivate()
{
    const bool isGuiThread =
        QThread::currentThread() == QCoreApplication::instance()->thread();

    if (!isGuiThread)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Destructor not called from GUI thread");
    }

    QMutexLocker lock(&m_lock);

    DeleteAllRegistrations();
    DeleteAllScreens();

    // Delete all outstanding queued notifications
    foreach(MythNotification *n, m_notifications)
    {
        delete n;
    }
    m_notifications.clear();

    delete m_screenStack;
    m_screenStack = NULL;
}

/**
 * Remove screen from screens list.
 * Qt slot called upon MythScreenType::Exiting().
 */
void NCPrivate::ScreenDeleted(void)
{
    MythUINotificationScreen *screen =
        static_cast<MythUINotificationScreen*>(sender());

    bool duefordeletion = m_deletedScreens.contains(screen);

    LOG(VB_GUI, LOG_DEBUG, LOC +
        QString("ScreenDeleted: Entering (%1)").arg(duefordeletion));
    // Check that screen wasn't about to be deleted
    if (duefordeletion)
    {
        m_deletedScreens.removeAll(screen);
    }

    int n = m_screens.indexOf(screen);
    if (n >= 0)
    {
        m_screens.removeAll(screen);
        RefreshScreenPosition();
    }
    else
    {
        LOG(VB_GUI, LOG_DEBUG, LOC +
            QString("Screen[%1] not found in screens list").arg(screen->m_id));
    }

    // remove the converted equivalent screen if any
    if (m_converted.contains(screen))
    {
        delete m_converted[screen];
    }
    m_converted.remove(screen);

    // search if an application had registered for it
    if (m_registrations.contains(screen->m_id))
    {
        if (!duefordeletion)
        {
            if (!m_screenStack)
            {
                // we're in the middle of being deleted
                m_registrations.remove(screen->m_id);
            }
            else
            {
                // don't remove the id from the list, as the application is still registered
                // re-create the screen
                MythUINotificationScreen *newscreen =
                    new MythUINotificationScreen(m_screenStack, *screen);
                connect(newscreen, SIGNAL(ScreenDeleted()), this, SLOT(ScreenDeleted()));
                m_registrations[screen->m_id] = newscreen;
                // Screen was deleted, add it to suspended list
                m_suspended.append(screen->m_id);
                LOG(VB_GUI, LOG_DEBUG, LOC +
                    "ScreenDeleted: Suspending registered screen");
            }
        }
        else
        {
            LOG(VB_GUI, LOG_DEBUG, LOC +
                "ScreenDeleted: Deleting registered screen");
        }
    }
}

void NCPrivate::ScreenStackDeleted(void)
{
    m_screenStack = NULL;
}

bool NCPrivate::Queue(MythNotification &notification)
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
        else
        {
            // check if notification card has been suspended, in which case
            // refuse all notification updates
            if (m_suspended.contains(id))
            {
                if (notification.type() == MythNotification::Update)
                {
                    delete tmp;
                    return false;
                }
                // got something else than an update, remove it from the
                // suspended list
                m_suspended.removeAll(id);
            }
        }
    }
    m_notifications.append(tmp);

    // Tell the GUI thread we have new notifications to process
    QCoreApplication::postEvent(
        GetMythMainWindow(), new MythUINotificationCenterEvent());

    return true;
}

void NCPrivate::ProcessQueue(void)
{
    QMutexLocker lock(&m_lock);

    DeleteAllScreens();

    foreach (MythNotification *n, m_notifications)
    {
        int id = n->GetId();
        bool created = false;
        MythUINotificationScreen *screen = NULL;

        if (id > 0)
        {
            screen = m_registrations[id];
        }
        if (!screen)
        {
            // We have a registration, but no screen. Create one and display it
            screen = CreateScreen(n);
            if (!screen) // Reads screen definition from xml, and constructs screen
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("ProcessQueue: couldn't create required screen"));
                continue; // something is wrong ; ignore
            }
            if (id > 0)
            {
                m_registrations[id] = screen;
            }
            created = true;
        }
        else
        {
            screen->SetNotification(*n);
        }

        // if the screen got allocated, but did't read theme yet, do it now
        if (screen && !screen->m_created)
        {
            if (!screen->Create())
            {
                delete screen;
                continue;
            }
            created = true;
        }

        if (created || !m_screens.contains(screen))
        {
            int pos = InsertScreen(screen);
            // adjust vertical positions
            RefreshScreenPosition(pos);
        }

        screen->doInit();
        delete n;
    }
    m_notifications.clear();
}

/**
 * CreateScreen will create a MythUINotificationScreen instance.
 * This screen will not be displayed until it is used.
 */
MythUINotificationScreen *NCPrivate::CreateScreen(MythNotification *n, int id)
{
    MythUINotificationScreen *screen;

    if (n)
    {
        screen = new MythUINotificationScreen(m_screenStack, *n);
    }
    else
    {
        screen = new MythUINotificationScreen(m_screenStack, id);
    }

    if (!screen->Create()) // Reads screen definition from xml, and constructs screen
    {
        // If we can't create the screen then we can't display it, so delete
        // and abort
        delete screen;
        return NULL;
    }
    connect(screen, SIGNAL(ScreenDeleted()), this, SLOT(ScreenDeleted()));
    return screen;
}

int NCPrivate::Register(void *from)
{
    QMutexLocker lock(&m_lock);

    if (!from)
        return -1;

    m_currentId++;
    m_registrations.insert(m_currentId, NULL);
    m_clients.insert(m_currentId, from);

    return m_currentId;
}

void NCPrivate::UnRegister(void *from, int id, bool closeimemdiately)
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

    if (m_registrations.contains(id))
    {
        MythUINotificationScreen *screen = m_registrations[id];
        if (screen != NULL)
        {
            // mark the screen for deletion if no timer is set
            if (screen->m_duration <= 0 || closeimemdiately)
            {
                m_deletedScreens.append(screen);
            }
        }
        m_registrations.remove(id);
    }
    m_clients.remove(id);

    // Tell the GUI thread we have something to process
    QCoreApplication::postEvent(
        GetMythMainWindow(), new MythUINotificationCenterEvent());
}

void NCPrivate::DeleteAllRegistrations(void)
{
    QMap<int, MythUINotificationScreen*>::iterator it = m_registrations.begin();

    for (; it != m_registrations.end();)
    {
        m_deletedScreens.append(*it);
        it = m_registrations.erase(it);
    }
    m_registrations.clear();
}

void NCPrivate::DeleteAllScreens(void)
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

/**
 * Insert screen into list of screens.
 * Returns index in screens list.
 */
int NCPrivate::InsertScreen(MythUINotificationScreen *screen)
{
    QList<MythUINotificationScreen*>::iterator it       = m_screens.begin();
    QList<MythUINotificationScreen*>::iterator itend    = m_screens.end();

//    if (screen->m_id > 0)
//    {
//        // we want a permanent screen; add it after the existing one
//        for (; it != itend; ++it)
//        {
//            if ((*it)->m_id <= 0 ||
//                (*it)->m_id > screen->m_id)
//                break; // reached the temporary screens
//        }
//        // it points to where we want to insert item
//    }
//    else
    {
        it = itend;
    }
    it = m_screens.insert(it, screen);

    return it - m_screens.begin();
}

/**
 * Remove screen from list of screens.
 * Returns index in screens list.
 */
int NCPrivate::RemoveScreen(MythUINotificationScreen *screen)
{
    QList<MythUINotificationScreen*>::iterator it       = m_screens.begin();
    QList<MythUINotificationScreen*>::iterator itend    = m_screens.end();

    for (; it != itend; ++it)
    {
        if (*it == screen)
            break;
    }

    if (it != itend)
    {
        it = m_screens.erase(it);
    }

    return it - m_screens.begin();
}

/**
 * Re-position screens on display.
 */
void NCPrivate::RefreshScreenPosition(int from)
{
    QList<MythUINotificationScreen*>::iterator it       = m_screens.begin();
    QList<MythUINotificationScreen*>::iterator itend    = m_screens.end();

    it += from;
    int position = 0;

    if (from > 0)
    {
        position = (*(it-1))->m_fullscreen ? 0 : (*(it-1))->m_index+1;
    }

    for (; it != itend; ++it)
    {
        if ((*it)->IsVisible())
        {
            (*it)->AdjustIndex(position++, true);
        }
        if ((*it)->m_fullscreen)
        {
            position = 0;
            continue;
        }
    }
}

void NCPrivate::GetNotificationScreens(QList<MythScreenType*> &_screens)
{
    QList<MythScreenType*> list;
    QVector<MythScreenType*> screens;

    if (!m_screenStack)
        return;

    m_screenStack->CheckDeletes();

    QMutexLocker lock(&m_lock);

    m_screenStack->GetScreenList(screens);

    QVector<MythScreenType*>::const_iterator it       = screens.begin();
    QVector<MythScreenType*>::const_iterator itend    = screens.end();

    int position = 0;
    for (; it != itend; ++it)
    {
        MythUINotificationScreen *screen =
            dynamic_cast<MythUINotificationScreen*>(*it);

        if (screen)
        {
            if ((screen->m_visibility & MythNotification::kPlayback) == 0)
                continue;

            MythUINotificationScreen *newscreen;

            if (!m_converted.contains(screen))
            {
                // screen hasn't been created, return it
                newscreen = new MythUINotificationScreen(NULL, *screen);
                // CreateScreen can never fail, no need to test return value
                m_converted[screen] = newscreen;
            }
            else
            {
                newscreen = m_converted[screen];
                // Copy old content in case it changed
                *newscreen = *screen;
            }
            newscreen->SetVisible(true);
            newscreen->m_index = position++;
            if (screen->m_fullscreen)
            {
                position = 0;
            }
            list.append(newscreen);
        }
        else
        {
            list.append(*it);
        }
    }
    _screens = list;
}

/////////////////////// MythUINotificationCenter

MythUINotificationCenter *MythUINotificationCenter::g_singleton = NULL;
QMutex MythUINotificationCenter::g_lock;

MythUINotificationCenter *MythUINotificationCenter::GetInstance(void)
{
    QMutexLocker lock(&g_lock);

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

MythUINotificationCenter::MythUINotificationCenter()
    : d(new NCPrivate())
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

    delete d;
    d = NULL;

    // if we're deleting global instancce; set g_singleton to 0
    // in practice this is always the case
    if (this == g_singleton)
    {
        g_singleton = NULL;
    }
}

bool MythUINotificationCenter::Queue(MythNotification &notification)
{
    return d->Queue(notification);
}

void MythUINotificationCenter::ProcessQueue(void)
{
    const bool isGuiThread =
        QThread::currentThread() == QCoreApplication::instance()->thread();

    if (!isGuiThread)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "ProcessQueue not called from GUI thread");
        return;
    }

    d->ProcessQueue();
}

int MythUINotificationCenter::Register(void *from)
{
    // Just in case we got deleted half-way while GUI thread was quitting
    if (!g_singleton)
        return false;

    return d->Register(from);
}

void MythUINotificationCenter::UnRegister(void *from, int id, bool closeimemdiately)
{
    // Just in case we got deleted half-way while GUI thread was quitting
    if (!g_singleton)
        return;

    d->UnRegister(from, id, closeimemdiately);
}

QDateTime MythUINotificationCenter::ScreenExpiryTime(MythScreenType *screen)
{
    MythUINotificationScreen *s =
        dynamic_cast<MythUINotificationScreen*>(screen);

    if (!s)
        return QDateTime();
    return s->m_expiry;
}

bool MythUINotificationCenter::ScreenCreated(MythScreenType *screen)
{
    MythUINotificationScreen *s =
        dynamic_cast<MythUINotificationScreen*>(screen);

    if (!s)
        return true;;
    return s->m_created;
}

void MythUINotificationCenter::GetNotificationScreens(QList<MythScreenType*> &_screens)
{
    d->GetNotificationScreens(_screens);
}

void MythUINotificationCenter::UpdateScreen(MythScreenType *screen)
{
    MythUINotificationScreen *s =
        dynamic_cast<MythUINotificationScreen*>(screen);

    if (!s)
        return;

    if (s->m_created)
    {
        s->doInit();
    }
}

void ShowNotificationError(const QString &msg,
                           const QString &from,
                           const QString &detail,
                           const PNMask priority,
                           const VNMask visibility)
{
    MythErrorNotification n(msg, from, detail);
    n.SetPriority(priority);
    n.SetVisibility(visibility);

    MythUINotificationCenter::GetInstance()->Queue(n);
}

void ShowNotification(const QString &msg,
                      const QString &from,
                      const QString &detail,
                      const PNMask priority,
                      const VNMask visibility)
{
    MythNotification n(msg, from, detail);
    n.SetPriority(priority);
    n.SetVisibility(visibility);

    MythUINotificationCenter::GetInstance()->Queue(n);
}
