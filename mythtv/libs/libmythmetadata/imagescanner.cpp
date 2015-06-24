#include "imagescanner.h"

#include <QtAlgorithms>

#include <imagethumbs.h>
#include <imagemetadata.h>
#include <imageutils.h>

/*!
 \brief  Constructor
*/
ImageScanThread::ImageScanThread() :
    MThread("ImageScanner"),
    m_db(),
    m_sg(ImageSg::getInstance()),
    m_progressCount(0),
    m_progressTotalCount(0),
    m_dir(ImageSg::getInstance()->GetImageFilters()),
    m_exclusions()
{
    QMutexLocker locker(&m_mutexState);
    m_state = kDormant;
}


/*!
 \brief  Destructor
*/
ImageScanThread::~ImageScanThread()
{
    cancel();
    wait();
}


/*!
 \brief  Clears the thumbnail list so that the thread can exit.
*/
void ImageScanThread::cancel()
{
    QMutexLocker locker(&m_mutexState);
    m_state = kInterrupt;
}


/*!
 \brief Return current scanner status
 \return ScannerState
*/
ScannerState ImageScanThread::GetState()
{
    QMutexLocker locker(&m_mutexState);
    return m_state;
}


/*!
 \brief Request scan start/stop, clear Db
 \param to New state
*/
void ImageScanThread::ChangeState(ScannerState to)
{
    QMutexLocker locker(&m_mutexState);
    m_state = to;

    // Restart thread if not already running
    if (!this->isRunning())
        this->start();
}


/*!
 \brief Returns number of images scanned & total number to scan
 \return QStringList "done/total"
*/
QStringList ImageScanThread::GetProgress()
{
    QMutexLocker locker(&m_mutexProgress);
    return QStringList() << QString::number(m_progressCount)
                         << QString::number(m_progressTotalCount);
}


/*!
 \brief Notify listeners of mode & progress
 \param mode Mode to broadcast
*/
void ImageScanThread::BroadcastStatus(QString mode)
{
    QStringList status;

    { // Release lock before sending message
        QMutexLocker locker(&m_mutexProgress);

        status << mode
               << QString::number(m_progressCount)
               << QString::number(m_progressTotalCount);
    }

    MythEvent me = MythEvent("IMAGE_SCAN_STATUS", status);
    gCoreContext->SendEvent(me);
}


/*!
 \brief Counts images in a dir subtree
 \details Ignores files/dirs that match exclusions regexp
 \param dir Parent of subtree
*/
void ImageScanThread::CountTree(QDir &dir)
{
    QFileInfoList files = dir.entryInfoList();

    foreach(const QFileInfo &fileInfo, files)
    {
        if (fileInfo.isFile())
            ++m_progressTotalCount;
        else if (m_exclusions.exactMatch(fileInfo.fileName()))
            LOG(VB_GENERAL, LOG_INFO, QString("%1: Excluding %2")
                .arg(objectName(), fileInfo.filePath()));
        else
        {
            dir.cd(fileInfo.fileName());
            CountTree(dir);
            dir.cdUp();
        }
    }
}


/*!
 \brief Counts images in a list of subtrees
 \param paths List of dirs
*/
void ImageScanThread::CountFiles(QStringList paths)
{
    // Get exclusions as comma-seperated list using glob chars * and ?
    QString excPattern = gCoreContext->GetSetting("GalleryIgnoreFilter", "");

    // Combine into a single regexp
    excPattern.replace(".", "\\."); // Preserve "."
    excPattern.replace("*", ".*");  // Convert glob wildcard "*"
    excPattern.replace("?", ".");  // Convert glob wildcard "?"
    excPattern.replace(",", "|");   // Convert list to OR's

    QString pattern = QString("^(%1)$").arg(excPattern);
    m_exclusions = QRegExp(pattern);

    LOG(VB_FILE, LOG_DEBUG, QString("%1: Exclude regexp is \"%2\"")
        .arg(objectName(), pattern));

    QMutexLocker locker(&m_mutexProgress);
    m_progressCount       = 0;
    m_progressTotalCount  = 0;

    // Release lock to broadcast
    locker.unlock();
    BroadcastStatus("SCANNING");
    locker.relock();

    // Use global image filters
    QDir dir = m_dir;
    foreach(const QString &sgDir, paths)
    {
        dir.cd(sgDir);
        CountTree(dir);
    }
}


/*!
 \brief Synchronises database to the storage group
 \details Reads all dirs and files in storage group and populates database with
 metadata for each. Broadcasts progress events whilst scanning and initiates
 thumbnail generation when finished.
*/
void ImageScanThread::run()
{
    RunProlog();

    setPriority(QThread::LowPriority);

    QStringList removed;

    // Scan requested ?
    if (m_state == kScan)
    {
        LOG(VB_GENERAL, LOG_INFO, objectName() + ": Starting scan");

        // Known files/dirs in the Db. Objects are either;
        // - deleted explicitly (when matched to the filesystem),
        // - or passed to the Thumbnail Generator (old dirs/files that have disappeared)
        m_dbDirMap = new ImageMap();
        m_dbFileMap = new ImageMap();

        // Scan all SG dirs
        QStringList paths = m_sg->GetStorageDirs();

        CountFiles(paths);

        // Load all available directories and files from the database so that
        // they can be compared against the ones on the filesystem.
        // Ignore root dir, which is notional
        m_db.ReadDbItems(*m_dbFileMap, *m_dbDirMap,
                           QString("file_id != %1").arg(ROOT_DB_ID));

        // Ensure Root dir exists as first db entry and update last scan time
        ImageItem root;
        root.m_id            = ROOT_DB_ID;
        root.m_name          = QString("");
        root.m_parentId      = 0;
        root.m_type          = kBaseDirectory;
        root.m_modTime       = QDateTime::currentMSecsSinceEpoch() / 1000;

        m_db.UpdateDbFile(&root);

        // Now start the actual syncronization
        foreach(const QString &path, paths)
        {
            QString base = path;
            if (!base.endsWith('/'))
                base.append('/');

            LOG(VB_FILE, LOG_INFO,
                QString("%1: Syncing from SG dir %2").arg(objectName(), path));

            SyncFilesFromDir(path, ROOT_DB_ID, base);
        }

        // Adding or updating directories has been completed.
        // The maps now only contain old directories & files that are not
        // in the filesystem anymore. Remove them from the database
        ImageList files = m_dbDirMap->values() + m_dbFileMap->values();
        m_db.RemoveFromDB(files);

        // Cleanup thumbnails
        removed = ImageThumb::getInstance()->DeleteThumbs(m_dbFileMap->values(),
                                                          m_dbDirMap->values());

        LOG(VB_GENERAL, LOG_INFO, objectName() + ": Finished scan");

        // Clean up containers & contents
        delete m_dbFileMap;
        delete m_dbDirMap;

        // Wait for 'urgent' thumbs to be generated before notifying clients.
        // Otherwise they'd have nothing to draw...
        WaitForThumbs();
    }

    // Scan has completed or been interrupted. Now process any Clear request
    if (m_state == kClear)
    {
        LOG(VB_GENERAL, LOG_INFO, objectName() + ": Clearing Database");

        m_db.ClearDb();
        ImageThumb::getInstance()->ClearAllThumbs();

        removed = QStringList("ALL");
    }

    ChangeState(kDormant);

    // Notify scan has finished
    BroadcastStatus("");

    // Notify clients of Db update
    gCoreContext->SendEvent(MythEvent("IMAGE_DB_CHANGED", removed));

    RunEpilog();
}


/*!
 \brief Blocks until all urgent thumbnails have been generated. Time-outs after
 10 secs
*/
void ImageScanThread::WaitForThumbs()
{
    // Wait up to 10s for queue to empty.
    int remaining = ImageThumb::getInstance()->GetQueueSize(kScannerUrgentPriority);

    { // Counts now represent pending thumbnails
        QMutexLocker locker(&m_mutexProgress);
        m_progressCount       = 0;
        m_progressTotalCount  = remaining;
    }

    int timeout = 100;
    while (remaining > 0 && --timeout > 0)
    {
        BroadcastStatus("THUMBNAILS");
        msleep(1000);
        remaining = ImageThumb::getInstance()->GetQueueSize(kScannerUrgentPriority);
        QMutexLocker locker(&m_mutexProgress);
        m_progressCount = std::max(m_progressTotalCount - remaining, 0);
    }
    BroadcastStatus("THUMBNAILS");
}


/*!
 \brief Scans a dir subtree and updates/populates db metadata to match filesystem
 \details Detects all files that match image/video filters. Files/dirs that
 match exclusions regexp are ignored.
 \param path Dir that is subtree root
 \param parentId Db id of the dir's parent dir
 \param baseDirectory The storage group root dir path
*/
void ImageScanThread::SyncFilesFromDir(QString path,
                                       int parentId,
                                       QString baseDirectory)
{
    // Use global image filters
    QDir dir = m_dir;
    dir.cd(path);
    QFileInfoList list = dir.entryInfoList();

    foreach(const QFileInfo &fileInfo, list)
    {
        // Ignore excluded files
        if (m_exclusions.exactMatch(fileInfo.fileName()))
            continue;

        { // Release lock before continuing
            QMutexLocker locker(&m_mutexState);
            if (m_state != kScan)
            {
                LOG(VB_GENERAL, LOG_INFO,
                    QString("%1: Scan interrupted in %2").arg(objectName(), path));
                return;
            }
        }

        if (fileInfo.isDir())
        {
            // Get the id. This will be new parent id
            // when we traverse down the current directory.
            int id = SyncDirectory(fileInfo, parentId, baseDirectory);

            // Get new files within this directory
            QString fileName = fileInfo.absoluteFilePath();
            SyncFilesFromDir(fileName, id, baseDirectory);
        }
        else
        {
            SyncFile(fileInfo, parentId, baseDirectory);

            // Update progress count
            { // Release lock quickly
                QMutexLocker locker(&m_mutexProgress);
                ++m_progressCount;
            }
            // report status on completion of every dir
            BroadcastStatus("SCANNING");
        }
    }
}


/*!
 \brief Updates/populates db for a dir
 \details Dir is updated if dir modified time has changed since last scan.
 \param fileInfo Dir info
 \param parentId Db id of the dir's parent dir
 \param baseDirectory The storage group root dir path
 \return int Db id of this dir in db
*/
int ImageScanThread::SyncDirectory(QFileInfo fileInfo,
                                   int parentId,
                                   QString baseDirectory)
{
    LOG(VB_FILE, LOG_DEBUG, QString("%1: Syncing directory %2")
        .arg(objectName(), fileInfo.absoluteFilePath()));

    // Load all required information of the directory
    ImageItem *dir = LoadDirectoryData(fileInfo, parentId, baseDirectory);

    ImageItem *dbDir = m_dbDirMap->value(dir->m_fileName);
    int id;

    if (dbDir)
    {
        // The directory already exists in the db. Retain its id
        id = dir->m_id = dbDir->m_id;

        // Check for change of contents
        if (dir->m_modTime != dbDir->m_modTime)
        {
            LOG(VB_FILE, LOG_INFO, QString("%1: Changed directory %2")
                .arg(objectName(), fileInfo.absoluteFilePath()));
            m_db.UpdateDbFile(dir);
        }

        // Remove the entry from the dbList
        m_dbDirMap->remove(dir->m_fileName);
        delete dbDir;
    }
    else
    {
        LOG(VB_FILE, LOG_INFO, QString("%1: New directory %2")
            .arg(objectName(), fileInfo.absoluteFilePath()));

        // The directory is not in the database list
        // add it to the database and get the new id. This
        // will be the new parent id for the subdirectories
        id = m_db.InsertDbDirectory(*dir);
    }
    delete dir;
    return id;
}



/*!
 \brief Updates/populates db for an image/video file
 \details Image is updated if file modified time has changed since last scan.
Extracts orientation, date and 2 comments from exif data
 \param fileInfo File info
 \param parentId Db id of the file's parent dir
 \param baseDirectory The storage group root dir path
*/
void ImageScanThread::SyncFile(QFileInfo fileInfo,
                               int parentId,
                               QString baseDirectory)
{
    // Load all required information of the file
    ImageItem *im = LoadFileData(fileInfo, baseDirectory);

    if (!im)
    {
        LOG(VB_FILE, LOG_DEBUG, QString("%1: Ignoring unknown file %2")
            .arg(objectName(), fileInfo.absoluteFilePath()));
        return;
    }

    // get db version of this file
    ImageItem* oldim = m_dbFileMap->value(im->m_fileName);

    if (oldim && oldim->m_modTime == im->m_modTime)
    {
        // File already known & hasn't changed
        // Remove the entry from the dbList
        m_dbFileMap->remove(im->m_fileName);
        delete oldim;
        delete im;
        return;
    }

    if (oldim)
    {
        LOG(VB_FILE, LOG_INFO, QString("%1: Modified file %2")
            .arg(objectName(), fileInfo.absoluteFilePath()));

        // changed images retain their existing id
        im->m_id = oldim->m_id;

        // Remove the entry from the dbList
        m_dbFileMap->remove(oldim->m_fileName);
        delete oldim;
    }
    else
    {
        LOG(VB_FILE, LOG_INFO, QString("%1: New file %2")
            .arg(objectName(), fileInfo.absoluteFilePath()));

        // new images will be assigned an id by the db AUTO-INCREMENT
        im->m_id = 0;
    }

    // Set the parent.
    im->m_parentId = parentId;

    // Set orientation, date, comment from file meta data
    ImageMetaData::PopulateMetaValues(im);

    // Update db
    m_db.UpdateDbFile(im);

    // Ensure thumbnail exists.
    // Do all top level images asap (they may be needed when scan finishes)
    // Thumb generator now owns image
    ImageThumbPriority thumbPriority = (parentId == ROOT_DB_ID
                         ? kScannerUrgentPriority : kBackgroundPriority);

    ImageThumb::getInstance()->CreateThumbnail(im, thumbPriority);
}


/*!
 \brief Creates metadata from directory info
 \param fileInfo Dir info
 \param parentId Db id of the dir's parent dir
 \param baseDirectory The storage group root dir path
 \return ImageItem New metadata object
*/
ImageItem* ImageScanThread::LoadDirectoryData(QFileInfo fileInfo,
                                              int parentId,
                                              QString baseDirectory)
{
    ImageItem *dirIm = new ImageItem();

    QDir dir(baseDirectory);
    dirIm->m_parentId    = parentId;
    dirIm->m_fileName    = dir.relativeFilePath(fileInfo.absoluteFilePath());
    dirIm->m_name        = fileInfo.fileName();
    dirIm->m_path        = dir.relativeFilePath(fileInfo.absolutePath());
    if (dirIm->m_path.isNull())
        dirIm->m_path = "";
    dirIm->m_modTime     = fileInfo.lastModified().toTime_t();
    dirIm->m_type        = kSubDirectory;

    return dirIm;
}


/*!
 \brief Creates an item for an image file
 \param fileInfo File info
 \param parentId Db id of the file's parent dir
 \param baseDirectory The storage group root dir path
 \return ImageItem New metadata object
*/
ImageItem* ImageScanThread::LoadFileData(QFileInfo fileInfo,
                                         QString baseDirectory)
{
    QString extension = fileInfo.suffix().toLower();
    int type = m_sg->GetImageType(extension);
    if (type == kUnknown)
        return NULL;

    ImageItem *image = new ImageItem();

    QDir baseDir(baseDirectory);
    image->m_fileName  = baseDir.relativeFilePath(fileInfo.absoluteFilePath());
    image->m_name      = fileInfo.fileName();
    image->m_path      = baseDir.relativeFilePath(fileInfo.absolutePath());
    image->m_modTime   = fileInfo.lastModified().toTime_t();
    image->m_size      = fileInfo.size();
    image->m_type      = type;
    image->m_extension = extension;
    image->m_thumbPath = ImageUtils::ThumbPathOf(image);

    return image;
}


ImageScan* ImageScan::m_instance = NULL;

/*!
 \brief Constructor
*/
ImageScan::ImageScan()
{
    m_imageScanThread = new ImageScanThread();
}


/*!
 \brief Destructor
*/
ImageScan::~ImageScan()
{
    if (m_imageScanThread)
    {
        delete m_imageScanThread;
        m_imageScanThread = NULL;
    }
}


/*!
 \brief Get singleton
 \return ImageScan Scanner object
*/
ImageScan* ImageScan::getInstance()
{
    if (!m_instance)
        m_instance = new ImageScan();

    return m_instance;
}


/*!
 \brief Process client requests for start scan, stop scan, clear Db and scan
  progress queries
 \param command Start, stop, clear or query
 \return QStringList ("ERROR", Error message) or
("OK", "SCANNING" | "", "done/total")
*/
QStringList ImageScan::HandleScanRequest(QStringList command)
{
    // Expects command & a single qualifier
    if (command.size() != 2)
        return QStringList("ERROR") << "Bad IMAGE_SCAN";

    if (!m_imageScanThread)
        // Should never happen
        return QStringList("ERROR") << "Scanner is missing";

    if (command[1] == "START")
    {
        // Must be dormant to start a scan
        bool valid = (m_imageScanThread->GetState() == kDormant);

        if (valid)
            m_imageScanThread->ChangeState(kScan);

        return valid ? QStringList("OK") : QStringList("ERROR") << "Scanner is busy";
    }
    else if (command[1] == "STOP")
    {
        // Must be scanning to interrupt
        bool valid = (m_imageScanThread->GetState() == kScan);

        if (valid)
            m_imageScanThread->ChangeState(kInterrupt);

        return valid ? QStringList("OK") : QStringList("ERROR") << "Scan not in progress";
    }
    else if (command[1] == "CLEAR")
    {
        // Must not be already clearing
        bool valid = (m_imageScanThread->GetState() != kClear);

        if (valid)
            m_imageScanThread->ChangeState(kClear);

        return valid ? QStringList("OK") : QStringList("ERROR") << "Clear already in progress";

    }
    else if (command[1] == "QUERY")
    {
        QStringList reply;
        reply << "OK"
              << (m_imageScanThread->isRunning() ? "SCANNING" : "")
              << m_imageScanThread->GetProgress();
        return reply;
    }
    LOG(VB_GENERAL, LOG_ERR, "ImageScanner: Unknown command");
    return QStringList("ERROR") << "Unknown command";
}
