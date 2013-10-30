
#include "galleryfilehelper.h"

// Qt headers
#include <QNetworkAccessManager>
#include <QXmlStreamReader>
#include <QNetworkReply>
#include <QEventLoop>

#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
#   include <QUrlQuery>
#endif

// MythTV headers
#include "mythcontext.h"
#include "storagegroup.h"
#include "remoteutil.h"

#include "gallerytypedefs.h"



/** \fn     GalleryFileHelper::GalleryFileHelper()
 *  \brief  Constructor
 *  \return void
 */
GalleryFileHelper::GalleryFileHelper()
{
    m_backendHost   = gCoreContext->GetSetting("MasterServerIP","localhost");
    m_backendPort   = gCoreContext->GetNumSetting("BackendStatusPort", 6544);

    m_manager = new QNetworkAccessManager();
}



/** \fn     GalleryFileHelper::~GalleryFileHelper()
 *  \brief  Destructor
 *  \return void
 */
GalleryFileHelper::~GalleryFileHelper()
{
    delete m_manager;
    m_manager = NULL;
}



/** \fn     GalleryFileHelper::StartSyncImages()
 *  \brief  Starts the image syncronization from the backend
 *  \return void
 */
void GalleryFileHelper::StartSyncImages()
{
    QUrl url(QString("http://%1:%2/Image/StartSync")
             .arg(m_backendHost)
             .arg(m_backendPort));

    SendRequest(url, QNetworkAccessManager::PostOperation);
}



/** \fn     GalleryFileHelper::StopSyncImages()
 *  \brief  Stops the image syncronization from the backend
 *  \return void
 */
void GalleryFileHelper::StopSyncImages()
{
    QUrl url(QString("http://%1:%2/Image/StopSync")
             .arg(m_backendHost)
             .arg(m_backendPort));

    SendRequest(url, QNetworkAccessManager::PostOperation);
}

/** \fn     GalleryFileHelper::GetSyncStatus()
 *  \brief  Reads the current image syncronization status
 *  \return Struct with bool running, int current image, int total images
 */
GallerySyncStatus GalleryFileHelper::GetSyncStatus()
{
    QUrl url(QString("http://%1:%2/Image/GetSyncStatus")
             .arg(m_backendHost)
             .arg(m_backendPort));

    GallerySyncStatus status;
    status.running = false;
    status.current = 0;
    status.total = 0;

    QByteArray ba = SendRequest(url, QNetworkAccessManager::GetOperation);
    if (ba.count() > 0)
    {
        bool ok;
        QXmlStreamReader xml(ba);
        while (!xml.atEnd())
        {
            xml.readNext();
            if (xml.isStartElement())
            {
                if (xml.name() == "Running")
                {
                    QString value = xml.readElementText();
                    if (value.compare("true") == 0)
                        status.running = true;
                }
                else if (xml.name() == "Current")
                    status.current = xml.readElementText().toInt(&ok);
                else if (xml.name() == "Total")
                    status.total = xml.readElementText().toInt(&ok);
            }
        }
    }

    return status;
}



/**
 *  \brief  Starts the thumbnail generation thread on the backend
 *  \return void
 */
void GalleryFileHelper::StartThumbGen()
{
    QUrl url(QString("http://%1:%2/Image/StartThumbnailGeneration")
             .arg(m_backendHost)
             .arg(m_backendPort));

    SendRequest(url, QNetworkAccessManager::PostOperation);
}



/**
 *  \brief  Stops the thumbnail generation thread on the backend
 *  \return void
 */
void GalleryFileHelper::StopThumbGen()
{
    QUrl url(QString("http://%1:%2/Image/StopThumbnailGeneration")
             .arg(m_backendHost)
             .arg(m_backendPort));

    SendRequest(url, QNetworkAccessManager::PostOperation);
}


void GalleryFileHelper::AddToThumbnailList(ImageMetadata* im)
{
    if (!im)
        return;

    int id = im->m_id;
    QUrl url(QString("http://%1:%2/Image/CreateThumbnail?Id=%3")
             .arg(m_backendHost)
             .arg(m_backendPort)
             .arg(id));

    SendRequest(url, QNetworkAccessManager::PostOperation);
}


void GalleryFileHelper::RecreateThumbnail(ImageMetadata* im)
{
    if (!im)
        return;

    int id = im->m_id;
    QUrl url(QString("http://%1:%2/Image/RecreateThumbnail?Id=%3")
             .arg(m_backendHost)
             .arg(m_backendPort)
             .arg(id));

    SendRequest(url, QNetworkAccessManager::PostOperation);
}


void GalleryFileHelper::SetThumbnailSize(int width, int height)
{
    QUrl url(QString("http://%1:%2/Image/SetThumbnailSize?Width=%3&Height=%4")
             .arg(m_backendHost)
             .arg(m_backendPort)
             .arg(width)
             .arg(height));

    SendRequest(url, QNetworkAccessManager::PostOperation);
}


/** \fn     GalleryFileHelper::RenameFile(const int &, const QString &)
 *  \brief  Renames the file via the service api
 *  \param  im The image metadata object that contains all required information
 *  \param  name The new name of the file (only the filename, no path)
 *  \return True if removal was successful, otherwise false
 */
bool GalleryFileHelper::RenameFile(ImageMetadata *im, const QString &name)
{
    bool ok = false;

    QUrl url(QString("http://%1:%2/Content/RenameFile")
             .arg(m_backendHost)
             .arg(m_backendPort));

#if QT_VERSION < QT_VERSION_CHECK(5,0,0)
    QUrl &urlQuery = url;
#else
    QUrlQuery urlQuery;
#endif

    urlQuery.addQueryItem("FileName", im->m_fileName);
    urlQuery.addQueryItem("NewName", name);

#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
    url.setQuery( urlQuery );
#endif

    QByteArray ba = SendRequest(url, QNetworkAccessManager::PostOperation);

    if (ba.count() > 0)
    {
        QXmlStreamReader xml(ba);
        while (!xml.atEnd())
        {
            xml.readNext();
            if (xml.isStartElement() && xml.name() == "bool")
            {
                ok = xml.readElementText().compare("true") == 0;

                LOG(VB_GENERAL, LOG_DEBUG,
                    QString("RenameFile - Status: %1").arg(ok));

                // The returned data from the service api has only one
                // value, so we stop when we have hit the first bool string
                break;
            }
        }
    }

    return ok;
}



/** \fn     GalleryFileHelper::RemoveFile(ImageMetadata *)
 *  \brief  Deletes the file via the service api
 *  \param  im The image metadata object that contains all required information
 *  \return True if removal was successful, otherwise false
 */
bool GalleryFileHelper::RemoveFile(ImageMetadata *im)
{
    bool ok = false;

    QUrl url(QString("http://%1:%2/Content/DeleteFile")
             .arg(m_backendHost)
             .arg(m_backendPort));

#if QT_VERSION < QT_VERSION_CHECK(5,0,0)
    QUrl &urlQuery = url;
#else
    QUrlQuery urlQuery;
#endif

    urlQuery.addQueryItem("FileName", im->m_fileName);

#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
    url.setQuery( urlQuery );
#endif

    QByteArray ba = SendRequest(url, QNetworkAccessManager::PostOperation);

    if (ba.count() > 0)
    {
        QXmlStreamReader xml(ba);
        while (!xml.atEnd())
        {
            xml.readNext();
            if (xml.isStartElement() && xml.name() == "bool")
            {
                ok = xml.readElementText().compare("true") == 0;

                LOG(VB_GENERAL, LOG_DEBUG,
                    QString("RemoveFile - Status: %1").arg(ok));

                // The returned data from the service api has only one
                // value, so we stop when we have hit the first bool string
                break;
            }
        }
    }

    return ok;
}



/** \fn     GalleryFileHelper::SetImageOrientation(ImageMetadata *)
 *  \brief  Saves the given value in the orientation exif tag
 *  \param  im The image metadata object that contains all required information
 *  \return True if saving the orientation was successful, otherwise false
 */
bool GalleryFileHelper::SetImageOrientation(ImageMetadata *im)
{
    // the orientation of the image.
    // See http://jpegclub.org/exif_orientation.html for details
    if (im->GetOrientation() < 1 || im->GetOrientation() > 8)
        return false;

    bool ok = false;

    QUrl url( QString("http://%1:%2/Image/SetImageInfo")
             .arg(m_backendHost)
             .arg(m_backendPort));

#if QT_VERSION < QT_VERSION_CHECK(5,0,0)
    QUrl &urlQuery = url;
#else
    QUrlQuery urlQuery;
#endif

    urlQuery.addQueryItem("Id", QString::number(im->m_id));
    urlQuery.addQueryItem("Tag", "Exif.Image.Orientation");
    urlQuery.addQueryItem("Value", QString::number(im->GetOrientation()));

#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
    url.setQuery( urlQuery );
#endif

    QByteArray ba = SendRequest(url, QNetworkAccessManager::PostOperation);
    if (ba.count() > 0)
    {
        QXmlStreamReader xml(ba);
        while (!xml.atEnd())
        {
            xml.readNext();
            if (xml.isStartElement() && xml.name() == "bool")
            {
                ok = xml.readElementText().compare("true") == 0;

                LOG(VB_GENERAL, LOG_DEBUG,
                    QString("SetExifOrientation - Status: %1").arg(ok));

                // The returned data from the service api has only one
                // value, so we stop when we have hit the first bool string
                break;
            }
        }
    }

    return ok;
}



/** \fn     GalleryFileHelper::GetExifValues(ImageMetadata *)
 *  \brief  Returns the XML data that contains all available exif header
            tags and values from the image specified by the id.
 *  \param  im The image metadata object that contains all required information
 *  \return The returned XML data
 */
QByteArray GalleryFileHelper::GetExifValues(ImageMetadata *im)
{
    QUrl url(QString("http://%1:%2/Image/GetImageInfoList")
             .arg(m_backendHost)
             .arg(m_backendPort));

#if QT_VERSION < QT_VERSION_CHECK(5,0,0)
    QUrl &urlQuery = url;
#else
    QUrlQuery urlQuery;
#endif

    urlQuery.addQueryItem("Id", QString::number(im->m_id));

#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
    url.setQuery( urlQuery );
#endif

    return SendRequest(url, QNetworkAccessManager::GetOperation);
}



/** \fn     GalleryFileHelper::SendRequest(QUrl &url, QNetworkAccessManager::Operation type)
 *  \brief  Calls the url with the given data either via
            a GET or POST and returns the retrieved data
 *  \param  url The url with all parameters that shall be called
 *  \param  type The type of the call, can be either GET or POST
 *  \return The returned XML data
 */
QByteArray GalleryFileHelper::SendRequest(QUrl &url,
                                          QNetworkAccessManager::Operation type)
{
    QByteArray ba;
    QNetworkReply *reply = NULL;
    QNetworkRequest request(url);

    if (type == QNetworkAccessManager::GetOperation)
    {
        reply = m_manager->get(request);
    }
    else if (type == QNetworkAccessManager::PostOperation)
    {
        request.setHeader(QNetworkRequest::ContentTypeHeader,
                          "application/x-www-form-urlencoded");
        reply = m_manager->post(request, QByteArray());
    }

    // Create a local event loop that blocks further processing
    // until the finished signal is emitted from the network manager
    QEventLoop loop;
    QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    loop.exec();

    // The network manager is done, continue
    if (reply)
    {
        if (reply->error() == QNetworkReply::NoError)
        {
            LOG(VB_GENERAL, LOG_DEBUG,
                QString("GalleryFileHelper SendRequest ok"));
            ba = reply->readAll();
        }
        else
        {
            LOG(VB_GENERAL, LOG_DEBUG,
                QString("GalleryFileHelper SendRequest error: %1")
                .arg(reply->errorString()));
        }
        reply->deleteLater();
    }

    return ba;
}
