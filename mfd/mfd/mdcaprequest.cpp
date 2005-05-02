/*
	mdcaprequest.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for mdcap requests
*/

#include <iostream>
using namespace std;

#include <qstringlist.h>

#include "httpinrequest.h"
#include "httpoutresponse.h"
#include "mdcaprequest.h"

MdcapRequest::MdcapRequest()
{
    mdcap_request_type = MDCAP_REQUEST_NOREQUEST;
    
    //
    //  Start up defaults
    //
    
    container_id = -1;
    list_id = -1;
    generation = 0;
    delta = 0;
    item_request = false;
    list_request = false;

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
    else if(the_path.startsWith("/containers"))
    {
        setRequestType(MDCAP_REQUEST_CONTAINERS);
        
        //
        //  Parse out /containers request details
        //
        
        QStringList request_tokens = QStringList::split( "/", the_path );
        if(request_tokens.count() < 3)
        {
            warning(QString("bad /containers request: \"%1\"")
                    .arg(the_path));
            setRequestType(MDCAP_REQUEST_SCREWEDUP);
            http_request->getResponse()->setError(400);
            return;
        }
        bool ok;
        container_id = request_tokens[1].toInt(&ok);
        if(!ok || container_id < 1) 
        {
            container_id = -1;
            warning(QString("bad container id: \"%1\"")
                    .arg(the_path));
            setRequestType(MDCAP_REQUEST_SCREWEDUP);
            http_request->getResponse()->setError(400);
            return;
        }
        if(request_tokens[2] == "items")
        {
            item_request = true;
        }        
        else if(request_tokens[2] == "lists")
        {
            list_request = true;
        }
        else
        {
            container_id = -1;
            warning(QString("bad type of container request: \"%1\"")
                    .arg(the_path));
            setRequestType(MDCAP_REQUEST_SCREWEDUP);
            http_request->getResponse()->setError(400);
            return;
        }
        
        //
        //  Set generation and delta values if we have them
        //
        
        QString generation_string = http_request->getVariable("generation");
        if(generation_string.length() > 0)
        {
            bool ok;
            generation = generation_string.toInt(&ok);
            if( !ok || generation < 1)
            {
                setRequestType(MDCAP_REQUEST_SCREWEDUP);
                http_request->getResponse()->setError(400);
                return;
                warning("bad generation get variable");
            }
        }
        QString delta_string = http_request->getVariable("delta");
        if(delta_string.length() > 0)
        {
            bool ok;
            delta = delta_string.toInt(&ok);
            if( !ok || delta < 1)
            {
                setRequestType(MDCAP_REQUEST_SCREWEDUP);
                http_request->getResponse()->setError(400);
                return;
                warning("bad delta get variable");
            }
        }
        
    }
    else if(the_path.startsWith("/commit") && the_path.contains("list"))
    {
        setRequestType(MDCAP_REQUEST_COMMIT_LIST);

        //
        //  Parse out /commit request details
        //
        
        QStringList request_tokens = QStringList::split( "/", the_path );
        if(request_tokens.count() < 4)
        {
            warning(QString("bad /commit request: \"%1\"")
                    .arg(the_path));
            setRequestType(MDCAP_REQUEST_SCREWEDUP);
            http_request->getResponse()->setError(400);
            return;
        }

        bool ok;
        container_id = request_tokens[1].toInt(&ok);
        if(!ok || container_id < 1) 
        {
            container_id = -1;
            warning(QString("bad container id in commit request: \"%1\"")
                    .arg(the_path));
            setRequestType(MDCAP_REQUEST_SCREWEDUP);
            http_request->getResponse()->setError(400);
            return;
        }
        if(request_tokens[2] != "list")
        {
            container_id = -1;
            warning(QString("bad 3rd token in list commit request: \"%1\"")
                    .arg(the_path));
            setRequestType(MDCAP_REQUEST_SCREWEDUP);
            http_request->getResponse()->setError(400);
            return;
        }
        
        list_id = request_tokens[3].toInt(&ok);
        if(!ok || container_id < 1) 
        {
            container_id = -1;
            list_id = -1;
            warning(QString("bad list id in commit request: \"%1\"")
                    .arg(the_path));
            setRequestType(MDCAP_REQUEST_SCREWEDUP);
            http_request->getResponse()->setError(400);
            return;
        }

    }
    else if(the_path.startsWith("/remove") && the_path.contains("list"))
    {
        setRequestType(MDCAP_REQUEST_REMOVE_LIST);

        //
        //  Parse out /remove request details
        //
        
        QStringList request_tokens = QStringList::split( "/", the_path );
        if(request_tokens.count() < 4)
        {
            warning(QString("bad /remove request: \"%1\"")
                    .arg(the_path));
            setRequestType(MDCAP_REQUEST_SCREWEDUP);
            http_request->getResponse()->setError(400);
            return;
        }

        bool ok;
        container_id = request_tokens[1].toInt(&ok);
        if(!ok || container_id < 1) 
        {
            container_id = -1;
            warning(QString("bad container id in remove request: \"%1\"")
                    .arg(the_path));
            setRequestType(MDCAP_REQUEST_SCREWEDUP);
            http_request->getResponse()->setError(400);
            return;
        }
        if(request_tokens[2] != "list")
        {
            container_id = -1;
            warning(QString("bad 3rd token in list remove request: \"%1\"")
                    .arg(the_path));
            setRequestType(MDCAP_REQUEST_SCREWEDUP);
            http_request->getResponse()->setError(400);
            return;
        }
        list_id = request_tokens[3].toInt(&ok);
        if(!ok || container_id < 1) 
        {
            container_id = -1;
            list_id = -1;
            warning(QString("bad list id in remove request: \"%1\"")
                    .arg(the_path));
            setRequestType(MDCAP_REQUEST_SCREWEDUP);
            http_request->getResponse()->setError(400);
            return;
        }

    }
    /*
    else if(the_path.startsWith("/commit") && the_path.contains("item"))
    {
        setRequestType(MDCAP_REQUEST_COMMIT_ITEM);
    }
    */
    else
    {
        warning(QString("does not understand the path of this request: %1")
              .arg(the_path));
              
        //
        //  Send an httpd 400 error to the client
        //

        setRequestType(MDCAP_REQUEST_SCREWEDUP);
        http_request->getResponse()->setError(400);
    }
}

MdcapRequest::~MdcapRequest()
{
}

