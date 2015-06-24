#include "galleryviews.h"

#include <cmath> // for qsrand

#include <QTime>

#include "gallerycommhelper.h"


/*!
 \brief Number of thumbnails to use for folders
*/
const static int   kMaxFolderThumbnails = 4;

/*!
 \brief Exif data with these titles comprise the Basic file info
*/
static QStringList kBasicInfoFields = QStringList()
                                      // Exif tags
                                      << "Date and Time"
                                      << "Exposure Time"
                                      << "FNumber"
                                      << "Focal Length"
                                      << "Image Description"
                                      << "User Comment"
                                      << "Manufacturer"
                                      << "Max Aperture Value"
                                      << "Model"
                                      << "Orientation"
                                      << "Pixel X Dimension"
                                      << "Pixel Y Dimension"
                                      // Video tags
                                      << "model"
                                      << "encoder";

/*!
 \brief 
 Tuning parameter for seasonal weights, between 0 and 1, where lower numbers
 give greater weight to seasonal photos. The leading beta shape controls
 dates that are approaching and the trailing beta shape controls dates that
 just passed. When these are set to 0.175 and 0.31, respectively, about one
 quarter of the photos are from the upcoming week in prior years and about one
 quarter of the photos are from the preceding month in prior years.
*/
const double LEADING_BETA_SHAPE  = 0.175;
/*!
 \brief See LEADING_BETA_SHAPE
*/
const double TRAILING_BETA_SHAPE = 0.31;
/*!
 \brief Photos without an exif timestamp will default to the mode of the beta distribution.
*/
const double DEFAULT_WEIGHT = std::pow(0.5, TRAILING_BETA_SHAPE - 1) *
                              std::pow(0.5, LEADING_BETA_SHAPE - 1);
/*!
 \brief The edges of the distribution get clipped to avoid a singularity.
*/
const qint64 BETA_CLIP = 60 * 60 * 24;

/*!
 \brief Reset menu state
*/
void MenuSubjects::Clear()
{
    m_selected       = NULL;
    m_selectedMarked = false;
    m_childCount     = 0;
    m_markedId.clear();
    m_prevMarkedId.clear();
    m_hiddenMarked   = false;
    m_unhiddenMarked = false;
}


/*!
 \brief Factory for initialising an info/details buttonlist

 \param container The Screen widget
 \param focusable Set if info list should be focusable (for scrolling)
 \param undefined Text to display for tags with no value
 \return InfoList The info buttonlist
*/
InfoList* InfoList::Create(MythScreenType *container,
                           bool focusable,
                           QString undefined)
{
    MythUIButtonList *btnList;
    bool              err = false;
    UIUtilE::Assign(container, btnList, "infolist", &err);
    if (err)
        return NULL;

    return new InfoList(container, focusable, undefined, btnList);
}


/*!
 \brief Info/details buttonlist constructor

 \param container The Screen widget
 \param focusable Set if info list should be focusable (for scrolling)
 \param undefined Text to display for tags with no value
 \param btnList Buttonlist provided by the theme
*/
InfoList::InfoList(MythScreenType *container, bool focusable, QString undefined,
                   MythUIButtonList *btnList)
    : m_screen(container),
    m_btnList(btnList),
    m_infoVisible(kNoInfo),
    m_undefinedText(undefined)
{
    m_btnList->SetVisible(false);
    m_btnList->SetCanTakeFocus(focusable);
}


/*!
 \brief Toggle infolist state for an image. Focusable widgets toggle between
 Basic & Full info. Non-focusable widgets toggle between Basic & Off.
 \param im The image/dir for which info is shown
*/
void InfoList::Toggle(const ImageItem *im)
{
    // Only focusable lists have an extra 'full' state as they can
    // be scrolled to view it all
    if (m_btnList->CanTakeFocus())

        // Start showing basic info then toggle between basic/full
        m_infoVisible = m_infoVisible == kBasicInfo ? kFullInfo : kBasicInfo;

    // Toggle between off/basic
    else if (m_infoVisible == kBasicInfo)
    {
        m_infoVisible = kNoInfo;
        m_btnList->SetVisible(false);
        return;
    }
    else
        m_infoVisible = kBasicInfo;

    Update(im);

    m_btnList->SetVisible(true);
}


/*!
 \brief Remove infolist from display
 \return bool True if buttonlist was displayed/removed
*/
bool InfoList::Hide()
{
    // Only handle event if info currently displayed
    bool handled = (m_infoVisible != kNoInfo);
    m_infoVisible = kNoInfo;

    m_btnList->SetVisible(false);

    return handled;
}


/*!
 \brief Populate a buttonlist item with exif tag name & value
 \param name Exif tag name
 \param value Exif tag value
*/
void InfoList::CreateButton(QString name, QString value)
{
    MythUIButtonListItem *item = new MythUIButtonListItem(m_btnList, "");

    InfoMap               infoMap;
    infoMap.insert("name", name);
    infoMap.insert("value", value.isEmpty() ? m_undefinedText : value);

    item->SetTextFromMap(infoMap);
}


/*!
 \brief Populates available exif details for the current image/dir.
 \param im An image or dir
*/
void InfoList::Update(const ImageItem *im)
{
    if (!im || m_infoVisible == kNoInfo)
        return;

    // Clear info
    m_btnList->Reset();

    if (im->IsFile())
    {
        // File stats
        CreateButton("Filename", im->m_name);
        CreateButton("File Modified",
                     QDateTime::fromTime_t(im->m_modTime).toString());
        CreateButton("File size", QString("%1 Kb").arg(im->m_size / 1024));
        CreateButton("Path", im->m_path);

        NameMap tags = GalleryBERequest::GetMetaData(im);

        // Now go through the info list and create a map for the buttonlist
        NameMap::const_iterator i = tags.constBegin();
        while (i != tags.constEnd())
        {
            if (m_infoVisible == kFullInfo || kBasicInfoFields.contains(i.key()))
                CreateButton(i.key(), i.value());
            ++i;
        }

        // Only give list focus if requested
        if (m_btnList->CanTakeFocus())
            m_screen->SetFocusWidget(m_btnList);
    }
    else if (im->m_id == ROOT_DB_ID)
    {
        // Gallery stats
        CreateButton("Last scan", QDateTime::fromTime_t(
                         im->m_modTime).toString());
        CreateButton("Contains", QString("%1 Images, %2 Directories")
                     .arg(im->m_fileCount).arg(im->m_dirCount));
    }
    else // dir
    {
        // File stats
        CreateButton("Dirname", im->m_name);
        CreateButton("Dir Modified",
                     QDateTime::fromTime_t(im->m_modTime).toString());
        CreateButton("Path", im->m_path);
        CreateButton("Contains", QString("%1 Images, %2 Directories")
                     .arg(im->m_fileCount).arg(im->m_dirCount));
    }
}


/*!
 \brief Get current displayed state of buttonlist
 \return InfoVisibleState
*/
InfoVisibleState InfoList::GetState()
{
    return m_infoVisible;
}


/*!
 \brief Get all images/dirs in view
 \return ImageList List of images/dirs
*/
ImageList FlatView::GetAllNodes() const
{
    ImageList files;
    foreach (ImageRef ptr, m_sequence)
        files.append(*ptr);
    return files;
}


/*!
 \brief Get current selection
 \return ImageItem* An image or NULL
*/
ImageItem* FlatView::GetSelected() const
{
    return m_active < 0 || m_active >= m_sequence.size()
           ? NULL : *m_sequence[m_active];
}


/*!
 \brief Get positional status
 \return QString "m/n" where m is selected index, n is total count in view
*/
QString FlatView::GetPosition() const
{
    return QString("%1/%2").arg(m_active + 1).arg(m_sequence.size());
}


/*!
 \brief Peeks at next image in view but does not advance iterator
 \return ImageItem The next image or NULL
*/
ImageItem* FlatView::HasNext() const
{
    return m_sequence.isEmpty() || m_active >= m_sequence.size() - 1
           ? NULL : *m_sequence[m_active + 1];
}


/*!
 \brief Peeks at previous image in view but does not decrement iterator
 \return ImageItem The previous image or NULL
*/
ImageItem* FlatView::HasPrev() const
{
    return m_sequence.isEmpty() || m_active <= 0
           ? NULL : *m_sequence[m_active - 1];
}


/*!
 \brief Updates view with images that have been updated.
 \param idList List of image ids that have been updated
 \return bool True if the current selection has been updated
*/
bool FlatView::Update(int id)
{
    bool activeUpdated = false;

    // Replace old version of image
    for (int j=0; j < m_images.size(); ++j)
    {
        if (m_images.at(j)->m_id == id)
        {
            // Get updated image
            ImageList files;
            m_db->ReadDbFilesById(files, QString::number(id), false);

            if (files.size() == 1)
            {
                activeUpdated = GetSelected() == m_images.at(j);

                // Replace image
                delete m_images.at(j);
                m_images.replace(j, files.at(0));

                LOG(VB_FILE, LOG_DEBUG, QString("Views: Modified id %1 @ index %2")
                    .arg(id).arg(j));
            }
            else
                qDeleteAll(files);

            break;
        }
    }
    return activeUpdated;
}


/*!
 \brief Select image
 \param id Image id
 \param fallback View index to select if image is not in view. Defaults to first
 image. If negative then current selection is not changed if the image is not found
 \return bool True if the image was found
*/
bool FlatView::Select(int id, int fallback)
{
    // Select first appearance of image
    for (int i = 0; i < m_sequence.size(); ++i)
    {
        ImageItem *im = *m_sequence.at(i);
        if (im->m_id == id)
        {
            m_active = i;
            return true;
        }
    }

    if (fallback >= 0)
        m_active = fallback;

    return false;
}


/*!
 \brief Reset view
 \param resetParent parent id is only reset to root when this is set
*/
void FlatView::Clear(bool resetParent)
{
    if (!m_images.isEmpty())
        qDeleteAll(m_images);
    m_images.clear();
    m_sequence.clear();
    m_active = -1;
    if (resetParent)
        m_parentId = ROOT_DB_ID;
}


/*!
 \brief Advance iterator and return next image. Wraps at end and regenerates
 view order on wrap, if necessary
 \return ImageItem Next image or NULL if empty
*/
ImageItem *FlatView::Next()
{
    if (m_sequence.isEmpty())
        return NULL;

    // wrap at end
    if (m_active >= m_sequence.size() - 1)
    {
        if (m_order == kOrdered)
            m_active = -1;
        // Regenerate unordered views on every repeat
        else if (!LoadFromDb())
            // Images have disappeared
            return NULL;
    }

    return *m_sequence[++m_active];
}


/*!
 \brief Decrements iterator and returns previous image. Wraps at start.
 \return ImageItem Previous image or NULL if empty
*/
ImageItem *FlatView::Prev()
{
    if (m_sequence.isEmpty())
        return NULL;

    if (m_active <= 0)
        m_active = m_sequence.size();

    return *m_sequence[--m_active];
}


/*!
 \brief Fills view with Db images, re-ordering them as required
 \param files List of images from database
*/
void FlatView::Populate(ImageList &files)
{
    // Do not reset parent
    Clear(false);

    if (files.isEmpty())
        return;

    // Store available images for view
    m_images = files;

    if (m_images.size() == 1 || m_order == kOrdered || m_order == kShuffle)
    {
        // Default sequence is ordered
        for (int i = 0; i < m_images.size(); ++i)
            m_sequence.append(&m_images.at(i));
    }

    if (m_images.size() > 1)
    {
        // Modify viewing sequence
        if (m_order == kShuffle)
        {
            std::random_shuffle(m_sequence.begin(), m_sequence.end());
        }
        else if (m_order == kRandom)
        {
            qsrand(QTime::currentTime().msec());
            int range = m_images.size() - 1;
            int rand, slot = range;
            for (int i = 0; i < m_images.size(); ++i)
            {
                rand = qrand() % range;
                // Avoid consecutive repeats
                slot = (rand < slot) ? rand : rand + 1;
                m_sequence.append(&m_images.at(slot));
//                LOG(VB_FILE, LOG_DEBUG, QString("Rand %1").arg(slot));
            }
        }
        else if (m_order == kSeasonal)
        {
            WeightList weights   = CalculateSeasonalWeights(m_images);
            double     maxWeight = weights.last();

            qsrand(QTime::currentTime().msec());
            for (int i = 0; i < m_images.size(); ++i)
            {
                double               randWeight = qrand() * maxWeight / RAND_MAX;
                WeightList::iterator it =
                        std::upper_bound(weights.begin(), weights.end(), randWeight);
                int                  slot = std::distance(weights.begin(), it);
                m_sequence.append(&m_images.at(slot));
            }
        }
    }
}


/*!
 \brief 
 * This method calculates a weight for the item based on how closely it was
 * taken to the current time of year. This means that New Year's pictures will
 * be displayed very frequently on every New Year's, and that anniversary
 * pictures will be favored again every anniversary. The weights are chosen
 * using a beta distribution with a tunable shape parameter.
 \param files List of database images
 \return WeightList Corresponding list of weightings
*/
WeightList FlatView::CalculateSeasonalWeights(ImageList &files)
{
    WeightList weights(files.size());
    double     totalWeight = 0;
    QDateTime  now         = QDateTime::currentDateTime();

    for (int i = 0; i < files.size(); ++i)
    {
        ImageItem *im      = files.at(i);
        double         weight;

        if (im->m_date == 0)
            weight = DEFAULT_WEIGHT;
        else
        {
            QDateTime      timestamp = QDateTime::fromTime_t(im->m_date);
            QDateTime curYearAnniversary =
                QDateTime(QDate(now.date().year(),
                                timestamp.date().month(),
                                timestamp.date().day()),
                          timestamp.time());

            bool      isAnniversaryPast = curYearAnniversary < now;

            QDateTime adjacentYearAnniversary =
                QDateTime(QDate(now.date().year() +
                                (isAnniversaryPast ? 1 : -1),
                                timestamp.date().month(),
                                timestamp.date().day()),
                          timestamp.time());

            double range = std::abs(
                curYearAnniversary.secsTo(adjacentYearAnniversary)) + BETA_CLIP;

            // This calculation is not normalized, because that would require the
            // beta function, which isn't part of the C++98 libraries. Weights
            // that aren't normalized work just as well relative to each other.
            weight = std::pow(abs(now.secsTo(
                                      isAnniversaryPast ? curYearAnniversary
                                      : adjacentYearAnniversary
                                      ) + BETA_CLIP) / range,
                              TRAILING_BETA_SHAPE - 1) *
                     std::pow(abs(now.secsTo(
                                      isAnniversaryPast ?
                                      adjacentYearAnniversary :
                                      curYearAnniversary
                                      ) + BETA_CLIP) / range,
                              LEADING_BETA_SHAPE - 1);
        }
        totalWeight += weight;
        weights[i]   = totalWeight;
    }
    return weights;
}


/*!
 \brief Populate view with database images from a directory
 \param parentId The dir id, if positive. Otherwise the view is refreshed using the
 existing parent dir
 \return bool True if resulting view is not empty
*/
bool FlatView::LoadFromDb(int parentId)
{
    if (parentId > 0)
        m_parentId = parentId;

    // Load child images of the parent
    ImageList files;
    m_db->ReadDbChildFiles(files, QString::number(m_parentId), false, true);

    // Load gallery datastore with current dir
    Populate(files);

    return !files.isEmpty();
}


/*!
 \brief Rotate view so that starting image is at front.
 \param id The image to be positioned
 \param offset Distance the image will be from element 0
*/
void FlatView::Rotate(int id, int offset)
{
    // Rotate sequence so that (first appearance of) specified image is
    // at offset from front
    for (int i = 0; i < m_sequence.size(); ++i)
    {
        ImageItem *im = *m_sequence.at(i);
        if (im->m_id == id)
        {
            int first = (i + offset) % m_sequence.size();
            if (first > 0)
                m_sequence = m_sequence.mid(first) + m_sequence.mid(0, first);
            break;
        }
    }
}


/*!
 \brief Constructs a view of images & directories that can be marked
 \param order Ordering to use for view
*/
DirectoryView::DirectoryView(SlideOrderType order, ImageDbReader *db)
    : FlatView(order, db)
{
    m_marked.Reset(ROOT_DB_ID);
    m_prevMarked.Reset();
}


/*!
 \brief Get all view items except the first (the parent dir)
 \return ImageIdList List of images/dirs
*/
ImageIdList DirectoryView::GetChildren() const
{
    ImageIdList children;
    for (int i = 1; i < m_sequence.size(); ++i)
    {
        ImageItem *im = *m_sequence.at(i);
        children.append(im->m_id);
    }
    return children;
}


/*!
 \brief Populate view from database as images/subdirs of a directory.
View is ordered: Parent dir, sub-dirs, images. Dir thumbnails are derived from their
subtree.
 \param parentId The dir id, if positive. Otherwise the view is refreshed using the
 existing parent dir
 \return bool True if resulting view is not empty
*/
bool DirectoryView::LoadFromDb(int parentId)
{
    // Invalid id signifies a refresh of the current view
    if (parentId > 0)
        SetDirectory(parentId);
    else
        parentId = m_parentId;

    // Load visible parent or root
    ImageItem *parent = GetDbParent(parentId);
    if (!parent)
    {
        Clear();
        return false;
    }

    m_parentId = parent->m_id;

    // Preserve current selection
    ImageItem *selected = GetSelected();
    int activeId = selected ? selected->m_id : 0;

    // Load db data for this parent dir
    ImageList dirs, images;
    GetDbTree(images, dirs, parent);

    // Construct view as:
    //  Parent kUpDirectory at start for navigating up.
    //  Ordered dirs
    //  Ordered files
    ImageList files = dirs + images;
    files.prepend(parent);

    Populate(files);

    // Reinstate selection, falling back to parent
    Select(activeId);

    return true;
}


/*!
 \brief Resets view
 \param resetParent parent id is only reset to root when this is set
*/
void DirectoryView::Clear(bool resetParent)
{
    ClearMarked();
    FlatView::Clear(resetParent);
}


/*!
 \brief Mark all images/dirs
*/
void DirectoryView::MarkAll()
{
    // Any marking clears previous marks
    m_prevMarked.Reset();

    m_marked.Add(GetChildren());
}


/*!
 \brief Mark/unmark an image/dir
 \param id The image/dir
 \param mark If true, mark item. Otherwise unmark
*/
void DirectoryView::Mark(int id, bool mark)
{
    if (mark)
    {
        // Any marking clears previous marks
        m_prevMarked.Reset();
        m_marked.Add(id);
    }
    else
        m_marked.Remove(id);
}


/*!
 \brief Mark all unmarked items, unmark all marked items
*/
void DirectoryView::InvertMarked()
{
    // Any marking clears previous marks
    m_prevMarked.Reset();

    m_marked.Invert(GetChildren());
}


/*!
 \brief Unmark all items
*/
void DirectoryView::ClearMarked()
{
    m_marked.Reset();
    m_prevMarked.Reset();
}


/*!
 \brief Manage markings on tree navigation
 \param newParent Id of new parent dir
*/
void DirectoryView::SetDirectory(int newParent)
{
    // Markings are cleared on every dir change
    // Any current markings become previous markingsConfirmDeleteSelected
    // Only 1 set of previous markings are preserved
    if (m_prevMarked.IsFor(newParent))
    {
        // Returned to dir of previous markings: reinstate them
        m_marked = m_prevMarked;
        m_prevMarked.Reset();
        return;
    }

    if (!m_marked.IsEmpty())
        // Preserve current markings
        m_prevMarked = m_marked;

    // Initialise current markings for new dir
    m_marked.Reset(newParent);
}


/*!
 \brief Determine current selection, markings & various info to support menu display
 \return MenuSubjects Current state
*/
MenuSubjects DirectoryView::GetMenuSubjects()
{
    MenuSubjects state;
    state.Clear();

    state.m_selected       = GetSelected();
    state.m_selectedMarked = state.m_selected
                             && m_marked.Contains(state.m_selected->m_id);

    // At least one set will be empty
    state.m_markedId     = m_marked.GetIds();
    state.m_prevMarkedId = m_prevMarked.GetIds();

    state.m_childCount   = m_sequence.size() - 1;

    // Only inspect children
    for (int i = 1; i < m_sequence.size(); ++i)
    {
        ImageItem *im = *m_sequence.at(i);

        if (m_marked.Contains(im->m_id))
        {
            state.m_hiddenMarked   = state.m_hiddenMarked || im->m_isHidden;
            state.m_unhiddenMarked = state.m_unhiddenMarked || !im->m_isHidden;
        }
    }
    return state;
}


/*!
 \brief Get parent dir from database
 \param parentId Id of dir
 \return ImageItem Dir image info
*/
ImageItem *DirectoryView::GetDbParent(int parentId) const
{
    ImageList dirs;
    m_db->ReadDbDirsById(dirs, QString::number(parentId), false, false);

    if (dirs.isEmpty())
    {
        // Fallback to top level if parent dir no longer exists
        // If no root then Db is empty
        return parentId == ROOT_DB_ID ? NULL : GetDbParent(ROOT_DB_ID);
    }

    // Override parent type
    if (dirs[0]->m_id != ROOT_DB_ID)
        dirs[0]->m_type = kUpDirectory;
    return dirs[0];
}


/*!
 \brief Get all dirs & images from database, populate stats and determine thumbnails
 for parent & sub-dirs as follows: Use user cover, if assigned. Otherwise derive
4 thumbnails from: first 4 images, then 1st thumbnail from first 4 sub-dirs,
then 2nd thumbnail from sub-dirs etc
 \param files List to fill with images
 \param dirs List to fill with sub-dirs
 \param parent Parent dir
*/
void DirectoryView::GetDbTree(ImageList &files,
                              ImageList &dirs,
                              ImageItem *parent) const
{
    if (!parent)
        return;

    // Load child images & dirs and set parent stats
    parent->m_fileCount = m_db->ReadDbChildFiles(
        files, QString::number(parent->m_id), false, true);
    parent->m_dirCount = m_db->ReadDbChildDirs(
        dirs, QString::number(parent->m_id), false, true);

    const ImageItem *userIm = NULL;

    // Load thumbs for child dirs
    foreach (ImageItem *im, dirs)
    {
        // Load sufficient thumbs from each dir as subsequent dirs may be empty
        GetDbSubTree(im, kMaxFolderThumbnails);

        // Locate user thumbnail amongst dirs
        if (!userIm && im->m_id == parent->m_userThumbnail)
            userIm = im;
    }

    // Try user assigned thumb, if defined
    if (parent->m_userThumbnail != 0)
    {
        if (!userIm)
            // Locate user thumb amongst images
            foreach (ImageItem *im, files)
            {
                if (im->m_id == parent->m_userThumbnail)
                {
                    userIm = im;
                    break;
                }
            }

        if (userIm)
        {
            // Using user assigned thumbnail
            parent->m_thumbIds   = userIm->m_thumbIds;
            parent->m_thumbNails = userIm->m_thumbNails;
            return;
        }
    }

    // Fill parent thumbs from images
    foreach (const ImageItem *im, files.mid(0, kMaxFolderThumbnails))
    {
        parent->m_thumbIds.append(im->m_id);
        parent->m_thumbNails.append(im->m_thumbNails.at(0));
    }

    // If sufficient images, we're done
    int thumbsNeeded = kMaxFolderThumbnails - files.size();
    if (thumbsNeeded <= 0)
        return;

    // Resort to dirs. Use 1st thumb from each dir, then 2nd from each, etc.
    for (int i = 0; i < kMaxFolderThumbnails; ++i)
        foreach (const ImageItem *im, dirs)
        {
            if (i < im->m_thumbIds.size())
            {
                parent->m_thumbIds.append(im->m_thumbIds.at(i));
                parent->m_thumbNails.append(im->m_thumbNails.at(i));
                if (--thumbsNeeded == 0)
                    // Found enough
                    return;
            }
        }
}


/*!
 \brief Populate directory stats & thumbnails recursively from database as follows:
Use user cover, if assigned. Otherwise derive 4 thumbnails from: first 4 images,
then 1st thumbnail from first 4 sub-dirs, then 2nd thumbnail from sub-dirs etc
 \param parent The parent dir
 \param limit Number of thumbnails required
 \param level Recursion level (to detect recursion deadlocks)
*/
void DirectoryView::GetDbSubTree(ImageItem *parent, int limit, int level) const
{
    // Load child images & dirs
    ImageList files, dirs;
    int       imageCount = m_db->ReadDbChildFiles(
                files, QString::number(parent->m_id), false, true);
    int       dirCount = m_db->ReadDbChildDirs(
                dirs, QString::number(parent->m_id), false, true);

    // Set parent stats
    parent->m_fileCount = imageCount;
    parent->m_dirCount  = dirCount;

    // Try user assigned thumb, if defined
    if (parent->m_userThumbnail != 0)
    {
        // Locate user thumb amongst images
        bool found = false;
        for (int i = 0; i < files.size(); ++i)
            if (files.at(i)->m_id == parent->m_userThumbnail)
            {
                // Only need this image so move it to front
                found = true;
                limit = 1;
                if (i != 0)
                    files.move(i, 0);
                break;
            }

        if (!found)
        {
            // Locate user thumb amongst dirs
            foreach (const ImageItem *im, dirs)
            {
                if (im->m_id == parent->m_userThumbnail)
                {
                    // Discard all children & use this dir only
                    ImageItem *tmp = new ImageItem(*im);
                    qDeleteAll(files);
                    files.clear();
                    qDeleteAll(dirs);
                    dirs.clear();
                    dirs.append(tmp);
                    break;
                }
            }
        }

    }

    // Fill parent thumbs from images
    foreach (const ImageItem *im, files)
    {
        parent->m_thumbIds.append(im->m_id);
        parent->m_thumbNails.append(im->m_thumbNails.at(0));
        if (--limit == 0)
            break;
    }

    if (limit > 0)
    {
        // Prevent lengthy/infinite recursion due to deep/cyclic folder
        // structures
        if (++level > 10)
            LOG(VB_GENERAL, LOG_NOTICE,
                "Directory thumbnails are more than 10 levels deep");
        else
        {
            // Recursively load subdir thumbs to try to get 1 thumb from each
            foreach (ImageItem *im, dirs)
            {
                // Load sufficient thumbs from each dir as subsequent dirs may
                // be empty
                GetDbSubTree(im, limit, level);

                if (im->m_thumbIds.size() > 0)
                {
                    // Add first thumbnail to parent thumb
                    parent->m_thumbIds.append(im->m_thumbIds.at(0));
                    parent->m_thumbNails.append(im->m_thumbNails.at(0));

                    // Quit when we have sufficient thumbs
                    if (--limit == 0)
                        break;
                }
            }

            if (limit > 0)
                // Insufficient dirs to supply 1 thumb per dir so use other dir
                // thumbs as well
                for (int i = 1; i < kMaxFolderThumbnails; ++i)
                    foreach (const ImageItem *im, dirs)
                    {
                        if (i < im->m_thumbIds.size())
                        {
                            parent->m_thumbIds.append(im->m_thumbIds.at(i));
                            parent->m_thumbNails.append(im->m_thumbNails.at(i));
                            if (--limit == 0)
                                break;
                        }
                    }
        }
    }
    qDeleteAll(files);
    qDeleteAll(dirs);
}


/*!
 \brief Populate view from database as images of a directory sub-tree. Default
order of a tree is depth-first traversal
 \param parentId The dir id, if valid. Otherwise the view is refreshed using the
 existing parent dir
 \return bool True if resulting view is not empty
*/
bool TreeView::LoadFromDb(int parentId)
{
    if (parentId > 0)
        m_parentId = parentId;

    // Load visible subtree of the parent
    // Ordered images of parent first, then depth-first recursion of ordered dirs
    ImageList  files, dirs;
    QStringList root = QStringList() << QString::number(m_parentId);
    m_db->ReadDbTree(files, dirs, root, false, true);

    // Load view
    Populate(files);

    // Discard dirs
    qDeleteAll(dirs);

    return !files.isEmpty();
}
