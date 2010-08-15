#include "dcrawhandler.h"

#include "config.h"

#include <cstddef>

#include <QByteArray>
#include <QFile>
#include <QImage>
#include <QIODevice>
#include <QProcess>
#include <QString>

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

    QProcess process(NULL);
    QString program = "dcraw";
    QStringList arguments;
    arguments << "-i" << path;
    process.start(program, arguments, QIODevice::NotOpen);

    bool finished = process.waitForFinished();
    if (!finished)
        return false;
    if (process.exitStatus() != QProcess::NormalExit)
        return false;
    bool success = (process.exitCode() == 0);
    return success;
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

    QProcess process(NULL);
    QString program = "dcraw";
    QStringList arguments;
    arguments << "-c" << "-w" << "-W";
#ifdef ICC_PROFILE
    arguments << "-p" << ICC_PROFILE;
#endif // ICC_PROFILE
    arguments << path;
    process.start(program, arguments, QIODevice::ReadOnly);

    bool finished = process.waitForFinished();
    if (!finished)
        return false;
    if (process.exitStatus() != QProcess::NormalExit)
        return false;
    if (process.exitCode() != 0)
        return false;

    QByteArray buffer = process.readAll();
    if (buffer.isEmpty())
        return false;

    bool loaded = image->loadFromData(buffer, "PPM");
    return loaded;
}

