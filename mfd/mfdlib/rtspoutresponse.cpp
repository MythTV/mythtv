/*
	rtspoutresponse.cpp

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for an object to build rtsp responses and send them
*/

#include <qdatetime.h>

#include "rtspserver.h"
#include "rtspoutresponse.h"

RtspOutResponse::RtspOutResponse(MFDRtspPlugin *owner, int cseq)
{
    parent = owner;
    command_sequence = cseq;
    headers.setAutoDelete(true);
    
    //
    //  Until something tells us otherwise, everything is fine
    //
    
    status_code = 200;
    status_string = "OK";
    
    //
    //  Build the standard headers
    //

    addHeader("CSeq", QString("%1").arg(command_sequence));
    addHeader("Server", "MythTV/1.0 (Probably Linux)");
    QDateTime current_time = QDateTime::currentDateTime (Qt::UTC);
    addHeader("Date", QString("%1").arg(current_time.toString("ddd, dd MMM yyyy hh:mm:ss")));
}

void RtspOutResponse::addHeader(const QString &field, const QString &value)
{
    HttpHeader *a_new_header = new HttpHeader(field, value);

    if(a_new_header->ok())
    {
        headers.insert(a_new_header->getField(), a_new_header);
    }
    else
    {
        warning(QString("RtspOutResponse::addHeader() can't make a header "
                        "with \"%1\" as a field value").arg(field));
    }
    
}

void RtspOutResponse::addText(std::vector<char> *buffer, QString text_to_add)
{
    buffer->insert(buffer->end(), text_to_add.ascii(), text_to_add.ascii() + text_to_add.length()); 
}



void RtspOutResponse::send(MFDServiceClientSocket *which_client)
{
    std::vector<char> outgoing_bytes;
    
    //
    //  Start with the response line
    //

    QString first_line = QString("RTSP/1.0 %1 %2\r\n")
                         .arg(status_code)
                         .arg(status_string);
    
    addText(&outgoing_bytes, first_line);
    
    QDictIterator<HttpHeader> it( headers );
    for( ; it.current(); ++it )
    {
        QString a_header = QString("%1: %2\r\n")
                           .arg(it.current()->getField())
                           .arg(it.current()->getValue());
        addText(&outgoing_bytes, a_header);
    }

    //
    //  Add the "end of headers" blank line
    //
    
    addText(&outgoing_bytes, "\r\n");

    sendBlock(which_client, outgoing_bytes);
}

bool RtspOutResponse::sendBlock(MFDServiceClientSocket *which_client, std::vector<char> &block_to_send)
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

        warning("rtspresponse asked to sendBlock(), "
                "but given block of zero size");
        keep_going = false;
    }


    while(keep_going)
    {
        if(parent)
        {
            if(!parent->keepGoing())
            {
                //
                //  We are shutting down or something terrible has happened
                //

                log("rtspresponse aborted sending some socket data because "
                    "it thinks its time to shut down", 6);

                return false;
            }
        }

        FD_ZERO(&writefds);

        if(which_client)
        {
            int a_socket = which_client->socket();
            if(a_socket > 0)
            {
                FD_SET(a_socket, &writefds);
                if(nfds <= a_socket)
                {
                    nfds = a_socket + 1;
                }
            }
            else
            {
                return false;
            }
        }
        else
        {
            return false;
        }
        
        timeout.tv_sec = 2;
        timeout.tv_usec = 0;
        int result = select(nfds, NULL, &writefds, NULL, &timeout);
        if(result < 0)
        {
            warning("rtspresponse got an error from "
                    "select() ... not sure what to do");
        }
        else
        {
            if(FD_ISSET(which_client->socket(), &writefds))
            {
                //
                //  Socket is available for writing
                //
            
                int bytes_sent = which_client->writeBlock( 
                                                            &(block_to_send[amount_written]), 
                                                            block_to_send.size() - amount_written
                                                         );
                if(bytes_sent < 0)
                {
                    //
                    //  Hmm, select() said we were ready, but now we're
                    //  getting an error ... client has gone away? This is
                    //  not usually a big deal (somebody may have just
                    //  pushed pause somewhere :-) )
                    //
                    
                    return false;
                
                }
                else if(bytes_sent >= (int) (block_to_send.size() - amount_written))
                {
                    //
                    //  All done
                    //
            
                    amount_written += bytes_sent;
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


void RtspOutResponse::log(const QString &log_message, int verbosity)
{
    if (parent)
    {
        parent->log(log_message, verbosity);
    }
    else
    {
        cout << "RtspOutResponse parent'less log: "
             << log_message
             << endl;
    }
}

void RtspOutResponse::warning(const QString &warning_message)
{
    if (parent)
    {
        parent->warning(warning_message);
    }
    else
    {
        cout << "RtspOutResponse parent'less log: WARNING: "
             << warning_message
             << endl;
    }
}



RtspOutResponse::~RtspOutResponse()
{
    headers.clear();
}

