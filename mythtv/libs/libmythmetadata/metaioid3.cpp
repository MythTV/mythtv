#include <set>

// qt
#include <QBuffer>

// Libmythbase
#include <mythlogging.h>
#include <mythcorecontext.h>

// Taglib
#include <flacfile.h>
#include <mpegfile.h>

// libmythmetadata
#include "metaioid3.h"
#include "musicmetadata.h"
#include "musicutils.h"

const String email = "music@mythtv.org";  // TODO username/ip/hostname?

MetaIOID3::MetaIOID3(void)
    : MetaIOTagLib(),
        m_file(NULL), m_fileType(kMPEG)
{
}

MetaIOID3::~MetaIOID3(void)
{
    CloseFile();
}

/*!
* \brief Open the file to read the tag
*
* \param filename The filename
* \returns A taglib file object for this format
*/
bool MetaIOID3::OpenFile(const QString &filename, bool forWriting)
{
    // Check if file is already opened
    if (m_file && (m_filename == filename) &&
        (!forWriting || !m_file->readOnly()))
        return true;

    if (m_file)
    {
        LOG(VB_FILE, LOG_DEBUG,
                        QString("MetaIO switch file: %1 New File: %2 Type: %3")
                                    .arg(m_filename)
                                    .arg(filename)
                                    .arg(m_fileType));
    }

    // If a file is open but it's not the requested file then close it first
    if (m_file)
        CloseFile();

    m_filename = filename;

    QString extension = m_filename.section('.', -1);

    if (extension.toLower() == "flac")
        m_fileType = kFLAC;
    else if (extension.toLower() == "mp3")
        m_fileType = kMPEG;
    else
        return false;

    QByteArray fname = m_filename.toLocal8Bit();
    // Open the file
    switch (m_fileType)
    {
        case kMPEG :
            m_file = new TagLib::MPEG::File(fname.constData());
            break;
        case kFLAC :
            m_file = new TagLib::FLAC::File(fname.constData());
            break;
    }

    // If the requested file could not be opened then close it and return false
    if (!m_file->isOpen() || (forWriting && m_file->readOnly()))
    {
        if (m_file->isOpen())
            LOG(VB_FILE, LOG_NOTICE,
                QString("Could not open file for writing: %1").arg(m_filename));
        else
            LOG(VB_FILE, LOG_ERR,
                QString("Could not open file: %1").arg(m_filename));

        CloseFile();
        return false;
    }

    return true;
}

bool MetaIOID3::SaveFile()
{
    if (!m_file)
        return false;

    saveTimeStamps();
    bool retval = m_file->save();
    restoreTimeStamps();

    return retval;
}

void MetaIOID3::CloseFile()
{
    LOG(VB_FILE, LOG_DEBUG, QString("MetaIO Close file: %1") .arg(m_filename));
    delete m_file;
    m_file = NULL;
    m_fileType = kMPEG;
    m_filename.clear();
}

TagLib::ID3v2::Tag* MetaIOID3::GetID3v2Tag(bool create)
{
    if (!m_file)
        return NULL;

    TagLib::ID3v2::Tag *tag = NULL;
    switch (m_fileType)
    {
        case kMPEG :
            tag = (static_cast<TagLib::MPEG::File*>(m_file))->ID3v2Tag(create);
            break;
        case kFLAC :
            tag = (static_cast<TagLib::FLAC::File*>(m_file))->ID3v2Tag(create);
            break;
    }

    return tag;
}

TagLib::ID3v1::Tag* MetaIOID3::GetID3v1Tag(bool create)
{
    if (!m_file)
        return NULL;

    TagLib::ID3v1::Tag *tag = NULL;
    switch (m_fileType)
    {
        case kMPEG :
            tag = (static_cast<TagLib::MPEG::File*>(m_file))->ID3v1Tag(create);
            break;
        case kFLAC :
            // Flac doesn't support ID3v1
            break;
    }

    return tag;
}

/*!
 * \copydoc MetaIO::write()
 */
bool MetaIOID3::write(const QString &filename, MusicMetadata* mdata)
{
    if (filename.isEmpty())
        return false;

    if (!OpenFile(filename, true))
        return false;

    TagLib::ID3v2::Tag *tag = GetID3v2Tag();

    if (!tag)
        return false;

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

    if (!SaveFile())
        return false;

    return true;
}

/*!
 * \copydoc MetaIO::read()
 */
MusicMetadata *MetaIOID3::read(const QString &filename)
{
    if (!OpenFile(filename))
        return NULL;

    TagLib::ID3v2::Tag *tag = GetID3v2Tag(true); // Create tag if none are found

    // if there is no ID3v2 tag, try to read the ID3v1 tag and copy it to
    // the ID3v2 tag structure
    if (tag->isEmpty())
    {
        TagLib::ID3v1::Tag *tag_v1 = GetID3v1Tag();

        if (!tag_v1)
            return NULL;

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

    MusicMetadata *metadata = new MusicMetadata(filename);

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
        {
            TagLib::StringList l = musicbrainz->fieldList();
            for (TagLib::StringList::ConstIterator it = l.begin(); it != l.end(); it++)
            {
                QString ID = TStringToQString((*it));

                if (ID == MYTH_MUSICBRAINZ_ALBUMARTIST_UUID)
                {
                    compilation = true;
                    break;
                }
            }
        }
    }

    // TLEN - Ignored intentionally, some encoders write bad values
    // e.g. Lame under certain circumstances will always write a length of
    // 27 hours

    // Length
    if (!tag->frameListMap()["TLEN"].isEmpty())
    {
        int length = tag->frameListMap()["TLEN"].front()->toString().toInt();
        LOG(VB_FILE, LOG_DEBUG,
            QString("MetaIOID3::read: Length for '%1' from tag is '%2'\n").arg(filename).arg(length));
    }

    metadata->setCompilation(compilation);

    metadata->setLength(getTrackLength(m_file));

    // The number of tracks on the album, if supplied
    if (!tag->frameListMap()["TRCK"].isEmpty())
    {
        QString trackFrame = TStringToQString(
                                tag->frameListMap()["TRCK"].front()->toString())
                                    .trimmed();
        int trackCount = trackFrame.section('/', -1).toInt();
        if (trackCount > 0)
            metadata->setTrackCount(trackCount);
    }

    LOG(VB_FILE, LOG_DEBUG,
            QString("MetaIOID3::read: Length for '%1' from properties is '%2'\n").arg(filename).arg(metadata->Length()));

    return metadata;
}

/*!
 * \brief Read the albumart image from the file
 *
 * \param filename The filename for which we want to find the albumart.
 * \param type The type of image we want - front/back etc
 * \returns A pointer to a QImage owned by the caller or NULL if not found.
 */
QImage* MetaIOID3::getAlbumArt(const QString &filename, ImageType type)
{
    QImage *picture = new QImage();

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
        case IT_ARTIST :
            apicType = AttachedPictureFrame::Artist;
            break;
        default:
            return picture;
    }

    if (OpenFile(filename))
    {
        TagLib::ID3v2::Tag *tag = GetID3v2Tag();
        if (tag && !tag->frameListMap()["APIC"].isEmpty())
        {
            TagLib::ID3v2::FrameList apicframes = tag->frameListMap()["APIC"];

            for(TagLib::ID3v2::FrameList::Iterator it = apicframes.begin();
                it != apicframes.end(); ++it)
            {
                AttachedPictureFrame *frame =
                                    static_cast<AttachedPictureFrame *>(*it);
                if (frame && frame->type() == apicType)
                {
                    picture->loadFromData((const uchar *)frame->picture().data(),
                                         frame->picture().size());
                    return picture;
                }
            }
        }
    }

    delete picture;

    return NULL;
}


/*!
 * \brief Read the albumart images from the file
 *
 * \param filename The filename for which we want to find the images.
 */
AlbumArtList MetaIOID3::getAlbumArtList(const QString &filename)
{
    AlbumArtList imageList;
    if (OpenFile(filename))
    {
        TagLib::ID3v2::Tag *tag = GetID3v2Tag();

        if (!tag)
            return imageList;

        imageList = readAlbumArt(tag);
    }

    return imageList;
}

/*!
 * \brief Read the albumart images from the file
 *
 * \param tag The ID3v2 tag object in which to look for Album Art
 * \returns A QList containing a list of AlbumArtImage's
 *          with the type and description of the APIC tag.
 */
AlbumArtList MetaIOID3::readAlbumArt(TagLib::ID3v2::Tag *tag)
{
    AlbumArtList artlist;

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
                LOG(VB_GENERAL, LOG_NOTICE,
                    "Music Scanner - Discarding APIC frame "
                    "with size less than 100 bytes");
                continue;
            }

            AlbumArtImage *art = new AlbumArtImage();

            if (frame->description().isEmpty())
                art->description.clear();
            else
                art->description = TStringToQString(frame->description());

            art->embedded = true;
            art->hostname = gCoreContext->GetHostName();

            QString ext = getExtFromMimeType(
                                TStringToQString(frame->mimeType()).toLower());

            switch (frame->type())
            {
                case AttachedPictureFrame::FrontCover :
                    art->imageType = IT_FRONTCOVER;
                    art->filename = QString("front") + ext;
                    break;
                case AttachedPictureFrame::BackCover :
                    art->imageType = IT_BACKCOVER;
                    art->filename = QString("back") + ext;
                    break;
                case AttachedPictureFrame::Media :
                    art->imageType = IT_CD;
                    art->filename = QString("cd") + ext;
                    break;
                case AttachedPictureFrame::LeafletPage :
                    art->imageType = IT_INLAY;
                    art->filename = QString("inlay") + ext;
                    break;
                case AttachedPictureFrame::Artist :
                    art->imageType = IT_ARTIST;
                    art->filename = QString("artist") + ext;
                    break;
                case AttachedPictureFrame::Other :
                    art->imageType = IT_UNKNOWN;
                    art->filename = QString("unknown") + ext;
                    break;
                default:
                    LOG(VB_GENERAL, LOG_ERR, "Music Scanner - APIC tag found "
                                             "with unsupported type");
                    delete art;
                    continue;
            }

            artlist.append(art);
        }
    }

    return artlist;
}

QString MetaIOID3::getExtFromMimeType(const QString &mimeType)
{
    if (mimeType == "image/png")
        return QString(".png");
    else if (mimeType == "image/jpeg" || mimeType == "image/jpg")
        return QString(".jpg");
    else if (mimeType == "image/gif")
        return QString(".gif");
    else if (mimeType == "image/bmp")
        return QString(".bmp");

    LOG(VB_GENERAL, LOG_ERR,
        "Music Scanner - Unknown image mimetype found - " + mimeType);

    return QString();
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
 * \param filename The music file to add the albumart
 * \param albumart The Album Art image to write
 * \returns True if successful
 *
 * \Note We always save the image in JPEG format
 */
bool MetaIOID3::writeAlbumArt(const QString &filename,
                              const AlbumArtImage *albumart)
{
    if (filename.isEmpty() || !albumart)
        return false;

    // load the image into a QByteArray
    QImage image(albumart->filename);
    QByteArray imageData;
    QBuffer buffer(&imageData);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "JPEG");

    AttachedPictureFrame::Type type = AttachedPictureFrame::Other;
    switch (albumart->imageType)
    {
        case IT_FRONTCOVER:
            type = AttachedPictureFrame::FrontCover;
            break;
        case IT_BACKCOVER:
            type = AttachedPictureFrame::BackCover;
            break;
        case IT_CD:
            type = AttachedPictureFrame::Media;
            break;
        case IT_INLAY:
            type = AttachedPictureFrame::LeafletPage;
            break;
        case IT_ARTIST:
            type = AttachedPictureFrame::Artist;
            break;
        default:
            type = AttachedPictureFrame::Other;
            break;
    }

    if (!OpenFile(filename, true))
        return false;

    TagLib::ID3v2::Tag *tag = GetID3v2Tag();

    if (!tag)
        return false;

    AttachedPictureFrame *apic = findAPIC(tag, type,
                                    QStringToTString(albumart->description));

    if (!apic)
    {
        apic = new AttachedPictureFrame();
        tag->addFrame(apic);
        apic->setType(type);
    }

    QString mimetype = "image/jpeg";

    TagLib::ByteVector bytevector;
    bytevector.setData(imageData.data(), imageData.size());

    apic->setMimeType(QStringToTString(mimetype));
    apic->setPicture(bytevector);
    apic->setDescription(QStringToTString(albumart->description));

    if (!SaveFile())
        return false;

    return true;
}

/*!
 * \brief Remove the albumart image from the file
 *
 * \param filename The music file to remove the albumart
 * \param albumart The Album Art image to remove
 * \returns True if successful
 */
bool MetaIOID3::removeAlbumArt(const QString &filename,
                               const AlbumArtImage *albumart)
{
    if (filename.isEmpty() || !albumart)
        return false;

    AttachedPictureFrame::Type type = AttachedPictureFrame::Other;
    switch (albumart->imageType)
    {
        case IT_FRONTCOVER:
            type = AttachedPictureFrame::FrontCover;
            break;
        case IT_BACKCOVER:
            type = AttachedPictureFrame::BackCover;
            break;
        case IT_CD:
            type = AttachedPictureFrame::Media;
            break;
        case IT_INLAY:
            type = AttachedPictureFrame::LeafletPage;
            break;
        case IT_ARTIST:
            type = AttachedPictureFrame::Artist;
            break;
        default:
            type = AttachedPictureFrame::Other;
            break;
    }

    if (!OpenFile(filename, true))
        return false;

    TagLib::ID3v2::Tag *tag = GetID3v2Tag();

    if (!tag)
        return false;

    AttachedPictureFrame *apic = findAPIC(tag, type,
                                    QStringToTString(albumart->description));
    if (!apic)
        return false;

    tag->removeFrame(apic);

    if (!SaveFile())
        return false;

    return true;
}

bool MetaIOID3::changeImageType(const QString &filename,
                                const AlbumArtImage* albumart,
                                ImageType newType)
{
    if (!albumart)
        return false;

    if (albumart->imageType == newType)
        return true;

    AttachedPictureFrame::Type type = AttachedPictureFrame::Other;
    switch (albumart->imageType)
    {
        case IT_FRONTCOVER:
            type = AttachedPictureFrame::FrontCover;
            break;
        case IT_BACKCOVER:
            type = AttachedPictureFrame::BackCover;
            break;
        case IT_CD:
            type = AttachedPictureFrame::Media;
            break;
        case IT_INLAY:
            type = AttachedPictureFrame::LeafletPage;
            break;
        case IT_ARTIST:
            type = AttachedPictureFrame::Artist;
            break;
        default:
            type = AttachedPictureFrame::Other;
            break;
    }

    if (!OpenFile(filename, true))
        return false;

    TagLib::ID3v2::Tag *tag = GetID3v2Tag();

    if (!tag)
        return false;

    AttachedPictureFrame *apic = findAPIC(tag, type,
                                    QStringToTString(albumart->description));
    if (!apic)
        return false;

    // set the new image type
    switch (newType)
    {
        case IT_FRONTCOVER:
            apic->setType(AttachedPictureFrame::FrontCover);
            break;
        case IT_BACKCOVER:
            apic->setType(AttachedPictureFrame::BackCover);
            break;
        case IT_CD:
            apic->setType(AttachedPictureFrame::Media);
            break;
        case IT_INLAY:
            apic->setType(AttachedPictureFrame::LeafletPage);
            break;
        case IT_ARTIST:
            apic->setType(AttachedPictureFrame::Artist);
            break;
        default:
            apic->setType(AttachedPictureFrame::Other);
            break;
    }

    if (!SaveFile())
        return false;

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

bool MetaIOID3::writeVolatileMetadata(const QString &filename, MusicMetadata* mdata)
{
    if (filename.isEmpty())
        return false;

    int rating = mdata->Rating();
    int playcount = mdata->PlayCount();

    if (!OpenFile(filename, true))
        return false;

    TagLib::ID3v2::Tag *tag = GetID3v2Tag();

    if (!tag)
        return false;

    bool result = (writeRating(tag, rating) && writePlayCount(tag, playcount));

    if (!SaveFile())
        return false;

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

bool MetaIOID3::TagExists(const QString &filename)
{
    if (!OpenFile(filename))
        return false;

    TagLib::ID3v1::Tag *v1_tag = GetID3v1Tag();
    TagLib::ID3v2::Tag *v2_tag = GetID3v2Tag();

    bool retval = false;

    if ((v2_tag && !v2_tag->isEmpty()) ||
        (v1_tag && !v1_tag->isEmpty()))
        retval = true;

    return retval;
}
