//! \file
//! \brief Provides transitions for slideshows

#ifndef GALLERYTRANSITIONS_H
#define GALLERYTRANSITIONS_H

#include <utility>

// MythTV headers
#include "galleryslide.h"


//! Available transitions
enum ImageTransitionType : std::uint8_t {
    // First transition must be 0 and useable by all painters
    kNoTransition      = 0,
    kRandomTransition  = 1,
    kBlendTransition   = 2,
    kTwistTransition   = 3,
    kSlideTransition   = 4,
    kZoomTransition    = 5,
    kSpinTransition    = 6,
    kLastTransSentinel = 7  // Must be last
};


//! Base class of an animated transition that can be accelerated & reversed
class Transition : public QObject
{
    Q_OBJECT
public:
    explicit Transition(const QString& name);
    ~Transition() override = default;

    virtual void Start(Slide &from, Slide &to, bool forwards, float speed = 1.0);
    virtual void SetSpeed(float /*speed*/) {}
    virtual void Pulse() = 0;
    virtual void Initialise()          {}
    virtual void Finalise()            {}

protected slots:
    virtual void Finished();

signals:
    void         finished();

protected:
    //! Seconds transition will last
    std::chrono::milliseconds m_duration {1s};
    //! The image currently displayed, which will be replaced
    //! (whatever the transition direction)
    Slide *m_old   {nullptr};
    //! The new image that will replace the current one
    //! (whatever the transition direction)
    Slide *m_new   {nullptr};
    // Transitions play forwards or backwards. Symmetric transitions can
    // define a one-way transition in terms of "prev" & "next" (as in
    // position rather than time). The reverse transition can then be
    // achieved by playing it backwards.
    // When played forwards next replaces prev, ie. prev = old, next = new
    // When played backwards prev replaces next, ie. prev = new, next = old
    //! The image occurring earlier in the slideshow sequence
    Slide *m_prev  {nullptr};
    //! The image occurring later in the slideshow sequence
    Slide *m_next  {nullptr};
};


using TransitionMap = QMap<int, Transition*>;


//! Switches images instantly with no effects
class TransitionNone : public Transition
{
public:
    TransitionNone() : Transition("None") {}
    void Start(Slide &from, Slide &to,
               bool forwards, float speed = 1.0) override; // Transition
    void Pulse()  override {} // Transition
};


//! Abstract class implementing sequential & parallel animations
class GroupTransition : public Transition
{
public:
    GroupTransition(GroupAnimation *animation, const QString& name);
    ~GroupTransition() override;
    void Start(Slide &from, Slide &to,
               bool forwards, float speed = 1.0) override; // Transition
    void SetSpeed(float speed) override; // Transition
    void Pulse() override; // Transition
    void Initialise() override = 0; // Transition
    void Finalise()   override = 0; // Transition

protected:
    GroupAnimation *m_animation {nullptr};
};


//! Image blends into the next by fading
class TransitionBlend : public GroupTransition
{
public:
    TransitionBlend() : GroupTransition(new ParallelAnimation(), 
                                        Transition::tr("Blend")) {}
    void Initialise() override; // GroupTransition
    void Finalise() override; // GroupTransition
};


//! Image twists into the next
class TransitionTwist : public GroupTransition
{
public:
    TransitionTwist() : GroupTransition(new SequentialAnimation(), 
                                        Transition::tr("Twist")) {}
    void Initialise() override; // GroupTransition
    void Finalise() override; // GroupTransition
};


//! Slide on from right, then off to left
class TransitionSlide : public GroupTransition
{
public:
    TransitionSlide() : GroupTransition(new ParallelAnimation(), 
                                        Transition::tr("Slide")) {}
    void Initialise() override; // GroupTransition
    void Finalise() override; // GroupTransition
};


//! Zooms from right, then away to left
class TransitionZoom : public GroupTransition
{
public:
    TransitionZoom() : GroupTransition(new ParallelAnimation(), 
                                       Transition::tr("Zoom")) {}
    void Initialise() override; // GroupTransition
    void Finalise() override; // GroupTransition
};


//! Images blend whilst rotating
class TransitionSpin : public TransitionBlend
{
public:
    TransitionSpin() { setObjectName(Transition::tr("Spin")); }
    void Initialise() override; // TransitionBlend
    void Finalise() override; // TransitionBlend
};


//! Invokes random transitions
class TransitionRandom : public Transition
{
    Q_OBJECT
public:
    explicit TransitionRandom(QList<Transition*>  peers)
        : Transition(Transition::tr("Random")), m_peers(std::move(peers)) {}
    void Start(Slide &from, Slide &to, bool forwards, float speed = 1.0) override; // Transition
    void SetSpeed(float speed) override // Transition
        { if (m_current) m_current->SetSpeed(speed); }
    void Pulse() override // Transition
        { if (m_current) m_current->Pulse(); }
    void Initialise() override // Transition
        { if (m_current) m_current->Initialise(); }
    void Finalise() override // Transition
        { if (m_current) m_current->Finalise(); }

protected slots:
    void Finished() override; // Transition

protected:
    //! Set of distinct transitions
    QList<Transition*> m_peers;
    //! Selected transition
    Transition        *m_current {nullptr};
};


//! Manages transitions available to s psinter
class TransitionRegistry
{
public:
    explicit TransitionRegistry(bool includeAnimations);
    ~TransitionRegistry()    { qDeleteAll(m_map); }
    TransitionMap GetAll() const { return m_map; }
    Transition &Select(int setting);

    //! A transition that updates instantly
    TransitionNone m_immediate;

private:
    //! All possible transitions
    TransitionMap m_map;
};


#endif // GALLERYTRANSITIONS_H
