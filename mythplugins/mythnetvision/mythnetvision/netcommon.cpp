#include <QDir>

#include <libmyth/mythcontext.h>
#include <libmythbase/mythdirs.h>
#include <libmythbase/remotefile.h>

#include "netcommon.h"

QString GetThumbnailFilename(const QString& url, const QString& title)
{
    QString fileprefix = GetConfDir();

    QDir dir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    fileprefix += "/cache/netvision-thumbcache";

    dir = QDir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    quint16 urlChecksum = qChecksum(url.toLocal8Bit().constData(), url.toLocal8Bit().size());
    quint16 titleChecksum = qChecksum(title.toLocal8Bit().constData(), title.toLocal8Bit().size());
#else
    quint16 urlChecksum = qChecksum(url.toLocal8Bit());
    quint16 titleChecksum = qChecksum(title.toLocal8Bit());
#endif
    QString sFilename = QString("%1/%2_%3")
        .arg(fileprefix).arg(urlChecksum).arg(titleChecksum);
    return sFilename;
}

QString GetMythXMLURL(void)
{
    QString MasterIP = gCoreContext->GetMasterServerIP();
    int MasterStatusPort = gCoreContext->GetMasterServerStatusPort();

    return QString("http://%1:%2/InternetContent/").arg(MasterIP)
        .arg(MasterStatusPort);
}

QUrl GetMythXMLSearch(const QString& url, const QString& query, const QString& grabber,
                         const QString& pagenum)
{
    QString tmp = QString("%1GetInternetSearch?Query=%2&Grabber=%3&Page=%4")
        .arg(url, query, grabber, pagenum);
    return {tmp};
}
