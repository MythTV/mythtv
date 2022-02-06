
// Libav*
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

// libmythmetadata
#include "metaiomp4.h"
#include "musicmetadata.h"

// mythtv
#include "libmyth/mythcontext.h"
#include "libmythbase/mythlogging.h"

/*!
 * \copydoc MetaIO::write()
 */
bool MetaIOMP4::write(const QString &filename, MusicMetadata* mdata)
{
    if (!mdata)
        return false;

    if (filename.isEmpty())
        return false;

// Disabled because it doesn't actually work. Better implemented with Taglib
// when we formally move to 1.6

//     AVFormatContext* p_context = nullptr;
//     AVFormatParameters* p_params = nullptr;
//     AVInputFormat* p_inputformat = nullptr;
//
//     QByteArray local8bit = filename.toLocal8Bit();
//     if ((av_open_input_file(&p_context, local8bit.constData(),
//                             p_inputformat, 0, p_params) < 0))
//     {
//         return nullptr;
//     }
//
//     if (av_find_stream_info(p_context) < 0)
//         return nullptr;
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
    QString title;
    QString artist;
    QString album;
    QString genre;
    int year = 0;
    int tracknum = 0;
    std::chrono::milliseconds length = 0ms;
    bool compilation = false;

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

#if 0
    //### Debugging, enable to dump a list of all field names/values found

    AVDictionaryEntry *tag = av_dict_get(p_context->metadata, "\0", nullptr,
                                         AV_METADATA_IGNORE_SUFFIX);
    while (tag != nullptr)
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
        compilation = (getFieldValue(p_context, "").toInt() != 0);
        length = duration_cast<std::chrono::milliseconds>(av_duration(p_context->duration));
    }

    metadataSanityCheck(&artist, &album, &title, &genre);

    auto *retdata = new MusicMetadata(filename, artist,
                                      compilation ? artist : "",
                                      album, title, genre, year,
                                      tracknum, length);

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
    AVDictionaryEntry *tag = av_dict_get(context->metadata, tagname, nullptr, 0);

    QString value;

    if (tag)
        value = QString::fromUtf8(tag->value);

    return value;
}

/*!
 * \brief Find the length of the track (in milliseconds)
 *
 * \param filename The filename for which we want to find the length.
 * \returns An integer (signed!) to represent the length in milliseconds.
 */
std::chrono::milliseconds MetaIOMP4::getTrackLength(const QString &filename)
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

    std::chrono::milliseconds rv =
        duration_cast<std::chrono::milliseconds>(av_duration(p_context->duration));

    avformat_close_input(&p_context);

    return rv;
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
