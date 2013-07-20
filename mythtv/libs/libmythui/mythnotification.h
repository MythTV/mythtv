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
typedef unsigned int    VNMask;

class MUI_PUBLIC MythNotification : public MythEvent
{
public:

    static Type New;
    static Type Update;
    static Type Info;
    static Type Error;
    static Type Warning;
    static Type Check;

    MythNotification(Type t, void *parent = NULL)
        : MythEvent(t), m_id(-1), m_parent(parent), m_fullScreen(false),
        m_duration(0), m_visibility(kAll), m_priority(kDefault)
    {
    }

    MythNotification(int id, void *parent)
        : MythEvent(Update), m_id(id), m_parent(parent), m_fullScreen(false),
        m_duration(0), m_visibility(kAll), m_priority(kDefault)
    {
    }

    MythNotification(const QString &title, const QString &author,
                     const QString &details = QString())
        : MythEvent(New), m_id(-1), m_parent(NULL), m_fullScreen(false),
        m_description(title), m_duration(0), m_visibility(kAll),
        m_priority(kDefault)
    {
        DMAP map;
        map["minm"] = title;
        map["asar"] = author;
        map["asal"] = details;
        m_metadata = map;
    }

    MythNotification(Type t, const QString &title, const QString &author,
                     const QString &details = QString())
        : MythEvent(t), m_id(-1), m_parent(NULL), m_fullScreen(false),
        m_description(title), m_duration(0), m_visibility(kAll),
        m_priority(kDefault)
    {
        DMAP map;
        map["minm"] = title;
        map["asar"] = author;
        map["asal"] = details;
        m_metadata = map;
    }

    MythNotification(Type t, const DMAP &metadata)
        : MythEvent(t), m_id(-1), m_parent(NULL), m_fullScreen(false),
        m_duration(0), m_metadata(metadata),
        m_visibility(kAll), m_priority(kDefault)
    {
    }

    virtual ~MythNotification()
    {
    }

    virtual MythEvent *clone(void) const    { return new MythNotification(*this); }

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
        kPlayback   = (1 << 0),
        kSettings   = (1 << 1),
        kWizard     = (1 << 2),
        kVideos     = (1 << 3),
        kMusic      = (1 << 4),
        kRecordings = (1 << 5),
    };

    // Setter
    /**
     * Optional MythNotification elements
     */
    /**
     * contains the application registration id
     * Required to update an existing notification screen owned by an application
     */
    void SetId(int id);
    /**
     * contains the parent address. Required if id is set
     * Id provided must match the parent address as provided during the
     * MythUINotificationCenter registration, otherwise the id value will be
     * ignored
     */
    void SetParent(void *parent)            { m_parent = parent; }
    /**
     * a notification may request to be displayed in full screen,
     * this request may not be fullfilled should the theme not handle full screen
     * notification
     */
    void SetFullScreen(bool f)              { m_fullScreen = f; }
    /**
     * contains a short description of the notification
     */
    void SetDescription(const QString &desc) { m_description = desc; }
    /**
     * metadata of the notification.
     * In DMAP format. DMAP can contains various information such as artist,
     * album name, author name, genre etc..
     */
    void SetMetaData(const DMAP &data)      { m_metadata = data; }
    /**
     * contains a duration during which the notification will be displayed for.
     * The duration is informative only as the MythUINotificationCenter will
     * determine automatically how long a notification can be displayed for
     * and will depend on priority, visibility and other factors
     */
    void SetDuration(int duration)          { m_duration = duration; };
    /**
     * contains an alternative notification style.
     * Should a style be defined, the Notification Center will attempt to load
     * an alternative theme and fall back to the default one if unsuccessful
     */
    void SetStyle(const QString &style)     { m_style = style; }
    /**
     * define a bitmask of Visibility
     */
    void SetVisibility(VNMask n)            { m_visibility = n; }
    /**
     * For future use, not implemented at this stage
     */
    void SetPriority(Priority n)              { m_priority = n; }

    /**
     * return Type object from type name
     */
    static Type TypeFromString(const QString &type);

    // Getter
    int         GetId(void) const           { return m_id; }
    void       *GetParent(void) const       { return m_parent; }
    bool        GetFullScreen(void) const   { return m_fullScreen; }
    QString     GetDescription(void) const  { return m_description; }
    DMAP        GetMetaData(void) const     { return m_metadata; }
    int         GetDuration(void) const     { return m_duration; };
    QString     GetStyle(void) const        { return m_style; }
    VNMask      GetVisibility(void) const   { return m_visibility; }
    Priority    GetPriority(void) const     { return m_priority; }

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
    QString     m_style;
    VNMask      m_visibility;
    Priority    m_priority;
};

class MUI_PUBLIC MythImageNotification : public virtual MythNotification
{
public:
    MythImageNotification(Type t, const QImage &image)
        : MythNotification(t), m_image(image)
    {
    }

    MythImageNotification(Type t, const QString &imagePath)
        : MythNotification(t), m_imagePath(imagePath)
    {
    }

    MythImageNotification(Type t, const QImage &image, const DMAP &metadata)
        : MythNotification(t, metadata), m_image(image)
    {
    }

    MythImageNotification(Type t, const QString &imagePath, const DMAP &metadata)
        : MythNotification(t, metadata), m_imagePath(imagePath)
    {
    }

    virtual MythEvent *clone(void) const    { return new MythImageNotification(*this); }

    // Setter
    /**
     * image to be displayed with the notification
     */
    void SetImage(const QImage &image)      { m_image = image; }
    /**
     * image filename to be displayed with the notification
     */
    void SetImagePath(const QString &image) { m_imagePath = image; }

    //Getter
    QImage GetImage(void) const             { return m_image; }
    QString GetImagePath(void) const        { return m_imagePath; }

protected:
    MythImageNotification(const MythImageNotification &o)
        : MythNotification(o), m_image(o.m_image), m_imagePath(o.m_imagePath)
    {
    }

    MythImageNotification &operator=(const MythImageNotification&);

protected:
    QImage      m_image;
    QString     m_imagePath;
};

class MUI_PUBLIC MythPlaybackNotification : public virtual MythNotification
{
public:
    MythPlaybackNotification(Type t, float progress, const QString &progressText)
        : MythNotification(t), m_progress(progress), m_progressText(progressText)
    {
    }

    MythPlaybackNotification(Type t, float progress, const QString &progressText,
                             const DMAP &metadata)
        : MythNotification(t, metadata),
        m_progress(progress), m_progressText(progressText)
    {
    }

    MythPlaybackNotification(Type t, int duration, int position)
        : MythNotification(t)
    {
        m_progress      = (float)position / (float)duration;
        m_progressText  = stringFromSeconds(duration);
    }

    virtual MythEvent *clone(void) const    { return new MythPlaybackNotification(*this); }

    // Setter
    /**
     * current playback position to be displayed with the notification.
     * Value to be between 0 <= x <= 1.
     * Note: x < 0 means no progress bar to be displayed.
     */
    void SetProgress(float progress)        { m_progress = progress; }
    /**
     * text to be displayed with the notification as duration or progress.
     */
    void SetProgressText(const QString &text) { m_progressText = text; }

    //Getter
    float GetProgress(void) const           { return m_progress; }
    QString GetProgressText(void) const     { return m_progressText; }

    // utility methods
    static QString stringFromSeconds(int time);

protected:
    MythPlaybackNotification(const MythPlaybackNotification &o)
        : MythNotification(o),
        m_progress(o.m_progress), m_progressText(o.m_progressText)
    {
    }

    MythPlaybackNotification &operator=(const MythPlaybackNotification&);

protected:
    float       m_progress;
    QString     m_progressText;
};

class MUI_PUBLIC MythMediaNotification : public MythImageNotification,
                                         public MythPlaybackNotification
{
public:
    MythMediaNotification(Type t, const QImage &image, const DMAP &metadata,
                          float progress, const QString &durationText)
        : MythNotification(t, metadata), MythImageNotification(t, image),
        MythPlaybackNotification(t, progress, durationText)
    {
    }

    MythMediaNotification(Type t, const QImage &image, const DMAP &metadata,
                          int duration, int position)
        : MythNotification(t, metadata), MythImageNotification(t, image),
        MythPlaybackNotification(t, duration, position)
    {
    }

    MythMediaNotification(Type t, const QString &imagePath, const DMAP &metadata,
                          float progress, const QString &durationText)
        : MythNotification(t, metadata), MythImageNotification(t, imagePath),
        MythPlaybackNotification(t, progress, durationText)
    {
    }

    MythMediaNotification(Type t, const QString &imagePath, const DMAP &metadata,
                          int duration, int position)
        : MythNotification(t, metadata), MythImageNotification(t, imagePath),
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

class MUI_PUBLIC MythErrorNotification : public MythNotification
{
public:
    MythErrorNotification(const QString &title, const QString &author,
                          const QString &details = QString())
        : MythNotification(Error, title, author, details)
    {
        SetDuration(10);
    }
};

class MUI_PUBLIC MythWarningNotification : public MythNotification
{
public:
    MythWarningNotification(const QString &title, const QString &author,
                            const QString &details = QString())
    : MythNotification(Warning, title, author, details)
    {
        SetDuration(10);
    }
};

class MUI_PUBLIC MythCheckNotification : public MythNotification
{
public:
    MythCheckNotification(const QString &title, const QString &author,
                          const QString &details = QString())
    : MythNotification(Check, title, author, details) { }
};

#endif /* defined(__MythTV__mythnotification__) */
