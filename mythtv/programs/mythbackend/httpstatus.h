#ifndef HTTPSTATUS_H_
#define HTTPSTATUS_H_

#include <qserversocket.h>

class MainServer;

class HttpStatus : public QServerSocket
{
    Q_OBJECT
  public:
    HttpStatus(MainServer *parent, int port);

    void newConnection(int socket);

  private slots:
    void readClient();
    void discardClient();

  private:
    MainServer *m_parent;
};

#endif
