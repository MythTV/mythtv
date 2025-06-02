#include <set>

// C++ headers
#include <cmath>

// qt
#include <QBuffer>

// Libmythbase
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythcorecontext.h"

// Taglib
#include <taglib/flacfile.h>
#include <taglib/mpegfile.h>

// libmythmetadata
#include "metaioid3.h"
#include "musicmetadata.h"
#include "musicutils.h"

const TagLib::String email = "music@mythtv.org";  // TODO username/ip/hostname?

/*!
* \brief Open the file to read the tag
*
* \param filename The filename
* \param forWriting Need write permission on the file.
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
                                    .arg(m_filename,
                                         filename,
                                         QString::number(m_fileType)));
    }

    // If a file is open but it's not the requested file then close it first
    if (m_file)
        CloseFile();

    m_filename = filename;

    QString extension = m_filename.section('.', -1);

    if (extension.toLower() == "flac")
        m_fileType = kFLAC;
    else if (extension.toLower() == "mp3" || extension.toLower() == "mp2")
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
        {
            LOG(VB_FILE, LOG_NOTICE,
                QString("Could not open file for writing: %1").arg(m_filename));
        }
        else
        {
            LOG(VB_FILE, LOG_ERR,
                QString("Could not open file: %1").arg(m_filename));
        }

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
    m_file = nullptr;
    m_fileType = kMPEG;
    m_filename.clear();
}

TagLib::ID3v2::Tag* MetaIOID3::GetID3v2Tag(bool create)
{
    if (!m_file)
        return nullptr;

    if (m_fileType == kMPEG)
    {
        auto *file = dynamic_cast<TagLib::MPEG::File*>(m_file);
        return (file != nullptr) ? file->ID3v2Tag(create) : nullptr;
    }

    if (m_fileType == kFLAC)
    {
        auto *file = dynamic_cast<TagLib::FLAC::File*>(m_file);
        return (file != nullptr) ? file->ID3v2Tag(create) : nullptr;
    }

    return nullptr;
}

TagLib::ID3v1::Tag* MetaIOID3::GetID3v1Tag(bool create)
{
    if (!m_file)
        return nullptr;

    if (m_fileType == kMPEG)
    {
        auto *file = dynamic_cast<TagLib::MPEG::File*>(m_file);
        return (file != nullptr) ? file->ID3v1Tag(create) : nullptr;
    }

    return nullptr;
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
    writeLastPlay(tag, mdata->LastPlay());

    // MusicBrainz ID
    TagLib::ID3v2::UserTextIdentificationFrame *musicbrainz = nullptr;
    musicbrainz = find(tag, "MusicBrainz Album Artist Id");

    if (mdata->Compilation())
    {

        if (!musicbrainz)
        {
            musicbrainz = new TagLib::ID3v2::UserTextIdentificationFrame(TagLib::String::UTF8);
            tag->addFrame(musicbrainz);
            musicbrainz->setDescription("MusicBrainz Album Artist Id");
        }

        musicbrainz->setText(MYTH_MUSICBRAINZ_ALBUMARTIST_UUID);
    }
    else if (musicbrainz)
    {
        tag->removeFrame(musicbrainz);
    }

    // Compilation Artist Frame (TPE4/2)
    if (!mdata->CompilationArtist().isEmpty())
    {
        TagLib::ID3v2::TextIdentificationFrame *tpe4frame = nullptr;
        TagLib::ID3v2::FrameList tpelist = tag->frameListMap()["TPE4"];
        if (!tpelist.isEmpty())
            tpe4frame = dynamic_cast<TagLib::ID3v2::TextIdentificationFrame *>(tpelist.front());

        if (!tpe4frame)
        {
            tpe4frame = new TagLib::ID3v2::TextIdentificationFrame(TagLib::ByteVector("TPE4"),
                                                    TagLib::String::UTF8);
            tag->addFrame(tpe4frame);
        }
        tpe4frame->setText(QStringToTString(mdata->CompilationArtist()));


        TagLib::ID3v2::TextIdentificationFrame *tpe2frame = nullptr;
        tpelist = tag->frameListMap()["TPE2"];
        if (!tpelist.isEmpty())
            tpe2frame = dynamic_cast<TagLib::ID3v2::TextIdentificationFrame *>(tpelist.front());

        if (!tpe2frame)
        {
            tpe2frame = new TagLib::ID3v2::TextIdentificationFrame(TagLib::ByteVector("TPE2"),
                                                    TagLib::String::UTF8);
            tag->addFrame(tpe2frame);
        }
        tpe2frame->setText(QStringToTString(mdata->CompilationArtist()));
    }

    return SaveFile();
}

/*!
 * \copydoc MetaIO::read()
 */
MusicMetadata *MetaIOID3::read(const QString &filename)
{
    if (!OpenFile(filename))
        return nullptr;

    TagLib::ID3v2::Tag *tag = GetID3v2Tag(true); // Create tag if none are found

    // if there is no ID3v2 tag, try to read the ID3v1 tag and copy it to
    // the ID3v2 tag structure
    if (tag->isEmpty())
    {
        TagLib::ID3v1::Tag *tag_v1 = GetID3v1Tag();

        if (!tag_v1)
            return nullptr;

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

    auto *metadata = new MusicMetadata(filename);

    ReadGenericMetadata(tag, metadata);

    bool compilation = false;

    // Compilation Artist (TPE4 Remix) or fallback to (TPE2 Band)
    // N.B. The existance of a either frame is NOT an indication that this
    // is a compilation, but if it is then one of them will probably hold
    // the compilation artist.
    TagLib::ID3v2::TextIdentificationFrame *tpeframe = nullptr;
    TagLib::ID3v2::FrameList tpelist = tag->frameListMap()["TPE4"];
    if (tpelist.isEmpty() || tpelist.front()->toString().isEmpty())
        tpelist = tag->frameListMap()["TPE2"];
    if (!tpelist.isEmpty())
        tpeframe = dynamic_cast<TagLib::ID3v2::TextIdentificationFrame *>(tpelist.front());

    if (tpeframe && !tpeframe->toString().isEmpty())
    {
        QString compilation_artist = TStringToQString(tpeframe->toString())
                                                                    .trimmed();
        metadata->setCompilationArtist(compilation_artist);
    }

    // Rating and playcount, stored in POPM frame
    TagLib::ID3v2::PopularimeterFrame *popm = findPOPM(tag, ""); // Global (all apps) tag

    // If no 'global' tag exists, look for the MythTV specific one
    if (!popm)
    {
        popm = findPOPM(tag, email);
    }

    // Fallback to using any POPM tag we can find
    if (!popm)
    {
        if (!tag->frameListMap()["POPM"].isEmpty())
            popm = dynamic_cast<TagLib::ID3v2::PopularimeterFrame *>
                                        (tag->frameListMap()["POPM"].front());
    }

    if (popm)
    {
        int rating = popm->rating();
        rating = lroundf(static_cast<float>(rating) / 255.0F * 10.0F);
        metadata->setRating(rating);
        metadata->setPlaycount(popm->counter());
    }

    // Look for MusicBrainz Album+Artist ID in TXXX Frame
    TagLib::ID3v2::UserTextIdentificationFrame *musicbrainz = find(tag,
                                            "MusicBrainz Album Artist Id");

    if (musicbrainz)
    {
        // If the MusicBrainz ID is the special "Various Artists" ID
        // then compilation is TRUE
        if (!compilation && !musicbrainz->fieldList().isEmpty())
        {
            for (auto & field : musicbrainz->fieldList())
            {
                QString ID = TStringToQString(field);

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
        QString("MetaIOID3::read: Length for '%1' from properties is '%2'\n")
        .arg(filename).arg(metadata->Length().count()));

    // Look for MythTVLastPlayed in TXXX Frame
    TagLib::ID3v2::UserTextIdentificationFrame *lastplayed = find(tag, "MythTVLastPlayed");
    if (lastplayed)
    {
        QString lastPlayStr = TStringToQString(lastplayed->toString());
        metadata->setLastPlay(QDateTime::fromString(lastPlayStr, Qt::ISODate));
    }

    // Part of a set
    if (!tag->frameListMap()["TPOS"].isEmpty())
    {
        QString pos = TStringToQString(
                        tag->frameListMap()["TPOS"].front()->toString()).trimmed();

        int discNumber = pos.section('/', 0, 0).toInt();
        int discCount  = pos.section('/', -1).toInt();

        if (discNumber > 0)
            metadata->setDiscNumber(discNumber);
        if (discCount > 0)
            metadata->setDiscCount(discCount);
    }

    return metadata;
}

/*!
 * \brief Read the albumart image from the file
 *
 * \param filename The filename for which we want to find the albumart.
 * \param type The type of image we want - front/back etc
 * \returns A pointer to a QImage owned by the caller or nullptr if not found.
 */
QImage* MetaIOID3::getAlbumArt(const QString &filename, ImageType type)
{
    auto *picture = new QImage();

    TagLib::ID3v2::AttachedPictureFrame::Type apicType
        = TagLib::ID3v2::AttachedPictureFrame::FrontCover;

    switch (type)
    {
        case IT_UNKNOWN :
            apicType = TagLib::ID3v2::AttachedPictureFrame::Other;
            break;
        case IT_FRONTCOVER :
            apicType = TagLib::ID3v2::AttachedPictureFrame::FrontCover;
            break;
        case IT_BACKCOVER :
            apicType = TagLib::ID3v2::AttachedPictureFrame::BackCover;
            break;
        case IT_CD :
            apicType = TagLib::ID3v2::AttachedPictureFrame::Media;
            break;
        case IT_INLAY :
            apicType = TagLib::ID3v2::AttachedPictureFrame::LeafletPage;
            break;
        case IT_ARTIST :
            apicType = TagLib::ID3v2::AttachedPictureFrame::Artist;
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

            for (auto & apicframe : apicframes)
            {
                auto *frame = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame *>(apicframe);
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

    return nullptr;
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

        for (auto & apicframe : apicframes)
        {
            auto *frame = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame *>(apicframe);
            if (frame == nullptr)
            {
                LOG(VB_GENERAL, LOG_DEBUG,
                    "Music Scanner - Cannot convert APIC frame");
                continue;
            }

            // Assume a valid image would have at least
            // 100 bytes of data (1x1 indexed gif is 35 bytes)
            if (frame->picture().size() < 100)
            {
                LOG(VB_GENERAL, LOG_NOTICE,
                    "Music Scanner - Discarding APIC frame "
                    "with size less than 100 bytes");
                continue;
            }

            auto *art = new AlbumArtImage();

            if (frame->description().isEmpty())
                art->m_description.clear();
            else
                art->m_description = TStringToQString(frame->description());

            art->m_embedded = true;
            art->m_hostname = gCoreContext->GetHostName();

            QString ext = getExtFromMimeType(
                                TStringToQString(frame->mimeType()).toLower());

            switch (frame->type())
            {
                case TagLib::ID3v2::AttachedPictureFrame::FrontCover :
                    art->m_imageType = IT_FRONTCOVER;
                    art->m_filename = QString("front") + ext;
                    break;
                case TagLib::ID3v2::AttachedPictureFrame::BackCover :
                    art->m_imageType = IT_BACKCOVER;
                    art->m_filename = QString("back") + ext;
                    break;
                case TagLib::ID3v2::AttachedPictureFrame::Media :
                    art->m_imageType = IT_CD;
                    art->m_filename = QString("cd") + ext;
                    break;
                case TagLib::ID3v2::AttachedPictureFrame::LeafletPage :
                    art->m_imageType = IT_INLAY;
                    art->m_filename = QString("inlay") + ext;
                    break;
                case TagLib::ID3v2::AttachedPictureFrame::Artist :
                    art->m_imageType = IT_ARTIST;
                    art->m_filename = QString("artist") + ext;
                    break;
                case TagLib::ID3v2::AttachedPictureFrame::Other :
                    art->m_imageType = IT_UNKNOWN;
                    art->m_filename = QString("unknown") + ext;
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
        return {".png"};
    if (mimeType == "image/jpeg" || mimeType == "image/jpg")
        return {".jpg"};
    if (mimeType == "image/gif")
        return {".gif"};
    if (mimeType == "image/bmp")
        return {".bmp"};

    LOG(VB_GENERAL, LOG_ERR,
        "Music Scanner - Unknown image mimetype found - " + mimeType);

    return {};
}

/*!
 * \brief Find an APIC tag by type and optionally description
 *
 * \param tag Pointer to TagLib::ID3v2::Tag object
 * \param type Type of picture to search for
 * \param description Description of picture to search for (optional)
 * \returns Pointer to frame
 */
TagLib::ID3v2::AttachedPictureFrame* MetaIOID3::findAPIC(TagLib::ID3v2::Tag *tag,
                                        TagLib::ID3v2::AttachedPictureFrame::Type type,
                                        const TagLib::String &description)
{
  TagLib::ID3v2::FrameList l = tag->frameList("APIC");
  for (auto & frame : l)
  {
    auto *f = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame *>(frame);
    if (f && f->type() == type &&
        (description.isEmpty() || f->description() == description))
      return f;
  }
  return nullptr;
}

/*!
 * \brief Write the albumart image to the file
 *
 * \param filename The music file to add the albumart
 * \param albumart The Album Art image to write
 * \returns True if successful
 *
 * \note We always save the image in JPEG format
 */
bool MetaIOID3::writeAlbumArt(const QString &filename,
                              const AlbumArtImage *albumart)
{
    if (filename.isEmpty() || !albumart)
        return false;

    // load the image into a QByteArray
    QImage image(albumart->m_filename);
    QByteArray imageData;
    QBuffer buffer(&imageData);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "JPEG");

    TagLib::ID3v2::AttachedPictureFrame::Type type = TagLib::ID3v2::AttachedPictureFrame::Other;
    switch (albumart->m_imageType)
    {
        case IT_FRONTCOVER:
            type = TagLib::ID3v2::AttachedPictureFrame::FrontCover;
            break;
        case IT_BACKCOVER:
            type = TagLib::ID3v2::AttachedPictureFrame::BackCover;
            break;
        case IT_CD:
            type = TagLib::ID3v2::AttachedPictureFrame::Media;
            break;
        case IT_INLAY:
            type = TagLib::ID3v2::AttachedPictureFrame::LeafletPage;
            break;
        case IT_ARTIST:
            type = TagLib::ID3v2::AttachedPictureFrame::Artist;
            break;
        default:
            type = TagLib::ID3v2::AttachedPictureFrame::Other;
            break;
    }

    if (!OpenFile(filename, true))
        return false;

    TagLib::ID3v2::Tag *tag = GetID3v2Tag();

    if (!tag)
        return false;

    TagLib::ID3v2::AttachedPictureFrame *apic = findAPIC(tag, type,
                                    QStringToTString(albumart->m_description));

    if (!apic)
    {
        apic = new TagLib::ID3v2::AttachedPictureFrame();
        tag->addFrame(apic);
        apic->setType(type);
    }

    QString mimetype = "image/jpeg";

    TagLib::ByteVector bytevector;
    bytevector.setData(imageData.data(), imageData.size());

    apic->setMimeType(QStringToTString(mimetype));
    apic->setPicture(bytevector);
    apic->setDescription(QStringToTString(albumart->m_description));

    return SaveFile();
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

    TagLib::ID3v2::AttachedPictureFrame::Type type = TagLib::ID3v2::AttachedPictureFrame::Other;
    switch (albumart->m_imageType)
    {
        case IT_FRONTCOVER:
            type = TagLib::ID3v2::AttachedPictureFrame::FrontCover;
            break;
        case IT_BACKCOVER:
            type = TagLib::ID3v2::AttachedPictureFrame::BackCover;
            break;
        case IT_CD:
            type = TagLib::ID3v2::AttachedPictureFrame::Media;
            break;
        case IT_INLAY:
            type = TagLib::ID3v2::AttachedPictureFrame::LeafletPage;
            break;
        case IT_ARTIST:
            type = TagLib::ID3v2::AttachedPictureFrame::Artist;
            break;
        default:
            type = TagLib::ID3v2::AttachedPictureFrame::Other;
            break;
    }

    if (!OpenFile(filename, true))
        return false;

    TagLib::ID3v2::Tag *tag = GetID3v2Tag();

    if (!tag)
        return false;

    TagLib::ID3v2::AttachedPictureFrame *apic = findAPIC(tag, type,
                                    QStringToTString(albumart->m_description));
    if (!apic)
        return false;

    tag->removeFrame(apic);

    return SaveFile();
}

bool MetaIOID3::changeImageType(const QString &filename,
                                const AlbumArtImage* albumart,
                                ImageType newType)
{
    if (!albumart)
        return false;

    if (albumart->m_imageType == newType)
        return true;

    TagLib::ID3v2::AttachedPictureFrame::Type type = TagLib::ID3v2::AttachedPictureFrame::Other;
    switch (albumart->m_imageType)
    {
        case IT_FRONTCOVER:
            type = TagLib::ID3v2::AttachedPictureFrame::FrontCover;
            break;
        case IT_BACKCOVER:
            type = TagLib::ID3v2::AttachedPictureFrame::BackCover;
            break;
        case IT_CD:
            type = TagLib::ID3v2::AttachedPictureFrame::Media;
            break;
        case IT_INLAY:
            type = TagLib::ID3v2::AttachedPictureFrame::LeafletPage;
            break;
        case IT_ARTIST:
            type = TagLib::ID3v2::AttachedPictureFrame::Artist;
            break;
        default:
            type = TagLib::ID3v2::AttachedPictureFrame::Other;
            break;
    }

    if (!OpenFile(filename, true))
        return false;

    TagLib::ID3v2::Tag *tag = GetID3v2Tag();

    if (!tag)
        return false;

    TagLib::ID3v2::AttachedPictureFrame *apic = findAPIC(tag, type,
                                    QStringToTString(albumart->m_description));
    if (!apic)
        return false;

    // set the new image type
    switch (newType)
    {
        case IT_FRONTCOVER:
            apic->setType(TagLib::ID3v2::AttachedPictureFrame::FrontCover);
            break;
        case IT_BACKCOVER:
            apic->setType(TagLib::ID3v2::AttachedPictureFrame::BackCover);
            break;
        case IT_CD:
            apic->setType(TagLib::ID3v2::AttachedPictureFrame::Media);
            break;
        case IT_INLAY:
            apic->setType(TagLib::ID3v2::AttachedPictureFrame::LeafletPage);
            break;
        case IT_ARTIST:
            apic->setType(TagLib::ID3v2::AttachedPictureFrame::Artist);
            break;
        default:
            apic->setType(TagLib::ID3v2::AttachedPictureFrame::Other);
            break;
    }

    return SaveFile();
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
TagLib::ID3v2::UserTextIdentificationFrame* MetaIOID3::find(TagLib::ID3v2::Tag *tag,
                                                const TagLib::String &description)
{
  TagLib::ID3v2::FrameList l = tag->frameList("TXXX");
  for (auto & frame : l)
  {
    auto *f = dynamic_cast<TagLib::ID3v2::UserTextIdentificationFrame *>(frame);
    if (f && f->description() == description)
      return f;
  }
  return nullptr;
}

/*!
 * \brief Find the POPM tag associated with MythTV
 *
 * \param tag Pointer to TagLib::ID3v2::Tag object
 * \param email Email address associated with this POPM frame
 * \returns Pointer to frame
 */
TagLib::ID3v2::PopularimeterFrame* MetaIOID3::findPOPM(TagLib::ID3v2::Tag *tag,
                                        const TagLib::String &_email)
{
  TagLib::ID3v2::FrameList l = tag->frameList("POPM");
  for (auto & frame : l)
  {
    auto *f = dynamic_cast<TagLib::ID3v2::PopularimeterFrame *>(frame);
    if (f && f->email() == _email)
      return f;
  }
  return nullptr;
}

bool MetaIOID3::writePlayCount(TagLib::ID3v2::Tag *tag, int playcount)
{
    if (!tag)
        return false;

    // MythTV Specific playcount Tag
    TagLib::ID3v2::PopularimeterFrame *popm = findPOPM(tag, email);

    if (!popm)
    {
        popm = new TagLib::ID3v2::PopularimeterFrame();
        tag->addFrame(popm);
        popm->setEmail(email);
    }

    int prevCount = popm->counter();
    int countDiff = playcount - prevCount;
    // Allow for situations where the user has rolled back to an old DB backup
    if (countDiff > 0)
    {
        popm->setCounter(playcount);

        // Global playcount Tag - Updated by all apps/hardware that support it
        TagLib::ID3v2::PopularimeterFrame *gpopm = findPOPM(tag, "");
        if (!gpopm)
        {
            gpopm = new TagLib::ID3v2::PopularimeterFrame();
            tag->addFrame(gpopm);
            gpopm->setEmail("");
        }
        gpopm->setCounter((gpopm->counter() > 0) ? gpopm->counter() + countDiff : playcount);
    }

    return true;
}

bool MetaIOID3::writeVolatileMetadata(const QString &filename, MusicMetadata* mdata)
{
    if (filename.isEmpty())
        return false;

    int rating = mdata->Rating();
    int playcount = mdata->PlayCount();
    QDateTime lastPlay = mdata->LastPlay();

    if (!OpenFile(filename, true))
        return false;

    TagLib::ID3v2::Tag *tag = GetID3v2Tag();

    if (!tag)
        return false;

    bool result = (writeRating(tag, rating) && writePlayCount(tag, playcount) &&
                   writeLastPlay(tag, lastPlay));

    if (!SaveFile())
        return false;

    return result;
}

bool MetaIOID3::writeRating(TagLib::ID3v2::Tag *tag, int rating)
{
    if (!tag)
        return false;

    int popmrating = lroundf(static_cast<float>(rating) / 10.0F * 255.0F);

    // MythTV Specific Rating Tag
    TagLib::ID3v2::PopularimeterFrame *popm = findPOPM(tag, email);

    if (!popm)
    {
        popm = new TagLib::ID3v2::PopularimeterFrame();
        tag->addFrame(popm);
        popm->setEmail(email);
    }
    popm->setRating(popmrating);

    // Global Rating Tag
    TagLib::ID3v2::PopularimeterFrame *gpopm = findPOPM(tag, "");
    if (!gpopm)
    {
        gpopm = new TagLib::ID3v2::PopularimeterFrame();
        tag->addFrame(gpopm);
        gpopm->setEmail("");
    }
    gpopm->setRating(popmrating);

    return true;
}

bool MetaIOID3::writeLastPlay(TagLib::ID3v2::Tag *tag, QDateTime lastPlay)
{
    if (!tag)
        return false;

    // MythTV Specific Rating Tag
    TagLib::ID3v2::UserTextIdentificationFrame *txxx = find(tag, "MythTVLastPlayed");

    if (!txxx)
    {
        txxx = new TagLib::ID3v2::UserTextIdentificationFrame();
        tag->addFrame(txxx);
        txxx->setDescription("MythTVLastPlayed");
    }
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
    lastPlay.setTimeSpec(Qt::UTC);
#else
    lastPlay.setTimeZone(QTimeZone(QTimeZone::UTC));
#endif
    txxx->setText(QStringToTString(lastPlay.toString(Qt::ISODate)));

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
