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

#include <vorbis/vorbisfile.h>

#include "httpresponse.h"
#include "mfd_plugin.h"



//
//  Couple of WRITE_ macros that help us create WAV headers 
//

#define WRITE_U32(buf, x) *(buf)     = (unsigned char)((x)&0xff);\
                          *((buf)+1) = (unsigned char)(((x)>>8)&0xff);\
                          *((buf)+2) = (unsigned char)(((x)>>16)&0xff);\
                          *((buf)+3) = (unsigned char)(((x)>>24)&0xff);

#define WRITE_U16(buf, x) *(buf)     = (unsigned char)((x)&0xff);\
                          *((buf)+1) = (unsigned char)(((x)>>8)&0xff);



HttpResponse::HttpResponse(MFDHttpPlugin *owner, HttpRequest *requestor)
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

void HttpResponse::setError(int error_number)
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

void HttpResponse::addText(std::vector<char> *buffer, QString text_to_add)
{
    buffer->insert(buffer->end(), text_to_add.ascii(), text_to_add.ascii() + text_to_add.length()); 
}

void HttpResponse::createHeaderBlock(
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
                QString file_range_header = QString("Content-Range: %1-%2/%3\r\n")
                                            .arg(range_begin)
                                            .arg(range_end)
                                            .arg(total_possible_range);
                addText(header_block, file_range_header);
                if((range_end - range_begin) + 1 != payload_size)
                {
                    if(parent)
                    {
                        parent->warning(QString("httpresponse is sending a range from "
                                                "%1 to %2 (size of %3), but "
                                                "the payload size is set to %4")
                                                .arg(range_begin)
                                                .arg(range_end)
                                                .arg((range_end - range_begin) + 1)
                                                .arg(payload_size));
                    }
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
    
    
    /*
        Debuggin Output:

    my_request->printHeaders();
    cout << "&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&& " << endl;
    for(uint i = 0; i < header_block->size(); i++)
    {
        cout << header_block->at(i);
    }
    cout << "&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&& " << endl;

    */
}


void HttpResponse::send(MFDServiceClientSocket *which_client)
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
            
            if(parent)
            {
                parent->warning("httpresponse got an invalid conversion value");
            }
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
        if(parent)
        {
            if(all_is_well)
            {
                //
                //  Hmmm ... we're not in an error state, but something
                //  still tried to send an empty payload
                //

                parent->warning("httpresponse asked to sendBlock(), "
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

                parent->log("httpresponse aborted sending some socket data because it thinks its time to shut down", 6);

                return false;
            }
        }

        FD_ZERO(&writefds);
        FD_SET(which_client->socket(), &writefds);
        if(nfds <= which_client->socket())
        {
            nfds = which_client->socket() + 1;
        }
        
        timeout.tv_sec = 2;
        timeout.tv_usec = 0;
        int result = select(nfds, NULL, &writefds, NULL, &timeout);
        if(result < 0)
        {
            if(parent)
            {
                parent->warning("httpresponse got an error from "
                                "select() ... not sure what to do");
            }
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

void HttpResponse::sendFile(QString file_path, int skip, FileSendTransformation transform)
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
    if(!file_to_send->exists())
    {
        if(parent)
        {
            parent->warning(QString("httpresponse was asked to send "
                                    "a file that does not exist: %1")
                                    .arg(file_path.local8Bit()));
        }
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
        if(parent)
        {
            parent->warning(QString("httpresponse could not open (permissions?) a "
                                    "file it was asked to send: %1")
                                    .arg(file_path.local8Bit()));
        }
        setError(404);
        delete file_to_send;
        file_to_send = NULL;
        return;
    }
    
    //
    //  Leave the file open for the actual send() function.
    //    


}


void HttpResponse::streamFile(MFDServiceClientSocket *which_client)
{

    std::vector<char> header_block;

    //
    //  Seek to where we need to be
    //

    if(!file_to_send->at(range_begin))
    {
        if(parent)
        {
            parent->warning("httpresponse could not seek in a "
                            "file it was asked to send. This is bad.");
        }
            
        setError(404);
        createHeaderBlock(&header_block, 0);
        sendBlock(which_client, header_block);
        return;
    }
    
    createHeaderBlock(&header_block, (range_end - range_begin) + 1);
    if(!sendBlock(which_client, header_block))
    {
        if(parent)
        {
            parent->warning("httpresponse could not send header block to client");
        }

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
                
            if(parent)
            {
                parent->log("httpresponse failed to send block "
                            "while streaming ogg file (client gone?)", 9);
            }

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

                parent->log(QString("httpresponse aborted reading a file "
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
    HttpResponse *http_response = (HttpResponse *)client_data;
    return http_response->flacWrite(frame, buffer);
}


extern "C" void flacMetadataCallback(
                                    const FLAC__FileDecoder *decoder, 
                                    const FLAC__StreamMetadata *metadata, 
                                    void *client_data
                                    )
{
    decoder = decoder;
    HttpResponse *http_response = (HttpResponse *)client_data;
    http_response->flacMetadata(metadata);
}

extern "C" void flacErrorCallback(
                                    const FLAC__FileDecoder *decoder, 
                                    FLAC__StreamDecoderErrorStatus status, 
                                    void *client_data
                                 )
{
    decoder = decoder;
    HttpResponse *http_response = (HttpResponse *)client_data;
    http_response->flacError(status);
}


//
//  *THREE* corresponding object methods that do the real work of the
//  *callbacks above
//


FLAC__StreamDecoderWriteStatus HttpResponse::flacWrite(
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
            parent->warning("httpresponse interrupted \"flac as wav\" "
                            "streaming as it is time to shut down ");
            return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
        }
        
    }
    
    //
    //  make sure we have a client to send to
    //
    
    if(!flac_client)
    {
        if(parent)
        {
            parent->warning("httpresponse could send flac PCM data as "
                            "it has no pointer to a client");
            return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
        }
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
        if(parent)
        {
            parent->log("httpresponse failed to send block while streaming "
                        "flac file (client gone?)", 9);
        }
        delete [] out_buffer;
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }

    delete [] out_buffer;
    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void HttpResponse::flacMetadata(const FLAC__StreamMetadata *metadata)
{
    flac_bitspersample = metadata->data.stream_info.bits_per_sample;
    flac_channels      = metadata->data.stream_info.channels;
    flac_frequency     = metadata->data.stream_info.sample_rate;
    flac_totalsamples  = metadata->data.stream_info.total_samples;
}

void HttpResponse::flacError(FLAC__StreamDecoderErrorStatus status)
{
    status = status;
    
    if(parent)
    {
        parent->log("httpresponse got an error during flac "
                    "decoding, but will try to continue", 3);
    }
}



void HttpResponse::convertToWavAndStreamFile(MFDServiceClientSocket *which_client)
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
        FILE *input_file = fopen(file_to_send->name().ascii(), "rb");
        if(!input_file)
        {
            if(parent)
            {
                parent->warning(QString("while trying to convert ogg to wav "
                                        "and stream it, httpresponse could "
                                        "not open file: %1")
                                        .arg(file_to_send->name().ascii()));
            }
            streamEmptyWav(which_client);
            return;
        }
        
        if(ov_open(input_file, &ov_file, NULL, 0) < 0)
        {
            //
            //  Crap, this is not really an ogg file.
            //

            if(parent)
            {
                parent->warning(QString("httpresponse does not really think "
                                        "this file is an ogg: %1 ")
                                        .arg(file_to_send->name().ascii()));
            }
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
            if(parent)
            {
                parent->warning("client asked to seek in an ogg "
                                "stream to somewhere inside the "
                                "header");
            }
            range_begin = 44;
        }
        
        //
        //  Now that we know the size, we can make and send the header block
        //
        
        header_block.clear();
        createHeaderBlock(&header_block, ((int) (range_end - range_begin) + 1));
        if(!sendBlock(which_client, header_block))
        {
            if(parent)
            {
                parent->warning("httpresponse was not able to send "
                                "an http header for an ogg stream");
            }
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
                if(parent)
                {
                    parent->warning("httpresponse was not able to send "
                                    "a wav header for an ogg");
                }
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
                    if(parent)
                    {
                        parent->warning("httpresponse was not able to seek "
                                        "in an ogg file");
                    }
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
                
                if(parent)
                {
                    parent->log("httpresponse failed to send block "
                                "while streaming ogg file (client gone?)", 9);
                }

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

                    parent->log(QString("httpresponse aborted reading a file "
                                        "to send because it thinks its time "
                                        "to shut down"), 6);
                    len = 0;
                }
            }
            if(section != 0)
            {
                if(parent)
                {
                    parent->warning(QString("httpresponse encountered multiple "
                                            "sections in an ogg file and doesn't "
                                            "really know what it should do ..."));
                }
                len = 0;
            }
            if(len < 0)
            {
                if(parent)
                {
                    parent->warning(QString("httpresponse found a hole in an "
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
            if(parent)
            {
                parent->warning("httpresponse failed to turn of md5 checking on a flac file");
            }
            FLAC__file_decoder_delete(flac_decoder);
            streamEmptyWav(which_client);
            return;
        }        

        if(!FLAC__file_decoder_set_filename(flac_decoder, file_to_send->name().ascii()))
        {
            if(parent)
            {
               parent->warning("httpresponse failed to set a filename for a flac decode");
            }
            FLAC__file_decoder_delete(flac_decoder);
            streamEmptyWav(which_client);
            return;
        }

        if(!FLAC__file_decoder_set_write_callback(flac_decoder, flacWriteCallback))
        {
            if(parent)
            {
               parent->warning("httpresponse failed to set a flac write callback");
            }
            FLAC__file_decoder_delete(flac_decoder);
            streamEmptyWav(which_client);
            return;
        }        
        
        if(!FLAC__file_decoder_set_metadata_callback(flac_decoder, flacMetadataCallback))
        {
            if(parent)
            {
               parent->warning("httpresponse failed to set a flac metadata callback");
            }
            FLAC__file_decoder_delete(flac_decoder);
            streamEmptyWav(which_client);
            return;
        }

        if(!FLAC__file_decoder_set_error_callback(flac_decoder, flacErrorCallback))
        {
            if(parent)
            {
               parent->warning("httpresponse failed to set a flac error callback");
            }
            FLAC__file_decoder_delete(flac_decoder);
            streamEmptyWav(which_client);
            return;
        }

        if(!FLAC__file_decoder_set_client_data(flac_decoder, this))
        {
            if(parent)
            {
               parent->warning("httpresponse failed to set flac client data");
            }
            FLAC__file_decoder_delete(flac_decoder);
            streamEmptyWav(which_client);
            return;
        }

        if(FLAC__file_decoder_init(flac_decoder) != FLAC__FILE_DECODER_OK)
        {
            if(parent)
            {
               parent->warning("httpresponse failed to init flac decoder");
            }
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
            if(parent)
            {
               parent->warning("httpresponse failed to extract flac metadata");
            }
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
            if(parent)
            {
               parent->warning("httpresponse did not get sensible flac metadata");
            }
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
            if(parent)
            {
                parent->warning("client asked to seek in a flac "
                                "stream to somewhere inside the "
                                "header");
            }
            range_begin = 44;
        }
        
        //
        //  We now know how big the file will be. Send the HTTP header.
        //
        
        header_block.clear();
        createHeaderBlock(&header_block, ((int) (range_end - range_begin) + 1));
        if(!sendBlock(which_client, header_block))
        {
            if(parent)
            {
                parent->warning("httpresponse was not able to send "
                                "an http header for a flac stream");
            }
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
                if(parent)
                {
                    parent->warning("httpresponse could not seek in a flac "
                                    "file it was trying to stream");
                }
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
                if(parent)
                {
                    parent->warning("httpresponse was not able to send "
                                    "a wav header for a flac");
                }
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

void HttpResponse::streamEmptyWav(MFDServiceClientSocket *which_client)
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
        if(range_begin == 0)
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
            sendBlock(which_client, payload);
        }
    }
    
}


HttpResponse::~HttpResponse()
{
    headers.clear();
    payload.clear();
}

