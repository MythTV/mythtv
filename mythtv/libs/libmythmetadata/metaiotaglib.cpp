// Std
#include <cmath>

// Libmyth
#include <mythlogging.h>

// Taglib
#include <audioproperties.h>
#include <tag.h>
#include <tstring.h>
#include <fileref.h>

// libmythmetadata
#include "metaiotaglib.h"
#include "musicmetadata.h"
#include "musicutils.h"

MetaIOTagLib::MetaIOTagLib()
    : MetaIO()
{
}

MetaIOTagLib::~MetaIOTagLib(void)
{
}

/*!
* \brief Writes metadata common to all tag formats to the tag
*
* \param tag A pointer to the tag
* \param metadata Pointer to the MusicMetadata
*/
void MetaIOTagLib::WriteGenericMetadata(Tag *tag,  const MusicMetadata *metadata)
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
void MetaIOTagLib::ReadGenericMetadata(Tag *tag, MusicMetadata *metadata)
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

    // If we don't have title and artist or don't have the length return NULL
    if (metadata->Title().isEmpty() && metadata->Artist().isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("MetaIOTagLib: Failed to read metadata from '%1'")
                .arg(metadata->Filename(false)));
    }
}

/*!
* \brief Find the length of the track (in seconds)
*
* \param file Pointer to file object
* \returns An integer (signed!) to represent the length in milliseconds.
*/
int MetaIOTagLib::getTrackLength(TagLib::File *file)
{
    int milliseconds = 0;

    if (file && file->audioProperties())
        milliseconds = file->audioProperties()->length() * 1000;

    return milliseconds;
}

/*!
* \brief Find the length of the track (in seconds)
*
* \param filename The filename for which we want to find the length.
* \returns An integer (signed!) to represent the length in milliseconds.
*/
int MetaIOTagLib::getTrackLength(const QString &filename)
{
    int milliseconds = 0;
    QByteArray fname = filename.toLocal8Bit();
    TagLib::FileRef *file = new TagLib::FileRef(fname.constData());

    if (file && file->audioProperties())
        milliseconds = file->audioProperties()->length() * 1000;

    // If we didn't get a valid length, add the metadata but show warning.
    if (milliseconds <= 1000)
        LOG(VB_GENERAL, LOG_ERR,
            QString("MetaIOTagLib: Failed to read length "
                    "from '%1'. It may be corrupt.").arg(filename));

    delete file;

    return milliseconds;
}
