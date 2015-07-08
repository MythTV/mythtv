#include "imagethumbs.h"

#include <exitcodes.h> // for previewgen

#include <QFile>
#include <QDir>
#include <QtAlgorithms>
#include <QImage>
#include <QThread>
#include <QMutexLocker>
#include <QMatrix>

#include <mythdirs.h>
#include <mythsystemlegacy.h>


/*!
 \brief Contstruct request for a single image
 \param action Request action
 \param im Image object that will be deleted.
 \param priority Request priority
 \param notify If true a 'thumbnail exists' event will be broadcast when done.
*/
ThumbTask::ThumbTask(QString action, ImageItem *im,
                     ImageThumbPriority priority, bool notify)
    : m_action(action),
      m_priority(priority),
      m_notify(notify)
{
    append(im);
}


/*!
 \brief Contstruct request for a list of images/dirs
 \param action Request action
 \param list Image objects that will be deleted.
 \param priority Request priority
 \param notify If true a 'thumbnail exists' event will be broadcast when done.
*/
ThumbTask::ThumbTask(QString action, ImageList &list,
                     ImageThumbPriority priority, bool notify)
    : ImageList(list),
      m_action(action),
      m_priority(priority),
      m_notify(notify)
{
    // Assume ownership of list contents
    list.clear();
}


/*!
 \brief  Construct worker thread
*/
ThumbThread::ThumbThread(QString name)
    : MThread(name), m_sg(ImageSg::getInstance())
{
    m_tempDir = QString("%1/%2").arg(GetConfDir(), TEMP_DIR);
    m_thumbDir = m_tempDir.absoluteFilePath(THUMBNAIL_DIR);
    m_tempDir.mkdir(THUMBNAIL_DIR);

    // Use priorities: 0 = image requests, 1 = video requests, 2 = urgent, 3 = background
    for (int i = 0; i <= kBackgroundPriority; ++i)
        m_thumbQueue.insert(static_cast<ImageThumbPriority>(i), new ThumbQueue());

    if (!gCoreContext->IsBackend())
        LOG(VB_GENERAL, LOG_ERR, "Thumbnail Generators MUST be run on a backend");
}


/*!
 \brief  Destructor
*/
ThumbThread::~ThumbThread()
{
    cancel();
    wait();
    qDeleteAll(m_thumbQueue);
}


/*!
 \brief  Handles thumbnail requests by priority
 \details Repeatedly processes next request from highest priority queue until all
 queues are empty, then quits. For Create requests an event is broadcast once the
 thumbnail exists. Dirs are only deleted if empty
 */
void ThumbThread::run()
{
    RunProlog();

    setPriority(QThread::LowestPriority);

    while (true)
    {
        // process next highest-priority task
        ThumbTask *task = NULL;
        {
            QMutexLocker locker(&m_mutex);
            foreach(ThumbQueue *q, m_thumbQueue)
                if (!q->isEmpty())
                {
                    task = q->takeFirst();
                    break;
                }
        }
        // quit when all queues exhausted
        if (!task)
            break;

        // Shouldn't receive empty requests
        if (task->isEmpty())
            continue;

        if (task->m_action == "CREATE")
        {
            ImageItem *im = task->at(0);

            LOG(VB_FILE, LOG_DEBUG, objectName()
                + QString(": Creating %1 (Id %2, priority %3)")
                .arg(im->m_fileName).arg(im->m_id).arg(task->m_priority));

            // Shouldn't receive any dirs or empty thumb lists
            if (im->m_thumbPath.isEmpty())
                continue;

            if (m_tempDir.exists(im->m_thumbPath))

                LOG(VB_FILE, LOG_DEBUG, objectName()
                    + QString(": Thumbnail %1 already exists")
                    .arg(im->m_thumbPath));

            else if (im->m_type == kImageFile)

                CreateImageThumbnail(im);

            else if (im->m_type == kVideoFile)

                CreateVideoThumbnail(im);

            else
                LOG(VB_FILE, LOG_ERR, objectName()
                    + QString(": Can't create thumbnail for type %1 : image %2")
                    .arg(im->m_type).arg(im->m_fileName));

            // notify clients when done
            if (task->m_notify)
            {
                QString id = QString::number(im->m_id);

                // Return requested thumbnails - FE uses it as a message signature
                MythEvent me = MythEvent("THUMB_AVAILABLE", id);
                gCoreContext->SendEvent(me);
            }
        }
        else if (task->m_action == "DELETE")
        {
            foreach(const ImageItem *im, *task)
            {
                if (m_tempDir.remove(im->m_thumbPath))

                    LOG(VB_FILE, LOG_DEBUG, objectName()
                        + QString(": Deleted thumbnail %1")
                        .arg(im->m_fileName));
                else
                    LOG(VB_FILE, LOG_WARNING, objectName()
                        + QString(": Couldn't delete thumbnail %1")
                        .arg(im->m_thumbPath));
            }
        }
        else if (task->m_action == "DELETE_DIR")
        {
            ImageList::const_iterator it = (*task).constEnd();
            while (it != (*task).constBegin())
            {
                ImageItem *im = *(--it);

                if (m_tempDir.rmdir(im->m_thumbPath))

                    LOG(VB_FILE, LOG_DEBUG, objectName()
                        + QString(": Deleted thumbdir %1")
                        .arg(im->m_fileName));
                else
                    LOG(VB_FILE, LOG_WARNING, objectName()
                        + QString(": Couldn't delete thumbdir %1")
                        .arg(im->m_thumbPath));
            }
        }
        else
            LOG(VB_FILE, LOG_ERR, objectName() + QString(": Unknown task %1")
                .arg(task->m_action));

        qDeleteAll(*task);
        delete task;
    }

    RunEpilog();
}


/*!
 \brief Rotates/reflects an image iaw its orientation
 \note Duplicates MythImage::Orientation
 \param im Image details
 \param image Image to be transformed
*/
void ThumbThread::Orientate(ImageItem *im, QImage &image)
{
    QMatrix matrix;
    switch (im->m_orientation)
    {
    case 1: // If the image is in its original state
        break;

    case 2: // The image is horizontally flipped
        image = image.mirrored(true, false);
        break;

    case 3: // The image is rotated 180°
        matrix.rotate(180);
        image = image.transformed(matrix, Qt::SmoothTransformation);
        break;

    case 4: // The image is vertically flipped
        image = image.mirrored(false, true);
        break;

    case 5: // The image is transposed (flipped horizontally, then rotated 90° CCW)
        matrix.rotate(90);
        image = image.transformed(matrix, Qt::SmoothTransformation);
        image = image.mirrored(true, false);
        break;

    case 6: // The image is rotated 90° CCW
        matrix.rotate(90);
        image = image.transformed(matrix, Qt::SmoothTransformation);
        break;

    case 7: // The image is transversed (flipped horizontally, then rotated 90° CW)
        matrix.rotate(270);
        image = image.transformed(matrix, Qt::SmoothTransformation);
        image = image.mirrored(true, false);
        break;

    case 8: // The image is rotated 90° CW
        matrix.rotate(270);
        image = image.transformed(matrix, Qt::SmoothTransformation);
        break;

    default:
        break;
    }
}

/*!
 \brief  Creates a picture thumbnail with the correct size and rotation
 \param  im The image
*/
void ThumbThread::CreateImageThumbnail(ImageItem *im)
{
    QString imagePath = m_sg->GetFilePath(im);

    if (!im->m_path.isEmpty())
        m_thumbDir.mkpath(im->m_path);

    // Absolute path of the BE thumbnail
    QString thumbPath = m_tempDir.absoluteFilePath(im->m_thumbPath);

    QImage image;
    if (!image.load(imagePath))
    {
        LOG(VB_FILE, LOG_ERR, QString("%1: Failed to open image %2")
            .arg(objectName(), imagePath));
        return;
    }

    // Resize & orientate now to optimise load/display time by FE's
    image = image.scaled(QSize(240,180), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    Orientate(im, image);

    // create the thumbnail
    if (image.save(thumbPath))

        LOG(VB_FILE, LOG_INFO, QString("%1: Created thumbnail for %2")
            .arg(objectName(), imagePath));
    else
        LOG(VB_FILE, LOG_ERR, QString("%1: Failed to create thumbnail for %2")
            .arg(objectName(), imagePath));
}


/*!
 \brief  Creates a video preview image with the correct size using mythpreviewgen
 \param  im The image
*/
void ThumbThread::CreateVideoThumbnail(ImageItem *im)
{
    QString videoPath = m_sg->GetFilePath(im);

    if (!im->m_path.isEmpty())
        m_thumbDir.mkpath(im->m_path);

    // Absolute path of the BE thumbnail
    QString thumbPath = m_tempDir.absoluteFilePath(im->m_thumbPath);

    QString cmd = "mythpreviewgen";
    QStringList args;
    args << logPropagateArgs.split(" ", QString::SkipEmptyParts);
    args << "--size 320x240"; // Video thumbnails are shown in slideshow
    args << "--infile"  << QString("\"%1\"").arg(videoPath);
    args << "--outfile" << QString("\"%1\"").arg(thumbPath);

    MythSystemLegacy ms(cmd, args, kMSRunShell);
    ms.SetDirectory(m_thumbDir.absolutePath());
    ms.Run();

    // If the process exited successful
    // then try to load the thumbnail
    if (ms.Wait() != GENERIC_EXIT_OK)
    {
        LOG(VB_FILE, LOG_ERR, QString("%1: Preview Generator failed for %2")
            .arg(objectName(), videoPath));
        return;
    }

    QImage image;
    if (image.load(thumbPath))
    {
        Orientate(im, image);

        image.save(thumbPath);

        LOG(VB_FILE, LOG_INFO, QString("%1: Created thumbnail for %2")
            .arg(objectName(), videoPath));
    }
    else
        LOG(VB_FILE, LOG_ERR, QString("%1: Failed to create thumbnail for %2")
            .arg(objectName(), videoPath));
}


/*!
 \brief Queues a Create request
 \param task The request
 */
void ThumbThread::QueueThumbnails(ThumbTask *task)
{
    // null tasks will terminate the thread prematurely
    if (task)
    {
        QMutexLocker locker(&m_mutex);
        m_thumbQueue.value(task->m_priority)->append(task);

        // restart if not already running
        if (!this->isRunning())
            this->start();
    }
}


/*!
 \brief Return size of a specific queue
 \param priority The queue of interest
 \return int Number of requests pending
*/
int ThumbThread::GetQueueSize(ImageThumbPriority priority)
{
    QMutexLocker locker(&m_mutex);
    ThumbQueue *thumbQueue = m_thumbQueue.value(priority);

    if (thumbQueue)
        return m_thumbQueue.value(priority)->size();

    return 0;
}


/*!
 \brief Clears thumbnail cache
*/
void ThumbThread::ClearThumbnails()
{
    LOG(VB_FILE, LOG_INFO, objectName() + ": Removing all thumbnails");

    // Clear all queues & wait for generator thread to terminate
    cancel();
    wait();

    // Remove all thumbnails
    RemoveDirContents(m_thumbDir.absolutePath());
}


/*!
 \brief Clears all files and sub-dirs within a directory
 \param dirName Dir to clear
 \return bool True on success
*/
bool ThumbThread::RemoveDirContents(QString dirName)
{
    // Delete all files
    QDir dir = QDir(dirName);
    bool result = true;

    foreach(const QFileInfo &info, dir.entryInfoList(QDir::AllEntries
                                                     | QDir::NoDotAndDotDot))
    {
        if (info.isDir())
        {
            RemoveDirContents(info.absoluteFilePath());
            result = dir.rmdir(info.absoluteFilePath());
        }
        else
            result = QFile::remove(info.absoluteFilePath());

        if (!result)
            LOG(VB_FILE, LOG_ERR, QString("%1: Can't delete %2")
                .arg(objectName(), info.absoluteFilePath()));
    }
    return result;
}


/*!
 \brief Clears all queues so that the thread can terminate.
*/
void ThumbThread::cancel()
{
    // Clear all queues
    QMutexLocker locker(&m_mutex);
    foreach(ThumbQueue *q, m_thumbQueue)
    {
        qDeleteAll(*q);
        q->clear();
    }
}


//////////////////////////////////////////////////////////////////////////


//! Thumbnail generator singleton
ImageThumb* ImageThumb::m_instance = NULL;


/*!
 \brief Constructor
*/
ImageThumb::ImageThumb()
{
    m_imageThumbThread = new ThumbThread("ImageThumbGen");
    m_videoThumbThread = new ThumbThread("VideoThumbGen");
}


/*!
 \brief Destructor
*/
ImageThumb::~ImageThumb()
{
    delete m_imageThumbThread;
    m_imageThumbThread = NULL;
    delete m_videoThumbThread;
    m_videoThumbThread = NULL;
}


/*!
 \brief Get generator
 \return ImageThumb Generator singleton
*/
ImageThumb* ImageThumb::getInstance()
{
    if (!m_instance)
        m_instance = new ImageThumb();

    return m_instance;
}


/*!
 \brief Return size of specific queue
 \param priority Queue of interest
 \return int Number of requests pending
*/
int ImageThumb::GetQueueSize(ImageThumbPriority priority)
{
    // Ignore video thread
    if (m_imageThumbThread)
        return m_imageThumbThread->GetQueueSize(priority);
    return 0;
}


/*!
 \brief Clears thumbnail cache, blocking until generator thread terminates
*/
void ImageThumb::ClearAllThumbs()
{
    // Image task will clear videos as well
    if (m_imageThumbThread)
        m_imageThumbThread->ClearThumbnails();
}


/*!
 \brief Creates thumbnails on-demand from clients
 \details Display requests are the highest priority. Thumbnails required for an image
node will be created before those that are part of a directory thumbnail.
A THUMBNAIL_CREATED event is broadcast for each image.
 \param imList List of images requiring thumbnails
*/
void ImageThumb::HandleCreateThumbnails(QStringList imList)
{
    if (imList.size() != 2)
        return;

    bool isForFolder = imList[1].toInt();

    // get specific image details from db
    ImageList images;
    ImageDbWriter db;
    db.ReadDbItemsById(images, imList[0]);

    foreach (ImageItem *im, images)
    {
        ImageThumbPriority priority = isForFolder
                ? kFolderRequestPriority : kPicRequestPriority;

        // notify clients when done; highest priority
        ThumbTask *task = new ThumbTask("CREATE", im, priority, true);

        if (im->m_type == kVideoFile)
        {
            if (m_videoThumbThread)
                m_videoThumbThread->QueueThumbnails(task);
        }
        else if (im->m_type == kImageFile)
        {
            if (m_imageThumbThread)
                m_imageThumbThread->QueueThumbnails(task);
        }
    }
}


/*!
 \brief Remove thumbnails from cache
 \param images List of obselete images
 \param dirs List of obselete dirs
 \return QStringList Csv list of deleted ids, empty (no modified ids), csv list of
 deleted thumbnail and image urls (compatible with FE cache)
*/
QStringList ImageThumb::DeleteThumbs(ImageList images, ImageList dirs)
{
    // Determine affected images and redundant images/thumbnails
    QStringList mesg = QStringList(""); // Empty item (no modified ids)
    QStringList ids;
    ImageSg *isg = ImageSg::getInstance();

    foreach (const ImageItem *im, images)
    {
        ids << QString::number(im->m_id);
        // Remove thumbnail
        mesg << isg->GenerateThumbUrl(im->m_thumbPath);
        // Remove cached image
        mesg << isg->GenerateUrl(im->m_fileName);
    }
    // Insert deleted ids at front
    mesg.insert(0, ids.join(","));

    if (!m_imageThumbThread)
        return QStringList();

    // FIXME: Video thread could be affected
    if (!images.isEmpty())
        // Delete BE thumbs with high priority to prevent future client
        // requests from usurping the Delete and using the old thumbs.
        // Thumb generator now owns the image objects
        m_imageThumbThread->QueueThumbnails(new ThumbTask("DELETE",
                                                          images,
                                                          kPicRequestPriority));
    if (!dirs.isEmpty())
        // Clean up thumbdirs as low priority
        m_imageThumbThread->QueueThumbnails(new ThumbTask("DELETE_DIR", dirs));

    return mesg;
}


/*!
 \brief Creates thumbnails for new images/dirs detected by scanner
 \param im Image
 \param priority Request priority
*/
void ImageThumb::CreateThumbnail(ImageItem *im, ImageThumbPriority priority)
{
    ThumbTask *task = new ThumbTask("CREATE", im, priority);

    if (im->m_type == kVideoFile)
    {
        if (m_videoThumbThread)
            m_videoThumbThread->QueueThumbnails(task);
    }
    else if (im->m_type == kImageFile)
    {
        if (m_imageThumbThread)
            m_imageThumbThread->QueueThumbnails(task);
    }
}
