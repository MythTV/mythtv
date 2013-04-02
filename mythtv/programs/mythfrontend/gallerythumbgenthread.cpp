// Qt headers
#include <QPainter>
#include <QFile>

// MythTV headers
#include "mythcontext.h"
#include "mythdirs.h"
#include "mythuihelper.h"
#include "mythsystem.h"
#include "exitcodes.h"

#include "imagemetadata.h"
#include "gallerythumbgenthread.h"



/** \fn     GalleryThumbGenThread::GalleryThumbGenThread()
 *  \brief  Constructor
 *  \return void
 */
GalleryThumbGenThread::GalleryThumbGenThread()
{
    m_fileHelper = new GalleryFileHelper();
    m_dbHelper = new GalleryDatabaseHelper();

    m_fileListSize = 0;
    m_pause = false;
}



/** \fn     GalleryThumbGenThread::~GalleryThumbGenThread()
 *  \brief  Destructor
 *  \return void
 */
GalleryThumbGenThread::~GalleryThumbGenThread()
{
    cancel();
    wait();

    if (m_fileHelper)
    {
        delete m_fileHelper;
        m_fileHelper = NULL;
    }

    if (m_dbHelper)
    {
        delete m_dbHelper;
        m_dbHelper = NULL;
    }
}



/** \fn     GalleryThumbGenThread::run()
 *  \brief  Called when the thread starts. Tries to generate
 *          thumbnails from the file list until its empty or aborted.
 *  \return void
 */
void GalleryThumbGenThread::run()
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



/** \fn     GalleryThumbGenThread::CreateImageThumbnail(ImageMetadata *, int)
 *  \brief  Creates a thumbnail with the correct size and rotation
 *  \param  im The thumbnail details
 *  \param  dataid The id of the thumbnail
 *  \return void
 */
void GalleryThumbGenThread::CreateImageThumbnail(ImageMetadata *im, int id)
{
    if (QFile(im->m_thumbFileNameList->at(id)).exists())
        return;

    QDir dir;
    if (!dir.exists(im->m_thumbPath))
        dir.mkpath(im->m_thumbPath);

    QString imageFileName = im->m_fileName;

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
        emit ThumbnailCreated(im, id);
}



/** \fn     GalleryThumbGenThread::CreateVideoThumbnail(ImageMetadata *)
 *  \brief  Creates a video preview image with the correct size
 *  \param  im The thumbnail details
 *  \return void
 */
void GalleryThumbGenThread::CreateVideoThumbnail(ImageMetadata *im)
{
    if (QFile(im->m_thumbFileNameList->at(0)).exists())
        return;

    QDir dir;
    if (!dir.exists(im->m_thumbPath))
        dir.mkpath(im->m_thumbPath);

    QString cmd = "mythpreviewgen";
    QStringList args;
    args << logPropagateArgs.split(" ", QString::SkipEmptyParts);
    args << "--infile"  << '"' + im->m_fileName + '"';
    args << "--outfile" << '"' + im->m_thumbFileNameList->at(0) + '"';

    MythSystem ms(cmd, args, kMSRunShell);
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
            emit ThumbnailCreated(im, 0);
    }
}



/** \fn     GalleryThumbGenThread::Resize(QImage)
 *  \brief  Resizes the thumbnail to prevent black areas 
 *          around the image when its shown in a widget.
 *  \param  The image that shall be resized
 *  \return void
 */
void GalleryThumbGenThread::Resize(QImage &image)
{
    // If the factor of the width to height of the image is smaller
    // than of the widget stretch the image horizontally. The image
    // will be higher then the widgets height, so it needs to be cropped.
    if ((image.width() / image.height()) < (m_width / m_height))
    {
        image = image.scaledToWidth(m_width, Qt::SmoothTransformation);

        // Copy a part of the image so that
        // the copied area has the size of the widget.
        if (image.height() > m_height)
        {
            int offset = (image.height() - m_height) / 2;
            image = image.copy(0, offset, m_width, m_height);
        }
    }
    else
    {
        image = image.scaledToHeight(m_height, Qt::SmoothTransformation);

        if (image.width() > m_width)
        {
            int offset = (image.width() - m_width) / 2;
            image = image.copy(offset, 0, m_width, m_height);
        }
    }
}



/** \fn     GalleryThumbGenThread::AddToThumbnailList(ImageMetadata *)
 *  \brief  Adds a file to the thumbnail list
 *  \param  im The file information
 *  \return void
 */
void GalleryThumbGenThread::AddToThumbnailList(ImageMetadata *im)
{
    if (!im)
        return;

    m_mutex.lock();
    m_fileList.append(im);
    m_fileListSize = m_fileList.size();
    m_mutex.unlock();
}



/** \fn     GalleryThumbGenThread::RecreateThumbnail(ImageMetadata *)
 *  \brief  Deletes the old thumbnail and creates a new one
 *  \param  im The thumbnail information
 *  \return void
 */
void GalleryThumbGenThread::RecreateThumbnail(ImageMetadata *im)
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



/** \fn     GalleryThumbGenThread::cancel()
 *  \brief  Clears the thumbnail list so that the thread can exit.
 *  \return void
 */
void GalleryThumbGenThread::cancel()
{
    m_mutex.lock();
    m_fileList.clear();
    m_fileListSize = 0;
    m_mutex.unlock();

    emit UpdateThumbnailProgress(0, 0);
}



/** \fn     GalleryThumbGenThread::Pause()
 *  \brief  Pauses the thumbnail generation
 *  \return void
 */
void GalleryThumbGenThread::Pause()
{
    m_pause = true;
}



/** \fn     GalleryThumbGenThread::Resume()
 *  \brief  Resumes the thumbnail generation
 *  \return void
 */
void GalleryThumbGenThread::Resume()
{
    m_condition.wakeAll();
    m_pause = false;
}



/** \fn     GalleryThumbGenThread::SetThumbnailSize(int, int)
 *  \brief  Saves and specifies the size of the thumbnails.
 *  \return void
 */
void GalleryThumbGenThread::SetThumbnailSize(int width, int height)
{
    if (width > 0)
        m_width = width;

    if (height > 0)
        m_height = height;
}
