#include "../config.h"

#ifdef AAC_AUDIO_SUPPORT
/*
	aacdecoder.cpp

	(c) 2003, 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	aac decoder methods
	

*/

#include <iostream>
#include <string>
#include <qobject.h>
#include <qiodevice.h>
#include <qfile.h>

#include <faad.h>

#include "aacdecoder.h"
#include "constants.h"
#include "buffer.h"
#include "output.h"
#include "recycler.h"
#include "metadata.h"

#include <mythtv/mythcontext.h>

//
//  C style callbacks (jump right back into the aacDecoder object)
//

extern "C" uint32_t read_callback(void *user_data, void *buffer, uint32_t length)
{
    aacDecoder *the_decoder_object = (aacDecoder*) user_data;
    if(the_decoder_object)
    {
        return the_decoder_object->aacRead((char *)buffer, length);
    }
    cerr << "read_callback called with no aacDecoder object assigned" << endl;
    return 0;
    
}
    
uint32_t seek_callback(void *user_data, uint64_t position)
{
    aacDecoder *the_decoder_object = (aacDecoder*) user_data;
    if(the_decoder_object)
    {
        return the_decoder_object->aacSeek(position);
    }
    cerr << "seek_callback called with no aacDecoder object assigned" << endl;
    return 0;
}        



/*
---------------------------------------------------------------------
*/



aacDecoder::aacDecoder(const QString &file, DecoderFactory *d, QIODevice *i, 
                       Output *o) 
          : Decoder(d, i, o)
{
    filename = file;
    inited = FALSE;
    user_stop = FALSE;
    stat = 0;
    bks = 0;
    done = FALSE;
    finish = FALSE;
    len = 0;
    sample_rate = 0;
    bitrate = 0;
    seekTime = -1.0;
    totalTime = 0.0;
    channels = 0;
    output_size = 0;
    output_buf = 0;
    output_bytes = 0;
    output_at = 0;
    
    mp4_file_flag = false;
    mp4_callback = NULL;
    timescale = 0;
    framesize = 0;
}

aacDecoder::~aacDecoder(void)
{
    if (inited)
    {
        deinit();
    }

    if (output_buf)
        delete [] output_buf;
    output_buf = 0;
}

void aacDecoder::stop()
{
    user_stop = TRUE;
}

void aacDecoder::flush(bool final)
{
    ulong min = final ? 0 : bks;

    while ((!done && !finish && seekTime <= 0) && output_bytes > min) 
    {
        output()->recycler()->mutex()->lock();

        while ((!done && !finish && seekTime <= 0) && 
               output()->recycler()->full()) 
        {
            mutex()->unlock();

            output()->recycler()->cond()->wait(output()->recycler()->mutex());

            mutex()->lock();
            done = user_stop;
        }

        if (user_stop || finish) 
        {
            inited = FALSE;
            done = TRUE;
        } 
        else 
        {
            ulong sz = output_bytes < bks ? output_bytes : bks;
            Buffer *b = output()->recycler()->get();

            memcpy(b->data, output_buf, sz);
            if (sz != bks) 
                memset(b->data + sz, 0, bks - sz);

            b->nbytes = bks;
            b->rate = bitrate;
            output_size += b->nbytes;
            output()->recycler()->add();

            output_bytes -= sz;
            memmove(output_buf, output_buf + sz, output_bytes);
            output_at = output_bytes;
        }

        if (output()->recycler()->full()) 
            output()->recycler()->cond()->wakeOne();

        output()->recycler()->mutex()->unlock();
    }
}

bool aacDecoder::initialize()
{
    bks = blockSize();

    inited = user_stop = done = finish = FALSE;
    len = sample_rate = bitrate = 0;
    stat = channels = 0;
    timescale = 0;
    framesize = 0;
    output_size = 0;
    seekTime = -1.0;
    totalTime = 0.0;
    
    mp4_file_flag = false;

    if (! input())
    {
        warning("aacDecoder: cannot initialize as it has no input");
        return false;
    }

   
    if (!output_buf)
        output_buf = new char[globalBufferSize];
    output_at = 0;
    output_bytes = 0;

    if (! input()->isOpen())
    {
        if (! input()->open(IO_ReadOnly))
        {
            error("aacDecoder: failed to open input");
            return FALSE;
        }
    }


    //
    //  See if we can seek
    //

    if(!input()->at(0))
    {
        warning("couldn't seek in input");
        return false;
    }
    

    //
    //  figure out if it's an mp4 file (aac in a mp4 wrapper a la Apple) or
    //  just a pure aac file.
    //
    
    mp4_file_flag = false;
    char header_buffer[8];
    input()->readBlock(header_buffer, 8);
    
    //
    //  Seek back to the begining, otherwise the decoder gets totally confused.
    //
    
    input()->at(0);
    
    if(
            header_buffer[4] == 'f' && 
            header_buffer[5] == 't' && 
            header_buffer[6] == 'y' && 
            header_buffer[7] == 'p'
      )
    {
        //
        //  It's an mp4/m4a (iTunes file) ... 
        //
        mp4_file_flag = true;
        return initializeMP4();
    }
    else
    {
        mp4_file_flag = false;
        warning("aacDecoder: stream is not mp4 ... not yet supported");
        input()->close();
        inited = false;
        return false;
    }
    
    input()->close();
    inited = false;
    return false;
}


bool aacDecoder::initializeMP4()
{
    //
    //  Open the callback structure for seeking and reading
    //

    mp4_callback = (mp4ff_callback_t*) malloc(sizeof(mp4ff_callback_t));
    mp4_callback->read = read_callback;
    mp4_callback->seek = seek_callback;
    mp4_callback->user_data = this;
    
    //
    //  Open decoder library (?)
    //
    
    decoder_handle = faacDecOpen();

    //
    //  Set configuration
    //
    
    faacDecConfigurationPtr config = faacDecGetCurrentConfiguration(decoder_handle);
    config->outputFormat = FAAD_FMT_16BIT;
    config->downMatrix = 0; // if 1, we could downmix 5.1 to 2 ... apparently
    config->dontUpSampleImplicitSBR = 1;
    faacDecSetConfiguration(decoder_handle, config);
                   
    //
    //  Open the mp4 input file  
    //                   

    mp4_input_file = mp4ff_open_read(mp4_callback);
    if(!mp4_input_file)
    {
        warning("could not open input as mp4 input file");
        faacDecClose(decoder_handle);
        free(mp4_callback);
        return false;
    }

    //
    //  Find the AAC track inside this mp4 container
    //

    if( (aac_track_number = getAACTrack(mp4_input_file)) < 0)
    {
        warning("could not find aac track inside mp4 input file");
        faacDecClose(decoder_handle);
        mp4ff_close(mp4_input_file);
        free(mp4_callback);
        return false;
    }

    //
    //  Get configuration in the right format and use it to do second level
    //  intialization of the faad lib
    //

    unsigned char *buffer = NULL;
    uint buffer_size;

    mp4ff_get_decoder_config(
                                mp4_input_file, 
                                aac_track_number, 
                                &buffer, 
                                &buffer_size
                            );    
    
    if(faacDecInit2(decoder_handle, buffer, buffer_size,
                    &sample_rate, &channels) < 0)
    {
        warning("aacDecoder: error in second stage initialization");
        faacDecClose(decoder_handle);
        mp4ff_close(mp4_input_file);
        free(mp4_callback);
        if(buffer)
        {
            free(buffer);
        }
        return 1;
    }

    timescale = mp4ff_time_scale(mp4_input_file, aac_track_number);
    framesize = 1024;
            
    //
    //  Fiddle with the default frame size if the data appears to need it
    // 

    mp4AudioSpecificConfig mp4ASC;
    if (buffer)
    {
        if (AudioSpecificConfig(buffer, buffer_size, &mp4ASC) >= 0)
        {
            if (mp4ASC.frameLengthFlag == 1) framesize = 960;
            if (mp4ASC.sbr_present_flag == 1) framesize *= 2;
        }
        free(buffer);
    }

    //
    //  Extract some information about the content
    //
    
    long samples = mp4ff_num_samples(mp4_input_file, aac_track_number);
    float f = 1024.0;
    float seconds;
    
    if (mp4ASC.sbr_present_flag == 1)
    {
        f = f * 2.0;
    }
    
    seconds = (float)samples*(float)(f-1.0)/(float)mp4ASC.samplingFrequency;

    totalTime = seconds;
    
    //
    //  Check we got same answers from mp4ASC
    //

    if(channels != mp4ASC.channelsConfiguration)
    {
        warning("accDecoder: possible confusion on number of channels");
    }

    if(sample_rate != mp4ASC.samplingFrequency)
    {
        warning("accDecoder: possible confusion on frequency");
    }


    //
    //  Setup the output
    //

    if (output())
    {

        output()->configure(sample_rate, 
                            channels, 
                            16, 
                            sample_rate * 
                            2 * 16);

    }

    inited = TRUE;
    return TRUE;
}

int aacDecoder::getAACTrack(mp4ff_t *infile)
{
    //
    //  Find an AAC track inside an mp4 container
    //

    int i, rc;
    int numTracks = mp4ff_total_tracks(infile);

    for (i = 0; i < numTracks; i++)
    {
        unsigned char *buff = NULL;
        uint buff_size = 0;
        mp4AudioSpecificConfig mp4ASC;

        mp4ff_get_decoder_config(infile, i, &buff, &buff_size);

        if (buff)
        {
            rc = AudioSpecificConfig(buff, buff_size, &mp4ASC);
            free(buff);

            if (rc < 0)
                continue;
            return i;
        }
    }

    //
    //  No AAC tracks
    // 
   
    return -1;
}

void aacDecoder::seek(double pos)
{
    seekTime = pos;
}

void aacDecoder::deinit()
{
    faacDecClose(decoder_handle);
    mp4ff_close(mp4_input_file);
    if(mp4_callback)
    {
        free(mp4_callback);
    }

    inited = user_stop = done = finish = FALSE;
    len = sample_rate = bitrate = 0;
    stat = channels = 0;
    timescale = 0;
    framesize = 0;
    output_size = 0;
    setInput(0);
    setOutput(0);

}

void aacDecoder::run()
{

    mutex()->lock();

    if (!inited) 
    {
        warning("aacDecoder: run() called without being init'd");
        mutex()->unlock();
        return;
    }

    log(QString("aacDecoder: starting decoding of %1")
                .arg(filename), 8);

    mutex()->unlock();

    long current_sample, total_numb_samples;


    total_numb_samples = mp4ff_num_samples(mp4_input_file, aac_track_number);
    current_sample = -1;
    uchar *buffer;
    uint  buffer_size;

    while (!done && !finish && !user_stop) 
    {
        mutex()->lock();


        ++current_sample;
        if (seekTime >= 0.0) 
        {
            //
            //  Crap ... seek ... well, this is approximately correct
            //        

            current_sample = (long int) (((double)(seekTime / totalTime)) * total_numb_samples);
            seekTime = -1.0;
        }
        
        if(current_sample >= total_numb_samples)
        {
            //
            //  We're done ... make sure we play all the remaining output
            //
            
            flush(TRUE);

            if (output())
            {
                output()->recycler()->mutex()->lock();
                while (! output()->recycler()->empty() && ! user_stop)
                {
                    output()->recycler()->cond()->wakeOne();
                    mutex()->unlock();
                    output()->recycler()->cond()->wait(
                                                output()->recycler()->mutex());
                    mutex()->lock();
                }
                output()->recycler()->mutex()->unlock();
            }

            done = TRUE;
            if (! user_stop)
            {
                finish = TRUE;
            }
        }
        else
        {

            unsigned int sample_count;

            buffer = NULL;
            buffer_size = 0;

            int rc = mp4ff_read_sample(
                                        mp4_input_file, 
                                        aac_track_number, 
                                        current_sample, 
                                        &buffer,  
                                        &buffer_size
                                      );
            if (rc == 0)
            {
                message("decoder error");
                done = TRUE;
            }
            else
            {
            
                faacDecFrameInfo frame_info;
                void *sample_buffer = faacDecDecode(
                                                    decoder_handle, 
                                                    &frame_info, 
                                                    buffer, 
                                                    buffer_size
                                                   );
            
                sample_count = frame_info.samples;

                //
                //  Munge the samples into the "right" format and send them
                //  to the output (after checking we're not going to exceed
                //  the output buffer size)
                //
                
                if(((sample_count * 2) + output_at ) >= globalBufferSize)
                {
                    warning("aacDecoder: gloablBufferSize too small, "
                            "truncating output (this is going to "
                            "sound like crap)");
                    sample_count = ((globalBufferSize - output_at) / 2) - 100;
                }
            
                char *char_buffer = (char *)sample_buffer;
                short *sample_buffer16 = (short*)char_buffer;
                for(uint i = 0; i < sample_count; i++)
                {
                    output_buf[output_at + (i*2)]     = (char)(sample_buffer16[i] & 0xFF);
                    output_buf[output_at + (i*2) + 1] = (char)((sample_buffer16[i] >> 8) & 0xFF);
                }
                
            
                if(sample_count > 0)
                {
                    output_at += sample_count * 2;
                    output_bytes += sample_count * 2;
                    if(output())
                    {
                        flush();
                    }
                }
            
                if(buffer)
                {
                    free(buffer);
                }
            }
        }
        mutex()->unlock();
    }

    flush(TRUE);

    mutex()->lock();
    log(QString("aacDecoder: completed decoding of %1")
                .arg(filename), 8);
    if(finish)
    {
        message("decoder finish");
    }
    else if(done)
    {
        message("decoder stop");
    }
    mutex()->unlock();

    deinit();
}


//
//  Extern C callbacks for metadata reading 
//


uint32_t md_read_callback(void *user_data, void *buffer, uint32_t length)
{
    return fread(buffer, 1, length, (FILE*)user_data);
}

uint32_t md_seek_callback(void *user_data, uint64_t position)
{
    return fseek((FILE*)user_data, position, SEEK_SET);
}


AudioMetadata* aacDecoder::getMetadata()
{
    QString artist = "", album = "", title = "", genre = "";
    QString writer = "", comment = "";
    int year = 0, tracknum = 0, length = 0;

    FILE *input = fopen(filename.ascii(), "r");
    if(!input)
    {
        warning(QString("could not open \"%1\" to read metadata")
                        .arg(filename.ascii()));
        return NULL;
    }

    //
    //  Create the callback structure
    //

    mp4ff_callback_t *mp4_cb = (mp4ff_callback_t*) malloc(sizeof(mp4ff_callback_t));
    mp4_cb->read = md_read_callback;
    mp4_cb->seek = md_seek_callback;
    mp4_cb->user_data = input;


    //
    //  Open the mp4 input file  
    //                   

    mp4ff_t *mp4_ifile = mp4ff_open_read(mp4_cb);
    if(!mp4_ifile)
    {
        warning(QString("could not open \"%1\" as mp4 for metadata")
                        .arg(filename.ascii()));
        free(mp4_cb);
        fclose(input);
        return NULL;
    }

    char *char_storage = NULL;

    //
    //  Look for metadata
    //

    if(mp4ff_meta_get_title(mp4_ifile, &char_storage))
    {
        title = QString::fromUtf8(char_storage);
        free(char_storage);
    }

    if(mp4ff_meta_get_artist(mp4_ifile, &char_storage))
    {
        artist = QString::fromUtf8(char_storage);
        free(char_storage);
    }

    if(mp4ff_meta_get_writer(mp4_ifile, &char_storage))
    {
        writer = QString::fromUtf8(char_storage);
        free(char_storage);
    }

    if(mp4ff_meta_get_album(mp4_ifile, &char_storage))
    {
        album = QString::fromUtf8(char_storage);
        free(char_storage);
    }

    if(mp4ff_meta_get_date(mp4_ifile, &char_storage))
    {
        year = QString(char_storage).toUInt();
        free(char_storage);
    }

    if(mp4ff_meta_get_comment(mp4_ifile, &char_storage))
    {
        comment = QString::fromUtf8(char_storage);
        free(char_storage);
    }
    
    if(mp4ff_meta_get_genre(mp4_ifile, &char_storage))
    {
        genre = QString::fromUtf8(char_storage);
        free(char_storage);
    }
    
    if(mp4ff_meta_get_track(mp4_ifile, &char_storage))
    {
        tracknum = QString(char_storage).toUInt();
        free(char_storage);
    }


    //
    //  Find the AAC track inside this mp4 which we need to do to find the
    //  length
    //

    int track_num;
    if( (track_num = getAACTrack(mp4_ifile)) < 0)
    {
        warning("could not find aac track to calculate metadata length");
        mp4ff_close(mp4_ifile);
        free(mp4_callback);
        fclose(input);
        return NULL;
    }

    unsigned char *buffer = NULL;
    uint buffer_size;

    mp4ff_get_decoder_config(
                                mp4_ifile, 
                                track_num, 
                                &buffer, 
                                &buffer_size
                            );    
    
    if(!buffer)
    {
        warning("could not get decoder config to calculate metadata length");
        mp4ff_close(mp4_ifile);
        free(mp4_callback);
        fclose(input);
        return NULL;
    }
   

    mp4AudioSpecificConfig mp4ASC;
    if (AudioSpecificConfig(buffer, buffer_size, &mp4ASC) < 0)
    {
        warning("could not get audio specifics to calculate metadata length");
        mp4ff_close(mp4_ifile);
        free(mp4_callback);
        fclose(input);
        return NULL;
    }
    
    long samples = mp4ff_num_samples(mp4_ifile, track_num);
    float f = 1024.0;

    if (mp4ASC.sbr_present_flag == 1)
    {
        f = f * 2.0;
    }
    
    float numb_seconds = (float)samples*(float)(f-1.0)/(float)mp4ASC.samplingFrequency;

    length = (int) (numb_seconds * 1000);

    mp4ff_close(mp4_ifile);
    free(mp4_cb);
    fclose(input);

    metadataSanityCheck(&artist, &album, &title, &genre);

    AudioMetadata *retdata = new AudioMetadata(
                                                filename, 
                                                artist, 
                                                album, 
                                                title, 
                                                genre,
                                                year, 
                                                tracknum, 
                                                length
                                              );
    retdata->setComposer(writer);
    retdata->setComment(comment);
    //retdata->setBitrate(mp4ASC.samplingFrequency);

    return retdata;
}    

uint32_t aacDecoder::aacRead(char *buffer, uint32_t length)
{
    if(input())
    {
        Q_LONG read_result = input()->readBlock(buffer, length);
        if(read_result < 1)
        {
            return 0;
        }
        return read_result; 
    }
    warning("aacDecoder: aacRead() was called, but there is no input");
    return 0;
}

uint32_t aacDecoder::aacSeek(uint64_t position)
{
    if(input())
    {
        return input()->at(position);
    }
    warning("aacDecoder: aacSeek() was called, but there is no input");
    return 0;
}

bool aacDecoderFactory::supports(const QString &source) const
{
    return (source.right(extension().length()).lower() == extension());
}

const QString &aacDecoderFactory::extension() const
{
    static QString ext(".m4a");
    return ext;
}

const QString &aacDecoderFactory::description() const
{
    static QString desc(QObject::tr("Windows Media Audio"));
    return desc;
}

Decoder *aacDecoderFactory::create(const QString &file, QIODevice *input, 
                                  Output *output, bool deletable)
{
    if (deletable)
        return new aacDecoder(file, this, input, output);

    static aacDecoder *decoder = 0;
    if (!decoder) 
    {
        decoder = new aacDecoder(file, this, input, output);
    } 
    else 
    {
        decoder->setInput(input);
        decoder->setOutput(output);
    }

    return decoder;
}

#endif
