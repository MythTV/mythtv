#ifndef METAIOFLACVORBIS_H_
#define METAIOFLACVORBIS_H_

// libmythmetadata
#include "metaiotaglib.h"
#include "musicmetadata.h"

// Taglib
#include <flacfile.h>

using TagLib::Tag;
using TagLib::String;

/*!
* \class MetaIOFLACVorbis
*
* \brief Read and write Vorbis (Xiph) tags in a FLAC file
*
* \copydetails MetaIO
*/
class META_PUBLIC MetaIOFLACVorbis : public MetaIOTagLib
{
public:
    MetaIOFLACVorbis(void);
    virtual ~MetaIOFLACVorbis(void);

    bool write(const QString &filename, MusicMetadata* mdata);
    bool writeAlbumArt(const QString &filename, const AlbumArtImage *albumart);
    bool removeAlbumArt(const QString &filename, const AlbumArtImage *albumart);

    MusicMetadata* read(const QString &filename);
    AlbumArtList getAlbumArtList(const QString &filename);
    QImage *getAlbumArt(const QString &filename, ImageType type);

    bool supportsEmbeddedImages(void) { return true; }

    bool changeImageType(const QString &filename, const AlbumArtImage *albumart,
                         ImageType newType);

    virtual bool TagExists(const QString &filename);

private:
    TagLib::FLAC::File *OpenFile(const QString &filename);
    TagLib::FLAC::Picture *getPictureFromFile(TagLib::FLAC::File *flacfile,
                                              ImageType type);
    TagLib::FLAC::Picture::Type PictureTypeFromImageType(ImageType itype);
    QString getExtFromMimeType(const QString &mimeType);
};

#endif
