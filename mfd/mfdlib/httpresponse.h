#ifndef HTTPRESPONSE_H_
#define HTTPRESPONSE_H_
/*
	httpresponse.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for an object to build http responses

*/

#include <qstring.h>
#include <qdict.h>

#include "httprequest.h"
#include "clientsocket.h"

class HttpResponse
{

  public:

    HttpResponse();
    ~HttpResponse();
    
    void setError(int error_number);
    void addText(int* index, char* block, QString new_text);
    void send(MFDServiceClientSocket *which_client);
    void addHeader(const QString &new_header);
    void setPayload(char *new_payload, int new_payload_size);
    void sendFile(QString file_path){file_to_serve = file_path;}
    
  private:
    
    int     status_code;
    QString status_string;
    bool    all_is_well;

    char    *payload;
    int     payload_size;
    QString file_to_serve;

    QDict<HttpHeader> headers;
};

#endif

