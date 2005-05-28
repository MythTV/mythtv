#ifndef SERVICECLIENT_H_
#define SERVICECLIENT_H_
/*
	serviceclient.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Base class for client lib objects that talk to mfd services

*/

#include <qstring.h>
#include <qstringlist.h>
#include <qsocketdevice.h>

enum MfdServiceType
{
    MFD_SERVICE_AUDIO_CONTROL = 0,
    MFD_SERVICE_METADATA,
    MFD_SERVICE_RTSP_CLIENT,
    MFD_SERVICE_TRANSCODER
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
                    uint l_port,
                    QString l_magic_token
                 );

    virtual ~ServiceClient();

    virtual bool    connect();
    int             getSocket();
    MfdServiceType  getType(){return service_type;}
    QString         getAddress(){return ip_address;}
    uint            getPort(){return port;}
    int             bytesAvailable();
    virtual void    handleIncoming();
    void            setName(const QString &a_name){name = a_name;}
    QString         getName(){return name;}
    int             getId(){return mfd_id;}
    QString         getMagicToken(){return magic_token;}
    virtual void    executeCommand(QStringList new_command);
    

  protected:
  
    QSocketDevice  *client_socket_to_service;
    int            mfd_id;
    MfdInterface   *mfd_interface;
    QString        ip_address;
    MfdServiceType service_type;
    uint           port;  
    QString        name;
    QString        magic_token;
};

#endif
