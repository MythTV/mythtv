#ifndef HTTPOUTREQUEST_H_
#define HTTPOUTREQUEST_H_
/*
	httpoutrequest.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Object to construct and send outgoing http requests

*/

#include <vector>
using namespace std;

#include <qsocketdevice.h>
#include <qdict.h>

#include "httpheader.h"
#include "httpgetvar.h"


class HttpOutRequest
{
    //
    //  Class for building _OUT_ going http requests
    //

  public:
  
    HttpOutRequest(
                    const QString &l_base_url,
                    const QString &l_host_address
                  );
                  
    virtual ~HttpOutRequest();

    bool send(QSocketDevice *where_to_send);
    void addGetVariable(const QString &label, int value);
    void addGetVariable(const QString &label, const QString &value);
    void addHeader(const QString &new_header);
    QString getRequestString();

    virtual void warning(const QString &warn_text);
        
  protected:

    bool sendBlock(
                    std::vector<char> what, 
                    QSocketDevice *where
                  );

    void addText(std::vector<char> *buffer, QString text_to_add);
    
    QString base_url;
    QString host_address;
    QString stored_request;

    QDict<HttpGetVariable>   get_variables;
    QDict<HttpHeader>        headers;

};

#endif

