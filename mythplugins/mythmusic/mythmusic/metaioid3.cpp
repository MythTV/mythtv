// MythMusic
#include "metaioid3.h"
#include "metadata.h"

// Libmyth
#include <mythverbose.h>
#include <set>

const String email = "music@mythtv.org";  // TODO username/ip/hostname?

MetaIOID3::MetaIOID3(void)
    : MetaIOTagLib()
{
}

MetaIOID3::~MetaIOID3(void)
{
}

/*!
* \brief Open the file to read the tag
*
* \param filename The filename
* \returns A taglib file object for this format
*/
TagLib::MPEG::File *MetaIOID3::OpenFile(const QString &filename)
{
    QByteArray fname = filename.toLocal8Bit();
    TagLib::MPEG::File *mpegfile = new TagLib::MPEG::File(fname.constData());

    if (!mpegfile->isOpen())
    {
        delete mpegfile;
        mpegfile = NULL;
    }

    return mpegfile;
}

/*!
 * \copydoc MetaIO::write()
 */
bool MetaIOID3::write(Metadata* mdata)
{
    TagLib::MPEG::File *mpegfile = OpenFile(mdata->Filename());

    if (!mpegfile)
        return false;

    TagLib::ID3v2::Tag *tag = mpegfile->ID3v2Tag();

    if (!tag)
    {
        delete mpegfile;
        return false;
    }

    WriteGenericMetadata(tag, mdata);

    // MythTV rating and playcount, stored in POPM frame
    writeRating(tag, mdata->Rating());
    writePlayCount(tag, mdata->PlayCount());

    // MusicBrainz ID
    UserTextIdentificationFrame *musicbrainz = NULL;
    musicbrainz = find(tag, "MusicBrainz Album Artist Id");

    if (mdata->Compilation())
    {

        if (!musicbrainz)
        {
            musicbrainz = new UserTextIdentificationFrame(TagLib::String::UTF8);
            tag->addFrame(musicbrainz);
            musicbrainz->setDescription("MusicBrainz Album Artist Id");
        }

        musicbrainz->setText(MYTH_MUSICBRAINZ_ALBUMARTIST_UUID);
    }
    else if (musicbrainz)
        tag->removeFrame(musicbrainz);

    // Compilation Artist Frame (TPE4/2)
    if (!mdata->CompilationArtist().isEmpty())
    {
        TextIdentificationFrame *tpe4frame = NULL;
        TagLib::ID3v2::FrameList tpelist = tag->frameListMap()["TPE4"];
        if (!tpelist.isEmpty())
            tpe4frame = (TextIdentificationFrame *)tpelist.front();

        if (!tpe4frame)
        {
            tpe4frame = new TextIdentificationFrame(TagLib::ByteVector("TPE4"),
                                                    TagLib::String::UTF8);
                                                    tag->addFrame(tpe4frame);
        }
        tpe4frame->setText(QStringToTString(mdata->CompilationArtist()));


        TextIdentificationFrame *tpe2frame = NULL;
        tpelist = tag->frameListMap()["TPE2"];
        if (!tpelist.isEmpty())
            tpe2frame = (TextIdentificationFrame *)tpelist.front();

        if (!tpe2frame)
        {
            tpe2frame = new TextIdentificationFrame(TagLib::ByteVector("TPE2"),
                                                    TagLib::String::UTF8);
                                                    tag->addFrame(tpe2frame);
        }
        tpe2frame->setText(QStringToTString(mdata->CompilationArtist()));
    }

    bool result = mpegfile->save();

    delete mpegfile;

    return result;
}

/*!
 * \copydoc MetaIO::read()
 */
Metadata *MetaIOID3::read(QString filename)
{
    TagLib::MPEG::File *mpegfile = OpenFile(filename);

    if (!mpegfile)
        return NULL;

    TagLib::ID3v2::Tag *tag = mpegfile->ID3v2Tag();

    if (!tag)
    {
        delete mpegfile;
        return NULL;
    }

    // if there is no ID3v2 tag, try to read the ID3v1 tag and copy it to the ID3v2 tag structure
    if (tag->isEmpty())
    {
        TagLib::ID3v1::Tag *tag_v1 = mpegfile->ID3v1Tag();

        if (!tag_v1)
        {
            delete mpegfile;
            return NULL;
        }

        if (!tag_v1->isEmpty())
        {
            tag->setTitle(tag_v1->title());
            tag->setArtist(tag_v1->artist());
            tag->setAlbum(tag_v1->album());
            tag->setTrack(tag_v1->track());
            tag->setYear(tag_v1->year());
            tag->setGenre(tag_v1->genre());
        }
    }

    Metadata *metadata = new Metadata(filename);

    ReadGenericMetadata(tag, metadata);

    bool compilation = false;

    // Compilation Artist (TPE4 Remix) or fallback to (TPE2 Band)
    // N.B. The existance of a either frame is NOT an indication that this
    // is a compilation, but if it is then one of them will probably hold
    // the compilation artist.
    TextIdentificationFrame *tpeframe = NULL;
    TagLib::ID3v2::FrameList tpelist = tag->frameListMap()["TPE4"];
    if (tpelist.isEmpty() || tpelist.front()->toString().isEmpty())
        tpelist = tag->frameListMap()["TPE2"];
    if (!tpelist.isEmpty())
        tpeframe = (TextIdentificationFrame *)tpelist.front();

    if (tpeframe && !tpeframe->toString().isEmpty())
    {
        QString compilation_artist = TStringToQString(tpeframe->toString())
                                                                    .trimmed();
        metadata->setCompilationArtist(compilation_artist);
    }

    // MythTV rating and playcount, stored in POPM frame
    PopularimeterFrame *popm = findPOPM(tag, email);

    if (!popm)
    {
        if (!tag->frameListMap()["POPM"].isEmpty())
            popm = dynamic_cast<PopularimeterFrame *>
                                        (tag->frameListMap()["POPM"].front());
    }

    if (popm)
    {
        int rating = popm->rating();
        rating = static_cast<int>(((static_cast<float>(rating)/255.0)
                                                                * 10.0) + 0.5);
        metadata->setRating(rating);
        metadata->setPlaycount(popm->counter());
    }

    // Look for MusicBrainz Album+Artist ID in TXXX Frame
    UserTextIdentificationFrame *musicbrainz = find(tag,
                                            "MusicBrainz Album Artist Id");

    if (musicbrainz)
    {
        // If the MusicBrainz ID is the special "Various Artists" ID
        // then compilation is TRUE
        if (!compilation && !musicbrainz->fieldList().isEmpty())
            compilation = (MYTH_MUSICBRAINZ_ALBUMARTIST_UUID
            == TStringToQString(musicbrainz->fieldList().front()));
    }

    // Length
    if (!tag->frameListMap()["TLEN"].isEmpty())
    {
        int length = tag->frameListMap()["TLEN"].front()->toString().toInt();
        metadata->setLength(length);
    }

    // Album Art
    if (!tag->frameListMap()["APIC"].isEmpty())
    {
        QList<struct AlbumArtImage> albumart;
        albumart = readAlbumArt(tag);
        metadata->setEmbeddedAlbumArt(albumart);
    }

    metadata->setCompilation(compilation);

    if (metadata->Length() <= 0)
    {
        TagLib::FileRef *fileref = new TagLib::FileRef(mpegfile);
        metadata->setLength(getTrackLength(fileref));
        // FileRef takes ownership of mpegfile, and is responsible for it's
        // deletion. Messy.
        delete fileref;
    }
    else
        delete mpegfile;

    return metadata;
}

/*!
 * \brief Read the albumart image from the file
 *
 * \param filename The filename for which we want to find the length.
 * \param type The type of image we want - front/back etc
 * \returns A QByteArray that can contains the image data.
 */
QImage MetaIOID3::getAlbumArt(QString filename, ImageType type)
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
    TagLib::MPEG::File *mpegfile = new TagLib::MPEG::File(fname.constData());

    if (mpegfile)
    {
        if (mpegfile->isOpen()
            && !mpegfile->ID3v2Tag()->frameListMap()["APIC"].isEmpty())
        {
            TagLib::ID3v2::FrameList apicframes =
                                    mpegfile->ID3v2Tag()->frameListMap()["APIC"];

            for(TagLib::ID3v2::FrameList::Iterator it = apicframes.begin();
                it != apicframes.end(); ++it)
            {
                AttachedPictureFrame *frame =
                                    static_cast<AttachedPictureFrame *>(*it);
                if (frame && frame->type() == apicType)
                {
                    QImage picture;
                    picture.loadFromData((const uchar *)frame->picture().data(),
                                         frame->picture().size());
                    return picture;
                }
            }
        }

        delete mpegfile;
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
AlbumArtList MetaIOID3::readAlbumArt(TagLib::ID3v2::Tag *tag)
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
                art.description.clear();
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
 * \brief Find an APIC tag by type and optionally description
 *
 * \param tag Pointer to TagLib::ID3v2::Tag object
 * \param type Type of picture to search for
 * \param description Description of picture to search for (optional)
 * \returns Pointer to frame
 */
AttachedPictureFrame* MetaIOID3::findAPIC(TagLib::ID3v2::Tag *tag,
                                        const AttachedPictureFrame::Type &type,
                                        const String &description)
{
  TagLib::ID3v2::FrameList l = tag->frameList("APIC");
  for(TagLib::ID3v2::FrameList::Iterator it = l.begin(); it != l.end(); ++it)
  {
    AttachedPictureFrame *f = static_cast<AttachedPictureFrame *>(*it);
    if (f && f->type() == type &&
        (description.isNull() || f->description() == description))
      return f;
  }
  return NULL;
}

/*!
 * \brief Write the albumart image to the file
 *
 * \param tag The ID3v2 tag object in which to look for Album Art
 * \returns True if successful
 */
bool MetaIOID3::writeAlbumArt(TagLib::ID3v2::Tag *tag, QByteArray *image,
                              const AttachedPictureFrame::Type &type)
{
    if (!tag || !image)
        return false;

    AttachedPictureFrame *apic = findAPIC(tag, type);

    if (!apic)
    {
        apic = new AttachedPictureFrame();
        tag->addFrame(apic);
        apic->setType(type);
    }

    TagLib::ByteVector bytevector;
    bytevector.setData(image->data(), image->size());
    delete image;

    apic->setPicture(bytevector);

    return true;
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
UserTextIdentificationFrame* MetaIOID3::find(TagLib::ID3v2::Tag *tag,
                                                const String &description)
{
  TagLib::ID3v2::FrameList l = tag->frameList("TXXX");
  for(TagLib::ID3v2::FrameList::Iterator it = l.begin(); it != l.end(); ++it)
  {
    UserTextIdentificationFrame *f =
                                static_cast<UserTextIdentificationFrame *>(*it);
    if (f && f->description() == description)
      return f;
  }
  return NULL;
}

/*!
 * \brief Find the POPM tag associated with MythTV
 *
 * \param tag Pointer to TagLib::ID3v2::Tag object
 * \param email Email address associated with this POPM frame
 * \returns Pointer to frame
 */
PopularimeterFrame* MetaIOID3::findPOPM(TagLib::ID3v2::Tag *tag,
                                        const String &email)
{
  TagLib::ID3v2::FrameList l = tag->frameList("POPM");
  for(TagLib::ID3v2::FrameList::Iterator it = l.begin(); it != l.end(); ++it)
  {
    PopularimeterFrame *f = static_cast<PopularimeterFrame *>(*it);
    if (f && f->email() == email)
      return f;
  }
  return NULL;
}

bool MetaIOID3::writePlayCount(TagLib::ID3v2::Tag *tag, int playcount)
{
    if (!tag)
        return false;

    PopularimeterFrame *popm = findPOPM(tag, email);

    if (!popm)
    {
        popm = new PopularimeterFrame();
        tag->addFrame(popm);
        popm->setEmail(email);
    }

    popm->setCounter(playcount);

    return true;
}

bool MetaIOID3::writeVolatileMetadata(const Metadata* mdata)
{
    QString filename = mdata->Filename();
    int rating = mdata->Rating();
    int playcount = mdata->PlayCount();
    TagLib::MPEG::File *mpegfile = OpenFile(filename);

    if (!mpegfile)
        return false;

    TagLib::ID3v2::Tag *tag = mpegfile->ID3v2Tag();

    if (!tag)
    {
        delete mpegfile;
        return false;
    }

    bool result = (writeRating(tag, rating) && writePlayCount(tag, playcount));

    mpegfile->save();
    delete mpegfile;

    return result;
}

bool MetaIOID3::writeRating(TagLib::ID3v2::Tag *tag, int rating)
{
    if (!tag)
        return false;

    PopularimeterFrame *popm = findPOPM(tag, email);

    if (!popm)
    {
        popm = new PopularimeterFrame();
        tag->addFrame(popm);
        popm->setEmail(email);
    }
    int popmrating = static_cast<int>(((static_cast<float>(rating) / 10.0)
                                                               * 255.0) + 0.5);
    popm->setRating(popmrating);

    return true;
}
