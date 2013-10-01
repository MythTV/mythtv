
#include "gallerywidget.h"

// Qt headers
#include <QByteArray>
#include <QXmlStreamReader>

// MythTV headers
#include "mythcontext.h"
#include "mythmainwindow.h"

#include "imagethumbgenthread.h"


ImageLoadingThread::ImageLoadingThread() :
    m_image(NULL),
    m_imageData(NULL),
    m_url()
{

}

void ImageLoadingThread::setImage(MythUIImage *image,
                                  ImageMetadata *imageData,
                                  QString &url)
{
    m_image = image;
    m_imageData = imageData;
    m_url = url;
}

void ImageLoadingThread::run()
{
    if (m_image && m_imageData)
    {
        m_image->SetFilename(m_url);
        m_image->SetOrientation(m_imageData->GetOrientation());
        m_image->SetZoom(m_imageData->GetZoom() / 100);
        m_image->Load(false);
    }
}




/** \fn     GalleryWidget::GalleryWidget(MythScreenStack *,
 *                       const char *, GalleryViewHelper *
 *  \brief  Constructor
 *  \param  parent The screen parent
 *  \param  name The name of the screen
 *  \param  ivh Main helper class of the image plugin
 *  \return void
 */
GalleryWidget::GalleryWidget(MythScreenStack *parent,
                         const char *name,
                         GalleryViewHelper *ivh) : MythScreenType(parent, name),
    m_menuPopup(NULL),
    m_image1(NULL),
    m_image2(NULL),
    m_status(NULL),
    m_infoList(NULL),
    m_fileList(NULL),
    m_fileDataList(NULL)
{
    m_gvh = ivh;
    m_fh = new GalleryFileHelper();

    m_ilt = new ImageLoadingThread();
    connect(m_ilt, SIGNAL(finished()),
            this, SLOT(HandleImageTransition()));

    m_backendHost = gCoreContext->GetSetting("BackendServerIP","localhost");
    m_backendPort = gCoreContext->GetSetting("BackendServerPort", "6543");

    m_slideShowType = kNoSlideShow;
    m_slideShowTime = gCoreContext->GetNumSetting("GallerySlideShowTime", 3000);
    m_transitionType = gCoreContext->GetNumSetting("GalleryTransitionType", kFade);

    // calculate the alphachange from the specified transition time.
    // an alphachange of 1 would take approx. 3,64s because the alpha value is
    // changed 70 times per second. 3,64 * 70 = 255 (max alpha).
    // example: 2 = abs(36400 / 15000) with transition time of 1500
    m_transitionTime = abs(((255 / 70) * 10000) /
                           (gCoreContext->GetNumSetting("GalleryTransitionTime", 1000) * 10));

    m_popupStack = GetMythMainWindow()->GetStack("popup stack");

    // Save the selected image in the gallery view
    // so that the initially selected image can be
    // restored when the GalleryWidget is detroyed.
    m_selectedNode = m_gvh->m_currentNode->getSelectedChild();

    // this timer is used for the slideshow
    m_timer = new QTimer();
    m_timer->setSingleShot(true);
    m_timer->setInterval(m_slideShowTime);

    m_infoVisible = false;
    m_index = 0;

    m_gvh->m_thumbGenThread->Pause();
}



/** \fn     GalleryWidget::~GalleryWidget()
 *  \brief  Destructor
 */
GalleryWidget::~GalleryWidget()
{
    if (m_timer)
        m_timer->stop();

    delete m_timer;
    m_timer = NULL;

    delete m_fileList;
    m_fileList = NULL;

    delete m_fileDataList;
    m_fileDataList = NULL;

    delete m_fh;
    m_fh = NULL;

    if (m_ilt)
        m_ilt->wait();

    delete m_ilt;
    m_ilt = NULL;

    // Set the selected image in the gallery view to the saved one
    // so that the initially selected image is the correct one again.
    m_gvh->m_currentNode->setSelectedChild(m_selectedNode);

    m_gvh->m_thumbGenThread->Resume();
}



/** \fn     GalleryWidget::Create()
 *  \brief  Initialises and shows the graphical elements
 *  \return True if successful otherwise false
 */
bool GalleryWidget::Create()
{
    if (!LoadWindowFromXML("image-ui.xml", "slideshow", this))
        return false;

    bool err = false;

    // Widget for showing the images
    UIUtilE::Assign(this, m_image1, "first_image", &err);
    UIUtilE::Assign(this, m_image2, "second_image", &err);
    UIUtilW::Assign(this, m_status, "status");

    // Widgets to show the details to an image
    UIUtilE::Assign(this, m_infoList, "infolist", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'slideshow'");
        return false;
    }

    HideFileDetails();
    BuildFocusList();
    SetFocusWidget(m_image1);

    m_fileDataList = new QList<ImageMetadata *>();
    m_fileDataList->append(new ImageMetadata());
    m_fileDataList->append(new ImageMetadata());

    m_fileList = new QList<MythUIImage *>();
    m_fileList->append(m_image1);
    m_fileList->append(m_image2);

    return true;
}



/** \fn     GalleryWidget::keyPressEvent(QKeyEvent *)
 *  \brief  Translates the keypresses and keys bound to the
 *          plugin to specific actions within the plugin
 *  \param  event The pressed key
 *  \return True if key was used, otherwise false
 */
bool GalleryWidget::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Images", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "LEFT")
            ShowPrevFile();
        else if (action == "RIGHT")
            ShowNextFile();
        else if (action == "INFO")
            ShowFileDetails();
        else if (action == "MENU")
            MenuMain();
        else if (action == "PLAY")
        {
            // If no slideshow is active and the user presses the play
            // button then start a normal slideshow. But if a slideshow
            // is already running then start or pause it.
            if (m_slideShowType == kNoSlideShow)
                StartNormalSlideShow();
            else
            {
                if (m_timer->isActive())
                    PauseSlideShow();
                else
                    ResumeSlideShow();
            }
        }
        else if (action == "PAUSE")
            PauseSlideShow();
        else if (action == "STOP")
            StopSlideShow();
        else if (action == "ROTRIGHT")
        {
            m_gvh->SetFileOrientation(kFileRotateCW);
            LoadFile();
        }
        else if (action == "ROTLEFT")
        {
            m_gvh->SetFileOrientation(kFileRotateCCW);
            LoadFile();
        }
        else if (action == "FLIPHORIZONTAL")
        {
            m_gvh->SetFileOrientation(kFileFlipHorizontal);
            LoadFile();
        }
        else if (action == "FLIPVERTICAL")
        {
            m_gvh->SetFileOrientation(kFileFlipVertical);
            LoadFile();
        }
        else if (action == "ZOOMIN")
        {
            m_gvh->SetFileZoom(kFileZoomIn);
            LoadFile();
        }
        else if (action == "ZOOMOUT")
        {
            m_gvh->SetFileZoom(kFileZoomOut);
            LoadFile();
        }
        else if (action == "ESCAPE")
        {
            if (m_infoVisible)
                HideFileDetails();
            else
                handled = false;
        }
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}



/** \fn     GalleryWidget::customEvent(QEvent *event)
 *  \brief  Translates the keypresses to
 *          specific actions within the plugin
 *  \param  event The custom event
 */
void GalleryWidget::customEvent(QEvent *event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = (DialogCompletionEvent*)(event);

        QString resultid  = dce->GetId();
        int     buttonnum = dce->GetResult();

        if (resultid == "metadatamenu")
        {
            switch (buttonnum)
            {
            case 0 :
                m_gvh->SetFileOrientation(kFileRotateCW);
                LoadFile();
                break;
            case 1 :
                m_gvh->SetFileOrientation(kFileRotateCCW);
                LoadFile();
                break;
            case 2 :
                m_gvh->SetFileOrientation(kFileFlipHorizontal);
                LoadFile();
                break;
            case 3 :
                m_gvh->SetFileOrientation(kFileFlipVertical);
                LoadFile();
                break;
            case 4 :
                m_gvh->SetFileZoom(kFileZoomIn);
                LoadFile();
                break;
            case 5 :
                m_gvh->SetFileZoom(kFileZoomOut);
                LoadFile();
                break;
            }
        }

        m_menuPopup = NULL;
    }
}



/** \fn     GalleryView::MenuMain()
 *  \brief  Shows a dialog popup with the main menu
 *  \return void
 */
void GalleryWidget::MenuMain()
{
    // Create the main menu that will contain the submenus above
    MythMenu *menu = new MythMenu(tr("Image Information"), this, "mainmenu");

    // If no slideshow type was given show the item to start it
    // otherwise show the items to stop or resume a slideshow.
    if (m_slideShowType == kNoSlideShow)
    {
        menu->AddItem(tr("Start Normal SlideShow"),
                      SLOT(StartNormalSlideShow()));
        menu->AddItem(tr("Start Random SlideShow"),
                      SLOT(StartRandomSlideShow()));
    }
    else
    {
        if (m_timer->isActive())
            menu->AddItem(tr("Pause SlideShow"), SLOT(PauseSlideShow()));
        else
        {
            if (m_slideShowType == kNormalSlideShow)
                menu->AddItem(tr("Resume SlideShow"),
                              SLOT(StartNormalSlideShow()));

            if (m_slideShowType == kRandomSlideShow)
                menu->AddItem(tr("Resume SlideShow"),
                              SLOT(StartRandomSlideShow()));
        }
    }

    MenuMetadata(menu);
    menu->AddItem(tr("Show Details"),   SLOT(FileDetails()));

    m_menuPopup = new MythDialogBox(menu, m_popupStack, "menuPopup");
    if (!m_menuPopup->Create())
    {
        delete m_menuPopup;
        m_menuPopup = NULL;
        return;
    }

    m_popupStack->AddScreen(m_menuPopup);
}



/** \fn     GalleryWidget::MenuMetadata(MythMenu *)
 *  \brief  Adds a new metadata menu entry into the main menu
 *  \param  mainMenu Parent that will hold the menu entry
 *  \return void
 */
void GalleryWidget::MenuMetadata(MythMenu *mainMenu)
{
    MythMenu *menu = new MythMenu(tr("Metadata Options"),
                                  this, "metadatamenu");

    menu->AddItem(tr("Rotate CW"));
    menu->AddItem(tr("Rotate CCW"));
    menu->AddItem(tr("Flip Horizontal"));
    menu->AddItem(tr("Flip Vertical"));

    if (m_fileDataList->at(m_index))
    {
        if (m_fileDataList->at(m_index)->GetZoom() < 300)
            menu->AddItem(tr("Zoom In"));

        if (m_fileDataList->at(m_index)->GetZoom() > 0)
            menu->AddItem(tr("Zoom Out"));
    }

    mainMenu->AddItem(tr("Meta Data Menu"), NULL, menu);
}



/** \fn     GalleryWidget::HideFileDetails()
 *  \brief  Hides the details of the current file
 *  \return void
 */
void GalleryWidget::HideFileDetails()
{
    m_infoList->SetVisible(false);
    m_infoVisible = false;
    SetFocusWidget(m_image1);
}



/** \fn     GalleryWidget::ShowFileDetails()
 *  \brief  Shows the available details of the current image file.
            The details will only be shown if the file is an image.
 *  \return void
 */
void GalleryWidget::ShowFileDetails()
{
    ImageMetadata *im = m_fileDataList->at(m_index);
    if (!im)
        return;

    if (im->m_type != kImageFile)
    {
        delete im;
        return;
    }

    // First remove all entries
    m_infoList->Reset();

    // This map holds all the exif tag values
    QMap<QString, QString> infoList;

    // Get all the available exif header information from the file
    // and create a data structure that can be displayed nicely
    QByteArray ba = m_fh->GetExifValues(im);
    if (ba.count() > 0)
    {
        bool readTagValues = false;
        QString key, value;

        QXmlStreamReader xml(ba);
        while (!xml.atEnd())
        {
            xml.readNext();

            // Read the general information
            if (xml.isStartElement() &&
                    (xml.name() == "Count"  ||
                     xml.name() == "File"   ||
                     xml.name() == "Path"   ||
                     xml.name() == "Size"   ||
                     xml.name() == "Extension"))
                infoList.insert(xml.name().toString(), xml.readElementText());

            if (xml.isStartElement() && xml.name() == "ImageMetadataInfo")
                readTagValues = true;

            if (readTagValues)
            {
                if (xml.isStartElement() && xml.name() == "Label")
                    key = xml.readElementText();

                if (xml.isStartElement() && xml.name() == "Value")
                    value = xml.readElementText();
            }

            if (xml.isEndElement() && xml.name() == "ImageMetadataInfo")
            {
                readTagValues = false;
                infoList.insert(key, value);
            }
        }
    }

    // Now go through the info list and create a map for the mythui buttonlist
    QMap<QString, QString>::const_iterator i = infoList.constBegin();
    while (i != infoList.constEnd())
    {
        MythUIButtonListItem *item = new MythUIButtonListItem(m_infoList, "");
        InfoMap infoMap;
        infoMap.insert("name", i.key());

        QString value = tr("Not defined");
        if (!i.value().isEmpty())
            value = i.value();

        infoMap.insert("value", value);

        item->SetTextFromMap(infoMap);

        ++i;
    }

    m_infoList->SetVisible(true);

    // All widgets are visible, remember this
    m_infoVisible = true;
    SetFocusWidget(m_infoList);

    delete im;
}



/** \fn     GalleryWidget::LoadFile()
 *  \brief  Stops any slideshow and loads the file
 *          from disk or memory in the background.
 *  \return void
 */
void GalleryWidget::LoadFile()
{
    // Pause the slideshow so that the timer can't fire
    // until the image loading thread has finished
    PauseSlideShow();

    // Switch the index
    m_index = (m_index == 0) ? 1 : 0;

    // Load the new data into the new index, this will be
    // the new image which will be shown in the foreground
    m_fileDataList->replace(m_index, m_gvh->GetImageMetadataFromSelectedNode());

    if (m_fileDataList->at(m_index))
    {
        // Show an information that
        // the image is being loaded
        if (m_status)
            m_status->SetVisible(true);

        m_fileList->at(m_index)->SetAlpha(0);

        // Get the full path and name of the image
        // or if its a video the first preview image
        QString fileName = m_fileDataList->at(m_index)->m_fileName;
        if (m_fileDataList->at(m_index)->m_type == kVideoFile)
            fileName = m_fileDataList->at(m_index)->m_thumbFileNameList->at(0);

        QString url = CreateImageUrl(fileName);

        // This thread will loads the image so the UI is not blocked.
        m_ilt->setImage(m_fileList->at(m_index),
                        m_fileDataList->at(m_index), url);
        m_ilt->start();
    }
}



/** \fn     GalleryWidget::CreateImageUrl(QString &fileName)
 *  \brief
 *  \return QString
 */
QString GalleryWidget::CreateImageUrl(QString &fileName)
{
    QString url = gCoreContext->GenMythURL(gCoreContext->GetSetting("MasterServerIP"),
                                           gCoreContext->GetNumSetting("MasterServerPort"),
                                           fileName,
                                           m_gvh->m_sgName);

    LOG(VB_GENERAL, LOG_INFO, QString("Loading image from url '%1'").arg(url));

    return url;
}



/** \fn     GalleryWidget::HandleImageTransition()
 *  \brief  Handles any specified effects between two
 *          images when the file loading was completed
 *  \return void
 */
void GalleryWidget::HandleImageTransition()
{
    // If the image loading is done then resume the slideshow which was
    // only paused to not to interfere with the loading process.
    ResumeSlideShow();

    // Also update the file details information in case its visible.
    if (m_infoVisible)
        ShowFileDetails();

    // Hide the status information
    // that can be displayed in the themes
    if (m_status)
        m_status->SetVisible(false);

    switch (m_transitionType)
    {
    case kFade:
        HandleFadeTransition();
        break;

    default:
        HandleNoTransition();
        break;
    }
}



/** \fn     GalleryWidget::HandleFadeTransition()
 *  \brief  Handles the fading between two images
 *  \return void
 */
void GalleryWidget::HandleFadeTransition()
{
    int index = (m_index == 0) ? 1 : 0;

    // Fade out the old image if its loaded. This will not be done when
    // called for the first time because there is no object at this index
    if (m_fileDataList->at(index))
        m_fileList->at(index)->AdjustAlpha(1, (m_transitionTime * -1), 0, 255);

    // Fade in the new image and if the next
    // file is a video then start playing it
    if (m_fileDataList->at(m_index))
    {
        m_fileList->at(m_index)->AdjustAlpha(1, m_transitionTime, 0, 255);
        if (m_fileDataList->at(m_index)->m_type == kVideoFile)
        {
            if (m_fileDataList->at(index))
                m_fileList->at(index)->SetAlpha(0);

            PauseSlideShow();
            GetMythMainWindow()->HandleMedia("Internal",
                                             m_fileDataList->at(m_index)->m_fileName);
        }
    }
}



/** \fn     GalleryWidget::HandleNoTransition()
 *  \brief  Just like the HandleFadeTransition() method but without any effects.
 *  \return void
 */
void GalleryWidget::HandleNoTransition()
{
    int index = (m_index == 0) ? 1 : 0;

    // Hide the old image if its loaded. This will not be done when
    // called for the first time because there is no object at this index
    if (m_fileDataList->at(index))
        m_fileList->at(index)->SetAlpha(0);

    // Immediately show the new image and if the next
    // file is a video then start playing it
    if (m_fileDataList->at(m_index))
    {
        m_fileList->at(m_index)->SetAlpha(255);
        if (m_fileDataList->at(m_index)->m_type == kVideoFile)
        {
            PauseSlideShow();
            GetMythMainWindow()->HandleMedia("Internal",
                                             m_fileDataList->at(m_index)->m_fileName);
        }
    }
}



/** \fn     GalleryWidget::ShowPrevFile()
 *  \brief  Loads the previous image file if possible
 *  \return void
 */
void GalleryWidget::ShowPrevFile()
{
    MythGenericTree *node = m_gvh->m_currentNode->getSelectedChild()->prevSibling(1);
    ImageMetadata *im = m_gvh->GetImageMetadataFromNode(node);

    // If a data object exists then a node must also exist
    if (im && (im->m_type == kImageFile ||
                 im->m_type == kVideoFile))
    {
        m_gvh->m_currentNode->setSelectedChild(node);
        LoadFile();
    }
    else
    {
        QString msg = "You have reached the beginning of the gallery.";
        ShowInformation(msg);
    }
}



/** \fn     GalleryWidget::ShowNextFile()
 *  \brief  Loads the next image file if possible
 *  \return void
 */
void GalleryWidget::ShowNextFile()
{
    PauseSlideShow();

    MythGenericTree *node = m_gvh->m_currentNode->getSelectedChild()->nextSibling(1);
    ImageMetadata *im = m_gvh->GetImageMetadataFromNode(node);

    // If a data object exists then a node must also exist
    if (im && (im->m_type == kImageFile ||
                 im->m_type == kVideoFile))
    {
        m_gvh->m_currentNode->setSelectedChild(node);
        LoadFile();
    }
    else
    {
        QString msg = "You have reached the end of the gallery.";
        ShowInformation(msg);
    }
}



/** \fn     GalleryWidget::ShowRandomFile()
 *  \brief  Loads a random image
 *  \return void
 */
void GalleryWidget::ShowRandomFile()
{
    volatile bool exit = false;

    int size = m_gvh->m_currentNode->getSelectedChild()->siblingCount();
    int counter = 0;
    int position = 0;

    MythGenericTree *node = NULL;
    ImageMetadata *im = NULL;

    PauseSlideShow();

    // Get a random node from the list. If its not an image or
    // video continue and try to get a new one until its an image
    // or all siblings have been checked.
    if (size > 0)
    {
        while (!exit)
        {
            position = (qrand() % size);
            counter++;

            node = m_gvh->m_currentNode->getChildAt(position);
            im = m_gvh->GetImageMetadataFromNode(node);

            if ((im && (im->m_type == kImageFile ||
                          im->m_type == kVideoFile)) || counter == size)
                exit = true;
        }

        // If we have data then is is already an image or
        // video. This has been checked in the while loop
        if (im)
        {
            m_gvh->m_currentNode->setSelectedChild(node);
            LoadFile();
        }
    }
    else
    {
        QString msg = "There are no images in the current directory.";
        ShowInformation(msg);
    }
}



/** \fn     GalleryWidget::ShowInformation(QString &msg)
 *  \brief  Shows a message to the user
 *  \param  msg The message that shall be shown
 *  \return void
 */
void GalleryWidget::ShowInformation(QString &msg)
{
    MythConfirmationDialog *okPopup =
            new MythConfirmationDialog(m_popupStack, msg, false);

    if (okPopup->Create())
        m_popupStack->AddScreen(okPopup);
    else
        delete okPopup;
}



/** \fn     GalleryWidget::StartNormalSlideShow()
 *  \brief  Starts a slideshow where the images are shown in normal order
 *  \return void
 */
void GalleryWidget::StartNormalSlideShow()
{
    if (!m_timer)
        return;

    StopSlideShow();
    m_slideShowType = kNormalSlideShow;
    connect(m_timer, SIGNAL(timeout()), this, SLOT(ShowNextFile()));
    ResumeSlideShow();
}



/** \fn     GalleryWidget::StartRandomSlideShow()
 *  \brief  Starts a slideshow where the images are shown randomly
 *  \return void
 */
void GalleryWidget::StartRandomSlideShow()
{
    if (!m_timer)
        return;

    StopSlideShow();
    m_slideShowType = kRandomSlideShow;
    connect(m_timer, SIGNAL(timeout()), this, SLOT(ShowRandomFile()));
    ResumeSlideShow();
}



/** \fn     GalleryWidget::ResumeSlideShow()
 *  \brief  Resumes a paused slideshow
 *  \return void
 */
void GalleryWidget::ResumeSlideShow()
{
    if (!m_timer)
        return;

    m_timer->start();
}



/** \fn     GalleryWidget::PauseSlideShow()
 *  \brief  Pauses an active slideshow
 *  \return void
 */
void GalleryWidget::PauseSlideShow()
{
    if (!m_timer)
        return;

    m_timer->stop();
}



/** \fn     GalleryWidget::StopSlideShow()
 *  \brief  Stops an active slideshow
 *  \return void
 */
void GalleryWidget::StopSlideShow()
{
    if (!m_timer)
        return;

    m_timer->stop();
    m_timer->disconnect();

    m_slideShowType = kNoSlideShow;
}
