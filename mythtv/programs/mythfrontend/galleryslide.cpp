// C++
#include <algorithm>
#include <cmath>       // for roundf

// MythTV
#include "libmythbase/mythlogging.h"
#include "libmythbase/sizetliteral.h"
#include "libmythmetadata/imagemetadata.h"
#include "libmythui/mythmainwindow.h"

// MythFrontend
#include "galleryslide.h"

#define LOC QString("Slide: ")
#define SBLOC QString("SlideBuffer: ")


// Number of slides to use for buffering image requests.
// When browsing quickly the buffer will load consecutive slides until it fills.
// If too large, rapid browsing will be stodgy (sequential access) for images that
// aren't cached (Cached images are always fast).
// If too small, rapid browsing will result in skipping slides rather than flicking
// quickly through them.
// Minimum is 4: 3 for displaying a transition, 1 to handle load requests
static constexpr size_t SLIDE_BUFFER_SIZE { 9 };


/*!
 \brief Initialise & start base animation
 \param forwards Direction
 \param speed Factor, 1 = real-time, 2 = double-time
*/
void AbstractAnimation::Start(bool forwards, float speed)
{
    m_forwards = forwards;
    m_speed    = speed;
    m_running  = true;
}


/*!
 \brief Initialises an animation
 \param from Start value
 \param to End value
 \param duration Animation duration
 \param curve Easing curve governing animation
 \param centre Zoom centre
*/
void Animation::Set(const QVariant& from, const QVariant& to,
                    std::chrono::milliseconds duration,
                    const QEasingCurve& curve, UIEffects::Centre centre)
{
    setStartValue(from);
    setEndValue(to);
    m_centre = centre;
    setDuration(duration.count());
    setEasingCurve(curve);
}


/*!
 \brief Start a single animation
 \param forwards Direction
 \param speed Speed factor, 1x, 2x etc
*/
void Animation::Start(bool forwards, float speed)
{
    auto duration_ms = std::chrono::milliseconds(duration());
    if (duration_ms == 0ms)
        return;

    m_elapsed = forwards ? 0ms : duration_ms;
    setCurrentTime(m_elapsed.count());

    AbstractAnimation::Start(forwards, speed);
}


/*! \brief Progress single animation
*/
void Animation::Pulse()
{
    if (!m_running)
        return;

    std::chrono::milliseconds current = MythDate::currentMSecsSinceEpochAsDuration();
    std::chrono::milliseconds interval = std::min(current - m_lastUpdate, 50ms);
    m_lastUpdate = current;
    m_elapsed += (m_forwards ? interval : -interval) * static_cast<int>(m_speed);
    setCurrentTime(m_elapsed.count());

    // Detect completion
    if ((m_forwards && m_elapsed.count() >= duration()) || (!m_forwards && m_elapsed <= 0ms))
        Finished();
}


/*!
 \brief Update animated value
 \param value Latest value, derived from animation time & easing curve
*/
void Animation::updateCurrentValue(const QVariant &value)
{
    if (m_parent && m_running)
    {
        m_parent->SetCentre(m_centre);

        switch (m_type)
        {
        case None:           break;
        case Position:       m_parent->SetPosition(value.toPoint()); break;
        case Alpha:          m_parent->SetAlpha(value.toInt()); break;
        case Zoom:           m_parent->SetZoom(value.toFloat()); break;
        case HorizontalZoom: m_parent->SetHorizontalZoom(value.toFloat()); break;
        case VerticalZoom:   m_parent->SetVerticalZoom(value.toFloat()); break;
        case Angle:          m_parent->SetAngle(value.toFloat()); break;
        }
    }
}


/*!
 \brief Add child animation to group
 \param child
*/
void GroupAnimation::Add(AbstractAnimation *child)
{
    // Signal group when child completes
    m_group.append(child);
    connect(child, &AbstractAnimation::finished, this, &GroupAnimation::Finished);
}


/*!
 \brief Delete all child animations
*/
void GroupAnimation::Clear()
{
    qDeleteAll(m_group);
    m_group.clear();
}


/*! \brief Progress sequential animation
*/
void SequentialAnimation::Pulse()
{
    if (!m_running || m_current < 0 || m_current >= m_group.size())
        return;

    // Pulse current running child
    m_group.at(m_current)->Pulse();
}


/*!
 \brief Start sequential animation
 \param forwards Direction. If false, the last child will be played (backwards) first
 \param speed Factor, 1x, 2x etc
*/
void SequentialAnimation::Start(bool forwards, float speed)
{
    if (m_group.empty())
        return;

    m_current = forwards ? 0 : m_group.size() - 1;

    // Start group, then first child
    GroupAnimation::Start(forwards, speed);
    m_group.at(m_current)->Start(m_forwards, m_speed);
}


/*!
 \brief Change speed of current child animation and all subsequent ones
 \param speed New speed factor
*/
void SequentialAnimation::SetSpeed(float speed)
{
    // Set group speed for subsequent children
    GroupAnimation::SetSpeed(speed);

    // Set active child
    if (!m_running || m_current < 0 || m_current >= m_group.size())
        return;

    m_group.at(m_current)->SetSpeed(speed);
}


/*!
 \brief A child animation has completed
*/
void SequentialAnimation::Finished()
{
    bool finished { false };

    // Finish group when last child finishes
    if (m_forwards)
    {
        m_current++;
        finished = (m_current == m_group.size());
    }
    else
    {
        m_current--;
        finished = (m_current < 0);
    }

    if (finished)
        AbstractAnimation::Finished();
    else
        // Start next child
        m_group.at(m_current)->Start(m_forwards, m_speed);
}


/*! \brief Progress parallel animations
*/
void ParallelAnimation::Pulse()
{
    if (m_running)
    {
        // Pulse all children
        for (AbstractAnimation *animation : std::as_const(m_group))
            animation->Pulse();
    }
}


/*!
 \brief Start parallel group. All children play simultaneously
 \param forwards Direction
 \param speed Factor, 1x, 2x etc
*/
void ParallelAnimation::Start(bool forwards, float speed)
{
    if (m_group.empty())
        return;

    m_finished = m_group.size();

    // Start group, then all children
    GroupAnimation::Start(forwards, speed);
    for (AbstractAnimation *animation : std::as_const(m_group))
        animation->Start(m_forwards, m_speed);
}


/*!
 \brief Change speed of group and all child animations
 \param speed New speed factor
*/
void ParallelAnimation::SetSpeed(float speed)
{
    // Set group speed, then all children
    GroupAnimation::SetSpeed(speed);
    for (AbstractAnimation *animation : std::as_const(m_group))
        animation->SetSpeed(m_speed);
}


/*!
 \brief A child animation has completed
*/
void ParallelAnimation::Finished()
{
    // Finish group when last child finishes
    if (--m_finished == 0)
        AbstractAnimation::Finished();
}


/*!
 \brief Update pan value
 \param value Pan position
*/
void PanAnimation::updateCurrentValue(const QVariant &value)
{
    if (m_parent && m_running)
    {
        Slide *image = m_parent;
        image->SetPan(value.toPoint());
    }
}


/*!
 \brief Clone slide from a theme MythUIImage
 \param parent Parent widget
 \param name Slide name
 \param image Theme MythUIImage to clone
*/
Slide::Slide(MythUIType *parent, const QString& name, MythUIImage *image)
    : MythUIImage(parent, name)
{
    // Clone from image
    CopyFrom(image);

    // Null parent indicates we should become a child of the image (after
    // copy to avoid recursion)
    if (!parent)
    {
        // Slides sit on top of parent image area
        SetArea(MythRect(image->GetArea().toQRect()));
        m_area.moveTo(0, 0);
        setParent(image);
        m_parent = image;
        image->AddChild(this);
    }

    // Provide animations for pan & zoom
    if (GetPainter()->SupportsAnimation())
    {
        m_zoomAnimation = new Animation(this, Animation::Zoom);
        m_panAnimation  = new PanAnimation(this);
    }

    connect(this, &MythUIImage::LoadComplete, this, &Slide::SlideLoaded);
}


/*!
 \brief Destructor
*/
Slide::~Slide()
{
    delete m_zoomAnimation;
    delete m_panAnimation;
    LOG(VB_GUI, LOG_DEBUG, "Deleted Slide " + objectName());
}


/*!
 \brief Reset slide to unused state
*/
void Slide::Clear()
{
    m_state = kEmpty;
    m_data.clear();
    m_waitingFor.clear();
    SetCropRect(0, 0, 0, 0);
    SetVisible(false);
}


/*!
   \brief Return debug status
   \return Char representing state
 */
QChar Slide::GetDebugState() const
{
    switch (m_state)
    {
    case kEmpty:   return 'e';
    case kFailed:  return 'f';
    case kLoaded:  return m_waitingFor ? 'r' : 'a';
    case kLoading: return m_waitingFor ? 'l' : 'p';
    }
    return '?';
}


/*!
 \brief Load slide with an image
 \details If the requested image is already loaded (due to pre-load) then returns
 immediately. Otherwise it initiates an image load in a child thread, provided
 an image is not already loading (multiple requests due to skipping). If loader
 is busy then the most recent request is queued. Intervening requests are discarded.
 \param im The image to load
 \param direction Navigation that caused this load. Determines transition direction
 \param notifyCompletion if True, emits a signal when the last requested image
 has loaded
 \return bool True if the requested image is already loaded
*/
bool Slide::LoadSlide(const ImagePtrK& im, int direction, bool notifyCompletion)
{
    m_direction  = direction;
    m_waitingFor = notifyCompletion ? im : ImagePtrK();

    if (im == m_data)
    {
        LOG(VB_FILE, LOG_DEBUG, LOC + QString("Already loading/loaded %1 in %2")
            .arg(im->m_filePath, objectName()));

        if (m_state >= kLoaded && notifyCompletion)
            // Image has been pre-loaded
            emit ImageLoaded(this);

        return (m_state >= kLoaded);
    }

    // Is a different image loading ?
    if (m_state == kLoading)
    {
        // Can't abort image loads, so must wait for it to finish
        // before starting new load
        m_waitingFor = im;

        LOG(VB_FILE, LOG_DEBUG, LOC + QString("Postponing load of %1 in %2")
            .arg(im->m_filePath, objectName()));

        return false;
    }

    // Start load
    m_data  = im;
    m_state = kLoading;

    if (im->m_type == kVideoFile)
    {
        // Use thumbnail, which has already been orientated
        SetFilename(im->m_thumbNails.at(0).second);
        SetOrientation(1);
    }
    else
    {
        // Load image, compensating for any Qt auto-orientation
        SetFilename(im->m_url);
        SetOrientation(Orientation(m_data->m_orientation).GetCurrent(true));
    }

    // Load in background
    Load(true);
    return false;
}


/*!
 \brief An image has completed loading
 \details If the loaded image matches the most recently requested and client is
 waiting then signals that the slide is ready. Otherwise starts loading the
 latest requested image, if different. Superseded loads are discarded.
*/
void Slide::SlideLoaded()
{
    m_state = m_images[0] ? kLoaded : kFailed;
    if (m_state == kFailed)
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to load %1").arg(m_data->m_filePath));

    // Ignore superseded requests and preloads
    if (m_data == m_waitingFor)
    {
        // Loaded image is the latest requested
        emit ImageLoaded(this);
    }
    else if (m_waitingFor)
    {
        LOG(VB_FILE, LOG_DEBUG, LOC + QString("Starting delayed load %1")
            .arg(m_waitingFor->m_filePath));

        // Start latest postponed load
        LoadSlide(m_waitingFor, m_direction, true);
    }
}


/*!
 \brief Initiate slide zoom
 \param percentage Factor of current zoom, ie. 100% = double-size. Sentinel
 0% = Reset to default/fullscreen
*/
void Slide::Zoom(int percentage)
{
    // Sentinel indicates reset to default zoom
    float newZoom = (percentage == 0)
            ? 1.0F
            : std::clamp(m_zoom * (1.0F + percentage / 100.0F), MIN_ZOOM, MAX_ZOOM);
    if (newZoom != m_zoom)
    {
        if (m_zoomAnimation)
        {
            m_zoomAnimation->Set(m_zoom, newZoom, 250ms, QEasingCurve::OutQuad);
            m_zoomAnimation->Start();
        }
        else
        {
            SetZoom(newZoom);
        }
    }
}


/*!
 \brief Sets slide zoom
 \details Applies zoom immediately
 \sa Slide::Zoom
 \param zoom New zoom level, 1.0 = full-size
*/
void Slide::SetZoom(float zoom)
{
    m_zoom          = zoom;
    m_effects.m_hzoom = m_effects.m_vzoom = zoom;

    // TODO
    // MythUIImage displaces widget or doesn't centre for some combinations of
    // zoom centre/cropping so frig centre for now.
    m_effects.m_centre = zoom < 1.0F ? UIEffects::Middle : UIEffects::TopLeft;

    SetPan(m_pan);
}


/*!
 \brief Initiate pan
 \param offset Offset to apply to current position. Sentinel (0,0) resets pan.
*/
void Slide::Pan(QPoint offset)
{
    // Panning only possible when zoomed in
    if (m_zoom > 1.0F)
    {
        QPoint start = m_pan;

        // Sentinel indicates reset to centre
        // Panning is applied to original (unzoomed) image co-ords.
        // Adjust offset for zoom so that pan moves a constant screen distance rather
        // than constant image distance
        QPoint dest = offset.isNull() ? QPoint(0, 0) : start + offset / m_zoom;

        if (m_panAnimation)
        {
            m_panAnimation->Set(start, dest, 250ms, QEasingCurve::Linear);
            m_panAnimation->Start();
        }
        else
        {
            SetPan(dest);
        }
    }
}


/*!
 \brief Sets slide pan
 \details Applies pan immediately
 \sa Slide::Pan
 \param pos New pan offset
*/
void Slide::SetPan(QPoint pos)
{
    if (m_state == kFailed)
    {
        m_pan = pos;
        return;
    }

    // Determine zoom of largest dimension
    QRect imageArea = m_images[m_curPos]->rect();
    float hRatio    = float(imageArea.height()) / m_area.height();
    float wRatio    = float(imageArea.width()) / m_area.width();
    float ratio     = std::max(hRatio, wRatio); // TODO create a Rational number class

    if (m_zoom != 0.0F)
        ratio /= m_zoom;

    // Determine crop area
    int h = std::min(int(roundf(m_area.height() * ratio)), imageArea.height());
    int w = std::min(int(roundf(m_area.width() * ratio)), imageArea.width());
    int x = imageArea.center().x() - (w / 2);
    int y = imageArea.center().y() - (h / 2);

    // Constrain pan to boundaries
    int limitX = (imageArea.width() - w) / 2;
    int limitY = (imageArea.height() - h) / 2;
    m_pan.setX(std::clamp(pos.x(), -limitX, limitX));
    m_pan.setY(std::clamp(pos.y(), -limitY, limitY));

    SetCropRect(x + m_pan.x(), y + m_pan.y(), w, h);
    SetRedraw();
}


/*!
 \brief Update pan & zoom animations
*/
void Slide::Pulse()
{
    // Update zoom/pan animations
    if (m_zoomAnimation)
        m_zoomAnimation->Pulse();

    if (m_panAnimation)
        m_panAnimation->Pulse();
}


SlideBuffer::~SlideBuffer()
{
    LOG(VB_GUI, LOG_DEBUG, "Deleted Slidebuffer");
}


void SlideBuffer::Teardown()
{
    QMutexLocker lock(&m_mutexQ);
    for (Slide *s : std::as_const(m_queue))
        s->Clear();
    LOG(VB_GUI, LOG_DEBUG, "Aborted Slidebuffer");
}


/*!
 \brief Construct buffer
 \details Slides are cloned from an XML image and become children of it.
 \param image Parent image used as a template for slides
*/
void SlideBuffer::Initialise(MythUIImage &image)
{
    // Require at least 4 slides: 2 for transitions, 1 to handle further requests
    // and 1 to prevent solitary slide from being used whilst it is loading
    size_t size = std::max(SLIDE_BUFFER_SIZE, 4_UZ);

    // Fill buffer with slides cloned from the XML image widget

    // Create first as a child of the XML image.
    auto *slide = new Slide(nullptr, "slide0", &image);

    // Buffer is notified when it has loaded image
    connect(slide, &Slide::ImageLoaded,
            this, qOverload<Slide*>(&SlideBuffer::Flush));

    m_queue.enqueue(slide);

    // Rest are simple clones of first
    for (size_t i = 1; i < size; ++i)
    {
        slide = new Slide(&image, QString("slide%1").arg(i), slide);

        // All slides (except first) start off hidden
        slide->SetVisible(false);

        // Buffer is notified when it has loaded image
        connect(slide, &Slide::ImageLoaded,
                this, qOverload<Slide*>(&SlideBuffer::Flush));

        m_queue.enqueue(slide);
    }

    m_nextLoad = 1;
}


/*!
 * \brief Determines buffer state for debug logging
 * \return String showing state of each slide
 */
QString SlideBuffer::BufferState()
{
    QMutexLocker lock(&m_mutexQ);

    QString state;
    for (int i = 0; i < m_queue.size(); ++i)
    {
        QChar code(m_queue.at(i)->GetDebugState());
        state += (i == m_nextLoad ? code.toUpper() : code);
    }
    return QString("[%1] (%2)").arg(state, m_queue.head()->objectName());
}


/*!
 \brief Assign an image to next available slide, start loading and signal
 when done.
 \param im Image to load
 \param direction Navigation causing the load
 \return bool True if image is already loaded
*/
bool SlideBuffer::Load(const ImagePtrK& im, int direction)
{
    if (!im)
        return false;

    QMutexLocker lock(&m_mutexQ);

    // Start loading image in next available slide
    Slide *slide = m_queue.at(m_nextLoad);

    // Further load requests will go to same slide if no free ones are available
    if (m_nextLoad < m_queue.size() - 1)
        ++m_nextLoad;

    LOG(VB_FILE, LOG_DEBUG, SBLOC + QString("Loading %1 in %2, %3")
        .arg(im->m_filePath, slide->objectName(), BufferState()));

    return slide->LoadSlide(im, direction, true);
}


/*!
 \brief Load an image in next available slide
 \param im Image to load
*/
void SlideBuffer::Preload(const ImagePtrK& im)
{
    if (!im)
        return;

    QMutexLocker lock(&m_mutexQ);

    // Start loading image in next available slide
    Slide *slide = m_queue.at(m_nextLoad);

    LOG(VB_FILE, LOG_DEBUG, SBLOC + QString("Preloading %1 in %2, %3")
        .arg(im->m_filePath, slide->objectName(), BufferState()));

    // Load silently
    slide->LoadSlide(im);
}


/*!
 \brief Move head slide to back of queue and flush waiting slides
 \return int Number of slides waiting for display at head
*/
void SlideBuffer::ReleaseCurrent()
{
    QMutexLocker lock(&m_mutexQ);

    // Reset slide & return to buffer for re-use
    Slide *slide = m_queue.dequeue();
    slide->Clear();
    m_queue.enqueue(slide);

    QString name = slide->objectName();

    // Free constrained load ptr now a spare slide is available
    if (!m_queue.at(--m_nextLoad)->IsEmpty())
        ++m_nextLoad;

    LOG(VB_FILE, LOG_DEBUG, SBLOC + QString("Released %1").arg(name));

    // Flush any pending slides that originate from multiple requests (skipping)
    Flush(m_queue.head(), "Pending");
}


/*!
 \brief Signal if any slides are waiting to be displayed
 \param slide The slide that has loaded or being tested
 \param reason Debug text describing reason for test
 \return int Number of slides available for display
*/
void SlideBuffer::Flush(Slide *slide, const QString& reason)
{
    QMutexLocker lock(&m_mutexQ);

    // Determine number of consecutive slides that are now available after head
    // Include last slide to ensure transition speed is consistent: it will never
    // be displayed because queue size is always > 2
    int available = 1;
    while (available < m_queue.size() && m_queue.at(available)->IsLoaded())
        ++available;

    if (available == 1)
        return;

    // Notify that more slides are available
    ImagePtrK im = slide->GetImageData();
    QString   path = im ? im->m_filePath : "Unknown";

    LOG(VB_FILE, LOG_DEBUG, SBLOC + QString("%1 %2 in %3, %4")
        .arg(reason, path, slide->objectName(), BufferState()));

    emit SlideReady(--available);
}

void SlideBuffer::Flush(Slide *slide)
{
    Flush(slide, "Loaded");
};
