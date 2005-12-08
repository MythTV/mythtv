// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//

#include "streaminput.h"

#include "mythtv/mythcontext.h"

#include <qapplication.h>
#include <qsocket.h>


StreamInput::StreamInput(const QUrl &source)
    : request(0), url(source), sock(0), stage(0)
{
}


void StreamInput::setup()
{
    if (! url.isValid())
        return;

    QString protocol = url.protocol();
    QString host = url.host();
    QString path = url.path();
    int port = url.port();

    if (protocol != "mqp" || host.isNull())
        return;

    if (port == -1)
        port = 42666;

    request = ".song " + path.utf8() + "\r\n";


    sock = new QSocket;
    connect(sock, SIGNAL(error(int)), this, SLOT(error(int)));
    connect(sock, SIGNAL(hostFound()), this, SLOT(hostfound()));
    connect(sock, SIGNAL(connected()), this, SLOT(connected()));
    connect(sock, SIGNAL(readyRead()), this, SLOT(readyread()));

    sock->connectToHost(host, port);

    while (stage != -1 && stage < 4) 
    {
        qDebug("processing one event: stage %d %d %ld",
               stage, sock->canReadLine(), sock->bytesAvailable());
        qApp->processOneEvent();
    }

    qDebug("disconnecting from socket");
    disconnect(sock, SIGNAL(error(int)), this, SLOT(error(int)));
    disconnect(sock, SIGNAL(hostFound()), this, SLOT(hostfound()));
    disconnect(sock, SIGNAL(connected()), this, SLOT(connected()));
    disconnect(sock, SIGNAL(readyRead()), this, SLOT(readyread()));

    if (stage == -1) 
    {
        // some sort of error
        delete sock;
        sock = 0;
    }
}


void StreamInput::hostfound()
{
    qDebug("host found");
    stage = 1;
}


void StreamInput::connected()
{
    qDebug("connected... sending request '%s' %d", request.data(), request.length());

    sock->writeBlock(request.data(), request.length());
    sock->flush();

    stage = 2;
}


void StreamInput::readyread()
{
    if (stage == 2) 
    {
        qDebug("readyread... checking response");
        
        if (! sock->canReadLine()) 
        {
            stage = -1;
            qDebug("can't read line");
            return;
        }
        
        QString line = sock->readLine();
        if (line.isEmpty()) 
        {
            stage = -1;
            qDebug("line is empty");
            return;
        }

        if (line.left(5) != "*GOOD") 
        {
            VERBOSE(VB_IMPORTANT, QString("server error response: %1")
                    .arg(line));
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


void StreamInput::error(int err)
{
    qDebug("socket error: %d", err);

    stage = -1;
}

