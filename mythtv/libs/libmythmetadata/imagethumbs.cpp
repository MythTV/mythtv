#include "imagethumbs.h"

#include <QDir>
#include <QStringList>

#include "libmythbase/mythcorecontext.h"  // for MYTH_APPNAME_MYTHPREVIEWGEN
#include "libmythbase/mythdirs.h"         // for GetAppBinDir
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythsystemlegacy.h"
#include "libmythui/mythimage.h"

#include "imagemetadata.h"

/*!
 \brief Destructor
*/
template <class DBFS>
ThumbThread<DBFS>::~ThumbThread()
{
    cancel();
    wait();
}


/*!
 \brief Clears all queues so that the thread will terminate.
*/
template <class DBFS>
void ThumbThread<DBFS>::cancel()
{
    // Clear all queues
    QMutexLocker locker(&m_mutex);
    m_requestQ.clear();
    m_backgroundQ.clear();
}


/*!
 \brief Queues a Create request
 \param task The request
*/
template <class DBFS>
void ThumbThread<DBFS>::Enqueue(const TaskPtr &task)
{
    if (task)
    {
        bool background = task->m_priority > kBackgroundPriority;

        QMutexLocker locker(&m_mutex);
        if (background)
            m_backgroundQ.insert(task->m_priority, task);
        else
            m_requestQ.insert(task->m_priority, task);

        // restart if not already running
        if ((m_doBackground || !background) && !this->isRunning())
            this->start();
    }
}


/*!
 \brief Clears thumbnail request queue
 \warning May block for several seconds
*/
template <class DBFS>
void ThumbThread<DBFS>::AbortDevice(int devId, const QString &action)
{
    if (action == "DEVICE CLOSE ALL" || action == "DEVICE CLEAR ALL")
    {
        if (isRunning())
            LOG(VB_FILE, LOG_INFO,
                QString("Aborting all thumbnails %1").arg(action));

        // Abort thumbnail generation for all devices
        cancel();
        return;
    }

    LOG(VB_FILE, LOG_INFO, QString("Aborting thumbnails (%1) for '%2'")
        .arg(action).arg(m_dbfs.DeviceName(devId)));

    // Remove all tasks for specific device from every queue
    QMutexLocker locker(&m_mutex);
    RemoveTasks(m_requestQ, devId);
    RemoveTasks(m_backgroundQ, devId);
    if (isRunning())
        // Wait until current task is complete - it may be using the device
        m_taskDone.wait(&m_mutex, 3000);
}


#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
#define QMutableMultiMapIterator QMutableMapIterator
#endif

/*!
  /brief Removes all tasks for a device from a task queue
 */
template <class DBFS>
void ThumbThread<DBFS>::RemoveTasks(ThumbQueue &queue, int devId)
{
    QMutableMultiMapIterator<int, TaskPtr> it(queue);
    while (it.hasNext())
    {
        it.next();
        TaskPtr task = it.value();
        // All thumbs in a task come from same device
        if (task && !task->m_images.isEmpty()
                && task->m_images.at(0)->m_device == devId)
            it.remove();
    }
}


/*!
 \brief  Handles thumbnail requests by priority
 \details Repeatedly processes next request from highest priority queue until all
  queues are empty, then quits. For Create requests an event is broadcast once the
  thumbnail exists. Dirs are only deleted if empty
*/
template <class DBFS>
void ThumbThread<DBFS>::run()
{
    RunProlog();

    setPriority(QThread::LowestPriority);

    while (true)
    {
        // Signal previous task is complete (its files have been closed)
        m_taskDone.wakeAll();

        // Do all we can to run in background
        QThread::yieldCurrentThread();

        // process next highest-priority task
        TaskPtr task;
        {
            QMutexLocker locker(&m_mutex);
            if (!m_requestQ.isEmpty())
                task = m_requestQ.take(m_requestQ.constBegin().key());
            else if (m_doBackground && !m_backgroundQ.isEmpty())
                task = m_backgroundQ.take(m_backgroundQ.constBegin().key());
            else
                // quit when both queues exhausted
                break;
        }

        // Shouldn't receive empty requests
        if (task->m_images.isEmpty())
            continue;

        if (task->m_action == "CREATE")
        {
            ImagePtrK im = task->m_images.at(0);

            QString err = CreateThumbnail(im, task->m_priority);

            if (!err.isEmpty())
            {
                LOG(VB_GENERAL, LOG_ERR,  QString("%1").arg(err));
            }
            else if (task->m_notify)
            {
                // notify clients when done
                m_dbfs.Notify("THUMB_AVAILABLE",
                              QStringList(QString::number(im->m_id)));
            }
        }
        else if (task->m_action == "DELETE")
        {
            for (const auto& im : std::as_const(task->m_images))
            {
                QString thumbnail = im->m_thumbPath;
                if (!QDir::root().remove(thumbnail))
                {
                    LOG(VB_FILE, LOG_WARNING,
                        QString("Failed to delete thumbnail %1").arg(thumbnail));
                    continue;
                }
                LOG(VB_FILE, LOG_DEBUG,
                    QString("Deleted thumbnail %1").arg(thumbnail));

                // Clean up empty dirs
                QString path = QFileInfo(thumbnail).path();
                if (QDir::root().rmpath(path))
                    LOG(VB_FILE, LOG_DEBUG,
                        QString("Cleaned up path %1").arg(path));
            }
        }
        else if (task->m_action == "MOVE")
        {
            for (const auto& im : std::as_const(task->m_images))
            {
                // Build new thumb path
                QString newThumbPath =
                        m_dbfs.GetAbsThumbPath(m_dbfs.ThumbDir(im->m_device),
                                               m_dbfs.ThumbPath(*im.data()));

                // Ensure path exists
                if (QDir::root().mkpath(QFileInfo(newThumbPath).path())
                        && QFile::rename(im->m_thumbPath, newThumbPath))
                {
                    LOG(VB_FILE, LOG_DEBUG, QString("Moved thumbnail %1 -> %2")
                        .arg(im->m_thumbPath, newThumbPath));
                }
                else
                {
                    LOG(VB_FILE, LOG_WARNING,
                        QString("Failed to rename thumbnail %1 -> %2")
                        .arg(im->m_thumbPath, newThumbPath));
                    continue;
                }

                // Clean up empty dirs
                QString path = QFileInfo(im->m_thumbPath).path();
                if (QDir::root().rmpath(path))
                    LOG(VB_FILE, LOG_DEBUG,
                        QString("Cleaned up path %1").arg(path));
            }
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Unknown task %1").arg(task->m_action));
        }
    }

    RunEpilog();
}


/*!
 \brief Generate thumbnail for an image
 \param im Image
 \param thumbPriority 
 */
template <class DBFS>
QString ThumbThread<DBFS>::CreateThumbnail(const ImagePtrK &im, int thumbPriority)
{
    if (QDir::root().exists(im->m_thumbPath))
    {
        LOG(VB_FILE, LOG_DEBUG,  QString("[%3] %2 already exists")
            .arg(im->m_thumbPath).arg(thumbPriority));
        return {}; // Notify anyway
    }

    // Local filenames are always absolute
    // Remote filenames are absolute from the scanner only
    // UI requests (derived from Db) are relative
    QString imagePath = m_dbfs.GetAbsFilePath(im);
    if (imagePath.isEmpty())
        return QString("Empty image path: %1").arg(im->m_filePath);

    // Ensure path exists
    QDir::root().mkpath(QFileInfo(im->m_thumbPath).path());

    QImage image;
    if (im->m_type == kImageFile)
    {
        if (!image.load(imagePath))
            return QString("Failed to open image %1").arg(imagePath);

        // Resize to optimise load/display time by FE's
        image = image.scaled(QSize(240,180), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    else if (im->m_type == kVideoFile)
    {
        // Run Preview Generator in foreground
        QString cmd = GetAppBinDir() + MYTH_APPNAME_MYTHPREVIEWGEN;
        QStringList args;
        args << "--size 320x240"; // Video thumbnails are also shown in slideshow
        args << QString("--infile '%1'").arg(imagePath);
        args << QString("--outfile '%1'").arg(im->m_thumbPath);

        MythSystemLegacy ms(cmd, args,
                            kMSRunShell           |
                            kMSDontBlockInputDevs |
                            kMSDontDisableDrawing |
                            kMSProcessEvents      |
                            kMSAutoCleanup        |
                            kMSPropagateLogs);
        ms.SetNice(10);
        ms.SetIOPrio(7);
        ms.Run(30s);
        if (ms.Wait() != GENERIC_EXIT_OK)
        {
            LOG(VB_GENERAL, LOG_ERR,  QString("Failed to run %2 %3")
                .arg(cmd, args.join(" ")));
            return QString("Preview Generator failed for %1").arg(imagePath);
        }

        if (!image.load(im->m_thumbPath))
            return QString("Failed to open preview %1").arg(im->m_thumbPath);
    }
    else
    {
        return QString("Can't create thumbnail for type %1 (image %2)")
                .arg(im->m_type).arg(imagePath);
    }

    // Compensate for any Qt auto-orientation
    int orientBy = Orientation(im->m_orientation)
            .GetCurrent(im->m_type == kImageFile);

    // Orientate now to optimise load/display time - no orientation
    // is required when displaying thumbnails
    image = MythImage::ApplyExifOrientation(image, orientBy);

    // Create the thumbnail
    if (!image.save(im->m_thumbPath))
        return QString("Failed to create thumbnail %1").arg(im->m_thumbPath);

    LOG(VB_FILE, LOG_INFO,  QString("[%2] Created %1")
        .arg(im->m_thumbPath).arg(thumbPriority));
    return {};
}


/*!
  \brief Pauses or restarts processing of background tasks (scanner requests)
 */
template <class DBFS>
void ThumbThread<DBFS>::PauseBackground(bool pause)
{
    QMutexLocker locker(&m_mutex);
    m_doBackground = !pause;

    // restart if not already running
    if (m_doBackground && !this->isRunning())
        this->start();
}


/*!
 \brief Constructor
 */
template <class DBFS>
ImageThumb<DBFS>::ImageThumb(DBFS *const dbfs)
    : m_dbfs(*dbfs),
      m_imageThread(new ThumbThread<DBFS>("ImageThumbs", dbfs)),
      m_videoThread(new ThumbThread<DBFS>("VideoThumbs", dbfs))
{}


/*!
 \brief Destructor
 \*/
template <class DBFS>
ImageThumb<DBFS>::~ImageThumb()
{
    delete m_imageThread;
    delete m_videoThread;
    m_imageThread = m_videoThread = nullptr;
}


/*!
 \brief Clears thumbnails for a device
 \param devId Device identity
 \param action [DEVICE] (CLOSE | CLEAR)
 \warning May block for several seconds
*/
template <class DBFS>
void ImageThumb<DBFS>::ClearThumbs(int devId, const QString &action)
{
    // Cancel pending requests for the device
    // Waits for current generator task to complete
    if (m_imageThread)
        m_imageThread->AbortDevice(devId, action);
    if (m_videoThread)
        m_videoThread->AbortDevice(devId, action);

    // Remove devices now they are not in use
    QStringList mountPaths = m_dbfs.CloseDevices(devId, action);
//    if (mountPaths.isEmpty())
//        return;

    // Generate file & thumbnail urls (as per image cache) of mountpoints
    QStringList mesg;
    for (const auto& mount : std::as_const(mountPaths))
        mesg << m_dbfs.MakeFileUrl(mount)
             << m_dbfs.MakeThumbUrl(mount);

    // Notify clients to clear cache
    m_dbfs.Notify("IMAGE_DEVICE_CHANGED", mesg);
}


/*!
 \brief Remove specific thumbnails
 \param images List of obselete images
 \return QString Csv list of deleted ids
*/
template <class DBFS>
QString ImageThumb<DBFS>::DeleteThumbs(const ImageList &images)
{
    // Determine affected images and redundant images/thumbnails
    QStringList ids;

    // Pictures & videos are deleted by their own threads
    ImageListK pics;
    ImageListK videos;
    for (const auto& im : std::as_const(images))
    {
        if (im->m_type == kVideoFile)
            videos.append(im);
        else
            pics.append(im);

        ids << QString::number(im->m_id);
    }

    if (!pics.isEmpty() && m_imageThread)
        m_imageThread->Enqueue(TaskPtr(new ThumbTask("DELETE", pics)));
    if (!videos.isEmpty() && m_videoThread)
        m_videoThread->Enqueue(TaskPtr(new ThumbTask("DELETE", videos)));
    return ids.join(",");
}


/*!
 \brief Creates a thumbnail
 \param im Image
 \param priority Request priority
 \param notify If true a THUMB_AVAILABLE event will be generated
*/
template <class DBFS>
void ImageThumb<DBFS>::CreateThumbnail(const ImagePtrK &im, int priority,
                                       bool notify)
{
    if (!im)
        return;

    // Allocate task a (hopefully) unique priority
    if (priority == kBackgroundPriority)
        priority = Priority(*im);

    TaskPtr task(new ThumbTask("CREATE", im, priority, notify));

    if (im->m_type == kImageFile && m_imageThread)
    {
        m_imageThread->Enqueue(task);
    }
    else if (im->m_type == kVideoFile && m_videoThread)
    {
        m_videoThread->Enqueue(task);
    }
    else
    {
        LOG(VB_FILE, LOG_INFO, QString("Ignoring create thumbnail %1, type %2")
            .arg(im->m_id).arg(im->m_type));
    }
}


/*!
 \brief Renames a thumbnail
 \param im Image
*/
template <class DBFS>
void ImageThumb<DBFS>::MoveThumbnail(const ImagePtrK &im)
{
    if (!im)
        return;

    TaskPtr task(new ThumbTask("MOVE", im));

    if (im->m_type == kImageFile && m_imageThread)
    {
        m_imageThread->Enqueue(task);
    }
    else if (im->m_type == kVideoFile && m_videoThread)
    {
        m_videoThread->Enqueue(task);
    }
    else
    {
        LOG(VB_FILE, LOG_INFO, QString("Ignoring move thumbnail %1, type %2")
            .arg(im->m_id).arg(im->m_type));
    }
}


/*!
  \brief Pauses or restarts processing of background tasks (scanner requests)
 */
template <class DBFS>
void ImageThumb<DBFS>::PauseBackground(bool pause)
{
    LOG(VB_FILE, LOG_INFO,  QString("Paused %1").arg(pause));

    if (m_imageThread)
        m_imageThread->PauseBackground(pause);
    if (m_videoThread)
        m_videoThread->PauseBackground(pause);
}


// Must define the valid template implementations to generate code for the
// instantiations (as they are defined in the cpp rather than header).
// Otherwise the linker will fail with undefined references...
#include "imagemanager.h"
template class ImageThumb<ImageDbLocal>;
template class ImageThumb<ImageDbSg>;
