// Qt headers

// MythTV headers
#include "mythcontext.h"
#include "storagegroup.h"
#include "imagescanthread.h"
#include "imageutils.h"



/** \fn     ImageScanThread::ImageScanThread()
 *  \brief  Constructor
 *  \return void
 */
ImageScanThread::ImageScanThread() : MThread("ImageScanThread")
{
    LOG(VB_GENERAL, LOG_INFO, QString("ImageScanThread::ctor"));

    // initialize all required data structures
    m_dbDirList   = new QMap<QString, ImageMetadata *>;
    m_dbFileList  = new QMap<QString, ImageMetadata *>;
    m_continue = false;

    m_progressCount       = 0;
    m_progressTotalCount  = 0;
}



/** \fn     ImageScanThread::~ImageScanThread()
 *  \brief  Destructor
 *  \return void
 */
ImageScanThread::~ImageScanThread()
{
    LOG(VB_GENERAL, LOG_INFO, QString("ImageScanThread::dtor"));

    if (m_dbDirList)
    {
        delete m_dbDirList;
        m_dbDirList = NULL;
    }

    if (m_dbFileList)
    {
        delete m_dbFileList;
        m_dbFileList = NULL;
    }
}



/** \fn     ImageScanThread::run()
 *  \brief  Called when the thread is started. Loads all storage groups files
 *          and directories and also from the database and syncronizes them.
 *  \return void
 */
void ImageScanThread::run()
{
    RunProlog();

    if (!m_continue)
    {
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("Image scanning thread not allowed to start."));
        return;
    }

    LOG(VB_GENERAL, LOG_DEBUG, QString("Syncronization started"));

    m_progressCount       = 0;
    m_progressTotalCount  = 0;
    
    // Load all available directories and files from the database so that
    // they can be compared against the ones on the filesystem.
    ImageUtils *iu = ImageUtils::getInstance();
    iu->LoadDirectoriesFromDB(m_dbDirList);
    iu->LoadFilesFromDB(m_dbFileList);

    QStringList paths = iu->GetStorageDirs();

    // Get the total list of files and directories that will be synced.
    // This is only an additional information that the themer can show.
    for (int i = 0; i < paths.size(); ++i)
    {
        QString path = paths.at(i);
        QDirIterator it(path, QDirIterator::Subdirectories);

        while(it.hasNext())
        {
            it.next();
            ++m_progressTotalCount;
        }
    }

    // Now start the actual syncronization
    for (int i = 0; i < paths.size(); ++i)
    {
        QString path = paths.at(i);
        SyncFilesFromDir(path, 0);
    }

    // Adding or updating directories have been completed.
    // The directory list still contains the remaining directories
    // that are not in the filesystem anymore. Remove them from the database
    QMap<QString, ImageMetadata *>::iterator i;
    for (i = m_dbDirList->begin(); i != m_dbDirList->end(); ++i)
    {
        iu->RemoveDirectoryFromDB(m_dbDirList->value(i.key()));
    }

    // Repeat the same for the file list.
    for (i = m_dbFileList->begin(); i != m_dbFileList->end(); ++i)
    {
        iu->RemoveFileFromDB(m_dbFileList->value(i.key()));
    }

    m_continue = false;
    m_progressCount       = 0;
    m_progressTotalCount  = 0;

    RunEpilog();
}



/** \fn     ImageScanThread::SyncFilesFromDir(QString &, int)
 *  \brief  Loads all available files from the path on the
 *          backend and syncs depending if they are a directory or file
 *  \param  path The current directory with the files that shall be scanned syncronized
 *  \param  parentId The id of the parent directory which is required for possible subdirectories
 *  \return void
 */
void ImageScanThread::SyncFilesFromDir(QString &path, int parentId)
{
    if (!m_continue)
    {
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("Syncing from SG dir %1 interrupted").arg(path));
        return;
    }

    LOG(VB_GENERAL, LOG_DEBUG,
        QString("Syncing from SG dir %1").arg(path));

    QDir dir(path);
    if (!dir.exists())
        return;

    // Only get files and dirs, no special and hidden stuff
    dir.setFilter(QDir::Dirs | QDir::Files |
                  QDir::NoDotAndDotDot | QDir::NoSymLinks);
    QFileInfoList list = dir.entryInfoList();
    if (list.isEmpty())
        return;

    for (QFileInfoList::iterator it = list.begin(); it != list.end(); ++it)
    {
        if (!m_continue)
        {
            LOG(VB_GENERAL, LOG_DEBUG,
                QString("Syncing from SG dir %1 interrupted").arg(path));
            return;
        }

        QFileInfo fileInfo = *it;
        if (fileInfo.isDir())
        {
            // Get the id. This will be new parent id
            // when we traverse down the current directory.
            int id = SyncDirectory(fileInfo, parentId);

            // Get new files within this directory
            QString fileName = fileInfo.absoluteFilePath();
            SyncFilesFromDir(fileName, id);
        }
        else
        {
            SyncFile(fileInfo, parentId);
        }

        // Increase the current progress count in case a
        // progressbar is used to show the sync progress
        if (m_progressTotalCount > m_progressCount)
            ++m_progressCount;
    }
}



/** \fn     ImageScanThread::SyncDirectory(QFileInfo &, int)
 *  \brief  Syncronizes a directory with the database.
 *          Either inserts or deletes the information in the database.
 *  \param  fileInfo The information of the directory
 *  \param  parentId The parent directory which will be saved with the file
 *  \return void
 */
int ImageScanThread::SyncDirectory(QFileInfo &fileInfo, int parentId)
{
    LOG(VB_GENERAL, LOG_DEBUG, QString("Syncing directory %1")
        .arg(fileInfo.absoluteFilePath()));

    ImageMetadata *im = new ImageMetadata();

    if (!m_dbDirList->contains(fileInfo.absoluteFilePath()))
    {
        // Load all required information of the directory
        ImageUtils *iu = ImageUtils::getInstance();
        iu->LoadDirectoryData(fileInfo, im, parentId);

        // The directory is not in the database list
        // add it to the database and get the new id. This
        // will be the new parent id for the subdirectories
        im->m_id = iu->InsertDirectoryIntoDB(im);
    }
    else
    {
        // The directory exists in the db list
        // Get the id which will be the new
        // parent id for the subdirectories
        im->m_id = m_dbDirList->value(fileInfo.absoluteFilePath())->m_id;

        // Remove the entry from the dbList
        // so we don't need to search again
        m_dbDirList->remove(fileInfo.absoluteFilePath());
    }

    int id = im->m_id;
    delete im;

    return id;
}



/** \fn     ImageScanThread::SyncFile(QFileInfo &, int)
 *  \brief  Syncronizes a file with the database. Either inserts,
 *          updates or deletes the information in the database.
 *  \param  fileInfo The information of the file
 *  \param  parentId The parent directory which will be saved with the file
 *  \return void
 */
void ImageScanThread::SyncFile(QFileInfo &fileInfo, int parentId)
{
    LOG(VB_GENERAL, LOG_DEBUG, QString("Syncing file %1")
        .arg(fileInfo.absoluteFilePath()));

    if (!m_dbFileList->contains(fileInfo.absoluteFilePath()))
    {
        ImageMetadata *im = new ImageMetadata();

        // Load all required information of the file
        ImageUtils *iu = ImageUtils::getInstance();
        iu->LoadFileData(fileInfo, im);

        // Only load the file if contains a valid file extension
        LOG(VB_GENERAL, LOG_INFO, QString("Type of file %1 is %2, extension %3").arg(im->m_fileName).arg(im->m_type).arg(im->m_extension));
        if (im->m_type != kUnknown)
        {
            // Load any required exif information if the file is an image
            if (im->m_type == kImageFile)
            {
                bool ok;

                int exifOrientation = iu->GetExifOrientation(im->m_fileName, &ok);
                if (ok)
                    im->SetOrientation(exifOrientation, true);

                int exifDate = iu->GetExifDate(im->m_fileName, &ok);
                if (ok)
                    im->m_date = exifDate;
            }

            // Load the parent id. This is the id of the file's path
            im->m_parentId = parentId;

            // The file is not in the database list
            // add it to the database.
            im->m_id = iu->InsertFileIntoDB(im);
        }
        delete im;
    }
    else
    {
        // Remove the entry from the dbList
        // so we don't need to search again
        m_dbFileList->remove(fileInfo.absoluteFilePath());
    }
}
