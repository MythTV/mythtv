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

//
//  Some defines ... probably want to get rid of these at some point
//

#define MAX_CLIENT_INCOMING 10240
#define HTTP_MAX_URL		1024
#define HTTP_MAX_HEADERS	1024
#define HTTP_MAX_AUTH		128


class HttpHeader
{

  public:
  
    HttpHeader(const QString &input_line);
    ~HttpHeader();
    const QString& getField();
    
  private:
  
    QString field;
    QString value;
};

class HttpRequest
{

  public:

    HttpRequest(char *raw_incoming, int incominh_length);
    ~HttpRequest();
    
    bool allIsWell(){return all_is_well;}
    
  private:

    int               readLine(int *index, char* parsing_buffer);
    char              *raw_request;
    int               raw_length;
    QString           top_line;
    QString           url;
    bool              all_is_well;
    QDict<HttpHeader> headers;
};

#endif

