#include "dcrawhandler.h"

#include "config.h"

#include <cstddef>

#include <QByteArray>
#include <QFile>
#include <QImage>
#include <QIODevice>
#include <QString>
#include "mythsystem.h"
#include "exitcodes.h"

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

    QStringList arguments;
    arguments << "-c" << "-w" << "-W";
#ifdef ICC_PROFILE
    arguments << "-p" << ICC_PROFILE;
#endif // ICC_PROFILE
    arguments << path;

    uint flags = kMSRunShell | kMSStdOut | kMSBuffered;
    MythSystem ms("dcraw", arguments, flags);
    ms.Run();
    if (ms.Wait() != GENERIC_EXIT_OK)
        return false;

    QByteArray buffer = ms.ReadAll();
    if (buffer.isEmpty())
        return false;

    bool loaded = image->loadFromData(buffer, "PPM");
    return loaded;
}

