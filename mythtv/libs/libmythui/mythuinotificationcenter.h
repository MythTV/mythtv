//
//  mythuinotificationcenter.h
//  MythTV
//
//  Created by Jean-Yves Avenard on 25/06/13.
//  Copyright (c) 2013 Bubblestuff Pty Ltd. All rights reserved.
//

#ifndef __MythTV__mythnotifications__
#define __MythTV__mythnotifications__

#include <QMutex>
#include <QList>
#include <QMap>
#include <QRect>
#include <QDateTime>

#include "mythuiexp.h"
#include "mythevent.h"

#include "mythnotification.h"

// .h

class MythUINotificationScreen;
class MythScreenStack;
class MythPainter;
class MythScreenType;
class QPaintDevice;
class MythNotificationScreenStack;

class MUI_PUBLIC MythUINotificationCenterEvent : public MythEvent
{
public:
    MythUINotificationCenterEvent() : MythEvent(kEventType) { }

    static Type kEventType;
};

class MUI_PUBLIC MythUINotificationCenter : public QObject
{
    Q_OBJECT

public:
    MythUINotificationCenter(void);
    virtual ~MythUINotificationCenter();

    /**
     * Queue a notification
     * Queue() is thread-safe and can be called from anywhere.
     * Typical use would be MythUINotificationCenter::GetInstance()->Queue(notification)
     */
    bool Queue(MythNotification &notification);

    /**
     * returns the MythUINotificationCenter singleton
     */
    static MythUINotificationCenter *GetInstance(void);

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
     * will return an invalid QDateTime if screen isn't a MythUINotificationScreen
     */
    QDateTime ScreenExpiryTime(MythScreenType *screen);
    /**
     * Return true if ::Create() has been called on screen.
     * will always return true should screen not be a MythUINotificationScreen
     */
    bool ScreenCreated(MythScreenType *screen);
    /**
     * Return the list of notification screens being currently displayed.
     * The list contains pointer of existing screen's copies, with ::Create()
     * not called yet.
     */
    void GetNotificationScreens(QList<MythScreenType*> &screens);
    /**
     * Will call ::doInit() if the screen is a MythUINotificationScreen and
     * ::Create() has been called for it already
     */
    void UpdateScreen(MythScreenType *screen);
    /**
     * ProcessQueue will be called by the GUI event handler and will process
     * all queued MythNotifications and delete screens marked to be deleted
     * ProcessQueue must be called from GUI thread
     */
    void ProcessQueue(void);

private slots:
    void ScreenDeleted(void);

private:
    friend class MythNotificationScreenStack;
    friend class MythUINotificationScreen;

    MythUINotificationScreen *CreateScreen(MythNotification *notification,
                                           int id = -1);
    MythNotificationScreenStack *GetScreenStack(void);
    void ScreenStackDeleted(void);
    void DeleteAllScreens(void);
    int InsertScreen(MythUINotificationScreen *screen);
    int RemoveScreen(MythUINotificationScreen *screen);
    void AdjustScreenPosition(int from, bool down);

private:
    MythNotificationScreenStack            *m_screenStack;
    QList<MythNotification*>                m_notifications;
    QList<MythUINotificationScreen*>        m_screens;
    QList<MythUINotificationScreen*>        m_deletedScreens;
    QMap<int, MythUINotificationScreen*>    m_registrations;
    QList<int>                              m_suspended;
    QMap<int, void*>                        m_clients;
    QMutex                                  m_lock;
    int                                     m_currentId;
    QMap<MythUINotificationScreen*, MythUINotificationScreen*> m_converted;

    // protected by g_lock
    static QMutex                           g_lock;
    static MythUINotificationCenter        *g_singleton;
};

#endif /* defined(__MythTV__mythnotifications__) */
