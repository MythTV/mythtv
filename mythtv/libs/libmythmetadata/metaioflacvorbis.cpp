
// Libmyth
#include <mythcontext.h>

// qt
#include <QBuffer>

// Taglib
#include <xiphcomment.h>

// libmythmetadata
#include "metaioflacvorbis.h"
#include "musicmetadata.h"
#include "musicutils.h"

MetaIOFLACVorbis::MetaIOFLACVorbis(void)
    : MetaIOTagLib()
{
}

MetaIOFLACVorbis::~MetaIOFLACVorbis(void)
{
}

/*!
* \brief Open the file to read the tag
*
* \param filename The filename
* \returns A taglib file object for this format
*/
TagLib::FLAC::File *MetaIOFLACVorbis::OpenFile(const QString &filename)
{
    QByteArray fname = filename.toLocal8Bit();
    TagLib::FLAC::File *flacfile =
                            new TagLib::FLAC::File(fname.constData());

    if (!flacfile->isOpen())
    {
        delete flacfile;
        flacfile = NULL;
    }

    return flacfile;
}


/*!
 * \copydoc MetaIO::write()
 */
bool MetaIOFLACVorbis::write(const MusicMetadata* mdata)
{
    if (!mdata)
        return false;

    m_filename = mdata->Filename(true);

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
            tag->removeField("MUSICBRAINZ_ALBUMARTISTID");
        }
        tag->removeField("COMPILATION_ARTIST");
    }

    saveTimeStamps();
    bool result = flacfile->save();
    restoreTimeStamps();

    if (flacfile)
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
        return NULL;

    TagLib::Ogg::XiphComment *tag = flacfile->xiphComment();

    if (!tag)
    {
        delete flacfile;
        return NULL;
    }

    MusicMetadata *metadata = new MusicMetadata(filename);

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

    if (!compilation && tag->contains("MUSICBRAINZ_ALBUMARTISTID"))
    {
        QString musicbrainzcode = TStringToQString(
        tag->fieldListMap()["MUSICBRAINZ_ALBUMARTISTID"].toString()).trimmed();
        if (musicbrainzcode == MYTH_MUSICBRAINZ_ALBUMARTIST_UUID)
            compilation = true;
    }

    metadata->setCompilation(compilation);

    if (metadata->Length() <= 0)
        metadata->setLength(getTrackLength(flacfile));

    delete flacfile;

    return metadata;
}

/*!
 * \brief Read the albumart image from the file
 *
 * \param filename The filename for which we want to find the albumart.
 * \param type The type of image we want - front/back etc
 * \returns A pointer to a QImage owned by the caller or NULL if not found.
 */
QImage* MetaIOFLACVorbis::getAlbumArt(const QString &filename, ImageType type)
{
    QImage *picture = new QImage();
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
            return NULL;
        }

        delete flacfile;
    }

    return picture;
}

TagLib::FLAC::Picture *MetaIOFLACVorbis::getPictureFromFile(
                                        TagLib::FLAC::File *flacfile,
                                        ImageType type)
{
    using TagLib::FLAC::Picture;

    Picture *pic = NULL;

    if (flacfile)
    {
        Picture::Type artType = PictureTypeFromImageType(type);

        // From what I can tell, FLAC::File maintains ownership of the Picture pointers, so no need to delete
        const TagLib::List<Picture *>& picList = flacfile->pictureList();

        for (TagLib::List<Picture *>::ConstIterator it = picList.begin();
                it != picList.end(); it++)
        {
            pic = *it;
            if (pic->type() == artType)
            {
                //found the type we were looking for
                break;
            }
        }
    }

    return pic;
}

TagLib::FLAC::Picture::Type MetaIOFLACVorbis::PictureTypeFromImageType(
                                                ImageType itype) {
    using TagLib::FLAC::Picture;
    Picture::Type artType = Picture::Other;
    switch (itype)
    {
        case IT_UNKNOWN :
            artType = Picture::Other;
            break;
        case IT_FRONTCOVER :
            artType = Picture::FrontCover;
            break;
        case IT_BACKCOVER :
            artType = Picture::BackCover;
            break;
        case IT_CD :
            artType = Picture::Media;
            break;
        case IT_INLAY :
            artType = Picture::LeafletPage;
            break;
        case IT_ARTIST :
            artType = Picture::Artist;
            break;
        default:
            return Picture::Other;
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
    using TagLib::FLAC::Picture;
    AlbumArtList artlist;
    TagLib::FLAC::File * flacfile = OpenFile(filename);

    if (flacfile)
    {
        const TagLib::List<Picture *>& picList = flacfile->pictureList();

        for(TagLib::List<Picture *>::ConstIterator it = picList.begin();
            it != picList.end(); it++)
        {
            Picture* pic = *it;
            // Assume a valid image would have at least
            // 100 bytes of data (1x1 indexed gif is 35 bytes)
            if (pic->data().size() < 100)
            {
                LOG(VB_GENERAL, LOG_NOTICE,
                    "Music Scanner - Discarding picture "
                    "with size less than 100 bytes");
                continue;
            }

            AlbumArtImage *art = new AlbumArtImage();

            if (pic->description().isEmpty())
                art->description.clear();
            else
                art->description = TStringToQString(pic->description());

            art->embedded = true;

            QString ext = getExtFromMimeType(
                                TStringToQString(pic->mimeType()).toLower());

            switch (pic->type())
            {
                case Picture::FrontCover :
                    art->imageType = IT_FRONTCOVER;
                    art->filename = QString("front") + ext;
                    break;
                case Picture::BackCover :
                    art->imageType = IT_BACKCOVER;
                    art->filename = QString("back") + ext;
                    break;
                case Picture::Media :
                    art->imageType = IT_CD;
                    art->filename = QString("cd") + ext;
                    break;
                case Picture::LeafletPage :
                    art->imageType = IT_INLAY;
                    art->filename = QString("inlay") + ext;
                    break;
                case Picture::Artist :
                    art->imageType = IT_ARTIST;
                    art->filename = QString("artist") + ext;
                    break;
                case Picture::Other :
                    art->imageType = IT_UNKNOWN;
                    art->filename = QString("unknown") + ext;
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
 * \Note We always save the image in JPEG format
 */
bool MetaIOFLACVorbis::writeAlbumArt(const QString &filename,
                              const AlbumArtImage *albumart)
{
#if TAGLIB_MAJOR_VERSION == 1 && TAGLIB_MINOR_VERSION >= 8
    using TagLib::FLAC::Picture;
    if (filename.isEmpty() || !albumart)
        return false;

    m_filename = filename;

    bool retval = false;

    // load the image into a QByteArray
    QImage image(albumart->filename);
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
        Picture *pic = getPictureFromFile(flacfile, albumart->imageType);

        if (pic)
        {
            // Remove the embedded image of the matching type
            flacfile->removePicture(pic, false);
        }
        else
        {
            // Create a new image of the correct type
            pic = new Picture();
            pic->setType(PictureTypeFromImageType(albumart->imageType));
        }

        TagLib::ByteVector bytevector;
        bytevector.setData(imageData.data(), imageData.size());

        pic->setData(bytevector);
        QString mimetype = "image/jpeg";

        pic->setMimeType(QStringToTString(mimetype));
        pic->setDescription(QStringToTString(albumart->description));

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
        TagLib::FLAC::Picture *pic = getPictureFromFile(flacfile, albumart->imageType);

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

    if (albumart->imageType == newType)
        return true;

    bool retval = false;

    TagLib::FLAC::File * flacfile = OpenFile(filename);

    // This presumes that there is only one art item of each type
    if (flacfile)
    {
        // Now see if the art is in the FLAC file
        TagLib::FLAC::Picture *pic = getPictureFromFile(flacfile, albumart->imageType);

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
