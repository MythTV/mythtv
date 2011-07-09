// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//

#include "streaminput.h"

#include "mythlogging.h"

#include <QApplication>

#define LOC      QString("StreamInput: ")
#define LOC_WARN QString("StreamInput, Warning: ")
#define LOC_ERR  QString("StreamInput, Error: ")

StreamInput::StreamInput(const QUrl &source) :
    request(QString::null), url(source), sock(NULL), stage(0)
{
}

void StreamInput::Setup(void)
{
    if (!url.isValid())
        return;

    QString protocol = url.scheme();
    QString host = url.host();
    QString path = url.path();
    int port = url.port();

    if (protocol != "mqp" || host.isEmpty())
        return;

    port = (port < 0) ? 42666 : port;

    request = path;
    request.detach();

    sock = new QTcpSocket();
    connect(sock, SIGNAL(Error(QAbstractSocket::SocketError)),
            this, SLOT(  Error(QAbstractSocket::SocketError)));
    connect(sock, SIGNAL(hostFound()), this, SLOT(HostFound()));
    connect(sock, SIGNAL(connected()), this, SLOT(Connected()));
    connect(sock, SIGNAL(readyRead()), this, SLOT(Readyread()));

    sock->connectToHost(host, port, QIODevice::ReadWrite);

    while (stage != -1 && stage < 4)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Processing one event: stage %1 %2 %3")
                .arg(stage).arg(sock->canReadLine())
                .arg(sock->bytesAvailable()));

        qApp->processEvents();
    }

    LOG(VB_GENERAL, LOG_INFO, LOC + "Disconnecting from socket");
    disconnect(sock, SIGNAL(Error(QAbstractSocket::SocketError)),
               this, SLOT(  Error(QAbstractSocket::SocketError)));
    disconnect(sock, SIGNAL(hostFound()), this, SLOT(HostFound()));
    disconnect(sock, SIGNAL(connected()), this, SLOT(Connected()));
    disconnect(sock, SIGNAL(readyRead()), this, SLOT(ReadyRead()));

    if (stage == -1)
    {
        // some sort of error
        delete sock;
        sock = NULL;
    }
}


void StreamInput::HostFound(void)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Host found");
    stage = 1;
}


void StreamInput::Connected(void)
{
    QString tmp = QString(".song %1\r\n").arg(QString(request.toUtf8()));
    QByteArray ba = tmp.toAscii();

    LOG(VB_GENERAL, LOG_INFO, LOC +
        QString("Connected... sending request '%1' %2")
            .arg(ba.constData()).arg(ba.length()));

    sock->write(ba.constData(), ba.length());
    sock->flush();

    stage = 2;
}


void StreamInput::ReadyRead(void)
{
    if (stage == 2)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "ReadyRead... checking response");

        if (! sock->canReadLine())
        {
            stage = -1;
            LOG(VB_GENERAL, LOG_ERR, LOC + "ReadyRead... can't read line");
            return;
        }

        QString line = sock->readLine();
        if (line.isEmpty())
        {
            stage = -1;
            LOG(VB_GENERAL, LOG_ERR, LOC + "ReadyRead... line is empty");
            return;
        }

        if (line.left(5) != "*GOOD")
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Server error response: %1").arg(line));
            stage = -1;
            return;
        }

        stage = 3;
    }
    else if (sock->bytesAvailable() > 65536 || sock->atEnd())
    {
        stage = 4;
    }
}

void StreamInput::Error(QAbstractSocket::SocketError)
{
    LOG(VB_GENERAL, LOG_ERR, LOC +
        QString("Socket error: %1").arg(sock->errorString()));

    stage = -1;
}

