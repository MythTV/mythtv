// C++
#include <utility>

// MythTV
#include "libmythui/mythdialogbox.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythuitext.h"

// MythFrontend
#include "galleryslideview.h"
#include "galleryviews.h"

#define LOC QString("Slideview: ")

// EXIF tag 0x9286 UserComment can contain garbage
static QString clean_comment(const QString &comment)
{
    QString result;
    std::copy_if(comment.cbegin(), comment.cend(), std::back_inserter(result), [](QChar x) { return x.isPrint(); } );
    return result;
}

/**
 *  \brief  Constructor
 *  \param  parent The screen parent
 *  \param  name The name of the screen
 *  \param  editsAllowed Are edits allowed. Affects what menu items are
 *                       presented to the user.
 */
GallerySlideView::GallerySlideView(MythScreenStack *parent, const char *name,
                                   bool editsAllowed)
    : MythScreenType(parent, name),
      m_mgr(ImageManagerFe::getInstance()),
      m_availableTransitions(GetMythPainter()->SupportsAnimation()),
      m_transition(m_availableTransitions.Select(
                       gCoreContext->GetNumSetting("GalleryTransitionType",
                                                   kBlendTransition))),
      m_infoList(*this),
      m_slideShowTime(gCoreContext->GetDurSetting<std::chrono::milliseconds>("GallerySlideShowTime", 3s)),
      m_showCaptions(gCoreContext->GetBoolSetting("GalleryShowSlideCaptions", true)),
      m_editsAllowed(editsAllowed)
{
    // Detect when transitions finish. Queued signal to allow redraw/pulse to
    // complete before handling event.
    connect(&m_transition, &Transition::finished,
            this, &GallerySlideView::TransitionComplete, Qt::QueuedConnection);
    connect(&m_updateTransition, &Transition::finished,
            this, &GallerySlideView::TransitionComplete, Qt::QueuedConnection);

    // Initialise slideshow timer
    m_timer.setSingleShot(true);
    m_timer.setInterval(m_slideShowTime);
    connect(&m_timer, &QTimer::timeout,
            this, qOverload<>(&GallerySlideView::ShowNextSlide));

    // Initialise status delay timer
    m_delay.setSingleShot(true);
    m_delay.setInterval(gCoreContext->GetDurSetting<std::chrono::milliseconds>("GalleryStatusDelay", 0s));
    connect(&m_delay, &QTimer::timeout, this, &GallerySlideView::ShowStatus);

    MythMainWindow::DisableScreensaver();
    LOG(VB_GUI, LOG_DEBUG, "Created Slideview");
}


/**
 *  \brief  Destructor
 */
GallerySlideView::~GallerySlideView()
{
    delete m_view;
    MythMainWindow::RestoreScreensaver();
    LOG(VB_GUI, LOG_DEBUG, "Deleted Slideview");
}


/**
 *  \brief  Initialises the graphical elements
 *  \return True if successful otherwise false
 */
bool GallerySlideView::Create()
{
    if (!LoadWindowFromXML("image-ui.xml", "slideshow", this))
        return false;

    // Get widgets from XML
    bool err = false;
    UIUtilE::Assign(this, m_uiImage, "image", &err);
    UIUtilW::Assign(this, m_uiStatus, "status");
    UIUtilW::Assign(this, m_uiSlideCount, "slidecount");
    UIUtilW::Assign(this, m_uiCaptionText, "caption");
    UIUtilW::Assign(this, m_uiHideCaptions, "hidecaptions");

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Cannot load screen 'Slideshow'");
        return false;
    }

    // Initialise details list
    if (!m_infoList.Create(true))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Cannot load 'Info buttonlist'");
        return false;
    }

    // Create display buffer
    m_slides.Initialise(*m_uiImage);

    if (m_uiHideCaptions)
        m_uiHideCaptions->SetText(m_showCaptions ? "" : tr("Hide"));

    BuildFocusList();
    SetFocusWidget(m_uiImage);

    // Detect when slides are available for display.
    // Queue so that keypress events always complete before transition starts
    connect(&m_slides, &SlideBuffer::SlideReady,
            this, &GallerySlideView::SlideAvailable, Qt::QueuedConnection);

    return true;
}


/*!
 \brief Update transition
*/
void GallerySlideView::Pulse()
{
    // Update transition animations
    m_transition.Pulse();
    MythScreenType::Pulse();
}


/**
 *  \brief  Handle keypresses
 *  \param  event The pressed key
 *  \return True if key was used, otherwise false
 */
bool GallerySlideView::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Images", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == "LEFT")
            ShowPrevSlide(1);
        else if (action == "RIGHT")
            ShowNextSlide(1);
        else if (action == "UP")
            ShowPrevSlide(10);
        else if (action == "DOWN")
            ShowNextSlide(10);
        else if (action == "INFO")
            ShowInfo();
        else if (action == "MENU")
            MenuMain();
        else if (action == "PLAY")
        {
            if (m_playing)
                Stop();
            else
                Play();
        }
        else if (action == "SELECT")
        {
            PlayVideo();
        }
        else if (action == "STOP")
        {
            Stop();
        }
        else if (action == "ROTRIGHT")
        {
            Transform(kRotateCW);
        }
        else if (action == "ROTLEFT")
        {
            Transform(kRotateCCW);
        }
        else if (action == "FLIPHORIZONTAL")
        {
            Transform(kFlipHorizontal);
        }
        else if (action == "FLIPVERTICAL")
        {
            Transform(kFlipVertical);
        }
        else if (action == "ZOOMIN")
        {
            Zoom(10);
        }
        else if (action == "ZOOMOUT")
        {
            Zoom(-10);
        }
        else if (action == "FULLSIZE")
        {
            Zoom();
        }
        else if (action == "SCROLLUP")
        {
            Pan(QPoint(0, 100));
        }
        else if (action == "SCROLLDOWN")
        {
            Pan(QPoint(0, -100));
        }
        else if (action == "SCROLLLEFT")
        {
            Pan(QPoint(-120, 0));
        }
        else if (action == "SCROLLRIGHT")
        {
            Pan(QPoint(120, 0));
        }
        else if (action == "RECENTER")
        {
            Pan();
        }
        else if (action == "ESCAPE" && !GetMythMainWindow()->IsExitingToMain())
        {
            // Exit info details, if shown
            handled = m_infoList.Hide();
        }
        else
        {
            handled = false;
        }
    }

    if (!handled)
        handled = MythScreenType::keyPressEvent(event);

    return handled;
}


/**
 *  \brief  Handle custom events
 *  \param  event The custom event
 */
void GallerySlideView::customEvent(QEvent *event)
{
    if (event->type() == MythEvent::kMythEventMessage)
    {
        auto *me = dynamic_cast<MythEvent *>(event);
        if (me == nullptr)
            return;

        const QString& message = me->Message();

        QStringList extra = me->ExtraDataList();

        if (message == "IMAGE_METADATA" && !extra.isEmpty())
        {
            int id = extra[0].toInt();
            ImagePtrK selected = m_view->GetSelected();

            if (selected && selected->m_id == id)
                m_infoList.Display(*selected, extra.mid(1));
        }
        else if (message == "THUMB_AVAILABLE")
        {
            if (!extra.isEmpty() && m_view->Update(extra[0].toInt()))
                ShowSlide(0);
        }
    }
    else if (event->type() == DialogCompletionEvent::kEventType)
    {
        auto *dce = (DialogCompletionEvent *)(event);

        QString resultid  = dce->GetId();
        int     buttonnum = dce->GetResult();

        if (resultid == "metadatamenu")
        {
            switch (buttonnum)
            {
            case 0: Transform(kRotateCW); break;
            case 1: Transform(kRotateCCW); break;
            case 2: Transform(kFlipHorizontal); break;
            case 3: Transform(kFlipVertical); break;
            case 4: Transform(kResetToExif); break;
            case 5: Zoom(10); break;
            case 6: Zoom(-10); break;
            }
        }
    }
}


/**
 *  \brief  Shows the popup menu
 */
void GallerySlideView::MenuMain()
{
    // Create the main menu that will contain the submenus above
    auto *menu = new MythMenu(tr("Slideshow Options"), this, "mainmenu");

    ImagePtrK im = m_slides.GetCurrent().GetImageData();
    if (im && im->m_type == kVideoFile)
        menu->AddItem(tr("Play Video"), qOverload<>(&GallerySlideView::PlayVideo));

    if (m_playing)
        menu->AddItem(tr("Stop"), &GallerySlideView::Stop);
    else
        menu->AddItem(tr("Start SlideShow"), qOverload<>(&GallerySlideView::Play));

    if (gCoreContext->GetBoolSetting("GalleryRepeat", false))
        menu->AddItem(tr("Turn Repeat Off"), &GallerySlideView::RepeatOff);
    else
        menu->AddItem(tr("Turn Repeat On"), qOverload<>(&GallerySlideView::RepeatOn));

    MenuTransforms(*menu);

    if (m_uiHideCaptions)
    {
        if (m_showCaptions)
            menu->AddItem(tr("Hide Captions"), &GallerySlideView::HideCaptions);
        else
            menu->AddItem(tr("Show Captions"), &GallerySlideView::ShowCaptions);
    }

    QString details;
    switch (m_infoList.GetState())
    {
    case kBasicInfo: details = tr("More Details"); break;
    case kFullInfo:  details = tr("Less Details"); break;
    default:
    case kNoInfo:    details = tr("Show Details"); break;
    }
    menu->AddItem(details, &GallerySlideView::ShowInfo);

    if (m_infoList.GetState() != kNoInfo)
        menu->AddItem(tr("Hide Details"), &GallerySlideView::HideInfo);

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    auto *menuPopup = new MythDialogBox(menu, popupStack, "menuPopup");
    if (menuPopup->Create())
        popupStack->AddScreen(menuPopup);
    else
        delete menuPopup;
}


/**
 *  \brief  Add Transforms submenu
 *  \param  mainMenu Parent menu
 */
void GallerySlideView::MenuTransforms(MythMenu &mainMenu)
{
    ImagePtrK im = m_slides.GetCurrent().GetImageData();
    if (im && !m_playing)
    {
        auto *menu = new MythMenu(tr("Transform Options"),
                                      this, "metadatamenu");
        if (m_editsAllowed)
        {
            menu->AddItem(tr("Rotate CW"));
            menu->AddItem(tr("Rotate CCW"));
            menu->AddItem(tr("Flip Horizontal"));
            menu->AddItem(tr("Flip Vertical"));
            menu->AddItem(tr("Reset to Exif"));
        }

        if (m_slides.GetCurrent().CanZoomIn())
            menu->AddItem(tr("Zoom In"));

        if (m_slides.GetCurrent().CanZoomOut())
            menu->AddItem(tr("Zoom Out"));

        mainMenu.AddItem(tr("Transforms"), nullptr, menu);
    }
}


/*!
 \brief  Start slideshow
 \param type       Browsing, Normal or Recursive
 \param parentId   The dir id, if positive. Otherwise the view is refreshed
                   using the existing parent dir
 \param selectedId Currently selected item. If not set, will default to the
                   first item.
*/
void GallerySlideView::Start(ImageSlideShowType type, int parentId, int selectedId)
{
    gCoreContext->addListener(this);

    if (type == kBrowseSlides)
    {
        // Browsing views a single ordered directory
        m_view = new FlatView(kOrdered);

        // Load db images
        m_view->LoadFromDb(parentId);

        // Display current selection, falling back to first
        m_view->Select(selectedId);

        // Display slide immediately
        ShowSlide();
    }
    else
    {
        int orderInt = gCoreContext->GetNumSetting("GallerySlideOrder", kOrdered);

        SlideOrderType order = (orderInt < kOrdered) || (orderInt > kSeasonal)
                ? kOrdered
                : static_cast<SlideOrderType>(orderInt);

        // Recursive uses a view of a directory subtree; Normal views a single directory
        m_view = (type == kRecursiveSlideShow) ? new TreeView(order)
                                               : new FlatView(order);
        // Load db images
        m_view->LoadFromDb(parentId);

        // Ordered views start from selected image
        if (order == kOrdered)
            // Adjust view so that slideshows show count rather than position
            m_view->Rotate(selectedId);

        // No transition for first image
        Play(false);
    }
}


void GallerySlideView::Close()
{
    gCoreContext->removeListener(this);

    // Stop further loads
    m_slides.Teardown();

    // Update gallerythumbview selection
    ImagePtrK im = m_view->GetSelected();
    if (im)
        emit ImageSelected(im->m_id);

    MythScreenType::Close();
}


/**
 *  \brief Stop a playing slideshow
 */
void GallerySlideView::Stop()
{
    m_playing = false;
    m_timer.stop();
    SetStatus(tr("Stopped"));
}


/**
 *  \brief Start a slideshow
 *  \param useTransition if false, slide will be updated instantly
 */
void GallerySlideView::Play(bool useTransition)
{
    // Start from next slide
    ShowNextSlide(1, useTransition);

    m_playing = true;
    if (!m_suspended)
        m_timer.start();
    SetStatus(tr("Playing"), true);
}


/*!
 \brief Pause transition timer temporarily
*/
void GallerySlideView::Suspend()
{
    m_timer.stop();
    m_suspended = true;
}


/*!
 \brief Unpause transition timer
*/
void GallerySlideView::Release()
{
    m_suspended = false;
    if (m_playing)
        m_timer.start();
}


/*!
 \brief Action transform request
 \param state Transform to apply
*/
void GallerySlideView::Transform(ImageFileTransform state)
{
    ImagePtrK im = m_view->GetSelected();
    if (im && !m_playing)
    {
        ImageIdList list;
        list.append(im->m_id);
        QString err = m_mgr.ChangeOrientation(state, list);
        if (!err.isEmpty())
            ShowOkPopup(err);
    }
}


/*!
 \brief Zoom current slide
 \param increment Percentage factor
*/
void GallerySlideView::Zoom(int increment)
{
    if (!m_playing)
        m_slides.GetCurrent().Zoom(increment);
}


/*!
 \brief Pan current slide
 \param offset Offset from current position
*/
void GallerySlideView::Pan(QPoint offset)
{
    if (!m_playing)
        m_slides.GetCurrent().Pan(offset);
}


/*!
 \brief Show exif info list
*/
void GallerySlideView::ShowInfo()
{
    m_infoList.Toggle(m_slides.GetCurrent().GetImageData());
}


/*!
 \brief Hide exif info list
*/
void GallerySlideView::HideInfo()
{
    m_infoList.Hide();
}


/*!
 \brief Show text widgets
*/
void GallerySlideView::ShowCaptions()
{
    m_showCaptions = true;
    m_uiHideCaptions->Reset();
}


/*!
 \brief Hide text widgets
*/
void GallerySlideView::HideCaptions()
{
    m_showCaptions = false;
    m_uiHideCaptions->SetText(tr("Hide"));
}


/*!
 \brief Display slide
 \param direction Navigation direction +1 = forwards, 0 = update, -1 = backwards
*/
void GallerySlideView::ShowSlide(int direction)
{
    ImagePtrK im = m_view->GetSelected();
    if (!im)
        // Reached view limits
        return;

    LOG(VB_FILE, LOG_DEBUG, LOC + QString("Selected %1").arg(im->m_filePath));

    // Suspend the timer until the transition has finished
    Suspend();

    // Load image from file
    if (!m_slides.Load(im, direction))
        // Image not yet available: show loading status
        SetStatus(tr("Loading"), true);
}


/*!
 \brief Start transition
 \details Displays image that has just loaded
 \param count Number of slides ready for display
*/
void GallerySlideView::SlideAvailable(int count)
{
    // Transition speed = 0.5x for every slide waiting. Min = 1x, Max = Half buffer size
    float speed = 0.5 + (count / 2.0);

    // Are we transitioning ?
    if (m_transitioning)
    {
        // More slides waiting for display: accelerate current transition
        LOG(VB_FILE, LOG_DEBUG, LOC + QString("Changing speed to %1").arg(speed));
        m_transition.SetSpeed(speed);
        return;
    }

    // We've been waiting for this slide: transition immediately
    m_transitioning = true;

    // Take next slide
    Slide &next = m_slides.GetNext();

    // Update loading status
    ClearStatus(next);

    // Update slide counts
    if (m_uiSlideCount)
        m_uiSlideCount->SetText(m_view->GetPosition());

    int direction = next.GetDirection();

    // Use instant transition for start-up & updates (dir = 0)
    // and browsing with transitions turned off
    Transition &transition =
            (direction != 0 &&
             (m_playing || gCoreContext->GetBoolSetting("GalleryBrowseTransition", false)))
            ? m_transition : m_updateTransition;

    // Reset any zoom before starting transition
    Zoom();
    transition.Start(m_slides.GetCurrent(), next, direction >= 0, speed);
}


/*!
 \brief Transition to new slide has finished
 \details Resets buffers & old slide. Starts next transition if slide loads
 are pending (skipping). Otherwise updates text widgets for new slide, pre-loads
 next slide & starts any video.
*/
void GallerySlideView::TransitionComplete()
{
    if (m_isDeleting)
        return;

    m_transitioning = false;

    // Release old slide, which may start a new transition
    m_slides.ReleaseCurrent();

    // No further actions when skipping
    // cppcheck-suppress knownConditionTrueFalse
    if (m_transitioning)
        return;

    // Preload next slide, if any
    m_slides.Preload(m_view->HasNext(1));

    // Populate display for new slide
    ImagePtrK im  = m_slides.GetCurrent().GetImageData();

    // Update any file details information
    m_infoList.Update(im);

    if (im && m_uiCaptionText)
    {
        // show the date & comment
        QStringList text;
        text << ImageManagerFe::LongDateOf(im);

        QString comment = clean_comment(im->m_comment);
        if (!comment.isEmpty())
            text << comment;

        m_uiCaptionText->SetText(text.join(" - "));
    }

    // Start any video unless we're paused or browsing
    if (im && im->m_type == kVideoFile)
    {
        if (m_playing)
            PlayVideo();
        else
            SetStatus(tr("Video"));
    }

    // Resume slideshow timer
    Release();
}


/*!
 \brief Display the previous slide in the sequence
*/
void GallerySlideView::ShowPrevSlide(int inc)
{
    if (m_playing && m_view->HasPrev(inc) == nullptr)
    {
        // Prohibit back-wrapping during slideshow: it will cause premature end
        //: Cannot go back beyond first slide of slideshow
        SetStatus(tr("Start"));
    }
    else if (m_view->Prev(inc))
    {
        ShowSlide(-1);
    }
}


/*!
 \brief Display the next slide in the sequence
 \param inc How many slides to move forward.
 \param useTransition if false, slide will be updated instantly
*/
void GallerySlideView::ShowNextSlide(int inc, bool useTransition)
{
    // Browsing always wraps; slideshows depend on repeat setting
    if (m_playing && m_view->HasNext(inc) == nullptr
            && !gCoreContext->GetBoolSetting("GalleryRepeat", false))
    {
        // Don't stop due to jumping past end
        if (inc == 1)
        {
            Stop();
            //: Slideshow has reached last slide
            SetStatus(tr("End"));
        }
    }
    else if (m_view->Next(inc))
    {
        ShowSlide(useTransition ? 1 : 0);
    }
    else
    {
        // No images
        Stop();
        m_infoList.Hide();
        m_slides.GetCurrent().Clear();
        if (m_uiSlideCount)
            m_uiSlideCount->SetText("0/0");
        if (m_uiCaptionText)
            m_uiCaptionText->Reset();
    }
}
void GallerySlideView::ShowNextSlide(void)
{
    ShowNextSlide(1, true);
}


/*!
 \brief Starts internal player for video
*/
void GallerySlideView::PlayVideo()
{
    if (m_slides.GetCurrent().FailedLoad())
        return;

    ImagePtrK im = m_slides.GetCurrent().GetImageData();

    if (im && im->m_type == kVideoFile)
        GetMythMainWindow()->HandleMedia("Internal", im->m_url);
}


/*!
 \brief Displays status text (Loading, Paused etc.)
 \param msg Text to show
 \param delay It true, delay showing the status.
*/
void GallerySlideView::SetStatus(QString msg, bool delay)
{
    m_statusText = std::move(msg);
    if (m_uiStatus)
    {
        if (delay)
            m_delay.start();
        else
            ShowStatus();
    }
}


void GallerySlideView::ShowStatus()
{
    if (m_uiStatus)
        m_uiStatus->SetText(m_statusText);
}


void GallerySlideView::ClearStatus(const Slide &slide)
{
    if (m_uiStatus)
    {
        m_delay.stop();

        if (slide.FailedLoad())
        {
            ImagePtrK im = slide.GetImageData();
            SetStatus(tr("Failed to load %1").arg(im ? im->m_filePath : "?"));
        }
        else
        {
            m_uiStatus->Reset();
        }
    }
}
