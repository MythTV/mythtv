/*
	httprequest.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for an object to hold and parse incoming http requests

*/

#include <iostream>
using namespace std;

#include <qstringlist.h>

#include "httprequest.h"
#include "httpresponse.h"

HttpGetVariable::HttpGetVariable(const QString &text_segment)
{
    //
    //  Take a line of text and parse into a matching set of HTTP get
    //  variable field names and values
    //
    
    field = text_segment.section('=', 0, 0);
    value = text_segment.section('=', 1);

    //
    //  Do %20 (etc.) changes ?
    //
}


const QString& HttpGetVariable::getField()
{
    return field;
}

const QString& HttpGetVariable::getValue()
{
    return value;
}

HttpGetVariable::~HttpGetVariable()
{
}

/*
---------------------------------------------------------------------
*/

HttpHeader::HttpHeader(const QString &input_line)
{
    //
    //  Take a line of text and parse into a matching set of HTTP headers
    //  field: value
    //
    
    field = input_line.section(':', 0, 0);
    value = input_line.section(':', 1);

    field = field.simplifyWhiteSpace();
    value = value.simplifyWhiteSpace();

}

const QString& HttpHeader::getField()
{
    return field;
}

const QString& HttpHeader::getValue()
{
    return value;
}

HttpHeader::~HttpHeader()
{
}

/*
---------------------------------------------------------------------
*/


HttpRequest::HttpRequest(char *raw_incoming, int incoming_length)
{
    raw_request = NULL;
    top_line = "";
    all_is_well = true;
    send_response = true;
    headers.setAutoDelete(true);
    
    //
    //  Every request gets a response
    //
    
    my_response = new HttpResponse(this);
    
    if(incoming_length > MAX_CLIENT_INCOMING)
    {
        cerr << "httprequest.o: http request too big .. this should not happen" << endl;
        all_is_well = false;
        return;
    }


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
    char parsing_buffer[MAX_CLIENT_INCOMING];
    while(readLine(&parse_point, parsing_buffer))
    {
        if(first_line)
        {
            //  
            //  First line of an http request is pretty important ... do
            //  some checking that things are sane GET /server-info HTTP/1.1
            //
            
            top_line = QString(parsing_buffer);
            QStringList line_tokens = QStringList::split( " ", top_line );

            if(line_tokens.count() < 3)
            {
                cerr << "httprequest.o: got a malformed first line: \"" 
                     << top_line 
                     << "\"" 
                     << endl;
                all_is_well = false;
                return;
            }
            else
            {
                if(line_tokens[0] != "GET")
                {
                    cerr << "httprequest.o: only know how to do \"GET\" "
                         << "requests, but got a \"" 
                         << line_tokens[0] 
                         << "\" request" 
                         << endl;
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
                    cerr << "httprequest.o: bad protocol and version in request ... "
                         << "should be \"HTTP/1.1\" but got \""
                         <<  line_tokens[2]
                         << "\""
                         << endl; 
                    all_is_well = false;
                    return;
                }
                else
                {
                    if(protocol_and_version[0] != "HTTP")
                    {
                        cerr << "httprequest.o: bad protocol "
                             << "should be \"HTTP\" but got \""
                             <<  protocol_and_version[0]
                             << "\""
                             << endl; 
                        all_is_well = false;
                        return;
                    }
                    if(protocol_and_version[1] != "1.1")
                    {
                        cerr << "httprequest.o: bad http version "
                             << "should be \"1.1\" but got \""
                             <<  protocol_and_version[1]
                             << "\""
                             << endl; 
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


}

int HttpRequest::readLine(int *parse_point, char *parsing_buffer)
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

QString HttpRequest::getHeader(const QString& field_label)
{
    HttpHeader *which_one = headers.find(field_label);
    if(which_one)
    {
        return which_one->getValue();
    }
    return NULL;
}

QString HttpRequest::getVariable(const QString& variable_name)
{
    HttpGetVariable *which_one = get_variables.find(variable_name);
    if(which_one)
    {
        return which_one->getValue();
    }
    return NULL;
}

HttpRequest::~HttpRequest()
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

