#include "gallerythumbview.h"

#include <QFileInfo>

#include <mythcontext.h>
#include <mythscreentype.h>
#include <mythuimultifilebrowser.h>
#include <mythuiprogressbar.h>
#include <imageutils.h>

#include "galleryconfig.h"

/*!
 *  \brief  Constructor
 *  \param  parent The screen parent
 *  \param  name The name of the screen
 */
GalleryThumbView::GalleryThumbView(MythScreenStack *parent, const char *name)
    : MythScreenType(parent, name),
    m_imageList(NULL),
    m_captionText(NULL),
    m_crumbsText(NULL),
    m_hideFilterText(NULL),
    m_typeFilterText(NULL),
    m_positionText(NULL),
    m_emptyText(NULL),
    m_scanProgressText(NULL),
    m_scanProgressBar(NULL),
    m_scanInProgress(false),
    m_zoomLevel(0),
    m_infoList(NULL),
    m_editsAllowed(false),
    m_deleteAfterImport(false),
    m_importTmp(NULL)
{
    // Handles BE messages for all views
    gCoreContext->addListener(this);

    m_popupStack = GetMythMainWindow()->GetStack("popup stack");
    m_mainStack  = GetMythMainWindow()->GetMainStack();

    // Filter db reads using current settings
    m_db = new ImageDbReader(gCoreContext->GetNumSetting("GallerySortOrder"),
                             gCoreContext->GetNumSetting("GalleryShowHidden"),
                             gCoreContext->GetNumSetting("GalleryShowType"));

    // This screen uses a single fixed view (Parent dir, ordered dirs, ordered images)
    m_view = new DirectoryView(kOrdered, m_db);

    m_menuState.Clear();
}


/*!
 *  \brief  Destructor
 */
GalleryThumbView::~GalleryThumbView()
{
    gCoreContext->removeListener(this);

    delete m_view;
    delete m_db;
    delete m_infoList;
    delete m_importTmp;
}


/*!
 *  \brief  Initialises and shows the graphical elements
 */
bool GalleryThumbView::Create()
{
    if (!LoadWindowFromXML("image-ui.xml", "gallery", this))
        return false;

    // Determine zoom levels supported by theme
    // images0 must exist; images1, images2 etc. are optional and enable zoom
    int               zoom = 0;
    MythUIButtonList *widget;
    do
    {
        QString name = QString("images%1").arg(zoom++);
        widget = dynamic_cast<MythUIButtonList *>(this->GetChild(name));
        if (widget)
        {
            m_zoomWidgets.append(widget);
            widget->SetVisible(false);
        }
    }
    while (widget);

    if (m_zoomWidgets.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "Thumbview: Screen 'Gallery' is missing 'images0'");
        return false;
    }
    LOG(VB_GUI, LOG_DEBUG, QString("Thumbview: Screen 'Gallery' found %1 zoom levels")
        .arg(m_zoomWidgets.size()));

    // File details list is managed elsewhere
    m_infoList = InfoList::Create(this, false, tr("Not defined"));
    if (!m_infoList)
    {
        LOG(VB_GENERAL, LOG_ERR, "Slideview: Cannot load screen 'Gallery'");
        return false;
    }

    UIUtilW::Assign(this, m_captionText,      "caption");
    UIUtilW::Assign(this, m_emptyText,        "noimages");
    UIUtilW::Assign(this, m_positionText,     "position");
    UIUtilW::Assign(this, m_crumbsText,       "breadcrumbs");
    UIUtilW::Assign(this, m_hideFilterText,   "hidefilter");
    UIUtilW::Assign(this, m_typeFilterText,   "typefilter");
    UIUtilW::Assign(this, m_scanProgressText, "scanprogresstext");
    UIUtilW::Assign(this, m_scanProgressBar,  "scanprogressbar");

    if (m_scanProgressText)
        m_scanProgressText->SetVisible(false);
    if (m_scanProgressBar)
        m_scanProgressBar->SetVisible(false);

    BuildFocusList();

    // Initialise list widget with appropriate zoom level for this theme.
    m_zoomLevel = gCoreContext->GetNumSetting("GalleryZoomLevel", 0);
    SelectZoomWidget(0);

    // Start in edit mode unless a password exists
    m_editsAllowed = gCoreContext->GetSetting("GalleryPassword").isEmpty();

    return true;
}


/*!
 *  \brief  Handle keypresses
 *  \param  event The pressed key
 */
bool GalleryThumbView::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Images", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "MENU")
            MenuMain();
        else if (action == "INFO")
            ShowDetails();
        else if (action == "ZOOMIN")
            ZoomIn();
        else if (action == "ZOOMOUT")
            ZoomOut();
        else if (action == "ROTRIGHT")
            RotateCW();
        else if (action == "ROTLEFT")
            RotateCCW();
        else if (action == "FLIPHORIZONTAL")
            FlipHorizontal();
        else if (action == "FLIPVERTICAL")
            FlipVertical();
        else if (action == "SLIDESHOW")
            Slideshow();
        else if (action == "RECURSIVESHOW")
            RecursiveSlideshow();
        else if (action == "MARK")
        {
            ImageItem *im = m_view->GetSelected();
            if (im && m_editsAllowed)
                MarkItem(!m_view->IsMarked(im->m_id));
        }
        else if (action == "ESCAPE" && !GetMythMainWindow()->IsExitingToMain())
        {
            // Exit info list, if shown
            handled = m_infoList->Hide();

            // Ascend the tree if first node is an kUpDirectory
            if (!handled)
            {
                ImageItem *data = m_view->GetParent();
                if (data && data->m_type == kUpDirectory)
                    handled = DirSelectUp();
            }
        }
        else
            handled = false;
    }

    if (!handled)
        handled = MythScreenType::keyPressEvent(event);

    return handled;
}


/*!
 *  \brief  Handle custom events
 *  \param  event The custom event
 */
void GalleryThumbView::customEvent(QEvent *event)
{
    if ((MythEvent::Type)(event->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me      = (MythEvent *)event;
        QString    message = me->Message();

        QStringList extra = me->ExtraDataList();

        if (message == "THUMB_AVAILABLE")
        {
            int id = extra[0].toInt();

            // Get all buttons waiting for this thumbnail
            QList<ThumbLocation> affected = m_pendingMap.values(id);

            // Thumbview is only concerned with thumbnails we've requested
            if (!affected.isEmpty())
            {
                LOG(VB_GENERAL, LOG_DEBUG,
                    QString("Thumbview: Rx %1 : %2").arg(message, extra.join(",")));

                QString url = "";

                // Set thumbnail for each button now it exists
                foreach(const ThumbLocation &location, affected)
                {
                    MythUIButtonListItem *button = location.first;
                    int index  = location.second;
                    ImageItem *im = button->GetData().value<ImageItem*>();

                    if (im)
                    {
                        // All buttons use the same url so only generate it once
                        if (url.isEmpty())
                            url = ImageSg::getInstance()->GenerateThumbUrl(im->m_thumbNails.at(index));

                        UpdateThumbnail(button, im, url, index);
                    }
                }

                // Cancel pending request & store url
                m_pendingMap.remove(id);
                m_url.insert(id, url);
            }

            // Pass on to Slideshow
            emit ThumbnailChanged(id);
        }
        else if (message == "IMAGE_DB_CHANGED")
        {
            // Expects either:
            // "ALL" when db cleared, or
            // csv list of deleted ids, csv list of changed ids,
            // csv list of urls to remove from cache
            LOG(VB_GENERAL, LOG_DEBUG,
                QString("Thumbview: Rx %1 : %2").arg(message, extra.join(",")));

            QStringList idDeleted, idChanged;

            if (!extra.isEmpty())
                idDeleted = extra.takeFirst().split(",",  QString::SkipEmptyParts);
            if (!extra.isEmpty())
                idChanged = extra.takeFirst().split(",",  QString::SkipEmptyParts);

            if (!idDeleted.isEmpty() && idDeleted[0] == "ALL")
            {
                m_view->Clear();
                m_url.clear();

                LOG(VB_FILE, LOG_DEBUG, "Thumbview: Clearing image cache");

                // Remove all cached thumbnails
                GetMythUI()->RemoveFromCacheByFile(QString("/%1/").arg(THUMBNAIL_DIR));
                // Remove all cached images
                GetMythUI()->RemoveFromCacheByFile(QString("//%1@").arg(IMAGE_STORAGE_GROUP));
            }
            else
            {
                LOG(VB_FILE, LOG_DEBUG, QString("Thumbview: Removing %1- Updating %2")
                    .arg(idDeleted.join(","), idChanged.join(",")));

                // Reset image urls so that new thumbnails are requested
                foreach (const QString &ident, idChanged)
                    m_url.remove(ident.toInt());

                // Cleanup image urls & marked files
                foreach (const QString &ident, idDeleted)
                {
                    m_url.remove(ident.toInt());
                    m_view->Mark(ident.toInt(), false);
                }

                // Remove cached thumbs & images
                foreach(const QString &url, extra)
                    GetMythUI()->RemoveFromCacheByFile(url);
            }

            // Refresh display
            LoadData();
        }
        else if (message == "IMAGE_SCAN_STATUS")
        {
            // Expects mode, scanned#, total#
            if (extra.size() == 3)
            {
                UpdateScanProgress(extra[0], extra[1], extra[2]);
            }
        }
    }
    else if (event->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = (DialogCompletionEvent *)(event);

        QString resultid  = dce->GetId();
        int     buttonnum = dce->GetResult();

        if (resultid == "FileRename")
        {
            QString newName = dce->GetResultText();
            if (m_menuState.m_selected)
            {
                QString err = GalleryBERequest::RenameFile(
                    m_menuState.m_selected->m_id, newName);

                if (!err.isEmpty())
                    ShowOkPopup(err);
            }
        }
        else if (resultid == "ImportFilesSelected")
        {
            QStringList files = dce->GetData().value<QStringList>();
            LOG(VB_FILE, LOG_DEBUG, "Thumbview: Import files " + files.join(","));
            ImportFiles(files);
        }
        else if (resultid == "MakeDir")
        {
            if (!m_menuState.m_selected)
                return;

            QStringList newPath;
            newPath << QString("%1/%2")
                .arg(m_menuState.m_selected->m_fileName,
                     dce->GetResultText());

            QString err = GalleryBERequest::MakeDirs(newPath);
            if (err.isEmpty())
                // Rescan to display new dir
                StartScan();
            else
                ShowOkPopup(err);
        }
        else if (resultid == "SlideOrderMenu")
        {
            SlideOrderType slideOrder = kOrdered;

            switch (buttonnum)
            {
                case 0: slideOrder = kOrdered; break;
                case 1: slideOrder = kShuffle; break;
                case 2: slideOrder = kRandom; break;
                case 3: slideOrder = kSeasonal; break;
            }
            gCoreContext->SaveSetting("GallerySlideOrder", slideOrder);
            LOG(VB_FILE, LOG_DEBUG, QString("Order %1").arg(slideOrder));
        }
        else if (resultid == "Password")
        {
            QString password = dce->GetResultText();
            m_editsAllowed = (password == gCoreContext->GetSetting("GalleryPassword"));
        }
        else if (buttonnum == 1)
        {
            // Confirm current file deletion
            if (resultid == "ConfirmDelete" && m_menuState.m_selected)
            {
                ImageIdList ids = ImageIdList() << m_menuState.m_selected->m_id;
                QString     err = GalleryBERequest::RemoveFiles(ids);
                if (!err.isEmpty())
                    ShowOkPopup(err);
            }
            // Confirm all selected file deletion
            else if (resultid == "ConfirmDeleteMarked")
            {
                QString err = GalleryBERequest::RemoveFiles(m_menuState.m_markedId);
                if (!err.isEmpty())
                    ShowOkPopup(err);
            }
        }
    }
}


/**
 * @brief Start Thumbnail screen
 */
void GalleryThumbView::Start()
{
    // Detect any current scans
    QStringList message = GalleryBERequest::ScanQuery();
    UpdateScanProgress(message[1], message[2], message[3]);

    // Always start showing top level images
    LoadData(ROOT_DB_ID);
}


/*!
 \brief Loads & displays images from database
 \param parent Id of parent dir
*/
void GalleryThumbView::LoadData(int parent)
{
    ResetUiSelection();

    // Load view for parent directory
    if (m_view->LoadFromDb(parent))
    {
        m_imageList->SetVisible(true);
        if (m_emptyText)
            m_emptyText->SetVisible(false);

        // Build the buttonlist
        BuildImageList();
    }
    else
    {
        m_infoList->Hide();
        m_imageList->SetVisible(false);
        if (m_emptyText)
        {
            m_emptyText->SetVisible(true);
            m_emptyText->SetText(tr("No images found in the database.\n"
                                    "Set Photographs storage group,\n"
                                    "then scan from menu."));
        }
    }
}


/*!
 *  \brief  Displays all images in current view
 */
void GalleryThumbView::BuildImageList()
{
    m_imageList->Reset();
    m_pendingMap.clear();

    // get all children from the the active node
    ImageList      children = m_view->GetAllNodes();
    ImageItem *selected = m_view->GetSelected();

    // go through the entire list and update
    foreach(ImageItem *im, children)
    if (im)
    {
        // Data must be set by constructor: First item is automatically
        // selected and must have data available for selection event, as
        // subsequent reselection of same item will always fail.
        MythUIButtonListItem *item = new MythUIButtonListItem(
            m_imageList, "", qVariantFromValue(im));

        item->setCheckable(true);
        item->setChecked(MythUIButtonListItem::NotChecked);

        // assign and display all information about
        // the current item, like title and subdirectory count
        UpdateImageItem(item);

        // Reinstate the active button item
        if (im == selected)
            m_imageList->SetItemCurrent(item);
    }

    // Updates all other widgets on the screen that show
    // information about the selected MythUIButtonListItem
    SetUiSelection(m_imageList->GetItemCurrent());
}


/*!
 *  \brief  Initialises a single buttonlist item
 *  \param  item The buttonlist item

 */
void GalleryThumbView::UpdateImageItem(MythUIButtonListItem *item)
{
    ImageItem *im = item->GetData().value<ImageItem *>();
    if (!im)
        return;

    // Allow themes to distinguish between roots, folders, pics, videos
    switch (im->m_type)
    {
        case kUpDirectory:
            item->DisplayState("upfolder", "buttontype");
            break;

        case kBaseDirectory:
        case kSubDirectory:
            if (im->m_dirCount > 0)
                item->SetText(QString("%1/%2")
                              .arg(im->m_fileCount).arg(im->m_dirCount),
                              "childcount");
            else
                item->SetText(QString::number(im->m_fileCount), "childcount");

            item->DisplayState("subfolder", "buttontype");
            break;

        case kImageFile:
            item->DisplayState("image", "buttontype");
            break;

        case kVideoFile:
            item->DisplayState("video", "buttontype");
            break;

        default:
            break;
    }

    // Allow theme to distinguish visible/hidden nodes
    QString hideState = (im->m_isHidden) ? "hidden" : "visible";
    item->DisplayState(hideState, "buttonstate");

    // No text for roots or Upfolders
    if (im->m_type >= kSubDirectory)
    {
        int     show = gCoreContext->GetNumSetting("GalleryShowCaption");
        QString text;
        switch (show)
        {
            case kNameCaption: text = im->m_name; break;
            case kDateCaption: text = ImageUtils::ImageDateOf(im); break;
            case kUserCaption: text = im->m_comment; break;
            default:
            case kNoCaption:   text = ""; break;
        }
        item->SetText(text);
    }

    // Set marked state
    MythUIButtonListItem::CheckState state
        = m_view->IsMarked(im->m_id)
          ? MythUIButtonListItem::FullChecked
          : MythUIButtonListItem::NotChecked;

    item->setChecked(state);

    // Thumbnails required from BE
    QStringList required = QStringList();

    if (im->m_thumbNails.size() == 1)
    {
        // Single thumbnail
        QString url = CheckThumbnail(item, im, required);

        if (!url.isEmpty())
            UpdateThumbnail(item, im, url);
    }
    else
    {
        // Dir showing up to 4 thumbs
        InfoMap thumbMap;
        for (int index = 0; index < im->m_thumbNails.size(); ++index)
        {
            QString url = CheckThumbnail(item, im, required, index);

            if (!url.isEmpty())
                thumbMap.insert(QString("thumbimage%1").arg(index), url);
        }

        // Set multiple images at same time
        if (!thumbMap.isEmpty())
            item->SetImageFromMap(thumbMap);
    }

    // Request BE to create/verify any missing thumbnails.
    if (!required.isEmpty())
        GalleryBERequest::CreateThumbnails(required, im->IsDirectory());
}


/*!
 \brief Verify thumbnail is known to exist
 \details Thumbnails are only displayed when known to exist on the BE. Otherwise
 repeated failures to load them severely hinders performance. Note a single
 specific BE request is much faster than scanning the image cache
 \param item The buttonlist item being constructed
 \param im Image data
 \param request List of ids that are unknown
 \param index Thumbnail index in buttonlist item (Dirs use 4 thumbnails)
 \return QString URL of thumbnail
*/
QString GalleryThumbView::CheckThumbnail(MythUIButtonListItem *item, ImageItem *im,
                                         QStringList &request, int index)
{
    int id = im->m_thumbIds.at(index);

    // Thumbs are retrieved & cached as urls
    QString url = m_url.value(id, "");

    if (!url.isEmpty())
        return url;

    // Request BE thumbnail check if it is not already pending
    if (!m_pendingMap.contains(id))
        request << QString::number(id);

    // Note this button is awaiting an update
    m_pendingMap.insertMulti(id, qMakePair(item, index));

    return "";
}


/*!
 \brief Update the buttonlist item with a thumbnail
 \param button Buttonlist item to update
 \param im Image data
 \param url URL of the thumbnail
 \param index Index of the thumbnail on the button
*/
void GalleryThumbView::UpdateThumbnail(MythUIButtonListItem *button,
                                       ImageItem *im, const QString &url,
                                       int index)
{
    if (im->m_thumbNails.size() == 1)
    {
        // Pics, dirs & videos use separate widgets
        QString widget;
        switch (im->m_type)
        {
            case kImageFile: widget = ""; break;
            case kVideoFile: widget = "videoimage"; break;
            default:         widget = "folderimage"; break;
        }

        button->SetImage(url, widget);
    }
    else
        // Dir with 4 thumbnails
        button->SetImage(url, QString("thumbimage%1").arg(index));
}


/*!
 \brief Update progressbar with scan status
 \param mode BE scan state
 \param current Number of images scanned
 \param total Total number of images to scan
*/
void GalleryThumbView::UpdateScanProgress(QString mode,
                                          QString current,
                                          QString total)
{
    if (!mode.isEmpty() && !m_scanInProgress)
    {
        // Scan just started
        if (m_scanProgressBar)
        {
            m_scanProgressBar->SetVisible(true);
            m_scanProgressBar->SetStart(0);
        }
        if (m_scanProgressText)
            m_scanProgressText->SetVisible(true);
    }

    m_scanInProgress = !mode.isEmpty();

    if (m_scanInProgress)
    {
        // Scan update
        if (m_scanProgressBar)
        {
            m_scanProgressBar->SetUsed(current.toInt());
            m_scanProgressBar->SetTotal(total.toInt());
        }
        if (m_scanProgressText)
            m_scanProgressText->SetText(tr("%1 of %3")
                                        .arg(current, total));
    }
    else
    {
        // Scan just finished
        if (m_scanProgressText)
            m_scanProgressText->SetVisible(false);
        if (m_scanProgressBar)
            m_scanProgressBar->SetVisible(false);
    }
}


/*!
 *  \brief  Clears all text widgets for selected item
 */
void GalleryThumbView::ResetUiSelection()
{
    if (m_positionText)
        m_positionText->Reset();

    if (m_captionText)
        m_captionText->Reset();

    if (m_crumbsText)
        m_crumbsText->Reset();

    if (m_hideFilterText)
        m_hideFilterText->Reset();

    if (m_typeFilterText)
        m_typeFilterText->Reset();
}


/*!
 *  \brief  Updates text widgets for selected item
 *  \param  item The selected buttonlist item
 */
void GalleryThumbView::SetUiSelection(MythUIButtonListItem *item)
{
    ImageItem *im = item->GetData().value<ImageItem *>();
    if (im)
    {
        if (m_captionText)
        {
            // show the date & comment of a node
            QStringList text;
            text << ImageUtils::ImageDateOf(im);

            if (!im->m_comment.isEmpty())
                text << im->m_comment;

            m_captionText->SetText(text.join(" - "));
        }

        if (m_hideFilterText)
        {
            int show = gCoreContext->GetNumSetting("GalleryShowHidden");
            m_hideFilterText->SetText(show ? tr("Hidden") : "");
        }

        if (m_typeFilterText)
        {
            int type = gCoreContext->GetNumSetting("GalleryShowType");
            QString text = "";
            switch (type)
            {
            case kPicAndVideo : text = ""; break;
            case kPicOnly     : text = tr("Pictures"); break;
            case kVideoOnly   : text = tr("Videos"); break;
            }

            m_typeFilterText->SetText(text);
        }

        // show the position of the image
        if (m_positionText)
            m_positionText->SetText(QString("%1/%2")
                                    .arg(m_imageList->GetCurrentPos())
                                    .arg(m_imageList->GetCount() - 1));

        // show the path of the image
        if (m_crumbsText)
        {
            if (im->m_id == ROOT_DB_ID)
                m_crumbsText->SetText(tr("Gallery"));
            else
            {
                QString crumbs = im->m_fileName;
                m_crumbsText->SetText(crumbs.replace("/", " - "));
            }
        }

        // Update any file details information
        m_infoList->Update(im);
    }
}


/*!
 *  \brief  Shows the main menu when the MENU button was pressed
 */
void GalleryThumbView::MenuMain()
{
    // Create the main menu
    MythMenu *menu = new MythMenu(tr("Gallery Options"), this, "mainmenu");

    // Menu options depend on the marked files and the current node
    m_menuState = m_view->GetMenuSubjects();

    if (m_menuState.m_selected)
    {
        if (m_editsAllowed)
        {
            MenuMarked(menu);
            MenuPaste(menu);
            MenuTransform(menu);
            MenuAction(menu);
        }
        MenuSlideshow(menu);
        MenuShow(menu);
        if (!m_editsAllowed)
            menu->AddItem(tr("Enable Edits"), SLOT(ShowPassword()));
    }

    // Scan control depends on current status
    if (m_scanInProgress)
        menu->AddItem(tr("Stop Scan"), SLOT(StopScan()));
    else
        menu->AddItem(tr("Start Scan"), SLOT(StartScan()));

    menu->AddItem(tr("Settings"), SLOT(ShowSettings()));

    MythDialogBox *popup = new MythDialogBox(menu, m_popupStack, "menuPopup");
    if (popup->Create())
        m_popupStack->AddScreen(popup);
    else
        delete popup;
}


/*!
 *  \brief  Adds a Marking submenu
 *  \param  mainMenu Parent menu
 */
void GalleryThumbView::MenuMarked(MythMenu *mainMenu)
{
    if (m_menuState.m_childCount == 0)
        return;

    QString title = tr("%1 marked").arg(m_menuState.m_markedId.size());

    MythMenu *menu = new MythMenu(title, this, "markmenu");

    // Mark/unmark selected
    if (m_menuState.m_selected->IsFile())
    {
        if (m_menuState.m_selectedMarked)
            menu->AddItem(tr("Unmark File"), SLOT(UnmarkItem()));
        else
            menu->AddItem(tr("Mark File"), SLOT(MarkItem()));
    }
    // Cannot mark/unmark parent dir from this level
    else if (m_menuState.m_selected->m_type == kSubDirectory)
    {
        if (m_menuState.m_selectedMarked)
            menu->AddItem(tr("Unmark Directory"), SLOT(UnmarkItem()));
        else
            menu->AddItem(tr("Mark Directory"), SLOT(MarkItem()));
    }

    // Mark All if unmarked files exist
    if (m_menuState.m_markedId.size() < m_menuState.m_childCount)
        menu->AddItem(tr("Mark All"), SLOT(MarkAll()));

    // Unmark All if marked files exist
    if (!m_menuState.m_markedId.isEmpty())
    {
        menu->AddItem(tr("Unmark All"), SLOT(UnmarkAll()));
        menu->AddItem(tr("Invert Marked"), SLOT(MarkInvertAll()));
    }

    mainMenu->AddItem(tr("Mark"), NULL, menu);
}


/*!
 \brief Add a Paste submenu
 \param mainMenu Parent menu
*/
void GalleryThumbView::MenuPaste(MythMenu *mainMenu)
{
    // Can only copy/move into dirs
    if (m_menuState.m_selected->IsDirectory())
    {
        // Operate on current marked files, if any
        ImageIdList files = m_menuState.m_markedId;
        if (files.isEmpty())
            files = m_menuState.m_prevMarkedId;
        if (files.isEmpty())
            return;

        QString title = tr("%1 marked").arg(files.size());

        MythMenu *menu = new MythMenu(title, this, "pastemenu");

        menu->AddItem(tr("Move Marked Into"), SLOT(Move()));
        menu->AddItem(tr("Copy Marked Into"), SLOT(Copy()));

        mainMenu->AddItem(tr("Paste"), NULL, menu);
    }
}


/*!
 \brief Add a Transform submenu
 \param mainMenu Parent menu
*/
void GalleryThumbView::MenuTransform(MythMenu *mainMenu)
{
    // Operate on marked files, if any, otherwise selected node
    if (!m_menuState.m_markedId.isEmpty())
    {
        QString title = tr("%1 marked").arg(m_menuState.m_markedId.size());

        MythMenu *menu = new MythMenu(title, this, "");

        menu->AddItem(tr("Rotate Marked CW"), SLOT(RotateCWMarked()));
        menu->AddItem(tr("Rotate Marked CCW"), SLOT(RotateCCWMarked()));
        menu->AddItem(tr("Flip Marked Horizontal"), SLOT(FlipHorizontalMarked()));
        menu->AddItem(tr("Flip Marked Vertical"), SLOT(FlipVerticalMarked()));
        menu->AddItem(tr("Reset Marked to Exif"), SLOT(ResetExifMarked()));

        mainMenu->AddItem(tr("Transforms"), NULL, menu);
    }
    else if (m_menuState.m_selected->IsFile())
    {
        MythMenu *menu = new MythMenu(m_menuState.m_selected->m_name, this, "");

        menu->AddItem(tr("Rotate CW"), SLOT(RotateCW()));
        menu->AddItem(tr("Rotate CCW"), SLOT(RotateCCW()));
        menu->AddItem(tr("Flip Horizontal"), SLOT(FlipHorizontal()));
        menu->AddItem(tr("Flip Vertical"), SLOT(FlipVertical()));
        menu->AddItem(tr("Reset to Exif"), SLOT(ResetExif()));

        mainMenu->AddItem(tr("Transforms"), NULL, menu);
    }
}


/*!
 \brief Add a Action submenu
 \param mainMenu Parent menu
*/
void GalleryThumbView::MenuAction(MythMenu *mainMenu)
{
    MythMenu *menu;

    // Operate on current marked files, if any
    if (m_menuState.m_markedId.size() > 0)
    {
        QString title = tr("%1 marked").arg(m_menuState.m_markedId.size());

        menu = new MythMenu(title, this, "actionmenu");

        // Only offer Hide/Unhide if relevant
        if (m_menuState.m_unhiddenMarked)
            menu->AddItem(tr("Hide Marked"), SLOT(HideMarked()));
        if (m_menuState.m_hiddenMarked)
            menu->AddItem(tr("Unhide Marked"), SLOT(UnhideMarked()));

        menu->AddItem(tr("Delete Marked"), SLOT(DeleteMarked()));
    }
    else
    {
        // Operate on selected file/dir
        menu = new MythMenu(m_menuState.m_selected->m_name, this, "actionmenu");

        // No actions on Base/Up dirs
        if (m_menuState.m_selected->m_type >= kSubDirectory)
        {
            menu->AddItem(tr("Use as Cover"), SLOT(SetCover()));

            if (m_menuState.m_selected->m_isHidden)
                menu->AddItem(tr("Unhide"), SLOT(Unhide()));
            else
                menu->AddItem(tr("Hide"), SLOT(HideItem()));

            menu->AddItem(tr("Delete"), SLOT(DeleteItem()));
            menu->AddItem(tr("Rename"), SLOT(ShowRenameInput()));
        }
        else if (m_menuState.m_selected->m_userThumbnail)
            menu->AddItem(tr("Reset Cover"), SLOT(ResetCover()));

        // Can only import/mkdir in a dir
        if (m_menuState.m_selected->IsDirectory())
        {
            menu->AddItem(tr("Create Directory"), SLOT(MakeDir()));
            MenuImport(menu);
        }
        menu->AddItem(tr("Eject media"), SLOT(Eject()));
    }

    mainMenu->AddItem(tr("Actions"), NULL, menu);
}


/*!
 \brief Add an Import submenu
 \param mainMenu Parent menu
*/
void GalleryThumbView::MenuImport(MythMenu *parent)
{
    MythMenu *menu = new MythMenu(tr("Import Options"), this, "importmenu");

    menu->AddItem(tr("By Moving"), SLOT(ShowMoveImport()));
    menu->AddItem(tr("By Copying"), SLOT(ShowImport()));

    if (gCoreContext->GetNumSetting("GalleryUseImportCmd"))
        menu->AddItem(tr("Run Import Command"), SLOT(RunImportCmd()));

    parent->AddItem(tr("Import"), NULL, menu);
}


/*!
 \brief Add a Slideshow submenu
 \param mainMenu Parent menu
*/
void GalleryThumbView::MenuSlideshow(MythMenu *mainMenu)
{
    int order = gCoreContext->GetNumSetting("GallerySlideOrder", kOrdered);

    QString ordering;
    switch (order)
    {
    case 0 : ordering = tr("Ordered\n"); break;
    case 1 : ordering = tr("Shuffled\n"); break;
    case 2 : ordering = tr("Random\n"); break;
    case 3 : ordering = tr("Seasonal\n"); break;
    default: ordering = tr("Unknown ordering\n"); break;
    }

    MythMenu *menu = new MythMenu(ordering + tr("Slideshow"), this, "SlideshowMenu");

    // Use selected dir or parent, if image selected
    if (m_menuState.m_selected->IsDirectory())
    {
        menu->AddItem(tr("Directory"), SLOT(Slideshow()));

        if (m_menuState.m_selected->m_dirCount > 0)
            menu->AddItem(tr("Recursive"), SLOT(RecursiveSlideshow()));
    }
    else
        menu->AddItem(tr("Current Directory"), SLOT(Slideshow()));

    MythMenu *orderMenu = new MythMenu(tr("Slideshow Order"), this, "SlideOrderMenu");

    orderMenu->AddItem(tr("Ordered"),  NULL, NULL, order == kOrdered);
    orderMenu->AddItem(tr("Shuffled"), NULL, NULL, order == kShuffle);
    orderMenu->AddItem(tr("Random"),   NULL, NULL, order == kRandom);
    orderMenu->AddItem(tr("Seasonal"), NULL, NULL, order == kSeasonal);

    menu->AddItem(tr("Change Order"), NULL, orderMenu);

    if (gCoreContext->GetNumSetting("GalleryRepeat", 0))
        menu->AddItem(tr("Turn Repeat Off"), SLOT(RepeatOff()));
    else
        menu->AddItem(tr("Turn Repeat On"), SLOT(RepeatOn()));

    mainMenu->AddItem(tr("Slideshow"), NULL, menu);
}


/*!
 \brief Add a Show submenu
 \param mainMenu Parent menu
*/
void GalleryThumbView::MenuShow(MythMenu *mainMenu)
{
    MythMenu *menu = new MythMenu(tr("Show Options"), this, "showmenu");

    int type = gCoreContext->GetNumSetting("GalleryShowType");
    if (type == kPicAndVideo)
    {
        menu->AddItem(tr("Hide Pictures"), SLOT(HidePictures()));
        menu->AddItem(tr("Hide Videos"), SLOT(HideVideos()));
    }
    else
        menu->AddItem(type == kPicOnly ? tr("Show Videos") : tr("Show Pictures"),
                      SLOT(ShowType()));

    int show = gCoreContext->GetNumSetting("GalleryShowCaption");
    if (show != kNoCaption)
        menu->AddItem(tr("Hide Captions"), SLOT(ShowCaptions()));
    if (show != kNameCaption)
        menu->AddItem(tr("Show Name Captions"), SLOT(CaptionsName()));
    if (show != kDateCaption)
        menu->AddItem(tr("Show Date Captions"), SLOT(CaptionsDate()));
    if (show != kUserCaption)
        menu->AddItem(tr("Show Comment Captions"), SLOT(CaptionsComment()));

    if (gCoreContext->GetNumSetting("GalleryShowHidden"))
        menu->AddItem(tr("Hide Hidden Files"), SLOT(HideHidden()));
    else
        menu->AddItem(tr("Show Hidden Files"), SLOT(ShowHidden()));

    if (m_zoomLevel > 0)
        menu->AddItem(tr("Zoom Out"), SLOT(ZoomOut()));
    if (m_zoomLevel < m_zoomWidgets.size() - 1)
        menu->AddItem(tr("Zoom In"), SLOT(ZoomIn()));

    mainMenu->AddItem(tr("Show"), NULL, menu);
}


/*!
 \brief Update item that has been selected
 \param item Buttonlist item
*/
void GalleryThumbView::ItemSelected(MythUIButtonListItem *item)
{
    ImageItem *im = item->GetData().value<ImageItem *>();
    if (!im)
        return;

    // update the position in the node list
    m_view->Select(im->m_id);

    // Update other widgets
    SetUiSelection(item);
}


/*!
 \brief Select item if it is displayed
 \param id Image id
*/
void GalleryThumbView::SelectImage(int id)
{
    // Only update selection if image is currently displayed
    if (m_view->Select(id, -1))
        BuildImageList();
}


/*!
 \brief Action item click
 \param item Buttonlist item
*/
void GalleryThumbView::ItemClicked(MythUIButtonListItem *item)
{
    if (!item)
        return;

    ImageItem *im = item->GetData().value<ImageItem *>();
    if (!im)
        return;

    switch (im->m_type)
    {
        case kBaseDirectory:
        case kSubDirectory:     DirSelectDown(); break;
        case kUpDirectory:      DirSelectUp(); break;
        case kImageFile:
        case kVideoFile:        StartSlideshow(kBrowseSlides); break;
    };
}


/*!
 \brief Action scan request
 \param start Start scan, if true. Otherwise stop scan
*/
void GalleryThumbView::StartScan(bool start)
{
    QString err = GalleryBERequest::ScanImagesAction(start);
    if (!err.isEmpty())
        ShowOkPopup(err);
}


/*!
 \brief Start slideshow screen
 \param mode Browse, Normal or Recursive
*/
void GalleryThumbView::StartSlideshow(ImageSlideShowType mode)
{
    ImageItem *selected = m_view->GetSelected();
    if (!selected)
        return;

    GallerySlideView *slide = new GallerySlideView(m_mainStack, "galleryslideview");
    if (slide->Create())
    {
        m_mainStack->AddScreen(slide);

        // Pass on thumbnail updates
        connect(this, SIGNAL(ThumbnailChanged(int)),
                slide, SLOT(ThumbnailChange(int)));

        // Update selected item when slideshow exits
        connect(slide, SIGNAL(ImageSelected(int)),
                this, SLOT(SelectImage(int)));

        if (selected->IsDirectory())
            // Show selected dir
            slide->Start(mode, selected->m_id);
        else
            // Show current dir starting at selection
            slide->Start(mode, selected->m_parentId, selected->m_id);
    }
    else
        delete slide;
}


/*!
 *  \brief Goes up one directory level
 */
bool GalleryThumbView::DirSelectUp()
{
    ImageItem *im = m_view->GetParent();
    if (im)
    {
        // Select the upfolder in the higher dir
        m_view->Select(im->m_id);

        // Create tree rooted at parent of the kUpFolder directory node
        LoadData(im->m_parentId);
    }
    return true;
}


/*!
 *  \brief Goes one directory level down
 */
void GalleryThumbView::DirSelectDown()
{
    ImageItem *im = m_view->GetSelected();
    if (im)
        // Create tree rooted at selected item
        LoadData(im->m_id);
}


/*!
 \brief Mark or unmark a single item
 \param mark Mark if true, otherwise unmark
*/
void GalleryThumbView::MarkItem(bool mark)
{
    ImageItem *im = m_view->GetSelected();
    if (im)
    {
        // Mark/unmark selected item
        m_view->Mark(im->m_id, mark);

        // Redisplay buttonlist as a parent dir may have been unmarked by this mark
        BuildImageList();
    }
}


/*!
 \brief Mark or unmark all items
 \param mark Mark if true, otherwise unmark
*/
void GalleryThumbView::MarkAll(bool mark)
{
    if (mark)
        m_view->MarkAll();
    else
        m_view->ClearMarked();

    // Redisplay buttonlist
    BuildImageList();
}


/*!
 \brief Invert all marked items
*/
void GalleryThumbView::MarkInvertAll()
{
    m_view->InvertMarked();

    // Redisplay buttonlist
    BuildImageList();
}


/*!
 \brief Apply transform to an image
 \param transform Rotation/Flip
*/
void GalleryThumbView::TransformItem(ImageFileTransform transform)
{
    ImageItem *im = m_view->GetSelected();
    if (im && m_editsAllowed)
    {
        ImageIdList ids;
        ids.append(im->m_id);
        QString err = GalleryBERequest::ChangeOrientation(transform, ids);
        if (!err.isEmpty())
            ShowOkPopup(err);
    }
}


/*!
 \brief Apply transform to marked images
 \param transform Rotation/Flip
*/
void GalleryThumbView::TransformMarked(ImageFileTransform transform)
{
    QString err = GalleryBERequest::ChangeOrientation(transform, m_menuState.m_markedId);
    if (!err.isEmpty())
        ShowOkPopup(err);
}


/*!
 \brief Hide or unhide item
 \param hide Hide if true; otherwise unhide
*/
void GalleryThumbView::HideItem(bool hide)
{
    if (m_menuState.m_selected)
    {
        ImageIdList ids;
        ids.append(m_menuState.m_selected->m_id);

        QString err = GalleryBERequest::HideFiles(hide, ids);
        if (!err.isEmpty())

            ShowOkPopup(err);

        else if (hide)

            // Unmark hidden file
            m_view->Mark(m_menuState.m_selected->m_id, false);
    }
}


/*!
 \brief Hide or unhide marked items
 \param hide Hide if true; otherwise unhide
*/
void GalleryThumbView::HideMarked(bool hide)
{
    QString err = GalleryBERequest::HideFiles(hide, m_menuState.m_markedId);
    if (!err.isEmpty())

        ShowOkPopup(err);

    else if (hide)

        // Unmark hidden files
        foreach (int id, m_menuState.m_markedId)
        {
            m_view->Mark(id, false);
        }
}


/*!
 \brief Confirm user deletion of an item
*/
void GalleryThumbView::DeleteItem()
{
    if (m_menuState.m_selected)
    {
        ShowDialog(tr("Do you want to delete\n%1 ?").arg(m_menuState.m_selected->m_name),
                   "ConfirmDelete");
    }
}


/*!
 \brief Confirm user deletion of marked files
*/
void GalleryThumbView::DeleteMarked()
{
    ShowDialog(tr("Do you want to delete all marked files ?"),
               "ConfirmDeleteMarked");
}


/*!
 \brief Show configuration screen
*/
void GalleryThumbView::ShowSettings()
{
    QString oldExclusions = gCoreContext->GetSetting("GalleryIgnoreFilter");
    int oldSort = gCoreContext->GetNumSetting("GallerySortOrder");

    // Show settings dialog
    GalleryConfig config = GalleryConfig(m_editsAllowed);
    config.exec();
    gCoreContext->ClearSettingsCache();

    QString exclusions = gCoreContext->GetSetting("GalleryIgnoreFilter");
    int sort = gCoreContext->GetNumSetting("GallerySortOrder");

    if (exclusions != oldExclusions)

        // Exclusions changed: request rescan
        GalleryBERequest::IgnoreDirs(exclusions);

    else if (sort != oldSort)
    {
        // Order changed: Update db view & reload
        m_db->SetSortOrder(sort);
        LoadData();
    }
}


/*!
 \brief Show or hide hidden files
 \param show Show hidden, if true. Otherwise hide hidden
*/
void GalleryThumbView::ShowHidden(bool show)
{
    // Save setting, update db view & reload
    gCoreContext->SaveSetting("GalleryShowHidden", show);
    m_db->SetVisibility(show);
    LoadData();
}


/*!
 \brief Show or hide captions
 \param setting None, Names, Dates or Comments
*/
void GalleryThumbView::ShowCaptions(int setting)
{
    gCoreContext->SaveSetting("GalleryShowCaption", setting);
    BuildImageList();
}


/*!
 \brief Show a confirmation dialog
 \param msg Text to display
 \param event Event label
*/
void GalleryThumbView::ShowDialog(QString msg, QString event)
{
    MythConfirmationDialog *popup =
        new MythConfirmationDialog(m_popupStack, msg, true);

    if (popup->Create())
    {
        popup->SetReturnEvent(this, event);
        m_popupStack->AddScreen(popup);
    }
    else
        delete popup;
}


/*!
 \brief Show dialog to allow input
*/
void GalleryThumbView::ShowRenameInput()
{
    if (m_menuState.m_selected)
    {
        QString base =
            QFileInfo(m_menuState.m_selected->m_name).completeBaseName();
        QString              msg   = tr("Enter a new name for '%1'.").arg(base);
        MythTextInputDialog *popup =
            new MythTextInputDialog(m_popupStack, msg, FilterNone, false, base);
        if (popup->Create())
        {
            popup->SetReturnEvent(this, "FileRename");
            m_popupStack->AddScreen(popup);
        }
        else
            delete popup;
    }
}


/*!
 \brief Shows exif info/details about an item
 */
void GalleryThumbView::ShowDetails()
{
    m_infoList->Toggle(m_view->GetSelected());
}


/*!
 \brief Displays dialog to accept password
*/
void GalleryThumbView::ShowPassword()
{
    QString msg = tr("Enter password:");
    MythTextInputDialog *popup =
        new MythTextInputDialog(m_popupStack, msg, FilterNone, true);
    if (popup->Create())
    {
        popup->SetReturnEvent(this, "Password");
        m_popupStack->AddScreen(popup);
    }
    else
        delete popup;
}


/*!
 \brief Show/hide pictures or videos
*/
void GalleryThumbView::ShowType(int type)
{
    gCoreContext->SaveSetting("GalleryShowType", type);
    m_db->SetType(type);
    LoadData();
}


/*!
 \brief Set or reset thumbnails to use for a directory cover
 \param reset Reset cover if true, otherwise assign selected item as cover of parent
*/
void GalleryThumbView::SetCover(bool reset)
{
    if (m_menuState.m_selected)
    {
        QString err = reset
                ? GalleryBERequest::SetCover(m_menuState.m_selected->m_id, 0)
                : GalleryBERequest::SetCover(m_menuState.m_selected->m_parentId,
                                              m_menuState.m_selected->m_id);
        if (!err.isEmpty())
            ShowOkPopup(err);
    }
}


/*!
 \brief Use larger buttonlist widgets
*/
void GalleryThumbView::ZoomOut()
{
    SelectZoomWidget(-1);
    BuildImageList();
}


/*!
 \brief Use smaller buttonlist widgets
*/
void GalleryThumbView::ZoomIn()
{
    SelectZoomWidget(1);
    BuildImageList();
}


/*!
 \brief Change buttonlist to use a different size
 \param change Adjustment, +1 to use bigger buttons, -1 for smaller buttons
*/
void GalleryThumbView::SelectZoomWidget(int change)
{
    m_zoomLevel += change;

    // constrain to zoom levels supported by theme
    if (m_zoomLevel < 0)
        m_zoomLevel = 0;
    if (m_zoomLevel >= m_zoomWidgets.size())
        m_zoomLevel = m_zoomWidgets.size() - 1;

    // store any requested change, but not constraining adjustments
    // Thus, changing to a theme with fewer zoom levels will not overwrite the
    // setting
    if (change != 0)
        gCoreContext->SaveSetting("GalleryZoomLevel", m_zoomLevel);

    // dump the current list widget
    if (m_imageList)
    {
        m_imageList->SetVisible(false);
        disconnect(m_imageList, 0, this, 0);
    }

    // initialise new list widget
    m_imageList = m_zoomWidgets.at(m_zoomLevel);

    m_imageList->SetVisible(true);
    SetFocusWidget(m_imageList);

    // Monitor list actions (after focus events have been ignored)
    connect(m_imageList, SIGNAL(itemClicked(MythUIButtonListItem *)),
            SLOT(ItemClicked(MythUIButtonListItem *)));
    connect(m_imageList, SIGNAL(itemSelected(MythUIButtonListItem *)),
            SLOT(ItemSelected(MythUIButtonListItem *)));
}


/*!
 \brief Show dialog to input new directory name
*/
void GalleryThumbView::MakeDir()
{
    MythTextInputDialog *popup = new MythTextInputDialog(
        m_popupStack, tr("Enter name of new directory"), FilterNone, false);
    if (popup->Create())
    {
        popup->SetReturnEvent(this, "MakeDir");
        m_popupStack->AddScreen(popup);
    }
    else
        delete popup;
}


/*!
 \brief Move marked files to selected dir. If no marked files, use previously
 marked files
*/
void GalleryThumbView::Move()
{
    ImageItem *destIm = m_menuState.m_selected;

    // Destination must be a dir
    if (!destIm || destIm->IsFile())
        return;

    // Use current markings, if any. Otherwise use previous markings
    ImageIdList files = m_menuState.m_markedId;
    if (files.isEmpty())
        files = m_menuState.m_prevMarkedId;
    if (files.isEmpty())
        return;

    // Move files/dirs in clipboard
    QString err = GalleryBERequest::MoveFiles(destIm->m_id, files);
    if (!err.isEmpty())
        ShowOkPopup(err);
}


/*!
 \brief Copy marked files to selected dir. If no marked files, use previously
 marked files
*/
void GalleryThumbView::Copy()
{
    // Destination must be a dir
    ImageItem *destIm = m_menuState.m_selected;
    if (!destIm || destIm->IsFile())
        return;

    // Use current markings, if any. Otherwise use previous markings
    ImageIdList files = m_menuState.m_markedId;
    if (files.isEmpty())
        files = m_menuState.m_prevMarkedId;
    if (files.isEmpty())
        return;

    // Get paste files from clipboard
    QStringList ids;
    foreach (int id, files)
        ids.append(QString::number(id));

    // Create new dir subtree & copy all files within it

    // Get all files/dirs in subtree
    ImageList images, dirs;
    m_db->ReadDbTree(images, dirs, ids);

    // Determine root dir of marked items
    ImageItem *baseIm;
    // Any dirs will be in hierarchial order so root must be derived from first
    if (!dirs.isEmpty())
        baseIm = dirs[0];
    // If no dirs, root can be derived from any image
    else if (!images.isEmpty())
        baseIm = images[0];
    else // should never happen
        return;

    QDir base = QDir("/" + baseIm->m_path);
    QDir destDir(destIm->m_id == ROOT_DB_ID ? "" : destIm->m_fileName);

    // Create dir subtree
    if (!dirs.isEmpty())
    {
        // Build list of destination paths for dirs
        QStringList newDirs;
        foreach(const ImageItem *im, dirs)
            newDirs << destDir.filePath(base.relativeFilePath("/" + im->m_fileName));

        qDeleteAll(dirs);

        // Request BE to create directories first
        QString err = GalleryBERequest::MakeDirs(newDirs);
        if (!err.isEmpty())
        {
            ShowOkPopup(err);
            return;
        }
    }

    // Build map of source & destination paths for each image
    NameMap transfer;
    foreach(const ImageItem *im, images)
    {
        ImageSg *sg = ImageSg::getInstance();
        QString srcName  = sg->GenerateUrl(im->m_fileName);
        QString destName = sg->GenerateUrl(
            destDir.filePath(base.relativeFilePath("/" + im->m_fileName)));
        transfer.insert(srcName, destName);
    }

    qDeleteAll(images);

    // Create progress dialog
    MythScreenStack      *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythUIProgressDialog *progress   =
        new MythUIProgressDialog(tr("Copying files"), popupStack, "copydialog");
    if (progress->Create())
        popupStack->AddScreen(progress, false);
    else
        delete progress;

    // Copy files in a separate thread
    if (RunWorker(new FileTransferWorker(false, transfer, progress)))
        // Rescan to detect new files
        GalleryBERequest::ScanImagesAction(true);
    else
        ShowOkPopup(tr("Failed to copy files"));
}


/*!
 \brief Executes user 'Import command'
*/
void GalleryThumbView::RunImportCmd()
{
    // Replace tokens
    QString cmd = gCoreContext->GetSetting("GalleryImportCmd");
    QString dir = gCoreContext->GetSetting("GalleryImportLocation");

    cmd.replace("%DIR%", QString("'%1'").arg(dir));

    if (cmd.contains("%TMPDIR%"))
    {
        // Remove previous temp dir
        delete m_importTmp;
        // Create temp dir
        m_importTmp = new QTemporaryDir();
        if (!m_importTmp->isValid())
        {
            ShowOkPopup(QString(tr("Failed to create temporary directory\n%1"))
                        .arg(m_importTmp->path()));
            return;
        }

        cmd.replace("%TMPDIR%", m_importTmp->path());
    }

    // Run command in a separate thread
    MythUIBusyDialog *busy = ShowBusyPopup(tr("Running Import command.\nPlease wait..."));

    int error = RunWorker(new ShellWorker(cmd));

    if (busy)
        busy->Close();

    if (error != 0)
        ShowOkPopup(QString(tr("Import command failed: Error %1")).arg(error));
}


/*!
 \brief Show import dialog
*/
void GalleryThumbView::ShowImport(bool deleteAfter)
{
    m_deleteAfterImport = deleteAfter;

    // Use temp dir if it exists
    QString start = m_importTmp
            ? m_importTmp->path()
            : gCoreContext->GetSetting("GalleryImportLocation", "/media");

    MythScreenStack        *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythUIMultiFileBrowser *fb = new MythUIMultiFileBrowser(popupStack, start);

    // Only browse supported image/video files
    QDir filters(ImageSg::getInstance()->GetImageFilters());
    fb->SetNameFilter(filters.nameFilters());

    fb->SetTypeFilter(QDir::AllDirs | QDir::Files | QDir::NoSymLinks | QDir::Readable);
    if (fb->Create())
    {
        fb->SetReturnEvent(this, "ImportFilesSelected");
        popupStack->AddScreen(fb);
    }
    else
        delete fb;
}


/*!
 \brief Import files into selected dir
 \param files List of file names
*/
void GalleryThumbView::ImportFiles(QStringList files)
{
    // Determine destination dir
    ImageItem *destIm = m_menuState.m_selected;

    // Destination must be a dir
    if (!destIm || destIm->IsFile())
        return;

    QDir    destDir(destIm->m_id == ROOT_DB_ID ? "" : destIm->m_fileName);

    // Build map of source & destination paths.
    // Assumes all files are images; no dirs permitted
    NameMap transfer;
    ImageSg *sg = ImageSg::getInstance();
    foreach(const QString &source, files)
    {
        QFileInfo fi(source);
        QString   destName(sg->GenerateUrl(destDir.filePath(fi.fileName())));
        transfer.insert(source, destName);
    }

    // Create progress dialog
    MythScreenStack      *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythUIProgressDialog *progress   =
        new MythUIProgressDialog(tr("Importing files"),
                                 popupStack,
                                 "importdialog");
    if (progress->Create())
        popupStack->AddScreen(progress, false);
    else
    {
        delete progress;
        progress = NULL;
    }

    // Import files in a separate thread
    if (RunWorker(new FileTransferWorker(m_deleteAfterImport, transfer, progress)))
        // Rescan to detect new files
        GalleryBERequest::ScanImagesAction(true);
    else
        ShowOkPopup(tr("Failed to import files"));
}
