#ifndef DAAPREQUEST_H_
#define DAAPREQUEST_H_
/*
	daaprequest.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    A little object for making daap requests

*/

#include <vector>
using namespace std;

#include <qstring.h>
#include <qsocketdevice.h>
#include <qdict.h>

#include "httpgetvar.h"
#include "httpheader.h"

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
    void addHeader(const QString &new_header);
    QString getRequestString();
        
  private:

    DaapInstance *parent;
    bool sendBlock(std::vector<char> what, QSocketDevice *where, bool ignore_shutdown = false);
    void addText(std::vector<char> *buffer, QString text_to_add);
    
    QString base_url;
    QString host_address;
    QString stored_request;

    QDict<HttpGetVariable>   get_variables;
    QDict<HttpHeader>        headers;
    
};

#endif  // daaprequest_h_
