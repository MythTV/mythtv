#ifndef DAAPREQUEST_H_
#define DAAPREQUEST_H_
/*
	daaprequest.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    A little object for making daap requests

*/

#include <qstring.h>
#include <qsocketdevice.h>
#include <qdict.h>

#include "httpgetvar.h"


class DaapInstance;

class DaapRequest
{

  public:

    DaapRequest(
                DaapInstance *owner,
                const QString &l_base_url, 
                const QString &l_host_address
               );
    ~DaapRequest();

    bool send(QSocketDevice *where_to_send, bool ignore_shutdown=false);
    void addGetVariable(const QString &label, int value);
    void addGetVariable(const QString &label, const QString &value);
    
  private:

    DaapInstance *parent;
    bool sendBlock(std::vector<char> what, QSocketDevice *where, bool ignore_shutdown = false);
    void addText(std::vector<char> *buffer, QString text_to_add);
    
    QString base_url;
    QString host_address;

    QDict<HttpGetVariable>   get_variables;
    
};

#endif  // daaprequest_h_
