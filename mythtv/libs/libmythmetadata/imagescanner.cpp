#include "imagescanner.h"

#include "libmythbase/mythcorecontext.h"  // for gCoreContext
#include "libmythbase/mythlogging.h"

#include "imagemetadata.h"

/*!
 \brief Constructor
 \param dbfs Database/filesystem adapter
 \param thumbGen Companion thumbnail generator
*/
template <class DBFS>
ImageScanThread<DBFS>::ImageScanThread(DBFS *const dbfs, ImageThumb<DBFS> *thumbGen)
    : MThread("ImageScanner"),
      m_dbfs(*dbfs),
      m_thumb(*thumbGen),
      m_dir(m_dbfs.GetImageFilters())
{ }


template <class DBFS>
ImageScanThread<DBFS>::~ImageScanThread()
{
    cancel();
    wait();
}


/*!
 \brief Clears queued items so that the thread can exit.
*/
template <class DBFS>
void ImageScanThread<DBFS>::cancel()
{
    m_mutexQueue.lock();
    m_clearQueue.clear();
    m_mutexQueue.unlock();

    QMutexLocker locker(&m_mutexState);
    m_scanning = false;
}


/*!
 \brief Return current scanner status
 \return bool True if requested state is "scan"
*/
template <class DBFS>
bool ImageScanThread<DBFS>::IsScanning()
{
    QMutexLocker locker(&m_mutexState);
    return m_scanning;
}


/*!
 \brief Get status of 'clear device' queue
 \return bool True if pending 'clear' requests exist
*/
template <class DBFS>
bool ImageScanThread<DBFS>::ClearsPending()
{
    QMutexLocker locker(&m_mutexQueue);
    return !m_clearQueue.isEmpty();
}


/*!
 \brief Run or interrupt scanner
 \details Clear requests are actioned before and after every scan
 \param scan If true, scan will start after pending clears are actioned
 If false, a running scan is interrupted. Pending clear requests will be
 actioned.
*/
template <class DBFS>
void ImageScanThread<DBFS>::ChangeState(bool scan)
{
    QMutexLocker locker(&m_mutexState);
    m_scanning = scan;

    // Restart thread if not already running
    if (!isRunning())
        start();
}


/*!
 \brief Queues a 'Clear Device' request, which will be actioned immediately.
 \details If scanner is already running, any scan will be aborted to process the
clear.
 \param devId Device id
 \param action [DEVICE] (CLOSE | CLEAR)
*/
template <class DBFS>
void ImageScanThread<DBFS>::EnqueueClear(int devId, const QString &action)
{
    m_mutexQueue.lock();
    m_clearQueue << qMakePair(devId, action);
    m_mutexQueue.unlock();

    ChangeState(false);
}


/*!
 \brief Returns number of images scanned & total number to scan
 \return QStringList (scanner id, \#done, \#total)
*/
template <class DBFS>
QStringList ImageScanThread<DBFS>::GetProgress()
{
    QMutexLocker locker(&m_mutexProgress);
    return QStringList() << QString::number(static_cast<int>(gCoreContext->IsBackend()))
                         << QString::number(m_progressCount)
                         << QString::number(m_progressTotalCount);
}

/*!
 \brief Synchronises database to the storage group
 \details Scans all dirs and files and populates database with
 metadata for each. Broadcasts progress events whilst scanning, initiates
 thumbnail generation and notifies clients when finished.
*/
template <class DBFS>
void ImageScanThread<DBFS>::run()
{
    RunProlog();

    setPriority(QThread::LowPriority);

    bool at_least_once { true };
    while (ClearsPending() || at_least_once)
    {
        at_least_once = false;

        // Process all clears before scanning
        while (ClearsPending())
        {
            m_mutexQueue.lock();
            if (m_clearQueue.isEmpty())
                break;
            ClearTask task = m_clearQueue.takeFirst();
            m_mutexQueue.unlock();

            int devId      = task.first;
            QString action = task.second;

            LOG(VB_GENERAL, LOG_INFO,
                QString("Clearing Filesystem: %1 %2").arg(action).arg(devId));

            // Clear Db
            m_dbfs.ClearDb(devId, action);

            // Pass on to thumb generator now scanning has stopped
            m_thumb.ClearThumbs(devId, action);
        }

        // Scan requested ?
        if (IsScanning())
        {
            LOG(VB_GENERAL, LOG_INFO,  "Starting scan");

            // Load known directories and files from the database
            if (!m_dbfs.ReadAllImages(m_dbFileMap, m_dbDirMap))
                // Abort on any Db error
                break;

            bool firstScan = m_dbFileMap.isEmpty();

            // Pause thumb generator so that scans are fast as possible
            m_thumb.PauseBackground(true);

            // Adapter determines list of dirs to scan
            StringMap paths = m_dbfs.GetScanDirs();

            CountFiles(paths.values());

            // Now start the actual syncronization
            m_seenFile.clear();
            m_changedImages.clear();
            StringMap::const_iterator i = paths.constBegin();
            while (i != paths.constEnd() && IsScanning())
            {
                SyncSubTree(QFileInfo(i.value()), GALLERY_DB_ID, i.key(), i.value());
                ++i;
            }

            // Release thumb generator asap
            m_thumb.PauseBackground(false);

            // Adding or updating directories has been completed.
            // The maps now only contain old directories & files that are not
            // in the filesystem anymore. Remove them from the database
            m_dbfs.RemoveFromDB(QVector<ImagePtr>::fromList(m_dbDirMap.values()));
            m_dbfs.RemoveFromDB(QVector<ImagePtr>::fromList(m_dbFileMap.values()));

            // Cleanup thumbnails
            QStringList mesg(m_thumb.DeleteThumbs(QVector<ImagePtr>::fromList(m_dbFileMap.values())));
            mesg << m_changedImages.join(",");

            // Cleanup dirs
            m_dbFileMap.clear();
            m_dbDirMap.clear();
            m_seenDir.clear();

            m_mutexProgress.lock();
            // (count == total) signals scan end
            Broadcast(m_progressTotalCount);
            // Must reset counts for scan queries
            m_progressCount = m_progressTotalCount = 0;
            m_mutexProgress.unlock();

            LOG(VB_GENERAL, LOG_INFO,  "Finished scan");

            // For initial scans pause briefly to give thumb generator a headstart
            // before being deluged by client requests
            if (firstScan)
                usleep(1s);

            // Notify clients of completion with removed & changed images
            m_dbfs.Notify("IMAGE_DB_CHANGED", mesg);

            ChangeState(false);
        }
    }

    RunEpilog();
}


/*!
 \brief Scans a dir subtree
 \details Uses a recursive depth-first scan to detect all files matching
 image/video filters. Dirs that match exclusions regexp are ignored.
 \param dirInfo Dir info at root of this subtree
 \param parentId Id of parent dir
 \param devId Device id being scanned
 \param base Root device path
*/
template <class DBFS>
void ImageScanThread<DBFS>::SyncSubTree(const QFileInfo &dirInfo, int parentId,
                                  int devId, const QString &base)
{
    // Ignore excluded dirs
    if (m_exclusions.match(dirInfo.fileName()).hasMatch())
    {
        LOG(VB_FILE, LOG_INFO,
            QString("Excluding dir %1").arg(dirInfo.absoluteFilePath()));
        return;
    }

    // Use global image filters
    QDir dir = m_dir;
    if (!dir.cd(dirInfo.absoluteFilePath()))
    {
        LOG(VB_FILE, LOG_INFO,
            QString("Failed to open dir %1").arg(dirInfo.absoluteFilePath()));
        return;
    }

    // Create directory node
    int id = SyncDirectory(dirInfo, devId, base, parentId);
    if (id == -1)
    {
        LOG(VB_FILE, LOG_INFO,
            QString("Failed to sync dir %1").arg(dirInfo.absoluteFilePath()));
        return;
    }

    // Sync its contents
    QFileInfoList entries = dir.entryInfoList();
    for (const auto & fileInfo : std::as_const(entries))
    {
        if (!IsScanning())
        {
            LOG(VB_GENERAL, LOG_INFO,
                QString("Scan interrupted in %2").arg(dirInfo.absoluteFilePath()));
            return;
        }

        if (fileInfo.isDir())
        {
            // Scan this directory
            SyncSubTree(fileInfo, id, devId, base);
        }
        else
        {
            SyncFile(fileInfo, devId, base, id);

            QMutexLocker locker(&m_mutexProgress);
            ++m_progressCount;

            // Throttle updates
            if (m_bcastTimer.elapsed() > 250)
                Broadcast(m_progressCount);
        }
    }
}


/*!
 \brief Updates/populates db for a dir
 \details Db is updated if dir modified time has changed since last scan.
 Contents are scanned even if dir is unchanged.
 Clones are dirs within the Storage Group with the same path (relative to SG).
 They resolve to a single Db dir - their contents are amalgamated.
 ie. <SG dir 1>/somePath/dirName and <SG dir 2>/somePath/dirName result in a
 single Gallery dir of "somePath/dirName"
 \param dirInfo Dir info
 \param devId Id of device containing dir
 \param base Device path
 \param parentId Db id of the dir's parent
 \return int Db id of this dir in db
*/
template <class DBFS>
 int ImageScanThread<DBFS>::SyncDirectory(const QFileInfo &dirInfo, int devId, const QString &base, int parentId)
{
    QString absFilePath = dirInfo.absoluteFilePath();

    LOG(VB_FILE, LOG_DEBUG, QString("Syncing directory %1").arg(absFilePath));

    ImagePtr dir(m_dbfs.CreateItem(dirInfo, parentId, devId, base));

    // Is dir already in Db ?
    if (m_dbDirMap.contains(dir->m_filePath))
    {
        ImagePtr dbDir = m_dbDirMap.value(dir->m_filePath);
        if (dbDir == nullptr)
            return -1;

        // The directory already exists in the db. Retain its id
        dir->m_id = dbDir->m_id;

        // Parent may have changed due to a move
        if (dir->m_modTime != dbDir->m_modTime
                || dir->m_parentId != dbDir->m_parentId)
        {
            LOG(VB_FILE, LOG_INFO,
                QString("Changed directory %1").arg(absFilePath));

            // Retain existing id & settings
            dir->m_isHidden      = dbDir->m_isHidden;
            dir->m_userThumbnail = dbDir->m_userThumbnail;

            m_dbfs.UpdateDbImage(*dir);
            // Note modified images
            m_changedImages << QString::number(dir->m_id);
        }

        // Remove the entry from the dbList
        m_dbDirMap.remove(dir->m_filePath);
    }
    // Detect clones (same path in different SG dir)
    else if (m_seenDir.contains(dir->m_filePath))
    {
        ImagePtr cloneDir = m_seenDir.value(dir->m_filePath);
        if (cloneDir == nullptr)
            return -1;

        // All clones point to same Db dir. Use latest
        if (cloneDir->m_modTime >= dir->m_modTime )
        {
            LOG(VB_FILE, LOG_INFO, QString("Directory %1 is an older clone of %2")
                .arg(absFilePath, cloneDir->m_filePath));

            // Use previous version
            dir = cloneDir;
        }
        else
        {
            LOG(VB_FILE, LOG_INFO,
                QString("Directory %1 is a more recent clone of %2")
                .arg(absFilePath, cloneDir->m_filePath));

            // Use new version
            dir->m_id = cloneDir->m_id;
            // Note modified time
            m_changedImages << QString::number(dir->m_id);
        }

        // Mark non-devices as cloned (for info display only)
        if (!dir->IsDevice())
        {
            dir->m_type = kCloneDir;
            m_dbfs.UpdateDbImage(*dir);
        }
    }
    else
    {
        LOG(VB_FILE, LOG_INFO, QString("New directory %1").arg(absFilePath));

        // Create new Db dir with new id
        dir->m_id = m_dbfs.InsertDbImage(*dir);
    }

    // Note it for clone detection
    m_seenDir.insert(dir->m_filePath, dir);

    return dir->m_id;
}


/*!
  \brief Read image date, orientation, comment from metadata
  \param[in] path Image filepath
  \param[in] type Picture or Video
  \param[out] comment Image comment
  \param[out] time Time/date of image capture
  \param[out] orientation Exif orientation code
 */
template <class DBFS>
void ImageScanThread<DBFS>::PopulateMetadata(
    const QString &path, int type, QString &comment,
    std::chrono::seconds &time,
    int &orientation)
{
    // Set orientation, date, comment from file meta data
    ImageMetaData *metadata = (type == kImageFile)
            ? ImageMetaData::FromPicture(path)
            : ImageMetaData::FromVideo(path);

    orientation  = metadata->GetOrientation();
    comment      = metadata->GetComment().simplified();
    QDateTime dt = metadata->GetOriginalDateTime();
    time         = (dt.isValid()) ? std::chrono::seconds(dt.toSecsSinceEpoch()) : 0s;

    delete metadata;
}


/*!
 \brief Updates/populates db for an image/video file
 \details Db is updated if file modified time has changed since last scan.
 Extracts orientation, date and 2 comments from exif/video metadata.
 Duplicates are files within the Storage Group with the same path (relative to SG).
 They are invalid (user error?) - only the first is accepted; others are ignored.
 ie. <SG dir 1>/somePath/fileName and <SG dir 2>/somePath/fileName result in a
 single image from <SG dir 1>. This is consistent with StorageGroup::FindFile(),
 which will never find the second file.
 \param fileInfo File info
 \param devId Id of device containing dir
 \param base Device path
 \param parentId Db id of the dir's parent
*/
template <class DBFS>
void ImageScanThread<DBFS>::SyncFile(const QFileInfo &fileInfo, int devId,
                               const QString &base, int parentId)
{
    // Ignore excluded files
    if (m_exclusions.match(fileInfo.fileName()).hasMatch())
    {
        LOG(VB_FILE, LOG_INFO,
            QString("Excluding file %1").arg(fileInfo.absoluteFilePath()));
        return;
    }

    QString absFilePath = fileInfo.absoluteFilePath();

    ImagePtr im(m_dbfs.CreateItem(fileInfo, parentId, devId, base));
    if (!im)
        // Ignore unknown file type
        return;

    if (m_dbFileMap.contains(im->m_filePath))
    {
        ImagePtrK dbIm = m_dbFileMap.value(im->m_filePath);

        // Parent may have changed due to a move
        if (im->m_modTime == dbIm->m_modTime && im->m_parentId == dbIm->m_parentId)
        {
            // File already known & hasn't changed
            // Remove it from removed list
            m_dbFileMap.remove(im->m_filePath);
            // Detect duplicates
            m_seenFile.insert(im->m_filePath, absFilePath);
            return;
        }

        LOG(VB_FILE, LOG_INFO, QString("Modified file %1").arg(absFilePath));

        // Retain existing id & settings
        im->m_id       = dbIm->m_id;
        im->m_isHidden = dbIm->m_isHidden;

        // Set date, comment from file meta data
        int fileOrient = 0;
        PopulateMetadata(absFilePath, im->m_type,
                         im->m_comment, im->m_date, fileOrient);

        // Reset file orientation, retaining existing setting
        int currentOrient = Orientation(dbIm->m_orientation).GetCurrent();
        im->m_orientation = Orientation(currentOrient, fileOrient).Composite();

        // Remove it from removed list
        m_dbFileMap.remove(im->m_filePath);
        // Note modified images
        m_changedImages << QString::number(im->m_id);

        // Update db
        m_dbfs.UpdateDbImage(*im);
    }
    else if (m_seenFile.contains(im->m_filePath))
    {
        LOG(VB_GENERAL, LOG_WARNING, QString("Ignoring %1 (Duplicate of %2)")
            .arg(absFilePath, m_seenFile.value(im->m_filePath)));
        return;
    }
    else
    {
        // New images will be assigned an id by the db AUTO-INCREMENT
        LOG(VB_FILE, LOG_INFO,  QString("New file %1").arg(absFilePath));

        // Set date, comment from file meta data
        int fileOrient = 0;
        PopulateMetadata(absFilePath, im->m_type,
                         im->m_comment, im->m_date, fileOrient);

        // Set file orientation
        im->m_orientation = Orientation(fileOrient, fileOrient).Composite();

        // Update db (Set id for thumb generator)
        im->m_id = m_dbfs.InsertDbImage(*im);
    }

    // Detect duplicate filepaths in SG
    m_seenFile.insert(im->m_filePath, absFilePath);

    // Populate absolute filename so that thumbgen doesn't need to locate file
    im->m_filePath = absFilePath;

    // Ensure thumbnail exists.
    m_thumb.CreateThumbnail(im);
}


/*!
 \brief Counts images in a dir subtree
 \details Ignores dirs that match exclusions regexp
 \param dir Root of subtree
*/
template <class DBFS>
void ImageScanThread<DBFS>::CountTree(QDir &dir)
{
    QFileInfoList entries = dir.entryInfoList();
    for (const auto & fileInfo : std::as_const(entries))
    {
        // Ignore excluded dirs/files
        if (m_exclusions.match(fileInfo.fileName()).hasMatch())
            continue;

        if (fileInfo.isFile())
        {
            ++m_progressTotalCount;
        }
        // Ignore missing dirs
        else if (dir.cd(fileInfo.fileName()))
        {
            CountTree(dir);
            dir.cdUp();
        }
    }
}


/*!
 \brief Counts images in a list of subtrees
 \param paths List of dir trees to scan
*/
template <class DBFS>
void ImageScanThread<DBFS>::CountFiles(const QStringList &paths)
{
    // Get exclusions as comma-seperated list using glob chars * and ?
    QString excPattern = gCoreContext->GetSetting("GalleryIgnoreFilter", "");

    // Combine into a single regexp
    excPattern.replace(".", "\\."); // Preserve "."
    excPattern.replace("*", ".*");  // Convert glob wildcard "*"
    excPattern.replace("?", ".");   // Convert glob wildcard "?"
    excPattern.replace(",", "|");   // Convert list to OR's

    QString pattern = QString("^(%1)$").arg(excPattern);
    m_exclusions = QRegularExpression(pattern);

    LOG(VB_FILE, LOG_DEBUG, QString("Exclude regexp is \"%1\"").arg(pattern));

    // Lock counts until counting complete
    QMutexLocker locker(&m_mutexProgress);
    m_progressCount       = 0;
    m_progressTotalCount  = 0;

    // Use global image filters
    QDir dir = m_dir;
    for (const auto& sgDir : std::as_const(paths))
    {
        // Ignore missing dirs
        if (dir.cd(sgDir))
            CountTree(dir);
    }
    // 0 signifies a scan start
    Broadcast(0);
}


/*!
 \brief Notify listeners of scan progress
 \details
 \note Count mutex must be held before calling this
 \param progress Number of images processed
*/
template <class DBFS>
void ImageScanThread<DBFS>::Broadcast(int progress)
{
    // Only 2 scanners are ever visible (FE & BE) so use bool as scanner id
    QStringList status;
    status << QString::number(static_cast<int>(gCoreContext->IsBackend()))
           << QString::number(progress)
           << QString::number(m_progressTotalCount);

    m_dbfs.Notify("IMAGE_SCAN_STATUS", status);

    // Reset broadcast throttle
    m_bcastTimer.start();
}


// Must define the valid template implementations to generate code for the
// instantiations (as they are defined in the cpp rather than header).
// Otherwise the linker will fail with undefined references...
#include "imagemanager.h"
template class ImageScanThread<ImageDbLocal>;
template class ImageScanThread<ImageDbSg>;
