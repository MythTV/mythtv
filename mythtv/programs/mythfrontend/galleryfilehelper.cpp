// Qt headers
#include <QNetworkAccessManager>
#include <QXmlStreamReader>
#include <QNetworkReply>
#include <QEventLoop>

// MythTV headers
#include "mythcontext.h"
#include "storagegroup.h"
#include "remoteutil.h"

#include "galleryfilehelper.h"
#include "gallerytypedefs.h"



/** \fn     GalleryFileHelper::GalleryFileHelper()
 *  \brief  Constructor
 *  \return void
 */
GalleryFileHelper::GalleryFileHelper()
{
    m_backendHost   = gCoreContext->GetSetting("BackendServerIP","localhost");
    m_backendPort   = gCoreContext->GetNumSetting("BackendStatusPort", 6544);

    m_manager = new QNetworkAccessManager();

    // Set the proxy for the manager to be the application
    // default proxy, which has already been setup
    m_manager->setProxy(QNetworkProxy::applicationProxy());
}



/** \fn     GalleryFileHelper::~GalleryFileHelper()
 *  \brief  Destructor
 *  \return void
 */
GalleryFileHelper::~GalleryFileHelper()
{
    if (m_manager)
    {
        delete m_manager;
        m_manager = NULL;
    }
}



/** \fn     GalleryFileHelper::SyncImages()
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



/** \fn     GalleryFileHelper::SyncImages()
 *  \brief  Starts the image syncronization from the backend
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



/** \fn     GalleryFileHelper::RenameFile(const int &, const QString &)
 *  \brief  Renames the file via the service api
 *  \param  id The database id of the file that shall be renamed
 *  \param  name The new name of the file (only the filename, no path)
 *  \return True if removal was successful, otherwise false
 */
bool GalleryFileHelper::RenameFile(ImageMetadata *im, const QString &name)
{
    bool ok = false;

    QUrl url(QString("http://%1:%2/Content/RenameFile")
             .arg(m_backendHost)
             .arg(m_backendPort));

    url.addQueryItem("FileName", im->m_fileName);
    url.addQueryItem("NewName", name);

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



/** \fn     GalleryFileHelper::RemoveFile(const int &)
 *  \brief  Deletes the file via the service api
*  \param   id The database id of the file that shall be renamed
 *  \return True if removal was successful, otherwise false
 */
bool GalleryFileHelper::RemoveFile(ImageMetadata *im)
{
    bool ok = false;

    QUrl url(QString("http://%1:%2/Content/DeleteFile")
             .arg(m_backendHost)
             .arg(m_backendPort));

    url.addQueryItem("FileName", im->m_fileName);

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



/** \fn     GalleryFileHelper::SetExifOrientation(const QString &, const long , bool *)
 *  \brief  Saves the given value in the orientation exif tag
 *  \param  fileName The filename that holds the exif data
 *  \param  orientation The value that shall be saved in the exif data
 *  \param  ok Will be set to true if the update was ok, otherwise false
 *  \return True if saving the orientation was successful, otherwise false
 */
bool GalleryFileHelper::SetImageOrientation(ImageMetadata *im)
{
    // the orientation of the image.
    // See http://jpegclub.org/exif_orientation.html for details
    if (im->GetOrientation() < 1 || im->GetOrientation() > 8)
        return false;

    bool ok = false;

    QUrl url(QString("http://%1:%2/Image/SetImageInfo")
             .arg(m_backendHost)
             .arg(m_backendPort));

    url.addQueryItem("Id", QString::number(im->m_id));
    url.addQueryItem("Tag", "Exif.Image.Orientation");
    url.addQueryItem("Value", QString::number(im->GetOrientation()));

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



/** \fn     GalleryFileHelper::GetExifValues(const int id)
 *  \brief  Returns the XML data that contains all available exif header
            tags and values from the image specified by the id.
 *  \param  id The database id of the file
 *  \return The returned XML data
 */
QByteArray GalleryFileHelper::GetExifValues(ImageMetadata *im)
{
    QUrl url(QString("http://%1:%2/Image/GetImageInfoList")
             .arg(m_backendHost)
             .arg(m_backendPort));

    url.addQueryItem("Id", QString::number(im->m_id));

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
