/*
    cddecoder.cpp

    (c) 2003-2005 Thor Sigvaldason and Isaac Richards
    Part of the mythTV project
    
    cd decoder
    
    Very closely based on work by Brad Hughes
    Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>

*/

#include "../config.h"

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <qobject.h>
#include <qiodevice.h>
#include <qfile.h>
#include <qfileinfo.h>
using namespace std;

#include <mythtv/audiooutput.h>

#include "cddecoder.h"
#include "constants.h"
#include "buffer.h"
//  #include "metadata.h"


#include "settings.h"

CdDecoder::CdDecoder(const QString &url_path, DecoderFactory *d, QIODevice *i, 
                     AudioOutput *o) 
         : Decoder(d, i, o)
{
    inited = FALSE;
    user_stop = FALSE;
    stat = 0;
    bks = 0;
    done = FALSE;
    finish = FALSE;
    len = 0;
    freq = 0;
    bitrate = 0;
    seekTime = -1.0;
    totalTime = 0.0;
    chan = 0;
    output_size = 0;
    output_buf = 0;
    output_bytes = 0;
    output_at = 0;

    device = NULL;
    paranoia = NULL;

    settracknum = -1;


    //
    //  Decompose the url path into a device name and a track number
    //

    QFileInfo file_info(url_path);
    filename = file_info.fileName();
    devicename = file_info.dirPath(true);
}

CdDecoder::~CdDecoder(void)
{
    if (inited)
        deinit();

    if (output_buf)
        delete [] output_buf;
    output_buf = 0;
}

void CdDecoder::stop()
{
    user_stop = TRUE;
}

void CdDecoder::flush(bool final)
{
    ulong min = final ? 0 : bks;

    while ((! done && ! finish) && output_bytes > min)
    {
        if (user_stop || finish) 
        {
            inited = FALSE;
            done = TRUE;
        } 
        else 
        {
            ulong sz = output_bytes < bks ? output_bytes : bks;

            int samples = (sz*8)/(chan*16);
            if (output()->AddSamples(output_buf, samples, -1))
            {
                output_bytes -= sz;
                memmove(output_buf, output_buf + sz, output_bytes);
                output_at = output_bytes;
            } 
            else 
            {
                mutex()->unlock();
                usleep(500);
                mutex()->lock();
                done = user_stop;
            }
        }
    }
}

bool CdDecoder::initialize()
{
    bks = blockSize();

    inited = user_stop = done = finish = FALSE;
    len = freq = bitrate = 0;
    stat = chan = 0;
    output_size = 0;
    seekTime = -1.0;
    totalTime = 0.0;

    //
    //  Set the device and track number
    //

    QFileInfo file_info(((QFile *)input())->name());
    filename = file_info.fileName();
    devicename = file_info.dirPath(true);
    tracknum = atoi(filename.section('.', 0, 0).ascii());
   
    if (!output_buf)
        output_buf = new char[globalBufferSize];
    output_at = 0;
    output_bytes = 0;

    device = cdda_identify(devicename.ascii(), 0, NULL);
    if (!device)
        return FALSE;

    if (cdda_open(device))
    {
        cdda_close(device);
        return FALSE;
    }

    cdda_verbose_set(device, CDDA_MESSAGE_FORGETIT, CDDA_MESSAGE_FORGETIT);
    start = cdda_track_firstsector(device, tracknum);
    end = cdda_track_lastsector(device, tracknum);
    
    if (start > end || end == start)
    {
        cdda_close(device);
        return FALSE;
    }
    
    paranoia = paranoia_init(device);
    paranoia_modeset(paranoia, PARANOIA_MODE_OVERLAP);
    paranoia_seek(paranoia, start, SEEK_SET);

    curpos = start;

    totalTime = ((end - start + 1) * CD_FRAMESAMPLES) / 44100.0;

    chan = 2;
    freq = 44100;

    if (output())
    {
        output()->Reconfigure(16, chan, freq);
        output()->SetSourceBitrate(44100 * 2 * 16);
    }

    inited = TRUE;
    return TRUE;
}

void CdDecoder::seek(double pos)
{
    seekTime = pos;
}

void CdDecoder::deinit()
{
    if (paranoia)
        paranoia_free(paranoia);
    if (device)
        cdda_close(device);

    device = NULL;
    paranoia = NULL;

    inited = user_stop = done = finish = FALSE;
    len = freq = bitrate = 0;
    stat = chan = 0;
    output_size = 0;
    setInput(0);
    setOutput(0);
}

static void paranoia_cb(long inpos, int function)
{       
    inpos = inpos; function = function;
}

void CdDecoder::run()
{
    mutex()->lock();

    if (! inited) {
        mutex()->unlock();

        return;
    }

    mutex()->unlock();

    int16_t *cdbuffer;

    while (! done && ! finish)
    {
        mutex()->lock();
        // decode

        if (seekTime >= 0.0)
        {
            curpos = (int)(((seekTime * 44100) / CD_FRAMESAMPLES) + start);
            paranoia_seek(paranoia, curpos, SEEK_SET);
            seekTime = -1.0;
        }

        curpos++;
        if (curpos <= end)
        {
            cdbuffer = paranoia_read(paranoia, paranoia_cb);

            memcpy((char *)(output_buf + output_at), (char *)cdbuffer, 
                   CD_FRAMESIZE_RAW);
            output_at += CD_FRAMESIZE_RAW;
            output_bytes += CD_FRAMESIZE_RAW;

            if (output())
            {
                flush();
            }
        }
        else
        {
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

int CdDecoder::getNumTracks(void)
{
    int cd = cd_init_device((char *)devicename.ascii());

    struct disc_info discinfo;
    if (cd_stat(cd, &discinfo) != 0)
    {
        warning("CdDecoder: couldn't stat CD");
        cd_finish(cd);
        return 0;
    }

    if (!discinfo.disc_present)
    {
        warning("CdDecoder: no disc present");
        cd_finish(cd);
        return 0;
    }

    int retval = discinfo.disc_total_tracks;

    cd_finish(cd);

    return retval;
}

int CdDecoder::getNumCDAudioTracks(void)
{
    int cd = cd_init_device((char *)devicename.ascii());

    struct disc_info discinfo;
    if (cd_stat(cd, &discinfo) != 0)
    {
        warning("CdDecoder: couldn't stat CD");
        cd_finish(cd);
        return 0;
    }

    if (!discinfo.disc_present)
    {
        warning("no disc present");
        cd_finish(cd);
        return 0;
    }

    int retval = 0;
    for( int i = 0 ; i < discinfo.disc_total_tracks; ++i)
    {
        if(discinfo.disc_track[i].track_type == CDAUDIO_TRACK_AUDIO)
        {
            ++retval;
        }
    }

    cd_finish(cd);

    return retval;
}

AudioMetadata* CdDecoder::getMetadata()
{
    warning("can't return metadata for a whole CD, just a track");
    return NULL;
}

AudioMetadata* CdDecoder::getMetadata(int track)
{

    QString artist = "", album = "", title = "", genre = "";
    int year = 0, length = 0, size = 0;
    
    tracknum = track;

    int cd = cd_init_device((char *)devicename.ascii());

    struct disc_info discinfo;
    if (cd_stat(cd, &discinfo) != 0)
    {
        warning("couldn't stat CD");
        cd_finish(cd);
        return NULL;
    }

    if (!discinfo.disc_present)
    {
        warning("no disc present");
        cd_finish(cd);
        return NULL;
    }
 
    if (tracknum > discinfo.disc_total_tracks)
    {
        warning("no such track on CD");
        cd_finish(cd);
        return NULL;
    }

    if(discinfo.disc_track[tracknum - 1].track_type != CDAUDIO_TRACK_AUDIO )
    {
        warning("exclude non audio tracks");
        cd_finish(cd);
        return NULL;
    }


    struct disc_data discdata;
    memset(&discdata, 0, sizeof(discdata));

    int ret = cddb_read_disc_data(cd, &discdata);

    if (ret < 0)
    {
        cd_finish(cd);
        warning("cddecoder.o: bad lookup :(");
        return NULL;
    }

    artist = QString::fromUtf8(discdata.data_artist);
    album = QString::fromUtf8(discdata.data_title);
    genre = cddb_genre(discdata.data_genre);
 
    if (!genre.isEmpty()) 
    {
        QString flet = genre.upper().left(1);
        QString rt = genre.right(genre.length()-1).lower();
        genre = flet + rt;
    }

    QString temptitle = QString::fromUtf8(discdata.data_track[tracknum - 1].track_name);        
    QString trackartist = QString::fromUtf8(discdata.data_track[tracknum - 1].track_artist);

    if (trackartist.length() > 0)
    {
        title = trackartist + " / " + temptitle;
    }
    else
    {
        title = temptitle;
    }

    cddb_write_data(cd, &discdata);

    length = discinfo.disc_track[tracknum - 1].track_length.minutes * 60 +
             discinfo.disc_track[tracknum - 1].track_length.seconds;
    length = length < 0 ? 0 : length;
    size = length * 176400;
    length *= 1000;

    if (artist.lower().left(7) == "various")
    {
        artist = QObject::tr("Various Artists");
    }

    //
    //  Munge the filename into a URL that will let us find this track again
    //
    
    QString url = QString("cd://localhost%1/%2.cda").arg(devicename).arg(tracknum);

    metadataSanityCheck(&artist, &album, &title, &genre);

    AudioMetadata *retdata = new AudioMetadata(
                                                url, 
                                                artist, 
                                                album, 
                                                title, 
                                                genre,
                                                year, 
                                                tracknum, 
                                                length
                                              );

    //
    //  Set some other information we can glean about the track
    //

    retdata->setTrackCount(discinfo.disc_total_tracks);
    if(discdata.data_artist_type == CDINDEX_MULTIPLE_ARTIST)
    {
        retdata->setCompilation(true);
    }
    retdata->setComment("Served to you by Myth");
    retdata->setSampleRate(44100);
    retdata->setBitrate((176400 * 8) / 1024);
    retdata->setSize(size);
    retdata->setFormat("cda");

    cd_finish(cd);
    return retdata;
}    


bool CdDecoderFactory::supports(const QString &source) const
{
    return (source.right(extension().length()).lower() == extension());
}

const QString &CdDecoderFactory::extension() const
{
    static QString ext(".cda");
    return ext;
}


const QString &CdDecoderFactory::description() const
{
    static QString desc(QObject::tr("Ogg Vorbis Audio"));
    return desc;
}

Decoder *CdDecoderFactory::create(const QString &file, QIODevice *input, 
                                  AudioOutput *output, bool deletable)
{
    if (deletable)
        return new CdDecoder(file, this, input, output);

    static CdDecoder *decoder = 0;
    if (! decoder) {
        decoder = new CdDecoder(file, this, input, output);
    } else {
        decoder->setInput(input);
        decoder->setOutput(output);
    }

    return decoder;
}
   
