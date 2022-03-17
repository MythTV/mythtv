
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

// libmythmetadata
#include "metaioavfcomment.h"
#include "musicmetadata.h"

// libmyth
#include "libmyth/mythcontext.h"
#include "libmythbase/mythconfig.h"

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
    QString artist;
    QString compilation_artist;
    QString album;
    QString title;
    QString genre;
    int year = 0;
    int tracknum = 0;

    AVFormatContext* p_context = nullptr;
    AVInputFormat* p_inputformat = nullptr;

    QByteArray local8bit = filename.toLocal8Bit();
    if ((avformat_open_input(&p_context, local8bit.constData(),
                             p_inputformat, nullptr) < 0))
    {
        return nullptr;
    }

    if (avformat_find_stream_info(p_context, nullptr) < 0)
        return nullptr;

    AVDictionaryEntry *tag = av_dict_get(p_context->metadata, "title", nullptr, 0);
    if (!tag)
    {
        readFromFilename(filename, artist, album, title, genre, tracknum);
    }
    else
    {
        title = tag->value;

        tag = av_dict_get(p_context->metadata, "author", nullptr, 0);
        if (tag)
            artist += tag->value;

        // compilation_artist???
        tag = av_dict_get(p_context->metadata, "album", nullptr, 0);
        if (tag)
            album += tag->value;

        tag = av_dict_get(p_context->metadata, "genre", nullptr, 0);
        if (tag)
            genre += tag->value;

        tag = av_dict_get(p_context->metadata, "year", nullptr, 0);
        if (tag)
            year = atoi(tag->value);

        tag = av_dict_get(p_context->metadata, "tracknum", nullptr, 0);
        if (tag)
            tracknum = atoi(tag->value);
    }

    std::chrono::milliseconds length = getTrackLength(p_context);

    auto *retdata = new MusicMetadata(filename, artist, compilation_artist, album,
                                     title, genre, year, tracknum, length);

    retdata->determineIfCompilation();

    avformat_close_input(&p_context);

    return retdata;
}

/*!
 * \brief Find the length of the track (in milliseconds)
 *
 * \param filename The filename for which we want to find the length.
 * \returns An integer (signed!) to represent the length in milliseconds.
 */
std::chrono::milliseconds MetaIOAVFComment::getTrackLength(const QString &filename)
{
    AVFormatContext* p_context = nullptr;
    AVInputFormat* p_inputformat = nullptr;

    // Open the specified file and populate the metadata info
    QByteArray local8bit = filename.toLocal8Bit();
    if ((avformat_open_input(&p_context, local8bit.constData(),
                             p_inputformat, nullptr) < 0))
    {
        return 0ms;
    }

    if (avformat_find_stream_info(p_context, nullptr) < 0)
        return 0ms;

    std::chrono::milliseconds rv = getTrackLength(p_context);

    avformat_close_input(&p_context);

    return rv;
}

/*!
 * \brief Find the length of the track (in milliseconds)
 *
 * \param pContext The AV Format Context.
 * \returns An integer (signed!) to represent the length in milliseconds.
 */
std::chrono::milliseconds MetaIOAVFComment::getTrackLength(AVFormatContext* pContext)
{
    if (!pContext)
        return 0ms;

    auto time = av_duration(pContext->duration);
    return duration_cast<std::chrono::milliseconds>(time);
}
