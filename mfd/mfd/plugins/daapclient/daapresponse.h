#ifndef DAAPRESPONSE_H_
#define DAAPRESPONSE_H_
/*
    daapresponse.h

    (c) 2003 Thor Sigvaldason and Isaac Richards
    Part of the mythTV project
    
    A little object for handling daap responses

*/

#include <qdict.h>

#include "httpheader.h"
#include "httpgetvar.h"

class DaapInstance;

class DaapResponse
{

  public:

    DaapResponse(
                    DaapInstance *owner,
                    char *raw_incoming, 
                    int length
                );
    ~DaapResponse();

    int readLine(int *parse_point, char *parsing_buffer, char *raw_incoming);


  private:

    DaapInstance *parent;
    int          raw_length;
    
    QDict<HttpHeader>      headers;
    QDict<HttpGetVariable> get_variables;

};

#endif  // daapresponse_h_
