#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <qobject.h>
#include <qiodevice.h>
using namespace std;

#include "vorbisdecoder.h"
#include "constants.h"
#include "buffer.h"
#include "output.h"
#include "recycler.h"
#include "metadata.h"

#include <mythtv/mythcontext.h>

// static functions for OggVorbis

static size_t oggread (void *buf, size_t size, size_t nmemb, void *src) {
    if (! src) return 0;

    VorbisDecoder *dogg = (VorbisDecoder *) src;
    int len = dogg->input()->readBlock((char *) buf, (size * nmemb));
    return len / size;
}

static int oggseek(void *src, int64_t offset, int whence) {
    VorbisDecoder *dogg = (VorbisDecoder *) src;

    if (! dogg->input()->isDirectAccess())
        return -1;

    long start = 0;
    switch (whence) {
    case SEEK_END:
        start = dogg->input()->size();
        break;

    case SEEK_CUR:
        start = dogg->input()->at();
        break;

    case SEEK_SET:
    default:
        start = 0;
    }

    if (dogg->input()->at(start + offset))
        return 0;
    return -1;
}

static int oggclose(void *src)
{
    VorbisDecoder *dogg = (VorbisDecoder *) src;
    dogg->input()->close();
    return 0;
}

static long oggtell(void *src)
{
    VorbisDecoder *dogg = (VorbisDecoder *) src;
    long t = dogg->input()->at();
    return t;
}

VorbisDecoder::VorbisDecoder(MythContext *context, const QString &file, 
                             DecoderFactory *d, QIODevice *i, Output *o) 
             : Decoder(context, d, i, o)
{
    filename = file;
    inited = FALSE;
    user_stop = FALSE;
    stat = 0;
    output_buf = 0;
    output_bytes = 0;
    output_at = 0;
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
}

VorbisDecoder::~VorbisDecoder(void)
{
    if (inited)
        deinit();

    if (output_buf)
        delete [] output_buf;
    output_buf = 0;
}

void VorbisDecoder::stop()
{
    user_stop = TRUE;
}

void VorbisDecoder::flush(bool final)
{
    ulong min = final ? 0 : bks;
            
    while ((! done && ! finish) && output_bytes > min) {
        output()->recycler()->mutex()->lock();
            
        while ((! done && ! finish) && output()->recycler()->full()) {
            mutex()->unlock();

            output()->recycler()->cond()->wait(output()->recycler()->mutex());

            mutex()->lock();
            done = user_stop;
        }

        if (user_stop || finish) {
            inited = FALSE;
            done = TRUE;
        } else {
            ulong sz = output_bytes < bks ? output_bytes : bks;
            Buffer *b = output()->recycler()->get();

            memcpy(b->data, output_buf, sz);
            if (sz != bks) memset(b->data + sz, 0, bks - sz);

            b->nbytes = bks;
            b->rate = bitrate;
            output_size += b->nbytes;
            output()->recycler()->add();

            output_bytes -= sz;
            memmove(output_buf, output_buf + sz, output_bytes);
            output_at = output_bytes;
        }

        if (output()->recycler()->full()) {
            output()->recycler()->cond()->wakeOne();
        }

        output()->recycler()->mutex()->unlock();
    }
}

bool VorbisDecoder::initialize()
{
    bks = blockSize();

    inited = user_stop = done = finish = FALSE;
    len = freq = bitrate = 0;
    stat = chan = 0;
    output_size = 0;
    seekTime = -1.0;
    totalTime = 0.0;

    if (! input()) {
        error("DecoderOgg: cannot initialize.  No input.");

        return FALSE;
    }

    if (! output_buf)
        output_buf = new char[globalBufferSize];
    output_at = 0;
    output_bytes = 0;

    if (! input()->isOpen()) {
        if (! input()->open(IO_ReadOnly)) {
            error("DecoderOgg: Failed to open input. Error " +
                  QString::number(input()->status()) + ".");
            return FALSE;
        }
    }

    ov_callbacks oggcb =
        {
            oggread,
            oggseek,
            oggclose,
            oggtell
        };
    if (ov_open_callbacks(this, &oggfile, NULL, 0, oggcb) < 0) {
        error("DecoderOgg: Cannot open stream.");

        return FALSE;
    }

    freq = 0;
    bitrate = ov_bitrate(&oggfile, -1) / 1000;
    chan = 0;

    totalTime = long(ov_time_total(&oggfile, 0));
    totalTime = totalTime < 0 ? 0 : totalTime;

    vorbis_info *ogginfo = ov_info(&oggfile, -1);
    if (ogginfo) {
        freq = ogginfo->rate;
        chan = ogginfo->channels;
    }

    if (output())
        output()->configure(freq, chan, 16, bitrate);

    inited = TRUE;
    return TRUE;
}

void VorbisDecoder::seek(double pos)
{
    seekTime = pos;
}

void VorbisDecoder::deinit()
{
    ov_clear(&oggfile);

    inited = user_stop = done = finish = FALSE;
    len = freq = bitrate = 0;
    stat = chan = 0;
    output_size = 0;
    setInput(0);
    setOutput(0);
}

void VorbisDecoder::run()
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

    int section = 0;

    while (! done && ! finish) {
        mutex()->lock();
        // decode

        if (seekTime >= 0.0) {
            ov_time_seek(&oggfile, double(seekTime));
            seekTime = -1.0;

            output_size = long(ov_time_tell(&oggfile)) * long(freq * chan * 2);
        }

        len = ov_read(&oggfile, (char *) (output_buf + output_at), bks, 0, 2, 1,
                      &section);

        if (len > 0) {
            bitrate = ov_bitrate_instant(&oggfile) / 1000;

            output_at += len;
            output_bytes += len;

            if (output())
                flush();
        } else if (len == 0) {
            flush(TRUE);

            if (output()) {
                output()->recycler()->mutex()->lock();
                while (! output()->recycler()->empty() && ! user_stop) {
                    output()->recycler()->cond()->wakeOne();
                    mutex()->unlock();
                    output()->recycler()->cond()->wait(
                                                output()->recycler()->mutex());
                    mutex()->lock();
                }
                output()->recycler()->mutex()->unlock();
            }

            done = TRUE;
            if (! user_stop) {
                finish = TRUE;
            }
        } else {
            // error in read
            error("DecoderOgg: Error while decoding stream, File appears to be "
                  "corrupted");

            finish = TRUE;
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

Metadata *VorbisDecoder::getMetadata(QSqlDatabase *db)
{
    Metadata *testdb = new Metadata(filename);
    if (testdb->isInDatabase(db))
        return testdb;

    delete testdb;

    QString artist = "", album = "", title = "", genre = "";
    int year = 0, tracknum = 0, length = 0;

    FILE *input = fopen(filename.ascii(), "r");

    if (!input)
        return NULL;

    OggVorbis_File vf;
    vorbis_comment *comment = NULL;

    if (ov_open(input, &vf, NULL, 0))
    {
        fclose(input);
        return NULL;
    }

    comment = ov_comment(&vf, -1);
    length = (int)ov_time_total(&vf, -1) * 1000;

    artist = getComment(comment, "artist");
    album = getComment(comment, "album");
    title = getComment(comment, "title");
    genre = getComment(comment, "genre");
    tracknum = atoi(getComment(comment, "tracknumber").ascii()); 
    year = atoi(getComment(comment, "date").ascii());

    ov_clear(&vf);

    Metadata *retdata = new Metadata(filename, artist, album, title, genre,
                                     year, tracknum, length);

    retdata->dumpToDatabase(db);

    return retdata;
}    

void VorbisDecoder::commitMetadata(Metadata *mdata)
{
}

QString VorbisDecoder::getComment(vorbis_comment *vc, const char *label)
{
    char *tag;
    QString retstr;


    if (vc && (tag = vorbis_comment_query(vc, (char *)label, 0)) != NULL)
        retstr = QString::fromUtf8(tag);
    else
        retstr = "";

    return retstr;
}

bool VorbisDecoderFactory::supports(const QString &source) const
{
    return (source.right(extension().length()).lower() == extension());
}


const QString &VorbisDecoderFactory::extension() const
{
    static QString ext(".ogg");
    return ext;
}


const QString &VorbisDecoderFactory::description() const
{
    static QString desc("Ogg Vorbis Audio");
    return desc;
}

Decoder *VorbisDecoderFactory::create(MythContext *context, const QString &file,
                                      QIODevice *input, Output *output,
                                      bool deletable)
{
    if (deletable)
        return new VorbisDecoder(context, file, this, input, output);

    static VorbisDecoder *decoder = 0;
    if (! decoder) {
        decoder = new VorbisDecoder(context, file, this, input, output);
    } else {
        decoder->setInput(input);
        decoder->setOutput(output);
    }

    return decoder;
}
   
