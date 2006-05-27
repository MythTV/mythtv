/*
	wavdecoder.cpp

	(c) 2003-2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	wav decoder methods
	
	Very closely based on work by Brad Hughes
	Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>

*/

#include "../config.h"

#include <iostream>
using namespace std;

#include <mythtv/audiooutput.h>

#include "wavdecoder.h"
#include "constants.h"

#include "settings.h"


//
//  This decoder exists not because it makes sense to have files around in
//  wav format (though if you do, well ... you _can_ use it for local
//  files). The idea is that different mfd's may be built with different
//  audio capabilities, and if you want to be able to stream between them
//  the serving one can (as a last resort) convert the content to wav on the
//  fly so that the client one can play it even if the client one does not
//  have a decoder for the true content's actual file format.
//


WavDecoder::WavDecoder(const QString &file, DecoderFactory *d, 
                             QIODevice *i, AudioOutput *o) 
             : Decoder(d, i, o)
{

    filename = file;
    inited = false;
    user_stop = false;
    stat = 0;
    output_buf = 0;
    output_bytes = 0;
    output_at = 0;
    bks = 0;
    done = false;
    finish = false;
    len = 0;
    freq = 0;
    bitrate = 0;
    byte_rate = 0;
    block_align = 0;
    seekTime = -1.0;
    totalTime = 0.0;
    chan = 0;
    output_size = 0;
    bits_per_sample = 0;
    pcm_data_size = 0;
}

void WavDecoder::stop()
{
    user_stop = true;
}

void WavDecoder::flush(bool final)
{
    ulong min = final ? 0 : bks;
            
    while ((! done && ! finish) && output_bytes > min) {

        if (user_stop || finish) {
            inited = FALSE;
            done = TRUE;
        } else {
            ulong sz = output_bytes < bks ? output_bytes : bks;

            int samples = (sz*8)/(chan*16);
            if (output()->AddSamples(output_buf, samples, -1))
            {
                output_bytes -= sz;
                memmove(output_buf, output_buf + sz, output_bytes);
                output_at = output_bytes;
            } else {
                mutex()->unlock();
                usleep(500);
                mutex()->lock();
                done = user_stop;
            }
        }
    }
}

bool WavDecoder::initialize()
{

    bks = blockSize();

    inited = user_stop = done = finish = false;
    len = freq = bitrate = byte_rate = 0;
    block_align = bits_per_sample = pcm_data_size = 0;
    stat = chan = 0;
    output_size = 0;
    seekTime = -1.0;
    totalTime = 0.0;


    if (! input())
    {
        warning("WavDecoder: cannot initialize as it has no input");
        return false;
    }

    if (! output_buf)
    {
        output_buf = new char[globalBufferSize];
    }

    output_at = 0;
    output_bytes = 0;

    if (! input()->isOpen())
    {
        if (! input()->open(IO_ReadOnly))
        {
            warning("WavDecoder: failed to open input");
            return false;
        }
    }


    //
    //  Read the wav header (first 44 bytes) to determine channels, bitrate, etc.
    //

    if(!readWavHeader(input()))
    {
        warning("WavDecoder: problem reading wav header");
        return false;
    }

    if (output()) 
    {
        output()->Reconfigure(bits_per_sample, chan, freq, false);
        output()->SetSourceBitrate(bitrate);
    }

    inited = true;
    return true;

}

bool WavDecoder::readWavHeader(QIODevice *the_input)
{
    if(!the_input)
    {
        return false;
    }
    
    //
    //  Seek to start of file/stream
    //
    
    if(! the_input->at(0))
    {
        return false;
    }

    //
    //  Read 44 bytes (a wav header is always 44 bytes)
    //
    
    char header[44];
    int length = the_input->readBlock(header, 44);

    if(length != 44)
    {
        //
        //  If we can't read 44 bytes, we have problems
        //
        
        return false;
    }
    
    
    //
    //  Check that the first four bytes are "RIFF"
    //
    
    if( 
        header[0] != 'R' ||
        header[1] != 'I' ||
        header[2] != 'F' ||
        header[3] != 'F'
      )
    {
        warning("WavDecoder: did not get RIFF as first bytes of wav");
        return false;
    }
    
    //
    //  Check that it's a WAVE
    //

    if( 
        header[8]  != 'W' ||
        header[9]  != 'A' ||
        header[10] != 'V' ||
        header[11] != 'E'
      )
    {
        warning("WavDecoder: did not get WAVE as internal format");
        return false;
    }

    //
    //  Make sure the rest of this fmt subchunk is 16 bytes long
    //    

    uint fmt_subchunk_length = getU32(header[16], header[17], header[18], header[19]);
    
    if(fmt_subchunk_length != 16)
    {
        warning("WavDecoder: wav format subchunk is not 16 bytes long");
        return false;
    }
    
    //
    //  Make sure the audio format is PCM
    //
    
    uint audio_format = getU16(header[20], header[21]);

    if(audio_format != 1)
    {
        warning("WavDecoder: wav file does not contain PCM data");
        return false;
    }    
    
    //
    //  Get the number of channels
    //
    
    chan = getU16(header[22], header[23]);
    if(chan < 1)
    {
        warning("WavDecoder: wav header said there were 0 channels");
        return false;
    }
    if(chan != 2)
    {
        log(QString("WavDecoder: wav header said there were "
                    "%1 channel(s), which is a bit odd")
                    .arg(chan), 2);
    }
    
    //
    //  Get the frequency
    //

    freq = getU32(header[24], header[25], header[26], header[27]);
    if(freq < 1)
    {
        warning("WavDecoder: wav header said frequency is 0");
        return false;
    }
    if(freq != 44100)
    {
        log(QString("WavDecoder: wav header said frequency is "
                    "%1 which seems a bit odd")
                    .arg(freq), 2);
    }

    //
    //  Get the _Byte_ rate
    //

    byte_rate = getU32(header[28], header[29], header[30], header[31]);
    if(byte_rate < 1)
    {
        warning("WavDecoder: wav header said byte rate is 0");
        return false;
    }
    
    bitrate = byte_rate * 8;
    
    //
    //  Get the block align
    //
    
    block_align = getU16(header[32], header[33]);
    if(block_align < 1)
    {
        warning("WavDecoder: wav header said block align is 0");
        return false;
    }
    
    //
    //  Get the number of bits per sample
    //
    
    bits_per_sample = getU16(header[34], header[35]);
    if(bits_per_sample < 1)
    {
        warning("WavDecoder: wav header said there were 0 bits per sample");
        return false;
    }
    if(bits_per_sample != 16)
    {
        log(QString("WavDecoder: wav header said there bits per sample is " 
                    "%1, which may not work properly")
                    .arg(bits_per_sample), 2);
    }
    
    //
    //  Check that we have some data
    //

    if( 
        header[36] != 'd' ||
        header[37] != 'a' ||
        header[38] != 't' ||
        header[39] != 'a'
      )
    {
        warning("WavDecoder: did not get \"data\" as subchunk tag");
        return false;
    }

    pcm_data_size = getU32(header[40], header[41], header[42], header[43]);
    if(pcm_data_size < 1)
    {
        warning("WavDecoder: wav header said there was no PCM data");
        return false;
    }    

    //
    //  Ok, we're done reading the header ... a bit of logic checking and
    //  calculations
    //


    if(pcm_data_size + 44 != (long) the_input->size())
    {
        warning("WavDecoder: pcm data size and file size are mismatched");
        return false;
    }
    
    long number_of_samples = (8 * pcm_data_size ) / ( chan * bits_per_sample);

    totalTime = ((double) number_of_samples) / ((double) freq);
    return true;
}

uint WavDecoder::getU32(char byte_zero, char byte_one, char byte_two, char byte_three)
{
    uint return_value = 0;

    return_value += (int) ((uchar) byte_three) * (256 * 256 * 256);
    return_value += (int) ((uchar) byte_two)   * (256 * 256);
    return_value += (int) ((uchar) byte_one)   * (256);
    return_value += (int) ((uchar) byte_zero);
    
    return return_value;    
}

uint WavDecoder::getU16(char byte_zero, char byte_one)
{
    uint return_value = 0;

    return_value += (int) ((uchar) byte_one)   * (256);
    return_value += (int) ((uchar) byte_zero);
    
    return return_value;    
}

void WavDecoder::seek(double pos)
{

    seekTime = pos;
    if(seekTime < 0.0)
    {
        seekTime = 0.0;
    }
    if(seekTime > totalTime)
    {
        seekTime = totalTime - 0.5;
    }

}

void WavDecoder::deinit()
{
    inited = user_stop = done = finish = false;
    len = freq = bitrate = 0;
    stat = chan = 0;
    output_size = 0;
    setInput(0);
    setOutput(0);
}

void WavDecoder::run()
{

    mutex()->lock();

    if (! inited)
    {
        warning("WavDecoder run() without being init()'d");
        mutex()->unlock();
        return;
    }

    mutex()->unlock();

    while (! done && ! finish)
    {
        mutex()->lock();
     
        //
        // "decode" some pcm data (ie. just read it)

        if (seekTime >= 0.0)
        {
            //
            //  Seek to where we need to be
            //
            
            long go_to_where = ((long) ((seekTime / totalTime) * pcm_data_size)) + 44;
            
            //
            //  ha ha, that seems simple ... except we can end up on a
            //  sample boundary or get our channels switched .. so correct for block_align.
            //
            
            while(go_to_where % block_align != 0)
            {
                ++go_to_where;
            }
            
            if(!input()->at(go_to_where))
            {
                warning("WavDecoder: cannot seem to seek (?)");
            }
            seekTime = -1.0;
        }

        len = input()->readBlock((char *) (output_buf + output_at), bks);

        if (len > 0) 
        {
            output_at += len;
            output_bytes += len;
            if (output())
            {
                flush();
            }
        } 
        else if (len == 0)
        {
            flush(true);

            if (output()) 
            {
                output()->Drain();
            }

            done = true;
            if (! user_stop)
            {
                finish = true;
            }
        } 
        else 
        {
            //
            //  negative length for readBlock, something bad happened
            //

            warning("WavDecoder: error from read");
            message("decoder error");
            finish = true;
        }

        mutex()->unlock();
    }

    mutex()->lock();
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



AudioMetadata *WavDecoder::getMetadata()
{
    //
    //  There is no metadata in a wav file
    //
    
    return NULL;
}    

WavDecoder::~WavDecoder(void)
{
    if (inited)
        deinit();

    if (output_buf)
        delete [] output_buf;
    output_buf = 0;
}


bool WavDecoderFactory::supports(const QString &source) const
{
    return (source.right(extension().length()).lower() == extension());
}


const QString &WavDecoderFactory::extension() const
{
    static QString ext(".wav");
    return ext;
}


const QString &WavDecoderFactory::description() const
{
    static QString desc(QObject::tr("WAV Audio"));
    return desc;
}

Decoder *WavDecoderFactory::create(const QString &file, QIODevice *input, 
                                      AudioOutput *output, bool deletable)
{
    if (deletable)
        return new WavDecoder(file, this, input, output);

    static WavDecoder *decoder = 0;
    if (! decoder) {
        decoder = new WavDecoder(file, this, input, output);
    } else {
        decoder->setInput(input);
        decoder->setOutput(output);
    }

    return decoder;
}
   
