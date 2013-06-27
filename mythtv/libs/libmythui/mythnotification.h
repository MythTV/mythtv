//
//  mythnotification.h
//  MythTV
//
//  Created by Jean-Yves Avenard on 25/06/13.
//  Copyright (c) 2013 Bubblestuff Pty Ltd. All rights reserved.
//

#ifndef __MythTV__mythnotification__
#define __MythTV__mythnotification__

#include <QMutex>
#include <QMap>
#include <QImage>

#include "mythevent.h"
#include "mythuiexp.h"

typedef QMap<QString,QString> DMAP;

class MUI_PUBLIC MythNotification : public MythEvent
{
public:

    static Type New;
    static Type Update;
    static Type Info;
    static Type Error;

    MythNotification(Type t, void *parent = NULL)
        : MythEvent(t), m_id(-1), m_parent(parent), m_fullScreen(false),
        m_duration(0), m_visibility(kAll), m_priority(kDefault)

    {
    }

    MythNotification(int id, void *parent) :
        MythEvent(Update), m_id(id), m_parent(parent), m_fullScreen(false),
        m_duration(0), m_visibility(kAll), m_priority(kDefault)
    {
    }

    virtual ~MythNotification()
    {
    }

    virtual MythEvent *clone(void) const { return new MythNotification(*this); }

    /** Priority enum
     * A notification can be given a priority. Display order of notification
     * will be sorted according to the priority level
     */
    enum Priority {
        kDefault    = 0,
        kLow,
        kMedium,
        kHigh,
        kHigher,
        kHighest,
    };

    /** Visibility enum
     * A notification can be given visibility mask allowing to not be visible
     * under some circumstances, like the screen currently being displayed.
     * This is used to prevent redundant information appearing more than once:
     * like in MythMusic, there's no need to show music notifications
     */
    enum Visibility {
        kNone       = 0,
        kAll        = ~0,
        kPlayback   = (1 << 1),
        kSettings   = (1 << 2),
        kWizard     = (1 << 3),
        kVideos     = (1 << 4),
        kMusic      = (1 << 5),
        kRecordings = (1 << 6),
    };

    // Setter
    /**
     * Optional MythNotification elements
     */
    /**
     * contains the application registration id
     * Required to update an existing notification screen owned by an application
     */
    void SetId(int id)                  { m_id = id; }
    /**
     * contains the parent address. Required if id is set
     * Id provided must match the parent address as provided during the
     * MythUINotificationCenter registration, otherwise the id value will be
     * ignored
     */
    void SetParent(void *parent)        { m_parent = parent; }
    /**
     * a notification may request to be displayed in full screen,
     * this request may not be fullfilled should the theme not handle full screen
     * notification
     */
    void SetFullScreem(bool f)          { m_fullScreen = f; }
    /**
     * contains a short description of the notification
     */
    void SetDescription(QString desc)   { m_description = desc; }
    /**
     * metadata of the notification.
     * In DMAP format. DMAP can contains various information such as artist,
     * album name, author name, genre etc..
     */
    void SetMetaData(DMAP data)         { m_metadata = data; }
    /**
     * contains a duration during which the notification will be displayed.
     * The duration is informative only as the MythUINotificationCenter will
     * determine automatically how long a notification can be displayed for
     * and will depend on priority, visibility and other factors
     */
    void SetDuration(int duration)      { m_duration = duration; };
    void SetVisibility(Visibility n)    { m_visibility = n; }
    void SetPriority(Priority n)        { m_priority = n; }

    // Getter
    int         GetId(void)             { return m_id; }
    void       *GetParent(void)         { return m_parent; }
    QString     GetDescription(void)    { return m_description; }
    DMAP        GetMetaData(void)       { return m_metadata; }
    int         GetDuration(void)       { return m_duration; };
    Visibility  GetVisibility(void)     { return m_visibility; }
    Priority    GetPriority(void)       { return m_priority; }

protected:
    MythNotification(const MythNotification &o)
        : MythEvent(o),
        m_id(o.m_id),      m_parent(o.m_parent),   m_fullScreen(o.m_fullScreen),
        m_description(o.m_description),
        m_duration(o.m_duration),                  m_metadata(o.m_metadata),
        m_visibility(o.m_visibility),              m_priority(o.m_priority)
    {
    }

    MythNotification &operator=(const MythNotification&);

protected:
    int         m_id;
    void       *m_parent;
    bool        m_fullScreen;
    QString     m_description;
    int         m_duration;
    DMAP        m_metadata;
    Visibility  m_visibility;
    Priority    m_priority;
};

class MUI_PUBLIC MythImageNotification : public virtual MythNotification
{
public:
    MythImageNotification(Type t)
        : MythNotification(t)
    {
    }

    MythImageNotification(Type t, QImage &image, DMAP &metadata)
        : MythNotification(t), m_image(image)
    {
        m_metadata = metadata;
    }

    virtual MythEvent *clone(void) const { return new MythImageNotification(*this); }

    // Setter
    /**
     * image to be displayed with the notification
     */
    void SetImage(QImage &image)        { m_image = image; }

    //Getter
    QImage GetImage(void)               { return m_image; }

protected:
    MythImageNotification(const MythImageNotification &o)
        : MythNotification(o), m_image(o.m_image)
    {
    }

    MythImageNotification &operator=(const MythImageNotification&);

protected:
    QImage      m_image;
};

class MUI_PUBLIC MythPlaybackNotification : public virtual MythNotification
{
public:
    MythPlaybackNotification(Type t, int duration, int position)
        : MythNotification(t), m_duration(duration), m_position(position)
    {
    }

    virtual MythEvent *clone(void) const { return new MythPlaybackNotification(*this); }

    // Setter
    /**
     * duration to be displayed with the notification
     */
    void SetDuration(int duration)      { m_duration = duration; }
    /**
     * current playback position to be displayed with the notification
     */
    void SetPosition(int position)      { m_position = position; }

    //Getter
    int GetDuration(void)               { return m_duration; }
    int GetPosition(void)               { return m_position; }

protected:
    MythPlaybackNotification(const MythPlaybackNotification &o)
        : MythNotification(o),
        m_duration(o.m_duration), m_position(o.m_position)
    {
    }

    MythPlaybackNotification &operator=(const MythPlaybackNotification&);

protected:
    int         m_duration;
    int         m_position;
};

class MUI_PUBLIC MythMediaNotification : public MythImageNotification,
                                         public MythPlaybackNotification
{
public:
    MythMediaNotification(Type t, QImage &image, DMAP &metadata,
                          int duration, int position)
        : MythNotification(t), MythImageNotification(t, image, metadata),
        MythPlaybackNotification(t, duration, position)
    {
    }

    virtual MythEvent *clone(void) const { return new MythMediaNotification(*this); }

protected:
    MythMediaNotification(const MythMediaNotification &o)
        : MythNotification(o), MythImageNotification(o), MythPlaybackNotification(o)
    {
    }

    MythMediaNotification &operator=(const MythMediaNotification&);
};

#endif /* defined(__MythTV__mythnotification__) */
