#ifndef RTSPCLIENT_H_
#define RTSPCLIENT_H_
/*
	rtspclient.h

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	client object to talk to an mfd's rtsp music server

*/

#include "serviceclient.h"

class RtspIn;

class RtspClient : public ServiceClient
{

  public:

    RtspClient(
                MfdInterface *the_mfd,
                int an_mfd,
                const QString &l_ip_address,
                uint l_port
               );
    ~RtspClient();

    bool connect();
    void turnOn();
    void turnOff();
    void executeCommand(QStringList new_command);

  private:
  
    RtspIn  *rtsp_in;

};

#endif
