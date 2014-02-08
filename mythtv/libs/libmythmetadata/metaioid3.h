#ifndef METAIOID3_H_
#define METAIOID3_H_

// libmythmetadata
#include "metaiotaglib.h"
#include "musicmetadata.h"

// Taglib
#include <id3v1tag.h>
#include <id3v2tag.h>
#include <textidentificationframe.h>
#include <attachedpictureframe.h>
#include <popularimeterframe.h>
#include <tfile.h>

// QT
#include <QList>

using TagLib::ID3v2::UserTextIdentificationFrame;
using TagLib::ID3v2::TextIdentificationFrame;
using TagLib::ID3v2::PopularimeterFrame;
using TagLib::ID3v2::AttachedPictureFrame;

/*!
* \class MetaIOID3
*
* \brief Read and write metadata in MPEG (mp3) ID3V2 tags
*
* Will read ID3v1 but always writes ID3v2.4 compliant tags.
*
* \copydetails MetaIO
*/
class META_PUBLIC MetaIOID3 : public MetaIOTagLib
{
  public:
    MetaIOID3(void);
    virtual ~MetaIOID3(void);

    virtual bool write(const QString &filename, MusicMetadata* mdata);
    bool writeVolatileMetadata(const QString &filename, MusicMetadata* mdata);

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
    bool OpenFile(const QString &filename, bool forWriting = false);
    bool SaveFile();
    void CloseFile();

    TagLib::ID3v2::Tag* GetID3v2Tag(bool create = false);
    TagLib::ID3v1::Tag* GetID3v1Tag(bool create = false);

    bool writePlayCount(TagLib::ID3v2::Tag *tag, int playcount);
    bool writeRating(TagLib::ID3v2::Tag *tag, int rating);

    AlbumArtList readAlbumArt(TagLib::ID3v2::Tag *tag);
    UserTextIdentificationFrame* find(TagLib::ID3v2::Tag *tag,
                                      const String &description);
    PopularimeterFrame* findPOPM(TagLib::ID3v2::Tag *tag, const String &email);
    AttachedPictureFrame* findAPIC(TagLib::ID3v2::Tag *tag,
                                   const AttachedPictureFrame::Type &type,
                                   const String &description = String::null);
    QString getExtFromMimeType(const QString &mimeType);

    TagLib::File *m_file;

    typedef enum { kMPEG, kFLAC } TagType;
    TagType m_fileType;
};

#endif
