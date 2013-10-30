
#include "galleryview.h"

// Qt headers

// MythTV headers
#include "mythcontext.h"

#include "galleryconfig.h"
#include "gallerytypedefs.h"
#include "imagescan.h"



/** \fn     GalleryView::GalleryView(MythScreenStack *, const char *)
 *  \brief  Constructor
 *  \param  parent The screen parent
 *  \param  name The name of the screen
 *  \return void
 */
GalleryView::GalleryView(MythScreenStack *parent, const char *name)
    : MythScreenType(parent, name),
      m_menuPopup(NULL),
      m_confirmPopup(NULL),
      m_inputPopup(NULL),
      m_imageList(NULL),
      m_captionText(NULL),
      m_crumbsText(NULL),
      m_positionText(NULL),
      m_imageText(NULL),
      m_selectedImage(NULL),
      m_syncProgressText(NULL),
      m_thumbProgressText(NULL)
{
    gCoreContext->addListener(this);

    m_popupStack = GetMythMainWindow()->GetStack("popup stack");
    m_mainStack = GetMythMainWindow()->GetMainStack();

    // interface between the frontend and the data
    m_galleryViewHelper = new GalleryViewHelper(this);

    // Fetches the syncronization status in the
    // background and updates a theme widget
    m_syncStatusThread = new GallerySyncStatusThread();

    connect(m_syncStatusThread,  SIGNAL(UpdateSyncProgress(int, int)),
            this,   SLOT(UpdateSyncProgress(int, int)));

    connect(m_syncStatusThread,  SIGNAL(finished()),
            this,   SLOT(ResetSyncProgress()));

    // Start the sync status thread so that an already 
    // running  background sync can be seen
    m_syncStatusThread->start();
}



/** \fn     GalleryView::~GalleryView()
 *  \brief  Destructor
 *  \return void
 */
GalleryView::~GalleryView()
{
    gCoreContext->removeListener(this);

    if (m_syncStatusThread)
    {
        m_syncStatusThread->quit();
        m_syncStatusThread->wait();
        delete m_syncStatusThread;
        m_syncStatusThread = NULL;
    }

    if (m_galleryViewHelper)
    {
        delete m_galleryViewHelper;
        m_galleryViewHelper = NULL;
    }
}



/** \fn     GalleryView::Create()
 *  \brief  Initialises and shows the graphical elements
 *  \return True if successful, otherwise false
 */
bool GalleryView::Create()
{
    if (!LoadWindowFromXML("image-ui.xml", "gallery", this))
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_imageList,     "images", &err);
    UIUtilW::Assign(this, m_captionText,   "title");
    UIUtilW::Assign(this, m_imageText,     "noimages");
    UIUtilW::Assign(this, m_selectedImage, "selectedimage");
    UIUtilW::Assign(this, m_positionText,  "position");
    UIUtilW::Assign(this, m_crumbsText,    "breadcrumbs");

    UIUtilW::Assign(this, m_syncProgressText, "syncprogresstext");
    UIUtilW::Assign(this, m_thumbProgressText, "thumbprogresstext");

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'gallery'");
        return false;
    }

    // set the size of the preview images (usually the thumbnails)
    m_galleryViewHelper->SetPreviewImageSize(m_imageList);

    if (m_syncProgressText)
        m_syncProgressText->SetVisible(false);

    if (m_thumbProgressText)
        m_thumbProgressText->SetVisible(false);

    BuildFocusList();
    SetFocusWidget(m_imageList);

    // connect the widgets with their slot methods
    connect(m_imageList, SIGNAL(itemClicked(MythUIButtonListItem *)),
            SLOT(ItemSelected(MythUIButtonListItem *)));
    connect(m_imageList, SIGNAL(itemSelected(MythUIButtonListItem *)),
            SLOT(UpdateImageItem(MythUIButtonListItem *)));

    return true;
}



/** \fn     GalleryView::keyPressEvent(QKeyEvent *)
 *  \brief  Translates the keypresses and keys bound to the
 *          plugin to specific actions within the plugin
 *  \param  event The pressed key
 *  \return True if key was used, otherwise false
 */
bool GalleryView::keyPressEvent(QKeyEvent *event)
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

        if (action == "MENU")
            MenuMain();
        else if (action == "INFO")
            MenuInformation();
        else if (action == "ROTRIGHT")
            FileRotateCW();
        else if (action == "ROTLEFT")
            FileRotateCCW();
        else if (action == "FLIPHORIZONTAL")
            FileFlipHorizontal();
        else if (action == "FLIPVERTICAL")
            FileFlipVertical();
        else if (action == "ZOOMIN")
            FileZoomIn();
        else if (action == "ZOOMOUT")
            FileZoomOut();
        else if (action == "SLIDESHOW")
            ShowFiles();
        else if (action == "RANDOMSHOW")
            ShowRandomFiles();
        else if (action == "MARK")
        {
            ImageMetadata *im = GetImageMetadataFromSelectedButton();
            if (im)
            {
                if (im->m_selected)
                    FileSelectOne();
                else
                    FileDeselectOne();
            }
        }
        else if (action == "ESCAPE")
        {
            // If the jumppoint is not active and the first node is of
            // the type kUpFolder then allow going up one directory
            if (!GetMythMainWindow()->IsExitingToMain())
            {
                ImageMetadata *data = m_galleryViewHelper->GetImageMetadataFromNode(0);
                if (data && data->m_type == kUpDirectory)
                    handled = DirSelectUp();
                else
                    handled = false;
            }
            else
                handled = false;
        }
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}



/** \fn     GalleryView::customEvent(QEvent *)
 *  \brief  Translates the keypresses to specific actions within the plugin
 *  \param  event The custom event
 *  \return void
 */
void GalleryView::customEvent(QEvent *event)
{
    if ((MythEvent::Type)(event->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)event;
        QString message = me->Message();

        if (message.startsWith("IMAGE_THUMB_CREATED"))
        {
            QStringList tokens = message.simplified().split(" ");
            int fileid = 0;
            if (tokens.size() >= 2)
            {
                fileid = tokens[1].toUInt();

                // FIXME: This sucks, must be a better way to do this
                //
                // get through the entire list of image items and find
                // the fileid that matches the created thumbnail filename
                for (int i = 0; i < m_imageList->GetCount(); i++)
                {
                    MythUIButtonListItem *item = m_imageList->GetItemAt(i);
                    if (!item)
                        continue;

                    ImageMetadata *im = GetImageMetadataFromButton(item);
                    if (!im)
                        continue;

                    if (im->m_id == fileid)
                    {
                        UpdateThumbnail(item, true);
                        break;
                    }
                }
            }
        }
    }
    else if (event->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = (DialogCompletionEvent*)(event);

        QString resultid  = dce->GetId();
        int     buttonnum = dce->GetResult();

        // Confirm current file deletion
        if (resultid == "confirmdelete")
        {
            switch (buttonnum)
            {
            case 0 :
                break;
            case 1 :
                FileDelete();
                break;
            }
        }

        // Confirm all selected file deletion
        if (resultid == "confirmdeleteselected")
        {
            switch (buttonnum)
            {
            case 0 :
                break;
            case 1 :
                FileDeleteSelected();
                break;
            }
        }

        // Synchronize the database
        if (resultid == "confirmstartsync")
        {
            switch (buttonnum)
            {
            case 0 :
                break;
            case 1 :
                // Start the sync, the API call will
                // check if a sync is running already
                m_galleryViewHelper->m_fileHelper->StartSyncImages();

                if (!m_syncStatusThread->isRunning())
                    m_syncStatusThread->start();

                break;
            }
        }

        // Stop the database sync
        if (resultid == "confirmstopsync")
        {
            switch (buttonnum)
            {
            case 0 :
                break;
            case 1 :
                if (m_syncStatusThread->isRunning())
                    m_syncStatusThread->quit();

                m_galleryViewHelper->m_fileHelper->StopSyncImages();
                break;
            }
        }

        if (resultid == "filerename")
        {
            QString newName = dce->GetResultText();
            FileRename(newName);
        }
    }
}



/** \fn     GalleryView::ResetImageItems()
 *  \brief  Resets the image related widgets by clearing all
            visible items and removing any shown text.
 *  \return void
 */
void GalleryView::ResetImageItems()
{
    m_imageList->Reset();

    if (m_positionText)
        m_positionText->Reset();

    if (m_captionText)
        m_captionText->Reset();

    if (m_crumbsText)
        m_crumbsText->Reset();
}



/** \fn     GalleryView::LoadData()
 *  \brief  Loads the available data from the database.
            If there is no data available the user needs to rescan.
 *  \return void
 */
void GalleryView::LoadData()
{
    ResetImageItems();

    m_imageText->SetText("Loading available images...");
    m_imageText->SetVisible(true);
    m_imageList->SetVisible(false);

    // loads the data from the database at the specified path
    int status = m_galleryViewHelper->LoadData();

    switch (status)
    {
    case kStatusNoBaseDir:
        m_imageText->SetText("No image storage group has been defined.\n"
                             "Please check the stoage group settings\n"
                             "and the directory permissions.");
        break;
    case kStatusNoFiles:
        m_imageText->SetText("No images in the database found.\n"
                             "You need to scan for new images.");
        break;
    case kStatusOk:
        m_imageText->SetText("");
        m_imageText->SetVisible(false);
        m_imageList->SetVisible(true);

        // set the first node as the selected node
        m_galleryViewHelper->m_currentNode->setSelectedChild(m_galleryViewHelper->m_currentNode->getChildAt(0));

        // loads the data from the MythGenericTree into the image list
        UpdateImageList();
        break;
    }
}



/** \fn     GalleryView::UpdateImageList()
 *  \brief  Updates the visible items
 *  \return void
 */
void GalleryView::UpdateImageList()
{
    m_imageList->Reset();

    // get all children from the the selected node
    MythGenericTree *selectedNode = m_galleryViewHelper->m_currentNode->getSelectedChild();
    QList<MythGenericTree *> *childs = m_galleryViewHelper->m_currentNode->getAllChildren();

    // go through the entire list and update
    QList<MythGenericTree *>::const_iterator it;
    for (it = childs->begin(); it != childs->end(); ++it)
    {
        if (*it != NULL)
        {
            MythUIButtonListItem *item = new MythUIButtonListItem(
                    m_imageList, QString(), 0,
                    true, MythUIButtonListItem::NotChecked);
            item->SetData(qVariantFromValue(*it));

            // assign and display all information about
            // the current item, like title and subdirectory count
            UpdateImageItem(item);

            // set the currently active node as selected in the image list
            if (*it == selectedNode)
                m_imageList->SetItemCurrent(item);
        }
    }

    // when the UpdateImageItem method is called the current node will also
    // be set to the current image item. After updating all items in the
    // image list we need to set the current node back to the on it was before
    m_galleryViewHelper->m_currentNode->setSelectedChild(selectedNode);

    // Updates all other widgets on the screen that show
    // information about the selected MythUIButtonListItem
    UpdateText(m_imageList->GetItemCurrent());
}



/** \fn     GalleryView::UpdateImageItem(MythUIButtonListItem *)
 *  \brief  Updates the visible representation of a MythUIButtonListItem
 *  \param  item The item that shall be updated
 *  \return void
 */
void GalleryView::UpdateImageItem(MythUIButtonListItem *item)
{
    if (!item)
        return;

    MythGenericTree *node = item->GetData().value<MythGenericTree *>();
    if (!node)
        return;

    // update the position in the node list
    m_galleryViewHelper->m_currentNode->setSelectedChild(node);

    ImageMetadata *im = node->GetData().value<ImageMetadata *>();
    if (!im)
        return;

    // Depending what the themer has done, display a small
    // icon that shows if the current item is a file or a folder
    // Also show an additional background image if required. This is
    // primarily useful when the item is a folder and a folder background
    // image shall be shown behind the small preview thumbnail images
    switch (im->m_type)
    {
    case kSubDirectory:

        item->SetText(QString::number(im->m_dirCount), "childcount");

        if (im->m_fileCount > 0)
            item->SetText(QString("%1/%2")
                          .arg(im->m_dirCount)
                          .arg(im->m_fileCount), "childcount");

        item->DisplayState("subfolder", "nodetype");
        break;

    case kUpDirectory:
        item->DisplayState("upfolder", "nodetype");
        break;

    case kImageFile:
        item->DisplayState("image", "nodetype");
        break;

    case kVideoFile:
        item->DisplayState("video", "nodetype");
        break;

    default:
        break;
    }

    // set the image as hidden or visible
    QString state = (im->m_isHidden) ? "hidden" : "visible";
    item->DisplayState(state, "nodevisibility");

    item->SetText(im->m_name, "title");
    item->SetText(im->m_name);

    // set the image as selected
    item->setChecked(MythUIButtonListItem::NotChecked);
    item->DisplayState("off", "nodecheck");
    if (im->m_selected)
    {
        item->setChecked(MythUIButtonListItem::FullChecked);
        item->DisplayState("full", "nodecheck");
    }

    // update the other widgets in the screen
    if (item == m_imageList->GetItemCurrent())
        UpdateText(item);

    // set the thumbnail image
    UpdateThumbnail(item);
}



/** \fn     GalleryView::UpdateThumbnail(MythUIButtonListItem *)
 *  \brief  Updates the thumbnail image of the given item
 *  \param  item The item that shall be updated
 *  \return void
 */
void GalleryView::UpdateThumbnail(MythUIButtonListItem *item, bool forceReload)
{
    if (!item)
        return;

    ImageMetadata *im = GetImageMetadataFromButton(item);
    if (!im)
        return;

    if (im->m_type == kUpDirectory || im->m_type == kSubDirectory)
    {
        for (int i = 0; i < im->m_thumbFileNameList->size(); ++i)
        {
            item->SetImage(im->m_thumbFileNameList->at(i),
                           QString("thumbimage%1").arg(i+1), forceReload);
        }
    }
    else
    {
        item->SetImage(im->m_thumbFileNameList->at(0), "", forceReload);
    }
}



/** \fn     GalleryView::UpdateThumbnail(ImageMetadata *, int)
 *  \brief  Updates the thumbnail image of an image or
 *          folder which contains the given image metadata
 *  \param  item The item that shall be updated
 *  \param  id The thumbnail id that shall be used,
 *          there are 4 ids when the item is a folder
 *  \return void
 */
void GalleryView::UpdateThumbnail(ImageMetadata *thumbImageMetadata, int id)
{
    // get through the entire list of image items and find
    // the filename that matches the created thumbnail filename
    for (int i = 0; i < m_imageList->GetCount(); i++)
    {
        MythUIButtonListItem *item = m_imageList->GetItemAt(i);
        if (!item)
            continue;

        ImageMetadata *im = GetImageMetadataFromButton(item);
        if (!im)
            continue;

        // Set the thumbnail image if the thumbnail
        // image names at the given index are the same
        if (thumbImageMetadata->m_thumbFileNameList->at(id).compare(
                    im->m_thumbFileNameList->at(id)) == 0)
        {
            // Set the images for the four thumbnail image widgets in case
            // the node is a folder. Otherwise set the buttonimage widget.
            if (im->m_type == kUpDirectory ||
                im->m_type == kSubDirectory)
            {
                item->SetImage(thumbImageMetadata->m_thumbFileNameList->at(id),
                               QString("thumbimage%1").arg(id+1), true);
            }
            else
            {
                item->SetImage(thumbImageMetadata->m_thumbFileNameList->at(0), "", true);
            }
            break;
        }
    }
}



/** \fn     GalleryView::ResetThumbnailProgress()
 *  \brief  Resets the status of the progress text
 *  \return void
 */
void GalleryView::ResetThumbnailProgress()
{
    if (m_thumbProgressText)
        m_thumbProgressText->SetVisible(false);
}



/** \fn     GalleryView::ResetSyncProgress()
 *  \brief  Resets the status of the progress text
 *  \return void
 */
void GalleryView::ResetSyncProgress()
{
    if (m_syncProgressText)
        m_syncProgressText->SetVisible(false);

    LoadData();
}



/** \fn     GalleryView::UpdateThumbnailProgress(int, int)
 *  \brief  Updates the dialog that shows the thumbnail creation progress
 *  \param  remaining The number of thumbnail still to be created
 *  \param  total The overall number of thumbnail that will be created
 *  \return void
 */
void GalleryView::UpdateThumbnailProgress(int remaining, int total)
{
    int current = total - remaining;

    if (m_thumbProgressText)
    {
        m_thumbProgressText->SetVisible(true);
        m_thumbProgressText->SetText(QString("%1 %2 %3")
                                     .arg(current)
                                     .arg(tr("of"))
                                     .arg(total));
    }
}



/** \fn     GalleryView::UpdateSyncProgress(int, int)
 *  \brief  Updates the widget that shows the sync progress
 *  \param  remaining The number of images still to be synced
 *  \param  total The overall number of images that will be synced
 *  \return void
 */
void GalleryView::UpdateSyncProgress(int current, int total)
{
    if (m_syncProgressText)
    {
        m_syncProgressText->SetVisible(true);
        m_syncProgressText->SetText(QString("%1 %2 %3")
                                     .arg(current)
                                     .arg(tr("of"))
                                     .arg(total));
    }
}



/** \fn     GalleryView::UpdateText(MythUIButtonListItem *)
 *  \brief  Updates all other widgets on the screen that show
 *          information about the selected MythUIButtonListItem.
 *  \param  item The item that shall be updated
 *  \return void.
 */
void GalleryView::UpdateText(MythUIButtonListItem *item)
{
    ImageMetadata *im = GetImageMetadataFromButton(item);
    if (im)
    {
        // show the name of the image
        if (m_captionText)
            m_captionText->SetText(im->m_name);

        // show the position of the image
        if (m_positionText)
            m_positionText->SetText(QString("%1 of %2")
                                    .arg(m_imageList->GetCurrentPos()+1)
                                    .arg(m_imageList->GetCount()));

        // show the path of the image
        if (m_crumbsText)
            m_crumbsText->SetText(im->m_path);
    }
}



/** \fn     GalleryView::ConfirmStartSync()
 *  \brief  Shows a confirmation dialog so the user can confirm his request
 *  \return void
 */
void GalleryView::ConfirmStartSync()
{
    QString msg = QString("Do you really want to synchronize?\n"
                          "This could take some time.");
    m_confirmPopup = new MythConfirmationDialog(m_popupStack, msg, true);

    if (m_confirmPopup->Create())
    {
        m_confirmPopup->SetReturnEvent(this, "confirmstartsync");
        m_popupStack->AddScreen(m_confirmPopup);
    }
    else
        delete m_confirmPopup;
}



/** \fn     GalleryView::ConfirmStopSync()
 *  \brief  Shows a confirmation dialog so the user can confirm his request
 *  \return void
 */
void GalleryView::ConfirmStopSync()
{
    QString msg = QString("The syncronization of the images with the database "
                          "is still running. Do you want to stop it or let it "
                          "run in the background until its complete?");
    m_confirmPopup = new MythConfirmationDialog(m_popupStack, msg, true);
    if (m_confirmPopup->Create())
    {
        m_confirmPopup->SetReturnEvent(this, "confirmstopsync");
        m_popupStack->AddScreen(m_confirmPopup);
    }
    else
        delete m_confirmPopup;
}



/** \fn     GalleryView::MenuInformation()
 *  \brief  Shows the menu when the INFO key was pressed
 *  \return void
 */
void GalleryView::MenuInformation()
{
    QString label = tr("Image Information");
    MythMenu *menu = new MythMenu(label, this, "infomenu");

    // only show the slideshow options and details menu when
    // the item is a video or image file
    ImageMetadata *im = GetImageMetadataFromSelectedButton();
    if (im)
    {
        if (im->m_type == kImageFile ||
            im->m_type == kVideoFile)
        {
            menu->AddItem(tr("Normal SlideShow"), SLOT(ShowFiles()));
            menu->AddItem(tr("Random Slideshow"), SLOT(ShowRandomFiles()));
        }

        if (im->m_type == kImageFile)
            menu->AddItem(tr("Show Details"), SLOT(FileDetails()));
    }

    m_menuPopup = new MythDialogBox(menu, m_popupStack, "menuPopup");
    if (!m_menuPopup->Create())
    {
        delete m_menuPopup;
        m_menuPopup = NULL;
        return;
    }

    m_popupStack->AddScreen(m_menuPopup);
}



/** \fn     GalleryView::MenuMain()
 *  \brief  Shows the main menu when the MENU button was pressed
 *  \return void
 */
void GalleryView::MenuMain()
{
    // Create the main menu that
    // will contain the submenus above
    MythMenu *menu = new MythMenu(tr("Image Options"), this, "mainmenu");

    // Depending on the status of the sync show either the
    // start sync or stop sync. The user can decide if he
    // wants to stop the sync before leaving the main screen.
    if (!m_syncStatusThread->isSyncRunning())
        menu->AddItem(tr("Start Syncronization"), SLOT(ConfirmStartSync()));
    else
        menu->AddItem(tr("Stop Syncronization"), SLOT(ConfirmStopSync()));

    // Add the available submenus to the main menu. The methods will
    // check if the requirements for showing the menu item is fulfilled
    MenuMetadata(menu);
    MenuSelection(menu);
    MenuFile(menu);

    menu->AddItem(tr("Settings"), SLOT(MenuSettings()));

    m_menuPopup = new MythDialogBox(menu, m_popupStack, "menuPopup");
    if (!m_menuPopup->Create())
    {
        delete m_menuPopup;
        m_menuPopup = NULL;
        return;
    }

    m_popupStack->AddScreen(m_menuPopup);
}



/** \fn     GalleryView::MenuMetadata(MythMenu *)
 *  \brief  Adds a new metadata menu entry into the main menu
 *  \param  mainMenu Parent that will hold the menu entry
 *  \return void
 */
void GalleryView::MenuMetadata(MythMenu *mainMenu)
{
    ImageMetadata *im = GetImageMetadataFromSelectedButton();
    if (im)
    {
        // only show the metadata menu
        // if the current item is an image
        if (im->m_type == kImageFile)
        {
            MythMenu *menu = new MythMenu(tr("Metadata Options"),
                                          this, "metadatamenu");

            menu->AddItem(tr("Rotate CW"), SLOT(FileRotateCW()));
            menu->AddItem(tr("Rotate CCW"), SLOT(FileRotateCCW()));
            menu->AddItem(tr("Flip Horizontal"), SLOT(FileFlipHorizontal()));
            menu->AddItem(tr("Flip Vertical"), SLOT(FileFlipVertical()));
            // menu->AddItem(tr("Zoom In"), SLOT(FileZoomIn()));
            // menu->AddItem(tr("Zoom Out"), SLOT(FileZoomOut()));

            mainMenu->AddItem(tr("Meta Data Menu"), NULL, menu);
        }
    }
}



/** \fn     GalleryView::MenuFile(MythMenu *)
 *  \brief  Adds a new file menu entry into the main menu
 *  \param  mainMenu Parent that will hold the menu entry
 *  \return void
 */
void GalleryView::MenuFile(MythMenu *mainMenu)
{
    ImageMetadata *im = GetImageMetadataFromSelectedButton();
    if (im)
    {
        // There are no options available for up folders
        // They are just there to navigate one level up
        if (im->m_type == kUpDirectory)
            return;

        QString type = "Directory";
        if (im->m_type == kImageFile ||
            im->m_type == kVideoFile)
            type = "File";

        MythMenu *menu = new MythMenu(tr("%1 Options").arg(type),
                                      this, "fileactionmenu");

        // Allow renaming and deletion only for files
        if (im->m_type == kImageFile ||
            im->m_type == kVideoFile)
        {
            menu->AddItem(tr("Delete File"),
                          SLOT(ConfirmFileDelete()));
            menu->AddItem(tr("Delete Selected Files"),
                          SLOT(ConfirmFileDeleteSelected()));
            menu->AddItem(tr("Rename File"),
                          SLOT(FileRenameInput()));
        }

        if (im->m_isHidden)
            menu->AddItem(tr("Unhide %1").arg(type), SLOT(FileUnhide()));
        else
            menu->AddItem(tr("Hide %1").arg(type), SLOT(FileHide()));

        mainMenu->AddItem(tr("File Menu"), NULL, menu);
    }
}



/** \fn     GalleryView::MenuSelection(MythMenu *)
 *  \brief  Adds a new selection menu entry into the main menu
 *  \param  mainMenu Parent that will hold the menu entry
 *  \return void
 */
void GalleryView::MenuSelection(MythMenu *mainMenu)
{
    ImageMetadata *im = GetImageMetadataFromSelectedButton();
    if (im)
    {
        // Selection / deselection is only
        // available for images or videos
        if (im->m_type == kImageFile ||
            im->m_type == kVideoFile)
        {
            MythMenu *menu = new MythMenu(tr("Selection Options"),
                                          this, "selectionmenu");

            if (!im->m_selected)
                menu->AddItem(tr("Select File"),
                              SLOT(FileSelectOne()));
            else
                menu->AddItem(tr("Deselect File"),
                              SLOT(FileDeselectOne()));

            menu->AddItem(tr("Select All Files"), SLOT(FileSelectAll()));
            menu->AddItem(tr("Deselect All Files"), SLOT(FileDeselectAll()));
            menu->AddItem(tr("Invert Selection"), SLOT(FileInvertAll()));

            mainMenu->AddItem(tr("Selection Menu"), NULL, menu);
        }
    }
}



/** \fn     GalleryView::MenuSettings(MythMenu *)
 *  \brief  Adds a new settings menu entry into the main menu
 *  \param  mainMenu Parent that will hold the menu entry
 *  \return void
 */
void GalleryView::MenuSettings()
{
    GalleryConfig *config = new GalleryConfig(m_mainStack, "galleryconfig");
    connect(config, SIGNAL(configSaved()), this, SLOT(LoadData()));

    if (config->Create())
    {
        m_mainStack->AddScreen(config);
    }
    else
        delete config;
}



/** \fn     GalleryView::ItemSelected(MythUIButtonListItem *)
 *  \brief  A new MythUIButtonListItem has been selected
 *  \param  item The given button item that has been selected
 *  \return void
 */
void GalleryView::ItemSelected(MythUIButtonListItem *item)
{
    if (!item)
        return;

    ImageMetadata *dm = GetImageMetadataFromButton(item);
    if (!dm)
        return;

    switch (dm->m_type)
    {
    case kSubDirectory:
        DirSelectDown();
        break;
    case kUpDirectory:
        DirSelectUp();
        break;
    case kImageFile:
        ShowFile();
        break;
    case kVideoFile:
        ShowFile();
        break;
    default:
        break;
    };
}



/** \fn     GalleryView::ShowFiles()
 *  \brief  Starts a slideshow with the images in normal order
 *  \return void
 */
void GalleryView::ShowFiles()
{
    GalleryWidget *widget = ShowFile();

    if (widget)
        widget->StartNormalSlideShow();
}



/** \fn     GalleryView::ShowRandomFiles()
 *  \brief  Starts a slide show with the images in random order
 *  \return void
 */
void GalleryView::ShowRandomFiles()
{
    GalleryWidget *widget = ShowFile();

    if (widget)
        widget->StartRandomSlideShow();
}



/** \fn     GalleryView::ShowFile()
 *  \brief  Creates the window that will show the images and slideshows
 *  \return The created window or NULL
 */
GalleryWidget* GalleryView::ShowFile()
{
    GalleryWidget *widget = new GalleryWidget(m_mainStack, "gallerywidget", m_galleryViewHelper);
    if (widget->Create())
    {
        ResetThumbnailProgress();
        m_mainStack->AddScreen(widget);
        widget->LoadFile();
    }
    else
    {
        delete widget;
        widget = NULL;
    }

    return widget;
}



/** \fn     GalleryView::DirSelectUp()
 *  \brief  Goes up one directory level
 *  \return void
 */
bool GalleryView::DirSelectUp()
{
    // Set the first node (upfolder) active
    m_galleryViewHelper->m_currentNode->setSelectedChild(m_galleryViewHelper->m_currentNode->getChildAt(0));

    // Get the data and with it the kUpFolder directory node
    int id = m_galleryViewHelper->GetImageMetadataFromSelectedNode()->m_id;

    m_galleryViewHelper->LoadTreeData();
    ResetThumbnailProgress();
    UpdateImageList();

    // Go through the entire list of image items and find
    // the directory id that matches the saved directory id
    for (int i = 0; i < m_imageList->GetCount(); i++)
    {
        MythUIButtonListItem *item = m_imageList->GetItemAt(i);
        if (!item)
            continue;

        ImageMetadata *data = GetImageMetadataFromButton(item);
        if (!data)
            continue;

        if (data->m_id == id)
        {
            m_imageList->SetItemCurrent(item);
            break;
        }
    }

    return true;
}



/** \fn     GalleryView::DirSelectDown()
 *  \brief  Goes one directory level down
 *  \return void
 */
void GalleryView::DirSelectDown()
{
    m_galleryViewHelper->LoadTreeData();
    m_galleryViewHelper->m_currentNode->setSelectedChild(m_galleryViewHelper->m_currentNode->getChildAt(0));

    ResetThumbnailProgress();
    UpdateImageList();
}



/** \fn     GalleryView::FileSelectOne()
 *  \brief  Marks a single file as selected
 *  \return void
 */
void GalleryView::FileSelectOne()
{
    MythUIButtonListItem *item = m_imageList->GetItemCurrent();
    if (!item)
        return;

    m_galleryViewHelper->SetNodeSelectionState(kNodeStateSelect, false);
    UpdateImageItem(item);
}



/** \fn     GalleryView::FileDeselectOne()
 *  \brief  Marks a single file as not selected
 *  \return void
 */
void GalleryView::FileDeselectOne()
{
    MythUIButtonListItem *item = m_imageList->GetItemCurrent();
    if (!item)
        return;

    m_galleryViewHelper->SetNodeSelectionState(kNodeStateDeselect, false);
    UpdateImageItem(item);
}



/** \fn     GalleryView::FileSelectAll()
 *  \brief  Marks all files as selected
 *  \return void
 */
void GalleryView::FileSelectAll()
{
    m_galleryViewHelper->SetNodeSelectionState(kNodeStateSelect, true);
    UpdateImageList();
}



/** \fn     GalleryView::FileDeselectAll()
 *  \brief  Marks all files as not selected
 *  \return void
 */
void GalleryView::FileDeselectAll()
{
    m_galleryViewHelper->SetNodeSelectionState(kNodeStateDeselect, true);
    UpdateImageList();
}



/** \fn     GalleryView::FileInvertAll()
 *  \brief  Inverts the current selection for all files
 *  \return void
 */
void GalleryView::FileInvertAll()
{
    m_galleryViewHelper->SetNodeSelectionState(kNodeStateInvert, true);
    UpdateImageList();
}



/** \fn     GalleryView::FileRotateCW()
 *  \brief  Rotates the selected file 90 clockwise.
 *          The thumbnail will also be updated.
 *  \return void
 */
void GalleryView::FileRotateCW()
{
    MythUIButtonListItem *item = m_imageList->GetItemCurrent();
    if (!item)
        return;

    m_galleryViewHelper->SetFileOrientation(kFileRotateCW);
    UpdateImageItem(item);
}



/** \fn     GalleryView::FileRotateCCW()
 *  \brief  Rotates the selected file 90 counter clockwise.
 *          The thumbnail will also be updated.
 *  \return void
 */
void GalleryView::FileRotateCCW()
{
    MythUIButtonListItem *item = m_imageList->GetItemCurrent();
    if (!item)
        return;

    m_galleryViewHelper->SetFileOrientation(kFileRotateCCW);
    UpdateImageItem(item);
}



/** \fn     GalleryView::FileFlipHorizontal()
 *  \brief  Flips the file horizontally.
 *          The thumbnail will also be updated.
 *  \return void
 */
void GalleryView::FileFlipHorizontal()
{
    MythUIButtonListItem *item = m_imageList->GetItemCurrent();
    if (!item)
        return;

    m_galleryViewHelper->SetFileOrientation(kFileFlipHorizontal);
    UpdateImageItem(item);
}



/** \fn     GalleryView::FileFlipVertical()
 *  \brief  Flips the file vertically.
 *          The thumbnail will also be updated.
 *  \return void
 */
void GalleryView::FileFlipVertical()
{
    MythUIButtonListItem *item = m_imageList->GetItemCurrent();
    if (!item)
        return;

    m_galleryViewHelper->SetFileOrientation(kFileFlipVertical);
    UpdateImageItem(item);
}



/** \fn     GalleryView::FileZoomIn()
 *  \brief  Zooms into the file. The thumbnail will also be updated.
 *  \return void
 */
void GalleryView::FileZoomIn()
{
    MythUIButtonListItem *item = m_imageList->GetItemCurrent();
    if (!item)
        return;

    m_galleryViewHelper->SetFileZoom(kFileZoomIn);
    UpdateImageItem(item);
}



/** \fn     GalleryView::FileZoomOut()
 *  \brief  Zooms out of the file. The thumbnail will also be updated.
 *  \return void
 */
void GalleryView::FileZoomOut()
{
    MythUIButtonListItem *item = m_imageList->GetItemCurrent();
    if (!item)
        return;

    m_galleryViewHelper->SetFileZoom(kFileZoomOut);
    UpdateImageItem(item);
}



/** \fn     GalleryView::FileHide()
 *  \brief  Marks the file as hidden and hides it from the visible image list
 *  \return void
 */
void GalleryView::FileHide()
{
    m_galleryViewHelper->SetNodeVisibilityState(kNodeStateInvisible);
    UpdateImageList();
}



/** \fn     GalleryView::FileHide()
 *  \brief  Marks the file as visible and unhides it from the visible image list
 *  \return void
 */
void GalleryView::FileUnhide()
{
    m_galleryViewHelper->SetNodeVisibilityState(kNodeStateVisible);
    UpdateImageList();
}



/** \fn     GalleryView::ConfirmFileDelete()
 *  \brief  Shows a confirmation dialog before
 *          the user can delete a single file
 *  \return void
 */
void GalleryView::ConfirmFileDelete()
{
    // Create a confirmation dialog to confirm the file deletion
    // of the currently selected image. This is only a safety precausion.
    ImageMetadata *data = GetImageMetadataFromSelectedButton();
    if (!data)
        return;

    QString msg = QString("Do you want to delete '\n%1'").arg(data->m_name);
    m_confirmPopup = new MythConfirmationDialog(m_popupStack, msg, true);

    if (m_confirmPopup->Create())
    {
        m_confirmPopup->SetReturnEvent(this, "confirmdelete");
        m_popupStack->AddScreen(m_confirmPopup);
    }
    else
        delete m_confirmPopup;
}



/** \fn     GalleryView::ConfirmFileDeleteSelected()
 *  \brief  Shows a confirmation dialog before the
 *          user can delete all selected files
 *  \return void
 */
void GalleryView::ConfirmFileDeleteSelected()
{
    QString msg = QString("Do you want to delete all selected files");
    m_confirmPopup = new MythConfirmationDialog(m_popupStack, msg, true);

    if (m_confirmPopup->Create())
    {
        m_confirmPopup->SetReturnEvent(this, "confirmdeleteselected");
        m_popupStack->AddScreen(m_confirmPopup);
    }
    else
        delete m_confirmPopup;
}



/** \fn     GalleryView::FileDelete()
 *  \brief  Deletes a single file
 *  \return void
 */
void GalleryView::FileDelete()
{
    m_galleryViewHelper->DeleteCurrentNode();
    UpdateImageList();
}



/** \fn     GalleryView::FileDeleteSelected()
 *  \brief  Deletes all selected files
 *  \return void
 */
void GalleryView::FileDeleteSelected()
{
    m_galleryViewHelper->DeleteSelectedNodes();
    UpdateImageList();
}



/** \fn     GalleryView::FileRenameInput()
 *  \brief  Shows a popup where the user can enter a new filename
 *  \return void
 */
void GalleryView::FileRenameInput()
{
    ImageMetadata *im = GetImageMetadataFromSelectedButton();
    if (im)
    {
        QString msg = QString("Enter a new name for '%1'.").arg(im->m_name);
        m_inputPopup = new MythTextInputDialog(m_popupStack, msg,
                                               FilterNone, false, im->m_name );

        if (m_inputPopup->Create())
        {
            m_inputPopup->SetReturnEvent(this, "filerename");
            m_popupStack->AddScreen(m_inputPopup);
        }
        else
            delete m_inputPopup;
    }
}



/** \fn     GalleryView::FileRename(QString &)
 *  \brief  Renames the current filename to the new filename
            if the new filename does not exist already.
 *  \param  New name of the file with the full path
 *  \return void
 */
void GalleryView::FileRename(QString &newName)
{
    m_galleryViewHelper->RenameCurrentNode(newName);
    UpdateImageList();
}



/** \fn     GalleryView::FileDetails()
 *  \brief  Shows details about the single file
 *  \return void
 */
void GalleryView::FileDetails()
{
    GalleryWidget *widget = ShowFile();

    if (widget)
        widget->ShowFileDetails();
}



/** \fn     GalleryView::GetImageMetadataFromSelectedButton()
 *  \brief  Returns the data of the currently selected image list item
 *  \return ImageMetadata
 */
ImageMetadata *GalleryView::GetImageMetadataFromSelectedButton()
{
    MythUIButtonListItem *item = m_imageList->GetItemCurrent();
    if (!item)
        return NULL;

    MythGenericTree *node = item->GetData().value<MythGenericTree *>();
    if (!node)
        return NULL;

    ImageMetadata *data = node->GetData().value<ImageMetadata *>();
    if (!data)
        return NULL;

    return data;
}



/** \fn     GalleryView::GetImageMetadataFromButton(MythUIButtonListItem *)
 *  \brief  Returns the data of the given image list item
 *  \param  item The given image list item
 *  \return ImageMetadata
 */
ImageMetadata *GalleryView::GetImageMetadataFromButton(MythUIButtonListItem *item)
{
    if (!item)
        return NULL;

    MythGenericTree *node = item->GetData().value<MythGenericTree *>();
    if (!node)
        return NULL;

    ImageMetadata *data = node->GetData().value<ImageMetadata *>();
    if (!data)
        return NULL;

    return data;
}
