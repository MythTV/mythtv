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

HttpInRequest::HttpInRequest(MFDHttpPlugin *owner, char *raw_incoming, int incoming_length)
{
    parent = owner;
    raw_request = NULL;
    top_line = "";
    raw_request_line = "";
    all_is_well = true;
    send_response = true;
    headers.setAutoDelete(true);
    get_variables.setAutoDelete(true); 
    expected_payload_size = -1;
       
    //
    //  Every request gets a response
    //
    
    my_response = new HttpOutResponse(parent, this);
    
    //
    //  Make and fill storage for the raw incoming data
    //

    raw_request = new char [incoming_length + 1];
    for(int i = 0; i < incoming_length; i ++)
    {
        raw_request[i] = raw_incoming[i];
    }
    raw_request[incoming_length] = '\0';
    raw_length = incoming_length;


    //
    //  Read the request
    //
    
    int parse_point = 0;
    bool first_line = true;
    char *parsing_buffer = new char[incoming_length + 1];
    while(readLine(&parse_point, parsing_buffer))
    {
        if(first_line)
        {
            //  
            //  First line of an http request is pretty important ... do
            //  some checking that things are sane 
            //  (ie. GET /some_url HTTP/1.1)
            //
            
            raw_request_line = QString(parsing_buffer);
            top_line = QString(parsing_buffer);
            QStringList line_tokens = QStringList::split( " ", top_line );

            if(line_tokens.count() < 3)
            {
                parent->warning(QString("httprequest got a malformed first line: \"%1\"")
                        .arg(top_line));
                all_is_well = false;
                return;
            }
            else
            {
                if(line_tokens[0] != "GET")
                {
                    parent->warning(QString("httprequest only knows how to do \"GET\" "
                                    "requests, but got a \"%1\" request")
                                    .arg(line_tokens[0])); 
                    all_is_well = false;
                    return;
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
                    return;
                }
                else
                {
                    if(protocol_and_version[0] != "HTTP")
                    {
                        parent->warning(QString("httprequest got bad protocol "
                                                "should be \"HTTP\" but got \"%1\"")
                                                .arg(protocol_and_version[0]));
                        all_is_well = false;
                        return;
                    }
                    if(protocol_and_version[1] != "1.1")
                    {
                        parent->warning(QString("httprequest got bad http version "
                                                "should be \"1.1\" but got \"%1\"")
                                                .arg(protocol_and_version[1]));
                        all_is_well = false;
                        return;
                    }
                }
                
            }
            first_line = false;
        }
        else
        {
            //
            //  Store the headers
            //

            QString header_line = QString(parsing_buffer);
            HttpHeader *new_header = new HttpHeader(header_line);
            headers.insert(new_header->getField(), new_header);
            
        }
    }

    // printHeaders();
    
    //
    //  Now we need to store the payload.
    //
    

    payload.clear();
    payload.insert( 
                    payload.begin(), 
                    (raw_incoming + parse_point), 
                    (raw_incoming + parse_point + (incoming_length - parse_point))
                  );
    
    //
    //  Make note of how many bytes we expect in total
    //
    
    HttpHeader *content_length_header = headers.find("Content-Length");
    if(content_length_header)
    {
        bool ok = true;
        int content_length = content_length_header->getValue().toInt(&ok);
        if(ok)
        {
            expected_payload_size = content_length;
            if(expected_payload_size != (int) payload.size())
            {
                cerr << "httpinrequest.o: mismatch between Content-Length: "
                     << "header and actual amount of data. Not good"
                     << endl;
            }
        }
        else
        {
            cerr << "httpinrequest.o: No Content-Length header in input, "
                 << "things will probably screw up."
                 << endl;
        }
    }

    delete [] parsing_buffer;
}

int HttpInRequest::readLine(int *parse_point, char *parsing_buffer)
{
    int  amount_read = 0;
    bool keep_reading = true;
    int  index = *parse_point;
    while(keep_reading)
    {
        
        if(index >= raw_length )
        {
            //
            //  No data at all.
            //

            parsing_buffer[amount_read] = '\0';
            keep_reading = false;
        }
         
        else if(raw_request[index] == '\r')
        {
            //
            // ignore
            //

            index++;
        }
        else if(raw_request[index] == '\n')
        {
            //
            //  done with this line
            //

            index++;
            parsing_buffer[amount_read] = '\0';
            //amount_read++;
            keep_reading = false;
        }
        else
        {
            parsing_buffer[amount_read] = raw_request[index];
            index++;
            amount_read++;
        }
    }
    *parse_point = index;
    return amount_read;
}

QString HttpInRequest::getRequest()
{
    return raw_request_line;
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
    cout << raw_request_line << endl;
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
    if(raw_request)
    {
        delete [] raw_request;
        raw_request = NULL;
    }

    headers.clear();
    get_variables.clear();
    
    if(my_response)
    {
        delete my_response;
    }
}

