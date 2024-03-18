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

#include <QQueue>

#include "libmythmetadata/imagetypes.h"
#include "libmythui/mythuiimage.h"

// Min/max zoom extents available in slideshow
#define MIN_ZOOM (0.1F)
#define MAX_ZOOM (20.0F)

class Slide;

//! \brief Base animation class that is driven by a Myth pulse and implements
//! variable speed
class AbstractAnimation : public QObject
{
    Q_OBJECT
public:
    AbstractAnimation() = default;
    virtual void Start(bool forwards, float speed = 1.0);
    virtual void Stop()                 { m_running = false; }
    virtual void SetSpeed(float speed) { m_speed = speed; }
    virtual void Pulse() = 0;
    virtual void Clear()                {}

protected slots:
    //! To be called when animation completes
    virtual void Finished()  { m_running = false; emit finished(); }

signals:
    //! Signals animation has finished
    void         finished();

protected:
    bool  m_forwards {true}; //!< Play direction
    bool  m_running {false}; //!< True whilst animation is active
    float m_speed   {0.0F};  //!< Real-time = 1.0, Double-speed = 2.0
};


//! \brief A single animation controlling alpha, zoom, rotation and position
//! \note This is almost compatible with MythUIAnimation (they could be merged)
class Animation : public AbstractAnimation, public QVariantAnimation
{
public:
    //! Supported effects
    enum Type : std::uint8_t {None, Alpha, Position, Zoom, HorizontalZoom, VerticalZoom, Angle};

    /*!
      \brief Create simple animation
      \param image Image to be animated
      \param type Effect to be animated
    */
    explicit Animation(Slide *image, Type type = Alpha)
        : m_parent(image), m_type(type) {}
    void Start(bool forwards = true, float speed = 1.0) override; // AbstractAnimation
    void Pulse() override; // AbstractAnimation
    void Set(const QVariant& from, const QVariant& to,
             std::chrono::milliseconds duration = 500ms,
             const QEasingCurve& curve = QEasingCurve::InOutCubic,
             UIEffects::Centre centre = UIEffects::Middle);
    void updateCurrentValue(const QVariant &value) override; // QVariantAnimation

protected:
    //! Image to be animated
    // Should be MythUItype but that impacts elsewhere: SetZoom must become
    // virtual, which causes compiler (fn hiding) warnings in subtitles
    Slide            *m_parent  {nullptr};
    Type              m_type;
    UIEffects::Centre m_centre  {UIEffects::Middle};
    //! Current millisec position within animation, 0..duration.
    //! Decreases duration..0 when playing backwards
    std::chrono::milliseconds m_elapsed {0ms};
    std::chrono::milliseconds m_lastUpdate { MythDate::currentMSecsSinceEpochAsDuration() };
};


//! \brief Abstract class for groups of animations
class GroupAnimation : public AbstractAnimation
{
public:
    GroupAnimation() = default;
    ~GroupAnimation() override                        { GroupAnimation::Clear(); }
    void Pulse() override                     = 0; // AbstractAnimation
    void Start(bool forwards, float speed = 1.0) override      // AbstractAnimation
        { AbstractAnimation::Start(forwards, speed); }
    void SetSpeed(float speed) override                        // AbstractAnimation
        { AbstractAnimation::SetSpeed(speed); }
    virtual void Add(AbstractAnimation *child);
    void Clear() override; // AbstractAnimation

protected:
    QList<AbstractAnimation *> m_group;
};


//! \brief A group of animations to be played sequentially
//! \details When played backwards the last animation will play first (backwards)
class SequentialAnimation : public GroupAnimation
{
    Q_OBJECT
public:
    SequentialAnimation() = default;
    void Pulse() override; // GroupAnimation
    void Start(bool forwards, float speed = 1.0) override; // GroupAnimation
    void SetSpeed(float speed) override; // GroupAnimation

protected slots:
    void Finished() override; // AbstractAnimation

protected:
    int m_current {-1}; //!< Index of child currently playing
};


//! \brief A group of animations to be played simultaneously
class ParallelAnimation : public GroupAnimation
{
    Q_OBJECT
public:
    ParallelAnimation() = default;
    void Pulse() override; // GroupAnimation
    void Start(bool forwards, float speed = 1.0) override; // GroupAnimation
    void SetSpeed(float speed) override; // GroupAnimation

protected slots:
    void Finished() override; // AbstractAnimation

protected:
    int m_finished {0}; //!< Count of child animations that have finished
};


//! \brief Specialised animation for panning slideshow images (MythUI doesn't
//! support panning)
class PanAnimation : public Animation
{
public:
    explicit PanAnimation(Slide *image) : Animation(image) {}
    void updateCurrentValue(const QVariant &value) override; // Animation
};


//! \brief A specialised image for slideshows
class Slide : public MythUIImage
{
    Q_OBJECT
public:
    Slide(MythUIType *parent, const QString& name, MythUIImage *image);
    ~Slide() override;

    void      Clear();
    bool      LoadSlide(const ImagePtrK& im, int direction = 0, bool notifyCompletion = false);
    ImagePtrK GetImageData() const  { return m_data; }
    void      Zoom(int percentage);
    void      SetZoom(float zoom);
    void      Pan(QPoint offset);
    void      SetPan(QPoint pos);
    bool      CanZoomIn() const     { return m_zoom < MAX_ZOOM; }
    bool      CanZoomOut() const    { return m_zoom > MIN_ZOOM; }
    bool      IsEmpty() const       { return m_state == kEmpty || !m_waitingFor; }
    bool      IsLoaded() const      { return m_state >= kLoaded && m_waitingFor; }
    bool      FailedLoad() const    { return m_state == kFailed; }
    int       GetDirection() const  { return m_direction; }
    void      Pulse() override; // MythUIImage

    QChar     GetDebugState() const;

public slots:
    void      SlideLoaded();

signals:
    //! Generated when the last requested image has loaded
    void      ImageLoaded(Slide*);

private:
    enum SlideState : std::uint8_t { kEmpty, kLoading, kLoaded, kFailed }; // Order is significant

    SlideState    m_state         {kEmpty};  //!< Slide validity
    ImagePtrK     m_data          {nullptr}; //!< The image currently loading/loaded
    //! The most recently requested image. Null for preloads. Differs from m_data when skipping
    ImagePtrK     m_waitingFor    {nullptr};
    float         m_zoom          {1.0F};    //!< Current zoom, 1.0 = fullsize
    //! Navigation that created this image, -1 = Prev, 0 = Update, 1 = Next
    int           m_direction     {0};
    Animation    *m_zoomAnimation {nullptr}; //!< Dedicated animation for zoom, if supported
    PanAnimation *m_panAnimation  {nullptr}; //!< Dedicated animation for panning, if supported
    QPoint        m_pan           {0,0};     //!< Pan position (0,0) = no pan
};


/*!
 \brief Provides a queue/pool of slides
 \details Slides are cloned from a theme-provided image definition, so are
 created at start-up and re-used (a pool).
 Image requests are assigned to successive slides. When loaded a slide becomes
 available for display in requested order (a queue).
 The head slide is the one displayed, its successor is also displayed during a
 transition. When a transition completes the head slide is removed and returned
 to the tail for re-use.
 If a rapid batch of requests fill the buffer then subsequent requests overwrite
 the last slide, discarding the previous image (jumping behaviour).
*/
class SlideBuffer : public QObject
{
    Q_OBJECT
public:
    SlideBuffer() = default;
    ~SlideBuffer() override;

    void   Initialise(MythUIImage &image);
    void   Teardown();
    bool   Load(const ImagePtrK& im, int direction);
    void   Preload(const ImagePtrK& im);
    void   ReleaseCurrent();
    Slide &GetCurrent()
    {
        QMutexLocker lock(&m_mutexQ);
        return *(m_queue.head());
    }

    Slide  &GetNext()
    {
        QMutexLocker lock(&m_mutexQ);
        return *(m_queue.at(1));
    }

signals:
    //! Signals that buffer has (count) loaded slides awaiting display
    void SlideReady(int count);

private slots:
    void Flush(Slide *slide, const QString& reason);
    void Flush(Slide *slide);

protected:
    QString BufferState();

    // Must be recursive to enable Flush->signal->Get whilst retaining lock
    QRecursiveMutex m_mutexQ;      //!< Queue protection
    QQueue<Slide*> m_queue;        //!< Queue of slides
    int            m_nextLoad {0}; //!< Index of first spare slide, (or last slide if none spare)
};

#endif // GALLERYSLIDE_H
