// C
#include <cstdio>
#include <cstdlib>

// C++
#include <iostream>
#include <string>
using namespace std;

// Qt
#include <QIODevice>
#include <QObject>
#include <QFile>

// MythTV
#include <audiooutput.h>
#include <mythcontext.h>
#include <mythmediamonitor.h>
#include <libavcodec/avcodec.h>

// MythMusic
#include "cddecoder.h"
#include "constants.h"
#include "metadata.h"

CdDecoder::CdDecoder(const QString &file, DecoderFactory *d, QIODevice *i,
                     AudioOutput *o) :
    Decoder(d, i, o),
    inited(false),   user_stop(false),
    devicename(""),
#if CONFIG_DARWIN
    m_diskID(0),     m_firstTrack(0),
    m_lastTrack(0),  m_leadout(0),
    m_lengthInSecs(0.0)
#endif
    stat(0),         output_buf(NULL),
    output_bytes(0), output_at(0),
    bks(0),          done(false),
    finish(false),   len(0),
    freq(0),         bitrate(0),
    chan(0),         output_size(0),
    totalTime(0.0),  seekTime(-1.0),
    settracknum(-1), tracknum(0),
#if defined(__linux__) || defined(__FreeBSD__)
    device(NULL),    paranoia(NULL),
#endif
    start(0),        end(0),
    curpos(0)
{
    setFilename(file);
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
            // Never buffer more than 5000ms of audio since this slows down
            // actions such as seeking or track changes made after decoding is
            // complete but audio remains in the buffer
            bool ok = (output()->GetAudioBufferedTime() <= 5000);
            if (ok) ok = output()->AddSamples(output_buf, samples, -1);
            if (ok)
            {
                output_bytes -= sz;
                memmove(output_buf, output_buf + sz, output_bytes);
                output_at = output_bytes;
            }
            else
            {
                unlock();
                usleep(5000);
                lock();
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

    filename = ((QFile *)input())->fileName();
    tracknum = filename.section('.', 0, 0).toUInt();

    if (!output_buf)
        output_buf = new char[globalBufferSize];
    output_at = 0;
    output_bytes = 0;

    QByteArray devname = devicename.toAscii();
    device = cdda_identify(devname.constData(), 0, NULL);
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
        const AudioSettings settings(
            16 /*bits*/, chan, CODEC_ID_PCM_S16LE, freq,
            false /* AC3/DTS passthru */);
        output()->Reconfigure(settings);
        output()->SetSourceBitrate(44100 * 2 * 16);
    }

    setCDSpeed(2);
    inited = TRUE;
    return TRUE;
}

void CdDecoder::seek(double pos)
{
    seekTime = pos;
}

void CdDecoder::deinit()
{
    setCDSpeed(-1);
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
    lock();

    if (! inited) {
        unlock();

        return;
    }

    stat = DecoderEvent::Decoding;

    unlock();

    {
        DecoderEvent e((DecoderEvent::Type) stat);
        dispatch(e);
    }

    int16_t *cdbuffer;

    while (! done && ! finish) {
        lock();
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

        unlock();
    }

    lock();

    if (finish)
        stat = DecoderEvent::Finished;
    else if (user_stop)
        stat = DecoderEvent::Stopped;

    unlock();

    {
        DecoderEvent e((DecoderEvent::Type) stat);
        dispatch(e);
    }

    deinit();
}

void CdDecoder::setCDSpeed(int speed)
{
    QMutexLocker lock(getMutex());
    MediaMonitor::SetCDSpeed(devicename.toLocal8Bit().constData(), speed);
}

int CdDecoder::getNumTracks(void)
{
    QByteArray devname = devicename.toAscii();
    int cd = cd_init_device(const_cast<char*>(devname.constData()));

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
    QByteArray devname = devicename.toAscii();
    int cd = cd_init_device(const_cast<char*>(devname.constData()));

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

    QString artist, album, compilation_artist, title, genre;
    int year = 0, tracknum = 0, length = 0;

    QByteArray devname = devicename.toAscii();
    int cd = cd_init_device(const_cast<char*>(devname.constData()));

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
        tracknum = filename.toUInt();
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
        VERBOSE(VB_IMPORTANT, QString("Error during CD lookup: %1").arg(ret));
        VERBOSE(VB_MEDIA, QString("cddb_read_disc_data() said: %1")
                .arg(cddb_message));
        return NULL;
    }

    compilation_artist = QString(M_QSTRING_UNICODE(discdata.data_artist))
                                                                    .trimmed();

    if (compilation_artist.toLower().left(7) == "various")
        compilation_artist = QObject::tr("Various Artists");

    album = QString(M_QSTRING_UNICODE(discdata.data_title)).trimmed();
    genre = cddb_genre(discdata.data_genre);

    if (!genre.isEmpty())
    {
        QString flet = genre.toUpper().left(1);
        QString rt = genre.right(genre.length()-1).toLower();
        genre = QString("%1%2").arg(flet).arg(rt).trimmed();
    }

    title  = QString(M_QSTRING_UNICODE(discdata.data_track[tracknum - 1].track_name))
                                                                    .trimmed();
    artist = QString(M_QSTRING_UNICODE(discdata.data_track[tracknum - 1].track_artist))
                                                                    .trimmed();

    if (artist.length() < 1)
    {
      artist = compilation_artist;
      compilation_artist.clear();
    }

    if (title.length() < 1)
        title = QObject::tr("Track %1").arg(tracknum);

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

static void set_cstring(char *dest, const char *src, size_t dest_buf_size)
{
    if (dest_buf_size < 1)
        return;
    strncpy(dest, src, dest_buf_size - 1);
    dest[dest_buf_size - 1] = 0;
}

void CdDecoder::commitMetadata(Metadata *mdata)
{
    QByteArray devname = devicename.toAscii();
    int cd = cd_init_device(const_cast<char*>(devname.constData()));

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
        VERBOSE(VB_IMPORTANT, QString("Error during CD lookup: %1").arg(ret));
        return;
    }

    if (mdata->Compilation())
    {
        if (mdata->CompilationArtist() != discdata.data_artist)
        {
            set_cstring(discdata.data_artist,
                        mdata->CompilationArtist().toUtf8().constData(), 256);
        }
    }
    else
    {
        if (mdata->Artist() != discdata.data_artist)
        {
            set_cstring(discdata.data_artist,
                        mdata->Artist().toUtf8().constData(), 256);
        }
    }
    if (mdata->Album() != discdata.data_title)
    {
        set_cstring(discdata.data_title,
                    mdata->Album().toUtf8().constData(), 256);
    }
    if (mdata->Title() != discdata.data_track[tracknum - 1].track_name)
    {
        set_cstring(discdata.data_track[tracknum - 1].track_name,
                    mdata->Title().toUtf8().constData(), 256);
    }

    if (mdata->Compilation())
    {
        if (mdata->Artist() != discdata.data_track[tracknum - 1].track_artist)
        {
            set_cstring(discdata.data_track[tracknum - 1].track_artist,
                        mdata->Artist().toUtf8().constData(), 256);
        }
    }
    else
    {
        discdata.data_track[tracknum - 1].track_artist[0] = 0;
    }

    cddb_write_data(cd, &discdata);

    cd_finish(cd);
}

bool CdDecoderFactory::supports(const QString &source) const
{
    return (source.right(extension().length()).toLower() == extension());
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
