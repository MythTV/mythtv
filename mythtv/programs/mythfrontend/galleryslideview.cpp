#include "galleryslideview.h"

#include <mythmainwindow.h>
#include <mythuitext.h>

#include "gallerycommhelper.h"


// Numbers of slides to use for buffering image requests.
// Each will remotely load its image from BE so too many will hamper performance
// Fewer will result in jumping between slides rather than flicking quickly through them
// Minimum is 3: 2 for displaying a transition, 1 to handle load requests
// Reasonable range is 4 - 16 ?
#define SLIDE_BUFFER_SIZE 8

/**
 *  \brief  Constructor
 *  \param  parent The screen parent
 *  \param  name The name of the screen
 */
GallerySlideView::GallerySlideView(MythScreenStack *parent,
                                   const char *name)
    : MythScreenType(parent, name),
    m_uiImage(NULL),
    m_uiStatus(NULL),
    m_uiSlideCount(NULL), m_uiCaptionText(NULL), m_uiHideCaptions(NULL),
    m_slideShowType(kBrowseSlides),
    m_slideShowTime(gCoreContext->GetNumSetting("GallerySlideShowTime", 3000)),
    m_paused(false),
    m_suspended(false),
    m_showCaptions(gCoreContext->GetNumSetting("GalleryShowSlideCaptions",
                                               true)),
    m_infoList(NULL),
    m_view(NULL),
    m_slides(NULL),
    m_slideCurrent(NULL),
    m_slideNext(NULL),
    m_pending(0),
    m_availableTransitions(GetMythPainter()->SupportsAnimation()),
    m_updateTransition()
{
    // Get notified when update Transition completes
    connect(&m_updateTransition, SIGNAL(finished()),
            this, SLOT(TransitionComplete()));

    // Initialise transition
    int setting = gCoreContext->GetNumSetting("GalleryTransitionType",
                                              kBlendTransition);
    m_transition = m_availableTransitions.Select(setting);

    // Transitions signal when complete
    connect(m_transition, SIGNAL(finished()), this, SLOT(TransitionComplete()));

    // Used by random transition
    qsrand(QTime::currentTime().msec());

    // Slideshow timer
    m_timer = new QTimer();
    m_timer->setSingleShot(true);
    m_timer->setInterval(m_slideShowTime);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(ShowNextSlide()));

    // Filter db reads using current settings.
    // These can't change whilst slideshow is displayed.
    m_db = new ImageDbReader(gCoreContext->GetNumSetting("GallerySortOrder"),
                             gCoreContext->GetNumSetting("GalleryShowHidden"),
                             gCoreContext->GetNumSetting("GalleryShowType"));
}


/**
 *  \brief  Destructor
 */
GallerySlideView::~GallerySlideView()
{
    delete m_timer;
    delete m_infoList;
    delete m_view;
    delete m_slides;
    delete m_slideCurrent;
    delete m_slideNext;
    delete m_db;
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

    // File details list is managed elsewhere
    m_infoList = InfoList::Create(this, true, tr("Not defined"));

    if (err || !m_infoList)
    {
        LOG(VB_GENERAL, LOG_ERR, "Slideview: Cannot load screen 'Slideshow'");
        return false;
    }

    // Create display buffer
    m_slides = new SlideBuffer(m_uiImage, SLIDE_BUFFER_SIZE);

    // Use empty slide to transition from
    m_slideCurrent = m_slides->TakeNext();
    if (!m_slideCurrent)
    {
        LOG(VB_GENERAL, LOG_ERR, "Slideview: Failed to initialise Slideshow");
        return false;
    }

    if (m_uiHideCaptions)
        m_uiHideCaptions->SetText(m_showCaptions ? "" : tr("Hide"));

    BuildFocusList();
    SetFocusWidget(m_uiImage);

    // Get notified when a slide is ready for display. Must be queued to
    // avoid event recursion, ie. event handlers must complete before new
    // slide notifications are processed.
    connect(m_slides, SIGNAL(SlideReady(int)),
            this, SLOT(SlideAvailable(int)), Qt::QueuedConnection);

    return true;
}


/*!
 \brief Update transition
*/
void GallerySlideView::Pulse()
{
    // Update transition animations
    if (m_transition)
        m_transition->Pulse(GetMythMainWindow()->GetDrawInterval());

    MythScreenType::Pulse();
}


/*!
 \brief Update changed images
 \param id Image that has changed
*/
void GallerySlideView::ThumbnailChange(int id)
{
    if (m_view->Update(id))
        // Current image has changed, so redisplay
        ShowSlide();
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

    bool        handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Images", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "LEFT")
            ShowPrevSlide();
        else if (action == "RIGHT")
            ShowNextSlide();
        else if (action == "INFO")
            ShowInfo();
        else if (action == "MENU")
            MenuMain();
        else if (action == "SLIDESHOW")
        {
            // If a slideshow is active then pause/un-pause it
            // Otherwise start a normal one
            if (m_slideShowType == kBrowseSlides)
                StartNormal();
            else if (m_paused)
                Resume();
            else
                Pause();
        }
        else if (action == "PAUSE")
            Pause();
        else if (action == "SELECT")
            PlayVideo();
        else if (action == "STOP")
            Stop();
        else if (action == "ROTRIGHT")
            Transform(kRotateCW);
        else if (action == "ROTLEFT")
            Transform(kRotateCCW);
        else if (action == "FLIPHORIZONTAL")
            Transform(kFlipHorizontal);
        else if (action == "FLIPVERTICAL")
            Transform(kFlipVertical);
        else if (action == "ZOOMIN")
            Zoom(10);
        else if (action == "ZOOMOUT")
            Zoom(-10);
        else if (action == "FULLSIZE")
            Zoom();
        else if (action == "SCROLLUP")
            Pan(QPoint(0, 100));
        else if (action == "SCROLLDOWN")
            Pan(QPoint(0, -100));
        else if (action == "SCROLLLEFT")
            Pan(QPoint(-120, 0));
        else if (action == "SCROLLRIGHT")
            Pan(QPoint(120, 0));
        else if (action == "RECENTER")
            Pan();
        else if (action == "ESCAPE" && !GetMythMainWindow()->IsExitingToMain())
        {
            // Exit info details, if shown
            handled = m_infoList->Hide();
            if (!handled)
            {
                // We're exiting - update gallerythumbview selection
                ImageItem *im = m_view->GetSelected();
                if (im)
                    emit ImageSelected(im->m_id);
            }
        }
        else
            handled = false;
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
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = (DialogCompletionEvent *)(event);

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
                case 4: Transform(kResetExif); break;
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
    MythMenu *menu =
        new MythMenu(tr("Slideshow Options"), this, "mainmenu");

    ImageItem *im = m_slideCurrent->GetImageData();
    if (im && im->m_type == kVideoFile)
        menu->AddItem(tr("Play Video"), SLOT(PlayVideo()));

    if (m_slideShowType == kBrowseSlides)
    {
        menu->AddItem(tr("Start SlideShow"),
                      SLOT(StartNormal()));
        menu->AddItem(tr("Start Recursive SlideShow"),
                      SLOT(StartRecursive()));
    }
    else
    {
        if (m_paused)
            menu->AddItem(tr("Resume"), SLOT(Resume()));
        else
            menu->AddItem(tr("Pause"), SLOT(Pause()));

        menu->AddItem(tr("Stop"), SLOT(Stop()));
    }

    MenuTransforms(menu);

    if (m_uiHideCaptions)
    {
        if (m_showCaptions)
            menu->AddItem(tr("Hide Captions"), SLOT(HideCaptions()));
        else
            menu->AddItem(tr("Show Captions"), SLOT(ShowCaptions()));
    }

    QString details;
    switch (m_infoList->GetState())
    {
        case kBasicInfo: details = tr("More Details"); break;
        case kFullInfo: details  = tr("Less Details"); break;
        default:
        case kNoInfo: details = tr("Show Details"); break;
    }
    menu->AddItem(details, SLOT(ShowInfo()));

    if (m_infoList->GetState() != kNoInfo)
        menu->AddItem(tr("Hide Details"), SLOT(HideInfo()));

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythDialogBox *menuPopup = new MythDialogBox(menu, popupStack, "menuPopup");
    if (menuPopup->Create())
        popupStack->AddScreen(menuPopup);
    else
        delete menuPopup;
}


/**
 *  \brief  Add Transforms submenu
 *  \param  mainMenu Parent menu
 */
void GallerySlideView::MenuTransforms(MythMenu *mainMenu)
{
    ImageItem *im = m_slideCurrent->GetImageData();
    if (im && SlideShowNotActive())
    {
        MythMenu *menu = new MythMenu(tr("Transform Options"),
                                      this, "metadatamenu");

        if (im->m_type == kImageFile)
        {
            menu->AddItem(tr("Rotate CW"));
            menu->AddItem(tr("Rotate CCW"));
            menu->AddItem(tr("Flip Horizontal"));
            menu->AddItem(tr("Flip Vertical"));
            menu->AddItem(tr("Reset to Exif"));
        }
        if (m_slideCurrent->CanZoomIn())
            menu->AddItem(tr("Zoom In"));

        if (m_slideCurrent->CanZoomOut())
            menu->AddItem(tr("Zoom Out"));

        mainMenu->AddItem(tr("Transforms"), NULL, menu);
    }
}


/*!
 \brief  Start slideshow
 \param type Browsing, Normal or Recursive
 \param view View to initialise slideshow from.
 \param newScreen True if starting from Thumbview, False otherwise
*/
void GallerySlideView::Start(ImageSlideShowType type, int parentId,
                             int selectedId, bool newScreen)
{
    // Cleanup any current slideshow
    Stop();
    delete m_view;

    m_slideShowType = type;

    if (type == kBrowseSlides)
    {
        // Browsing views a single ordered directory
        m_view = new FlatView(kOrdered, m_db);

        // Load db images
        m_view->LoadFromDb(parentId);

        // Display current selection, falling back to first
        m_view->Select(selectedId);

        // Display slide immediately
        ShowSlide();
    }
    else
    {
        int orderInt = gCoreContext->GetNumSetting("GallerySlideOrder",
                                                   kOrdered);

        SlideOrderType order = orderInt < kOrdered || orderInt > kSeasonal
                ? kOrdered
                : static_cast<SlideOrderType>(orderInt);

        // Recursive uses a view of a directory subtree; Normal views a single directory
        m_view = (type == kRecursiveSlideShow)
                ? new TreeView(order, m_db) : new FlatView(order, m_db);

        // Load db images
        m_view->LoadFromDb(parentId);

        // Ordered views start from selected image
        if (order == kOrdered)
            // Adjust list so that slideshows show count rather than position
            // Start at selection for new screens; old screens start at the subsequent
            // image (they're already showing the selected)
            m_view->Rotate(selectedId, newScreen ? 0 : 1);

        // Show first image, immediately for new screens
        ShowNextSlide(!newScreen);

        // Start slideshow
        Resume();
    }
}


/**
 *  \brief  Stops an active slideshow
 */
void GallerySlideView::Stop()
{
    m_timer->stop();
    m_slideShowType = kBrowseSlides;
    if (m_uiStatus)
        // Clear paused text
        m_uiStatus->SetVisible(false);
}


/**
 *  \brief  Pauses an active slideshow
 */
void GallerySlideView::Pause()
{
    m_timer->stop();
    m_paused = true;
    SetStatus(tr("Paused"));
}


/**
 *  \brief  Resumes a paused slideshow
 */
void GallerySlideView::Resume()
{
    m_paused = false;
    if (m_uiStatus)
        m_uiStatus->SetVisible(false);
    if (!m_suspended)
        m_timer->start();
}


/*!
 \brief Pause transition timer temporarily
*/
void GallerySlideView::Suspend()
{
    m_timer->stop();
    m_suspended = true;
}


/*!
 \brief Unpause transition timer
*/
void GallerySlideView::Release()
{
    m_suspended = false;
    if (!m_paused && m_slideShowType != kBrowseSlides)
        m_timer->start();
}


/*!
 \brief Action transform request
 \param state Transform to apply
*/
void GallerySlideView::Transform(ImageFileTransform state)
{
    ImageItem *im = m_view->GetSelected();
    if (im && SlideShowNotActive())
    {
        ImageIdList list;
        list.append(im->m_id);
        QString err = GalleryBERequest::ChangeOrientation(state, list);
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
    if (SlideShowNotActive())
        m_slideCurrent->Zoom(increment);
}


/*!
 \brief Pan current slide
 \param offset Offset from current position
*/
void GallerySlideView::Pan(QPoint offset)
{
    if (SlideShowNotActive())
        m_slideCurrent->Pan(offset);
}


/*!
 \brief Show exif info list
*/
void GallerySlideView::ShowInfo()
{
    ImageItem *im = m_slideCurrent->GetImageData();
    if (im)
        m_infoList->Toggle(im);
}


/*!
 \brief Hide exif info list
*/
void GallerySlideView::HideInfo()
{
    m_infoList->Hide();
}


/*!
 \brief Show text widgets
*/
void GallerySlideView::ShowCaptions()
{
    m_showCaptions = true;
    m_uiHideCaptions->SetText("");
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
    ImageItem *im = m_view->GetSelected();
    if (!im)
        // Reached view limits
        return;

    LOG(VB_FILE, LOG_DEBUG, QString("Slideview: Gallery showing %1")
        .arg(im->m_fileName));

    // Suspend the timer until the transition has finished
    Suspend();

    // Load image from file
    if (!m_slides->Load(im, direction))
        // Image not yet available: show loading status
        SetStatus(tr("Loading"));
}


/*!
 \brief Calculates transition speed factor
 \details x0.5 for every slide waiting. Min = x1, Max = Half buffer size, so
 that buffer retains enough free slots
 \param available Number of slides ready for display
 \return float Factor to speed up transition by
*/
static float Speed(int available)
{
    return qMin(1.0 + available / 2.0, SLIDE_BUFFER_SIZE / 2.0);
}


/*!
 \brief Start transition
 \details Displays image that has just loaded
 \param pic Gallery cyclic slide that has loaded the image
 \param direction Navigation direction
 \param skipping True if multiple requests have been received. Transitions will
 be accelerated
*/
void GallerySlideView::SlideAvailable(int count)
{
    // Are we transitioning ?
    if (m_slideNext)
    {
        // More slides waiting for display: accelerate current transition
        m_transition->SetSpeed(Speed(count));
        return;
    }

    // We've been waiting for this slide: transition immediately
    // Reset any zoom/pan first
    Pan();
    Zoom();

    // Take next slide
    m_slideNext = m_slides->TakeNext();

    // Update loading status
    if (m_uiStatus)
    {
        if (!m_slideNext->FailedLoad())

            m_uiStatus->SetVisible(false);

        else if (ImageItem *im = m_slideNext->GetImageData())

            SetStatus(tr("Failed to load %1").arg(im->m_name));
    }

    int direction = m_slideNext->GetDirection();

    // Use special transition for updates/start-up
    Transition *transition = (direction == 0) ? &m_updateTransition : m_transition;

    // Start transition, transitioning quickly if more slides waiting
    transition->Start(m_slideCurrent, m_slideNext, direction >= 0, Speed(m_pending));
}


/*!
 \brief Transition to new slide has finished
 \details Resets buffers & old slide. Starts next transition if slide loads
 are pending (skipping). Otherwise updates text widgets for new slide, pre-loads
 next slide & starts any video.
*/
void GallerySlideView::TransitionComplete()
{
    // Release old slide. May cause further transitions but they won't be handled
    // until we've finished here
    m_pending = m_slides->Release(m_slideCurrent);

    m_slideCurrent = m_slideNext;
    m_slideNext    = NULL;

    // Update slide counts
    if (m_uiSlideCount)
        m_uiSlideCount->SetText(m_view->GetPosition());

    ImageItem *im = m_slideCurrent->GetImageData();

    LOG(VB_FILE, LOG_DEBUG, QString("Slideview: Finished transition to %1")
        .arg(im->m_fileName));

    // No further actions when skipping
    if (m_pending > 0)
        return;

    // Preload next slide, if any
    m_slides->Preload(m_view->HasNext());

    // Update any file details information
    m_infoList->Update(im);

    if (m_uiCaptionText)
    {
        // show the date & comment
        QStringList text;
        text << ImageUtils::ImageDateOf(im);

        if (!im->m_comment.isEmpty())
            text << im->m_comment;

        m_uiCaptionText->SetText(text.join(" - "));
    }

    // Start any video unless we're paused or browsing
    if (im && im->m_type == kVideoFile)
    {
        if (m_paused || m_slideShowType == kBrowseSlides)

            SetStatus(tr("Video"));
        else
            PlayVideo();
    }

    // Resume slideshow timer
    Release();
}


/*!
 \brief Display the previous slide in the sequence
*/
void GallerySlideView::ShowPrevSlide()
{
    if (m_view->Prev())
        ShowSlide(-1);
}


/*!
 \brief Display the next slide in the sequence
 \param useTransition if false, slide will be updated instantly
*/
void GallerySlideView::ShowNextSlide(bool useTransition)
{
    // Browsing always wraps; slideshows depend on repeat setting
    if (!m_view->HasNext()
            && m_slideShowType != kBrowseSlides
            && !gCoreContext->GetNumSetting("GalleryRepeat", false))
    {
        Stop();
        SetStatus(tr("End"));
    }
    else if (m_view->Next())
        ShowSlide(useTransition ? 1 : 0);
    else
    {
        // No images
        Stop();
        SetStatus(tr("Empty"));
        m_infoList->Hide();
        m_slideCurrent->Clear();
        if (m_uiSlideCount)
            m_uiSlideCount->SetText(tr("None"));
        if (m_uiCaptionText)
            m_uiCaptionText->SetText("");
    }
}


/*!
 \brief Starts internal player for video
*/
void GallerySlideView::PlayVideo()
{
    if (!m_slideCurrent || m_slideCurrent->FailedLoad())
        return;

    ImageItem *im = m_slideCurrent->GetImageData();

    if (im && im->m_type == kVideoFile)
    {
        QString url = ImageSg::getInstance()->GenerateUrl(im->m_fileName);
        GetMythMainWindow()->HandleMedia("Internal", url);
    }
}


/*!
 \brief Displays status text (Loading, Paused etc.)
 \param msg Text to show
*/
void GallerySlideView::SetStatus(QString msg)
{
    if (m_uiStatus)
    {
        m_uiStatus->SetText(msg);
        m_uiStatus->SetVisible(true);
    }
}
