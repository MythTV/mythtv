#include <QDir>

#include <mythdirs.h>
#include <mythcontext.h>
#include <remotefile.h>

#include "netcommon.h"

QString GetThumbnailFilename(QString url, QString title)
{
    QString fileprefix = GetConfDir();

    QDir dir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    fileprefix += "/cache/netvision-thumbcache";

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

QString GetMythXMLURL(void)
{
    QString MasterIP = gCoreContext->GetMasterServerIP();
    int MasterStatusPort = gCoreContext->GetMasterServerStatusPort();

    return QString("http://%1:%2/InternetContent/").arg(MasterIP)
        .arg(MasterStatusPort);
}

QUrl GetMythXMLSearch(QString url, QString query, QString grabber,
                         uint pagenum)
{
    QString tmp = QString("%1GetInternetSearch?Query=%2&Grabber=%3&Page=%4")
        .arg(url).arg(query).arg(grabber).arg(QString::number(pagenum));
    return QUrl(tmp);
}
