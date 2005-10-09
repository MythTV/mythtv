#ifndef SERVERSOCKET_H_
#define SERVERSOCKET_H_
/*
	serversocket.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for the lcd server-side of socket communications

*/

#include <qsocket.h>
#include <qserversocket.h>



class LCDServerSocket : public QServerSocket
{

    Q_OBJECT

  public:
  
    LCDServerSocket(int port, QObject *parent = 0);
    
    void newConnection(int socket);

  signals:
  
    void newConnect(QSocket *);
    void endConnect(QSocket *);
    
  private slots:
  
    void discardClient();
    
};

#endif  // serversocket_h_

