#ifndef MFDSERVERSOCKET_H_
#define MFDSERVERSOCKET_H_
/*
	serversocket.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for the mfd server-side of socket communications

*/

#include <qsocket.h>
#include <qserversocket.h>

class MFDClientSocket : public QSocket
{
    //
    //  All that this class does is augment
    //  QSocket with an internal member
    //  called unique_identifier. Why do we
    //  do this? Because commands come in the
    //  server socket, get passed down to a plugin
    //  (which is running in it's own thread). Once
    //  the command has been processed, it's passed
    //  back up as an Event (sockets are not thread
    //  safe) and then a response might go back to
    //  the client. In the meantime, however, the client
    //  might have gone away. This unique identified lets
    //  us keep track of where the command came from, so
    //  we know where to send back any response.
    //

  public:
      
    MFDClientSocket(int identifier, QObject* parent, const char* name);
    int getIdentifier();
        
  private:
  
    int unique_identifier;   
};

class MFDServerSocket : public QServerSocket
{

    Q_OBJECT

  public:
  
    MFDServerSocket(int port, QObject *parent = 0);
    
    void newConnection(int socket);

  signals:
  
    void newConnect(MFDClientSocket *);
    void endConnect(MFDClientSocket *);
    
  private slots:
  
    void discardClient();
   
  private:
  
    int bumpClientIdentifiers();
    int client_identifiers; 
};

#endif  // mfdserversocket_h_

