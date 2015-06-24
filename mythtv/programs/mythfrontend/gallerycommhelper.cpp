#include "gallerycommhelper.h"

#include <unistd.h> // for usleep

#include <QApplication>

#include <mythsystemlegacy.h>
#include <remotefile.h>


/*!
 * \brief Request BE to start/stop synchronising the image db to the storage group
 * \param start Start scan, if true. Otherwise stop scan
 * \return QString Error message, if not empty
 */
QString GalleryBERequest::ScanImagesAction(bool start)
{
    QStringList strList;
    strList << "IMAGE_SCAN" << (start ? "START" : "STOP");

    bool ok = gCoreContext->SendReceiveStringList(strList, true);
    return ok ? "" : strList[1];
}


/*!
 * \brief Returns BE scan status
 * \return QStringList State ("ERROR" | "OK"), Mode ("SCANNING" | ""),
 * Progress count, Total
 */
QStringList GalleryBERequest::ScanQuery()
{
    QStringList strList;
    strList << "IMAGE_SCAN" << "QUERY";

    if (!gCoreContext->SendReceiveStringList(strList))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Image: Scan query failed : %1")
            .arg(strList.join(",")));

        // Default to no scan
        strList.clear();
        strList << "ERROR" << "" << "0" << "0";
    }
    return strList;
}


/*!
 * \brief Request BE to clear image db
 * \return QString Error message, if not empty
 */
QString GalleryBERequest::ClearDatabase()
{
    QStringList strList;
    strList << "IMAGE_SCAN" << "CLEAR";

    bool ok = gCoreContext->SendReceiveStringList(strList, true);
    return ok ? "" : strList[1];
}


/*!
 * \brief Request BE to create thumbnails for images
 * \param ids List of image ids
 * \param isForFolder Set if images are for a directory cover, which are generated
 * after images
 */
void GalleryBERequest::CreateThumbnails(QStringList ids, bool isForFolder)
{
    LOG(VB_FILE, LOG_DEBUG, QString("Image: Sending CREATE_THUMBNAILS %1 (forFolder %2)")
        .arg(ids.join(",")).arg(isForFolder));

    QStringList message;
    message << ids.join(",") << QString::number(isForFolder);
    gCoreContext->SendEvent(MythEvent("CREATE_THUMBNAILS", message));
}


/*!
 * \brief Request BE to hide/unhide images
 * \param hidden Hide | unhide
 * \param ids List of image ids
 * \return QString Error message, if not empty
 */
QString GalleryBERequest::HideFiles(bool hidden, ImageIdList ids)
{
    if (ids.isEmpty())
        return "Bad Hide Request";

    QStringList idents;
    foreach (int id, ids)
    {
        idents.append(QString::number(id));
    }

    QStringList message;
    message << "IMAGE_HIDE" << QString::number(hidden) << idents.join(",");

    bool ok = gCoreContext->SendReceiveStringList(message, true);
    return ok ? "" : message[1];
}


/*!
 * \brief Request BE to rename an image
 * \param  id An image id
 * \param  name New name of the file (only the filename, no path)
 * \return QString Error message, if not empty
 */
QString GalleryBERequest::RenameFile(int id, QString name)
{
    QStringList message;
    message << "IMAGE_RENAME" << QString::number(id) << name;

    bool ok = gCoreContext->SendReceiveStringList(message, true);
    return ok ? "" : message[1];
}


/*!
 * \brief Request BE to delete images
 * \param  ids List of image ids
 * \return QString Error message, if not empty
 */
QString GalleryBERequest::RemoveFiles(ImageIdList ids)
{
    QStringList idents;
    foreach (int id, ids)
    {
        idents.append(QString::number(id));
    }

    QStringList message;
    message << "IMAGE_DELETE" << idents.join(",");

    bool ok = gCoreContext->SendReceiveStringList(message, true);
    return ok ? "" : message[1];
}


/*!
 * \brief Request BE to apply an orientation transform to images
 * \param  transform Transformation to apply
 * \param  ids List of image ids
 * \return QString Error message, if not empty
 */
QString GalleryBERequest::ChangeOrientation(ImageFileTransform transform,
                                            ImageIdList ids)
{
    QStringList idents;
    foreach (int id, ids)
    {
        idents.append(QString::number(id));
    }

    QStringList message;
    message << "IMAGE_TRANSFORM"
         << QString::number(transform)
         << idents.join(",");

    bool ok = gCoreContext->SendReceiveStringList(message, true);
    return ok ? "" : message[1];
}


/*!
 * \brief Request BE to set image to use as a cover thumbnail(s)
 * \param  parent Directory id
 * \param  to Image to use as cover
 * \return QString Error message, if not empty
 */
QString GalleryBERequest::SetCover(int parent, int to)
{
    QStringList message;
    message << "IMAGE_COVER"
         << QString::number(parent)
         << QString::number(to);

    bool ok = gCoreContext->SendReceiveStringList(message, true);
    return ok ? "" : message[1];
}


/*!
 * \brief Returns all exif tags and values from an image
 * \param  im An image
 * \return QString Error message, if not empty
 */
NameMap GalleryBERequest::GetMetaData(const ImageItem *im)
{
    QMap<QString, QString> tags;
    QStringList            strlist;
    strlist << "IMAGE_GET_METADATA"
            << QString::number(im->m_id);

    if (gCoreContext->SendReceiveStringList(strlist))
    {
        // Each string contains a Label<seperator>Value
        QString seperator = strlist[1];
        for (int i = 2; i < strlist.size(); ++i)
        {
            QStringList parts = strlist[i].split(seperator);

            LOG(VB_FILE, LOG_DEBUG,
                QString("Image: Metadata %1 : %2").arg(parts[0], parts[1]));

            tags.insert(parts[0], parts[1]);
        }
    }
    return tags;
}


/*!
 * \brief Moves images to an image directory
 * \param destination Id of an image directory
 * \param ids Ids of images/dirs to move
 * \return QString Error message
 */
QString GalleryBERequest::MoveFiles(int destination, ImageIdList ids)
{
    QStringList idents;
    foreach (int id, ids)
    {
        idents.append(QString::number(id));
    }

    QStringList message("IMAGE_MOVE");
    message << QString::number(destination) << idents.join(",");

    bool ok = gCoreContext->SendReceiveStringList(message, true);
    return ok ? "" : message[1];
}


/*!
 * \brief Create directories
 * \param names List of directory paths
 * \return QString Error message, if not empty
 */
QString GalleryBERequest::MakeDirs(QStringList names)
{
    names.insert(0, "IMAGE_CREATE_DIRS");

    bool ok = gCoreContext->SendReceiveStringList(names, true);
    return ok ? "" : names[1];
}


/*!
 * \brief Set directories to ignore during scan & rescan
 * \param excludes Comma separated list of dir names/patterns to exclude. Glob
 * characters * and ? permitted.
 * \return QString Error message, if not empty
 */
QString GalleryBERequest::IgnoreDirs(QString excludes)
{
    QStringList message("IMAGE_IGNORE");
    message << excludes;

    bool ok = gCoreContext->SendReceiveStringList(message, true);
    return ok ? "" : message[1];
}


int RunWorker(WorkerThread *worker)
{
    worker->start();

    // Wait for worker to complete
    while (!worker->isFinished())
    {
        usleep(1000);
        qApp->processEvents();
    }

    int result = worker->GetResult();
    delete worker;

    return result;
}


void ShellWorker::run()
{
    RunProlog();
    LOG(VB_GENERAL, LOG_INFO, QString("Image: Executing \"%1\"").arg(m_command));
    m_result = myth_system(m_command);
    RunEpilog();
}


/*!
 * \brief Thread worker to copy/move files
 * \param deleteAfter Remove original file after copy, if set
 * \param files Map of source/destination filepaths
 * \param dialog Dialog that reflects progress
 */
FileTransferWorker::FileTransferWorker(bool deleteAfter, NameMap files,
                                       MythUIProgressDialog *dialog)
    : WorkerThread("FileTransfer"),
    m_delete(deleteAfter),
    m_files(files),
    m_dialog(dialog)
{
}


void FileTransferWorker::run()
{
    RunProlog();

    uint total    = m_files.size();
    uint progress = 0;
    m_result = 0;
    QString action = m_delete ? tr("Moving %1") : tr("Copying %1");

    foreach (const QString &src, m_files.keys())
    {
        // Update progress dialog
        if (m_dialog)
        {
            QFileInfo            fi(src);
            int size = fi.size() / 1024;
            QString message = action.arg(fi.fileName())
                    + (size > 0 ? QString(" (%2 Kb)").arg(size) : "");
            ProgressUpdateEvent *pue     =
                new ProgressUpdateEvent(++progress, total, message);

            QApplication::postEvent(m_dialog, pue);
        }

        LOG(VB_FILE, LOG_INFO, action.arg(src) + QString(" -> %1").arg(m_files[src]));

        // Copy file
        if (RemoteFile::CopyFile(src, m_files[src]))
        {
            ++m_result;
            if (m_delete)
            {
                // Delete source file
                QFile file(src);
                if (!file.remove())
                    LOG(VB_FILE, LOG_ERR, QString("Image: Failed to delete %1")
                        .arg(src));
            }
        }
        else
            LOG(VB_FILE, LOG_ERR, QString("Image: Failed to copy %1 to %2")
                .arg(src, m_files[src]));
    }

    if (m_dialog)
        m_dialog->Close();

    RunEpilog();
}
