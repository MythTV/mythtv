//! \file
//! \brief Defines specialised images used by the Gallery slideshow and
//! the animation framework used by transformations
//! \details Qt Animation Framework doesn't support acceleration and is driven
//! by a timer that isn't synchronised to Myth's Pulse.
//! So we provide our own lightweight version.
//! An animation may be a hierarchy of sequential or parallel groups to achieve
//! any combination of consecutive and/or simultaneous effects

#ifndef GALLERYSLIDE_H
#define GALLERYSLIDE_H

#include <QThread>
#include <QQueue>
#include <QVariantAnimation>

#include <mythuiimage.h>
#include <imageutils.h>

// Min/max zoom extents available in slideshow
#define MIN_ZOOM 0.1
#define MAX_ZOOM 20.0


//! An wrapped image loader so that we can detect when it completes
class ImageLoadingThread : public QThread
{
public:
    explicit ImageLoadingThread(MythUIImage *image) : QThread(), m_image(image) {}

protected:
    void run()   { if (m_image) m_image->Load(false); }

    MythUIImage *m_image;
};


class Slide;

//! \brief Base animation class that is driven by a Myth pulse and implements
//! variable speed
//! \note Although concrete, for convenience, this shouldn't be used directly
class AbstractAnimation : public QObject
{
    Q_OBJECT
public:
    AbstractAnimation() : m_forwards(true), m_running(false), m_speed(0.0) {}
    virtual bool Start(bool forwards, float speed = 1.0);
    virtual void Stop()                { m_running = false; }
    virtual void SetSpeed(float speed) { m_speed = speed; }
    virtual void Pulse(int interval) = 0;
    virtual void Clear()               {}
    bool         IsRunning() const     { return m_running; }

protected slots:
    //! To be called when animation completes
    virtual void Finished()  { m_running = false; emit finished(); }

signals:
    //! Signals animation has finished
    void         finished();

protected:
    // Play direction
    bool  m_forwards;
    bool  m_running;
    // Real-time = 1.0, Double-speed = 2.0
    float m_speed;
};


//! \brief A single animation controlling alpha, zoom, rotation and position
//! \note This is almost compatible with MythUIAnimation (they could be merged)
class Animation : public AbstractAnimation, public QVariantAnimation
{
public:
    //! Supported effects
    enum Type {None, Alpha, Position, Zoom, HorizontalZoom, VerticalZoom, Angle};

    Animation(Slide *image, Type type = Alpha);
    virtual bool Start(bool forwards = true, float speed = 1.0);
    virtual void Pulse(int interval);
    void         Set(QVariant from, QVariant to,
                     int duration = 500,
                     QEasingCurve curve = QEasingCurve::InOutCubic,
                     UIEffects::Centre = UIEffects::Middle);
    virtual void updateCurrentValue(const QVariant &value);

protected:
    //! Image to be animated
    // Should be MythUItype but that impacts elsewhere: SetZoom must become
    // virtual, which causes compiler warnings in subtitles (fn hiding)
    Slide            *m_parent;
    Type              m_type;
    UIEffects::Centre m_centre;
    //! Current millisec position within animation, 0..duration.
    //! Decreases duration..0 when playing backwards
    int               m_elapsed;
};


//! \brief Abstract class for groups of animations
class GroupAnimation : public AbstractAnimation
{
public:
    GroupAnimation() : AbstractAnimation()                 {}
    virtual ~GroupAnimation()                              { Clear(); }
    virtual void Pulse(int interval)                     = 0;
    virtual bool Start(bool forwards, float speed = 1.0) = 0;
    virtual void SetSpeed(float speed)                   = 0;
    virtual void Add(AbstractAnimation *child);
    virtual void Clear();

protected:
    QList<AbstractAnimation *> m_group;
};


//! \brief A group of animations to be played sequentially
//! \details When played backwards the last animation will play first (backwards)
class SequentialAnimation : public GroupAnimation
{
    Q_OBJECT
public:
    SequentialAnimation() : GroupAnimation(), m_current(-1)  {}
    virtual void Pulse(int interval);
    virtual bool Start(bool forwards, float speed = 1.0);
    virtual void SetSpeed(float speed);

protected slots:
    virtual void Finished();

protected:
    //! Index of child currently playing
    int m_current;
};


//! \brief A group of animations to be played simultaneously
class ParallelAnimation : public GroupAnimation
{
    Q_OBJECT
public:
    ParallelAnimation() : GroupAnimation(), m_finished(0)  {}
    virtual void Pulse(int interval);
    virtual bool Start(bool forwards, float speed = 1.0);
    virtual void SetSpeed(float speed);

protected slots:
    virtual void Finished();

protected:
    //! Count of child animations that have finished
    int m_finished;
};


//! \brief Specialised animation for panning slideshow images (MythUI doesn't
//! support panning)
class PanAnimation : public Animation
{
public:
    explicit PanAnimation(Slide *image) : Animation(image) {}
    virtual void updateCurrentValue(const QVariant &value);
};


//! \brief A specialised image for slideshows
class Slide : public MythUIImage
{
    Q_OBJECT
public:
    Slide(MythUIType *parent, QString name, MythUIImage *image);
    ~Slide();

    bool       Load(ImageItem *im, int direction = 0, bool waiting = false);
    bool       Display(ImageItem *im);
    ImageItem *GetImageData() const  { return m_data; }
    void       Zoom(int percentage);
    void       SetZoom(float);
    void       SetPan(QPoint value);
    void       Pan(QPoint distance);
    bool       CanZoomIn() const     { return m_zoom < MAX_ZOOM; }
    bool       CanZoomOut() const    { return m_zoom > MIN_ZOOM; }
    void       Clear();
    bool       IsAvailable() const   { return m_isReady; }
    bool       FailedLoad() const    { return m_loadFailed; }
    int        GetDirection() const  { return m_direction; }
    void       Lock(bool set = true) { m_locked = set; }
    bool       IsLocked() const      { return m_locked; }
    void       Pulse();

public slots:
    void       LoadComplete();

signals:
    //! Generated when the last requested image has loaded
    void       ImageLoaded(Slide*);

private:
    //! Separate thread loads images from BE
    ImageLoadingThread *m_ilt;
    //! The image currently loading/loaded
    ImageItem      *m_data;
    //! The most recently requested image. Differs from m_data when skipping
    ImageItem      *m_waitingFor;
    //! Current zoom, 1.0 = fullsize
    float           m_zoom;
    //! True when image is available for display
    bool            m_isReady;
    //! True when image load failed (it's disappeared or BE is down)
    bool            m_loadFailed;
    bool            m_locked;
    //! Navigation that created this image, -1 = Prev, 0 = Update, 1 = Next
    int             m_direction;
    //! Dedicated animation for zoom
    Animation      *m_zoomAnimation;
    //! Dedicated animation for panning
    PanAnimation   *m_panAnimation;
    //! Pan position (0,0) = no pan
    QPoint          m_pan;
};


/*!
 \brief Provides a queue/pool of slides
 \details Slides are cloned from a theme-provided image definition, so are
 created at start-up and re-used (a pool).
 Image requests are assigned to successive slides. When loaded a slide becomes
 available for display in requested order (a queue). When displayed the head
 slide is removed from the queue and returned to the tail for re-use only when
 it is no longer displayed. If a rapid batch of requests fill the buffer then
 subsequent requests overwrite the last slide, discarding the previous image
 (jumping behaviour)
*/
class SlideBuffer : public QObject
{
    Q_OBJECT
public:
    SlideBuffer(MythUIImage *image, int size);
    ~SlideBuffer();
    bool Load(ImageItem *im, int direction);
    void Preload(ImageItem *im);
    Slide* TakeNext();
    int Release(Slide*);

signals:
    //! Signals that buffer has (count) loaded slides awaiting display
    void SlideReady(int count);

private slots:
    int FlushAvailable(Slide*, QString reason = "Loaded");

protected:
    //! Queue of slides awaiting display, loading or spare
    QQueue<Slide*> m_queue;
    //! Queue index of first spare slide, or last slide if none spare
    int m_nextLoad;
};

#endif // GALLERYSLIDE_H
