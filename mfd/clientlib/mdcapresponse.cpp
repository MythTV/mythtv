/*
	mdcapresponse.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    A little object for parsing incoming mdcap responses

*/

#include "mdcapresponse.h"

MdcapResponse::MdcapResponse(
                                char *raw_incoming, 
                                int length
                            )
            :HttpInResponse(raw_incoming, length)
{
}

void MdcapResponse::warning(const QString &warn_text)
{
    cout << "WARNING mdcapresponse.o: " << warn_text << endl;
}

MdcapResponse::~MdcapResponse()
{
}
