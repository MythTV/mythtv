#ifndef SERVICECLIENT_H_
#define SERVICECLIENT_H_
/*
	serviceclient.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Base class for client lib objects that talk to mfd services

*/

#include <qstring.h>
#include <qsocketdevice.h>

enum MfdServiceType
{
    MFD_SERVICE_AUDIO_CONTROL = 0,
    MFD_SERVICE_METADATA
};


class MfdInterface;

class ServiceClient
{

  public:

    ServiceClient(
                    MfdInterface *the_mfd,
                    int which_mfd,
                    MfdServiceType l_service_type,
                    const QString &l_ip_address,
                    uint l_port
                 );

    virtual ~ServiceClient();

    bool            connect();
    int             getSocket();
    MfdServiceType  getType(){return service_type;}
    QString         getAddress(){return ip_address;}
    uint            getPort(){return port;}
    int             bytesAvailable();
    virtual void    handleIncoming();

  protected:
  
    QSocketDevice  *client_socket_to_service;
    int            mfd_id;
    MfdInterface   *mfd_interface;

  private:
  
    MfdServiceType service_type;
    QString        ip_address;
    uint           port;  
};

#endif
