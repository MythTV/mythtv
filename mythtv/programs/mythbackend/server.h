#ifndef SERVER_H_
#define SERVER_H_

#include <qsocket.h>
#include <qserversocket.h>

class RefSocket : public QSocket
{
  public:
    RefSocket(QObject *parent = 0, const char *name = 0) 
           : QSocket(parent, name), refCount(0) { }
    void UpRef(void) { refCount++; }
    bool DownRef(void) { refCount--; 
                         if (refCount < 0) { delete this; return true; }
                         return false; 
                       }

  private:
    int refCount;
};

class MythServer : public QServerSocket
{
    Q_OBJECT
  public:
    MythServer(int port, QObject *parent = 0);

    void newConnection(int socket);

  signals:
    void newConnect(RefSocket *);
    void endConnect(RefSocket *);

  private slots:
    void discardClient();
};


#endif
