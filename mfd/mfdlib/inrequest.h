#ifndef INREQUEST_H_
#define INREQUEST_H_
/*
	inrequest.h

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for a generic (and generally inherited) object to handle
	http-like incoming requests.

*/

#include <vector>
using namespace std;

#include <qdict.h>

#include "httpheader.h"

class MFDBasePlugin;
class MFDServiceClientSocket;

class InRequest
{

  public:

    InRequest(MFDBasePlugin *owner, MFDServiceClientSocket *a_client, bool dbo = false);
    virtual ~InRequest();


    bool    parseIncomingBytes();
    bool    eatOneByte();
    bool    checkMessageHeadersEnd();
    void    parseMessageHeaderBlock();
    void    parseHeaders();
    void    consumePayload();
    bool    allIsWell(){return all_is_well;}
    void    log(const QString &log_message, int verbosity);
    void    warning(const QString &warning_message);

    //
    //  Debugging
    //
    
    void    printYourself();

    
  protected:

    MFDBasePlugin           *parent; 
    MFDServiceClientSocket  *client_socket;
    bool                    all_is_well;
    std::vector<char>       message_header_block;
    std::vector<char>       payload;
    int                     timeout_count;
    QString                 request_line;
    QDict<HttpHeader>       headers;
    uint                    payload_size;
    bool                    debug_on;
};

#endif

