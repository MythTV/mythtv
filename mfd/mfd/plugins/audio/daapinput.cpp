/*
	daapinput.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Magical little class that makes remote daap resources look like local
	direct access files
*/

#include <time.h>
#include <iostream>
using namespace std;

#include <qhostaddress.h>
#include <qstringlist.h>

#include "daapinput.h"
#include "../daapclient/daaprequest.h"

//
//  Set up the structure to make daap calls
//

DaapInput::DaapInput(MFDServicePlugin *owner, QUrl a_url, DaapServerType l_daap_server_type)
          :QIODevice()
{
    socket_to_daap_server = NULL;

    parent = owner;
    setFlags(IO_Direct);    //  Lie!
    all_is_well = true;
    my_url = a_url;
    daap_server_type = l_daap_server_type;
    connected = false;
    payload_size = 0;
    payload_bytes_read = 0;
    total_possible_range = 0;
    range_begin = 0;
    range_end = 0;
    connection_count = 0;
    
    //
    //  parse out my url
    //
    
    if(my_url.protocol() != "daap" &&
       my_url.protocol() != "mdaap")
    {
        warning(QString("I don't speak this protocol: %1")
                .arg(my_url.protocol()));
        all_is_well = false;
    }

    my_host = my_url.host();
    my_port = my_url.port();
    my_path_and_query = my_url.encodedPathAndQuery();
    
}

bool DaapInput::open(int)
{
    log(QString("beginning to go and retrieve daap://%1:%2%3")
        .arg(my_host)
        .arg(my_port)
        .arg(my_url.path())
        , 5);
    if(socket_to_daap_server)
    {
        delete socket_to_daap_server;
    }

    socket_to_daap_server = new QSocketDevice (QSocketDevice::Stream);
    socket_to_daap_server->setBlocking(false);

    QHostAddress host_address;
    if(!host_address.setAddress(my_host))
    {
        warning(QString("could not create address for this host: %1")
                .arg(my_host));
        all_is_well = false;
        return false;
    }
    
    int connect_tries = 0;
    
    
    while(! (socket_to_daap_server->connect(host_address, my_port)))
    {
        //
        //  We give this a few attempts. It can take a few on non-blocking sockets.
        //

        ++connect_tries;
        if(connect_tries > 10)
        {
            warning(QString("no luck connecting to %1:%2")
                    .arg(my_host)
                    .arg(my_port));
            all_is_well = false;
            connected = false;
            return connected;
        }
        
        //
        //  Sleep for a bit
        //

        struct timespec timeout;
        timeout.tv_sec = 0;
        timeout.tv_nsec = 1000000;
        nanosleep(&timeout, NULL);
        
    }
    
    connected = true;   
    connection_count++;
    
    //
    //  We have made the basic connection, now we need to see if we can get
    //  the file. Note we pass a Connection: close header, to let the server
    //  know we're going to close this connection whenever we feel like it.
    //
    
    int daap_request_id =  parent->getMfd()->getPluginManager()->bumpDaapRequestId();

    DaapRequest initial_request(
                                NULL, 
                                QString("daap://%1:%2%3").arg(my_host).arg(my_port).arg(my_path_and_query),
                                my_host,
                                daap_server_type,
                                parent->getMfd()->getPluginManager(),
                                daap_request_id
                               );

    initial_request.addHeader("Connection: close");
    initial_request.setHashingUrl(my_path_and_query);
    initial_request.addHeader(QString("Client-DAAP-Request-ID: %1").arg(daap_request_id));
    initial_request.send(socket_to_daap_server, true);


    //
    //  Get ourselves positioned on the first byte of the payload
    //    

    eatThroughHeadersAndGetToPayload();

    if(!all_is_well)
    {
        return false;
    }
    return true;


}

Q_LONG DaapInput::readBlock( char *data, long unsigned int maxlen )
{

    if(fake_seek_position >= total_possible_range)
    {
        if(fake_seek_position > total_possible_range)
        {
            warning("something tried to read beyond my stream size");
        }
        return 0;
    }


    //
    //  If there's anything in our payload buffer send that first
    //
    
    if(payload.size() > 0)
    {
        if(maxlen >= payload.size())
        {
            int how_much = payload.size();
            for(uint i = 0; i < payload.size(); i++)
            {
                data[i] = payload[i];
            }
            payload.clear();
            fake_seek_position += how_much;
            return how_much;
        }
        else
        {
            int how_much = maxlen;
            for(uint i = 0; i < maxlen; i++)
            {
                data[i] = payload.front();
                payload.erase(payload.begin());
            }
            fake_seek_position += how_much;
            return how_much;
        }
    }

    //
    //  Otherwise, we need to send whatever we can pull from the server
    //

    if(socket_to_daap_server->socket() > 0)
    {
        socket_to_daap_server->waitForMore (5000);  //  wait up to 5 seconds for data to arrive
        Q_LONG length = socket_to_daap_server->readBlock(data, maxlen);

        if(length > 0)
        {
            fake_seek_position += length;
            payload_bytes_read += length;
            return length;
        }
        else
        {
            warning("problem reading from daapserver");
            return -1;
        }
    }

    warning("daap server seems to have gone away");
    return -1;
}



Q_ULONG DaapInput::size() const
{
    return total_possible_range;
}




unsigned long int DaapInput::at() const
{
    return fake_seek_position;
}


bool DaapInput::at(unsigned long int an_offset)
{

    //
    //  If something want to seek to the end of the file, just say we're there
    //
    
    if(an_offset == total_possible_range)
    {
        fake_seek_position = an_offset;
        payload.clear();
        return true;
    }

    //
    //  Can't seek beyond the end of the file !
    //

    if(an_offset > total_possible_range)
    {
        warning("something wanted me to seek beyond my size");
        return false;
    } 

    //
    //  If we happen to be already seeked to where it wants us to be, then we're fine
    //
    
    if(an_offset == fake_seek_position)
    {
        return true;
    }


    //
    //  To actually "seek", first we need to throw away the current daap connection
    //

    socket_to_daap_server->close();
    delete socket_to_daap_server;
    socket_to_daap_server = NULL;
    socket_to_daap_server = new QSocketDevice (QSocketDevice::Stream);
    socket_to_daap_server->setBlocking(false);
    connected = false;
    
    //
    //  Reopen the connection
    //
    
    QHostAddress host_address;
    if(!host_address.setAddress(my_host))
    {
        warning(QString("could not create address for this host: %1")
                .arg(my_host));
        all_is_well = false;
        return false;
    }
    
    int connect_tries = 0;
    while(! (socket_to_daap_server->connect(host_address, my_port)))
    {
        //
        //  We give this a few attempts. It can take a few on non-blocking sockets.
        //

        ++connect_tries;
        if(connect_tries > 10)
        {
            warning(QString("no luck connecting to %1:%2")
                    .arg(my_host)
                    .arg(my_port));
            all_is_well = false;
            connected = false;
            return false;
        }
        
        //
        //  Sleep for a bit
        //

        struct timespec timeout;
        timeout.tv_sec = 0;
        timeout.tv_nsec = 1000000;
        nanosleep(&timeout, NULL);
        
    }
    
    connected = true;   
    connection_count++;

    //
    //  Send a request that positions us exactly where the at(int) seek call asked to be
    //
    
    int daap_request_id =  parent->getMfd()->getPluginManager()->bumpDaapRequestId();

    DaapRequest seek_request(
                                NULL, 
                                QString("daap://%1:%2%3").arg(my_host).arg(my_port).arg(my_path_and_query),
                                my_host,
                                daap_server_type,
                                parent->getMfd()->getPluginManager(),
                                daap_request_id
                               );

    seek_request.addHeader("Connection: close");
    seek_request.addHeader(QString("Range: bytes=%1-").arg(an_offset));    
    seek_request.setHashingUrl(my_path_and_query);
    seek_request.addHeader(QString("Client-DAAP-Request-ID: %1").arg(daap_request_id));
    seek_request.send(socket_to_daap_server, true);


    //
    //  Get ourselves positioned on the first byte of the payload
    //    

    eatThroughHeadersAndGetToPayload();

    if(!all_is_well)
    {
        warning("header eating failed");
        return false;
    }
    return true;

}





void DaapInput::close()
{
}

bool DaapInput::isOpen() const
{
    return true;
}

int DaapInput::status()
{
    return 1;
}

void DaapInput::eatThroughHeadersAndGetToPayload()
{

    //
    //  NB. This code assumes all the headers will be in the first block. If
    //  you think that sucks, you're probably right. Feel free to improve
    //  it.
    //
    
    static int buffer_size = 8192;
    char incoming[buffer_size];
    int length = 0;

    //
    //  Read off something from the socket
    //

    socket_to_daap_server->waitForMore (5000);  //  wait up to 5 seconds for data to arrive
    length = socket_to_daap_server->readBlock(incoming, buffer_size);

    if(length < 0)
    {
        warning("got no data from daap server (increase wait time?)");
        all_is_well = false;
        return;
    }

    //
    //  Parse what came in
    //
    
    int parse_point = 0;
    bool first_line = true;
    char parsing_buffer[buffer_size]; //  FIX
    QString top_line = "";

    while(readLine(&parse_point, parsing_buffer, incoming, length))
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
                //
                //  Bad response line
                //
                
                warning("bad response line from daap server");
                all_is_well = false;
                return;
            }
            if(line_tokens[0] != "HTTP/1.1")
            {
                //
                //  Bad http version
                //
                
                warning("bad HTTP version from daap server");
                all_is_well = false;
                return;
            }
                
            //
            //  Set the http status code
            //
                
            bool ok = true;
            int status_code = line_tokens[1].toInt(&ok);
            if(!ok)
            {
                //
                //  Couldn't parse status code
                //

                warning("daap server sent us un-parse-able status code");
                all_is_well = false;
                return;
                
            }
            if(status_code != 200 && status_code !=206)
            {
                //
                //  Bad status code
                //  
                
                warning("daap server sent us bad status code (not HTTP OK == 200|206)");
                warning(QString("raw is: %1").arg(incoming));
                all_is_well = false;
                return;
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

    //
    //  Everything from this point on is the beginning (or all, if it's really short) of the content payload
    //
        
    payload.clear();
    
    if(length > parse_point)
    {
        //
        //  We got some data in our first readBlock() which is actually part of the payload
        //
    
        for(int i = 0; i < length - parse_point; i++)
        {
            payload.push_back(incoming[parse_point + i]);
        }
    }


    payload_bytes_read = length - parse_point;
    
    //
    //  Make note of how many bytes we expect in total.
    //
    

    HttpHeader *content_length_header = headers.find("Content-Length");
    if(content_length_header)
    {
        bool ok = true;
        uint content_length = content_length_header->getValue().toUInt(&ok);
        if(ok)
        {
            payload_size = content_length;
        }
        else
        {
            warning("got bad payload size in http headers");
            payload_size = 0;
            all_is_well = false;
        }
    }
    else
    {
        //
        //  NO Content Length header !!!!!
        //
        
        warning("got no Content-Length header in response");
        payload_size = 0;
        all_is_well = false;
    }
    
    HttpHeader *content_range_header = headers.find("Content-Range");
    if(content_range_header)
    {
        
        //
        //  Parse the content range into useable variables
        //
    
        QString range_begin_string          = content_range_header->getValue().section('-', 0, 0);
        range_begin_string = range_begin_string.remove("bytes ");
        QString range_end_string            = content_range_header->getValue().section('-', 1, 1);
                range_end_string            = range_end_string.section('/', 0, 0);
        QString total_possible_range_string = content_range_header->getValue().section('/', -1, -1);

        bool ok = true;
        
        int range_begin_candidate = range_begin_string.toInt(&ok);
        if(ok)
        {
            range_begin = range_begin_candidate;         
        }
        else
        {
            range_begin = 0;
            all_is_well = false;
            warning(QString("could not set range begin from "
                            "this string: \"%1\"")
                            .arg(range_begin_string));
        }
        

        int range_end_candidate = range_end_string.toInt(&ok);
        if(ok)
        {
            range_end = range_end_candidate;         
        }
        else
        {
            range_end = 0;
            all_is_well = false;
            warning(QString("could not set range end from "
                            "this Content-Range header: %1")
                            .arg(content_range_header->getValue()));
        }
        
        int total_possible_range_candidate = total_possible_range_string.toInt(&ok);
        if(ok)
        {
            total_possible_range = total_possible_range_candidate;         
        }
        else
        {
            total_possible_range = 0;
            all_is_well = false;
            warning(QString("could not set total possible range"
                            "from this Content-Range header: %1")
                            .arg(content_range_header->getValue()));
        }
        
        if(all_is_well)
        {
            /*
            log(QString("parsed content range in daap response as "
                        "%1-%2/%3")
                        .arg(range_begin)
                        .arg(range_end)
                        .arg(total_possible_range), 9);
            */
            fake_seek_position = range_begin;
        }
        else
        {
            fake_seek_position = 0;
        }
    }
    else
    {
        total_possible_range = payload_size;
        fake_seek_position = 0;
        range_begin = 0;
        range_end = 0;
    }
}

int DaapInput::readLine(int *parse_point, char *parsing_buffer, char *raw_response, int raw_length)
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
         
        else if(raw_response[index] == '\r')
        {
            //
            // ignore
            //

            index++;
        }
        else if(raw_response[index] == '\n')
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
            parsing_buffer[amount_read] = raw_response[index];
            index++;
            amount_read++;
        }
    }
    *parse_point = index;
    return amount_read;
}

void DaapInput::log(const QString &log_message, int verbosity)
{
    if(parent)
    {
        QString marked_log_message = QString("from DaapInput object: %1").arg(log_message);
        parent->log(marked_log_message, verbosity);
    }
}

void DaapInput::warning(const QString &warn_message)
{
    if(parent)
    {
        QString marked_warn_message = QString("from DaapInput object: %1").arg(warn_message);
        parent->warning(marked_warn_message);
    }
}

DaapInput::~DaapInput()
{
    log(QString("finished with daap://%1:%2%3 (took %4 connection(s))")
        .arg(my_host)
        .arg(my_port)
        .arg(my_url.path())
        .arg(connection_count), 5);
    if(socket_to_daap_server)
    {
        delete socket_to_daap_server;
        socket_to_daap_server = NULL;
    }
}

