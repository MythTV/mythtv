#include <math.h>

#include "metaiotaglib.h"
#include "metadata.h"

#include <mythtv/mythcontext.h>

MetaIOTagLib::MetaIOTagLib(void)
    : MetaIO(".mp3")
{
}

MetaIOTagLib::~MetaIOTagLib(void)
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
bool MetaIOTagLib::write(Metadata* mdata, bool exclusive)
{

    if (!mdata)
        return false;

    File *taglib = new TagLib::MPEG::File(mdata->Filename().local8Bit());

    Tag *tag = taglib->tag();

    if (!tag)
        return false;

    if (!mdata->Artist().isEmpty())
        tag->setArtist(QStringToTString(mdata->Artist()));

    if (mdata->Compilation())
    {

        UserTextIdentificationFrame *musicbrainz =
            new UserTextIdentificationFrame(TagLib::String::UTF8);
        taglib->ID3v2Tag()->addFrame(musicbrainz);
        musicbrainz->setDescription("MusicBrainz Album Artist Id");
        musicbrainz->setText(MYTH_MUSICBRAINZ_ALBUMARTIST_UUID);

        if (!mdata->CompilationArtist().isEmpty())
        {
            // Compilation Artist (TPE4)
            taglib->ID3v2Tag()->frameListMap()["TPE4"].front()->setText(
                QStringToTString(mdata->CompilationArtist()));
        }
    }

    if (!mdata->Title().isEmpty())
        tag->setTitle(QStringToTString(mdata->Title()));

    if (!mdata->Album().isEmpty())
        tag->setAlbum(QStringToTString(mdata->Album()));

    if (mdata->Year() > 999 && mdata->Year() < 10000) // 4 digit year.
        tag->setYear(mdata->Year());

    if (mdata->Rating() > 0 || mdata->PlayCount() > 0)
    {
        // Needs to be implemented for taglib by subclassing ID3v2::Frames
        // with one to handle POPM frames
    }

    if (!mdata->Genre().isEmpty())
        tag->setGenre(QStringToTString(mdata->Genre()));

    if (0 != mdata->Track())
        tag->setTrack(mdata->Track());

    bool result = taglib->save();

    return (result);
}


//==========================================================================
/*!
 * \brief Reads Metadata from a file.
 *
 * \param filename The filename to read metadata from.
 * \returns Metadata pointer or NULL on error
 */
Metadata* MetaIOTagLib::read(QString filename)
{
    QString artist = "", compilation_artist = "", album = "", title = "",
            genre = "";
    int year = 0, tracknum = 0, length = 0, playcount = 0, rating = 0, id = 0;
    bool compilation = false;

    QString extension = filename.section( '.', -1 ) ;

    File *taglib = new TagLib::MPEG::File(filename.local8Bit());

    Tag *tag = taglib->tag();

    if (!tag)
        return NULL;

    if (! tag->isEmpty())
    {

        title = TStringToQString(tag->title());
        artist = TStringToQString(tag->artist());
        album = TStringToQString(tag->album());

        // Compilation Artist (TPE4)
        if(!taglib->ID3v2Tag()->frameListMap()["TPE4"].isEmpty())
            compilation_artist = TStringToQString(
            taglib->ID3v2Tag()->frameListMap()["TPE4"].front()->toString());

        // Look for MusicBrainz Album+Artist ID in TXXX Frame
        UserTextIdentificationFrame *musicbrainz = find(taglib->ID3v2Tag(),
                "MusicBrainz Album Artist Id");

        if (musicbrainz)
        {
            // If the MusicBrainz ID is the special "Various Artists" ID
            // then compilation is TRUE
            compilation = (MYTH_MUSICBRAINZ_ALBUMARTIST_UUID
                == TStringToQString(musicbrainz->toString()));
        }

        // Get Track Num dealing with 1/16, 2/16 etc. format
        tracknum = tag->track();

        year = tag->year();

        // Genre.
        genre = TStringToQString(tag->genre());

        // Length
        if(!taglib->ID3v2Tag()->frameListMap()["TLEN"].isEmpty())
            length = taglib->ID3v2Tag()->frameListMap()["TLEN"].front()->toString().toInt();
    }

    // Fallback to filename reading
    if (title.isEmpty())
    {
        readFromFilename(filename, artist, album, title, genre, tracknum);
    }

    if (length <= 0)
        length = taglib->audioProperties()->length() * 1000;

    // If we don't have title and artist or don't have the length return NULL
    if ((title.isEmpty() && artist.isEmpty()) || length<=0)
    {
        VERBOSE(VB_IMPORTANT, QString("MetaIOTagLib: Failed to read metadata from '%1'")
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
int MetaIOTagLib::getTrackLength(QString filename)
{
    File *taglib = new TagLib::MPEG::File(filename.local8Bit());

    int seconds = taglib->audioProperties()->length();

    return seconds;
}

UserTextIdentificationFrame* MetaIOTagLib::find(TagLib::ID3v2::Tag *tag, const String &description)
{
  TagLib::ID3v2::FrameList l = tag->frameList("TXXX");
  for(TagLib::ID3v2::FrameList::Iterator it = l.begin(); it != l.end(); ++it) {
    UserTextIdentificationFrame *f = static_cast<UserTextIdentificationFrame *>(*it);
    if(f && f->description() == description)
      return f;
  }
  return 0;
}
