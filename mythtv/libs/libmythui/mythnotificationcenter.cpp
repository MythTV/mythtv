//
//  mythnotificationcenter.cpp
//  MythTV
//
//  Created by Jean-Yves Avenard on 25/06/13.
//  Copyright (c) 2013 Bubblestuff Pty Ltd. All rights reserved.
//

// Qt headers
#include <QCoreApplication>
#include <QEvent>
#include <QObject>
#include <QThread>
#include <QTimer>

// MythTV headers
#include "mythcorecontext.h"
#include "mythmainwindow.h"

#include "mythnotificationcenter.h"
#include "mythnotificationcenter_private.h"

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

//// MythNotificationCenterEvent

QEvent::Type MythNotificationCenterEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

//// class MythNotificationScreenStack

void MythNotificationScreenStack::PopScreen(MythScreenType *screen, bool allowFade,
                                            bool deleteScreen)
{
    if (!screen || screen->IsDeleting())
        return;

    bool poppedFullscreen = screen->IsFullscreen();

    screen->aboutToHide();

    if (m_Children.isEmpty())
        return;

    MythMainWindow *mainwindow = GetMythMainWindow();

    screen->setParent(0);
    if (allowFade && m_DoTransitions && !mainwindow->IsExitingToMain())
    {
        screen->SetFullscreen(false);
        if (deleteScreen)
        {
            screen->SetDeleting(true);
            m_ToDelete.push_back(screen);
        }
        screen->AdjustAlpha(1, -kFadeVal);
    }
    else
    {
        for (int i = 0; i < m_Children.size(); ++i)
        {
            if (m_Children.at(i) == screen)
            {
                m_Children.remove(i);
                break;
            }
        }
        if (deleteScreen)
            screen->deleteLater();

        screen = NULL;
    }

    m_topScreen = NULL;

    RecalculateDrawOrder();

    // If we're fading it, we still want to draw it.
    if (screen && !m_DrawOrder.contains(screen))
        m_DrawOrder.push_back(screen);

    if (!m_Children.isEmpty())
    {
        QVector<MythScreenType *>::Iterator it;
        for (it = m_DrawOrder.begin(); it != m_DrawOrder.end(); ++it)
        {
            if (*it != screen && !(*it)->IsDeleting())
            {
                m_topScreen = (*it);
                (*it)->SetAlpha(255);
                if (poppedFullscreen)
                    (*it)->aboutToShow();
            }
        }
    }

    if (m_topScreen)
    {
        m_topScreen->SetRedraw();
    }
    else
    {
        // Screen still needs to be redrawn if we have popped the last screen
        // off the popup stack, or similar
        if (mainwindow->GetMainStack())
        {
            MythScreenType *mainscreen = mainwindow->GetMainStack()->GetTopScreen();
            if (mainscreen)
                mainscreen->SetRedraw();
        }
    }
}

MythScreenType *MythNotificationScreenStack::GetTopScreen(void) const
{
    if (m_Children.isEmpty())
        return NULL;
    // The top screen is the only currently displayed first, if there's a
    // fullscreen notification displayed, it's the last one
    MythScreenType *top = m_Children.front();
    QVector<MythScreenType *>::const_iterator it = m_Children.end() - 1;

    // loop from last to 2nd
    for (; it != m_Children.begin(); --it)
    {
        MythNotificationScreen *s = dynamic_cast<MythNotificationScreen *>(*it);

        if (!s)
        {
            // if for whatever reason it's not a notification on our screen
            // it will be dropped as we don't know how it appears
            top = s;
        }
        if (s->m_fullscreen)
        {
            top = s;
            break;
        }
    }
    return top;
}

//// class MythNotificationScreen

MythNotificationScreen::MythNotificationScreen(MythScreenStack *stack,
                                                   int id)
    : MythScreenType(stack, "mythnotification"),    m_id(id),
      m_duration(-1),           m_progress(-1.0),   m_fullscreen(false),
      m_added(false),
      m_created(false),         m_content(kNone),   m_update(kAll),
      m_type(MythNotification::New),
      m_artworkImage(NULL),     m_titleText(NULL),  m_originText(NULL),
      m_descriptionText(NULL),  m_extraText(NULL),  m_progresstextText(NULL),
      m_progressBar(NULL),      m_errorState(NULL), m_mediaState(NULL),
      m_index(0),
      m_timer(new QTimer(this)),
      m_visibility(MythNotification::kAll),
      m_priority(MythNotification::kDefault),
      m_refresh(true)
{
    // Set timer if need be
    SetSingleShotTimer(m_duration);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(ProcessTimer()));
}

MythNotificationScreen::MythNotificationScreen(MythScreenStack *stack,
                                                   MythNotification &notification)
    : MythScreenType(stack, "mythnotification"),    m_id(notification.GetId()),
      m_duration(notification.GetDuration()),       m_progress(-1.0),
      m_fullscreen(false),
      m_added(false),           m_created(false),   m_content(kNone),
      m_update(kAll),           m_type(MythNotification::New),
      m_artworkImage(NULL),     m_titleText(NULL),  m_originText(NULL),
      m_descriptionText(NULL),  m_extraText(NULL),  m_progresstextText(NULL),
      m_progressBar(NULL),      m_errorState(NULL), m_mediaState(NULL),
      m_index(0),
      m_timer(new QTimer(this)),
      m_visibility(MythNotification::kAll),
      m_priority(MythNotification::kDefault),
      m_refresh(true)
{
    SetNotification(notification);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(ProcessTimer()));
}

MythNotificationScreen::MythNotificationScreen(MythScreenStack *stack,
                                                   const MythNotificationScreen &s)
    : MythScreenType(stack, "mythnotification"),
      m_duration(-1),           m_progress(-1.0),   m_fullscreen(false),
      m_added(false),
      m_created(false),         m_content(kNone),   m_update(kAll),
      m_type(MythNotification::New),
      m_artworkImage(NULL),     m_titleText(NULL),  m_originText(NULL),
      m_descriptionText(NULL),  m_extraText(NULL),  m_progresstextText(NULL),
      m_progressBar(NULL),      m_errorState(NULL), m_mediaState(NULL),
      m_timer(new QTimer(this)),
      m_visibility(MythNotification::kAll),
      m_priority(MythNotification::kDefault),
      m_refresh(true)
{
    *this = s;
    connect(m_timer, SIGNAL(timeout()), this, SLOT(ProcessTimer()));
}

MythNotificationScreen::~MythNotificationScreen()
{
    m_timer->stop();
    LOG(VB_GUI, LOG_DEBUG, LOC + "MythNotificationScreen dtor");
    // We can't rely on Exiting() default MythScreenType signal as
    // by the time it is emitted, the destructor would have already been called
    // making the members unusable
    emit ScreenDeleted();
}

void MythNotificationScreen::SetNotification(MythNotification &notification)
{
    bool update;
    m_update = kNone;

    m_type = notification.type();

    if (m_type == MythNotification::Error   ||
        m_type == MythNotification::Warning ||
        m_type == MythNotification::Check)
    {
        m_update |= kImage;
        update = false;
    }
    else if (m_type == MythNotification::Update)
    {
        update = true;
    }
    else
    {
        update = false;
    }

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

    MythMediaNotification *media =
        dynamic_cast<MythMediaNotification*>(&notification);
    if (media && m_imagePath.isEmpty() && m_image.isNull())
    {
        m_update |= kNoArtwork;
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
    if (!m_visibility)
    {
        // no visibility is all visibility to get around QVariant always making 0 the default
        m_visibility = ~0;
    }
    m_priority      = notification.GetPriority();

    // Set timer if need be
    SetSingleShotTimer(m_duration, update);

    // We need to re-run init
    m_refresh = true;
}

bool MythNotificationScreen::Create(void)
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

    m_artworkImage      = dynamic_cast<MythUIImage*>(GetChild("image"));
    m_titleText         = dynamic_cast<MythUIText*>(GetChild("title"));
    m_originText        = dynamic_cast<MythUIText*>(GetChild("origin"));
    m_descriptionText   = dynamic_cast<MythUIText*>(GetChild("description"));
    m_extraText         = dynamic_cast<MythUIText*>(GetChild("extra"));
    m_progresstextText  = dynamic_cast<MythUIText*>(GetChild("progress_text"));
    m_progressBar       = dynamic_cast<MythUIProgressBar*>(GetChild("progress"));
    m_errorState        = dynamic_cast<MythUIStateType*>(GetChild("errorstate"));
    m_mediaState        = dynamic_cast<MythUIStateType*>(GetChild("mediastate"));

    SetErrorState();

    if (m_mediaState && (m_update & kImage))
    {
        m_mediaState->DisplayState(m_content & kNoArtwork ? "noartwork" : "ok");
        LOG(VB_GUI, LOG_DEBUG, LOC + QString("Create: Set media state to %1").arg(m_content & kNoArtwork ? "noartwork" : "ok"));
    }

    // store original position
    m_position      = GetPosition();
    m_created = true;

    if ((m_visibility & ~MythNotification::kPlayback) == 0)
    {
        // Visibility will be set automatically during video playback
        // so can be ignored here
        SetVisible(false);
    }

    // We need to re-run init
    m_refresh = true;

    return true;
}

/**
 * Update the various fields of a MythNotificationScreen.
 */
void MythNotificationScreen::Init(void)
{
    if (!m_refresh) // nothing got changed so far, return
        return;

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
            MythImage *img = m_artworkImage->GetPainter()->GetFormatImage();
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

    SetErrorState();

    if (m_mediaState && (m_update & kImage))
    {
        m_mediaState->DisplayState(m_update & kNoArtwork ? "noartwork" : "ok");
        LOG(VB_GUI, LOG_DEBUG, LOC + QString("Init: Set media state to %1").arg(m_update & kNoArtwork ? "noartwork" : "ok"));
    }

    // No field will be refreshed the next time unless specified otherwise
    m_update = kNone;

    if (GetScreenStack() && !m_added)
    {
        GetScreenStack()->AddScreen(this);
        m_added = true;
    }
    m_refresh = false;
}

void MythNotificationScreen::SetErrorState(void)
{
    if (!m_errorState)
        return;

    const char *state;

    if (m_type == MythNotification::Error)
    {
        state = "error";
    }
    else if (m_type == MythNotification::Warning)
    {
        state = "warning";
    }
    else if (m_type == MythNotification::Check)
    {
        state = "check";
    }
    else
    {
        state = "ok";
    }
    LOG(VB_GUI, LOG_DEBUG, LOC + QString("SetErrorState: Set error state to %1").arg(state));
    m_errorState->DisplayState(state);
}

/**
 * Update artwork image.
 * must call Init() for screen to be updated.
 */
void MythNotificationScreen::UpdateArtwork(const QImage &image)
{
    m_image = image;
    // We need to re-run init
    m_refresh = true;
}

/**
 * Update artwork image via URL or file path.
 * must call Init() for screen to be updated.
 */
void MythNotificationScreen::UpdateArtwork(const QString &image)
{
    m_imagePath = image;
    // We need to re-run init
    m_refresh = true;
}

/**
 * Read some DMAP tag to extract title, artist, album and file format.
 * must call Init() for screen to be updated.
 * If metadata update flag is set; a Null string means to leave the text field
 * unchanged.
 */
void MythNotificationScreen::UpdateMetaData(const DMAP &data)
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
    // We need to re-run init
    m_refresh = true;
}

/**
 * Update playback position information.
 * must call Init() for screen to be updated.
 */
void MythNotificationScreen::UpdatePlayback(float progress, const QString &text)
{
    m_progress      = progress;
    m_progresstext  = text;
    // We need to re-run init
    m_refresh = true;
}

/**
 * Update Y position of the screen
 * All children elements will be relocated.
 */
void MythNotificationScreen::AdjustYPosition(void)
{
    MythPoint point = m_position;
    point.setY(m_position.getY().toInt() + (GetHeight() + HGAP) * m_index);

    if (point == GetPosition())
        return;

    SetPosition(point);
    // We need to re-run init
    m_refresh = true;
}

void MythNotificationScreen::AdjustIndex(int by, bool set)
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

/**
 * set index, without recalculating coordinates
 */
void MythNotificationScreen::SetIndex(int index)
{
    if (index != m_index)
    {
        m_refresh = true;
        m_index = index;
    }
}

int MythNotificationScreen::GetHeight(void)
{
    return GetArea().getHeight().toInt();
}

void MythNotificationScreen::ProcessTimer(void)
{
    LOG(VB_GUI, LOG_DEBUG, LOC + "ProcessTimer()");
    // delete screen
    GetScreenStack()->PopScreen(this, true, true);
}

MythNotificationScreen &MythNotificationScreen::operator=(const MythNotificationScreen &s)
{
    // check if anything have changed
    m_refresh = !(
                  m_id == s.m_id &&
                  m_image == s.m_image &&
                  m_imagePath == s.m_imagePath &&
                  m_title == s.m_title &&
                  m_origin == s.m_origin &&
                  m_description == s.m_description &&
                  m_extra == s.m_extra &&
                  m_duration == s.m_duration &&
                  m_progress == s.m_progress &&
                  m_progresstext == s.m_progresstext &&
                  m_content == s.m_content &&
                  m_fullscreen == s.m_fullscreen &&
                  m_expiry == s.m_expiry &&
                  m_index == s.m_index &&
                  m_style == s.m_style &&
                  m_visibility == s.m_visibility &&
                  m_priority == s.m_priority &&
                  m_type == s.m_type
                  );

    m_id            = s.m_id;
    m_image         = s.m_image;
    m_imagePath     = s.m_imagePath;
    m_title         = s.m_title;
    m_origin        = s.m_origin;
    m_description   = s.m_description;
    m_extra         = s.m_extra;
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
    m_type          = s.m_type;

    m_update = m_content; // so all fields are initialised regardless of notification type

    return *this;
}

void MythNotificationScreen::SetSingleShotTimer(int s, bool update)
{
    // only registered application can display non-expiring notification
    if (m_id > 0 && s < 0)
        return;

    int ms = s * 1000;
    ms = ms <= DEFAULT_DURATION ? DEFAULT_DURATION : ms;

    if (!update)
    {
        m_creation = MythDate::current();
    }
    m_expiry = MythDate::current().addMSecs(ms);

    m_timer->stop();
    m_timer->setSingleShot(true);
    m_timer->start(ms);
}

// Public event handling
bool MythNotificationScreen::keyPressEvent(QKeyEvent *event)
{
    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];

        if (action == "ESCAPE")
        {
            if (MythDate::current() < m_creation.addMSecs(MIN_LIFE))
                return true; // was updated less than 1s ago, ignore
        }
    }
    if (!handled)
    {
        handled = MythScreenType::keyPressEvent(event);
    }
    return handled;
}

/////////////////////// NCPrivate

NCPrivate::NCPrivate()
    : m_currentId(0)
{
    m_screenStack = new MythNotificationScreenStack(GetMythMainWindow(),
                                                    "mythnotificationcenter",
                                                    this);
    m_originalScreenStack = m_screenStack;
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

    DeleteUnregistered();
    DeleteAllRegistrations();
    DeleteAllScreens();

    // Delete all outstanding queued notifications
    foreach(MythNotification *n, m_notifications)
    {
        delete n;
    }
    m_notifications.clear();

    delete m_screenStack;
    m_originalScreenStack = m_screenStack = NULL;
}

/**
 * Remove screen from screens list.
 * Qt slot called upon MythScreenType::Exiting().
 */
void NCPrivate::ScreenDeleted(void)
{
    MythNotificationScreen *screen =
        static_cast<MythNotificationScreen*>(sender());

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
        int num = m_screens.removeAll(screen);
        LOG(VB_GUI, LOG_DEBUG, LOC +
            QString("%1 screen removed from screens list").arg(num));
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
                m_unregistered.remove(screen->m_id);
            }
            else
            {
                // don't remove the id from the list, as the application is still registered
                // re-create the screen
                MythNotificationScreen *newscreen =
                    new MythNotificationScreen(m_screenStack, *screen);
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

bool NCPrivate::Queue(const MythNotification &notification)
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
        GetMythMainWindow(), new MythNotificationCenterEvent());

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
        MythNotificationScreen *screen = NULL;

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
                delete n;
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
                delete n;
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

    DeleteUnregistered();
}

/**
 * CreateScreen will create a MythNotificationScreen instance.
 * This screen will not be displayed until it is used.
 */
MythNotificationScreen *NCPrivate::CreateScreen(MythNotification *n, int id)
{
    MythNotificationScreen *screen;

    if (n)
    {
        screen = new MythNotificationScreen(m_screenStack, *n);
    }
    else
    {
        screen = new MythNotificationScreen(m_screenStack, id);
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

    // queue the de-registration
    m_unregistered[id] = closeimemdiately;

    m_clients.remove(id);

    // Tell the GUI thread we have something to process
    QCoreApplication::postEvent(
        GetMythMainWindow(), new MythNotificationCenterEvent());
}

void NCPrivate::DeleteAllRegistrations(void)
{
    QMap<int, MythNotificationScreen*>::iterator it = m_registrations.begin();

    for (; it != m_registrations.end(); ++it)
    {
        if (*it)
        {
            m_deletedScreens.append(*it);
        }
    }
    m_registrations.clear();
}

void NCPrivate::DeleteAllScreens(void)
{
    // delete all screens waiting to be deleted
    while(!m_deletedScreens.isEmpty())
    {
        MythNotificationScreen *screen = m_deletedScreens.last();
        // we remove the screen from the list before deleting the screen
        // so the MythScreenType::Exiting() signal won't process it a second time
        m_deletedScreens.removeLast();
        if (m_screenStack == NULL &&
            screen->GetScreenStack() == m_originalScreenStack)
        {
            // our screen stack got deleted already and all its children
            // would have been marked for deletion during the
            // ScreenStack destruction but not yet deleted as the event loop may
            // not be running; so we can leave the screen alone.
            // However for clarity, call deleteLater()
            // as it is safe to call deleteLater more than once
            screen->deleteLater();
        }
        else if (screen->GetScreenStack() == m_screenStack)
        {
            screen->GetScreenStack()->PopScreen(screen, true, true);
        }
        else if (screen->GetScreenStack() == NULL)
        {
            // this screen was never added to a screen stack, delete it now
            delete screen;
        }
    }
}

void NCPrivate::DeleteUnregistered(void)
{
    QMap<int,bool>::iterator it = m_unregistered.begin();
    bool needdelete = false;

    for (; it != m_unregistered.end(); ++it)
    {
        int id = it.key();
        bool closeimemdiately = it.value();
        MythNotificationScreen *screen = NULL;

        if (m_registrations.contains(id))
        {
            screen = m_registrations[id];
            if (screen != NULL && !m_suspended.contains(id))
            {
                // mark the screen for deletion if no timer is set
                if (screen->m_duration <= 0 || closeimemdiately)
                {
                    m_deletedScreens.append(screen);
                    needdelete = true;
                }
            }
            m_registrations.remove(id);
        }

        if (m_suspended.contains(id))
        {
            // screen had been suspended, delete suspended screen
            delete screen;
            m_suspended.removeAll(id);
        }
    }
    m_unregistered.clear();

    if (needdelete)
    {
        DeleteAllScreens();
    }
}

/**
 * Insert screen into list of screens.
 * Returns index in screens list.
 */
int NCPrivate::InsertScreen(MythNotificationScreen *screen)
{
    QList<MythNotificationScreen*>::iterator it       = m_screens.begin();
    QList<MythNotificationScreen*>::iterator itend    = m_screens.end();

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
int NCPrivate::RemoveScreen(MythNotificationScreen *screen)
{
    QList<MythNotificationScreen*>::iterator it       = m_screens.begin();
    QList<MythNotificationScreen*>::iterator itend    = m_screens.end();

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
    QList<MythNotificationScreen*>::iterator it       = m_screens.begin();
    QList<MythNotificationScreen*>::iterator itend    = m_screens.end();

    int position = 0;

    for (; it != itend; ++it)
    {
        if ((*it)->IsVisible())
        {
            (*it)->AdjustIndex(position++, true);
        }
        else
        {
            (*it)->AdjustIndex(position, true);
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
        MythNotificationScreen *screen =
            dynamic_cast<MythNotificationScreen*>(*it);

        if (screen)
        {
            if ((screen->m_visibility & MythNotification::kPlayback) == 0)
                continue;

            MythNotificationScreen *newscreen;

            if (!m_converted.contains(screen))
            {
                // screen hasn't been created, return it
                newscreen = new MythNotificationScreen(NULL, *screen);
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
            newscreen->SetIndex(position++);
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

int NCPrivate::DisplayedNotifications(void) const
{
    return m_screens.size();
}

int NCPrivate::QueuedNotifications(void) const
{
    return m_notifications.size();
}

bool NCPrivate::RemoveFirst(void)
{
    QMutexLocker lock(&m_lock);

    if (m_screens.isEmpty())
        return false;

    // The top screen is the only currently displayed first, if there's a
    // fullscreen notification displayed, it's the last one
    MythNotificationScreen *top = m_screens.front();
    QList<MythNotificationScreen *>::const_iterator it = m_screens.end() - 1;

    // loop from last to 2nd
    for (; it != m_screens.begin(); --it)
    {
        MythNotificationScreen *s = *it;

        if (s->m_fullscreen)
        {
            top = s;
            break;
        }
    }

    if (MythDate::current() < top->m_creation.addMSecs(MIN_LIFE))
        return false;

    // simulate time-out
    top->ProcessTimer();
    return true;
}

/////////////////////// MythNotificationCenter

MythNotificationCenter *MythNotificationCenter::GetInstance(void)
{
    return GetNotificationCenter();
}

MythNotificationCenter::MythNotificationCenter()
    : d(new NCPrivate())
{
    const bool isGuiThread =
        QThread::currentThread() == QCoreApplication::instance()->thread();

    if (!isGuiThread)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Constructor not called from GUI thread");
    }
}

MythNotificationCenter::~MythNotificationCenter()
{
    const bool isGuiThread =
        QThread::currentThread() == QCoreApplication::instance()->thread();

    if (!isGuiThread)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Destructor not called from GUI thread");
    }

    delete d;
    d = NULL;
}

bool MythNotificationCenter::Queue(const MythNotification &notification)
{
    return d->Queue(notification);
}

void MythNotificationCenter::ProcessQueue(void)
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

int MythNotificationCenter::Register(void *from)
{
    return d->Register(from);
}

void MythNotificationCenter::UnRegister(void *from, int id, bool closeimemdiately)
{
    d->UnRegister(from, id, closeimemdiately);
}

QDateTime MythNotificationCenter::ScreenExpiryTime(const MythScreenType *screen)
{
    const MythNotificationScreen *s =
        dynamic_cast<const MythNotificationScreen*>(screen);

    if (!s)
        return QDateTime();
    return s->m_expiry;
}

bool MythNotificationCenter::ScreenCreated(const MythScreenType *screen)
{
    const MythNotificationScreen *s =
        dynamic_cast<const MythNotificationScreen*>(screen);

    if (!s)
        return true;;
    return s->m_created;
}

void MythNotificationCenter::GetNotificationScreens(QList<MythScreenType*> &_screens)
{
    d->GetNotificationScreens(_screens);
}

void MythNotificationCenter::UpdateScreen(MythScreenType *screen)
{
    MythNotificationScreen *s =
        dynamic_cast<MythNotificationScreen*>(screen);

    if (!s)
        return;

    if (s->m_created)
    {
        s->doInit();
    }
}

int MythNotificationCenter::DisplayedNotifications(void) const
{
    return d->DisplayedNotifications();
}

int MythNotificationCenter::QueuedNotifications(void) const
{
    return d->QueuedNotifications();
}

bool MythNotificationCenter::RemoveFirst(void)
{
    return d->RemoveFirst();
}

void ShowNotificationError(const QString &msg,
                           const QString &from,
                           const QString &detail,
                           const VNMask visibility,
                           const MythNotification::Priority priority)
{
    ShowNotification(true, msg, from, detail);
}

void ShowNotification(const QString &msg,
                      const QString &from,
                      const QString &detail,
                      const VNMask visibility,
                      const MythNotification::Priority priority)
{
    ShowNotification(false, msg, from, detail,
                     QString(), QString(), QString(), -1, -1, false,
                     visibility, priority);
}

void ShowNotification(bool  error,
                      const QString &msg,
                      const QString &origin,
                      const QString &detail,
                      const QString &image,
                      const QString &extra,
                      const QString &progress_text, float progress,
                      int   duration,
                      bool  fullscreen,
                      const VNMask visibility,
                      const MythNotification::Priority priority,
                      const QString &style)
{
    ShowNotification(error ? MythNotification::Error : MythNotification::New,
                     msg, origin, detail, image, extra, progress_text, progress,
                     duration, fullscreen, visibility, priority, style);
}

void ShowNotification(MythNotification::Type type,
                      const QString &msg,
                      const QString &origin,
                      const QString &detail,
                      const QString &image,
                      const QString &extra,
                      const QString &progress_text, float progress,
                      int   duration,
                      bool  fullscreen,
                      const VNMask visibility,
                      const MythNotification::Priority priority,
                      const QString &style)
{
    if (!GetNotificationCenter())
        return;

    MythNotification *n;
    DMAP data;

    data["minm"] = msg;
    data["asar"] = origin.isNull() ? QCoreApplication::translate("(Common)",
                                                                 "MythTV") : origin;
    data["asal"] = detail;
    data["asfm"] = extra;

    if (type == MythNotification::Error   ||
        type == MythNotification::Warning ||
        type == MythNotification::Check)
    {
        n = new MythNotification(type, data);
        if (!duration && type != MythNotification::Check)
        {
            // default duration for those type of notifications is 10s
            duration = 10;
        }
    }
    else
    {
        if (!image.isEmpty())
        {
            if (progress >= 0)
            {
                n = new MythMediaNotification(type,
                                              image, data,
                                              progress, progress_text);
            }
            else
            {
                n = new MythImageNotification(type, image, data);
            }
        }
        else if (progress >= 0)
        {
            n = new MythPlaybackNotification(type,
                                             progress, progress_text, data);
        }
        else
        {
            n = new MythNotification(type, data);
        }
    }
    n->SetDuration(duration);
    n->SetFullScreen(fullscreen);
    n->SetPriority(priority);
    n->SetVisibility(visibility);

    MythNotificationCenter::GetInstance()->Queue(*n);
    delete n;
}
