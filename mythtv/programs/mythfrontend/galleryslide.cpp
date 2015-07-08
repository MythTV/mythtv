#include "galleryslide.h"

#include <math.h>

#include <mythmainwindow.h>


/*!
 \brief Initialise & start base animation
 \param forwards Direction
 \param speed Factor, 1 = real-time, 2 = double-time
 \return bool Always True
*/
bool AbstractAnimation::Start(bool forwards, float speed)
{
    m_forwards = forwards;
    m_speed    = speed;
    m_running  = true;
    return true;
}


/*!
 \brief Create simple animation
 \param image Image to be animated
 \param type Effect to be animated
*/
Animation::Animation(Slide *image, Type type)
    : AbstractAnimation(),
      QVariantAnimation(),
      m_parent(image), m_type(type), m_centre(UIEffects::Middle),
      m_elapsed(0)
{
}


/*!
 \brief Initialises an animation
 \param from Start value
 \param to End value
 \param duration Animation duration
 \param curve Easing curve governing animation
 \param centre Zoom centre
*/
void Animation::Set(QVariant from, QVariant to, int duration,
                    QEasingCurve curve, UIEffects::Centre centre)
{
    setStartValue(from);
    setEndValue(to);
    m_centre = centre;
    setDuration(duration);
    setEasingCurve(curve);
}


/*!
 \brief Start a single animation
 \param forwards Direction
 \param speed Speed factor, 1x, 2x etc
 \return bool True if started
*/
bool Animation::Start(bool forwards, float speed)
{
    if (duration() == 0)
        return false;

    m_elapsed = forwards ? 0 : duration();
    setCurrentTime(m_elapsed);

    return AbstractAnimation::Start(forwards, speed);
}


/*!
 \brief Progress single animation
 \param interval Millisecs since last update
*/
void Animation::Pulse(int interval)
{
    if (!m_running)
        return;

    m_elapsed += (m_forwards ? interval : -interval) * m_speed;

    setCurrentTime(m_elapsed);

    // Detect completion
    if ((m_forwards && m_elapsed >= duration())
        || (!m_forwards && m_elapsed <= 0))
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
            case Position: m_parent->SetPosition(value.toPoint()); break;
            case Alpha: m_parent->SetAlpha(value.toInt()); break;
            case Zoom: m_parent->SetZoom(value.toFloat()); break;
            case HorizontalZoom: m_parent->SetHorizontalZoom(value.toFloat());
                break;
            case VerticalZoom: m_parent->SetVerticalZoom(value.toFloat());
                break;
            case Angle: m_parent->SetAngle(value.toFloat()); break;
            case None:;
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
    connect(child, SIGNAL(finished()), this, SLOT(Finished()));
}


/*!
 \brief Delete all child animations
*/
void GroupAnimation::Clear()
{
    qDeleteAll(m_group);
    m_group.clear();
}


/*!
 \brief Progress sequential animation
 \param interval Millisecs since last update
*/
void SequentialAnimation::Pulse(int interval)
{
    if (!m_running || m_current < 0 || m_current >= m_group.size())
        return;

    // Pulse current running child
    m_group.at(m_current)->Pulse(interval);
}


/*!
 \brief Start sequential animation
 \param forwards Direction. If false, the last child will be played (backwards) first
 \param speed Factor, 1x, 2x etc
 \return bool Always true
*/
bool SequentialAnimation::Start(bool forwards, float speed)
{
    if (m_group.size() == 0)
        return false;

    m_current = forwards ? 0 : m_group.size() - 1;

    // Start group, then first child
    AbstractAnimation::Start(forwards, speed);
    m_group.at(m_current)->Start(m_forwards, m_speed);
    return true;
}


/*!
 \brief Change speed of current child animation and all subsequent ones
 \param speed New speed factor
*/
void SequentialAnimation::SetSpeed(float speed)
{
    // Set group speed for subsequent children
    AbstractAnimation::SetSpeed(speed);

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
    // Finish group when last child finishes
    if ((m_forwards && ++m_current == m_group.size())
        || (!m_forwards && --m_current < 0))
        AbstractAnimation::Finished();
    else
        // Start next child
        m_group.at(m_current)->Start(m_forwards, m_speed);
}


/*!
 \brief Progress parallel animations
 \param interval Millisecs since last update
*/
void ParallelAnimation::Pulse(int interval)
{
    if (m_running)
        // Pulse all children
        foreach(AbstractAnimation *animation, m_group)
            animation->Pulse(interval);
}


/*!
 \brief Start parallel group. All children play simultaneously
 \param forwards Direction
 \param speed Factor, 1x, 2x etc
 \return bool Always true
*/
bool ParallelAnimation::Start(bool forwards, float speed)
{
    if (m_group.size() == 0)
        return false;

    m_finished = m_group.size();

    // Start group, then all children
    AbstractAnimation::Start(forwards, speed);
    foreach(AbstractAnimation *animation, m_group)
        animation->Start(m_forwards, m_speed);

    return true;
}


/*!
 \brief Change speed of group and all child animations
 \param speed New speed factor
*/
void ParallelAnimation::SetSpeed(float speed)
{
    // Set group speed, then all children
    AbstractAnimation::SetSpeed(speed);
    foreach(AbstractAnimation *animation, m_group)
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
        Slide *image = dynamic_cast<Slide *>(m_parent);
        image->SetPan(value.toPoint());
    }
}


/*!
 \brief Clone slide from a theme MythUIImage
 \param parent Parent widget
 \param name Slide name
 \param image Theme MythUIImage to clone
*/
Slide::Slide(MythUIType *parent, QString name, MythUIImage *image)
    : MythUIImage(parent, name),
    m_ilt(new ImageLoadingThread(this)),
    m_data(NULL),
    m_waitingFor(NULL),
    m_zoom(1.0),
    m_isReady(false),
    m_loadFailed(false),
    m_locked(false),
    m_direction(0),
    m_zoomAnimation(NULL),
    m_panAnimation(NULL),
    m_pan(QPoint(0,0))
{
    // Clone from image
    CopyFrom(image);

    // Null parent indicates we should become a child of the image (after
    // copy to avoid recursion)
    if (!parent)
    {
        // Slides sit on top of parent image area
        SetArea(MythRect(image->GetArea().toQRect()));
        m_Area.moveTo(0, 0);
        m_Parent = image;
        image->AddChild(this);
    }

    // Listen for "image completed load" signals
    connect(m_ilt, SIGNAL(finished()), this, SLOT(LoadComplete()));

    // Provide animations for pan & zoom
    if (GetPainter()->SupportsAnimation())
    {
        m_zoomAnimation = new Animation(this, Animation::Zoom);
        m_panAnimation  = new PanAnimation(this);
    }
}


/*!
 \brief Destroy slide
*/
Slide::~Slide()
{
    if (m_ilt)
        m_ilt->wait();
    delete m_ilt;
    delete m_zoomAnimation;
    delete m_panAnimation;
}


/*!
 \brief Reset slide to unused state
*/
void Slide::Clear()
{
    m_isReady   = false;
    m_loadFailed = false;
    m_data       = NULL;
}


/*!
 \brief Load slide with an image
 \details If the requested image is already loaded (due to pre-load) then returns immediately.
 Otherwise it initiates an image load in a child thread, provided an image is not already loading
 (multiple requests due to skipping). If loader is busy then the most recent request is queued.
 Intervening requests are discarded.
 \param im The image to load
 \param direction Navigation that caused this load. Determines transition direction
 \param notifyCompletion if True, emits a signal when the last requested image has loaded
 \return bool True if the requested image is already loaded
*/
bool Slide::Load(ImageItem *im, int direction, bool notifyCompletion)
{
    m_waitingFor = notifyCompletion ? im : NULL;
    m_direction = direction;

    if (im == m_data)
    {
        LOG(VB_FILE, LOG_DEBUG, QString("Slide: Already loading/loaded %1 in %2")
            .arg(im->m_fileName, objectName()));

        if (m_isReady && notifyCompletion)
            // Image has been pre-loaded
            emit ImageLoaded(this);

        return m_isReady;
    }

    // Is a different image loading ?
    if (m_ilt && m_ilt->isRunning())
    {
        // Can't abort image loads, so must wait for it to finish
        // before starting new load
        m_waitingFor = im;

        LOG(VB_FILE, LOG_DEBUG, QString("Slide: Postponing load of %1 in %2")
            .arg(im->m_fileName, objectName()));

        return false;
    }

    // Start load
    m_data      = im;
    m_isReady   = m_loadFailed = false;
    ImageSg *sg = ImageSg::getInstance();

    if (im->m_type == kVideoFile)
    {
        // Use thumbnail, which has already been orientated
        SetFilename(sg->GenerateThumbUrl(im->m_thumbNails.at(0)));
        SetOrientation(1);
    }
    else
    {
        SetFilename(sg->GenerateUrl(im->m_fileName));
        SetOrientation(m_data->m_orientation);
    }

    SetCropRect(0, 0, 0, 0);
    m_ilt->start();
    return false;
}


/*!
 \brief An image has completed loading
 \details If the loaded image matches the most recently requested and client is waiting then
 signals that the slide is ready. Otherwise starts loading the latest requested image, if
 different. Superseded loads are discarded.
*/
void Slide::LoadComplete()
{
    m_isReady = true;
    m_loadFailed = (m_Images[0] == NULL);

    // Ignore superseded requests and preloads
    if (m_data == m_waitingFor)
    {
        // Loaded image is the latest requested
        emit ImageLoaded(this);
    }
    else if (m_waitingFor)
    {
        LOG(VB_FILE, LOG_DEBUG, QString("Slide: Starting delayed load %1")
            .arg(m_waitingFor->m_fileName));

        // Start latest postponed load
        Load(m_waitingFor, m_direction, true);
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
                    ? 1.0
                    : qMax(MIN_ZOOM,
                           qMin(MAX_ZOOM, m_zoom * (1.0 + percentage / 100.0)));
    if (newZoom != m_zoom)
    {
        if (m_zoomAnimation)
        {
            m_zoomAnimation->Set(m_zoom, newZoom, 250, QEasingCurve::OutQuad);
            m_zoomAnimation->Start();
        }
        else
            SetZoom(newZoom);
    }
}


/*!
 \brief Initiate pan
 \param offset Offset to apply to current position. Sentinel (0,0) resets pan.
*/
void Slide::Pan(QPoint offset)
{
    // Panning only possible when zoomed in
    if (m_zoom > 1.0)
    {
        QPoint start = m_pan;

        // Sentinel indicates reset to centre
        // Panning is applied to original (unzoomed) image co-ords.
        // Adjust offset for zoom so that pan moves a constant screen distance rather
        // than constant image distance
        QPoint dest = offset.isNull() ? QPoint(0, 0) : start + offset / m_zoom;

        if (m_panAnimation)
        {
            m_panAnimation->Set(start, dest, 250, QEasingCurve::Linear);
            m_panAnimation->Start();
        }
        else
            SetPan(dest);
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
    m_Effects.hzoom = m_Effects.vzoom = zoom;

    // TODO
    // MythUIImage displaces widget or doesn't centre for some combinations of
    // zoom centre/cropping so frig centre for now.
    m_Effects.centre = zoom < 1.0 ? UIEffects::Middle : UIEffects::TopLeft;

    SetPan(m_pan);
}


/*!
 \brief Sets slide pan
 \details Applies pan immediately
 \sa Slide::Pan
 \param pos New pan offset
*/
void Slide::SetPan(QPoint pos)
{
    if (m_loadFailed)
    {
        m_pan = pos;
        return;
    }

    // Determine zoom of largest dimension
    QRect imageArea = m_Images[m_CurPos]->rect();
    float hRatio    = float(imageArea.height()) / m_Area.height();
    float wRatio    = float(imageArea.width()) / m_Area.width();
    float ratio     = qMax(hRatio, wRatio) / m_zoom;

    // Determine crop area
    int h = qMin(int(roundf(m_Area.height() * ratio)), imageArea.height());
    int w = qMin(int(roundf(m_Area.width() * ratio)), imageArea.width());
    int x = imageArea.center().x() - w / 2;
    int y = imageArea.center().y() - h / 2;

    // Constrain pan to boundaries
    int limitX = (imageArea.width() - w) / 2;
    int limitY = (imageArea.height() - h) / 2;
    m_pan.setX(qMax(qMin(pos.x(), limitX), -limitX));
    m_pan.setY(qMax(qMin(pos.y(), limitY), -limitY));

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
        m_zoomAnimation->Pulse(GetMythMainWindow()->GetDrawInterval());

    if (m_panAnimation)
        m_panAnimation->Pulse(GetMythMainWindow()->GetDrawInterval());
}


/*!
 \brief Construct buffer
 \details Slides are cloned from an image and become children of it. Only the
 first is visible. Requires a minimum of 3 slides: 2 for a transition, 1 to handle
 requests
 \param image Parent image used as a template for slides
 \param size Number of slides in buffer
*/
SlideBuffer::SlideBuffer(MythUIImage *image, int size)
    : m_nextLoad(1)
{
    if (size < 3)
        // Require at least 3 slides: 2 for transitions & 1 to handle further requests
        return;

    // Fill buffer with slides cloned from the XML image widget

    // Create first as a child of the image.
    Slide *slide = new Slide(NULL, "slide0", image);

    // Buffer is notified when it has loaded image
    connect(slide, SIGNAL(ImageLoaded(Slide *)),
            this, SLOT(FlushAvailable(Slide *)));

    m_queue.enqueue(slide);

    // Rest are simple clones of first
    for (int i = 1; i < size; ++i)
    {
        slide = new Slide(image, QString("slide%1").arg(i), slide);

        // All slides (except first) start off hidden
        slide->SetVisible(false);

        // Buffer is notified when it has loaded image
        connect(slide, SIGNAL(ImageLoaded(Slide *)),
                this, SLOT(FlushAvailable(Slide *)));

        m_queue.enqueue(slide);
    }
}


/*!
 \brief Destructor
*/
SlideBuffer::~SlideBuffer()
{
    // TODO: Detach from parent
    qDeleteAll(m_queue);
}


/*!
 \brief Assign an image to next available slide, start loading and signal
 when done.
 \param im Image to load
 \param direction Navigation causing the load
 \return bool True if image is already loaded
*/
bool SlideBuffer::Load(ImageItem *im, int direction)
{
    // Start loading image in next available slide
    Slide *slide = m_queue.at(m_nextLoad);

    // Further load requests will go to same slide if no free ones are available
    QString extra = "";
    if (m_nextLoad < m_queue.size() - 1)
        ++m_nextLoad;
    else
        extra = "(No spare slides)";

    LOG(VB_FILE, LOG_DEBUG, QString("SlideBuffer: Loading %1 in %2, next load in slot %3 %4")
        .arg(im->m_fileName, slide->objectName()).arg(m_nextLoad).arg(extra));

    return slide->Load(im, direction, true);
}


/*!
 \brief Load an image in next available slide
 \param im Image to load
*/
void SlideBuffer::Preload(ImageItem *im)
{
    if (im)
    {
        // Start loading image in next available slide
        Slide *slide = m_queue.at(m_nextLoad);

        LOG(VB_FILE, LOG_DEBUG, QString("SlideBuffer: Preloading %1 in %2 in slot %3")
            .arg(im->m_fileName, slide->objectName()).arg(m_nextLoad));

        // Load silently
        m_queue.at(m_nextLoad)->Load(im);
    }
}


/*!
 \brief Remove & return slide from head of queue
 \return Slide Next slide for display
*/
Slide* SlideBuffer::TakeNext()
{
    if (m_queue.isEmpty())
        // Shouldn't happen unless init failed
        return NULL;

    // Dispense next slide & adjust load ptr to account for queue shuffling
    Slide *next = m_queue.dequeue();
    --m_nextLoad;

    ImageItem *im = next->GetImageData();
    LOG(VB_FILE, LOG_DEBUG, QString("SlideBuffer: Taking %1 in %2, next load in slot %3")
        .arg(im ? im->m_fileName : "Empty", next->objectName()).arg(m_nextLoad));

    return next;
}


/*!
 \brief Return slide to queue and flush next image, if it is waiting for display
 \param slide Unused slide for re-use
 \return int Number of slides waiting for display at head
*/
int SlideBuffer::Release(Slide *slide)
{
    LOG(VB_FILE, LOG_DEBUG, QString("SlideBuffer: Releasing %1").arg(slide->objectName()));

    // Reset slide & return to buffer for re-use
    slide->Clear();
    m_queue.enqueue(slide);

    // Free constrained load ptr now a spare slide is available
    if (m_queue.at(m_nextLoad)->IsAvailable())
        ++m_nextLoad;

    // Flush any pending slides that originate from multiple requests (skipping)
    return FlushAvailable(m_queue.head(), "Pending");
}


/*!
 \brief Signal if any slides are waiting to be displayed
 \param slide The slide that has loaded or being tested
 \param reason Debug text describing reason for test
 \return int Number of slides available for display
*/
int SlideBuffer::FlushAvailable(Slide *slide, QString reason)
{
    // Determine number of consecutive slides that are now available at head
    // Ignore last slide: it can't be displayed because it may start handling a new request
    int available = 0;
    while (available < m_queue.size() - 1 && m_queue.at(available)->IsAvailable())
        ++available;

    // Notify if slides are available
    if (available > 0)
    {
        LOG(VB_FILE, LOG_DEBUG, QString("SlideBuffer: %1 %2 in %3 (%4 available)")
            .arg(reason, slide->GetImageData()->m_fileName, slide->objectName())
            .arg(available));

        emit SlideReady(available);
    }

    return available;
}
