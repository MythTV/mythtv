#include "gallerytransitions.h"

#include <utility>

#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythrandom.h"

#define LOC QString("Transition: ")


Transition::Transition(const QString& name)
  : m_duration(gCoreContext->GetDurSetting<std::chrono::milliseconds>("GalleryTransitionTime", 1s))
{
    setObjectName(name);
}


/*!
 \brief Returns the requested transition or an alternative if it is not suitable for
 the painter
 \param setting The user selected transition setting
 \return Transition The transition object or an alternative
*/
Transition &TransitionRegistry::Select(int setting)
{
    // Guard against garbage
    int value = (setting >= kNoTransition && setting < kLastTransSentinel)
            ? setting : kNoTransition;

    // If chosen transition isn't viable for painter then use previous ones.
    // First transition must always be useable by all painters
    Transition *result = nullptr;
    while (value >= kNoTransition && !result)
        result = m_map.value(value--, nullptr);

    if (result)
        return *result;

    LOG(VB_GENERAL, LOG_CRIT,
        LOC + QString("No transitions found for setting %1").arg(setting));

    return m_immediate;
}


/*!
 \brief Constructor.
 \param includeAnimations Whether to use animated transitions (zoom, rotate)
*/
TransitionRegistry::TransitionRegistry(bool includeAnimations)
{
    // Create all non-animated transitions to be used by Random
    m_map.insert(kBlendTransition, new TransitionBlend());
    m_map.insert(kSlideTransition, new TransitionSlide());

    // Create all animated transitions to be used by Random
    if (includeAnimations)
    {
        m_map.insert(kTwistTransition, new TransitionTwist());
        m_map.insert(kZoomTransition,  new TransitionZoom());
        m_map.insert(kSpinTransition,  new TransitionSpin());
    }

    // Create random set from those already defined
    m_map.insert(kRandomTransition, new TransitionRandom(m_map.values()));

    // Create other transitions that aren't to be used by Random
    m_map.insert(kNoTransition, new TransitionNone());
}


/*!
 \brief Start base transition
 \param from Image currently displayed
 \param to Image to be displayed
 \param forwards Direction
 \param speed Unused
*/
void Transition::Start(Slide &from, Slide &to,
                       bool forwards, float speed)
{
    LOG(VB_FILE, LOG_DEBUG, LOC +
        QString("Starting transition %1 -> %2 (forwards= %3, speed= %4)")
        .arg(from.objectName(), to.objectName()).arg(forwards).arg(speed));

    m_old = &from;
    m_new = &to;

    // When playing forwards next image replaces prev image
    // When playing backwards prev image replaces next image
    m_prev  = forwards ? m_old : m_new;
    m_next = forwards ? m_new : m_old;

    // Set up transition
    Initialise();

    // Make new image visible
    m_new->SetVisible(true);
}


/*!
 \brief Transition has completed
*/
void Transition::Finished()
{
    // Hide old image before it is reset
    m_old->SetVisible(false);

    // Undo transition effects
    Finalise();

    LOG(VB_FILE, LOG_DEBUG, LOC +
        QString("Finished transition to %1").arg(m_new->objectName()));

    emit finished();
}


/*!
 \brief Switch images
 \param from Image currently displayed
 \param to Image to be displayed
 \param forwards Direction
 \param speed Factor, 1.0 = real-time
*/
void TransitionNone::Start(Slide &from, Slide &to,
                           bool forwards, float speed)
{
    Transition::Start(from, to, forwards, speed);

    // Finish immediately
    Finished();
}


/*!
 \brief Create group transition
 \param animation Group animation
 \param name The name for this transition. This string will be used in
             log messages.
*/
GroupTransition::GroupTransition(GroupAnimation *animation, const QString& name)
    : Transition(name), m_animation(animation)
{
    // Complete transition when the group finishes
    connect(m_animation, &AbstractAnimation::finished, this, &GroupTransition::Finished);
}


/*!
 \brief Destroy group transition
*/
GroupTransition::~GroupTransition()
{
    // Group owns animation
    delete m_animation;
}


/*!
 \brief Start group transition
 \param from Image currently displayed
 \param to Image to be displayed
 \param forwards Direction
 \param speed Factor, 1.0 = real-time
*/
void GroupTransition::Start(Slide &from, Slide &to,
                            bool forwards, float speed)
{
    // Clear previous transition
    m_animation->Clear();

    Transition::Start(from, to, forwards, speed);

    // Start group
    m_animation->Start(forwards, speed);
}


/*!
 \brief Change transition speed
 \param speed Factor, 1x, 2x etc.
*/
void GroupTransition::SetSpeed(float speed)
{
    if (m_animation)
        m_animation->SetSpeed(speed);
}


/*! \brief Update group transition
*/
void GroupTransition::Pulse()
{
    if (m_animation)
        m_animation->Pulse();
}


/*!
 \brief Set up Blend transition
*/
void TransitionBlend::Initialise()
{
    // Fade out prev image
    auto *oldPic = new Animation(m_prev, Animation::Alpha);
    oldPic->Set(255, 0, m_duration, QEasingCurve::InOutQuad);
    m_animation->Add(oldPic);

    // Fade in next image
    auto *newPic = new Animation(m_next, Animation::Alpha);
    newPic->Set(0, 255, m_duration, QEasingCurve::InOutQuad);
    m_animation->Add(newPic);

    // Hide new image until it's needed
    m_new->SetAlpha(0);
}


/*!
 \brief Reset Blend transition
*/
void TransitionBlend::Finalise()
{
    // Undo effects of the transition
    m_old->SetAlpha(255);
}


/*!
 \brief Set up Twist transition
*/
void TransitionTwist::Initialise()
{
    // Reduce hzoom of left image to nothing
    auto *oldPic = new Animation(m_prev, Animation::HorizontalZoom);
    oldPic->Set(1.0, 0.0, m_duration / 2, QEasingCurve::InQuart);
    m_animation->Add(oldPic);

    // Increase hzoom of right image from nothing to full
    auto *newPic = new Animation(m_next, Animation::HorizontalZoom);
    newPic->Set(0.0, 1.0, m_duration / 2, QEasingCurve::OutQuart);
    m_animation->Add(newPic);

    // Hide new image until it's needed
    m_new->SetHorizontalZoom(0);
}


/*!
 \brief Reset Twist transition
*/
void TransitionTwist::Finalise()
{
    // Undo effects of the transition
    m_old->SetHorizontalZoom(1.0);
}


/*!
 \brief Set up Slide transition
*/
void TransitionSlide::Initialise()
{
    // Both images are same size
    int width = m_old->GetArea().width();

    // Slide off to left
    auto *oldPic = new Animation(m_prev, Animation::Position);
    oldPic->Set(QPoint(0, 0), QPoint(-width, 0), m_duration, QEasingCurve::InOutQuart);
    m_animation->Add(oldPic);

    // Slide in from right
    auto *newPic = new Animation(m_next, Animation::Position);
    newPic->Set(QPoint(width, 0), QPoint(0, 0), m_duration, QEasingCurve::InOutQuart);
    m_animation->Add(newPic);

    // Hide new image until it's needed
    m_new->SetPosition(width, 0);
}


/*!
 \brief Reset Slide transition
*/
void TransitionSlide::Finalise()
{
    // Undo effects of the transition
    m_old->SetPosition(0,0);
}


/*!
 \brief Set up Zoom transition
*/
void TransitionZoom::Initialise()
{
    // Both images are same size
    int width = m_old->GetArea().width();

    // Zoom away to left
    auto *oldZoom = new Animation(m_prev, Animation::Zoom);
    oldZoom->Set(1.0, 0.0, m_duration, QEasingCurve::OutQuad);
    m_animation->Add(oldZoom);

    auto *oldMove = new Animation(m_prev, Animation::Position);
    oldMove->Set(QPoint(0, 0), QPoint(-width / 2, 0), m_duration,
                 QEasingCurve::InQuad);
    m_animation->Add(oldMove);

    // Zoom in from right
    auto *newZoom = new Animation(m_next, Animation::Zoom);
    newZoom->Set(0.0, 1.0, m_duration, QEasingCurve::InQuad);
    m_animation->Add(newZoom);

    auto *newMove = new Animation(m_next, Animation::Position);
    newMove->Set(QPoint(width / 2, 0), QPoint(0, 0), m_duration,
                 QEasingCurve::OutQuad);
    m_animation->Add(newMove);

    // Hide new image until it's needed
    m_new->SetZoom(0.0);
}


/*!
 \brief Reset Zoom transition

*/
void TransitionZoom::Finalise()
{
    // Undo effects of the transition
    m_old->SetZoom(1.0);
    m_old->SetPosition(0, 0);
}


/*!
 \brief Set up Spin transition
*/
void TransitionSpin::Initialise()
{
    // Set up blend
    TransitionBlend::Initialise();

    // Add simultaneous spin
    auto *an = new Animation(m_prev, Animation::Angle);
    an->Set(0.0, 360.1, m_duration, QEasingCurve::InOutQuad);
    m_animation->Add(an);

    an = new Animation(m_next, Animation::Angle);
    an->Set(0.0, 360.1, m_duration, QEasingCurve::InOutQuad);
    m_animation->Add(an);

    // Zoom prev away, then back
    auto *seq = new SequentialAnimation();
    m_animation->Add(seq);

    an = new Animation(m_prev, Animation::Zoom);
    an->Set(1.0, 0.5, m_duration / 2, QEasingCurve::InOutQuad);
    seq->Add(an);

    an = new Animation(m_prev, Animation::Zoom);
    an->Set(0.5, 1.0, m_duration / 2, QEasingCurve::InOutQuad);
    seq->Add(an);

    // Zoom next away, then back
    seq = new SequentialAnimation();
    m_animation->Add(seq);

    an = new Animation(m_next, Animation::Zoom);
    an->Set(1.0, 0.5, m_duration / 2, QEasingCurve::InOutQuad);
    seq->Add(an);

    an = new Animation(m_next, Animation::Zoom);
    an->Set(0.5, 1.0, m_duration / 2, QEasingCurve::InOutQuad);
    seq->Add(an);
}


/*!
 \brief Reset Spin transition

*/
void TransitionSpin::Finalise()
{
    // Undo blend
    TransitionBlend::Finalise();

    // Undo effects of the transition
    m_old->SetZoom(1.0);
    m_old->SetAngle(0.0);
}


/*!
 \brief Starts a random transition from a set of its peer transitions,
 \param from Image currently displayed
 \param to Image to be displayed
 \param forwards Direction
 \param speed Unused
*/
void TransitionRandom::Start(Slide &from, Slide &to, bool forwards, float speed)
{
    // Select a random peer.
    int rand = MythRandom(0, m_peers.size() - 1);
    m_current = m_peers[rand];
    // Invoke peer
    connect(m_current, &Transition::finished, this, &TransitionRandom::Finished);
    m_current->Start(from, to, forwards, speed);
}


/*!
 \brief Invoked when peer transition completes
*/
void TransitionRandom::Finished()
{
    // Clean up
    disconnect(m_current, nullptr, nullptr, nullptr);
    m_current = nullptr;
    emit finished();
}


