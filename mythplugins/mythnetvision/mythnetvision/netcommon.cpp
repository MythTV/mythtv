#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QEventLoop>

#include <mythdirs.h>
#include <mythdb.h>
#include <mythcontext.h>
#include <netgrabbermanager.h>

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
    // Get MasterServerIP
    //   The data for MasterServerIP setting is the same as the data for
    //   BackendServerIP setting
    QString MasterIP = gCoreContext->GetSetting("MasterServerIP");

    MSqlQuery query(MSqlQuery::InitCon());
    // Get hostname of Master Server by comparing BackendServerIP to
    // the just-retrieved MasterServerIP
    query.prepare("SELECT hostname FROM settings "
                  " WHERE value = 'BackendServerIP'"
                  "   AND data = :IPADDRESS");
    query.bindValue(":IPADDRESS", MasterIP);
    if (!query.exec() || !query.next())
        MythDB::DBError("Find Master Server Hostname", query);

    QString MasterHost = query.value(0).toString();

    // Use hostname to get BackendStatusPort
    int MasterStatusPort = gCoreContext->GetNumSettingOnHost("BackendStatusPort",
                                                          MasterHost);

    return QString("http://%1:%2/InternetContent/").arg(MasterIP).arg(MasterStatusPort);
}

QUrl GetMythXMLSearch(QString url, QString query, QString grabber,
                         uint pagenum)
{
    QString tmp = QString("%1GetInternetSearch?Query=%2&Grabber=%3&Page=%4")
        .arg(url).arg(query).arg(grabber).arg(QString::number(pagenum));
    return QUrl(tmp);
}
