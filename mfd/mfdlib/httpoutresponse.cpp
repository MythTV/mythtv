/*
    httpoutresponse.cpp

    (c) 2003 Thor Sigvaldason and Isaac Richards
    Part of the mythTV project
    
    Methods for an object to buildup http responses

*/

#include "../config.h"

#ifdef WMA_AUDIO_SUPPORT
#include <mythtv/ffmpeg/avformat.h>
#include <mythtv/ffmpeg/avcodec.h>
#endif

#include <iostream>
using namespace std;
#include <fcntl.h>
#include <unistd.h>
#include <math.h>

#include <qdatetime.h>
#include <qfileinfo.h>
#include <vorbis/vorbisfile.h>

#include <cdaudio.h>
extern "C" {
#include <cdda_interface.h>
#include <cdda_paranoia.h>
}



#include "httpoutresponse.h"
#include "httpserver.h"



//
//  Couple of WRITE_ macros that help us create WAV headers 
//

#define WRITE_U32(buf, x) *(buf)     = (unsigned char)((x)&0xff);\
                          *((buf)+1) = (unsigned char)(((x)>>8)&0xff);\
                          *((buf)+2) = (unsigned char)(((x)>>16)&0xff);\
                          *((buf)+3) = (unsigned char)(((x)>>24)&0xff);

#define WRITE_U16(buf, x) *(buf)     = (unsigned char)((x)&0xff);\
                          *((buf)+1) = (unsigned char)(((x)>>8)&0xff);



HttpOutResponse::HttpOutResponse(MFDHttpPlugin *owner, HttpInRequest *requestor)
{
    parent = owner;
    my_request = requestor;
    headers.setAutoDelete(true);
    all_is_well = true;
    status_code = 200;
    status_string = "OK";
    payload.clear();
    range_begin = -1;
    range_end = -1;
    total_possible_range = -1;
    stored_skip = 0;
    file_to_send = NULL;
    file_transformation = FILE_TRANSFORM_NONE;
    bytes_in_content_range_header = false;

    //
    //  flac stuff (only used if this response is going to end up streaming
    //  a flac as a wav)
    //
    
    flac_bitspersample = 0;
    flac_channels = 0;
    flac_frequency = 0;
    flac_totalsamples = 0;
    flac_client = NULL;
}

void HttpOutResponse::setError(int error_number)
{
    all_is_well = false;
    
    //
    //  These are all HTTP 1.1 status responses. Not strictly all *errors*,
    //  but you get the idea.
    //

    if(error_number == 204)
    {
        status_code = error_number;
        status_string = "No Content";
    }
    else if(error_number == 400)
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

void HttpOutResponse::addText(std::vector<char> *buffer, QString text_to_add)
{
    buffer->insert(buffer->end(), text_to_add.ascii(), text_to_add.ascii() + text_to_add.length()); 
}

void HttpOutResponse::createHeaderBlock(
                                        std::vector<char> *header_block,
                                        int payload_size
                                    )
{
    //
    //  Build the first line. Note that the status code and status string
    //  are already set (by default 200, "OK").
    //
    
    QString first_line = QString("HTTP/1.1 %1 %2\r\n")
                         .arg(status_code)
                         .arg(status_string);
    
    addText(header_block, first_line);
    
    //
    //  Always add the server header
    //
    
    QString server_header = QString("Server: MythTV Embedded Server\r\n");
    addText(header_block, server_header);

    //
    //  Always do the date
    //

    QDateTime current_time = QDateTime::currentDateTime (Qt::UTC);
    QString date_header = QString("Date: %1 GMT\r\n").arg(current_time.toString("ddd, dd MMM yyyy hh:mm:ss"));
    addText(header_block, date_header);


    if(all_is_well)
    {


        //
        //  No errors, set the Content-Length Header
        //
        
        QString content_length_header = QString("Content-Length: %1\r\n")
                                        .arg(payload_size);
        addText(header_block, content_length_header);
        
        //
        //  If the request said it was going to close the connection after
        //  the response, let the requesting client know that's just fine.
        //
        
        if(my_request)
        {
            if(my_request->getHeader("Connection") == "close")
            {
                QString connection_header = QString("Connection: close\r\n");
                addText(header_block, connection_header);
            }
        }

        //
        //  Explain that we are happy to accept content ranges (which now
        //  actually work - hurray!)
        //
        
        QString content_ranges_header = QString("Accept-Ranges: bytes\r\n");
        addText(header_block, content_ranges_header);
        
        //
        //  If the request that lead to this response was a Range: request
        //  (eg. seeking in a file) explain the byte ranges returned
        //
        
        if(my_request)
        {
            if(
                my_request->getHeader("Range") &&
                range_begin          > -1 &&
                range_end            > -1 &&
                total_possible_range > -1
              )
            {
                QString file_range_header;
                
                //
                //  Some versions of iTunes want the word "bytes" to appear
                //  in the Content-Range: header, and some don't
                //

                if(bytes_in_content_range_header)
                {                
                    file_range_header = QString("Content-Range: bytes %1-%2/%3\r\n")
                                                .arg(range_begin)
                                                .arg(range_end)
                                                .arg(total_possible_range);
                }
                else
                {
                    file_range_header = QString("Content-Range: %1-%2/%3\r\n")
                                                .arg(range_begin)
                                                .arg(range_end)
                                                .arg(total_possible_range);
                }
                addText(header_block, file_range_header);
                if((range_end - range_begin) + 1 != payload_size)
                {
                        warning(QString("httpresponse is sending a range from "
                                        "%1 to %2 (size of %3), but "
                                        "the payload size is set to %4")
                                        .arg(range_begin)
                                        .arg(range_end)
                                        .arg((range_end - range_begin) + 1)
                                        .arg(payload_size));
                }
            }
        }

        //
        //  Set any other headers that were assigned
        //
        
        QDictIterator<HttpHeader> it( headers );
        for( ; it.current(); ++it )
        {
            QString a_header = QString("%1: %2\r\n")
                               .arg(it.current()->getField())
                               .arg(it.current()->getValue());
            addText(header_block, a_header);
        }
        
    }
    else
    {

        //
        //  Errors, just send an empty payload with minimal headers
        //

        payload.clear();

        QString content_type_header = QString("Content-Type: text/html\r\n");
        addText(header_block, content_type_header);

        QString content_length_header = QString("Content-Length: 0\r\n");
        addText(header_block, content_length_header);

    }

    //
    //
    //  Add an end of headers blank line
    //
           
    QString blank_line = QString("\r\n");
    addText(header_block, blank_line);
    
    
    
    /*    Debuggin Output:  */
   
    
    /*

    if(my_request)
    {
        my_request->printHeaders();
    }
    cout << "&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&& " << endl;
    for(uint i = 0; i < header_block->size(); i++)
    {
        cout << header_block->at(i);
    }
    cout << "&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&& " << endl;
    
    */
}


void HttpOutResponse::send(MFDServiceClientSocket *which_client)
{

    if(file_to_send)
    {
        //
        //  We are sending a file, not the payload in memory!
        //  Call the relevant function to stream this file
        //
        
        if(file_transformation == FILE_TRANSFORM_NONE)
        {
            streamFile(which_client);
        }
        else if(file_transformation == FILE_TRANSFORM_TOWAV)
        {
            convertToWavAndStreamFile(which_client);
        }
        else
        {
            //
            //  Should not happen
            //
            
            warning("httpresponse got an invalid conversion value");
        }
        
        
        file_to_send->close();
        delete file_to_send;
        file_to_send = NULL;

    }
    else
    {
        //
        //  Send already created payload
        //

        std::vector<char> header_block;
        createHeaderBlock(&header_block, payload.size());

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

}


bool HttpOutResponse::sendBlock(MFDServiceClientSocket *which_client, std::vector<char> &block_to_send)
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
        if(parent)
        {
            if(all_is_well)
            {
                //
                //  Hmmm ... we're not in an error state, but something
                //  still tried to send an empty payload
                //

                warning("httpresponse asked to sendBlock(), "
                        "but given block of zero size");
            }
        }
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

                log("httpresponse aborted sending some socket data because "
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
            warning("httpresponse got an error from "
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

void HttpOutResponse::addHeader(const QString &new_header)
{
    HttpHeader *a_new_header = new HttpHeader(new_header);
    headers.insert(a_new_header->getField(), a_new_header);
}

void HttpOutResponse::setPayload(char *new_payload, int new_payload_size)
{
    if(new_payload_size > MAX_CLIENT_OUTGOING)
    {
        cerr << "httpresponse.o: something is trying to send an http "
             << "request with a huge payload size of " 
             << new_payload_size 
             << endl;

        new_payload_size = MAX_CLIENT_OUTGOING;
    }


    payload.clear();

    payload.insert(payload.end(), new_payload, new_payload + new_payload_size);
}

void HttpOutResponse::setPayload(QValueVector<char> *new_payload)
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

void HttpOutResponse::clearPayload()
{
    payload.clear();
}

void HttpOutResponse::addToPayload(const QString &new_content)
{

    for(uint i = 0; i < new_content.length(); i++)
    {
        payload.push_back(new_content.at(i));
    }
}

void HttpOutResponse::sendFile(QString file_path, int skip, FileSendTransformation transform)
{
    //
    //  Store the skip value (in case we need it later for a transformed file stream)
    //
    
    stored_skip = skip;
    
    //
    //  Store whatever transformation has been requested
    //
    
    file_transformation = transform;
    
    //
    //  Because we're sending a file, we need to fill in the boundaries of
    //  the file size, so that the client can send seeking requests.
    //
    
    file_to_send = new QFile(file_path.local8Bit());
    QFileInfo file_info(file_path.local8Bit());
    if(file_info.extension(false) == "cda")
    {
        range_begin = skip;
        return;
    }

    if(!file_to_send->exists())
    {
        warning(QString("httpresponse was asked to send "
                        "a file that does not exist: %1")
                        .arg(file_path.local8Bit()));
        setError(404);
        delete file_to_send;
        file_to_send = NULL;
        return;
    }

    //
    //  Figure out which part of the file to send, with a little sanity
    //  checking.
    //

    range_begin = skip;
    range_end = file_to_send->size() - 1;
    if(range_end < 0)
    {
        range_end = 0;
    }
    if(range_begin > range_end)
    {
        range_begin = 0;
    }

    total_possible_range = file_to_send->size();

    //
    //  Do some checking to see if the file is actually available
    //

    if(!file_to_send->open(IO_ReadOnly | IO_Raw))
    {
        warning(QString("httpresponse could not open (permissions?) a "
                        "file it was asked to send: %1")
                       .arg(file_path.local8Bit()));
        setError(404);
        delete file_to_send;
        file_to_send = NULL;
        return;
    }
    
    //
    //  Leave the file open for the actual send() function.
    //    


}


void HttpOutResponse::streamFile(MFDServiceClientSocket *which_client)
{

    std::vector<char> header_block;

    //
    //  Seek to where we need to be
    //

    if(!file_to_send->at(range_begin))
    {
        warning("httpresponse could not seek in a "
                "file it was asked to send. This is bad.");
        setError(404);
        createHeaderBlock(&header_block, 0);
        sendBlock(which_client, header_block);
        return;
    }
    
    createHeaderBlock(&header_block, (range_end - range_begin) + 1);
    if(!sendBlock(which_client, header_block))
    {
        warning("httpresponse could not send header block to client");
        return;
    }

    int  len;
    char buf[8192];

    len = file_to_send->readBlock(buf, 8192);
    while(len > 0)
    {
        payload.clear();
        payload.insert(payload.begin(), buf, buf + len);
        if(!sendBlock(which_client, payload))
        {
            //
            //  Stop sending 
            //
                
            log("httpresponse failed to send block "
                "while streaming file (client gone?)", 9);
            return;
        }
        else
        {
            len = file_to_send->readBlock(buf, 8192);
        }

        if(parent)
        {
            //
            //  Check to see if our parent is trying to stop 
            //
            
            if(!parent->keepGoing())
            {
                //
                //  Oh crap, we're shutting down
                //

                log(QString("httpresponse aborted reading a file "
                            "to send because it thinks its time "
                            "to shut down"), 6);
                return;
            }
        }
    }
}

//
//  *THREE* C functions that flac decoding (see below) needs to use because it
//  uses callbacks
//



extern "C" FLAC__StreamDecoderWriteStatus flacWriteCallback(
                                                            const FLAC__FileDecoder *flac_decoder, 
                                                            const FLAC__Frame *frame, 
                                                            const FLAC__int32 *const buffer[], 
                                                            void *client_data
                                                           )
{
    flac_decoder = flac_decoder;
    HttpOutResponse *http_response = (HttpOutResponse *)client_data;
    return http_response->flacWrite(frame, buffer);
}


extern "C" void flacMetadataCallback(
                                    const FLAC__FileDecoder *decoder, 
                                    const FLAC__StreamMetadata *metadata, 
                                    void *client_data
                                    )
{
    decoder = decoder;
    HttpOutResponse *http_response = (HttpOutResponse *)client_data;
    http_response->flacMetadata(metadata);
}

extern "C" void flacErrorCallback(
                                    const FLAC__FileDecoder *decoder, 
                                    FLAC__StreamDecoderErrorStatus status, 
                                    void *client_data
                                 )
{
    decoder = decoder;
    HttpOutResponse *http_response = (HttpOutResponse *)client_data;
    http_response->flacError(status);
}


//
//  *THREE* corresponding object methods that do the real work of the
//  *callbacks above
//


FLAC__StreamDecoderWriteStatus HttpOutResponse::flacWrite(
                                                        const FLAC__Frame *frame, 
                                                        const FLAC__int32 *const buffer[]
                                                      )
{

    //
    // First thing we do is check to see if we should be shutting down 
    //
    
    if(parent)
    {
        if(!parent->keepGoing())
        {
            warning("httpresponse interrupted \"flac as wav\" "
                    "streaming as it is time to shut down ");
            return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
        }
        
    }
    
    //
    //  make sure we have a client to send to
    //
    
    if(!flac_client)
    {
        warning("httpresponse could send flac PCM data as "
                "it has no pointer to a client");
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }
    
    
    //
    //  take the data we got from the decoder and munge into the right form
    //  to send out the socket
    //

    unsigned int samples = frame->header.blocksize;


    //
    //  create a buffer just large enough to hold all the PCM data
    //

    int  buffer_length = samples * flac_channels * (flac_bitspersample / 8);
    char *out_buffer = new char [buffer_length];
    
    buffer_length = 0;

    unsigned int cursamp;
    int sample;
    int channel;


    //
    //  Stuff the data into the buffer
    //

    if (flac_bitspersample == 8)
    {
        for (cursamp = 0; cursamp < samples; cursamp++)
        {
            for (channel = 0; channel < flac_channels; channel++)
            {
               sample = (FLAC__int8)buffer[channel][cursamp];
               *(out_buffer + buffer_length++) = ((sample >> 0) & 0xff);
            }
        }   
    }
    else if (flac_bitspersample == 16)
    {
        for (cursamp = 0; cursamp < samples; cursamp++)
        {
            for (channel = 0; channel < flac_channels; channel++)
            { 
               sample = (FLAC__int16)buffer[channel][cursamp];             
               *(out_buffer + buffer_length++) = ((sample >> 0) & 0xff);
               *(out_buffer + buffer_length++) = ((sample >> 8) & 0xff);
            }
        }
    }
    
    //
    //  Send the buffer (PCM) data down the socket
    //

    payload.clear();
    payload.insert(payload.begin(), out_buffer, out_buffer + buffer_length);
    if(!sendBlock(flac_client, payload))
    {
        log("httpresponse failed to send block while streaming "
            "flac file (client gone?)", 9);
        delete [] out_buffer;
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }

    delete [] out_buffer;
    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void HttpOutResponse::flacMetadata(const FLAC__StreamMetadata *metadata)
{
    flac_bitspersample = metadata->data.stream_info.bits_per_sample;
    flac_channels      = metadata->data.stream_info.channels;
    flac_frequency     = metadata->data.stream_info.sample_rate;
    flac_totalsamples  = metadata->data.stream_info.total_samples;
}

void HttpOutResponse::flacError(FLAC__StreamDecoderErrorStatus status)
{
    status = status;
    
    log("httpresponse got an error during flac "
        "decoding, but will try to continue", 3);
}



void HttpOutResponse::convertToWavAndStreamFile(MFDServiceClientSocket *which_client)
{
    std::vector<char> header_block;

    if(file_to_send->name().section(".", -1, -1) == "ogg")
    {
        //
        //  Decode and send an ogg. This is pretty much taken from the
        //  vorbistools example program oggdec, which is Copyright 2002,
        //  Michael Smith <msmith@labyrinth.net.au>
        //
        
        OggVorbis_File ov_file;
        FILE *input_file = fopen(file_to_send->name().local8Bit(), "rb");
        if(!input_file)
        {
            warning(QString("while trying to convert ogg to wav "
                            "and stream it, httpresponse could "
                            "not open file: %1")
                            .arg(file_to_send->name().local8Bit()));
            streamEmptyWav(which_client);
            return;
        }
        
        if(ov_open(input_file, &ov_file, NULL, 0) < 0)
        {
            //
            //  Crap, this is not really an ogg file.
            //

            warning(QString("httpresponse does not really think "
                            "this file is an ogg: %1 ")
                            .arg(file_to_send->name().local8Bit()));
            fclose(input_file);
            streamEmptyWav(which_client);
            return;
        }
        
        //
        //  Figure out the PCM length of this file with the 44 bytes
        //  required for a WAV header. Note that this assumes the bits per
        //  sample will always be 16.
        //
        
        int64_t final_file_size = 44 +
                                ((
                                ov_pcm_total(&ov_file, 0) *
                                ov_info(&ov_file, 0)->channels *
                                16
                                ) / 8 );

        //
        //  Do some calculations to deal with range requests. 
        //

        range_begin = stored_skip;
        range_end = final_file_size - 1;
        if(range_end < 0)
        {
            range_end = 0;
        }
        if(range_begin > range_end)
        {
            range_begin = 0;
        }

        total_possible_range = final_file_size;
        if(range_begin > 0 && range_begin < 44)
        {
            warning("client asked to seek in an ogg "
                    "stream to somewhere inside the "
                    "header");
            range_begin = 44;
        }
        
        //
        //  Now that we know the size, we can make and send the header block
        //
        
        header_block.clear();
        createHeaderBlock(&header_block, ((int) (range_end - range_begin) + 1));
        if(!sendBlock(which_client, header_block))
        {
            warning("httpresponse was not able to send "
                    "an http header for an ogg stream");
            ov_clear(&ov_file);
            return;
        }

        int bits = 16;
        if(range_begin == 0)
        {
            //
            //  Build the wav header, but only if the client hasn't already
            //  asked to seek to a point beyond it
            //
        
            unsigned char headbuf[44];

            int channels = ov_info(&ov_file,0)->channels;
            int samplerate = ov_info(&ov_file,0)->rate;
            int bytespersec = channels*samplerate*bits/8;
            int align = channels*bits/8;
            int samplesize = bits;
                   
            memcpy(headbuf, "RIFF", 4);
            WRITE_U32(headbuf+4, final_file_size-8);
            memcpy(headbuf+8, "WAVE", 4);
            memcpy(headbuf+12, "fmt ", 4);
            WRITE_U32(headbuf+16, 16);
            WRITE_U16(headbuf+20, 1); /* format */
            WRITE_U16(headbuf+22, channels);
            WRITE_U32(headbuf+24, samplerate);
            WRITE_U32(headbuf+28, bytespersec);
            WRITE_U16(headbuf+32, align);
            WRITE_U16(headbuf+34, samplesize);
            memcpy(headbuf+36, "data", 4);
            WRITE_U32(headbuf+40, final_file_size - 44);

            payload.clear();
            payload.insert(payload.begin(), headbuf, headbuf + 44);
            if(!sendBlock(which_client, payload))
            {
                warning("httpresponse was not able to send "
                        "a wav header for an ogg");
                ov_clear(&ov_file);
                return;
            }
        }
        
        //
        //  Seek to where we need to be
        //

        ogg_int64_t ogg_seek_location = 0;
        
        if(range_begin > 43 && ov_info(&ov_file, 0)->channels > 0)
        {
            ogg_seek_location= (range_begin - 44) 
                               / 
                               ((
                                ov_info(&ov_file, 0)->channels *
                                16
                               ) / 8 );
        }
        
        if( ogg_seek_location > 0 && 
            ogg_seek_location < ov_pcm_total(&ov_file, 0))
        {
            int seek_result = ov_pcm_seek(&ov_file, ogg_seek_location);
    
            if(seek_result != 0)
            {
                    warning("httpresponse was not able to seek "
                            "in an ogg file");
                    ov_clear(&ov_file);
                    return;
            }
        }


        //
        //  Decode and stream it out
        //
        
        char buf[8192];
        int buflen = 8192;
        int section = 0;
        int len;

        len = ov_read(&ov_file, buf, buflen, 0, bits/8, 1, &section);
        while(len != 0)
        {
            payload.clear();
            payload.insert(payload.begin(), buf, buf + len);
            if(!sendBlock(which_client, payload))
            {
                //
                //  Stop sending 
                //
                
                log("httpresponse failed to send block "
                    "while streaming ogg file (client gone?)", 9);

                len = 0;
            }
            else
            {
                len = ov_read(&ov_file, buf, buflen, 0, bits/8, 1, &section);
            }

            if(parent)
            {
                //
                //  Check to see if our parent is trying to stop 
                //
            
                if(!parent->keepGoing())
                {
                    //
                    //  Oh crap, we're shutting down
                    //

                    log(QString("httpresponse aborted reading a file "
                                "to send because it thinks its time "
                                "to shut down"), 6);
                    len = 0;
                }
            }
            if(section != 0)
            {
                warning(QString("httpresponse encountered multiple "
                                "sections in an ogg file and doesn't "
                                "really know what it should do ..."));
                len = 0;
            }
            if(len < 0)
            {
                warning(QString("httpresponse found a hole in an "
                                "ogg file, will try to keep going"));
                //
                //  Try up to 3 (?) times
                //
                int numb_tries = 0;
                while(numb_tries < 3)
                {
                    len = ov_read(&ov_file, buf, buflen, 0, bits/8, 1, &section);
                    if(len > 0)
                    {
                        numb_tries = 4;
                    }
                    else
                    {
                        len = 0;
                    }
                }
            }
        }
        
        ov_clear(&ov_file);
    }
    else if(
            file_to_send->name().section(".", -1, -1) == "flac" ||
            file_to_send->name().section(".", -1, -1) == "fla"
           )
    {
        //
        //  Decode and send a flac.
        //
        
        FLAC__FileDecoder *flac_decoder = FLAC__file_decoder_new();
        flac_client = which_client;

        //
        //  Setup the decoder with the file to decode and all the silly
        //  callbacks
        //

        if(!FLAC__file_decoder_set_md5_checking(flac_decoder, false))
        {
            warning("httpresponse failed to turn of md5 checking on a flac file");
            FLAC__file_decoder_delete(flac_decoder);
            streamEmptyWav(which_client);
            return;
        }        

        if(!FLAC__file_decoder_set_filename(flac_decoder, file_to_send->name().local8Bit()))
        {
            warning("httpresponse failed to set a filename for a flac decode");
            FLAC__file_decoder_delete(flac_decoder);
            streamEmptyWav(which_client);
            return;
        }

        if(!FLAC__file_decoder_set_write_callback(flac_decoder, flacWriteCallback))
        {
            warning("httpresponse failed to set a flac write callback");
            FLAC__file_decoder_delete(flac_decoder);
            streamEmptyWav(which_client);
            return;
        }        
        
        if(!FLAC__file_decoder_set_metadata_callback(flac_decoder, flacMetadataCallback))
        {
            warning("httpresponse failed to set a flac metadata callback");
            FLAC__file_decoder_delete(flac_decoder);
            streamEmptyWav(which_client);
            return;
        }

        if(!FLAC__file_decoder_set_error_callback(flac_decoder, flacErrorCallback))
        {
            warning("httpresponse failed to set a flac error callback");
            FLAC__file_decoder_delete(flac_decoder);
            streamEmptyWav(which_client);
            return;
        }

        if(!FLAC__file_decoder_set_client_data(flac_decoder, this))
        {
            warning("httpresponse failed to set flac client data");
            FLAC__file_decoder_delete(flac_decoder);
            streamEmptyWav(which_client);
            return;
        }

        if(FLAC__file_decoder_init(flac_decoder) != FLAC__FILE_DECODER_OK)
        {
            warning("httpresponse failed to init flac decoder");
            FLAC__file_decoder_delete(flac_decoder);
            streamEmptyWav(which_client);
            return;
        }


        //
        //  Tell the decoder to read in metadata (execution will jump to
        //  flacMetadata(), then come back here). We need the metadata to
        //  calculate the size of the file once decoded (so that we can
        //  build the HTTP headers with a Content-Length: setting).
        //

        if(!FLAC__file_decoder_process_until_end_of_metadata(flac_decoder))
        {
            warning("httpresponse failed to extract flac metadata");
            FLAC__file_decoder_finish (flac_decoder);
            FLAC__file_decoder_delete(flac_decoder);
            streamEmptyWav(which_client);
            return;
        }
        
        //
        //  Check that we got "sensible" metadata.
        //
        
        if(
            flac_bitspersample == 0 ||
            flac_channels == 0 ||
            flac_frequency == 0 ||
            flac_totalsamples == 0
          )
        {
            warning("httpresponse did not get sensible flac metadata");
            FLAC__file_decoder_finish (flac_decoder);
            FLAC__file_decoder_delete(flac_decoder);
            streamEmptyWav(which_client);
            return;
        }
        
        //
        //  Figure out the PCM length of this file with the 44 bytes
        //  required for a WAV header. I (thor) have no idea if this
        //  math is correct, but it seems to work for 16 bit 2 channel
        //  flacs at 44100.
        //
        
        int64_t final_file_size = 44 +
                                ((
                                flac_totalsamples *
                                flac_channels *
                                flac_bitspersample
                                ) / 8 );


        //
        //  Handle range requests (seeking)
        //
        
        range_begin = stored_skip;
        range_end = final_file_size - 1;
        
        if(range_end < 0)
        {
            range_end = 0;
        }
        if(range_begin > range_end)
        {
            range_begin = 0;
        }

        total_possible_range = final_file_size;

        if(range_begin > 0 && range_begin < 44)
        {
            warning("client asked to seek in a flac "
                    "stream to somewhere inside the "
                    "header");
            range_begin = 44;
        }
        
        //
        //  We now know how big the file will be. Send the HTTP header.
        //
        
        header_block.clear();
        createHeaderBlock(&header_block, ((int) (range_end - range_begin) + 1));
        if(!sendBlock(which_client, header_block))
        {
            warning("httpresponse was not able to send "
                    "an http header for a flac stream");
            FLAC__file_decoder_finish (flac_decoder);
            FLAC__file_decoder_delete(flac_decoder);
            return;
        }
        
        //
        //  Seek to where we need to be
        //

        if(range_begin > 43 && flac_channels > 0)
        {
            FLAC__uint64 seek_position = (range_begin - 44) 
                                         /
                                         ((
                                            flac_channels *
                                            flac_bitspersample
                                         ) / 8); 
            
            if(!FLAC__file_decoder_seek_absolute(flac_decoder, seek_position))
            {
                warning("httpresponse could not seek in a flac "
                        "file it was trying to stream");
                FLAC__file_decoder_finish (flac_decoder);
                FLAC__file_decoder_delete(flac_decoder);
                
                streamEmptyWav(which_client);
                return;
            }
        }

        if(range_begin == 0)
        {
            //
            //  Build the wav header, but only if the client hasn't already
            //  asked to seek to a point beyond it
            //
        
            unsigned char headbuf[44];

            int bits = flac_bitspersample;
            int channels = flac_channels;
            int samplerate = flac_frequency;
            int bytespersec = (flac_channels * samplerate * flac_bitspersample )/8;
            int align = channels*bits/8;
            int samplesize = bits;
                   
            memcpy(headbuf, "RIFF", 4);
            WRITE_U32(headbuf+4, final_file_size-8);
            memcpy(headbuf+8, "WAVE", 4);
            memcpy(headbuf+12, "fmt ", 4);
            WRITE_U32(headbuf+16, 16);
            WRITE_U16(headbuf+20, 1); // format
            WRITE_U16(headbuf+22, channels);
            WRITE_U32(headbuf+24, samplerate);
            WRITE_U32(headbuf+28, bytespersec);
            WRITE_U16(headbuf+32, align);
            WRITE_U16(headbuf+34, samplesize);
            memcpy(headbuf+36, "data", 4);
            WRITE_U32(headbuf+40, final_file_size - 44);

            payload.clear();
            payload.insert(payload.begin(), headbuf, headbuf + 44);
            if(!sendBlock(which_client, payload))
            {
                warning("httpresponse was not able to send "
                        "a wav header for a flac");
                FLAC__file_decoder_finish (flac_decoder);
                FLAC__file_decoder_delete(flac_decoder);
                return;
            }
        }
        
        //
        //  OK ... HTTP header sent, first bit of payload (wav header) sent
        //  if client was seek'd to beginning, now we need to actually
        //  stream out the PCM samples. This efficitvely turns control over
        //  to the flacWrite() function (above).
        //
        
        FLAC__file_decoder_process_until_end_of_file(flac_decoder);        
        
        //
        //  Shut stuff down
        //

        FLAC__file_decoder_finish (flac_decoder);
        FLAC__file_decoder_delete(flac_decoder);

        return;
    }
#ifdef WMA_AUDIO_SUPPORT
    else if(file_to_send->name().section(".", -1, -1) == "wma")
    {

        //
        //  Decode and send a .wma (as a wav)
        //

        AVFormatContext     *format_context = NULL;
        AVInputFormat       *input_format = NULL;
        AVFormatParameters  *format_parameters = NULL;
    
        av_register_all();  
        
        //
        //  Open the input file
        //

        if (av_open_input_file(
                                &format_context, 
                                file_to_send->name().local8Bit(), 
                                input_format, 0, 
                                format_parameters
                              ) < 0)
        {
            warning(QString("httpresponse had problem opening \"%1\"")
                            .arg(file_to_send->name().local8Bit()));
            streamEmptyWav(which_client);
            return;
        }

        //
        //  Determine stream format and populate data in format_context
        //

        if (av_find_stream_info(format_context) < 0)
        {
            warning(QString("httpresponse could not find "
                            "stream info in \"%1\"")
                            .arg(file_to_send->name().local8Bit()));
            av_close_input_file(format_context);
            streamEmptyWav(which_client);
            return;
        } 
        
        //
        //  Before we can build the WAV header, we need to know how large
        //  this stream will be when fully decoded. I have no idea how to
        //  just ask libavformat for a size in bytes of what the decoded
        //  stream _will_ be, but I do know how to ask it exactly how many
        //  milliseconds long the content is. I also know that if I multiply
        //  the sample rate (ie. 44100) times the bits per sample (ie. 16)
        //  times the number of channels, I know how many bytes the file
        //  should end up being (with 44 extra for the wav header). 
        //


        //
        //  We figure out the file size and stuff up here, because if we
        //  take too long before at least sending our HTTP headers, iTunes
        //  craps out on us.
        //
        
        av_estimate_timings(format_context);
        double exact_time_in_micro_seconds = (format_context->duration);
        int64_t expected_file_size = 44 + ((long)
                                    (((44100 * 16 * 2)/8) * 
                                    (exact_time_in_micro_seconds 
                                    / (AV_TIME_BASE + 0.0))));

        //
        //  Handle range requests (seeking)
        //
        
        range_begin = stored_skip;
        range_end = expected_file_size - 1;
        
        if(range_end < 0)
        {
            range_end = 0;
        }
        if(range_begin > range_end)
        {
            range_begin = 0;
        }

        total_possible_range = expected_file_size;

        if(range_begin > 0 && range_begin < 44)
        {
            warning("client asked to seek in a wma "
                    "stream to somewhere inside the "
                    "header");
            range_begin = 44;
        }
        
        header_block.clear();
        createHeaderBlock(&header_block, ((int) (range_end - range_begin) + 1));
        if(!sendBlock(which_client, header_block))
        {
            warning("httpresponse was not able to send "
                    "an http header for a wma stream");
            av_close_input_file(format_context);
            return;
        }

        //
        //  Store what kind of decoder we need to use
        //
        
        AVCodecContext *audio_decoder_context = &format_context->streams[0]->codec;
        
        //
        //  Store the input format
        //
        
        input_format = format_context->iformat;

        //
        //  Get a PCM output format
        //
        
        AVOutputFormat *output_format = guess_format("wav", NULL, NULL);

        if(!output_format)
        {
            warning(QString("httpresponse could not get "
                            "an output format for \"%1\"")
                            .arg(file_to_send->name().local8Bit()));
            av_close_input_file(format_context);
            streamEmptyWav(which_client);
            return;
        }


        //
        //  Create and populate the output context
        //
        
        AVFormatContext *output_context = (AVFormatContext *)av_mallocz(sizeof(AVFormatContext));
        if(!output_context)
        {
            warning("httpresponse could not av_mallocz() "
                    "an output context (memory low?)");
            av_close_input_file(format_context);
            streamEmptyWav(which_client);
            return;
        }

        output_context->oformat = output_format;        
        
        AVStream *decoded_stream = av_new_stream(output_context, 0);
        decoded_stream->codec.codec_type = CODEC_TYPE_AUDIO;
        decoded_stream->codec.codec_id = output_context->oformat->audio_codec;
        decoded_stream->codec.sample_rate = audio_decoder_context->sample_rate;
        decoded_stream->codec.channels = audio_decoder_context->channels;
        decoded_stream->codec.bit_rate = audio_decoder_context->bit_rate;
    
        av_set_parameters(output_context, NULL);

        //
        //  Prepare the decoding codec
        //        

        AVCodec *decoding_codec = avcodec_find_decoder(audio_decoder_context->codec_id);
        if(!decoding_codec)
        {
            warning(QString("httpresponse could not get "
                            "a decoding codec for \"%1\"")
                            .arg(file_to_send->name().local8Bit()));
            av_free(output_context);
            av_close_input_file(format_context);
            streamEmptyWav(which_client);
            return;
        }

        //
        //  Open for decoding
        //

        if(avcodec_open(audio_decoder_context, decoding_codec) < 0)
        {
            warning(QString("httpresponse could not open "
                            "a decoding codec for \"%1\"")
                            .arg(file_to_send->name().local8Bit()));
            av_free(output_context);
            av_close_input_file(format_context);
            streamEmptyWav(which_client);
            return;
            
        }

        int total_bytes_sent = 0;

        if(range_begin == 0)
        {
            //
            //  Build the wav header, but only if the client hasn't already
            //  asked to seek to a point beyond it
            //
        
            unsigned char headbuf[44];

            int bits = 16;
            int channels = 2;
            int samplerate = 44100;
            int bytespersec = (channels * samplerate * bits )/8;
            int align = channels*bits/8;
            int samplesize = bits;
                   
            memcpy(headbuf, "RIFF", 4);
            WRITE_U32(headbuf+4, expected_file_size-8);
            memcpy(headbuf+8, "WAVE", 4);
            memcpy(headbuf+12, "fmt ", 4);
            WRITE_U32(headbuf+16, 16);
            WRITE_U16(headbuf+20, 1); // format
            WRITE_U16(headbuf+22, channels);
            WRITE_U32(headbuf+24, samplerate);
            WRITE_U32(headbuf+28, bytespersec);
            WRITE_U16(headbuf+32, align);
            WRITE_U16(headbuf+34, samplesize);
            memcpy(headbuf+36, "data", 4);
            WRITE_U32(headbuf+40, expected_file_size - 44);

            payload.clear();
            payload.insert(payload.begin(), headbuf, headbuf + 44);
            if(!sendBlock(which_client, payload))
            {
                warning("httpresponse was not able to send "
                        "a wav header for a wma");
                av_close_input_file(format_context);
                return;
            }
            total_bytes_sent += 44;
        }
        
        
        bool keep_decoding = true;
        
        av_read_play(format_context);

        //
        //  Seek to where we need to be
        //
        
        if(range_begin > 0)
        {
            int64_t starting_time_in_micro_seconds = (int64_t) 
                                        ((range_begin / (total_possible_range + 0.0)) 
                                        * exact_time_in_micro_seconds);
            if(av_seek_frame(format_context, 0, starting_time_in_micro_seconds, 
                AVSEEK_FLAG_BYTE) < 0)
            {
                warning("failed to seek in wma file, http headers "
                        "(already sent) have wrong Content-Length "
                        "... giving up (WMA Sucks!!!)");
                av_close_input_file(format_context);
                return;
            }
        }
        
        AVPacket *pkt;
        AVPacket pkt1;
        pkt = &pkt1;
        unsigned char *ptr;
        int len, data_size;
        char samples[AVCODEC_MAX_AUDIO_FRAME_SIZE];
        bool client_gone = false;
        while(keep_decoding)
        {
            if (av_read_frame(format_context, pkt) < 0)
            {
                //
                //  We're at the end
                //  

                keep_decoding = false;
            }

            // Get the pointer to the data and its length

            ptr = pkt->data;
            len = pkt->size;
                
            while (len > 0)  
            {
                int dec_len = avcodec_decode_audio(
                                                    audio_decoder_context, 
                                                    (short *)samples, 
                                                    &data_size, 
                                                    ptr, 
                                                    len
                                                  );    
                if (dec_len < 0) 
                {
                    //
                    //  No audio left in this pkt
                    //
                    break;
                }

                ptr += dec_len;
                len -= dec_len;

                //
                //  We want to send as many bytes as we waid we would in the
                //  http header. But because that was a guesstimate, we need
                //  to check it here. This may cut off a few microseconds of
                //  the track. If you don't like that, don't convert wma's
                //  to wav on the fly :-)
                //

                payload.clear();
                if(total_bytes_sent + data_size <= (range_end - range_begin) + 1)
                {
                    total_bytes_sent += data_size;
                    payload.insert(payload.end(), samples, samples + data_size);
                }
                else
                {
                    total_bytes_sent += ((range_end - range_begin) + 1) - total_bytes_sent;
                    payload.insert(payload.end(), samples, samples + (((range_end - range_begin) + 1) - total_bytes_sent));
                    keep_decoding = false; 
                }
                if(!sendBlock(which_client, payload))
                {
                    keep_decoding = false;
                    client_gone = true;
                    break;
                }

            }
            av_free_packet(pkt);
        }

        if(!client_gone)
        {
            if(total_bytes_sent < (range_end - range_begin) + 1)
            {
                //
                //  Pad up with 0 level sound if we have not quit sent as much as we said we would
                //
            
                char zero_pad[1];
                zero_pad[0] = 0;
                payload.clear();
                for(int i = 0; i < (((range_end - range_begin) + 1) - total_bytes_sent); i++)
                {
                    total_bytes_sent += 1;
                    payload.insert(payload.end(), zero_pad, zero_pad + 1);
                }
                sendBlock(which_client, payload);
            }
        }
        av_free(output_context);
        av_close_input_file(format_context);
    }
#endif
    else if(file_to_send->name().section(".", -1, -1) == "cda")
    {
        //
        //  Decode and send an cd audio track as a wave
        //
        
        QFileInfo file_info(file_to_send->name());
        QUrl device_url = QUrl(file_to_send->name());
        QString filename = file_info.fileName();

        int tracknum = atoi(filename.section('.', 0, 0).ascii());

        cdrom_drive *device = cdda_identify(device_url.dirPath(), 0, NULL);

        if(!device)
        {
            warning(QString("could not get a device at \"%1\"")
                            .arg(device_url.dirPath()));
            streamEmptyWav(which_client);
            return;
        }
        
        if(cdda_open(device))
        {
            cdda_close(device);
            warning(QString("could not open device \"%1\"")
                            .arg(device_url.dirPath()));
            streamEmptyWav(which_client);
            return;
        }

        cdda_verbose_set(device, CDDA_MESSAGE_FORGETIT, CDDA_MESSAGE_FORGETIT);
        long int start = cdda_track_firstsector(device, tracknum);
        long int end = cdda_track_lastsector(device, tracknum);

        if (start > end || end == start)
        {
            cdda_close(device);
            warning("cd track says end sector <= start sector");
            streamEmptyWav(which_client);
            return;
        }

        long total_numb_samples  = ((end - start + 1) * CD_FRAMESAMPLES);

        //
        //  If we multiple the total number of samples x 2 (channels) and by
        //  2 again (16 bits per sample), we get the total number of bytes
        //  the wav file would be.
        //
        
        int64_t final_file_size = 44 + (total_numb_samples * 2 * 2);


        //
        //  Do some calculations to deal with range requests. 
        //

        range_begin = stored_skip;
        range_end = final_file_size - 1;
        if(range_end < 0)
        {
            range_end = 0;
        }
        if(range_begin > range_end)
        {
            range_begin = 0;
        }

        total_possible_range = final_file_size;


        if(range_begin > 0 && range_begin < 44)
        {
            warning("client asked to seek in an ogg "
                    "stream to somewhere inside the "
                    "header");
            range_begin = 44;
        }
        
        //
        //  Now that we know the size, we can make and send the header block
        //
        
        header_block.clear();
        createHeaderBlock(&header_block, ((int) (range_end - range_begin) + 1));
        if(!sendBlock(which_client, header_block))
        {
            warning("httpresponse was not able to send "
                    "an http header for a cda stream");
            cdda_close(device);
            return;
        }

        int bits = 16;
        if(range_begin == 0)
        {
            //
            //  Build the wav header, but only if the client hasn't already
            //  asked to seek to a point beyond it
            //
        
            unsigned char headbuf[44];

            int channels = 2;
            int samplerate = 44100;
            int bytespersec = channels*samplerate*bits/8;
            int align = channels*bits/8;
            int samplesize = bits;
                   
            memcpy(headbuf, "RIFF", 4);
            WRITE_U32(headbuf+4, final_file_size-8);
            memcpy(headbuf+8, "WAVE", 4);
            memcpy(headbuf+12, "fmt ", 4);
            WRITE_U32(headbuf+16, 16);
            WRITE_U16(headbuf+20, 1); 
            WRITE_U16(headbuf+22, channels);
            WRITE_U32(headbuf+24, samplerate);
            WRITE_U32(headbuf+28, bytespersec);
            WRITE_U16(headbuf+32, align);
            WRITE_U16(headbuf+34, samplesize);
            memcpy(headbuf+36, "data", 4);
            WRITE_U32(headbuf+40, final_file_size - 44);

            payload.clear();
            payload.insert(payload.begin(), headbuf, headbuf + 44);
            if(!sendBlock(which_client, payload))
            {
                if(parent)
                {
                    parent->warning("httpresponse was not able to send "
                                    "a wav header for a cda");
                }
                cdda_close(device);
                return;
            }
        }
        
        //
        //  Initialize the paranoia stuff
        //

        cdrom_paranoia *paranoia = paranoia_init(device);
        paranoia_modeset(paranoia, PARANOIA_MODE_OVERLAP);

        paranoia_seek(paranoia, start, SEEK_SET);

        long int curpos = start;

        //
        //  Seek to where we need to be
        //

        if(range_begin >= 44)
        {
            double some_value = (((((range_begin - 44) / 4.0) / (CD_FRAMESAMPLES + 0.0)) - 1.0) + start);
            curpos = lrint(some_value);
        }
    
        paranoia_seek(paranoia, curpos, SEEK_SET);

        //
        //  Decode and stream it out
        //
        

        int16_t *cdbuffer;

        bool  keep_decoding = true;
        while(keep_decoding)
        {
            curpos++;
            if(curpos <= end)
            {
                cdbuffer = paranoia_read(paranoia, NULL);
                payload.clear();
                payload.insert(payload.begin(), (char *)cdbuffer, (char *)cdbuffer + CD_FRAMESIZE_RAW);
                if(!sendBlock(which_client, payload))
                {
                    //
                    //  Stop sending 
                    //
                
                    log("httpresponse failed to send block "
                        "while streaming cda file (client gone?)", 9);

                    keep_decoding = false;
                }
            }
            else
            {
                //
                //  we're done
                //

                keep_decoding = false;
            }

            if(parent)
            {
                //
                //  Check to see if our parent is trying to stop 
                //
            
                if(!parent->keepGoing())
                {
                    //
                    //  Oh crap, we're shutting down
                    //

                    log(QString("httpresponse aborted reading a file "
                                "to send because it thinks its time "
                                "to shut down"), 6);
                    keep_decoding = false;
                }
            }
        }

        paranoia_free(paranoia);
        cdda_close(device);        

    }
    else
    {
        if(parent)
        {
            parent->warning(QString("httpresponse does not know how to "
                                    "decode the following file: %1 ")
                                    .arg(file_to_send->name().ascii()));
        }

        streamEmptyWav(which_client);
        return;
    }
    
}

void HttpOutResponse::streamEmptyWav(MFDServiceClientSocket *which_client)
{

    //
    //  If we get an error while trying to set up a file for conversion to a
    //  wav, we call this function which just sends an empty (actually, a
    //  realitvely small) wav file. We do this because iTunes pukes if you try and
    //  send it a 404 or any other http error when it's expecting a wav
    //  file.
    //
    

    //
    //  This file_size needs to be big enough for the client to swallow
    //  (fill it's buffers enough that it starts trying to parse it).
    //

    static int file_size = 8192;


    std::vector<char> header_block;
    range_begin = -1;
    range_end = -1;
    total_possible_range = -1;
    header_block.clear();
    createHeaderBlock(&header_block, file_size);
    if(sendBlock(which_client, header_block))
    {
        char headbuf[file_size];

        memcpy(headbuf, "RIFF", 4);
        WRITE_U32(headbuf+4, file_size - 4);
        memcpy(headbuf+8, "WAVE", 4);
        memcpy(headbuf+12, "fmt ", 4);
        WRITE_U32(headbuf+16, 16);
        WRITE_U16(headbuf+20, 1);
        WRITE_U16(headbuf+22, 1);
        WRITE_U32(headbuf+24, 8000);
        WRITE_U32(headbuf+28, 16000);
        WRITE_U16(headbuf+32, 2);
        WRITE_U16(headbuf+34, 16);
        memcpy(headbuf+36, "data", 4);
        WRITE_U32(headbuf+40, file_size - 44);
            
        for(int i = 44; i < file_size - 1; i = i + 2)
        {
            WRITE_U16(headbuf + i, 0);
        }
            
        payload.clear();
        payload.insert(payload.begin(), headbuf, headbuf + file_size);
        if(!sendBlock(which_client, payload))
        {
            log("failed to sendBlock() while sending empty wav", 9);
        }
    }
    
}


void HttpOutResponse::log(const QString &log_text, int verbosity)
{
    if(parent)
    {
        parent->log(log_text, verbosity);
    }
}

void HttpOutResponse::warning(const QString &warn_text)
{
    if(parent)
    {
        parent->warning(warn_text);
    }
    else
    {
        cerr << warn_text << endl;
    }
}

HttpOutResponse::~HttpOutResponse()
{
    if(file_to_send)
    {
        delete file_to_send;
    }
    headers.clear();
    payload.clear();
}

