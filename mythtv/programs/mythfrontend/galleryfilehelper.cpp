
#include "galleryfilehelper.h"

// Qt headers
#include <QString>

// MythTV headers
#include "mythcontext.h"


/** \fn     GalleryFileHelper::GalleryFileHelper()
 *  \brief  Constructor
 *  \return void
 */
GalleryFileHelper::GalleryFileHelper()
{
}


/** \fn     GalleryFileHelper::~GalleryFileHelper()
 *  \brief  Destructor
 *  \return void
 */
GalleryFileHelper::~GalleryFileHelper()
{
}


/** \fn     GalleryFileHelper::StartSyncImages()
 *  \brief  Starts the image syncronization from the backend
 *  \return void
 */
void GalleryFileHelper::StartSyncImages()
{
    QStringList strList;
    strList << "IMAGE_SCAN" << "START";

    if (!gCoreContext->SendReceiveStringList(strList))

        LOG(VB_GENERAL, LOG_ERR, "Sync start request failed");
}


/** \fn     GalleryFileHelper::StopSyncImages()
 *  \brief  Stops the image syncronization from the backend
 *  \return void
 */
void GalleryFileHelper::StopSyncImages()
{
    QStringList strList;
    strList << "IMAGE_SCAN" << "STOP";

    if (!gCoreContext->SendReceiveStringList(strList))

        LOG(VB_GENERAL, LOG_ERR, "Sync stop request failed");
}


/** \fn     GalleryFileHelper::GetSyncStatus()
 *  \brief  Reads the current image syncronization status
 *  \return Struct with bool running, int current image, int total images
 */
GallerySyncStatus GalleryFileHelper::GetSyncStatus()
{
    GallerySyncStatus status;
    status.running = false;
    status.current = 0;
    status.total = 0;

    QStringList strList;

    strList << "IMAGE_GET_SCAN_STATUS";
    bool ok = gCoreContext->SendReceiveStringList(strList);

    // expect request status, sync-running status, number done, total number
    if (ok && strList.size() == 4)
    {
        status.running = strList[1].toInt(); // convert int to bool
        status.current = strList[2].toInt();
        status.total   = strList[3].toInt();
    }

    return status;
}


void GalleryFileHelper::AddToThumbnailList(ImageMetadata* im, bool recreate)
{
    if (!im || im->m_thumbFileIdList.size() == 0)
        return;

    QStringList message;
    message << "IMAGE_THUMBNAILS"
            << QString::number(recreate);

    // folders use multiple thumbnails
    for (int i = 0; i < im->m_thumbFileIdList.size(); i++)

        message << QString::number(im->m_thumbFileIdList.at(i));

    gCoreContext->SendReceiveStringList(message, true);
}


/** \fn     GalleryFileHelper::RenameFile(const int &, const QString &)
 *  \brief  Renames the file via the service api
 *  \param  im The image metadata object that contains all required information
 *  \param  name The new name of the file (only the filename, no path)
 *  \return True if removal was successful, otherwise false
 */
bool GalleryFileHelper::RenameFile(ImageMetadata *im, const QString &name)
{
    QStringList strlist;
    strlist << "IMAGE_RENAME"
            << QString::number(im->m_id)
            << name;

    return gCoreContext->SendReceiveStringList(strlist, true);
}


/** \fn     GalleryFileHelper::RemoveFile(ImageMetadata *)
 *  \brief  Deletes the file via the service api
 *  \param  im The image metadata object that contains all required information
 *  \return True if removal was successful, otherwise false
 */
bool GalleryFileHelper::RemoveFile(ImageMetadata *im)
{
    QStringList strlist;
    strlist << "IMAGE_DELETE"
            << QString::number(im->m_id);

    return gCoreContext->SendReceiveStringList(strlist, true);
}


/** \fn     GalleryFileHelper::SetImageOrientation(ImageMetadata *)
 *  \brief  Saves the given value in the orientation exif tag
 *  \param  im The image metadata object that contains all required information
 *  \return True if saving the orientation was successful, otherwise false
 */
bool GalleryFileHelper::SetImageOrientation(ImageMetadata *im)
{
    QStringList strlist;
    strlist << "IMAGE_SET_EXIF"
            << QString::number(im->m_id)
            << "ORIENTATION"
            << QString::number(im->GetOrientation());

    return gCoreContext->SendReceiveStringList(strlist, true);
}


/** \fn     GalleryFileHelper::GetExifValues(ImageMetadata *)
 *  \brief  Returns the XML data that contains all available exif header
            tags and values from the image specified by the id.
 *  \param  im The image metadata object that contains all required information
 *  \return The returned XML data
 */
QMap<QString, QString> GalleryFileHelper::GetExifValues(ImageMetadata *im)
{
    QStringList strlist;
    strlist << "IMAGE_GET_EXIF"
            << QString::number(im->m_id);

    QMap<QString, QString> properties;

    if (gCoreContext->SendReceiveStringList(strlist, true))
    {
        // Each string contains a Label<seperator>Value
        QString seperator = strlist[1];
        for (int i = 2; i < strlist.size(); ++i)
        {
            QStringList parts = strlist[i].split(seperator);
            properties.insert(parts[0], parts[1]);
        }
    }
    return properties;
}
