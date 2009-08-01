#include <math.h>

#include "metaiowavpack.h"
#include "metadata.h"

#include <apetag.h>
#include <apeitem.h>

#include <mythtv/mythcontext.h>

#undef QStringToTString
#define QStringToTString(s) TagLib::String(s.toUtf8().data(), TagLib::String::UTF8)
#undef TStringToQString
#define TStringToQString(s) QString::fromUtf8(s.toCString(true))

MetaIOWavPack::MetaIOWavPack(void)
    : MetaIO(".wv")
{
}

MetaIOWavPack::~MetaIOWavPack(void)
{
}

/*!
 * \brief Writes metadata back to a file
 *
 * \param mdata A pointer to a Metadata object
 * \param exclusive Flag to indicate if only the data in mdata should be
 *                  in the file. If false, any unrecognised tags already
 *                  in the file will be maintained.
 * \returns Boolean to indicate success/failure.
 */
bool MetaIOWavPack::write(Metadata* mdata, bool exclusive)
{
    (void) exclusive;

    if (!mdata)
        return false;

    QByteArray fname = mdata->Filename().toLocal8Bit();
    TagLib::WavPack::File *taglib = new TagLib::WavPack::File(fname.constData());

    Tag *tag = taglib->tag();

    if (!tag)
    {
        if (taglib)
            delete taglib;
        return false;
    }

    if (!mdata->Artist().isEmpty())
        tag->setArtist(QStringToTString(mdata->Artist()));

    // APE Only Tags
    if (taglib->APETag())
    {
        // Compilation Artist ("Album artist")
        if (mdata->Compilation())
        {
            TagLib::String key="Album artist";
            TagLib::APE::Item item=TagLib::APE::Item(key,
                QStringToTString(mdata->CompilationArtist()));
               taglib->APETag()->setItem(key, item);
        }
        else
            taglib->APETag()->removeItem("Album artist");
    }

    if (!mdata->Title().isEmpty())
        tag->setTitle(QStringToTString(mdata->Title()));

    if (!mdata->Album().isEmpty())
        tag->setAlbum(QStringToTString(mdata->Album()));

    if (mdata->Year() > 999 && mdata->Year() < 10000) // 4 digit year.
        tag->setYear(mdata->Year());

    if (!mdata->Genre().isEmpty())
        tag->setGenre(QStringToTString(mdata->Genre()));

    if (0 != mdata->Track())
        tag->setTrack(mdata->Track());

    bool result = taglib->save();

    if (taglib)
        delete taglib;

    return (result);
}


/*!
 * \brief Reads Metadata from a file.
 *
 * \param filename The filename to read metadata from.
 * \returns Metadata pointer or NULL on error
 */
Metadata* MetaIOWavPack::read(QString filename)
{
    QString artist, compilation_artist, album, title, genre;
    int year = 0, tracknum = 0, length = 0, playcount = 0, rating = 0, id = 0;
    bool compilation = false;
    QList<struct AlbumArtImage> albumart;

    QString extension = filename.section( '.', -1 ) ;

    QByteArray fname = filename.toLocal8Bit();
    TagLib::WavPack::File *taglib = new TagLib::WavPack::File(fname.constData());

    Tag *tag = taglib->tag();

    if (!tag)
    {
        if (taglib)
            delete taglib;
        return NULL;
    }

    // Basic Tags
    if (! tag->isEmpty())
    {
        title = TStringToQString(tag->title()).trimmed();
        artist = TStringToQString(tag->artist()).trimmed();
        album = TStringToQString(tag->album()).trimmed();
        tracknum = tag->track();
        year = tag->year();
        genre = TStringToQString(tag->genre()).trimmed();
    }

    // APE Only Tags
    if (taglib->APETag())
    {
        // Compilation Artist ("Album artist")
        if(taglib->APETag()->itemListMap().contains("Album artist"))
        {
            compilation = true;
            compilation_artist = TStringToQString(
            taglib->APETag()->itemListMap()["Album artist"].toString())
            .trimmed();
        }

    }

    // Fallback to filename reading
    if (title.isEmpty())
        readFromFilename(filename, artist, album, title, genre, tracknum);

    if (length <= 0 && taglib->audioProperties())
        length = taglib->audioProperties()->length() * 1000;

    if (taglib)
        delete taglib;

    // If we didn't get a valid length, add the metadata but show warning.
    if (length <= 0)
        VERBOSE(VB_GENERAL, QString("MetaIOWavPack: Failed to read length "
                "from '%1'. It may be corrupt.").arg(filename));

    // If we don't have title and artist or don't have the length return NULL
    if (title.isEmpty() && artist.isEmpty())
    {
        VERBOSE(VB_IMPORTANT,
                QString("MetaIOWavPack: Failed to read metadata from '%1'")
                .arg(filename));
        return NULL;
    }

    Metadata *retdata = new Metadata(filename, artist, compilation_artist, album,
                                     title, genre, year, tracknum, length,
                                     id, rating, playcount);

    retdata->setCompilation(compilation);

    return retdata;
}

/*!
 * \brief Find the length of the track (in seconds)
 *
 * \param filename The filename for which we want to find the length.
 * \returns An integer (signed!) to represent the length in seconds.
 */
int MetaIOWavPack::getTrackLength(QString filename)
{
    int seconds = 0;
    QByteArray fname = filename.toLocal8Bit();
    TagLib::WavPack::File *taglib = new TagLib::WavPack::File(fname.constData());

    if (taglib)
    {
        seconds = taglib->audioProperties()->length();
        delete taglib;
    }

    return seconds;
}
