/*
    daapresponse.cpp

    (c) 2003 Thor Sigvaldason and Isaac Richards
    Part of the mythTV project
    
    A little object for handling daap responses

*/

#include "daapresponse.h"
#include "daapinstance.h"

DaapResponse::DaapResponse(
                            DaapInstance *owner,
                            char *raw_incoming, 
                            int length
                          )
             :HttpInResponse(raw_incoming, length)
{
    parent = owner;
}

void DaapResponse::warning(const QString &warn_text)
{
    if(parent)
    {
        parent->warning(warn_text);
    }
    else
    {
        cout << "WARNING daapresponse.o: " << warn_text << endl;
    }
}

DaapResponse::~DaapResponse()
{
}
