//
//  mythnotificationcenter_private.h
//  MythTV
//
//  Created by Jean-Yves Avenard on 30/06/13.
//  Copyright (c) 2013 Bubblestuff Pty Ltd. All rights reserved.
//

#ifndef MythTV_mythnotificationcenter_private_h
#define MythTV_mythnotificationcenter_private_h

#include <QTimer>
#include <QMutex>
#include <QList>
#include <QMap>

#include <stdint.h>

#include "mythscreenstack.h"
#include "mythscreentype.h"
#include "mythuiimage.h"
#include "mythuitext.h"
#include "mythuiprogressbar.h"
#include "mythnotificationcenter.h"
#include "mythuistatetype.h"

// Forward declarations
class MythNotificationScreen;
class MythNotificationScreenStack;

#define MIN_LIFE 1000

class NCPrivate : public QObject
{
    Q_OBJECT

public slots:
    void ScreenDeleted(void);

public:
    NCPrivate(void);
    virtual ~NCPrivate();

    /**
     * Queue a notification
     * Queue() is thread-safe and can be called from anywhere.
     * Typical use would be GetNotificationCenter()->Queue(notification)
     */
    bool Queue(const MythNotification &notification);

    /**
     * returns the MythNotificationCenter singleton
     */
    static MythNotificationCenter *GetInstance(void);

    /**
     * An application can register in which case it will be assigned a
     * reusable screen, which can be modified or updated
     * Register takes a pointer of the client registering. This is used
     * to make sure a registered Id can only be used by the client who registered
     * for it.
     * Returns a unique Id that can be provided with a MythNotification
     * Return -1 is case of error
     */
    int  Register(void *from);
    /**
     * Unregister the client.
     * If the notification had set a duration, the screen will be left to
     * laps, unless forcedisconnect is set; in which case the screen will
     * be closed immediately.
     */
    void UnRegister(void *from, int id, bool closeimemdiately = false);

    /*
     * OSD drawing utilities
     */

    /**
     * Return when the given screen is going to expire
     * will return an invalid QDateTime if screen isn't a MythNotificationScreen
     */
    QDateTime ScreenExpiryTime(const MythScreenType *screen);
    /**
     * Return true if \::Create() has been called on screen.
     * will always return true should screen not be a MythNotificationScreen
     */
    bool ScreenCreated(const MythScreenType *screen);
    /**
     * Return the list of notification screens being currently displayed.
     * The list contains pointer of existing screen's copies, with \::Create()
     * not called yet.
     */
    void GetNotificationScreens(QList<MythScreenType*> &screens);
    /**
     * Will call \::doInit() if the screen is a MythNotificationScreen and
     * \::Create() has been called for it already
     */
    void UpdateScreen(MythScreenType *screen);
    /**
     * Returns number of notifications currently displayed
     */
    int DisplayedNotifications(void) const;
    /**
     * Returns number of notifications currently queued
     */
    int QueuedNotifications(void) const;
    /**
     * Will remove the oldest notification from the stack
     * return true if a screen was removed; or false if nothing was done
     */
    bool RemoveFirst(void);
    /**
     * ProcessQueue will be called by the GUI event handler and will process
     * all queued MythNotifications and delete screens marked to be deleted
     * ProcessQueue must be called from GUI thread
     */
    void ProcessQueue(void);

    void ScreenStackDeleted(void);

private:

    MythNotificationScreen *CreateScreen(MythNotification *notification,
                                           int id = -1);
    void DeleteAllRegistrations(void);
    void DeleteAllScreens(void);
    void DeleteUnregistered(void);
    int InsertScreen(MythNotificationScreen *screen);
    int RemoveScreen(MythNotificationScreen *screen);
    void RefreshScreenPosition(int from = 0);

    MythNotificationScreenStack            *m_originalScreenStack;
    MythNotificationScreenStack            *m_screenStack;
    QList<MythNotification*>                m_notifications;
    QList<MythNotificationScreen*>        m_screens;
    QList<MythNotificationScreen*>        m_deletedScreens;
    QMap<int, MythNotificationScreen*>    m_registrations;
    QList<int>                              m_suspended;
    QMap<int,bool>                          m_unregistered;
    QMap<int, void*>                        m_clients;
    QMutex                                  m_lock;
    int                                     m_currentId;
    QMap<MythNotificationScreen*, MythNotificationScreen*> m_converted;
};

class MythNotificationScreen : public MythScreenType
{
    Q_OBJECT

public:
    MythNotificationScreen(MythScreenStack *stack,
                             int id = -1);
    MythNotificationScreen(MythScreenStack *stack,
                             MythNotification &notification);
    MythNotificationScreen(MythScreenStack *stack,
                             const MythNotificationScreen &screen);
    
    virtual ~MythNotificationScreen();

    bool keyPressEvent(QKeyEvent *event);

    // These two methods are declared by MythScreenType and their signatures
    // should not be changed
    virtual bool Create(void);
    virtual void Init(void);

    void SetNotification(MythNotification &notification);

    void UpdateArtwork(const QImage &image);
    void UpdateArtwork(const QString &image);
    void UpdateMetaData(const DMAP &data);
    void UpdatePlayback(float progress, const QString &text);
    void UpdateFrom(const MythNotificationScreen &s);

    void SetSingleShotTimer(int s, bool update = false);
    void SetErrorState(void);

    // UI methods
    void AdjustYPosition(void);
    void AdjustIndex(int by, bool set=false);
    void SetIndex(int value);
    int  GetHeight(void);

    enum Content {
        kNone       = 0,
        kImage      = 1 << 0,
        kDuration   = 1 << 1,
        kMetaData   = 1 << 2,
        kStyle      = 1 << 3,
        kNoArtwork  = 1 << 4,
        kAll        = ~kNone,
    };

signals:
    void ScreenDeleted();

public slots:
    void ProcessTimer(void);

public:
    int                         m_id;
    QImage                      m_image;
    QString                     m_imagePath;
    QString                     m_title;
    QString                     m_origin;
    QString                     m_description;
    QString                     m_extra;
    int                         m_duration;
    float                       m_progress;
    QString                     m_progresstext;
    bool                        m_fullscreen;
    bool                        m_added;
    bool                        m_created;
    uint32_t                    m_content;
    uint32_t                    m_update;
    MythNotification::Type      m_type;
    MythUIImage                *m_artworkImage;
    MythUIText                 *m_titleText;
    MythUIText                 *m_originText;
    MythUIText                 *m_descriptionText;
    MythUIText                 *m_extraText;
    MythUIText                 *m_progresstextText;
    MythUIProgressBar          *m_progressBar;
    MythUIStateType            *m_errorState;
    MythUIStateType            *m_mediaState;
    QDateTime                   m_creation, m_expiry;
    int                         m_index;
    MythPoint                   m_position;
    QTimer                     *m_timer;
    QString                     m_style;
    VNMask                      m_visibility;
    MythNotification::Priority  m_priority;
    bool                        m_refresh;
};

//// class MythScreenNotificationStack

class MythNotificationScreenStack : public MythScreenStack
{
public:
    MythNotificationScreenStack(MythMainWindow *parent, const QString& name,
                                NCPrivate *owner)
        : MythScreenStack(parent, name), m_owner(owner)
    {
    }

    virtual ~MythNotificationScreenStack()
    {
        m_owner->ScreenStackDeleted();
    }

    void CheckDeletes()
    {
        QVector<MythScreenType*>::const_iterator it;

        for (it = m_ToDelete.begin(); it != m_ToDelete.end(); ++it)
        {
            (*it)->SetAlpha(0);
            (*it)->SetVisible(false);
            (*it)->Close();
        }
        MythScreenStack::CheckDeletes();
    }

    static const int kFadeVal = 20;

    virtual void PopScreen(MythScreenType *screen, bool allowFade = true,
                           bool deleteScreen = true);
    virtual MythScreenType *GetTopScreen(void) const;
private:
    NCPrivate *m_owner;

};

#endif
