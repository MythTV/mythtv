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

#include "mythuiexp.h"
#include "mythevent.h"

#include "mythnotification.h"

// .h

class MythUINotificationScreen;
class MythScreenStack;

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
     * Unregister the client and release/remove the corresponding screen
     */
    void UnRegister(void *from, int id);

    /**
     * ProcessQueue will be called by the GUI event handler and will process
     * all queued MythNotifications and delete screens marked to be deleted
     * ProcessQueue must be called from GUI thread
     */
    void ProcessQueue(void);

private slots:
    void ScreenDeleted(void);

private:
    MythUINotificationScreen *CreateScreen(int id);
    MythScreenStack *GetScreenStack(void);
    void DeleteAllScreens(void);

private:
    MythScreenStack                        *m_screenStack;
    QList<MythNotification*>                m_notifications;
    QList<MythUINotificationScreen*>        m_deletedScreens;
    QMap<int, MythUINotificationScreen*>    m_registrations;
    QMap<int, void*>                        m_clients;
    static QMutex                           m_lock;
    // protected by m_lock
    static int                              m_currentId;
    static MythUINotificationCenter        *g_singleton;
};

#endif /* defined(__MythTV__mythnotifications__) */
