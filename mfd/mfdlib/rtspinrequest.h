#ifndef RTSPINREQUEST_H_
#define RTSPINREQUEST_H_
/*
	rtspinrequest.h

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for an object to hold and parse incoming rtsp requests

*/

#include <vector>
using namespace std;


#include <qstring.h>
#include <qdict.h>
#include <qurl.h>

#include "httpheader.h"

class MFDRtspPlugin;
class MFDServiceClientSocket;


enum RtspRequestType
{
    RRT_UNKNOWN,
    RRT_DESCRIBE,
    RRT_GET_PARAMETER,
    RRT_OPTIONS,
    RRT_PAUSE,
    RRT_PLAY,
    RRT_PING,
    RRT_REDIRECT,
    RRT_SETUP,
    RRT_SET_PARAMETER,
    RRT_TEARDOWN
};



class RtspInRequest
{

  public:

    RtspInRequest(MFDRtspPlugin *owner, MFDServiceClientSocket *a_client);
    ~RtspInRequest();

    bool            parseIncomingBytes();
    bool            eatOneByte();
    bool            checkMessageHeadersEnd();
    void            parseMessageHeaderBlock();
    void            parseHeaders();
    void            consumePayload();
    void            parseRequestLine();
    RtspRequestType getRequest(){ return request; }
    int             getCSeq(){ return command_sequence; }

    void    log(const QString &log_message, int verbosity);
    void    warning(const QString &warning_message);

    //
    //  Debugging
    //
    
    void    printYourself();
    
  private:

    MFDRtspPlugin           *parent; 
    MFDServiceClientSocket  *client_socket;
    bool                    all_is_well;
    std::vector<char>       message_header_block;
    std::vector<char>       payload;
    int                     timeout_count;
    QString                 request_line;
    QDict<HttpHeader>       headers;
    uint                    payload_size;
    RtspRequestType         request;
    QUrl                    request_url;
    int                     command_sequence;
};

#endif

