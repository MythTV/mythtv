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
{
    parent = owner;
    raw_length = length;
    headers.setAutoDelete(true);
    get_variables.setAutoDelete(true);

/*
    
    //
    //  Read the response
    //
    
    int parse_point = 0;
    bool first_line = true;
    char parsing_buffer[10000]; //  FIX
    QString top_line = "";

    while(readLine(&parse_point, parsing_buffer, raw_incoming))
    {
        if(first_line)
        {
            //  
            //  First line of a daap response is pretty important ... do
            //  some checking that things are sane
            //
            
            
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

*/
    
}

int DaapResponse::readLine(int *parse_point, char *parsing_buffer, char *raw_request)
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



DaapResponse::~DaapResponse()
{
    headers.clear();
    get_variables.clear();
}
