/*
	httpoutrequest.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	

*/

#include <iostream>
using namespace std;

#include "httpinrequest.h"
#include "httpoutrequest.h"



HttpOutRequest::HttpOutRequest(
                                const QString &l_base_url,
                                const QString &l_host_address
                              )
{
    base_url = l_base_url;
    host_address = l_host_address;
    get_variables.setAutoDelete(true);
    stored_request = "";
}

void HttpOutRequest::addGetVariable(const QString& label, int value)
{
    QString int_string = QString("%1").arg(value);
    HttpGetVariable *new_get = new HttpGetVariable(label, int_string);
    get_variables.insert( label, new_get);
}

void HttpOutRequest::addGetVariable(const QString& label, const QString &value)
{
    HttpGetVariable *new_get = new HttpGetVariable(label, value);
    get_variables.insert( label, new_get);
}

void HttpOutRequest::addText(std::vector<char> *buffer, QString text_to_add)
{
    buffer->insert(buffer->end(), text_to_add.ascii(), text_to_add.ascii() + text_to_add.length());
}

bool HttpOutRequest::send(QSocketDevice *where_to_send)
{
    std::vector<char>  the_request;
    
    //
    //  Expand the base url with any Get Variables that exist
    //

    QString extended_url = base_url;
    bool first_get = true;
    
    QDictIterator<HttpGetVariable> it( get_variables );
    for( ; it.current(); ++it )
    {
        if(first_get)
        {
            extended_url.append("?");
            first_get = false;
        }
        else
        {
            extended_url.append("&");
        }
        extended_url.append(it.current()->getField());
        extended_url.append("=");
        extended_url.append(it.current()->getValue());
    }
    
    //
    //  Make the request line (eg. /server-info)
    //

    QString top_line = QString("GET %1 HTTP/1.1\r\n").arg(extended_url);
    addText(&the_request, top_line);

    //
    //  If there's a payload, add a Content-Length: header
    //
    
    if(payload.size() > 0)
    {
        QString content_length_header = QString("Content-Length: %1\r\n")
                                        .arg(payload.size());
        addText(&the_request, content_length_header);
    }

    //
    //  You can change this to stored_request = extended URL if you want
    //  to get debugging output that includes the GET variables
    //
    
    stored_request = base_url;
    
    //
    //  Add headers that the calling code set
    //
    
    QDictIterator<HttpHeader> an_it( headers );
    for( ; an_it.current(); ++an_it )
    {
        QString a_header = QString("%1: %2\r\n")
                           .arg(an_it.current()->getField())
                           .arg(an_it.current()->getValue());
        addText(&the_request, a_header);
    }
   
    //
    //  Add the final blank line
    //

    addText(&the_request, "\r\n");

    if(sendBlock(the_request, where_to_send))
    {
        if(payload.size() > 0)
        {
            sendBlock(payload, where_to_send);
        }
    }

    return true;
}

bool HttpOutRequest::sendBlock(std::vector<char> block_to_send, QSocketDevice *where_to_send)
{
        
    //  Debugging:
    
    /*
    cout << "=========== Debugging Output - HTTP request being sent  ==================" << endl;
    for(uint i = 0; i < block_to_send.size(); i++)
    {
        cout << block_to_send.at(i);
    }
    cout << "==========================================================================" << endl;
    */

    //
    //  May be overkill, but we do everything on select()'s in case the
    //  network is really slow/crappy.
    //

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
        warning("http out request was asked to sendBlock() "
                "of zero size ");
        keep_going = false;
    }


    while(keep_going)
    {

        FD_ZERO(&writefds);
        if(where_to_send)
        {
            if(where_to_send->socket() > 0)
            {
                FD_SET(where_to_send->socket(), &writefds);
                if(nfds <= where_to_send->socket())
                {
                    nfds = where_to_send->socket() + 1;
                }
            }
        }
        
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        int result = select(nfds, NULL, &writefds, NULL, &timeout);
        if(result < 0)
        {
            warning("http out request got an error from "
                    "select() ... not sure what to do");
        }
        else
        {
            if(!where_to_send)
            {
                warning("http out request's socket to a "
                        "server went away in the middle "
                        "of sending something");
                return false;
            }
            if(where_to_send->socket() < 1)
            {
                warning("http out request's socket to the "
                        "server got closed in the middle "
                        "of sending something");
                return false;
            }

            if(FD_ISSET(where_to_send->socket(), &writefds))
            {
                //
                //  Socket is available for writing
                //
            
                int bytes_sent = where_to_send->writeBlock( 
                                                            &(block_to_send[amount_written]), 
                                                            block_to_send.size() - amount_written
                                                          );
                if(bytes_sent < 0)
                {
                    //
                    //  If the error is QSocketDevice::NoError, keep trying
                    //  for a while
                    //

                    if (where_to_send->error() == QSocketDevice::NoError)
                    {
                        int try_more = 0;
                        bytes_sent = 0;
                        while(try_more < 10)
                        {
                            bytes_sent = where_to_send->writeBlock( 
                                                                    &(block_to_send[amount_written]), 
                                                                    block_to_send.size() - amount_written
                                                                  );
                            if(bytes_sent >= 0)
                            {
                                try_more = 20;
                            }
                            else
                            {
                                usleep(200);
                                try_more++;
                                if(try_more >= 10)
                                {
                                    warning("server not reachable even after multiple attempts, giving up");
                                    return false;
                                }
                            }
                        }
                    }
                    else
                    {
                        //
                        //  Hmm, select() said we were ready, but now we're
                        //  getting an error ... server has gone away?
                        //

                        warning("http out request seems to have "
                                "lost contact with the server ");
                        return false;
                    }
                
                }
                if(bytes_sent >= (int) (block_to_send.size() - amount_written))
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
    }
    
    return true;
}

QString HttpOutRequest::getRequestString()
{
    QString return_value;
    if(stored_request.length() < 1)
    {
        //
        //  It hasn't been sent yet, we have to assemble it ourself
        //

        QString extended_url = base_url;
        bool first_get = true;
    
        QDictIterator<HttpGetVariable> it( get_variables );
        for( ; it.current(); ++it )
        {
            if(first_get)
            {
                extended_url.append("?");
                first_get = false;
            }
            else
            {
                extended_url.append("&");
            }
            extended_url.append(it.current()->getField());
            extended_url.append("=");
            extended_url.append(it.current()->getValue());
        }

        return_value = extended_url;
    }
    else
    {
        return_value = stored_request;
    }
    return return_value;
}

void HttpOutRequest::addHeader(const QString &new_header)
{
    HttpHeader *a_new_header = new HttpHeader(new_header);
    headers.insert(a_new_header->getField(), a_new_header);
}

void HttpOutRequest::setPayload(QValueVector<char> *new_payload)
{
    uint new_payload_size = new_payload->size();

    if(new_payload_size > MAX_CLIENT_OUTGOING)
    {
        cerr << "httpresponse.o: something is trying to send an http "
             << "request with a huge payload size of " 
             << new_payload_size 
             << endl;

        new_payload_size = MAX_CLIENT_OUTGOING;
    }

    payload.clear();
    
    for(uint i = 0; i < new_payload_size; i++)
    {
        payload.insert(payload.end(), new_payload->at(i));
    }
}



void HttpOutRequest::warning(const QString &warn_text)
{
    cerr << "WARNING httpoutrequest.o: " << warn_text << endl;
}
                  
HttpOutRequest::~HttpOutRequest()
{
    get_variables.clear();
}


