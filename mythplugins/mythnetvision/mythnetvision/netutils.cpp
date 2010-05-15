#include <QDir>

#include <mythdirs.h>
#include "netutils.h"

QString GetDisplaySeasonEpisode(int seasEp, int digits)
{
    QString seasEpNum = QString::number(seasEp);

    if (digits == 2 && seasEpNum.size() < 2)
        seasEpNum.prepend("0");

    return seasEpNum;
}

QString GetThumbnailFilename(QString url, QString title)
{
    QString fileprefix = GetConfDir();

    QDir dir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    fileprefix += "/MythNetvision";

    dir = QDir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    fileprefix += "/thumbcache";

    dir = QDir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    QString sFilename = QString("%1/%2_%3")
        .arg(fileprefix)
        .arg(qChecksum(url.toLocal8Bit().constData(),
                       url.toLocal8Bit().size()))
        .arg(qChecksum(title.toLocal8Bit().constData(),
                       title.toLocal8Bit().size()));
    return sFilename;
}
