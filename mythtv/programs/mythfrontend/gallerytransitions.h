//! \file
//! \brief Provides transitions for slideshows

#ifndef GALLERYTRANSITIONS_H
#define GALLERYTRANSITIONS_H

#include <QMap>
#include <QList>

#include <mythcorecontext.h>

#include "galleryslide.h"


//! Available transitions
enum ImageTransitionType {
    // First transition must be useable by all painters
    kNoTransition      = 0,
    kRandomTransition  = 1,
    kBlendTransition   = 2,
    kTwistTransition   = 3,
    kSlideTransition   = 4,
    kZoomTransition    = 5,
    kSpinTransition    = 6,
};


//! Base class of an animated transition that can be accelerated & reversed
class Transition : public QObject
{
    Q_OBJECT
public:
    Transition(QString name)
        : m_duration(gCoreContext->GetNumSetting("GalleryTransitionTime", 1000)),
          m_old(NULL), m_new(NULL), m_prev(NULL), m_next(NULL),
          m_name(name)                 {}
    virtual ~Transition()              {}

    virtual void Start(Slide *from, Slide *to, bool forwards, float speed = 1.0);
    virtual void SetSpeed(float speed) {}
    virtual void Pulse(int interval)   = 0;
    virtual void Initialise()          {}
    virtual void Finalise()            {}
    QString GetName()                  { return m_name; }

protected slots:
    virtual void Finished();

signals:
    void         finished();

protected:
    //! Seconds transition will last
    int m_duration;
    //! The image currently displayed, which will be replaced
    //! (whatever the transition direction)
    Slide *m_old;
    //! The new image that will replace the current one
    //! (whatever the transition direction)
    Slide *m_new;
    // Transitions play forwards or backwards. Symmetric transitions can
    // define a one-way transition in terms of "prev" & "next" (as in
    // position rather than time). The reverse transition can then be
    // achieved by playing it backwards.
    // When played forwards next replaces prev, ie. prev = old, next = new
    // When played backwards prev replaces next, ie. prev = new, next = old
    //! The image occurring earlier in the slideshow sequence
    Slide *m_prev;
    //! The image occurring later in the slideshow sequence
    Slide *m_next;
    //! Name that appears in settings
    QString m_name;
};


typedef QMap<int, Transition*> TransitionMap;


//! Switches images instantly with no effects
class TransitionNone : public Transition
{
public:
    TransitionNone() : Transition("None") {}
    virtual void Start(Slide *from, Slide *to,
                       bool forwards, float speed = 1.0);
    virtual void Pulse(int) {}
};


//! Abstract class implementing sequential & parallel animations
class GroupTransition : public Transition
{
public:
    GroupTransition(GroupAnimation *animation, QString name);
    virtual ~GroupTransition();
    virtual void Start(Slide *from, Slide *to,
                       bool forwards, float speed = 1.0);
    virtual void SetSpeed(float speed);
    virtual void Pulse(int interval);
    virtual void Initialise() = 0;
    virtual void Finalise()    = 0;

protected:
    GroupAnimation *m_animation;
};


//! Image blends into the next by fading
class TransitionBlend : public GroupTransition
{
public:
    TransitionBlend() : GroupTransition(new ParallelAnimation(), tr("Blend")) {}
    virtual void Initialise();
    virtual void Finalise();
};


//! Image twists into the next
class TransitionTwist : public GroupTransition
{
public:
    TransitionTwist() : GroupTransition(new SequentialAnimation(), tr("Twist")) {}
    virtual void Initialise();
    virtual void Finalise();
};


//! Slide on from right, then off to left
class TransitionSlide : public GroupTransition
{
public:
    TransitionSlide() : GroupTransition(new ParallelAnimation(), tr("Slide")) {}
    virtual void Initialise();
    virtual void Finalise();
};


//! Zooms from right, then away to left
class TransitionZoom : public GroupTransition
{
public:
    TransitionZoom() : GroupTransition(new ParallelAnimation(), tr("Zoom")) {}
    virtual void Initialise();
    virtual void Finalise();
};


//! Images blend whilst rotating
class TransitionSpin : public TransitionBlend
{
public:
    TransitionSpin() : TransitionBlend() { m_name = tr("Spin"); }
    virtual void Initialise();
    virtual void Finalise();
};


//! Invokes random transitions
class TransitionRandom : public Transition
{
    Q_OBJECT
public:
    TransitionRandom(TransitionMap peers) : Transition(tr("Random")),
          m_peers(peers.values()), m_current(NULL) {}
    virtual void Start(Slide *from, Slide *to,
                       bool forwards, float speed = 1.0);
    virtual void Pulse(int interval) { if (m_current) m_current->Pulse(interval); }
    virtual void Initialise()        { if (m_current) m_current->Initialise(); }
    virtual void Finalise()          { if (m_current) m_current->Finalise(); }

protected slots:
    void Finished();

protected:
    QList<Transition*> m_peers;
    Transition *m_current;
};


//! Manages transitions available to s psinter
class TransitionRegistry
{
public:
    TransitionRegistry(bool includeAnimations);
    ~TransitionRegistry()    { qDeleteAll(m_map); }
    const TransitionMap GetAll() { return m_map; }
    Transition *Select(int setting);

private:
    TransitionMap m_map;
};


#endif // GALLERYTRANSITIONS_H
