/*
	rtspinrequest.cpp

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for an object to hold and parse incoming rtsp requests

*/

#include "rtspinrequest.h"
#include "rtspserver.h"

#define MAX_WAIT_ATTEMPTS 120    //  max number of 1/2 second attempts to wait
                                 //  for more client data to arrive

RtspInRequest::RtspInRequest(
                                MFDRtspPlugin *owner, 
                                MFDServiceClientSocket *a_client,
                                bool dbo
                            )
{
    parent = owner;
    client_socket = a_client;
    all_is_well = true;
    timeout_count = 0;
    headers.setAutoDelete(true);
    payload_size = 0;
    request = RRT_UNKNOWN;
    command_sequence = -1;
    debug_on = dbo;
}

bool RtspInRequest::parseIncomingBytes()
{
    //
    //  We need to read the bytes coming in from the socket and parse them
    //  into a request. Start by eating one byte at a time until we get a
    //  blank line. This will fill in the raw message header block
    //

    bool keep_going = true;
    while(keep_going)
    {
        keep_going = eatOneByte();
    }
    if(!all_is_well)
    {
        return false;
    }


    //
    //  Need to parse the message header block into the request_line and headers.
    //

    parseMessageHeaderBlock();
    if(!all_is_well)
    {
        return false;
    }



    if(debug_on)
    {
        printYourself();
    }

    parseHeaders();
    if(!all_is_well)
    {
        return false;
    }

    if(payload_size > 0)
    {
        consumePayload();
        if(!all_is_well)
        {
            return false;
        }

    }

    parseRequestLine();
    if(!all_is_well)
    {
        return false;
    }

    if(request == RRT_UNKNOWN)
    {
        return false;
    }

    return true;
}

bool RtspInRequest::eatOneByte()
{
    //
    //  Read off the socket returning true until we get a blank line or some
    //  kind of error condition
    //

    char incoming;    
    int length = client_socket->readBlock(&incoming, 1);
    
    if (length == 1)
    {
        //
        //  Deal with CR/LF stuff
        //

        
        message_header_block.push_back(incoming);
        timeout_count = 0;

        if (incoming == '\r' || incoming == '\n')        
        {
            //
            //  We may be at the end of the opening message block
            //

            if (checkMessageHeadersEnd())
            {
                //
                //  All done
                //

                return false;
            }
            else
            {
                //
                //  Nope, we have not eaten all the way through the message
                //  header block
                //
                
                return true;
            }
        }
        
        return true;
    }
    else if (length == 0)
    {
        //
        //  Socket closed?
        //
    
        warning("RtspInRequest::eatOneByte() got socket slammed shut on it");
        all_is_well = false;
        return false;
    }
    else if (length < 1)
    {
        //
        //  Socket in error state, but error may simply be that data has not
        //  yet arrived. We wait for more, but only up to a limit.
        //
        
        bool keep_going = true;
        if(parent)
        {
            keep_going = parent->keepGoing();
        }
        
        if(keep_going)
        {
            ++timeout_count;
            if(timeout_count > MAX_WAIT_ATTEMPTS)
            {
                log(QString("RtspInRequest::eatOneByte() waited 0.5 seconds "
                            "x %1 times for client to complete request. "
                            "Giving up.").arg(MAX_WAIT_ATTEMPTS), 2);
                all_is_well = false;
                return false;
            }
            client_socket->waitForMore(500);
            return true;
        }
        else
        {
            all_is_well = false;
            return false;
        }
        
    }
    else if (length > 1)
    {
        warning("RtspInRequest::eatOneByte() asked for 1 byte and got more");
        all_is_well = false;
        return false;
    }
    
    warning("RtspInRequest::eatOneByte() has disproven the law of the "
            "excluded middle");
    all_is_well = false;
    return false;
}

bool RtspInRequest::checkMessageHeadersEnd()
{
    //
    //  Are the message headers over?
    //  Equivalently, did we get a blank line?
    //
    //  Start with special cases
    //
    
    if (message_header_block.size() < 2)
    {
        if (message_header_block[0] == '\r' || message_header_block[0] == '\n')
        {
            warning("RtspInRequest::checkMessageHeadersEnd() saw blank and "
                    "invalid request (CRLF mess)");
            all_is_well = false;
            return true;
        }
    }
    
    else if (message_header_block.size() == 2)
    {
        if ( (message_header_block[0] == '\r' || message_header_block[0] == '\n') &&
             (message_header_block[1] == '\r' || message_header_block[1] == '\n') )
        {
            warning("RtspInRequest::checkMessageHeadersEnd() saw blank and "
                    "largely invalid request (partial CRLF mess)");
            all_is_well = false;
            return true;
        }
    }
    else if (message_header_block.size() == 3)
    {
        if ( (message_header_block[1] == '\r' || message_header_block[1] == '\n') &&
             (message_header_block[2] == '\r' || message_header_block[2] == '\n') )
        {
            warning("RtspInRequest::checkMessageHeadersEnd() saw blank and "
                    "overly short request (short CRLF mess)");
            all_is_well = false;
            return true;
        }
    }
    else
    {
        int size = message_header_block.size();

        if ( message_header_block[size - 4] == '\r' &&
             message_header_block[size - 3] == '\n' &&
             message_header_block[size - 2] == '\r' &&
             message_header_block[size - 1] == '\n' )
        {
            //
            //  Perfect dual CRLF indicating end of message headers
            //
                
            return true;
        }
        else if ( message_header_block[size - 4] == '\n' &&
                  message_header_block[size - 3] == '\r' &&
                  message_header_block[size - 2] == '\n' &&
                  message_header_block[size - 1] == '\r' )
        {
            //
            //  *sigh* 
            //
            
            warning("RtspInRequest::checkMessageHeadersEnd() client is "
                    "sending LFCR instead of CRLF!");
            return true;
        }
        else
        {
            //
            //  Still need to check for stupid clients that send only CR's or only LF's
            //
            
            if ( message_header_block[size - 2] == '\r' &&
                 message_header_block[size - 1] == '\r' )
            {
                warning("RtspInRequest::checkMessageHeadersEnd() client is "
                        "sending only CR's to denote end of line (no LF)");
                return true;
            }
            if ( message_header_block[size - 2] == '\n' &&
                 message_header_block[size - 1] == '\n' )
            {
                warning("RtspInRequest::checkMessageHeadersEnd() client is "
                        "sending only LF's to denote end of line (no CR)");
                return true;
            }
        }
    }    
    
    return false;
}

void RtspInRequest::parseMessageHeaderBlock()
{

    QString current_line;
    for( uint parse = 0; parse < message_header_block.size(); ++parse)
    {
        char current = message_header_block.at(parse);
        if (current == '\r' || current == '\n' )
        {
            if (current_line.length() > 0)
            {
                if (request_line.length() == 0)
                {
                    request_line = current_line;
                }
                else
                {
                    HttpHeader *new_header = new HttpHeader(current_line);
                    if (new_header->ok())
                    {
                        headers.insert(new_header->getField(), new_header);
                    }
                    else
                    {
                        warning(QString("RtspInRequest::parseMessage"
                                        "HeaderBlock() could not parse this "
                                        "into a header: \"%1\"")
                                       .arg(current_line));
                        delete new_header;
                    }
                }
                current_line = "";
            }
        }
        else
        {
            current_line.append(current);
        }
    } 
}

void RtspInRequest::parseHeaders()
{
    //
    //  Check for a Content-Length header so we can tell if there's a
    //  payload to read off the client socket. Also, check for a CSeq
    //  header, as the RFC says there MUST be one.
    //
    
    HttpHeader *content_length_header = headers.find("Content-Length");
    if(content_length_header)
    {
        bool ok = true;
        int content_length = content_length_header->getValue().toInt(&ok);
        if(ok)
        {
            payload_size = content_length;
        }
        else
        {
            warning("RtspInRequest::parseHeaders() could not "
                    "figure out Content-Length header. Bad.");
        }
    }    

    HttpHeader *cseq_header = headers.find("CSeq");
    if(cseq_header)
    {
        bool ok = true;
        int cseq_number = cseq_header->getValue().toInt(&ok);
        if(ok)
        {
            command_sequence = cseq_number;
        }
        else
        {
            warning("RtspInRequest::parseHeaders() could not "
                    "figure out CSeq header.");
        }
    }   
    else
    {
        warning("RtspInRequest::parseHeaders() could not find "
                "a CSeq header (violates the spec)");
    } 
}

void RtspInRequest::consumePayload()
{
    if (payload_size < 1)
    {
        return;
    }

    //
    //  Just churn through exactly as much as the Content-Length header said
    //  should be there
    //
    

    bool keep_going = true;
    char incoming;    
    
    while(keep_going)
    {
        int length = client_socket->readBlock(&incoming, 1);
    
        if (length == 1)
        {
            payload.push_back(incoming);
            if(payload.size() == payload_size)
            {
                keep_going = false;
            }
            timeout_count = 0;
        }
        
        else if (length == 0)
        {
            //
            //  Socket closed?
            //
    
            warning("RtspInRequest::consumePayload() got socket "
                    "slammed shut on it");
            all_is_well = false;
            keep_going = false;
        }
        else if (length < 1)
        {
            //
            //  Socket in error state, but error may simply be that data has not
            //  yet arrived. We wait for more, but only up to a limit.
            //
        
            bool keep_waiting = true;
            if(parent)
            {
                keep_waiting = parent->keepGoing();
            }
        
            if(keep_waiting)
            {
                ++timeout_count;
                if(timeout_count > MAX_WAIT_ATTEMPTS)
                {
                    log(QString("RtspInRequest::consumePayload() waited 0.5 "
                                "seconds x %1 times for client to complete "
                                "request. Giving up.")
                                .arg(MAX_WAIT_ATTEMPTS), 2);
                    all_is_well = false;
                    keep_going = false;
                    keep_waiting = false;
                }
                else
                {
                    client_socket->waitForMore(500);
                }
            }
            else
            {
                all_is_well = false;
                keep_going = false;
            }
        }
        else if (length > 1)
        {
            warning("RtspInRequest::eatOneByte() asked for 1 byte and got more");
            all_is_well = false;
            keep_going = false;
        }
    }    
}

void RtspInRequest::parseRequestLine()
{
    //
    //  just want to check that it is valid, well formed, etc.
    //
    
    QString command_string = request_line.section(' ', 0, 0);
    
    if (command_string.upper() == "DESCRIBE")
    {
        request = RRT_DESCRIBE;
    }
    else if (command_string.upper() == "GET_PARAMETER")
    {
        request = RRT_GET_PARAMETER;
    }
    else if (command_string.upper() == "OPTIONS")
    {
        request = RRT_OPTIONS;
    }
    else if (command_string.upper() == "PAUSE")
    {
        request = RRT_PAUSE;
    }
    else if (command_string.upper() == "PLAY")
    {
        request = RRT_PLAY;
    }
    else if (command_string.upper() == "PING")
    {
        request = RRT_PING;
    }
    else if (command_string.upper() == "REDIRECT")
    {
        request = RRT_REDIRECT;
    }
    else if (command_string.upper() == "SETUP")
    {
        request = RRT_SETUP;
    }
    else if (command_string.upper() == "SET_PARAMETER")
    {
        request = RRT_SET_PARAMETER;
    }
    else if (command_string.upper() == "TEARDOWN")
    {
        request = RRT_TEARDOWN;
    }
    else
    {
        warning(QString("RtspInRequest::parseRequestLine() bad request "
                        "command in request line \"%1\"").arg(request_line));
        all_is_well = false;
        return;
    }

    request_url = QUrl(request_line.section(' ', 1, 1));

    if (! request_url.isValid() )
    {
        warning(QString("RtspInRequest::parseRequestLine() bad request URL "
                        "in request line \"%1\"").arg(request_line));
        all_is_well = false;
        return;
    }    

    QString protocol_and_version = request_line.section(' ', 2, 2);
    if (protocol_and_version != "RTSP/1.0")
    {
        warning(QString("RtspInRequest::parseRequestLine() bad protocol/"
                        "version in request line \"%1\"").arg(request_line));
        all_is_well = false;
        return;
    }
    
   
}

QString RtspInRequest::getHeaderValue(const QString &header_field)
{
    QString response = "";
    
    HttpHeader *which_header= headers[header_field];
    
    if(which_header)
    {
        response = which_header->getValue();
    }
    
    return response;
}


void RtspInRequest::log(const QString &log_message, int verbosity)
{
    if (parent)
    {
        parent->log(log_message, verbosity);
    }
    else
    {
        cout << "RtspInRequest parent'less log: "
             << log_message
             << endl;
    }
}

void RtspInRequest::warning(const QString &warning_message)
{
    if (parent)
    {
        parent->warning(warning_message);
    }
    else
    {
        cout << "RtspInRequest parent'less log: WARNING: "
             << warning_message
             << endl;
    }
}

void RtspInRequest::printYourself()
{
    cout << "C->S " << request_line << endl;
    if(headers.count() > 0)
    {
        QDictIterator<HttpHeader> iterator( headers );
        for( ; iterator.current(); ++iterator )
        {
            cout << "     " << iterator.currentKey() << ": " << iterator.current()->getValue() << endl;
        }
    }
    else
    {
        cout << "     " << "NO HEADERS" << endl;
    }
    
}

RtspInRequest::~RtspInRequest()
{
    headers.clear();
}


