#ifndef DAAPCLIENT_H_
#define DAAPCLIENT_H_
/*
	daapclient.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for a daap content monitor

*/

#include <qsocketdevice.h>

#include "mfd_plugin.h"

class DaapClient: public MFDServicePlugin
{

  public:

    DaapClient(MFD *owner, int identity);
    ~DaapClient();

    void    run();
    void    readSocket();
    void    addDaapServer(QString server_address, uint server_port, QString service_name);
    void    removeDaapServer(QString server_address, uint server_port, QString service_name);

  private:
  
    QSocketDevice *client_socket_to_mfd;

};

#endif  // daapclient_h_
