
// libmythmetadata
#include "metaiomp4.h"
#include "musicmetadata.h"

// Libmyth
#include <mythlogging.h>
#include <mythcontext.h>

// Libav*
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

MetaIOMP4::MetaIOMP4(void)
    : MetaIO()
{
    QMutexLocker locker(avcodeclock);
    av_register_all();
}

MetaIOMP4::~MetaIOMP4(void)
{
}

/*!
 * \copydoc MetaIO::write()
 */
bool MetaIOMP4::write(MusicMetadata* mdata)
{
    if (!mdata)
        return false;

// Disabled because it doesn't actually work. Better implemented with Taglib
// when we formally move to 1.6

//     AVFormatContext* p_context = NULL;
//     AVFormatParameters* p_params = NULL;
//     AVInputFormat* p_inputformat = NULL;
//
//     QString filename = mdata->Filename();
//
//     QByteArray local8bit = filename.toLocal8Bit();
//     if ((av_open_input_file(&p_context, local8bit.constData(),
//                             p_inputformat, 0, p_params) < 0))
//     {
//         return NULL;
//     }
//
//     if (av_find_stream_info(p_context) < 0)
//         return NULL;
//
//     QByteArray artist = mdata->Artist().toUtf8();
//     QByteArray album  = mdata->Album().toUtf8();
//     QByteArray title  = mdata->Title().toUtf8();
//     QByteArray genre  = mdata->Genre().toUtf8();
//     QByteArray date   = QString::number(mdata->Year()).toUtf8();
//     QByteArray track  = QString::number(mdata->Track()).toUtf8();
//     QByteArray comp   = QString(mdata->Compilation() ? "1" : "0").toUtf8();
//
//     AVMetadata* avmetadata = p_context->metadata;
//
//     av_metadata_set(&avmetadata, "author", artist.constData());
//     av_metadata_set(&avmetadata, "album", album.constData());
//     av_metadata_set(&avmetadata, "title", title.constData());
//     av_metadata_set(&avmetadata, "genre", genre.constData());
//     av_metadata_set(&avmetadata, "year", date.constData());
//     av_metadata_set(&avmetadata, "track", track.constData());
//     av_metadata_set(&avmetadata, "compilation", comp.constData());
//
//     av_close_input_file(p_context);

    return true;
}

/*!
 * \copydoc MetaIO::read()
 */
MusicMetadata* MetaIOMP4::read(const QString &filename)
{
    QString title, artist, album, genre;
    int year = 0, tracknum = 0, length = 0;
    bool compilation = false;

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

#if 0
    //### Debugging, enable to dump a list of all field names/values found

    AVDictionaryEntry *tag = av_dict_get(p_context->metadata, "\0", NULL,
                                         AV_METADATA_IGNORE_SUFFIX);
    while (tag != NULL)
    {
        LOG(VB_GENERAL, LOG_DEBUG, QString("Tag: %1 Value: %2")
                .arg(tag->key) .arg(QString::fromUtf8(tag->value)));
        tag = av_dict_get(p_context->metadata, "\0", tag,
                          AV_METADATA_IGNORE_SUFFIX);
    }
    //####
#endif

    title = getFieldValue(p_context, "title");
    if (title.isEmpty())
    {
        readFromFilename(filename, artist, album, title, genre, tracknum);
    }
    else
    {
        title = getFieldValue(p_context, "title");
        artist = getFieldValue(p_context, "author");
        // Author is the correct fieldname, but
        // we've been saving to artist for years
        if (artist.isEmpty())
            artist = getFieldValue(p_context, "artist");
        album = getFieldValue(p_context, "album");
        year = getFieldValue(p_context, "year").toInt();
        genre = getFieldValue(p_context, "genre");
        tracknum = getFieldValue(p_context, "track").toInt();
        compilation = getFieldValue(p_context, "").toInt();
        length = getTrackLength(p_context);
    }

    metadataSanityCheck(&artist, &album, &title, &genre);

    MusicMetadata *retdata = new MusicMetadata(filename,
                                     artist,
                                     compilation ? artist : "",
                                     album,
                                     title,
                                     genre,
                                     year,
                                     tracknum,
                                     length);

    retdata->setCompilation(compilation);

    avformat_close_input(&p_context);

    return retdata;
}

/*!
 * \brief Retrieve the value of a named metadata field
 *
 * \param context AVFormatContext of the file
 * \param tagname The name of the field
 * \returns A string containing the requested value
 */
QString MetaIOMP4::getFieldValue(AVFormatContext* context, const char* tagname)
{
    AVDictionaryEntry *tag = av_dict_get(context->metadata, tagname, NULL, 0);

    QString value;

    if (tag)
        value = QString::fromUtf8(tag->value);

    return value;
}

/*!
 * \brief Find the length of the track (in seconds)
 *
 * \param filename The filename for which we want to find the length.
 * \returns An integer (signed!) to represent the length in seconds.
 */
int MetaIOMP4::getTrackLength(const QString &filename)
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
int MetaIOMP4::getTrackLength(AVFormatContext* pContext)
{
    if (!pContext)
        return 0;

    av_estimate_timings(pContext, 0);

    return (pContext->duration / AV_TIME_BASE) * 1000;
}

/*!
 * \brief Replace any empty strings in extracted metadata with sane defaults
 *
 * \param artist Artist
 * \param album Album
 * \param title Title
 * \param genre Genre
 */
void MetaIOMP4::metadataSanityCheck(QString *artist, QString *album,
                                    QString *title, QString *genre)
{
    if (artist->isEmpty())
        artist->append("Unknown Artist");

    if (album->isEmpty())
        album->append("Unknown Album");

    if (title->isEmpty())
        title->append("Unknown Title");

    if (genre->isEmpty())
        genre->append("Unknown Genre");
}
