#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <qobject.h>
#include <qiodevice.h>
using namespace std;

#include "cddecoder.h"
#include "constants.h"
#include "buffer.h"
#include "output.h"
#include "recycler.h"
#include "metadata.h"
#include "settings.h"

extern Settings *settings;

CdDecoder::CdDecoder(const QString &file, DecoderFactory *d, 
                     QIODevice *i, Output *o) 
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
    output_size = 0;
    settracknum = -1;

    devicename = (char *)(settings->GetSetting("CDDevice").c_str());
}

CdDecoder::~CdDecoder(void)
{
    if (inited)
        deinit();
}

void CdDecoder::stop()
{
    user_stop = TRUE;
}

bool CdDecoder::initialize()
{
    inited = user_stop = done = finish = FALSE;
    len = freq = bitrate = 0;
    stat = chan = 0;
    output_size = 0;
    seekTime = -1.0;
    totalTime = 0.0;

    tracknum = atoi(filename.ascii());
    
    cd_desc = cd_init_device((char *)devicename.ascii());

    freq = 0;
    chan = 0;

    struct disc_info discinfo;
    if (cd_stat(cd_desc, &discinfo) != 0)
    {
        error("Couldn't stat CD, Error.");
        cd_finish(cd_desc);
        return false;
    }

    if (!discinfo.disc_present)
    {
        error("No disc present");
        cd_finish(cd_desc);
        return false;
    }

    if (tracknum > discinfo.disc_total_tracks)
    {
        error("No such track on CD");
        cd_finish(cd_desc);
        return false;
    }

    totalTime = discinfo.disc_track[tracknum - 1].track_length.minutes * 60 +  
                discinfo.disc_track[tracknum - 1].track_length.seconds;
    totalTime = totalTime < 0 ? 0 : totalTime;

    inited = TRUE;
    return TRUE;
}

void CdDecoder::seek(double pos)
{
    seekTime = pos;
}

void CdDecoder::deinit()
{
    cd_stop(cd_desc);
    cd_finish(cd_desc);

    inited = user_stop = done = finish = FALSE;
    len = freq = bitrate = 0;
    stat = chan = 0;
    output_size = 0;
    setInput(0);
    setOutput(0);
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

    cd_play_pos(cd_desc, tracknum, 0);

    stat = OutputEvent::Playing;
    OutputEvent e((OutputEvent::Type)stat);
    dispatch(e);

    while (! done && ! finish) {
        mutex()->lock();
        // decode

        if (seekTime >= 0.0) {
            cd_play_pos(cd_desc, tracknum, (int)seekTime);

            seekTime = -1.0;
        }

        if (user_stop || finish)
        {
            inited = FALSE;
            done = TRUE;
        }

        struct disc_info discinfo;
        cd_stat(cd_desc, &discinfo);
        if (discinfo.disc_mode == CDAUDIO_COMPLETED ||
            discinfo.disc_mode == CDAUDIO_NOSTATUS)
        {
            if (!user_stop)
                finish = TRUE;
        }

        int tracktime = discinfo.disc_track_time.minutes * 60 +
                        discinfo.disc_track_time.seconds;

        OutputEvent e(tracktime, 0, 176400, 44100, 0, 2);
        dispatch(e);
       
        mutex()->unlock();

        usleep(50000);
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

Metadata *CdDecoder::getMetadata(QSqlDatabase *db, int track)
{
    settracknum = track;
    return getMetadata(db);
}

Metadata *CdDecoder::getMetadata(QSqlDatabase *db)
{
    db = db;

    QString artist = "", album = "", title = "", genre = "";
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


    struct disc_data discdata;
    memset(&discdata, 0, sizeof(discdata));

    int ret = cddb_read_disc_data(cd, &discdata);

    if (ret < 0)
    {
        cout << "bad lookup :(\n";
        return NULL;
    }

    artist = discdata.data_artist;
    album = discdata.data_title;
    QString temptitle = discdata.data_track[tracknum - 1].track_name;        
    QString trackartist = discdata.data_track[tracknum - 1].track_artist;

    if (trackartist.length() > 0)
        title = trackartist + " / " + temptitle;
    else
        title = temptitle;

    cddb_write_data(cd, &discdata);

    length = discinfo.disc_track[tracknum - 1].track_length.minutes * 60 +
             discinfo.disc_track[tracknum - 1].track_length.seconds;
    length = length < 0 ? 0 : length;
    length *= 1000;

    if (artist.lower().left(7) == "various")
    {
        artist = "Various Artists";
    }

    Metadata *retdata = new Metadata(filename, artist, album, title, genre,
                                     year, tracknum, length);

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
        cout << "bad lookup :(\n";
        return;
    }

   
    if (mdata->Artist() != discdata.data_artist)
        strncpy(discdata.data_artist, mdata->Artist().ascii(), 256);
    if (mdata->Album() != discdata.data_title)
        strncpy(discdata.data_title, mdata->Album().ascii(), 256);
    if (mdata->Title() != discdata.data_track[tracknum - 1].track_name)
    {
        strncpy(discdata.data_track[tracknum - 1].track_name, mdata->Title(),
                256);
        strncpy(discdata.data_track[tracknum - 1].track_artist, "", 256);
    }

    cddb_write_data(cd, &discdata);
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
    static QString desc("Ogg Vorbis Audio");
    return desc;
}

Decoder *CdDecoderFactory::create(const QString &file,
                                  QIODevice *input,
                                  Output *output,
                                  bool deletable)
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
   
