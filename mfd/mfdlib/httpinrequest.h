#ifndef HTTPINREQUEST_H_
#define HTTPINREQUEST_H_
/*
	httpinrequest.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for an object to hold and parse incoming http requests

*/

#include <vector>
using namespace std;

#include <qstring.h>
#include <qdict.h>

class HttpOutResponse;
class MFDHttpPlugin;

#include "httpheader.h"
#include "httpgetvar.h"
#include "inrequest.h"

//
//  Some defines ... probably want to get rid of these at some point
//

#define MAX_CLIENT_INCOMING 10240       // FIX
#define MAX_CLIENT_OUTGOING 12000000    // FIX


class HttpInRequest : public InRequest
{
    //
    //  Class that handles _IN_ coming Http requests
    //

  public:

    HttpInRequest(MFDHttpPlugin *owner, MFDServiceClientSocket *a_client, bool dbo = false);
    ~HttpInRequest();
    
    bool                parseRequestLine();
    HttpOutResponse*    getResponse(){return my_response;}
    const QString&      getUrl(){return url;} 
    QString             getRequest();
    QString             getHeader(const QString &field_label);
    QString             getVariable(const QString &variable_name);
    void                sendResponse(bool yes_or_no){send_response = yes_or_no;}
    bool                sendResponse(){return send_response;}
    void                printRequest();     //  Debugging
    void                printHeaders();     //  Debugging
    void                printGetVariables();//  Debugging
    MFDBasePlugin*      getParent(){return parent;}
    std::vector<char>*  getPayload(){return &payload;}

  private:

    QString             url;
    HttpOutResponse     *my_response;
    bool                send_response;

    QDict<HttpGetVariable> get_variables;
};

#endif

