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
#include <qfile.h>

#include "httprequest.h"
#include "clientsocket.h"

enum FileSendTransformation {
    FILE_TRANSFORM_NONE = 0,
    FILE_TRANSFORM_TOWAV
};



class HttpResponse
{

  public:

    HttpResponse(MFDHttpPlugin *owner, HttpRequest *requestor);
    ~HttpResponse();
    
    void setError(int error_number);
    void send(MFDServiceClientSocket *which_client);
    void addHeader(const QString &new_header);
    void setPayload(char *new_payload, int new_payload_size);
    void sendFile(
                    QString file_path, 
                    int skip, 
                    FileSendTransformation transform = FILE_TRANSFORM_NONE
                 );
    
  private:
    
    void addText(std::vector<char> *buffer, QString text_to_add);
    void createHeaderBlock(std::vector<char> *header_block, int payload_size);
    bool sendBlock(MFDServiceClientSocket *which_client, std::vector<char> block_to_send);
    void streamFile(MFDServiceClientSocket *which_client);
    void convertToWavAndStreamFile(MFDServiceClientSocket *which_client);
    void streamEmptyWav(MFDServiceClientSocket *which_client);

    int     status_code;
    QString status_string;
    bool    all_is_well;


    std::vector<char> payload;

    QDict<HttpHeader> headers;
    HttpRequest       *my_request;
    MFDHttpPlugin     *parent; 
    
    
    int     range_begin;
    int     range_end;
    int     total_possible_range;
    int     stored_skip;
    
    QFile                  *file_to_send;
    FileSendTransformation file_transformation;
    
};

#endif

