#ifndef DAAPRESPONSE_H_
#define DAAPRESPONSE_H_
/*
    daapresponse.h

    (c) 2003 Thor Sigvaldason and Isaac Richards
    Part of the mythTV project
    
    A little object for handling daap responses

*/

#include <vector>
using namespace std;

#include <qdict.h>

#include "httpheader.h"

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

    int                 readLine(int *parse_point, char *parsing_buffer, char *raw_incoming);
    void                printHeaders();    // Debugging
    std::vector<char>*  getPayload(){return &payload;}
    QString             getHeader(const QString &field_label);

  private:

    DaapInstance *parent;
    int          raw_length;
    bool         all_is_well;
    int          status_code;
        
    QDict<HttpHeader>   headers;
    std::vector<char>   payload;
};

#endif  // daapresponse_h_
