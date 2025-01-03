#include "galleryviews.h"

#include <algorithm> // std::shuffle, upper_bound
#include <cmath>     // std::pow
#include <cstdint>
#include <iterator>  // std::distance
#include <random>
#include <vector>

#include "libmythbase/mythrandom.h"

#define LOC QString("Galleryviews: ")

//! Number of thumbnails to use for folders
const static int   kMaxFolderThumbnails = 4;

//! Tuning parameter for seasonal weights, between 0 and 1, where lower numbers
//! give greater weight to seasonal photos. The leading beta shape controls
//! dates that are approaching and the trailing beta shape controls dates that
//! just passed. When these are set to 0.175 and 0.31, respectively, about one
//! quarter of the photos are from the upcoming week in prior years and about one
//! quarter of the photos are from the preceding month in prior years.
const double LEADING_BETA_SHAPE  = 0.175;
//! See LEADING_BETA_SHAPE
const double TRAILING_BETA_SHAPE = 0.31;

//! Photos without an exif timestamp will default to the mode of the beta distribution.
const double DEFAULT_WEIGHT = std::pow(0.5, TRAILING_BETA_SHAPE - 1) *
                              std::pow(0.5, LEADING_BETA_SHAPE - 1);
//! The edges of the distribution get clipped to avoid a singularity.
static constexpr qint64 BETA_CLIP { 24LL * 60 * 60 };

void MarkedFiles::Add(const ImageIdList& newIds)
{
    for (int newid : newIds)
        insert(newid);
}

void MarkedFiles::Invert(const ImageIdList &all)
{
    QSet tmp;
    for (int tmpint : all)
        tmp.insert(tmpint);
    for (int tmpint : std::as_const(*this))
        tmp.remove(tmpint);
    swap(tmp);
}

/*!
 \brief Get all images/dirs in view
 \return ImageList List of images/dirs
*/
ImageListK FlatView::GetAllNodes() const
{
    ImageListK files;
    for (int id : std::as_const(m_sequence))
        files.append(m_images.value(id));
    return files;
}


/*!
 \brief Get current selection
 \return ImagePtr An image or nullptr
*/
ImagePtrK FlatView::GetSelected() const
{
    return m_active < 0 || m_active >= m_sequence.size()
            ? ImagePtrK() : m_images.value(m_sequence.at(m_active));
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
 \brief Updates view with images that have been updated.
 \param id Image id to update
 \return bool True if the current selection has been updated
*/
bool FlatView::Update(int id)
{
    ImagePtrK im = m_images.value(id);
    if (!im)
        return false;

    // Get updated image
    ImageList files;
    ImageList dirs;
    ImageIdList ids = ImageIdList() << id;
    if (m_mgr.GetImages(ids, files, dirs) != 1 || files.size() != 1)
        return false;

    bool active = (im == GetSelected());

    // Replace image
    m_images.insert(id, files.at(0));

    LOG(VB_FILE, LOG_DEBUG, LOC + QString("Modified id %1").arg(id));

    return active;
}


/*!
 \brief Selects first occurrence of an image
 \param id Image id
 \param fallback View index to select if image is not in view. Defaults to first
 image. If negative then current selection is not changed if the image is not found
 \return bool True if the image was found
*/
bool FlatView::Select(int id, int fallback)
{
    // Select first appearance of image
    int index = m_sequence.indexOf(id);
    if (index >= 0)
    {
        m_active = index;
        return true;
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
    m_images.clear();
    m_sequence.clear();
    m_active = -1;
    if (resetParent)
        m_parentId = GALLERY_DB_ID;
}


/*!
 \brief Peeks at next image in view but does not advance iterator
 \return ImageItem The next image or nullptr
*/
ImagePtrK FlatView::HasNext(int inc) const
{
    return m_sequence.isEmpty() || m_active + inc >= m_sequence.size()
            ? ImagePtrK() : m_images.value(m_sequence.at(m_active + inc));
}


/*!
 \brief Advance iterator and return next image, wrapping if necessary.
 Regenerates unordered views on wrap.
 \return ImageItem Next image or nullptr if empty
*/
ImagePtrK FlatView::Next(int inc)
{
    if (m_sequence.isEmpty())
        return {};

    // Preserve index as it may be reset when wrapping
    int next = m_active + inc;

    // Regenerate unordered views when wrapping
    if (next >= m_sequence.size() && m_order != kOrdered && !LoadFromDb(m_parentId))
        // Images have disappeared
        return {};

    m_active = next % m_sequence.size();
    return m_images.value(m_sequence.at(m_active));
}


/*!
 \brief Peeks at previous image in view but does not decrement iterator
 \return ImageItem The previous image or nullptr
*/
ImagePtrK FlatView::HasPrev(int inc) const
{
    return m_sequence.isEmpty() || m_active < inc
            ? ImagePtrK() : m_images.value(m_sequence.at(m_active - inc));
}


/*!
 \brief Decrements iterator and returns previous image. Wraps at start.
 \return ImageItem Previous image or nullptr if empty
*/
ImagePtrK FlatView::Prev(int inc)
{
    if (m_sequence.isEmpty())
        return {};

    // Wrap avoiding modulo of negative uncertainty
    m_active -= inc % m_sequence.size();
    if (m_active < 0)
        m_active += m_sequence.size();

    return m_images.value(m_sequence.at(m_active));
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

    for (const QSharedPointer<ImageItem> & im : std::as_const(files))
    {
        // Add image to view
        m_images.insert(im->m_id, im);

        // Cache all displayed images
        if (im->IsFile())
            Cache(im->m_id, im->m_parentId, im->m_url, im->m_thumbNails.at(0).second);
    }

    if (files.size() == 1 || m_order == kOrdered || m_order == kShuffle)
    {
        // Default sequence is ordered
        for (const QSharedPointer<ImageItem> & im : std::as_const(files))
            m_sequence.append(im->m_id);
    }

    if (files.size() > 1)
    {
        // Modify viewing sequence
        if (m_order == kShuffle)
        {
            std::shuffle(m_sequence.begin(), m_sequence.end(),
                         std::mt19937(std::random_device()()));
        }
        else if (m_order == kRandom)
        {
            // An image is not a valid candidate for its successor
            // add files.size() elements from files in a random order
            // to m_sequence allowing non-consecutive repetition
            int size  = files.size();
            int range = files.size() - 1;
            int last  = size; // outside of the random interval [0, size)
            int count = 0;
            while (count < size)
            {
                int rand = MythRandom(0, range);

                // Avoid consecutive repeats
                if (last == rand)
                {
                    continue;
                }
                last = rand;
                m_sequence.append(files.at(rand)->m_id);
                count++;
            }
        }
        else if (m_order == kSeasonal)
        {
            WeightList cdf = CalculateSeasonalWeights(files); // not normalized to 1.0
            std::vector<uint32_t> weights;
            weights.reserve(cdf.size());
            for (int i = 0; i < cdf.size(); i++)
            {
                weights.emplace_back(lround(cdf[i] / cdf.back() * UINT32_MAX));
            }
            // exclude the last value so the past the end iterator is not returned
            // by std::upper_bound
            if (!weights.empty())
            {
                uint32_t maxWeight = weights.back() - 1;

                for (int count = 0; count < files.size(); ++count)
                {
                    uint32_t randWeight = MythRandom(0, maxWeight);
                    auto it = std::upper_bound(weights.begin(), weights.end(), randWeight);
                    int    index      = std::distance(weights.begin(), it);
                    m_sequence.append(files.at(index)->m_id);
                }
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
        ImagePtrK im = files.at(i);
        double weight = 0;

        if (im->m_date == 0s)
            weight = DEFAULT_WEIGHT;
        else
        {
            QDateTime timestamp = QDateTime::fromSecsSinceEpoch(im->m_date.count());
            QDateTime curYearAnniversary =
                    QDateTime(QDate(now.date().year(),
                                    timestamp.date().month(),
                                    timestamp.date().day()),
                              timestamp.time());

            bool isAnniversaryPast = curYearAnniversary < now;

            QDateTime adjacentYearAnniversary =
                    QDateTime(QDate(now.date().year() +
                                    (isAnniversaryPast ? 1 : -1),
                                    timestamp.date().month(),
                                    timestamp.date().day()),
                              timestamp.time());

            double range = llabs(curYearAnniversary.secsTo(
                                   adjacentYearAnniversary)) + BETA_CLIP;

            // This calculation is not normalized, because that would require the
            // beta function, which isn't part of the C++98 libraries. Weights
            // that aren't normalized work just as well relative to each other.
            QDateTime d1(isAnniversaryPast ? curYearAnniversary
                                           : adjacentYearAnniversary);
            QDateTime d2(isAnniversaryPast ? adjacentYearAnniversary
                                           : curYearAnniversary);
            weight = std::pow(llabs(now.secsTo(d1) + BETA_CLIP) / range,
                              TRAILING_BETA_SHAPE - 1)
                    * std::pow(llabs(now.secsTo(d2) + BETA_CLIP) / range,
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
    m_parentId = parentId;

    // Load child images of the parent
    ImageList files;
    ImageList dirs;
    m_mgr.GetChildren(m_parentId, files, dirs);

    // Load gallery datastore with current dir
    Populate(files);

    return !files.isEmpty();
}


/*!
 \brief Clears UI cache
*/
void FlatView::ClearCache()
{
    LOG(VB_FILE, LOG_DEBUG, LOC + "Cleared File cache");
    m_fileCache.clear();
}


/*!
 \brief Clear file from UI cache and optionally from view
 \param id
 \param remove If true, file is also deleted from view
 \return QStringList Url of image & thumbnail to remove from image cache
*/
QStringList FlatView::ClearImage(int id, bool remove)
{
    if (remove)
    {
        m_sequence.removeAll(id);
        m_images.remove(id);
    }

    QStringList urls;
    FileCacheEntry file = m_fileCache.take(id);

    if (!file.m_url.isEmpty())
        urls << file.m_url;

    if (!file.m_thumbUrl.isEmpty())
        urls << file.m_thumbUrl;

    LOG(VB_FILE, LOG_DEBUG, LOC + QString("Cleared %1 from file cache (%2)")
        .arg(id).arg(urls.join(",")));
    return urls;
}


/*!
 \brief Rotate view so that starting image is at front.
 \param id The image to be positioned
*/
void FlatView::Rotate(int id)
{
    // Rotate sequence so that (first appearance of) specified image is
    // at offset from front
    int index = m_sequence.indexOf(id);
    if (index >= 0)
    {
        int first = index % m_sequence.size();
        if (first > 0)
            m_sequence = m_sequence.mid(first) + m_sequence.mid(0, first);
    }
}


/*!
 * \brief Cache image properties to optimize UI
 * \param id Image id
 * \param parent Its dir
 * \param url Image url
 * \param thumb Thumbnail url
 */
void FlatView::Cache(int id, int parent, const QString &url, const QString &thumb)
{
    // Cache parent dir so that dir thumbs are updated when a child changes.
    // Also store urls for image cache cleanup
    FileCacheEntry cached(parent, url, thumb);
    m_fileCache.insert(id, cached);
    LOG(VB_FILE, LOG_DEBUG, LOC + "Caching " + cached.ToString(id));
}


QString DirCacheEntry::ToString(int id) const
{
    QStringList ids;
    for (const auto & thumb : std::as_const(m_thumbs))
        ids << QString::number(thumb.first);
    return QString("Dir %1 (%2, %3) Thumbs %4 (%5) Parent %6")
            .arg(id).arg(m_fileCount).arg(m_dirCount).arg(ids.join(","))
            .arg(m_thumbCount).arg(m_parent);
}


/*!
 \brief Constructs a view of images & directories that can be marked
 \param order Ordering to use for view
*/
DirectoryView::DirectoryView(SlideOrderType order)
    : FlatView(order)
{
    m_marked.Clear();
    m_prevMarked.Clear();
}


/*!
 \brief Get positional status
 \return QString "m/n" where m is selected index (0 for parent), n is total images
*/
QString DirectoryView::GetPosition() const
{
    return QString("%1/%2").arg(m_active).arg(m_sequence.size() - 1);
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
    // Determine parent (defaulting to ancestor) & get initial children
    ImageList files;
    ImageList dirs;
    ImagePtr parent;
    int count = 0;
    // Root is guaranteed to return at least 1 item
    while ((count = m_mgr.GetDirectory(parentId, parent, files, dirs)) == 0)
    {
        // Fallback if dir no longer exists
        // Ascend to Gallery for gallery subdirs, Root for device dirs & Gallery
        parentId = parentId > PHOTO_DB_ID ? PHOTO_DB_ID : GALLERY_DB_ID;
    }

    SetDirectory(parentId);
    m_parentId = parentId;

    // No SG & no devices uses special 'empty' screen
    if (!parent || (parentId == GALLERY_DB_ID && count == 1))
    {
        parent.clear();
        return false;
    }

    // Populate all subdirs
    for (const ImagePtr & im : std::as_const(dirs))
    {
        if (im)
            // Load sufficient thumbs from each dir as subsequent dirs may be empty
            LoadDirThumbs(*im, kMaxFolderThumbnails);
    }

    // Populate parent
    if (!PopulateFromCache(*parent, kMaxFolderThumbnails))
        PopulateThumbs(*parent, kMaxFolderThumbnails, files, dirs);

    // Dirs shown before images
    ImageList images = dirs + files;

    // Validate marked images
    if (!m_marked.isEmpty())
    {
        QSet<int> ids;
        for (const QSharedPointer<ImageItem> & im : std::as_const(images))
            ids.insert(im->m_id);
        m_marked.intersect(ids);
    }

    // Parent is always first (for navigating up).
    images.prepend(parent);

    // Preserve current selection before view is destroyed
    ImagePtrK selected = GetSelected();
    int activeId = selected ? selected->m_id : 0;

    // Construct view
    Populate(images);

    // Reinstate selection, falling back to parent
    Select(activeId);

    return true;
}


/*!
 \brief Populate thumbs for a dir
 \param parent Parent dir
 \param thumbsNeeded Number of thumbnails needed
 \param level Recursion depth
*/
void DirectoryView::LoadDirThumbs(ImageItem &parent, int thumbsNeeded, int level)
{
    // Use cached data, if available
    if (PopulateFromCache(parent, thumbsNeeded))
        return;

    // Load child images & dirs
    ImageList files;
    ImageList dirs;
    m_mgr.GetChildren(parent.m_id, files, dirs);

    PopulateThumbs(parent, thumbsNeeded, files, dirs, level);
}


/*!
 \brief Populate directory stats & thumbnails recursively from database as follows:
Use user cover, if assigned. Otherwise derive 4 thumbnails from: first 4 images,
then 1st thumbnail from first 4 sub-dirs, then 2nd thumbnail from sub-dirs etc
 \param parent The parent dir
 \param thumbsNeeded Number of thumbnails required
 \param files A list of files to process
 \param dirs A list of directories to process
 \param level Recursion level (to detect recursion deadlocks)
*/
void DirectoryView::PopulateThumbs(ImageItem &parent, int thumbsNeeded,
                                   const ImageList &files, const ImageList &dirs,
                                   int level)
{
    // Set parent stats
    parent.m_fileCount = files.size();
    parent.m_dirCount  = dirs.size();

    // Locate user assigned thumb amongst children, if defined
    ImagePtr userIm;
    if (parent.m_userThumbnail != 0)
    {
        ImageList images = files + dirs;
        // ImageItem has been explicitly marked Q_DISABLE_COPY
        for (const ImagePtr & im : std::as_const(images))
        {
            if (im && im->m_id == parent.m_userThumbnail)
            { // cppcheck-suppress useStlAlgorithm
                userIm = im;
                break;
            }
        }
    }

    // Children to use as thumbnails
    ImageList thumbFiles;
    ImageList thumbDirs;

    if (!userIm)
    {
        // Construct multi-thumbnail from all children
        thumbFiles = files;
        thumbDirs  = dirs;
    }
    else if (userIm->IsFile())
    {
        thumbFiles.append(userIm);
        thumbsNeeded = 1;
    }
    else
    {
        thumbDirs.append(userIm);
    }

    // Fill parent thumbs from child files first
    // Whilst they're available fill as many as possible for cache
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    for (int i = 0; i < std::min(kMaxFolderThumbnails, thumbFiles.size()); ++i)
#else
    for (int i = 0; i < std::min(kMaxFolderThumbnails, static_cast<int>(thumbFiles.size())); ++i)
#endif
    {
        parent.m_thumbNails.append(thumbFiles.at(i)->m_thumbNails.at(0));
        --thumbsNeeded;
    }

    // Only recurse if necessary
    if (thumbsNeeded > 0)
    {
        // Prevent lengthy/infinite recursion due to deep/cyclic folder
        // structures
        if (++level > 10)
        {
            LOG(VB_GENERAL, LOG_NOTICE, LOC +
                "Directory thumbnails are more than 10 levels deep");
        }
        else
        {
            // Recursively load subdir thumbs to try to get 1 thumb from each
            for (const ImagePtr & im : std::as_const(thumbDirs))
            {
                if (!im)
                    continue;

                // Load sufficient thumbs from each dir as subsequent dirs may
                // be empty
                LoadDirThumbs(*im, thumbsNeeded, level);

                if (!im->m_thumbNails.empty())
                {
                    // Add first thumbnail to parent thumb
                    parent.m_thumbNails.append(im->m_thumbNails.at(0));

                    // Quit when we have sufficient thumbs
                    if (--thumbsNeeded == 0)
                        break;
                }
            }

            // If insufficient dirs to supply 1 thumb per dir, use other dir
            // thumbs (indices 1-3) as well
            int i = 0;
            while (thumbsNeeded > 0 && ++i < kMaxFolderThumbnails)
            {
                for (const QSharedPointer<ImageItem> & im : std::as_const(thumbDirs))
                {
                    if (i < im->m_thumbNails.size())
                    {
                        parent.m_thumbNails.append(im->m_thumbNails.at(i));
                        if (--thumbsNeeded == 0)
                            break;
                    }
                }
            }
        }
    }

    // Flag the cached entry with number of thumbs loaded. If future uses require
    // more, then the dir must be reloaded.
    // For user thumbs and dirs with insufficient child images, the cache is always valid
    int scanned = (userIm || thumbsNeeded > 0)
            ? kMaxFolderThumbnails
            : parent.m_thumbNails.size();

    // Cache result to optimize navigation
    Cache(parent, scanned);
}


/*!
 \brief Resets view
 \param resetParent parent id is only reset to root when this is set
*/
void DirectoryView::Clear(bool /*resetParent*/)
{
    ClearMarked();
    ClearCache();
    FlatView::Clear();
}


/*!
 \brief Mark all images/dirs
*/
void DirectoryView::MarkAll()
{
    // Any marking clears previous marks
    m_prevMarked.Clear();
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
        m_prevMarked.Clear();
        m_marked.Add(id);
    }
    else
    {
        m_prevMarked.remove(id);
        m_marked.remove(id);
    }
}


/*!
 \brief Mark all unmarked items, unmark all marked items
*/
void DirectoryView::InvertMarked()
{
    // Any marking clears previous marks
    m_prevMarked.Clear();
    m_marked.Invert(GetChildren());
}


/*!
 \brief Unmark all items
*/
void DirectoryView::ClearMarked()
{
    m_marked.Clear();
    m_prevMarked.Clear();
}


/*!
 \brief Manage markings on tree navigation
 \param newParent Id of new parent dir
*/
void DirectoryView::SetDirectory(int newParent)
{
    if (m_marked.IsFor(newParent))
        // Directory hasn't changed
        return;

    // Markings are cleared on every dir change
    // Any current markings become previous markings
    // Only 1 set of previous markings are preserved
    if (m_prevMarked.IsFor(newParent))
    {
        // Returned to dir of previous markings: reinstate them
        m_marked = m_prevMarked;
        m_prevMarked.Clear();
        return;
    }

    if (!m_marked.isEmpty())
        // Preserve current markings
        m_prevMarked = m_marked;

    // Initialise current markings for new dir
    m_marked.Initialise(newParent);
}


/*!
 \brief Determine current selection, markings & various info to support menu display
 \return MenuSubjects Current state
*/
MenuSubjects DirectoryView::GetMenuSubjects()
{
    // hiddenMarked is true if 1 or more marked items are hidden
    // unhiddenMarked is true if 1 or more marked items are not hidden
    bool hiddenMarked   = false;
    bool unhiddenMarked = false;
    for (int id : std::as_const(m_marked))
    {
        ImagePtrK im = m_images.value(id);
        if (!im)
            continue;

        if (im->m_isHidden)
            hiddenMarked = true;
        else
            unhiddenMarked = true;

        if (hiddenMarked && unhiddenMarked)
            break;
    }

    return {GetSelected(), m_sequence.size() - 1,
            m_marked,      m_prevMarked,
            hiddenMarked,  unhiddenMarked};
}


/*!
 \brief Retrieve cached dir, if available
 \param dir Dir image
 \param required Number of thumbnails required
 \return bool True if a cache entry exists
*/
bool DirectoryView::PopulateFromCache(ImageItem &dir, int required)
{
    DirCacheEntry cached(m_dirCache.value(dir.m_id));
    if (cached.m_dirCount == -1 || cached.m_thumbCount < required)
        return false;

    dir.m_fileCount  = cached.m_fileCount;
    dir.m_dirCount   = cached.m_dirCount;
    dir.m_thumbNails = cached.m_thumbs;

    LOG(VB_FILE, LOG_DEBUG, LOC + "Using cached " + cached.ToString(dir.m_id));
    return true;
}


/*!
 \brief Cache displayed dir
 \param dir Dir image
 \param thumbCount Number of populated thumbnails
*/
void DirectoryView::Cache(ImageItemK &dir, int thumbCount)
{
    // Cache counts & thumbnails for each dir so that we don't need to reload its
    // children from Db each time it's displayed
    DirCacheEntry cacheEntry(dir.m_parentId, dir.m_dirCount, dir.m_fileCount,
                             dir.m_thumbNails, thumbCount);

    m_dirCache.insert(dir.m_id, cacheEntry);

    // Cache images used by dir thumbnails
    for (const ThumbPair & thumb : std::as_const(dir.m_thumbNails))
    {
        // Do not overwrite any existing image url nor parent.
        // Image url is cached when image is displayed as a child, but not as a
        // ancestor dir thumbnail
        // First cache attempt will be by parent. Subsequent attempts may be
        // by ancestor dirs.
        if (!m_fileCache.contains(thumb.first))
            FlatView::Cache(thumb.first, dir.m_id, "", thumb.second);
    }
    LOG(VB_FILE, LOG_DEBUG, LOC + "Caching " + cacheEntry.ToString(dir.m_id));
}


/*!
 \brief Clears UI cache
*/
void DirectoryView::ClearCache()
{
    LOG(VB_FILE, LOG_DEBUG, LOC + "Cleared Dir cache");
    m_dirCache.clear();
    FlatView::ClearCache();
}


/*!
 \brief Clear file/dir and all its ancestors from UI cache so that ancestor
 thumbnails are recalculated. Optionally deletes file/dir from view.
 \param id Image id
 \param deleted If true, file is also deleted from view
 \return QStringList Url of image & thumbnail to remove from image cache
*/
QStringList DirectoryView::RemoveImage(int id, bool deleted)
{
    QStringList urls;
    int dirId = id;

    if (deleted)
    {
        m_marked.remove(id);
        m_prevMarked.remove(id);
    }

    // If id is a file then start with its parent
    if (m_fileCache.contains(id))
    {
        // Clear file cache & start from its parent dir
        dirId = m_fileCache.value(id).m_parent;
        urls = FlatView::ClearImage(id, deleted);
    }

    // Clear ancestor dirs
    while (m_dirCache.contains(dirId))
    {
        LOG(VB_FILE, LOG_DEBUG, LOC + QString("Cleared %1 from dir cache").arg(dirId));
        DirCacheEntry dir = m_dirCache.take(dirId);
        dirId = dir.m_parent;
    }
    return urls;
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
    m_parentId = parentId;

    // Load visible subtree of the parent
    // Ordered images of parent first, then breadth-first recursion of ordered dirs
    ImageList files;
    m_mgr.GetImageTree(m_parentId, files);

    // Load view
    Populate(files);

    return !files.isEmpty();
}
