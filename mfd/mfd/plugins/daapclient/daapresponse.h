#ifndef DAAPRESPONSE_H_
#define DAAPRESPONSE_H_
/*
    daapresponse.h

    (c) 2003 Thor Sigvaldason and Isaac Richards
    Part of the mythTV project
    
    A little object for handling daap responses

*/

#include "httpinresponse.h"

class DaapInstance;

class DaapResponse : public HttpInResponse
{

  public:

    DaapResponse(
                    DaapInstance *owner,
                    char *raw_incoming, 
                    int length
                );
    ~DaapResponse();

    void         warning(const QString &warn_text);

  private:

    DaapInstance *parent;
};

#endif  // daapresponse_h_
