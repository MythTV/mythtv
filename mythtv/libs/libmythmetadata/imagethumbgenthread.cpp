// Qt headers
#include <QPainter>
#include <QFile>

// MythTV headers
#include "mythcontext.h"
#include "mythdirs.h"
#include "mythuihelper.h"
#include "mythsystemlegacy.h"
#include "exitcodes.h"

#include "imagemetadata.h"
#include "imageutils.h"
#include "imagethumbgenthread.h"

/** \fn     ImageThumbGenThread::ImageThumbGenThread()
 *  \brief  Constructor
 *  \return void
 */
ImageThumbGenThread::ImageThumbGenThread()
        :   m_progressCount(0), m_progressTotalCount(0),
            m_width(0), m_height(0),
            m_pause(false), m_fileListSize(0)
{
    QString sgName = IMAGE_STORAGE_GROUP;
    m_storageGroup = StorageGroup(sgName, gCoreContext->GetHostName());

    if (!gCoreContext->IsMasterBackend())
        LOG(VB_GENERAL, LOG_ERR, "ImageThumbGenThread MUST be run on the master backend");
}



/** \fn     ImageThumbGenThread::~ImageThumbGenThread()
 *  \brief  Destructor
 *  \return void
 */
ImageThumbGenThread::~ImageThumbGenThread()
{
    cancel();
    wait();
}



/** \fn     ImageThumbGenThread::run()
 *  \brief  Called when the thread starts. Tries to generate
 *          thumbnails from the file list until its empty or aborted.
 *  \return void
 */
void ImageThumbGenThread::run()
{
    volatile bool exit = false;

    m_mutex.lock();
    m_fileListSize = m_fileList.size();
    m_mutex.unlock();

    while (!exit)
    {
        ImageMetadata *im = NULL;

        m_mutex.lock();
        if (!m_fileList.isEmpty())
            im = m_fileList.takeFirst();

        // Update the progressbar even if the thumbnail will not be created
        emit UpdateThumbnailProgress(m_fileList.size(), m_fileListSize);
        m_mutex.unlock();

        if (im)
        {
            if (im->m_type == kSubDirectory ||
                im->m_type == kUpDirectory)
            {
                for (int i = 0; i < im->m_thumbFileNameList->size(); ++i)
                    CreateImageThumbnail(im, i);
            }
            else if (im->m_type == kImageFile)
            {
                CreateImageThumbnail(im, 0);
            }
            else if (im->m_type == kVideoFile)
            {
                CreateVideoThumbnail(im);
            }
        }

        delete im;

        m_mutex.lock();
        exit = m_fileList.isEmpty();
        m_mutex.unlock();

        // Allows the thread to be paused when Pause() was called
        m_mutex.lock();
        if (m_pause)
            m_condition.wait(&m_mutex);
        m_mutex.unlock();
    }
}



/** \fn     ImageThumbGenThread::CreateImageThumbnail(ImageMetadata *, int)
 *  \brief  Creates a thumbnail with the correct size and rotation
 *  \param  im The thumbnail details
 *  \param  dataid The id of the thumbnail
 *  \return void
 */
void ImageThumbGenThread::CreateImageThumbnail(ImageMetadata *im, int id)
{
    if (QFile(im->m_thumbFileNameList->at(id)).exists())
        return;

    QDir dir;
    if (!dir.exists(im->m_thumbPath))
        dir.mkpath(im->m_thumbPath);

    QString imageFileName = m_storageGroup.FindFile(im->m_fileName);

    // If a folder thumbnail shall be created we need to get
    // the real filename from the thumbnail filename by removing
    // the configuration directory and the MythImage path
    if (im->m_type == kSubDirectory ||
        im->m_type == kUpDirectory)
    {
        imageFileName = im->m_thumbFileNameList->at(id);
        imageFileName = imageFileName.mid(GetConfDir().append("/MythImage/").count());
    }

    QImage image;
    if (!image.load(imageFileName))
        return;

    QMatrix matrix;
    switch (im->GetOrientation())
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

    case 5: // The image is transposed (rotated 90° CW flipped horizontally)
        matrix.rotate(90);
        image = image.transformed(matrix, Qt::SmoothTransformation);
        image = image.mirrored(true, false);
        break;

    case 6: // The image is rotated 90° CCW
        matrix.rotate(270);
        image = image.transformed(matrix, Qt::SmoothTransformation);
        break;

    case 7: // The image is transversed  (rotated 90° CW and flipped vertically)
        matrix.rotate(90);
        image = image.transformed(matrix, Qt::SmoothTransformation);
        image = image.mirrored(false, true);
        break;

    case 8: // The image is rotated 90° CW
        matrix.rotate(90);
        image = image.transformed(matrix, Qt::SmoothTransformation);
        break;

    default:
        break;
    }

    Resize(image);

    // save the image in the thumbnail directory
    if (image.save(im->m_thumbFileNameList->at(id)))
    {
        QString msg = "IMAGE_THUMB_CREATED %1";
        gCoreContext->SendMessage(msg.arg(im->m_id));
    }
}



/** \fn     ImageThumbGenThread::CreateVideoThumbnail(ImageMetadata *)
 *  \brief  Creates a video preview image with the correct size
 *  \param  im The thumbnail details
 *  \return void
 */
void ImageThumbGenThread::CreateVideoThumbnail(ImageMetadata *im)
{
    if (QFile(im->m_thumbFileNameList->at(0)).exists())
        return;

    QDir dir;
    if (!dir.exists(im->m_thumbPath))
        dir.mkpath(im->m_thumbPath);

    QString videoFileName = m_storageGroup.FindFile(im->m_fileName);

    QString cmd = "mythpreviewgen";
    QStringList args;
    args << logPropagateArgs.split(" ", QString::SkipEmptyParts);
    args << "--infile"  << '"' + videoFileName + '"';
    args << "--outfile" << '"' + im->m_thumbFileNameList->at(0) + '"';

    MythSystemLegacy ms(cmd, args, kMSRunShell);
    ms.SetDirectory(im->m_thumbPath);
    ms.Run();

    // If the process exited successful
    // then try to load the thumbnail
    if (ms.Wait() == GENERIC_EXIT_OK)
    {
        QImage image;
        if (!image.load(im->m_thumbFileNameList->at(0)))
            return;

        Resize(image);

        // save the default image in the thumbnail directory
        if (image.save(im->m_thumbFileNameList->at(0)))
        {
            emit ThumbnailCreated(im, 0);
            QString msg = "IMAGE_THUMB_CREATED %1";
            gCoreContext->SendMessage(msg.arg(im->m_id));
        }
    }
}



/** \fn     ImageThumbGenThread::Resize(QImage)
 *  \brief  Resizes the thumbnail to prevent black areas 
 *          around the image when its shown in a widget.
 *  \param  The image that shall be resized
 *  \return void
 */
void ImageThumbGenThread::Resize(QImage &image)
{
    QSize size = QSize(m_width, m_height);

    image = image.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}



/** \fn     ImageThumbGenThread::AddToThumbnailList(ImageMetadata *)
 *  \brief  Adds a file to the thumbnail list
 *  \param  im The file information
 *  \return void
 */
void ImageThumbGenThread::AddToThumbnailList(ImageMetadata *im)
{
    if (!im)
        return;

    m_mutex.lock();
    m_fileList.append(im);
    m_fileListSize = m_fileList.size();
    m_mutex.unlock();
}



/** \fn     ImageThumbGenThread::RecreateThumbnail(ImageMetadata *)
 *  \brief  Deletes the old thumbnail and creates a new one
 *  \param  im The thumbnail information
 *  \return void
 */
void ImageThumbGenThread::RecreateThumbnail(ImageMetadata *im)
{
    if (!im)
        return;

    if (QFile::remove(im->m_thumbFileNameList->at(0)))
    {
        GetMythUI()->RemoveFromCacheByFile(
                    im->m_thumbFileNameList->at(0));

        AddToThumbnailList(im);
    }
}



/** \fn     ImageThumbGenThread::cancel()
 *  \brief  Clears the thumbnail list so that the thread can exit.
 *  \return void
 */
void ImageThumbGenThread::cancel()
{
    m_mutex.lock();
    while (!m_fileList.isEmpty())
        delete m_fileList.takeFirst();
    m_fileListSize = 0;
    m_mutex.unlock();

    emit UpdateThumbnailProgress(0, 0);
}



/** \fn     ImageThumbGenThread::Pause()
 *  \brief  Pauses the thumbnail generation
 *  \return void
 */
void ImageThumbGenThread::Pause()
{
    m_pause = true;
}



/** \fn     ImageThumbGenThread::Resume()
 *  \brief  Resumes the thumbnail generation
 *  \return void
 */
void ImageThumbGenThread::Resume()
{
    m_condition.wakeAll();
    m_pause = false;
}



/** \fn     ImageThumbGenThread::SetThumbnailSize(int, int)
 *  \brief  Saves and specifies the size of the thumbnails.
 *  \return void
 */
void ImageThumbGenThread::SetThumbnailSize(int width, int height)
{
    if (width > 0)
        m_width = width;

    if (height > 0)
        m_height = height;
}


//////////////////////////////////////////////////////////////////////////


ImageThumbGen* ImageThumbGen::m_instance = NULL;

ImageThumbGen::ImageThumbGen()
{
    m_imageThumbGenThread = new ImageThumbGenThread();
}



ImageThumbGen::~ImageThumbGen()
{
    delete m_imageThumbGenThread;
    m_imageThumbGenThread = NULL;
}



ImageThumbGen* ImageThumbGen::getInstance()
{
    if (!m_instance)
        m_instance = new ImageThumbGen();

    return m_instance;
}



void ImageThumbGen::StartThumbGen()
{
    if (m_imageThumbGenThread && !m_imageThumbGenThread->isRunning())
        m_imageThumbGenThread->start();
}



void ImageThumbGen::StopThumbGen()
{
    if (m_imageThumbGenThread && m_imageThumbGenThread->isRunning())
        m_imageThumbGenThread->cancel();
}



bool ImageThumbGen::ThumbGenIsRunning()
{
    if (m_imageThumbGenThread)
        return m_imageThumbGenThread->isRunning();

    return false;
}



int ImageThumbGen::GetCurrent()
{
    if (m_imageThumbGenThread)
        return m_imageThumbGenThread->m_progressCount;

    return 0;
}



int ImageThumbGen::GetTotal()
{
    if (m_imageThumbGenThread)
        return m_imageThumbGenThread->m_progressTotalCount;

    return 0;
}



bool ImageThumbGen::AddToThumbnailList(ImageMetadata *im)
{
    if (!m_imageThumbGenThread)
        return false;

    m_imageThumbGenThread->AddToThumbnailList(im);

    return true;
}



bool ImageThumbGen::RecreateThumbnail(ImageMetadata *im)
{
    if (!m_imageThumbGenThread)
        return false;

    m_imageThumbGenThread->RecreateThumbnail(im);

    return true;
}



bool ImageThumbGen::SetThumbnailSize(int width, int height)
{
    if (!m_imageThumbGenThread)
        return false;

    m_imageThumbGenThread->SetThumbnailSize(width, height);

    return true;
}
