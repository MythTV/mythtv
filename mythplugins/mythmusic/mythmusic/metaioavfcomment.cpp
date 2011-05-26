
// Mythmusic
#include "metaioavfcomment.h"
#include "metadata.h"

// libmyth
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
bool MetaIOAVFComment::write(Metadata* mdata)
{
    // Wont implement....
    mdata = mdata; // -Wall annoyance
    return false;
}

/*!
* \copydoc MetaIO::read()
*/
Metadata* MetaIOAVFComment::read(QString filename)
{
    QString artist, compilation_artist, album, title, genre;
    int year = 0, tracknum = 0, length = 0;

    AVFormatContext* p_context = NULL;
    AVFormatParameters* p_params = NULL;
    AVInputFormat* p_inputformat = NULL;

    QByteArray local8bit = filename.toLocal8Bit();
    if ((av_open_input_file(&p_context, local8bit.constData(),
                           p_inputformat, 0, p_params) < 0))
    {
        return NULL;
    }

    if (av_find_stream_info(p_context) < 0)
        return NULL;

    AVMetadataTag *tag = av_metadata_get(p_context->metadata, "title", NULL, 0);
    if (!tag)
    {
        readFromFilename(filename, artist, album, title, genre, tracknum);
    }
    else
    {
	title = (char *)tag->value;

	tag = av_metadata_get(p_context->metadata, "author", NULL, 0);
        if (tag)
	    artist += (char *)tag->value;

        // compilation_artist???
	tag = av_metadata_get(p_context->metadata, "album", NULL, 0);
        if (tag)
            album += (char *)tag->value;

	tag = av_metadata_get(p_context->metadata, "genre", NULL, 0);
        if (tag)
            genre += (char *)tag->value;

	tag = av_metadata_get(p_context->metadata, "year", NULL, 0);
        if (tag)
            year = atoi(tag->value);

	tag = av_metadata_get(p_context->metadata, "tracknum", NULL, 0);
        if (tag)
            tracknum = atoi(tag->value);
    }

    length = getTrackLength(p_context);

    Metadata *retdata = new Metadata(filename, artist, compilation_artist, album,
                                     title, genre, year, tracknum, length);

    retdata->determineIfCompilation();

    av_close_input_file(p_context);

    return retdata;
}

/*!
 * \brief Find the length of the track (in seconds)
 *
 * \param filename The filename for which we want to find the length.
 * \returns An integer (signed!) to represent the length in seconds.
 */
int MetaIOAVFComment::getTrackLength(QString filename)
{
    AVFormatContext* p_context = NULL;
    AVFormatParameters* p_params = NULL;
    AVInputFormat* p_inputformat = NULL;

    // Open the specified file and populate the metadata info
    QByteArray local8bit = filename.toLocal8Bit();
    if ((av_open_input_file(&p_context, local8bit.constData(),
                           p_inputformat, 0, p_params) < 0))
    {
        return 0;
    }

    if (av_find_stream_info(p_context) < 0)
        return 0;

    int rv = getTrackLength(p_context);

    av_close_input_file(p_context);

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

    av_estimate_timings(pContext, 0);

    return (pContext->duration / AV_TIME_BASE) * 1000;
}
