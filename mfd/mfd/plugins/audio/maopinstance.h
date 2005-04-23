#ifndef MAOPINSTANCE_H_
#define MAOPINSTANCE_H_
/*
	maopinstance.h

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for an object that knows how to talk maop (to mfd speaker plugins)

*/

#include <qstring.h>
#include <qsocketdevice.h>
#include <qmutex.h>

class AudioPlugin;

#include "../../../mfdlib/service.h"

class MaopInstance
{

  public:

    MaopInstance(
                    AudioPlugin *owner, 
                    QString l_name, 
                    QString l_address, 
                    uint l_port, 
                    ServiceLocationDescription l_location,
                    int l_id
                );
    ~MaopInstance();
    
    QString getName(){ return name; }
    QString getAddress(){ return address; }
    bool    isThisYou(QString l_name, QString l_address, uint l_port);
    bool    allIsWell(){ return all_is_well; }
    int     getFileDescriptor(){ return socket_fd; }
    void    checkIncoming();
    void    sendRequest(const QString &the_request);
    void    markForUse(bool y_or_n){ marked_for_use = y_or_n; }
    bool    markedForUse(){ return marked_for_use; }
    int     getId(){ return id; }
    QString getHostname();
    
  private:

    void log(const QString &log_message, int verbosity);
    void warning(const QString &warn_message);
    
    QString                     name;
    QString                     address;
    QString                     hostname;
    uint                        port;
    ServiceLocationDescription  location;
    bool                        all_is_well;
    QMutex                      client_socket_mutex;
    QSocketDevice              *client_socket_to_maop_service;
    AudioPlugin                *parent;
    int                         socket_fd;
    bool                        currently_open;
    QString                     current_url;
    bool                        marked_for_use;
    int                         id;
};

#endif  // maopinstance_h_
