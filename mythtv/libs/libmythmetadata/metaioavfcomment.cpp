
// libmythmetadata
#include "metaioavfcomment.h"
#include "musicmetadata.h"

// libmyth
#include <mythconfig.h>
#include <mythcontext.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

MetaIOAVFComment::MetaIOAVFComment(void)
    : MetaIO()
{
    QMutexLocker locker(avcodeclock);
    av_register_all();
}

MetaIOAVFComment::~MetaIOAVFComment(void)
{
}

/*!
 * \copydoc MetaIO::write()
 */
bool MetaIOAVFComment::write(const QString & /*filename*/, MusicMetadata* /*mdata*/)
{
    // Wont implement....
    return false;
}

/*!
* \copydoc MetaIO::read()
*/
MusicMetadata* MetaIOAVFComment::read(const QString &filename)
{
    QString artist, compilation_artist, album, title, genre;
    int year = 0, tracknum = 0, length = 0;

    AVFormatContext* p_context = NULL;
    AVInputFormat* p_inputformat = NULL;

    QByteArray local8bit = filename.toLocal8Bit();
    if ((avformat_open_input(&p_context, local8bit.constData(),
                             p_inputformat, NULL) < 0))
    {
        return NULL;
    }

    if (avformat_find_stream_info(p_context, NULL) < 0)
        return NULL;

    AVDictionaryEntry *tag = av_dict_get(p_context->metadata, "title", NULL, 0);
    if (!tag)
    {
        readFromFilename(filename, artist, album, title, genre, tracknum);
    }
    else
    {
        title = (char *)tag->value;

        tag = av_dict_get(p_context->metadata, "author", NULL, 0);
        if (tag)
            artist += (char *)tag->value;

        // compilation_artist???
        tag = av_dict_get(p_context->metadata, "album", NULL, 0);
        if (tag)
            album += (char *)tag->value;

        tag = av_dict_get(p_context->metadata, "genre", NULL, 0);
        if (tag)
            genre += (char *)tag->value;

        tag = av_dict_get(p_context->metadata, "year", NULL, 0);
        if (tag)
            year = atoi(tag->value);

        tag = av_dict_get(p_context->metadata, "tracknum", NULL, 0);
        if (tag)
            tracknum = atoi(tag->value);
    }

    length = getTrackLength(p_context);

    MusicMetadata *retdata = new MusicMetadata(filename, artist, compilation_artist, album, 
                                     title, genre, year, tracknum, length);

    retdata->determineIfCompilation();

    avformat_close_input(&p_context);

    return retdata;
}

/*!
 * \brief Find the length of the track (in seconds)
 *
 * \param filename The filename for which we want to find the length.
 * \returns An integer (signed!) to represent the length in seconds.
 */
int MetaIOAVFComment::getTrackLength(const QString &filename)
{
    AVFormatContext* p_context = NULL;
    AVInputFormat* p_inputformat = NULL;

    // Open the specified file and populate the metadata info
    QByteArray local8bit = filename.toLocal8Bit();
    if ((avformat_open_input(&p_context, local8bit.constData(),
                             p_inputformat, NULL) < 0))
    {
        return 0;
    }

    if (avformat_find_stream_info(p_context, NULL) < 0)
        return 0;

    int rv = getTrackLength(p_context);

    avformat_close_input(&p_context);

    return rv;
}

/*!
 * \brief Find the length of the track (in seconds)
 *
 * \param pContext The AV Format Context.
 * \returns An integer (signed!) to represent the length in seconds.
 */
int MetaIOAVFComment::getTrackLength(AVFormatContext* pContext)
{
    if (!pContext)
        return 0;

    return (pContext->duration / AV_TIME_BASE) * 1000;
}
