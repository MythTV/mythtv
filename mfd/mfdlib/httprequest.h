#ifndef HTTPREQUEST_H_
#define HTTPREQUEST_H_
/*
	httprequest.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for an object to hold and parse incoming http requests

*/

#include <qstring.h>
#include <qdict.h>

class HttpResponse;

//
//  Some defines ... probably want to get rid of these at some point
//

#define MAX_CLIENT_INCOMING 10240
#define MAX_CLIENT_OUTGOING 12000000    // FIX


class HttpGetVariable
{

  public:
  
    HttpGetVariable(const QString &text_segment);
    ~HttpGetVariable();

    const QString& getField();
    const QString& getValue();

  private:
  
    QString field;
    QString value;
};

class HttpHeader
{

  public:
  
    HttpHeader(const QString &input_line);
    ~HttpHeader();

    const QString& getField();
    const QString& getValue();

  private:
  
    QString field;
    QString value;
};

class HttpRequest
{

  public:

    HttpRequest(char *raw_incoming, int incoming_length);
    ~HttpRequest();
    
    HttpResponse*   getResponse(){return my_response;}
    bool            allIsWell(){return all_is_well;}
    const QString&  getUrl(){return url;} 
    QString         getHeader(const QString &field_lable);
    QString         getVariable(const QString &variable_name);
    void            sendResponse(bool yes_or_no){send_response = yes_or_no;}
    bool            sendResponse(){return send_response;}

  private:

    int          readLine(int *index, char* parsing_buffer);
    char         *raw_request;
    int          raw_length;
    QString      top_line;
    QString      url;
    bool         all_is_well;
    HttpResponse *my_response;
    bool         send_response;

    QDict<HttpHeader>      headers;
    QDict<HttpGetVariable> get_variables;

    
};

#endif

