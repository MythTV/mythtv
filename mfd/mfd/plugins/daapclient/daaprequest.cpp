/*
	daaprequest.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    A little object for making daap requests

*/

#include <vector>
#include <iostream>
using namespace std;

#include "daaprequest.h"
#include "daapinstance.h"

DaapRequest::DaapRequest(
                            DaapInstance *owner,
                            const QString& l_base_url, 
                            const QString& l_host_address
                        )
{
    parent = owner;
    base_url = l_base_url;
    host_address = l_host_address;
    get_variables.setAutoDelete(true);
    stored_request = "";
}

void DaapRequest::addGetVariable(const QString& label, int value)
{
    QString int_string = QString("%1").arg(value);
    HttpGetVariable *new_get = new HttpGetVariable(label, int_string);
    get_variables.insert( label, new_get);
}

void DaapRequest::addGetVariable(const QString& label, const QString &value)
{
    HttpGetVariable *new_get = new HttpGetVariable(label, value);
    get_variables.insert( label, new_get);
}

void DaapRequest::addText(std::vector<char> *buffer, QString text_to_add)
{
    buffer->insert(buffer->end(), text_to_add.ascii(), text_to_add.ascii() + text_to_add.length());
}

bool DaapRequest::send(QSocketDevice *where_to_send, bool ignore_shutdown)
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
    //  You can change this to stored_request = extended URL if you want
    //  to get debugging output that includes the GET variables
    //
    
    stored_request = base_url;
    
    //
    //  Add a few more "standard" daap headers (ie. things that iTunes sends
    //  when it is a client)
    //

    addText(&the_request, "Cache-Control: no-cache\r\n");
    addText(&the_request, "Accept: */*\r\n");

    /*
        Might want to add these at some point
        
        x-audiocast-udpport:49154
        icy-metadata:1
    */
    
    addText(&the_request, "Client-DAAP-Version: 2.0\r\n");
    addText(&the_request, "User-Agent: MythTV/1.0 (Probably Linux)\r\n");

    //
    //  Add the server address (which the HTTP 1.1 spec is fairly adamant
    //  *must* be in there)
    // 
   
    QString host_line = QString("Host: %1\r\n").arg(host_address);
    addText(&the_request, host_line);

    
    //
    //  Add any additional headers that the calling program set
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

    sendBlock(the_request, where_to_send, ignore_shutdown);

    return true;
}

bool DaapRequest::sendBlock(std::vector<char> block_to_send, QSocketDevice *where_to_send, bool ignore_shutdown)
{
        
    //  Debugging:
    /*
    cout << "=========== Debugging Output - DAAP request being sent  ==================" << endl;
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
        warning("daap request was asked to sendBlock() "
                "of zero size ");
        keep_going = false;
    }


    while(keep_going)
    {
        if(parent)
        {
            if(!parent->keepGoing() && !ignore_shutdown)
            {
                //
                //  time to escape out of this
                //

                parent->log("daap request aborted a sendBlock() "
                        "as it's time to go", 6);

                return false;
            }
        }

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
            if(parent)
            {
                parent->warning("daap request got an error from "
                                "select() ... not sure what to do");
            }
        }
        else
        {
            if(!where_to_send)
            {
                if(parent)
                {
                    parent->warning("daap request's socket to the "
                                    "server went away in the middle "
                                    "of sending something");
                }
                return false;
            }
            if(where_to_send->socket() < 1)
            {
                if(parent)
                {
                    parent->warning("daap request's socket to the "
                                    "server got closed in the middle "
                                    "of sending something");
                }
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
                    //  Hmm, select() said we were ready, but now we're
                    //  getting an error ... server has gone away?
                    //
                    
                    if(parent)
                    {
                        parent->warning("daap request seems to have "
                                        "lost contact with the server ");
                    }
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
    }
    
    return true;
}

QString DaapRequest::getRequestString()
{
    QString return_value;
    if(stored_request.length() < 1)
    {
        return_value = "ERROR request not yet sent ERROR";
    }
    else
    {
        return_value = stored_request;
    }
    return return_value;
}

void DaapRequest::addHeader(const QString &new_header)
{
    HttpHeader *a_new_header = new HttpHeader(new_header);
    headers.insert(a_new_header->getField(), a_new_header);
}



DaapRequest::~DaapRequest()
{
    get_variables.clear();
}
