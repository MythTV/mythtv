#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <qobject.h>
#include <qiodevice.h>
#include <qfile.h>
using namespace std;

#include "cddecoder.h"
#include "constants.h"
#include <mythtv/audiooutput.h>
#include "metadata.h"

#include <mythtv/mythcontext.h>

CdDecoder::CdDecoder(const QString &file, DecoderFactory *d, QIODevice *i, 
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
    freq = 0;
    bitrate = 0;
    seekTime = -1.0;
    totalTime = 0.0;
    chan = 0;
    output_buf = 0;
    output_bytes = 0;
    output_at = 0;

    device = NULL;
    paranoia = NULL;

    settracknum = -1;

    devicename = gContext->GetSetting("CDDevice");
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

bool CdDecoder::initialize()
{
    bks = blockSize();

    inited = user_stop = done = finish = FALSE;
    len = freq = bitrate = 0;
    stat = chan = 0;
    seekTime = -1.0;
    totalTime = 0.0;

    filename = ((QFile *)input())->name();
    tracknum = atoi(filename.ascii());
   
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

    stat = DecoderEvent::Decoding;

    mutex()->unlock();

    {
        DecoderEvent e((DecoderEvent::Type) stat);
        dispatch(e);
    }

    int16_t *cdbuffer;

    while (! done && ! finish) {
        mutex()->lock();
        // decode

        if (seekTime >= 0.0) {
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
                flush();
        }
        else
        {
            flush(TRUE);

            if (output()) {
                output()->Drain();
            }

            done = TRUE;
            if (! user_stop) {
                finish = TRUE;
            }
        } 

        mutex()->unlock();
    }

    mutex()->lock();

    if (finish)
        stat = DecoderEvent::Finished;
    else if (user_stop)
        stat = DecoderEvent::Stopped;

    mutex()->unlock();

    {
        DecoderEvent e((DecoderEvent::Type) stat);
        dispatch(e);
    }

    deinit();
}

int CdDecoder::getNumTracks(void)
{
    int cd = cd_init_device((char *)devicename.ascii());

    struct disc_info discinfo;
    if (cd_stat(cd, &discinfo) != 0)
    {
        error("Couldn't stat CD, Error.");
        cd_finish(cd);
        return 0;
    }

    if (!discinfo.disc_present)
    {
        error("No disc present");
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
        error("Couldn't stat CD, Error.");
        cd_finish(cd);
        return 0;
    }

    if (!discinfo.disc_present)
    {
        error("No disc present");
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

Metadata* CdDecoder::getMetadata(int track)
{
    settracknum = track;
    return getMetadata();
}

Metadata *CdDecoder::getLastMetadata()
{
    Metadata *return_me;
    for(int i = getNumTracks(); i > 0; --i)
    {
        settracknum = i;
        return_me = getMetadata();
        if(return_me)
        {
            return return_me;
        }            
    }
    return NULL;
}

Metadata *CdDecoder::getMetadata()
{

    QString artist = "", album = "", compilation_artist = "", title = "", genre = "";
    int year = 0, tracknum = 0, length = 0;

    int cd = cd_init_device((char *)devicename.ascii());

    struct disc_info discinfo;
    if (cd_stat(cd, &discinfo) != 0)
    {
        error("Couldn't stat CD, Error.");
        cd_finish(cd);
        return NULL;
    }

    if (!discinfo.disc_present)
    {
        error("No disc present");
        cd_finish(cd);
        return NULL;
    }
 
    if (settracknum == -1)
        tracknum = atoi(filename.ascii());
    else
    {
        tracknum = settracknum;
        filename = QString("%1.cda").arg(tracknum);
    }

    settracknum = -1;

    if (tracknum > discinfo.disc_total_tracks)
    {
        error("No such track on CD");
        cd_finish(cd);
        return NULL;
    }

    if(discinfo.disc_track[tracknum - 1].track_type != CDAUDIO_TRACK_AUDIO )
    {
        error("Exclude non audio tracks");
        cd_finish(cd);
        return NULL;
    }


    struct disc_data discdata;
    memset(&discdata, 0, sizeof(discdata));

    int ret = cddb_read_disc_data(cd, &discdata);

    if (ret < 0)
    {
        cd_finish(cd);
        VERBOSE(VB_ALL, QString("Error during CD lookup: %1").arg(ret));
        return NULL;
    }

    compilation_artist = QString::fromUtf8(discdata.data_artist);

    if (compilation_artist.lower().left(7) == "various")
    {
        compilation_artist = QObject::tr("Various Artists");
    }


    album = QString::fromUtf8(discdata.data_title);
    genre = cddb_genre(discdata.data_genre);
 
    if (!genre.isEmpty()) 
    {
        QString flet = genre.upper().left(1);
        QString rt = genre.right(genre.length()-1).lower();
        genre = flet + rt;
    }

    title = QString::fromUtf8(discdata.data_track[tracknum - 1].track_name);        
    artist = QString::fromUtf8(discdata.data_track[tracknum - 1].track_artist);

    if (artist.length() < 1)
    {
      artist = compilation_artist;
      compilation_artist = "";
    }
    
    cddb_write_data(cd, &discdata);

    length = discinfo.disc_track[tracknum - 1].track_length.minutes * 60 +
             discinfo.disc_track[tracknum - 1].track_length.seconds;
    length = length < 0 ? 0 : length;
    length *= 1000;

    Metadata *retdata = new Metadata(filename, artist, compilation_artist,
                                     album, title, genre, year, tracknum, length);

    retdata->determineIfCompilation(true);

    cd_finish(cd);
    return retdata;
}    

void CdDecoder::commitMetadata(Metadata *mdata)
{
    int cd = cd_init_device((char *)devicename.ascii());

    struct disc_info discinfo;
    if (cd_stat(cd, &discinfo) != 0)
    {
        error("Couldn't stat CD, Error.");
        cd_finish(cd);
        return;
    }

    if (!discinfo.disc_present)
    {
        error("No disc present");
        cd_finish(cd);
        return;
    }

    tracknum = mdata->Track();

    if (tracknum > discinfo.disc_total_tracks)
    {
        error("No such track on CD");
        cd_finish(cd);
        return;
    }


    struct disc_data discdata;
    int ret = cddb_read_disc_data(cd, &discdata);

    if (ret < 0)
    {
        cd_finish(cd);
        VERBOSE(VB_ALL, QString("Error during CD lookup: %1").arg(ret));
        return;
    }
  
    if (mdata->Compilation())
    {
        if (mdata->CompilationArtist() != discdata.data_artist)
            strncpy(discdata.data_artist, mdata->CompilationArtist().utf8(), 256);
    }
    else
    {
        if (mdata->Artist() != discdata.data_artist)
            strncpy(discdata.data_artist, mdata->Artist().utf8(), 256);
    }
    if (mdata->Album() != discdata.data_title)
        strncpy(discdata.data_title, mdata->Album().utf8(), 256);
    if (mdata->Title() != discdata.data_track[tracknum - 1].track_name)
    {
        strncpy(discdata.data_track[tracknum - 1].track_name, mdata->Title().utf8(),
                256);

    }

    if (mdata->Compilation())
    {
        if (mdata->Artist() != discdata.data_track[tracknum - 1].track_artist)
        {
            strncpy(discdata.data_track[tracknum - 1].track_artist, 
                    mdata->Artist().utf8(), 256);
        }
    }
    else
    {
        if ("" != discdata.data_track[tracknum - 1].track_artist)
        {
            strncpy(discdata.data_track[tracknum - 1].track_artist, "", 256);
        }
    }
    
    cddb_write_data(cd, &discdata);

    cd_finish(cd);
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
        decoder->setFilename(file);
        decoder->setOutput(output);
    }

    return decoder;
}
   
