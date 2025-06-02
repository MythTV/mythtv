#ifndef METAIOFLACVORBIS_H_
#define METAIOFLACVORBIS_H_

// libmythmetadata
#include "metaiotaglib.h"
#include "musicmetadata.h"

// Taglib
#include <taglib/flacfile.h>

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
    MetaIOFLACVorbis(void) = default;
    ~MetaIOFLACVorbis(void) override = default;

    bool write(const QString &filename, MusicMetadata* mdata) override; // MetaIOTagLib
    bool writeAlbumArt(const QString &filename, const AlbumArtImage *albumart) override; // MetaIO
    bool removeAlbumArt(const QString &filename, const AlbumArtImage *albumart) override; // MetaIO

    MusicMetadata* read(const QString &filename) override; // MetaIOTagLib
    AlbumArtList getAlbumArtList(const QString &filename) override; // MetaIO
    QImage *getAlbumArt(const QString &filename, ImageType type) override; // MetaIO

    bool supportsEmbeddedImages(void) override { return true; } // MetaIO

    bool changeImageType(const QString &filename, const AlbumArtImage *albumart,
                         ImageType newType) override; // MetaIO

    bool TagExists(const QString &filename) override; // MetaIO

private:
    static TagLib::FLAC::File *OpenFile(const QString &filename);
    static TagLib::FLAC::Picture *getPictureFromFile(TagLib::FLAC::File *flacfile,
                                                     ImageType type);
    static TagLib::FLAC::Picture::Type PictureTypeFromImageType(ImageType itype);
    static QString getExtFromMimeType(const QString &mimeType);
};

#endif
