/*
	httpresponse.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for an object to buildup http responses

*/

#include <iostream>
using namespace std;
#include <unistd.h>

#include <qdatetime.h>
#include <qfile.h>

#include "httpresponse.h"


HttpResponse::HttpResponse()
{
    headers.setAutoDelete(true);
    all_is_well = true;
    status_code = 200;
    status_string = "OK";
    payload = NULL;
    payload_size = 0;
    file_to_serve = "";
}

void HttpResponse::setError(int error_number)
{
    all_is_well = false;
    
    if(error_number == 400)
    {
        status_code = error_number;
        status_string = "Bad Request";
    }
    else if(error_number == 401)
    {
        status_code = error_number;
        status_string = "Unauthorized";
    }
    else if(error_number == 402)
    {
        status_code = error_number;
        status_string = "Payment Required";
    }
    else if(error_number == 403)
    {
        status_code = error_number;
        status_string = "Forbidden";
    }
    else if(error_number == 404)
    {
        status_code = error_number;
        status_string = "Not Found";
    }
    else if(error_number == 405)
    {
        status_code = error_number;
        status_string = "Method Not Allowed";
    }
    else if(error_number == 406)
    {
        status_code = error_number;
        status_string = "Not Acceptable";
    }
    else if(error_number == 407)
    {
        status_code = error_number;
        status_string = "Proxy Authentication Required";
    }
    else if(error_number == 408)
    {
        status_code = error_number;
        status_string = "Request Time-out";
    }
    else if(error_number == 409)
    {
        status_code = error_number;
        status_string = "Conflict";
    }
    else if(error_number == 410)
    {
        status_code = error_number;
        status_string = "Gone";
    }
    else if(error_number == 411)
    {
        status_code = error_number;
        status_string = "Length Required";
    }
    else if(error_number == 412)
    {
        status_code = error_number;
        status_string = "Precondition Failed";
    }
    else if(error_number == 413)
    {
        status_code = error_number;
        status_string = "Request Entity Too Large";
    }
    else if(error_number == 414)
    {
        status_code = error_number;
        status_string = "Request-URI Too Large";
    }
    else if(error_number == 415)
    {
        status_code = error_number;
        status_string = "Unsupported Media Type";
    }
    else if(error_number == 416)
    {
        status_code = error_number;
        status_string = "Requested range not satisfiable";
    }
    else if(error_number == 417)
    {
        status_code = error_number;
        status_string = "Expectation Failed";
    }
    else if(error_number == 500)
    {
        status_code = error_number;
        status_string = "Internal Server Error";
    }
    else if(error_number == 501)
    {
        status_code = error_number;
        status_string = "Not Implemented";
    }
    else if(error_number == 502)
    {
        status_code = error_number;
        status_string = "Bad Gateway";
    }
    else if(error_number == 503)
    {
        status_code = error_number;
        status_string = "Service Unavailable";
    }
    else if(error_number == 504)
    {
        status_code = error_number;
        status_string = "Gateway Time-out";
    }
    else if(error_number == 505)
    {
        status_code = error_number;
        status_string = "HTTP Version not supported";
    }
    else
    {
        cerr << "httpresponse.o: somebody tried to set a non defined error code" << endl;
    }
}

void HttpResponse::addText(int *index, char* block, QString new_text)
{
    int position = *index;
    for(uint i = 0; i < new_text.length(); i++)
    {
        block[position] = new_text[i].latin1();
        position++;
    }
    *index = position;
}

void HttpResponse::send(MFDServiceClientSocket *which_client)
{
    char outgoing_block[MAX_CLIENT_OUTGOING];
    int  current_point = 0;

    //
    //  Build the first line
    //
    
    QString first_line = QString("HTTP/1.1 %1 %2\r\n")
                         .arg(status_code)
                         .arg(status_string);

    addText(&current_point, outgoing_block, first_line);

    //
    //  Always add the server header
    //
    
    QString server_header = QString("Server: MythTV Embedded Server\r\n");
    addText(&current_point, outgoing_block, server_header); 

    //
    //  Always do the date
    //

    QDateTime current_time = QDateTime::currentDateTime (Qt::UTC);
    QString date_header = QString("Date: %1 GMT\r\n").arg(current_time.toString("ddd, dd MMM yyyy hh:mm:ss"));
    addText(&current_point, outgoing_block, date_header);


    if(all_is_well)
    {
        if(file_to_serve.length() > 0)
        {
            QFile the_file(file_to_serve);
            if(the_file.exists())
            {
                QString file_size_header = QString("Content-Length: %1\r\n").arg(the_file.size());
                addText(&current_point, outgoing_block, file_size_header);
                payload = NULL;
                payload_size = 0;
                
                //
                //  Add an end of headers blank line
                //    
    
                outgoing_block[current_point] = '\r';
                current_point++;
                outgoing_block[current_point] = '\n';
                current_point++;

            }
            else
            {
                cout << "crappity crap crap" << endl;
            }
    
        }
        else
        {
            //
            //  No errors, send all headers
            //
        
            QDictIterator<HttpHeader> it( headers );
            for( ; it.current(); ++it )
            {
                QString a_header = QString("%1: %2\r\n")
                                   .arg(it.current()->getField())
                                   .arg(it.current()->getValue());
                addText(&current_point, outgoing_block, a_header);
            }
        
            //
            //  Add an end of headers blank line
            //    
    
            outgoing_block[current_point] = '\r';
            current_point++;
            outgoing_block[current_point] = '\n';
            current_point++;
        
            //
            //  the payload
            //
        
            if(payload)
            {
                for(int i = 0; i < payload_size; i++)
                {
                    outgoing_block[current_point] = payload[i];
                    current_point++;
                }
            }
        }
    }
    else
    {
        //
        //  Errors, just send an empty payload with minimal headers
        //
        
        QString content_type_header = QString("Content-Type: text/html\r\n");
        addText(&current_point, outgoing_block, content_type_header);

        QString content_length_header = QString("Content-Length: 0\r\n");
        addText(&current_point, outgoing_block, content_length_header);

        //
        //  Add a final blank line
        //    
    
        outgoing_block[current_point] = '\r';
        current_point++;
        outgoing_block[current_point] = '\n';
        current_point++;
    
    }
    
    //
    //  All done, send it
    //
    //  We may need to do multiple writes ...
    
    bool keep_writing = true;
    while(keep_writing)
    {
        int result = which_client->writeBlock(outgoing_block, current_point);
        cout << "sent " << result << " bytes" << endl;
        if(result < 0)
        {
            //
            //  Socket is full, wait a bit
            //

            usleep(100);
        }
        else
        {
            if(result == 0 || result == current_point)
            {
                keep_writing = false;
            }
            else if(result < current_point)
            {
                for(int i = 0; i < current_point - result; i++)
                {
                    ++*outgoing_block;
                }
                current_point = current_point - result;
            }
        }
    }

    if(file_to_serve.length() > 0)
    {
        QFile the_file(file_to_serve);
        if(the_file.open(IO_Raw | IO_ReadOnly))
        {
            char in_and_out[10000];
            int how_much;
            bool keep_going = true;
            while( keep_going)
            {
                how_much = the_file.readLine(in_and_out, 1024);
                if(which_client->writeBlock(in_and_out, how_much) < 0)
                {
                    keep_going = false;
                }
            }
            the_file.close();
        }
        
    }    
    
        
    outgoing_block[current_point] = '\0';
    cout << "######################################"
         << endl
         << "Sending:"
         << endl
         << outgoing_block
         << endl;
    
}

void HttpResponse::addHeader(const QString &new_header)
{
    HttpHeader *a_new_header = new HttpHeader(new_header);
    headers.insert(a_new_header->getField(), a_new_header);
}

void HttpResponse::setPayload(char *new_payload, int new_payload_size)
{
    if(new_payload_size > MAX_CLIENT_OUTGOING)
    {
        cerr << "httpresponse.o: something is trying to send an http request with a huge payload size of " << new_payload_size << endl;
        new_payload_size = MAX_CLIENT_OUTGOING;
    }

    if(payload)
    {
        delete payload;
        payload = NULL;
    }
    
    payload = new char[new_payload_size];
    
    for(int i = 0; i < new_payload_size; i++)
    {
        payload[i] = new_payload[i];
    }
    payload_size = new_payload_size;
}
    
HttpResponse::~HttpResponse()
{
    headers.clear();
    if(payload)
    {
        delete payload;
        payload = NULL;
    }
}

