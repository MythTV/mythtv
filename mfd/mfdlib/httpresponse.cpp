/*
	httpresponse.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for an object to buildup http responses

*/

#include <iostream>
using namespace std;
#include <fcntl.h>
#include <unistd.h>

#include <qdatetime.h>
#include <qfile.h>

#include "httpresponse.h"


HttpResponse::HttpResponse(HttpRequest *requestor)
{
    my_request = requestor;
    headers.setAutoDelete(true);
    all_is_well = true;
    status_code = 200;
    status_string = "OK";
    payload.clear();
}

void HttpResponse::setError(int error_number)
{
    all_is_well = false;
    
    //
    //  These are all HTTP 1.1 status responses
    //

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

void HttpResponse::addText(std::vector<char> *buffer, QString text_to_add)
{
    buffer->insert(buffer->end(), text_to_add.ascii(), text_to_add.ascii() + text_to_add.length()); 
}

void HttpResponse::send(MFDServiceClientSocket *which_client)
{
    std::vector<char> header_block;

    //
    //  Build the first line. Note that the status code and status string
    //  are already set (by default 200, "OK").
    //
    
    QString first_line = QString("HTTP/1.1 %1 %2\r\n")
                         .arg(status_code)
                         .arg(status_string);
    
    addText(&header_block, first_line);
    
    //
    //  Always add the server header
    //
    
    QString server_header = QString("Server: MythTV Embedded Server\r\n");
    addText(&header_block, server_header);

    //
    //  Always do the date
    //

    QDateTime current_time = QDateTime::currentDateTime (Qt::UTC);
    QString date_header = QString("Date: %1 GMT\r\n").arg(current_time.toString("ddd, dd MMM yyyy hh:mm:ss"));
    addText(&header_block, date_header);


    if(all_is_well)
    {


        //
        //  No errors, set the Content-Length Header
        //
        
        QString content_length_header = QString("Content-Length: %1\r\n").arg(payload.size());
        addText(&header_block, content_length_header);
        
        //
        //  Explain that we are happy to accept content ranges (which we'll figure out real soon now)
        //
        
        QString content_ranges_header = QString("Accept-Ranges: bytes\r\n");
        addText(&header_block, content_ranges_header);

        //
        //  Set any other headers that were assigned
        //
        
        QDictIterator<HttpHeader> it( headers );
        for( ; it.current(); ++it )
        {
            QString a_header = QString("%1: %2\r\n")
                               .arg(it.current()->getField())
                               .arg(it.current()->getValue());
            addText(&header_block, a_header);
        }
        
               
    }
    else
    {

        //
        //  Errors, just send an empty payload with minimal headers
        //

        payload.clear();

        QString content_type_header = QString("Content-Type: text/html\r\n");
        addText(&header_block, content_type_header);

        QString content_length_header = QString("Content-Length: 0\r\n");
        addText(&header_block, content_length_header);

    }

    //
    //
    //  Add an end of headers blank line
    //
           
    QString blank_line = QString("\r\n");
    addText(&header_block, blank_line);
    

    
    //
    //  All done, send it
    //
    //  (if the header goes through, try and send the payload)
    //
    
    if(sendBlock(which_client, header_block))
    {
        sendBlock(which_client, payload);    
    }
}


bool HttpResponse::sendBlock(MFDServiceClientSocket *which_client, std::vector<char> block_to_send)
{
    int nfds = 0;
    fd_set writefds;
    struct  timeval timeout;
    int amount_written = 0;
    bool keep_going = true;

    //
    //  Could be that our payload (for example) is empty.
    //

    if(block_to_send.size() < 1)
    {
        keep_going = false;
    }


    while(keep_going)
    {
        FD_ZERO(&writefds);
        FD_SET(which_client->socket(), &writefds);
        if(nfds <= which_client->socket())
        {
            nfds = which_client->socket() + 1;
        }
        
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        int result = select(nfds, NULL, &writefds, NULL, &timeout);
        if(result < 0)
        {
            cerr << "Crappity crap crap" << endl;
        }

        if(FD_ISSET(which_client->socket(), &writefds))
        {
            //
            //  Socket is available for writing
            //
            
            int bytes_sent = which_client->writeBlock( &(block_to_send[amount_written]), block_to_send.size() - amount_written);
            if(bytes_sent < 0)
            {
                //
                //  Hmm, select() said we were ready, but now we're getting
                //  an error ... client has gone away?
                //
                
                return false;
                
            }
            else if(bytes_sent >= (int) (block_to_send.size() - amount_written))
            {
                //
                //  All done
                //

                keep_going = false;
            }
            else
            {
                amount_written += bytes_sent;
            }
        }
        else
        {
            //
            //  We just time'd out
            //
        }

    }
    return true;
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


    payload.clear();

    payload.insert(payload.end(), new_payload, new_payload + new_payload_size);
}

void HttpResponse::sendFile(QString file_path)
{

    int	 fd;
    int  len;
	char buf[10000];

    fd = open(file_path.ascii(),O_RDONLY);
    if (fd < 0)
	{
	    //  my_request-> do a real warning()
	    cout << "Crappity crap crap" << endl;
	    setError(404);
        return;
    }

    //  lseek(fd, 0, SEEK_SET);
    len = read(fd, buf, 10000);
    while(len > 0)
    {
        payload.insert(payload.end(), buf, buf + len);
		len = read(fd, buf, 10000);
    }
	close(fd);
}
    
HttpResponse::~HttpResponse()
{
    headers.clear();
    payload.clear();
}

