#ifndef CLIENT_SOCKET_H_
#define CLIENT_SOCKET_H_
/*
	clientsocket.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for the a client socket

*/

#include <qsocketdevice.h>
#include <qmutex.h>

class MFDServiceClientSocket : public QSocketDevice
{
  public:
  
    MFDServiceClientSocket(int identifier, int socket_id, Type type);
    int getIdentifier();

    void lockReadMutex();
    void lockWriteMutex();
    void unlockReadMutex();
    void unlockWriteMutex();

    void setReading(bool on_or_off);
    bool isReading();
    
  private:
  
    int unique_identifier;
    QMutex  write_mutex;
    QMutex  read_mutex;

    bool    reading_state;
    QMutex  reading_state_mutex;
};

#endif
