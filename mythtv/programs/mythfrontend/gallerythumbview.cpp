// C++
#include <algorithm>
#include <chrono> // for milliseconds
#include <thread> // for sleep_for
#include <utility>

// Qt
#include <QApplication>

// MythTV
#include "libmythbase/compat.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythsystemlegacy.h"
#include "libmythbase/remotefile.h"
#include "libmythui/mythdialogbox.h"
#include "libmythui/mythprogressdialog.h"
#include "libmythui/mythuiprogressbar.h"
#include "libmythui/mythuitext.h"

// MythFrontend
#include "galleryconfig.h"
#include "gallerythumbview.h"

#define LOC QString("Thumbview: ")

// EXIF tag 0x9286 UserComment can contain garbage
static QString clean_comment(const QString &comment)
{
    QString result;
    std::copy_if(comment.cbegin(), comment.cend(), std::back_inserter(result), [](QChar x) { return x.isPrint(); } );
    return result;
}

//! Worker thread for running import
class ShellThread: public MThread
{
public:
    ShellThread(QString cmd, QString path)
        : MThread("Import"), m_command(std::move(cmd)), m_path(std::move(path)) {}

    int GetResult(void) const { return m_result; }

    void run() override // MThread
    {
        RunProlog();

        QString cmd = QString("cd %1 && %2").arg(m_path, m_command);
        LOG(VB_GENERAL, LOG_INFO, QString("Executing \"%1\"").arg(cmd));

        m_result = myth_system(cmd);

        LOG(VB_GENERAL, LOG_INFO, QString(" ...with result %1").arg(m_result));

        RunEpilog();
    }

private:
    int     m_result {0};
    QString m_command;
    QString m_path;
};


//! Worker thread for copying/moving files
class TransferThread : public MThread
{
    Q_DECLARE_TR_FUNCTIONS(FileTransferWorker);
public:
    using TransferMap = QMap<ImagePtrK, QString>;
    using ImageSet = QSet<ImagePtrK>;

    TransferThread(TransferMap files, bool move, MythUIProgressDialog *dialog)
        : MThread("FileTransfer"),
          m_move(move), m_files(std::move(files)), m_dialog(dialog) {}

    ImageSet GetResult(void) { return m_failed; }

    void run() override // MThread
    {
        RunProlog();

        QString action = m_move ? tr("Moving") : tr("Copying");

        // Sum file sizes
        auto keys = m_files.keys();
        auto add_size = [](int t, const ImagePtrK & im){ return t + im->m_size; };
        int total = std::accumulate(keys.cbegin(), keys.cend(), 0, add_size);

        int progressSize = 0;
        for (auto it = m_files.constKeyValueBegin();
             it != m_files.constKeyValueEnd(); it++)
        {
            const ImagePtrK & im = it->first;
            QString newPath = it->second;
            // Update progress dialog
            if (m_dialog)
            {
                QString message = QString("%1 %2\n%3")
                        .arg(action, QFileInfo(im->m_url).fileName(),
                             ImageAdapterBase::FormatSize(im->m_size / 1024));

                auto *pue = new ProgressUpdateEvent(progressSize, total, message);
                QApplication::postEvent(m_dialog, pue);
            }

            LOG(VB_FILE, LOG_INFO, QString("%2 %3 -> %4")
                .arg(action, im->m_url, newPath));

            bool success = m_move ? RemoteFile::MoveFile(im->m_url, newPath)
                                  : RemoteFile::CopyFile(im->m_url, newPath,
                                                         false, true);
            if (!success)
            {
                // Flag failures
                m_failed.insert(im);

                LOG(VB_GENERAL, LOG_ERR,
                    QString("%1: Failed to copy/move %2 -> %3")
                    .arg(objectName(), im->m_url, m_files[im]));
            }

            progressSize += im->m_size;
        }

        // Update progress dialog
        if (m_dialog)
        {
            auto *pue =
                    new ProgressUpdateEvent(progressSize, total, tr("Complete"));
            QApplication::postEvent(m_dialog, pue);
        }

        RunEpilog();
    }

private:
    bool                  m_move;   //!< Copy if false, Move if true
    TransferMap           m_files;  //!< Maps source filepath to destination filepath
    ImageSet              m_failed; //! Images for which copy/move failed
    MythUIProgressDialog *m_dialog; //!< Progress dialog for transfer
};


/*!
 \brief Runs a worker thread and waits for it to finish
 \param worker Thread to execute
*/
static void WaitUntilDone(MThread &worker)
{
    worker.start();
    while (!worker.isFinished())
    {
        std::this_thread::sleep_for(1ms);
        QCoreApplication::processEvents();
    }
}


/*!
 *  \brief  Constructor
 *  \param  parent The screen parent
 *  \param  name The name of the screen
 */
GalleryThumbView::GalleryThumbView(MythScreenStack *parent, const char *name)
    : MythScreenType(parent, name),
      m_popupStack(*GetMythMainWindow()->GetStack("popup stack")),
      m_mgr(ImageManagerFe::getInstance()),
      // This screen uses a single fixed view (Parent dir, ordered dirs, ordered images)
      m_view(new DirectoryView(kOrdered)),
      m_infoList(*this),
      // Start in edit mode unless a password exists
      m_editsAllowed(gCoreContext->GetSetting("GalleryPassword").isEmpty())
{
    // Hide hidden when edits disallowed
    if (!m_editsAllowed)
        m_mgr.SetVisibility(false);
}


/*!
 \brief  Destructor
 */
GalleryThumbView::~GalleryThumbView()
{
    LOG(VB_GUI, LOG_DEBUG, LOC + "Exiting Gallery");
    delete m_view;
}


/*!
 \brief Exit Gallery
*/
void GalleryThumbView::Close()
{
    LOG(VB_GUI, LOG_DEBUG, LOC + "Closing Gallery");

    gCoreContext->removeListener(this);

    // Cleanup local devices
    m_mgr.CloseDevices();

    // Cleanup view
    m_view->Clear();

    MythScreenType::Close();
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
    QString name = QString("images%1").arg(zoom++);
    auto *widget = dynamic_cast<MythUIButtonList *>(this->GetChild(name));
    while (widget)
    {
        m_zoomWidgets.append(widget);
        widget->SetVisible(false);

        name = QString("images%1").arg(zoom++);
        widget = dynamic_cast<MythUIButtonList *>(this->GetChild(name));
    }

    if (m_zoomWidgets.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Screen 'Gallery' is missing 'images0'");
        return false;
    }
    LOG(VB_GUI, LOG_DEBUG, LOC + QString("Screen 'Gallery' found %1 zoom levels")
        .arg(m_zoomWidgets.size()));

    // File details list is managed elsewhere
    if (!m_infoList.Create(false))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Cannot load 'Info buttonlist'");
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
        const QString& action = actions[i];
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
        else if (action == "COVER")
        {
            ImagePtrK im = m_view->GetSelected();
            if (m_editsAllowed && im)
            {
                if (im == m_view->GetParent())
                {
                    // Reset dir
                    m_mgr.SetCover(im->m_id, 0);
                }
                else
                {
                    // Set parent cover
                    m_mgr.SetCover(im->m_parentId, im->m_id);
                }
            }
        }
        else if (action == "PLAY")
        {
            Slideshow();
        }
        else if (action == "RECURSIVESHOW")
        {
            ImagePtrK im = m_view->GetSelected();
            if (im && im->IsDirectory())
                RecursiveSlideshow();
        }
        else if (action == "MARK")
        {
            ImagePtrK im = m_view->GetSelected();
            if (m_editsAllowed && im && im != m_view->GetParent())
                DoMarkItem(!m_view->IsMarked(im->m_id));
        }
        else if (action == "ESCAPE" && !GetMythMainWindow()->IsExitingToMain())
        {
            // Exit info list, if shown
            handled = m_infoList.Hide();

            // Ascend the tree unless parent is root,
            // or a device and multiple devices/imports exist
            if (!handled)
            {
                ImagePtrK node = m_view->GetParent();
                if (node && node->m_id != GALLERY_DB_ID
                        && (!node->IsDevice() || m_mgr.DeviceCount() > 0))
                    handled = DirSelectUp();
            }
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


/*!
 *  \brief  Handle custom events
 *  \param  event The custom event
 */
void GalleryThumbView::customEvent(QEvent *event)
{

    if (event->type() == MythEvent::kMythEventMessage)
    {
        auto *me = dynamic_cast<MythEvent *>(event);
        if (me == nullptr)
            return;

        const QString& mesg  = me->Message();
        QStringList extra = me->ExtraDataList();

        // Internal messages contain a hostname. Ignore other FE messages
        QStringList token = mesg.split(' ');
        if (token.size() >= 2 && token[1] != gCoreContext->GetHostName())
            return;

        if (token[0] == "IMAGE_METADATA")
        {
            int id = extra[0].toInt();
            ImagePtrK selected = m_view->GetSelected();

            if (selected && selected->m_id == id)
                m_infoList.Display(*selected, extra.mid(1));
        }
        else if (token[0] == "THUMB_AVAILABLE")
        {
            int id = extra[0].toInt();

            // Note existance of all thumbs
            m_thumbExists.insert(id);

            // Get all buttons waiting for this thumbnail
            QList<ThumbLocation> affected = m_pendingMap.values(id);

            // Only concerned with thumbnails we've requested
            if (affected.isEmpty())
                return;

            LOG(VB_GENERAL, LOG_DEBUG, LOC +
                QString("Rx %1 : %2").arg(token[0], extra.join(",")));

            // Thumb url was cached when request was sent
            QString url = m_view->GetCachedThumbUrl(id);

            // Set thumbnail for each button now it exists
            for (const ThumbLocation & location : std::as_const(affected))
            {
                MythUIButtonListItem *button = location.first;
                int                   index  = location.second;

                auto im = button->GetData().value<ImagePtrK>();
                if (im)
                    UpdateThumbnail(button, im, url, index);
            }

            // Cancel pending request
            m_pendingMap.remove(id);
        }
        else if (token[0] == "IMAGE_DB_CHANGED")
        {
            // Expects csv list of deleted ids, csv list of changed ids
            LOG(VB_GENERAL, LOG_DEBUG, LOC +
                QString("Rx %1 : %2").arg(token[0], extra.join(",")));

            if (!extra.isEmpty())
            {
                QStringList idDeleted =
                        extra[0].split(",", Qt::SkipEmptyParts);
                RemoveImages(idDeleted);
            }
            if (extra.size() >= 2)
            {
                QStringList idChanged =
                        extra[1].split(",", Qt::SkipEmptyParts);
                RemoveImages(idChanged, false);
            }

            // Refresh display
            LoadData(m_view->GetParentId());
        }
        else if (token[0] == "IMAGE_DEVICE_CHANGED")
        {
            // Expects list of url prefixes
            LOG(VB_GENERAL, LOG_DEBUG, LOC +
                QString("Rx %1 : %2").arg(token[0], extra.join(",")));

            // Clear everything. Local devices will be rebuilt
            m_view->Clear();
            m_thumbExists.clear();

            // Remove thumbs & images from image cache using supplied prefixes
            for (const QString & url : std::as_const(extra))
                GetMythUI()->RemoveFromCacheByFile(url);

            // Refresh display
            LoadData(m_view->GetParentId());
        }
        else if (token[0] == "IMAGE_SCAN_STATUS" && extra.size() == 3)
        {
            // Expects scanner id, scanned#, total#
            UpdateScanProgress(extra[0], extra[1].toInt(), extra[2].toInt());
        }
    }
    else if (event->type() == DialogCompletionEvent::kEventType)
    {
        auto *dce = (DialogCompletionEvent *)(event);

        QString resultid  = dce->GetId();
        int     buttonnum = dce->GetResult();

        if (resultid == "FileRename")
        {
            QString newName = dce->GetResultText();
            if (m_menuState.m_selected)
            {
                QString err = m_mgr.RenameFile(m_menuState.m_selected,
                                               newName);
                if (!err.isEmpty())
                    ShowOkPopup(err);
            }
        }
        else if (resultid == "MakeDir")
        {
            if (m_menuState.m_selected)
            {
                // Prohibit subtrees
                QString name = dce->GetResultText();
                QString err  = name.contains("/")
                        ? tr("Invalid Name")
                        : m_mgr.MakeDir(m_menuState.m_selected->m_id,
                                        QStringList(name));
                if (!err.isEmpty())
                    ShowOkPopup(err);
            }
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
            LOG(VB_FILE, LOG_DEBUG, LOC + QString("Order %1").arg(slideOrder));
        }
        else if (resultid == "ImageCaptionMenu")
        {
            ImageCaptionType captions = kNoCaption;

            switch (buttonnum)
            {
            case 0: captions = kNameCaption; break;
            case 1: captions = kDateCaption; break;
            case 2: captions = kUserCaption; break;
            case 3: captions = kNoCaption;   break;
            }
            gCoreContext->SaveSetting("GalleryImageCaption", captions);
            BuildImageList();
        }
        else if (resultid == "DirCaptionMenu")
        {
            ImageCaptionType captions = kNoCaption;

            switch (buttonnum)
            {
            case 0: captions = kNameCaption; break;
            case 1: captions = kDateCaption; break;
            case 2: captions = kNoCaption;   break;
            }
            gCoreContext->SaveSetting("GalleryDirCaption", captions);
            BuildImageList();
        }
        else if (resultid == "Password")
        {
            QString password = dce->GetResultText();
            m_editsAllowed = (password == gCoreContext->GetSetting("GalleryPassword"));
        }
        else if (buttonnum == 1)
        {
            // Confirm current file deletion
            QString err;
            if (resultid == "ConfirmDelete" && m_menuState.m_selected)
            {
                ImageIdList ids = ImageIdList() << m_menuState.m_selected->m_id;
                err = m_mgr.DeleteFiles(ids);
            }
            // Confirm marked file deletion
            else if (resultid == "ConfirmDeleteMarked")
            {
                err = m_mgr.DeleteFiles(m_menuState.m_markedId);
            }
            else
            {
                return;
            }

            if (!err.isEmpty())
                ShowOkPopup(err);
        }
    }
}


/*!
 \brief Cleanup UI & image caches when a device is removed
 \param ids List of ids to remove from image cache
 \param deleted If true, images are also deleted from view
*/
void GalleryThumbView::RemoveImages(const QStringList &ids, bool deleted)
{
    for (const QString & id : std::as_const(ids))
    {
        // Remove image from view
        QStringList urls = m_view->RemoveImage(id.toInt(), deleted);
        // Cleanup url lookup
        m_thumbExists.remove(id.toInt());

        // Remove thumbs & images from image cache
        for (const QString & url : std::as_const(urls))
        {
            LOG(VB_FILE, LOG_DEBUG, LOC +
                QString("Clearing image cache of '%1'").arg(url));

            GetMythUI()->RemoveFromCacheByFile(url);
        }
    }
}


/*!
 \brief Start Thumbnail screen
 */
void GalleryThumbView::Start()
{
    // Detect any running BE scans
    // Expects OK, scanner id, current#, total#
    QStringList message = ImageManagerFe::ScanQuery();
    if (message.size() == 4 && message[0] == "OK")
    {
        UpdateScanProgress(message[1], message[2].toInt(), message[3].toInt());
    }

    // Only receive events after device/scan status has been established
    gCoreContext->addListener(this);

    // Start at Root if devices exist. Otherwise go straight to SG node
    int start = m_mgr.DetectLocalDevices() ? GALLERY_DB_ID : PHOTO_DB_ID;

    LoadData(start);
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
        {
            m_emptyText->SetVisible(false);
            m_emptyText->Reset();
        }

        // Construct the buttonlist
        BuildImageList();
    }
    else
    {
        m_infoList.Hide();
        m_imageList->SetVisible(false);
        if (m_emptyText)
        {
            m_emptyText->SetVisible(true);
            m_emptyText->SetText(tr("No images found.\n"
                                    "Scan storage group using menu,\n"
                                    "or insert/mount local media.\n"));
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

    // Get parent & all children
    ImageListK nodes    = m_view->GetAllNodes();
    ImagePtrK  selected = m_view->GetSelected();

    // go through the entire list and update
    for (const ImagePtrK & im : std::as_const(nodes))
    {
        if (im)
        {
            // Data must be set by constructor: First item is automatically
            // selected and must have data available for selection event, as
            // subsequent reselection of same item will always fail.
            auto *item = new MythUIButtonListItem(m_imageList, "",
                                                  QVariant::fromValue(im));

            item->setCheckable(true);
            item->setChecked(MythUIButtonListItem::NotChecked);

            // assign and display all information about
            // the current item, like title and subdirectory count
            UpdateImageItem(item);

            // Treat parent differently
            if (im == nodes[0])
            {
                // Only non-root parents can ascend
                if (im->m_id != GALLERY_DB_ID)
                    item->DisplayState("upfolder", "parenttype");
            }
            else if (im == selected)
            {
                // Reinstate the active button item. Note this would fail for parent
                m_imageList->SetItemCurrent(item);
            }
        }
    }
}


/*!
 *  \brief  Initialises a single buttonlist item
 *  \param  item The buttonlist item
 */
void GalleryThumbView::UpdateImageItem(MythUIButtonListItem *item)
{
    auto im = item->GetData().value<ImagePtrK >();
    if (!im)
        return;

    // Allow themes to distinguish between roots, folders, pics, videos
    switch (im->m_type)
    {
    case kDevice:
    case kCloneDir:
    case kDirectory:
        if (im->m_dirCount > 0)
        {
            item->SetText(QString("%1/%2")
                          .arg(im->m_fileCount).arg(im->m_dirCount),
                          "childcount");
        }
        else
        {
            item->SetText(QString::number(im->m_fileCount), "childcount");
        }

        item->DisplayState(im->IsDevice() ? "device" : "subfolder", "buttontype");
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

    // Caption
    QString text;
    int show = gCoreContext->GetNumSetting(
                im->IsFile() ? "GalleryImageCaption"
                             : "GalleryDirCaption");
    switch (show)
    {
    case kNameCaption: text = m_mgr.CrumbName(*im); break;
    case kDateCaption: text = m_mgr.ShortDateOf(im); break;
    case kUserCaption: text = clean_comment(im->m_comment); break;
    default:
    case kNoCaption:   text = ""; break;
    }
    item->SetText(text);

    // Set marked state
    MythUIButtonListItem::CheckState state
            = m_view->IsMarked(im->m_id)
            ? MythUIButtonListItem::FullChecked
            : MythUIButtonListItem::NotChecked;

    item->setChecked(state);

    // Thumbnails required
    ImageIdList request;

    if (im->m_thumbNails.size() == 1)
    {
        // Single thumbnail
        QString url = CheckThumbnail(item, im, request, 0);

        if (!url.isEmpty())
            UpdateThumbnail(item, im, url, 0);
    }
    else
    {
        // Dir showing up to 4 thumbs. Set them all at same time
        InfoMap thumbMap;
        for (int index = 0; index < im->m_thumbNails.size(); ++index)
        {
            QString url = CheckThumbnail(item, im, request, index);
            if (!url.isEmpty())
                thumbMap.insert(QString("thumbimage%1").arg(index), url);
        }
        if (!thumbMap.isEmpty())
            item->SetImageFromMap(thumbMap);
    }

    // Request creation/verification of unknown thumbnails.
    if (!request.isEmpty())
        m_mgr.CreateThumbnails(request, im->IsDirectory());
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
QString GalleryThumbView::CheckThumbnail(MythUIButtonListItem *item, const ImagePtrK& im,
                                         ImageIdList &request, int index)
{
    ThumbPair thumb(im->m_thumbNails.at(index));
    int id = thumb.first;

    if (m_thumbExists.contains(id))
        return thumb.second;

    // Request BE thumbnail check if it is not already pending
    if (!m_pendingMap.contains(id))
        request << id;

    // Note this button is awaiting an update
    m_pendingMap.insert(id, qMakePair(item, index));

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
                                       const ImagePtrK& im, const QString &url,
                                       int index)
{
    if (im->m_thumbNails.size() == 1)
    {
        // Pics, dirs & videos use separate widgets
        switch (im->m_type)
        {
        case kImageFile: button->SetImage(url); break;
        case kVideoFile: button->SetImage(url, "videoimage"); break;
        default:         button->SetImage(url, "folderimage"); break;
        }
    }
    else
    {
        // Dir with 4 thumbnails
        button->SetImage(url, QString("thumbimage%1").arg(index));
    }
}


/*!
 \brief Update progressbar with scan status
 \details Combines progress of both BE & FE scanners
 \param scanner Scanner id
 \param current Number of images scanned
 \param total Total number of images to scan
*/
void GalleryThumbView::UpdateScanProgress(const QString &scanner,
                                          int current, int total)
{
    // Scan update
    m_scanProgress.insert(scanner, qMakePair(current, total));

    // Detect end of this scan
    if (current >= total)
    {
        LOG(VB_GUI, LOG_DEBUG, LOC + QString("Scan Finished %1 %2/%3")
            .arg(scanner).arg(current).arg(total));

        // Mark inactive scanner
        m_scanActive.remove(scanner);

        // Detect end of last scan
        if (m_scanActive.isEmpty())
        {
            if (m_scanProgressText)
            {
                m_scanProgressText->SetVisible(false);
                m_scanProgressText->Reset();
            }
            if (m_scanProgressBar)
            {
                m_scanProgressBar->SetVisible(false);
                m_scanProgressBar->Reset();
            }

            m_scanProgress.clear();

            return;
        }
    }
    else
    {
        // Detect first scan update
        if (m_scanActive.isEmpty())
        {
            // Show progressbar when first scan starts
            if (m_scanProgressBar)
            {
                m_scanProgressBar->SetVisible(true);
                m_scanProgressBar->SetStart(0);
            }
            if (m_scanProgressText)
                m_scanProgressText->SetVisible(true);
        }

        if (!m_scanActive.contains(scanner))
        {
            LOG(VB_GUI, LOG_DEBUG, LOC + QString("Scan Started %1 %2/%3")
                .arg(scanner).arg(current).arg(total));

            // Mark active scanner
            m_scanActive.insert(scanner);
        }
    }

    // Aggregate all running scans
    int currentAgg = 0;
    int totalAgg = 0;
    for (IntPair scan : std::as_const(m_scanProgress))
    {
        currentAgg += scan.first;
        totalAgg   += scan.second;
    }

    if (m_scanProgressBar)
    {
        m_scanProgressBar->SetUsed(currentAgg);
        m_scanProgressBar->SetTotal(totalAgg);
    }
    if (m_scanProgressText)
        m_scanProgressText->SetText(tr("%L1 of %L3").arg(currentAgg).arg(totalAgg));
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
    auto im = item->GetData().value<ImagePtrK >();
    if (im)
    {
        // update the position in the node list
        m_view->Select(im->m_id);

        // show the name/path of the image
        if (m_crumbsText)
            m_crumbsText->SetText(m_mgr.CrumbName(*im, true));

        if (m_captionText)
        {
            // show the date & comment of non-root nodes
            QStringList text;
            if (im->m_id != GALLERY_DB_ID)
            {
                if (im->IsFile() || im->IsDevice())
                    text << ImageManagerFe::LongDateOf(im);

                QString comment = clean_comment(im->m_comment);
                if (!comment.isEmpty())
                    text << comment;
            }
            m_captionText->SetText(text.join(" - "));
        }

        if (m_hideFilterText)
        {
            m_hideFilterText->SetText(m_mgr.GetVisibility() ? tr("Hidden") : "");
        }

        if (m_typeFilterText)
        {
            QString text = "";
            switch (m_mgr.GetType())
            {
            case kPicAndVideo : text = ""; break;
            case kPicOnly     : text = tr("Pictures"); break;
            case kVideoOnly   : text = tr("Videos"); break;
            }
            m_typeFilterText->SetText(text);
        }

        // show the position of the image
        if (m_positionText)
            m_positionText->SetText(m_view->GetPosition());

        // Update any file details information
        m_infoList.Update(im);
    }
}


/*!
 *  \brief  Shows the main menu when the MENU button was pressed
 */
void GalleryThumbView::MenuMain()
{
    // Create the main menu
    auto *menu = new MythMenu(tr("Gallery Options"), this, "mainmenu");

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
            menu->AddItem(tr("Enable Edits"), &GalleryThumbView::ShowPassword);
    }

    // Depends on current status of backend scanner - string(number(isBackend()))
    if (m_scanActive.contains("1"))
        menu->AddItem(tr("Stop Scan"), &GalleryThumbView::StopScan);
    else
        menu->AddItem(tr("Scan Storage Group"), &GalleryThumbView::StartScan);

    menu->AddItem(tr("Settings"), &GalleryThumbView::ShowSettings);

    auto *popup = new MythDialogBox(menu, &m_popupStack, "menuPopup");
    if (popup->Create())
        m_popupStack.AddScreen(popup);
    else
        delete popup;
}


/*!
 *  \brief  Adds a Marking submenu
 *  \param  mainMenu Parent menu
 */
void GalleryThumbView::MenuMarked(MythMenu *mainMenu)
{
    ImagePtrK parent = m_view->GetParent();

    if (m_menuState.m_childCount == 0 || parent.isNull())
        return;

    QString   title = tr("%L1 marked").arg(m_menuState.m_markedId.size());
    auto *menu  = new MythMenu(title, this, "markmenu");

    // Mark/unmark selected
    if (m_menuState.m_selected->IsFile())
    {
        if (m_menuState.m_selectedMarked)
            menu->AddItem(tr("Unmark File"), &GalleryThumbView::UnmarkItem);
        else
            menu->AddItem(tr("Mark File"), &GalleryThumbView::MarkItem);
    }
    // Cannot mark/unmark parent dir from this level
    else if (!m_menuState.m_selected->IsDevice()
             && m_menuState.m_selected != parent)
    {
        if (m_menuState.m_selectedMarked)
            menu->AddItem(tr("Unmark Directory"), &GalleryThumbView::UnmarkItem);
        else
            menu->AddItem(tr("Mark Directory"), &GalleryThumbView::MarkItem);
    }

    if (parent->m_id != GALLERY_DB_ID)
    {
        // Mark All if unmarked files exist
        if (m_menuState.m_markedId.size() < m_menuState.m_childCount)
            menu->AddItem(tr("Mark All"), &GalleryThumbView::MarkAll);

        // Unmark All if marked files exist
        if (!m_menuState.m_markedId.isEmpty())
        {
            menu->AddItem(tr("Unmark All"), &GalleryThumbView::UnmarkAll);
            menu->AddItem(tr("Invert Marked"), &GalleryThumbView::MarkInvertAll);
        }
    }

    if (menu->IsEmpty())
        delete menu;
    else
        mainMenu->AddItem(tr("Mark"), nullptr, menu);
}


/*!
 \brief Add a Paste submenu
 \param mainMenu Parent menu
*/
void GalleryThumbView::MenuPaste(MythMenu *mainMenu)
{
    // Can only copy/move into non-root dirs
    if (m_menuState.m_selected->IsDirectory()
            && m_menuState.m_selected->m_id != GALLERY_DB_ID)
    {
        // Operate on current marked files, if any
        ImageIdList files = m_menuState.m_markedId;
        if (files.isEmpty())
            files = m_menuState.m_prevMarkedId;
        if (files.isEmpty())
            return;

        QString title = tr("%L1 marked").arg(files.size());

        auto *menu = new MythMenu(title, this, "pastemenu");

        menu->AddItem(tr("Move Marked Into"), &GalleryThumbView::Move);
        menu->AddItem(tr("Copy Marked Into"), qOverload<>(&GalleryThumbView::Copy));

        mainMenu->AddItem(tr("Paste"), nullptr, menu);
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
        QString title = tr("%L1 marked").arg(m_menuState.m_markedId.size());

        auto *menu = new MythMenu(title, this, "");

        menu->AddItem(tr("Rotate Marked CW"), &GalleryThumbView::RotateCWMarked);
        menu->AddItem(tr("Rotate Marked CCW"), &GalleryThumbView::RotateCCWMarked);
        menu->AddItem(tr("Flip Marked Horizontal"), &GalleryThumbView::FlipHorizontalMarked);
        menu->AddItem(tr("Flip Marked Vertical"), &GalleryThumbView::FlipVerticalMarked);
        menu->AddItem(tr("Reset Marked to Exif"), &GalleryThumbView::ResetExifMarked);

        mainMenu->AddItem(tr("Transforms"), nullptr, menu);
    }
    else if (m_menuState.m_selected->IsFile())
    {
        auto *menu = new MythMenu(m_menuState.m_selected->m_baseName, this, "");

        menu->AddItem(tr("Rotate CW"), &GalleryThumbView::RotateCW);
        menu->AddItem(tr("Rotate CCW"), &GalleryThumbView::RotateCCW);
        menu->AddItem(tr("Flip Horizontal"), &GalleryThumbView::FlipHorizontal);
        menu->AddItem(tr("Flip Vertical"), &GalleryThumbView::FlipVertical);
        menu->AddItem(tr("Reset to Exif"), &GalleryThumbView::ResetExif);

        mainMenu->AddItem(tr("Transforms"), nullptr, menu);
    }
}


/*!
 \brief Add a Action submenu
 \param mainMenu Parent menu
*/
void GalleryThumbView::MenuAction(MythMenu *mainMenu)
{
    MythMenu *menu = nullptr;
    ImagePtrK selected = m_menuState.m_selected;

    // Operate on current marked files, if any
    if (!m_menuState.m_markedId.empty())
    {
        QString title = tr("%L1 marked").arg(m_menuState.m_markedId.size());

        menu = new MythMenu(title, this, "actionmenu");

        // Only offer Hide/Unhide if relevant
        if (m_menuState.m_unhiddenMarked)
            menu->AddItem(tr("Hide Marked"), &GalleryThumbView::HideMarked);
        if (m_menuState.m_hiddenMarked)
            menu->AddItem(tr("Unhide Marked"), &GalleryThumbView::UnhideMarked);

        menu->AddItem(tr("Delete Marked"), &GalleryThumbView::DeleteMarked);
    }
    else
    {
        // Operate on selected file/dir
        menu = new MythMenu(selected->m_baseName, this, "actionmenu");

        // Prohibit actions on devices and parent dirs
        if (!selected->IsDevice() && selected != m_view->GetParent())
        {
            if (selected->m_isHidden)
                menu->AddItem(tr("Unhide"), &GalleryThumbView::Unhide);
            else
                menu->AddItem(tr("Hide"), &GalleryThumbView::HideItem);

            menu->AddItem(tr("Use as Cover"), &GalleryThumbView::SetCover);
            menu->AddItem(tr("Delete"),       &GalleryThumbView::DeleteItem);
            menu->AddItem(tr("Rename"),       &GalleryThumbView::ShowRenameInput);
        }
        else if (selected->m_userThumbnail)
        {
            menu->AddItem(tr("Reset Cover"), &GalleryThumbView::ResetCover);
        }
    }

    // Can only mkdir in a non-root dir
    if (selected->IsDirectory()
            && selected->m_id != GALLERY_DB_ID)
        menu->AddItem(tr("Create Directory"), &GalleryThumbView::MakeDir);

    // Only show import command on root, when defined
    if (selected->m_id == GALLERY_DB_ID
            && !gCoreContext->GetSetting("GalleryImportCmd").isEmpty())
        menu->AddItem(tr("Import"), &GalleryThumbView::Import);

    // Only show eject when devices (excluding import) exist
    if (selected->IsDevice() && selected->IsLocal())
        menu->AddItem(tr("Eject media"), &GalleryThumbView::Eject);

    if (menu->IsEmpty())
        delete menu;
    else
        mainMenu->AddItem(tr("Actions"), nullptr, menu);
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
    case kShuffle  : ordering = tr("Shuffled"); break;
    case kRandom   : ordering = tr("Random"); break;
    case kSeasonal : ordering = tr("Seasonal"); break;
    default:
    case kOrdered  : ordering = tr("Ordered"); break;
    }

    auto *menu = new MythMenu(tr("Slideshow") + " (" + ordering + ")",
                              this, "SlideshowMenu");

    // Use selected dir or parent, if image selected
    if (m_menuState.m_selected->IsDirectory())
    {
        if (m_menuState.m_selected->m_fileCount > 0)
            menu->AddItem(tr("Directory"), &GalleryThumbView::Slideshow);

        if (m_menuState.m_selected->m_dirCount > 0)
            menu->AddItem(tr("Recursive"), &GalleryThumbView::RecursiveSlideshow);
    }
    else
    {
        menu->AddItem(tr("Current Directory"), &GalleryThumbView::Slideshow);
    }

    auto *orderMenu = new MythMenu(tr("Slideshow Order"), this, "SlideOrderMenu");

    orderMenu->AddItem(tr("Ordered"),  nullptr, nullptr, order == kOrdered);
    orderMenu->AddItem(tr("Shuffled"), nullptr, nullptr, order == kShuffle);
    orderMenu->AddItem(tr("Random"),   nullptr, nullptr, order == kRandom);
    orderMenu->AddItem(tr("Seasonal"), nullptr, nullptr, order == kSeasonal);

    menu->AddItem(tr("Change Order"), nullptr, orderMenu);

    if (gCoreContext->GetBoolSetting("GalleryRepeat", false))
        menu->AddItem(tr("Turn Repeat Off"), &GalleryThumbView::RepeatOff);
    else
        menu->AddItem(tr("Turn Repeat On"), &GalleryThumbView::RepeatOn);

    mainMenu->AddItem(tr("Slideshow"), nullptr, menu);
}


/*!
 \brief Add a Show submenu
 \param mainMenu Parent menu
*/
void GalleryThumbView::MenuShow(MythMenu *mainMenu)
{
    auto *menu = new MythMenu(tr("Show Options"), this, "showmenu");

    int type = m_mgr.GetType();
    if (type == kPicAndVideo)
    {
        menu->AddItem(tr("Hide Pictures"), &GalleryThumbView::HidePictures);
        menu->AddItem(tr("Hide Videos"), &GalleryThumbView::HideVideos);
    }
    else
    {
        menu->AddItem(type == kPicOnly ? tr("Show Videos") : tr("Show Pictures"),
                      &GalleryThumbView::ShowType);
    }

    int show = gCoreContext->GetNumSetting("GalleryImageCaption");
    auto *captionMenu = new MythMenu(tr("Image Captions"), this,
                                     "ImageCaptionMenu");

    captionMenu->AddItem(tr("Name"),    nullptr, nullptr, show == kNameCaption);
    captionMenu->AddItem(tr("Date"),    nullptr, nullptr, show == kDateCaption);
    captionMenu->AddItem(tr("Comment"), nullptr, nullptr, show == kUserCaption);
    captionMenu->AddItem(tr("None"),    nullptr, nullptr, show == kNoCaption);

    menu->AddItem(tr("Image Captions"), nullptr, captionMenu);

    show = gCoreContext->GetNumSetting("GalleryDirCaption");
    captionMenu = new MythMenu(tr("Directory Captions"), this, "DirCaptionMenu");

    captionMenu->AddItem(tr("Name"), nullptr, nullptr, show == kNameCaption);
    captionMenu->AddItem(tr("Date"), nullptr, nullptr, show == kDateCaption);
    captionMenu->AddItem(tr("None"), nullptr, nullptr, show == kNoCaption);

    menu->AddItem(tr("Directory Captions"), nullptr, captionMenu);

    if (m_editsAllowed)
    {
        if (m_mgr.GetVisibility())
            menu->AddItem(tr("Hide Hidden Items"), &GalleryThumbView::HideHidden);
        else
            menu->AddItem(tr("Show Hidden Items"), &GalleryThumbView::ShowHidden);
    }

    if (m_zoomLevel > 0)
        menu->AddItem(tr("Zoom Out"), &GalleryThumbView::ZoomOut);
    if (m_zoomLevel < m_zoomWidgets.size() - 1)
        menu->AddItem(tr("Zoom In"), &GalleryThumbView::ZoomIn);

    QString details = m_infoList.GetState() == kNoInfo
            ? tr("Show Details") : tr("Hide Details");

    menu->AddItem(details, &GalleryThumbView::ShowDetails);

    mainMenu->AddItem(tr("Show"), nullptr, menu);
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

    auto im = item->GetData().value<ImagePtrK>();
    if (!im)
        return;

    switch (im->m_type)
    {
    case kDevice:
    case kCloneDir:
    case kDirectory:
        if (im == m_view->GetParent())
            DirSelectUp();
        else
            DirSelectDown();
        break;

    case kImageFile:
    case kVideoFile:
        StartSlideshow(kBrowseSlides); break;
    };
}


/*!
 \brief Action scan request
 \param start Start scan, if true. Otherwise stop scan
*/
void GalleryThumbView::DoScanAction(bool start)
{
    QString err = m_mgr.ScanImagesAction(start);
    if (!err.isEmpty())
        ShowOkPopup(err);
}


/*!
 \brief Start slideshow screen
 \param mode Browse, Normal or Recursive
*/
void GalleryThumbView::StartSlideshow(ImageSlideShowType mode)
{
    ImagePtrK selected = m_view->GetSelected();
    if (!selected)
        return;

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *slide = new GallerySlideView(mainStack, "galleryslideview",
                                       m_editsAllowed);
    if (slide->Create())
    {
        mainStack->AddScreen(slide);

        // Update selected item when slideshow exits
        connect(slide, &GallerySlideView::ImageSelected,
                this, &GalleryThumbView::SelectImage);

        if (selected->IsDirectory())
        {
            // Show selected dir
            slide->Start(mode, selected->m_id);
        }
        else
        {
            // Show current dir starting at selection
            slide->Start(mode, selected->m_parentId, selected->m_id);
        }
    }
    else
    {
        delete slide;
    }
}


/*!
 *  \brief Goes up one directory level
 */
bool GalleryThumbView::DirSelectUp()
{
    ImagePtrK im = m_view->GetParent();
    if (im)
    {
        LOG(VB_GUI, LOG_DEBUG, LOC +
            QString("Going up from %1").arg(im->m_filePath));

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
    ImagePtrK im = m_view->GetSelected();
    if (im)
    {
        LOG(VB_GUI, LOG_DEBUG, LOC +
            QString("Going down to %1").arg(im->m_filePath));

        // Create tree rooted at selected item
        LoadData(im->m_id);
    }
}


/*!
 \brief Mark or unmark a single item
 \param mark Mark if true, otherwise unmark
*/
void GalleryThumbView::DoMarkItem(bool mark)
{
    ImagePtrK im = m_view->GetSelected();
    if (im)
    {
        // Mark/unmark selected item
        m_view->Mark(im->m_id, mark);

        // Redisplay buttonlist as a parent dir may have been unmarked
        BuildImageList();
    }
}


/*!
 \brief Mark or unmark all items
 \param mark Mark if true, otherwise unmark
*/
void GalleryThumbView::DoMarkAll(bool mark)
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
    ImagePtrK im = m_view->GetSelected();
    if (im && m_editsAllowed)
    {
        ImageIdList ids;
        ids.append(im->m_id);
        QString err = m_mgr.ChangeOrientation(transform, ids);
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
    QString err = m_mgr.ChangeOrientation(transform, m_menuState.m_markedId);
    if (!err.isEmpty())
        ShowOkPopup(err);
}


/*!
 \brief Hide or unhide item
 \param hide Hide if true; otherwise unhide
*/
void GalleryThumbView::DoHideItem(bool hide)
{
    if (m_menuState.m_selected)
    {
        ImageIdList ids;
        ids.append(m_menuState.m_selected->m_id);

        QString err = m_mgr.HideFiles(hide, ids);
        if (!err.isEmpty())
        {
            ShowOkPopup(err);
        }
        else if (hide && !m_mgr.GetVisibility())
        {
            // Unmark invisible file
            m_view->Mark(m_menuState.m_selected->m_id, false);
        }
    }
}


/*!
 \brief Hide or unhide marked items
 \param hide Hide if true; otherwise unhide
*/
void GalleryThumbView::DoHideMarked(bool hide)
{
    QString err = m_mgr.HideFiles(hide, m_menuState.m_markedId);
    if (!err.isEmpty())
    {
        ShowOkPopup(err);
    }
    else if (hide && !m_mgr.GetVisibility())
    {
        // Unmark invisible files
        for (int id : std::as_const(m_menuState.m_markedId))
            m_view->Mark(id, false);
    }
}


/*!
 \brief Confirm user deletion of an item
*/
void GalleryThumbView::DeleteItem()
{
    if (m_menuState.m_selected)
        ShowDialog(tr("Do you want to delete\n%1 ?")
                   .arg(m_menuState.m_selected->m_baseName), "ConfirmDelete");
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
    // Show settings dialog
    auto *config = new GallerySettings(m_editsAllowed);
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *ssd = new StandardSettingDialog(mainStack, "gallerysettings", config);
    if (!ssd->Create())
    {
        delete ssd;
        return;
    }

    mainStack->AddScreen(ssd);

    // Effect setting changes when dialog saves on exit

    connect(config, &GallerySettings::ClearDbPressed,
            this,   &GalleryThumbView::ClearSgDb);

    connect(config, &GallerySettings::OrderChanged,
            this,   [this]()
    {
        // Update db view, reset cover cache & reload
        int sortIm  = gCoreContext->GetNumSetting("GalleryImageOrder");
        int sortDir = gCoreContext->GetNumSetting("GalleryDirOrder");
        m_mgr.SetSortOrder(sortIm, sortDir);
        m_view->ClearCache();
        LoadData(m_view->GetParentId());
    });

    connect(config, &GallerySettings::DateChanged,
            this,   [this]()
    {
        QString date = gCoreContext->GetSetting("GalleryDateFormat");
        m_mgr.SetDateFormat(date);
        BuildImageList();
    });

    connect(config, &GallerySettings::ExclusionsChanged,
            this,   [this]()
    {
        // Request rescan
        QString exclusions = gCoreContext->GetSetting("GalleryIgnoreFilter");
        m_view->ClearCache();
        ImageManagerFe::IgnoreDirs(exclusions);
    });
}


/*!
 \brief Show or hide hidden files
 \param show Show hidden, if true. Otherwise hide hidden
*/
void GalleryThumbView::DoShowHidden(bool show)
{
    gCoreContext->SaveBoolSetting("GalleryShowHidden", show);

    // Update Db(s)
    m_mgr.SetVisibility(show);

    // Reset dir thumbnail cache
    m_view->ClearCache();;

    LoadData(m_view->GetParentId());
}


/*!
 \brief Show a confirmation dialog
 \param msg Text to display
 \param event Event label
*/
void GalleryThumbView::ShowDialog(const QString& msg, const QString& event)
{
    auto *popup = new MythConfirmationDialog(&m_popupStack, msg, true);

    if (popup->Create())
    {
        popup->SetReturnEvent(this, event);
        m_popupStack.AddScreen(popup);
    }
    else
    {
        delete popup;
    }
}


/*!
 \brief Show dialog to allow input
*/
void GalleryThumbView::ShowRenameInput()
{
    if (m_menuState.m_selected)
    {
        QString base = QFileInfo(m_menuState.m_selected->m_baseName).completeBaseName();
        QString msg  = tr("Enter a new name:");
        auto *popup = new MythTextInputDialog(&m_popupStack, msg, FilterNone,
                                              false, base);
        if (popup->Create())
        {
            popup->SetReturnEvent(this, "FileRename");
            m_popupStack.AddScreen(popup);
        }
        else
        {
            delete popup;
        }
    }
}


/*!
 \brief Shows exif info/details about an item
 */
void GalleryThumbView::ShowDetails()
{
    m_infoList.Toggle(m_view->GetSelected());
}


/*!
 \brief Displays dialog to accept password
*/
void GalleryThumbView::ShowPassword()
{
    QString msg = tr("Enter password:");
    auto *popup = new MythTextInputDialog(&m_popupStack, msg, FilterNone, true);
    if (popup->Create())
    {
        popup->SetReturnEvent(this, "Password");
        m_popupStack.AddScreen(popup);
    }
    else
    {
        delete popup;
    }
}


/*!
 \brief Show/hide pictures or videos
*/
void GalleryThumbView::DoShowType(int type)
{
    gCoreContext->SaveSetting("GalleryShowType", type);

    // Update Db(s)
    m_mgr.SetType(type);

    // Reset dir thumbnail cache
    m_view->ClearCache();

    LoadData(m_view->GetParentId());
}


/*!
 \brief Set or reset thumbnails to use for a directory cover
 \param reset Reset cover if true, otherwise assign selected item as cover of parent
*/
void GalleryThumbView::DoSetCover(bool reset)
{
    if (m_menuState.m_selected)
    {
        QString err = reset ? m_mgr.SetCover(m_menuState.m_selected->m_id, 0)
                            : m_mgr.SetCover(m_menuState.m_selected->m_parentId,
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
    m_zoomLevel = std::max(m_zoomLevel, 0);
    if (m_zoomLevel >= m_zoomWidgets.size())
        m_zoomLevel = m_zoomWidgets.size() - 1;

    // Store any requested change, but not constraining adjustments
    // Thus, changing to a theme with fewer zoom levels will not overwrite the
    // setting
    if (change != 0)
        gCoreContext->SaveSetting("GalleryZoomLevel", m_zoomLevel);

    // dump the current list widget
    if (m_imageList)
    {
        m_imageList->SetVisible(false);
        disconnect(m_imageList, nullptr, this, nullptr);
    }

    // initialise new list widget
    m_imageList = m_zoomWidgets.at(m_zoomLevel);

    m_imageList->SetVisible(true);
    SetFocusWidget(m_imageList);

    // Monitor list actions (after focus events have been ignored)
    connect(m_imageList, &MythUIButtonList::itemClicked,
            this, &GalleryThumbView::ItemClicked);
    connect(m_imageList, &MythUIButtonList::itemSelected,
            this, &GalleryThumbView::SetUiSelection);
}


/*!
 \brief Show dialog to input new directory name
*/
void GalleryThumbView::MakeDir()
{
    auto *popup = new MythTextInputDialog(&m_popupStack,
                                          tr("Enter name of new directory"),
                                          FilterNone, false);
    if (popup->Create())
    {
        popup->SetReturnEvent(this, "MakeDir");
        m_popupStack.AddScreen(popup);
    }
    else
    {
        delete popup;
    }
}


/*!
 \brief Remove local device (or Import) from Gallery
*/
void GalleryThumbView::Eject()
{
    ImagePtrK dir = m_menuState.m_selected;
    if (dir)
        m_mgr.CloseDevices(dir->m_device, true);
}


/*!
 \brief Copy marked images to selected dir. If no marked files, use previously
 marked files. Will not overwrite/duplicate existing files on destination host.
 \details Copies files and dir subtrees and updates Db to preserve states such as
 hidden, orientation & cover.
 Attempts to copy all files and reports the number of failures (but not which ones)
 \param deleteAfter If set, source images will be deleted after being successfully
 copied.
*/
void GalleryThumbView::Copy(bool deleteAfter)
{
    // Destination must be a dir
    ImagePtrK destDir = m_menuState.m_selected;
    if (!destDir || destDir->IsFile())
        return;

    // Use current markings, if any. Otherwise use previous markings
    ImageIdList markedIds = m_menuState.m_markedId;
    if (markedIds.isEmpty())
    {
        markedIds = m_menuState.m_prevMarkedId;
        if (markedIds.isEmpty())
        {
            ShowOkPopup(tr("No files specified"));
            return;
        }
    }

    // Get all files/dirs in subtree(s). Only files are copied
    ImageList files;
    ImageList dirs;
    m_mgr.GetDescendants(markedIds, files, dirs);

    if (dirs.isEmpty() && files.isEmpty())
    {
        ShowOkPopup(tr("No images"));
        // Nothing to clean up
        return;
    }

    // Child dirs appear before their subdirs. If no dirs, images are all direct children
    ImagePtrK aChild = dirs.isEmpty() ? files[0] : dirs[0];

    // Determine parent path including trailing /
    int basePathSize = aChild->m_filePath.size() - aChild->m_baseName.size();

    // Update filepaths for Db & generate URLs for filesystem copy
    // Only copy files, destination dirs will be created automatically
    TransferThread::TransferMap transfers;
    for (const ImagePtr & im : std::as_const(files))
    {
        // Replace base path with destination path
        im->m_filePath = ImageManagerFe::ConstructPath(destDir->m_filePath,
                                             im->m_filePath.mid(basePathSize));

        transfers.insert(im, m_mgr.BuildTransferUrl(im->m_filePath,
                                                    destDir->IsLocal()));
    }

    // Create progress dialog
    MythScreenStack      *popupStack = GetMythMainWindow()->GetStack("popup stack");
    auto *progress = new MythUIProgressDialog(tr("Copying files"), popupStack,
                                              "copydialog");
    if (progress->Create())
        popupStack->AddScreen(progress, false);
    else
    {
        delete progress;
        progress = nullptr;
    }

    // Copy files in a servant thread
    TransferThread copy(transfers, false, progress);
    WaitUntilDone(copy);
    TransferThread::ImageSet failed = copy.GetResult();

    if (progress)
        progress->Close();

    if (!failed.isEmpty())
        ShowOkPopup(tr("Failed to copy %L1/%Ln file(s)", nullptr, transfers.size())
                    .arg(failed.size()));

    // Don't update Db for files that failed
    for (const ImagePtrK & im : std::as_const(failed))
        transfers.remove(im);

    ImageListK newImages = transfers.keys();

    // Include dirs
    QStringList dirPaths;
    for (const ImagePtr & im : std::as_const(dirs))
    {
        QString relPath = im->m_filePath.mid(basePathSize);

        dirPaths << relPath;

        // Replace base path with destination path
        im->m_filePath = ImageManagerFe::ConstructPath(destDir->m_filePath, relPath);

        // Append dirs so that hidden state & cover is preserved for new dirs
        // Pre-existing dirs will take precedance over these.
        newImages.append(im);
    }

    // Copy empty dirs as well (will fail for non-empty dirs)
    if (!dirPaths.isEmpty())
        m_mgr.MakeDir(destDir->m_id, dirPaths, false);

    if (!newImages.isEmpty())
    {
        // Update Db
        m_mgr.CreateImages(destDir->m_id, newImages);

        if (deleteAfter)
        {
            // Delete files/dirs that have been successfully copied
            // Will fail for dirs containing images that failed to copy
            ImageIdList ids;
            for (const ImagePtrK & im : std::as_const(newImages))
                ids << im->m_id;

            m_mgr.DeleteFiles(ids);
        }
    }
}


/*!
 \brief Move marked images to selected dir. If no marked files, use previously
 marked files. Will not overwrite/duplicate existing files on destination host.
 \details When moving between different hosts the files are copied then deleted.
 When moving on same host the files are renamed by the filesystem (which may also
 copy/delete). The Db is updated to preserve states such as hidden, orientation & cover.
 Attempts to move all files and reports the number of failures (but not which ones)
 Successful moves are unmarked, failed ones remain marked.
*/
void GalleryThumbView::Move()
{
    // Destination must be a dir
    ImagePtrK destDir = m_menuState.m_selected;
    if (!destDir || destDir->IsFile())
        return;

    // Use current markings, if any. Otherwise use previous markings
    ImageIdList markedIds = m_menuState.m_markedId;
    if (markedIds.isEmpty())
    {
        markedIds = m_menuState.m_prevMarkedId;
        if (markedIds.isEmpty())
        {
            ShowOkPopup(tr("No files specified"));
            return;
        }
    }

    // Note UI mandates that transferees are either all local or all remote
    if (destDir->IsLocal() != ImageItem::IsLocalId(markedIds[0]))
    {
        // Moves between hosts require copy/delete
        Copy(true);
        return;
    }

    // Get marked images. Each file and dir will be renamed
    ImageList files;
    ImageList dirs;
    if (m_mgr.GetImages(markedIds, files, dirs) <= 0)
    {
        ShowOkPopup(tr("No images specified"));
        // Nothing to clean up
        return;
    }
    ImageList images;
    if (!dirs.isEmpty())
        images += dirs;
    if (!files.isEmpty())
        images += files;

    // Determine parent from first dir or pic
    ImagePtr aChild = images[0];

    // Determine parent path including trailing /
    // Note UI mandates that transferees all have same parent.
    int basePathSize = aChild->m_filePath.size() - aChild->m_baseName.size();
    QString parentPath = aChild->m_filePath.left(basePathSize);

    // Determine destination URLs
    TransferThread::TransferMap transfers;
    for (const QSharedPointer<ImageItem> & im : std::as_const(images))
    {
        // Replace base path with destination path
        QString newPath = ImageManagerFe::ConstructPath(destDir->m_filePath,
                                              im->m_filePath.mid(basePathSize));

        transfers.insert(im, m_mgr.BuildTransferUrl(newPath, aChild->IsLocal()));
    }

    // Create progress dialog
    MythScreenStack      *popupStack = GetMythMainWindow()->GetStack("popup stack");
    auto *progress = new MythUIProgressDialog(tr("Moving files"), popupStack,
                                              "movedialog");

    if (progress->Create())
        popupStack->AddScreen(progress, false);
    else
    {
        delete progress;
        progress = nullptr;
    }

    // Move files in a servant thread
    TransferThread move(transfers, true, progress);
    WaitUntilDone(move);
    TransferThread::ImageSet failed = move.GetResult();

    if (progress)
        progress->Close();

    if (!failed.isEmpty())
        ShowOkPopup(tr("Failed to move %L1/%Ln file(s)", nullptr, transfers.size())
                    .arg(failed.size()));

    // Don't update Db for files that failed
    for (const ImagePtrK & im : std::as_const(failed))
        transfers.remove(im);

    if (!transfers.isEmpty())
    {
        ImageListK moved = transfers.keys();

        // Unmark moved files
        for (const ImagePtrK & im : std::as_const(moved))
            m_view->Mark(im->m_id, false);

        // Update Db
        m_mgr.MoveDbImages(destDir, moved, parentPath);
    }
}


/*!
 \brief Executes user 'Import command'
*/
void GalleryThumbView::Import()
{
    QString path = m_mgr.CreateImport();
    if (path.isEmpty())
    {
        ShowOkPopup(tr("Failed to create temporary directory."));
        return;
    }

    // Replace placeholder in command
    QString cmd = gCoreContext->GetSetting("GalleryImportCmd");
    cmd.replace("%TMPDIR%", path);

    // Run command in a separate thread
    MythUIBusyDialog *busy =
            ShowBusyPopup(tr("Running Import command.\nPlease wait..."));

    ShellThread thread(cmd, path);
    WaitUntilDone(thread);

    if (busy)
        busy->Close();

    int error = thread.GetResult();
    if (error != 0)
        ShowOkPopup(tr("Import command failed.\nError: %1").arg(error));

    // Rescan local devices
    QString err = m_mgr.ScanImagesAction(true, true);
    if (!err.isEmpty())
        LOG(VB_GENERAL, LOG_ERR, LOC + err);
}

void GalleryThumbView::DoRepeat(int on)
{
    gCoreContext->SaveSetting("GalleryRepeat", on);
}
