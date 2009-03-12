#include <cmath>

#include "metaiotaglib.h"
#include "metadata.h"

#include <mythverbose.h>

#undef QStringToTString
#define QStringToTString(s) TagLib::String(s.toUtf8().data(), TagLib::String::UTF8)
#undef TStringToQString
#define TStringToQString(s) QString::fromUtf8(s.toCString(true))


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
    (void) exclusive;

    if (!mdata)
        return false;

    QByteArray fname = mdata->Filename().toLocal8Bit();
    File *taglib = new TagLib::MPEG::File(fname.constData());

    Tag *tag = taglib->tag();

    if (!tag)
    {
        if (taglib)
            delete taglib;
        return false;
    }

    if (!mdata->Artist().isEmpty())
        tag->setArtist(QStringToTString(mdata->Artist()));

    // MusicBrainz ID
    UserTextIdentificationFrame *musicbrainz = NULL;
    musicbrainz = find(taglib->ID3v2Tag(), "MusicBrainz Album Artist Id");

    // Compilation Artist (TPE4)
    TextIdentificationFrame *tpeframe = NULL;
    TagLib::ID3v2::FrameList tpelist = taglib->ID3v2Tag()->frameListMap()["TPE4"];
    if (!tpelist.isEmpty())
        tpeframe = (TextIdentificationFrame *)tpelist.front();

    if (mdata->Compilation())
    {

        if (!musicbrainz)
        {
            musicbrainz = new UserTextIdentificationFrame(TagLib::String::UTF8);
            taglib->ID3v2Tag()->addFrame(musicbrainz);
            musicbrainz->setDescription("MusicBrainz Album Artist Id");
        }

        musicbrainz->setText(MYTH_MUSICBRAINZ_ALBUMARTIST_UUID);

        if (!mdata->CompilationArtist().isEmpty())
        {
            if (!tpeframe) {
                tpeframe = new TextIdentificationFrame(TagLib::ByteVector("TPE4"), TagLib::String::UTF8);
                taglib->ID3v2Tag()->addFrame(tpeframe);
            }

            tpeframe->setText(QStringToTString(mdata->CompilationArtist()));
        }
    }
    else
    {
        if (tpeframe)
            taglib->ID3v2Tag()->removeFrame(tpeframe);
        if (musicbrainz)
            taglib->ID3v2Tag()->removeFrame(musicbrainz);
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
Metadata* MetaIOTagLib::read(QString filename)
{
    QString artist = "", compilation_artist = "", album = "", title = "",
            genre = "";
    int year = 0, tracknum = 0, length = 0, playcount = 0, rating = 0, id = 0;
    bool compilation = false;
    QList<struct AlbumArtImage> albumart;

    QString extension = filename.section( '.', -1 ) ;

    QByteArray fname = filename.toLocal8Bit();
    File *taglib = new TagLib::MPEG::File(fname.constData());

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

    // ID3V2 Only Tags
    if (taglib->ID3v2Tag())
    {
        // Compilation Artist (TPE4)
        if(!taglib->ID3v2Tag()->frameListMap()["TPE4"].isEmpty())
        {
            compilation_artist = TStringToQString(
            taglib->ID3v2Tag()->frameListMap()["TPE4"].front()->toString())
                .trimmed();
        }

        // Look for MusicBrainz Album+Artist ID in TXXX Frame
        UserTextIdentificationFrame *musicbrainz = find(taglib->ID3v2Tag(),
                "MusicBrainz Album Artist Id");

        if (musicbrainz)
        {
            // If the MusicBrainz ID is the special "Various Artists" ID
            // then compilation is TRUE
            if (! musicbrainz->fieldList().isEmpty())
                compilation = (MYTH_MUSICBRAINZ_ALBUMARTIST_UUID
                == TStringToQString(musicbrainz->fieldList().front()));
        }

        // Length
        if(!taglib->ID3v2Tag()->frameListMap()["TLEN"].isEmpty())
            length = taglib->ID3v2Tag()->frameListMap()["TLEN"].front()->toString().toInt();

        // Album Art
        if(!taglib->ID3v2Tag()->frameListMap()["APIC"].isEmpty())
        {
            albumart = readAlbumArt(taglib->ID3v2Tag());
        }
    }

    // Fallback to filename reading
    if (title.isEmpty())
    {
        readFromFilename(filename, artist, album, title, genre, tracknum);
    }

    if (length <= 0 && taglib->audioProperties())
        length = taglib->audioProperties()->length() * 1000;

    if (taglib)
        delete taglib;

    // If we didn't get a valid length, add the metadata but show warning.
    if (length <= 0)
        VERBOSE(VB_GENERAL, QString("MetaIOTagLib: Failed to read length "
                "from '%1'. It may be corrupt.").arg(filename));

    // If we don't have title and artist or don't have the length return NULL
    if (title.isEmpty() && artist.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, QString("MetaIOTagLib: Failed to read metadata from '%1'")
                .arg(filename));
        return NULL;
    }

    Metadata *retdata = new Metadata(filename, artist, compilation_artist, album,
                                     title, genre, year, tracknum, length,
                                     id, rating, playcount);

    retdata->setCompilation(compilation);
    retdata->setEmbeddedAlbumArt(albumart);

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
    int seconds = 0;
    QByteArray fname = filename.toLocal8Bit();
    File *taglib = new TagLib::MPEG::File(fname.constData());

    if (taglib)
    {
        seconds = taglib->audioProperties()->length();
        delete taglib;
    }

    return seconds;
}

/*!
 * \brief Read the albumart image from the file
 *
 * \param filename The filename for which we want to find the length.
 * \param type The type of image we want - front/back etc
 * \returns A QByteArray that can contains the image data.
 */
QImage MetaIOTagLib::getAlbumArt(QString filename, ImageType type)
{
    QImage picture;

    AttachedPictureFrame::Type apicType 
        = AttachedPictureFrame::FrontCover;

    switch (type)
    {
        case IT_UNKNOWN :
            apicType = AttachedPictureFrame::Other;
            break;
        case IT_FRONTCOVER :
            apicType = AttachedPictureFrame::FrontCover;
            break;
        case IT_BACKCOVER :
            apicType = AttachedPictureFrame::BackCover;
            break;
        case IT_CD :
            apicType = AttachedPictureFrame::Media;
            break;
        case IT_INLAY :
            apicType = AttachedPictureFrame::LeafletPage;
            break;
        default:
            return picture;
    }

    QByteArray fname = filename.toLocal8Bit();
    File *taglib = new TagLib::MPEG::File(fname.constData());

    if (taglib)
    {
        if (taglib->isOpen()
            && !taglib->ID3v2Tag()->frameListMap()["APIC"].isEmpty())
        {
            TagLib::ID3v2::FrameList apicframes =
                                    taglib->ID3v2Tag()->frameListMap()["APIC"];

            for(TagLib::ID3v2::FrameList::Iterator it = apicframes.begin();
                it != apicframes.end(); ++it)
            {
                AttachedPictureFrame *frame =
                                    static_cast<AttachedPictureFrame *>(*it);
                if(frame && frame->type() == apicType)
                {
                    QImage picture;
                    picture.loadFromData((const uchar *)frame->picture().data(),
                                         frame->picture().size());
                    return picture;
                }
            }
        }

        delete taglib;
    }

    return picture;
}

/*!
 * \brief Read the albumart image from the file
 *
 * \param tag The ID3v2 tag object in which to look for Album Art
 * \returns A QValueList containing a list of AlbumArtImage structs
 *          with the type and description of the APIC tag.
 */
AlbumArtList MetaIOTagLib::readAlbumArt(TagLib::ID3v2::Tag *tag)
{
    QList<struct AlbumArtImage> artlist;

    if (!tag->frameListMap()["APIC"].isEmpty())
    {
        TagLib::ID3v2::FrameList apicframes = tag->frameListMap()["APIC"];

        for(TagLib::ID3v2::FrameList::Iterator it = apicframes.begin();
            it != apicframes.end(); ++it)
        {

            AttachedPictureFrame *frame =
                static_cast<AttachedPictureFrame *>(*it);

            // Assume a valid image would have at least
            // 100 bytes of data (1x1 indexed gif is 35 bytes)
            if (frame->picture().size() < 100)
            {
                VERBOSE(VB_GENERAL, "Music Scanner - Discarding APIC frame "
                                    "with size less than 100 bytes");
                continue;
            }

            AlbumArtImage art;

            if (frame->description().isEmpty())
            {
                art.description = "";
            }
            else {
                art.description = TStringToQString(frame->description());
            }

            art.embedded = true;

            switch (frame->type())
            {
                case AttachedPictureFrame::FrontCover :
                    art.imageType = IT_FRONTCOVER;
                    break;
                case AttachedPictureFrame::BackCover :
                    art.imageType = IT_BACKCOVER;
                    break;
                case AttachedPictureFrame::Media :
                    art.imageType = IT_CD;
                    break;
                case AttachedPictureFrame::LeafletPage :
                    art.imageType = IT_INLAY;
                    break;
                case AttachedPictureFrame::Other :
                    art.imageType = IT_UNKNOWN;
                    break;
                default:
                    VERBOSE(VB_GENERAL, "Music Scanner - APIC tag found "
                                        "with unsupported type");
                    continue;
            }

            artlist.append(art);
        }
    }

    return artlist;
}

/*!
 * \brief Find the a custom comment tag by description.
 *        This is a copy of the same function in the
 *        TagLib::ID3v2::UserTextIdentificationFrame Class with a static
 *        instead of dynamic cast.
 *
 * \param tag Pointer to TagLib::ID3v2::Tag object
 * \param description Description of tag to search for
 * \returns Pointer to frame
 */
UserTextIdentificationFrame* MetaIOTagLib::find(TagLib::ID3v2::Tag *tag,
                                                const String &description)
{
  TagLib::ID3v2::FrameList l = tag->frameList("TXXX");
  for(TagLib::ID3v2::FrameList::Iterator it = l.begin(); it != l.end(); ++it)
  {
    UserTextIdentificationFrame *f =
                                static_cast<UserTextIdentificationFrame *>(*it);
    if(f && f->description() == description)
      return f;
  }
  return 0;
}
