/*
	aacdecoder.cpp

	(c) 2003, 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	aac decoder methods
	
*/

#include <faad.h>

#ifdef __STDC_LIMIT_MACROS
#define FAAD_MODIFIED
#endif

#include <iostream>
#include <string>
#include <qobject.h>
#include <qiodevice.h>
#include <qfile.h>

#include "aacdecoder.h"
#include "constants.h"
#include <mythtv/audiooutput.h>
#include "metadata.h"
#include "metaiomp4.h"

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
    if (the_decoder_object)
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
                       AudioOutput *o) 
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
        if (user_stop || finish) 
        {
            inited = FALSE;
            done = TRUE;
        } 
        else 
        {
            ulong sz = output_bytes < bks ? output_bytes : bks;

	    int samples = (sz * 8) / (channels * 16);
	    if (output()->AddSamples(output_buf, samples, -1))
	      {
		output_bytes -= sz;
		memmove(output_buf, output_buf + sz, output_bytes);
		output_at = output_bytes;
	      } else {
		unlock();
		usleep(500);
		lock();
		done = user_stop;
	      }
	    
        }
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
    seekTime = -1.0;
    totalTime = 0.0;
    
    mp4_file_flag = false;

    if (! input())
    {
        error("aacDecoder: cannot initialize as it has no input");
        return false;
    }

   
    if (!output_buf) {
        output_buf = new char[globalBufferSize];
    }
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

    if (!input()->at(0))
    {
      error("couldn't seek in input");
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
    
    if (
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
        error("aacDecoder: stream is not mp4 ... not yet supported");
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
    if (!mp4_input_file)
    {
      error("could not open input as mp4 input file");
        faacDecClose(decoder_handle);
        free(mp4_callback);
        return false;
    }

    //
    //  Find the AAC track inside this mp4 container
    //

    if ( (aac_track_number = getAACTrack(mp4_input_file)) < 0)
    {
      error("could not find aac track inside mp4 input file");
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
    
    if (faacDecInit2(decoder_handle, buffer, buffer_size,
#ifdef FAAD_MODIFIED
                     (uint32_t*)
#endif
                     &sample_rate, &channels) < 0)
    {
        error("aacDecoder: error in second stage initialization");
        faacDecClose(decoder_handle);
        mp4ff_close(mp4_input_file);
        free(mp4_callback);
        if (buffer)
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

    // If max_bitrate == avg_bitrate, then file is fixed bitrate
    if (mp4ff_get_avg_bitrate(mp4_input_file, aac_track_number) ==
	mp4ff_get_max_bitrate(mp4_input_file, aac_track_number))
    {
	bitrate = mp4ff_get_avg_bitrate(mp4_input_file, aac_track_number) / 1000;
    }
    
    //
    //  Check we got same answers from mp4ASC
    //

    if (channels != mp4ASC.channelsConfiguration)
    {
      error("accDecoder: possible confusion on number of channels");
    }

    if (sample_rate != mp4ASC.samplingFrequency)
    {
      error("accDecoder: possible confusion on frequency");
    }


    //
    //  Setup the output
    //

    if (output())
    {
        output()->Reconfigure(16, channels, sample_rate,
                              false /* AC3/DTS pass through */);
        output()->SetSourceBitrate(bitrate);
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
    if (mp4_callback)
    {
        free(mp4_callback);
    }

    inited = user_stop = done = finish = FALSE;
    len = sample_rate = bitrate = 0;
    stat = channels = 0;
    timescale = 0;
    framesize = 0;
    setInput(0);
    setOutput(0);

}

void aacDecoder::run()
{

    lock();

    if (!inited) 
    {
      error("aacDecoder: run() called without being init'd");
        unlock();
        return;
    }

    //cerr << "aacDecoder: starting decoding of " << filename << endl;

    stat = DecoderEvent::Decoding;

    unlock();

    {
        DecoderEvent e((DecoderEvent::Type) stat);
        dispatch(e);
    }

    long current_sample, total_numb_samples;


    total_numb_samples = mp4ff_num_samples(mp4_input_file, aac_track_number);
    current_sample = -1;
    uchar *buffer;
    uint  buffer_size;

    while (!done && !finish && !user_stop) 
    {
        lock();

        ++current_sample;
        if (seekTime >= 0.0) 
        {
            //
            //  Crap ... seek ... well, this is approximately correct
            //        

            current_sample = (long int) (((double)(seekTime / totalTime)) * total_numb_samples);
            seekTime = -1.0;
        }
        
        if (current_sample >= total_numb_samples)
        {
            //
            //  We're done ... make sure we play all the remaining output
            //
            
            flush(TRUE);

            if (output())
            {
		output()->Drain();
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
                error("decoder error reading sample");
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
                
                if (((sample_count * 2) + output_at ) >= globalBufferSize)
                {
                    error("aacDecoder: gloablBufferSize too small, "
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
            
                if (sample_count > 0)
                {
                    output_at += sample_count * 2;
                    output_bytes += sample_count * 2;
                    if (output())
                    {
			// If source is VBR, bitrate == 0
			if (bitrate)
			{
			    output()->SetSourceBitrate(bitrate);
			} 
                        else 
                        {
			    output()->SetSourceBitrate(
				(int) ((float) (frame_info.bytesconsumed * 8) /
				       (frame_info.samples / 
				        frame_info.num_front_channels) 
				* frame_info.samplerate) / 1000);
			}

                        flush();
                    }
                }
            
                if (buffer)
                {
                    free(buffer);
                }
            }
        }
        unlock();
    }

    flush(TRUE);

    lock();

    //cerr << "aacDecoder: completed decoding of " << filename << endl;
    if (finish)
    {
      //cerr << "decoder finish" << endl;
        stat = DecoderEvent::Finished;
    }
    else if (user_stop) {
        stat = DecoderEvent::Stopped;
    }

    unlock();

    {
        DecoderEvent e((DecoderEvent::Type) stat);
        dispatch(e);
    }

    deinit();
}

MetaIO *aacDecoder::doCreateTagger(void)
{
    return new MetaIOMP4;
}

uint32_t aacDecoder::aacRead(char *buffer, uint32_t length)
{
    if (input())
    {
        Q_LONG read_result = input()->readBlock(buffer, length);
        if (read_result < 1)
        {
            return 0;
        }
        return read_result; 
    }
    error("aacDecoder: aacRead() was called, but there is no input");
    return 0;
}

uint32_t aacDecoder::aacSeek(uint64_t position)
{
    if (input())
    {
        return input()->at(position);
    }
    error("aacDecoder: aacSeek() was called, but there is no input");
    return 0;
}


bool aacDecoderFactory::supports(const QString &source) const
{
    bool res = false;
    QStringList list = QStringList::split("|", extension());

    for (QStringList::Iterator it = list.begin(); it != list.end(); ++it)
    {
        if (*it == source.right((*it).length()).lower())
        {
            res = true;
            break;
        }
    }
    return res;
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
                                  AudioOutput *output, bool deletable)
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
