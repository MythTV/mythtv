#include "httpstatus.h"

#include <iostream>
using namespace std;

#include <qregexp.h>
#include <qstringlist.h>
#include <qtextstream.h>
#include <qsocket.h>

#include "mainserver.h"

HttpStatus::HttpStatus(MainServer *parent, int port)
          : QServerSocket(port, 1)
{
    m_parent = parent;

    if (!ok())
    { 
        cerr << "Failed to bind to port " << port << endl;
        exit(0);
    }
}

void HttpStatus::newConnection(int socket)
{
    QSocket *s = new QSocket(this);
    connect(s, SIGNAL(readyRead()), this, SLOT(readClient()));
    connect(s, SIGNAL(delayedCloseFinished()), this, SLOT(discardClient()));
    s->setSocket(socket);
}

void HttpStatus::readClient(void)
{
    QSocket *socket = (QSocket *)sender();
    if (!socket)
        return;

    if (socket->canReadLine()) 
    {
        QStringList tokens = QStringList::split(QRegExp("[ \r\n][ \r\n]*"), 
                                                socket->readLine());
        if (tokens[0] == "GET") 
        {
            m_parent->PrintStatus(socket);
            socket->close();
        }
    }
}

void HttpStatus::discardClient(void)
{
    QSocket *socket = (QSocket *)sender();
    if (!socket)
        return;
    delete socket;
}

