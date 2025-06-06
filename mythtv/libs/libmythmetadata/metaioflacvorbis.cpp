
// libmythbase
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"

// qt
#include <QBuffer>

// Taglib
#include <taglib/xiphcomment.h>

// libmythmetadata
#include "metaioflacvorbis.h"
#include "musicmetadata.h"
#include "musicutils.h"

/*!
* \brief Open the file to read the tag
*
* \param filename The filename
* \returns A taglib file object for this format
*/
TagLib::FLAC::File *MetaIOFLACVorbis::OpenFile(const QString &filename)
{
    QByteArray fname = filename.toLocal8Bit();
    auto *flacfile = new TagLib::FLAC::File(fname.constData());

    if (!flacfile->isOpen())
    {
        delete flacfile;
        flacfile = nullptr;
    }

    return flacfile;
}


/*!
 * \copydoc MetaIO::write()
 */
bool MetaIOFLACVorbis::write(const QString &filename, MusicMetadata* mdata)
{
    if (!mdata)
        return false;

    if (filename.isEmpty())
        return false;

    m_filename = filename;

    TagLib::FLAC::File *flacfile = OpenFile(m_filename);

    if (!flacfile)
        return false;

    TagLib::Ogg::XiphComment *tag = flacfile->xiphComment(true);

    if (!tag)
    {
        delete flacfile;
        return false;
    }

    WriteGenericMetadata(tag, mdata);

    // Compilation
    if (mdata->Compilation())
    {
        tag->addField("MUSICBRAINZ_ALBUMARTISTID",
                          MYTH_MUSICBRAINZ_ALBUMARTIST_UUID, true);
        tag->addField("COMPILATION_ARTIST",
                        QStringToTString(mdata->CompilationArtist()), true);
    }
    else
    {
        // Don't remove the musicbrainz field unless it indicated a compilation
        if (tag->contains("MUSICBRAINZ_ALBUMARTISTID") &&
            (tag->fieldListMap()["MUSICBRAINZ_ALBUMARTISTID"].toString() ==
                MYTH_MUSICBRAINZ_ALBUMARTIST_UUID))
        {
            tag->removeFields("MUSICBRAINZ_ALBUMARTISTID");
        }
        tag->removeFields("COMPILATION_ARTIST");
    }

    saveTimeStamps();
    bool result = flacfile->save();
    restoreTimeStamps();

    delete flacfile;

    return (result);
}

/*!
 * \copydoc MetaIO::read()
 */
MusicMetadata* MetaIOFLACVorbis::read(const QString &filename)
{
    TagLib::FLAC::File *flacfile = OpenFile(filename);

    if (!flacfile)
        return nullptr;

    TagLib::Ogg::XiphComment *tag = flacfile->xiphComment();

    if (!tag)
    {
        delete flacfile;
        return nullptr;
    }

    auto *metadata = new MusicMetadata(filename);

    ReadGenericMetadata(tag, metadata);

    bool compilation = false;

    if (tag->contains("COMPILATION_ARTIST"))
    {
        QString compilation_artist = TStringToQString(
            tag->fieldListMap()["COMPILATION_ARTIST"].toString()).trimmed();
        if (compilation_artist != metadata->Artist())
        {
            metadata->setCompilationArtist(compilation_artist);
            compilation = true;
        }
    }
    else if (tag->contains("ALBUMARTIST"))
    {
        QString compilation_artist = TStringToQString(
            tag->fieldListMap()["ALBUMARTIST"].toString()).trimmed();
        if (compilation_artist != metadata->Artist())
        {
            metadata->setCompilationArtist(compilation_artist);
            compilation = true;
        }
    }

    if (!compilation && tag->contains("MUSICBRAINZ_ALBUMARTISTID"))
    {
        QString musicbrainzcode = TStringToQString(
        tag->fieldListMap()["MUSICBRAINZ_ALBUMARTISTID"].toString()).trimmed();
        if (musicbrainzcode == MYTH_MUSICBRAINZ_ALBUMARTIST_UUID)
            compilation = true;
    }

    metadata->setCompilation(compilation);

    if (metadata->Length() <= 0ms)
        metadata->setLength(getTrackLength(flacfile));

    if (tag->contains("DISCNUMBER"))
    {
        bool valid = false;
        int n = tag->fieldListMap()["DISCNUMBER"].toString().toInt(&valid);
        if (valid)
            metadata->setDiscNumber(n);
    }

    if (tag->contains("TOTALTRACKS"))
    {
        bool valid = false;
        int n = tag->fieldListMap()["TOTALTRACKS"].toString().toInt(&valid);
        if (valid)
            metadata->setTrackCount(n);
    }

    if (tag->contains("TOTALDISCS"))
    {
        bool valid = false;
        int n = tag->fieldListMap()["TOTALDISCS"].toString().toInt(&valid);
        if (valid)
            metadata->setDiscCount(n);
    }

    delete flacfile;

    return metadata;
}

/*!
 * \brief Read the albumart image from the file
 *
 * \param filename The filename for which we want to find the albumart.
 * \param type The type of image we want - front/back etc
 * \returns A pointer to a QImage owned by the caller or nullptr if not found.
 */
QImage* MetaIOFLACVorbis::getAlbumArt(const QString &filename, ImageType type)
{
    auto *picture = new QImage();
    TagLib::FLAC::File * flacfile = OpenFile(filename);

    if (flacfile)
    {
        TagLib::FLAC::Picture* pic = getPictureFromFile(flacfile, type);
        if (pic)
        {
            picture->loadFromData((const uchar *)pic->data().data(),
                                  pic->data().size());
        }
        else
        {
            delete picture;
            return nullptr;
        }

        delete flacfile;
    }

    return picture;
}

TagLib::FLAC::Picture *MetaIOFLACVorbis::getPictureFromFile(
                                        TagLib::FLAC::File *flacfile,
                                        ImageType type)
{
    TagLib::FLAC::Picture *pic = nullptr;

    if (flacfile)
    {
        TagLib::FLAC::Picture::Type artType = PictureTypeFromImageType(type);

        // From what I can tell, FLAC::File maintains ownership of the Picture pointers, so no need to delete
        const TagLib::List<TagLib::FLAC::Picture *>& picList = flacfile->pictureList();

        for (auto *entry : picList)
        {
            if (entry->type() == artType)
            {
                //found the type we were looking for
                pic = entry;
                break;
            }
        }
    }

    return pic;
}

TagLib::FLAC::Picture::Type MetaIOFLACVorbis::PictureTypeFromImageType(
                                                ImageType itype) {
    TagLib::FLAC::Picture::Type artType = TagLib::FLAC::Picture::Other;
    switch (itype)
    {
        case IT_UNKNOWN :
            artType = TagLib::FLAC::Picture::Other;
            break;
        case IT_FRONTCOVER :
            artType = TagLib::FLAC::Picture::FrontCover;
            break;
        case IT_BACKCOVER :
            artType = TagLib::FLAC::Picture::BackCover;
            break;
        case IT_CD :
            artType = TagLib::FLAC::Picture::Media;
            break;
        case IT_INLAY :
            artType = TagLib::FLAC::Picture::LeafletPage;
            break;
        case IT_ARTIST :
            artType = TagLib::FLAC::Picture::Artist;
            break;
        default:
            return TagLib::FLAC::Picture::Other;
    }

    return artType;
}

/*!
 * \brief Read the albumart images from the file
 *
 * \param filename The filename for which we want to find the images.
 */
AlbumArtList MetaIOFLACVorbis::getAlbumArtList(const QString &filename)
{
    AlbumArtList artlist;
    TagLib::FLAC::File * flacfile = OpenFile(filename);

    if (flacfile)
    {
        const TagLib::List<TagLib::FLAC::Picture *>& picList = flacfile->pictureList();

        for (auto *pic : picList)
        {
            // Assume a valid image would have at least
            // 100 bytes of data (1x1 indexed gif is 35 bytes)
            if (pic->data().size() < 100)
            {
                LOG(VB_GENERAL, LOG_NOTICE,
                    "Music Scanner - Discarding picture "
                    "with size less than 100 bytes");
                continue;
            }

            auto *art = new AlbumArtImage();

            if (pic->description().isEmpty())
                art->m_description.clear();
            else
                art->m_description = TStringToQString(pic->description());

            art->m_embedded = true;
            art->m_hostname = gCoreContext->GetHostName();

            QString ext = getExtFromMimeType(
                                TStringToQString(pic->mimeType()).toLower());

            switch (pic->type())
            {
                case TagLib::FLAC::Picture::FrontCover :
                    art->m_imageType = IT_FRONTCOVER;
                    art->m_filename = QString("front") + ext;
                    break;
                case TagLib::FLAC::Picture::BackCover :
                    art->m_imageType = IT_BACKCOVER;
                    art->m_filename = QString("back") + ext;
                    break;
                case TagLib::FLAC::Picture::Media :
                    art->m_imageType = IT_CD;
                    art->m_filename = QString("cd") + ext;
                    break;
                case TagLib::FLAC::Picture::LeafletPage :
                    art->m_imageType = IT_INLAY;
                    art->m_filename = QString("inlay") + ext;
                    break;
                case TagLib::FLAC::Picture::Artist :
                    art->m_imageType = IT_ARTIST;
                    art->m_filename = QString("artist") + ext;
                    break;
                case TagLib::FLAC::Picture::Other :
                    art->m_imageType = IT_UNKNOWN;
                    art->m_filename = QString("unknown") + ext;
                    break;
                default:
                    LOG(VB_GENERAL, LOG_ERR, "Music Scanner - picture found "
                                             "with unsupported type");
                    delete art;
                    continue;
            }

            artlist.append(art);
        }
    }

    delete flacfile;
    return artlist;
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
bool MetaIOFLACVorbis::writeAlbumArt(const QString &filename,
                              const AlbumArtImage *albumart)
{
#if TAGLIB_MAJOR_VERSION == 1 && TAGLIB_MINOR_VERSION >= 8
    if (filename.isEmpty() || !albumart)
        return false;

    m_filename = filename;

    bool retval = false;

    // load the image into a QByteArray
    QImage image(albumart->m_filename);
    QByteArray imageData;
    QBuffer buffer(&imageData);
    buffer.open(QIODevice::WriteOnly);
    // Write the image data to a file
    image.save(&buffer, "JPEG");

    TagLib::FLAC::File * flacfile = OpenFile(filename);

    // This presumes that there is only one art item of each type
    if (flacfile)
    {
        // Now see if the art is in the FLAC file
        TagLib::FLAC::Picture *pic = getPictureFromFile(flacfile, albumart->m_imageType);

        if (pic)
        {
            // Remove the embedded image of the matching type
            flacfile->removePicture(pic, false);
        }
        else
        {
            // Create a new image of the correct type
            pic = new TagLib::FLAC::Picture();
            pic->setType(PictureTypeFromImageType(albumart->m_imageType));
        }

        TagLib::ByteVector bytevector;
        bytevector.setData(imageData.data(), imageData.size());

        pic->setData(bytevector);
        QString mimetype = "image/jpeg";

        pic->setMimeType(QStringToTString(mimetype));
        pic->setDescription(QStringToTString(albumart->m_description));

        flacfile->addPicture(pic);

        saveTimeStamps();
        retval = flacfile->save();
        restoreTimeStamps();

        delete flacfile;
    }
    else
    {
        retval = false;
    }

    return retval;
#else
    LOG(VB_GENERAL, LOG_WARNING,
        "TagLib 1.8.0 or later is required to write albumart to flac xiphComment tags");
    return false;
#endif
}

/*!
 * \brief Remove the albumart image from the file
 *
 * \param filename The music file to remove the albumart
 * \param albumart The Album Art image to remove
 * \returns True if successful
 */
bool MetaIOFLACVorbis::removeAlbumArt(const QString &filename,
                               const AlbumArtImage *albumart)
{
#if TAGLIB_MAJOR_VERSION == 1 && TAGLIB_MINOR_VERSION >= 8

    if (filename.isEmpty() || !albumart)
        return false;

    bool retval = false;

    TagLib::FLAC::File * flacfile = OpenFile(filename);

    // This presumes that there is only one art item of each type
    if (flacfile)
    {
        // Now see if the art is in the FLAC file
        TagLib::FLAC::Picture *pic = getPictureFromFile(flacfile, albumart->m_imageType);

        if (pic)
        {
            // Remove the embedded image of the matching type
            flacfile->removePicture(pic, false);
            flacfile->save();
            retval = true;
        }
        else
        {
            retval = false;
        }

        delete flacfile;
    }
    else
    {
        retval = false;
    }

    return retval;
#else
    LOG(VB_GENERAL, LOG_WARNING,
        "TagLib 1.8.0 or later is required to remove albumart from flac xiphComment tags");
    return false;
#endif
}

bool MetaIOFLACVorbis::changeImageType(const QString &filename,
                                const AlbumArtImage* albumart,
                                ImageType newType)
{
    if (filename.isEmpty() || !albumart)
        return false;

    if (albumart->m_imageType == newType)
        return true;

    bool retval = false;

    TagLib::FLAC::File * flacfile = OpenFile(filename);

    // This presumes that there is only one art item of each type
    if (flacfile)
    {
        // Now see if the art is in the FLAC file
        TagLib::FLAC::Picture *pic = getPictureFromFile(flacfile, albumart->m_imageType);

        if (pic)
        {
            pic->setType(PictureTypeFromImageType(newType));
            flacfile->save();
            retval = true;
        }
        else
        {
            retval = false;
        }

        delete flacfile;
    }
    else
    {
        retval = false;
    }

    return retval;
}

bool MetaIOFLACVorbis::TagExists(const QString &filename)
{
    TagLib::FLAC::File *flacfile = OpenFile(filename);

    if (!flacfile)
        return false;

    TagLib::Ogg::XiphComment *tag = flacfile->xiphComment(false);

    bool retval = false;
    if (tag && !tag->isEmpty())
        retval = true;

    delete flacfile;

    return retval;
}

QString MetaIOFLACVorbis::getExtFromMimeType(const QString &mimeType)
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
