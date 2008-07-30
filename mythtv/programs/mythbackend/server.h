#ifndef SERVER_H_
#define SERVER_H_

#include <QTcpServer>

class MythSocket;

class MythServer : public QTcpServer
{
    Q_OBJECT

  signals:
    void newConnect(MythSocket*);

  protected:
    virtual void incomingConnection(int socket);
};


#endif
