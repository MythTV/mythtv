/*
	mdcaprequest.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for mdcap requests
*/

#include <iostream>
using namespace std;

#include "httpinrequest.h"
#include "httpoutresponse.h"
#include "mdcaprequest.h"

MdcapRequest::MdcapRequest()
{
    mdcap_request_type = MDCAP_REQUEST_NOREQUEST;
}

void MdcapRequest::parsePath(HttpInRequest *http_request)
{
    QString the_path = http_request->getUrl();

    //
    //  Figure out what kind of request this is. Note these are the basic
    //  building blocks of an mdcap exchange.
    //
    
    if(the_path == "/server-info")
    {
        setRequestType(MDCAP_REQUEST_SERVINFO);
    }
    else if(the_path == "/login")
    {
        setRequestType(MDCAP_REQUEST_LOGIN);
    }
    else if(the_path == "/logout")
    {
        setRequestType(MDCAP_REQUEST_LOGOUT);
    }
    else if(the_path.startsWith("/update"))
    {
        setRequestType(MDCAP_REQUEST_UPDATE);
    }
/*
    else if(the_path.startsWith("/databases"))
    {
        daap_request->setRequestType(DAAP_REQUEST_DATABASES);
        log(QString(": a client asked for /databases (%1)").arg(the_path), 9);
    }
    else if(the_path.startsWith("/resolve"))
    {
        daap_request->setRequestType(DAAP_REQUEST_RESOLVE);
        log(": a client asked for /resolve", 9);
    }
    else if(the_path.startsWith("/browse"))
    {
        daap_request->setRequestType(DAAP_REQUEST_BROWSE);
        log(": a client asked for /browse", 9);
    }
*/
    else
    {
        warning(QString("does not understand the path of this request: %1")
              .arg(the_path));
              
        //
        //  Send an httpd 400 error to the client
        //

        http_request->getResponse()->setError(400);

    }

    
}

MdcapRequest::~MdcapRequest()
{
}

