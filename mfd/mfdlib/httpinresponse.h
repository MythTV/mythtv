#ifndef HTTPINRESPONSE_H_
#define HTTPINRESPONSE_H_
/*
	httpinresponse.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for an object to parse incoming http responses
*/

#include <vector>
#include <iostream>
using namespace std;

#include <qstring.h>
#include <qdict.h>

#include "httpheader.h"

class HttpInResponse
{

  public:

    HttpInResponse(
                    char *raw_incoming, 
                    int length
                );
    virtual ~HttpInResponse();

    int                 readLine(int *parse_point, char *parsing_buffer, char *raw_incoming);
    bool                complete();
    void                appendToPayload(char *raw_incoming, int length);
    void                printHeaders();    // Debugging
    std::vector<char>*  getPayload(){return &payload;}
    QString             getHeader(const QString &field_label);
    bool                allIsWell(){return all_is_well;}
    void                allIsWell(bool yes_or_no){all_is_well = yes_or_no;}
    virtual void        warning(const QString &warn_text);
    
  protected:

    int          raw_length;
    bool         all_is_well;
    int          status_code;
    int          expected_payload_size;
        
    QDict<HttpHeader>   headers;
    QString             preserved_top_line;
    std::vector<char>   payload;
};

#endif

