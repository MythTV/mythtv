#ifndef MDCAPRESPONSE_H_
#define MDCAPRESPONSE_H_
/*
	mdcapresponse.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    A little object for handling mdcap responses

*/

#include "../mfdlib/httpinresponse.h"

class MdcapResponse : public HttpInResponse
{

  public:

    MdcapResponse(
                    char *raw_incoming, 
                    int length
                 );
               
    ~MdcapResponse();

    void warning(const QString &warn_text);
};

#endif 
