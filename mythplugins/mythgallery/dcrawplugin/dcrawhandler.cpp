#include "dcrawhandler.h"

#include "config.h"

#include <cstddef>

#include <QByteArray>
#include <QFile>
#include <QImage>
#include <QIODevice>
#include <QString>

#include "mythsystemlegacy.h"
#include "exitcodes.h"
#include "../mythgallery/galleryutil.h"
#include "mythlogging.h"

namespace
{

bool getPath(QIODevice *device, QString &path)
{
    QFile *file = qobject_cast<QFile *>(device);
    if (!file)
        return false;
    path = file->fileName();
    return true;
}

}    // anonymous namespace

bool DcrawHandler::canRead() const
{
    QString path;
    bool isFile = getPath(device(), path);
    if (!isFile)
        // It would still be possible to process this file,
        // but piping the image data to dcraw would be
        // difficult. This code path wouldn't be exercised in
        // MythGallery anyway. So for simplicity we give up.
        return false;

    QString command = "dcraw -i " + path;
    return (myth_system(command) == GENERIC_EXIT_OK);
}

bool DcrawHandler::read(QImage *image)
{
    QString path;
    bool isFile = getPath(device(), path);
    if (!isFile)
        // It would still be possible to process this file,
        // but piping the image data to dcraw would be
        // difficult. This code path wouldn't be exercised in
        // MythGallery anyway. So for simplicity we give up.
        return false;

    path = "'" + path + "'";
    QStringList arguments;
    arguments << "-c" << "-w" << "-W";
#ifdef ICC_PROFILE
    arguments << "-p" << ICC_PROFILE;
#endif // ICC_PROFILE
    arguments << path;

    uint flags = kMSRunShell | kMSStdOut;
    MythSystemLegacy ms("dcraw", arguments, flags);
    ms.Run();
    if (ms.Wait() != GENERIC_EXIT_OK)
        return false;

    QByteArray buffer = ms.ReadAll();
    if (buffer.isEmpty())
        return false;

    bool loaded = image->loadFromData(buffer, "PPM");
    return loaded;
}

int DcrawHandler::loadThumbnail(QImage *image, QString fileName)
{
    QStringList arguments;
    arguments << "-e" << "-c";
    arguments << "'" + fileName + "'";

    uint flags = kMSRunShell | kMSStdOut;
    MythSystemLegacy ms("dcraw", arguments, flags);
    ms.Run();
    if (ms.Wait() != GENERIC_EXIT_OK)
        return -1;

    QByteArray buffer = ms.ReadAll();
    if (buffer.isEmpty())
        return -1;

    if (!image->loadFromData(buffer, "JPG"))
        return -1;

    int rotateAngle = 0;

#ifdef EXIF_SUPPORT
    const unsigned char *buf = (const unsigned char *)buffer.constData();
    int size = buffer.size();
    rotateAngle = GalleryUtil::GetNaturalRotation(buf, size);
#endif

    return rotateAngle;
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
