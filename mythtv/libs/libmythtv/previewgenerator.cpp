#include "previewgenerator.h"

void PreviewGenerator::Run(void)
{
    QString server = gContext->GetSetting("MasterServerIP", "localhost");
    int port       = gContext->GetNumSetting("MasterServerPort", 6543);

    serverSock = gContext->ConnectServer(NULL/*eventSock*/, server, port);
    if (!serverSock)
    {
        eventSock->deleteLater();
        return;
    }

    QStringList strlist = "QUERY_GENPIXMAP";
    programInfo.ToStringList(strlist);

    if (serverSock)
        WriteStringList(serverSock, strlist);

    bool ok = false;
    if (serverSock)
        ok = ReadStringList(serverSock, strlist, false);

    if (ok)
    {
        QMutexLocker locker(&previewLock);
        emit previewReady(&programInfo);
    }

    if (serverSock)
    {
        delete serverSock;
        serverSock = NULL;
    }
    if (eventSock)
        eventSock->deleteLater();;
}

void PreviewGenerator::EventSocketConnected(void)
{
    QString str = QString("ANN Playback %1 %2")
        .arg(gContext->GetHostName()).arg(true);
    QStringList strlist = str;
    WriteStringList(eventSock, strlist);
    ReadStringList(eventSock, strlist);
}

void PreviewGenerator::EventSocketClosed(void)
{
    VERBOSE(VB_IMPORTANT, "Preview Event socket closed.");
    if (serverSock)
    {
        QSocketDevice *tmp = serverSock;
        serverSock = NULL;
        delete tmp;
    }
}

void PreviewGenerator::EventSocketRead(void)
{
    while (eventSock->state() == QSocket::Connected &&
           eventSock->bytesAvailable() > 0)
    {
        QStringList strlist;
        if (!ReadStringList(eventSock, strlist))
            continue;
    }
}
