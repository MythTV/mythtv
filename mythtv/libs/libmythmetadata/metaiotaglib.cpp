// Std
#include <cmath>

// Libmyth
#include "libmythbase/mythlogging.h"

// Taglib
#include <taglib/audioproperties.h>
#include <taglib/tag.h>
#include <taglib/tstring.h>
#include <taglib/fileref.h>

// libmythmetadata
#include "metaiotaglib.h"
#include "musicmetadata.h"
#include "musicutils.h"

/*!
* \brief Writes metadata common to all tag formats to the tag
*
* \param tag A pointer to the tag
* \param metadata Pointer to the MusicMetadata
*/
void MetaIOTagLib::WriteGenericMetadata(TagLib::Tag *tag,  const MusicMetadata *metadata)
{
    if (!tag || !metadata)
        return;

    if (!metadata->Artist().isEmpty())
        tag->setArtist(QStringToTString(metadata->Artist()));

    if (!metadata->Title().isEmpty())
        tag->setTitle(QStringToTString(metadata->Title()));

    if (!metadata->Album().isEmpty())
        tag->setAlbum(QStringToTString(metadata->Album()));

    if (metadata->Year() > 999 && metadata->Year() < 10000) // 4 digit year.
        tag->setYear(metadata->Year());

    if (!metadata->Genre().isEmpty())
        tag->setGenre(QStringToTString(metadata->Genre()));

    if (0 != metadata->Track())
        tag->setTrack(metadata->Track());
}

/*!
* \brief Writes metadata common to all tag formats to the tag
*
* \param tag A pointer to the tag
* \param metadata Pointer to the MusicMetadata
*/
void MetaIOTagLib::ReadGenericMetadata(TagLib::Tag *tag, MusicMetadata *metadata)
{
    if (!tag || ! metadata)
        return;

    // Basic Tags
    if (!tag->isEmpty())
    {
        metadata->setTitle(TStringToQString(tag->title()).trimmed());
        metadata->setArtist(TStringToQString(tag->artist()).trimmed());
        metadata->setAlbum(TStringToQString(tag->album()).trimmed());
        metadata->setTrack(tag->track());
        metadata->setYear(tag->year());
        metadata->setGenre(TStringToQString(tag->genre()).trimmed());
    }

    // Fallback to filename reading
    if (metadata->Title().isEmpty())
        readFromFilename(metadata);

    // If we don't have title and artist or don't have the length return nullptr
    if (metadata->Title().isEmpty() && metadata->Artist().isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("MetaIOTagLib: Failed to read metadata from '%1'")
                .arg(metadata->Filename(false)));
    }
}

/*!
* \brief Find the length of the track (in milliseconds)
*
* \param file Pointer to file object
* \returns An integer (signed!) to represent the length in milliseconds.
*/
std::chrono::milliseconds MetaIOTagLib::getTrackLength(TagLib::File *file)
{
    std::chrono::milliseconds milliseconds = 0ms;

    if (file && file->audioProperties())
        milliseconds = std::chrono::milliseconds(file->audioProperties()->lengthInMilliseconds());

    return milliseconds;
}

/*!
* \brief Find the length of the track (in milliseconds)
*
* \param filename The filename for which we want to find the length.
* \returns An integer (signed!) to represent the length in milliseconds.
*/
std::chrono::milliseconds MetaIOTagLib::getTrackLength(const QString &filename)
{
    std::chrono::milliseconds milliseconds = 0ms;
    QByteArray fname = filename.toLocal8Bit();
    auto *file = new TagLib::FileRef(fname.constData());

    if (file && file->audioProperties())
        milliseconds = std::chrono::milliseconds(file->audioProperties()->lengthInMilliseconds());

    // If we didn't get a valid length, add the metadata but show warning.
    if (milliseconds <= 1s)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("MetaIOTagLib: Failed to read length "
                    "from '%1'. It may be corrupt.").arg(filename));
    }

    delete file;

    return milliseconds;
}
