#ifndef SERVERSOCKET_H_
#define SERVERSOCKET_H_
/*
    serversocket.h

    (c) 2003 Thor Sigvaldason and Isaac Richards
    Part of the mythTV project

    Headers for the mtd server-side of socket communications

*/

#include <Q3Socket>
#include <Q3ServerSocket>

class Q3Socket;

class MTDServerSocket : public Q3ServerSocket
{

    Q_OBJECT

  public:
  
    MTDServerSocket(int port, QObject *parent = 0);
    
    void newConnection(int socket);

  signals:
  
    void newConnect(Q3Socket *);
    void endConnect(Q3Socket *);
    
  private slots:
  
    void discardClient();
    
};

#endif  // serversocket_h_

