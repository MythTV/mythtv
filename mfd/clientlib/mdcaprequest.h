#ifndef MDCAPREQUEST_H_
#define MDCAPREQUEST_H_
/*
	mdcaprequest.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    A little object for making mdcap requests

*/

#include <vector>
using namespace std;

#include <qstring.h>
#include <qsocketdevice.h>
#include <qdict.h>

#include "../mfdlib/httpoutrequest.h"

class MdcapRequest : public HttpOutRequest
{

  public:

    MdcapRequest(
                    const QString& l_base_url, 
                    const QString& l_host_address
                );
               
    ~MdcapRequest();

};

#endif 
