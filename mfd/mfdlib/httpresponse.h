#ifndef HTTPRESPONSE_H_
#define HTTPRESPONSE_H_
/*
	httpresponse.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for an object to build http responses

*/

#include <vector>
#include <iostream>
using namespace std;

#include <qstring.h>
#include <qdict.h>

#include "httprequest.h"
#include "clientsocket.h"

class HttpResponse
{

  public:

    HttpResponse(HttpRequest *requestor);
    ~HttpResponse();
    
    void setError(int error_number);
    void addText(std::vector<char> *buffer, QString text_to_add);
    void send(MFDServiceClientSocket *which_client);
    bool sendBlock(MFDServiceClientSocket *which_client, std::vector<char> block_to_send);
    void addHeader(const QString &new_header);
    void setPayload(char *new_payload, int new_payload_size);
    void sendFile(QString file_path);
    
  private:
    
    int     status_code;
    QString status_string;
    bool    all_is_well;


    std::vector<char> payload;

    QDict<HttpHeader> headers;
    HttpRequest *my_request;
};

#endif

