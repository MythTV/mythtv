/*
	httpinrequest.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for an object to hold and parse incoming http requests

*/

#include <iostream>
using namespace std;

#include <qstringlist.h>

#include "httpinrequest.h"
#include "httpoutresponse.h"
#include "httpserver.h"

//HttpInRequest::HttpInRequest(MFDHttpPlugin *owner, char *raw_incoming, int incoming_length)
HttpInRequest::HttpInRequest(
                                MFDHttpPlugin *owner, 
                                MFDServiceClientSocket *a_client, 
                                bool dbo
                            )
             :InRequest(owner, a_client, dbo)
{
    url = "";
    all_is_well = true;
    send_response = true;
    headers.setAutoDelete(true);
    get_variables.setAutoDelete(true); 
       
    //
    //  Every request gets a response
    //
    
    my_response = new HttpOutResponse(parent, this);
    

}

bool HttpInRequest::parseRequestLine()
{
    //  
    //  First line of an http request is pretty important ... do
    //  some checking that things are sane 
    //  (e.g. GET /some_url HTTP/1.1)
    //
    
    QStringList line_tokens = QStringList::split( " ", request_line );

    if(line_tokens.count() < 3)
    {
        parent->warning(QString("httprequest got a malformed first line: \"%1\"")
                .arg(request_line));
        all_is_well = false;
        return false;
    }
    else
    {
        if(line_tokens[0] != "GET")
        {
            parent->warning(QString("httprequest only knows how to do \"GET\" "
                            "requests, but got a \"%1\" request")
                            .arg(line_tokens[0])); 
            all_is_well = false;
            return false;
        }
        
        //
        //  Set the URL 
        //
        
        url = line_tokens[1].section('?', 0, 0);
        
        //
        //  Pull out any http get variables
        //
        
        QString get_variables_string = line_tokens[1].section('?', 1);
        
        QStringList get_variables_list = QStringList::split( "&", get_variables_string );
        
        for(uint i= 0; i < get_variables_list.count(); i++)
        {
            HttpGetVariable *new_gv = new HttpGetVariable(get_variables_list[i]);
            get_variables.insert(new_gv->getField(), new_gv);
        }
        
        
        //
        //  Check that we're speaking _http_ and that version is 1.1
        //
        
        QStringList protocol_and_version = QStringList::split( "/", line_tokens[2]);
        if(protocol_and_version.count() != 2)
        {
            parent->warning(QString("httprequest got bad protocol and version in request ... "
                            "should be \"HTTP/1.1\" but got \"%1\"")
                            .arg(line_tokens[2]));
            all_is_well = false;
            return false;
        }
        else
        {
            if(protocol_and_version[0] != "HTTP")
            {
                parent->warning(QString("httprequest got bad protocol "
                                        "should be \"HTTP\" but got \"%1\"")
                                        .arg(protocol_and_version[0]));
                all_is_well = false;
                return false;
            }
            if(protocol_and_version[1] != "1.1")
            {
                parent->warning(QString("httprequest got bad http version "
                                        "should be \"1.1\" but got \"%1\". "
                                        "Will try to keep going...")
                                        .arg(protocol_and_version[1]));
            }
        }
    }
    
    return true;
}

QString HttpInRequest::getRequest()
{
    return request_line;
}

QString HttpInRequest::getHeader(const QString& field_label)
{
    HttpHeader *which_one = headers.find(field_label);
    if(which_one)
    {
        return which_one->getValue();
    }
    return NULL;
}

QString HttpInRequest::getVariable(const QString& variable_name)
{
    HttpGetVariable *which_one = get_variables.find(variable_name);
    if(which_one)
    {
        return which_one->getValue();
    }
    return NULL;
}

void HttpInRequest::printRequest()
{
    cout << "==============================Debugging output==============================" << endl;
    cout << "HTTP request line was:" << endl;
    cout << request_line << endl;
    cout << "============================================================================" << endl;
    
}

void HttpInRequest::printHeaders()
{
    //
    //  Debugging code to print all the headers that came in on this request
    //
    
    cout << "==============================Debugging output==============================" << endl;
    cout << "HTTP request contained the following headers:" << endl;

    if(headers.count() > 0)
    {
        QDictIterator<HttpHeader> iterator( headers );
        for( ; iterator.current(); ++iterator )
        {
            cout << "\t" << iterator.currentKey() << ": " << iterator.current()->getValue() << endl;
        }
    }
    else
    {
        cout << "\t" << "NO HEADERS" << endl;
    }
    cout << "============================================================================" << endl;
    
}

void HttpInRequest::printGetVariables()
{
    //
    //  Debugging code to print all the headers that came in on this request
    //
    
    cout << "==============================Debugging output==============================" << endl;
    cout << "HTTP request contained the following get variables:" << endl;

    QDictIterator<HttpGetVariable> iterator( get_variables );
    for( ; iterator.current(); ++iterator )
    {
        cout << "\t" << iterator.currentKey() << "=" << iterator.current()->getValue() << endl;
    }
    cout << "============================================================================" << endl;
    
}

HttpInRequest::~HttpInRequest()
{
    get_variables.clear();
    
    if(my_response)
    {
        delete my_response;
    }
}

